// detection.cpp, Gaitway step and walking detection implementation.
// Pure logic, NO Arduino dependency. No em dashes. See detection.h.
#include "detection.h"
#include "angles.h"
#include <cmath>

namespace gaitway {

StepDetector::StepDetector(const DetectorConfig& cfg) : cfg_(cfg) {
    reset();
}

void StepDetector::reset() {
    state_            = State::IDLE;
    have_prev_        = false;
    have_rate_        = false;
    have_step_        = false;
    confirming_       = false;
    prev_pitch_       = 0.0f;
    prev_ms_          = 0;
    rate_f_           = 0.0f;
    confirm_start_ms_ = 0;
    excursion_accum_  = 0.0f;
    refract_start_ms_ = 0;
    last_step_ms_     = 0;
    swing_start_ms_   = 0;
    swing_min_        = 0.0f;
    swing_max_        = 0.0f;
}

StepEvent StepDetector::update(float thigh_pitch_deg, uint32_t now_ms) {
    StepEvent ev;

    if (!have_prev_) {
        have_prev_  = true;
        prev_pitch_ = thigh_pitch_deg;
        prev_ms_    = now_ms;
        return ev;
    }

    float dt_s = (float)(now_ms - prev_ms_) / 1000.0f;
    if (dt_s <= 0.0f) {
        // Duplicate or out of order timestamp, keep state, do not integrate.
        return ev;
    }

    float rate_raw = angularRate(thigh_pitch_deg, prev_pitch_, dt_s);
    float delta    = wrapDelta(thigh_pitch_deg, prev_pitch_);   // this sample's excursion
    prev_pitch_    = thigh_pitch_deg;
    prev_ms_       = now_ms;

    // First order low pass on the rate (Gate 1 fix 1).
    if (!have_rate_) {
        rate_f_    = rate_raw;
        have_rate_ = true;
    } else {
        rate_f_ = cfg_.rate_lp_alpha * rate_raw + (1.0f - cfg_.rate_lp_alpha) * rate_f_;
    }

    float ar = std::fabs(rate_f_);

    switch (state_) {
        case State::IDLE:
            if (ar > cfg_.swing_rate_on) {
                if (!confirming_) {
                    confirming_      = true;
                    confirm_start_ms_ = now_ms;
                    excursion_accum_ = delta;
                } else {
                    excursion_accum_ += delta;
                }
                if ((now_ms - confirm_start_ms_) >= cfg_.t_confirm_ms) {
                    // Emit exactly one STEP for this swing episode (Gate 1 fix 2).
                    ev.fired     = true;
                    ev.direction = (excursion_accum_ >= 0.0f) ? Dir::FWD : Dir::BWD;
                    last_step_ms_ = now_ms;
                    have_step_    = true;
                    state_        = State::SWING;
                    confirming_   = false;
                    // Begin tracking swing amplitude and duration for analytics.
                    swing_start_ms_ = now_ms;
                    swing_min_      = thigh_pitch_deg;
                    swing_max_      = thigh_pitch_deg;
                }
            } else {
                confirming_      = false;
                excursion_accum_ = 0.0f;
            }
            break;

        case State::SWING:
            if (thigh_pitch_deg < swing_min_) swing_min_ = thigh_pitch_deg;
            if (thigh_pitch_deg > swing_max_) swing_max_ = thigh_pitch_deg;
            // Wait for the swing to wind down before allowing the next one.
            if (ar < cfg_.swing_rate_off) {
                state_            = State::REFRACTORY;
                refract_start_ms_ = now_ms;
                ev.ended          = true;
                ev.amplitude_deg  = swing_max_ - swing_min_;
                ev.duration_ms    = (uint16_t)(now_ms - swing_start_ms_);
            }
            break;

        case State::REFRACTORY:
            if ((now_ms - refract_start_ms_) >= cfg_.t_refract_ms) {
                state_ = State::IDLE;
            }
            break;
    }

    return ev;
}

bool StepDetector::isWalking(uint32_t now_ms) const {
    if (!have_step_) return false;
    return (now_ms - last_step_ms_) <= cfg_.walk_window_ms;
}

} // namespace gaitway
