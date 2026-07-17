# Project Gaitway v2 Integration Specification

Team U-43 | EIC 2026 Phase B | Draft v1.0, 17 July 2026
Owner: Akhila (sensing, coding, system integration)
Interface partner: Joseph (actuation, exoskeleton structure)

---

## 1. Purpose

This document defines the v2 sensing and control architecture for Project Gaitway. It replaces the v1 (Term 4) system, which suffered from long-wire I2C failures, high detection latency, and unreliable walking detection. It serves three roles:

1. Implementation brief for firmware development (Claude Code handoff)
2. Interface contract between the sensing hub (Akhila) and the actuation subsystem (Joseph)
3. System architecture reference for the Phase B report

v1 lessons this design answers: long I2C wires unreliable, TCA mux fragility, 80 ms sampling too slow, amplitude-based movement score missed slow gait, actuation response lag, GVS never integrated with sensing, left thigh sensor failure took down asymmetry sensing.

---

## 2. System overview

```
                         +---------------------------+
   LEFT THIGH NODE       |        WAIST HUB          |
  +----------------+     |  XIAO ESP32-S3            |
  | XIAO ESP32-C3  |     |                           |
  | BNO085 (I2C,   | ESP-NOW (WiFi) --> [RX]         |
  |  short wires)  |     |                           |
  | LiPo 800 mAh   |     |  Waist BNO085  (I2C,      |
  +----------------+     |   on-board, short wires)  |
                         |                           |
   RIGHT THIGH SENSOR    |                           |
  +----------------+     |                           |
  | BNO085 in      |---->| UART1 RX (RVC frames)     |
  | UART-RVC mode  |     |                           |
  +----------------+     |                           |
                         |  UART1 TX --> GVS ESP32   |
                         |  UART0/2 TX --> ODrive MCU|
                         |  USB serial --> laptop log |
                         +---------------------------+
```

Design principle: everything safety-relevant and latency-sensitive stays wired. The left thigh node is wireless because it is the longest cable run in v1, it crosses the body, and its data is not required for actuation or GVS. Loss of the left node degrades analytics only, never safety.

---

## 3. Hardware allocation

| Role | Board | Sensor | Link to hub | Power |
|---|---|---|---|---|
| Waist hub | XIAO ESP32-S3 | BNO085, I2C mode (PS0/PS1 jumpers set for I2C), addr 0x4A | on-board, wires under 10 cm | shared battery, regulated logic rail |
| Right thigh | none (sensor only) | BNO085 in UART-RVC mode (PS0 high) | 4-core cable up right leg to hub UART1 RX | hub 3V3 |
| Left thigh node | XIAO ESP32-C3 | BNO085, I2C mode, short wires | ESP-NOW to hub | 800 mAh LiPo on C3 charge pads, slide switch |
| GVS board | existing second ESP32 | DAC output stage (unchanged from v1) | UART from hub TX | isolated supply, shared ground reference per v1 design |
| Actuation | Joseph: ODrive S1 + CubeMars RI80 | Hall feedback to ODrive | UART command stream from hub | motor battery rail, separate regulation, common ground, star point |

Wiring rules carried over from v1 lessons:
- No I2C run longer than about 10 cm. Long runs are UART-RVC or wireless only.
- JST connectors on every detachable cable. No direct-soldered inter-module wires.
- Strain relief (zip-tie anchor or adhesive) at every board-edge cable exit.
- Twisted pair for the right-thigh UART cable: signal twisted with GND.
- Single shared battery per body region is fine, but logic and motor rails regulated separately, grounds joined at one star point.

---

## 4. Data acquisition

### 4.1 Right thigh, UART-RVC
- BNO085 UART-RVC streams at 100 Hz fixed: header 0xAAAA, index, yaw, pitch, roll (int16, centidegrees), x/y/z accel, checksum.
- Parser validates header and checksum, discards malformed frames, maintains a last-valid timestamp.

### 4.2 Waist, I2C
- BNO085 in I2C mode using the standard SH-2 driver (same library stack as PASS knee firmware).
- Game rotation vector at 100 Hz. Convert quaternion to pitch and roll for the classifiers.
- Apply the PASS-derived bring-up hardening: boot delay after reset before feature enable, retry loop on enable, per-sensor liveness watchdog (re-init if no report within 500 ms).

