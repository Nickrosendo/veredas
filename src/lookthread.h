/*
 * lookthread: the Documentary camera's look-ahead copy (lookahead.h) stepped
 * on a thread of its own, so its ticks no longer land on the frames that tick
 * the shown world (the stutter measured 2026-11-23: a tick frame carried both
 * worlds' ticks, about 80 ms, and the first 150 frames after a world opened
 * carried one look-ahead tick each).
 *
 * Two threads, one rule. The worker thread only ever steps the copy
 * (look.ahead) towards a target tick. The window's thread (the main thread)
 * does everything else: it makes the copy (sim_copy reads the shown world,
 * which only the main thread may touch), sets the target, and reads the copy
 * when the camera picks. Who may touch the copy is decided by busy, under the
 * lock: while busy, the copy is the worker's alone; while not busy, it is the
 * main thread's, for as long as it holds the lock.
 *
 * The main thread never waits for a tick: when the worker is busy, a pick
 * simply waits for a later frame (lookthread_borrow gives NULL). Only a new
 * world (lookthread_reset) and quitting (lookthread_stop) wait, at most one
 * tick.
 *
 * Without a thread (the browser build, which has none, or --frames runs that
 * must give the same pictures every time) the same calls work on the main
 * thread as before: lookthread_sync steps the copy itself within its budget.
 */
#ifndef VEREDAS_LOOKTHREAD_H
#define VEREDAS_LOOKTHREAD_H

#include <stdbool.h>
#include <stddef.h> /* size_t */
#include <stdint.h>

#include "lookahead.h"
#include "sim.h"

/* Emscripten builds the browser page without threads (they would need a
 * different page setup and special server headers), so there the copy keeps
 * running on the main thread. Everywhere else: POSIX threads, which Linux has
 * and MinGW brings on Windows (winpthreads). */
#ifdef __EMSCRIPTEN__
#define LOOKTHREAD_THREADS 0
#else
#define LOOKTHREAD_THREADS 1
#include <pthread.h>
#endif

typedef struct {
    /* The copy and its bookkeeping. look.ahead is the worker's while busy;
     * every other field of look is the main thread's alone. */
    Lookahead look;
    bool started; /* a worker thread is running */
#if LOOKTHREAD_THREADS
    pthread_t thread;
    pthread_mutex_t lock; /* guards the fields below and who owns look.ahead */
    pthread_cond_t wake;  /* the main thread to the worker: more to do, or quit */
    pthread_cond_t idle;  /* the worker to the main thread: a step is done */
#endif
    /* Under the lock: */
    bool quit;       /* the worker should end */
    bool busy;       /* the worker is stepping look.ahead, without the lock */
    uint64_t target; /* the worker steps the copy until its tick reaches this */
    uint64_t done;   /* look.ahead.tick, as the worker last left it */
} LookThread;

/* Empty, no thread yet; lead in ticks, as lookahead_init. */
void lookthread_init(LookThread *t, uint64_t lead);
/* Starts the worker thread. False when there are no threads (the browser
 * build) or one could not be made: then every call below works on the main
 * thread, as the camera did before it had a thread. */
bool lookthread_start(LookThread *t);
/* Once a frame: makes a fresh copy when it is needed and the worker is not
 * busy, and sets the target to shown->tick + lead. Never waits for a tick.
 * Without a thread it steps the copy itself until budget_ms by now_ms has
 * passed (lookahead_sync). False, with a one-line message, only when a copy
 * could not be made (memory ran out). */
bool lookthread_sync(LookThread *t, const Sim *shown, uint64_t (*now_ms)(void),
                     uint64_t budget_ms, char *err, size_t len);
/* The copy for the main thread to read (lookahead_pick, lookahead_quiet...),
 * or NULL when the worker is stepping it: then try again next frame. A
 * non-NULL borrow holds the lock, so it must be given back with
 * lookthread_return, soon: the worker cannot start its next step meanwhile. */
const Lookahead *lookthread_borrow(LookThread *t);
void lookthread_return(LookThread *t);
/* Frees the copy when the camera has no work (its memory back). When the
 * worker is busy it is told to stop after this step, and a later call frees
 * it. Never waits. */
void lookthread_drop(LookThread *t);
/* Frees the copy and sets a new lead, for a new world: waits for the worker's
 * step to end, at most one tick. */
void lookthread_reset(LookThread *t, uint64_t lead);
/* Waits until the worker has run the copy to its target (or its world ended).
 * For tests and for measuring; the window never waits. */
void lookthread_wait(LookThread *t);
/* What the frame log reports: whether a copy exists, the tick it was made at,
 * and how far it has run. */
void lookthread_progress(LookThread *t, bool *has, uint64_t *copied_at,
                         uint64_t *tick);
/* Ends the worker thread (waiting for its step to end) and frees the copy.
 * Safe without a thread and safe to call twice. */
void lookthread_stop(LookThread *t);

#endif
