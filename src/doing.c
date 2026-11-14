#include "doing.h"

#include <stdbool.h>
#include <stdio.h> /* snprintf */

#include "brain.h"     /* ACT_ */
#include "chronicle.h" /* name_from_id */

/* Light below this is dark (years.h: a third of every day). The pool's light
 * is the one the Sim set for this tick, the light every creature sees by. */
#define DARK_BELOW 0.25f

/* Whether a plant grows on c's cell or one of the eight around it.
 * world_idx wraps round the map's edges, as the world itself does. */
static bool food_beside(const World *w, const Creature *c)
{
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            if (w->veg[world_idx(w, c->x + dx, c->y + dy)] > 0) {
                return true;
            }
        }
    }
    return false;
}

/* What an attack is aimed at: "hunting Ralulus" for a meat-eater, "fighting
 * Ralulus" for anyone else, or the plain word when nobody stands ahead. */
static void attack_text(const Sim *s, const Creature *c, char *out, size_t size)
{
    /* The slot is c's place in the pool's array. Subtracting two pointers into
     * the same array counts the elements between them (not the bytes): a
     * ptrdiff_t, a signed whole number, here 0 or more. */
    int idx = (int)(c - s->pool.items);
    int target = creature_attack_target(&s->world, &s->pool, idx, &s->set);
    const char *verb = creature_is_hunter(c) ? "hunting" : "fighting";
    if (target < 0) {
        snprintf(out, size, "%s", verb);
        return;
    }
    char name[16];
    name_from_id(s->pool.items[target].id, name, sizeof(name));
    snprintf(out, size, "%s %s", verb, name);
}

/* Resting: why, as far as its state tells. "Not hungry" is energy above half
 * of what mating needs, the same measure its own brain senses. */
static const char *rest_text(const Sim *s, const Creature *c)
{
    bool fed = c->energy > 0.5f * creature_mate_needs(c, &s->set);
    if (fed && food_beside(&s->world, c)) {
        return "resting beside food: not hungry";
    }
    return fed ? "resting: not hungry" : "resting";
}

void doing_text(const Sim *s, const Creature *c, char *out, size_t size)
{
    const char *text = "resting";
    switch (c->current.kind) {
    case ACT_SLEEP:
        if (c->hibernating) {
            text = "hibernating";
        } else if (s->pool.light < DARK_BELOW) {
            text = "asleep for the night";
        } else {
            text = "napping";
        }
        break;
    case ACT_ATTACK:
        attack_text(s, c, out, size);
        return; /* the line is written already */
    case ACT_BITE:
        text = "eating";
        break;
    case ACT_MATE:
        text = "looking for a mate";
        break;
    case ACT_MOVE:
        if (c->mate_wish > 0) {
            text = "looking for a mate";
        } else {
            text = c->meal_known ? "walking to food" : "wandering";
        }
        break;
    case ACT_TURN_LEFT:
    case ACT_TURN_RIGHT:
        text = "looking around";
        break;
    default: /* ACT_REST, and any kind a brain should never give */
        text = rest_text(s, c);
        break;
    }
    snprintf(out, size, "%s", text);
}
