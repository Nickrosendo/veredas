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

/* True for the menus menu_draw draws: trait, weather, skip and setup. The skip
 * under way and the summary after it stay lines of text. */
bool menu_is_drawn(const Sandbox *b);
/* Draws the open menu with its top left corner at (x, y), nothing when none
 * of those four is open. demo: the browser demo's build (no menu differs yet,
 * kept so the caller says it). */
void menu_draw(Ui *u, Canvas *c, int x, int y, const Sandbox *b, const Sim *s,
               bool demo);

#endif
