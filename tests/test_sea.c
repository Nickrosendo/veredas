/*
 * The sea half of the water and air design: the sea body and where it may
 * stand, plankton, founders placed by body, the birth cell, the brake for each
 * habitat, and a sea animal travelling through a save.
 */
#include "test.h"

#include <math.h>   /* fabsf */
#include <stdlib.h> /* free */
#include <string.h> /* memcmp */

#include "actions.h"
#include "brain_script.h"
#include "notices.h"
#include "creature.h"
#include "plankton.h"
#include "save.h"
#include "settings.h"
#include "sim.h"
#include "world.h"
#include "years.h"

/* A world of all soil, fertility 1, no vegetation. */
static void make_world(World *w, int width, int height)
{
    CHECK(world_init(w, width, height));
    for (int i = 0; i < width * height; i++) {
        w->terrain[i] = TERRAIN_SOIL;
        w->fertility[i] = 1.0f;
    }
}

/* A size-1 plant-eater at (x, y) with a sea body (swim 1) or a land one
 * (swim 0), spawned where that body may stand. */
static int spawn_body(Pool *p, World *w, int x, int y, bool sea)
{
    int idx = pool_spawn_body(p, w, x, y, sea);
    if (idx >= 0) {
        Genome *g = &p->items[idx].genome;
        g->g[GENE_SIZE] = 0.5f;
        g->g[GENE_SPEED] = 1.0f;
        g->g[GENE_LEAF] = 1.0f;
        g->g[GENE_SWIM] = sea ? 1.0f : 0.0f;
        p->items[idx].energy = 10.0f;
    }
    return idx;
}

static void test_walkable_for_sea_and_land(void)
{
    World w;
    make_world(&w, 4, 1);
    w.terrain[0] = TERRAIN_WATER;
    w.terrain[1] = TERRAIN_SOIL;
    w.terrain[2] = TERRAIN_SHALLOW;
    w.terrain[3] = TERRAIN_ROCK;
    CHECK(world_walkable_for(&w, 0, true) && !world_walkable_for(&w, 0, false));
    CHECK(!world_walkable_for(&w, 1, true) && world_walkable_for(&w, 1, false));
    CHECK(world_walkable_for(&w, 2, true) && world_walkable_for(&w, 2, false));
    CHECK(!world_walkable_for(&w, 3, true) && !world_walkable_for(&w, 3, false));
    /* The old answer is the land body's. */
    for (int i = 0; i < 4; i++) {
        CHECK(world_walkable(&w, i) == world_walkable_for(&w, i, false));
    }
    world_free(&w);
}

/* The line is on the raw gene: 0.79 is a land body, 0.8 a sea body. */
static void test_sea_body_line(void)
{
    Creature c = {0};
    c.genome.g[GENE_SWIM] = 0.79f;
    CHECK(!creature_sea_body(&c));
    c.genome.g[GENE_SWIM] = SEA_FROM;
    CHECK(creature_sea_body(&c));
}

/* A sea body swims north through deep water, and soil ahead stops it: the
 * blocked sense says so, and a move turns it aside into the water. */
static void test_sea_body_moves_in_deep_water_not_on_soil(void)
{
    World w;
    make_world(&w, 8, 8);
    for (int i = 0; i < 64; i++) {
        w.terrain[i] = TERRAIN_WATER;
    }
    Settings s = settings_default();
    Pool p;
    CHECK(pool_init(&p, 2));
    int i = spawn_body(&p, &w, 3, 3, true);
    CHECK(i >= 0);
    CHECK(pool_spawn_body(&p, &w, 4, 4, false) == -1); /* no land body at sea */
    Rng r;
    rng_seed(&r, 1);
    creature_act(&w, &p, i, (Action){.kind = ACT_MOVE, .hold = 1}, &s, &r);
    CHECK(p.items[i].x == 3 && p.items[i].y == 2);

    w.terrain[world_idx(&w, 3, 1)] = TERRAIN_SOIL; /* ahead now */
    Senses in = {0};
    creature_sense(&w, &p, i, &s, &in);
    CHECK(in.v[SENSE_BLOCKED] == 1.0f);
    creature_act(&w, &p, i, (Action){.kind = ACT_MOVE, .hold = 1}, &s, &r);
    const Creature *c = &p.items[i];
    CHECK(w.terrain[world_idx(&w, c->x, c->y)] == TERRAIN_WATER);
    CHECK(c->y == 2 && c->x != 3);
    pool_free(&p);
    world_free(&w);
}

