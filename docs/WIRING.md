# Gaitway wiring guide

Every connection for the one-leg (right side) build. Pin names come straight from
the firmware (`firmware/*/include/*.h`). Verify each pin against the physical
board silkscreen before powering on. No em dashes.

Nodes:
- Hub = XIAO ESP32-C3
- Left node = XIAO ESP32-C3 (wireless)
- Output node = classic ESP32 (drives GVS analog board + commands the ODrive)
- ODrive S1 + CubeMars RI80 (motor)
- GVS analog board (the bipolar current source)

## 0. Parts checklist

- [ ] 2x Seeed XIAO ESP32-C3 (hub, left node)
- [ ] 1x classic ESP32 dev board (output node, the v1 board with the DAC)
- [ ] 3x BNO085 breakout (waist, right thigh, left thigh)
- [ ] ODrive S1 (ordered) + CubeMars RI80 V2.0 KV75 with Hall (ordered)
- [ ] GVS analog board (the v1 Howland current-source board)
- [ ] 1x LiPo for the left node (on its BAT pads)
- [ ] Motor battery in the ODrive S1 range (12 to 48 V), fused
- [ ] Bipolar +/- 14.5 V (or +/-15 V) supply for the GVS op-amps
- [ ] JST connectors, twisted-pair wire, external status LED + resistor
- [ ] USB-C cables (hub + output node for flashing/logging), a motor E-stop switch

## 1. POWER (read first)

Three separate power domains. Getting these crossed is what destroys parts.

| Rail | Voltage | Feeds | Source |
|---|---|---|---|
| Logic | 3.3 V / 5 V | the 3 ESP32s, all 3 BNO085s | USB (bench) or a 5 V regulator (wearable); left node from its LiPo |
| Motor | 12 to 48 V | ODrive S1 DC bus only | motor battery, FUSED |
| GVS analog | +/- 14.5 V | the GVS op-amp board rails only | bipolar supply / DC-DC module |

Rules (from the spec, these are not optional):
- Logic and motor rails are regulated SEPARATELY. Never power an ESP32 from the
  motor battery directly.
- ALL grounds meet at ONE star point: logic GND, motor GND, GVS GND joined once.
- Fuse the motor battery. Add a brake resistor if the battery cannot absorb regen.
- The BNO085s run at 3.3 V logic. The ESP32 GPIO are all 3.3 V, so no level
  shifting is needed anywhere in the logic domain.

## 2. Hub (XIAO ESP32-C3)

Powers/logs over USB on the bench. One hardware UART (Serial1) does both the
right-thigh RVC in and the merged command stream out.

| Hub pin | Silk | Connects to | Purpose |
|---|---|---|---|
| 3V3 | 3V3 | waist BNO085 VIN | sensor power |
| GND | GND | waist BNO085 GND, star ground | ground |
| GPIO6 | D4/SDA | waist BNO085 SDA | I2C (wires < 10 cm) |
| GPIO7 | D5/SCL | waist BNO085 SCL | I2C (wires < 10 cm) |
| GPIO20 | D7/RX | right-thigh BNO085 TX | RVC frames IN |
| GPIO21 | D6/TX | output node GPIO16 | merged GW1+GV1 OUT |
| GPIO4 | D2 | external LED anode (LED cathode to GND via ~330 ohm) | status |
| USB-C | - | laptop | flashing + CSV logging |

Waist BNO085 is in I2C mode (addr 0x4A), mounted on the belt next to the hub.

## 3. Right thigh BNO085 (UART-RVC, cable up the leg)

Bare sensor, no MCU. Mode pins strapped locally at the sensor.

| BNO085 pin | Connects to | Notes |
|---|---|---|
| VIN | hub 3V3 | up the cable |
| GND | hub GND | up the cable, twist with TX |
| PS0 | 3V3 (HIGH) | selects UART-RVC (verify on your breakout) |
| PS1 | GND (LOW) | selects UART-RVC (verify on your breakout) |
| TX | hub GPIO20 (D7) | the only signal wire |

Cable up the leg carries 3 wires only: 3V3, GND, TX. Twist TX with GND. JST at
both ends. If frame_error_count is high at bring-up, suspect PS0/PS1 or the TX
pin label first.

## 4. Left thigh node (XIAO ESP32-C3, wireless)

