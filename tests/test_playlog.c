#include "test.h"

#include <stdio.h>  /* fopen, fgets, remove */
#include <string.h> /* strcmp, strstr */

#include "playlog.h"

/* A frame's view with only the mode set: the surface at Normal, nobody
 * followed, counted for the frame speed. Most tests here care about one
 * thing at a time. */
static PlaylogView mode(bool sandbox)
{
    return (PlaylogView){.sandbox = sandbox, .zoom = PLAYLOG_ZOOM_NORMAL,
                         .level = PLAYLOG_LEVEL_SURFACE, .timed = true};
}

/* One frame in that mode, for the tests from before format 3. */
static void frame_in(Playlog *p, uint64_t now_ms, bool sandbox)
{
    PlaylogView v = mode(sandbox);
    playlog_frame(p, now_ms, &v);
}

/* True when the file has this line, newline included. */
static bool file_has(const char *path, const char *want)
{
    FILE *f = fopen(path, "r");
    if (f == NULL) {
        return false;
    }
    char line[96];
    bool found = false;
    while (!found && fgets(line, sizeof(line), f) != NULL) {
        found = strcmp(line, want) == 0;
    }
    fclose(f);
    return found;
}

/* Time goes to the mode the window was in; the first Sandbox moment is kept. */
static void test_time_by_mode(void)
{
    Playlog p;
    playlog_init(&p);
    frame_in(&p, 1000, false); /* the first frame starts the clock */
    frame_in(&p, 4000, false); /* 3 s watching */
    frame_in(&p, 5000, true);  /* 1 s more watching, then m */
    frame_in(&p, 7000, true);  /* 2 s in Sandbox */
    frame_in(&p, 6000, false); /* a clock that went back: no time */
    CHECK(p.ms_in[0] == 4000);
    CHECK(p.ms_in[1] == 2000);
    CHECK(p.first_sandbox_ms == 4000);
}

/* Keys by character, the rest as other; the file has one line each. */
static void test_written_file(void)
{
    Playlog p;
    playlog_init(&p);
    frame_in(&p, 0, false);
    playlog_key(&p, 'g');
    playlog_key(&p, 'g');
    playlog_key(&p, ' ');
    playlog_key(&p, 1000); /* an arrow */
    playlog_click(&p);
    frame_in(&p, 65000, false);
    const char *path = "build/playlog_test.txt";
    char err[160];
    CHECK(playlog_write(&p, path, "9.9.9", err, sizeof(err)));
    FILE *f = fopen(path, "r");
    CHECK(f != NULL);
    char line[64];
    bool saw_g = false, saw_space = false, saw_other = false, saw_seconds = false;
    while (f != NULL && fgets(line, sizeof(line), f) != NULL) {
        saw_g = saw_g || strcmp(line, "key g 2\n") == 0;
        saw_space = saw_space || strcmp(line, "key space 1\n") == 0;
        saw_other = saw_other || strcmp(line, "key other 1\n") == 0;
        saw_seconds = saw_seconds || strcmp(line, "seconds 65\n") == 0;
    }
    if (f != NULL) {
        fclose(f);
    }
    CHECK(saw_g && saw_space && saw_other && saw_seconds);
    remove(path);
    CHECK(!playlog_write(&p, "build/no/such/folder/playlog.txt", "9.9.9", err,
                         sizeof(err)));
    CHECK(strstr(err, "cannot write the playlog") != NULL);
}

/* A click on a button is counted under its key's name, apart from the key
 * pressed on the keyboard; a button with no key of its own is "other". */
static void test_buttons_apart_from_keys(void)
{
    Playlog p;
    playlog_init(&p);
    frame_in(&p, 0, false);
    playlog_button(&p, 'g');
    playlog_button(&p, 'g');
    playlog_key(&p, 'g');
    playlog_button(&p, ' ');
    playlog_button(&p, 3020); /* a page's close button */
    frame_in(&p, 1000, false);
    const char *path = "build/playlog_buttons.txt";
    char err[160];
    CHECK(playlog_write(&p, path, "9.9.9", err, sizeof(err)));
    FILE *f = fopen(path, "r");
    CHECK(f != NULL);
    char line[64];
    bool button_g = false, key_g = false, button_space = false, button_other = false;
    while (f != NULL && fgets(line, sizeof(line), f) != NULL) {
        button_g = button_g || strcmp(line, "button g 2\n") == 0;
        key_g = key_g || strcmp(line, "key g 1\n") == 0;
        button_space = button_space || strcmp(line, "button space 1\n") == 0;
        button_other = button_other || strcmp(line, "button other 1\n") == 0;
    }
    if (f != NULL) {
        fclose(f);
    }
    CHECK(button_g);
    CHECK(key_g);
    CHECK(button_space);
    CHECK(button_other);
    remove(path);
}