/* In deep water a sea body pays what it pays in the shallows; a full swimmer
 * pays nothing extra there. And deep water spares it the summer's heat. */
static void test_sea_step_and_heat_in_deep_water(void)
{
    World w;
    make_world(&w, 8, 8);
    Settings s = settings_default();
    Pool p;
    CHECK(pool_init(&p, 1));
    int deep = world_idx(&w, 3, 3);
    int shallow = world_idx(&w, 3, 2);
    w.terrain[deep] = TERRAIN_WATER;
    w.terrain[shallow] = TERRAIN_SHALLOW;
    int i = spawn_body(&p, &w, 3, 3, true);
    Creature *c = &p.items[i];
    CHECK(creature_step_extra(&w, c, deep, &s) == creature_step_extra(&w, c, shallow, &s));
    CHECK(creature_step_extra(&w, c, deep, &s) == 0.0f);
    c->genome.g[GENE_SWIM] = 0.9f;
    CHECK(fabsf(creature_step_extra(&w, c, deep, &s) - 0.1f * s.rough_step_cost) < 1e-6f);
    CHECK(creature_heat_cost(&w, c, &s, SEASON_SUMMER) == 0.0f);
    pool_free(&p);
    world_free(&w);
}

/* ---- plankton ---- */

/* An 8 x 8 world: soil on the left half, deep water on the right, with the
 * temperature rising to the east (x / 7). */
static void make_coast(World *w, const Settings *s)
{
    make_world(w, 8, 8);
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            int i = world_idx(w, x, y);
            w->terrain[i] = x < 4 ? TERRAIN_SOIL : TERRAIN_WATER;
            w->temperature[i] = (float)x / 7.0f;
        }
    }
    plankton_prepare(w, s);
    plankton_start(w);
}

static void test_plankton_only_on_deep_water(void)
{
    Settings s = settings_default();
    World w;
    make_coast(&w, &s);
    for (int i = 0; i < 64; i++) {
        if (w.terrain[i] == TERRAIN_WATER) {
            CHECK(w.plankton_cap[i] > 0.0f && w.plankton[i] == w.plankton_cap[i]);
        } else {
            CHECK(w.plankton_cap[i] == 0.0f && w.plankton[i] == 0.0f);
        }
    }
    world_free(&w);
}

/* Nearer the shore is richer at the same warmth, and warmer is richer at the
 * same distance. The world wraps, so on an 8-wide map x 4 and x 7 are both
 * one step from soil; x 5 and x 6 are two. */
static void test_plankton_warmer_and_nearer_shore_is_richer(void)
{
    Settings s = settings_default();
    World w;
    make_coast(&w, &s);
    float x4 = w.plankton_cap[world_idx(&w, 4, 0)];
    float x5 = w.plankton_cap[world_idx(&w, 5, 0)];
    float x7 = w.plankton_cap[world_idx(&w, 7, 0)];
    CHECK(x7 > x4); /* same distance, warmer */
    /* x 5 is warmer than x 4 but farther out: by the rule's numbers. */
    float warm5 = s.plankton_cold + (1.0f - s.plankton_cold) * 5.0f / 7.0f;
    CHECK(fabsf(x5 - warm5 / (1.0f + 2.0f / s.plankton_shore)) < 1e-6f);
    world_free(&w);
}

