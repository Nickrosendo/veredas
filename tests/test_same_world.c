/*
 * The world is the same, tick for tick, after the cheaper tick study's
 * speed-ups (2026-11-25) as before them: see same_world.h. This one is small
 * enough for every "make test"; slow_same_world.c does a full-size world of
 * well over a thousand animals in "make test-slow".
 */
#include "test.h"

#include "same_world.h"

/* Seed 4 at 96 by 54, grown 600 ticks (106 animals, a sixth of them flyers
 * and half at sea), then 300 ticks compared (148 animals at the end): the
 * number the code before the study made (commit 5f8f6d3), on gcc and clang.
 * Re-pinned for rules 7 (health grows with the body, 2026-11-27): another
 * world, 108 animals growing to 147. */
#define SEED_4_96X54 0xb75a4d76e7966769ULL

static void test_small_world_same_as_before(void)
{
    int before = 0, after = 0;
    uint64_t fold = same_world_fold(4, 96, 54, 600, 300, &before, &after);
    CHECK(before > 0 && after > 0);
    printf("seed 4 at 96x54, ticks 601-900: 0x%016llx (%d -> %d animals)\n",
           (unsigned long long)fold, before, after);
    if (same_world_maths_as_pinned()) {
        CHECK(fold == SEED_4_96X54);
    } else {
        printf("  (another maths library: not compared with the pinned value)\n");
    }
}

int main(void)
{
    test_small_world_same_as_before();
    TEST_SUMMARY();
}
