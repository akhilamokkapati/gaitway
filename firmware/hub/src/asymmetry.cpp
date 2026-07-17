// asymmetry.cpp, see asymmetry.h. Pure logic, no Arduino. No em dashes.
#include "asymmetry.h"

namespace gaitway {

AsymmetryMetric::AsymmetryMetric(uint8_t window_steps) {
    window_ = window_steps == 0 ? 1 : (window_steps > CAP ? CAP : window_steps);
    reset();
}

void AsymmetryMetric::reset() {
    left_  = Ring();
    right_ = Ring();
}

void AsymmetryMetric::push(Ring& r, float amp, uint16_t dur) {
    r.amp[r.head] = amp;
    r.dur[r.head] = dur;
    r.head = (uint8_t)((r.head + 1) % window_);
    if (r.n < window_) ++r.n;
}

float AsymmetryMetric::meanAmp(const Ring& r) {
    if (r.n == 0) return 0.0f;
    float s = 0.0f;
    for (uint8_t i = 0; i < r.n; ++i) s += r.amp[i];
    return s / (float)r.n;
}

float AsymmetryMetric::meanDur(const Ring& r) {
    if (r.n == 0) return 0.0f;
    float s = 0.0f;
    for (uint8_t i = 0; i < r.n; ++i) s += (float)r.dur[i];
    return s / (float)r.n;
}

void AsymmetryMetric::feedRightStep(float amplitude_deg, uint16_t duration_ms) {
    push(right_, amplitude_deg, duration_ms);
}

void AsymmetryMetric::feedLeftStep(float amplitude_deg, uint16_t duration_ms) {
    push(left_, amplitude_deg, duration_ms);
}

AsymResult AsymmetryMetric::result(bool left_stale) const {
    AsymResult out{false, 0.0f, 0.0f, left_.n, right_.n};
    if (left_stale || left_.n == 0 || right_.n == 0) return out;

    float ra = meanAmp(right_), rd = meanDur(right_);
    if (ra < 1e-3f || rd < 1e-3f) return out;   // avoid divide by near zero

    out.available       = true;
    out.amplitude_ratio = meanAmp(left_) / ra;
    out.duration_ratio  = meanDur(left_) / rd;
    return out;
}

} // namespace gaitway