static void test_plankton_grows_back_and_thins_in_winter(void)
{
    Settings s = settings_default();
    World w;
    make_coast(&w, &s);
    int i = world_idx(&w, 5, 0);
    float cap = w.plankton_cap[i];
    w.plankton[i] = 0.0f;
    plankton_step(&w, &s, SEASON_SPRING);
    CHECK(fabsf(w.plankton[i] - s.plankton_growth * cap) < 1e-6f);
    for (int k = 0; k < 2000; k++) {
        plankton_step(&w, &s, SEASON_SPRING);
    }
    CHECK(fabsf(w.plankton[i] - cap) < 1e-4f);
    for (int k = 0; k < 2000; k++) {
        plankton_step(&w, &s, SEASON_WINTER);
    }
    CHECK(fabsf(w.plankton[i] - cap * s.plankton_winter) < 1e-4f);
    /* Soil stays at 0 through it all. */
    CHECK(w.plankton[world_idx(&w, 1, 0)] == 0.0f);
    world_free(&w);
}

/* A sea body's bite takes plankton steps as leaves; a land body beside the
 * same water cannot. */
static void test_sea_body_bites_plankton_as_leaves(void)
{
    Settings s = settings_default();
    s.digest_floor = 0.25f;
    World w;
    make_world(&w, 8, 8);
    int sea_cell = world_idx(&w, 3, 2); /* ahead of (3, 3) */
    w.terrain[sea_cell] = TERRAIN_WATER;
    w.terrain[world_idx(&w, 3, 3)] = TERRAIN_SHALLOW;
    w.plankton[sea_cell] = 0.6f; /* two whole steps */
    Pool p;
    CHECK(pool_init(&p, 2));
    Rng r;
    rng_seed(&r, 1);

    int land = spawn_body(&p, &w, 3, 3, false);
    BiteChoice none = creature_bite_choice(&w, &p, land, &s);
    CHECK(none.kind == BITE_NOTHING);
    pool_kill(&p, &w, land, &s, DEATH_HUNGER);
    w.carrion[world_idx(&w, 3, 3)] = 0.0f; /* the body it left */

    int sea = spawn_body(&p, &w, 3, 3, true);
    BiteChoice bite = creature_bite_choice(&w, &p, sea, &s);
    CHECK(bite.kind == BITE_PLANT && bite.plankton && bite.cell == sea_cell);
    creature_act(&w, &p, sea, (Action){.kind = ACT_BITE, .hold = 1}, &s, &r);
    /* Size 1 bites one step (bite_steps); the gain is one leaf step's. */
    CHECK(fabsf(w.plankton[sea_cell] - 0.35f) < 1e-6f);
    CHECK(fabsf(p.items[sea].energy - (10.0f + s.bite_veg_energy)) < 1e-5f);
    pool_free(&p);
    world_free(&w);
}

/* A sea body's view counts plankton as food ahead; a land body's does not. */
static void test_plankton_seen_as_food_by_sea_body(void)
{
    Settings s = settings_default();
    World w;
    make_world(&w, 16, 16);
    for (int i = 0; i < 256; i++) {
        w.terrain[i] = TERRAIN_WATER;
    }
    w.terrain[world_idx(&w, 8, 12)] = TERRAIN_SHALLOW;
    w.plankton[world_idx(&w, 8, 10)] = 1.0f; /* two cells ahead, north */
    Pool p;
    CHECK(pool_init(&p, 2));
    p.light = 1.0f;
    int sea = spawn_body(&p, &w, 8, 12, true);
    p.items[sea].genome.g[GENE_SIGHT] = 1.0f;
    Senses in = {0};
    creature_sense(&w, &p, sea, &s, &in);
    CHECK(in.v[SENSE_VIEWS + VIEW_AHEAD * VIEW_VALUES + VIEW_PLANT] > 0.0f);
    p.items[sea].genome.g[GENE_SWIM] = 0.0f; /* the same body, on land terms */
    Senses land = {0};
    creature_sense(&w, &p, sea, &s, &land);
    CHECK(land.v[SENSE_VIEWS + VIEW_AHEAD * VIEW_VALUES + VIEW_PLANT] == 0.0f);
    pool_free(&p);
    world_free(&w);
}

