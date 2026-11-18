/*
 * plankton: the sea's food (the water and air design, 5.3). One number per
 * cell, 0..1, on deep water only. It has no genes: it grows towards a cap set
 * by how warm the water is and how near the shore, and thins in winter. A sea
 * body's bite takes it as leaf food, PLANKTON_STEP at a time, as a bite takes
 * a plant's leaves one density step at a time.
 */
#ifndef VEREDAS_PLANKTON_H
#define VEREDAS_PLANKTON_H

#include "settings.h"
#include "world.h"

/* One bite step of plankton: a full cell (1) is four steps, as a full plant
 * (VEG_MAX) is four steps of leaves. */
#define PLANKTON_STEP 0.25f
/* A deep cell holding this much, one bite step, counts as plankton-rich for
 * the sea's birth brake, as a cell holding any plant counts for the land's.
 * Half a cell was tried first: grazing kept nearly every cell under it, and
 * the brake shut sea births entirely. */
#define PLANKTON_RICH PLANKTON_STEP

/* Works out every cell's cap (World.plankton_cap) from the terrain and the
 * temperature: 0 off deep water. Call it once the world's terrain and
 * climate exist, and again after loading a world. */
void plankton_prepare(World *w, const Settings *s);
/* Fills every deep cell to its cap: a new world's sea starts full. */
void plankton_start(World *w);
/* One update: every deep cell moves plankton_growth of the way to its cap,
 * or to plankton_winter of it in winter (so a winter thins the sea, and a
 * spring grows it back). season is a SEASON_ value. */
void plankton_step(World *w, const Settings *s, int season);
/* How many deep cells hold PLANKTON_RICH or more. */
int plankton_count_rich(const World *w);
/* How many bite steps of plankton cell i offers: whole PLANKTON_STEPs. */
int plankton_steps(const World *w, int i);

#endif
