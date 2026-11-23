#include "test.h"

#include <string.h> /* strcmp */

#include "levels.h"

/* A camera at a zoom step on a level, over a 64 by 32 world. */
static Camera camera_at(int zoom, int level)
{
    Camera c;
    camera_init(&c, 64, 32, 320, 200);
    c.zoom = zoom;
    c.level = level;
    return c;
}

static void test_the_list(void)
{
    CHECK(LEVEL_HIGH_SKY < LEVEL_SKY && LEVEL_SKY < LEVEL_SURFACE &&
          LEVEL_SURFACE < LEVEL_UNDERWATER && LEVEL_UNDERWATER < LEVEL_ABYSS);
    CHECK(levels_built(LEVEL_SURFACE));
    CHECK(levels_built(LEVEL_UNDERWATER));
    CHECK(levels_built(LEVEL_SKY)); /* step 3, with the air half */
    CHECK(!levels_built(LEVEL_HIGH_SKY));
    CHECK(!levels_built(LEVEL_ABYSS));
    CHECK(strcmp(levels_name(LEVEL_SURFACE), "") == 0);
    CHECK(strcmp(levels_name(LEVEL_UNDERWATER), "Underwater") == 0);
    CHECK(strcmp(levels_name(LEVEL_SKY), "Sky") == 0);
    CHECK(strcmp(levels_name(-1), "") == 0);
    CHECK(strcmp(levels_name(LEVEL_COUNT), "") == 0);
    Camera c;
    camera_init(&c, 64, 32, 320, 200);
    CHECK(c.level == LEVEL_SURFACE);
}

/* The keys step and stop at the ends of what is built. */
static void test_step_stops_at_the_ends(void)
{
    CHECK(levels_step(LEVEL_SURFACE, 1) == LEVEL_UNDERWATER);
    CHECK(levels_step(LEVEL_UNDERWATER, 1) == LEVEL_UNDERWATER); /* no abyss yet */
    CHECK(levels_step(LEVEL_UNDERWATER, -1) == LEVEL_SURFACE);
    CHECK(levels_step(LEVEL_SURFACE, -1) == LEVEL_SKY);
    CHECK(levels_step(LEVEL_SKY, -1) == LEVEL_SKY); /* no high sky yet */
    CHECK(levels_step(LEVEL_SKY, 1) == LEVEL_SURFACE);
    CHECK(levels_step(LEVEL_SURFACE, 0) == LEVEL_SURFACE);
    LevelFollow f;
    levels_follow_init(&f);
    Camera c = camera_at(CAMERA_FAR, LEVEL_SURFACE);
    levels_key(&f, &c, 1, false);
    CHECK(c.level == LEVEL_UNDERWATER);
    CHECK(c.zoom == CAMERA_FAR); /* a key keeps the zoom */
    levels_key(&f, &c, 1, false);
    CHECK(c.level == LEVEL_UNDERWATER);
    levels_key(&f, &c, -1, false);
    CHECK(c.level == LEVEL_SURFACE);
    levels_key(&f, &c, -1, false);
    CHECK(c.level == LEVEL_SKY);
    levels_key(&f, &c, -1, false);
    CHECK(c.level == LEVEL_SKY);
    CHECK(!f.held); /* nobody followed: nothing to hold against */
}

/* The dive: only at Close, only on the surface, only over water, only in. */
static void test_dive_only_over_water_at_close(void)
{
    Camera c = camera_at(CAMERA_CLOSE, LEVEL_SURFACE);
    CHECK(!levels_dive(&c, 1, false)); /* over land: nothing, as today */
    CHECK(c.level == LEVEL_SURFACE);
    CHECK(!levels_dive(&c, -1, true)); /* outward on the surface: a zoom */
    CHECK(c.level == LEVEL_SURFACE);
    CHECK(levels_dive(&c, 1, true));
    CHECK(c.level == LEVEL_UNDERWATER);
    CHECK(c.zoom == CAMERA_CLOSE);
    CHECK(!levels_dive(&c, 1, true)); /* already down */
    CHECK(c.level == LEVEL_UNDERWATER);
    /* Out from underwater at Close: up, and still Close. */
    CHECK(levels_dive(&c, -1, false));
    CHECK(c.level == LEVEL_SURFACE);
    CHECK(c.zoom == CAMERA_CLOSE);

    for (int zoom = CAMERA_NORMAL; zoom < CAMERA_ZOOMS; zoom++) {
        Camera n = camera_at(zoom, LEVEL_SURFACE);
        CHECK(!levels_dive(&n, 1, true));
        CHECK(n.level == LEVEL_SURFACE);
        /* Underwater at a wider zoom, the wheel zooms: it is not taken. */
        n.level = LEVEL_UNDERWATER;
        CHECK(!levels_dive(&n, -1, true));
        CHECK(!levels_dive(&n, 1, true));
        CHECK(n.level == LEVEL_UNDERWATER);
    }
}

