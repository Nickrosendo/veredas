/*
 * doing: one short line on what a creature is doing and why, for the card in
 * the window and the inspect panel in the terminal.
 *
 * Read only from what the creature already holds: its current action, its
 * sleep, its wish to mate, its energy, its remembered meal, the plants on and
 * around its cell and the light. Nothing new is kept for it, so the
 * simulation is exactly as it was (the same fingerprint).
 */
#ifndef AGON_DOING_H
#define AGON_DOING_H

#include <stddef.h>

#include "creature.h"
#include "sim.h"

/* The longest line doing_text writes, with its '\0': "fighting " and a name
 * of up to 15 letters fit with room to spare. */
#define DOING_LEN 48

/* Writes the line for c, a living creature in s, into out (size bytes,
 * always ended with '\0'). The first case that fits wins: hibernating,
 * asleep for the night, napping, fighting or hunting someone, eating,
 * looking for a mate, walking to food, looking around, wandering, resting
 * (beside food, not hungry; not hungry; plain). */
void doing_text(const Sim *s, const Creature *c, char *out, size_t size);

#endif
