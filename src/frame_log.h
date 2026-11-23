/*
 * frame_log: where each frame of the window spends its time, one line a
 * frame, for finding stutter (--frame-log PATH). A smooth window needs every
 * frame done inside its 33 ms; an average hides the few that are not, so this
 * keeps every frame. Nothing about the world is in it, only times and counts.
 *
 * A frame is cut into parts by marks: frame_log_begin at its start, then
 * frame_log_part each time one part ends, then frame_log_end. Times are in
 * microseconds (millionths of a second) from the caller's clock; the file
 * shows milliseconds with two decimals.
 */
#ifndef VEREDAS_FRAME_LOG_H
#define VEREDAS_FRAME_LOG_H

#include <stdbool.h>
#include <stddef.h> /* size_t */
#include <stdint.h>
#include <stdio.h> /* FILE */

/* The parts of a frame, in the order the window runs them. */
enum {
    FRAME_PART_WORLD,   /* events, then the shown world's ticks (or the prelude's) */
    FRAME_PART_CAMERA,  /* the camera's look-ahead copy: made, run on, a pick */
    FRAME_PART_OTHER,   /* the other world when split, notices, clocks */
    FRAME_PART_DRAW,    /* the picture, into the canvas */
    FRAME_PART_PRESENT, /* the canvas to the screen */
    FRAME_PARTS
};

/* What happened in a frame besides its times. */
typedef struct {
    int ticks;          /* ticks the shown world ran */
    int camera_ticks;   /* ticks the look-ahead copy ran */
    bool camera_copied; /* the look-ahead copy was made afresh */
    bool drawn;         /* a picture was drawn (a paused window skips some) */
    bool prelude;       /* the world's first years were running */
    int zoom;           /* the camera's zoom step (camera.h) */
    /* Pictures the drawing had to make, not find in its caches: ground tiles
     * (tiles.h) and animal pictures (sprite.h). Many after a zoom. */
    long tile_builds, sprite_builds;
} FrameFacts;

typedef struct {
    FILE *out;            /* NULL while off: every call then does nothing */
    long frame;           /* frames written so far */
    uint64_t last_start;  /* the last frame's start; 0 before the first */
    uint64_t start, mark; /* this frame's start, and where its last part ended */
    uint64_t part_us[FRAME_PARTS];
} FrameLog;

/* Off: nothing is written. */
void frame_log_init(FrameLog *f);
/* Starts writing to path, a header line first. false, with the reason in
 * err, when the file cannot be opened. */
bool frame_log_open(FrameLog *f, const char *path, char *err, size_t len);
void frame_log_begin(FrameLog *f, uint64_t now_us);
/* The time since the last mark goes to part. */
void frame_log_part(FrameLog *f, int part, uint64_t now_us);
/* Writes the frame's line. */
void frame_log_end(FrameLog *f, uint64_t now_us, const FrameFacts *facts);
/* Closes the file, if open. */
void frame_log_close(FrameLog *f);

#endif
