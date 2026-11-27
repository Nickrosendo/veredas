#include "playlog.h"

#include <errno.h>  /* errno */
#include <stdio.h>  /* fopen, fprintf, fclose, snprintf */
#include <string.h> /* strerror */

void playlog_init(Playlog *p)
{
    *p = (Playlog){.first_sandbox_ms = -1, .first_follow_ms = -1};
    /* A designated initializer can set one array element ([0] = -1) but not
     * all of them; a loop sets the rest. */
    for (int v = 0; v < PLAYLOG_VERBS; v++) {
        p->first_verb_ms[v] = -1;
    }
    for (int l = 0; l < PLAYLOG_LEVELS; l++) {
        p->first_level_ms[l] = -1;
    }
}

/* The milliseconds since the first frame, or 0 before it (or for a clock that
 * went back). */
static int64_t since_start(const Playlog *p, uint64_t now_ms)
{
    return p->started && now_ms > p->start_ms ? (int64_t)(now_ms - p->start_ms) : 0;
}

void playlog_frame(Playlog *p, uint64_t now_ms, const PlaylogView *view)
{
    bool sandbox = view->sandbox;
    if (!p->started) {
        p->started = true;
        p->start_ms = now_ms;
        p->last_ms = now_ms;
        p->sandbox = sandbox;
        p->view = *view; /* a struct copies whole, like the number it holds */
    }
    /* A clock that went back (it should not) counts as no time, rather than
     * as an unsigned wrap round to billions of milliseconds. */
    uint64_t step = now_ms > p->last_ms ? now_ms - p->last_ms : 0;
    /* The time since the last frame was spent in the mode it was then. A
     * bool used as an index is 0 or 1. */
    p->ms_in[p->sandbox] += step;
    if (sandbox && p->first_sandbox_ms < 0) {
        p->first_sandbox_ms = (int64_t)(now_ms - p->start_ms);
    }
    p->sandbox = sandbox;
    /* The same step to the rest of the last frame's view. The tests on the
     * index: a value from outside would write past the array's end, and C
     * would not stop it. */
    const PlaylogView *was = &p->view;
    if (was->zoom >= 0 && was->zoom < PLAYLOG_ZOOMS) {
        p->ms_zoom[was->zoom] += step;
    }
    if (was->level >= 0 && was->level < PLAYLOG_LEVELS) {
        p->ms_level[was->level] += step;
    }
    if (was->following) {
        p->ms_following += step;
    }
    if (was->timed) {
        p->timed_frames++;
        p->timed_ms += step;
        if (step > PLAYLOG_LATE_MS) {
            p->late_frames++;
        }
    }
    /* The level shown from this frame on: its first moment, once. */
    int level = view->level;
    if (level >= 0 && level < PLAYLOG_LEVELS && p->first_level_ms[level] < 0) {
        p->first_level_ms[level] = since_start(p, now_ms);
    }
    p->view = *view;
    p->last_ms = now_ms;
}

void playlog_follow(Playlog *p, uint64_t now_ms)
{
    p->follows++;
    if (p->first_follow_ms < 0) {
        p->first_follow_ms = since_start(p, now_ms);
    }
}

void playlog_machine(Playlog *p, int window_w, int window_h, int cpu_cores)
{
    p->window_w = window_w;
    p->window_h = window_h;
    p->cpu_cores = cpu_cores;
}

void playlog_key(Playlog *p, int key)
{
    if (key > 0 && key < PLAYLOG_KEYS) {
        p->keys[key]++;
    } else {
        p->other_keys++;
    }
}

void playlog_click(Playlog *p)
{
    p->clicks++;
}

void playlog_button(Playlog *p, int key)
{
    if (key > 0 && key < PLAYLOG_KEYS) {
        p->buttons[key]++;
    } else {
        p->other_buttons++;
    }
}

void playlog_verb(Playlog *p, int verb, uint64_t now_ms)
{
    /* A verb number from outside the table would write past its end: C does
     * not check an array index, so this function does. */
    if (verb < 0 || verb >= PLAYLOG_VERBS) {
        return;
    }
    p->verbs[verb]++;
    if (p->first_verb_ms[verb] < 0) {
        p->first_verb_ms[verb] = since_start(p, now_ms);
    }
}

