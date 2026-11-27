#include "preludethread.h"

#include <stdlib.h> /* malloc, free */
#include <string.h> /* memcpy */

void preludethread_init(PreludeThread *t, PreludeGlanceFn fn, void *ctx,
                        size_t glance_size)
{
    *t = (PreludeThread){0};
    t->glance_fn = fn;
    t->glance_ctx = ctx;
    t->glance_size = glance_size;
}

#if PRELUDETHREAD_THREADS

/* Whether the world is at the prelude's end: the rule app_prelude_settle
 * keeps on the main thread. Only on the thread that owns sim. */
static bool at_end(const PreludeThread *t)
{
    return t->sim->tick >= t->until || sim_is_over(t->sim);
}

static void fill_glance(PreludeThread *t, unsigned char *out)
{
    if (t->glance_fn != NULL) {
        t->glance_fn(t->glance_ctx, t->sim, out);
    }
}


/*
 * The worker thread's whole life, as in lookthread.c: it holds the lock
 * except while it steps and while it sleeps (pthread_cond_wait lets go of the
 * lock while asleep and takes it back before returning).
 *
 * A step is one tick and one glance, both without the lock: the world is the
 * worker's alone while away, so nothing else may read it, and nothing else
 * reads glance_next. Only the copy into glance, which the main thread reads,
 * needs the lock, and it takes microseconds.
 */
static void *worker(void *arg)
{
    PreludeThread *t = arg;
    pthread_mutex_lock(&t->lock);
    while (!t->quit) {
        if (!t->away || t->finished) {
            /* In a loop: a wait may end with nobody having signalled. */
            pthread_cond_wait(&t->wake, &t->lock);
            continue;
        }
        t->busy = true;
        pthread_mutex_unlock(&t->lock);
        sim_step(t->sim);
        bool end = at_end(t);
        fill_glance(t, t->glance_next);
        pthread_mutex_lock(&t->lock);
        t->busy = false;
        t->finished = end;
        memcpy(t->glance, t->glance_next, t->glance_size);
        /* Wakes preludethread_take or _wait, if one is waiting. */
        pthread_cond_broadcast(&t->idle);
    }
    pthread_mutex_unlock(&t->lock);
    return NULL;
}

static void free_glances(PreludeThread *t)
{
    free(t->glance);
    free(t->glance_next);
    t->glance = t->glance_next = NULL;
}

bool preludethread_start(PreludeThread *t)
{
    if (t->started) {
        return true;
    }
    /* At least one byte each, so a caller with no glance still gets memory
     * (malloc(0) may give NULL, which would read as running out). */
    size_t size = t->glance_size > 0 ? t->glance_size : 1;
    t->glance = malloc(size);
    t->glance_next = malloc(size);
    if (t->glance == NULL || t->glance_next == NULL) {
        free_glances(t);
        return false;
    }
    pthread_mutex_init(&t->lock, NULL);
    pthread_cond_init(&t->wake, NULL);
    pthread_cond_init(&t->idle, NULL);
    t->quit = t->busy = t->away = t->finished = false;
    if (pthread_create(&t->thread, NULL, worker, t) != 0) {
        pthread_cond_destroy(&t->idle);
        pthread_cond_destroy(&t->wake);
        pthread_mutex_destroy(&t->lock);
        free_glances(t);
        return false;
    }
    t->started = true;
    return true;
}

bool preludethread_give(PreludeThread *t, Sim *sim, uint64_t until)
{
    if (!t->started || t->away) {
        return false;
    }
    /* The worker is asleep (not away), so these plain writes race with
     * nothing; the lock below publishes them to it. */
    t->sim = sim;
    t->until = until;
    if (at_end(t)) {
        return false; /* nothing to run: the caller ends its prelude */
    }
    /* The first glance, here, while the world is still this thread's: the
     * screen has something to draw before the first tick ends. */
    fill_glance(t, t->glance_next);
    pthread_mutex_lock(&t->lock);
    memcpy(t->glance, t->glance_next, t->glance_size);
    t->finished = false;
    t->away = true;
    /* Under the lock, so the worker cannot be between looking at away and
     * going to sleep: the signal cannot be lost. */
    pthread_cond_signal(&t->wake);
    pthread_mutex_unlock(&t->lock);
    return true;
}

bool preludethread_glance(PreludeThread *t, void *out)
{
    if (!t->away) {
        return false;
    }
    pthread_mutex_lock(&t->lock);
    memcpy(out, t->glance, t->glance_size);
    bool finished = t->finished;
    pthread_mutex_unlock(&t->lock);
    return finished;
}

void preludethread_take(PreludeThread *t)
{
    if (!t->away) {
        return;
    }
    pthread_mutex_lock(&t->lock);
    t->away = false; /* the worker runs no further step after this one */
    while (t->busy) {
        pthread_cond_wait(&t->idle, &t->lock);
    }
    pthread_mutex_unlock(&t->lock);
    /* Not busy and not away: the world is this thread's again. The unlock
     * after the worker's last step and the lock here order every write of
     * that step before every read that follows. */
}

void preludethread_wait(PreludeThread *t)
{
    if (!t->away) {
        return;
    }
    pthread_mutex_lock(&t->lock);
    while (!t->finished) {
        pthread_cond_wait(&t->idle, &t->lock);
    }
    pthread_mutex_unlock(&t->lock);
}

void preludethread_stop(PreludeThread *t)
{
    if (!t->started) {
        return;
    }
    preludethread_take(t);
    pthread_mutex_lock(&t->lock);
    t->quit = true;
    pthread_cond_signal(&t->wake);
    pthread_mutex_unlock(&t->lock);
    /* Waits for worker to return: after this nothing else touches t. */
    pthread_join(t->thread, NULL);
    pthread_cond_destroy(&t->idle);
    pthread_cond_destroy(&t->wake);
    pthread_mutex_destroy(&t->lock);
    free_glances(t);
    t->started = false;
}

#else /* no threads: the browser build */

bool preludethread_start(PreludeThread *t)
{
    (void)t; /* there is nothing to start */
    return false;
}

bool preludethread_give(PreludeThread *t, Sim *sim, uint64_t until)
{
    (void)t;
    (void)sim;
    (void)until;
    return false; /* the caller runs the prelude itself */
}

bool preludethread_glance(PreludeThread *t, void *out)
{
    (void)t;
    (void)out;
    return false;
}

void preludethread_take(PreludeThread *t) { (void)t; }
void preludethread_wait(PreludeThread *t) { (void)t; }
void preludethread_stop(PreludeThread *t) { (void)t; }

#endif
