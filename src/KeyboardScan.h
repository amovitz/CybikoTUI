#pragma once

#include <stdint.h>
#include "SerialBus.h"

/*
// Key Matrix
	A9	A8	A7	A6	A5	A4	A3	A2	A1	A10
D0			F1	F2	F3	F4	F5	F6	F7	Help
D1						Q		G		.
D2						A		B		!
D3					Ent	Z		N		;
D4					Sel	X		H		P
D5					Mnu	S		Y
D6					Spc	W		U
D7				Tab		E		J
D8				Del			D		M
D9				Ins			C		)
D10				Esc			(		,
D11			Up				V		K
D12			Rit				F		I
D13			Dwn				R		O
D14			Lft				T		L
D15	Shift	Fn

// Key Shift/Fn alternates
Base Shift  Fn
q	 Q	    1
w	 W	    2
e	 E	    3
r	 R	    4
t	 T	    5
y	 Y	    6
u	 U	    7
I	 I	    8
o	 O	    9
p	 P	    0
a	 A	    @
s	 S	    &
d	 D	    $
f	 F  	%
g	 G  	*
h	 H  	+
j	 J  	-
k	 K  	_
l	 L  	=
;	 |	    :
z	 Z	    “
x	 X  	#
c	 C	    @
v	 V	    {
b	 B	    }
n	 N	    <
m	 M	    >
,	 ~	    '
.	 \	    /
!	 ^	    ?
(		    [
)		    ]
*/

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

    volatile uint16_t* ptr =
        reinterpret_cast<volatile uint16_t*>(0x00EFF801);

    return *ptr != 0xFFFF;
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

// ---------------------------------------------------------------------
// Resolved key decoding
//
// Rows D0, D2, D3, D4, D7, D12 are "funky": pressing a key on one of
// these rows flips that row's bit on every OTHER column in the row, but
// never on the pressed key's own column.
//
// row = D-index (0-15) = bit index in the 16-bit column read.
// col = 0-9 = A1-A10 (matches columns[]/columnMasks[] above).
// ---------------------------------------------------------------------

struct KeyCell { uint8_t row; uint8_t col; const char* name; };

static constexpr KeyCell keyMatrix[] =
{
    // D0 (funky)
    {0,0,"F7"}, {0,1,"F6"}, {0,2,"F5"}, {0,3,"F4"},
    {0,4,"F3"}, {0,5,"F2"}, {0,6,"F1"}, {0,9,"Help"},
    // D1
    {1,1,"G"}, {1,3,"Q"}, {1,9,"."},
    // D2 (funky)
    {2,1,"B"}, {2,3,"A"}, {2,9,"!"},
    // D3 (funky)
    {3,1,"N"}, {3,3,"Z"}, {3,4,"Enter"}, {3,9,";"},
    // D4 (funky)
    {4,1,"H"}, {4,3,"X"}, {4,4,"Select"}, {4,9,"P"},
    // D5
    {5,1,"Y"}, {5,3,"S"}, {5,4,"Menu"},
    // D6
    {6,1,"U"}, {6,3,"W"}, {6,4,"Space"},
    // D7 (funky)
    {7,1,"J"}, {7,3,"E"}, {7,5,"Tab"},
    // D8
    {8,0,"M"}, {8,2,"D"}, {8,5,"Delete"},
    // D9
    {9,0,")"}, {9,2,"C"}, {9,5,"Insert"},
    // D10
    {10,0,","}, {10,2,"("}, {10,5,"Escape"},
    // D11
    {11,0,"K"}, {11,2,"V"}, {11,6,"Up"},
    // D12 (funky)
    {12,0,"I"}, {12,2,"F"}, {12,6,"Right"},
    // D13
    {13,0,"O"}, {13,2,"R"}, {13,6,"Down"},
    // D14
    {14,0,"L"}, {14,2,"T"}, {14,6,"Left"},
    // D15
    {15,7,"Fn"}, {15,8,"Shift"},
};
static constexpr int keyMatrixCount = sizeof(keyMatrix) / sizeof(keyMatrix[0]);

static constexpr bool funkyRow[16] =
{
    /*D0 */ true,  /*D1 */ false, /*D2 */ true,  /*D3 */ true,
    /*D4 */ true,  /*D5 */ false, /*D6 */ false, /*D7 */ true,
    /*D8 */ false, /*D9 */ false, /*D10*/ false, /*D11*/ false,
    /*D12*/ true,  /*D13*/ false, /*D14*/ false, /*D15*/ false,
};

// Bitmask (bit i = column i) of which columns belong to each funky row's
// group. Built once from keyMatrix so it can't drift out of sync.
static uint16_t funkyGroupMask[16] = {0};

