// KeyboardScan.h — address/data-bus matrix scan, algorithm confirmed from
// emu2/KeyboardDevice.cpp (XtremeKeyboardDevice::read). Column = address
// bits 1-10 (A1-A10), one cleared to select; row = 16 bits per column
// (D0-D15), split into high/low byte via address bit 0. Active-low:
// pressed = 0, released = 1.
#pragma once
#include <stdint.h>
#include "SerialBus.h"

// Confirmed from emu2/Main.cpp: cpu.setExternalArea(7, keyboard.get()).
// The H8S bus splits the 24-bit external address space into 8 areas of
// 2MB each (area N = N*0x200000 to N*0x200000+0x1FFFFF); area 7 is
// 0xE00000-0xFFFFFF. My earlier guess (0x700000) was wrong — that's
// inside area 3, the SAME area as flash, so those reads were hitting
// flash's bus response instead of a floating bus. This one's grounded
// in the actual area-to-device mapping, not elimination-by-linker-script.
static const uint32_t KBD_BASE = 0x00E00000;

inline uint16_t scanColumn(int col) { // col: 0-9
    uint32_t addr = KBD_BASE;
    for (int i = 1; i <= 10; i++)
        addr |= (1u << i);           // all column bits set (deselected)...
    addr &= ~(1u << (col + 1));      // ...except the one we're selecting

    volatile uint8_t* lowPtr  = reinterpret_cast<volatile uint8_t*>(addr | 1);
    volatile uint8_t* highPtr = reinterpret_cast<volatile uint8_t*>(addr & ~1u);
    pollSerial();
    uint8_t lowByte  = *lowPtr;   // rows 0-7
    pollSerial();
    uint8_t highByte = *highPtr; // rows 8-15
    pollSerial();
    return (static_cast<uint16_t>(highByte) << 8) | lowByte;
}

// Scans all 10 columns. Bit clear (0) = pressed, set (1) = released.
inline void scanKeyboard(uint16_t state[10]) {
    for (int col = 0; col < 10; col++)
        state[col] = scanColumn(col);
}