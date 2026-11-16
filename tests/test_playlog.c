#include "test.h"

#include <stdio.h>  /* fopen, fgets, remove */
#include <string.h> /* strcmp, strstr */

#include "playlog.h"

/* Time goes to the mode the window was in; the first Sandbox moment is kept. */
static void test_time_by_mode(void)
{
    Playlog p;
    playlog_init(&p);
    playlog_frame(&p, 1000, false); /* the first frame starts the clock */
    playlog_frame(&p, 4000, false); /* 3 s watching */
    playlog_frame(&p, 5000, true);  /* 1 s more watching, then m */
    playlog_frame(&p, 7000, true);  /* 2 s in Sandbox */
    playlog_frame(&p, 6000, false); /* a clock that went back: no time */
    CHECK(p.ms_in[0] == 4000);
    CHECK(p.ms_in[1] == 2000);
    CHECK(p.first_sandbox_ms == 4000);
}

/* Keys by character, the rest as other; the file has one line each. */
static void test_written_file(void)
{
    Playlog p;
    playlog_init(&p);
    playlog_frame(&p, 0, false);
    playlog_key(&p, 'g');
    playlog_key(&p, 'g');
    playlog_key(&p, ' ');
    playlog_key(&p, 1000); /* an arrow */
    playlog_click(&p);
    playlog_frame(&p, 65000, false);
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
    playlog_frame(&p, 0, false);
    playlog_button(&p, 'g');
    playlog_button(&p, 'g');
    playlog_key(&p, 'g');
    playlog_button(&p, ' ');
    playlog_button(&p, 3020); /* a page's close button */
    playlog_frame(&p, 1000, false);
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
    playlog_frame(&p, 1000, false);
    playlog_verb(&p, PLAYLOG_VERB_WEATHER, 91000);
    playlog_verb(&p, PLAYLOG_VERB_WEATHER, 95000);
    playlog_verb(&p, PLAYLOG_VERBS, 2000);
    playlog_verb(&p, -1, 2000);
    playlog_frame(&p, 100000, true);
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
        head = head || (n == 1 && strcmp(line, "veredas-playlog 2\n") == 0);
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
    playlog_frame(&p, 0, false);
    playlog_frame(&p, 5000, false);
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

int main(void)
{
    test_verbs();
    test_no_verbs();
    test_time_by_mode();
    test_written_file();
    test_buttons_apart_from_keys();
    TEST_SUMMARY();
}
