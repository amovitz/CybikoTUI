#pragma once

#include <string.h>
#include "KeyboardScan.h"

/*
 * Resolves raw column scans into actual key events and emits EVT_KEY.
 *
 * Two calls, that's the whole API:
 *   initKeyboard()   -- once at startup
 *   pollKeyboard()   -- once per main loop iteration
 *
 * This file assumes EVT_KEY is defined in SerialBus.h alongside EVT_DEBUG.
 * If it isn't yet, add it there (any unused event id).
 *
 * EVT_KEY payload is 2 bytes: [pressed(0/1), code]
 *   - for keys with a Shift/Fn alternate, code is the resolved ASCII char
 *   - for keys without one (F-keys, arrows, Ent/Sel/Mnu/Tab/Del/Ins/Esc,
 *     Help), code is one of the KEY_* constants below (0x80+)
 *   - Shift and Fn themselves never emit an event -- they're tracked
 *     locally and just change how the *next* key resolves.
 */

// ---------------------------------------------------------------------
// Full key matrix. row = D-index (0-15) = bit index in the 16-bit column
// read. col = 0-9 = A1-A10 (matches columns[]/columnMasks[] in
// KeyboardScan.h). Cross-checked against columnMasks[]: for every column,
// the bits left clear in its mask match exactly the rows listed here for
// that column.
// ---------------------------------------------------------------------
struct KeyCell
{
    uint8_t row;
    uint8_t col;
    const char *name;
};

static constexpr KeyCell keyMatrix[] =
    {
        // D0 (funky)
        {0, 0, "F7"},
        {0, 1, "F6"},
        {0, 2, "F5"},
        {0, 3, "F4"},
        {0, 4, "F3"},
        {0, 5, "F2"},
        {0, 6, "F1"},
        {0, 9, "Help"},
        // D1
        {1, 1, "G"},
        {1, 3, "Q"},
        {1, 9, "."},
        // D2 (funky)
        {2, 1, "B"},
        {2, 3, "A"},
        {2, 9, "!"},
        // D3 (funky)
        {3, 1, "N"},
        {3, 3, "Z"},
        {3, 4, "Enter"},
        {3, 9, ";"},
        // D4 (funky)
        {4, 1, "H"},
        {4, 3, "X"},
        {4, 4, "Select"},
        {4, 9, "P"},
        // D5
        {5, 1, "Y"},
        {5, 3, "S"},
        {5, 4, "Menu"},
        // D6
        {6, 1, "U"},
        {6, 3, "W"},
        {6, 4, "Space"},
        // D7 (funky)
        {7, 1, "J"},
        {7, 3, "E"},
        {7, 5, "Tab"},
        // D8
        {8, 0, "M"},
        {8, 2, "D"},
        {8, 5, "Delete"},
        // D9
        {9, 0, ")"},
        {9, 2, "C"},
        {9, 5, "Insert"},
        // D10
        {10, 0, ","},
        {10, 2, "("},
        {10, 5, "Escape"},
        // D11
        {11, 0, "K"},
        {11, 2, "V"},
        {11, 6, "Up"},
        // D12 (funky)
        {12, 0, "I"},
        {12, 2, "F"},
        {12, 6, "Right"},
        // D13
        {13, 0, "O"},
        {13, 2, "R"},
        {13, 6, "Down"},
        // D14
        {14, 0, "L"},
        {14, 2, "T"},
        {14, 6, "Left"},
        // D15
        {15, 7, "Fn"},
        {15, 8, "Shift"},
};
static constexpr int keyMatrixCount = sizeof(keyMatrix) / sizeof(keyMatrix[0]);

static constexpr bool funkyRow[16] =
    {
        /*D0 */ true,
        /*D1 */ false,
        /*D2 */ true,
        /*D3 */ true,
        /*D4 */ true,
        /*D5 */ false,
        /*D6 */ false,
        /*D7 */ true,
        /*D8 */ false,
        /*D9 */ false,
        /*D10*/ false,
        /*D11*/ false,
        /*D12*/ true,
        /*D13*/ false,
        /*D14*/ false,
        /*D15*/ false,
};

static uint16_t funkyGroupMask[16] = {0};
static uint16_t funkyPending[16] = {0};
static int8_t funkyHeld[16];

static const char *findKeyName(int row, int col)
{
    for (int i = 0; i < keyMatrixCount; ++i)
    {
        pollSerial();

        if (keyMatrix[i].row == row && keyMatrix[i].col == col)
            return keyMatrix[i].name;
    }
    return nullptr;
}

