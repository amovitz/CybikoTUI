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
        redrawAll();
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

    // Row-major, 8 horizontal pixels packed per byte, MSB = leftmost pixel.
    // Inferred from the original demo's loop (`for x < getStride()`, not
    // `< getWidth()`, with y advancing one full row per iteration) — that
    // only makes sense if stride is bytes-per-row and pixels within a byte
    // run left-to-right, not stacked vertically like the original guess.
    void setPixel(unsigned char* fb, int stride, int x, int y, bool on) {
        if (x < 0 || y < 0 || y >= SCREEN_H) return;
        int byteIndex = (x / 8) + y * stride;
        unsigned char bit = 1 << (7 - (x % 8));
        if (on) fb[byteIndex] |= bit;
        else    fb[byteIndex] &= ~bit;
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