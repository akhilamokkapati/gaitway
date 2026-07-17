// command_stream.h, edge-triggered command line emitter for the hub outputs.
// Pure logic, NO Arduino dependency, host testable. No em dashes.
//
// Both hub output streams (GW1 to actuation, GV1 to GVS) follow the same rules
// (spec 6): ASCII lines, edge triggered on state change, a heartbeat every 500 ms
// of silence, and a minimum 100 ms gap between any two lines (the v1 command
// flooding fix). This class owns that timing so both callers stay identical and
// the behaviour is verified once on the host.
//
// Usage each loop: pass the currently desired command line (for example
// "GW1,ASSIST,FWD" or "GW1,STOP"). update() returns a line to transmit now, or
// nullptr when nothing should be sent yet.
#pragma once
#include <cstdint>
#include <cstddef>

namespace gaitway {

class CommandStream {
public:
    // heartbeat_line is the exact HB text for this stream, e.g. "GW1,HB".
    CommandStream(const char* heartbeat_line, uint16_t min_gap_ms, uint16_t heartbeat_ms);

    // Returns a line to send now (owned by this object, valid until next call),
    // or nullptr if nothing should be sent at time now_ms.
    const char* update(const char* desired, uint32_t now_ms);

    uint32_t lastSentMs() const { return last_ms_; }

private:
    static const size_t CAP = 24;
    char     hb_[CAP];
    char     cur_[CAP];      // last state line actually sent
    uint16_t min_gap_ms_;
    uint16_t heartbeat_ms_;
    uint32_t last_ms_;       // time of the last line of any kind
    bool     initialised_;
};

} // namespace gaitway
