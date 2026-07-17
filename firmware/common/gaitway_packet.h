// gaitway_packet.h, shared ESP-NOW packet schema for the left thigh node.
// Included by BOTH firmware/left_node and firmware/hub (via -I ../common) so the
// two sides can never disagree on the wire format. Pure logic, NO Arduino
// dependency, host testable. No em dashes.
#pragma once
#include <cstdint>
#include <cstddef>

namespace gaitway {

static const uint8_t GW_SCHEMA_VERSION   = 1;
static const uint8_t NODE_LEFT_THIGH     = 0x02;
static const uint8_t STATUS_SENSOR_OK    = 0x01;   // status bit0
static const uint8_t STATUS_BATTERY_LOW  = 0x02;   // status bit1

// Left node telemetry, sent every 20 ms (50 Hz). Packed little-endian, 16 bytes.
#pragma pack(push, 1)
struct LeftNodePacket {
    uint8_t  schema_version;   // = GW_SCHEMA_VERSION
    uint8_t  node_id;          // = NODE_LEFT_THIGH
    uint32_t node_millis;      // sender clock, for staleness detection
    int16_t  pitch_cdeg;       // centidegrees
    int16_t  roll_cdeg;        // centidegrees
    int16_t  pitch_rate_cds;   // centideg/s, computed on the node
    uint16_t seq;              // rolling sequence number
    uint8_t  status;           // bit0 sensor_ok, bit1 battery_low
    uint8_t  crc8;             // over the first 15 bytes
};
#pragma pack(pop)
static_assert(sizeof(LeftNodePacket) == 16, "LeftNodePacket must be 16 packed bytes");

// Hub -> node calibration request. Distinct 4 byte packet.
#pragma pack(push, 1)
struct CalRequestPacket {
    uint8_t schema_version;    // = GW_SCHEMA_VERSION
    uint8_t node_id;           // = NODE_LEFT_THIGH (target)
    uint8_t cmd;               // 1 = capture standing baseline
    uint8_t crc8;              // over the first 3 bytes
};
#pragma pack(pop)
static_assert(sizeof(CalRequestPacket) == 4, "CalRequestPacket must be 4 packed bytes");

static const uint8_t CAL_CMD_CAPTURE = 1;

// CRC-8 (polynomial 0x07, init 0x00) over len bytes.
inline uint8_t gwCrc8(const uint8_t* data, size_t len) {
    uint8_t crc = 0x00;
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (int b = 0; b < 8; ++b)
            crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x07) : (uint8_t)(crc << 1);
    }
    return crc;
}

inline uint8_t leftPacketCrc(const LeftNodePacket& p) {
    return gwCrc8(reinterpret_cast<const uint8_t*>(&p), sizeof(LeftNodePacket) - 1);
}
inline bool leftPacketValid(const LeftNodePacket& p) {
    return p.schema_version == GW_SCHEMA_VERSION &&
           p.node_id == NODE_LEFT_THIGH &&
           p.crc8 == leftPacketCrc(p);
}

inline uint8_t calRequestCrc(const CalRequestPacket& p) {
    return gwCrc8(reinterpret_cast<const uint8_t*>(&p), sizeof(CalRequestPacket) - 1);
}

} // namespace gaitway