/* The verbs' names in the file, in the order of the PLAYLOG_VERB_ values. */
static const char *const VERB_NAMES[PLAYLOG_VERBS] = {"trait", "weather", "skip",
                                                      "split"};

/* The names of the zoom steps and levels in the file, in their enums' order;
 * NULL for a level not built yet, which gets no line. */
static const char *const ZOOM_NAMES[PLAYLOG_ZOOMS] = {"close", "normal", "far",
                                                      "world"};
static const char *const LEVEL_NAMES[PLAYLOG_LEVELS] = {NULL, "sky", "surface",
                                                        "underwater", NULL};

/*
 * Whole seconds for each of n times that together make total_ms, so that they
 * add up to total_ms's own whole seconds: each is cut down to its whole
 * seconds, and the seconds that cutting lost go one each to the times that
 * lost the most (on a tie, the first in the table). Cut one by one, 10.4 s
 * and 7.6 s would be 10 + 7 = 17 against a total of 18.
 */
static void share_seconds(const uint64_t *ms, int n, uint64_t total_ms,
                          unsigned long long *out)
{
    uint64_t whole = 0;
    bool given[PLAYLOG_LEVELS] = {false}; /* the larger of the two tables */
    for (int i = 0; i < n; i++) {
        out[i] = (unsigned long long)(ms[i] / 1000u);
        whole += ms[i] / 1000u;
    }
    /* left: the seconds still owed. A clock that went back can make the
     * times add up to less than the total; then nothing is owed. */
    uint64_t want = total_ms / 1000u;
    uint64_t left = want > whole ? want - whole : 0;
    for (; left > 0; left--) {
        int best = -1;
        for (int i = 0; i < n; i++) {
            if (!given[i] && (best < 0 || ms[i] % 1000u > ms[best] % 1000u)) {
                best = i;
            }
        }
        if (best < 0) {
            break; /* every time has had one already */
        }
        given[best] = true;
        out[best]++;
    }
}

/* Whole seconds of a time that may be -1 (never), as the file writes it. */
static long long seconds_or_never(int64_t ms)
{
    return ms < 0 ? -1LL : (long long)(ms / 1000);
}

/* A key's name in the file: the character itself, or a word for a key that
 * is blank or would make the line hard to read. */
static void key_name(int key, char *out, size_t size)
{
    switch (key) {
    case ' ': snprintf(out, size, "space"); break;
    case '\t': snprintf(out, size, "tab"); break;
    case '\r': snprintf(out, size, "enter"); break;
    case '\b': snprintf(out, size, "backspace"); break;
    default:
        if (key > ' ' && key < 127) {
            snprintf(out, size, "%c", key);
        } else {
            snprintf(out, size, "code%d", key);
        }
    }
}

