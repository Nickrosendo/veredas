/*
 * rounding: a float rounded to the nearest whole number without calling the
 * maths library, for the creatures' views (creature.c's Fan). In a header,
 * as static inline, so the view code and tests/slow_rounding.c compile the
 * very same lines: the slow test checks it against lroundf for every float
 * it is used on.
 */
#ifndef VEREDAS_ROUNDING_H
#define VEREDAS_ROUNDING_H

/* Below this size round_half_away is lroundf, checked float by float
 * (tests/slow_rounding.c). */
#define ROUND_HALF_AWAY_LIMIT 1000000.0f

/* v rounded to the nearest whole number, halves away from zero, for |v| below
 * ROUND_HALF_AWAY_LIMIT: the same int as lroundf, checked for every one of the
 * 2.46 billion floats in that range (the cheaper tick study, 2026-11-25), now
 * kept as tests/slow_rounding.c (the smooth window study, 2026-11-27), without
 * the call. (int)v cuts the fraction off, towards zero; v - (float)t is then
 * exactly the fraction, since taking a float's whole part off only clears
 * bits; a comparison is an int, 1 or 0, so the last line adds or takes one
 * without a branch. */
static inline int round_half_away(float v)
{
    int t = (int)v;
    float rest = v - (float)t;
    return t + (rest >= 0.5f) - (rest <= -0.5f);
}

#endif
