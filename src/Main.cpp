// Main.cpp — Cybiko-side firmware for the Pi TUI link.
//
// TODO(confirm): SCI channel. Set to 0 for now — change to whichever
// channel is actually free once you've settled the physical Pi<->H8S
// wiring (recall SCI1/SCI2 may already be claimed by the RF radio or a
// boot/debug console per RFSerial.cpp/BootSerial.cpp in emu2/).
#define SCI_CHANNEL 0

#include "Serial.hpp"
#include "LCDDriver.h"
#include "TextConsole.h"
#include "protocol.h"
#include "KeyboardScan.h"

static uint16_t prevKeyState[10] = {0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF};

static LCDDriver lcd;
static TextConsole console;

static uint8_t readByte() {
    return sciRead<SCI_CHANNEL>();
}

static bool sciAvailable() {
    return SSRx(SCI_CHANNEL) & SSR_RDRF;
}

static void toHex2(char* buf, uint8_t v) {
    const char* digits = "0123456789ABCDEF";
    buf[0] = digits[(v >> 4) & 0xF];
    buf[1] = digits[v & 0xF];
    buf[2] = ' ';
}

static void writeByte(uint8_t b) {
    sciWrite<SCI_CHANNEL>(b);
}

static void handlePendingFrame() {
    if(!sciAvailable()) {
        return;
    }

    Frame f = readFrame(readByte);

    switch (f.type) {
        /* DISPLAY */
        case CMD_CLEAR:
            console.clear();
            writeFrame(writeByte, EVT_ACK, nullptr, 0);
            break;

        case CMD_SET_CURSOR:
            if (f.len >= 2)
                console.setCursor(f.payload[0], f.payload[1]);
            writeFrame(writeByte, EVT_ACK, nullptr, 0);
            break;

        case CMD_WRITE_TEXT:
            console.writeText(reinterpret_cast<const char*>(f.payload), f.len);
            writeFrame(writeByte, EVT_ACK, nullptr, 0);
            break;

        case CMD_PUT_CHAR:
            if (f.len >= 3)
                console.putChar(f.payload[0], f.payload[1], f.payload[2]);
            break;

        /* ALIVE */
        case CMD_PING:
            writeFrame(writeByte, EVT_PONG, nullptr, 0);
            break;
        //TODO: Reset
        
        /* KEYS */
        //TODO: Map all keys

        /* VIBRATION */
        //TODO: Ouput Control
        //TODO: Timed output

        /* SPEAKER */
        //TODO: MIDI

        /* UNIMPLEMENTED */
        default:
            break; // unknown command, ignore
    }
}

int main() {
    // Seed the keyboard state
    uint16_t keyState[10];
    scanKeyboard(prevKeyState);

    // SMR=0x00: async, 8N1, CKS=00 (divider 1)
    // SCMR=0x00: default (unconfirmed against H8S/2323 reserved-bit
    //   requirements — first thing to check if bytes come out garbled)
    // BRR=4: with the 18.432MHz clock inferred from emu2's Main.cpp,
    //   this gives an exact 115200 baud (18432000 / (32*1*(4+1)) = 115200)
    sci0Init(0x00, 0x00, 4);
    SCR0 |= SCR_TE | SCR_RE; // sci0Init() leaves SCR0=0 — TE/RE must be set explicitly

    console.begin(&lcd);

    // TEMP: draw something immediately so you can confirm the console/font
    // renderer works before serial is wired up at all. Remove once you've
    // visually confirmed this shows correctly on the real panel.
    console.writeText("HELLO CYBIKO\nSERIAL PENDING", 27);

    for (;;) {
        handlePendingFrame();

        // keycode = col*16 + row (matches your Key Map image: col=A-line
        // index 0-9, row=D-line index 0-15). Runs every iteration
        // regardless of serial activity — prints hex to screen on press
        // even with nothing connected on the wire.
        // scanKeyboard(keyState);
        // for (int col = 0; col < 10; col++) {
        //     uint16_t changed = keyState[col] ^ prevKeyState[col];
        //     for (int row = 0; row < 16; row++) {
        //         if (changed & (1 << row)) {
        //             bool pressed = !(keyState[col] & (1 << row)); // active-low
        //             uint8_t keycode = col * 16 + row;

        //             if (pressed) {
        //                 char hex[4];
        //                 toHex2(hex, keycode);
        //                 hex[3] = 0;
        //                 console.writeText(hex, 3);
        //             }

        //             uint8_t payload[2] = { keycode, (uint8_t)(pressed ? 1 : 0) };
        //             writeFrame(writeByte, EVT_KEY, payload, 2);
        //         }
        //     }
        //     prevKeyState[col] = keyState[col];
        // }
    }

    return 0;
}