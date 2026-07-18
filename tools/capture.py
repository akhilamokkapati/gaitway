#!/usr/bin/env python3
"""capture.py, log the Gaitway hub CSV stream to a file, over USB or WiFi.

Every demo session is automatically a dataset (spec 6.3). Two modes:

  USB (tethered, for tuning and clean data):
      python tools/capture.py --port COM8

  WiFi (untethered, for the demo): join the hub's SoftAP first (SSID
  Gaitway-Hub), then:
      python tools/capture.py --udp
  In UDP mode you can type a line and press Enter to send it to the hub, e.g.
  type  c  to calibrate wirelessly.

Needs pyserial for USB mode: pip install pyserial. No em dashes.
"""
import argparse
import datetime
import os
import socket
import sys
import threading


def out_path(arg):
    if arg:
        os.makedirs(os.path.dirname(os.path.abspath(arg)), exist_ok=True)
        return arg
    stamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
    os.makedirs("logs", exist_ok=True)
    return os.path.join("logs", f"session_{stamp}.csv")


def run_serial(args):
    try:
        import serial  # pyserial
    except ImportError:
        print("ERROR: pyserial not installed. Run: pip install pyserial")
        sys.exit(2)
    out = out_path(args.out)
    print(f"# capturing {args.port} @ {args.baud} -> {out}   (Ctrl-C to stop)")
    ser = serial.Serial(args.port, args.baud, timeout=1)
    lines = 0
    try:
        with open(out, "w", newline="") as f:
            f.write(f"# capture start {datetime.datetime.now().isoformat()}\n")
            while True:
                raw = ser.readline()
                if not raw:
                    continue
                text = raw.decode("utf-8", errors="replace").rstrip("\r\n")
                f.write(text + "\n"); f.flush()
                lines += 1
                print(text)
    except KeyboardInterrupt:
        print(f"\n# stopped, {lines} lines -> {out}")
    finally:
        ser.close()


def run_udp(args):
    out = out_path(args.out)
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.bind(("", args.udp_port))
    print(f"# listening for the hub on UDP {args.udp_port} -> {out}")
    print(f"# join the '{args.ssid}' WiFi first. Type a line + Enter to send it "
          f"to the hub (e.g. 'c' to calibrate). Ctrl-C to stop.")

    def sender():
        for line in sys.stdin:
            cmd = line.strip()
            if cmd:
                sock.sendto(cmd.encode(), (args.hub_ip, args.udp_port))
                print(f"# sent '{cmd}' -> {args.hub_ip}:{args.udp_port}")
    threading.Thread(target=sender, daemon=True).start()

    lines = 0
    try:
        with open(out, "w", newline="") as f:
            f.write(f"# capture start {datetime.datetime.now().isoformat()} (WiFi)\n")
            while True:
                data, _ = sock.recvfrom(2048)
                text = data.decode("utf-8", errors="replace").rstrip("\r\n")
                f.write(text + "\n"); f.flush()
                lines += 1
                print(text)
    except KeyboardInterrupt:
        print(f"\n# stopped, {lines} lines -> {out}")
    finally:
        sock.close()


def main():
    ap = argparse.ArgumentParser(description="Capture Gaitway hub CSV (USB or WiFi).")
    ap.add_argument("--port", help="serial port for USB mode, e.g. COM8")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--udp", action="store_true", help="WiFi mode: receive UDP from the hub AP")
    ap.add_argument("--udp-port", type=int, default=4210)
    ap.add_argument("--hub-ip", default="192.168.4.1", help="hub SoftAP IP")
    ap.add_argument("--ssid", default="Gaitway-Hub")
    ap.add_argument("--out", default=None, help="output CSV path (default: timestamped)")
    args = ap.parse_args()

    if args.udp:
        run_udp(args)
    elif args.port:
        run_serial(args)
    else:
        print("ERROR: give --port COMx (USB) or --udp (WiFi).")
        sys.exit(2)


if __name__ == "__main__":
    main()
