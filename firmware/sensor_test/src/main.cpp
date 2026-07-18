// sensor_test, bring-up check for the hub's two right-side sensors.
// Reads the waist BNO085 (I2C) and the right-thigh BNO085 (UART-RVC) at the same
// time and prints live pitch/roll numbers from both, so you can confirm each one
// responds when you tilt it. Self-contained (only needs Adafruit BNO08x). Flash
// this to the hub C3, then tilt each sensor and watch the numbers move.
// No em dashes.
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_BNO08x.h>
#include <math.h>

// XIAO ESP32-C3 pins, same as the hub
#define PIN_SDA     6    // D4
#define PIN_SCL     7    // D5
#define PIN_RVC_RX  20   // D7, right-thigh sensor TX

Adafruit_BNO08x   waist(-1);
sh2_SensorValue_t val;
bool     waistOK   = false;
uint8_t  waistAddr = 0;

static void quatToEuler(float w, float x, float y, float z,
                        float& pitch, float& roll, float& yaw) {
    const float D = 57.29578f;
    roll  = atan2f(2 * (w * x + y * z), 1 - 2 * (x * x + y * y)) * D;
    float s = 2 * (w * y - z * x); if (s > 1) s = 1; if (s < -1) s = -1;
    pitch = asinf(s) * D;
    yaw   = atan2f(2 * (w * z + x * y), 1 - 2 * (y * y + z * z)) * D;
}

// Minimal RVC parser (sliding 19-byte window)
uint8_t  rbuf[19]; uint8_t rhave = 0;
uint32_t rFrames = 0, rErr = 0;
float    rtPitch = 0, rtRoll = 0, rtYaw = 0;

static void feedRvc(uint8_t b) {
    if (rhave < 19) rbuf[rhave++] = b;
    else { for (int i = 0; i < 18; i++) rbuf[i] = rbuf[i + 1]; rbuf[18] = b; }
    if (rhave < 19) return;
    if (rbuf[0] == 0xAA && rbuf[1] == 0xAA) {
        uint8_t sum = 0; for (int i = 2; i <= 17; i++) sum += rbuf[i];
        if (sum == rbuf[18]) {
            int16_t yaw = (int16_t)(rbuf[3] | (rbuf[4] << 8));
            int16_t pit = (int16_t)(rbuf[5] | (rbuf[6] << 8));
            int16_t rol = (int16_t)(rbuf[7] | (rbuf[8] << 8));
            rtYaw = yaw / 100.0f; rtPitch = pit / 100.0f; rtRoll = rol / 100.0f;
            rFrames++; rhave = 0; return;
        }
        rErr++;
    }
}

static bool tryWaist(uint8_t addr) {
    if (waist.begin_I2C(addr, &Wire)) {
        delay(120);
        for (int i = 0; i < 3; i++) {
            if (waist.enableReport(SH2_GAME_ROTATION_VECTOR, 10000)) return true;
            delay(50);
        }
    }
    return false;
}

void setup() {
    Serial.begin(115200);
    delay(400);
    Wire.begin(PIN_SDA, PIN_SCL);
    Wire.setClock(400000);
    if      (tryWaist(0x4B)) { waistOK = true; waistAddr = 0x4B; }
    else if (tryWaist(0x4A)) { waistOK = true; waistAddr = 0x4A; }
    Serial1.begin(115200, SERIAL_8N1, PIN_RVC_RX, -1);
    if (waistOK) Serial.printf("# waist BNO085 OK at 0x%02X\n", waistAddr);
    else         Serial.println("# waist BNO085 NOT FOUND, check I2C wiring");
    Serial.println("# tilt each sensor, the numbers should move");
}

float wp = 0, wr = 0, wy = 0;
uint32_t last = 0;
uint32_t waistEvents = 0;

void loop() {
    if (waistOK && waist.getSensorEvent(&val) &&
        val.sensorId == SH2_GAME_ROTATION_VECTOR) {
        quatToEuler(val.un.gameRotationVector.real, val.un.gameRotationVector.i,
                    val.un.gameRotationVector.j, val.un.gameRotationVector.k, wp, wr, wy);
        waistEvents++;
    }

    while (Serial1.available()) feedRvc((uint8_t)Serial1.read());

    if (millis() - last >= 200) {
        last = millis();
        if (waistOK)
            Serial.printf("WAIST[0x%02X evt=%lu] pitch=%6.1f roll=%6.1f | THIGH p=%6.1f r=%6.1f y=%6.1f frames=%lu err=%lu\n",
                          waistAddr, (unsigned long)waistEvents, wp, wr,
                          rtPitch, rtRoll, rtYaw, (unsigned long)rFrames, (unsigned long)rErr);
        else
            Serial.printf("WAIST[NOT FOUND on I2C] | THIGH p=%6.1f r=%6.1f y=%6.1f frames=%lu err=%lu\n",
                          rtPitch, rtRoll, rtYaw, (unsigned long)rFrames, (unsigned long)rErr);
    }
}
