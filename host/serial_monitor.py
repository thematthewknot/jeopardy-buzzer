#!/usr/bin/env python3
"""
Minimal CDC serial monitor for the Jeopardy base station.

Prints status lines from the XIAO (ARM, BUZZ order, LOCKOUT, etc.).
Keystrokes are injected by the base itself via USB HID — this script is
optional observability only.

Usage:
  python3 serial_monitor.py              # auto-detect /dev/ttyACM* or ttyUSB*
  python3 serial_monitor.py /dev/ttyACM0
  python3 serial_monitor.py --list

Requires: pyserial (see requirements.txt)
"""

from __future__ import annotations

import argparse
import sys
import time


def list_ports() -> list[str]:
    try:
        from serial.tools import list_ports
    except ImportError:
        print("pyserial not installed. pip install -r requirements.txt", file=sys.stderr)
        sys.exit(1)
    return [p.device for p in list_ports.comports()]


def pick_port(explicit: str | None) -> str:
    if explicit:
        return explicit
    ports = list_ports()
    prefer = [p for p in ports if "ACM" in p or "USB" in p or "usbmodem" in p]
    candidates = prefer or ports
    if not candidates:
        print("No serial ports found. Plug in the base over USB.", file=sys.stderr)
        sys.exit(1)
    if len(candidates) > 1:
        print("Multiple ports; using first. Pass an explicit path to override:", file=sys.stderr)
        for p in candidates:
            print(f"  {p}", file=sys.stderr)
    return candidates[0]


def main() -> int:
    ap = argparse.ArgumentParser(description="Jeopardy base CDC status monitor")
    ap.add_argument("port", nargs="?", help="Serial device (e.g. /dev/ttyACM0)")
    ap.add_argument("-b", "--baud", type=int, default=115200)
    ap.add_argument("--list", action="store_true", help="List serial ports and exit")
    args = ap.parse_args()

    if args.list:
        for p in list_ports():
            print(p)
        return 0

    try:
        import serial
    except ImportError:
        print("pyserial not installed. pip install -r requirements.txt", file=sys.stderr)
        return 1

    port = pick_port(args.port)
    print(f"Opening {port} @ {args.baud}", flush=True)

    while True:
        try:
            with serial.Serial(port, args.baud, timeout=0.5) as ser:
                # Give TinyUSB a moment after open
                time.sleep(0.2)
                while True:
                    line = ser.readline()
                    if not line:
                        continue
                    try:
                        text = line.decode("utf-8", errors="replace").rstrip("\r\n")
                    except Exception:
                        text = repr(line)
                    print(text, flush=True)
        except serial.SerialException as e:
            print(f"Serial error: {e}; retrying in 2s...", file=sys.stderr)
            time.sleep(2)
        except KeyboardInterrupt:
            print("\nBye.", file=sys.stderr)
            return 0


if __name__ == "__main__":
    sys.exit(main())