/* Each verb's count and first moment; the earliest of them on its own line;
 * a verb before the first frame counts as at the start; a number outside the
 * table is ignored. */
static void test_verbs(void)
{
    Playlog p;
    playlog_init(&p);
    playlog_verb(&p, PLAYLOG_VERB_SPLIT, 500); /* before any frame */
    frame_in(&p, 1000, false);
    playlog_verb(&p, PLAYLOG_VERB_WEATHER, 91000);
    playlog_verb(&p, PLAYLOG_VERB_WEATHER, 95000);
    playlog_verb(&p, PLAYLOG_VERBS, 2000);
    playlog_verb(&p, -1, 2000);
    frame_in(&p, 100000, true);
    CHECK(p.verbs[PLAYLOG_VERB_WEATHER] == 2);
    CHECK(p.first_verb_ms[PLAYLOG_VERB_WEATHER] == 90000);
    CHECK(p.first_verb_ms[PLAYLOG_VERB_SPLIT] == 0);
    CHECK(p.first_verb_ms[PLAYLOG_VERB_TRAIT] == -1);
    const char *path = "build/playlog_verbs.txt";
    char err[160];
    CHECK(playlog_write(&p, path, "9.9.9", err, sizeof(err)));
    FILE *f = fopen(path, "r");
    CHECK(f != NULL);
    char line[64];
    int n = 0;
    bool head = false, version = false, first = false, weather = false,
         trait = false, skip = false;
    while (f != NULL && fgets(line, sizeof(line), f) != NULL) {
        n++;
        head = head || (n == 1 && strcmp(line, "veredas-playlog 3\n") == 0);
        version = version || (n == 2 && strcmp(line, "version 9.9.9\n") == 0);
        first = first || strcmp(line, "first_verb_second 0\n") == 0;
        weather = weather || strcmp(line, "verb weather 2 90\n") == 0;
        trait = trait || strcmp(line, "verb trait 0 -1\n") == 0;
        skip = skip || strcmp(line, "verb skip 0 -1\n") == 0;
    }
    if (f != NULL) {
        fclose(f);
    }
    CHECK(head);
    CHECK(version);
    CHECK(first);
    CHECK(weather);
    CHECK(trait);
    CHECK(skip);
    remove(path);
}

/* No verb at all: the earliest is -1, as for Sandbox never opened. */
static void test_no_verbs(void)
{
    Playlog p;
    playlog_init(&p);
    frame_in(&p, 0, false);
    frame_in(&p, 5000, false);
    const char *path = "build/playlog_noverbs.txt";
    char err[160];
    CHECK(playlog_write(&p, path, "9.9.9", err, sizeof(err)));
    FILE *f = fopen(path, "r");
    CHECK(f != NULL);
    char line[64];
    bool never = false;
    while (f != NULL && fgets(line, sizeof(line), f) != NULL) {
        never = never || strcmp(line, "first_verb_second -1\n") == 0;
    }
    if (f != NULL) {
        fclose(f);
    }
    CHECK(never);
    remove(path);
}

/* Time at each zoom and level goes to the one the view showed since the last
 * frame; the zoom seconds and the level seconds each add up to "seconds",
 * whatever the rounding. */
