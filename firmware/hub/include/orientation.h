// orientation.h, quaternion to pitch/roll for the waist BNO085.
// Pure logic, header only, NO Arduino dependency, host testable. No em dashes.
//
// The BNO085 game rotation vector gives a unit quaternion (w, x, y, z). The lean
// and step classifiers work in pitch and roll degrees. Mounting orientation is
// handled downstream by per sensor sign constants and the calibrated baseline
// (config.h), so this is a plain aerospace ZYX conversion.
#pragma once
#include <cmath>

namespace gaitway {

// Fills pitch and roll in degrees. yaw is filled too for logging.
inline void quatToEuler(float w, float x, float y, float z,
                        float& pitch_deg, float& roll_deg, float& yaw_deg) {
    const float RAD2DEG = 57.2957795131f;

    // roll (rotation about x)
    float sinr_cosp = 2.0f * (w * x + y * z);
    float cosr_cosp = 1.0f - 2.0f * (x * x + y * y);
    roll_deg = std::atan2(sinr_cosp, cosr_cosp) * RAD2DEG;

    // pitch (rotation about y), clamped to avoid NaN at the poles
    float sinp = 2.0f * (w * y - z * x);
    if (sinp > 1.0f)  sinp = 1.0f;
    if (sinp < -1.0f) sinp = -1.0f;
    pitch_deg = std::asin(sinp) * RAD2DEG;

    // yaw (rotation about z)
    float siny_cosp = 2.0f * (w * z + x * y);
    float cosy_cosp = 1.0f - 2.0f * (y * y + z * z);
    yaw_deg = std::atan2(siny_cosp, cosy_cosp) * RAD2DEG;
}

} // namespace gaitway
