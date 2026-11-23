#include "fish.h"

#include <string.h> /* memcpy */

/* The four sides, for the drift: fish swim to a side neighbour, never
 * diagonally, so a shoal's edge moves one cell at a time. */
static const int SIDE_X[4] = {0, 1, 0, -1};
static const int SIDE_Y[4] = {-1, 0, 1, 0};

void fish_start(World *w, const Settings *s)
{
    int n = w->w * w->h;
    for (int i = 0; i < n; i++) {
        /* plankton_cap is 0 off deep water, and fish_start_rich is checked to
         * be above 0, so land never gets fish here. */
        bool rich = w->terrain[i] == TERRAIN_WATER &&
                    w->plankton_cap[i] >= s->fish_start_rich;
        w->fish[i] = rich ? s->fish_cap_share * w->plankton_cap[i] : 0.0f;
    }
}

bool fish_scares(const Creature *c, uint32_t now, const Settings *s)
{
    /* fish_fear_ticks is an int checked to be 0 or more, so the cast is safe. */
    return c->alive && c->fish_fed && now - c->fish_meal_at <= (uint32_t)s->fish_fear_ticks;
}

/* Growth on every deep cell: the fish move fish_growth of the way to the cap
 * the cell's plankton sets, up or down, so fish thin where plankton thins (in
 * winter too). What they grow they eat from the plankton, fish_graze of it. */
static void grow(World *w, const Settings *s)
{
    int n = w->w * w->h;
    for (int i = 0; i < n; i++) {
        if (w->terrain[i] != TERRAIN_WATER) {
            continue;
        }
        float cap = s->fish_cap_share * w->plankton[i];
        float grown = s->fish_growth * (cap - w->fish[i]);
        w->fish[i] += grown;
        if (grown > 0.0f) {
            float left = w->plankton[i] - s->fish_graze * grown;
            w->plankton[i] = left > 0.0f ? left : 0.0f;
        }
    }
}

/* Marks the cells whose fish are scared: each living creature that ate fish
 * lately marks its own cell and the eight around with its own cell number, so
 * the drift knows which way is away. Where two eaters' marks meet, the later
 * slot's wins: either way is away from an eater. */
static void mark_scared(World *w, const Pool *p, const Settings *s)
{
    int n = w->w * w->h;
    for (int i = 0; i < n; i++) {
        w->fish_scare[i] = -1;
    }
    for (int k = 0; k < p->capacity; k++) {
        const Creature *c = &p->items[k];
        if (!fish_scares(c, p->now, s)) {
            continue;
        }
        int own = world_idx(w, c->x, c->y);
        for (int dy = -1; dy <= 1; dy++) {
            for (int dx = -1; dx <= 1; dx++) {
                w->fish_scare[world_idx(w, c->x + dx, c->y + dy)] = own;
            }
        }
    }
}

/* The squared distance between two cells across the wrapping map: the shorter
 * way round on each axis. */
static int wrapped_distance2(const World *w, int a, int b)
{
    int dx = a % w->w - b % w->w;
    int dy = a / w->w - b / w->w;
    dx = dx < 0 ? -dx : dx;
    dy = dy < 0 ? -dy : dy;
    if (dx > w->w - dx) {
        dx = w->w - dx;
    }
    if (dy > w->h - dy) {
        dy = w->h - dy;
    }
    return dx * dx + dy * dy;
}

/* Where cell i's fish go this update, and how big a share: the deep side
 * neighbour farthest from the eater marking it, fish_flee of them; else the
 * deep side neighbour with the most plankton, if richer than i, fish_drift of
 * them. -1 for staying put. Ties go to the first side in SIDE_ order. */
static int drift_target(const World *w, int i, const Settings *s, float *share)
{
    int x = i % w->w, y = i / w->w;
    int eater = w->fish_scare[i];
    int best = -1;
    int far = eater >= 0 ? wrapped_distance2(w, i, eater) : 0;
    float rich = w->plankton[i];
    for (int k = 0; k < 4; k++) {
        int next = world_idx(w, x + SIDE_X[k], y + SIDE_Y[k]);
        if (w->terrain[next] != TERRAIN_WATER) {
            continue;
        }
        if (eater >= 0) {
            int d = wrapped_distance2(w, next, eater);
            if (d > far) {
                far = d;
                best = next;
            }
        } else if (w->plankton[next] > rich) {
            rich = w->plankton[next];
            best = next;
        }
    }
    *share = eater >= 0 ? s->fish_flee : s->fish_drift;
    return best;
}

/* Every cell's move is worked out from the fish as they were before any moved
 * (read from fish, written into fish_next), so the result does not depend on
 * the order the cells are visited in. */
static void drift(World *w, const Settings *s)
{
    int n = w->w * w->h;
    /* memcpy copies raw bytes: a float array copied this way is the same floats. */
    memcpy(w->fish_next, w->fish, (size_t)n * sizeof(float));
    for (int i = 0; i < n; i++) {
        if (w->fish[i] <= 0.0f) {
            continue; /* off deep water fish is always 0 */
        }
        float share;
        int to = drift_target(w, i, s, &share);
        if (to < 0) {
            continue;
        }
        float moved = share * w->fish[i];
        w->fish_next[i] -= moved;
        w->fish_next[to] += moved;
    }
    for (int i = 0; i < n; i++) {
        float f = w->fish_next[i];
        /* A cell can receive from up to four sides: kept within 0..1. */
        w->fish[i] = f > 1.0f ? 1.0f : (f < 0.0f ? 0.0f : f);
    }
}

void fish_step(World *w, const Pool *p, const Settings *s)
{
    grow(w, s);
    mark_scared(w, p, s);
    drift(w, s);
}

int fish_count_cells(const World *w, float at_least)
{
    int n = w->w * w->h;
    int count = 0;
    for (int i = 0; i < n; i++) {
        count += w->fish[i] >= at_least;
    }
    return count;
}
