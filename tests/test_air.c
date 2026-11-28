/*
 * The air half of the water and air design: the wing gene and its dodge, the
 * flyer line, the sky grid, going up and coming down, striking from the air,
 * the senses of the air, births and the flyers' brake, and a flyer travelling
 * through a save.
 */
#include "test.h"

#include <math.h>   /* fabsf */
#include <stdlib.h> /* free, abs */
#include <string.h> /* strstr */

#include "actions.h"
#include "creature.h"
#include "notices.h"
#include "save.h"
#include "settings.h"
#include "sim.h"
#include "world.h"

/* A world of all soil, fertility 1, no vegetation. */
static void make_world(World *w, int width, int height)
{
    CHECK(world_init(w, width, height));
    for (int i = 0; i < width * height; i++) {
        w->terrain[i] = TERRAIN_SOIL;
        w->fertility[i] = 1.0f;
    }
}

/* A size-1 plant-eater at (x, y) on the ground, with wings gene wings. */
static int spawn_winged(Pool *p, World *w, int x, int y, float wings)
{
    BodyKind body = {false, wings >= AIR_FROM};
    int idx = pool_spawn_as(p, w, x, y, body);
    if (idx >= 0) {
        Genome *g = &p->items[idx].genome;
        g->g[GENE_SIZE] = 0.5f;
        g->g[GENE_SPEED] = 1.0f;
        g->g[GENE_LEAF] = 1.0f;
        g->g[GENE_WINGS] = wings;
        p->items[idx].energy = 50.0f;
        p->items[idx].move_points = 1.0f;
    }
    return idx;
}

/* Makes c a stranger to a creature with all-zero weights: unlike genes (below
 * family_peace) and a far colour (past colour_peace), so a strike lands. */
static void make_stranger(Creature *c)
{
    c->genome.g[GENE_HUE] = 0.5f;
    for (int k = GENE_TRAITS; k < GENOME_LEN; k++) {
        c->genome.g[k] = 1.0f;
    }
}

static Action act(int kind)
{
    return (Action){.kind = (uint8_t)kind, .hold = 1};
}

/* ---- the gene and the line ---- */

static void test_flyer_line(void)
{
    Creature c = {0};
    c.genome.g[GENE_WINGS] = 0.79f;
    CHECK(!creature_flyer(&c));
    CHECK(creature_habitat(&c) == HABITAT_LAND);
    c.genome.g[GENE_WINGS] = AIR_FROM;
    CHECK(creature_flyer(&c));
    CHECK(creature_habitat(&c) == HABITAT_AIR);
    c.genome.g[GENE_SWIM] = 1.0f; /* a gull: a flyer first */
    CHECK(creature_habitat(&c) == HABITAT_AIR);
}

/* Equal sizes hit half the time. Full wings dodge wing_dodge of those hits;
 * no wings dodge none. Counted over many swings at a target on the cell ahead. */
static int hits_on(float wings, uint64_t seed)
{
    Settings s = settings_default();
    World w;
    make_world(&w, 8, 8);
    Pool p;
    CHECK(pool_init(&p, 2));
    int a = spawn_winged(&p, &w, 3, 4, 0.0f);
    int t = spawn_winged(&p, &w, 3, 3, wings); /* north of a: ahead */
    make_stranger(&p.items[t]);
    Rng r;
    rng_seed(&r, seed);
    int hits = 0;
    for (int k = 0; k < 2000; k++) {
        p.items[t].health = 1000.0f;
        uint32_t before = p.items[t].hits_taken;
        creature_act(&w, &p, a, act(ACT_ATTACK), &s, &r);
        hits += p.items[t].hits_taken > before;
        p.items[a].energy = 50.0f;
        p.now += 1000; /* no gang effect between swings */
    }
    pool_free(&p);
    world_free(&w);
    return hits;
}

static void test_wings_dodge_some_hits(void)
{
    int bare = hits_on(0.0f, 1);
    int winged = hits_on(1.0f, 1);
    CHECK(bare > 900 && bare < 1100);
    float expected = (float)bare * (1.0f - settings_default().wing_dodge);
    CHECK(fabsf((float)winged - expected) < 90.0f);
}

static void test_wings_cost_upkeep(void)
{
    Settings s = settings_default();
    Creature c = {0};
    float without = creature_upkeep_cost(&c, &s);
    c.genome.g[GENE_WINGS] = 0.5f;
    CHECK(fabsf(creature_upkeep_cost(&c, &s) - without - 0.5f * s.wings_upkeep) < 1e-6f);
}

/* Only a flyer may stand on rock; a walker beside the same rock turns aside. */
static void test_flyer_may_stand_on_rock(void)
{
    World w;
    make_world(&w, 8, 8);
    w.terrain[world_idx(&w, 3, 2)] = TERRAIN_ROCK;
    CHECK(world_ground_for(&w, world_idx(&w, 3, 2), false, true));
    CHECK(!world_ground_for(&w, world_idx(&w, 3, 2), false, false));
    Pool p;
    CHECK(pool_init(&p, 2));
    CHECK(pool_spawn_as(&p, &w, 3, 2, (BodyKind){false, false}) == -1);
    int f = pool_spawn_as(&p, &w, 3, 2, (BodyKind){false, true});
    CHECK(f >= 0);
    pool_free(&p);
    world_free(&w);
}

