/*
 * camera_picks: the Documentary camera's picks, without a window, for the
 * hunting verdict's fifth bar (the livelier hunting design, section 6). A
 * world is run headless to tick FROM; then, for WATCH ticks, the camera picks
 * by the window's own rule (src/window/window_main.c, update_look): a pick
 * when nobody is followed or the followed creature has died, the look-ahead
 * copy run to its end first (as the window does for --frames runs), the
 * kinds of the last two picks marking what it is tired of. The player never
 * touches anything, so the camera is never held off by a key.
 *
 * Prints a PICK line per pick (tick, reason number, the reason's words) and a
 * last line: PICKS total N sea_sky N, sea_sky counting the picks for a dive
 * or a hunt at sea. A measuring tool, built by "make camera", never part of
 * "make test".
 */
#include <stdio.h>
#include <stdlib.h> /* strtol */
#include <string.h> /* strcmp */

#include "lifepage.h" /* life_find_slot */
#include "lookahead.h"
#include "settings.h"
#include "sim.h"

static int usage(void)
{
    fprintf(stderr, "usage: camera_picks --seed N --from TICK --watch TICKS "
                    "[--config PATH]\n");
    return 2;
}

int main(int argc, char **argv)
{
    long seed = 1, from = 12000, watch = 1200;
    const char *config = NULL;
    for (int i = 1; i + 1 < argc; i += 2) {
        if (strcmp(argv[i], "--seed") == 0) {
            seed = strtol(argv[i + 1], NULL, 10);
        } else if (strcmp(argv[i], "--from") == 0) {
            from = strtol(argv[i + 1], NULL, 10);
        } else if (strcmp(argv[i], "--watch") == 0) {
            watch = strtol(argv[i + 1], NULL, 10);
        } else if (strcmp(argv[i], "--config") == 0) {
            config = argv[i + 1];
        } else {
            return usage();
        }
    }
    if (argc % 2 == 0 || seed < 0 || from < 0 || watch < 0) {
        return usage();
    }
    Settings set = settings_default();
    char err[256];
    if (config != NULL && !settings_load(&set, config, err, sizeof(err))) {
        fprintf(stderr, "%s\n", err);
        return 1;
    }
    Sim sim;
    /* The window's own world size on a desktop. */
    if (!sim_init(&sim, &set, (uint64_t)seed, 256, 144)) {
        fprintf(stderr, "could not make the world\n");
        return 1;
    }
    for (long t = 0; t < from && !sim_is_over(&sim); t++) {
        sim_step(&sim);
    }
    Lookahead look;
    lookahead_init(&look, (uint64_t)(sim.set.ticks_per_year / 4));
    uint32_t followed = LOOKAHEAD_NOBODY;
    int last[2] = {LOOK_QUIET, LOOK_QUIET};
    int total = 0, sea_sky = 0;
    for (long t = 0; t < watch && !sim_is_over(&sim); t++) {
        sim_step(&sim);
        lookahead_sync_all(&look, &sim);
        if (followed != LOOKAHEAD_NOBODY && life_find_slot(&sim, followed) >= 0) {
            continue; /* still following someone alive */
        }
        unsigned tired = (1u << lookahead_kind(last[0])) | (1u << lookahead_kind(last[1]));
        int reason;
        char why[LOOKAHEAD_WHY];
        uint32_t id = lookahead_pick(&look, &sim, tired, &reason, why, sizeof(why));
        if (id == LOOKAHEAD_NOBODY) {
            followed = LOOKAHEAD_NOBODY;
            continue;
        }
        followed = id;
        last[1] = last[0];
        last[0] = reason;
        total++;
        sea_sky += reason == LOOK_DIVE || reason == LOOK_SEA_HUNT;
        printf("PICK %llu %d %s\n", (unsigned long long)sim.tick, reason,
               lookahead_reason_text(reason));
    }
    printf("PICKS total %d sea_sky %d\n", total, sea_sky);
    lookahead_free(&look);
    sim_free(&sim);
    return 0;
}
