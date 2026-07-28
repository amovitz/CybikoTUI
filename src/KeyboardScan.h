#pragma once

#include <stdint.h>
#include "SerialBus.h"

// Cyborn's original 9-column addressing scheme (confirmed working on real
// Cybiko Classic hardware, decoded from keypad.S): area 7 (0xE00000), with
// all of bits 1-19 held HIGH except the one column being selected. i.e.
//   addr(col) = 0xEFFFFF & ~(1 << col)
// The Xtreme adds a 10th column (A10, per the schematic/pinout), so this
// extends the identical pattern one bit further rather than guessing a new
// scheme. Bit 0 doesn't matter per the original comment, left set to match
// Cyborn's convention.
static constexpr uint32_t columns[10] =
{
    0xEFFFFD, // A1
    0xEFFFFB, // A2
    0xEFFFF7, // A3
    0xEFFFEF, // A4
    0xEFFFDF, // A5
    0xEFFFBF, // A6
    0xEFFF7F, // A7
    0xEFFEFF, // A8
    0xEFFDFF, // A9
    0xEFFBFF, // A10 -- new column, not present on the Classic
};

static uint16_t columnMasks[10] =
{
    0x80FE, // A1
    0xFF00, // A2
    0x80FE, // A3
    0xFF00, // A4
    0xFF86, // A5
    0xF87E, // A6
    0x87FE, // A7
    0x7FFF, // A8
    0x7FFF, // A9
    0xFFE0, // A10
};
 
// Returns non-zero if any key in the matrix is pressed.
// Equivalent to the assembly's __keypressed.
inline bool anyKeyPressed()
{
    pollSerial();

    return true;
    
    // Below causes issues with serial responsivness,
    //   suspect hitting a reg I shouldn't.

    // volatile uint16_t* ptr =
    //     reinterpret_cast<volatile uint16_t*>(0x00EFF801);

    // return *ptr != 0xFFFF;
}
 
inline uint16_t scanColumn(int column)
{
    pollSerial();

    if (column < 0 || column >= 10 || !anyKeyPressed())
        return 0xFFFF;
        
    volatile uint16_t* p =
        reinterpret_cast<volatile uint16_t*>(columns[column]);

    pollSerial();
 
    return *p | columnMasks[column];
}


inline uint16_t scanRawAddress(uint32_t addr)
{

    anyKeyPressed();
    
    volatile uint16_t* p =
        reinterpret_cast<volatile uint16_t*>(addr);

    pollSerial();
    
    return *p;
}

inline void scanKeyboard(uint16_t state[10])
{
    for (int column = 0; column < 10; ++column) {
        state[column] = scanColumn(column);
    }
}

static uint16_t baseline[10];

static void captureKeyboardBaseline()
{
    for (int i = 0; i < 10; ++i)
    {
        pollSerial();

        volatile uint16_t* p = reinterpret_cast<volatile uint16_t*>(columns[i]);

        baseline[i] = *p | columnMasks[i];
    }
}

static void scanKeyboardDiffs()
{
    uint8_t payload[7];

    for (int i = 0; i < 10; ++i)
    {
        pollSerial();

        if (!anyKeyPressed()) {
            baseline[i] = 0xFFFF;
            continue;
        }
        
        volatile uint16_t* p = reinterpret_cast<volatile uint16_t*>(columns[i]);

        uint16_t now = *p | columnMasks[i];
        uint16_t diff = now ^ baseline[i];

        if (diff)
        {
            payload[0] = i+1;

            payload[1] = (columns[i] >> 8) & 0xFF;
            payload[2] = columns[i] & 0xFF;

            payload[3] = (baseline[i] >> 8) & 0xFF;
            payload[4] = baseline[i] & 0xFF;

            payload[5] = now >> 8;
            payload[6] = now & 0xFF;

            writeFrame(writeByte, EVT_DEBUG, payload, sizeof(payload));

            baseline[i] = now;
        }
    }
}