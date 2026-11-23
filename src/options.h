/*
 * options: the command line. Parses argv into an Options struct and
 * checks every value; prints nothing itself, so tests can run it. main
 * decides what to print and which exit code to return.
 */
#ifndef VEREDAS_OPTIONS_H
#define VEREDAS_OPTIONS_H

#include <stdbool.h>
#include <stddef.h> /* size_t */
#include <stdint.h>

/* The largest world width or height --size accepts. */
#define OPTIONS_MAX_SIDE 4096
/* --once runs this many ticks unless --ticks says otherwise. */
#define OPTIONS_DEFAULT_TICKS 2000
#define OPTIONS_MAX_TICKS 100000000L
/* The range --speed accepts, in ticks per second. */
#define OPTIONS_MAX_SPEED 1000
/* The most --ancestors accepts. */
#define OPTIONS_MAX_ANCESTORS 1000
/* The most --frames accepts. */
#define OPTIONS_MAX_FRAMES 100000000L

typedef struct {
    bool seed_given;
    uint64_t seed;
    bool size_given;
    int width, height; /* world cells */
    int speed;         /* ticks per second; 0 when not given */
    bool once;
    long ticks;        /* how far --once runs */
    bool no_floor;
    bool help, version;
    bool list_settings;      /* --settings */
    bool checksum;           /* --checksum: print the world's on leaving */
    /* --ancestors: years a new world evolves unseen before it opens; -1 when
     * not given, and the setting ancestor_years decides. */
    int ancestors;
    /* For the window program only; the terminal program refuses them. */
    long frames;             /* --frames: stop after this many; 0 for never */
    const char *screenshot_path; /* --screenshot: the last frame goes here */
    const char *zoom_name;   /* --zoom: the starting zoom step's name */
    /* --window: the window's starting size in pixels, read like --size; both
     * 0 when not given, and the window program uses its own default. */
    int window_w, window_h;
    /* --until: after --frames, go on until this moment ("pounce", "giant")
     * and stop on it. Checked by the window program. Points into argv. */
    const char *until_name;
    /* --record: every frame of a --frames run goes into this file (or pipe)
     * as a PPM picture, one after another, for a video tool to read. Points
     * into argv. */
    const char *record_path;
    /* --after: with --until, frames to go on for once the moment has come,
     * the camera following it, so a recorded clip shows what came next; 0
     * stops on the moment, as without it. */
    long after;
    bool paused;             /* --paused: start as if space were pressed */
    bool no_art;             /* --no-art: start with the drawn frames off (a key) */
    bool sandbox;            /* --sandbox: the window starts in Sandbox mode (m) */
    /* --code: a world code, or a link holding one, that the window opens at
     * the start instead of a new world. Points into argv. */
    const char *code;
    /* --playlog: a file the window writes a record of the sitting to on
     * leaving (playlog.h). Points into argv. */
    const char *playlog_path;
    /* --frame-log: a file the window writes each frame's times to, one line
     * a frame (frame_log.h), for finding stutter. Points into argv. */
    const char *frame_log_path;
    /* --keys: keys pressed one a frame during the last frames of --frames, so
     * a test or a screenshot can open the L page. Points into argv. */
    const char *keys;
    /* --hits: on leaving, print every button of the last frame drawn, one a
     * line, so a test can find a page's close button. */
    bool hits;
    /* Paths from the command line, NULL when not given. They point into
     * argv, which lives as long as main does; nothing is copied or freed. */
    const char *config_path;
    const char *save_path;
    const char *load_path;
} Options;

/* Parses "WxH", two whole numbers from 1 to OPTIONS_MAX_SIDE joined by
 * an 'x', with nothing before, between or after. False for anything
 * else; *w and *h are then unchanged. */
bool parse_size(const char *text, int *w, int *h);

/* Parses a whole number ("123") or 0x hexadecimal ("0x7b") that fits a
 * uint64_t. No sign, no spaces, nothing after it. */
bool parse_seed(const char *text, uint64_t *seed);

/* Parses a decimal whole number from min to max, nothing before or
 * after it but an optional '-' sign. */
bool parse_long(const char *text, long min, long max, long *out);

/* Fills *o from the command line. False on any mistake, with a one-line
 * message (no newline) in err, a buffer of err_size bytes. argv's
 * entries may be reordered (getopt_long moves options before other
 * arguments); the strings themselves are not changed. */
bool options_parse(int argc, char **argv, Options *o, char *err,
                   size_t err_size);

/* The text --help prints. */
extern const char OPTIONS_USAGE[];

#endif
