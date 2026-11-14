/*
 * bars: the bar of buttons along the bottom of the window, and the small
 * world menu above its right end. Each button presses a key the window already
 * answers to; the state structs say what to draw, so the bars are tested
 * without a window.
 */
#ifndef AGON_BARS_H
#define AGON_BARS_H

#include <stdbool.h>

#include "canvas.h"
#include "ui.h"

typedef struct {
    bool paused, sandbox, split, demo, following, look_on;
    int speed;            /* ticks a second, for the "2/s" label */
    bool world_menu_open; /* the world button drawn pressed */
} BottomState;

/* The bar, UI_BAR_H tall, from (0, y) across w pixels. Too narrow for every
 * word, the words go (the tips stay); still too narrow, the least needed
 * buttons go, their keys still working. */
void bar_bottom_draw(Ui *u, Canvas *c, int y, int w, const BottomState *st);

/* The world menu, opened by UI_KEY_WORLD_MENU: its right edge at right_x and
 * its bottom at bottom_y. Rows: new world 'r', copy code 'c', open code 'o',
 * save 's', quit 'q'; a browser page (page) has no save or quit. */
void bar_world_menu_draw(Ui *u, Canvas *c, int right_x, int bottom_y, bool page);

#endif