/* ---- up and down ---- */

/* A flyer on the ground moves: it is up, on the sky grid and off the ground,
 * one cell north, over deep water if that is what lies there. */
static void test_flyer_takes_off_and_flies_over_water(void)
{
    Settings s = settings_default();
    World w;
    make_world(&w, 8, 8);
    for (int x = 0; x < 8; x++) {
        w.terrain[world_idx(&w, x, 2)] = TERRAIN_WATER;
    }
    Pool p;
    CHECK(pool_init(&p, 1));
    int f = spawn_winged(&p, &w, 3, 3, 1.0f);
    Rng r;
    rng_seed(&r, 1);
    float before = p.items[f].energy;
    creature_act(&w, &p, f, act(ACT_MOVE), &s, &r);
    const Creature *c = &p.items[f];
    int cell = world_idx(&w, 3, 2);
    CHECK(c->up == 1 && c->x == 3 && c->y == 2);
    CHECK(w.sky[SKY_LOW][cell] == f && w.occupant[cell] == -1);
    CHECK(w.occupant[world_idx(&w, 3, 3)] == -1 && w.sky[SKY_LOW][world_idx(&w, 3, 3)] == -1);
    /* move_cost plus the air's price by size, nothing for the ground. */
    float paid = before - c->energy;
    CHECK(fabsf(paid - s.move_cost - s.flight_step_cost * creature_size(c, &s)) < 1e-5f);
    pool_free(&p);
    world_free(&w);
}

/* Half-wings never go up: the step is a walk, and water stops it. */
static void test_walker_never_goes_up(void)
{
    Settings s = settings_default();
    World w;
    make_world(&w, 8, 8);
    Pool p;
    CHECK(pool_init(&p, 1));
    int f = spawn_winged(&p, &w, 3, 3, 0.79f);
    Rng r;
    rng_seed(&r, 1);
    creature_act(&w, &p, f, act(ACT_MOVE), &s, &r);
    CHECK(p.items[f].up == 0 && p.items[f].y == 2);
    CHECK(w.occupant[world_idx(&w, 3, 2)] == f);
    pool_free(&p);
    world_free(&w);
}

/* A bigger body pays more for each step in the air. */
static void test_flight_costs_by_size(void)
{
    Settings s = settings_default();
    float paid[2];
    for (int k = 0; k < 2; k++) {
        World w;
        make_world(&w, 8, 8);
        Pool p;
        CHECK(pool_init(&p, 1));
        int f = spawn_winged(&p, &w, 3, 3, 1.0f);
        p.items[f].genome.g[GENE_SIZE] = k == 0 ? 0.0f : 1.0f;
        Rng r;
        rng_seed(&r, 1);
        float before = p.items[f].energy;
        creature_act(&w, &p, f, act(ACT_MOVE), &s, &r);
        paid[k] = before - p.items[f].energy;
        pool_free(&p);
        world_free(&w);
    }
    CHECK(paid[1] > paid[0] + 0.1f);
}

/* Up a flyer, then the brain bites with a plant beside it: it comes down and
 * eats in the same tick. */
static void test_flyer_lands_to_bite_plant(void)
{
    Settings s = settings_default();
    World w;
    make_world(&w, 8, 8);
    Pool p;
    CHECK(pool_init(&p, 1));
    int f = spawn_winged(&p, &w, 3, 3, 1.0f);
    Rng r;
    rng_seed(&r, 1);
    creature_act(&w, &p, f, act(ACT_MOVE), &s, &r); /* up, at (3, 2) */
    CHECK(p.items[f].up == 1);
    int plant = world_idx(&w, 3, 1);
    w.veg[plant] = VEG_MAX;
    w.plant[plant].energy = 10.0f;
    float before = p.items[f].energy;
    creature_act(&w, &p, f, act(ACT_BITE), &s, &r);
    CHECK(p.items[f].up == 0);
    CHECK(w.occupant[world_idx(&w, 3, 2)] == f && w.sky[SKY_LOW][world_idx(&w, 3, 2)] == -1);
    CHECK(p.items[f].energy > before);
    CHECK(p.air_meals_plant == 1 && p.air_landings == 1);
    pool_free(&p);
    world_free(&w);
}

/* Over rock a flyer comes down to rest: it stands on the rock. */
static void test_flyer_lands_on_rock_to_rest(void)
{
    Settings s = settings_default();
    World w;
    make_world(&w, 8, 8);
    w.terrain[world_idx(&w, 3, 2)] = TERRAIN_ROCK;
    Pool p;
    CHECK(pool_init(&p, 1));
    int f = spawn_winged(&p, &w, 3, 3, 1.0f);
    Rng r;
    rng_seed(&r, 1);
    creature_act(&w, &p, f, act(ACT_MOVE), &s, &r);
    CHECK(p.items[f].up == 1);
    creature_act(&w, &p, f, act(ACT_REST), &s, &r);
    CHECK(p.items[f].up == 0 && w.occupant[world_idx(&w, 3, 2)] == f);
    pool_free(&p);
    world_free(&w);
}

