// main.cpp, Gaitway waist hub (XIAO ESP32-S3).
// Gate 2 scope: sense the waist (I2C) and right thigh (UART-RVC), calibrate a
// standing baseline, run step/walking detection, and log CSV at 50 Hz.
// Command output streams (GW1 to actuation, GV1 to GVS) and the lean classifier
// arrive in Gate 3 and Gate 4, hooks are marked below. No em dashes.
#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <esp_now.h>
#include <Adafruit_BNO08x.h>

#include "config.h"
#include "pins.h"
#include "angles.h"
#include "orientation.h"
#include "calibration.h"
#include "detection.h"
#include "rvc_parser.h"
#include "command_stream.h"
#include "lean_classifier.h"
#include "gaitway_packet.h"
#include "asymmetry.h"

using namespace gaitway;

// Output command streams (spec 6). GW1 to Joseph's actuation MCU on UART2 TX,
// GV1 to the GVS node on UART1 TX (UART1 RX carries the RVC frames).
static CommandStream gw1("GW1,HB", CMD_MIN_GAP_MS, CMD_HEARTBEAT_MS);
static CommandStream gv1("GV1,HB", CMD_MIN_GAP_MS, CMD_HEARTBEAT_MS);
static const char*   assistShort = "STOP";   // for the CSV assist_cmd column
static const char*   gvsShort    = "STOP";   // for the CSV gvs_state column

// ---------------------------------------------------------------------------
// Sensors and detection
// ---------------------------------------------------------------------------
static Adafruit_BNO08x   waist(-1);          // no hardware reset pin wired
static sh2_SensorValue_t waistValue;
static RvcParser         rtParser;

static DetectorConfig makeDetCfg() {
    DetectorConfig c;
    c.swing_rate_on   = SWING_RATE_ON;
    c.swing_rate_off  = SWING_RATE_OFF;
    c.t_confirm_ms    = T_CONFIRM_MS;
    c.t_refract_ms    = T_REFRACT_MS;
    c.rate_lp_alpha   = RATE_LP_ALPHA;
    c.walk_window_ms  = WALK_WINDOW_MS;
    return c;
}
static StepDetector detector(makeDetCfg());

static LeanConfig makeLeanCfg() {
    LeanConfig c;
    c.enter        = GVS_ENTER;
    c.exit         = GVS_EXIT;
    c.confirm_ms   = T_LEAN_CONFIRM_MS;
    c.min_dwell_ms = GVS_MIN_DWELL_MS;
    return c;
}
static LeanClassifier gvsLean(makeLeanCfg());

// Left leg analytics (spec 5.6). A second detector runs on the wireless left
// thigh pitch to extract per-step amplitude and duration for the asymmetry
// metric. This path is analytics only and never gates assist or GVS.
static StepDetector     leftDetector(makeDetCfg());
static AsymmetryMetric  asym(ASYM_WINDOW_STEPS);

// Shared with the ESP-NOW receive callback (runs in the WiFi task).
static portMUX_TYPE     ltMux = portMUX_INITIALIZER_UNLOCKED;
static volatile bool    ltNew = false;
static LeftNodePacket   ltPkt;
static uint32_t         ltRecvMs = 0;
static uint32_t         lastLeftMs = 0;   // last valid packet, for staleness
static float            ltPitch = 0.0f;   // latest left pitch deviation, for logging
static bool             ltStale = true;
static uint32_t         lastAsymMs = 0;

static void sendCalRequest();             // defined in the ESP-NOW section below

// ---------------------------------------------------------------------------
// Live state
// ---------------------------------------------------------------------------
static bool   calibrated = false;
static bool   fault      = false;

static float  waistPitch = 0, waistRoll = 0, waistYaw = 0;   // latest raw, sign corrected
static float  rtPitch    = 0;                                 // latest raw, sign corrected
static float  rtRate     = 0;                                 // filtered, from detector

static float  baseWaistPitch = 0, baseWaistRoll = 0, baseRtPitch = 0;

