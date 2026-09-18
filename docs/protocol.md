# ESP-NOW Protocol

Shared header: `firmware/common/protocol.h`.

## Transport

- **PHY:** ESP-NOW over Wi-Fi (STA mode), unencrypted for simplicity.
- **Channel:** Base sets Wi-Fi channel 1 (`WIFI_CHANNEL` in base `config.h`). Players use channel 0 in peer info (follow AP/current channel); for best reliability keep all units powered in the same RF environment. If buzzes are flaky across distance, set the same fixed channel on players via `esp_wifi_set_channel` (future TODO).
- **Addressing:**
  - Base **broadcasts** `MSG_ARM`, `MSG_DISARM`, `MSG_LOCKOUT`, `MSG_RESET` to `FF:FF:FF:FF:FF:FF`.
  - Players send `MSG_BUZZ` **unicast** to the learned base MAC (or broadcast if base not yet known).
  - Base replies `MSG_ACK` unicast to the buzzing player.

## Wire format

```c
typedef struct __attribute__((packed)) {
  uint8_t  magic;         // 0x4A ('J')
  uint8_t  version;       // 1
  uint8_t  type;          // JeopardyMsgType
  uint8_t  player_id;     // 0 = base/broadcast, 1..8 contestant
  uint16_t seq;           // round id OR buzz place
  uint32_t timestamp_ms;  // sender millis()
} JeopardyMsg;            // 10 bytes
```

Receivers **must** check `magic == 0x4A` and `version == 1`.

## Message types

| Type | Value | Direction | Meaning |
|------|-------|-----------|---------|
| `MSG_ARM` | 1 | Base → all | Accept presses; `seq` = round id |
| `MSG_DISARM` | 2 | Base → all | Ignore presses |
| `MSG_BUZZ` | 3 | Player → base | First press after ARM; `player_id` set |
| `MSG_LOCKOUT` | 4 | Base → all | Round locked; `player_id` = winner |
| `MSG_RESET` | 5 | Base → all | Clear round → idle |
| `MSG_ACK` | 6 | Base → player | Buzz accepted; `seq` = place (1=first) |
| `MSG_HEARTBEAT` | 7 | Either | Optional; unused in default firmware |

## Round flow

1. Host arms base → `MSG_ARM` (round++).
2. Players enter ARMED (LED blink); ignore until this.
3. First player press → `MSG_BUZZ`.
4. Base appends to order, sends `MSG_ACK` with `seq=1`, broadcasts `MSG_LOCKOUT`.
5. Later `MSG_BUZZ` still recorded and ACK'd with place 2, 3, … until RESET/DISARM.
6. Players that never pressed go LOCKED_OUT on `MSG_LOCKOUT`; winner stays solid LED; late buzzers double-blink after ACK place &gt; 1.

## Pairing

**Default (players):** `PAIR_LEARN_FROM_ARM` — remember the MAC of the first trusted `MSG_ARM` / control message and unicast BUZZ there.

**Fixed MAC:** set `PAIR_LEARN_FROM_ARM` to `0` and define `BASE_MAC_0` … `BASE_MAC_5` in player `config.h` (or build flags). Print base MAC from CDC boot line `BASE_MAC aa:bb:...`.

Player ID is compile-time: `-DPLAYER_ID=n` or edit `config.h`.

## CDC status lines (base → host)

Examples printed over USB CDC (115200):

```
ARM round=3
BUZZ order=1 id=2 round=3
LOCKOUT winner=2 round=3
BUZZ order=2 id=5 round=3
RESET round=3
HOST_KEY
```

Parse with `host/serial_monitor.py`. Keystrokes are **not** sent over serial — they are USB HID from the base.
