#include "plankton.h"

#include <stdlib.h> /* malloc, free */

#include "years.h" /* SEASON_WINTER */

/* The distance of every cell from the nearest cell that is not deep water,
 * in steps across the four sides, written into dist (n ints). A breadth-first
 * search: every non-deep cell starts at 0 in a queue, and each cell taken from
 * the queue gives its unvisited deep neighbours its distance plus one. Each
 * cell enters the queue once, so the whole map costs one pass. With no shore
 * at all (a world all of deep water) every cell keeps the -1 it started with.
 * False when the queue cannot be allocated. */
static bool shore_distances(const World *w, int *dist)
{
    int n = w->w * w->h;
    /* The queue holds cell numbers; head is the next to take, tail where the
     * next goes. Every cell is pushed at most once, so n slots are enough. */
    int *queue = malloc((size_t)n * sizeof(int));
    if (queue == NULL) {
        return false;
    }
    int head = 0, tail = 0;
    for (int i = 0; i < n; i++) {
        if (w->terrain[i] == TERRAIN_WATER) {
            dist[i] = -1;
        } else {
            dist[i] = 0;
            queue[tail++] = i;
        }
    }
    static const int DX[4] = {1, -1, 0, 0};
    static const int DY[4] = {0, 0, 1, -1};
    while (head < tail) {
        int cell = queue[head++];
        int x = cell % w->w, y = cell / w->w;
        for (int k = 0; k < 4; k++) {
            int next = world_idx(w, x + DX[k], y + DY[k]);
            if (dist[next] < 0) {
                dist[next] = dist[cell] + 1;
                queue[tail++] = next;
            }
        }
    }
    free(queue);
    return true;
}

void plankton_prepare(World *w, const Settings *s)
{
    int n = w->w * w->h;
    int *dist = malloc((size_t)n * sizeof(int));
    bool known = dist != NULL && shore_distances(w, dist);
    for (int i = 0; i < n; i++) {
        if (w->terrain[i] != TERRAIN_WATER) {
            w->plankton_cap[i] = 0.0f;
            continue;
        }
        /* Without the distances (out of memory) every cell counts as by the
         * shore: a richer sea, but a working one. The cast makes the division
         * a float one. */
        float away = known && dist[i] > 0 ? (float)dist[i] : 0.0f;
        float warm = s->plankton_cold + (1.0f - s->plankton_cold) * w->temperature[i];
        w->plankton_cap[i] = warm / (1.0f + away / s->plankton_shore);
    }
    free(dist); /* free(NULL) does nothing */
}

void plankton_start(World *w)
{
    int n = w->w * w->h;
    for (int i = 0; i < n; i++) {
        w->plankton[i] = w->plankton_cap[i];
    }
}

void plankton_step(World *w, const Settings *s, int season)
{
    float share = season == SEASON_WINTER ? s->plankton_winter : 1.0f;
    int n = w->w * w->h;
    for (int i = 0; i < n; i++) {
        /* Off deep water the cap is 0 and so is the plankton: nothing moves. */
        float target = w->plankton_cap[i] * share;
        w->plankton[i] += s->plankton_growth * (target - w->plankton[i]);
    }
}

int plankton_count_rich(const World *w)
{
    int n = w->w * w->h;
    int rich = 0;
    for (int i = 0; i < n; i++) {
        /* A bool used as a number is 0 or 1. */
        rich += w->plankton[i] >= PLANKTON_RICH;
    }
    return rich;
}

int plankton_steps(const World *w, int i)
{
    /* Converting a float to int drops the fraction (rounds towards zero):
     * 0.6 / 0.25 = 2.4 gives 2 whole steps. The plankton is never negative. */
    return (int)(w->plankton[i] / PLANKTON_STEP);
}