// ---------------------------------------------------------------------
// Shift/Fn alternates, transcribed from the comment block in
// KeyboardScan.h. Two things to flag from the source table itself:
//   - "I" row lists base=I, shift=I (identical) -- almost certainly meant
//     lowercase base 'i'; that's what's used below.
//   - "a" and "c" both list Fn='@' -- kept as given, may be a transcription
//     duplicate in the source, worth a physical spot-check.
//   - Z's Fn character was a curly quote (") in the source; substituted
//     with a plain ASCII double-quote here since curly quotes aren't ASCII.
// A 0 in shift or fn means "no alternate", falls back to base.
// ---------------------------------------------------------------------
struct KeyAlt
{
    const char *name;
    char base;
    char shift;
    char fn;
};

static constexpr KeyAlt keyAlternates[] =
    {
        {"Q", 'q', 'Q', '1'},
        {"W", 'w', 'W', '2'},
        {"E", 'e', 'E', '3'},
        {"R", 'r', 'R', '4'},
        {"T", 't', 'T', '5'},
        {"Y", 'y', 'Y', '6'},
        {"U", 'u', 'U', '7'},
        {"I", 'i', 'I', '8'},
        {"O", 'o', 'O', '9'},
        {"P", 'p', 'P', '0'},
        {"A", 'a', 'A', '@'},
        {"S", 's', 'S', '&'},
        {"D", 'd', 'D', '$'},
        {"F", 'f', 'F', '%'},
        {"G", 'g', 'G', '*'},
        {"H", 'h', 'H', '+'},
        {"J", 'j', 'J', '-'},
        {"K", 'k', 'K', '_'},
        {"L", 'l', 'L', '='},
        {";", ';', '|', ':'},
        {"Z", 'z', 'Z', '"'},
        {"X", 'x', 'X', '#'},
        {"C", 'c', 'C', '@'}, // Cybiko Logo on Fn
        {"V", 'v', 'V', '{'},
        {"B", 'b', 'B', '}'},
        {"N", 'n', 'N', '<'},
        {"M", 'm', 'M', '>'},
        {",", ',', '~', '\''},
        {".", '.', '\\', '/'},
        {"!", '!', '^', '?'},
        {"(", '(', '(', '['},
        {")", ')', ')', ']'},

        {"Enter", '\n', '\n', '\n'},
        {"Space", ' ', ' ', ' '},
        {"Tab", '\t', '\t', '\t'},
        {"Delete", '\b', '\b', '\b'},
};
static constexpr int keyAlternatesCount = sizeof(keyAlternates) / sizeof(keyAlternates[0]);

static char resolveChar(const char *name, bool shift, bool fn)
{
    for (int i = 0; i < keyAlternatesCount; ++i)
    {
        if (strcmp(keyAlternates[i].name, name) != 0)
            continue;

        pollSerial();

        if (fn && keyAlternates[i].fn)
            return keyAlternates[i].fn;
        if (shift && keyAlternates[i].shift)
            return keyAlternates[i].shift;
        return keyAlternates[i].base;
    }
    return 0; // not a typing key
}

// Non-typing keys get a code in the 0x80+ range so they never collide
// with a resolved ASCII char.
enum : uint8_t
{
    KEY_F1 = 0x80,
    KEY_F2,
    KEY_F3,
    KEY_F4,
    KEY_F5,
    KEY_F6,
    KEY_F7,
    KEY_HELP,
    KEY_SELECT,
    KEY_MENU,
    KEY_INSERT,
    KEY_UP,
    KEY_RIGHT,
    KEY_DOWN,
    KEY_LEFT,
};

static uint8_t resolveSpecialCode(const char *name)
{
    if (!strcmp(name, "F1"))
        return KEY_F1;
    if (!strcmp(name, "F2"))
        return KEY_F2;
    if (!strcmp(name, "F3"))
        return KEY_F3;
    if (!strcmp(name, "F4"))
        return KEY_F4;
    if (!strcmp(name, "F5"))
        return KEY_F5;
    if (!strcmp(name, "F6"))
        return KEY_F6;
    if (!strcmp(name, "F7"))
        return KEY_F7;
    if (!strcmp(name, "Help"))
        return KEY_HELP;
    if (!strcmp(name, "Select"))
        return KEY_SELECT;
    if (!strcmp(name, "Menu"))
        return KEY_MENU;
    if (!strcmp(name, "Insert"))
        return KEY_INSERT;
    if (!strcmp(name, "Up"))
        return KEY_UP;
    if (!strcmp(name, "Right"))
        return KEY_RIGHT;
    if (!strcmp(name, "Down"))
        return KEY_DOWN;
    if (!strcmp(name, "Left"))
        return KEY_LEFT;
    return 0;
}

