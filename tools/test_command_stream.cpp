// test_command_stream.cpp, host test for the shipped CommandStream timing logic.
// Compile: g++ -std=c++17 -I firmware/hub/include tools/test_command_stream.cpp
//          firmware/hub/src/command_stream.cpp -o tools/build/test_command_stream
// No em dashes.
#include "command_stream.h"
#include <cstdio>
#include <cstring>
#include <vector>

using namespace gaitway;

static int failures = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("  FAIL: %s\n", msg); ++failures; } } while (0)

struct Step { uint32_t t; const char* desired; const char* expect; };  // expect nullptr = no line

int main() {
    printf("[test_command_stream]\n");

    CommandStream s("GW1,HB", 100, 500);   // 100 ms gap, 500 ms heartbeat

    std::vector<Step> steps = {
        {0,   "GW1,STOP",       "GW1,STOP"},        // first line goes out at once
        {20,  "GW1,STOP",       nullptr},           // no change, no heartbeat yet
        {50,  "GW1,ASSIST,FWD", nullptr},           // edge, but inside the 100 ms gap
        {100, "GW1,ASSIST,FWD", "GW1,ASSIST,FWD"},  // gap satisfied, emit the change
        {300, "GW1,ASSIST,FWD", nullptr},           // steady, heartbeat not due
        {590, "GW1,ASSIST,FWD", nullptr},           // still under 500 ms of silence
        {600, "GW1,ASSIST,FWD", "GW1,HB"},          // 500 ms since last line -> heartbeat
        {650, "GW1,STOP",       nullptr},           // edge inside gap after the heartbeat
        {700, "GW1,STOP",       "GW1,STOP"},        // gap satisfied
        // two rapid changes inside one gap collapse to the latest desired
        {720, "GW1,ASSIST,FWD", nullptr},
        {740, "GW1,ASSIST,BWD", nullptr},
        {800, "GW1,ASSIST,BWD", "GW1,ASSIST,BWD"},
    };

    std::vector<uint32_t> emitTimes;
    for (const auto& st : steps) {
        const char* line = s.update(st.desired, st.t);
        if (st.expect == nullptr) {
            CHECK(line == nullptr, st.desired);
            if (line != nullptr) printf("    at t=%u got '%s', expected nothing\n", st.t, line);
        } else {
            CHECK(line != nullptr && std::strcmp(line, st.expect) == 0, st.expect);
            if (line == nullptr) printf("    at t=%u got nothing, expected '%s'\n", st.t, st.expect);
            else if (std::strcmp(line, st.expect) != 0)
                printf("    at t=%u got '%s', expected '%s'\n", st.t, line, st.expect);
            if (line) emitTimes.push_back(st.t);
        }
    }

    // No two emitted lines closer than the 100 ms minimum gap.
    for (size_t i = 1; i < emitTimes.size(); ++i)
        CHECK(emitTimes[i] - emitTimes[i - 1] >= 100, "minimum 100 ms spacing between lines");

    printf(failures == 0 ? "COMMAND STREAM: PASS\n" : "COMMAND STREAM: FAIL (%d)\n", failures);
    return failures == 0 ? 0 : 1;
}
