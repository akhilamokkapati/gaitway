// lean_classifier.cpp, see lean_classifier.h. Pure logic, no Arduino. No em dashes.
#include "lean_classifier.h"
#include <cmath>

namespace gaitway {

LeanClassifier::LeanClassifier(const LeanConfig& cfg) : cfg_(cfg) { reset(); }

void LeanClassifier::reset() {
    state_            = Lean::STOP;
    confirm_cand_     = Lean::STOP;
    confirming_       = false;
    confirm_start_    = 0;
    last_polarity_    = Lean::STOP;
    last_polarity_ms_ = 0;
}

Lean LeanClassifier::update(float d_roll, uint32_t now_ms) {
    // Stopping is immediate and never gated (safety): inside the exit band, stop.
    if (state_ != Lean::STOP && std::fabs(d_roll) < cfg_.exit) {
        state_      = Lean::STOP;
        confirming_ = false;
        return state_;
    }

    // Candidate polarity from the current lean.
    Lean cand = Lean::STOP;
    if (d_roll >  cfg_.enter) cand = Lean::RIGHT;
    else if (d_roll < -cfg_.enter) cand = Lean::LEFT;

    // In the dead band (between exit and enter), hold the current state.
    if (cand == Lean::STOP) { confirming_ = false; return state_; }

    // Already in the candidate polarity: nothing to do.
    if (cand == state_) { confirming_ = false; return state_; }

    // Confirm the candidate for T_LEAN_CONFIRM before switching to it.
    if (!confirming_ || confirm_cand_ != cand) {
        confirming_    = true;
        confirm_cand_  = cand;
        confirm_start_ = now_ms;
    }
    if ((uint32_t)(now_ms - confirm_start_) < cfg_.confirm_ms) {
        return state_;
    }

    // Enforce the dwell only when switching to the OPPOSITE polarity.
    if (last_polarity_ != Lean::STOP && cand != last_polarity_ &&
        (uint32_t)(now_ms - last_polarity_ms_) < cfg_.min_dwell_ms) {
        return state_;   // hold until the dwell has elapsed
    }

    state_            = cand;
    last_polarity_    = cand;
    last_polarity_ms_ = now_ms;
    confirming_       = false;
    return state_;
}

} // namespace gaitway
