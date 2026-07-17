// test_packet_asym.cpp, host tests for the ESP-NOW packet CRC/validation and the
// asymmetry metric. Compile with -I firmware/hub/include -I firmware/common and
// firmware/hub/src/asymmetry.cpp. No em dashes.
#include "gaitway_packet.h"
#include "asymmetry.h"
#include <cstdio>
#include <cmath>

using namespace gaitway;

static int failures = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("  FAIL: %s\n", msg); ++failures; } } while (0)

static LeftNodePacket makePkt(int16_t pitch_cd, uint16_t seq) {
    LeftNodePacket p{};
    p.schema_version = GW_SCHEMA_VERSION;
    p.node_id        = NODE_LEFT_THIGH;
    p.node_millis    = 123456;
    p.pitch_cdeg     = pitch_cd;
    p.roll_cdeg      = -250;
    p.pitch_rate_cds = 4200;
    p.seq            = seq;
    p.status         = STATUS_SENSOR_OK;
    p.crc8           = leftPacketCrc(p);
    return p;
}

int main() {
    printf("[test_packet_asym]\n");

    // Packet: a well formed packet validates.
    {
        LeftNodePacket p = makePkt(1500, 7);
        CHECK(leftPacketValid(p), "valid packet passes");
    }
    // Corrupted payload fails the CRC.
    {
        LeftNodePacket p = makePkt(1500, 8);
        p.pitch_cdeg += 1;                 // change a byte without fixing the crc
        CHECK(!leftPacketValid(p), "corrupted packet rejected");
    }
    // Wrong schema or node id rejected even with a matching crc.
    {
        LeftNodePacket p = makePkt(0, 9);
        p.schema_version = 2; p.crc8 = leftPacketCrc(p);
        CHECK(!leftPacketValid(p), "wrong schema rejected");
        LeftNodePacket q = makePkt(0, 10);
        q.node_id = 0x03; q.crc8 = leftPacketCrc(q);
        CHECK(!leftPacketValid(q), "wrong node id rejected");
    }

    // Asymmetry: simple ratios over a full window.
    {
        AsymmetryMetric a(10);
        for (int i = 0; i < 5; ++i) { a.feedRightStep(10.0f, 500); a.feedLeftStep(5.0f, 450); }
        AsymResult r = a.result(false);
        CHECK(r.available, "asym available with both sides and fresh");
        CHECK(std::fabs(r.amplitude_ratio - 0.5f) < 1e-4, "amplitude ratio 0.5");
        CHECK(std::fabs(r.duration_ratio - 0.9f) < 1e-4, "duration ratio 0.9");
        CHECK(r.n_left == 5 && r.n_right == 5, "counts");
    }
    // Stale left data -> unavailable, never synthesized.
    {
        AsymmetryMetric a(10);
        a.feedRightStep(10.0f, 500); a.feedLeftStep(5.0f, 500);
        CHECK(!a.result(true).available, "stale left -> unavailable");
    }
    // One sided data -> unavailable.
    {
        AsymmetryMetric a(10);
        a.feedRightStep(10.0f, 500);
        CHECK(!a.result(false).available, "right only -> unavailable");
    }
    // Rolling window keeps only the most recent N steps.
    {
        AsymmetryMetric a(3);
        for (int v = 1; v <= 5; ++v) a.feedRightStep((float)v, 100);  // last 3 are 3,4,5 -> mean 4
        for (int i = 0; i < 3; ++i) a.feedLeftStep(4.0f, 100);
        AsymResult r = a.result(false);
        CHECK(r.available && std::fabs(r.amplitude_ratio - 1.0f) < 1e-4, "rolling window mean");
    }

    printf(failures == 0 ? "PACKET+ASYM: PASS\n" : "PACKET+ASYM: FAIL (%d)\n", failures);
    return failures == 0 ? 0 : 1;
}
