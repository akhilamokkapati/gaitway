// replay_harness.cpp, host test driver for the SHIPPED StepDetector.
// Compiled together with firmware/hub/src/detection.cpp so the tested code is
// exactly the code that runs on the hub. No em dashes.
//
// Input:  "time_ms pitch_deg" lines on stdin, one sample per line.
// Output: "STEP time_ms DIR" for every emitted step, then "WALK end DIR".
// Config: argv = on off t_confirm_ms t_refract_ms alpha walk_window_ms
#include "detection.h"
#include <cstdio>
#include <cstdlib>

using namespace gaitway;

int main(int argc, char** argv) {
    DetectorConfig cfg;
    cfg.swing_rate_on  = argc > 1 ? (float)atof(argv[1]) : 40.0f;
    cfg.swing_rate_off = argc > 2 ? (float)atof(argv[2]) : 15.0f;
    cfg.t_confirm_ms   = argc > 3 ? (uint16_t)atoi(argv[3]) : 30;
    cfg.t_refract_ms   = argc > 4 ? (uint16_t)atoi(argv[4]) : 250;
    cfg.rate_lp_alpha  = argc > 5 ? (float)atof(argv[5]) : 1.0f;
    cfg.walk_window_ms = argc > 6 ? (uint16_t)atoi(argv[6]) : 1500;

    StepDetector det(cfg);

    unsigned long t_ms = 0;
    double pitch = 0.0;
    unsigned long last_t = 0;
    while (scanf("%lu %lf", &t_ms, &pitch) == 2) {
        StepEvent ev = det.update((float)pitch, (uint32_t)t_ms);
        if (ev.fired) {
            const char* d = ev.direction == Dir::FWD ? "FWD"
                          : ev.direction == Dir::BWD ? "BWD" : "NONE";
            printf("STEP %lu %s\n", t_ms, d);
        }
        last_t = t_ms;
    }
    printf("WALK %lu %s\n", last_t, det.isWalking((uint32_t)last_t) ? "WALK" : "STAND");
    return 0;
}
