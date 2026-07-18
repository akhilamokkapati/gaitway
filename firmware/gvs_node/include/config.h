// config.h, Gaitway output node (classic ESP32): GVS DAC + ODrive S1 commander.
// This node receives GW1 (motor) and GV1 (GVS) command lines from the hub on ONE
// UART, drives the GVS analog stage via the DAC, and commands the ODrive over
// UART. Classic ESP32 is required for the two DAC channels. No em dashes.
#pragma once

// ###########################################################################
// #  VERIFY every pin, DAC code, and torque below on the bench BEFORE any    #
// #  powered test. GVS: confirm ~0 current at GVS_MID and under the sub       #
// #  3.5 mA hardware ceiling at LEFT/RIGHT, into a resistor, never skin       #
// #  first. Motor: confirm direction and a conservative torque limit set in   #
// #  odrivetool. Developer in the loop.                                       #
// ###########################################################################

// ---- Hub link, commands IN (both GW1 and GV1 on one UART). Old RXD2/TXD2. ----
#define PIN_HUB_RX        16
#define PIN_HUB_TX        17          // optional back-channel, unused for now

// ---- ODrive S1 link, UART ASCII ----
#define PIN_ODRIVE_TX     4           // -> ODrive UART RX, TODO verify free pin
#define PIN_ODRIVE_RX     5           // <- ODrive UART TX, TODO verify free pin
// One-time ODrive setup in odrivetool (NOT in this firmware): calibrate the
// RI80, set controller.config.control_mode = TORQUE_CONTROL, set conservative
// current/torque limits, and enable startup_closed_loop_control so the ODrive
// boots ready. This firmware then only streams torque setpoints.
const float         ODRIVE_ASSIST_TORQUE   = 0.5f;   // Nm, CONSERVATIVE, bench-tune
const unsigned long MOTOR_CMD_TIMEOUT_MS   = 2000;   // lost link -> zero torque
const int           MOTOR_FWD_SIGN         = +1;     // flip if FWD drives the wrong way

// ---- GVS DAC. Classic ESP32: DAC1 = GPIO25 (signal), DAC2 = GPIO26 (Vref). ----
const int PIN_DAC_SIGNAL = 25;        // V3, swings around the midpoint
const int PIN_DAC_VREF   = 26;        // V4, held at the midpoint to make Vref
// Bench-verified codes from the v1 board (0..255 over 0..3.3 V).
// 127 ~ 1.65 V = ZERO CURRENT = the safe boot and fail-safe state.
const int GVS_MID   = 127;            // OFF / zero current
const int GVS_LEFT  = 106;            // below midpoint (one polarity)
const int GVS_RIGHT = 148;            // above midpoint (other polarity)
// Pulsed delivery: one pulse per lean onset, with a refractory gap. Limits total
// charge into the tissue (safer than sustained DC).
const unsigned long GVS_PULSE_MS   = 120;    // pulse width
const unsigned long GVS_MIN_OFF_MS = 1000;   // minimum gap between pulses

// ---- Fail-safe: this much silence from the hub -> GVS midpoint + zero torque.
const unsigned long HUB_SILENCE_MS = 2000;
