/* doing_text: one line per case, on a small world made by hand. */
#include "test.h"

#include <stdio.h>  /* snprintf */
#include <stdbool.h>
#include <string.h> /* strcmp, strlen */

#include "brain.h"
#include "brain_script.h"
#include "chronicle.h"
#include "doing.h"
#include "sim.h"

/* A 16x16 world of bare soil, nobody in it, and peace never holding: any
 * creature ahead is a target, so the attack lines can be tested. */
static void make_sim(Sim *sim)
{
    Settings set = settings_default();
    snprintf(set.brain_name, sizeof(set.brain_name), "script");
    set.start_share = 0.0f;
    set.min_creatures = 0;
    /* Kinship is 1 only for identical genomes; two spawned ones differ. */
    set.family_peace = 1.0f;
    set.colour_peace = 0.0f; /* no two hues are closer than 0 */
    CHECK(sim_init(sim, &set, 7, 16, 16));
    /* With no world the lines below would write through NULL. */
    if (sim->world.terrain == NULL) {
        return;
    }
    for (int i = 0; i < 16 * 16; i++) {
        sim->world.terrain[i] = TERRAIN_SOIL;
        sim->world.veg[i] = 0;
    }
}

static Creature *spawn_at(Sim *sim, int x, int y)
{
    int slot = pool_spawn(&sim->pool, &sim->world, x, y);
    CHECK(slot >= 0);
    Creature *c = &sim->pool.items[slot];
    c->energy = 1.0f; /* hungry until a test says otherwise */
    c->health = 10.0f;
    c->current = (Action){.kind = ACT_REST, .hold = 1};
    return c;
}

/* Whether c's line is exactly want. */
static bool says(const Sim *sim, const Creature *c, const char *want)
{
    char out[DOING_LEN];
    doing_text(sim, c, out, sizeof(out));
    if (strcmp(out, want) != 0) {
        printf("  said \"%s\", wanted \"%s\"\n", out, want);
        return false;
    }
    return true;
}

static void test_sleep(void)
{
    Sim sim;
    make_sim(&sim);
    Creature *c = spawn_at(&sim, 4, 4);
    c->current.kind = ACT_SLEEP;
    c->hibernating = 1;
    CHECK(says(&sim, c, "hibernating"));
    c->hibernating = 0;
    sim.pool.light = 0.1f;
    CHECK(says(&sim, c, "asleep for the night"));
    sim.pool.light = 0.9f;
    CHECK(says(&sim, c, "napping"));
    sim_free(&sim);
}

static void test_attack(void)
{
    Sim sim;
    make_sim(&sim);
    Creature *c = spawn_at(&sim, 4, 4);
    c->heading = HEADING_EAST;
    c->current.kind = ACT_ATTACK;
    CHECK(says(&sim, c, "fighting")); /* nobody ahead */
    /* spawn may move the pool's array: c is looked up again after it. */
    uint32_t id = c->id;
    Creature *other = spawn_at(&sim, 5, 4);
    uint32_t other_id = other->id;
    /* Spawned bodies share one genome: kinship 1, and family is not struck. */
    other->genome.g[GENE_ARMOUR] = 1.0f - other->genome.g[GENE_ARMOUR];
    c = NULL;
    for (int i = 0; i < sim.pool.capacity; i++) {
        if (sim.pool.items[i].alive && sim.pool.items[i].id == id) {
            c = &sim.pool.items[i];
        }
    }
    CHECK(c != NULL);
    if (c == NULL) {
        sim_free(&sim);
        return;
    }
    char name[16], want[DOING_LEN];
    name_from_id(other_id, name, sizeof(name));
    snprintf(want, sizeof(want), "fighting %s", name);
    CHECK(says(&sim, c, want));
    c->genome.g[GENE_MEAT] = 1.0f;
    c->genome.g[GENE_LEAF] = 0.0f;
    c->genome.g[GENE_FRUIT] = 0.0f;
    snprintf(want, sizeof(want), "hunting %s", name);
    CHECK(says(&sim, c, want));
    sim_free(&sim);
}

static void test_eat_mate_move(void)
{
    Sim sim;
    make_sim(&sim);
    Creature *c = spawn_at(&sim, 4, 4);
    c->current.kind = ACT_BITE;
    CHECK(says(&sim, c, "eating"));
    c->current.kind = ACT_MATE;
    CHECK(says(&sim, c, "looking for a mate"));
    c->current.kind = ACT_MOVE;
    c->mate_wish = 10;
    CHECK(says(&sim, c, "looking for a mate"));
    c->mate_wish = 0;
    c->meal_known = 1;
    CHECK(says(&sim, c, "walking to food"));
    c->meal_known = 0;
    CHECK(says(&sim, c, "wandering"));
    c->current.kind = ACT_TURN_LEFT;
    CHECK(says(&sim, c, "looking around"));
    sim_free(&sim);
}

static void test_rest(void)
{
    Sim sim;
    make_sim(&sim);
    Creature *c = spawn_at(&sim, 0, 0);
    CHECK(says(&sim, c, "resting")); /* hungry */
    c->energy = creature_mate_needs(c, &sim.set); /* above half of it */
    CHECK(says(&sim, c, "resting: not hungry"));
    /* A plant across the map's edge is still beside a creature at 0,0. */
    sim.world.veg[world_idx(&sim.world, 15, 15)] = 1;
    CHECK(says(&sim, c, "resting beside food: not hungry"));
    sim_free(&sim);
}

/* Every line, with the longest name, fits DOING_LEN; a small buffer is cut,
 * never overrun, and ends with '\0'. */
static void test_fits(void)
{
    Sim sim;
    make_sim(&sim);
    Creature *c = spawn_at(&sim, 4, 4);
    c->energy = creature_mate_needs(c, &sim.set);
    sim.world.veg[world_idx(&sim.world, 4, 5)] = 1;
    char small[8];
    doing_text(&sim, c, small, sizeof(small));
    CHECK(strlen(small) == 7);
    CHECK(strcmp(small, "resting") == 0);
    sim_free(&sim);
}

int main(void)
{
    /* The scripted brain lives in the tests only; the Sim finds it by name. */
    CHECK(brain_register(&BRAIN_SCRIPT));
    test_sleep();
    test_attack();
    test_eat_mate_move();
    test_rest();
    test_fits();
    TEST_SUMMARY();
}
