#include "actions.h"

#include <math.h>   /* fabsf */
#include <stdlib.h> /* realloc, free */

#include "genome.h"
#include "sim.h"

const int ACTIONS_GIVABLE[] = {
    GENE_SIZE, GENE_ARMOUR, GENE_WEAPON, GENE_SWIM,
    GENE_LEGS, GENE_NIGHT_EYES, GENE_LEARN,
};
/* A check made by the compiler, not at run time: if someone adds a trait to
 * the list and forgets the count in actions.h, the build stops here. */
_Static_assert(sizeof(ACTIONS_GIVABLE) / sizeof(ACTIONS_GIVABLE[0]) ==
                   ACTIONS_GIVABLE_COUNT,
               "ACTIONS_GIVABLE_COUNT must match the list");

bool actions_add(ActionLog *log, PlayerAction a)
{
    if (log->count > 0 && a.tick < log->items[log->count - 1].tick) {
        return false;
    }
    if (log->count == log->capacity) {
        /* Doubling keeps the copying cheap overall: n entries cost fewer than
         * 2n copies in all. The first time capacity is 0, so start at 16. */
        int bigger = log->capacity == 0 ? 16 : log->capacity * 2;
        /* realloc into a new pointer first: on failure it returns NULL and
         * leaves the old block as it was, and writing NULL straight into
         * log->items would lose the only pointer to that block (a leak). */
        PlayerAction *grown =
            realloc(log->items, (size_t)bigger * sizeof(*grown));
        if (grown == NULL) {
            return false;
        }
        log->items = grown;
        log->capacity = bigger;
    }
    log->items[log->count] = a; /* a struct assignment copies every field */
    log->count++;
    return true;
}

void actions_clear(ActionLog *log)
{
    log->count = 0;
    log->next = 0;
}

void actions_free(ActionLog *log)
{
    free(log->items);
    *log = (ActionLog){0};
}

void actions_seek(ActionLog *log, uint64_t tick)
{
    log->next = 0;
    while (log->next < log->count && log->items[log->next].tick < tick) {
        log->next++;
    }
}

/* The slot of the living creature with this id, or -1. A walk over the whole
 * pool: actions are rare, so a faster index would not pay for itself. */
static int find_living(const Pool *p, uint32_t id)
{
    for (int i = 0; i < p->capacity; i++) {
        if (p->items[i].alive && p->items[i].id == id) {
            return i;
        }
    }
    return -1;
}

/* Where c's body would be after a gift that makes it body: its own cell when
 * it may stay (a flyer up keeps flying; on the ground, ground the new body can
 * use), else the nearest free cell that body can use within GIFT_MOVE_RADIUS;
 * -1 for none. */
static int gift_cell(const World *w, const Creature *c, BodyKind body)
{
    int own = world_idx(w, c->x, c->y);
    if (c->up > 0 && body.flyer) {
        return own;
    }
    if (c->up == 0 && world_ground_for(w, own, body.sea, body.flyer)) {
        return own;
    }
    return creature_nearest_ground(w, c->x, c->y, body, GIFT_MOVE_RADIUS);
}

static void apply_trait(Sim *s, const PlayerAction *a)
{
    int idx = find_living(&s->pool, a->creature);
    if (idx < 0 || a->gene >= GENE_TRAITS) {
        return;
    }
    Creature *c = &s->pool.items[idx];
    float old = c->genome.g[a->gene];
    c->genome.g[a->gene] = a->value;
    /* A swim or wings gift may move the body across a line: it goes to the
     * nearest ground its new body can use, and a flyer that loses its wings
     * up in the air comes down there. actions_give_trait checked there was
     * some; should the cells have filled since, the gift does nothing. */
    int from = world_idx(&s->world, c->x, c->y);
    int to = gift_cell(&s->world, c, creature_body_type(c));
    if (to < 0) {
        c->genome.g[a->gene] = old;
        return;
    }
    if (to != from || (c->up > 0 && !creature_flyer(c))) {
        creature_grid(&s->world, c)[from] = -1;
        c->up = 0;
        s->world.occupant[to] = idx;
        c->x = to % s->world.w;
        c->y = to / s->world.w;
    }
    /* A smaller or less armoured body has a lower most health: keep the
     * health it has within the new limit, as a newborn's is. */
    float most = creature_max_health(c, &s->set);
    if (c->health > most) {
        c->health = most;
    }
    /* The kinship cache assumes a genome never changes after birth, so what
     * it remembers about this creature is now wrong. Forgetting all of it
     * costs some speed for a moment and changes no result. */
    pool_forget_kinship(&s->pool);
}