static uint32_t lastWaistMs = 0;     // last waist report time, for the watchdog
static bool     stepPending = false; // latched STEP for the next CSV row
static Dir      lastDir     = Dir::NONE;

// Calibration accumulators
static bool         calibrating = false;
static RunningStats calWaistPitch, calWaistRoll, calRtPitch;

static uint32_t lastLogMs = 0;

// ---------------------------------------------------------------------------
// Waist BNO085 hardened bring up (PASS pattern, spec 4.2)
// ---------------------------------------------------------------------------
static bool waistEnableReports() {
    for (int i = 0; i < 3; ++i) {                       // retry 3 times, 50 ms apart
        if (waist.enableReport(SH2_GAME_ROTATION_VECTOR, 10000)) return true;  // 100 Hz
        delay(50);
    }
    return false;
}

static bool waistBegin() {
    Wire.begin(PIN_WAIST_SDA, PIN_WAIST_SCL);
    Wire.setClock(400000);
    if (!waist.begin_I2C(WAIST_BNO_I2C_ADDR, &Wire)) return false;
    delay(120);                                         // >= 100 ms after reset
    return waistEnableReports();
}

// ---------------------------------------------------------------------------
// Status LED patterns
// ---------------------------------------------------------------------------
static void ledWrite(bool on) {
#if STATUS_LED_ACTIVE_LOW
    digitalWrite(PIN_STATUS_LED, on ? LOW : HIGH);
#else
    digitalWrite(PIN_STATUS_LED, on ? HIGH : LOW);
#endif
}

static void updateLed(uint32_t now) {
    uint32_t ph = now % 1000;
    bool on;
    if (fault)               on = (now % 200) < 100;          // fast blink, fault
    else if (!calibrated)    on = ph < 500;                   // slow blink, waiting
    else if (detector.isWalking(now)) on = (now % 250) < 125; // pulse, walking
    else                     on = true;                       // solid, calibrated idle
    ledWrite(on);
}

// ---------------------------------------------------------------------------
// Config echo, printed once before the CSV header (safety law 5)
// ---------------------------------------------------------------------------
static void printConfigEcho() {
    Serial.println("# gaitway hub config");
    Serial.printf("# SWING_RATE_ON=%.1f SWING_RATE_OFF=%.1f T_CONFIRM_MS=%u T_REFRACT_MS=%u\n",
                  SWING_RATE_ON, SWING_RATE_OFF, T_CONFIRM_MS, T_REFRACT_MS);
    Serial.printf("# RATE_LP_ALPHA=%.2f WALK_WINDOW_MS=%u\n", RATE_LP_ALPHA, WALK_WINDOW_MS);
    Serial.printf("# GVS_ENTER=%.1f GVS_EXIT=%.1f T_LEAN_CONFIRM_MS=%u GVS_MIN_DWELL_MS=%u\n",
                  GVS_ENTER, GVS_EXIT, T_LEAN_CONFIRM_MS, GVS_MIN_DWELL_MS);
    Serial.printf("# CAL_FRAMES=%u CAL_STILL_STDDEV=%.2f SENSOR_TIMEOUT_MS=%u LT_STALE_MS=%u\n",
                  CAL_FRAMES, CAL_STILL_STDDEV, SENSOR_TIMEOUT_MS, LT_STALE_MS);
    Serial.printf("# RT_PITCH_SIGN=%+.0f WAIST_PITCH_SIGN=%+.0f WAIST_ROLL_SIGN=%+.0f\n",
                  RT_PITCH_SIGN, WAIST_PITCH_SIGN, WAIST_ROLL_SIGN);
    Serial.println("millis,waist_pitch,waist_roll,rt_pitch,rt_rate,lt_pitch,lt_stale,"
                   "step_evt,walk_state,dir,gvs_state,assist_cmd");
}

// ---------------------------------------------------------------------------
// Calibration (spec 5.1)
// ---------------------------------------------------------------------------
static void startCalibration() {
    calibrating = true;
    calibrated  = false;
    calWaistPitch = RunningStats();
    calWaistRoll  = RunningStats();
    calRtPitch    = RunningStats();
    sendCalRequest();                 // ask the left node to capture its own baseline
    Serial.println("# calibration: hold still, collecting");
}