// ---------------------------------------------------------------------
// State + hooks
// ---------------------------------------------------------------------
static bool shiftHeld = false;
static bool fnHeld = false;

static void initKeyboard()
{
    for (int i = 0; i < keyMatrixCount; ++i)
    {
        const KeyCell &k = keyMatrix[i];
        if (funkyRow[k.row])
            funkyGroupMask[k.row] |= static_cast<uint16_t>(1u << k.col);
    }

    for (int r = 0; r < 16; ++r)
    {
        funkyPending[r] = 0;
        funkyHeld[r] = -1;
    }

    shiftHeld = false;
    fnHeld = false;

    captureKeyboardBaseline();
}

static void emitKeyEvent(const char *name, bool pressed)
{
    // Shift/Fn are handled locally: track state, no EVT_KEY of their own.
    if (!strcmp(name, "Shift"))
    {
        shiftHeld = pressed;
        return;
    }
    if (!strcmp(name, "Fn"))
    {
        fnHeld = pressed;
        return;
    }

    uint8_t code;
    char c = resolveChar(name, shiftHeld, fnHeld);
    code = c ? static_cast<uint8_t>(c) : resolveSpecialCode(name);

    if (!code)
        return; // unrecognized name, nothing to emit

    uint8_t payload[2] = {code, static_cast<uint8_t>(pressed ? 1 : 0)};
    writeFrame(writeByte, EVT_KEY, payload, sizeof(payload));
}

// Call once per main loop iteration. Scans, resolves funky-row ghosting
// by exclusion, and emits EVT_KEY (or updates Shift/Fn state) for
// whatever changed.
static void pollKeyboard()
{
    for (int col = 0; col < 10; ++col)
    {
        pollSerial();

        if (!anyKeyPressed())
        {
            baseline[col] = 0xFFFF;
            continue;
        }

        uint16_t now = scanColumn(col);
        uint16_t diff = now ^ baseline[col];

        if (!diff)
            continue;

        for (int row = 0; row < 16; ++row)
        {
            if (!((diff >> row) & 1))
                continue;

            if (!funkyRow[row])
            {
                bool pressed = !((now >> row) & 1);
                const char *name = findKeyName(row, col);
                if (name)
                    emitKeyEvent(name, pressed);

                baseline[col] = static_cast<uint16_t>(
                    (baseline[col] & ~(1u << row)) | (now & (1u << row)));
                continue;
            }

            // Funky row: record that this column just moved off baseline.
            funkyPending[row] |= static_cast<uint16_t>(1u << col);

            uint16_t group = funkyGroupMask[row];
            uint16_t missing = group & ~funkyPending[row];

            if (missing == 0)
            {
                // Every column in the group moved -- can't identify a key
                // this cycle, settle quietly and wait for the next.
                for (int i = 0; i < 10; ++i)
                {
                    if (!(group & (1u << i)))
                        continue;
                    uint16_t v = (i == col) ? now : scanColumn(i);
                    baseline[i] = static_cast<uint16_t>(
                        (baseline[i] & ~(1u << row)) | (v & (1u << row)));
                }
                funkyPending[row] = 0;
                continue;
            }

            bool isSingleBit = (missing & (missing - 1)) == 0;
            if (isSingleBit)
            {
                int missingCol = 0;
                while (!((missing >> missingCol) & 1))
                    ++missingCol;

                const char *name = findKeyName(row, missingCol);
                bool pressed = (funkyHeld[row] == -1);
                funkyHeld[row] = pressed ? static_cast<int8_t>(missingCol) : -1;

                if (name)
                    emitKeyEvent(name, pressed);

                for (int i = 0; i < 10; ++i)
                {
                    if (!(group & (1u << i)))
                        continue;
                    uint16_t v = (i == col) ? now : scanColumn(i);
                    baseline[i] = static_cast<uint16_t>(
                        (baseline[i] & ~(1u << row)) | (v & (1u << row)));
                }
                funkyPending[row] = 0;
            }
            // else: still more than one column missing, keep accumulating --
            // resolves itself on a later column/sweep, no fixed timeout.
        }
    }
}