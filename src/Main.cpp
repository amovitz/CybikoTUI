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
#include "Protocol.h"
#include "KeyboardScan.h"
#include "SerialBus.h"

static uint16_t prevKeyState[10] = {0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF};

static LCDDriver lcd;
static TextConsole console;

static void handlePendingFrame() {
    Frame f;
    if (!tryGetFrame(f)) {
        return; // no complete frame yet -- fine, we'll pick up where we left off next call
    }

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
    // scanKeyboard(prevKeyState);

    serialInit();
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
        scanKeyboard(keyState);
        for (int col = 0; col < 10; col++) {
            handlePendingFrame();
            uint16_t changed = keyState[col] ^ prevKeyState[col];
            for (int row = 0; row < 16; row++) {
                handlePendingFrame();
                if (changed & (1 << row)) {
                    bool pressed = !(keyState[col] & (1 << row)); // active-low
                    uint8_t keycode = col * 16 + row;

                    if (pressed) {
                        char hex[4];
                        toHex2(hex, keycode);
                        hex[3] = 0;
                        console.writeText(hex, 3);
                    }

                    handlePendingFrame();

                    uint8_t payload[2] = { keycode, (uint8_t)(pressed ? 1 : 0) };
                    writeFrame(writeByte, EVT_KEY, payload, 2);
                }
            }
            prevKeyState[col] = keyState[col];
        }
    }

    return 0;
}