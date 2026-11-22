/*
 * sprite: the pictures of creatures, and the cache that keeps them. A body
 * (its species, its rounded traits, its size level and the picture size) and a
 * pose go through the skeleton into a picture of inks; a palette turns the inks
 * into the family's colours when it is drawn. Pictures are drawn the first time
 * they are asked for and kept, so a frame of an unchanged world draws none.
 * Pure: the same body and pose always give the same picture.
 */
#ifndef VEREDAS_SPRITE_H
#define VEREDAS_SPRITE_H

#include <stdbool.h>
#include <stdint.h>

#include "creature.h"  /* Creature */
#include "settings.h"  /* Settings */
#include "skeleton.h"  /* SpritePose, the INK_ and ACTION_ values; includes species.h */

/*
 * How many tiles wide a body of this size is drawn: 1 below SPRITE_TWO_TILES,
 * 2 below SPRITE_THREE_TILES, then 3, however large. Only drawing reads it: in
 * the simulation every creature takes one cell. A size that is not a number
 * gives 1.
 */
#define SPRITE_TWO_TILES 3.0f
#define SPRITE_THREE_TILES 6.0f
int sprite_tiles_for(float size);

/* Which picture size: the world at Normal (Close doubles it), the world at Far,
 * and the detailed pictures of the lab and the panel. K is multiplied by 1, 0.5
 * and 1.6. Normal and Far use the small grid, detail the large one; Far skips
 * the marks. */
enum { SPRITE_BASE_NORMAL, SPRITE_BASE_FAR, SPRITE_BASE_DETAIL, SPRITE_BASES };

/* Everything a picture depends on but the pose. traits are rounded (0, 1/3,
 * 2/3 or 1), so close relatives share one body and so one set of pictures. */
typedef struct {
    BodyTraits traits;
    uint8_t species, k_level, base;
} SpriteBody;

/* From plain traits, a chosen species and a real size: the lab (task 9) and the
 * family strip use it. traits are rounded inside. */
SpriteBody sprite_body_of(const BodyTraits *traits, int species, float real_size, int base);
/* A living creature's: species_traits of its genome, the role from the rounded
 * traits, species_pick(lineage, role, creature_size), then sprite_body_of. */
SpriteBody sprite_body(const Creature *c, const Settings *s, int base);
/* Whether two bodies are the same body (field by field: a struct cannot be
 * compared with ==, and memcmp would also compare its padding bytes). */
bool sprite_body_equal(const SpriteBody *a, const SpriteBody *b);

/* How many frames each action's cycle is cut into, in ACTION_ order. */
extern const int SPRITE_FRAMES[ACTION_COUNT];  /* 24, 24, 16, 30, 39, 54, 6 */

/* One picture, facing right, as the cache keeps it. Grid pixel (x, y) is
 * ink[y * w + x]; (ox, oy) is the ground point under the body, the pixel drawn
 * at the creature's foot. Only x0..x1 - 1 by y0..y1 - 1 holds ink. The cache
 * keeps only that box, so its pictures have x0 = y0 = 0, x1 = w and y1 = h, and
 * the ground point may lie outside them. */
typedef struct {
    const uint8_t *ink;      /* w * h inks, inside the cache */
    int16_t w, h, ox, oy;    /* its grid and the ground point */
    int16_t x0, y0, x1, y1, eye_x, eye_y, shadow_dx;  /* eye -1, -1: none (Far) */
    uint8_t shadow_rx, shadow_ry;
    bool eye_big, eye_detail;   /* eye_detail: K above 1.6, the mockup's larger eyes */
} SpritePicture;

/* Every ink's colour, as canvas pixels. ink[INK_NONE] is never drawn. */
typedef struct { uint32_t ink[INK_COUNT]; } SpritePalette;
/* A creature's colours: the hue is render_display_hue(hue), the family's hue as
 * the design says (section 5.2); saturation and lightness from the species' look. */
void sprite_palette(SpritePalette *out, int species, float hue);
/* The species' own look colour, hue included: the lab. */
void sprite_palette_look(SpritePalette *out, int species);
/* in with every colour flash (0..1) of the way to white: a creature being hit. */
void sprite_palette_flash(SpritePalette *out, const SpritePalette *in, float flash);

