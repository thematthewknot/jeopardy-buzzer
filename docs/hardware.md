# Hardware — Jeopardy Wireless Buzzers

## BOM (per contestant)

| Item | Qty | Notes |
|------|-----|--------|
| Seeed Studio XIAO ESP32-S3 | 1 | Not Sense (unless you want the extra board) |
| Momentary pushbutton | 1 | SPST, arcade or tactile |
| LED + series resistor | 1 | e.g. 5 mm LED + 220 Ω (3V3) |
| LiPo 3.7 V rechargeable | 1 | 200–500 mAh typical; JST or solder to BAT pads |
| Enclosure | 1 | 3D-print or project box; leave USB/antenna access |
| Hookup wire / protoboard | — | Optional |
| U.FL antenna | 1 | Included with XIAO; attach for range |

## BOM (base station)

| Item | Qty | Notes |
|------|-----|--------|
| Seeed Studio XIAO ESP32-S3 | 1 | USB to host PC |
| Momentary button (ARM) | 1 | Required |
| Momentary button (HOST key) | 0–1 | Optional; long-press on ARM if omitted |
| LED + resistor | 1 | Status (armed / locked) |
| USB-C cable (data) | 1 | Must support data, not charge-only |

## XIAO ESP32-S3 pin map (D0–D10)

| Silk | GPIO | Suggested use (this project) |
|------|------|------------------------------|
| D0 | GPIO1 | External status LED (active HIGH) |
| D1 | GPIO2 | Button (active LOW, INPUT_PULLUP) |
| D2 | GPIO3 | Base only: host key button |
| D3 | GPIO4 | Free |
| D4 | GPIO5 | Free (SDA) |
| D5 | GPIO6 | Free (SCL) |
| D6 | GPIO43 | Free (TX) |
| D7 | GPIO44 | Free (RX) |
| D8 | GPIO7 | Free |
| D9 | GPIO8 | Free |
| D10 | GPIO9 | Free |
| USER_LED | GPIO21 | Onboard orange LED (**active LOW**) |
| BAT+ / BAT− | — | Rear pads for LiPo |
| 3V3 / GND / 5V | — | Power |

Official pinout: [Seeed XIAO ESP32-S3 wiki](https://wiki.seeedstudio.com/xiao_esp32s3_getting_started/).

## Wiring — contestant

```
  3V3 ── LED anode ──(220Ω)── LED cathode ── D0 (GPIO1)
  OR (common cathode): D0 ── 220Ω ── LED anode; cathode ── GND
  (Firmware drives D0 HIGH = on.)

  Button: D1 (GPIO2) ── button ── GND
          (internal pull-up enabled)

  Battery: LiPo + → BAT+ pad, LiPo − → BAT− pad (closest to USB is −)
  Antenna: snap U.FL antenna onto connector before first RF use
```

Onboard USER_LED (GPIO21) is mirrored in firmware for bench testing without an external LED.

## Wiring — base

```
  ARM button:  D1 (GPIO2) ── button ── GND
  HOST button: D2 (GPIO3) ── button ── GND   (optional)
  Status LED:  same as contestant on D0
  USB-C → Linux PC (HID keyboard + CDC serial)
```

### Single-button mode

In `firmware/base/include/config.h` set `USE_DEDICATED_HOST_BTN` to `0`:

- **Short press** — ARM (or RESET then ARM if a round is active)
- **Long press** (~700 ms) — inject host keystroke (`HOST_KEY`, default space)

With dedicated host button (`USE_DEDICATED_HOST_BTN 1`):

- ARM short = arm/reset-arm; ARM long = DISARM
- HOST short or long = keystroke

## Battery notes

- XIAO ESP32-S3 has onboard charge management; charge via USB-C while LiPo is attached.
- Charge LED (red): blinks while charging; off when full (see Seeed wiki).
- Under battery power, the **5V pin is not powered**; use 3V3 for peripherals.
- There is **no ADC battery-sense pin** by default; add a divider to a free ADC GPIO if you need SOC.
- Deep sleep is not implemented in this firmware (players stay awake for low latency). Typical draw with Wi-Fi/ESP-NOW active is tens of mA — size the pack for your session length.
- Observe polarity carefully on BAT pads; reverse polarity can damage the board.

## Enclosure notes

- Contestant: large top button accessible; LED visible; USB-C accessible for charging; leave clearance for U.FL pigtail.
- Avoid fully metal enclosures (RF shielding). Plastic or wood preferred.
- Base: keep USB free; mount ARM where the host can reach without looking away from the board.
- Label each player enclosure with its `PLAYER_ID` (1–8).

## PlatformIO board

- **Board ID:** `seeed_xiao_esp32s3`
- Flash 8 MB, PSRAM present; upload ~460800 baud.
