#include "test.h"

#include <string.h> /* memcmp, memcpy */

#include "canvas.h"
#include "weather.h"
#include "weather_fx.h"

/* A canvas larger than the view, so the border round the view can be checked. */
#define CW 300
#define CH 200
#define VX 40
#define VY 30
#define VW 200
#define VH 120

/* Every pixel of a canvas, kept to compare with later (static: 240 KB). */
static uint32_t before[CW * CH];

static void grey_canvas(Canvas *c)
{
    canvas_fill(c, canvas_rgb(90, 110, 70));
}

static int pixels_changed(const Canvas *c)
{
    int n = 0;
    for (int i = 0; i < CW * CH; i++) {
        n += c->px[i] != before[i];
    }
    return n;
}

static void test_nothing_drawn_at_zero_or_none(void)
{
    Canvas c;
    CHECK(canvas_init(&c, CW, CH));
    grey_canvas(&c);
    memcpy(before, c.px, sizeof(before));
    weather_fx_draw(&c, VX, VY, VW, VH, WEATHER_HARD_WINTER, 0.0f, 5000);
    CHECK(pixels_changed(&c) == 0);
    weather_fx_draw(&c, VX, VY, VW, VH, WEATHER_NONE, 1.0f, 5000);
    CHECK(pixels_changed(&c) == 0);
    weather_fx_draw(&c, VX, VY, VW, VH, WEATHER_COUNT, 1.0f, 5000);
    CHECK(pixels_changed(&c) == 0);
    canvas_free(&c);
}

/* Every kind changes the view, and nothing outside it. */
static void test_only_inside_the_view(void)
{
    Canvas c;
    CHECK(canvas_init(&c, CW, CH));
    for (int kind = WEATHER_DROUGHT; kind < WEATHER_COUNT; kind++) {
        grey_canvas(&c);
        memcpy(before, c.px, sizeof(before));
        weather_fx_draw(&c, VX, VY, VW, VH, kind, 1.0f, 12345);
        int inside = 0, outside = 0;
        for (int y = 0; y < CH; y++) {
            for (int x = 0; x < CW; x++) {
                bool in = x >= VX && x < VX + VW && y >= VY && y < VY + VH;
                bool changed = c.px[y * CW + x] != before[y * CW + x];
                inside += in && changed;
                outside += !in && changed;
            }
        }
        CHECK(inside > VW * VH / 2); /* the wash reaches most of the view */
        CHECK(outside == 0);
    }
    canvas_free(&c);
}

/* A view hanging off the canvas's edges is cut, never written past them (the
 * sanitizer in the test build would catch a write outside the block). */
static void test_view_off_the_canvas(void)
{
    Canvas c;
    CHECK(canvas_init(&c, CW, CH));
    grey_canvas(&c);
    weather_fx_draw(&c, -50, -40, CW + 100, CH + 80, WEATHER_HARD_WINTER, 1.0f, 777);
    weather_fx_draw(&c, CW - 10, CH - 10, 400, 400, WEATHER_DROUGHT, 1.0f, 777);
    weather_fx_draw(&c, 0, 0, 0, 0, WEATHER_WARM_YEAR, 1.0f, 777);
    canvas_free(&c);
}

/* The same ms gives the same pixels; a later one moves the specks. */
static void test_same_ms_same_pixels(void)
{
    Canvas a, b;
    CHECK(canvas_init(&a, CW, CH));
    CHECK(canvas_init(&b, CW, CH));
    for (int kind = WEATHER_DROUGHT; kind < WEATHER_COUNT; kind++) {
        grey_canvas(&a);
        grey_canvas(&b);
        weather_fx_draw(&a, VX, VY, VW, VH, kind, 1.0f, 40000);
        weather_fx_draw(&b, VX, VY, VW, VH, kind, 1.0f, 40000);
        CHECK(memcmp(a.px, b.px, sizeof(uint32_t) * CW * CH) == 0);
        grey_canvas(&b);
        weather_fx_draw(&b, VX, VY, VW, VH, kind, 1.0f, 41000);
        CHECK(memcmp(a.px, b.px, sizeof(uint32_t) * CW * CH) != 0);
    }
    canvas_free(&a);
    canvas_free(&b);
}

