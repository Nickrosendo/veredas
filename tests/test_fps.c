#include "test.h"

#include <string.h> /* strcmp */

#include "fps.h"

static bool text_is(const FpsMeter *m, const char *expected)
{
    char out[32];
    fps_text(m, out, sizeof(out));
    if (strcmp(out, expected) != 0) {
        printf("got '%s', expected '%s'\n", out, expected);
        return false;
    }
    return true;
}

/* Nothing to show before the first half second ends. */
static void test_empty_until_half_a_second(void)
{
    FpsMeter m;
    fps_reset(&m);
    CHECK(text_is(&m, "-- fps"));
    for (uint64_t t = 1000; t < 1000 + FPS_WINDOW_MS; t += 33) {
        fps_frame(&m, t, 2);
    }
    CHECK(!m.has_value);
    CHECK(text_is(&m, "-- fps"));
}

/* A steady 30 frames a second (the window's fixed 33 ms clock, as in a
 * --frames run) reads as 30, with the work averaged. */
static void test_steady_thirty(void)
{
    FpsMeter m;
    fps_reset(&m);
    for (int i = 0; i <= 60; i++) {
        fps_frame(&m, 5000u + (uint64_t)i * 33u, i % 2 == 0 ? 3u : 5u);
    }
    CHECK(m.has_value);
    CHECK(m.per_second > 29.5f && m.per_second < 30.5f);
    CHECK(m.work_per_frame > 3.5f && m.work_per_frame < 4.5f);
    CHECK(text_is(&m, "30 fps, 4.0 ms"));
}

/* The number holds between averages: frames inside a half second do not move
 * it, which is what keeps it from flickering. */
static void test_holds_between_averages(void)
{
    FpsMeter m;
    fps_reset(&m);
    fps_frame(&m, 0, 0);
    for (uint64_t t = 100; t <= 500; t += 100) {
        fps_frame(&m, t, 1); /* 5 frames in 500 ms: 10 a second */
    }
    CHECK(m.has_value);
    float first = m.per_second;
    CHECK(first > 9.9f && first < 10.1f);
    fps_frame(&m, 510, 1); /* much faster, but the half second is not over */
    fps_frame(&m, 520, 1);
    CHECK(m.per_second == first);
}

/* A clock that goes backwards, or stands still, never divides by zero or
 * shows a nonsense number. */
static void test_clock_going_backwards(void)
{
    FpsMeter m;
    fps_reset(&m);
    fps_frame(&m, 1000, 0);
    fps_frame(&m, 900, 0);
    fps_frame(&m, 1000, 0);
    CHECK(!m.has_value);
}

int main(void)
{
    test_empty_until_half_a_second();
    test_steady_thirty();
    test_holds_between_averages();
    test_clock_going_backwards();
    TEST_SUMMARY();
}
