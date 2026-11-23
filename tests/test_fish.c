/*
 * Fish (the livelier hunting design, 5.1): a gene-less layer on deep water
 * that grows from plankton, drifts toward it and away from fish eaters; who
 * may bite it, how it is seen, and the shoals a new world starts with.
 */
#include "test.h"

#include <math.h>   /* fabsf */
#include <stdlib.h> /* free */
#include <string.h> /* memcmp */

#include "brain.h"
#include "creature.h"
#include "fish.h"
#include "plankton.h"
#include "save.h"
#include "settings.h"
#include "sim.h"
#include "world.h"

/* A world of deep water, plankton and fish at 0. */
static void make_sea(World *w, int width, int height)
{
    CHECK(world_init(w, width, height));
    for (int i = 0; i < width * height; i++) {
        w->terrain[i] = TERRAIN_WATER;
    }
}

/* An empty pool for fish_step to read, with the tick set to now. */
static void empty_pool(Pool *p, int capacity, uint32_t now)
{
    CHECK(pool_init(p, capacity));
    p->now = now;
}

/* ---- the layer ---- */

static void test_fish_grow_on_rich_water(void)
{
    Settings s = settings_default();
    s.fish_drift = 0.0f; /* growth alone */
    s.fish_graze = 0.0f;
    World w;
    make_sea(&w, 4, 1);
    w.plankton[0] = 1.0f;
    w.plankton[2] = 0.2f;
    Pool p;
    empty_pool(&p, 1, 0);
    for (int k = 0; k < 300; k++) {
        fish_step(&w, &p, &s);
    }
    CHECK(fabsf(w.fish[0] - s.fish_cap_share * 1.0f) < 1e-3f);
    CHECK(fabsf(w.fish[2] - s.fish_cap_share * 0.2f) < 1e-3f);
    CHECK(w.fish[1] == 0.0f); /* no plankton, no fish */
    pool_free(&p);
    world_free(&w);
}

static void test_fish_graze_plankton(void)
{
    Settings s = settings_default();
    s.fish_drift = 0.0f;
    World w;
    make_sea(&w, 1, 1);
    w.plankton[0] = 1.0f;
    Pool p;
    empty_pool(&p, 1, 0);
    fish_step(&w, &p, &s);
    float grown = s.fish_growth * s.fish_cap_share;
    CHECK(fabsf(w.fish[0] - grown) < 1e-6f);
    CHECK(fabsf(w.plankton[0] - (1.0f - s.fish_graze * grown)) < 1e-6f);
    pool_free(&p);
    world_free(&w);
}

/* The mean x of the fish on a row, weighted by the amount. */
static float fish_middle(const World *w)
{
    float sum = 0.0f, weight = 0.0f;
    for (int x = 0; x < w->w; x++) {
        sum += (float)x * w->fish[x];
        weight += w->fish[x];
    }
    return weight > 0.0f ? sum / weight : -1.0f;
}

static void test_fish_drift_toward_plankton(void)
{
    Settings s = settings_default();
    s.fish_growth = 0.0f; /* drift alone */
    World w;
    make_sea(&w, 16, 1);
    for (int x = 0; x < 16; x++) {
        w.plankton[x] = 0.05f * (float)x; /* richer to the east, up to x 15 */
    }
    w.fish[4] = 0.5f;
    w.fish[5] = 0.5f;
    Pool p;
    empty_pool(&p, 1, 0);
    float before = fish_middle(&w);
    for (int k = 0; k < 10; k++) {
        fish_step(&w, &p, &s);
    }
    CHECK(fish_middle(&w) > before + 1.0f);
    pool_free(&p);
    world_free(&w);
}

/* A creature that ate fish lately, at (x, 0) on a sea one row high. */
static int fed_eater(Pool *p, World *w, int x, uint32_t ate_at)
{
    int idx = pool_spawn_body(p, w, x, 0, true);
    CHECK(idx >= 0);
    if (idx >= 0) {
        p->items[idx].fish_fed = 1;
        p->items[idx].fish_meal_at = ate_at;
    }
    return idx;
}

