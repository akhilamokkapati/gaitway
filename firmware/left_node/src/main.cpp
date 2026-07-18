// main.cpp, Gaitway left thigh node (XIAO ESP32-C3).
// Samples the BNO085 at 100 Hz, computes pitch/roll and pitch rate on the node,
// and transmits telemetry to the hub over ESP-NOW at 50 Hz. Analytics only: this
// data never gates assist or GVS on the hub. Handles a calibration request from
// the hub by capturing its own 1 s standing baseline and reporting deviations
// thereafter. No em dashes.
#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <Adafruit_BNO08x.h>

#include "config.h"
#include "gaitway_packet.h"     // shared schema
#include "angles.h"             // wrap-aware helpers
#include "orientation.h"        // quaternion to pitch/roll
#include "calibration.h"        // RunningStats

using namespace gaitway;

static Adafruit_BNO08x   bno(-1);
static sh2_SensorValue_t val;

static bool     calibrated = false;
static bool     calibrating = false;
static RunningStats calPitch, calRoll;
static float    basePitch = 0, baseRoll = 0;

static float    pitch = 0, roll = 0, yaw = 0;
static float    pitchRate = 0;          // filtered, deg/s
static bool     haveRate = false;
static float    prevPitch = 0;
static uint32_t prevMs = 0;
static uint16_t seq = 0;
static uint32_t lastTxMs = 0;
static bool     sensorOk = false;

// ---- BNO085 hardened bring up (same pattern as the hub) --------------------
static bool enableReports() {
    for (int i = 0; i < 3; ++i) {
        if (bno.enableReport(SH2_GAME_ROTATION_VECTOR, 10000)) return true;
        delay(50);
    }
    return false;
}
static bool bnoBegin() {
    Wire.begin();
    Wire.setClock(400000);
    if (!bno.begin_I2C(NODE_BNO_ADDR, &Wire)) return false;
    delay(120);
    return enableReports();
}

// ---- ESP-NOW ---------------------------------------------------------------
// Calibration request from the hub. Signature targets Arduino-ESP32 2.x. On core
// 3.x change the first arg to const esp_now_recv_info_t*.
static void onRecv(const uint8_t* /*mac*/, const uint8_t* data, int len) {
    if (len != (int)sizeof(CalRequestPacket)) return;
    CalRequestPacket req;
    memcpy(&req, data, sizeof(req));
    if (req.schema_version != GW_SCHEMA_VERSION) return;
    if (req.node_id != NODE_LEFT_THIGH) return;
    if (req.crc8 != calRequestCrc(req)) return;
    if (req.cmd == CAL_CMD_CAPTURE) {
        calibrating = true;
        calibrated  = false;
        calPitch = RunningStats();
        calRoll  = RunningStats();
        Serial.println("# left node: calibration request received, capturing baseline");
    }
}

static void espnowSetup() {
    WiFi.mode(WIFI_STA);
    esp_wifi_set_channel(WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);  // match the hub AP channel
    Serial.print("# left node MAC: ");
    Serial.println(WiFi.macAddress());   // print for pairing into the hub HUB_MAC

    if (esp_now_init() != ESP_OK) {
        Serial.println("# FATAL: esp_now_init failed");
        return;
    }
    esp_now_register_recv_cb(onRecv);

    esp_now_peer_info_t peer{};
    memcpy(peer.peer_addr, HUB_MAC, 6);
    peer.channel = 0;
    peer.encrypt = false;
    if (esp_now_add_peer(&peer) != ESP_OK)
        Serial.println("# WARN: could not add hub peer, is HUB_MAC set?");
}

// ---- sampling --------------------------------------------------------------
static void sample(uint32_t now) {
    if (bno.wasReset()) enableReports();
    if (!bno.getSensorEvent(&val)) return;
    if (val.sensorId != SH2_GAME_ROTATION_VECTOR) return;

    quatToEuler(val.un.gameRotationVector.real, val.un.gameRotationVector.i,
                val.un.gameRotationVector.j, val.un.gameRotationVector.k,
                pitch, roll, yaw);
    sensorOk = true;

    // Pitch rate on the node, wrap aware, real dt, lightly filtered.
    if (prevMs != 0) {
        float dt = (float)(now - prevMs) / 1000.0f;
        if (dt > 0) {
            float raw = angularRate(pitch, prevPitch, dt);
            pitchRate = haveRate ? (NODE_RATE_LP_ALPHA * raw + (1 - NODE_RATE_LP_ALPHA) * pitchRate)
                                 : raw;
            haveRate = true;
        }
    }
    prevPitch = pitch;
    prevMs    = now;

    if (calibrating) {
        calPitch.add(pitch);
        calRoll.add(roll);
        if (calPitch.n >= NODE_CAL_FRAMES) {
            basePitch = (float)calPitch.mean;
            baseRoll  = (float)calRoll.mean;
            calibrated  = true;
            calibrating = false;
            Serial.printf("# left node calibrated: pitch=%.2f roll=%.2f\n", basePitch, baseRoll);
        }
    }
}

// ---- transmit --------------------------------------------------------------
static void transmit(uint32_t now) {
    // After calibration report deviations from baseline, else raw (wrap aware).
    float p = calibrated ? wrapDelta(pitch, basePitch) : pitch;
    float r = calibrated ? wrapDelta(roll, baseRoll)   : roll;

    LeftNodePacket pkt{};
    pkt.schema_version = GW_SCHEMA_VERSION;
    pkt.node_id        = NODE_LEFT_THIGH;
    pkt.node_millis    = now;
    pkt.pitch_cdeg     = (int16_t)lroundf(p * 100.0f);
    pkt.roll_cdeg      = (int16_t)lroundf(r * 100.0f);
    pkt.pitch_rate_cds = (int16_t)lroundf(pitchRate * 100.0f);
    pkt.seq            = seq++;
    pkt.status         = sensorOk ? STATUS_SENSOR_OK : 0;
    // Battery low: XIAO ESP32-C3 has no simple fuel gauge, stub as not low.
    // TODO if a divider is wired to a spare ADC, set STATUS_BATTERY_LOW here.
    pkt.crc8           = leftPacketCrc(pkt);

    esp_now_send(HUB_MAC, (const uint8_t*)&pkt, sizeof(pkt));
}

void setup() {
    Serial.begin(115200);
    delay(300);
    if (!bnoBegin()) Serial.println("# FATAL: left node BNO085 not found");
    espnowSetup();
    Serial.println("# left node up, streaming at 50 Hz");
}

void loop() {
    uint32_t now = millis();
    sample(now);
    if (now - lastTxMs >= TX_PERIOD_MS) {
        lastTxMs = now;
        transmit(now);
    }
}