static void finishCalibration() {
    calibrating = false;
    double sp = calWaistPitch.stddev(), sr = calWaistRoll.stddev(), srt = calRtPitch.stddev();
    if (sp > CAL_STILL_STDDEV || sr > CAL_STILL_STDDEV || srt > CAL_STILL_STDDEV) {
        Serial.printf("# calibration REJECTED, too much motion (stddev wp=%.2f wr=%.2f rt=%.2f), send c to retry\n",
                      sp, sr, srt);
        return;
    }
    baseWaistPitch = (float)calWaistPitch.mean;
    baseWaistRoll  = (float)calWaistRoll.mean;
    baseRtPitch    = (float)calRtPitch.mean;
    detector.reset();
    leftDetector.reset();
    gvsLean.reset();
    asym.reset();
    calibrated = true;
    Serial.printf("# calibrated: waist_pitch=%.2f waist_roll=%.2f rt_pitch=%.2f\n",
                  baseWaistPitch, baseWaistRoll, baseRtPitch);
}

// ---------------------------------------------------------------------------
// Sensor service
// ---------------------------------------------------------------------------
static void serviceWaist(uint32_t now) {
    if (waist.wasReset()) waistEnableReports();          // re-enable after a soft reset
    if (!waist.getSensorEvent(&waistValue)) return;
    if (waistValue.sensorId != SH2_GAME_ROTATION_VECTOR) return;

    float p, r, y;
    quatToEuler(waistValue.un.gameRotationVector.real,
                waistValue.un.gameRotationVector.i,
                waistValue.un.gameRotationVector.j,
                waistValue.un.gameRotationVector.k, p, r, y);
    waistPitch = WAIST_PITCH_SIGN * p;
    waistRoll  = WAIST_ROLL_SIGN  * r;
    waistYaw   = y;
    lastWaistMs = now;

    if (calibrating) {
        calWaistPitch.add(waistPitch);
        calWaistRoll.add(waistRoll);
        if (calWaistPitch.n >= CAL_FRAMES && calRtPitch.n >= CAL_FRAMES) finishCalibration();
    }
}

static void serviceRightThigh(uint32_t now) {
    while (Serial1.available()) {
        RvcFrame fr;
        if (!rtParser.feedByte((uint8_t)Serial1.read(), now, fr)) continue;

        rtPitch = RT_PITCH_SIGN * fr.pitch_deg;

        if (calibrating) {
            calRtPitch.add(rtPitch);
            if (calWaistPitch.n >= CAL_FRAMES && calRtPitch.n >= CAL_FRAMES) finishCalibration();
            continue;
        }
        if (!calibrated) continue;

        // Feed detection with the baseline relative, wrap aware deviation.
        float dev = wrapDelta(rtPitch, baseRtPitch);
        StepEvent ev = detector.update(dev, now);
        rtRate = detector.lastRate();
        if (ev.fired) {
            stepPending = true;
            lastDir = ev.direction;
        }
        if (ev.ended) asym.feedRightStep(ev.amplitude_deg, ev.duration_ms);
    }
}

// ---------------------------------------------------------------------------
// Watchdogs (spec 4.2/4.3, safety law 3). When fault is set, computeAssist and
// computeGvs both force STOP, so both streams stop within one gap period.
// ---------------------------------------------------------------------------
static void serviceWatchdogs(uint32_t now) {
    bool waistDead = (now - lastWaistMs) > SENSOR_TIMEOUT_MS;
    bool rtDead    = calibrated && rtParser.validCount() > 0 &&
                     (now - rtParser.lastValidMillis()) > SENSOR_TIMEOUT_MS;

    if (waistDead || rtDead) {
        if (!fault) Serial.printf("# FAULT: %s watchdog trip, re-initialising\n",
                                  waistDead ? "waist" : "right-thigh");
        fault = true;
        if (waistDead) { waistBegin(); lastWaistMs = now; }  // recovery via re-init
    } else if (fault) {
        fault = false;
        Serial.println("# fault cleared");
    }
}

