#include "preludethread.h"

#include "save.h" /* sim_copy */

void preludethread_init(PreludeThread *t)
{
    *t = (PreludeThread){0};
}

#if PRELUDETHREAD_THREADS

/* Hands the main thread a copy of the worker's world, replacing a copy it
 * has not taken yet. The copy is made without the lock (it takes about
 * 30 ms, and the main thread asks for the lock every frame); only the hand
 * over is under it. A copy that cannot be made (memory ran out) is skipped:
 * the main thread goes on showing the one it has. */
static void hand_over_copy(PreludeThread *t)
{
    char err[160];
    Sim fresh;
    if (!sim_copy(&fresh, &t->world, err, sizeof(err))) {
        return;
    }
    /* A struct is copied whole by =, its pointers included: the memory
     * fresh points to now belongs to t->copy. */
    pthread_mutex_lock(&t->lock);
    Sim old = t->copy;
    bool had = t->has_copy;
    t->copy = fresh;
    t->has_copy = true;
    pthread_mutex_unlock(&t->lock);
    if (had) {
        sim_free(&old); /* never taken: nobody else points at it */
    }
}

/*
 * The worker thread's whole life: tick after tick until the run is over or
 * the main thread asks it to stop, publishing how far it has got after each.
 * The lock is held only to read stop and publish the tick, never during a
 * tick, so the main thread never waits for one just to look.
 */
static void *worker(void *arg)
{
    PreludeThread *t = arg;
    uint64_t last_copy = t->now_ms();
    for (;;) {
        pthread_mutex_lock(&t->lock);
        bool stop = t->stop;
        pthread_mutex_unlock(&t->lock);
        if (stop || !t->prelude.active) {
            break;
        }
        app_prelude_tick(&t->prelude, &t->world);
        pthread_mutex_lock(&t->lock);
        t->tick = t->world.tick;
        pthread_mutex_unlock(&t->lock);
        /* Unsigned subtraction: right even if the clock wrapped round. No
         * copy once the run is over: the whole world is about to be handed
         * over anyway. */
        uint64_t now = t->now_ms();
        if (t->prelude.active && now - last_copy >= PRELUDETHREAD_COPY_MS) {
            hand_over_copy(t);
            last_copy = t->now_ms();
        }
    }
    /* From here on the worker does not touch world again: setting finished
     * under the lock hands it to the main thread. */
    pthread_mutex_lock(&t->lock);
    t->finished = true;
    pthread_mutex_unlock(&t->lock);
    return NULL;
}

bool preludethread_start(PreludeThread *t, const Sim *shown, const Prelude *p,
                         uint64_t (*now_ms)(void))
{
    char err[160];
    preludethread_init(t);
    /* sim_copy fills t->world only on success: on failure there is nothing
     * to free. */
    if (!sim_copy(&t->world, shown, err, sizeof(err))) {
        return false;
    }
    t->prelude = *p;
    t->now_ms = now_ms;
    t->tick = t->world.tick;
    pthread_mutex_init(&t->lock, NULL);
    /* pthread_create returns 0 on success and an error number otherwise. The
     * worker may start before pthread_create returns: everything it reads
     * was set above. */
    if (pthread_create(&t->thread, NULL, worker, t) != 0) {
        pthread_mutex_destroy(&t->lock);
        sim_free(&t->world);
        preludethread_init(t);
        return false;
    }
    t->started = true;
    return true;
}

uint64_t preludethread_progress(PreludeThread *t, bool *finished)
{
    if (!t->started) {
        *finished = true;
        return 0;
    }
    pthread_mutex_lock(&t->lock);
    uint64_t tick = t->tick;
    *finished = t->finished;
    pthread_mutex_unlock(&t->lock);
    return tick;
}

bool preludethread_take_copy(PreludeThread *t, Sim *shown)
{
    if (!t->started) {
        return false;
    }
    pthread_mutex_lock(&t->lock);
    bool had = t->has_copy;
    Sim fresh = t->copy;
    t->has_copy = false;
    t->copy = (Sim){0};
    pthread_mutex_unlock(&t->lock);
    if (!had) {
        return false;
    }
    /* The old world is freed outside the lock: only this thread knows it. */
    sim_free(shown);
    *shown = fresh;
    return true;
}

void preludethread_end(PreludeThread *t, Sim *shown, Prelude *p)
{
    if (!t->started) {
        return;
    }
    pthread_mutex_lock(&t->lock);
    t->stop = true;
    pthread_mutex_unlock(&t->lock);
    /* Waits for worker to return, at most one tick: after this nothing else
     * touches t, so no lock is needed below. */
    pthread_join(t->thread, NULL);
    pthread_mutex_destroy(&t->lock);
    if (t->has_copy) {
        sim_free(&t->copy);
    }
    sim_free(shown);
    *shown = t->world;
    *p = t->prelude;
    preludethread_init(t);
}

#else /* no threads: the browser build */

bool preludethread_start(PreludeThread *t, const Sim *shown, const Prelude *p,
                         uint64_t (*now_ms)(void))
{
    /* Casts to void say "unused on purpose", which keeps the compiler quiet. */
    (void)shown;
    (void)p;
    (void)now_ms;
    preludethread_init(t);
    return false;
}

uint64_t preludethread_progress(PreludeThread *t, bool *finished)
{
    (void)t;
    *finished = true;
    return 0;
}

bool preludethread_take_copy(PreludeThread *t, Sim *shown)
{
    (void)t;
    (void)shown;
    return false;
}

void preludethread_end(PreludeThread *t, Sim *shown, Prelude *p)
{
    (void)t;
    (void)shown;
    (void)p;
}

#endif