static void test_home_level(void)
{
    Creature c;
    memset(&c, 0, sizeof(c));
    CHECK(levels_home(&c) == LEVEL_SURFACE);
    c.genome.g[GENE_SWIM] = 1.0f;
    CHECK(levels_home(&c) == LEVEL_UNDERWATER);
    /* A flyer on the ground is a surface body; up in the air, a sky one (a
     * gull too, over the sea). */
    c.genome.g[GENE_SWIM] = 0.0f;
    c.genome.g[GENE_WINGS] = 1.0f;
    CHECK(levels_home(&c) == LEVEL_SURFACE);
    c.up = 1;
    CHECK(levels_home(&c) == LEVEL_SKY);
    c.genome.g[GENE_SWIM] = 1.0f;
    CHECK(levels_home(&c) == LEVEL_SKY);
}

/* Following a flyer: the view rises to the sky once it has stayed up for
 * LEVELS_SETTLE_MS, and comes back down once it has stayed down as long
 * (the air half's talk, 2026-11-21). The change itself is at once, no fade. */
static void test_follow_flyer_waits_to_settle(void)
{
    LevelFollow f;
    levels_follow_init(&f);
    Camera c = camera_at(CAMERA_NORMAL, LEVEL_SURFACE);
    levels_follow(&f, &c, true, 7, LEVEL_SURFACE, 0);
    CHECK(c.level == LEVEL_SURFACE);
    levels_follow(&f, &c, true, 7, LEVEL_SKY, 1000); /* it goes up */
    CHECK(c.level == LEVEL_SURFACE);
    levels_follow(&f, &c, true, 7, LEVEL_SKY, 1000 + LEVELS_SETTLE_MS - 1);
    CHECK(c.level == LEVEL_SURFACE);
    levels_follow(&f, &c, true, 7, LEVEL_SKY, 1000 + LEVELS_SETTLE_MS);
    CHECK(c.level == LEVEL_SKY);
    CHECK(!f.fading);
    CHECK(levels_fade_keep(&f, 1000 + LEVELS_SETTLE_MS) == 256);
    levels_follow(&f, &c, true, 7, LEVEL_SURFACE, 5000); /* it lands */
    CHECK(c.level == LEVEL_SKY);
    levels_follow(&f, &c, true, 7, LEVEL_SURFACE, 5000 + LEVELS_SETTLE_MS);
    CHECK(c.level == LEVEL_SURFACE);
}

/* A bird hopping up and down more often than the settle time never flips the
 * view; each hop starts the wait again. */
static void test_hopping_flyer_does_not_flip(void)
{
    LevelFollow f;
    levels_follow_init(&f);
    Camera c = camera_at(CAMERA_CLOSE, LEVEL_SURFACE);
    levels_follow(&f, &c, true, 7, LEVEL_SURFACE, 0);
    for (uint64_t t = 100; t < 20000; t += 100) {
        int home = (t / 1000) % 2 == 0 ? LEVEL_SURFACE : LEVEL_SKY; /* 1 s each */
        levels_follow(&f, &c, true, 7, home, t);
        CHECK(c.level == LEVEL_SURFACE);
    }
}

/* A newly followed flyer up takes the view to the sky at once (no wait):
 * the wait is only for the same animal changing level. A key held while
 * following still wins over a settled change. */
