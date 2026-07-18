# Power plan

Three separate power domains. Plan all three now, wire once. Grounds from all
three meet at ONE star point. No em dashes.

## Quick answer: can LiPo power everything?

| Domain | LiPo? | What it needs |
|---|---|---|
| Logic (ESP32s + sensors) | Yes, single 1S cell | 3.7 V straight to the XIAO BAT pads |
| Motor (ODrive + RI80) | Yes, but MULTI-cell | a 4S to 6S pack (14.8 to 22 V), NOT a single cell |
| GVS analog board | No, not directly | a +/- 15 V DC-DC converter (bipolar rails) |

## 1. Logic domain (3.3 V) - your boards

- **Hub (XIAO C3):** 1x 1S LiPo, ~1000 mAh, soldered to the BAT+ / BAT- pads.
  Charges over USB. Powers the two BNO085s from its 3V3 pin. Add a slide switch.
- **Left node (XIAO C3):** 1x 1S LiPo, ~800 mAh, on its BAT pads. Slide switch.
- **Output node (classic ESP32):** the classic ESP32 is NOT LiPo-native like the
  XIAO (no onboard charger, the 3V3 pin wants a regulated 3.3 V). Feed it a clean
  **5 V into VIN**: easiest is a small 5 V USB power bank, or a 5 V buck converter
  tapped from the motor pack.

Runtime: a C3 + 2 sensors draws ~100 to 150 mA, so 800 to 1000 mAh gives hours.

## 2. Motor domain (Joseph's, but plan for it)

- **Battery:** a **6S LiPo (~22 V nominal)** is the sweet spot inside the ODrive
  S1 range (12 to 48 V). 4S (~14.8 V) also works but with less headroom. Do NOT
  use a single cell, it is far too low.
- **Capacity / C-rating:** ~1500 to 2200 mAh, 25C or higher, so it can deliver the
  assist current peaks.
- **Must-haves:** an inline **fuse**, a **brake resistor** on the ODrive (the
  motor regenerates energy when assisting against motion, and a full battery
  cannot always absorb it), and a **main kill switch / E-stop**.
- Keep this pack physically and electrically separate from the logic rail.
  Charge it with a proper balance charger.

## 3. GVS domain (+/- 14.5 V bipolar)

The GVS op-amp board needs a **positive and a negative rail** (the schematic shows
V+ = +14.5 V and V- = -14.5 V). A single LiPo gives one polarity, so you cannot
feed it directly. Use one of:

- **A +/- 15 V DC-DC converter module** (recommended), fed from 5 V or from the
  motor pack. Prefer an **isolated** module: good practice for a stimulation
  supply. This is a small, cheap part (for example a +/-15 V "DCW" style module).
- Or two battery packs in series with a center tap (bulky, avoid).

Notes:
- The DAC signal and Vref (0 to 3.3 V) come from the output-node ESP32
  (GPIO25 / GPIO26). The +/- 15 V only powers the op-amps.
- The sub 3.5 mA current ceiling and the isolation of the OUTPUT stage (to the
  electrodes) live in the analog board hardware, not in the supply.

## 4. Suggested power bill of materials

- [ ] 1x 1S LiPo ~1000 mAh (hub) + slide switch
- [ ] 1x 1S LiPo ~800 mAh (left node) + slide switch
- [ ] 1x 5 V USB power bank (output node) OR a 5 V buck from the motor pack
- [ ] 1x 6S LiPo pack + inline fuse + brake resistor + E-stop (motor, Joseph)
- [ ] 1x +/- 15 V DC-DC converter module (GVS), isolated if possible
- [ ] 1S LiPo balance charger (logic cells) + 6S balance charger (motor pack)

## 5. Grounding and safety (do not skip)

- ALL grounds (logic, motor, GVS) join at ONE star point. A floating ground
  between domains causes noise and unsafe potentials.
- Fuse the motor pack. E-stop within reach during any motor test.
- LiPo care: correct polarity to the BAT pads (reversing kills the board), never
  charge unattended, do not puncture or over-discharge.
- GVS: bench-test the current into a resistor with a multimeter first, never
  skin, and only after advisor sign-off.

## 6. Untethered vs tethered (recap)

- Tethered (USB): powers the boards AND carries the CSV log. Use for tuning and
  capturing report data.
- Untethered (LiPo / power bank): powers the boards for the demo, and the hub
  streams the log over WiFi (SoftAP) instead of USB. See the README WiFi section.
