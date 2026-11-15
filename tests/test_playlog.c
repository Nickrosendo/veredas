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
    CHECK(playlog_write(&p, path, err, sizeof(err)));
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
    CHECK(!playlog_write(&p, "build/no/such/folder/playlog.txt", err, sizeof(err)));
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
    CHECK(playlog_write(&p, path, err, sizeof(err)));
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

int main(void)
{
    test_time_by_mode();
    test_written_file();
    test_buttons_apart_from_keys();
    TEST_SUMMARY();
}
