// angles.h, wrap aware angle helpers. Single source of truth for angle math.
// Pure logic, no Arduino dependency, host testable. No em dashes.
#pragma once
#include <cmath>

namespace gaitway {

// Wrap aware signed difference a - b, result in (-180, 180].
// Never subtract raw angles anywhere in the codebase, always use this.
inline float wrapDelta(float a, float b) {
    return std::fmod(a - b + 540.0f, 360.0f) - 180.0f;
}

// Angular rate in deg/s from two samples, wrap aware, using real dt seconds.
inline float angularRate(float current, float previous, float dt_s) {
    if (dt_s <= 0.0f) return 0.0f;
    return wrapDelta(current, previous) / dt_s;
}

} // namespace gaitway