static void test_zoom_and_level_time(void)
{
    Playlog p;
    playlog_init(&p);
    PlaylogView v = mode(false);
    playlog_frame(&p, 0, &v);
    v.zoom = PLAYLOG_ZOOM_FAR; /* 10.4 s at Normal on the surface, then Far */
    playlog_frame(&p, 10400, &v);
    v.level = PLAYLOG_LEVEL_UNDERWATER; /* 5.3 s at Far, then underwater */
    playlog_frame(&p, 15700, &v);
    v.zoom = PLAYLOG_ZOOM_CLOSE;
    v.level = PLAYLOG_LEVEL_SKY; /* 2.2 s underwater at Far */
    playlog_frame(&p, 17900, &v);
    v.zoom = PLAYLOG_ZOOM_WORLD;
    playlog_frame(&p, 20500, &v); /* 2.6 s in the sky at Close */
    playlog_frame(&p, 21900, &v); /* 1.4 s in the sky at World */
    CHECK(p.ms_zoom[PLAYLOG_ZOOM_NORMAL] == 10400);
    CHECK(p.ms_zoom[PLAYLOG_ZOOM_FAR] == 7500);
    CHECK(p.ms_zoom[PLAYLOG_ZOOM_CLOSE] == 2600);
    CHECK(p.ms_zoom[PLAYLOG_ZOOM_WORLD] == 1400);
    CHECK(p.ms_level[PLAYLOG_LEVEL_SURFACE] == 15700);
    CHECK(p.ms_level[PLAYLOG_LEVEL_UNDERWATER] == 2200);
    CHECK(p.ms_level[PLAYLOG_LEVEL_SKY] == 4000);
    CHECK(p.first_level_ms[PLAYLOG_LEVEL_UNDERWATER] == 15700);
    CHECK(p.first_level_ms[PLAYLOG_LEVEL_SKY] == 17900);
    const char *path = "build/playlog_zoom.txt";
    char err[160];
    CHECK(playlog_write(&p, path, "9.9.9", err, sizeof(err)));
    /* 21.9 s in all: 21 whole seconds, and each group adds up to 21. */
    CHECK(file_has(path, "seconds 21\n"));
    /* 10.4 + 7.5 + 2.6 + 1.4 cut to whole seconds one by one is only 20:
     * the second left over goes to the one with the largest part cut off. */
    CHECK(file_has(path, "seconds_zoom close 3\n"));
    CHECK(file_has(path, "seconds_zoom normal 10\n"));
    CHECK(file_has(path, "seconds_zoom far 7\n"));
    CHECK(file_has(path, "seconds_zoom world 1\n"));
    CHECK(file_has(path, "seconds_level sky 4\n"));
    CHECK(file_has(path, "seconds_level surface 15\n"));
    CHECK(file_has(path, "seconds_level underwater 2\n"));
    CHECK(file_has(path, "first_underwater_second 15\n"));
    CHECK(file_has(path, "first_sky_second 17\n"));
    /* The levels not built yet have no line. */
    CHECK(!file_has(path, "seconds_level high_sky 0\n"));
    remove(path);
}

/* Follows the player starts are counted with the first one's moment; the
 * time on a followed animal counts whoever started the follow. */
static void test_following(void)
{
    Playlog p;
    playlog_init(&p);
    playlog_follow(&p, 0); /* before any frame: at the start */
    CHECK(p.follows == 1);
    CHECK(p.first_follow_ms == 0);
    playlog_init(&p);
    PlaylogView v = mode(false);
    playlog_frame(&p, 1000, &v);
    v.following = true; /* the camera's own pick: time, but no follow */
    playlog_frame(&p, 4000, &v);
    playlog_follow(&p, 9000); /* a click on an animal, 8 s in */
    playlog_frame(&p, 9000, &v);
    playlog_follow(&p, 12000);
    v.following = false;
    playlog_frame(&p, 12500, &v);
    playlog_frame(&p, 20000, &v);
    CHECK(p.follows == 2);
    CHECK(p.first_follow_ms == 8000);
    CHECK(p.ms_following == 8500);
    const char *path = "build/playlog_follow.txt";
    char err[160];
    CHECK(playlog_write(&p, path, "9.9.9", err, sizeof(err)));
    CHECK(file_has(path, "follows 2\n"));
    CHECK(file_has(path, "first_follow_second 8\n"));
    CHECK(file_has(path, "seconds_following 8\n"));
    remove(path);
}

/* The frame speed: the mean gap between frames and the gaps over 40 ms a
 * second, counted only over frames after the prelude and not paused. */