/* ---- founders ---- */

static void count_bodies(const Sim *s, int *land, int *sea, bool *sea_at_sea)
{
    *land = *sea = 0;
    *sea_at_sea = true;
    for (int i = 0; i < s->pool.capacity; i++) {
        const Creature *c = &s->pool.items[i];
        if (!c->alive) {
            continue;
        }
        if (creature_sea_body(c)) {
            (*sea)++;
            *sea_at_sea = *sea_at_sea &&
                          s->world.terrain[world_idx(&s->world, c->x, c->y)] == TERRAIN_WATER;
        } else {
            (*land)++;
        }
    }
}

static int count_terrain(const World *w, int kind)
{
    int n = 0;
    for (int i = 0; i < w->w * w->h; i++) {
        n += w->terrain[i] == kind;
    }
    return n;
}

/* The land gets start_share of its soil, as before the sea; the sea gets
 * founders_per_sea_cell of its deep water, every one of them out at sea. */
static void test_founders_by_body(void)
{
    Settings set = settings_default();
    Sim s;
    CHECK(sim_init(&s, &set, 2, 96, 64));
    int land, sea;
    bool at_sea;
    count_bodies(&s, &land, &sea, &at_sea);
    int soil = count_terrain(&s.world, TERRAIN_SOIL);
    int deep = count_terrain(&s.world, TERRAIN_WATER);
    CHECK(deep > 0);
    CHECK(land == (int)(set.start_share * (float)soil + 0.5f));
    CHECK(sea == (int)(set.founders_per_sea_cell * (float)deep + 0.5f));
    CHECK(at_sea);
    sim_free(&s);
}

/* With no deep water at all the search still ends, with the land full. */
static void test_founders_end_without_deep_water(void)
{
    Settings set = settings_default();
    set.water_level = 0.0f;
    set.shallow_band = 0.0f;
    Sim s;
    CHECK(sim_init(&s, &set, 3, 48, 32));
    int land, sea;
    bool at_sea;
    count_bodies(&s, &land, &sea, &at_sea);
    CHECK(count_terrain(&s.world, TERRAIN_WATER) == 0);
    CHECK(sea == 0);
    CHECK(land == (int)(set.start_share * (float)count_terrain(&s.world, TERRAIN_SOIL) + 0.5f));
    sim_free(&s);
}

/* ---- births ---- */

/* Two grown, fed adults at (3, 3) and (4, 3), on whatever ground the test
 * laid there, with swim genes swim_a and swim_b and the rest zero. */
static void ready_pair(World *w, Pool *p, int *a, int *b, float swim_a, float swim_b,
                       const Settings *s)
{
    CHECK(pool_init(p, 16));
    *a = pool_spawn_body(p, w, 3, 3, swim_a >= SEA_FROM);
    *b = pool_spawn_body(p, w, 4, 3, swim_b >= SEA_FROM);
    CHECK(*a >= 0 && *b >= 0);
    for (int k = 0; k < 2; k++) {
        Creature *c = &p->items[k == 0 ? *a : *b];
        c->genome.g[GENE_SIZE] = 0.5f;
        c->genome.g[GENE_LEAF] = 1.0f;
        c->genome.g[GENE_SWIM] = k == 0 ? swim_a : swim_b;
        c->age = (uint32_t)s->mate_age;
        c->energy = s->mate_threshold + 10.0f;
    }
}

/* Sea parents in the shallows, soil all round: a sea child has nowhere to be
 * born, so there is no birth and nobody pays. With one deep cell beside them
 * the child is born there. */
