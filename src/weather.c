#include "weather.h"

#include <stdio.h> /* snprintf */

#include "years.h" /* years_season */

/* How hard each event pushes. Chosen so each moves a number that can be
 * measured within a year (the design's section 11); tuned by probe on
 * 2026-11-13. */
#define DROUGHT_PLANT_GAIN 0.3f  /* plants gain under a third as much */
#define DROUGHT_UPKEEP 1.5f      /* and pay half again as much to live */
#define DROUGHT_RAIN_GAIN 0.3f   /* soil takes under a third of the rain */
#define DROUGHT_SEED_FIT 0.15f   /* a seed needs a better place to take */
#define DROUGHT_SPROUT 0.5f      /* and bare cells beside plants sprout half as often */
#define WINTER_COLD_RISE 0.2f    /* temperature counted as cold rises by this */
#define WINTER_COLD_COST 3.0f    /* and the cold costs three times as much */
#define WINTER_PLANT_GAIN 0.6f   /* plants grow slower in the cold */
#define WARM_COLD_DROP 0.2f
#define WARM_COLD_COST 0.3f
#define WARM_PLANT_GAIN 1.3f

static float at_most_one(float v)
{
    return v > 1.0f ? 1.0f : v;
}

bool weather_active(const Weather *w, uint64_t tick)
{
    return w->kind != WEATHER_NONE && tick < w->until;
}

Settings weather_settings(const Settings *base, const Weather *w, uint64_t tick)
{
    Settings s = *base; /* a whole copy: changing s cannot change *base */
    if (!weather_active(w, tick)) {
        return s;
    }
    switch (w->kind) {
    case WEATHER_DROUGHT:
        s.plant_gain *= DROUGHT_PLANT_GAIN;
        s.plant_upkeep *= DROUGHT_UPKEEP;
        s.rain_gain *= DROUGHT_RAIN_GAIN;
        s.plant_seed_fit = at_most_one(s.plant_seed_fit + DROUGHT_SEED_FIT);
        s.veg_sprout_rate *= DROUGHT_SPROUT;
        break;
    case WEATHER_HARD_WINTER:
        s.region_cold_below += WINTER_COLD_RISE;
        s.cold_upkeep *= WINTER_COLD_COST;
        s.plant_gain *= WINTER_PLANT_GAIN;
        break;
    case WEATHER_WARM_YEAR:
        s.region_cold_below -= WARM_COLD_DROP;
        s.cold_upkeep *= WARM_COLD_COST;
        s.plant_gain *= WARM_PLANT_GAIN;
        break;
    default: break;
    }
    return s;
}

const char *weather_name(int kind)
{
    switch (kind) {
    case WEATHER_DROUGHT: return "drought";
    case WEATHER_HARD_WINTER: return "hard winter";
    case WEATHER_WARM_YEAR: return "warm year";
    default: return "clear";
    }
}

float weather_left(const Weather *w, uint64_t tick, uint64_t length)
{
    if (length == 0 || !weather_active(w, tick)) {
        return 0.0f;
    }
    /* Active means tick < until, so until - tick is at least 1 and the
     * unsigned subtraction cannot wrap round. */
    uint64_t left = w->until - tick;
    if (left >= length) {
        return 1.0f;
    }
    return (float)left / (float)length;
}

void weather_label(const Weather *w, uint64_t tick, int ticks_per_year, char *out,
                   size_t size)
{
    if (size == 0) {
        return;
    }
    if (!weather_active(w, tick)) {
        out[0] = '\0';
        return;
    }
    /* The last tick it holds is until - 1; until is above tick, so above 0. */
    snprintf(out, size, "%s, ends in %s", weather_name(w->kind),
             years_season(w->until - 1, ticks_per_year));
}
