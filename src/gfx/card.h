/*
 * card: the followed animal's card, drawn where the world panel is while
 * someone is followed. Top to bottom: its picture, its name, its kind of body,
 * family colour and age, what it is doing and why (doing.h) in a highlighted
 * box, energy and health bars, what it eats with its young and kills, a bar
 * per trait a player can give, its family's bodies over time, and buttons:
 * story (l), let go (Esc) and, in Sandbox, give trait (g).
 *
 * The buttons keep the bottom of the card; the parts above them that do not
 * fit a short window are left out from the bottom up (the family strip
 * first), so nothing is drawn outside the card's rectangle.
 */
#ifndef AGON_CARD_H
#define AGON_CARD_H

#include <stdbool.h>
#include <stdint.h>

#include "canvas.h"
#include "creature.h"
#include "family.h"
#include "scene.h"
#include "sim.h"
#include "ui.h"

/* Draws the card for followed (living, in s) inside the rectangle x, y, w, h,
 * and adds its buttons to u. body_ms is the body clock the picture breathes
 * by. */
void card_draw(Ui *u, Scene *sc, Canvas *c, int x, int y, int w, int h,
               const Sim *s, const Families *f, const Creature *followed,
               uint64_t body_ms, bool sandbox);

#endif
