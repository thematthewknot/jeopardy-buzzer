# Build notes

## Board ID chosen

- **PlatformIO:** `seeed_xiao_esp32s3` (`platform = espressif32`)
- Confirmed present in PlatformIO espressif32 boards package.
- Fallback if your PIO is ancient: `esp32-s3-devkitc-1` with upload tweaks — **not required** when `seeed_xiao_esp32s3` is available.

## USB modes

| Firmware | `ARDUINO_USB_MODE` | Why |
|----------|--------------------|-----|
| Player | Board default (`1`) | Native USB Serial/JTAG CDC for debug |
| Base | Forced `0` via `build_unflags`/`build_flags` | TinyUSB SoftUSB for **HID keyboard + CDC** |

Flashing the base the first time after switching USB mode may require **BOOT held** while plugging USB, then upload. After that, use the ACM/CDC port.

## Build commands

```bash
# Player id=1 (default)
cd firmware/player && pio run -t upload

# Player id=3
cd firmware/player && pio run -e player --project-option "build_flags=-DPLAYER_ID=3 -I../common -Iinclude" -t upload
# or edit include/config.h / platformio.ini build_flags

# Base
cd firmware/base && pio run -t upload

# Optional host monitor
cd host && pip install -r requirements.txt
python3 serial_monitor.py
```

Shared protocol lives in `firmware/common/protocol.h` (included via `-I../common`).

## TODOs / known limitations

1. **Wi-Fi channel lock on players** — base forces channel 1; players currently follow default. If multi-room RF is noisy, add matching `esp_wifi_set_channel` on players.
2. **No encryption** on ESP-NOW — fine for living-room Jeopardy; add LMK/PMK if you need it.
3. **No deep sleep** on players — battery life is session-oriented.
4. **HOST_KEY** defaults to space; set `-DHOST_KEY=KEY_F1` or edit `config.h` for your game software.
5. **Duplicate BUZZ** from same id in one round is ignored; no explicit retransmit if ACK lost (player already shows LOCKED_IN optimistically).
6. First flash of base after enabling TinyUSB may need BOOT button held.
