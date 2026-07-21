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

static LCDDriver lcd;
static TextConsole console;

static uint8_t readByte() {
    return sciRead<SCI_CHANNEL>();
}

static void writeByte(uint8_t b) {
    sciWrite<SCI_CHANNEL>(b);
}

int main() {
    // SMR=0x00: async, 8N1, CKS=00 (divider 1)
    // SCMR=0x00: default (unconfirmed against H8S/2323 reserved-bit
    //   requirements — first thing to check if bytes come out garbled)
    // BRR=4: with the 18.432MHz clock inferred from emu2's Main.cpp,
    //   this gives an exact 115200 baud (18432000 / (32*1*(4+1)) = 115200)
    sci0Init(0x00, 0x00, 4);

    console.begin(&lcd);

    // TEMP: draw something immediately so you can confirm the console/font
    // renderer works before serial is wired up at all. Remove once you've
    // visually confirmed this shows correctly on the real panel.
    console.writeText("HELLO CYBIKO\nSERIAL PENDING", 26);

    for (;;) {
        Frame f = readFrame(readByte);

        switch (f.type) {
            case CMD_CLEAR:
                console.clear();
                break;

            case CMD_SET_CURSOR:
                if (f.len >= 2)
                    console.setCursor(f.payload[0], f.payload[1]);
                break;

            case CMD_WRITE_TEXT:
                console.writeText(reinterpret_cast<const char*>(f.payload), f.len);
                break;

            case CMD_PUT_CHAR:
                if (f.len >= 3)
                    console.putChar(f.payload[0], f.payload[1], f.payload[2]);
                break;

            case CMD_PING:
                writeFrame(writeByte, EVT_PONG, nullptr, 0);
                break;

            default:
                break; // unknown command, ignore
        }

        // TODO: hook up keyboard scanning here and call
        // writeFrame(writeByte, EVT_KEY, keyPayload, 2) on key events.
        // Haven't seen the keyboard driver yet (not in lib/H8 — may live
        // elsewhere in the repo, or need writing from scratch), so this
        // is a stub for now.
    }

    return 0;
}
