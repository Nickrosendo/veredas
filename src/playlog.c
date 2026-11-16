#include "playlog.h"

#include <errno.h>  /* errno */
#include <stdio.h>  /* fopen, fprintf, fclose, snprintf */
#include <string.h> /* strerror */

void playlog_init(Playlog *p)
{
    *p = (Playlog){.first_sandbox_ms = -1};
    /* A designated initializer can set one array element ([0] = -1) but not
     * all of them; a loop sets the rest. */
    for (int v = 0; v < PLAYLOG_VERBS; v++) {
        p->first_verb_ms[v] = -1;
    }
}

void playlog_frame(Playlog *p, uint64_t now_ms, bool sandbox)
{
    if (!p->started) {
        p->started = true;
        p->start_ms = now_ms;
        p->last_ms = now_ms;
        p->sandbox = sandbox;
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
    p->last_ms = now_ms;
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
        uint64_t at = p->started && now_ms > p->start_ms ? now_ms - p->start_ms : 0;
        p->first_verb_ms[verb] = (int64_t)at;
    }
}

/* The verbs' names in the file, in the order of the PLAYLOG_VERB_ values. */
static const char *const VERB_NAMES[PLAYLOG_VERBS] = {"trait", "weather", "skip",
                                                      "split"};

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
    /* 2: the version line and the verb lines were added (2026-11-16). */
    fprintf(f, "veredas-playlog 2\n");
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
    /* fclose writes what the library still holds in its buffer; a full disk
     * shows up here, not at fprintf. */
    if (fclose(f) != 0) {
        snprintf(err, len, "cannot finish the playlog %s", path);
        return false;
    }
    return true;
}
