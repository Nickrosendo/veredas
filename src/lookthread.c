#include "lookthread.h"

void lookthread_init(LookThread *t, uint64_t lead)
{
    *t = (LookThread){0};
    lookahead_init(&t->look, lead);
}

#if LOOKTHREAD_THREADS

/* Whether the worker should step the copy now. Read under the lock and only
 * while not busy, the only times look.ahead may be looked at from outside a
 * step. */
static bool has_work(const LookThread *t)
{
    return t->look.has && t->done < t->target && !sim_is_over(&t->look.ahead);
}

/*
 * The worker thread's whole life. It holds the lock except while it steps,
 * and while it waits: pthread_cond_wait lets go of the lock while it sleeps
 * and takes it back before it returns, so the main thread can get in.
 *
 * The step itself runs without the lock (a tick is about 35 ms, and holding
 * the lock that long would make every frame wait). busy = true is what keeps
 * the main thread's hands off look.ahead meanwhile.
 */
static void *worker(void *arg)
{
    LookThread *t = arg;
    pthread_mutex_lock(&t->lock);
    while (!t->quit) {
        if (!has_work(t)) {
            /* In a loop, as POSIX asks: a wait may also end with nobody
             * having signalled ("spurious wakeups"), so the condition is
             * checked again each time. */
            pthread_cond_wait(&t->wake, &t->lock);
            continue;
        }
        t->busy = true;
        pthread_mutex_unlock(&t->lock);
        sim_step(&t->look.ahead);
        pthread_mutex_lock(&t->lock);
        t->busy = false;
        t->done = t->look.ahead.tick;
        /* broadcast, not signal: wakes every waiter (lookthread_reset,
         * _wait or _stop), of which there is at most one today. */
        pthread_cond_broadcast(&t->idle);
    }
    pthread_mutex_unlock(&t->lock);
    return NULL;
}

bool lookthread_start(LookThread *t)
{
    if (t->started) {
        return true;
    }
    pthread_mutex_init(&t->lock, NULL);
    pthread_cond_init(&t->wake, NULL);
    pthread_cond_init(&t->idle, NULL);
    t->quit = false;
    t->busy = false;
    /* pthread_create returns 0 on success and an error number otherwise (not
     * -1 and errno, as most POSIX calls do). The new thread starts in worker
     * at once, possibly before pthread_create has returned here: everything
     * it reads was set above. */
    if (pthread_create(&t->thread, NULL, worker, t) != 0) {
        pthread_cond_destroy(&t->idle);
        pthread_cond_destroy(&t->wake);
        pthread_mutex_destroy(&t->lock);
        return false;
    }
    t->started = true;
    return true;
}

static void lock(LookThread *t)
{
    if (t->started) {
        pthread_mutex_lock(&t->lock);
    }
}

static void unlock(LookThread *t)
{
    if (t->started) {
        pthread_mutex_unlock(&t->lock);
    }
}

/* Sleeps until the worker's step is over. Called with the lock held. */
static void wait_not_busy(LookThread *t)
{
    while (t->started && t->busy) {
        pthread_cond_wait(&t->idle, &t->lock);
    }
}

#else /* no threads: the browser build */

bool lookthread_start(LookThread *t)
{
    (void)t; /* there is nothing to start */
    return false;
}

static void lock(LookThread *t) { (void)t; }
static void unlock(LookThread *t) { (void)t; }
static void wait_not_busy(LookThread *t) { (void)t; }

#endif

bool lookthread_sync(LookThread *t, const Sim *shown, uint64_t (*now_ms)(void),
                     uint64_t budget_ms, char *err, size_t len)
{
    if (!t->started) {
        return lookahead_sync(&t->look, shown, now_ms, budget_ms, err, len);
    }
    lock(t);
    if (t->busy) {
        /* The copy is the worker's: only the target may change. A copy that
         * is no longer the future, or has fallen behind the shown world, is
         * stopped after this step, and a later frame makes a new one. These
         * reads are of look's own fields and done, never of look.ahead. */
        if (!lookahead_is_future(&t->look, shown) || t->done < shown->tick) {
            t->target = 0;
        } else {
            t->target = shown->tick + t->look.lead;
        }
        unlock(t);
        return true;
    }
    /* Not busy: the copy is ours while we hold the lock. A new copy (about
     * 30 ms) is made here, on the main thread, because it reads the shown
     * world, which the main thread changes. */
    bool ok = lookahead_copy(&t->look, shown, err, len);
    if (ok) {
        t->done = t->look.ahead.tick;
        t->target = shown->tick + t->look.lead;
#if LOOKTHREAD_THREADS
        /* Under the lock, so the worker cannot be between checking for work
         * and going to sleep: the signal cannot be lost. */
        pthread_cond_signal(&t->wake);
#endif
    } else {
        t->done = t->target = 0;
    }
    unlock(t);
    return ok;
}

const Lookahead *lookthread_borrow(LookThread *t)
{
    lock(t);
    if (t->busy) {
        unlock(t);
        return NULL;
    }
    return &t->look;
}

void lookthread_return(LookThread *t)
{
    unlock(t);
}

void lookthread_drop(LookThread *t)
{
    lock(t);
    if (t->busy) {
        t->target = 0; /* stop after this step; a later call frees it */
    } else {
        lookahead_free(&t->look);
        t->done = t->target = 0;
    }
    unlock(t);
}

void lookthread_reset(LookThread *t, uint64_t lead)
{
    lock(t);
    wait_not_busy(t);
    lookahead_free(&t->look);
    t->look.lead = lead;
    t->done = t->target = 0;
    unlock(t);
}

void lookthread_wait(LookThread *t)
{
#if LOOKTHREAD_THREADS
    if (!t->started) {
        return;
    }
    pthread_mutex_lock(&t->lock);
    while (t->busy || has_work(t)) {
        pthread_cond_wait(&t->idle, &t->lock);
    }
    pthread_mutex_unlock(&t->lock);
#else
    (void)t;
#endif
}

void lookthread_progress(LookThread *t, bool *has, uint64_t *copied_at,
                         uint64_t *tick)
{
    lock(t);
    *has = t->look.has;
    *copied_at = t->look.copied_at;
    /* Without a thread the copy's own tick; with one, the tick the worker
     * last published (look.ahead.tick may be changing right now). */
    if (t->started) {
        *tick = t->done;
    } else {
        *tick = t->look.has ? t->look.ahead.tick : 0;
    }
    unlock(t);
}

void lookthread_stop(LookThread *t)
{
#if LOOKTHREAD_THREADS
    if (t->started) {
        pthread_mutex_lock(&t->lock);
        t->quit = true;
        pthread_cond_signal(&t->wake);
        pthread_mutex_unlock(&t->lock);
        /* Waits for worker to return: after this nothing else touches t. */
        pthread_join(t->thread, NULL);
        pthread_cond_destroy(&t->idle);
        pthread_cond_destroy(&t->wake);
        pthread_mutex_destroy(&t->lock);
        t->started = false;
    }
#endif
    lookahead_free(&t->look);
    t->done = t->target = 0;
}
