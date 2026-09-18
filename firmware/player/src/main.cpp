/**
 * Jeopardy contestant firmware — ESP-NOW player node.
 *
 * States: IDLE → ARMED → LOCKED_IN | LATE | LOCKED_OUT → IDLE (on RESET/DISARM)
 * Ignores button until MSG_ARM; first press sends MSG_BUZZ then locks locally.
 */
#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <string.h>

#include "config.h"
#include "protocol.h"

enum PlayerState : uint8_t {
  ST_IDLE = 0,
  ST_ARMED,
  ST_LOCKED_IN,   /* this player buzzed (awaiting / got ACK place) */
  ST_LATE,        /* buzzed but not first (ACK place > 1) */
  ST_LOCKED_OUT,  /* never buzzed; round locked by someone else */
};

static PlayerState g_state = ST_IDLE;
static uint16_t g_round = 0;
static uint16_t g_buzz_seq = 0;
static uint8_t g_place = 0; /* 0 unknown, 1 = first, ... */
static bool g_base_known = false;
static uint8_t g_base_mac[6] = {0};
static volatile bool g_rx_pending = false;
static JeopardyMsg g_rx_msg;
static uint8_t g_rx_mac[6];

static bool g_btn_stable = false; /* true = pressed (logical) */
static bool g_btn_last_raw = false;
static uint32_t g_btn_last_change = 0;
static bool g_btn_armed_edge = false; /* rising edge of press while ARMED */

static void set_leds(bool on) {
  digitalWrite(PIN_LED, on ? HIGH : LOW);
#if USE_ONBOARD_LED
  digitalWrite(PIN_LED_ONBOARD, on ? LOW : HIGH); /* active low */
#endif
}

static void update_leds() {
  const uint32_t now = millis();
  switch (g_state) {
    case ST_IDLE:
    case ST_LOCKED_OUT:
      set_leds(false);
      break;
    case ST_ARMED:
      set_leds(((now / LED_BLINK_ARMED_MS) & 1) != 0);
      break;
    case ST_LOCKED_IN:
      set_leds(true);
      break;
    case ST_LATE: {
      const uint32_t period = LED_BLINK_LATE_ON_MS + LED_BLINK_LATE_OFF_MS;
      const uint32_t t = now % period;
      set_leds(t < LED_BLINK_LATE_ON_MS);
      break;
    }
  }
}

static void send_msg(const uint8_t *dest, JeopardyMsgType type, uint8_t pid,
                     uint16_t seq) {
  JeopardyMsg m = {};
  m.magic = JEOPARDY_MAGIC;
  m.version = JEOPARDY_VERSION;
  m.type = static_cast<uint8_t>(type);
  m.player_id = pid;
  m.seq = seq;
  m.timestamp_ms = millis();

  esp_err_t err = esp_now_send(dest, reinterpret_cast<uint8_t *>(&m), sizeof(m));
  if (err != ESP_OK) {
    Serial.printf("esp_now_send failed: %d\n", (int)err);
  }
}

static void ensure_peer(const uint8_t *mac) {
  if (esp_now_is_peer_exist(mac)) {
    return;
  }
  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, mac, 6);
  peer.channel = 0;
  peer.encrypt = false;
  esp_err_t err = esp_now_add_peer(&peer);
  if (err != ESP_OK && err != ESP_ERR_ESPNOW_EXIST) {
    Serial.printf("add_peer failed: %d\n", (int)err);
  }
}

static void on_data_recv(const uint8_t *mac, const uint8_t *data, int len) {
  if (len < (int)sizeof(JeopardyMsg)) {
    return;
  }
  JeopardyMsg m;
  memcpy(&m, data, sizeof(m));
  if (!jeopardy_msg_valid(&m)) {
    return;
  }
  memcpy(g_rx_mac, mac, 6);
  g_rx_msg = m;
  g_rx_pending = true;
}

static void learn_base(const uint8_t *mac) {
#if PAIR_LEARN_FROM_ARM
  if (!g_base_known || memcmp(g_base_mac, mac, 6) != 0) {
    memcpy(g_base_mac, mac, 6);
    g_base_known = true;
    ensure_peer(g_base_mac);
    Serial.printf("Learned base MAC %02X:%02X:%02X:%02X:%02X:%02X\n",
                  g_base_mac[0], g_base_mac[1], g_base_mac[2], g_base_mac[3],
                  g_base_mac[4], g_base_mac[5]);
  }
#else
  (void)mac;
#endif
}

static bool from_trusted_base(const uint8_t *mac) {
#if PAIR_LEARN_FROM_ARM
  if (!g_base_known) {
    return true; /* accept first control to learn */
  }
  return memcmp(g_base_mac, mac, 6) == 0;
#else
  static const uint8_t fixed[6] = {BASE_MAC_0, BASE_MAC_1, BASE_MAC_2,
                                   BASE_MAC_3, BASE_MAC_4, BASE_MAC_5};
  return memcmp(fixed, mac, 6) == 0;
#endif
}

