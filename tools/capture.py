#!/usr/bin/env python3
"""capture.py, log the Gaitway hub USB serial CSV stream to a file.

Every demo session is automatically a dataset (spec 6.3). This reads the hub's
serial output line by line and writes it verbatim (config echo comments included)
to a timestamped CSV, while echoing to the screen so you can watch it live.

    python tools/capture.py --port COM7
    python tools/capture.py --port COM7 --out logs/walk_test.csv

Needs pyserial: pip install pyserial. No em dashes.
"""
import argparse
import datetime
import os
import sys

try:
    import serial  # pyserial
except ImportError:
    print("ERROR: pyserial not installed. Run: pip install pyserial")
    sys.exit(2)


def main():
    ap = argparse.ArgumentParser(description="Capture Gaitway hub serial to CSV.")
    ap.add_argument("--port", required=True, help="serial port, e.g. COM7 or /dev/ttyUSB0")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--out", default=None, help="output CSV path (default: timestamped)")
    args = ap.parse_args()

    out = args.out
    if out is None:
        stamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
        os.makedirs("logs", exist_ok=True)
        out = os.path.join("logs", f"session_{stamp}.csv")
    os.makedirs(os.path.dirname(os.path.abspath(out)), exist_ok=True)

    print(f"# capturing {args.port} @ {args.baud} -> {out}")
    print("# Ctrl-C to stop")
    ser = serial.Serial(args.port, args.baud, timeout=1)
    lines = 0
    try:
        with open(out, "w", newline="") as f:
            f.write(f"# capture start {datetime.datetime.now().isoformat()}\n")
            f.write(f"# source {args.port} @ {args.baud}\n")
            while True:
                raw = ser.readline()
                if not raw:
                    continue
                text = raw.decode("utf-8", errors="replace").rstrip("\r\n")
                f.write(text + "\n")
                f.flush()
                lines += 1
                print(text)
    except KeyboardInterrupt:
        print(f"\n# stopped, {lines} lines written to {out}")
    finally:
        ser.close()


if __name__ == "__main__":
    main()
