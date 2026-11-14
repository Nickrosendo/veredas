#include "icons.h"

/*
 * The icons are pictures in a table (icons_data.c, generated from the PNGs in
 * tools/art/icons/): drawing one is drawing its block of colour indexes.
 */
void icon_draw(Canvas *c, int x, int y, int icon)
{
    if (icon < 0 || icon >= ICON_COUNT) {
        return;
    }
    /* The whole canvas as the clip: canvas_picture also clips to it. */
    CanvasClip all = {0, 0, c->w, c->h};
    /* ICON_INK[icon] is an array of 256 bytes; passed to a function it
     * becomes a pointer to its first byte, the way every C array does. */
    canvas_picture(c, ICON_INK[icon], ICON_SIZE, ICON_SIZE, ICON_SIZE, x, y, 1,
                   false, ICON_COLOURS, all);
}
