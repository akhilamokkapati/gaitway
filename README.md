# Project Gaitway v2

Sensing and control firmware for the Gaitway unilateral stroke rehabilitation
exoskeleton, Engineering Innovation Challenge 2026, Team U-43.

This repo is separate from the PASS knee project. It reuses proven BNO085
bring-up patterns from PASS but shares no code.

## What it does

A waist hub senses gait (right thigh) and balance (waist) and drives two outputs:

- Hip flexion assist, over UART to Joseph's ODrive S1 + CubeMars RI80 (GW1 stream).
- Galvanic Vestibular Stimulation for balance correction (GV1 stream to a DAC board).

A wireless left thigh node (ESP-NOW) provides asymmetry analytics only, never
safety critical data.

See `gaitway_v2_integration_spec.md` for the authoritative design and interface
contracts, `CLAUDE.md` for the engineering constraints, `docs/TUNING.md` for
threshold tuning, and `docs/BRINGUP.md` for the gate-by-gate hardware checklist.

## Layout

| Path | What |
|---|---|
| `firmware/hub/` | Waist hub firmware (XIAO ESP32-S3) |
| `firmware/left_node/` | Wireless left thigh node (XIAO ESP32-C3) |
| `firmware/gvs_node/` | GVS stimulation node (ESP32 + DAC) |
| `firmware/common/` | Shared ESP-NOW packet schema |
| `tools/` | python3 host tools: tests, capture, plot, fake actuator |
| `docs/` | TUNING.md, BRINGUP.md |

## Host tests (run before flashing anything)

The detection, RVC parser, command stream, lean classifier, calibration,
orientation, packet CRC, and asymmetry logic are all pure C++ compiled and tested
on the host (the tested code is the shipped code). Needs g++ and python3.

```
python tools/run_host_tests.py      # must print: ==== HOST TESTS: ALL PASS ====
```

## Flashing (PlatformIO)

Each target is its own PlatformIO project. Monitor at 115200.

```
# Hub (XIAO ESP32-S3)
pio run -d firmware/hub -t upload
pio device monitor -b 115200

# Left thigh node (XIAO ESP32-C3)
pio run -d firmware/left_node -t upload

# GVS node (ESP32 + DAC)  -- VERIFY PINS FIRST, see below
pio run -d firmware/gvs_node -t upload
```

### Pairing the wireless node

1. Flash the left node, open its monitor, copy the `# left node MAC: ...` line.
2. Put that MAC into `firmware/hub/include/config.h` `LEFT_NODE_MAC`.
3. Flash the hub, copy its `# hub MAC: ...` line into
   `firmware/left_node/include/config.h` `HUB_MAC`.
4. Reflash both. The hub logs `lt_stale 0` once packets arrive.

### GVS node, before any powered test

Confirm every pin and the DAC on-level in `firmware/gvs_node/include/config.h`
against the physical v1 board, and bench-test the DAC output with a multimeter.
The output is off at boot and fails safe to off on stop, silence, or garbage. The
sub-3.5 mA current ceiling is hardware enforced, not in software.

## Running a session

1. Wear the rig, stand still, send `c` on the hub serial to calibrate. The hub
   rejects the capture if you moved (stddev too high) and asks you to retry.
2. Capture the session: `python tools/capture.py --port COMx`.
3. Plot it for the report: `python tools/plot_session.py logs/<file>.csv`.
4. Watch the actuation contract independently:
   `python tools/fake_actuator.py --port COMy` (GW1) or `--expect GV1`.

## Status LED (hub onboard, active low)

| Pattern | Meaning |
|---|---|
| Slow blink (1 Hz) | Not calibrated, waiting for `c` |
| Solid | Calibrated, standing |
| Fast pulse (4 Hz) | Walking |
| Fast blink (5 Hz) | Fault (sensor watchdog trip) |

## CSV columns

```
millis,waist_pitch,waist_roll,rt_pitch,rt_rate,lt_pitch,lt_stale,step_evt,walk_state,dir,gvs_state,assist_cmd
```

Angles are deviations from the standing baseline (degrees). `step_evt` is 1 only
on the firing sample. `lt_pitch` is empty while the left node is stale. Config
values are echoed as `#` comment lines at the top of every session, and the
asymmetry ratio is logged as a periodic `# asym ...` line.
