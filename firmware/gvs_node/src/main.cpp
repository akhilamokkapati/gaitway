// main.cpp, Gaitway GVS node firmware (fresh, replaces the lost v1 code).
// Receives GV1 command lines from the hub and drives the DAC level and polarity
// pins. Fail safe by construction: boot state is output off, and the output is
// forced off on GV1,STOP, on 2 s of silence, on any parse garbage, and on buffer
// overflow. No stimulation is ever produced without an explicit LEFT or RIGHT
// command. This file is shown in the competition report, so it stays small and
// readable. No em dashes.
#include <Arduino.h>
#include "config.h"

enum class Gvs { OFF, LEFT, RIGHT };

static Gvs      state       = Gvs::OFF;
static uint32_t lastCmdMs   = 0;
static char     lineBuf[32];
static uint8_t  lineLen     = 0;

// Set the physical output. OFF means DAC zero and both polarity pins low.
static void applyOutput(Gvs s) {
    switch (s) {
        case Gvs::LEFT:
            digitalWrite(PIN_POL_A, HIGH);
            digitalWrite(PIN_POL_B, LOW);
            dacWrite(PIN_DAC, DAC_LEVEL_ON);
            break;
        case Gvs::RIGHT:
            digitalWrite(PIN_POL_A, LOW);
            digitalWrite(PIN_POL_B, HIGH);
            dacWrite(PIN_DAC, DAC_LEVEL_ON);
            break;
        case Gvs::OFF:
        default:
            dacWrite(PIN_DAC, 0);
            digitalWrite(PIN_POL_A, LOW);
            digitalWrite(PIN_POL_B, LOW);
            break;
    }
    state = s;
}

static void failSafeOff(const char* why) {
    if (state != Gvs::OFF) Serial.printf("# GVS OFF (%s)\n", why);
    applyOutput(Gvs::OFF);
}

// Parse one complete GV1 line. Unknown or malformed lines fail safe to off.
static void handleLine(const char* line) {
    if (strcmp(line, "GV1,LEFT") == 0)      { applyOutput(Gvs::LEFT);  lastCmdMs = millis(); }
    else if (strcmp(line, "GV1,RIGHT") == 0){ applyOutput(Gvs::RIGHT); lastCmdMs = millis(); }
    else if (strcmp(line, "GV1,STOP") == 0) { failSafeOff("stop");     lastCmdMs = millis(); }
    else if (strcmp(line, "GV1,HB") == 0)   { lastCmdMs = millis(); }   // keep state alive
    else                                    { failSafeOff("garbage");  }  // anything else is unsafe
}

void setup() {
    Serial.begin(115200);
    pinMode(PIN_POL_A, OUTPUT);
    pinMode(PIN_POL_B, OUTPUT);
    applyOutput(Gvs::OFF);                 // boot state is output off
    Serial1.begin(115200, SERIAL_8N1, PIN_GV1_RX, -1);   // GV1 receive only
    lastCmdMs = millis();
    Serial.println("# gaitway GVS node up, output OFF, waiting for GV1 commands");
}

void loop() {
    while (Serial1.available()) {
        char c = (char)Serial1.read();
        if (c == '\n' || c == '\r') {
            if (lineLen > 0) {
                lineBuf[lineLen] = '\0';
                handleLine(lineBuf);
                lineLen = 0;
            }
        } else if (lineLen < sizeof(lineBuf) - 1) {
            lineBuf[lineLen++] = c;
        } else {
            // Overflowing line, cannot trust it: reset and fail safe.
            lineLen = 0;
            failSafeOff("overflow");
        }
    }

    // Lost link: no command for GV1_SILENCE_MS forces the output off.
    if ((uint32_t)(millis() - lastCmdMs) > GV1_SILENCE_MS) {
        failSafeOff("silence");
        lastCmdMs = millis();              // avoid repeating the message every loop
    }
}