static void test_fish_flee_eater(void)
{
    Settings s = settings_default();
    s.fish_growth = 0.0f;
    World w;
    make_sea(&w, 16, 1);
    w.fish[7] = 0.5f; /* beside the eater, to its east */
    w.fish[5] = 0.5f; /* beside it, to its west */
    Pool p;
    empty_pool(&p, 2, 100);
    fed_eater(&p, &w, 6, 95);
    fish_step(&w, &p, &s);
    /* Both sides slide away from x 6: fish_flee of each moves one cell out. */
    CHECK(fabsf(w.fish[7] - 0.5f * (1.0f - s.fish_flee)) < 1e-6f);
    CHECK(fabsf(w.fish[8] - 0.5f * s.fish_flee) < 1e-6f);
    CHECK(fabsf(w.fish[4] - 0.5f * s.fish_flee) < 1e-6f);
    pool_free(&p);
    world_free(&w);
}

/* A dead creature, and an old meal, scare nothing: the fish drift as usual. */
static void test_dead_eater_scares_no_fish(void)
{
    Settings s = settings_default();
    s.fish_growth = 0.0f;
    s.fish_drift = 0.0f; /* so any move at all would be fleeing */
    World w;
    make_sea(&w, 16, 1);
    w.fish[7] = 0.5f;
    Pool p;
    empty_pool(&p, 2, 1000);
    int dead = fed_eater(&p, &w, 6, 999);
    pool_kill(&p, &w, dead, &s, DEATH_HUNGER);
    fed_eater(&p, &w, 8, 1000 - (uint32_t)s.fish_fear_ticks - 1); /* too long ago */
    fish_step(&w, &p, &s);
    CHECK(w.fish[7] == 0.5f);
    CHECK(w.fish[8] == 0.0f && w.fish[6] == 0.0f);
    pool_free(&p);
    world_free(&w);
}

static void test_fish_never_off_deep_water(void)
{
    Settings s = settings_default();
    World w;
    make_sea(&w, 12, 12);
    for (int i = 0; i < 144; i++) {
        int x = i % 12;
        w.terrain[i] = x < 3 ? TERRAIN_SOIL : x < 4 ? TERRAIN_ROCK
                       : x < 5 ? TERRAIN_SHALLOW : TERRAIN_WATER;
        w.temperature[i] = 0.7f;
    }
    plankton_prepare(&w, &s);
    plankton_start(&w);
    fish_start(&w, &s);
    Pool p;
    empty_pool(&p, 4, 0);
    fed_eater(&p, &w, 6, 0); /* scares fish toward the shore */
    for (int k = 0; k < 200; k++) {
        p.now = (uint32_t)k;
        p.items[0].fish_meal_at = (uint32_t)k;
        fish_step(&w, &p, &s);
        plankton_step(&w, &s, 0);
    }
    bool dry_clean = true, some_fish = false;
    for (int i = 0; i < 144; i++) {
        if (w.terrain[i] != TERRAIN_WATER) {
            dry_clean = dry_clean && w.fish[i] == 0.0f;
        } else {
            some_fish = some_fish || w.fish[i] > 0.0f;
        }
    }
    CHECK(dry_clean);
    CHECK(some_fish);
    pool_free(&p);
    world_free(&w);
}

/* A world with shoals goes through a save and comes back the same, and
 * carries on the same. */
static void test_save_keeps_fish(void)
{
    Settings set = settings_default();
    Sim a, b;
    char err[256] = "";
    CHECK(sim_init(&a, &set, 2, 96, 64));
    for (int i = 0; i < 200; i++) {
        sim_step(&a);
    }
    CHECK(fish_count_cells(&a.world, 0.01f) > 0);
    unsigned char *bytes = NULL;
    size_t len = 0;
    CHECK(save_to_memory(&a, &bytes, &len, err, sizeof(err)));
    CHECK(save_from_memory(&b, bytes, len, err, sizeof(err)));
    free(bytes);
    int n = a.world.w * a.world.h;
    CHECK(memcmp(a.world.fish, b.world.fish, (size_t)n * sizeof(float)) == 0);
    CHECK(sim_checksum(&a) == sim_checksum(&b));
    for (int i = 0; i < 100; i++) {
        sim_step(&a);
        sim_step(&b);
    }
    CHECK(sim_checksum(&a) == sim_checksum(&b));
    sim_free(&a);
    sim_free(&b);
}

