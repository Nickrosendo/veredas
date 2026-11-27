/*
 * preludethread: a new world's first years (the prelude, app.h) run on a
 * thread of their own, so the window opens at once and its "Growing a new
 * world" screen animates smoothly while they run (the stutter measured
 * 2026-11-23 and 24: the prelude ran in 28 ms slices on the window's thread,
 * and each slice ended a tick late, about 37 to 40 ms, so the opening seconds
 * dropped frames).
 *
 * The worker has a world of its own. When the run starts, the shown world is
 * copied (sim_copy, through the save format, as the camera's look-ahead and
 * the branches are), and only that copy is stepped. The window's thread (the
 * main thread) keeps its own world, which it may read and draw as it likes.
 * About once a second the worker also makes a copy of its world and hands it
 * over (preludethread_take_copy), so the panel and the title move with the
 * run instead of standing at year 0. When the run is over, or stopped, the
 * main thread waits for the worker to end and takes its world: a struct
 * assignment, as the branches swap worlds, so no copy is made at the end.
 *
 * A copy at tick 0 grows into the same world as the original would: the
 * fingerprint at the end is the same as when the prelude runs on the main
 * thread (tests/test_preludethread.c checks it).
 *
 * Who may touch what: world belongs to the worker while it runs, and to the
 * main thread once finished is true. Everything under the lock is shared.
 * The main thread never waits for a tick, except when it stops the run
 * (preludethread_end): then at most one.
 *
 * Without threads (the browser build) preludethread_start says false and the
 * window runs the prelude in slices on its own thread, as before.
 */
#ifndef VEREDAS_PRELUDETHREAD_H
#define VEREDAS_PRELUDETHREAD_H

#include <stdbool.h>
#include <stdint.h>

#include "app.h" /* Prelude */
#include "sim.h"

/* As in lookthread.h: no threads in the browser build. */
#ifdef __EMSCRIPTEN__
#define PRELUDETHREAD_THREADS 0
#else
#define PRELUDETHREAD_THREADS 1
#include <pthread.h>
#endif

/* How often the worker hands the main thread a copy of its world to show, in
 * milliseconds. A copy of a grown world takes about 30 ms of the worker's
 * time, so once a second costs the run about 3%. */
#define PRELUDETHREAD_COPY_MS 1000

typedef struct {
    Sim world;       /* the worker's world while it runs (see above) */
    Prelude prelude; /* the worker's own record of the run */
    uint64_t (*now_ms)(void); /* the clock for the copies' spacing */
    bool started;    /* a worker thread is running, or has finished and not been joined */
#if PRELUDETHREAD_THREADS
    pthread_t thread;
    pthread_mutex_t lock; /* guards the fields below */
#endif
    /* Under the lock: */
    bool stop;       /* the main thread asks the worker to end after its tick */
    bool finished;   /* the worker has ended: world is the main thread's now */
    uint64_t tick;   /* world.tick as the worker last left it */
    bool has_copy;   /* copy holds a world the main thread has not taken yet */
    Sim copy;
} PreludeThread;

/* Empty, no thread. */
void preludethread_init(PreludeThread *t);
/* Copies shown and starts a worker that runs prelude p on the copy. False,
 * with nothing started, when there are no threads or the copy or the thread
 * could not be made: the caller then runs the prelude itself. now_ms is the
 * clock that spaces the copies the worker hands over. */
bool preludethread_start(PreludeThread *t, const Sim *shown, const Prelude *p,
                         uint64_t (*now_ms)(void));
/* How far the run has got (the worker's tick) and whether it is over. */
uint64_t preludethread_progress(PreludeThread *t, bool *finished);
/* When the worker has handed over a newer copy of its world, it replaces
 * *shown (whose memory is freed) and true is returned; else nothing changes. */
bool preludethread_take_copy(PreludeThread *t, Sim *shown);
/* Ends the run: asks the worker to stop after its tick (unless it has
 * finished already), waits for it, and puts its world in *shown, whose
 * memory is freed. p becomes the worker's record of the run: active stays
 * true only when the run was cut short with years still to go. Safe to call
 * when nothing was started: then nothing changes. */
void preludethread_end(PreludeThread *t, Sim *shown, Prelude *p);

#endif
