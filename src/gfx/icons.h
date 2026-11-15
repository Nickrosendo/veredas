/*
 * icons: the 16 by 16 pictures on the buttons, the top line and the menus.
 */
#ifndef VEREDAS_ICONS_H
#define VEREDAS_ICONS_H

#include "canvas.h"

#define ICON_SIZE 16

/* An enum is a list of named int constants, numbered from 0 in order; the
 * last, ICON_COUNT, is therefore how many icons there are. */
enum {
    ICON_PAUSE,
    ICON_PLAY,
    ICON_SLOWER,
    ICON_FASTER,
    ICON_ZOOM_OUT,
    ICON_ZOOM_IN,
    ICON_STORY,
    ICON_NOTICES,
    ICON_CAMERA,
    ICON_JUMP,
    ICON_TRAIT,
    ICON_WEATHER,
    ICON_SKIP,
    ICON_BRANCH,
    ICON_OTHER_BRANCH,
    ICON_COMPARE,
    ICON_NEW_START,
    ICON_MODE_WATCH,
    ICON_MODE_SANDBOX,
    ICON_WORLD,
    ICON_HELP,
    ICON_CLOSE,
    ICON_LET_GO,
    ICON_SPRING,
    ICON_SUMMER,
    ICON_AUTUMN,
    ICON_WINTER,
    ICON_ANIMALS,
    ICON_DROUGHT,
    ICON_HARD_WINTER,
    ICON_WARM_YEAR,
    ICON_COUNT
};

/*
 * The drawn icons, generated into icons_data.c by tools/art/icons_to_c.py from
 * the pictures in tools/art/icons/. ICON_INK[icon] is 16 rows of 16 numbers,
 * each an index into ICON_COLOURS; 0 means nothing is drawn there.
 *
 * "extern" says these arrays exist and are defined in another .c file: this
 * header only promises them to whoever includes it, and the linker joins the
 * promise to icons_data.c's definition. ICON_COLOUR_COUNT must match the
 * converter's table (it prints the number): too few and the compiler stops
 * at the extra entries; too many and C fills the rest with 0, which the
 * tests catch.
 */
#define ICON_COLOUR_COUNT 15
extern const uint32_t ICON_COLOURS[ICON_COLOUR_COUNT];
extern const uint8_t ICON_INK[ICON_COUNT][ICON_SIZE * ICON_SIZE];

/* Draws icon with its top left corner at (x, y), clipped to the canvas.
 * An icon outside 0 .. ICON_COUNT - 1 draws nothing. */
void icon_draw(Canvas *c, int x, int y, int icon);
/* The same, every colour at half brightness: a button that can't be used. */
void icon_draw_dim(Canvas *c, int x, int y, int icon);

#endif
