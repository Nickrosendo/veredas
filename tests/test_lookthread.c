/*
 * The look-ahead copy on its own thread (lookthread.h) must give the camera
 * exactly what the copy on the main thread gives: the same copy, tick for
 * tick, and the same picks. These tests run the shown world on the main
 * thread while the worker steps its copy, so the memory checkers (and the
 * thread checker, make test-threads) see the two really running at once.
 */
#include "test.h"

#include <string.h> /* strcmp */

#include "actions.h"
#include "genome.h"
#include "lookahead.h"
#include "lookthread.h"
#include "settings.h"
#include "sim.h"

/* Shorter than the window's season, as in test_lookahead.c: the checkers are
 * slow. */
#define LEAD 30

/* The clock for lookthread_sync's no-thread branch: never moves, so the
 * budget never runs out and each sync goes to the end. */
static uint64_t still_clock(void)
{
    return 0;
}

static int first_alive(const Sim *s)
{
    for (int i = 0; i < s->pool.capacity; i++) {
        if (s->pool.items[i].alive) {
            return i;
        }
    }
    return -1;
}

/* The pick through the thread: sync, wait for the worker, borrow, pick. */
static uint32_t thread_pick(LookThread *t, const Sim *s, unsigned tired, int *reason,
                            char *why, size_t len)
{
    char err[160];
    CHECK(lookthread_sync(t, s, still_clock, 1000, err, sizeof(err)));
    lookthread_wait(t);
    const Lookahead *l = lookthread_borrow(t);
    CHECK(l != NULL); /* the worker is done, so not busy */
    uint32_t id = LOOKAHEAD_NOBODY;
    if (l != NULL) {
        CHECK(lookahead_ready(l, s));
        id = lookahead_pick(l, s, tired, reason, why, len);
        lookthread_return(t);
    }
    return id;
}

/* The same world watched both ways: the copy on the thread matches the copy
 * run by lookahead_sync_all, and every pick and reason line is the same, with
 * the shown world stepping while the worker steps. */
static void test_same_picks_as_sync_all(bool threaded)
{
    Settings set = settings_default();
    Sim s;
    CHECK(sim_init(&s, &set, 3, 64, 32));
    Lookahead plain;
    lookahead_init(&plain, LEAD);
    LookThread t;
    lookthread_init(&t, LEAD);
    /* Unstarted, every call works on the main thread: the browser's way. */
    if (threaded) {
        CHECK(lookthread_start(&t));
    }
    int picks = 0;
    for (int tick = 0; tick < 60; tick++) {
        char err[160];
        /* Sets the target first, so the worker steps while the shown world
         * steps below: the two threads really run at once. */
        CHECK(lookthread_sync(&t, &s, still_clock, 1000, err, sizeof(err)));
        sim_step(&s);
        lookahead_sync_all(&plain, &s);
        int r1, r2;
        char why1[LOOKAHEAD_WHY], why2[LOOKAHEAD_WHY];
        uint32_t a = lookahead_pick(&plain, &s, 0, &r1, why1, sizeof(why1));
        uint32_t b = thread_pick(&t, &s, 0, &r2, why2, sizeof(why2));
        CHECK(a == b);
        CHECK(r1 == r2);
        CHECK(strcmp(why1, why2) == 0);
        picks += a != LOOKAHEAD_NOBODY;
        const Lookahead *l = lookthread_borrow(&t);
        CHECK(l != NULL);
        if (l != NULL) {
            CHECK(l->ahead.tick == plain.ahead.tick);
            CHECK(sim_checksum(&l->ahead) == sim_checksum(&plain.ahead));
            lookthread_return(&t);
        }
    }
    CHECK(picks > 0);
    lookthread_stop(&t);
    lookthread_stop(&t); /* safe twice */
    lookahead_free(&plain);
    sim_free(&s);
}

/* Watching is free on a thread too: the shown world ends as one never
 * watched. */
