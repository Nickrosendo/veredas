#include "test.h"

#include <string.h> /* strstr, strlen */

#include "actions.h"
#include "creature.h"
#include "genome.h"
#include "lifepage.h" /* life_find_slot */
#include "lookahead.h"
#include "settings.h"
#include "sim.h"

/* The look-ahead in these tests: shorter than the window's season (150 ticks)
 * because the worlds run under the memory checkers, which are slow. */
#define LEAD 60

static int first_alive(const Sim *s)
{
    for (int i = 0; i < s->pool.capacity; i++) {
        if (s->pool.items[i].alive) {
            return i;
        }
    }
    return -1;
}

/* Watching is free: two worlds from the same seed, one with a look-ahead
 * kept level with it and asked for a pick every tick, end the same. */
static void test_watching_is_free(void)
{
    Settings set = settings_default();
    Sim watched, plain;
    CHECK(sim_init(&watched, &set, 9, 64, 32));
    CHECK(sim_init(&plain, &set, 9, 64, 32));
    Lookahead l;
    lookahead_init(&l, LEAD);
    char why[LOOKAHEAD_WHY];
    for (int t = 0; t < 200; t++) {
        lookahead_sync_all(&l, &watched);
        int reason;
        (void)lookahead_pick(&l, &watched, 0, false, &reason, why, sizeof(why));
        sim_step(&watched);
        sim_step(&plain);
    }
    CHECK(sim_checksum(&watched) == sim_checksum(&plain));
    CHECK(sim_story_checksum(&watched) == sim_story_checksum(&plain));
    lookahead_free(&l);
    sim_free(&watched);
    sim_free(&plain);
}

/* The copy runs exactly lead ticks ahead once synced, and it is the real
 * future: the shown world, stepped that far, meets the copy's fingerprint. */
static void test_copy_is_the_future(void)
{
    Settings set = settings_default();
    Sim s;
    CHECK(sim_init(&s, &set, 4, 64, 32));
    Lookahead l;
    lookahead_init(&l, LEAD);
    CHECK(!lookahead_ready(&l, &s));
    lookahead_sync_all(&l, &s);
    CHECK(lookahead_ready(&l, &s));
    CHECK(l.ahead.tick == s.tick + LEAD);
    uint64_t future = sim_checksum(&l.ahead);
    for (int t = 0; t < LEAD; t++) {
        sim_step(&s);
    }
    CHECK(sim_checksum(&s) == future);
    lookahead_free(&l);
    sim_free(&s);
}

/* The pick is a creature alive in the shown world, with a reason; before
 * the copy is ready there is no pick. */
static void test_pick_is_alive(void)
{
    Settings set = settings_default();
    Sim s;
    CHECK(sim_init(&s, &set, 2, 64, 32));
    for (int t = 0; t < 100; t++) {
        sim_step(&s);
    }
    Lookahead l;
    lookahead_init(&l, LEAD);
    char why[LOOKAHEAD_WHY];
    int reason;
    CHECK(lookahead_pick(&l, &s, 0, false, &reason, why, sizeof(why)) ==
          LOOKAHEAD_NOBODY);
    lookahead_sync_all(&l, &s);
    uint32_t id = lookahead_pick(&l, &s, 0, false, &reason, why, sizeof(why));
    CHECK(id != LOOKAHEAD_NOBODY);
    CHECK(life_find_slot(&s, id) >= 0);
    CHECK(strlen(why) > 0);
    lookahead_free(&l);
    sim_free(&s);
}

/* An action in the shown world makes the copy's future wrong: the next sync
 * starts a new copy, and it is not ready until it has run ahead again. */
