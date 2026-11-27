/*
 * round_half_away (rounding.h) is lroundf for every float below
 * ROUND_HALF_AWAY_LIMIT in size: all 2.46 billion of them, walked by their
 * bit patterns, both signs. The cheaper tick study (2026-11-25) ran this check
 * once and recorded its answer; the smooth window design (2026-11-27,
 * section 5.1) asked for it to be kept, and a run takes too long for every
 * "make test", so it is in "make test-slow".
 */
#include "test.h"

#include <math.h>   /* lroundf */
#include <stdint.h>
#include <string.h> /* memcpy */

#include "rounding.h"

/* The float whose bits are u. memcpy, not a pointer cast: reading a uint32_t
 * through a float pointer breaks C's aliasing rule; the compiler turns the
 * copy into one move. */
static float float_of(uint32_t u)
{
    float f;
    memcpy(&f, &u, sizeof f);
    return f;
}

static void test_same_as_lroundf_below_the_limit(void)
{
    /* Positive floats in order of size are their bit patterns in order, from
     * 0 (zero) up; the sign is the top bit. */
    uint32_t top = 0;
    memcpy(&top, &(float){ROUND_HALF_AWAY_LIMIT}, sizeof top);
    uint64_t checked = 0, differ = 0;
    for (uint32_t u = 0; u < top; u++) {
        float v = float_of(u);
        float w = -v;
        differ += round_half_away(v) != (int)lroundf(v);
        differ += round_half_away(w) != (int)lroundf(w);
        checked += 2;
    }
    printf("%llu floats with |v| < %.0f checked, %llu differ\n",
           (unsigned long long)checked, (double)ROUND_HALF_AWAY_LIMIT,
           (unsigned long long)differ);
    CHECK(checked > 2000000000ULL);
    CHECK(differ == 0);
}

int main(void)
{
    test_same_as_lroundf_below_the_limit();
    TEST_SUMMARY();
}
