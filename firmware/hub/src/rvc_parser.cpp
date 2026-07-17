// rvc_parser.cpp, BNO085 UART-RVC parser implementation.
// Pure logic, NO Arduino dependency. No em dashes. See rvc_parser.h.
//
// Resync strategy: a sliding 19 byte window. Every time the window is full we
// test whether it currently frames a valid packet (header 0xAA 0xAA plus a
// matching checksum). On a hit we emit and start a fresh window, on a miss we
// drop the oldest byte and slide by one. This reacquires after ANY corruption
// (dropped byte, stray 0xAA before a header, leading garbage), losing the
// minimum possible data, which a fixed state machine cannot do for the triple
// 0xAA case.
#include "rvc_parser.h"

namespace gaitway {

static inline int16_t le16(const uint8_t* p) {
    return (int16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

RvcParser::RvcParser() { reset(); }

void RvcParser::reset() {
    have_          = 0;
    err_           = 0;
    valid_         = 0;
    last_valid_ms_ = 0;
}

bool RvcParser::checksumOk(const uint8_t* b) {
    uint8_t sum = 0;
    for (int i = 2; i <= 17; ++i) sum = (uint8_t)(sum + b[i]);  // bytes 2..17
    return sum == b[18];
}

void RvcParser::decode(const uint8_t* b, RvcFrame& out) {
    out.index     = b[2];
    out.yaw_deg   = le16(&b[3])  / 100.0f;
    out.pitch_deg = le16(&b[5])  / 100.0f;
    out.roll_deg  = le16(&b[7])  / 100.0f;
    out.ax_g      = le16(&b[9])  / 1000.0f;
    out.ay_g      = le16(&b[11]) / 1000.0f;
    out.az_g      = le16(&b[13]) / 1000.0f;
}

bool RvcParser::feedByte(uint8_t b, uint32_t now_ms, RvcFrame& out) {
    if (have_ < 19) {
        buf_[have_++] = b;
    } else {
        // Window full, slide by one: drop oldest, append newest.
        for (int i = 0; i < 18; ++i) buf_[i] = buf_[i + 1];
        buf_[18] = b;
    }

    if (have_ < 19) return false;

    // Only a window that starts on the header is a frame candidate.
    if (buf_[0] == 0xAA && buf_[1] == 0xAA) {
        if (checksumOk(buf_)) {
            decode(buf_, out);
            ++valid_;
            last_valid_ms_ = now_ms;
            have_ = 0;              // start a fresh window after a clean frame
            return true;
        }
        // Header aligned but checksum failed: a genuine corrupt frame.
        ++err_;
    }
    // Otherwise keep sliding on the next byte until a frame aligns.
    return false;
}

} // namespace gaitway