// Per-funky-row scratch state, sized/positioned right next to baseline[]
// since that's the array this decoder shares with scanKeyboardDiffs().
static uint16_t funkyPending[16] = {0};
static int8_t   funkyHeld[16];

static const char* findKeyName(int row, int col)
{
    for (int i = 0; i < keyMatrixCount; ++i)
        if (keyMatrix[i].row == row && keyMatrix[i].col == col)
            return keyMatrix[i].name;
    return nullptr;
}

// Call once, alongside captureKeyboardBaseline(), before using
// scanKeyboardKeysDebug().
static void initKeyMatrix()
{
    for (int i = 0; i < keyMatrixCount; ++i)
    {
        const KeyCell& k = keyMatrix[i];
        if (funkyRow[k.row])
            funkyGroupMask[k.row] |= static_cast<uint16_t>(1u << k.col);
    }

    for (int r = 0; r < 16; ++r)
    {
        funkyPending[r] = 0;
        funkyHeld[r] = -1;
    }
}

static void sendKeyDebug(uint8_t row, uint8_t col, bool pressed, const char* name)
{
    uint8_t payload[16];

    payload[0] = row;
    payload[1] = col;
    payload[2] = pressed ? 1 : 0;

    int len = 3;
    if (name)
    {
        while (name[len - 3] && len < static_cast<int>(sizeof(payload)))
        {
            payload[len] = static_cast<uint8_t>(name[len - 3]);
            ++len;
        }
    }

    writeFrame(writeByte, EVT_DEBUG, payload, len);
}


static uint16_t baseline[10];

static void captureKeyboardBaseline()
{
    initKeyMatrix();

    for (int i = 0; i < 10; ++i)
    {
        pollSerial();

        volatile uint16_t* p = reinterpret_cast<volatile uint16_t*>(columns[i]);

        baseline[i] = *p | columnMasks[i];
    }
}

// state, if non-null, is filled with the raw scanned value for every
// column (same thing scanKeyboard() would give you) so you don't lose
// access to the underlying uint16_t list while testing.
static void scanKeyboardDiffs(uint16_t state[10] = nullptr)
{
    uint8_t payload[7];

    for (int i = 0; i < 10; ++i)
    {
        pollSerial();

        if (!anyKeyPressed()) {
            baseline[i] = 0xFFFF;
            if (state) state[i] = 0xFFFF;
            continue;
        }
        
        volatile uint16_t* p = reinterpret_cast<volatile uint16_t*>(columns[i]);

        uint16_t now = *p | columnMasks[i];
        uint16_t diff = now ^ baseline[i];

        if (state) state[i] = now;

        if (diff)
        {
            // Unchanged: same raw hex dump as before.
            payload[0] = i+1;

            payload[1] = (columns[i] >> 8) & 0xFF;
            payload[2] = columns[i] & 0xFF;

            payload[3] = (baseline[i] >> 8) & 0xFF;
            payload[4] = baseline[i] & 0xFF;

            payload[5] = now >> 8;
            payload[6] = now & 0xFF;

            // writeFrame(writeByte, EVT_DEBUG, payload, sizeof(payload));

            // New: additionally resolve which key this diff belongs to and
            // send a readable frame
            for (int row = 0; row < 16; ++row)
            {
                if (!((diff >> row) & 1))
                    continue;

                if (!funkyRow[row])
                {
                    bool pressed = !((now >> row) & 1);
                    const char* name = findKeyName(row, i);
                    sendKeyDebug(static_cast<uint8_t>(row), static_cast<uint8_t>(i),
                                 pressed, name ? name : "?");
                    continue;
                }

                // Funky row: accumulate against the shared baseline[]
                // this uses the exact same baseline this function just
                // rebased below, so it stays in lockstep with the raw dump.
                funkyPending[row] |= static_cast<uint16_t>(1u << i);

                uint16_t group   = funkyGroupMask[row];
                uint16_t missing = group & ~funkyPending[row];

                if (missing == 0)
                {
                    funkyPending[row] = 0;
                    continue;
                }

                bool isSingleBit = (missing & (missing - 1)) == 0;
                if (isSingleBit)
                {
                    int missingCol = 0;
                    while (!((missing >> missingCol) & 1))
                        ++missingCol;

                    const char* name = findKeyName(row, missingCol);
                    bool pressed = (funkyHeld[row] == -1);
                    funkyHeld[row] = pressed ? static_cast<int8_t>(missingCol) : -1;

                    sendKeyDebug(static_cast<uint8_t>(row), static_cast<uint8_t>(missingCol),
                                 pressed, name ? name : "?");

                    funkyPending[row] = 0;
                }
            }

            baseline[i] = now;
        }
    }
}