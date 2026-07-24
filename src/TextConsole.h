// TextConsole.h — a small character-grid "TUI" renderer on top of the raw
// LCDDriver framebuffer. Grid dims and pixel-set logic are placeholders
// until LCDDriver.h's real width/height/pixel-packing are confirmed —
// search for "TODO(confirm)" below.
#pragma once
#include "LCDDriver.h"
#include "font5x7.h"

// TODO(confirm): real panel size from LCDDriver.h. 160x100 assumed from
// general Cybiko specs, not yet confirmed for your specific unit/build.
static const int SCREEN_W = 160;
static const int SCREEN_H = 100;

static const int CELL_W = 6; // 5px glyph + 1px gap
static const int CELL_H = 8; // 7px glyph + 1px gap
static const int GRID_COLS = SCREEN_W / CELL_W; // 26
static const int GRID_ROWS = SCREEN_H / CELL_H; // 12

class TextConsole {
public:
    void begin(LCDDriver* driver) {
        lcd = driver;
        clear();
    }

    void clear() {
        for (int r = 0; r < GRID_ROWS; r++)
            for (int c = 0; c < GRID_COLS; c++)
                grid[r][c] = ' ';
        cursorRow = cursorCol = 0;

        fill(0b00);
    }

    void fill(int color = 0b00) {
        // Direct memset instead of walking every cell through setPixel() —
        // 0x00 = all 4 pixels/byte at level 0 (assumed lightest, matching
        // setPixel's "off" mapping). If setPixel's polarity turns out to
        // be inverted for this panel, use 0xFF here too for consistency.
        auto fb = lcd->getFramebuffer();
        int totalBytes = lcd->getStride() * lcd->getHeight();
        for(int i = 0; i < totalBytes; i++) {
            fb[i] = color << 6 | color << 4 | color << 2 | color;
        }
 
        lcd->setDirty();
        lcd->updateDisplay();
    }

    void setCursor(int row, int col) {
        if (row < 0 || row >= GRID_ROWS || col < 0 || col >= GRID_COLS)
            return;
        cursorRow = row;
        cursorCol = col;
    }

    // Writes text at the cursor, wraps at end of row, scrolls at bottom.
    // Handles '\n' as a newline. Redraws only the cells touched.
    void writeText(const char* text, int len) {
        for (int i = 0; i < len; i++) {
            char c = text[i];
            if (c == '\n') {
                cursorCol = 0;
                cursorRow++;
            } else {
                putCharAt(cursorRow, cursorCol, c);
                cursorCol++;
                if (cursorCol >= GRID_COLS) {
                    cursorCol = 0;
                    cursorRow++;
                }
            }
            if (cursorRow >= GRID_ROWS)
                scroll();
        }
        lcd->setDirty();
        lcd->updateDisplay();
    }

    // Direct single-cell write, does not move the cursor.
    void putChar(int row, int col, char c) {
        putCharAt(row, col, c);
        lcd->setDirty();
        lcd->updateDisplay();
    }

private:
    LCDDriver* lcd = nullptr;
    char grid[64][64]; // sized generously; only [GRID_ROWS][GRID_COLS] used
    int cursorRow = 0, cursorCol = 0;

    void putCharAt(int row, int col, char c) {
        if (row < 0 || row >= GRID_ROWS || col < 0 || col >= GRID_COLS)
            return;
        grid[row][col] = c;
        drawCell(row, col);
    }

    void scroll() {
        for (int r = 1; r < GRID_ROWS; r++)
            for (int c = 0; c < GRID_COLS; c++)
                grid[r - 1][c] = grid[r][c];
        for (int c = 0; c < GRID_COLS; c++)
            grid[GRID_ROWS - 1][c] = ' ';
        cursorRow = GRID_ROWS - 1;
        redrawAll();
    }

    void redrawAll() {
        for (int r = 0; r < GRID_ROWS; r++)
            for (int c = 0; c < GRID_COLS; c++)
                drawCell(r, c);
        lcd->setDirty();
        lcd->updateDisplay();
    }

    // Row-major, 2 bits per pixel (4-level greyscale), 4 horizontal pixels
    // packed per byte, MSB-first pixel pairs. "on" is mapped to 0b11
    // (assumed darkest level) and "off" to 0b00 (assumed lightest) — if
    // this renders as white-on-black instead of black-on-white, the
    // polarity is inverted for this panel; swap 0b11/0b00 below if so.
    void setPixel(unsigned char* fb, int stride, int x, int y, int grey) {
        if (x < 0 || y < 0 || y >= SCREEN_H) return;
        int byteIndex = (x / 4) + y * stride;
        int shift = 6 - 2 * (x % 4); // pixel 0 -> bits 7:6, pixel 1 -> bits 5:4, etc.
        unsigned char mask = 0b11 << shift;
        unsigned char value = grey << shift;
        fb[byteIndex] = (fb[byteIndex] & ~mask) | value;
    }

    void drawCell(int row, int col) {
        auto fb = lcd->getFramebuffer();
        int stride = lcd->getStride();
        int ox = col * CELL_W;
        int oy = row * CELL_H;

        // clear the cell first (including the 1px gap column/row)
        for (int y = 0; y < CELL_H; y++)
            for (int x = 0; x < CELL_W; x++)
                setPixel(fb, stride, ox + x, oy + y, 0b00);

        const unsigned char* rows = findGlyph(grid[row][col]);
        for (int gy = 0; gy < 7; gy++) {
            unsigned char bits = rows[gy];
            for (int gx = 0; gx < 5; gx++) {
                bool on = (bits >> (4 - gx)) & 1;
                if (on) setPixel(fb, stride, ox + gx, oy + gy, 0b11);
            }
        }
    }
};