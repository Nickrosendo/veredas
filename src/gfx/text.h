/*
 * text: the window's two fonts, both 16 pixels tall and both made from the
 * glyphs of Spleen 8x16 (tools/font/bdf_to_c.py).
 *
 * - The proportional font, for words: each letter only as wide as its ink,
 *   with one empty pixel column between letters. Drawn see-through.
 * - The fixed font, for columns lined up by spaces (the panel's graphs and
 *   tables): every letter 8 wide, drawn on a background, the same contract
 *   as a terminal's character cell.
 *
 * ASCII only: a byte outside printable ASCII draws as '?'.
 */
#ifndef AGON_TEXT_H
#define AGON_TEXT_H

#include <stddef.h>
#include <stdint.h>

#include "canvas.h"

#define TEXT_H 16
#define TEXT_FIXED_W 8
#define TEXT_GLYPHS 95 /* ASCII 32 (space) to 126 (~) */

/* The generated tables (text_data.c). */
extern const uint8_t TEXT_ROWS[TEXT_GLYPHS][TEXT_H];
extern const uint8_t TEXT_LEFT[TEXT_GLYPHS];
extern const uint8_t TEXT_WIDTH[TEXT_GLYPHS];

/* Width in pixels in the proportional font: every letter's width plus one
 * pixel between letters, none after the last. 0 for "". */
int text_width(const char *ascii);
/* Draws in the proportional font with its top left corner at (x, y), only
 * the inked pixels. Returns the x just after the last letter, which is
 * x + text_width(ascii). */
int text_draw(Canvas *c, int x, int y, const char *ascii, uint32_t fg);
/* Writes into out (size bytes, always terminated) ascii itself when it fits
 * in max_w pixels, else as many of its leading letters as fit followed by
 * "...", else "" when not even "..." fits. */
void text_cut(const char *ascii, int max_w, char *out, size_t size);
/* Draws in the fixed font: each letter an 8 by 16 cell filled with bg, then
 * its ink in fg. */
void text_draw_fixed(Canvas *c, int x, int y, const char *ascii, uint32_t fg,
                     uint32_t bg);

#endif
