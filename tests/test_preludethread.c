/*
 * The ancestors' years on their own thread (preludethread.h) must make
 * exactly the world the main thread's prelude makes: the same number of
 * ticks and the same fingerprint (sim_checksum), whether the worker runs to
 * the end or is stopped part way by a key. The main thread reads the
 * published glance while the worker steps, as the window does, so the
 * memory checkers and the thread checker (make test-threads) see the two
 * threads really running at once.
 */
#include "test.h"

#include <time.h> /* nanosleep */

#include "app.h"
#include "panel.h"
#include "preludethread.h"
#include "settings.h"
#include "sim.h"

/* Short years, as in other tests: the checkers are slow. */
#define TICKS_PER_YEAR 40
#define YEARS 3

/* What this test's screen would show: the tick, and the panel's facts, as
 * the window gathers them, so the stats scratch buffer is written between
 * ticks just as there. */
typedef struct {
    uint64_t tick;
    int animals;
    PanelFacts panel;
    int glances; /* how many times the worker filled one */
} Glance;

/* A millisecond between looks, as a frame would wait: the checkers are slow,
 * and a loop that never waits would only fight the worker for the lock. */
static void pause_a_moment(void)
{
    struct timespec ms = {.tv_sec = 0, .tv_nsec = 1000000};
    nanosleep(&ms, NULL);
}

/* ctx counts the calls: memory the worker writes while the world is away,
 * read by the main thread only after taking it back. */
static void fill(void *ctx, const Sim *s, void *out)
{
    int *calls = ctx;
    (*calls)++;
    Glance *g = out;
    g->tick = s->tick;
    g->animals = s->pool.count;
    panel_facts(s, &g->panel);
    g->glances = *calls;
}

static void open_world(Sim *s, uint64_t seed)
{
    Settings set = settings_default();
    set.ticks_per_year = TICKS_PER_YEAR;
    CHECK(sim_init(s, &set, seed, 64, 32));
}

/* The main thread's prelude, whole: the reference. */
static void plain_prelude(Sim *s, uint64_t seed)
{
    open_world(s, seed);
    Prelude p;
    app_prelude_start(&p, s, YEARS);
    CHECK(p.active);
    app_prelude_finish(&p, s);
    CHECK(!p.active);
}

/* To the end on the thread, the main thread looking at the glance meanwhile:
 * the same world, tick for tick. */
static void test_same_world_as_finish(void)
{
    Sim plain;
    plain_prelude(&plain, 11);

    Sim s;
    open_world(&s, 11);
    Prelude p;
    app_prelude_start(&p, &s, YEARS);
    int calls = 0;
    PreludeThread t;
    preludethread_init(&t, fill, &calls, sizeof(Glance));
    CHECK(preludethread_start(&t));
    CHECK(preludethread_give(&t, &s, p.until));
    CHECK(t.away);
    /* Read while the worker steps: the ticks seen only go up, and never past
     * the end. */
    Glance g;
    uint64_t last = 0;
    bool finished = false, in_order = true;
    while (!finished) {
        finished = preludethread_glance(&t, &g);
        in_order = in_order && g.tick >= last && g.tick <= p.until;
        last = g.tick;
        pause_a_moment();
    }
    CHECK(in_order);
    preludethread_take(&t);
    CHECK(!t.away);
    app_prelude_settle(&p, &s);
    CHECK(!p.active);
    CHECK(s.tick == plain.tick);
    CHECK(s.tick == p.until);
    CHECK(sim_checksum(&s) == sim_checksum(&plain));
    /* The last glance is the world as it ended, filled once on give and
     * once after each tick. */
    CHECK(g.tick == s.tick);
    CHECK(g.animals == s.pool.count);
    CHECK(calls == (int)p.until + 1);
    CHECK(g.glances == calls);
    preludethread_stop(&t);
    sim_free(&s);
    sim_free(&plain);
}

/* A key part way: the world comes back where it got to, and is the world the
 * main thread makes in as many ticks. Then given again, to its end, as after
 * the setup screen's Esc: still the same world as one run straight through. */
static void test_taken_back_part_way(void)
{
    Sim s;
    open_world(&s, 12);
    Prelude p;
    app_prelude_start(&p, &s, YEARS);
    PreludeThread t;
    int calls = 0;
    preludethread_init(&t, fill, &calls, sizeof(Glance));
    CHECK(preludethread_start(&t));
    CHECK(preludethread_give(&t, &s, p.until));
    Glance g;
    do {
        pause_a_moment();
        (void)preludethread_glance(&t, &g);
    } while (g.tick < 5);
    preludethread_take(&t); /* waits for the step under way, no more */
    uint64_t stopped_at = s.tick;
    CHECK(stopped_at >= 5 && stopped_at <= p.until);
    /* The world is this thread's: it stands still now. */
    CHECK(s.tick == stopped_at);

    Sim same;
    open_world(&same, 12);
    while (same.tick < stopped_at) {
        sim_step(&same);
    }
    CHECK(sim_checksum(&s) == sim_checksum(&same));

    app_prelude_settle(&p, &s);
    if (p.active) {
        CHECK(preludethread_give(&t, &s, p.until));
        preludethread_wait(&t);
        preludethread_take(&t);
        app_prelude_settle(&p, &s);
    }
    CHECK(!p.active);
    Sim plain;
    plain_prelude(&plain, 12);
    CHECK(s.tick == plain.tick);
    CHECK(sim_checksum(&s) == sim_checksum(&plain));
    preludethread_stop(&t);
    preludethread_stop(&t); /* twice is fine */
    sim_free(&s);
    sim_free(&same);
    sim_free(&plain);
}

/* Nothing to give: without a worker, or with a world already at its end,
 * give says no and the world stays the caller's. */
static void test_give_says_no(void)
{
    Sim s;
    open_world(&s, 13);
    PreludeThread t;
    preludethread_init(&t, NULL, NULL, 0);
    CHECK(!preludethread_give(&t, &s, 10)); /* not started */
    CHECK(!t.away);
    CHECK(preludethread_start(&t));
    CHECK(!preludethread_give(&t, &s, 0)); /* already at the end */
    CHECK(!t.away);
    CHECK(preludethread_give(&t, &s, 3)); /* no glance function: fine */
    preludethread_wait(&t);
    preludethread_take(&t);
    CHECK(s.tick == 3);
    preludethread_stop(&t); /* ends the worker; the world was taken back */
    sim_free(&s);
}

int main(void)
{
    test_same_world_as_finish();
    test_taken_back_part_way();
    test_give_says_no();
    TEST_SUMMARY();
}
