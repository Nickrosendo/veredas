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

/* The zoom steps and levels, in the camera's order (camera.h, levels.h; the
 * window checks at compile time that they agree). Repeated here so the
 * playlog, like the rest of the engine's plain files, needs nothing from the
 * drawing code. */
enum { PLAYLOG_ZOOM_CLOSE, PLAYLOG_ZOOM_NORMAL, PLAYLOG_ZOOM_FAR, PLAYLOG_ZOOM_WORLD,
       PLAYLOG_ZOOMS };
enum { PLAYLOG_LEVEL_HIGH_SKY, PLAYLOG_LEVEL_SKY, PLAYLOG_LEVEL_SURFACE,
       PLAYLOG_LEVEL_UNDERWATER, PLAYLOG_LEVEL_ABYSS, PLAYLOG_LEVELS };

/* A frame later than this, in ms from the one before, reads as a stutter
 * (the window's tick budget keeps under it, window_main.c FRAME_TARGET_US). */
#define PLAYLOG_LATE_MS 40u

/* What the window showed at a frame, as the playlog needs it. */
typedef struct {
    bool sandbox;   /* the mode */
    int zoom;       /* a PLAYLOG_ZOOM_ value */
    int level;      /* a PLAYLOG_LEVEL_ value */
    bool following; /* the view is on a followed animal, whoever picked it */
    /* The frame counts for the frame speed: after the prelude, not paused.
     * The loading ring's frames and the cheap paused ones would hide or fake
     * stutter. */
    bool timed;
} PlaylogView;

typedef struct {
    bool started;
    uint64_t start_ms;   /* the first frame */
    uint64_t last_ms;    /* the frame before this one */
    bool sandbox;        /* the mode at the last frame */
    PlaylogView view;    /* the whole view at the last frame */
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
    /* Format 3. Time at each zoom and level; when each level was first shown
     * (-1 while never). */
    uint64_t ms_zoom[PLAYLOG_ZOOMS];
    uint64_t ms_level[PLAYLOG_LEVELS];
    int64_t first_level_ms[PLAYLOG_LEVELS];
    /* Follows the player started, the first one's moment (-1 while never),
     * and the time the view spent on a followed animal. */
    uint32_t follows;
    int64_t first_follow_ms;
    uint64_t ms_following;
    /* The gaps between timed frames: how many, their sum, and how many were
     * over PLAYLOG_LATE_MS. */
    uint32_t timed_frames;
    uint64_t timed_ms;
    uint32_t late_frames;
    /* The machine, as playlog_machine was last told (0 while never). */
    int window_w, window_h;
    int cpu_cores;
} Playlog;

/* Empty: nothing counted yet. */
void playlog_init(Playlog *p);
/* Called once a frame with the clock and what the window shows then: the time
 * since the last frame goes to the mode, zoom, level and following the window
 * had at the last frame, and to the frame speed if that frame was timed. A
 * zoom or level outside the tables gets no time. */
void playlog_frame(Playlog *p, uint64_t now_ms, const PlaylogView *view);
/* The player started following an animal (a click on it, j, a notice, a
 * panel line), at the clock's now_ms. Not for the camera's own picks. */
void playlog_follow(Playlog *p, uint64_t now_ms);
/* The window's size in pixels and the computer's logical core count, for the
 * file's last lines. */
void playlog_machine(Playlog *p, int window_w, int window_h, int cpu_cores);
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
