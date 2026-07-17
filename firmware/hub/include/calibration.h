// calibration.h, running mean/stddev for standing calibration stillness checks.
// Pure logic, header only, NO Arduino dependency, host testable. No em dashes.
//
// Standing calibration (spec 5.1): collect 100 frames while the wearer stands
// still, store the mean as the baseline, and reject the capture if any signal's
// stddev exceeds CAL_STILL_STDDEV (the wearer moved). Welford's algorithm keeps
// it single pass and numerically stable.
#pragma once
#include <cstdint>
#include <cmath>

namespace gaitway {

struct RunningStats {
    uint32_t n    = 0;
    double   mean = 0.0;
    double   m2   = 0.0;   // sum of squares of differences from the mean

    void reset() { n = 0; mean = 0.0; m2 = 0.0; }

    void add(double x) {
        ++n;
        double delta = x - mean;
        mean += delta / (double)n;
        m2   += delta * (x - mean);
    }

    double variance() const { return n > 1 ? m2 / (double)(n - 1) : 0.0; }
    double stddev()   const { return std::sqrt(variance()); }
};

} // namespace gaitway
