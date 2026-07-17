#!/usr/bin/env python3
"""replay_reference.py, Gate 1 host test for Gaitway step detection.

Compiles the SHIPPED C++ detector (firmware/hub/src/detection.cpp) with a small
host harness and drives it with the healthy reference gait cycle, repeated N
times, plus a noisy standing trace. Asserts:

  1. exactly one STEP per gait cycle, all in the FORWARD direction
  2. zero STEPs on a flat standing line with 0.5 deg gaussian noise

Compiling the real C++ (not a python reimplementation) means the tested code is
the code that ships on the hub. No em dashes anywhere.

Usage:
    python tools/replay_reference.py            run the gate test
    python tools/replay_reference.py --sweep    scan thresholds (tuning aid)
"""
import csv
import os
import random
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
BUILD = os.path.join(HERE, "build")
REF_CSV = os.path.join(ROOT, "reference_gait_cycle_100hz.csv")

# Reference subject is a fast healthy walker (swing peak ~183 deg/s), so its
# SWING_RATE_ON sits well above its stance phase velocity. Slow stroke patients
# use the lower config.h default, tuned per subject from logged data.
REF = dict(on=90.0, off=15.0, t_confirm=30, t_refract=250, alpha=0.4, walk=1500)
# Noise test uses the patient config.h defaults, the worst case for false steps.
NOISE = dict(on=40.0, off=15.0, t_confirm=30, t_refract=250, alpha=0.4, walk=1500)

N_CYCLES = 3
SAMPLE_MS = 10          # 100 Hz
NOISE_SECONDS = 3
NOISE_STDDEV = 0.5
NOISE_SEED = 2


def find_gpp():
    g = shutil.which("g++")
    if g:
        return g
    guess = (r"C:\Users\akhil\AppData\Local\Microsoft\WinGet\Packages"
             r"\BrechtSanders.WinLibs.POSIX.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe"
             r"\mingw64\bin\g++.exe")
    if os.path.exists(guess):
        return guess
    print("ERROR: g++ not found. Install a C++ compiler (WinLibs / MinGW).")
    sys.exit(2)


def compile_harness():
    os.makedirs(BUILD, exist_ok=True)
    exe = os.path.join(BUILD, "replay_harness.exe" if os.name == "nt" else "replay_harness")
    cmd = [
        find_gpp(), "-std=c++17", "-O2", "-Wall",
        "-I", os.path.join(ROOT, "firmware", "hub", "include"),
        os.path.join(HERE, "replay_harness.cpp"),
        os.path.join(ROOT, "firmware", "hub", "src", "detection.cpp"),
        "-o", exe,
    ]
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode != 0:
        print("COMPILE FAILED:\n" + r.stdout + r.stderr)
        sys.exit(2)
    return exe


def load_reference_cycle():
    rows = []
    with open(REF_CSV) as f:
        for r in csv.DictReader(f):
            rows.append(float(r["hip_angle_deg"]))
    # Drop the final sample, it duplicates the first (cycle wraps cleanly).
    return rows[:-1]


def reference_stream(n_cycles):
    cycle = load_reference_cycle()
    period_ms = len(cycle) * SAMPLE_MS + SAMPLE_MS  # one extra step wraps to start
    lines = []
    for c in range(n_cycles):
        for i, ang in enumerate(cycle):
            lines.append(f"{c * period_ms + i * SAMPLE_MS} {ang:.4f}")
    return "\n".join(lines) + "\n"


def noise_stream():
    random.seed(NOISE_SEED)
    n = NOISE_SECONDS * 100
    lines = []
    for i in range(n):
        ang = 10.0 + random.gauss(0.0, NOISE_STDDEV)
        lines.append(f"{i * SAMPLE_MS} {ang:.4f}")
    return "\n".join(lines) + "\n"


def run(exe, cfg, stream):
    args = [exe, str(cfg["on"]), str(cfg["off"]), str(cfg["t_confirm"]),
            str(cfg["t_refract"]), str(cfg["alpha"]), str(cfg["walk"])]
    r = subprocess.run(args, input=stream, capture_output=True, text=True)
    steps = []
    walk = None
    for line in r.stdout.splitlines():
        p = line.split()
        if p and p[0] == "STEP":
            steps.append((int(p[1]), p[2]))
        elif p and p[0] == "WALK":
            walk = p[2]
    return steps, walk


def sweep(exe):
    print("ON sweep on reference (alpha=%.2f), want %d FWD steps:" % (REF["alpha"], N_CYCLES))
    ref = reference_stream(N_CYCLES)
    for on in [40, 60, 70, 80, 90, 100, 110, 120]:
        cfg = dict(REF); cfg["on"] = float(on)
        steps, _ = run(exe, cfg, ref)
        dirs = ",".join(d for _, d in steps)
        print(f"  ON={on:3d} -> {len(steps)} steps [{dirs}]")
    print("alpha sweep on noisy standing (ON=40), want 0 steps:")
    noise = noise_stream()
    for a in [1.0, 0.6, 0.5, 0.4, 0.3, 0.2]:
        cfg = dict(NOISE); cfg["alpha"] = a
        steps, _ = run(exe, cfg, noise)
        print(f"  alpha={a:.1f} -> {len(steps)} false steps")


def main():
    exe = compile_harness()
    if "--sweep" in sys.argv:
        sweep(exe)
        return

    ok = True

    ref_steps, ref_walk = run(exe, REF, reference_stream(N_CYCLES))
    fwd = sum(1 for _, d in ref_steps if d == "FWD")
    print(f"[reference] {N_CYCLES} cycles -> {len(ref_steps)} steps, {fwd} FWD, "
          f"walk={ref_walk}")
    for t, d in ref_steps:
        print(f"           STEP @ {t} ms  {d}")
    if len(ref_steps) != N_CYCLES or fwd != N_CYCLES:
        print(f"  FAIL: expected exactly {N_CYCLES} FWD steps")
        ok = False
    else:
        print("  PASS: one FWD step per cycle")

    noise_steps, _ = run(exe, NOISE, noise_stream())
    print(f"[noise] {NOISE_SECONDS}s standing, {NOISE_STDDEV} deg noise -> "
          f"{len(noise_steps)} steps")
    if len(noise_steps) != 0:
        print("  FAIL: expected 0 steps on noisy standing")
        ok = False
    else:
        print("  PASS: zero false steps on noisy standing")

    print("\nGATE 1 REPLAY:", "PASS" if ok else "FAIL")
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
