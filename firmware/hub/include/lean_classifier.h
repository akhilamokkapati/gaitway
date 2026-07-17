// lean_classifier.h, waist-roll lean detector that drives the GVS command stream.
// Pure logic, NO Arduino dependency, host testable. No em dashes.
//
// Spec 5.5: dRoll is the wrap-aware waist roll deviation from the standing
// baseline. Enter stimulation when |dRoll| > GVS_ENTER sustained for
// T_LEAN_CONFIRM, exit when |dRoll| < GVS_EXIT (hysteresis, ENTER > EXIT).
// dRoll > 0 -> RIGHT, dRoll < 0 -> LEFT. LEFT and RIGHT are mutually exclusive
// (a single state), and a minimum dwell separates a change from one polarity to
// the OTHER so the stimulation cannot oscillate. Stopping is never delayed.
#pragma once
#include <cstdint>

namespace gaitway {

enum class Lean : uint8_t { STOP = 0, LEFT = 1, RIGHT = 2 };

struct LeanConfig {
    float    enter;         // deg, |dRoll| to enter stimulation
    float    exit;          // deg, |dRoll| to leave (must be < enter)
    uint16_t confirm_ms;    // sustained beyond enter before entering
    uint16_t min_dwell_ms;  // minimum time between opposite polarities
};

class LeanClassifier {
public:
    explicit LeanClassifier(const LeanConfig& cfg);
    void reset();

    // Feed the waist roll deviation (deg) at time now_ms. Returns the current
    // GVS state.
    Lean update(float d_roll, uint32_t now_ms);

    Lean state() const { return state_; }

private:
    LeanConfig cfg_;
    Lean       state_;
    Lean       confirm_cand_;   // direction currently being confirmed
    bool       confirming_;
    uint32_t   confirm_start_;
    Lean       last_polarity_;  // last LEFT/RIGHT entered, persists through STOP
    uint32_t   last_polarity_ms_;
};

} // namespace gaitway
