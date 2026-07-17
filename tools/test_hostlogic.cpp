// test_hostlogic.cpp, host tests for calibration stats and quaternion->euler.
// Compile: g++ -std=c++17 -I firmware/hub/include tools/test_hostlogic.cpp
//          -o tools/build/test_hostlogic
// No em dashes.
#include "calibration.h"
#include "orientation.h"
#include "angles.h"
#include <cstdio>
#include <cmath>

using namespace gaitway;

static int failures = 0;
static const double PI = 3.14159265358979323846;
#define CHECK(cond, msg) do { if (!(cond)) { printf("  FAIL: %s\n", msg); ++failures; } } while (0)

int main() {
    printf("[test_hostlogic]\n");

    // RunningStats: mean and stddev of a known set.
    {
        RunningStats s;
        double xs[] = {10.0, 12.0, 14.0, 16.0, 18.0};  // mean 14, sample stddev ~3.1623
        for (double x : xs) s.add(x);
        CHECK(std::fabs(s.mean - 14.0) < 1e-9, "running mean");
        CHECK(std::fabs(s.stddev() - 3.16227766) < 1e-6, "running sample stddev");
        CHECK(s.n == 5, "sample count");
    }
    // Perfectly still signal -> zero stddev, accepted by any positive threshold.
    {
        RunningStats s;
        for (int i = 0; i < 100; ++i) s.add(7.5);
        CHECK(s.stddev() < 1e-12, "still signal has zero stddev");
        CHECK(s.stddev() <= 1.0, "still signal passes stillness threshold 1.0");
    }

    // quatToEuler: identity quaternion is level.
    {
        float p, r, yw;
        quatToEuler(1.0f, 0.0f, 0.0f, 0.0f, p, r, yw);
        CHECK(std::fabs(p) < 1e-3 && std::fabs(r) < 1e-3 && std::fabs(yw) < 1e-3,
              "identity quaternion is 0 pitch/roll/yaw");
    }
    // 90 degree roll about x: w = x = cos/sin(45).
    {
        float p, r, yw;
        float c = std::cos(PI / 4.0), s = std::sin(PI / 4.0);
        quatToEuler(c, s, 0.0f, 0.0f, p, r, yw);
        CHECK(std::fabs(r - 90.0f) < 1e-2, "90 deg roll about x");
        CHECK(std::fabs(p) < 1e-2, "roll leaves pitch at 0");
    }
    // 30 degree pitch about y.
    {
        float p, r, yw;
        float c = std::cos(PI / 12.0), s = std::sin(PI / 12.0);  // 15 deg half angle -> 30 deg
        quatToEuler(c, 0.0f, s, 0.0f, p, r, yw);
        CHECK(std::fabs(p - 30.0f) < 1e-2, "30 deg pitch about y");
        CHECK(std::fabs(r) < 1e-2, "pitch leaves roll at 0");
    }

    // wrapDelta sanity at the +/-180 seam.
    {
        CHECK(std::fabs(wrapDelta(179.0f, -179.0f) - (-2.0f)) < 1e-3, "wrap across +180 seam");
        CHECK(std::fabs(wrapDelta(-179.0f, 179.0f) - (2.0f)) < 1e-3, "wrap across -180 seam");
        CHECK(std::fabs(wrapDelta(10.0f, 4.0f) - 6.0f) < 1e-3, "plain difference");
    }

    printf(failures == 0 ? "HOST LOGIC: PASS\n" : "HOST LOGIC: FAIL (%d)\n", failures);
    return failures == 0 ? 0 : 1;
}