/* Over deep water a flyer (not a sea body) cannot come down to sleep: it does
 * not sleep in the air, it rests and its brain chooses again next tick. */
static void test_flyer_cannot_sleep_in_the_air(void)
{
    Settings s = settings_default();
    World w;
    make_world(&w, 8, 8);
    w.terrain[world_idx(&w, 3, 2)] = TERRAIN_WATER;
    Pool p;
    CHECK(pool_init(&p, 1));
    int f = spawn_winged(&p, &w, 3, 3, 1.0f);
    Rng r;
    rng_seed(&r, 1);
    creature_act(&w, &p, f, act(ACT_MOVE), &s, &r);
    Creature *c = &p.items[f];
    c->current = (Action){.kind = ACT_SLEEP, .hold = 50};
    c->hold_left = 49;
    creature_act(&w, &p, f, c->current, &s, &r);
    CHECK(c->up == 1);
    CHECK(!creature_asleep(c) && c->hold_left == 0);
    pool_free(&p);
    world_free(&w);
}

/* A hunting flyer up strikes the animal on the ground below it and comes
 * down on a free cell beside it. */
static void test_flyer_strikes_from_above_and_lands_beside(void)
{
    Settings s = settings_default();
    World w;
    make_world(&w, 8, 8);
    Pool p;
    CHECK(pool_init(&p, 2));
    int f = spawn_winged(&p, &w, 3, 3, 1.0f);
    Creature *hawk = &p.items[f];
    hawk->genome.g[GENE_LEAF] = 0.0f;
    hawk->genome.g[GENE_MEAT] = 1.0f;
    hawk->genome.g[GENE_SIZE] = 1.0f; /* bigger: likely to hit */
    Rng r;
    rng_seed(&r, 1);
    creature_act(&w, &p, f, act(ACT_MOVE), &s, &r); /* up, at (3, 2) */
    int prey = spawn_winged(&p, &w, 3, 2, 0.0f); /* below it, on the ground */
    CHECK(prey >= 0);
    make_stranger(&p.items[prey]);
    CHECK(creature_attack_target(&w, &p, f, &s) == prey);
    creature_act(&w, &p, f, act(ACT_BITE), &s, &r);
    CHECK(hawk->up == 0);
    int dx = abs(hawk->x - 3), dy = abs(hawk->y - 2);
    CHECK(dx <= 1 && dy <= 1 && (dx + dy) > 0);
    CHECK(w.occupant[world_idx(&w, hawk->x, hawk->y)] == f);
    CHECK(w.sky[SKY_LOW][world_idx(&w, 3, 2)] == -1);
    pool_free(&p);
    world_free(&w);
}

/* A wolf facing the cell under a flyer up strikes nobody. */
static void test_flyer_up_cannot_be_struck(void)
{
    Settings s = settings_default();
    World w;
    make_world(&w, 8, 8);
    Pool p;
    CHECK(pool_init(&p, 2));
    int f = spawn_winged(&p, &w, 3, 3, 1.0f);
    Rng r;
    rng_seed(&r, 1);
    creature_act(&w, &p, f, act(ACT_MOVE), &s, &r); /* up, at (3, 2) */
    int wolf = spawn_winged(&p, &w, 3, 3, 0.0f); /* south of it, facing north */
    CHECK(wolf >= 0);
    make_stranger(&p.items[wolf]);
    CHECK(creature_attack_target(&w, &p, wolf, &s) == -1);
    pool_free(&p);
    world_free(&w);
}

/* A flyer that dies up frees its sky cell, and its body falls as carrion onto
 * the water below. */
static void test_flyer_dies_up_frees_sky_and_drops_carrion(void)
{
    Settings s = settings_default();
    World w;
    make_world(&w, 8, 8);
    int sea = world_idx(&w, 3, 2);
    w.terrain[sea] = TERRAIN_WATER;
    Pool p;
    CHECK(pool_init(&p, 1));
    int f = spawn_winged(&p, &w, 3, 3, 1.0f);
    Rng r;
    rng_seed(&r, 1);
    creature_act(&w, &p, f, act(ACT_MOVE), &s, &r);
    CHECK(p.items[f].up == 1);
    pool_kill(&p, &w, f, &s, DEATH_HUNGER);
    CHECK(w.sky[SKY_LOW][sea] == -1 && w.occupant[sea] == -1);
    CHECK(w.carrion[sea] > 0.0f);
    pool_free(&p);
    world_free(&w);
}

/* SENSE_ALOFT is 1 up and 0 on the ground. A walker sees a flyer up ahead in
 * VIEW_ALOFT, not as a creature in VIEW_SEEN. */