### 4.3 Left thigh node, ESP-NOW
- C3 samples its BNO085 at 100 Hz, transmits a packet every 20 ms (50 Hz) to the hub MAC.
- Packet schema v1 (packed struct, little-endian):

```
uint8  schema_version   // = 1
uint8  node_id          // = LEFT_THIGH (0x02)
uint32 node_millis      // sender clock, for staleness detection
int16  pitch_cdeg       // centidegrees
int16  roll_cdeg
int16  pitch_rate_cds   // centideg/s, computed on the node
uint16 seq              // rolling sequence number
uint8  status           // bit0 sensor_ok, bit1 battery_low
uint8  crc8
```

- Hub-side liveness rule: if no valid packet for 200 ms, mark left-thigh data stale. All consumers of left-thigh data must check the stale flag. Detection and actuation never depend on this stream.

### 4.4 Angle conventions
- All pitch and roll values are deviations from the calibrated standing baseline (see 5.1), in degrees, after wrap-aware differencing: delta = ((a - b + 540) mod 360) - 180.
- Positive thigh pitch = thigh raised forward (swing direction). Verify sign on first bring-up and set a per-sensor sign constant in config.

---

## 5. Detection algorithms (hub firmware)

### 5.1 Standing calibration
- On command (button or serial 'c'), collect 100 frames (1 s at 100 Hz) while the wearer stands still.
- Store baselines: waist pitch, waist roll, right thigh pitch; request and store left thigh baseline via a calibration packet if the node is live.
- Reject calibration if variance during capture exceeds a stillness threshold; prompt retry.

### 5.2 Step event detector (replaces v1 movement score)
Event-based detection on right thigh pitch velocity. Rationale: v1's amplitude score at 12.5 Hz was laggy and missed low-amplitude gait; swing initiation produces a sharp, short-latency velocity signature even in slow gait.

State machine per detector:

```
IDLE:
  if |thigh_pitch_rate| > SWING_RATE_ON for >= T_CONFIRM (e.g. 30 ms)
      -> SWING_DETECTED (emit STEP event, record direction sign)
SWING_DETECTED:
  when |thigh_pitch_rate| < SWING_RATE_OFF -> REFRACTORY
REFRACTORY:
  hold T_REFRACT (e.g. 250 ms) to prevent double-count, then -> IDLE
```

- SWING_RATE_ON, SWING_RATE_OFF, T_CONFIRM, T_REFRACT are config constants tuned from logged data on day 1 of testing. Starting guesses: ON 40 deg/s, OFF 15 deg/s.
- Hysteresis (ON > OFF) and refractory period carry over the v1 anti-chatter intent, applied at event level.

### 5.3 Walking state
- WALKING = at least one STEP event within the last WALK_WINDOW (default 1500 ms).
- STANDING = no STEP event for WALK_WINDOW.
- No separate start/stop thresholds; the event window replaces v1's dual-threshold score logic.

### 5.4 Direction classification
- Primary: sign of thigh pitch excursion during the detected swing relative to baseline (forward swing raises the thigh).
- Cross-check: waist pitch deviation sign (v1 method, demoted to secondary).
- If primary and secondary disagree, hold the previous direction (do not flip on a single ambiguous step).

### 5.5 Lean classifier (GVS trigger)
- dRoll = waist roll - baseline roll (wrap-aware).
- Enter stimulation when |dRoll| > GVS_ENTER (start 4.5 deg, from v1 tuning) sustained T_LEAN_CONFIRM (100 ms).
- Exit when |dRoll| < GVS_EXIT (2.0 deg).
- Direction: dRoll > 0 -> GVS_RIGHT, dRoll < 0 -> GVS_LEFT.
- Hard mutual exclusion and minimum dwell between polarity changes (500 ms) to prevent oscillation.

