# Hardware bring-up checklist

Gate by gate, what to wire and what to physically verify before moving on. All
firmware is written and its pure logic is host-verified; these are the on-board
steps. Nothing should be flashed before Gate 1 passes. No em dashes.

## Gate 1, host logic (no hardware)

```
python tools/run_host_tests.py   ->   ==== HOST TESTS: ALL PASS ====
```
Nothing flashes until this passes.

## Gate 2, hub senses

Wire:
- Waist BNO085 to the XIAO ESP32-C3 I2C, addr 0x4A, wires UNDER 10 cm
  (SDA D4/GPIO5, SCL D5/GPIO6, confirm on the board).
- Right thigh BNO085 in UART-RVC mode (PS0 tied high), sensor TX into the hub RX
  pin in `pins.h` (verify the pin is free, avoid strapping pins).

Verify:
- Flash the hub, `python tools/capture.py --port COMx`.
- Confirm the config echo and CSV header print at boot.
- Stand still, send `c`, confirm `# calibrated: ...`.
- Standing shows near-zero `rt_rate`.
- Hand-swing the thigh sensor: pitch swings, `step_evt` fires, `walk_state` goes
  WALK. Check the `plot_session.py` figure.
- First wear only: confirm the sign constants in `config.h` (log shows raw and
  corrected), flip RT_PITCH_SIGN / WAIST_*_SIGN if a forward lean reads negative.
- If RVC frame_error_count is high, suspect the checksum range (bytes 2..17) or
  the reserved bytes first, and ask before changing the parser.

## Gate 3, hub commands

Wire:
- Hub UART2 TX to Joseph's actuation MCU RX (GW1), common ground.
- Hub UART1 TX to the GVS node RX (GV1).

Verify with `python tools/fake_actuator.py --port COMy`:
- `GW1,STOP` at boot, heartbeats every 500 ms when idle.
- Hand-swing into WALK: `GW1,ASSIST,FWD`, no line spacing under 100 ms.
- Pull the waist or thigh sensor: both streams go STOP within a gap period, fault
  LED pattern, recovery after re-init.

## Gate 4, GVS node

BEFORE POWER: confirm the pins and DAC on-level in
`firmware/gvs_node/include/config.h` against the physical v1 board.

Verify:
- Bench: multimeter on the DAC output. Boot is off. Send `GV1,LEFT` / `GV1,RIGHT`
  by hand, confirm the level and polarity change, `GV1,STOP` and 2 s silence go
  to zero.
- End to end: lean the waist sensor by hand, confirm the hub emits GV1,LEFT/RIGHT
  and the node output follows, recenter goes STOP. Film the lean demo when it
  works.

## Gate 5, left node

Wire: BNO085 to the XIAO ESP32-C3 (short wires), LiPo on the charge pads.

Pair: copy each board's boot MAC into the other's config (see README), reflash.

Verify:
- Hub shows `lt_stale 0` and populated `lt_pitch` once packets arrive.
- Send `c`: the node logs `# left node calibrated`.
- Walk: the `# asym ...` line reports amplitude and duration ratios.
- Switch the node OFF mid-session: within 200 ms `lt_stale` returns to 1,
  `lt_pitch` empties, `# asym unavailable`, and assist and GVS are UNAFFECTED.

## Gate 6, polish

- Confirm the config echo header matches the constants in `config.h`.
- Confirm the LED patterns (README table).
- Tune SWING_RATE_ON from a real walking `plot_session.py` figure per
  `docs/TUNING.md`, then re-run `tools/run_host_tests.py`.
