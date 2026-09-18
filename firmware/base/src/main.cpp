/**
 * Jeopardy base station — ESP-NOW controller + USB HID keyboard + CDC log.
 *
 * Buttons (default dual-button wiring):
 *   D1 ARM:  short press → ARM if idle/disarmed, RESET+ARM if round active
 *            long press  → DISARM (if USE_DEDICATED_HOST_BTN), else host key
 *   D2 HOST: press → inject HOST_KEY via TinyUSB HID (optional)
 *
 * Single-button mode (USE_DEDICATED_HOST_BTN=0):
 *   short → ARM/RESET cycle, long → host keystroke
 *
 * Buzz order: every MSG_BUZZ after ARM is recorded; first triggers LOCKOUT
 * broadcast and winner LED feedback on players; later buzzes still ACK'd.
 */
#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

#include "USB.h"
#include "USBHIDKeyboard.h"

#include "config.h"
#include "protocol.h"

USBHIDKeyboard Keyboard;

enum RoundPhase : uint8_t {
  PHASE_IDLE = 0,
  PHASE_ARMED,
  PHASE_LOCKED,
};

static RoundPhase g_phase = PHASE_IDLE;
static uint16_t g_round = 0;
static uint8_t g_order[MAX_ORDER];
static uint8_t g_order_len = 0;
static uint8_t g_winner = 0;

static volatile bool g_rx_pending = false;
static JeopardyMsg g_rx_msg;
static uint8_t g_rx_mac[6];

struct Btn {
  uint8_t pin;
  bool stable;
  bool last_raw;
  uint32_t last_change;
  uint32_t press_start;
  bool pressed;
  bool long_fired;
};

static Btn g_btn_arm = {PIN_BTN_ARM, false, false, 0, 0, false, false};
#if USE_DEDICATED_HOST_BTN
static Btn g_btn_host = {PIN_BTN_HOST, false, false, 0, 0, false, false};
#endif

static void set_leds(bool on) {
  digitalWrite(PIN_LED, on ? HIGH : LOW);
#if USE_ONBOARD_LED
  digitalWrite(PIN_LED_ONBOARD, on ? LOW : HIGH);
#endif
}

static void update_status_led() {
  const uint32_t now = millis();
  switch (g_phase) {
    case PHASE_IDLE:
      set_leds(false);
      break;
    case PHASE_ARMED:
      set_leds(((now / 200) & 1) != 0);
      break;
    case PHASE_LOCKED:
      set_leds(true);
      break;
  }
}

static void cdc_printf(const char *fmt, ...) {
  char buf[160];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  Serial.print(buf);
}

static void ensure_peer(const uint8_t *mac) {
  if (esp_now_is_peer_exist(mac)) {
    return;
  }
  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, mac, 6);
  peer.channel = 0;
  peer.encrypt = false;
  esp_now_add_peer(&peer);
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
  ensure_peer(dest);
  esp_err_t err = esp_now_send(dest, reinterpret_cast<uint8_t *>(&m), sizeof(m));
  if (err != ESP_OK) {
    cdc_printf("WARN send fail type=%u err=%d\n", (unsigned)type, (int)err);
  }
}

static void broadcast(JeopardyMsgType type, uint8_t pid, uint16_t seq) {
  uint8_t bcast[6];
  memset(bcast, 0xFF, 6);
  send_msg(bcast, type, pid, seq);
}

static void clear_order() {
  memset(g_order, 0, sizeof(g_order));
  g_order_len = 0;
  g_winner = 0;
}

static void do_arm() {
  g_round++;
  if (g_round == 0) {
    g_round = 1;
  }
  clear_order();
  g_phase = PHASE_ARMED;
  broadcast(MSG_ARM, 0, g_round);
  cdc_printf("ARM round=%u\n", g_round);
}

static void do_disarm() {
  g_phase = PHASE_IDLE;
  broadcast(MSG_DISARM, 0, g_round);
  cdc_printf("DISARM round=%u\n", g_round);
}

static void do_reset() {
  clear_order();
  g_phase = PHASE_IDLE;
  broadcast(MSG_RESET, 0, g_round);
  cdc_printf("RESET round=%u\n", g_round);
}

