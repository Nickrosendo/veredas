#include "icons.h"

#include <stdbool.h>

/*
 * The icons are pictures in a table (icons_data.c, generated from the PNGs in
 * tools/art/icons/): drawing one is drawing its block of colour indexes with
 * a table of colours, the icons' own or a dimmed copy.
 */
static void draw_with(Canvas *c, int x, int y, int icon, const uint32_t *colours)
{
    if (icon < 0 || icon >= ICON_COUNT) {
        return;
    }
    /* The whole canvas as the clip: canvas_picture also clips to it. */
    CanvasClip all = {0, 0, c->w, c->h};
    /* ICON_INK[icon] is an array of 256 bytes; passed to a function it
     * becomes a pointer to its first byte, the way every C array does. */
    canvas_picture(c, ICON_INK[icon], ICON_SIZE, ICON_SIZE, ICON_SIZE, x, y, 1,
                   false, colours, all);
}

void icon_draw(Canvas *c, int x, int y, int icon)
{
    draw_with(c, x, y, icon, ICON_COLOURS);
}

void icon_draw_dim(Canvas *c, int x, int y, int icon)
{
    /* A local copy of the 15 colours, each at half. Halving all three
     * channels at once: shifting the whole 0xFFRRGGBB right by one moves
     * each channel's lowest bit into the top of the channel below it (red's
     * into green's), so the mask keeps 7 bits of each channel and clears
     * those strays; the opacity byte is then put back whole. */
    uint32_t half[ICON_COLOUR_COUNT];
    for (int i = 0; i < ICON_COLOUR_COUNT; i++) {
        half[i] = 0xFF000000u | ((ICON_COLOURS[i] >> 1) & 0x007F7F7Fu);
    }
    draw_with(c, x, y, icon, half);
}
