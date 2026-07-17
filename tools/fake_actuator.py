#!/usr/bin/env python3
"""fake_actuator.py, stand-in for Joseph's actuation MCU.

Listens on a serial port and prints the GW1 command lines the hub sends, with
timestamps and inter-line spacing. Flags the two things that matter for the
contract (spec 6.1):
  - spacing under 100 ms  (the hub should never flood, v1 CMD_GAP rule)
  - a gap over 2 s        (the motor side treats this as a lost link and stops)

This is an independent test harness: Joseph can run it with a USB-UART dongle on
the hub's GW1 TX pin, no motor required. It also works for the GV1 stream with
--expect GV1. Needs pyserial: pip install pyserial. No em dashes.
"""
import argparse
import sys
import time

try:
    import serial  # pyserial
except ImportError:
    print("ERROR: pyserial not installed. Run: pip install pyserial")
    sys.exit(2)

MIN_GAP_S = 0.100
LOST_LINK_S = 2.0


def main():
    ap = argparse.ArgumentParser(description="Watch a Gaitway command stream.")
    ap.add_argument("--port", required=True, help="serial port, e.g. COM8 or /dev/ttyUSB0")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--expect", default="GW1", help="expected line prefix (GW1 or GV1)")
    args = ap.parse_args()

    ser = serial.Serial(args.port, args.baud, timeout=0.25)
    print(f"# listening on {args.port} @ {args.baud}, expecting {args.expect},* lines")
    print(f"# flags: spacing < {int(MIN_GAP_S*1000)} ms, gap > {LOST_LINK_S:g} s")
    print("# Ctrl-C to stop\n")

    t0 = time.monotonic()
    last = None
    count = 0
    warned_silence = False
    spacings = []
    try:
        while True:
            raw = ser.readline()
            now = time.monotonic()
            if not raw:
                # No line this cycle. Warn once if the link would be considered lost.
                if last is not None and (now - last) > LOST_LINK_S and not warned_silence:
                    print(f"[{now-t0:8.3f}s] WARN silence {now-last:.2f}s > {LOST_LINK_S:g}s, "
                          f"motor side would STOP")
                    warned_silence = True
                continue

            line = raw.decode("utf-8", errors="replace").strip()
            if not line:
                continue
            count += 1
            warned_silence = False
            gap = None if last is None else now - last
            last = now

            flags = []
            if gap is not None:
                spacings.append(gap)
                if gap < MIN_GAP_S:
                    flags.append(f"SPACING {gap*1000:.0f}ms < {int(MIN_GAP_S*1000)}ms (flooding)")
                if gap > LOST_LINK_S:
                    flags.append(f"GAP {gap:.2f}s > {LOST_LINK_S:g}s (lost link)")
            if not line.startswith(args.expect + ",") and line != args.expect:
                flags.append(f"unexpected prefix (want {args.expect})")

            gap_txt = "   first" if gap is None else f"+{gap*1000:7.1f}ms"
            tag = ("  <-- " + "; ".join(flags)) if flags else ""
            print(f"[{now-t0:8.3f}s] ({gap_txt}) {line}{tag}")
    except KeyboardInterrupt:
        print(f"\n# stopped: {count} lines")
        if spacings:
            print(f"# spacing min {min(spacings)*1000:.0f}ms  max {max(spacings)*1000:.0f}ms  "
                  f"mean {sum(spacings)/len(spacings)*1000:.0f}ms")
    finally:
        ser.close()


if __name__ == "__main__":
    main()