static void test_aloft_senses(void)
{
    Settings s = settings_default();
    World w;
    make_world(&w, 16, 16);
    Pool p;
    CHECK(pool_init(&p, 2));
    p.light = 1.0f;
    int f = spawn_winged(&p, &w, 8, 6, 1.0f);
    Rng r;
    rng_seed(&r, 1);
    Senses in = {0};
    creature_sense(&w, &p, f, &s, &in);
    CHECK(in.v[SENSE_ALOFT] == 0.0f);
    creature_act(&w, &p, f, act(ACT_MOVE), &s, &r); /* up, at (8, 5) */
    creature_sense(&w, &p, f, &s, &in);
    CHECK(in.v[SENSE_ALOFT] == 1.0f);

    int walker = spawn_winged(&p, &w, 8, 9, 0.0f); /* facing north at it */
    p.items[walker].genome.g[GENE_SIGHT] = 1.0f;
    Senses seen = {0};
    creature_sense(&w, &p, walker, &s, &seen);
    const float *ahead = &seen.v[SENSE_VIEWS + VIEW_AHEAD * VIEW_VALUES];
    CHECK(ahead[VIEW_ALOFT] > 0.0f);
    CHECK(ahead[VIEW_SEEN] == 0.0f);
    CHECK(seen.v[SENSE_ALOFT] == 0.0f);
    pool_free(&p);
    world_free(&w);
}

/* ---- the air's payoff (the livelier hunting design, 5.2) ---- */

/* A meat-eating flyer up over a stranger standing below it: the flyer's slot
 * in *f, the prey's returned. */
static int dive_scene(Pool *p, World *w, Rng *r, const Settings *s, int *f)
{
    *f = spawn_winged(p, w, 3, 3, 1.0f);
    Creature *hawk = &p->items[*f];
    hawk->genome.g[GENE_LEAF] = 0.0f;
    hawk->genome.g[GENE_MEAT] = 1.0f;
    hawk->genome.g[GENE_SIZE] = 1.0f;
    creature_act(w, p, *f, act(ACT_MOVE), s, r); /* up, at (3, 2) */
    int prey = spawn_winged(p, w, 3, 2, 0.0f);
    make_stranger(&p->items[prey]);
    p->items[prey].genome.g[GENE_ARMOUR] = 0.0f;
    p->items[prey].genome.g[GENE_WINGS] = 0.0f; /* nothing dodged */
    return prey;
}

/* A hit struck from the air takes dive_strike_bonus times the attacker's size
 * more than the same hit from the ground would. */
static void test_dive_hits_harder(void)
{
    Settings s = settings_default();
    bool landed = false;
    for (uint64_t seed = 1; seed < 64 && !landed; seed++) {
        World w;
        make_world(&w, 8, 8);
        Pool p;
        CHECK(pool_init(&p, 2));
        Rng r;
        rng_seed(&r, seed);
        int f;
        int prey = dive_scene(&p, &w, &r, &s, &f);
        p.items[prey].health = 100.0f; /* lives through any hit */
        creature_act(&w, &p, f, act(ACT_BITE), &s, &r);
        float taken = 100.0f - p.items[prey].health;
        if (taken > 0.0f) {
            landed = true;
            const Creature *hawk = &p.items[f];
            float a = creature_size(hawk, &s);
            float raised = 1.0f + s.weapon_damage * hawk->genome.g[GENE_WEAPON];
            float ground = s.bite_damage * a * raised;
            CHECK(fabsf(taken - (ground + s.dive_strike_bonus * a)) < 1e-4f);
            CHECK(p.dive_kills == 0);
        }
        pool_free(&p);
        world_free(&w);
    }
    CHECK(landed);
}

/* A kill struck from the air is counted, and as a walker's; so is the strike
 * itself, aimed at a walker, landed or not, with the share of health the
 * walker had before a landed one. */
static void test_dive_kill_counted(void)
{
    Settings s = settings_default();
    bool killed = false;
    for (uint64_t seed = 1; seed < 64 && !killed; seed++) {
        World w;
        make_world(&w, 8, 8);
        Pool p;
        CHECK(pool_init(&p, 2));
        Rng r;
        rng_seed(&r, seed);
        int f;
        int prey = dive_scene(&p, &w, &r, &s, &f);
        p.items[prey].health = 0.001f;
        float share = 0.001f / creature_max_health(&p.items[prey], &s);
        creature_act(&w, &p, f, act(ACT_BITE), &s, &r);
        CHECK(p.dive_swings == 1 && p.dive_aimed_land == 1);
        if (p.items[prey].health <= 0.0f) {
            killed = true;
            CHECK(p.dive_kills == 1 && p.dive_kills_land == 1);
            CHECK(p.dive_hits_land == 1);
            CHECK(fabs(p.dive_hit_share_sum - share) < 1e-9);
        } else {
            CHECK(p.dive_hits_land == 0 && p.dive_hit_share_sum == 0.0);
        }
        pool_free(&p);
        world_free(&w);
    }
    CHECK(killed);
}

/* A strike from the air reaches around (the livelier hunting design, section
 * 9): it aims at the animal right below, or else at the one on the ground of
 * the cell ahead; never at one beside or behind. */