bool playlog_write(const Playlog *p, const char *path, const char *version,
                   char *err, size_t len)
{
    FILE *f = fopen(path, "w");
    if (f == NULL) {
        /* errno: the reason fopen failed, kept by the C library in a global
         * number; strerror turns it into words. Read at once, before any other
         * call can change it. */
        snprintf(err, len, "cannot write the playlog %s: %s", path, strerror(errno));
        return false;
    }
    uint64_t total = p->last_ms - p->start_ms;
    /* 2: the version line and the verb lines were added (2026-11-16).
     * 3: the view, following and frame speed lines at the end (2026-11-27). */
    fprintf(f, "veredas-playlog 3\n");
    fprintf(f, "version %s\n", version);
    /* Seconds, whole: %llu with a cast, since uint64_t has no letter of its
     * own in printf (inttypes.h has PRIu64 for it; the cast is simpler). */
    fprintf(f, "seconds %llu\n", (unsigned long long)(total / 1000u));
    fprintf(f, "seconds_documentary %llu\n", (unsigned long long)(p->ms_in[0] / 1000u));
    fprintf(f, "seconds_sandbox %llu\n", (unsigned long long)(p->ms_in[1] / 1000u));
    fprintf(f, "first_sandbox_second %lld\n", seconds_or_never(p->first_sandbox_ms));
    /* The earliest of the verbs: the playtest asks whether any was found. */
    int64_t first_verb = -1;
    for (int v = 0; v < PLAYLOG_VERBS; v++) {
        int64_t at = p->first_verb_ms[v];
        if (at >= 0 && (first_verb < 0 || at < first_verb)) {
            first_verb = at;
        }
    }
    fprintf(f, "first_verb_second %lld\n", seconds_or_never(first_verb));
    for (int v = 0; v < PLAYLOG_VERBS; v++) {
        fprintf(f, "verb %s %u %lld\n", VERB_NAMES[v], (unsigned)p->verbs[v],
                seconds_or_never(p->first_verb_ms[v]));
    }
    fprintf(f, "clicks %u\n", (unsigned)p->clicks);
    for (int k = 0; k < PLAYLOG_KEYS; k++) {
        if (p->keys[k] > 0) {
            char name[16];
            key_name(k, name, sizeof(name));
            fprintf(f, "key %s %u\n", name, (unsigned)p->keys[k]);
        }
    }
    fprintf(f, "key other %u\n", (unsigned)p->other_keys);
    for (int k = 0; k < PLAYLOG_KEYS; k++) {
        if (p->buttons[k] > 0) {
            char name[16];
            key_name(k, name, sizeof(name));
            fprintf(f, "button %s %u\n", name, (unsigned)p->buttons[k]);
        }
    }
    fprintf(f, "button other %u\n", (unsigned)p->other_buttons);
    /* Format 3: after every format 2 line, so a reader that skips the lines
     * it does not know reads the old ones as before. */
    unsigned long long secs[PLAYLOG_LEVELS]; /* the larger table again */
    share_seconds(p->ms_zoom, PLAYLOG_ZOOMS, total, secs);
    for (int z = 0; z < PLAYLOG_ZOOMS; z++) {
        fprintf(f, "seconds_zoom %s %llu\n", ZOOM_NAMES[z], secs[z]);
    }
    share_seconds(p->ms_level, PLAYLOG_LEVELS, total, secs);
    for (int l = 0; l < PLAYLOG_LEVELS; l++) {
        if (LEVEL_NAMES[l] != NULL) {
            fprintf(f, "seconds_level %s %llu\n", LEVEL_NAMES[l], secs[l]);
        }
    }
    fprintf(f, "first_underwater_second %lld\n",
            seconds_or_never(p->first_level_ms[PLAYLOG_LEVEL_UNDERWATER]));
    fprintf(f, "first_sky_second %lld\n",
            seconds_or_never(p->first_level_ms[PLAYLOG_LEVEL_SKY]));
    fprintf(f, "first_follow_second %lld\n", seconds_or_never(p->first_follow_ms));
    fprintf(f, "seconds_following %llu\n", (unsigned long long)(p->ms_following / 1000u));
    fprintf(f, "follows %u\n", (unsigned)p->follows);
    /* One decimal; 0.0 with no timed frame rather than a division by zero,
     * which for doubles gives "nan", not a crash, but reads as nothing. */
    double mean = 0.0, late = 0.0;
    if (p->timed_frames > 0) {
        mean = (double)p->timed_ms / (double)p->timed_frames;
    }
    if (p->timed_ms > 0) {
        late = (double)p->late_frames * 1000.0 / (double)p->timed_ms;
    }
    fprintf(f, "frame_ms_mean %.1f\n", mean);
    fprintf(f, "late_frames_per_second %.1f\n", late);
    fprintf(f, "window %d %d\n", p->window_w, p->window_h);
    fprintf(f, "cpu_cores %d\n", p->cpu_cores);
    /* fclose writes what the library still holds in its buffer; a full disk
     * shows up here, not at fprintf. */
    if (fclose(f) != 0) {
        snprintf(err, len, "cannot finish the playlog %s", path);
        return false;
    }
    return true;
}