static void test_action_makes_a_new_copy(void)
{
    Settings set = settings_default();
    Sim s;
    CHECK(sim_init(&s, &set, 5, 64, 32));
    Lookahead l;
    lookahead_init(&l, LEAD);
    lookahead_sync_all(&l, &s);
    CHECK(lookahead_ready(&l, &s));
    int slot = first_alive(&s);
    CHECK(slot >= 0);
    CHECK(actions_give_trait(&s, s.pool.items[slot].id, GENE_ARMOUR, 1.0f));
    CHECK(!lookahead_ready(&l, &s));
    lookahead_sync_all(&l, &s);
    CHECK(lookahead_ready(&l, &s));
    /* The new copy carries the gift: it is the future of the changed world. */
    uint64_t future = sim_checksum(&l.ahead);
    for (int t = 0; t < LEAD; t++) {
        sim_step(&s);
    }
    CHECK(sim_checksum(&s) == future);
    lookahead_free(&l);
    sim_free(&s);
}

/* The score: action first. A hunter killed beats a first kill, which beats
 * growing giant, which beats a child; nothing coming scores least. */
static void test_score_order(void)
{
    LifeTally now = {0};
    LifeTally child = now, kill = now, giant = now, hunter = now;
    child.children = 1;
    kill.kills = 1;
    giant.flags = TALLY_GIANT;
    hunter.kills = 1;
    hunter.hunter_kills = 1;
    int why = -1;
    float s_none = lookahead_score(&now, 100, &now, 250, &why);
    CHECK(why == LOOK_QUIET);
    float s_child = lookahead_score(&now, 100, &child, 250, &why);
    CHECK(strstr(lookahead_reason_text(why), "young") != NULL);
    float s_kill = lookahead_score(&now, 100, &kill, 250, &why);
    CHECK(strstr(lookahead_reason_text(why), "first kill") != NULL);
    float s_giant = lookahead_score(&now, 100, &giant, 250, &why);
    CHECK(strstr(lookahead_reason_text(why), "giant") != NULL);
    float s_hunter = lookahead_score(&now, 100, &hunter, 250, &why);
    CHECK(strstr(lookahead_reason_text(why), "hunter") != NULL);
    CHECK(s_none < s_child);
    CHECK(s_child < s_giant);
    CHECK(s_giant < s_kill);
    CHECK(s_kill < s_hunter);
    /* A second kill is no first: the line says so. */
    LifeTally one_kill = kill, two_kills = kill;
    two_kills.kills = 2;
    (void)lookahead_score(&one_kill, 100, &two_kills, 250, &why);
    CHECK(why == LOOK_MORE_KILLS);
}

/* The newer reasons and the kinds: a heavy wound and kin lost are named, kin
 * lost counts at most three, and every reason has a kind, the quiet season
 * alone the quiet kind. */
static void test_wounds_kin_and_kinds(void)
{
    LifeTally now = {0};
    LifeTally wound = now, three = now, ten = now;
    wound.heavy_wounds = 1;
    three.kin_lost = 3;
    ten.kin_lost = 10;
    int why = -1;
    (void)lookahead_score(&now, 100, &wound, 250, &why);
    CHECK(why == LOOK_WOUND);
    CHECK(lookahead_kind(why) == LOOK_KIND_FIGHT);
    float s_three = lookahead_score(&now, 100, &three, 250, &why);
    CHECK(why == LOOK_KIN_LOST);
    CHECK(lookahead_kind(why) == LOOK_KIND_FAMILY);
    float s_ten = lookahead_score(&now, 100, &ten, 250, &why);
    CHECK(s_ten == s_three);
    for (int r = 0; r < LOOK_REASON_COUNT; r++) {
        int k = lookahead_kind(r);
        CHECK(k >= 0 && k < LOOK_KIND_COUNT);
        CHECK((k == LOOK_KIND_QUIET) == (r == LOOK_QUIET));
        CHECK(strlen(lookahead_reason_text(r)) > 0);
    }
}

/* Tired of the first pick's kind, the camera picks another kind when one is
 * worth enough, and never names a reason of the tired kind unless it has to:
 * then it is the same creature as the untired pick. */