| Node pin | Silk | Connects to |
|---|---|---|
| 3V3 | 3V3 | left BNO085 VIN |
| GND | GND | left BNO085 GND |
| GPIO6 | D4/SDA | left BNO085 SDA (short wires) |
| GPIO7 | D5/SCL | left BNO085 SCL (short wires) |
| BAT+/BAT- | back pads | LiPo | 

No wires to the rest of the system: it talks to the hub over ESP-NOW. Pair by
copying each board's boot-printed MAC into the other's config (see README).

## 5. Output node (classic ESP32): GVS + ODrive commander

| ESP32 pin | Connects to | Purpose |
|---|---|---|
| GPIO16 | hub GPIO21 (D6) | commands IN (GW1 + GV1) |
| GPIO17 | (optional) hub RX | back-channel, unused for now |
| GPIO4 | ODrive UART RX | motor commands OUT |
| GPIO5 | ODrive UART TX | (ODrive replies) |
| GPIO25 | GVS board "DAC" input | DAC1, bipolar signal |
| GPIO26 | GVS board "Vref" input | DAC2, held at 1.65 V midpoint |
| GND | ODrive GND, GVS GND, star ground | common reference |
| USB-C | laptop (bench) | flashing / debug |

GPIO16/17 are free on a WROOM ESP32 (your v1 board). If yours is a WROVER
(PSRAM), those pins are taken, pick two other free UART-capable pins and update
`firmware/gvs_node/include/config.h`.

## 6. GVS analog board (Howland bipolar current source)

| GVS board node | Connects to |
|---|---|
| DAC input | output-node GPIO25 |
| Vref input | output-node GPIO26 |
| V+ rail | +14.5 V |
| V- rail | -14.5 V |
| GND | star ground |
| Tissue / output | electrodes (see safety) |

The firmware holds Vref (GPIO26) at code 127 (about 1.65 V) and swings the DAC
(GPIO25): 127 = zero current (OFF), 106 and 148 = the two polarities. The sub
3.5 mA current ceiling is set by the board hardware, not software.

## 7. Motor: ODrive S1 + CubeMars RI80

| Connection | Detail |
|---|---|
| Motor phases (3) | RI80 3 thick phase wires -> ODrive S1 phase terminals (screw) |
| Hall sensors (5) | RI80 Hall: 5 V, GND, H1, H2, H3 -> ODrive S1 Hall/GPIO inputs |
| DC bus | motor battery (12 to 48 V, fused) -> ODrive VBUS / GND |
| UART | ODrive GPIO1 (TX) / GPIO2 (RX), 3.3 V -> output node GPIO5 / GPIO4 |
| GND | ODrive GND to the star point (common with the output node) |

One-time setup in odrivetool over USB (NOT in firmware):
1. Set DC bus / current limits CONSERVATIVELY first.
2. Motor calibration, then Hall encoder setup (encoder mode HALL).
3. `controller.config.control_mode = TORQUE_CONTROL`.
4. Set a low torque/current limit for first tests.
5. Enable `startup_closed_loop_control` so it boots ready.
6. Confirm the ASCII torque command works and that FWD drives the intended way
   (flip MOTOR_FWD_SIGN in config.h if not).
Keep clear of the linkage and have an E-stop / battery kill switch during tests.

## 8. Inter-node link summary

```
laptop --USB--> Hub (C3)
Hub  --I2C(<10cm)-->  Waist BNO085
Hub  <--RVC (1 wire up leg)--  Right-thigh BNO085
Hub  --UART (GPIO21 -> GPIO16)-->  Output node (ESP32)
Hub  ~~ESP-NOW~~  Left node (C3) + Left BNO085
Output node --UART (GPIO4/5)-->  ODrive S1 --3ph+Hall-->  RI80 motor
Output node --DAC/Vref (GPIO25/26)-->  GVS board --> electrodes
```

## 9. Safety before power

- GVS: bench-test the current INTO A RESISTOR with a multimeter first, never
  skin. Confirm ~0 at midpoint and under 3.5 mA at LEFT/RIGHT. Electrodes only
  after advisor sign-off.
- Motor: conservative ODrive limits, E-stop reachable, stay clear of the linkage.
- Fuse the motor battery. Double check the +/-14.5 V and motor grounds meet the
  logic ground at the single star point before applying any rail.

## 10. Build order (do NOT wire everything at once)

Follow `docs/BRINGUP.md` gate by gate: hub + sensors first (Gate 2), then the
command link + fake_actuator (Gate 3), then the GVS bench test (Gate 4), then the
wireless node (Gate 5). Prove each stage before adding the next.
