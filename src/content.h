/*
 * content: what an optional content pack adds to the engine. The public build
 * has none (content_none.c); the paid build is the same engine with a private
 * pack compiled in by `make CONTENT=../veredas-content`, whose own file
 * defines content_get instead. Exactly one definition is linked, chosen by the
 * Makefile, so the engine never names anything private and needs no call at
 * start-up to learn what the pack holds.
 */
#ifndef VEREDAS_CONTENT_H
#define VEREDAS_CONTENT_H

#include <stdbool.h>

/* The longest pack name a save can record, in bytes, not counting the '\0'. */
#define CONTENT_NAME_MAX 31

/* art.h's type. Naming the struct here without its fields (an incomplete
 * type) is enough to hold pointers to it, so this core header does not pull
 * in the drawing side's headers. */
struct ArtSet;

typedef struct {
    /* The pack's name, as a world that needs it records it: "" for none. */
    const char *name;
    /* Drawn frames that take the place of the built-in ones for their species
     * (art.c looks here first): art_set_count of them, or NULL and 0. */
    const struct ArtSet *art_sets;
    int art_set_count;
} Content;

/* The pack this program was built with. Never NULL. */
const Content *content_get(void);

/* Whether this build can open a world that needs the pack called `needs`:
 * true for "" (a world that needs none) and for this build's own pack. */
bool content_has(const char *needs);

#endif
