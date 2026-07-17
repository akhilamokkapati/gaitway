// detection.h, Gaitway step and walking detection.
// Pure logic, NO Arduino dependency, host testable with g++. No em dashes.
//
// Design notes (Gate 1 findings, approved additions to spec 5.2):
//  1. The computed thigh pitch rate is low pass filtered before thresholding.
//     Raw per sample differencing turns small angle noise into large false
//     rates (0.5 deg noise at 100 Hz produces rate spikes near 150 deg/s),
//     which the spec's bare threshold would count as steps. RATE_LP_ALPHA
//     controls the smoothing.
//  2. Exactly one STEP is emitted per swing episode, and its direction is taken
//     from the NET pitch excursion accumulated across the confirm window, not
//     from a single instantaneous sample. This makes direction robust and stops
//     a single swing from emitting more than one event.
//
// The swing threshold SWING_RATE_ON must sit ABOVE the subject's stance phase
// peak velocity and BELOW the swing peak. That gap is subject specific and is
// tuned from logged data (see docs/TUNING.md). The healthy reference subject is
// fast, so the host replay test uses a higher ON than the slow stroke default
// in config.h.
#pragma once
#include <cstdint>

namespace gaitway {

struct DetectorConfig {
    float    swing_rate_on;   // deg/s, enter swing
    float    swing_rate_off;  // deg/s, leave swing (must be < swing_rate_on)
    uint16_t t_confirm_ms;    // sustained above ON before emitting a STEP
    uint16_t t_refract_ms;    // dead time after a swing, prevents double count
    float    rate_lp_alpha;   // 0 < alpha <= 1 on the rate, lower = smoother
    uint16_t walk_window_ms;  // WALKING if a STEP fired within this window
};

enum class Dir : uint8_t { NONE = 0, FWD = 1, BWD = 2 };

struct StepEvent {
    bool fired;      // true only on the exact sample a STEP is emitted
    Dir  direction;  // direction of that STEP (NONE when fired == false)
};

// Step event detector, state machine on filtered right thigh pitch velocity.
class StepDetector {
public:
    explicit StepDetector(const DetectorConfig& cfg);
    void reset();

    // Feed one right thigh pitch sample (degrees, sign corrected, baseline
    // relative or raw, only the rate and excursion sign matter) at time now_ms.
    // Returns an event whose fired flag is true only on the firing sample.
    StepEvent update(float thigh_pitch_deg, uint32_t now_ms);

    bool  isWalking(uint32_t now_ms) const;
    float lastRate() const { return rate_f_; }     // filtered rate, for logging
    uint32_t lastStepMs() const { return last_step_ms_; }

private:
    enum class State : uint8_t { IDLE, SWING, REFRACTORY };

    DetectorConfig cfg_;
    State    state_;
    bool     have_prev_;
    bool     have_rate_;
    bool     have_step_;
    bool     confirming_;
    float    prev_pitch_;
    uint32_t prev_ms_;
    float    rate_f_;             // low pass filtered rate, deg/s
    uint32_t confirm_start_ms_;
    float    excursion_accum_;    // net wrap aware excursion across confirm window
    uint32_t refract_start_ms_;
    uint32_t last_step_ms_;
};

} // namespace gaitway
