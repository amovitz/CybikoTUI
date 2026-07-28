// protocol.h — TLV framing: [SYNC:1][TYPE:1][LEN:1][PAYLOAD:LEN][CHECKSUM:1]
// checksum = XOR of TYPE, LEN, and all payload bytes (SYNC is not checksummed).
//
// REWRITTEN for the async-polling architecture: parsing is now a state
// machine (FrameParser) fed a few bytes at a time from whatever's
// currently sitting in the RX ring buffer. It never blocks and never
// spins -- if the buffer runs dry mid-frame, poll() just returns false
// and picks up exactly where it left off next time it's called, even if
// that's several main-loop iterations later. This matches a world where
// nothing can afford to sit and wait, not even briefly.
#pragma once
#include <stdint.h>

enum CommandType : uint8_t {
    // N/ACK
    EVT_ACK         = 0x00, // Bi-Directional, no payload
    EVT_NACK        = 0xFF, // Bi-Directional, no payload

    // Screen commands
    CMD_CLEAR       = 0xC1, // Pi -> Cybiko, no payload
    CMD_SET_CURSOR  = 0xC2, // Pi -> Cybiko, payload: row, col
    CMD_WRITE_TEXT  = 0xC3, // Pi -> Cybiko, payload: ASCII bytes
    CMD_PUT_CHAR    = 0xC4, // Pi -> Cybiko, payload: row, col, char

    // Ping/Pong
    CMD_PING        = 0xC0, // Pi -> Cybiko, no payload
    EVT_PONG        = 0xE0, // Cybiko -> Pi, no payload

    // Keyboard
    CMD_DUMP_KEYS   = 0xC6, // Pi -> Cybiko, no payload -- request a raw keyboard scan
    EVT_KEY         = 0xE1, // Cybiko -> Pi, payload: keycode, state (1=down,0=up)
    EVT_DEBUG       = 0xDB, // Cybiko -> Pi, payload: 10x uint16 LE (low byte=rows0-7, high byte=rows8-15), one pair per column in scan order
};

static const int MAX_PAYLOAD = 64;
static const uint8_t FRAME_SYNC = 0xAA; // chosen to not collide with any CommandType above

struct Frame {
    uint8_t type;
    uint8_t len;
    uint8_t payload[MAX_PAYLOAD];
};

// Incremental, non-blocking frame parser. One instance persists for the
// life of the program (own it in SerialBus.h) and gets fed bytes on every
// poll. State survives across calls, so a frame can arrive spread across
// many main-loop iterations without any of them blocking.
class FrameParser {
public:
    // TryReadByteFn contract: bool tryReadByte(uint8_t& out) -- true and
    // fills `out` if a byte was available, false if the queue is empty
    // right now. poll() drains as many bytes as are currently queued.
    //
    // Returns true exactly once a complete, checksum-valid frame lands in
    // `out` (that byte was the last one consumed this call). Returns
    // false otherwise -- either the queue ran dry mid-frame (call again
    // later, state is preserved) or a bad checksum was hit and silently
    // dropped (state already reset to hunt for the next SYNC, safe to
    // call again immediately or later, same as running dry).
    template<typename TryReadByteFn>
    bool poll(TryReadByteFn tryReadByte, Frame& out) {
        uint8_t b;
        while (tryReadByte(b)) {
            switch (state_) {
                case WAIT_SYNC:
                    if (b == FRAME_SYNC) state_ = READ_TYPE;
                    // anything else while hunting is noise -- ignore, keep hunting
                    break;

                case READ_TYPE:
                    frame_.type = b;
                    checksum_ = b;
                    state_ = READ_LEN;
                    break;

                case READ_LEN:
                    frame_.len = b;
                    checksum_ ^= b;
                    if (b > MAX_PAYLOAD) {
                        state_ = WAIT_SYNC; // bogus length -- resync, don't trust it
                        break;
                    }
                    payloadIdx_ = 0;
                    state_ = (b == 0) ? READ_CHECKSUM : READ_PAYLOAD;
                    break;

                case READ_PAYLOAD:
                    frame_.payload[payloadIdx_++] = b;
                    checksum_ ^= b;
                    if (payloadIdx_ >= frame_.len) state_ = READ_CHECKSUM;
                    break;

                case READ_CHECKSUM:
                    state_ = WAIT_SYNC; // always reset -- ready for the next frame either way
                    if (b == checksum_) {
                        out = frame_;
                        return true;
                    }
                    // bad checksum: drop silently, already resynced, keep draining
                    break;
            }
        }
        return false; // ran out of queued bytes without completing a frame this call
    }

private:
    enum State : uint8_t { WAIT_SYNC, READ_TYPE, READ_LEN, READ_PAYLOAD, READ_CHECKSUM };

    State state_ = WAIT_SYNC;
    Frame frame_{};
    uint8_t payloadIdx_ = 0;
    uint8_t checksum_ = 0;
};

// Sends a frame the same way it's parsed (Cybiko -> Pi direction, e.g. key events).
template<typename WriteByteFn>
void writeFrame(WriteByteFn writeByte, uint8_t type, const uint8_t* payload, uint8_t len) {
    uint8_t checksum = type ^ len;
    writeByte(FRAME_SYNC);
    writeByte(type);
    writeByte(len);
    for (int i = 0; i < len; i++) {
        writeByte(payload[i]);
        checksum ^= payload[i];
    }
    writeByte(checksum);
}