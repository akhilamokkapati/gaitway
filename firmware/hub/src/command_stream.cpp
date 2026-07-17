// command_stream.cpp, see command_stream.h. Pure logic, no Arduino. No em dashes.
#include "command_stream.h"
#include <cstring>

namespace gaitway {

CommandStream::CommandStream(const char* heartbeat_line, uint16_t min_gap_ms, uint16_t heartbeat_ms)
    : min_gap_ms_(min_gap_ms), heartbeat_ms_(heartbeat_ms), last_ms_(0), initialised_(false) {
    std::strncpy(hb_, heartbeat_line, CAP - 1);
    hb_[CAP - 1] = '\0';
    cur_[0] = '\0';
}

const char* CommandStream::update(const char* desired, uint32_t now_ms) {
    if (!initialised_) {
        initialised_ = true;
        // Allow the first line to go out immediately (gap already satisfied).
        last_ms_ = now_ms - min_gap_ms_;
        cur_[0]  = '\0';
    }

    bool gap_ok = (uint32_t)(now_ms - last_ms_) >= min_gap_ms_;

    if (std::strcmp(desired, cur_) != 0) {
        // State change (edge). Emit as soon as the minimum gap allows.
        if (gap_ok) {
            std::strncpy(cur_, desired, CAP - 1);
            cur_[CAP - 1] = '\0';
            last_ms_ = now_ms;
            return cur_;
        }
        return nullptr;  // edge pending, still inside the gap
    }

    // Steady state: send a heartbeat after enough silence.
    if ((uint32_t)(now_ms - last_ms_) >= heartbeat_ms_) {
        last_ms_ = now_ms;
        return hb_;
    }
    return nullptr;
}

} // namespace gaitway
