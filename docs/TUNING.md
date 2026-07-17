# Tuning the step detector

All values live in `firmware/hub/include/config.h`. This note explains how to
pick the two that matter most, using a `plot_session.py` figure or the host
replay. No em dashes.

## Background: why a bare velocity threshold is not enough

One healthy gait cycle contains TWO large thigh velocity excursions, not one:

- the forward swing, a sharp high velocity peak (reference subject: +183 deg/s),
- the stance phase, where the planted thigh rotates backward under the body
  (reference subject: down to -90 deg/s, sustained).

A plain `|rate| > SWING_RATE_ON` detector at a low threshold counts BOTH as
steps, giving two events per cycle (one FWD, one BWD). It also counts sensor
noise, because raw sample to sample differencing turns 0.5 deg of angle noise at
100 Hz into rate spikes near 150 deg/s.

The detector answers both problems:

1. `RATE_LP_ALPHA` low pass filters the rate before thresholding (kills noise).
2. One STEP is emitted per swing episode, direction from the net excursion sign.
3. `SWING_RATE_ON` must sit ABOVE the subject's stance velocity and BELOW the
   swing peak, so only the swing triggers.

## SWING_RATE_ON

Rule: put it between the stance phase peak and the swing peak for THIS wearer.

Host replay evidence (reference subject, alpha 0.4):

```
ON = 40..70  -> extra BWD steps from stance phase (too low)
ON = 80..120 -> exactly one FWD step per cycle (correct)
```

The reference subject is fast and healthy, so the replay test uses ON = 90. A
slow stroke patient's swing peak may be 3 to 5x lower (roughly 40 to 60 deg/s)
AND their stance velocity scales down too, so the same rule gives a lower ON.
That is why the `config.h` default is 40, a stroke starting guess, not 90.

How to set it on day 1:
1. Capture a standing then walking session (`capture.py`).
2. Plot it (`plot_session.py`), look at the `rt_rate` panel.
3. Read the swing peak height and the stance dip height.
4. Set `SWING_RATE_ON` about 1.3 to 1.5x the stance dip magnitude, comfortably
   below the swing peak. Set `SWING_RATE_OFF` to roughly a third of ON.

## RATE_LP_ALPHA

Lower alpha = more smoothing = fewer noise false positives, but too low adds lag
and can clip a fast swing. Host replay on 0.5 deg standing noise:

```
alpha = 1.0 -> 6 false steps (no filtering)
alpha = 0.6 -> 3 false steps
alpha = 0.3..0.5 -> 0 false steps
```

Default 0.4 sits in the middle of the clean band. Only lower it if a real sensor
shows more static noise than the 0.5 deg test case.

## Verify after any change

```
python tools/replay_reference.py
```

Must print `GATE 1 REPLAY: PASS` before flashing anything.