/* ---- eating and seeing fish ---- */

/* A size-1 meat eater with a sea body or wings at (x, y), placed where that
 * body may stand. */
static int spawn_eater(Pool *p, World *w, int x, int y, bool sea, bool wings)
{
    int idx = pool_spawn_as(p, w, x, y, (BodyKind){sea, wings});
    if (idx >= 0) {
        Genome *g = &p->items[idx].genome;
        g->g[GENE_SIZE] = 0.5f;
        g->g[GENE_MEAT] = 1.0f;
        g->g[GENE_SWIM] = sea ? 1.0f : 0.0f;
        g->g[GENE_WINGS] = wings ? 1.0f : 0.0f;
        p->items[idx].energy = 10.0f;
    }
    return idx;
}

/* A sea body's bite takes fish as meat: fish_yield of what it takes, then
 * digested by its meat gene; the fish fall by the take over fish_energy; the
 * meal is remembered. */
static void test_sea_body_bites_fish_as_meat(void)
{
    Settings s = settings_default();
    s.fish_energy = 100.0f; /* a full cell holds more than one bite */
    World w;
    make_sea(&w, 8, 8);
    int fish_cell = world_idx(&w, 3, 2); /* ahead of (3, 3) */
    w.fish[fish_cell] = 1.0f;
    Pool p;
    CHECK(pool_init(&p, 2));
    p.now = 77;
    Rng r;
    rng_seed(&r, 1);
    int sea = spawn_eater(&p, &w, 3, 3, true, false);
    BiteChoice bite = creature_bite_choice(&w, &p, sea, &s);
    CHECK(bite.kind == BITE_FISH && bite.cell == fish_cell);
    float size = creature_size(&p.items[sea], &s);
    float take = s.bite_meat_energy * size; /* less than a full cell holds */
    CHECK(take < s.fish_energy);
    float before = p.items[sea].energy;
    creature_act(&w, &p, sea, (Action){.kind = ACT_BITE, .hold = 1}, &s, &r);
    CHECK(fabsf(w.fish[fish_cell] - (1.0f - take / s.fish_energy)) < 1e-5f);
    /* Meat gene 1 with the others 0: digestion keeps all of it. */
    CHECK(fabsf(p.items[sea].energy - (before + take * s.fish_yield)) < 1e-4f);
    CHECK(p.items[sea].fish_fed == 1 && p.items[sea].fish_meal_at == 77);
    CHECK(p.sea_meals_meat == 1 && p.sea_meals_fish == 1);
    pool_free(&p);
    world_free(&w);
}

/* A flyer that came down on deep water bites fish; the same flyer up in the
 * air, or on the shore, does not reach them. */
static void test_diving_flyer_bites_fish(void)
{
    Settings s = settings_default();
    World w;
    make_sea(&w, 8, 8);
    w.fish[world_idx(&w, 3, 2)] = 1.0f;
    Pool p;
    CHECK(pool_init(&p, 2));
    int bird = spawn_eater(&p, &w, 3, 3, false, true);
    /* pool_spawn_as refuses a plain flyer on deep water: put it there by
     * hand, as a dive does. */
    CHECK(bird < 0);
    w.terrain[world_idx(&w, 3, 3)] = TERRAIN_SOIL;
    bird = spawn_eater(&p, &w, 3, 3, false, true);
    CHECK(bird >= 0);
    CHECK(creature_bite_choice(&w, &p, bird, &s).kind != BITE_FISH); /* on the shore */
    w.terrain[world_idx(&w, 3, 3)] = TERRAIN_WATER; /* as if it dived there */
    CHECK(creature_fish_eater(&w, &p.items[bird]));
    CHECK(creature_bite_choice(&w, &p, bird, &s).kind == BITE_FISH);
    pool_free(&p);
    world_free(&w);
}

