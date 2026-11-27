/*
 * The cheaper tick study's check on a full-size world (2026-11-25): see
 * same_world.h. Too slow for every "make test" with the memory checkers on,
 * so "make test-slow" runs it; test_same_world.c does a small world every time.
 */
#include "test.h"

#include "same_world.h"

/* Seed 4 at 256 by 144, the window's size, grown 400 ticks (1,147 animals,
 * 277 flyers and 374 at sea when the compared ticks begin), then 300 ticks
 * compared (1,486 animals at the end): the number the code before the study
 * made (commit 5f8f6d3). Re-pinned for rules 7 (health grows with the body,
 * 2026-11-27): another world, 1,106 animals growing to 1,466. */
#define SEED_4_256X144 0x064bba195c50aff5ULL

static void test_full_world_same_as_before(void)
{
    int before = 0, after = 0;
    uint64_t fold = same_world_fold(4, 256, 144, 400, 300, &before, &after);
    CHECK(before > 1000 && after > 1000);
    printf("seed 4 at 256x144, ticks 401-700: 0x%016llx (%d -> %d animals)\n",
           (unsigned long long)fold, before, after);
    if (same_world_maths_as_pinned()) {
        CHECK(fold == SEED_4_256X144);
    } else {
        printf("  (another maths library: not compared with the pinned value)\n");
    }
}

int main(void)
{
    test_full_world_same_as_before();
    TEST_SUMMARY();
}
