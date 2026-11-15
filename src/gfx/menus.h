/*
 * menus: the Sandbox tools' menus as boxes of buttons. Every row presses the
 * key that menu already takes (sandbox.c), so a click and a key do the same
 * thing, through the same code.
 */
#ifndef AGON_MENUS_H
#define AGON_MENUS_H

#include <stdbool.h>

#include "canvas.h"
#include "sandbox.h"
#include "sim.h"
#include "ui.h"

/* A setup row's checkbox: MENU_CHECK_SIZE square, at (hit x + 4, hit y + 5);
 * a checked one has its middle filled in the button text's colour. */
#define MENU_CHECK_SIZE 10

/* Every menu is this wide. */
#define MENU_W 380
/* Where a menu wants its top left corner, from the area's: a little in from
 * the corner of the map. */
#define MENU_INSET_X 8
#define MENU_INSET_Y 8
/* Air kept between a menu and the area's edges. */
#define MENU_MARGIN 2

/* True for the menus menu_draw draws: trait, weather, skip and setup. The skip
 * under way and the summary after it stay lines of text. */
bool menu_is_drawn(const Sandbox *b);
/* Where a w x h menu goes inside the area (the map, between the top line and
 * the bottom bar, which are drawn over it): near the area's top left corner,
 * moved left and up so it ends inside. False, with x and y untouched, when
 * the area is too small for it. */
bool menu_place(int area_x, int area_y, int area_w, int area_h, int w, int h,
                int *x, int *y);
/* Draws the open menu inside the area, nothing when none of those four is
 * open. When the area cannot hold the menu, a line saying so instead, with a
 * close button: the menu's keys still work. demo: the browser demo's build
 * (no menu differs yet, kept so the caller says it). */
void menu_draw(Ui *u, Canvas *c, int area_x, int area_y, int area_w, int area_h,
               const Sandbox *b, const Sim *s, bool demo);

#endif
