/*
 * weather_fx: a weather event drawn over the map while it holds. Drawing
 * only: the ground keeps its colours and the simulation is never read or
 * touched. Hard winter is falling snow under a cold blue-grey wash, drought
 * is dust blowing across under a warm yellow haze, a warm year is pale
 * specks of light rising under a brighter, greener cast.
 */
#ifndef AGON_WEATHER_FX_H
#define AGON_WEATHER_FX_H

#include <stdbool.h>
#include <stdint.h>

#include "canvas.h"

/* How long the effect takes to come in, and to go after the event ends. */
#define WEATHER_FX_FADE_MS 1500
/* About one speck per this many pixels of the view, at full strength. */
#define WEATHER_FX_PIXELS_PER_SPECK 2000

/*
 * Draws kind's effect (a WEATHER_ kind from weather.h) over the view
 * rectangle at strength 0..1; 0, WEATHER_NONE or an unknown kind draws
 * nothing. ms moves the flakes and the dust; the same ms draws the same
 * pixels. No state: each speck's place is worked out from its number and ms.
 */
void weather_fx_draw(Canvas *c, int vx, int vy, int vw, int vh, int kind,
                     float strength, uint64_t ms);

/* The fade: moves *strength toward 1 while active, toward 0 otherwise, by
 * elapsed_ms / WEATHER_FX_FADE_MS, kept within 0..1. */
void weather_fx_fade(float *strength, bool active, uint64_t elapsed_ms);

#endif