/* A walker on the shore, and a walker in the shallows, beside a deep cell
 * full of fish take none. */
static void test_shore_bite_takes_no_fish(void)
{
    Settings s = settings_default();
    World w;
    make_sea(&w, 8, 8);
    w.fish[world_idx(&w, 3, 2)] = 1.0f;
    w.terrain[world_idx(&w, 3, 3)] = TERRAIN_SOIL;
    w.terrain[world_idx(&w, 5, 3)] = TERRAIN_SHALLOW;
    w.fish[world_idx(&w, 5, 2)] = 1.0f;
    Pool p;
    CHECK(pool_init(&p, 2));
    int shore = spawn_eater(&p, &w, 3, 3, false, false);
    int shallow = spawn_eater(&p, &w, 5, 3, false, false);
    CHECK(shore >= 0 && shallow >= 0);
    CHECK(creature_bite_choice(&w, &p, shore, &s).kind == BITE_NOTHING);
    CHECK(creature_bite_choice(&w, &p, shallow, &s).kind == BITE_NOTHING);
    pool_free(&p);
    world_free(&w);
}

/* A sea body's view counts fish ahead as meat food, in the carrion value; a
 * walker's view of the same water does not. */
static void test_fish_seen_as_meat(void)
{
    Settings s = settings_default();
    World w;
    make_sea(&w, 16, 16);
    w.fish[world_idx(&w, 8, 6)] = 1.0f; /* two cells north of (8, 8) */
    Pool p;
    CHECK(pool_init(&p, 2));
    p.light = 1.0f;
    int sea = spawn_eater(&p, &w, 8, 8, true, false);
    Senses seen = {0};
    p.items[sea].genome.g[GENE_SIGHT] = 1.0f;
    creature_sense(&w, &p, sea, &s, &seen);
    CHECK(seen.v[SENSE_VIEWS + VIEW_AHEAD * VIEW_VALUES + VIEW_CARRION] > 0.1f);
    pool_kill(&p, &w, sea, &s, DEATH_HUNGER);
    w.carrion[world_idx(&w, 8, 8)] = 0.0f;
    w.scent[world_idx(&w, 8, 7)] = 0.0f;
    w.terrain[world_idx(&w, 8, 8)] = TERRAIN_SOIL;
    int walker = spawn_eater(&p, &w, 8, 8, false, false);
    p.items[walker].genome.g[GENE_SIGHT] = 1.0f;
    creature_sense(&w, &p, walker, &s, &seen);
    CHECK(seen.v[SENSE_VIEWS + VIEW_AHEAD * VIEW_VALUES + VIEW_CARRION] == 0.0f);
    pool_free(&p);
    world_free(&w);
}

/* A new world starts with shoals on part of its sea. */
static void test_founders_have_shoals(void)
{
    Settings set = settings_default();
    Sim a;
    CHECK(sim_init(&a, &set, 3, 96, 64));
    int deep = 0;
    for (int i = 0; i < 96 * 64; i++) {
        deep += a.world.terrain[i] == TERRAIN_WATER;
    }
    int shoals = fish_count_cells(&a.world, FISH_SHOWN);
    CHECK(deep > 0);
    CHECK(shoals > 0 && shoals < deep);
    sim_free(&a);
}

int main(void)
{
    test_fish_grow_on_rich_water();
    test_fish_graze_plankton();
    test_fish_drift_toward_plankton();
    test_fish_flee_eater();
    test_dead_eater_scares_no_fish();
    test_fish_never_off_deep_water();
    test_save_keeps_fish();
    test_sea_body_bites_fish_as_meat();
    test_diving_flyer_bites_fish();
    test_shore_bite_takes_no_fish();
    test_fish_seen_as_meat();
    test_founders_have_shoals();
    TEST_SUMMARY();
}
