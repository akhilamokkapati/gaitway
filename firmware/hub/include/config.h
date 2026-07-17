// config.h, Gaitway hub, single source of truth for all tunables.
// Every value here is echoed into the CSV log header on boot (safety law 5).
// No em dashes anywhere. Wrap aware angle math only (see angles.h).
#pragma once

// ---------------------------------------------------------------------------
// Sampling and timing
// ---------------------------------------------------------------------------
static const float    SAMPLE_HZ        = 100.0f;   // BNO085 report rate
static const uint32_t LOG_HZ           = 50;       // CSV output rate to laptop

// ---------------------------------------------------------------------------
// Standing calibration (spec 5.1)
// ---------------------------------------------------------------------------
static const uint16_t CAL_FRAMES       = 100;      // 1 s at 100 Hz
static const float    CAL_STILL_STDDEV = 1.0f;     // reject if any signal stddev exceeds this (deg)

// ---------------------------------------------------------------------------
// Step event detector (spec 5.2). Runs on right thigh pitch velocity, deg/s.
// Defaults below are STARTING GUESSES tuned for slow stroke gait. They are
// deliberately low. Healthy reference gait peaks near 183 deg/s, so the host
// replay test uses its own threshold (see tools/replay_reference.py), it does
// NOT use these patient defaults.
// ---------------------------------------------------------------------------
static const float    SWING_RATE_ON    = 40.0f;    // deg/s, enter swing
static const float    SWING_RATE_OFF   = 15.0f;    // deg/s, leave swing (hysteresis, ON > OFF)
static const uint16_t T_CONFIRM_MS     = 30;       // sustained above ON before emitting STEP
static const uint16_t T_REFRACT_MS     = 250;      // dead time after swing to prevent double count

// Rate estimation. Raw per sample differencing amplifies sensor noise into large
// false rates (0.5 deg noise at 100 Hz -> up to ~150 deg/s). RATE_LP_ALPHA is a
// first order low pass on the computed rate, 1.0 = no filtering. Verified on the
// host replay: alpha in [0.3, 0.5] gives zero false steps on 0.5 deg noise while
// preserving real swings. See docs/TUNING.md.
static const float    RATE_LP_ALPHA    = 0.4f;     // 0 < alpha <= 1, lower = smoother

// ---------------------------------------------------------------------------
// Walking state (spec 5.3)
// ---------------------------------------------------------------------------
static const uint16_t WALK_WINDOW_MS   = 1500;     // WALKING if a STEP within this window

// ---------------------------------------------------------------------------
// Lean classifier for GVS (spec 5.5). Operates on waist roll deviation.
// ---------------------------------------------------------------------------
static const float    GVS_ENTER        = 4.5f;     // deg, |dRoll| to enter stimulation
static const float    GVS_EXIT         = 2.0f;     // deg, |dRoll| to exit
static const uint16_t T_LEAN_CONFIRM_MS = 100;     // sustained before entering
static const uint16_t GVS_MIN_DWELL_MS = 500;      // minimum time between polarity changes

// ---------------------------------------------------------------------------
// Asymmetry metric (spec 5.6, analytics only)
// ---------------------------------------------------------------------------
static const uint8_t  ASYM_WINDOW_STEPS = 10;      // rolling window for left vs right ratio

// ---------------------------------------------------------------------------
// Command interfaces (spec 6). Shared by GW1 (actuation) and GV1 (GVS).
// ---------------------------------------------------------------------------
static const uint16_t CMD_MIN_GAP_MS   = 100;      // minimum spacing between command lines
static const uint16_t CMD_HEARTBEAT_MS = 500;      // heartbeat when no other traffic

// ---------------------------------------------------------------------------
// Liveness watchdogs (spec 4.2, 4.3, safety law 3)
// ---------------------------------------------------------------------------
static const uint16_t SENSOR_TIMEOUT_MS = 500;     // waist / right thigh re-init if silent this long
static const uint16_t LT_STALE_MS       = 200;     // left node data stale after this (analytics only)

// ---------------------------------------------------------------------------
// Per sensor mounting sign (spec 4.4). Verify on first wear, log raw + corrected.
// +1 means positive pitch = thigh raised forward.
// ---------------------------------------------------------------------------
static const float    RT_PITCH_SIGN    = +1.0f;    // right thigh
static const float    WAIST_PITCH_SIGN = +1.0f;    // waist pitch
static const float    WAIST_ROLL_SIGN  = +1.0f;    // waist roll (positive = lean right)

// ---------------------------------------------------------------------------
// ESP-NOW left node (spec 4.3, 8). Set LEFT_NODE_MAC to the node's STA MAC,
// which it prints at boot. Used to send it a calibration request; incoming
// packets are matched by node_id, not MAC.
// ---------------------------------------------------------------------------
static const uint8_t  LEFT_NODE_MAC[6] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };  // TODO SET
