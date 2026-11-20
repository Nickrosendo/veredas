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
    CHECK(!levels_built(LEVEL_SKY)); /* step 3, with the air half */
    CHECK(!levels_built(LEVEL_HIGH_SKY));
    CHECK(!levels_built(LEVEL_ABYSS));
    CHECK(strcmp(levels_name(LEVEL_SURFACE), "") == 0);
    CHECK(strcmp(levels_name(LEVEL_UNDERWATER), "Underwater") == 0);
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
    CHECK(levels_step(LEVEL_SURFACE, -1) == LEVEL_SURFACE); /* no sky yet */
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
    levels_key(&f, &c, -1, false);
    CHECK(c.level == LEVEL_SURFACE);
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

int main(void)
{
    test_the_list();
    test_step_stops_at_the_ends();
    test_dive_only_over_water_at_close();
    test_home_level();
    test_follow_and_hold();
    test_cut_fades();
    TEST_SUMMARY();
}