/* Small-grid pictures kept. Each is cut to its inked box first, which is 296
 * bytes on average on a crowded world (2026-11-07) against 3,584 for the whole
 * grid: SPRITE_ENTRIES of them go in slots of SPRITE_TRIMMED_BYTES, and the few
 * bigger ones in SPRITE_ENTRIES / 6 whole-grid slots. 27.2 MB at most, taken
 * only as pictures fill it. Doubled from 12,288 on 2026-11-21: the air half's
 * busier world (birds drawn from the skeleton, 24 poses each) asked for more
 * pictures than that held, so pictures were thrown out and drawn again soon
 * after; doubled, Far zoom at 1080p took about 6% less time a frame, for 7 MB
 * more at the peak of a 600-frame run (95 to 102 MB). */
#define SPRITE_ENTRIES 24576
#define SPRITE_TRIMMED_BYTES 512
#define SPRITE_DETAIL_ENTRIES 512  /* large-grid pictures */

/* One kept picture: its key, the next entry of the same hash chain (-1: none)
 * and its two neighbours in the store's use order (newer, older; -1: none).
 * Its inks are in the store's block. */
typedef struct {
    uint64_t key;
    int32_t next;
    int32_t newer, older;
    SpritePicture picture;
} SpriteEntry;

/* Pictures of up to slot_bytes inks each. heads[b] is the first entry whose
 * key hashes to bucket b, or -1. used counts the entries handed out so far:
 * until the store is full a new picture takes the next one, then the one
 * unused longest. Every entry handed out is on one chain in the order they
 * were last asked for, newest to oldest, so that one is found at once
 * (newest, oldest: its two ends, -1 while the store is empty). */
typedef struct {
    SpriteEntry *entries;
    uint8_t *inks;         /* entries * slot_bytes bytes */
    int32_t *heads;
    int32_t newest, oldest;
    int capacity, buckets, used, slot_bytes;
} SpriteStore;

typedef struct {
    /* trimmed: small-grid pictures of up to SPRITE_TRIMMED_BYTES once cut;
     * small: the bigger ones; large: the detailed grid's (the panel, the lab). */
    SpriteStore trimmed, small, large;
    SkeletonRaster *raster;  /* the scratch every picture is drawn in: 22 KB */
    long builds;  /* pictures drawn so far: the tests and slow_pictures read it */
    bool art;     /* true: species with drawn frames (art.h) use them; on from init */
} SpriteCache;

/* All the memory, allocated once: entries trimmed small pictures, entries / 6
 * (at least 1) bigger ones, and detail_entries detailed ones. False when it
 * runs out; nothing to free then. */
bool sprite_cache_init(SpriteCache *c, int entries, int detail_entries);
/* Frees the memory and empties *c; safe to call twice. */
void sprite_cache_free(SpriteCache *c);
/* Forgets every picture: after r or a load. builds is kept. */
void sprite_cache_reset(SpriteCache *c);
/* Turns the drawn frames (art.h) on or off; the pictures kept are forgotten
 * when that changes, since they were drawn the other way. */
void sprite_cache_set_art(SpriteCache *c, bool on);
/* Drawn the first time it is asked for; the picture stays good until the next
 * call, which may reuse its entry. action is an ACTION_ value; frame is cut to
 * the action's cycle; pose_at (pose.h) gives its pose. */
SpritePicture sprite_picture(SpriteCache *c, const SpriteBody *b, int action, int frame);
/* Where up to count (at most 3) scars fall on the picture's body, as shares of
 * its extent: grid pixels written to out, how many returned (at most max). */
int sprite_scar_pixels(const SpritePicture *p, int count, int16_t out[][2], int max);
/* One pixel of an eye over a picture: its offset from (eye_x, eye_y) in grid
 * pixels, and its ink. */
typedef struct { int8_t dx, dy; uint8_t ink; } SpriteEyePixel;
#define SPRITE_EYE_PIXELS_MAX 10
/* The eye of picture p, open (eyes 1), half shut (0.4) or shut (0), as the
 * mockup draws it (v9:483-492): in order, a later pixel over an earlier one.
 * How many were written to out; 0 when the picture has no eye (Far). */
int sprite_eye_pixels(const SpritePicture *p, float eyes,
                      SpriteEyePixel out[SPRITE_EYE_PIXELS_MAX]);
/* The K a body is drawn at: its level's K times the species' scale, capped so
 * the body fits its grid, times the base's factor. */
float sprite_k(const SpriteBody *b);

#endif
