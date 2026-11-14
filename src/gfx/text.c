#include "text.h"

#include <string.h>

#define FIRST_GLYPH 32
#define GAP 1 /* empty columns between two letters of the proportional font */

/* The glyph number of one byte of text: 0 for ' ', 94 for '~', and '?' for
 * anything else. */
static int glyph_of(char ch)
{
    /* char may be signed, so a byte above 127 can be negative here: it is
     * turned into unsigned char before it becomes an index. */
    int glyph = (int)(unsigned char)ch - FIRST_GLYPH;
    if (glyph < 0 || glyph >= TEXT_GLYPHS) {
        glyph = '?' - FIRST_GLYPH;
    }
    return glyph;
}

int text_width(const char *ascii)
{
    int w = 0;
    for (int i = 0; ascii[i] != '\0'; i++) {
        if (i > 0) {
            w += GAP;
        }
        w += TEXT_WIDTH[glyph_of(ascii[i])];
    }
    return w;
}

/*
 * Draws columns first .. first + width - 1 of a glyph with that first column
 * at x: only the ink, in fg. Writes the pixels directly when the whole glyph
 * is on the canvas (text is redrawn every frame, so this is the common case),
 * and through the clipping canvas_rect otherwise.
 */
static void draw_glyph_ink(Canvas *c, int x, int y, int glyph, int first,
                           int width, uint32_t fg)
{
    const uint8_t *rows = TEXT_ROWS[glyph];
    bool inside = x >= 0 && y >= 0 && x + width <= c->w && y + TEXT_H <= c->h;
    for (int row = 0; row < TEXT_H; row++) {
        /* The leftmost pixel is the top bit (0x80) of the byte, so column i
         * is bit 7 - i. uint8_t becomes int before the shift ("integer
         * promotion"), which is why shifting it is safe. */
        unsigned bits = rows[row];
        for (int i = 0; i < width; i++) {
            if (((bits >> (7 - (first + i))) & 1u) == 0) {
                continue;
            }
            if (inside) {
                c->px[(y + row) * c->w + x + i] = fg;
            } else {
                canvas_rect(c, x + i, y + row, 1, 1, fg);
            }
        }
    }
}

int text_draw(Canvas *c, int x, int y, const char *ascii, uint32_t fg)
{
    for (int i = 0; ascii[i] != '\0'; i++) {
        if (i > 0) {
            x += GAP;
        }
        int glyph = glyph_of(ascii[i]);
        draw_glyph_ink(c, x, y, glyph, TEXT_LEFT[glyph], TEXT_WIDTH[glyph], fg);
        x += TEXT_WIDTH[glyph];
    }
    return x;
}

/* Copies at most n bytes of from into out, never more than size - 1, and
 * terminates. Returns how many were copied. */
static size_t copy_some(char *out, size_t size, const char *from, size_t n)
{
    if (n > size - 1) {
        n = size - 1;
    }
    memcpy(out, from, n);
    out[n] = '\0';
    return n;
}

void text_cut(const char *ascii, int max_w, char *out, size_t size)
{
    if (size == 0) {
        return;
    }
    size_t len = strlen(ascii);
    if (text_width(ascii) <= max_w) {
        copy_some(out, size, ascii, len);
        return;
    }
    int dots = text_width("...");
    if (dots > max_w || size < 4) {
        out[0] = '\0';
        return;
    }
    /* Adds letters while the letters, the gap after them and "..." still fit
     * in max_w, and while out has room for them, "..." and the terminator.
     * keep counts the letters taken; w is their width. */
    size_t keep = 0;
    int w = 0;
    while (keep < len) {
        int next = w + (keep > 0 ? GAP : 0) + TEXT_WIDTH[glyph_of(ascii[keep])];
        if (next + GAP + dots > max_w || keep + 1 + 4 > size) {
            break;
        }
        w = next;
        keep++;
    }
    size_t n = copy_some(out, size, ascii, keep);
    memcpy(out + n, "...", 4); /* 4: the three dots and the terminator */
}

void text_draw_fixed(Canvas *c, int x, int y, const char *ascii, uint32_t fg,
                     uint32_t bg)
{
    for (int i = 0; ascii[i] != '\0'; i++) {
        int cell_x = x + i * TEXT_FIXED_W;
        canvas_rect(c, cell_x, y, TEXT_FIXED_W, TEXT_H, bg);
        draw_glyph_ink(c, cell_x, y, glyph_of(ascii[i]), 0, TEXT_FIXED_W, fg);
    }
}
