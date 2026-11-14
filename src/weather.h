/*
 * weather: events a player sets off in Sandbox, each lasting one season (the
 * player agency design, 6.3). An event does not add new rules: for its season
 * the world runs on a copy of the settings with a few numbers pushed, so every
 * rule the world already has carries its effect.
 */
#ifndef AGON_WEATHER_H
#define AGON_WEATHER_H

#include <stdbool.h>
#include <stddef.h> /* size_t */
#include <stdint.h>

#include "settings.h"

enum {
    WEATHER_NONE,
    WEATHER_DROUGHT,     /* plants grow less, soil takes less rain, fewer seeds take */
    WEATHER_HARD_WINTER, /* colder: more ground counts as cold, and cold costs more */
    WEATHER_WARM_YEAR,   /* milder: less cold ground, cold costs less, plants grow more */
    WEATHER_COUNT
};

/* The event under way: kind, and the tick it ends at (it holds while the tick
 * is below until). Part of the world's state: saved and fingerprinted. */
typedef struct {
    uint8_t kind;
    uint64_t until;
} Weather;

/* The settings the world runs on at tick: base itself (a copy of it) with no
 * event or one that has ended, else base with the event's numbers pushed.
 * Returned by value: a Settings is a few hundred bytes, cheap to copy once a
 * tick, and the caller's base is never touched. */
Settings weather_settings(const Settings *base, const Weather *w, uint64_t tick);
/* Whether an event holds at tick. */
bool weather_active(const Weather *w, uint64_t tick);
/* How much of the event is left at tick, 1 at its start down to 0 at its end;
 * 0 with none, one that has ended, or a length of 0. length is the event's
 * length in ticks (a season: ticks_per_year / 4, as actions.c sets it). */
float weather_left(const Weather *w, uint64_t tick, uint64_t length);
/* "hard winter, ends in summer": the event's name and the season its last tick
 * falls in. "" with none or one that has ended. Always terminated within size. */
void weather_label(const Weather *w, uint64_t tick, int ticks_per_year, char *out,
                   size_t size);
/* "drought", "hard winter", "warm year"; "clear" for none. */
const char *weather_name(int kind);

#endif
