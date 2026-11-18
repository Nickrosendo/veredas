/*
 * lab: the species lab, a page of the window (the b key) that shows every
 * species in rows by role, each standing on a strip of ground with its name,
 * drawn from the detailed pictures. It only looks: it reads the picture cache
 * and writes pixels, and touches nothing of the simulation.
 */
#ifndef VEREDAS_LAB_H
#define VEREDAS_LAB_H

#include <stdint.h>

#include "canvas.h"
#include "sprite.h"

/* Species per row: the most any row has (the hunters, sea hunters with them). */
#define LAB_COLUMNS 5
/* The rows: one per land role (hunters, swimmers, armoured, runners,
 * watchers, plant-eaters, as the ROLE_ values number them). A sea animal is
 * shown in its land role's row: a seventh row would not fit a 1280 x 720
 * window at scale 2. */
#define LAB_ROWS ROLE_SEA_HUNTER
/* The number keys 1 to LAB_KEYS choose what the animals do. */
#define LAB_KEYS 6
/* A ground strip's bottom: the line and the six rows of ground under it, in
 * art pixels. */
#define LAB_GROUND_ROWS 7

/* The ACTION_ value number key `number` (1 to LAB_KEYS: walk, run, rest,
 * graze, bite, sleep) chooses; -1 for any other number. */
int lab_action_of_key(int number);

/* The row a species is shown in: its role, a sea role's land twin at sea
 * (sea hunters with the hunters, the sea's armoured with the armoured, the
 * sea's grazer with the plant-eaters). */
int lab_row_of(int species);
/* The species shown in row `row` (0 to LAB_ROWS - 1) at this column, in table
 * order; -1 for an empty cell or a place outside the page. */
int lab_species_at(int row, int column);

/* Every species in rows by role, each on a strip of ground, from the detailed
 * pictures (SPRITE_BASE_DETAIL, the mockup lab's scale) at scale 2, or scale 1
 * when the area is too small for two. action is an ACTION_ value; body_ms the
 * window's body clock, which picks each animal's frame of the action (each at
 * its own pace and out of step) and its blinks. parts true puts every part
 * painted over a species' frames on it (plates, a horn, a fin), to look at
 * them. Draws only inside the area (x, y, w, h); an area too small for the
 * page is just filled. */
void lab_draw(SpriteCache *sprites, Canvas *c, int action, bool parts, uint64_t body_ms,
              int x, int y, int w, int h);

/* The light screen while a world's first years run (the prelude): a strip
 * of ground across (x, y, w) with a few animals walking along it, left to
 * right and round again, by the body clock body_ms. A few dozen pixels' worth
 * of cached pictures a frame, so it costs next to nothing beside the world.
 * Drawn only when h holds lab_parade_height(w) rows. */
void lab_draw_parade(SpriteCache *sprites, Canvas *c, uint64_t body_ms, int x, int y,
                     int w, int h);
/* The parade strip's height in pixels for an area w wide: 100, or 200 (the
 * animals at twice the size) from 1200 wide. */
int lab_parade_height(int w);

/* The sky's colour above a ground strip, as a canvas pixel. */
uint32_t lab_sky(void);
/* A strip of the mockup's lab ground in the box (x, y, w, h): sky, then a
 * line and ground of two greens by hash in its bottom LAB_GROUND_ROWS art
 * pixels of `scale` screen pixels each. salt keeps strips from repeating one
 * another. Drawn only inside clip. Returns the line's canvas row: a picture's
 * ground point goes one art pixel below it. The panel uses it too. */
int lab_draw_ground(Canvas *c, int x, int y, int w, int h, int scale, int salt,
                    CanvasClip clip);

/* Where one cell's strip is for an area of w by h at the origin: its box in
 * (bx, by, bw, bh), and the scale the page is drawn at (0, with the box
 * untouched, when the area is too small for the page). The tests read it. */
int lab_cell_box(SpriteCache *sprites, int action, int w, int h, int role, int column,
                 int *bx, int *by, int *bw, int *bh);

#endif