// ---------------------------------------------------------------------------
// Command intents (spec 6, safety law 1). Assist only while calibrated AND
// walking AND not faulted. GVS lean classifier arrives in Gate 4, STOP for now.
// ---------------------------------------------------------------------------
static const char* computeAssist(uint32_t now) {
    if (!calibrated || fault) { assistShort = "STOP"; return "GW1,STOP"; }
    if (detector.isWalking(now) && lastDir != Dir::NONE) {
        if (lastDir == Dir::FWD) { assistShort = "FWD"; return "GW1,ASSIST,FWD"; }
        assistShort = "BWD"; return "GW1,ASSIST,BWD";
    }
    assistShort = "STOP";
    return "GW1,STOP";
}

static const char* computeGvs(uint32_t now) {
    // GVS never outputs before calibration or during a fault (safety).
    if (!calibrated || fault) { gvsShort = "STOP"; return "GV1,STOP"; }
    float dRoll = wrapDelta(waistRoll, baseWaistRoll);
    switch (gvsLean.update(dRoll, now)) {
        case Lean::LEFT:  gvsShort = "LEFT";  return "GV1,LEFT";
        case Lean::RIGHT: gvsShort = "RIGHT"; return "GV1,RIGHT";
        default:          gvsShort = "STOP";  return "GV1,STOP";
    }
}

// ---------------------------------------------------------------------------
// ESP-NOW left node (spec 4.3, 8). Analytics only, guarded by the stale flag.
// ---------------------------------------------------------------------------
// Callback signature targets Arduino-ESP32 2.x. On core 3.x the first argument
// becomes const esp_now_recv_info_t*.
static void onLeftRecv(const uint8_t* /*mac*/, const uint8_t* data, int len) {
    if (len != (int)sizeof(LeftNodePacket)) return;
    LeftNodePacket p;
    memcpy(&p, data, sizeof(p));
    if (!leftPacketValid(p)) return;          // schema, node id, and crc8
    portENTER_CRITICAL_ISR(&ltMux);
    ltPkt      = p;
    ltRecvMs   = millis();
    lastLeftMs = ltRecvMs;
    ltNew      = true;
    portEXIT_CRITICAL_ISR(&ltMux);
}

static void espnowSetup() {
    WiFi.mode(WIFI_STA);
    Serial.print("# hub MAC: ");
    Serial.println(WiFi.macAddress());
    if (esp_now_init() != ESP_OK) {
        Serial.println("# WARN: esp_now_init failed, left node analytics disabled");
        return;
    }
    esp_now_register_recv_cb(onLeftRecv);
    esp_now_peer_info_t peer{};
    memcpy(peer.peer_addr, LEFT_NODE_MAC, 6);
    peer.channel = 0;
    peer.encrypt = false;
    if (esp_now_add_peer(&peer) != ESP_OK)
        Serial.println("# WARN: could not add left-node peer, is LEFT_NODE_MAC set?");
}

static void sendCalRequest() {
    CalRequestPacket req{};
    req.schema_version = GW_SCHEMA_VERSION;
    req.node_id        = NODE_LEFT_THIGH;
    req.cmd            = CAL_CMD_CAPTURE;
    req.crc8           = calRequestCrc(req);
    esp_now_send(LEFT_NODE_MAC, (const uint8_t*)&req, sizeof(req));
}

static void serviceLeft(uint32_t now) {
    bool haveNew = false;
    LeftNodePacket p;
    uint32_t recvMs = 0;
    portENTER_CRITICAL(&ltMux);
    if (ltNew) { p = ltPkt; recvMs = ltRecvMs; ltNew = false; haveNew = true; }
    portEXIT_CRITICAL(&ltMux);

    if (haveNew) {
        ltPitch = p.pitch_cdeg / 100.0f;      // node sends baseline deviation already
        StepEvent ev = leftDetector.update(ltPitch, recvMs);
        if (ev.ended) asym.feedLeftStep(ev.amplitude_deg, ev.duration_ms);
    }
    ltStale = (lastLeftMs == 0) || ((now - lastLeftMs) > LT_STALE_MS);
}