static void test_birth_cell_suits_the_child(void)
{
    Settings s = settings_default();
    s.birth_per_plant = 0.0f;
    s.birth_per_plankton = 0.0f;
    s.mutation_rate = 0.0f; /* the child's swim is a parent's: 1 */
    World w;
    make_world(&w, 8, 8);
    w.terrain[world_idx(&w, 3, 3)] = TERRAIN_SHALLOW;
    w.terrain[world_idx(&w, 4, 3)] = TERRAIN_SHALLOW;
    Pool p;
    int a, b;
    ready_pair(&w, &p, &a, &b, 1.0f, 1.0f, &s);
    Rng r;
    rng_seed(&r, 1);
    Action mate = {.kind = ACT_MATE, .hold = 1};
    float before = p.items[a].energy;
    CHECK(creature_act(&w, &p, a, mate, &s, &r) == -1);
    CHECK(creature_act(&w, &p, b, mate, &s, &r) == -1);
    CHECK(p.count == 2 && p.items[a].energy == before);

    int deep = world_idx(&w, 3, 2);
    w.terrain[deep] = TERRAIN_WATER;
    p.items[a].mate_wish = 0;
    p.items[b].mate_wish = 0;
    CHECK(creature_act(&w, &p, a, mate, &s, &r) == -1);
    int child = creature_act(&w, &p, b, mate, &s, &r);
    CHECK(child >= 0);
    if (child >= 0) {
        CHECK(world_idx(&w, p.items[child].x, p.items[child].y) == deep);
        CHECK(p.sea_alive == 1);
        CHECK(p.items[child].sea_line == 0); /* sea parents: no crossing */
    }
    pool_free(&p);
    world_free(&w);
}

/* Land parents just under the line, with genes that change a lot at birth:
 * some children come out over the line. Each such child is born in the water
 * beside them and carries sea_line; a land child on land without it. */
static void test_sea_child_of_land_parents(void)
{
    Settings s = settings_default();
    s.birth_per_plant = 0.0f;
    s.birth_per_plankton = 0.0f;
    s.mutation_rate = 1.0f;
    s.trait_mutation_scale = 10.0f;
    int sea_children = 0, land_children = 0;
    for (uint64_t seed = 1; seed <= 60; seed++) {
        World w;
        make_world(&w, 8, 8);
        for (int x = 0; x < 8; x++) {
            w.terrain[world_idx(&w, x, 2)] = TERRAIN_WATER; /* a channel north */
        }
        Pool p;
        int a, b;
        ready_pair(&w, &p, &a, &b, 0.79f, 0.79f, &s);
        Rng r;
        rng_seed(&r, seed);
        Action mate = {.kind = ACT_MATE, .hold = 1};
        creature_act(&w, &p, a, mate, &s, &r);
        int child = creature_act(&w, &p, b, mate, &s, &r);
        if (child >= 0) {
            const Creature *c = &p.items[child];
            int ground = w.terrain[world_idx(&w, c->x, c->y)];
            if (creature_sea_body(c)) {
                sea_children++;
                CHECK(ground == TERRAIN_WATER && c->sea_line == 1);
            } else {
                land_children++;
                CHECK(ground == TERRAIN_SOIL && c->sea_line == 0);
            }
        }
        pool_free(&p);
        world_free(&w);
    }
    CHECK(sea_children > 0 && land_children > 0);
}

/* A sea pair's brake counts sea bodies against the plankton: with none rich
 * there is no birth; land bodies do not count against it. */
static void test_sea_brake(void)
{
    Settings s = settings_default();
    s.birth_per_plant = 0.0f;
    World w;
    make_world(&w, 8, 8);
    for (int i = 0; i < 64; i++) {
        w.terrain[i] = TERRAIN_WATER;
    }
    Pool p;
    int a, b;
    ready_pair(&w, &p, &a, &b, 1.0f, 1.0f, &s);
    p.sea_alive = 2;
    p.plankton_rich = 0;
    Rng r;
    rng_seed(&r, 1);
    Action mate = {.kind = ACT_MATE, .hold = 1};
    creature_act(&w, &p, a, mate, &s, &r);
    CHECK(creature_act(&w, &p, b, mate, &s, &r) == -1);
    p.plankton_rich = 1000; /* room for 200 */
    CHECK(creature_act(&w, &p, b, mate, &s, &r) >= 0);
    pool_free(&p);
    world_free(&w);
}