### 5.6 Asymmetry metric (analytics only, left node required)
- When left data is fresh: per-step comparison of left vs right thigh pitch amplitude and swing duration; report ratio over a rolling 10-step window.
- When stale: metric reported as unavailable. Never synthesized.
- Future (tier 2, implement only if time allows): use left-leg stance events (left thigh pitch returning through baseline) as the timing reference to gate right-side assist. Document in report as designed-for, demonstrated-if-ready.

---

## 6. Command interfaces (hub outputs)

### 6.1 To actuation (Joseph) - THE CONTRACT
- Physical: UART, 3V3 logic, 115200 8N1, hub TX to actuation MCU RX, common ground.
- ASCII line protocol, newline-terminated, so it is testable with any serial monitor:

```
GW1,ASSIST,FWD\n      // begin/continue forward assist
GW1,ASSIST,BWD\n      // begin/continue backward assist
GW1,STOP\n            // stop assist
GW1,HB\n              // heartbeat, sent every 500 ms when no other traffic
```

- Rate limiting: minimum 100 ms between command lines (v1 CMD_GAP fix carried over). State changes are edge-triggered: send once on change plus heartbeat, not continuous flooding.
- Actuation-side rule (Joseph): if no line received for 2 s, treat as lost link and stop assist. Motor side owns all torque/current/velocity limits; the hub only ever expresses intent.
- Test harness: Joseph can drive his side with a laptop typing these lines; Akhila can verify hub output with a USB-UART dongle. Neither side blocks the other.

### 6.2 To GVS board
- Same physical pattern, separate UART pins, 115200 8N1:

```
GV1,LEFT\n
GV1,RIGHT\n
GV1,STOP\n
GV1,HB\n
```

- Rate limit 100 ms, edge-triggered plus heartbeat. GVS board must stop stimulation on 2 s silence.
- Electrical isolation of the stimulation output stage is unchanged from v1; current ceiling remains hardware-enforced below 3.5 mA.

### 6.3 To laptop (logging)
- USB serial, 115200. CSV lines at 50 Hz:

```
millis,waist_pitch,waist_roll,rt_pitch,rt_rate,lt_pitch,lt_stale,step_evt,walk_state,dir,gvs_state,assist_cmd
```

- Every demo session is automatically a dataset. These logs are the quantitative results for the Phase B report (step detection latency, lean-to-stimulation latency, standing vs walking traces) and the source for report figures.

---

## 7. Safety rules (firmware-enforced)

1. Assist commands only while WALKING and calibrated. Uncalibrated system never commands assist.
2. GVS and assist streams are independent but both stop on their own fault conditions.
3. Any sensor fault on waist or right thigh (liveness watchdog trip) -> immediate STOP on both command streams, status LED fault pattern.
4. Left node staleness never affects assist or GVS.
5. All thresholds and limits in one config header, values recorded in the report.

---

## 8. Build and test plan (6 days)

Day 1: Hub bring-up. Waist I2C + right thigh RVC streaming and logged. Calibration routine. Sign conventions verified.
Day 2: Step detector tuned from day-1 logs. Walking state + direction working live on screen. Hand Joseph the contract (section 6.1).
Day 3: GVS integration. Lean classifier driving the GVS board over UART. Film the lean demo as soon as it works.
Day 4: Left thigh ESP-NOW node + staleness handling + asymmetry metric. Integration with Joseph's actuation to whatever state the motor is in.
Day 5: Full-system rehearsal, filming for the 2-minute video, capture clean logs for report figures.
Day 6: Buffer. Report figures, video edit, upload margin before 23 July.

Report is drafted in parallel from day 1; sections 2 to 7 of this spec map directly onto the report's system design section.

---

## 9. Open items

- [ ] Confirm BNO085 spare count available for Gaitway without touching the PASS knee unit
- [ ] Confirm GVS board from v1 still functions (bench test with multimeter, day 1)
- [ ] Verify CubeMars RI80 + ODrive S1 arrival (Joseph)
- [ ] Agree actuation MCU choice on Joseph's side (what receives the UART contract and talks to ODrive)
- [ ] Decide demo assist behaviour with Joseph: swing-triggered pulse vs continuous-while-walking
- [ ] Right-thigh cable length measurement on the actual harness before crimping