const char *actions_gene_name(int gene)
{
    switch (gene) {
    case GENE_SIZE: return "size";
    case GENE_ARMOUR: return "armour";
    case GENE_WEAPON: return "horn";
    case GENE_SWIM: return "swimming";
    case GENE_LEGS: return "long legs";
    case GENE_NIGHT_EYES: return "night eyes";
    case GENE_LEARN: return "learning";
    case GENE_WINGS: return "wings";
    default: return "?";
    }
}

/* The slider's traits, and wings, which are given only whole (the w key). */
static bool givable(int gene)
{
    if (gene == GENE_WINGS) {
        return true;
    }
    for (int i = 0; i < ACTIONS_GIVABLE_COUNT; i++) {
        if (ACTIONS_GIVABLE[i] == gene) {
            return true;
        }
    }
    return false;
}

bool actions_body_fits(const Sim *s, uint32_t creature, int gene, float value)
{
    int idx = find_living(&s->pool, creature);
    if (idx < 0) {
        return false;
    }
    const Creature *c = &s->pool.items[idx];
    BodyKind body = creature_body_type(c);
    if (gene == GENE_SWIM) {
        body.sea = value >= SEA_FROM;
    } else if (gene == GENE_WINGS) {
        body.flyer = value >= AIR_FROM;
    }
    return gift_cell(&s->world, c, body) >= 0;
}

bool actions_give_trait(Sim *s, uint32_t creature, int gene, float value)
{
    /* Written as "not inside" so that NaN, for which every comparison is
     * false, is refused too. */
    int idx = find_living(&s->pool, creature);
    if (!givable(gene) || !(value >= 0.0f && value <= 1.0f) || idx < 0) {
        return false;
    }
    if (!actions_body_fits(s, creature, gene, value)) {
        return false;
    }
    PlayerAction a = {.tick = s->tick, .kind = ACTION_TRAIT,
                      .gene = (uint8_t)gene, .creature = creature,
                      .value = value};
    return actions_add(&s->actions, a);
}

bool actions_weather(Sim *s, int kind)
{
    if (kind <= WEATHER_NONE || kind >= WEATHER_COUNT) {
        return false;
    }
    PlayerAction a = {.tick = s->tick, .kind = ACTION_WEATHER,
                      .weather = (uint8_t)kind};
    return actions_add(&s->actions, a);
}

/* One season from now, replacing any event under way. */
static void apply_weather(Sim *s, const PlayerAction *a)
{
    if (a->weather <= WEATHER_NONE || a->weather >= WEATHER_COUNT) {
        return;
    }
    uint64_t season = (uint64_t)s->set.ticks_per_year / 4u;
    s->weather = (Weather){.kind = a->weather, .until = s->tick + season};
}

void actions_apply_due(Sim *s)
{
    ActionLog *log = &s->actions;
    while (log->next < log->count && log->items[log->next].tick <= s->tick) {
        const PlayerAction *a = &log->items[log->next];
        /* An entry for a tick already gone (only a damaged log has one) is
         * skipped rather than applied late. */
        if (a->tick == s->tick && a->kind == ACTION_TRAIT) {
            apply_trait(s, a);
        } else if (a->tick == s->tick && a->kind == ACTION_WEATHER) {
            apply_weather(s, a);
        }
        log->next++;
    }
}

int actions_carriers(const Sim *s, int gene, float value)
{
    int n = 0;
    for (int i = 0; i < s->pool.capacity; i++) {
        const Creature *c = &s->pool.items[i];
        /* A bool used as a number is 0 or 1, so this counts the matches. */
        n += c->alive && fabsf(c->genome.g[gene] - value) < ACTIONS_CARRIER_TOLERANCE;
    }
    return n;
}

bool actions_gift_repeats(const ActionLog *log, int i)
{
    const PlayerAction *a = &log->items[i];
    for (int j = 0; j < i; j++) {
        const PlayerAction *b = &log->items[j];
        if (b->kind == ACTION_TRAIT && b->gene == a->gene &&
            b->value == a->value) {
            return true;
        }
    }
    return false;
}