static void test_watching_is_free(void)
{
    Settings set = settings_default();
    Sim watched, unwatched;
    CHECK(sim_init(&watched, &set, 8, 64, 32));
    CHECK(sim_init(&unwatched, &set, 8, 64, 32));
    LookThread t;
    lookthread_init(&t, LEAD);
    CHECK(lookthread_start(&t));
    char err[160];
    for (int tick = 0; tick < 60; tick++) {
        /* No wait: the worker runs behind or level, as in the window. */
        CHECK(lookthread_sync(&t, &watched, still_clock, 1000, err, sizeof(err)));
        sim_step(&watched);
        sim_step(&unwatched);
    }
    CHECK(sim_checksum(&watched) == sim_checksum(&unwatched));
    lookthread_stop(&t);
    sim_free(&watched);
    sim_free(&unwatched);
}

/* A gift to the shown world makes the copy wrong; the next syncs make a new
 * copy, which is the changed world's future. */
static void test_action_makes_a_new_copy(void)
{
    Settings set = settings_default();
    Sim s;
    CHECK(sim_init(&s, &set, 5, 64, 32));
    LookThread t;
    lookthread_init(&t, LEAD);
    CHECK(lookthread_start(&t));
    char err[160];
    CHECK(lookthread_sync(&t, &s, still_clock, 1000, err, sizeof(err)));
    lookthread_wait(&t);
    int slot = first_alive(&s);
    CHECK(slot >= 0);
    CHECK(actions_give_trait(&s, s.pool.items[slot].id, GENE_ARMOUR, 1.0f));
    /* Two syncs: the first may find the worker busy and only stop it. */
    for (int i = 0; i < 2; i++) {
        CHECK(lookthread_sync(&t, &s, still_clock, 1000, err, sizeof(err)));
        lookthread_wait(&t);
    }
    bool has;
    uint64_t copied_at, tick;
    lookthread_progress(&t, &has, &copied_at, &tick);
    CHECK(has);
    CHECK(tick == s.tick + LEAD);
    const Lookahead *l = lookthread_borrow(&t);
    CHECK(l != NULL);
    uint64_t future = 0;
    if (l != NULL) {
        CHECK(lookahead_ready(l, &s));
        future = sim_checksum(&l->ahead);
        lookthread_return(&t);
    }
    for (int i = 0; i < LEAD; i++) {
        sim_step(&s);
    }
    CHECK(sim_checksum(&s) == future);
    lookthread_stop(&t);
    sim_free(&s);
}

/* Drop frees the copy (now or, if busy, on a later call); reset frees it at
 * once and takes a new lead. */
static void test_drop_and_reset(void)
{
    Settings set = settings_default();
    Sim s;
    CHECK(sim_init(&s, &set, 6, 64, 32));
    LookThread t;
    lookthread_init(&t, LEAD);
    CHECK(lookthread_start(&t));
    char err[160];
    CHECK(lookthread_sync(&t, &s, still_clock, 1000, err, sizeof(err)));
    lookthread_drop(&t); /* likely busy: stopped, not yet freed */
    lookthread_wait(&t);
    lookthread_drop(&t); /* now idle: freed */
    bool has;
    uint64_t copied_at, tick;
    lookthread_progress(&t, &has, &copied_at, &tick);
    CHECK(!has);
    CHECK(lookthread_sync(&t, &s, still_clock, 1000, err, sizeof(err)));
    lookthread_reset(&t, LEAD / 2);
    lookthread_progress(&t, &has, &copied_at, &tick);
    CHECK(!has);
    CHECK(t.look.lead == LEAD / 2);
    CHECK(lookthread_sync(&t, &s, still_clock, 1000, err, sizeof(err)));
    lookthread_wait(&t);
    lookthread_progress(&t, &has, &copied_at, &tick);
    CHECK(has && tick == s.tick + LEAD / 2);
    lookthread_stop(&t);
    sim_free(&s);
}

int main(void)
{
    test_same_picks_as_sync_all(true);
    test_same_picks_as_sync_all(false);
    test_watching_is_free();
    test_action_makes_a_new_copy();
    test_drop_and_reset();
    TEST_SUMMARY();
}
