// test_lean.cpp, host test for the shipped LeanClassifier (GVS trigger logic).
// Compile: g++ -std=c++17 -I firmware/hub/include tools/test_lean.cpp
//          firmware/hub/src/lean_classifier.cpp -o tools/build/test_lean
// No em dashes.
#include "lean_classifier.h"
#include <cstdio>

using namespace gaitway;

static int failures = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("  FAIL: %s\n", msg); ++failures; } } while (0)

static const char* name(Lean l) {
    return l == Lean::LEFT ? "LEFT" : l == Lean::RIGHT ? "RIGHT" : "STOP";
}

// Hold d_roll from t_from to t_to at 10 ms steps, return the last state.
static Lean hold(LeanClassifier& c, float d_roll, uint32_t t_from, uint32_t t_to) {
    Lean s = c.state();
    for (uint32_t t = t_from; t <= t_to; t += 10) s = c.update(d_roll, t);
    return s;
}

int main() {
    printf("[test_lean]\n");
    LeanConfig cfg{4.5f, 2.0f, 100, 500};   // enter, exit, confirm ms, dwell ms

    // Enter RIGHT after the confirm window, with correct sign.
    {
        LeanClassifier c(cfg);
        CHECK(hold(c, 6.0f, 0, 90) == Lean::STOP, "not entered before confirm window");
        CHECK(c.update(6.0f, 100) == Lean::RIGHT, "enter RIGHT at confirm time");
        CHECK(hold(c, 6.0f, 110, 300) == Lean::RIGHT, "stays RIGHT while leaning");
        // Hysteresis: dropping into the exit band stops immediately.
        CHECK(c.update(1.5f, 310) == Lean::STOP, "exit to STOP inside exit band");
    }

    // Dead band between exit and enter never triggers.
    {
        LeanClassifier c(cfg);
        CHECK(hold(c, 3.0f, 0, 500) == Lean::STOP, "dead-band lean does not stimulate");
    }

    // A brief spike shorter than the confirm window is rejected.
    {
        LeanClassifier c(cfg);
        c.update(6.0f, 0);
        CHECK(c.update(6.0f, 50) == Lean::STOP, "50 ms spike does not enter");
        CHECK(c.update(0.0f, 60) == Lean::STOP, "returns to STOP after spike");
    }

    // LEFT direction from a negative lean.
    {
        LeanClassifier c(cfg);
        CHECK(hold(c, -6.0f, 0, 100) == Lean::LEFT, "enter LEFT on negative dRoll");
    }

    // Minimum dwell between opposite polarities.
    {
        LeanClassifier c(cfg);
        hold(c, 6.0f, 0, 100);                      // RIGHT entered at t=100
        CHECK(c.state() == Lean::RIGHT, "RIGHT entered");
        c.update(1.0f, 310);                        // stop
        CHECK(c.state() == Lean::STOP, "stopped before the flip");
        // Strong opposite lean, but within 500 ms of the RIGHT entry: held off.
        CHECK(hold(c, -6.0f, 320, 590) == Lean::STOP, "LEFT held off during dwell");
        // After the dwell since RIGHT entry (t=100) has elapsed: LEFT allowed.
        CHECK(c.update(-6.0f, 600) == Lean::LEFT, "LEFT enters once dwell elapsed");
    }

    printf(failures == 0 ? "LEAN: PASS\n" : "LEAN: FAIL (%d)\n", failures);
    return failures == 0 ? 0 : 1;
}
