/*
 * speed_report: runs one world headless and prints how long each creature's
 * tick took on average, in seconds per million creature-ticks. A measuring
 * tool, built by "make speed" and never part of "make test". The design's
 * speed bar compares this number, on 10 headless years of seed 1, between
 * the code before a phase and after it. Runs must be serial: two at once
 * share the memory bus and both come out slow.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h> /* clock_gettime */

#include "settings.h"
#include "sim.h"

/* Seconds since some fixed moment, as a double. CLOCK_MONOTONIC never jumps
 * when the system clock is set, which a wall-clock time could. */
static double now_seconds(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec + (double)t.tv_nsec * 1e-9;
}

int main(int argc, char **argv)
{
    long seed = 1;
    long years = 10;
    for (int i = 1; i + 1 < argc; i += 2) {
        if (strcmp(argv[i], "--seed") == 0) {
            seed = strtol(argv[i + 1], NULL, 10);
        } else if (strcmp(argv[i], "--years") == 0) {
            years = strtol(argv[i + 1], NULL, 10);
        }
    }
    Settings set = settings_default();
    static Sim sim; /* too large for the stack */
    if (!sim_init(&sim, &set, (uint64_t)seed, 256, 144)) {
        fprintf(stderr, "speed_report: could not build the world\n");
        return 1;
    }
    long ticks = years * (long)set.ticks_per_year;
    uint64_t creature_ticks = 0;
    double start = now_seconds();
    for (long t = 0; t < ticks && !sim_is_over(&sim); t++) {
        creature_ticks += (uint64_t)sim.pool.count;
        sim_step(&sim);
    }
    double took = now_seconds() - start;
    printf("seed %ld years %ld: %.2f s, %llu creature-ticks, %.3f s per M\n",
           seed, years, took, (unsigned long long)creature_ticks,
           took / ((double)creature_ticks / 1e6));
    sim_free(&sim);
    return 0;
}