/* A land pair's brake counts only land bodies: with room for three land bodies
 * and two alive, some births happen, though four sea bodies would fill the
 * room if they counted. */
static void test_land_brake_ignores_sea_bodies(void)
{
    Settings s = settings_default();
    s.birth_per_plant = 0.5f;
    int births = 0;
    for (uint64_t seed = 1; seed <= 20; seed++) {
        World w;
        make_world(&w, 8, 8);
        for (int x = 0; x < 4; x++) {
            w.terrain[world_idx(&w, x, 6)] = TERRAIN_WATER;
        }
        Pool p;
        int a, b;
        ready_pair(&w, &p, &a, &b, 0.0f, 0.0f, &s);
        for (int x = 0; x < 4; x++) {
            CHECK(spawn_body(&p, &w, x, 6, true) >= 0);
        }
        p.sea_alive = 4;
        p.plants = 6; /* room for 3 */
        Rng r;
        rng_seed(&r, seed);
        Action mate = {.kind = ACT_MATE, .hold = 1};
        creature_act(&w, &p, a, mate, &s, &r);
        births += creature_act(&w, &p, b, mate, &s, &r) >= 0;
        pool_free(&p);
        world_free(&w);
    }
    CHECK(births > 0);
}

/* A world with sea animals saves and loads with them still at sea, the same
 * fingerprint, and the plankton's caps made again: both go on the same. */
static void test_sea_world_saved(void)
{
    Settings set = settings_default();
    Sim a, b;
    char err[256] = "";
    CHECK(sim_init(&a, &set, 2, 96, 64));
    for (int i = 0; i < 300; i++) {
        sim_step(&a);
    }
    int land, sea;
    bool at_sea;
    count_bodies(&a, &land, &sea, &at_sea);
    CHECK(sea > 0);
    unsigned char *bytes = NULL;
    size_t len = 0;
    CHECK(save_to_memory(&a, &bytes, &len, err, sizeof(err)));
    CHECK(save_from_memory(&b, bytes, len, err, sizeof(err)));
    free(bytes);
    CHECK(sim_checksum(&a) == sim_checksum(&b));
    int n = a.world.w * a.world.h;
    CHECK(memcmp(a.world.plankton_cap, b.world.plankton_cap, (size_t)n * sizeof(float)) == 0);
    for (int i = 0; i < 100; i++) {
        sim_step(&a);
        sim_step(&b);
    }
    CHECK(sim_checksum(&a) == sim_checksum(&b));
    sim_free(&a);
    sim_free(&b);
}

/* ---- the Sandbox's sea body, and the notice ---- */

/* A 48 x 16 world of soil with deep water from x 36 to 47 (it wraps, so the
 * water also lies 1 to 12 cells west of x 0). The first living creature is
 * moved to (x, 8) as a land body; its id is returned. */
static uint32_t coast_world(Sim *s, int x)
{
    Settings set = settings_default();
    CHECK(sim_init(s, &set, 5, 48, 16));
    World *w = &s->world;
    for (int i = 0; i < 48 * 16; i++) {
        w->terrain[i] = i % 48 >= 36 ? TERRAIN_WATER : TERRAIN_SOIL;
        w->occupant[i] = -1;
    }
    /* Everybody off the map but one, so cells are free and nothing moves. */
    int kept = -1;
    for (int i = 0; i < s->pool.capacity; i++) {
        Creature *c = &s->pool.items[i];
        if (!c->alive) {
            continue;
        }
        if (kept < 0) {
            kept = i;
            c->x = x;
            c->y = 8;
            c->genome.g[GENE_SWIM] = 0.2f;
            c->energy = 50.0f;
            w->occupant[world_idx(w, x, 8)] = i;
        } else {
            c->alive = false; /* not through pool_kill: no carrion wanted */
            s->pool.count--;
        }
    }
    CHECK(kept >= 0);
    return s->pool.items[kept].id;
}