/* Half strength draws fewer specks and a fainter wash than full strength. */
static void test_strength_scales(void)
{
    Canvas c;
    CHECK(canvas_init(&c, CW, CH));
    grey_canvas(&c);
    memcpy(before, c.px, sizeof(before));
    weather_fx_draw(&c, VX, VY, VW, VH, WEATHER_HARD_WINTER, 0.5f, 9000);
    uint32_t half = c.px[(VY + 1) * CW + VX + 1];
    grey_canvas(&c);
    weather_fx_draw(&c, VX, VY, VW, VH, WEATHER_HARD_WINTER, 1.0f, 9000);
    uint32_t full = c.px[(VY + 1) * CW + VX + 1];
    /* Toward blue: the blue byte rises with strength (unless a flake sat
     * there, which would only make it whiter still). */
    CHECK((half & 0xFFu) > (before[(VY + 1) * CW + VX + 1] & 0xFFu));
    CHECK((full & 0xFFu) >= (half & 0xFFu));
    canvas_free(&c);
}

/*
 * The wash mixes two pixels per 64-bit multiply; a wrong mask would leak one
 * colour byte into the next. A view of odd width, too small for any speck,
 * covers both paths: pairs through the 64-bit mix, the last pixel of each row
 * alone. Each pixel must come out as a 1 by 1 view of its colour does, which
 * always takes the one-pixel path.
 */
static void test_wash_pairs_match_single_pixels(void)
{
    Canvas c, one;
    CHECK(canvas_init(&c, 41, 39)); /* 1599 pixels: below one speck's 2000 */
    CHECK(canvas_init(&one, 1, 1));
    uint32_t seed = 12345u;
    for (int i = 0; i < 41 * 39; i++) {
        seed = seed * 1664525u + 1013904223u; /* a quick pseudo-random step */
        c.px[i] = 0xFF000000u | (seed >> 8);
    }
    /* Corners and the extremes, where a carry would show first. */
    c.px[0] = 0xFFFFFFFFu;
    c.px[1] = 0xFF000000u;
    c.px[2] = 0xFFFF00FFu;
    c.px[3] = 0xFF00FF00u;
    memcpy(before, c.px, sizeof(uint32_t) * 41 * 39);
    for (int kind = WEATHER_DROUGHT; kind < WEATHER_COUNT; kind++) {
        memcpy(c.px, before, sizeof(uint32_t) * 41 * 39);
        weather_fx_draw(&c, 0, 0, 41, 39, kind, 0.8f, 0);
        int wrong = 0;
        for (int i = 0; i < 41 * 39; i++) {
            one.px[0] = before[i];
            weather_fx_draw(&one, 0, 0, 1, 1, kind, 0.8f, 0);
            wrong += c.px[i] != one.px[0];
        }
        CHECK(wrong == 0);
    }
    canvas_free(&c);
    canvas_free(&one);
}

static void test_fade(void)
{
    float s = 0.0f;
    weather_fx_fade(&s, true, 750);
    CHECK(s > 0.499f && s < 0.501f);
    weather_fx_fade(&s, true, 10000);
    CHECK(s == 1.0f);
    weather_fx_fade(&s, false, 750);
    CHECK(s > 0.499f && s < 0.501f);
    weather_fx_fade(&s, false, 100000);
    CHECK(s == 0.0f);
    weather_fx_fade(&s, false, 0);
    CHECK(s == 0.0f);
    /* A strength outside 0..1 (never set, say) comes back inside. */
    s = 7.0f;
    weather_fx_fade(&s, true, 0);
    CHECK(s == 1.0f);
}

int main(void)
{
    test_nothing_drawn_at_zero_or_none();
    test_only_inside_the_view();
    test_view_off_the_canvas();
    test_same_ms_same_pixels();
    test_strength_scales();
    test_wash_pairs_match_single_pixels();
    test_fade();
    TEST_SUMMARY();
}
