// Main.cpp — Cybiko-side firmware for the Pi TUI link.
//
#define SCI_CHANNEL 0
#define KEYBOARD_POLL 25


#include "Serial.hpp"
#include "LCDDriver.h"
#include "TextConsole.h"
#include "Protocol.h"
#include "KeyDecode.h"
#include "SerialBus.h"


static uint16_t keyState[10];
static uint16_t prevKeyState[10] = {0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF};

static LCDDriver lcd;
static TextConsole console;


//
// Frame decoding/handling
//
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
        
        //TODO: Raw draw for images

        /* ALIVE */
        case CMD_PING:
            writeFrame(writeByte, EVT_PONG, nullptr, 0);
            break;

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

//
// Main
//
int main() {
    // Init the screen
    console.begin(&lcd);
    console.writeText("HELLO CYBIKO\nSERIAL PENDING", 27);
    
    // Init the keyboard
    int8_t pollKeyboardInterval = KEYBOARD_POLL;
    initKeyboard();
    pollKeyboard();

    // Init the serial bus
    serialInit();

    for (;;) {
        // Serial Comms
        handlePendingFrame();

        // Keyboard Input
        if (--pollKeyboardInterval <= 0) {
            pollKeyboard();
            pollKeyboardInterval = KEYBOARD_POLL;
        }
    }

    return 0;
}