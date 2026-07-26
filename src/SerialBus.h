#pragma once
#include "Serial.hpp"
#include "Protocol.h"

static uint8_t rxBuf[64];
static uint8_t rxHead = 0, rxTail = 0;

// CHANGED: SerialBus now owns the frame parser too, not just raw bytes.
// This one instance persists across calls -- a frame can take several
// main-loop iterations to fully arrive and that's fine, state carries
// over between them.
static FrameParser rxParser;

static void serialInit(void) {
    // SMR=0x00: async, 8N1, CKS=00 (divider 1)
    // SCMR=0x00: default (unconfirmed against H8S/2323 reserved-bit
    //   requirements — first thing to check if bytes come out garbled)
    // BRR=4: with the 18.432MHz clock inferred from emu2's Main.cpp,
    //   this gives an exact 115200 baud (18432000 / (32*1*(4+1)) = 115200)
    sci0Init(0x00, 0x00, 4);
    SCR0 |= SCR_TE | SCR_RE; // sci0Init() leaves SCR0=0 — TE/RE must be set explicitly
}

static void toHex2(char* buf, uint8_t v) {
    const char* digits = "0123456789ABCDEF";
    buf[0] = digits[(v >> 4) & 0xF];
    buf[1] = digits[v & 0xF];
    buf[2] = ' ';
}

// Cheap, non-blocking. Call this constantly -- inside loops, between bus
// reads, anywhere you're spending more than a few cycles. This is the
// ONLY place that touches the hardware SSR/RDR registers.
inline void pollSerial(void) {
    uint8_t status = SSRx(SCI_CHANNEL);
    if (status & SSR_ORER) {
        SSRx(SCI_CHANNEL) &= ~SSR_ORER;
    }
    if (status & SSR_RDRF) {
        uint8_t b = RDRx(SCI_CHANNEL);
        SSRx(SCI_CHANNEL) &= ~SSR_RDRF;
        uint8_t next = (uint8_t)((rxHead + 1) % sizeof(rxBuf));
        if (next != rxTail) {          // drop on full buffer, don't corrupt indices
            rxBuf[rxHead] = b;
            rxHead = next;
        }
    }
}

// Non-blocking pull from the software buffer. Used internally by
// tryGetFrame() -- you shouldn't need to call this directly anymore.
static bool tryReadByte(uint8_t& out) {
    if (rxHead == rxTail) return false;
    out = rxBuf[rxTail];
    rxTail = (uint8_t)((rxTail + 1) % sizeof(rxBuf));
    return true;
}

// NEW: the one function callers actually need. Drains any hardware byte
// that's arrived, feeds everything currently queued through the parser,
// and returns true only when a complete valid frame comes out the other
// end. Never blocks, never spins beyond what's already buffered -- safe
// to call every iteration of the main loop no matter what else is going
// on. Replaces the old sciAvailable() + readFrame(readByte) pairing.
static bool tryGetFrame(Frame& f) {
    pollSerial();
    return rxParser.poll(tryReadByte, f);
}

static void writeByte(uint8_t b) {
    sciWrite<SCI_CHANNEL>(b);
}