static void test_new_flyer_at_once_and_key_wins(void)
{
    LevelFollow f;
    levels_follow_init(&f);
    Camera c = camera_at(CAMERA_NORMAL, LEVEL_SURFACE);
    levels_follow(&f, &c, true, 8, LEVEL_SKY, 0);
    CHECK(c.level == LEVEL_SKY);
    levels_key(&f, &c, +1, true); /* down to the surface, held */
    CHECK(c.level == LEVEL_SURFACE);
    levels_follow(&f, &c, true, 8, LEVEL_SKY, 10000);
    CHECK(c.level == LEVEL_SURFACE);
    levels_follow(&f, &c, true, 8, LEVEL_SURFACE, 20000);
    levels_follow(&f, &c, true, 8, LEVEL_SKY, 30000);
    levels_follow(&f, &c, true, 8, LEVEL_SKY, 30000 + LEVELS_SETTLE_MS);
    CHECK(c.level == LEVEL_SURFACE);
}

/* Following takes the view to the creature's level; a key wins until let go
 * or another is followed; letting go keeps the level. */
static void test_follow_and_hold(void)
{
    LevelFollow f;
    levels_follow_init(&f);
    Camera c = camera_at(CAMERA_NORMAL, LEVEL_SURFACE);
    levels_follow(&f, &c, true, 7, LEVEL_UNDERWATER, 1000); /* a click on a shark */
    CHECK(c.level == LEVEL_UNDERWATER);
    CHECK(levels_fade_keep(&f, 1000) == 256); /* a click: no fade */
    levels_follow(&f, &c, true, 7, LEVEL_UNDERWATER, 1033);
    CHECK(c.level == LEVEL_UNDERWATER);

    levels_key(&f, &c, -1, true); /* up, while following the shark */
    CHECK(c.level == LEVEL_SURFACE);
    CHECK(f.held);
    for (uint64_t t = 1066; t < 2000; t += 33) {
        levels_follow(&f, &c, true, 7, LEVEL_UNDERWATER, t);
    }
    CHECK(c.level == LEVEL_SURFACE); /* the key still wins */

    levels_follow(&f, &c, false, 0, LEVEL_SURFACE, 2000); /* let go */
    CHECK(!f.held);
    CHECK(c.level == LEVEL_SURFACE); /* stays where it is */
    levels_follow(&f, &c, true, 7, LEVEL_UNDERWATER, 2033); /* the shark again */
    CHECK(c.level == LEVEL_UNDERWATER);

    levels_key(&f, &c, -1, true);
    CHECK(c.level == LEVEL_SURFACE);
    levels_follow(&f, &c, true, 9, LEVEL_UNDERWATER, 2066); /* another shark */
    CHECK(!f.held);
    CHECK(c.level == LEVEL_UNDERWATER);

    levels_follow(&f, &c, true, 11, LEVEL_SURFACE, 2100); /* a wolf */
    CHECK(c.level == LEVEL_SURFACE);
    levels_follow(&f, &c, false, 0, LEVEL_SURFACE, 2133);
    levels_key(&f, &c, 1, false);
    levels_follow(&f, &c, false, 0, LEVEL_SURFACE, 2166);
    CHECK(c.level == LEVEL_UNDERWATER); /* nobody followed: the key's level */

    /* The dive while following a land animal holds the view down. */
    Camera d = camera_at(CAMERA_CLOSE, LEVEL_SURFACE);
    levels_follow(&f, &d, true, 12, LEVEL_SURFACE, 3000);
    CHECK(levels_dive(&d, 1, true));
    levels_dived(&f, true);
    levels_follow(&f, &d, true, 12, LEVEL_SURFACE, 3033);
    CHECK(d.level == LEVEL_UNDERWATER);
}