static void handle_rx() {
  if (!g_rx_pending) {
    return;
  }
  noInterrupts();
  JeopardyMsg m = g_rx_msg;
  uint8_t mac[6];
  memcpy(mac, g_rx_mac, 6);
  g_rx_pending = false;
  interrupts();

  if (!from_trusted_base(mac)) {
    return;
  }

  switch (m.type) {
    case MSG_ARM:
      learn_base(mac);
      g_round = m.seq;
      g_place = 0;
      g_state = ST_ARMED;
      g_btn_armed_edge = false;
      Serial.printf("ARM round=%u\n", g_round);
      break;

    case MSG_DISARM:
      g_state = ST_IDLE;
      g_place = 0;
      Serial.println("DISARM");
      break;

    case MSG_RESET:
      g_round = m.seq;
      g_place = 0;
      g_state = ST_IDLE;
      Serial.printf("RESET round=%u\n", g_round);
      break;

    case MSG_LOCKOUT:
      if (g_state == ST_ARMED) {
        g_state = ST_LOCKED_OUT;
      }
      /* LOCKED_IN / LATE keep their LED pattern */
      Serial.printf("LOCKOUT winner=%u round=%u\n", m.player_id, m.seq);
      break;

    case MSG_ACK:
      if (m.player_id == PLAYER_ID) {
        g_place = (uint8_t)m.seq;
        if (g_place == 1) {
          g_state = ST_LOCKED_IN;
        } else if (g_place > 1) {
          g_state = ST_LATE;
        }
        Serial.printf("ACK place=%u\n", g_place);
      }
      break;

    case MSG_HEARTBEAT:
      break;

    default:
      break;
  }
}

static void poll_button() {
  const bool raw =
#if BUTTON_ACTIVE_LOW
      (digitalRead(PIN_BUTTON) == LOW);
#else
      (digitalRead(PIN_BUTTON) == HIGH);
#endif

  const uint32_t now = millis();
  if (raw != g_btn_last_raw) {
    g_btn_last_raw = raw;
    g_btn_last_change = now;
  }
  if ((now - g_btn_last_change) < DEBOUNCE_MS) {
    return;
  }

  if (raw && !g_btn_stable) {
    g_btn_stable = true;
    if (g_state == ST_ARMED) {
      g_btn_armed_edge = true;
    }
  } else if (!raw && g_btn_stable) {
    g_btn_stable = false;
  }
}

static void try_buzz() {
  if (!g_btn_armed_edge) {
    return;
  }
  g_btn_armed_edge = false;
  if (g_state != ST_ARMED) {
    return;
  }

  g_buzz_seq++;
  g_state = ST_LOCKED_IN; /* optimistic; ACK may demote to LATE */
  set_leds(true);

  uint8_t dest[6];
  if (g_base_known) {
    memcpy(dest, g_base_mac, 6);
    ensure_peer(dest);
  } else {
    /* Broadcast until base learned (first ARM usually comes first) */
    memset(dest, 0xFF, 6);
    ensure_peer(dest);
  }
  send_msg(dest, MSG_BUZZ, PLAYER_ID, g_buzz_seq);
  Serial.printf("BUZZ id=%u seq=%u\n", PLAYER_ID, g_buzz_seq);
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.printf("\nJeopardy player id=%d\n", PLAYER_ID);

  pinMode(PIN_BUTTON, INPUT_PULLUP);
  pinMode(PIN_LED, OUTPUT);
#if USE_ONBOARD_LED
  pinMode(PIN_LED_ONBOARD, OUTPUT);
#endif
  set_leds(false);

#if !PAIR_LEARN_FROM_ARM
  g_base_mac[0] = BASE_MAC_0;
  g_base_mac[1] = BASE_MAC_1;
  g_base_mac[2] = BASE_MAC_2;
  g_base_mac[3] = BASE_MAC_3;
  g_base_mac[4] = BASE_MAC_4;
  g_base_mac[5] = BASE_MAC_5;
  g_base_known = true;
#endif

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(50);

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    while (true) {
      delay(1000);
    }
  }
  esp_now_register_recv_cb(on_data_recv);

  /* Broadcast peer for early BUZZ before learn */
  uint8_t bcast[6];
  memset(bcast, 0xFF, 6);
  ensure_peer(bcast);

#if !PAIR_LEARN_FROM_ARM
  ensure_peer(g_base_mac);
#endif

  uint8_t mac[6];
  WiFi.macAddress(mac);
  Serial.printf("STA MAC %02X:%02X:%02X:%02X:%02X:%02X\n", mac[0], mac[1],
                mac[2], mac[3], mac[4], mac[5]);
}

void loop() {
  handle_rx();
  poll_button();
  try_buzz();
  update_leds();
  delay(1);
}