static void test_air_strike_reaches_ahead(void)
{
    Settings s = settings_default();
    World w;
    make_world(&w, 8, 8);
    Pool p;
    CHECK(pool_init(&p, 8));
    Rng r;
    rng_seed(&r, 1);
    int f = spawn_winged(&p, &w, 3, 3, 1.0f);
    creature_act(&w, &p, f, act(ACT_MOVE), &s, &r); /* up, at (3, 2) */
    CHECK(p.items[f].up > 0);
    p.items[f].heading = HEADING_NORTH;
    /* Nobody below or ahead: nothing to strike. */
    CHECK(creature_attack_target(&w, &p, f, &s) == -1);
    /* One beside (east) and one behind (south): still nothing. */
    int beside = spawn_winged(&p, &w, 4, 2, 0.0f);
    make_stranger(&p.items[beside]);
    int behind = spawn_winged(&p, &w, 3, 3, 0.0f);
    make_stranger(&p.items[behind]);
    CHECK(creature_attack_target(&w, &p, f, &s) == -1);
    /* One on the ground of the cell ahead (north, at (3, 1)): struck. */
    int ahead = spawn_winged(&p, &w, 3, 1, 0.0f);
    make_stranger(&p.items[ahead]);
    CHECK(creature_attack_target(&w, &p, f, &s) == ahead);
    /* Turned to face the one beside, that one is ahead now. */
    p.items[f].heading = HEADING_EAST;
    CHECK(creature_attack_target(&w, &p, f, &s) == beside);
    p.items[f].heading = HEADING_NORTH;
    /* One right below as well: below comes first. */
    int below = spawn_winged(&p, &w, 3, 2, 0.0f);
    make_stranger(&p.items[below]);
    CHECK(creature_attack_target(&w, &p, f, &s) == below);
    /* A close relative below (all-zero genes, like the flyer's weights) is not
     * struck, so the reach goes on to the stranger ahead. */
    Genome kin = p.items[f].genome;
    p.items[below].genome = kin;
    CHECK(creature_attack_target(&w, &p, f, &s) == ahead);
    pool_free(&p);
    world_free(&w);
}

/* A dive at the animal ahead lands, and brings the flyer down beside it. */
static void test_dive_at_the_cell_ahead(void)
{
    Settings s = settings_default();
    bool killed = false;
    for (uint64_t seed = 1; seed < 64 && !killed; seed++) {
        World w;
        make_world(&w, 8, 8);
        Pool p;
        CHECK(pool_init(&p, 2));
        Rng r;
        rng_seed(&r, seed);
        int f = spawn_winged(&p, &w, 3, 3, 1.0f);
        Creature *hawk = &p.items[f];
        hawk->genome.g[GENE_LEAF] = 0.0f;
        hawk->genome.g[GENE_MEAT] = 1.0f;
        hawk->genome.g[GENE_SIZE] = 1.0f;
        creature_act(&w, &p, f, act(ACT_MOVE), &s, &r); /* up, at (3, 2) */
        p.items[f].heading = HEADING_NORTH;
        int prey = spawn_winged(&p, &w, 3, 1, 0.0f);
        make_stranger(&p.items[prey]);
        p.items[prey].genome.g[GENE_ARMOUR] = 0.0f;
        p.items[prey].health = 0.001f;
        creature_act(&w, &p, f, act(ACT_BITE), &s, &r);
        if (p.items[prey].health <= 0.0f) {
            killed = true;
            CHECK(p.dive_kills == 1 && p.dive_kills_land == 1);
            const Creature *down = &p.items[f];
            CHECK(down->up == 0);
            int dx = abs(down->x - 3);
            int dy = abs(down->y - 1);
            CHECK(dx <= 1 && dy <= 1 && (dx + dy) > 0);
        }
        pool_free(&p);
        world_free(&w);
    }
    CHECK(killed);
}

/* Up, a flyer's view sight is aloft_sight times its sight on the ground. */
/* Up in the air, the view ahead also takes in the cell right below, where a
 * strike from the air aims first (the livelier hunting design, section 10.1):
 * a walker standing there is seen at the clarity of distance 0. The views to
 * the sides do not count it. A walker on the ground has nothing under it but
 * itself, and does not see itself. */
