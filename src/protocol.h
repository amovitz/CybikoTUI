// protocol.h — TLV framing: [TYPE:1][LEN:1][PAYLOAD:LEN][CHECKSUM:1]
// checksum = XOR of TYPE, LEN, and all payload bytes.
#pragma once
#include <stdint.h>

enum CommandType : uint8_t {
    CMD_CLEAR       = 0x01, // Pi -> Cybiko, no payload
    CMD_SET_CURSOR  = 0x02, // Pi -> Cybiko, payload: row, col
    CMD_WRITE_TEXT  = 0x03, // Pi -> Cybiko, payload: ASCII bytes
    CMD_PUT_CHAR    = 0x04, // Pi -> Cybiko, payload: row, col, char
    CMD_PING        = 0x05, // Pi -> Cybiko, no payload

    EVT_KEY         = 0x81, // Cybiko -> Pi, payload: keycode, state (1=down,0=up)
    EVT_PONG        = 0x82, // Cybiko -> Pi, no payload
};

static const int MAX_PAYLOAD = 64;

struct Frame {
    uint8_t type;
    uint8_t len;
    uint8_t payload[MAX_PAYLOAD];
};

// Reads one byte at a time via the supplied blocking read function until a
// full, checksum-valid frame is assembled. Bad checksums are silently
// dropped and the parser resyncs on the next TYPE byte — simple and robust
// enough for a point-to-point link, though it means a single dropped byte
// mid-frame costs you that one frame rather than corrupting state.
template<typename ReadByteFn>
Frame readFrame(ReadByteFn readByte) {
    Frame f;
    for (;;) {
        f.type = readByte();
        f.len = readByte();
        if (f.len > MAX_PAYLOAD) continue; // resync: bogus length, try again

        uint8_t checksum = f.type ^ f.len;
        for (int i = 0; i < f.len; i++) {
            f.payload[i] = readByte();
            checksum ^= f.payload[i];
        }
        uint8_t rxChecksum = readByte();
        if (rxChecksum == checksum)
            return f;
        // checksum mismatch: drop this frame, loop and try to resync
    }
}

// Sends a frame the same way it's parsed (Cybiko -> Pi direction, e.g. key events).
template<typename WriteByteFn>
void writeFrame(WriteByteFn writeByte, uint8_t type, const uint8_t* payload, uint8_t len) {
    uint8_t checksum = type ^ len;
    writeByte(type);
    writeByte(len);
    for (int i = 0; i < len; i++) {
        writeByte(payload[i]);
        checksum ^= payload[i];
    }
    writeByte(checksum);
}
