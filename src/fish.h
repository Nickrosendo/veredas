/*
 * fish: shoals in the sea (the livelier hunting design, 5.1). One number per
 * cell, 0..1, on deep water only, like plankton. Fish have no genes and no
 * brains: they grow where there is plankton, eating a little of it, drift
 * toward richer plankton, and slide away from any creature that ate fish
 * lately. Sea bodies and flyers standing on deep water bite them as meat.
 *
 * The drift away from eaters is a rule of a food layer, as plankton's growth
 * is: it reads only what a creature did (Creature.fish_meal_at), never what
 * kind of creature it is.
 */
#ifndef VEREDAS_FISH_H
#define VEREDAS_FISH_H

#include "creature.h" /* Pool */
#include "settings.h"
#include "world.h"

/* A cell needs at least this much fish to show on screen (scene.c). */
#define FISH_SHOWN 0.1f

/* A new world's shoals: every deep cell whose plankton cap is at least
 * fish_start_rich starts at its fish cap (fish_cap_share of its plankton cap);
 * the rest of the sea starts empty, and fills as fish drift and grow. Call
 * after plankton_prepare. Draws no random number. */
void fish_start(World *w, const Settings *s);
/* One update (every fish_every ticks): growth toward a cap set by each cell's
 * plankton, then the drift (fish.c). p is read for the creatures that ate fish
 * lately; p->now must be the current tick. */
void fish_step(World *w, const Pool *p, const Settings *s);
/* How many cells hold at least this much fish. */
int fish_count_cells(const World *w, float at_least);
/* Whether c ate fish recently enough to scare them: within fish_fear_ticks of
 * now. Unsigned subtraction, so right across the 32-bit tick's wrap. */
bool fish_scares(const Creature *c, uint32_t now, const Settings *s);

#endif
