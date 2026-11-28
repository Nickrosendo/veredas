/*
 * The three views walk a fan of lines (creature.c). A Sim's pool keeps one fan
 * for all its creatures (pool_cache_views); a pool made by hand has none, and
 * each creature works out its own. Both must give every creature exactly the
 * same senses, bit for bit: the cache may only change how fast a view is
 * walked, never what it sees. Checked on a grown world, for every creature
 * awake, with the number of lines changed under the cache (so it is made
 * again) and with a sight long enough to walk past the steps the fan keeps.
 */
#include "test.h"

#include <string.h> /* memcmp */

#include "brain.h"
#include "creature.h"
#include "settings.h"
#include "sim.h"

/* How many creatures saw the same with the pool's fan as with their own. */
static int same_both_ways(const Sim *s, const Settings *set)
{
    /* A copy of the pool's fields, the same slots and caches, but with no fan
     * cache: each look then makes its own fan. The struct is copied, not
     * what it points to, so both read the same creatures. */
    Pool own = s->pool;
    own.fan_cache = NULL;
    int same = 0, looked = 0;
    for (int i = 0; i < s->pool.capacity; i++) {
        if (!s->pool.items[i].alive) {
            continue;
        }
        Senses cached, plain;
        creature_sense(&s->world, &s->pool, i, set, &cached);
        creature_sense(&s->world, &own, i, set, &plain);
        looked++;
        /* memcmp: every byte, so even a float that differs in its last bit
         * counts as different. */
        if (memcmp(cached.v, plain.v, sizeof(cached.v)) == 0) {
            same++;
        }
    }
    CHECK(looked > 0);
    CHECK(same == looked);
    return same;
}

int main(void)
{
    Settings set = settings_default();
    Sim s;
    CHECK(sim_init(&s, &set, 4, 96, 54));
    CHECK(s.pool.fan_cache != NULL);
    for (int t = 0; t < 400; t++) {
        sim_step(&s);
    }
    CHECK(same_both_ways(&s, &s.set) > 50);

    /* Other numbers of lines: the pool's fan is made again for each. */
    const int lines[] = {1, 2, VIEW_LINES_MAX, 7};
    for (size_t k = 0; k < sizeof(lines) / sizeof(lines[0]); k++) {
        Settings other = s.set;
        other.view_lines = lines[k];
        (void)same_both_ways(&s, &other);
    }

    /* A long sight: views reach past the steps the fan keeps worked out. */
    Settings far = s.set;
    far.sight_max = 40;
    (void)same_both_ways(&s, &far);

    sim_free(&s);
    TEST_SUMMARY();
}
