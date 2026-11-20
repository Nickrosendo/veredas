/*
 * bars: the bar of buttons along the bottom of the window, and the small
 * world menu above its right end. Each button presses a key the window already
 * answers to; the state structs say what to draw, so the bars are tested
 * without a window.
 */
#ifndef VEREDAS_BARS_H
#define VEREDAS_BARS_H

#include <stdbool.h>
#include <stddef.h> /* size_t */
#include <stdint.h>

#include "canvas.h"
#include "ui.h"

typedef struct {
    bool paused, sandbox, split, demo, following, look_on;
    /* Whether a level key would go anywhere: a level above, a level below. */
    bool level_up, level_down;
    int speed;            /* ticks a second, for the "2/s" label */
    bool world_menu_open; /* the world button drawn pressed */
    bool menu_open;       /* a Sandbox menu or skip open: the bar dim, every
                           * button pressing UI_KEY_MENU_FIRST */
} BottomState;

/* The bar, UI_BAR_H tall, from (0, y) across w pixels. Too narrow for every
 * word, the words go (the tips stay); still too narrow, the least needed
 * buttons go, their keys still working. */
void bar_bottom_draw(Ui *u, Canvas *c, int y, int w, const BottomState *st);

/* The world menu, opened by UI_KEY_WORLD_MENU: its right edge at right_x and
 * its bottom at bottom_y. Rows: new world 'r', copy code 'c', open code 'o',
 * save 's', quit 'q'; a browser page (page) has no save or quit. */
void bar_world_menu_draw(Ui *u, Canvas *c, int right_x, int bottom_y, bool page);

typedef struct {
    bool sandbox, paused, split, game_over;
    char branch;              /* 'A' or 'B' while split */
    int64_t year;
    int season;               /* a SEASON_ value (years.h) */
    int animals, speed;
    int weather_kind;         /* a WEATHER_ value; WEATHER_NONE for no box */
    float weather_left;       /* 1 at the event's start, 0 at its end */
    char weather_label[48];   /* "drought, ends in summer" */
    char following[24];       /* the followed animal's name, "" when nobody */
    char level[24];           /* the level shown, "" on the surface (levels.h) */
    char fps[24];             /* the frames-a-second counter (F3), "" when off */
    char notice[256];         /* the message of the moment, "" for none */
} TopState;

/* The top line, UI_BAR_H tall across w pixels, as boxes: mode, year and
 * season, animals, speed (or paused, or game over), the level when not the
 * surface, the branch while split,
 * the weather while an event holds, the notice in the room left, and who is
 * followed at the right end, with the frames-a-second counter left of that
 * while it is on. Nothing in it is a button. */
void bar_top_draw(Ui *u, Canvas *c, int w, const TopState *st);
/* "1,234,567 animals"; "1 animal" for one. */
void bar_animals_text(int animals, char *out, size_t size);

#endif
