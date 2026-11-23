#include "test.h"

#include <stdio.h>  /* fopen, fgets, remove */
#include <string.h> /* strcmp, strncmp, strstr */

#include "frame_log.h"

/* Off, nothing is written and nothing breaks. */
static void test_off_does_nothing(void)
{
    FrameLog f;
    frame_log_init(&f);
    frame_log_begin(&f, 1000);
    frame_log_part(&f, FRAME_PART_DRAW, 2000);
    FrameFacts facts = {0};
    frame_log_end(&f, 3000, &facts);
    CHECK(f.frame == 0);
    frame_log_close(&f);
}

/* Each part gets the time since the mark before it, a part cut twice adds
 * up, and the gap is measured from one frame's start to the next. */
static void test_two_frames(void)
{
    const char *path = "build/frame_log_test.txt";
    FrameLog f;
    char err[160];
    CHECK(frame_log_open(&f, path, err, sizeof(err)));
    frame_log_begin(&f, 1000000);                   /* frame 0 at 1 s */
    frame_log_part(&f, FRAME_PART_WORLD, 1040000);  /* a 40 ms tick */
    frame_log_part(&f, FRAME_PART_CAMERA, 1050000); /* 10 ms */
    frame_log_part(&f, FRAME_PART_WORLD, 1051500);  /* 1.5 ms more world */
    frame_log_part(&f, FRAME_PART_DRAW, 1061500);   /* 10 ms */
    FrameFacts facts = {.ticks = 1, .camera_ticks = 2, .drawn = true, .zoom = 1,
                        .tile_builds = 300, .sprite_builds = 4};
    frame_log_end(&f, 1062000, &facts);
    frame_log_begin(&f, 1100000); /* frame 1, 100 ms after frame 0 */
    FrameFacts still = {.prelude = true};
    frame_log_end(&f, 1100250, &still);
    frame_log_close(&f);

    FILE *in = fopen(path, "r");
    CHECK(in != NULL);
    char line[3][200] = {{0}};
    for (int i = 0; i < 3 && in != NULL; i++) {
        if (fgets(line[i], sizeof(line[i]), in) == NULL) {
            break;
        }
    }
    if (in != NULL) {
        fclose(in);
    }
    CHECK(strncmp(line[0], "frame gap_ms work_ms", 20) == 0);
    CHECK(strcmp(line[1], "0 0.00 62.00 41.50 10.00 0.00 10.00 0.00 1 2 0 1 0 1 300 4\n") == 0);
    CHECK(strcmp(line[2], "1 100.00 0.25 0.00 0.00 0.00 0.00 0.00 0 0 0 0 1 0 0 0\n") == 0);
    remove(path);
}

/* A file that cannot be made says why, and stays off. */
static void test_cannot_open(void)
{
    FrameLog f;
    char err[160];
    CHECK(!frame_log_open(&f, "build/no/such/folder/frames.txt", err, sizeof(err)));
    CHECK(strstr(err, "--frame-log") != NULL);
    CHECK(f.out == NULL);
}

int main(void)
{
    test_off_does_nothing();
    test_two_frames();
    test_cannot_open();
    TEST_SUMMARY();
}