static void test_aloft_sees_below(void)
{
    Settings s = settings_default();
    World w;
    make_world(&w, 16, 16);
    Pool p;
    CHECK(pool_init(&p, 2));
    p.light = 1.0f;
    Rng r;
    rng_seed(&r, 1);
    int f = spawn_winged(&p, &w, 8, 9, 1.0f);
    creature_act(&w, &p, f, act(ACT_MOVE), &s, &r); /* up, at (8, 8) */
    CHECK(p.items[f].up > 0);
    p.items[f].heading = HEADING_NORTH;
    Senses in = {0};
    creature_sense(&w, &p, f, &s, &in);
    CHECK(in.v[SENSE_VIEWS + VIEW_AHEAD * VIEW_VALUES + VIEW_SEEN] == 0.0f);

    int walker = spawn_winged(&p, &w, 8, 8, 0.0f); /* right below it */
    CHECK(walker >= 0);
    make_stranger(&p.items[walker]);
    p.items[walker].heading = HEADING_NORTH;
    creature_sense(&w, &p, f, &s, &in);
    const float *ahead = &in.v[SENSE_VIEWS + VIEW_AHEAD * VIEW_VALUES];
    float expected = sight_reach(&p.items[walker], world_idx(&w, 8, 8), &w, &s);
    CHECK(expected > 0.0f);
    CHECK(fabsf(ahead[VIEW_SEEN] - expected) < 1e-6f);
    CHECK(ahead[VIEW_RELSIZE] > 0.0f);
    CHECK(in.v[SENSE_VIEWS + VIEW_LEFT * VIEW_VALUES + VIEW_SEEN] == 0.0f);
    CHECK(in.v[SENSE_VIEWS + VIEW_RIGHT * VIEW_VALUES + VIEW_SEEN] == 0.0f);

    /* The walker, with the flyer up and nobody else on the ground, sees no
     * creature in any view: not the flyer (only VIEW_ALOFT tells of it, and
     * only where the flyer is in a view) and not itself. */
    Senses ground = {0};
    creature_sense(&w, &p, walker, &s, &ground);
    for (int view = 0; view < VIEW_COUNT; view++) {
        CHECK(ground.v[SENSE_VIEWS + view * VIEW_VALUES + VIEW_SEEN] == 0.0f);
    }
    pool_free(&p);
    world_free(&w);
}

/* Up, a flyer's view sight is aloft_sight times its sight on the ground. */
static void test_aloft_sees_farther(void)
{
    Settings s = settings_default();
    World w;
    make_world(&w, 16, 16);
    Pool p;
    CHECK(pool_init(&p, 1));
    int f = spawn_winged(&p, &w, 8, 8, 1.0f);
    p.items[f].genome.g[GENE_SIGHT] = 0.5f;
    float down = creature_view_sight(&p.items[f], 1.0f, &s);
    Rng r;
    rng_seed(&r, 1);
    creature_act(&w, &p, f, act(ACT_MOVE), &s, &r);
    CHECK(p.items[f].up == 1);
    float up = creature_view_sight(&p.items[f], 1.0f, &s);
    CHECK(fabsf(up - down * s.aloft_sight) < 1e-5f);
    pool_free(&p);
    world_free(&w);
}

/* Up, a view reads the strongest scent of the first aloft_scent cells ahead;
 * down, only the first. */
static void test_aloft_smells_farther(void)
{
    Settings s = settings_default();
    World w;
    make_world(&w, 16, 16);
    Pool p;
    CHECK(pool_init(&p, 1));
    p.light = 1.0f;
    int f = spawn_winged(&p, &w, 8, 9, 1.0f); /* faces north */
    Rng r;
    rng_seed(&r, 1);
    creature_act(&w, &p, f, act(ACT_MOVE), &s, &r); /* up, at (8, 8) */
    CHECK(p.items[f].up == 1);
    w.scent[world_idx(&w, 8, 8 - s.aloft_scent)] = 1.0f; /* aloft_scent ahead */
    Senses in = {0};
    creature_sense(&w, &p, f, &s, &in);
    CHECK(in.v[SENSE_VIEWS + VIEW_AHEAD * VIEW_VALUES + VIEW_CARRION] > 0.0f);
    p.items[f].up = 0; /* the same place, read as from the ground */
    w.sky[SKY_LOW][world_idx(&w, 8, 8)] = -1;
    w.occupant[world_idx(&w, 8, 8)] = f;
    Senses ground = {0};
    creature_sense(&w, &p, f, &s, &ground);
    CHECK(ground.v[SENSE_VIEWS + VIEW_AHEAD * VIEW_VALUES + VIEW_CARRION] == 0.0f);
    pool_free(&p);
    world_free(&w);
}

/* ---- births and the brake ---- */

/* Two ready flyers on rock, rock all round: their child is a flyer and is born
 * on the rock beside them. */
static void test_flyer_child_born_on_rock(void)
{
    Settings s = settings_default();
    s.birth_per_plant = 0.0f;
    s.birth_per_air = 0.0f;
    s.mutation_rate = 0.0f;
    World w;
    make_world(&w, 8, 8);
    for (int i = 0; i < 64; i++) {
        w.terrain[i] = TERRAIN_ROCK;
    }
    Pool p;
    CHECK(pool_init(&p, 3));
    int a = spawn_winged(&p, &w, 3, 3, 1.0f);
    int b = spawn_winged(&p, &w, 4, 3, 1.0f);
    for (int k = 0; k < 2; k++) {
        Creature *c = &p.items[k == 0 ? a : b];
        c->age = (uint32_t)s.mate_age;
        c->energy = 100.0f;
    }
    Rng r;
    rng_seed(&r, 1);
    creature_act(&w, &p, a, act(ACT_MATE), &s, &r);
    int child = creature_act(&w, &p, b, act(ACT_MATE), &s, &r);
    CHECK(child >= 0);
    if (child >= 0) {
        const Creature *c = &p.items[child];
        CHECK(creature_flyer(c) && c->up == 0 && c->air_line == 0);
        CHECK(w.terrain[world_idx(&w, c->x, c->y)] == TERRAIN_ROCK);
        CHECK(p.air_alive == 1);
    }
    pool_free(&p);
    world_free(&w);
}