static const Creature *living(const Sim *s, uint32_t id)
{
    for (int i = 0; i < s->pool.capacity; i++) {
        if (s->pool.items[i].alive && s->pool.items[i].id == id) {
            return &s->pool.items[i];
        }
    }
    return NULL;
}

/* A sea body given four cells from the water: the creature is moved into it at
 * the next tick. Given 18 cells from it: refused, nothing recorded. */
static void test_gift_sea_body(void)
{
    Sim s;
    uint32_t id = coast_world(&s, 32);
    CHECK(actions_give_trait(&s, id, GENE_SWIM, 0.9f));
    sim_step(&s);
    const Creature *c = living(&s, id);
    CHECK(c != NULL);
    if (c != NULL) {
        CHECK(creature_sea_body(c));
        CHECK(s.world.terrain[world_idx(&s.world, c->x, c->y)] == TERRAIN_WATER);
    }
    sim_free(&s);

    id = coast_world(&s, 18);
    int before = s.actions.count;
    CHECK(!actions_body_fits(&s, id, GENE_SWIM, 0.9f));
    CHECK(!actions_give_trait(&s, id, GENE_SWIM, 0.9f));
    CHECK(s.actions.count == before);
    /* Any other gift still goes through: the body stays a land body. */
    CHECK(actions_give_trait(&s, id, GENE_SIZE, 0.9f));
    sim_free(&s);
}

/* A living sea body of a line that took to the sea is news once, naming its
 * family; one whose swim the player gave is not. */
static void test_notice_took_to_the_sea(void)
{
    Sim s;
    uint32_t id = coast_world(&s, 40); /* out at sea */
    Creature *c = (Creature *)living(&s, id);
    c->genome.g[GENE_SWIM] = 1.0f;
    Notices n;
    notices_reset(&n);
    notices_check(&n, &s); /* the first look tells nothing */
    c->sea_line = 1;
    n.last_sample = s.stats.tick + 1; /* as if a new sample were taken */
    notices_check(&n, &s);
    const Notice *x = notices_newest(&n);
    CHECK(x != NULL && strstr(x->text, "took to the sea") != NULL);
    CHECK(x != NULL && x->creature == id);
    uint32_t total = n.total;
    n.last_sample = s.stats.tick + 1;
    notices_check(&n, &s);
    CHECK(n.total == total); /* once per family */
    sim_free(&s);

    id = coast_world(&s, 40);
    c = (Creature *)living(&s, id);
    c->genome.g[GENE_SWIM] = 1.0f;
    CHECK(actions_give_trait(&s, id, GENE_SWIM, 1.0f));
    sim_step(&s); /* applies the gift */
    c = (Creature *)living(&s, id);
    CHECK(c != NULL);
    notices_reset(&n);
    notices_check(&n, &s);
    if (c != NULL) {
        c->sea_line = 1;
    }
    n.last_sample = s.stats.tick + 1;
    notices_check(&n, &s);
    CHECK(n.total == 0);
    sim_free(&s);
}

int main(void)
{
    test_walkable_for_sea_and_land();
    test_sea_body_line();
    test_sea_body_moves_in_deep_water_not_on_soil();
    test_sea_step_and_heat_in_deep_water();
    test_plankton_only_on_deep_water();
    test_plankton_warmer_and_nearer_shore_is_richer();
    test_plankton_grows_back_and_thins_in_winter();
    test_sea_body_bites_plankton_as_leaves();
    test_plankton_seen_as_food_by_sea_body();
    test_founders_by_body();
    test_founders_end_without_deep_water();
    test_birth_cell_suits_the_child();
    test_sea_child_of_land_parents();
    test_sea_brake();
    test_land_brake_ignores_sea_bodies();
    test_sea_world_saved();
    test_gift_sea_body();
    test_notice_took_to_the_sea();
    TEST_SUMMARY();
}
