#include "font.h"

void font_block(Canvas *c, int x, int y, int height_eighths, uint32_t fg,
                uint32_t bg)
{
    int height = height_eighths < 0 ? 0 : height_eighths;
    height = height > 8 ? 8 : height;
    canvas_rect(c, x, y, FONT_W, FONT_H, bg);
    /* An eighth of 16 rows is 2 rows. */
    canvas_rect(c, x, y + FONT_H - 2 * height, FONT_W, 2 * height, fg);
}
