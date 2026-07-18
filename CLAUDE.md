# CLAUDE.md, Project Gaitway v2

Gaitway is a unilateral (right side) stroke rehabilitation exoskeleton for the
Engineering Innovation Challenge 2026 (Team U-43). This firmware senses gait and
balance and drives two outputs: a hip flexion assist motor (Joseph's ODrive S1 +
CubeMars RI80 subsystem, commanded over UART) and a Galvanic Vestibular
Stimulation (GVS) board that corrects balance. GVS integration with sensing is
the primary deliverable. Report and demo video due 23 July 2026.

This project is SEPARATE from the PASS knee project. It reuses proven BNO085
patterns from PASS but shares no code or repo.

## Architecture

```
 LEFT THIGH NODE (wireless)          WAIST HUB                     GVS NODE
 XIAO ESP32-C3 + BNO085(I2C)   XIAO ESP32-C3                 ESP32 + DAC output
 + LiPo, ESP-NOW ---------->   waist BNO085 (I2C, <10cm)     stage (existing HW)
                               right thigh BNO085 --UART-RVC--> UART1 RX
                               UART TX ----------------------->  GVS node RX
                               UART TX --> Joseph's actuation MCU (ODrive side)
                               USB serial --> laptop CSV logging
```

Design law: everything safety relevant and latency sensitive is wired. The left
node is wireless because its data is analytics only and its v1 cable was the
failure point.

## Hard constraints (non negotiable)

- No I2C run over 10 cm. Long runs are UART-RVC or wireless only.
- Wrap aware angle math ONLY: delta = fmod(a - b + 540.0, 360.0) - 180.0. Never
  subtract raw angles.
- Command streams (GW1 to actuation, GV1 to GVS) are edge triggered on state
  change, plus a heartbeat every 500 ms of silence, with a minimum 100 ms gap
  between lines. This is the v1 command flooding fix.
- Safety laws (firmware enforced, see spec section 7): no assist unless
  calibrated AND walking; waist or right thigh watchdog trip stops both streams;
  left node staleness never affects assist or GVS; GVS fails safe to output off
  on stop, silence, garbage, or boot.
- All tunables live in config.h. Config values are echoed into the CSV log header
  on boot so every session records the config that produced it.
- Joseph's GW1 contract is frozen. Do not change GW1,ASSIST,FWD / GW1,ASSIST,BWD
  / GW1,STOP / GW1,HB without explicit approval.
- No em dashes anywhere in code, comments, or docs. Use commas, colons, or
  parentheses.
- BNO085 bring up hardening is mandatory (from PASS): delay >= 100 ms after
  hardReset before enabling reports, retry enableReport 3 times 50 ms apart,
  per sensor liveness watchdog re-inits the sensor if no report for 500 ms.

## Toolchain

- PlatformIO, Arduino framework.
- Boards: seeed_xiao_esp32c3 (hub), seeed_xiao_esp32c3 (left node),
  esp32dev classic ESP32 (output node: GVS DAC + ODrive commander, needs the DAC).
- Monitor 115200.
- Host tests (detection logic) compile with g++, no Arduino dependency.
- Only libraries allowed beyond the framework: Adafruit BNO08x and its
  dependencies. Ask before adding anything else.

## Repo layout

```
firmware/hub/         PlatformIO project, waist hub
firmware/left_node/   PlatformIO project, wireless left thigh node
firmware/gvs_node/    PlatformIO project, GVS stimulation node
tools/                python3 host tools (capture, plot, replay, fake actuator)
docs/                 TUNING.md and notes
gaitway_v2_integration_spec.md   authoritative design document
reference_gait_cycle_100hz.csv   healthy reference cycle for host tests
```

The detection algorithms live in a pure logic C++ module under firmware/hub with
NO Arduino dependency, so the exact shipped code is host testable via g++.

## Build gates (follow strictly, see spec section 8 and handoff section 13)

1. Logic proven on host: detection module + replay test passing. Nothing flashes
   before this passes.
2. Hub senses: waist I2C + RVC parser + calibration + CSV logging.
3. Hub commands: both UART streams, edge + heartbeat + gap, STOP on fault.
4. GVS node: fresh firmware, bench verified, lean to stimulation end to end.
5. Left node: ESP-NOW, staleness handling, asymmetry metric.
6. Polish: config echo, LED patterns, README, TUNING.md.

Ask before: deviating from the spec, adding libraries, changing GW1/GV1
contracts, or if real sensor behaviour contradicts the spec.
