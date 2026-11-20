/*
 * fps: the frames-a-second counter (F3 in the window). It is told when each
 * frame is shown and how long the frame's work took, and gives back averages
 * over the last half second or so, so the number on screen changes twice a
 * second instead of flickering every frame. It reads no clock itself: the
 * caller passes the time, so tests can pass any times they like.
 */
#ifndef VEREDAS_FPS_H
#define VEREDAS_FPS_H

#include <stdbool.h>
#include <stddef.h> /* size_t */
#include <stdint.h>

/* How long each average covers, in milliseconds. */
#define FPS_WINDOW_MS 500u

typedef struct {
    uint64_t window_start; /* when the current half second began */
    bool started;          /* window_start holds a real time */
    int frames;            /* frames shown since window_start */
    uint64_t work_ms;      /* their work, added up */
    /* The last finished half second's averages; has_value is false until
     * the first one ends. */
    bool has_value;
    float per_second;
    float work_per_frame; /* milliseconds */
} FpsMeter;

/* An empty meter: shows nothing until half a second of frames has passed. */
void fps_reset(FpsMeter *m);
/* One frame shown at now_ms (any clock that counts milliseconds), after
 * work_ms of work. */
void fps_frame(FpsMeter *m, uint64_t now_ms, uint64_t work_ms);
/* "30 fps, 4.2 ms" into out; "-- fps" before the first average. */
void fps_text(const FpsMeter *m, char *out, size_t size);

#endif
