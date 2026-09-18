# Jeopardy Wireless Buzzer

ESP-NOW wireless buzzers for Jeopardy-style games using **Seeed Studio XIAO ESP32-S3**.

- **Contestants:** button + LED + battery; ignore presses until the base ARMs the round; first press sends `BUZZ` and locks the LED.
- **Base:** USB to a Linux (or any) PC; broadcasts ARM / DISARM / LOCKOUT / RESET; records **buzz-in order** (1st, 2nd, 3rd…); first buzz locks the round for winner feedback while later buzzes are still logged.
- **Host keystrokes:** TinyUSB **HID keyboard** from the base (no daemon). Optional `host/serial_monitor.py` prints CDC status lines (`BUZZ order=…`).

Repository: https://github.com/thematthewknot/jeopardy-buzzer

## Quick start

### 1. Hardware

See [docs/hardware.md](docs/hardware.md) for BOM, pin map, wiring, battery, and enclosure notes.

Suggested pins:

| Role | Silk | GPIO |
|------|------|------|
| LED | D0 | 1 |
| Buzz / ARM button | D1 | 2 |
| Host button (base) | D2 | 3 |
| Onboard USER LED | — | 21 (active LOW) |

### 2. Flash contestants

```bash
cd firmware/player
pio run -t upload                          # PLAYER_ID=1
# For each other unit, set -DPLAYER_ID=2 .. 8 (see BUILD_NOTES.md)
```

### 3. Flash base

```bash
cd firmware/base
pio run -t upload
```

Base overrides USB to TinyUSB (`ARDUINO_USB_MODE=0`) for HID+CDC. First upload may need BOOT-mode; details in [BUILD_NOTES.md](BUILD_NOTES.md).

### 4. Play

1. Plug base into the PC (it enumerates as a keyboard + serial port).
2. Power players (USB or LiPo).
3. **Short-press ARM** on the base → players blink (armed).
4. Contestants buzz; first gets solid LED; base prints order on CDC and broadcasts lockout.
5. **Short-press ARM** again → RESET then ARM for the next clue.
6. Press **HOST** button (or long-press ARM in single-button mode) to type the configured key (default **space**) into the focused host app.

Optional monitors:

- **Web UI (Web Serial):** open [`host/web/index.html`](host/web/index.html) in Chrome/Edge via a local server (see that page’s hint), click **Connect base**, pick the XIAO CDC port.
- **CLI:**

```bash
cd host && pip install -r requirements.txt
python3 serial_monitor.py
```

## Layout

```
jeopardy-buzzer/
  README.md
  BUILD_NOTES.md
  .gitignore
  docs/
    hardware.md
    protocol.md
  firmware/
    common/protocol.h
    player/          # PlatformIO — contestant
    base/            # PlatformIO — base + HID
  host/
    serial_monitor.py
    requirements.txt
    web/index.html   # Web Serial buzz-order UI
```

## Protocol

Packed ESP-NOW structs: [docs/protocol.md](docs/protocol.md), `firmware/common/protocol.h`.

Messages: `MSG_ARM`, `MSG_DISARM`, `MSG_BUZZ`, `MSG_LOCKOUT`, `MSG_RESET`, `MSG_ACK`, optional `MSG_HEARTBEAT`.

**Pairing:** players learn the base MAC from the first `MSG_ARM` (default), or use a compile-time `BASE_MAC_*` (see player `include/config.h`).

## Button behavior (base)

| Mode | Short ARM | Long ARM | HOST button |
|------|-----------|----------|-------------|
| Dual (`USE_DEDICATED_HOST_BTN=1`) | ARM or RESET+ARM | DISARM | Inject `HOST_KEY` |
| Single (`=0`) | ARM or RESET+ARM | Inject `HOST_KEY` | n/a |

Debounce: 40 ms. `HOST_KEY` default: space — change in `firmware/base/include/config.h`.

## License

Use freely for your game night. No warranty.
