/*
 * levels: the stack of places the map can show, top to bottom (the levels
 * design, 5.1): a high sky, the sky, the surface (the map as it always was),
 * underwater and an abyss. The sky, the surface and underwater are built;
 * the others are rows kept so that a new level is one more entry here and one
 * more look in scene.c. Pure rules, no drawing: which level comes next, when
 * the wheel dives, which level a followed animal takes the view to, and the
 * short fade of a camera's cut between levels.
 *
 * The level shown lives in Camera.level. The simulation never sees it.
 */
#ifndef VEREDAS_LEVELS_H
#define VEREDAS_LEVELS_H

#include <stdbool.h>
#include <stdint.h>

#include "camera.h"
#include "creature.h"

/* Top to bottom. An enum's names are numbered from 0 in order, so a lower
 * number is higher up, and the last, LEVEL_COUNT, is how many there are. */
enum {
    LEVEL_HIGH_SKY,
    LEVEL_SKY,
    LEVEL_SURFACE,
    LEVEL_UNDERWATER,
    LEVEL_ABYSS,
    LEVEL_COUNT
};

/* A camera's cut between levels: darker for half of this, the level changes,
 * lighter for the other half; in window milliseconds (the water's clock). At
 * its darkest the map keeps LEVELS_FADE_KEEP 256ths of its light. */
#define LEVELS_FADE_MS 600u
#define LEVELS_FADE_KEEP 24
/* A followed creature that changes level (a flyer going up or coming down)
 * takes the view with it only once it has stayed on its new level this long,
 * in window milliseconds, so a bird hopping up and down does not flip the
 * view (the air half's talk, 2026-11-21). */
#define LEVELS_SETTLE_MS 2000u

/* True for a level that can be shown: the sky, the surface and underwater. */
bool levels_built(int level);
/* The level's name for the top line: "" for the surface, which needs none. */
const char *levels_name(int level);
/* The next built level from level in direction dir (-1 up, +1 down), or level
 * itself when there is none that way. */
int levels_step(int level, int dir);
/* The level a creature is on: the sky for a flyer up in the air, underwater
 * for a sea body, else the surface (a flyer on the ground included). */
int levels_home(const Creature *c);

/*
 * The wheel (or ] and [) at the edge of the zoom steps: steps > 0 at Close on
 * the surface with the point under the mouse on water dives underwater, and
 * steps < 0 underwater at Close comes back up, both keeping Close. True when
 * it changed the level; false when the caller should zoom as usual.
 */
bool levels_dive(Camera *cam, int steps, bool over_water);

/*
 * Following and levels (the levels design, 5.4). While a creature is
 * followed, the view goes to its level, unless a level key was pressed while
 * following it (held), which wins until the player lets go or another creature
 * is followed. cut_next: the next new creature followed was picked by the
 * camera, a notice or j, so a change of level is a fade, not a jump.
 */
typedef struct {
    bool held;      /* a level key won over the followed creature's level */
    bool known;     /* id below is the creature followed last frame */
    uint32_t id;
    bool cut_next;  /* set by the caller just before such a follow */
    bool fading;
    bool switched;  /* during a fade: the level has changed already */
    uint64_t fade_start;
    int fade_to;
    bool settling;  /* the followed one is on another level than the view */
    int settle_home;
    uint64_t settle_start; /* when it was first seen on settle_home */
} LevelFollow;

/* Nothing held, nobody known, no fade. */
void levels_follow_init(LevelFollow *f);
/* A level key: one step in dir, at once; while following it is held. */
void levels_key(LevelFollow *f, Camera *cam, int dir, bool following);
/* The dive took the view to another level: like a key, it wins while following. */
void levels_dived(LevelFollow *f, bool following);
/*
 * Once a frame, after the camera has moved to the followed creature. following
 * and id say who is followed now; home is its level (levels_home). Starts or
 * runs a fade, changing the level at its middle. The same creature on a new
 * level takes the view there once it has stayed LEVELS_SETTLE_MS.
 */
void levels_follow(LevelFollow *f, Camera *cam, bool following, uint32_t id, int home,
                   uint64_t now_ms);
/* How much light the map keeps this frame, in 256ths: 256 unless fading. */
int levels_fade_keep(const LevelFollow *f, uint64_t now_ms);

#endif
