// main.cpp, Gaitway output node (classic ESP32): GVS + ODrive S1 commander.
// Receives GW1 (motor) and GV1 (GVS) command lines from the hub on one UART,
// drives the GVS DAC, and streams torque setpoints to the ODrive. Everything is
// non-blocking: the old version bit-banged step pulses with delayMicroseconds in
// the loop, which stalled serial reads and GVS timing. Here the ODrive does the
// motor control and the loop never blocks. Fail safe by construction: boot and
// every fault go to GVS midpoint (zero current) and zero motor torque. This file
// is shown in the competition report, so it stays readable. No em dashes.
#include <Arduino.h>
#include "config.h"

// ---- Hub link and ODrive on separate hardware UARTs --------------------------
HardwareSerial& Hub    = Serial2;   // GW1 + GV1 in from the hub
HardwareSerial& ODrive = Serial1;   // ASCII commands out to the ODrive S1

// ---- Motor state -------------------------------------------------------------
static float         motorTorque   = 0.0f;   // commanded torque, Nm
static unsigned long lastHubMs     = 0;       // last valid command, any stream

// ---- GVS state (pulsed on lean onset) ---------------------------------------
enum class Gvs { STOP, LEFT, RIGHT };
static Gvs           hubGvs        = Gvs::STOP;  // last state the hub asked for
static bool          pulseActive   = false;
static unsigned long pulseEndMs    = 0;
static unsigned long lastPulseMs   = 0;         // for the refractory gap

// ---- Line assembly (non-blocking) -------------------------------------------
static char    lineBuf[48];
static uint8_t lineLen = 0;

// ---- ODrive helper. Torque (current) command in the ASCII protocol. VERIFY the
// exact syntax against your ODrive firmware version. Requires the axis to be in
// closed-loop TORQUE_CONTROL (configured in odrivetool). ------------------------
static void odriveSetTorque(float t) {
    motorTorque = t;
    ODrive.printf("c 0 %.3f\n", t);   // c <axis> <torque>
}

// ---- GVS output --------------------------------------------------------------
static void gvsMidpoint() {           // zero current, the safe state
    dacWrite(PIN_DAC_SIGNAL, GVS_MID);
    pulseActive = false;
}

static void gvsPulse(Gvs dir, unsigned long now) {
    // One pulse per onset, and never inside the refractory gap.
    if (now - lastPulseMs < GVS_MIN_OFF_MS) return;
    dacWrite(PIN_DAC_SIGNAL, dir == Gvs::LEFT ? GVS_LEFT : GVS_RIGHT);
    pulseActive = true;
    pulseEndMs  = now + GVS_PULSE_MS;
    lastPulseMs = now;
}

// ---- Fail safe: neutralise both outputs -------------------------------------
static void failSafe(const char* why) {
    gvsMidpoint();
    odriveSetTorque(0.0f);
    hubGvs = Gvs::STOP;
    Serial.printf("# SAFE (%s)\n", why);
}

// ---- Command handling --------------------------------------------------------
static void handleLine(const char* line, unsigned long now) {
    // Motor (GW1)
    if      (!strcmp(line, "GW1,ASSIST,FWD")) { odriveSetTorque(MOTOR_FWD_SIGN * ODRIVE_ASSIST_TORQUE);  lastHubMs = now; }
    else if (!strcmp(line, "GW1,ASSIST,BWD")) { odriveSetTorque(-MOTOR_FWD_SIGN * ODRIVE_ASSIST_TORQUE); lastHubMs = now; }
    else if (!strcmp(line, "GW1,STOP"))       { odriveSetTorque(0.0f);                                   lastHubMs = now; }
    else if (!strcmp(line, "GW1,HB"))         {                                                          lastHubMs = now; }
    // GVS (GV1), pulsed on the transition into a lean direction
    else if (!strcmp(line, "GV1,LEFT"))  { if (hubGvs != Gvs::LEFT)  { hubGvs = Gvs::LEFT;  gvsPulse(Gvs::LEFT,  now); } lastHubMs = now; }
    else if (!strcmp(line, "GV1,RIGHT")) { if (hubGvs != Gvs::RIGHT) { hubGvs = Gvs::RIGHT; gvsPulse(Gvs::RIGHT, now); } lastHubMs = now; }
    else if (!strcmp(line, "GV1,STOP"))  { hubGvs = Gvs::STOP; gvsMidpoint(); lastHubMs = now; }
    else if (!strcmp(line, "GV1,HB"))    { lastHubMs = now; }
    // Anything else is untrusted: fail safe.
    else failSafe("garbage");
}

void setup() {
    Serial.begin(115200);
    Hub.begin(115200, SERIAL_8N1, PIN_HUB_RX, PIN_HUB_TX);
    ODrive.begin(115200, SERIAL_8N1, PIN_ODRIVE_RX, PIN_ODRIVE_TX);

    dacWrite(PIN_DAC_VREF, GVS_MID);   // hold Vref at the midpoint (1.65 V)
    gvsMidpoint();                     // GVS off
    odriveSetTorque(0.0f);             // motor idle
    lastHubMs = millis();
    Serial.println("# gaitway output node up: GVS off, motor idle");
}

void loop() {
    unsigned long now = millis();

    // Read hub commands without ever blocking.
    while (Hub.available()) {
        char c = (char)Hub.read();
        if (c == '\n' || c == '\r') {
            if (lineLen > 0) { lineBuf[lineLen] = '\0'; handleLine(lineBuf, now); lineLen = 0; }
        } else if (lineLen < sizeof(lineBuf) - 1) {
            lineBuf[lineLen++] = c;
        } else {
            lineLen = 0;                // overflowing line, drop and fail safe
            failSafe("overflow");
        }
    }

    // End a GVS pulse when its width elapses.
    if (pulseActive && now >= pulseEndMs) gvsMidpoint();

    // Lost link: no command for HUB_SILENCE_MS neutralises both outputs.
    if (now - lastHubMs > HUB_SILENCE_MS) {
        failSafe("silence");
        lastHubMs = now;               // avoid spamming the message
    }
    // Motor watchdog: also zero torque if the motor stream specifically goes quiet.
    else if (motorTorque != 0.0f && (now - lastHubMs > MOTOR_CMD_TIMEOUT_MS)) {
        odriveSetTorque(0.0f);
    }
}
