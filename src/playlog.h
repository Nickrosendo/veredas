/*
 * playlog: a record of one sitting in the window, for playtests: how long it
 * lasted, how long in each mode, how soon Sandbox was first opened, and how
 * often each key and click was used. Written to a plain text file on leaving
 * (--playlog PATH), one "name value" pair a line, so a few players' files can
 * be read side by side or added up with a short script. Nothing is sent
 * anywhere; nothing about the world is in it.
 */
#ifndef VEREDAS_PLAYLOG_H
#define VEREDAS_PLAYLOG_H

#include <stdbool.h>
#include <stddef.h> /* size_t */
#include <stdint.h>

/* Keys with a character (below 128) are counted one by one; arrows, Escape
 * and the rest together as "other". */
enum { PLAYLOG_KEYS = 128 };

/* The Sandbox verbs a playtest watches for: the things a player does to a
 * world, as against looking at it. A new start from the setup screen is not
 * one of them (it begins another world rather than changing this one). */
enum {
    PLAYLOG_VERB_TRAIT,   /* g, then Enter: a trait given */
    PLAYLOG_VERB_WEATHER, /* w and a digit: a weather event started */
    PLAYLOG_VERB_SKIP,    /* k and a digit: a skip started */
    PLAYLOG_VERB_SPLIT,   /* x: the world split in two */
    PLAYLOG_VERBS
};

typedef struct {
    bool started;
    uint64_t start_ms;   /* the first frame */
    uint64_t last_ms;    /* the frame before this one */
    bool sandbox;        /* the mode at the last frame */
    uint64_t ms_in[2];   /* time in Documentary [0] and Sandbox [1] */
    int64_t first_sandbox_ms; /* after the start; -1 while never */
    uint32_t keys[PLAYLOG_KEYS];
    uint32_t other_keys;
    /* Clicks on buttons, by the key each stands for, kept apart from the keys
     * themselves: the next playtest shows which buttons were found. */
    uint32_t buttons[PLAYLOG_KEYS];
    uint32_t other_buttons;
    uint32_t clicks;
    /* Each verb: how often it was done, and when first, in ms after the
     * start (-1 while never). */
    uint32_t verbs[PLAYLOG_VERBS];
    int64_t first_verb_ms[PLAYLOG_VERBS];
} Playlog;

/* Empty: nothing counted yet. */
void playlog_init(Playlog *p);
/* Called once a frame with the clock and the mode then: the time since the
 * last frame goes to the mode the window was in. */
void playlog_frame(Playlog *p, uint64_t now_ms, bool sandbox);
/* One key pressed (a KEY_ value or a character), one click on the world. */
void playlog_key(Playlog *p, int key);
void playlog_click(Playlog *p);
/* One click on a button standing for key; a button with no key below 128 (a
 * close button, say) counts as "other". */
void playlog_button(Playlog *p, int key);
/* One verb done (a PLAYLOG_VERB_ value) at the clock's now_ms. Done before the
 * first frame, it counts as at the start. */
void playlog_verb(Playlog *p, int verb, uint64_t now_ms);
/* Writes the record to path, replacing the file, with version (the program's,
 * VEREDAS_VERSION) as its second line, so a tester's file says which build
 * made it. False with one line in err when the file cannot be written. */
bool playlog_write(const Playlog *p, const char *path, const char *version,
                   char *err, size_t len);

#endif
