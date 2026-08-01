// TextConsole.h — a small character-grid "TUI" renderer on top of the raw
// LCDDriver framebuffer. Grid dims and pixel-set logic are placeholders
// until LCDDriver.h's real width/height/pixel-packing are confirmed —
// search for "TODO(confirm)" below.
#pragma once
#include <string.h>
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

        // Direct byte-fill instead of walking every cell through setPixel()
        // — hand-rolled rather than memset(), since this newlib build may
        // not have it linkable. 0x00 = all 4 pixels/byte at level 0
        // (assumed lightest, matching setPixel's "off" mapping). If
        // setPixel's polarity turns out inverted for this panel, use 0xFF
        // here too for consistency.
        auto fb = lcd->getFramebuffer();
        int totalBytes = lcd->getStride() * lcd->getHeight();
        for (int i = 0; i < totalBytes; i++)
            fb[i] = 0x00;

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
    // Handles '\n' newline, '\b' backspace (erase-and-move-back), '\t' tab
    // (advances to next 4-column stop, filling with spaces). Redraws only
    // the cells touched.
    void writeText(const char* text, int len) {
        for (int i = 0; i < len; i++) {
            char c = text[i];
            if (c == '\n') {
                cursorCol = 0;
                cursorRow++;
            } else if (c == '\b') {
                if (cursorCol > 0) {
                    cursorCol--;
                } else if (cursorRow > 0) {
                    cursorRow--;
                    cursorCol = GRID_COLS - 1;
                }
                putCharAt(cursorRow, cursorCol, ' ');
            } else if (c == '\t') {
                int next = (cursorCol / 4 + 1) * 4;
                while (cursorCol < next && cursorCol < GRID_COLS) {
                    putCharAt(cursorRow, cursorCol, ' ');
                    cursorCol++;
                }
                if (cursorCol >= GRID_COLS) {
                    cursorCol = 0;
                    cursorRow++;
                }
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

    // Every row except the new blank one already holds correct pixel
    // data one cell-row too high, so shift the framebuffer bytes directly
    // instead of re-rasterizing them, and only rasterize the row that's
    // actually new (26 cells instead of 312 -- about a 12x cut).
    //
    // Manual byte loops instead of memmove()/memset() to stay consistent
    // with clear()'s note above about this newlib build's libc linking.
    void scroll() {
        for (int r = 1; r < GRID_ROWS; r++)
            for (int c = 0; c < GRID_COLS; c++)
                grid[r - 1][c] = grid[r][c];
        for (int c = 0; c < GRID_COLS; c++)
            grid[GRID_ROWS - 1][c] = ' ';
        cursorRow = GRID_ROWS - 1;

        auto fb = lcd->getFramebuffer();
        int stride = lcd->getStride();
        int rowBytes = stride * CELL_H;
        int totalBytes = stride * lcd->getHeight();
        int keepBytes = rowBytes * (GRID_ROWS - 1);

        // Shift everything up by one cell-row. Safe to do forward (low to
        // high address) since dst index is always behind src index by
        // rowBytes, so no byte is overwritten before it's read.
        for (int i = 0; i < keepBytes; i++)
            fb[i] = fb[i + rowBytes];

        // Blank whatever's left (the last cell-row, plus any stride
        // padding beyond GRID_ROWS*CELL_H if the panel height doesn't
        // divide evenly).
        for (int i = keepBytes; i < totalBytes; i++)
            fb[i] = 0x00;

        // Only the bottom row needs re-rasterizing -- everything above it
        // is already correct, just relocated.
        for (int c = 0; c < GRID_COLS; c++)
            drawCell(GRID_ROWS - 1, c);

        lcd->setDirty();
        lcd->updateDisplay();
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
    void setPixel(unsigned char* fb, int stride, int x, int y, bool on) {
        if (x < 0 || y < 0 || y >= SCREEN_H) return;
        int byteIndex = (x / 4) + y * stride;
        int shift = 6 - 2 * (x % 4); // pixel 0 -> bits 7:6, pixel 1 -> bits 5:4, etc.
        unsigned char mask = 0b11 << shift;
        unsigned char value = (on ? 0b11 : 0b00) << shift;
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
                setPixel(fb, stride, ox + x, oy + y, false);

        const unsigned char* rows = findGlyph(grid[row][col]);
        for (int gy = 0; gy < 7; gy++) {
            unsigned char bits = rows[gy];
            for (int gx = 0; gx < 5; gx++) {
                bool on = (bits >> (4 - gx)) & 1;
                if (on) setPixel(fb, stride, ox + gx, oy + gy, true);
            }
        }
    }
};