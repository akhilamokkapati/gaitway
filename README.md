# Project Gaitway v2

Sensing and control firmware for the Gaitway unilateral stroke rehabilitation
exoskeleton, Engineering Innovation Challenge 2026, Team U-43.

This repo is separate from the PASS knee project. It reuses proven BNO085
bring up patterns from PASS but shares no code.

## What it does

A waist hub senses gait (right thigh) and balance (waist) and drives two outputs:

- Hip flexion assist, over UART to Joseph's ODrive S1 + CubeMars RI80 (GW1 stream).
- Galvanic Vestibular Stimulation for balance correction (GV1 stream to a DAC board).

A wireless left thigh node (ESP-NOW) provides asymmetry analytics only, never
safety critical data.

See `gaitway_v2_integration_spec.md` for the authoritative design and interface
contracts, and `CLAUDE.md` for the engineering constraints.

## Layout

| Path | What |
|---|---|
| `firmware/hub/` | Waist hub firmware (XIAO ESP32-S3) |
| `firmware/left_node/` | Wireless left thigh node (XIAO ESP32-C3) |
| `firmware/gvs_node/` | GVS stimulation node (ESP32 + DAC) |
| `tools/` | python3 host tools: capture, plot, replay, fake actuator |
| `docs/` | Tuning notes |

## Status

Gate 1 (host logic) in progress. Nothing flashes until the detection module and
its replay test pass on the host. See `CLAUDE.md` for the full gate list.

## Build (once past Gate 1)

Per target with PlatformIO:

```
pio run -d firmware/hub -t upload
pio device monitor -b 115200
```

Host detection tests need a C++ compiler (g++) and python3. See `docs/` and
`tools/replay_reference.py`.