/* Walking parents just under the line, with genes that change a lot at birth:
 * a flyer child carries air_line. */
static void test_air_line_set(void)
{
    Settings s = settings_default();
    s.birth_per_plant = 0.0f;
    s.birth_per_air = 0.0f;
    s.mutation_rate = 1.0f;
    s.trait_mutation_scale = 10.0f;
    int flyers = 0, walkers = 0;
    for (uint64_t seed = 1; seed <= 60; seed++) {
        World w;
        make_world(&w, 8, 8);
        Pool p;
        CHECK(pool_init(&p, 3));
        int a = spawn_winged(&p, &w, 3, 3, 0.79f);
        int b = spawn_winged(&p, &w, 4, 3, 0.79f);
        for (int k = 0; k < 2; k++) {
            Creature *c = &p.items[k == 0 ? a : b];
            c->age = (uint32_t)s.mate_age;
            c->energy = 100.0f;
        }
        Rng r;
        rng_seed(&r, seed);
        creature_act(&w, &p, a, act(ACT_MATE), &s, &r);
        int child = creature_act(&w, &p, b, act(ACT_MATE), &s, &r);
        if (child >= 0) {
            const Creature *c = &p.items[child];
            if (creature_flyer(c)) {
                flyers++;
                CHECK(c->air_line == 1);
            } else {
                walkers++;
                CHECK(c->air_line == 0);
            }
        }
        pool_free(&p);
        world_free(&w);
    }
    CHECK(flyers > 0 && walkers > 0);
}

/* A flyers' pair counts flyers against a share of the land's and the sea's
 * room: with no plants and no plankton there is no birth; land bodies do not
 * count against it. */
static void test_air_brake(void)
{
    Settings s = settings_default();
    s.mutation_rate = 0.0f;
    World w;
    make_world(&w, 8, 8);
    Pool p;
    CHECK(pool_init(&p, 3));
    int a = spawn_winged(&p, &w, 3, 3, 1.0f);
    int b = spawn_winged(&p, &w, 4, 3, 1.0f);
    for (int k = 0; k < 2; k++) {
        Creature *c = &p.items[k == 0 ? a : b];
        c->age = (uint32_t)s.mate_age;
        c->energy = 100.0f;
    }
    p.air_alive = 2;
    p.plants = 0;
    p.plankton_rich = 0;
    Rng r;
    rng_seed(&r, 1);
    creature_act(&w, &p, a, act(ACT_MATE), &s, &r);
    CHECK(creature_act(&w, &p, b, act(ACT_MATE), &s, &r) == -1);
    p.plants = 10000; /* room for 0.25 * 0.4 * 10000 = 1000 flyers */
    CHECK(creature_act(&w, &p, b, act(ACT_MATE), &s, &r) >= 0);
    pool_free(&p);
    world_free(&w);
}

/* ---- saves ---- */

/* A world run until some flyers are up saves and loads with them still up
 * over whatever lies below, the same fingerprint, and goes on the same. */
static void test_save_keeps_flyers_up(void)
{
    Settings set = settings_default();
    Sim a, b;
    char err[256] = "";
    CHECK(sim_init(&a, &set, 3, 96, 64));
    int up = 0;
    for (int i = 0; i < 400 && up == 0; i++) {
        sim_step(&a);
        for (int k = 0; k < a.pool.capacity; k++) {
            up += a.pool.items[k].alive && a.pool.items[k].up > 0;
        }
    }
    CHECK(up > 0);
    unsigned char *bytes = NULL;
    size_t len = 0;
    CHECK(save_to_memory(&a, &bytes, &len, err, sizeof(err)));
    CHECK(save_from_memory(&b, bytes, len, err, sizeof(err)));
    free(bytes);
    CHECK(sim_checksum(&a) == sim_checksum(&b));
    for (int i = 0; i < 100; i++) {
        sim_step(&a);
        sim_step(&b);
    }
    CHECK(sim_checksum(&a) == sim_checksum(&b));
    sim_free(&a);
    sim_free(&b);
}

/* ---- the Sandbox's wings, and the notice ---- */

/* A 48 x 16 world of soil with deep water from x 36 to 47. Every creature
 * but the first is taken off; the first stands at (x, 8) with half-wings and a
 * land body. Its id is returned. */