static void test_tired_kind_turns(void)
{
    Settings set = settings_default();
    Sim s;
    CHECK(sim_init(&s, &set, 3, 128, 72));
    for (int t = 0; t < 1200; t++) {
        sim_step(&s);
    }
    Lookahead l;
    lookahead_init(&l, LEAD);
    lookahead_sync_all(&l, &s);
    char why[LOOKAHEAD_WHY];
    int first, second;
    uint32_t a = lookahead_pick(&l, &s, 0, false, &first, why, sizeof(why));
    CHECK(a != LOOKAHEAD_NOBODY);
    unsigned tired = 1u << lookahead_kind(first);
    uint32_t b = lookahead_pick(&l, &s, tired, false, &second, why, sizeof(why));
    CHECK(b != LOOKAHEAD_NOBODY);
    CHECK(lookahead_kind(second) != lookahead_kind(first) || b == a);
    lookahead_free(&l);
    sim_free(&s);
}

/* A new world (another seed) or one put back in time is not the copy's past:
 * the copy is dropped and made again. */
static void test_new_world_drops_copy(void)
{
    Settings set = settings_default();
    Sim s;
    CHECK(sim_init(&s, &set, 3, 64, 32));
    Lookahead l;
    lookahead_init(&l, LEAD);
    lookahead_sync_all(&l, &s);
    CHECK(lookahead_ready(&l, &s));
    CHECK(sim_reseed(&s, 77));
    CHECK(!lookahead_ready(&l, &s));
    lookahead_sync_all(&l, &s);
    CHECK(lookahead_ready(&l, &s));
    CHECK(l.ahead.seed == 77);
    lookahead_free(&l);
    lookahead_free(&l); /* twice is safe */
    sim_free(&s);
}

/* A coming dive kill (also a kill) is named as a dive and outscores a plain
 * first kill; fish meals make a hunt at sea, at most FISH_MOST of them; both
 * are hunts. */
static void test_score_dive_and_sea_hunt(void)
{
    LifeTally now = {0};
    LifeTally kill = now, dive = now, sea = now, fish = now, more_fish = now;
    kill.kills = 1;
    dive.kills = 1;
    dive.dive_kills = 1;
    sea.kills = 1;
    sea.sea_kills = 1;
    fish.fish_meals = 20;
    more_fish.fish_meals = 200;
    int why = -1;
    float s_kill = lookahead_score(&now, 100, &kill, 250, &why);
    float s_dive = lookahead_score(&now, 100, &dive, 250, &why);
    CHECK(why == LOOK_DIVE);
    CHECK(strcmp(lookahead_reason_text(why), "a dive coming") == 0);
    CHECK(s_dive > s_kill);
    (void)lookahead_score(&now, 100, &sea, 250, &why);
    CHECK(why == LOOK_SEA_HUNT);
    float s_fish = lookahead_score(&now, 100, &fish, 250, &why);
    CHECK(why == LOOK_SEA_HUNT);
    CHECK(strcmp(lookahead_reason_text(why), "a hunt at sea coming") == 0);
    CHECK(lookahead_score(&now, 100, &more_fish, 250, &why) == s_fish);
    CHECK(lookahead_kind(LOOK_DIVE) == LOOK_KIND_HUNT);
    CHECK(lookahead_kind(LOOK_SEA_HUNT) == LOOK_KIND_HUNT);
}

/* Inside the hunt kind a coming dive or hunt at sea outranks a coming land
 * kill of a hunter (the livelier hunting design, section 11): one of each,
 * from a life with no kills yet (each side gets the first kill's worth) and
 * from one with kills behind it. A dive that kills a hunter is named a dive. */