/* The camera's cut: dark, the level changes at the middle, light again. */
static void test_cut_fades(void)
{
    LevelFollow f;
    levels_follow_init(&f);
    Camera c = camera_at(CAMERA_NORMAL, LEVEL_SURFACE);
    levels_follow(&f, &c, true, 3, LEVEL_SURFACE, 0);
    f.cut_next = true; /* Documentary's camera picks a shark */
    const uint64_t start = 10000;
    levels_follow(&f, &c, true, 4, LEVEL_UNDERWATER, start);
    CHECK(!f.cut_next); /* once per pick */
    CHECK(c.level == LEVEL_SURFACE);
    int last = 256;
    int changes = 0;
    int was;
    for (uint64_t t = start; t < start + LEVELS_FADE_MS / 2; t += 30) {
        levels_follow(&f, &c, true, 4, LEVEL_UNDERWATER, t);
        int keep = levels_fade_keep(&f, t);
        CHECK(keep <= last); /* darker and darker */
        CHECK(c.level == LEVEL_SURFACE);
        last = keep;
    }
    levels_follow(&f, &c, true, 4, LEVEL_UNDERWATER, start + LEVELS_FADE_MS / 2);
    CHECK(levels_fade_keep(&f, start + LEVELS_FADE_MS / 2) == LEVELS_FADE_KEEP);
    CHECK(c.level == LEVEL_UNDERWATER);
    last = LEVELS_FADE_KEEP;
    was = c.level;
    for (uint64_t t = start + LEVELS_FADE_MS / 2; t <= start + LEVELS_FADE_MS + 60;
         t += 30) {
        levels_follow(&f, &c, true, 4, LEVEL_UNDERWATER, t);
        int keep = levels_fade_keep(&f, t);
        CHECK(keep >= last); /* lighter again */
        last = keep;
        changes += c.level != was;
        was = c.level;
    }
    CHECK(last == 256);
    CHECK(!f.fading);
    CHECK(changes == 0); /* changed once, at the middle, never back */
    CHECK(c.level == LEVEL_UNDERWATER);

    /* A cut to the same level: no fade. */
    f.cut_next = true;
    levels_follow(&f, &c, true, 5, LEVEL_UNDERWATER, 20000);
    CHECK(!f.fading);
    CHECK(levels_fade_keep(&f, 20000) == 256);
    /* A key during a fade stops it at once. */
    f.cut_next = true;
    levels_follow(&f, &c, true, 6, LEVEL_SURFACE, 30000);
    CHECK(f.fading);
    levels_key(&f, &c, 1, true);
    CHECK(!f.fading);
    CHECK(levels_fade_keep(&f, 30100) == 256);
    /* A pick that failed (nobody followed): the cut is forgotten. */
    f.cut_next = true;
    levels_follow(&f, &c, false, 0, LEVEL_SURFACE, 40000);
    CHECK(!f.cut_next);
}

/* The camera's pick for a dive: the cut goes to the surface, where the dive
 * ends, although the flyer is up; the view waits there while it stays up, and
 * once it has come down it is followed as any other. */
static void test_dive_pick_cuts_to_surface(void)
{
    LevelFollow f;
    levels_follow_init(&f);
    Camera c = camera_at(CAMERA_NORMAL, LEVEL_UNDERWATER);
    f.cut_next = true;
    f.cut_level = LEVEL_SURFACE;
    levels_follow(&f, &c, true, 9, LEVEL_SKY, 0);
    CHECK(f.fading && f.fade_to == LEVEL_SURFACE);
    levels_follow(&f, &c, true, 9, LEVEL_SKY, LEVELS_FADE_MS);
    CHECK(c.level == LEVEL_SURFACE);
    levels_follow(&f, &c, true, 9, LEVEL_SKY, 10 * LEVELS_SETTLE_MS); /* still up */
    CHECK(c.level == LEVEL_SURFACE);
    levels_follow(&f, &c, true, 9, LEVEL_SURFACE, 11 * LEVELS_SETTLE_MS); /* down */
    CHECK(f.cut_level == -1);
    levels_follow(&f, &c, true, 9, LEVEL_SKY, 12 * LEVELS_SETTLE_MS); /* up again */
    levels_follow(&f, &c, true, 9, LEVEL_SKY, 13 * LEVELS_SETTLE_MS);
    CHECK(c.level == LEVEL_SKY);
}

int main(void)
{
    test_dive_pick_cuts_to_surface();
    test_the_list();
    test_step_stops_at_the_ends();
    test_dive_only_over_water_at_close();
    test_home_level();
    test_follow_flyer_waits_to_settle();
    test_hopping_flyer_does_not_flip();
    test_new_flyer_at_once_and_key_wins();
    test_follow_and_hold();
    test_cut_fades();
    TEST_SUMMARY();
}
