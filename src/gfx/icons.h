/*
 * icons: the 16 by 16 pictures on the buttons, the top line and the menus.
 */
#ifndef AGON_ICONS_H
#define AGON_ICONS_H

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

/* Draws icon with its top left corner at (x, y), clipped to the canvas.
 * An icon outside 0 .. ICON_COUNT - 1 draws nothing. */
void icon_draw(Canvas *c, int x, int y, int icon);

#endif
