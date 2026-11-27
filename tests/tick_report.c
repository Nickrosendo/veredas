/*
 * tick_report: loads a saved world, runs it some ticks, and prints how long
 * one tick took (the median, the mean and the slowest) and one number that
 * stands for the world after every tick (each tick's sim_checksum and
 * sim_story_checksum folded together). A measuring tool, built by
 * "make ticks" and never part of "make test": two builds that print the same
 * last number ran the same world, tick for tick. Runs must be serial: two at
 * once share the memory bus and both come out slow.
 *
 *   tick_report world.save [ticks] [--no-checksums]
 *
 * --no-checksums leaves the checksums out, for a profiler: they read the
 * whole world and would take most of its samples.
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h> /* clock_gettime */

#include "save.h"
#include "sim.h"

static double now_seconds(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec + (double)t.tv_nsec * 1e-9;
}

static int by_value(const void *a, const void *b)
{
    double x = *(const double *)a, y = *(const double *)b;
    return (x > y) - (x < y);
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: tick_report world.save [ticks] [--no-checksums]\n");
        return 2;
    }
    long ticks = argc > 2 ? strtol(argv[2], NULL, 10) : 300;
    bool sums = !(argc > 3 && strcmp(argv[3], "--no-checksums") == 0);
    if (ticks < 1) {
        ticks = 1;
    }
    static Sim sim; /* too large for the stack */
    char err[256];
    if (!save_read(&sim, argv[1], err, sizeof err)) {
        fprintf(stderr, "tick_report: %s\n", err);
        return 1;
    }
    double *took = malloc((size_t)ticks * sizeof *took);
    if (took == NULL) {
        return 1;
    }
    int animals_start = sim.pool.count;
    uint64_t fold = 0;
    long ran = 0;
    for (; ran < ticks && !sim_is_over(&sim); ran++) {
        double start = now_seconds();
        sim_step(&sim);
        took[ran] = now_seconds() - start;
        /* Folding outside the timed part keeps the checksums' own cost out
         * of the tick's time. */
        if (sums) {
            fold = fold * 1099511628211ULL ^ sim_checksum(&sim);
            fold = fold * 1099511628211ULL ^ sim_story_checksum(&sim);
        }
    }
    double sum = 0;
    for (long i = 0; i < ran; i++) {
        sum += took[i];
    }
    qsort(took, (size_t)ran, sizeof *took, by_value);
    printf("%ld ticks, animals %d -> %d: median %.2f ms, mean %.2f ms, "
           "slowest %.2f ms; checksums %016llx\n",
           ran, animals_start, sim.pool.count,
           ran ? took[ran / 2] * 1e3 : 0.0, ran ? sum / (double)ran * 1e3 : 0.0,
           ran ? took[ran - 1] * 1e3 : 0.0, (unsigned long long)fold);
    free(took);
    sim_free(&sim);
    return 0;
}
