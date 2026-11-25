/*
 * Shared by test_same_world.c and slow_same_world.c: a world grown from a seed
 * and then stepped tick by tick, folded into one number, to compare with the
 * number the code before the cheaper tick study made (the cheaper tick study,
 * 2026-11-25). A speed-up must not change the world, and "the same" is checked
 * here, not assumed: after every one of the compared ticks, both the world's
 * fingerprint (sim_checksum) and its story's (sim_story_checksum) go into the
 * number, so a difference on any tick, even one that later evened out, shows.
 *
 * The pinned numbers depend on the maths library, as the seed-7 fingerprint
 * in test_sim.c does: they were made on a machine whose seed-7 fingerprint is
 * test_sim.c's SEED_7_AFTER_500, and are checked only where it is that too.
 * Elsewhere (GitHub's machines, Windows) the number is printed for reading.
 * When the rules change, both pinned numbers change with them, like the
 * seed-7 one.
 */
#ifndef VEREDAS_SAME_WORLD_H
#define VEREDAS_SAME_WORLD_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "settings.h"
#include "sim.h"

/* test_sim.c's SEED_7_AFTER_500: seed 7 at 64 by 32 after 500 steps, on the
 * machine the numbers below were pinned on. */
#define SAME_WORLD_MATHS 0x8898b6ba0272fbb8ULL

/* Whether this machine's maths gives the worlds the pinned numbers came from. */
static bool same_world_maths_as_pinned(void)
{
    Settings set = settings_default();
    Sim s;
    if (!sim_init(&s, &set, 7, 64, 32)) {
        return false;
    }
    for (int i = 0; i < 500; i++) {
        sim_step(&s);
    }
    bool same = sim_checksum(&s) == SAME_WORLD_MATHS;
    sim_free(&s);
    return same;
}

/* Grows seed's world, w by h, for grow ticks, then steps it ticks more,
 * folding each of those ticks' two fingerprints into the answer (as
 * tests/tick_report.c does). *before and *after are how many animals were
 * alive when the compared ticks began and ended; 0 on a failed start. */
static uint64_t same_world_fold(uint64_t seed, int w, int h, int grow, int ticks,
                                int *before, int *after)
{
    Settings set = settings_default();
    Sim s;
    *before = *after = 0;
    if (!sim_init(&s, &set, seed, w, h)) {
        return 0;
    }
    for (int i = 0; i < grow; i++) {
        sim_step(&s);
    }
    *before = s.pool.count;
    uint64_t fold = 0;
    for (int i = 0; i < ticks; i++) {
        sim_step(&s);
        /* 1099511628211 is the 64-bit FNV prime: multiplying by it spreads
         * every bit of the number so far before the next fingerprint is
         * mixed in with ^ (exclusive or). */
        fold = fold * 1099511628211ULL ^ sim_checksum(&s);
        fold = fold * 1099511628211ULL ^ sim_story_checksum(&s);
    }
    *after = s.pool.count;
    sim_free(&s);
    return fold;
}

#endif
