// test_rvc.cpp, host test for the shipped RvcParser (firmware/hub/src/rvc_parser.cpp).
// Compile: g++ -std=c++17 -I firmware/hub/include tools/test_rvc.cpp
//          firmware/hub/src/rvc_parser.cpp -o tools/build/test_rvc
// No em dashes.
#include "rvc_parser.h"
#include <cstdio>
#include <cstdint>
#include <cmath>
#include <vector>

using namespace gaitway;

static int failures = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("  FAIL: %s\n", msg); ++failures; } } while (0)

// Build a valid 19 byte RVC frame with the correct checksum.
static std::vector<uint8_t> makeFrame(uint8_t index, int16_t yaw_cd, int16_t pitch_cd,
                                      int16_t roll_cd, int16_t ax, int16_t ay, int16_t az) {
    std::vector<uint8_t> f(19, 0);
    f[0] = 0xAA; f[1] = 0xAA; f[2] = index;
    auto put = [&](int i, int16_t v) { f[i] = (uint8_t)(v & 0xFF); f[i + 1] = (uint8_t)((v >> 8) & 0xFF); };
    put(3, yaw_cd); put(5, pitch_cd); put(7, roll_cd);
    put(9, ax); put(11, ay); put(13, az);
    // bytes 15..17 reserved = 0
    uint8_t sum = 0;
    for (int i = 2; i <= 17; ++i) sum = (uint8_t)(sum + f[i]);
    f[18] = sum;
    return f;
}

static int feed(RvcParser& p, const std::vector<uint8_t>& bytes, uint32_t& t, RvcFrame& last) {
    int decoded = 0;
    for (uint8_t b : bytes) {
        RvcFrame fr;
        if (p.feedByte(b, t++, fr)) { last = fr; ++decoded; }
    }
    return decoded;
}

int main() {
    printf("[test_rvc]\n");

    // 1. A valid frame decodes with correct values.
    {
        RvcParser p; uint32_t t = 0; RvcFrame last{};
        auto f = makeFrame(7, 4500, 1234, -678, 10, -20, 990);
        int d = feed(p, f, t, last);
        CHECK(d == 1, "valid frame decodes exactly once");
        CHECK(p.validCount() == 1 && p.frameErrorCount() == 0, "counts: 1 valid, 0 error");
        CHECK(last.index == 7, "index decoded");
        CHECK(std::fabs(last.yaw_deg   - 45.00f) < 1e-3, "yaw centideg -> deg");
        CHECK(std::fabs(last.pitch_deg - 12.34f) < 1e-3, "pitch centideg -> deg");
        CHECK(std::fabs(last.roll_deg  - (-6.78f)) < 1e-3, "roll sign + scale");
        CHECK(std::fabs(last.az_g - 0.990f) < 1e-3, "accel mg -> g");
        CHECK(p.lastValidMillis() != 0, "last valid timestamp set");
    }

    // 2. A corrupted checksum is rejected and counted, not decoded.
    {
        RvcParser p; uint32_t t = 0; RvcFrame last{};
        auto f = makeFrame(1, 100, 200, 300, 0, 0, 0);
        f[18] ^= 0xFF;  // break checksum
        int d = feed(p, f, t, last);
        CHECK(d == 0, "corrupt frame not decoded");
        CHECK(p.validCount() == 0 && p.frameErrorCount() == 1, "counts: 0 valid, 1 error");
    }

    // 3. Leading garbage then valid frames: parser resyncs on the header.
    {
        RvcParser p; uint32_t t = 0; RvcFrame last{};
        std::vector<uint8_t> stream = {0x00, 0xFF, 0xAA, 0x00, 0x13, 0xAA};  // junk incl stray 0xAA
        for (int k = 0; k < 3; ++k) {
            auto f = makeFrame((uint8_t)k, 0, (int16_t)(k * 100), 0, 0, 0, 0);
            stream.insert(stream.end(), f.begin(), f.end());
        }
        int d = feed(p, stream, t, last);
        CHECK(d == 3, "3 valid frames decoded after leading garbage");
        CHECK(std::fabs(last.pitch_deg - 2.00f) < 1e-3, "last frame value correct");
    }

    // 4. A dropped byte desyncs one frame, then the parser recovers.
    {
        RvcParser p; uint32_t t = 0; RvcFrame last{};
        auto a = makeFrame(0, 0, 500, 0, 0, 0, 0);
        auto adrop = a; adrop.erase(adrop.begin() + 10);  // lose one payload byte
        feed(p, adrop, t, last);
        int recovered = 0;
        for (int k = 0; k < 5; ++k) {
            auto f = makeFrame((uint8_t)k, 0, (int16_t)(1000 + k), 0, 0, 0, 0);
            recovered += feed(p, f, t, last);
        }
        CHECK(recovered >= 4, "parser recovers and decodes following frames");
        CHECK(p.frameErrorCount() >= 1, "the dropped byte was counted as an error");
    }

    // 5. A 0xAA byte inside the payload must not cause a false resync.
    {
        RvcParser p; uint32_t t = 0; RvcFrame last{};
        auto f = makeFrame(0xAA, 0, 4242, 0, 0, 0, 0);  // index byte is 0xAA
        int d = feed(p, f, t, last);
        CHECK(d == 1, "frame with 0xAA payload byte still decodes");
        CHECK(std::fabs(last.pitch_deg - 42.42f) < 1e-3, "value intact");
    }

    printf(failures == 0 ? "RVC PARSER: PASS\n" : "RVC PARSER: FAIL (%d)\n", failures);
    return failures == 0 ? 0 : 1;
}