static void logAsymmetry(uint32_t now) {
    if (now - lastAsymMs < 2000) return;      // every 2 s, as a comment line
    lastAsymMs = now;
    AsymResult a = asym.result(ltStale);
    if (a.available)
        Serial.printf("# asym amp=%.2f dur=%.2f nL=%u nR=%u\n",
                      a.amplitude_ratio, a.duration_ratio, a.n_left, a.n_right);
    else
        Serial.printf("# asym unavailable (left %s, nL=%u nR=%u)\n",
                      ltStale ? "stale" : "ok", a.n_left, a.n_right);
}

// ---------------------------------------------------------------------------
// CSV logging at 50 Hz
// ---------------------------------------------------------------------------
static const char* dirStr(Dir d) {
    return d == Dir::FWD ? "FWD" : d == Dir::BWD ? "BWD" : "NONE";
}

static void logRow(uint32_t now) {
    float wp = calibrated ? wrapDelta(waistPitch, baseWaistPitch) : 0.0f;
    float wr = calibrated ? wrapDelta(waistRoll,  baseWaistRoll)  : 0.0f;
    float rp = calibrated ? wrapDelta(rtPitch,    baseRtPitch)    : 0.0f;
    const char* walk = detector.isWalking(now) ? "WALK" : "STAND";

    // millis,waist_pitch,waist_roll,rt_pitch,rt_rate,lt_pitch,lt_stale,
    // step_evt,walk_state,dir,gvs_state,assist_cmd
    // lt_pitch is empty while the left node is stale (never synthesized).
    char ltbuf[16];
    if (ltStale) ltbuf[0] = '\0';
    else         snprintf(ltbuf, sizeof(ltbuf), "%.2f", ltPitch);

    Serial.printf("%lu,%.2f,%.2f,%.2f,%.1f,%s,%d,%d,%s,%s,%s,%s\n",
                  (unsigned long)now, wp, wr, rp, rtRate,
                  ltbuf, ltStale ? 1 : 0, stepPending ? 1 : 0, walk,
                  dirStr(lastDir), gvsShort, assistShort);
    stepPending = false;
}

// ---------------------------------------------------------------------------
void setup() {
    Serial.begin(115200);
    delay(300);
    pinMode(PIN_STATUS_LED, OUTPUT);
    ledWrite(false);

    // UART1: RX = right-thigh RVC frames, TX = GV1 stream to the GVS node.
    Serial1.begin(115200, SERIAL_8N1, PIN_RT_RVC_RX, PIN_GVS_TX);
    // UART2: TX = GW1 stream to Joseph's actuation MCU (RX unused).
    Serial2.begin(115200, SERIAL_8N1, -1, PIN_ACT_TX);

    espnowSetup();   // left node analytics link (also prints the hub MAC)

    if (!waistBegin()) {
        Serial.println("# FATAL: waist BNO085 not found, check I2C wiring (<10cm) and address");
        fault = true;
    }
    lastWaistMs = millis();

    printConfigEcho();
    Serial.println("# send 'c' while standing still to calibrate");
}

void loop() {
    uint32_t now = millis();

    if (Serial.available()) {
        int cmd = Serial.read();
        if (cmd == 'c' || cmd == 'C') startCalibration();
    }

    serviceWaist(now);
    serviceRightThigh(now);
    serviceLeft(now);
    serviceWatchdogs(now);

    // Command streams: compute intent, emit edge/heartbeat lines within the gap.
    const char* gwLine = gw1.update(computeAssist(now), now);
    if (gwLine) Serial2.println(gwLine);
    const char* gvLine = gv1.update(computeGvs(now), now);
    if (gvLine) Serial1.println(gvLine);

    if (now - lastLogMs >= (uint32_t)(1000 / LOG_HZ)) {
        lastLogMs = now;
        logRow(now);
    }

    logAsymmetry(now);
    updateLed(now);
}