static uint32_t coast_world(Sim *s, int x)
{
    Settings set = settings_default();
    CHECK(sim_init(s, &set, 5, 48, 16));
    World *w = &s->world;
    for (int i = 0; i < 48 * 16; i++) {
        w->terrain[i] = i % 48 >= 36 ? TERRAIN_WATER : TERRAIN_SOIL;
        w->occupant[i] = -1;
        w->sky[SKY_LOW][i] = -1;
    }
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
            c->up = 0;
            c->genome.g[GENE_SWIM] = 0.2f;
            c->genome.g[GENE_WINGS] = 0.2f;
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

static Creature *living(Sim *s, uint32_t id)
{
    for (int i = 0; i < s->pool.capacity; i++) {
        if (s->pool.items[i].alive && s->pool.items[i].id == id) {
            return &s->pool.items[i];
        }
    }
    return NULL;
}

/* Puts the creature up in the air over (x, 8): a flyer's place over water. */
static void lift(Sim *s, Creature *c, int x)
{
    World *w = &s->world;
    int slot = (int)(c - s->pool.items);
    creature_grid(w, c)[world_idx(w, c->x, c->y)] = -1;
    c->x = x;
    c->y = 8;
    c->up = 1;
    w->sky[SKY_LOW][world_idx(w, x, 8)] = slot;
}

/* Wings given make a flyer. A flyer up over the water whose wings are taken
 * comes down on the nearest land; with no land near enough the gift is
 * refused and nothing is recorded. */
static void test_gift_wings(void)
{
    Sim s;
    uint32_t id = coast_world(&s, 20);
    CHECK(actions_give_trait(&s, id, GENE_WINGS, 0.9f));
    sim_step(&s);
    Creature *c = living(&s, id);
    CHECK(c != NULL && creature_flyer(c));
    if (c != NULL) {
        lift(&s, c, 39); /* over the sea, three cells from the shore */
        CHECK(actions_give_trait(&s, id, GENE_WINGS, 0.3f));
        sim_step(&s);
        c = living(&s, id);
        CHECK(c != NULL);
        if (c != NULL) {
            CHECK(!creature_flyer(c) && c->up == 0);
            CHECK(s.world.terrain[world_idx(&s.world, c->x, c->y)] == TERRAIN_SOIL);
            CHECK(s.world.occupant[world_idx(&s.world, c->x, c->y)] == (int)(c - s.pool.items));
        }
    }
    sim_free(&s);

    id = coast_world(&s, 20);
    c = living(&s, id);
    c->genome.g[GENE_WINGS] = 1.0f;
    for (int i = 0; i < 48 * 16; i++) {
        s.world.terrain[i] = i % 48 >= 4 ? TERRAIN_WATER : TERRAIN_SOIL;
    }
    lift(&s, c, 24); /* twenty cells from any land */
    int before = s.actions.count;
    CHECK(!actions_body_fits(&s, id, GENE_WINGS, 0.3f));
    CHECK(!actions_give_trait(&s, id, GENE_WINGS, 0.3f));
    CHECK(s.actions.count == before);
    CHECK(actions_give_trait(&s, id, GENE_WINGS, 0.95f)); /* still a flyer: fine */
    sim_free(&s);
}

/* A living flyer of a line that took to the air is news once; one whose wings
 * the player gave is not. */
static void test_notice_took_to_the_air(void)
{
    Sim s;
    uint32_t id = coast_world(&s, 20);
    Creature *c = living(&s, id);
    c->genome.g[GENE_WINGS] = 1.0f;
    Notices n;
    notices_reset(&n);
    notices_check(&n, &s);
    c->air_line = 1;
    n.last_sample = s.stats.tick + 1;
    notices_check(&n, &s);
    const Notice *x = notices_newest(&n);
    CHECK(x != NULL && strstr(x->text, "took to the air") != NULL);
    CHECK(x != NULL && x->creature == id);
    uint32_t total = n.total;
    n.last_sample = s.stats.tick + 1;
    notices_check(&n, &s);
    CHECK(n.total == total);
    sim_free(&s);

    id = coast_world(&s, 20);
    CHECK(actions_give_trait(&s, id, GENE_WINGS, 1.0f));
    sim_step(&s);
    c = living(&s, id);
    CHECK(c != NULL);
    notices_reset(&n);
    notices_check(&n, &s);
    if (c != NULL) {
        c->air_line = 1;
    }
    n.last_sample = s.stats.tick + 1;
    notices_check(&n, &s);
    CHECK(n.total == 0);
    sim_free(&s);
}

int main(void)
{
    test_flyer_line();
    test_wings_dodge_some_hits();
    test_wings_cost_upkeep();
    test_flyer_may_stand_on_rock();
    test_flyer_takes_off_and_flies_over_water();
    test_walker_never_goes_up();
    test_flight_costs_by_size();
    test_flyer_lands_to_bite_plant();
    test_flyer_lands_on_rock_to_rest();
    test_flyer_cannot_sleep_in_the_air();
    test_flyer_strikes_from_above_and_lands_beside();
    test_flyer_up_cannot_be_struck();
    test_flyer_dies_up_frees_sky_and_drops_carrion();
    test_aloft_senses();
    test_dive_hits_harder();
    test_dive_kill_counted();
    test_air_strike_reaches_ahead();
    test_dive_at_the_cell_ahead();
    test_aloft_sees_below();
    test_aloft_sees_farther();
    test_aloft_smells_farther();
    test_flyer_child_born_on_rock();
    test_air_line_set();
    test_air_brake();
    test_save_keeps_flyers_up();
    test_gift_wings();
    test_notice_took_to_the_air();
    TEST_SUMMARY();
}
