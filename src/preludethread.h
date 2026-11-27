/*
 * preludethread: the ancestors' years (the prelude, app.h) run on a thread of
 * their own, so the "Growing a new world" screen keeps turning at 30 frames a
 * second and the years end as fast as the machine runs them (measured
 * 2026-11-25: on the window's thread a tick of about 38 ms landed on every
 * frame, 23 frames a second, and 8 years took about 2 minutes).
 *
 * The same two-thread rule as lookthread.h, with one difference: there the
 * worker steps a copy, here it steps the shown world itself. So while the
 * world is away (given to the worker and not yet taken back) the window's
 * thread (the main thread) must not read it at all, not even its tick. What
 * the screen needs instead is a glance: a block of bytes the worker fills
 * from the world after each tick, while the world is still its alone, and
 * then copies, under the lock, to where the main thread reads it. What a
 * glance holds is the caller's business (the window's: the counts on the
 * title line, the panel's numbers, the family strip), so this file knows
 * only its size and the function that fills it.
 *
 * Who owns the world is decided by away, which only the main thread changes,
 * and busy, under the lock: while away, the world (and whatever the glance
 * function writes to) is the worker's; once taken back, the main thread's
 * again. Taking it back waits for the step under way, at most one tick: the
 * only time the main thread waits.
 *
 * Without a thread (the browser build, or --frames runs, which must give the
 * same pictures every time) preludethread_give says no and the caller runs
 * the prelude on its own thread, as before (app_prelude_step / _finish).
 */
#ifndef VEREDAS_PRELUDETHREAD_H
#define VEREDAS_PRELUDETHREAD_H

#include <stdbool.h>
#include <stddef.h> /* size_t */
#include <stdint.h>

#include "sim.h"

/* As LOOKTHREAD_THREADS (lookthread.h): no threads in the browser page. */
#ifdef __EMSCRIPTEN__
#define PRELUDETHREAD_THREADS 0
#else
#define PRELUDETHREAD_THREADS 1
#include <pthread.h>
#endif

/* Fills out (glance_size bytes) with what the screen shows of sim. Runs on
 * the worker after each tick, and once on the main thread when the world is
 * given; either way on the thread that owns sim. ctx is passed through. It
 * may write to ctx's memory, which is then the worker's while the world is
 * away. */
typedef void (*PreludeGlanceFn)(void *ctx, const Sim *sim, void *out);

typedef struct {
    bool started; /* a worker thread is running */
    /* Set and cleared only by the main thread (give and take): it may read
     * it without the lock; the worker reads it under the lock. */
    bool away;
#if PRELUDETHREAD_THREADS
    pthread_t thread;
    pthread_mutex_t lock; /* guards the fields below and who owns *sim */
    pthread_cond_t wake;  /* the main thread to the worker: a world, or quit */
    pthread_cond_t idle;  /* the worker to the main thread: a step is done */
#endif
    /* Set by give, while the worker sleeps; read by the worker while away. */
    Sim *sim;
    uint64_t until;
    PreludeGlanceFn glance_fn;
    void *glance_ctx;
    size_t glance_size;
    unsigned char *glance_next; /* the worker's, filled after each tick */
    /* Under the lock: */
    unsigned char *glance; /* the last glance published */
    bool quit;     /* the worker should end */
    bool busy;     /* the worker is stepping *sim, without the lock */
    bool finished; /* *sim has reached until, or its world has died */
} PreludeThread;

/* Empty, no thread yet. glance_size bytes are kept twice for the glances
 * (allocated by start). */
void preludethread_init(PreludeThread *t, PreludeGlanceFn fn, void *ctx,
                        size_t glance_size);
/* Starts the worker thread. False when there are no threads (the browser
 * build), or a thread or the glances' memory could not be had: then give
 * always says no. */
bool preludethread_start(PreludeThread *t);
/* Gives sim to the worker, which runs ticks until sim->tick reaches until
 * or the world dies. Fills the first glance here, on the main thread. False,
 * with sim still the caller's, when there is no worker, the world is already
 * away, or there is nothing to run. After true the caller must not touch sim
 * (nor what the glance function writes to) until preludethread_take. */
bool preludethread_give(PreludeThread *t, Sim *sim, uint64_t until);
/* Copies the last published glance into out (glance_size bytes) and says
 * whether the worker has finished, so the world can be taken back at once.
 * Only while away. Never waits for a tick. */
bool preludethread_glance(PreludeThread *t, void *out);
/* Takes the world back: waits for the step under way (at most one tick) and
 * the worker runs no more. Does nothing when the world is not away. */
void preludethread_take(PreludeThread *t);
/* Waits until the worker has finished. For tests; the window never waits. */
void preludethread_wait(PreludeThread *t);
/* Takes the world back, ends the worker thread and frees the glances. Safe
 * without a thread and safe to call twice. */
void preludethread_stop(PreludeThread *t);

#endif