static void test_dive_and_sea_beat_hunter_kill(void)
{
    for (uint32_t before = 0; before <= 5; before += 5) {
        LifeTally now = {.kills = before};
        LifeTally hunter = now, dive = now, sea = now, dive_hunter = now;
        hunter.kills = before + 1;
        hunter.hunter_kills = 1;
        dive.kills = before + 1;
        dive.dive_kills = 1;
        sea.kills = before + 1;
        sea.sea_kills = 1;
        dive_hunter = hunter;
        dive_hunter.dive_kills = 1;
        int why = -1;
        float s_hunter = lookahead_score(&now, 100, &hunter, 250, &why);
        CHECK(why == LOOK_HUNTER_KILL);
        float s_dive = lookahead_score(&now, 100, &dive, 250, &why);
        CHECK(why == LOOK_DIVE);
        float s_sea = lookahead_score(&now, 100, &sea, 250, &why);
        CHECK(why == LOOK_SEA_HUNT);
        CHECK(s_dive > s_hunter);
        CHECK(s_sea > s_hunter);
        (void)lookahead_score(&now, 100, &dive_hunter, 250, &why);
        CHECK(why == LOOK_DIVE);
    }
}

/* The first two lives alive on screen and alive, in the same slot, in the
 * copy: their slots in *a and *b. False when the world has fewer. */
static bool two_kept(const Sim *s, const Lookahead *l, int *a, int *b)
{
    *a = -1;
    *b = -1;
    for (int i = 0; i < s->pool.capacity; i++) {
        const Creature *c = &s->pool.items[i];
        const Creature *later = &l->ahead.pool.items[i];
        if (!c->alive || !later->alive || later->id != c->id) {
            continue;
        }
        if (*a < 0) {
            *a = i;
        } else {
            *b = i;
            return true;
        }
    }
    return false;
}

/* Hunt picks alternate (the livelier hunting design, section 12): a land
 * hunter with many kills coming (six, three of them hunters: about 111) against
 * a life with fewer (six kills, one of them a dive or a sea kill: 68 to 83,
 * its biggest part its kills). The life with the dive or sea kill
 * is picked, and named for it, when the last hunt pick was not one or there
 * was none; the land hunter when the last hunt pick was a dive or a sea hunt.
 * Every other life kept in the copy is given its tally of now: nothing
 * coming. The copy's tallies are set by hand, so what is coming is known. */
static void test_hunt_picks_alternate(void)
{
    for (int sea = 0; sea <= 1; sea++) {
        Settings set = settings_default();
        Sim s;
        CHECK(sim_init(&s, &set, 2, 64, 32));
        for (int t = 0; t < 300; t++) {
            sim_step(&s);
        }
        Lookahead l;
        lookahead_init(&l, LEAD);
        lookahead_sync_all(&l, &s);
        for (int i = 0; i < s.pool.capacity; i++) {
            Creature *later = &l.ahead.pool.items[i];
            if (s.pool.items[i].alive && later->alive &&
                later->id == s.pool.items[i].id) {
                later->tally = s.pool.items[i].tally;
            }
        }
        int land, other;
        CHECK(two_kept(&s, &l, &land, &other));
        LifeTally *hunter = &l.ahead.pool.items[land].tally;
        hunter->kills += 6;
        hunter->hunter_kills += 3;
        LifeTally *diver = &l.ahead.pool.items[other].tally;
        diver->kills += 6;
        if (sea) {
            diver->sea_kills += 1;
        } else {
            diver->dive_kills += 1;
        }
        int expected = sea ? LOOK_SEA_HUNT : LOOK_DIVE;
        uint32_t land_id = s.pool.items[land].id;
        uint32_t diver_id = s.pool.items[other].id;
        /* Its biggest part alone is its kills ("more kills to come", or
         * "first kill soon" for a life with none yet), not the dive or the sea
         * kill: the pick names the dive or the sea kill instead, so the
         * counted reason and the line agree. */
        int part;
        (void)lookahead_score(&s.pool.items[other].tally, s.pool.items[other].age,
                              diver, l.ahead.pool.items[other].age, &part);
        CHECK(part == LOOK_MORE_KILLS || part == LOOK_FIRST_KILL);
        char why[LOOKAHEAD_WHY];
        int reason;
        /* No hunt pick yet, or the last one a land kill: the same false. */
        uint32_t id = lookahead_pick(&l, &s, 0, false, &reason, why, sizeof(why));
        CHECK(id == diver_id);
        CHECK(reason == expected);
        CHECK(strstr(why, lookahead_reason_text(expected)) != NULL);
        /* The state the callers keep: a land kill pick leaves it false, a
         * dive or sea pick sets it, a pick of another kind leaves it. */
        CHECK(!lookahead_sea_sky_last(true, LOOK_HUNTER_KILL));
        CHECK(!lookahead_sea_sky_last(false, LOOK_MORE_KILLS));
        CHECK(lookahead_sea_sky_last(false, expected));
        CHECK(lookahead_sea_sky_last(true, LOOK_YOUNG));
        CHECK(!lookahead_sea_sky_last(false, LOOK_GIANT));
        /* The last hunt pick was a dive or a sea hunt: the best score wins. */
        id = lookahead_pick(&l, &s, 0, true, &reason, why, sizeof(why));
        CHECK(id == land_id);
        CHECK(reason == LOOK_HUNTER_KILL);
        lookahead_free(&l);
        sim_free(&s);
    }
}

