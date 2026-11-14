/*
 * font: the size of a character cell in the window, 8 by 16 (the shape of a
 * terminal's), and the block characters of the panel's graphs. The letters
 * themselves are drawn by text.h.
 */
#ifndef AGON_FONT_H
#define AGON_FONT_H

#include <stdint.h>

#include "canvas.h"

#define FONT_W 8
#define FONT_H 16

/* One character cell at (x, y) filled with bg, then with its bottom
 * height_eighths / 8 in fg: the eight block heights of the panel's graphs. */
void font_block(Canvas *c, int x, int y, int height_eighths, uint32_t fg,
                uint32_t bg);

#endif