static void test_frame_speed(void)
{
    Playlog p;
    playlog_init(&p);
    PlaylogView v = mode(false);
    v.timed = false; /* the prelude: its slow frames do not count */
    playlog_frame(&p, 0, &v);
    playlog_frame(&p, 500, &v);
    v.timed = true;
    playlog_frame(&p, 1000, &v); /* the gap before this was the prelude's */
    /* 30 frames of 33 ms, 3 of 60 ms, and one more of 33 ms below. */
    uint64_t t = 1000;
    for (int i = 0; i < 30; i++) {
        t += 33;
        playlog_frame(&p, t, &v);
    }
    for (int i = 0; i < 3; i++) {
        t += 60;
        playlog_frame(&p, t, &v);
    }
    /* Paused at the next frame: the gap before it still ran unpaused, the
     * slow ones after it do not count. */
    v.timed = false;
    playlog_frame(&p, t + 33, &v);
    playlog_frame(&p, t + 233, &v);
    playlog_frame(&p, t + 433, &v);
    CHECK(p.timed_frames == 34);
    CHECK(p.timed_ms == 1203);
    CHECK(p.late_frames == 3);
    playlog_machine(&p, 1280, 720, 8);
    const char *path = "build/playlog_speed.txt";
    char err[160];
    CHECK(playlog_write(&p, path, "9.9.9", err, sizeof(err)));
    CHECK(file_has(path, "frame_ms_mean 35.4\n"));          /* 1203 / 34 */
    CHECK(file_has(path, "late_frames_per_second 2.5\n")); /* 3 / 1.203 s */
    CHECK(file_has(path, "window 1280 720\n"));
    CHECK(file_has(path, "cpu_cores 8\n"));
    remove(path);
}

/* Nothing of it happened: the first moments say -1, the speed 0, and every
 * new line is still there for the reader. */
static void test_format3_never(void)
{
    Playlog p;
    playlog_init(&p);
    PlaylogView v = mode(false);
    v.timed = false;
    playlog_frame(&p, 0, &v);
    playlog_frame(&p, 3000, &v);
    const char *path = "build/playlog_never.txt";
    char err[160];
    CHECK(playlog_write(&p, path, "9.9.9", err, sizeof(err)));
    CHECK(file_has(path, "veredas-playlog 3\n"));
    CHECK(file_has(path, "first_underwater_second -1\n"));
    CHECK(file_has(path, "first_sky_second -1\n"));
    CHECK(file_has(path, "first_follow_second -1\n"));
    CHECK(file_has(path, "follows 0\n"));
    CHECK(file_has(path, "seconds_following 0\n"));
    CHECK(file_has(path, "seconds_zoom normal 3\n"));
    CHECK(file_has(path, "seconds_level surface 3\n"));
    CHECK(file_has(path, "frame_ms_mean 0.0\n"));
    CHECK(file_has(path, "late_frames_per_second 0.0\n"));
    CHECK(file_has(path, "window 0 0\n"));
    CHECK(file_has(path, "cpu_cores 0\n"));
    /* A format 2 line still in its place. */
    CHECK(file_has(path, "first_sandbox_second -1\n"));
    remove(path);
}

/* A zoom or level from outside the tables goes nowhere rather than past an
 * array's end. */
static void test_view_out_of_range(void)
{
    Playlog p;
    playlog_init(&p);
    PlaylogView v = mode(false);
    v.zoom = 99;
    v.level = -3;
    playlog_frame(&p, 0, &v);
    playlog_frame(&p, 1000, &v);
    uint64_t zoom_sum = 0, level_sum = 0;
    for (int z = 0; z < PLAYLOG_ZOOMS; z++) {
        zoom_sum += p.ms_zoom[z];
    }
    for (int l = 0; l < PLAYLOG_LEVELS; l++) {
        level_sum += p.ms_level[l];
    }
    CHECK(zoom_sum == 0);
    CHECK(level_sum == 0);
}

int main(void)
{
    test_verbs();
    test_no_verbs();
    test_time_by_mode();
    test_written_file();
    test_buttons_apart_from_keys();
    test_zoom_and_level_time();
    test_following();
    test_frame_speed();
    test_format3_never();
    test_view_out_of_range();
    TEST_SUMMARY();
}