/* The camera re-picks after a while (the livelier hunting design, section
 * 9): its pick may be left once followed for LOOKAHEAD_REPICK_TICKS and quiet
 * (nothing coming in the copy); not sooner, not while something is coming,
 * not before the copy is ready. The copy's tally is set by hand, so what is
 * coming is known exactly. */
static void test_repick_when_quiet(void)
{
    Settings set = settings_default();
    Sim s;
    CHECK(sim_init(&s, &set, 2, 64, 32));
    for (int t = 0; t < 300; t++) {
        sim_step(&s);
    }
    Lookahead l;
    lookahead_init(&l, LEAD);
    lookahead_sync_all(&l, &s);
    /* One alive on screen and still alive, in the same slot, in the copy. */
    int slot = -1;
    for (int i = 0; i < s.pool.capacity && slot < 0; i++) {
        const Creature *c = &s.pool.items[i];
        const Creature *later = &l.ahead.pool.items[i];
        if (c->alive && later->alive && later->id == c->id) {
            slot = i;
        }
    }
    CHECK(slot >= 0);
    uint32_t id = s.pool.items[slot].id;
    Creature *later = &l.ahead.pool.items[slot];
    /* Nothing coming: the same tally later as now. */
    later->tally = s.pool.items[slot].tally;
    CHECK(lookahead_quiet(&l, &s, id));
    uint64_t now = s.tick;
    CHECK(!lookahead_repick_due(&l, &s, id, now));
    CHECK(!lookahead_repick_due(&l, &s, id, now - (LOOKAHEAD_REPICK_TICKS - 1)));
    CHECK(lookahead_repick_due(&l, &s, id, now - LOOKAHEAD_REPICK_TICKS));
    /* A pick "made" after the world's tick (a world put back) is not due. */
    CHECK(!lookahead_repick_due(&l, &s, id, now + 1));
    /* A kill coming: not quiet, so the camera stays however long it has
     * followed. */
    later->tally.kills++;
    CHECK(!lookahead_quiet(&l, &s, id));
    CHECK(!lookahead_repick_due(&l, &s, id, now - LOOKAHEAD_REPICK_TICKS));
    later->tally.kills--;
    /* Nobody known: no answer, so no re-pick. */
    CHECK(!lookahead_quiet(&l, &s, LOOKAHEAD_NOBODY));
    /* No copy ready: nobody knows what is coming. */
    lookahead_free(&l);
    CHECK(!lookahead_quiet(&l, &s, id));
    CHECK(!lookahead_repick_due(&l, &s, id, now - LOOKAHEAD_REPICK_TICKS));
    sim_free(&s);
}

int main(void)
{
    test_score_dive_and_sea_hunt();
    test_dive_and_sea_beat_hunter_kill();
    test_watching_is_free();
    test_copy_is_the_future();
    test_pick_is_alive();
    test_action_makes_a_new_copy();
    test_score_order();
    test_wounds_kin_and_kinds();
    test_tired_kind_turns();
    test_hunt_picks_alternate();
    test_new_world_drops_copy();
    test_repick_when_quiet();
    TEST_SUMMARY();
}
