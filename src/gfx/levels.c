#include "levels.h"

bool levels_built(int level)
{
    return level == LEVEL_SKY || level == LEVEL_SURFACE || level == LEVEL_UNDERWATER;
}

const char *levels_name(int level)
{
    /* static const: one table for the whole run, not built on every call.
     * Each entry is a pointer to a string literal, which lives for the whole
     * run too, so handing it out is safe. */
    static const char *const NAMES[LEVEL_COUNT] = {
        "High sky", "Sky", "", "Underwater", "Abyss",
    };
    return level >= 0 && level < LEVEL_COUNT ? NAMES[level] : "";
}

int levels_step(int level, int dir)
{
    if (dir == 0) {
        return level;
    }
    int step = dir < 0 ? -1 : 1;
    /* The next built level that way: rows not built yet are passed over. */
    for (int next = level + step; next >= 0 && next < LEVEL_COUNT; next += step) {
        if (levels_built(next)) {
            return next;
        }
    }
    return level;
}

int levels_home(const Creature *c)
{
    if (c->up > 0) {
        return LEVEL_SKY; /* a flyer up in the air; down, it is a surface body */
    }
    return creature_sea_body(c) ? LEVEL_UNDERWATER : LEVEL_SURFACE;
}

bool levels_dive(Camera *cam, int steps, bool over_water)
{
    if (cam->zoom != CAMERA_CLOSE) {
        return false;
    }
    if (steps > 0 && cam->level == LEVEL_SURFACE && over_water) {
        cam->level = LEVEL_UNDERWATER;
        return true;
    }
    if (steps < 0 && cam->level == LEVEL_UNDERWATER) {
        cam->level = LEVEL_SURFACE;
        return true;
    }
    return false;
}

void levels_follow_init(LevelFollow *f)
{
    /* A compound literal: every field not named is 0 or false. */
    *f = (LevelFollow){.fade_to = LEVEL_SURFACE};
}

void levels_key(LevelFollow *f, Camera *cam, int dir, bool following)
{
    cam->level = levels_step(cam->level, dir);
    f->fading = false; /* the player's own key: at once, no fade */
    if (following) {
        f->held = true;
    }
}

void levels_dived(LevelFollow *f, bool following)
{
    f->fading = false;
    if (following) {
        f->held = true;
    }
}

/* A fade under way: the level changes at its middle, and it ends after
 * LEVELS_FADE_MS. */
static void run_fade(LevelFollow *f, Camera *cam, uint64_t now_ms)
{
    if (!f->fading) {
        return;
    }
    /* Unsigned: a clock that went back would make now - start wrap round to
     * a huge number, so that case is tested first and counts as the end. */
    uint64_t elapsed = now_ms < f->fade_start ? LEVELS_FADE_MS : now_ms - f->fade_start;
    if (!f->switched && elapsed >= LEVELS_FADE_MS / 2) {
        cam->level = f->fade_to;
        f->switched = true;
    }
    if (elapsed >= LEVELS_FADE_MS) {
        cam->level = f->fade_to; /* in case the middle was never seen */
        f->fading = false;
    }
}

void levels_follow(LevelFollow *f, Camera *cam, bool following, uint32_t id, int home,
                   uint64_t now_ms)
{
    run_fade(f, cam, now_ms);
    if (!following) {
        /* Let go: the view stays on the level it is on. */
        f->known = false;
        f->held = false;
        f->cut_next = false;
        f->settling = false;
        return;
    }
    if (!f->known || f->id != id) {
        bool cut = f->cut_next;
        f->known = true;
        f->id = id;
        f->held = false;
        f->cut_next = false;
        f->settling = false;
        if (home == cam->level) {
            f->fading = false;
        } else if (cut) {
            f->fading = true;
            f->switched = false;
            f->fade_start = now_ms;
            f->fade_to = home;
        } else {
            cam->level = home;
            f->fading = false;
        }
        return;
    }
    /* The same creature as before: its level, unless a key holds the view or
     * a fade is already taking it there, and only once it has stayed there
     * LEVELS_SETTLE_MS. */
    if (f->held || f->fading || home == cam->level) {
        f->settling = false;
        return;
    }
    if (!f->settling || f->settle_home != home) {
        f->settling = true; /* first seen on this level: the wait starts */
        f->settle_home = home;
        f->settle_start = now_ms;
        return;
    }
    /* As in run_fade: a clock that went back counts as the wait being over,
     * not as a subtraction wrapping round. */
    if (now_ms < f->settle_start || now_ms - f->settle_start >= LEVELS_SETTLE_MS) {
        cam->level = home;
        f->settling = false;
    }
}

int levels_fade_keep(const LevelFollow *f, uint64_t now_ms)
{
    if (!f->fading) {
        return 256;
    }
    const uint64_t half = LEVELS_FADE_MS / 2;
    uint64_t elapsed = now_ms < f->fade_start ? 0 : now_ms - f->fade_start;
    /* How far into darkness: 0 at either end, half at the middle. */
    uint64_t depth = elapsed < half ? elapsed
                     : elapsed < LEVELS_FADE_MS ? LEVELS_FADE_MS - elapsed
                                                : 0;
    /* Integer maths, multiplied before dividing so the fraction is not lost:
     * (256 - 24) * 300 is 69,600, far inside the 64-bit type. */
    return 256 - (int)((uint64_t)(256 - LEVELS_FADE_KEEP) * depth / half);
}
