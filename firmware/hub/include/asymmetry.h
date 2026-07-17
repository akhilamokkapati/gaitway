// asymmetry.h, left vs right gait asymmetry metric (analytics only, spec 5.6).
// Pure logic, NO Arduino dependency, host testable. No em dashes.
//
// Per step it compares left and right thigh swing amplitude and swing duration,
// reporting the ratio (left / right) over a rolling window of the most recent
// steps on each side. The result is available only when both sides have data
// AND the left node is not stale. Left data is never synthesized: when the node
// is stale the metric reports unavailable.
#pragma once
#include <cstdint>

namespace gaitway {

struct AsymResult {
    bool    available;
    float   amplitude_ratio;   // mean(left amplitude) / mean(right amplitude)
    float   duration_ratio;    // mean(left duration) / mean(right duration)
    uint8_t n_left;
    uint8_t n_right;
};

class AsymmetryMetric {
public:
    explicit AsymmetryMetric(uint8_t window_steps);
    void reset();

    void feedRightStep(float amplitude_deg, uint16_t duration_ms);
    void feedLeftStep(float amplitude_deg, uint16_t duration_ms);

    // left_stale comes from the hub staleness rule (no packet for 200 ms).
    AsymResult result(bool left_stale) const;

private:
    static const uint8_t CAP = 20;
    struct Ring {
        float    amp[CAP];
        uint16_t dur[CAP];
        uint8_t  n    = 0;
        uint8_t  head = 0;
    };
    void push(Ring& r, float amp, uint16_t dur);
    static float meanAmp(const Ring& r);
    static float meanDur(const Ring& r);

    uint8_t window_;
    Ring    left_;
    Ring    right_;
};

} // namespace gaitway
