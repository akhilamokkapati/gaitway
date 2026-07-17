// rvc_parser.h, BNO085 UART-RVC frame parser for the right thigh sensor.
// Pure logic, NO Arduino dependency, host testable. No em dashes.
//
// Frame layout (spec 4.1, handoff 4), 19 bytes at 100 Hz, 115200 8N1:
//   byte 0-1  header 0xAA 0xAA
//   byte 2    index
//   byte 3-4  yaw   (int16 LE, centidegrees)
//   byte 5-6  pitch (int16 LE, centidegrees)
//   byte 7-8  roll  (int16 LE, centidegrees)
//   byte 9-14 x/y/z accel (int16 LE, mg)
//   byte 15-17 reserved
//   byte 18   checksum = low byte of sum of bytes 2..17
//
// VERIFY this layout against the real sensor at bring up. If frameErrorCount is
// high, suspect the checksum range (bytes 2..17) or the reserved bytes first.
// The parser resynchronises: it scans for the header, validates the checksum,
// drops bad frames silently but counts them, and exposes the last valid time.
#pragma once
#include <cstdint>

namespace gaitway {

struct RvcFrame {
    uint8_t index;
    float   yaw_deg;
    float   pitch_deg;
    float   roll_deg;
    float   ax_g;
    float   ay_g;
    float   az_g;
};

class RvcParser {
public:
    RvcParser();
    void reset();

    // Feed one received byte at time now_ms. Returns true when a valid frame
    // completes, filling out. Returns false otherwise (mid frame or bad frame).
    bool feedByte(uint8_t b, uint32_t now_ms, RvcFrame& out);

    uint32_t frameErrorCount() const { return err_; }
    uint32_t validCount()      const { return valid_; }
    uint32_t lastValidMillis() const { return last_valid_ms_; }

private:
    static bool checksumOk(const uint8_t* b);
    static void decode(const uint8_t* b, RvcFrame& out);

    uint8_t  buf_[19];    // sliding window of the most recent bytes
    uint8_t  have_;       // bytes currently in the window (0..19)
    uint32_t err_;
    uint32_t valid_;
    uint32_t last_valid_ms_;
};

} // namespace gaitway
