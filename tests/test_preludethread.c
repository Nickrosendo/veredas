/*
 * The prelude on its own thread (preludethread.h) must grow the same world as
 * the prelude run on the main thread: the same fingerprint and the same life
 * stories at the end, whether it runs to its end or is stopped part way and
 * then finished. The main thread keeps reading its own world meanwhile, as
 * the window does, so the memory checkers (and the thread checker, make
 * test-threads) see the two threads really running at once.
 */
#include "test.h"

#include <time.h> /* nanosleep */

#include "app.h"
#include "preludethread.h"
#include "settings.h"
#include "sim.h"

/* Short, as in the other threaded tests: the checkers are slow. */
#define PRELUDE_TICKS 40

/* A clock that never moves: the worker never hands over a copy. */
static uint64_t still_clock(void)
{
    return 0;
}

/* A clock a second on at every look: the worker hands over a copy after
 * every tick. Read only by the worker thread, so the counter needs no lock. */
static uint64_t fast_clock(void)
{
    static uint64_t now = 0;
    now += PRELUDETHREAD_COPY_MS;
    return now;
}

/* A thousandth of a second's sleep, so a wait loop does not spin. */
static void nap(void)
{
    struct timespec t = {0, 1000000L};
    nanosleep(&t, NULL);
}

/* A prelude of PRELUDE_TICKS ticks for a world at tick 0. */
static Prelude prelude_for(const Sim *s)
{
    Prelude p;
    app_prelude_start(&p, s, 1);
    p.until = PRELUDE_TICKS; /* shorter than a year */
    return p;
}

/* The world a prelude grows on the main thread, from the same seed. */
static void grown_plain(Sim *out, uint64_t seed)
{
    Settings set = settings_default();
    CHECK(sim_init(out, &set, seed, 64, 32));
    Prelude p = prelude_for(out);
    app_prelude_finish(&p, out);
}

/* Run to the end on the thread: the same world as on the main thread. */
static void test_same_world_as_plain(void)
{
    Settings set = settings_default();
    Sim plain, shown;
    grown_plain(&plain, 4);
    CHECK(sim_init(&shown, &set, 4, 64, 32));
    Prelude p = prelude_for(&shown);
    PreludeThread t;
    preludethread_init(&t);
    CHECK(preludethread_start(&t, &shown, &p, still_clock));
    CHECK(t.started);
    bool finished = false;
    while (!finished) {
        /* The main thread reads its own world while the worker ticks. */
        CHECK(shown.tick == 0);
        CHECK(sim_checksum(&shown) != 0);
        CHECK(preludethread_progress(&t, &finished) <= PRELUDE_TICKS);
        nap();
    }
    preludethread_end(&t, &shown, &p);
    CHECK(!t.started);
    CHECK(!p.active);
    CHECK(shown.tick == PRELUDE_TICKS);
    CHECK(sim_checksum(&shown) == sim_checksum(&plain));
    CHECK(sim_story_checksum(&shown) == sim_story_checksum(&plain));
    sim_free(&shown);
    sim_free(&plain);
}

/* Stopped part way, as a key does, and then finished, as the setup screen's
 * Esc lets it go on: still the same world in the end. */
static void test_stopped_then_finished(void)
{
    Settings set = settings_default();
    Sim plain, shown;
    grown_plain(&plain, 6);
    CHECK(sim_init(&shown, &set, 6, 64, 32));
    Prelude p = prelude_for(&shown);
    PreludeThread t;
    preludethread_init(&t);
    CHECK(preludethread_start(&t, &shown, &p, still_clock));
    bool finished = false;
    while (preludethread_progress(&t, &finished) < PRELUDE_TICKS / 4 && !finished) {
        nap();
    }
    preludethread_end(&t, &shown, &p);
    CHECK(shown.tick >= PRELUDE_TICKS / 4);
    CHECK(shown.tick <= PRELUDE_TICKS);
    /* Years to go: still on, for the caller to end or go on with. */
    CHECK(p.active == (shown.tick < PRELUDE_TICKS));
    /* Going on, on a thread again, from where it stopped. */
    if (p.active) {
        CHECK(preludethread_start(&t, &shown, &p, still_clock));
        do {
            nap();
            (void)preludethread_progress(&t, &finished);
        } while (!finished);
        preludethread_end(&t, &shown, &p);
    }
    CHECK(!p.active);
    CHECK(sim_checksum(&shown) == sim_checksum(&plain));
    sim_free(&shown);
    sim_free(&plain);
}

/* With a clock that says a second has passed after every tick, the worker
 * hands over copies; each one taken is the world at some tick of the run, and
 * the end is still the plain world. */
static void test_copies_handed_over(void)
{
    Settings set = settings_default();
    Sim plain, shown;
    grown_plain(&plain, 9);
    CHECK(sim_init(&shown, &set, 9, 64, 32));
    Prelude p = prelude_for(&shown);
    PreludeThread t;
    preludethread_init(&t);
    CHECK(preludethread_start(&t, &shown, &p, fast_clock));
    int taken = 0;
    bool finished = false;
    while (!finished) {
        (void)preludethread_progress(&t, &finished);
        if (preludethread_take_copy(&t, &shown)) {
            taken++;
            /* A world of the run, after a tick of it: past the start and
             * not past the end. */
            CHECK(shown.tick > 0);
            CHECK(shown.tick <= PRELUDE_TICKS);
        }
        nap();
    }
    preludethread_end(&t, &shown, &p);
    CHECK(taken > 0);
    CHECK(sim_checksum(&shown) == sim_checksum(&plain));
    sim_free(&shown);
    sim_free(&plain);
}

/* Nothing started: the calls change nothing. */
static void test_unstarted(void)
{
    Settings set = settings_default();
    Sim s;
    CHECK(sim_init(&s, &set, 2, 64, 32));
    uint64_t before = sim_checksum(&s);
    Prelude p = prelude_for(&s);
    PreludeThread t;
    preludethread_init(&t);
    bool finished = false;
    CHECK(preludethread_progress(&t, &finished) == 0);
    CHECK(finished);
    CHECK(!preludethread_take_copy(&t, &s));
    preludethread_end(&t, &s, &p);
    CHECK(p.active);
    CHECK(sim_checksum(&s) == before);
    sim_free(&s);
}

int main(void)
{
    test_same_world_as_plain();
    test_stopped_then_finished();
    test_copies_handed_over();
    test_unstarted();
    TEST_SUMMARY();
}