static void inject_host_key() {
  Keyboard.press(HOST_KEY);
  delay(20);
  Keyboard.release(HOST_KEY);
  cdc_printf("HOST_KEY\n");
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

static bool already_in_order(uint8_t id) {
  for (uint8_t i = 0; i < g_order_len; i++) {
    if (g_order[i] == id) {
      return true;
    }
  }
  return false;
}

static void handle_buzz(const uint8_t *mac, const JeopardyMsg &m) {
  if (g_phase != PHASE_ARMED && g_phase != PHASE_LOCKED) {
    return; /* ignore buzz while idle */
  }
  if (m.player_id < 1 || m.player_id > JEOPARDY_MAX_PLAYERS) {
    return;
  }
  if (already_in_order(m.player_id)) {
    return;
  }
  if (g_order_len >= MAX_ORDER) {
    return;
  }

  ensure_peer(mac);
  const uint8_t place = (uint8_t)(g_order_len + 1);
  g_order[g_order_len++] = m.player_id;

  /* ACK with place so player can show first vs late LED */
  send_msg(mac, MSG_ACK, m.player_id, place);

  cdc_printf("BUZZ order=%u id=%u round=%u\n", place, m.player_id, g_round);

  if (place == 1) {
    g_winner = m.player_id;
    g_phase = PHASE_LOCKED;
    broadcast(MSG_LOCKOUT, g_winner, g_round);
    cdc_printf("LOCKOUT winner=%u round=%u\n", g_winner, g_round);
  }
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

  if (m.type == MSG_BUZZ) {
    handle_buzz(mac, m);
  } else if (m.type == MSG_HEARTBEAT) {
    cdc_printf("HB id=%u\n", m.player_id);
  }
}

static bool read_raw(uint8_t pin) {
#if BUTTON_ACTIVE_LOW
  return digitalRead(pin) == LOW;
#else
  return digitalRead(pin) == HIGH;
#endif
}

enum BtnEvent : uint8_t { BTN_NONE = 0, BTN_SHORT, BTN_LONG };

static BtnEvent poll_btn(Btn &b) {
  BtnEvent ev = BTN_NONE;
  const bool raw = read_raw(b.pin);
  const uint32_t now = millis();

  if (raw != b.last_raw) {
    b.last_raw = raw;
    b.last_change = now;
  }
  if ((now - b.last_change) < DEBOUNCE_MS) {
    return BTN_NONE;
  }

  if (raw && !b.stable) {
    b.stable = true;
    b.pressed = true;
    b.long_fired = false;
    b.press_start = now;
  } else if (raw && b.stable && b.pressed && !b.long_fired) {
    if ((now - b.press_start) >= LONG_PRESS_MS) {
      b.long_fired = true;
      ev = BTN_LONG;
    }
  } else if (!raw && b.stable) {
    b.stable = false;
    if (b.pressed && !b.long_fired) {
      ev = BTN_SHORT;
    }
    b.pressed = false;
  }
  return ev;
}

static void handle_arm_button(BtnEvent ev) {
  if (ev == BTN_NONE) {
    return;
  }
#if USE_DEDICATED_HOST_BTN
  if (ev == BTN_SHORT) {
    if (g_phase == PHASE_IDLE) {
      do_arm();
    } else {
      do_reset();
      do_arm();
    }
  } else if (ev == BTN_LONG) {
    do_disarm();
  }
#else
  /* Single button: short = arm/reset cycle, long = host key */
  if (ev == BTN_SHORT) {
    if (g_phase == PHASE_IDLE) {
      do_arm();
    } else {
      do_reset();
      do_arm();
    }
  } else if (ev == BTN_LONG) {
    inject_host_key();
  }
#endif
}

void setup() {
  /* TinyUSB CDC + HID (USB_MODE=0) */
  Keyboard.begin();
  USB.begin();
  Serial.begin(115200);
  delay(400);

  pinMode(PIN_BTN_ARM, INPUT_PULLUP);
#if USE_DEDICATED_HOST_BTN
  pinMode(PIN_BTN_HOST, INPUT_PULLUP);
#endif
  pinMode(PIN_LED, OUTPUT);
#if USE_ONBOARD_LED
  pinMode(PIN_LED_ONBOARD, OUTPUT);
#endif
  set_leds(false);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  esp_wifi_set_channel(WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);
  delay(50);

  if (esp_now_init() != ESP_OK) {
    cdc_printf("ESP-NOW init failed\n");
    while (true) {
      delay(1000);
    }
  }
  esp_now_register_recv_cb(on_data_recv);

  uint8_t bcast[6];
  memset(bcast, 0xFF, 6);
  ensure_peer(bcast);

  uint8_t mac[6];
  WiFi.macAddress(mac);
  cdc_printf("Jeopardy base ready\n");
  cdc_printf("BASE_MAC %02X:%02X:%02X:%02X:%02X:%02X\n", mac[0], mac[1], mac[2],
             mac[3], mac[4], mac[5]);
  cdc_printf("HOST_KEY=%d USE_DEDICATED_HOST_BTN=%d\n", (int)HOST_KEY,
             USE_DEDICATED_HOST_BTN);
}

void loop() {
  handle_rx();

  BtnEvent arm_ev = poll_btn(g_btn_arm);
  handle_arm_button(arm_ev);

#if USE_DEDICATED_HOST_BTN
  BtnEvent host_ev = poll_btn(g_btn_host);
  if (host_ev == BTN_SHORT || host_ev == BTN_LONG) {
    inject_host_key();
  }
#endif

  update_status_led();
  delay(1);
}
