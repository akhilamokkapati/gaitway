#!/usr/bin/env python3
"""plot_session.py, four-panel figure from a Gaitway session CSV.

Panels (spec 6.3, these go straight into the Phase B report):
  1. angles over time (waist pitch, waist roll, right thigh pitch)
  2. right thigh rate with the SWING_RATE_ON/OFF thresholds drawn
  3. step events as markers
  4. walk and gvs state as shaded bands

Thresholds are read from the '#' config echo lines in the CSV when present, so
each figure is annotated with the exact config that produced it. Falls back to
CLI values otherwise.

    python tools/plot_session.py logs/session.csv
    python tools/plot_session.py logs/session.csv --out figs/session.png

Needs matplotlib: pip install matplotlib. No em dashes.
"""
import argparse
import csv
import sys

try:
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from matplotlib.patches import Patch
except ImportError:
    print("ERROR: matplotlib not installed. Run: pip install matplotlib")
    sys.exit(2)


def parse_csv(path, on_default, off_default):
    on, off = on_default, off_default
    rows = []
    header = None
    with open(path) as f:
        for line in f:
            line = line.rstrip("\n")
            if line.startswith("#"):
                if "SWING_RATE_ON=" in line:
                    for tok in line.replace("#", " ").split():
                        if tok.startswith("SWING_RATE_ON="):
                            on = float(tok.split("=")[1])
                        elif tok.startswith("SWING_RATE_OFF="):
                            off = float(tok.split("=")[1])
                continue
            if header is None:
                header = line.split(",")
                continue
            parts = line.split(",")
            if len(parts) != len(header):
                continue
            rows.append(dict(zip(header, parts)))
    return header, rows, on, off


def fnum(row, key):
    v = row.get(key, "")
    try:
        return float(v)
    except ValueError:
        return float("nan")


def state_spans(rows, t, key, active_values):
    """Yield (t_start, t_end, value) spans where column key holds an active value."""
    spans = []
    cur = None
    start = None
    for i, r in enumerate(rows):
        v = r.get(key, "")
        active = v in active_values
        if active and cur is None:
            cur, start = v, t[i]
        elif cur is not None and (not active or v != cur):
            spans.append((start, t[i], cur))
            cur = v if active else None
            start = t[i] if active else None
    if cur is not None:
        spans.append((start, t[-1], cur))
    return spans


def main():
    ap = argparse.ArgumentParser(description="Plot a Gaitway session CSV.")
    ap.add_argument("csv")
    ap.add_argument("--out", default=None, help="output PNG (default: <csv>.png)")
    ap.add_argument("--on", type=float, default=40.0, help="fallback SWING_RATE_ON")
    ap.add_argument("--off", type=float, default=15.0, help="fallback SWING_RATE_OFF")
    args = ap.parse_args()

    header, rows, on, off = parse_csv(args.csv, args.on, args.off)
    if not rows:
        print("ERROR: no data rows found in", args.csv)
        sys.exit(1)

    t = [fnum(r, "millis") / 1000.0 for r in rows]  # seconds
    wp = [fnum(r, "waist_pitch") for r in rows]
    wr = [fnum(r, "waist_roll") for r in rows]
    rp = [fnum(r, "rt_pitch") for r in rows]
    rr = [fnum(r, "rt_rate") for r in rows]
    steps = [t[i] for i, r in enumerate(rows) if r.get("step_evt") == "1"]

    fig, ax = plt.subplots(4, 1, figsize=(11, 10), sharex=True)

    ax[0].plot(t, wp, label="waist pitch", lw=1)
    ax[0].plot(t, wr, label="waist roll", lw=1)
    ax[0].plot(t, rp, label="right thigh pitch", lw=1)
    ax[0].set_ylabel("angle (deg)")
    ax[0].set_title("Gaitway session: angles, rate, steps, state")
    ax[0].legend(loc="upper right", fontsize=8)
    ax[0].grid(True, alpha=0.3)

    ax[1].plot(t, rr, color="tab:red", lw=1, label="rt_rate")
    for lvl, name in [(on, "ON"), (-on, None), (off, "OFF"), (-off, None)]:
        ax[1].axhline(lvl, color="gray", ls="--", lw=0.8)
    ax[1].axhline(on, color="gray", ls="--", lw=0.8, label=f"ON={on:g}")
    ax[1].axhline(off, color="silver", ls=":", lw=0.8, label=f"OFF={off:g}")
    ax[1].set_ylabel("rt rate (deg/s)")
    ax[1].legend(loc="upper right", fontsize=8)
    ax[1].grid(True, alpha=0.3)

    ax[2].plot(t, rp, color="tab:blue", lw=0.8, alpha=0.5, label="rt pitch")
    for s in steps:
        ax[2].axvline(s, color="tab:green", lw=1.2, alpha=0.8)
    ax[2].plot([], [], color="tab:green", label=f"STEP (n={len(steps)})")
    ax[2].set_ylabel("angle (deg)")
    ax[2].legend(loc="upper right", fontsize=8)
    ax[2].grid(True, alpha=0.3)

    walk_spans = state_spans(rows, t, "walk_state", {"WALK"})
    gvs_spans = state_spans(rows, t, "gvs_state", {"LEFT", "RIGHT"})
    for (a, b, _) in walk_spans:
        ax[3].axvspan(a, b, ymin=0.55, ymax=0.95, color="tab:orange", alpha=0.5)
    for (a, b, v) in gvs_spans:
        color = "tab:purple" if v == "LEFT" else "tab:cyan"
        ax[3].axvspan(a, b, ymin=0.05, ymax=0.45, color=color, alpha=0.5)
    ax[3].set_ylim(0, 1)
    ax[3].set_yticks([])
    ax[3].set_ylabel("state")
    ax[3].set_xlabel("time (s)")
    ax[3].legend(handles=[
        Patch(color="tab:orange", alpha=0.5, label="WALK"),
        Patch(color="tab:purple", alpha=0.5, label="GVS LEFT"),
        Patch(color="tab:cyan", alpha=0.5, label="GVS RIGHT"),
    ], loc="upper right", fontsize=8, ncol=3)

    fig.tight_layout()
    out = args.out or (args.csv.rsplit(".", 1)[0] + ".png")
    fig.savefig(out, dpi=130)
    print(f"wrote {out}  ({len(rows)} rows, {len(steps)} steps, ON={on:g} OFF={off:g})")


if __name__ == "__main__":
    main()
