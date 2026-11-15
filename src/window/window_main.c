/*
 * veredas-window: the simulation in a window. One function, app_frame, does
 * one frame; platform_run calls it. There is no loop here, because a browser
 * has to own the loop.
 */
#include <inttypes.h> /* PRIx64 */
#include <stdio.h>    /* snprintf, printf, fputs */
#include <limits.h>   /* INT_MAX */
#include <stdlib.h>   /* abs, free */
#include <string.h>   /* strcmp */
#include <time.h>     /* time */

#include "app.h"
#include "bars.h"
#include "branch.h"
#include "camera.h"
#include "canvas.h"
#include "card.h"
#include "cells.h"
#include "cells_canvas.h"
#include "family.h"
#include "font.h"
#include "inspect.h"
#include "lab.h"
#include "lifepage.h"
#include "lookahead.h"
#include "menus.h"
#include "notices.h"
#include "options.h"
#include "palette.h"
#include "panel.h" /* panel_format_count */
#include "platform.h"
#include "rng.h"
#include "sandbox.h"
#include "compare.h"
#include "keyhelp.h"
#include "playlog.h"
#include "worldcode.h"
#include "save.h"
#include "scene.h"
#include "settings.h"
#include "sim.h"
#include "ui.h"
#include "weather_fx.h"
#include "years.h"

#define FRAMES_PER_SECOND 30
#define WINDOW_W 1280
#define WINDOW_H 720
/* A press that moves further than this, in pixels, is a drag, not a click. */
#define CLICK_SLOP 4
#ifdef VEREDAS_DEMO
/* The browser demo's smaller map: its peak population (1,400-2,300 over 15
 * years, seeds 1-4) stays well inside the demo's pool of 3,000, where the full
 * map's (3,000-4,400) would not. */
#define DEFAULT_WORLD_W 192
#define DEFAULT_WORLD_H 108
#else
#define DEFAULT_WORLD_W 256
#define DEFAULT_WORLD_H 144
#endif
#define NOTICE_FRAMES (2 * FRAMES_PER_SECOND)
/* A notice (notices.h) stays longer, so there is time to press j. */
#define RARE_NOTICE_FRAMES (10 * FRAMES_PER_SECOND)
/* How many notices the e list shows, newest first: one per number key. */
#define NOTICE_LIST_ROWS 9
/* How many screen pixels an arrow key pans by. */
#define ARROW_PAN 48
/* How long the look-ahead camera waits, with nobody followed and no key or
 * mouse touched, before it picks someone: a player who let go or panned is
 * looking at something, and the camera should not pull them away at once. */
#define LOOK_IDLE_FRAMES (10 * FRAMES_PER_SECOND)

/* The first-minute hints, one bit each in App.hints: after the ancestors,
 * after the first click on a creature, with the first notice. */
enum {
    HINT_START = 1u << 0,
    HINT_FOLLOW = 1u << 1,
    HINT_NOTICES = 1u << 2,
};

static const char WINDOW_USAGE[] =
    "usage: veredas-window [options]\n"
    "  --seed N          the world's seed\n"
    "  --size WxH        world size in cells (default 256x144)\n"
    "  --speed N         ticks per second (default 2)\n"
    "  --no-floor        turn off a floor a settings file set\n"
    "  --config PATH     read settings from a file\n"
    "  --save PATH       write the world there on quit and on the s key\n"
    "  --load PATH       resume a saved world\n"
    "  --zoom NAME       start at close, normal, far or world\n"
    "  --checksum        print a fingerprint of the world on leaving\n"
    "  --frames N        stop after N frames, without waiting between them\n"
    "  --screenshot PATH write the last frame there as a PPM image\n"
    "  --paused          start paused, as if space were pressed first\n"
    "  --no-art          start with the animals drawn from the skeleton, not their\n"
    "                    drawn frames (the a key switches)\n"
    "  --sandbox         start in Sandbox mode, with the tools (the m key switches)\n"
    "  --code TEXT       open this world code (or a link holding one) and replay\n"
    "                    it up to where it was copied, instead of a new world\n"
    "  --playlog PATH    on leaving, write a record of the sitting there: time in\n"
    "                    each mode and how often each key was used (for playtests;\n"
    "                    nothing about the world, and nothing is sent anywhere)\n"
    "  --keys TEXT       with --frames: press these keys, one a frame, in the\n"
    "                    last frames (\"l\" shows the life story page, \"b\" the\n"
    "                    species lab, \"~\" presses nothing for a frame)\n"
    "  --hits            on leaving, print where the last frame drew each button\n"
    "                    and the key it presses (for tests)\n"
    "  -h, --help        this text\n"
    "  --version         the version\n"
    "keys: space pause, +/- speed, r new seed, s save, q quit, arrows move,\n"
    "      [ and ] zoom, Esc lets go of a creature, n new world after game over,\n"
    "      l opens the life story of the followed creature (or of the most\n"
    "      notable life) and closes it again, j follows who the newest notice\n"
    "      is about (or, with the story open, the creature it tells of).\n"
    "      e lists the notices (rare bodies, giants, tactics first seen, big\n"
    "      families gone, a given trait spreading or gone); 1 to 9 or a\n"
    "      click on one follows who it is about.\n"
    "      b opens the species lab, every animal side by side, and closes it\n"
    "      again; in it 1 to 6 choose the action shown (walk, run, rest,\n"
    "      graze, bite, sleep), and p puts every painted part (plates, a\n"
    "      horn, a fin) on its species.\n"
    "      a switches the animals between their drawn frames and the skeleton.\n"
    "      f switches the camera that looks a season ahead and, when nobody is\n"
    "      followed for 10 seconds or the one followed dies, follows whoever\n"
    "      has the most coming (on in Documentary, off in Sandbox).\n"
    "      m switches between Documentary (watching) and Sandbox. In Sandbox,\n"
    "      g gives the followed creature a trait (1 to 7 pick it, , and . set\n"
    "      its value, Enter gives), w starts a weather event for a season and\n"
    "      k skips 1, 5 or 20 years ahead (Esc stops), then says what happened.\n"
    "      x splits the world in two from now, Tab shows the other branch,\n"
    "      and x twice keeps the one shown. A skip moves both branches.\n"
    "      v compares the two branches side by side (and opens by itself\n"
    "      when a skip ends while split).\n"
    "      u opens a new start: how wildly bodies change from parent to young\n"
    "      and which traits may jump; Enter starts the world again with them.\n"
    "      c copies the world's code (seed, settings and everything done to\n"
    "      it) to the clipboard, in either mode; o opens a code from the\n"
    "      clipboard and replays it up to where it was copied.\n"
    "mouse: wheel zooms, dragging moves, clicking a creature follows it,\n"
    "       clicking bare ground lets go. On the panel, clicking \"top hunter\",\n"
    "       \"oldest\" or the largest family's body follows that creature.\n"
    "       Every button (the bar along the bottom, the boxes along the top,\n"
    "       the menus and the card) presses a key, which its hover label names.\n";

/* Everything a frame needs, passed through platform_run as a void *. */
typedef struct {
    Sim sim;
    Scene scene;
    Families families;
    Canvas canvas;
    Camera camera;
    Layout layout;
    Inspect inspect;
    LifePage page;     /* the L page, drawn over the world while open */
    bool lab_open;     /* the species lab (b), drawn over the world while open */
    int lab_action;    /* the ACTION_ value it shows */
    bool lab_parts;    /* p: the lab shows every painted part on its species */
    /* Rare things as they happen (notices.h): for the world shown, and for
     * the other branch while split. The screen's, not the world's. */
    Notices notices, other_notices;
    bool notices_open; /* the e list, drawn over the world */
    const Options *opt;
    Rng seed_source;
    int speed;
    int ticks_due; /* ticks owed, in thirtieths, as in the terminal program */
    bool paused, running, failed;
    bool redraw;    /* while paused: something happened that would show */
    int last_ambient; /* the scenery's step in the last frame drawn, paused or running */
    bool pressed;  /* the left button went down over the world */
    bool dragging; /* and has moved far enough not to be a click */
    int press_x, press_y;
    int mouse_x, mouse_y;
    long frames_done;
    int notice_frames;
    char notice[256];
    uint64_t body_ms;  /* a clock that stops while paused */
    uint64_t last_ms;
    /* A new world's ancestors evolving, shown fast-forwarded (app.h), and how
     * many years each new world gets. */
    Prelude prelude;
    int prelude_years;
    /* The mode, Documentary or Sandbox, and the Sandbox menus (sandbox.h).
     * The screen's, not the world's: never saved. */
    Sandbox sandbox;
    /* A world split in two (branch.h): a->sim is always the one on screen,
     * branches.other the one behind it. keep_asked: x was pressed once while
     * split, and a second x keeps the world shown. */
    Branches branches;
    bool keep_asked;
    /* The look-ahead camera (lookahead.h): a copy a season ahead, and whether
     * the camera may pick who to follow. On by default in Documentary, off in
     * Sandbox; f switches it. idle_frames counts frames with no key or mouse;
     * look_now asks for a pick at once (the followed creature died). */
    Lookahead look;
    bool look_on;
    bool look_now;
    /* The camera's last two reasons (LookReason), newest first, so its next
     * pick leans to something else. 0 is LOOK_QUIET, a fine start. */
    int look_last[2];
    int idle_frames;
    bool keys_open; /* the ? page, every key, drawn over the world */
    /* v: the two branches side by side, while split; it opens by itself when
     * a skip ends while split. was_skipping: a skip was running last frame. */
    bool compare_open;
    bool was_skipping;
    Playlog playlog; /* counted always, written only with --playlog */
    /* The one-time hints already given this run, as HINT_ bits: each is said
     * once, the first time it would help. */
    unsigned hints;
    /* The buttons (ui.h): where the last frame drew them, the mouse over them.
     * world_menu_open: the world button's small menu is showing. */
    Ui ui;
    bool world_menu_open;
    /* The weather drawn over the map (weather_fx.h): how strong, 0..1, the
     * last event's kind, so its fade-out still draws it after it ends, and
     * the effect's own clock, at half speed while paused. */
    float weather_fx;
    int weather_fx_kind;
    uint64_t weather_fx_ms;
} App;

static void apply_window_size(App *a, int w, int h)
{
    /* If memory runs out the old canvas stays, and is simply stretched. */
    if (canvas_resize(&a->canvas, w, h)) {
        scene_layout(&a->layout, w, h);
        camera_resize(&a->camera, a->layout.view_w, a->layout.view_h);
    }
}

/* True when this window pixel is on the world's part of the window. */
static bool over_view(const App *a, int x, int y)
{
    const Layout *l = &a->layout;
    return x >= l->view_x && x < l->view_x + l->view_w && y >= l->view_y &&
           y < l->view_y + l->view_h;
}

/* Panning means looking elsewhere, which following would undo next frame. */
static void pan(App *a, int dx, int dy)
{
    inspect_stop(&a->inspect);
    camera_pan(&a->camera, dx, dy);
}

static int notice_box(const App *a, int *x, int *y, int *w, int *h);
static void follow_notice(App *a, const Notice *x);
/* Declared here, defined further down: the new-world functions use it. */
static void say(App *a, const char *text, int frames);

static void click(App *a, int x, int y)
{
    playlog_click(&a->playlog);
    /* The pages and menus over the map are buttons now (ui.h): a click on
     * one never gets here, so whatever is under this point is the world. */
    int sx = x - a->layout.view_x;
    int sy = y - a->layout.view_y;
    if (!camera_on_world(&a->camera, sx, sy)) {
        inspect_stop(&a->inspect);
        return;
    }
    float wx, wy;
    camera_to_world(&a->camera, sx, sy, &wx, &wy);
    scene_click(&a->inspect, &a->sim, wx, wy);
    if ((a->hints & HINT_FOLLOW) == 0 && inspect_creature(&a->inspect, &a->sim) != NULL) {
        a->hints |= HINT_FOLLOW;
#ifdef VEREDAS_DEMO
        bool can_give = false; /* the demo has weather only */
#else
        bool can_give = a->sandbox.on;
#endif
        say(a, can_give ? "its card is on the right: story, let go and give trait "
                          "are buttons there"
                        : "its card is on the right: story and let go are buttons "
                          "there",
            3 * NOTICE_FRAMES);
    }
}

/* A click on one of the world panel's lines: follow who it names. */
static void follow_from_panel(App *a, int key)
{
    int slot;
    switch (key) {
    case UI_KEY_FOLLOW_TOP_HUNTER:
        slot = inspect_highest_level(&a->sim);
        break;
    case UI_KEY_FOLLOW_OLDEST:
        slot = inspect_oldest(&a->sim);
        break;
    default: /* UI_KEY_FOLLOW_LARGEST_FAMILY */
        slot = inspect_largest_of_family(&a->sim,
                                         family_largest(&a->families, &a->sim));
        break;
    }
    if (slot >= 0) {
        inspect_follow(&a->inspect, &a->sim, slot);
    } else {
        say(a, "nobody to follow", NOTICE_FRAMES);
    }
}

/* Everything on the screen that was about the old world, made fresh for a new
 * one in a->sim: the drawing, the followed creature, the pages, the menus, the
 * branch and the look-ahead copy. */
static void forget_old_world(App *a)
{
    scene_reset(&a->scene);
    inspect_stop(&a->inspect);
    /* Ids start again in a new world: an old id would name a stranger. */
    life_page_close(&a->page);
    notices_reset(&a->notices);
    a->notices_open = false;
    family_reset(&a->families);
    a->ticks_due = 0;
    /* A menu or a summary was about the old world. */
    a->sandbox.menu = SANDBOX_MENU_NONE;
    skip_free(&a->sandbox.skip);
    /* r replaces the world on screen; its other branch goes with it. */
    branch_free(&a->branches);
    a->keep_asked = false;
    a->compare_open = false;
    sandbox_weather_forget(&a->sandbox);
    /* The copy was of the old world; the new one gets picked for at once. */
    lookahead_free(&a->look);
    a->idle_frames = LOOK_IDLE_FRAMES;
}

/* Builds a->sim again from seed with its own settings, and starts its
 * ancestors. */
static void start_world(App *a, uint64_t seed)
{
    if (!sim_reseed(&a->sim, seed)) {
        a->failed = true; /* the brain could not be reopened */
        a->running = false;
        return;
    }
    forget_old_world(a);
    app_prelude_start(&a->prelude, &a->sim, a->prelude_years);
    if (a->opt->frames > 0) {
        app_prelude_finish(&a->prelude, &a->sim); /* as at the start */
    }
}

static void new_world(App *a)
{
    start_world(a, rng_next(&a->seed_source));
}

/* The setup screen's Enter: the same seed again, with its settings, so the
 * new start can be told apart from the old by the settings alone. */
static void restart_with_setup(App *a)
{
    sandbox_setup_apply(&a->sandbox, &a->sim.set);
    start_world(a, a->sim.seed);
    say(a, "a new start: the same seed, with the bodies' new rules",
        2 * NOTICE_FRAMES);
}

/* Where a copied world code is also printed: in a terminal, except in the
 * browser (no terminal) and on Windows (its clipboard keeps the text after
 * veredas closes, and the window program may have no terminal). A plain 0 or 1,
 * so it works both in #if and as a value in C. */
#if defined(__EMSCRIPTEN__) || defined(_WIN32)
#define CODE_IN_TERMINAL 0
#else
#define CODE_IN_TERMINAL 1
#endif

/*
 * In the browser, c copies a link to this page with the code in it: whoever
 * opens the link sees the world replayed (web/shell.html hands the code= field
 * to --code). Returns the link as a new string, or NULL outside a browser or
 * when memory ran out; then the bare code is copied.
 */
static char *page_link(const char *code)
{
    char *address = platform_page_address();
    if (address == NULL) {
        return NULL;
    }
    /* "?" and the field's name, then the code; + 2 for the '?' and the '\0'. */
    size_t room = strlen(address) + strlen(WORLDCODE_LINK_FIELD) + strlen(code) + 2;
    char *link = malloc(room);
    if (link != NULL) {
        snprintf(link, room, "%s?%s%s", address, WORLDCODE_LINK_FIELD, code);
    }
    free(address);
    return link;
}

/* c: the world shown, as a code on the clipboard (a link in the browser). */
static void copy_code(App *a)
{
    char *text = NULL;
    if (!worldcode_write(&a->sim, &text)) {
        say(a, "no memory left to make the world code", NOTICE_FRAMES);
        return;
    }
    char *link = page_link(text);
    if (link != NULL) {
        if (platform_clipboard_set(link)) {
            say(a, "link to this world copied: whoever opens it sees it replayed",
                2 * NOTICE_FRAMES);
        } else {
            say(a, "could not put the link on the clipboard", NOTICE_FRAMES);
        }
        free(link);
        free(text);
        return;
    }
    /* Also in the terminal veredas was started from. On Linux with X11 the
     * clipboard is not a store: the program that copied keeps the text and
     * hands it over when another one pastes, so without a clipboard manager
     * the code is gone once veredas closes. The printed line stays. And in a
     * --frames run (a test or a screenshot) on every system, Windows too,
     * so the window smoke test can read the code back. fflush: stdout into
     * a pipe or file is written in blocks, and this line should be there at
     * once. */
    if (CODE_IN_TERMINAL || a->opt->frames > 0) {
        printf("world code: %s\n", text);
        fflush(stdout);
    }
    if (platform_clipboard_set(text)) {
        char line[96];
        snprintf(line, sizeof(line), "world code copied (%zu characters%s): o "
                                     "opens it",
                 strlen(text), CODE_IN_TERMINAL ? "; also in the terminal" : "");
        say(a, line, 2 * NOTICE_FRAMES);
    } else {
        say(a, CODE_IN_TERMINAL ? "could not use the clipboard: the world code "
                                  "is in the terminal"
                                : "could not put the world code on the clipboard",
            NOTICE_FRAMES);
    }
    free(text);
}

/*
 * The browser demo's memory is fixed at 128 MB, and it holds two worlds (the
 * one shown and the look-ahead copy), the drawing and, while a copy is made,
 * the copy's bytes. A world's size is mostly its creature pool: every slot is
 * kept whether a creature is in it or not, about 4.5 kB each, so the pool, not
 * the map, is what the demo cuts. Measured on a 192 by 108 world after three
 * years: 15.4 MB a world, 28.9 MB of drawing, about 60 MB in all (8,000 slots
 * would be 37 MB a world and 106 MB in all). The demo opens a code only up to
 * these.
 */
#ifdef VEREDAS_DEMO
#define DEMO_POOL_CAPACITY 3000
#define CODE_MAX_CELLS (DEFAULT_WORLD_W * DEFAULT_WORLD_H)
#define CODE_MAX_POOL DEMO_POOL_CAPACITY
#else
#define CODE_MAX_CELLS (OPTIONS_MAX_SIDE * OPTIONS_MAX_SIDE)
#define CODE_MAX_POOL INT_MAX
#endif

/* Reads text's code, alone or inside a link, into *code; false with one line
 * in err and nothing to free. */
static bool read_code(const char *text, WorldCode *code, char *err, size_t len)
{
    if (!worldcode_read(text, code, err, len)) {
        return false;
    }
    /* Converted before multiplying, so a 4096 by 4096 code cannot overflow
     * an int. */
    if ((long)code->w * (long)code->h > (long)CODE_MAX_CELLS ||
        code->set.pool_capacity > CODE_MAX_POOL) {
        snprintf(err, len, "that world is too big for the browser demo");
        worldcode_free(code);
        return false;
    }
    return true;
}

/*
 * text's world (a code, or a link holding one) becomes the world on screen,
 * replayed from its first tick in slices (the prelude's run) up to where it
 * was copied. The new world is built aside first, so a code that cannot be
 * built leaves the old one running and says why in err. Its size and creature
 * pool may differ from the old one's, so the drawing and the families, which
 * are sized by them, are made again.
 */
static bool open_code_text(App *a, const char *text, char *err, size_t len)
{
    WorldCode code;
    if (!read_code(text, &code, err, len)) {
        return false;
    }
    Sim fresh;
    bool built = worldcode_start(&fresh, &code, err, len);
    uint64_t until = code.until;
    worldcode_free(&code);
    if (!built) {
        return false;
    }
    forget_old_world(a);
    sim_free(&a->sim);
    /* A struct assignment copies every field, the pointers included: fresh's
     * memory now belongs to a->sim, and fresh must not be freed. */
    a->sim = fresh;
    bool art = a->scene.sprites.art;
    scene_free(&a->scene);
    family_free(&a->families);
    if (!scene_init(&a->scene, &a->sim) ||
        !family_init(&a->families, a->sim.pool.capacity)) {
        platform_error("out of memory");
        a->failed = true;
        a->running = false;
        return true; /* the code was fine; the frame loop ends */
    }
    sprite_cache_set_art(&a->scene.sprites, art);
    int level = a->camera.level;
    camera_init(&a->camera, a->sim.world.w, a->sim.world.h, a->layout.view_w,
                a->layout.view_h);
    a->camera.level = level;
    lookahead_init(&a->look, (uint64_t)(a->sim.set.ticks_per_year / 4));
    app_prelude_replay(&a->prelude, &a->sim, until);
    if (a->opt->frames > 0) {
        app_prelude_finish(&a->prelude, &a->sim);
    }
    a->redraw = true;
    return true;
}

/* o: the clipboard's code (or a link holding one) becomes the world shown. */
static void open_code(App *a)
{
    char *text = platform_clipboard_get();
    if (text == NULL) {
        say(a, "copy a world code first, then o", NOTICE_FRAMES);
        return;
    }
    char err[128];
    if (!open_code_text(a, text, err, sizeof(err))) {
        say(a, err, 2 * NOTICE_FRAMES);
    }
    free(text);
}

static void say(App *a, const char *text, int frames)
{
    snprintf(a->notice, sizeof(a->notice), "%s", text);
    a->notice_frames = frames;
}

/* l: the followed creature's story, else the most notable one; again, closed. */
static void toggle_page(App *a)
{
    if (a->page.open) {
        life_page_close(&a->page);
    } else if (a->inspect.following) {
        life_page_open(&a->page, &a->sim, a->inspect.id);
    } else if (!life_page_open_most_notable(&a->page, &a->sim)) {
        say(a, "no life recorded yet", NOTICE_FRAMES);
    }
}

/* The newest notice that names a creature, or NULL. */
static const Notice *newest_with_creature(const Notices *n)
{
    for (uint32_t back = 0; back < NOTICES_KEPT; back++) {
        const Notice *x = notices_get(n, back);
        if (x == NULL || x->creature != NOTICE_NOBODY) {
            return x;
        }
    }
    return NULL;
}

/* Follows who notice x is about, or says why not. */
static void follow_notice(App *a, const Notice *x)
{
    int slot = x != NULL && x->creature != NOTICE_NOBODY
                   ? life_find_slot(&a->sim, x->creature) : -1;
    if (slot >= 0) {
        inspect_follow(&a->inspect, &a->sim, slot);
        a->notices_open = false;
    } else {
        say(a, x == NULL ? "no notice there"
               : x->creature == NOTICE_NOBODY ? "that notice is about nobody to follow"
                                  : "that creature is dead",
            NOTICE_FRAMES);
    }
}

/* j: follow the creature the open page tells of, else the newest notice's. */
static void jump(App *a)
{
    if (a->page.open) {
        int slot = a->page.alive ? life_find_slot(&a->sim, a->page.id) : -1;
        if (slot >= 0) {
            inspect_follow(&a->inspect, &a->sim, slot);
        } else {
            say(a, "that creature is dead", NOTICE_FRAMES);
        }
        return;
    }
    follow_notice(a, newest_with_creature(&a->notices));
}

/* The e list's box: where draw_notices puts it, and where a click finds a row.
 * Returns the number of rows, 0 when there is nothing to show or no room. */
static int notice_box(const App *a, int *x, int *y, int *w, int *h)
{
    const Layout *l = &a->layout;
    int rows = (int)(a->notices.total < NOTICE_LIST_ROWS ? a->notices.total
                                                         : NOTICE_LIST_ROWS);
    *x = l->view_x + FONT_W;
    *y = l->view_y + FONT_H / 2;
    *w = l->view_w - 2 * FONT_W;
    /* A heading row, the rows, and half a row of margin above and below. */
    *h = (rows + 2) * FONT_H;
    if (*w < FONT_W * 20 || *h > l->view_h) {
        return 0;
    }
    return rows;
}

/* Puts the other branch on screen. What the window remembers about the old
 * world's bodies and families is about different creatures now, so it starts
 * again, as for a new world; the camera keeps following the same id, which in
 * a branch is the same creature, if it is alive on this side too. */
static void switch_branch(App *a)
{
    if (!a->branches.split) {
        say(a, "one world only: x splits it in two", NOTICE_FRAMES);
        return;
    }
    if (!branch_switch(&a->branches, &a->sim)) {
        say(a, "the other branch is still catching up", NOTICE_FRAMES);
        return;
    }
    scene_reset(&a->scene);
    family_reset(&a->families);
    life_page_close(&a->page);
    /* The event watched was the other branch's, from that branch's start. */
    sandbox_weather_forget(&a->sandbox);
    /* Each branch keeps its own list. A struct assignment copies every byte,
     * the arrays inside included, and Notices points at nothing, so this is
     * a whole swap. */
    Notices shown = a->notices;
    a->notices = a->other_notices;
    a->other_notices = shown;
    say(a, a->branches.shown == 0 ? "branch A" : "branch B", NOTICE_FRAMES);
}

/* x: splits the world in two; while split, x twice keeps the one shown. */
static void branch_key(App *a)
{
    char err[160];
    if (!a->branches.split) {
        if (branch_make(&a->branches, &a->sim, err, sizeof(err))) {
            a->other_notices = a->notices; /* the same past on both sides */
            say(a, "split in two: this is A, Tab shows B; "
                   "try something in one", 2 * NOTICE_FRAMES);
        } else {
            say(a, err, NOTICE_FRAMES);
        }
    } else if (!a->keep_asked) {
        a->keep_asked = true;
        say(a, "x again keeps this branch and drops the other", NOTICE_FRAMES);
    } else {
        branch_keep_shown(&a->branches);
        a->keep_asked = false;
        say(a, "kept this branch; one world again", NOTICE_FRAMES);
    }
}

/* Every key, from the keyboard or a button. The playlog counts keyboard keys
 * where they arrive (EV_KEY, --keys) and buttons in press_button, so a button's
 * click is not counted as its key as well. */
static void handle_key(App *a, int key)
{
    /* While the ancestors evolve, any key but q ends that where it got to,
     * and does nothing else: it was pressed to stop the waiting. */
    bool in_setup = a->sandbox.menu == SANDBOX_MENU_SETUP;
#ifndef VEREDAS_DEMO
    /* m while the ancestors evolve: Sandbox, and its setup screen, since a new
     * start is cheapest before anything has been watched. */
    if (a->prelude.active && !a->prelude.replay && !in_setup &&
        (key == 'm' || key == 'M')) {
        a->sandbox.on = true;
        a->look_on = false;
        sandbox_open_setup(&a->sandbox, &a->sim.set);
        a->redraw = true;
        return;
    }
#endif
    if (a->prelude.active && key != 'q' && !in_setup) {
        app_prelude_stop(&a->prelude);
        a->notice[0] = '\0';
        a->redraw = true;
        return;
    }
    /* The open notice list takes the number keys: each follows a row. */
    if (a->notices_open && key >= '1' && key <= '9') {
        follow_notice(a, notices_get(&a->notices, (uint32_t)(key - '1')));
        a->redraw = true;
        return;
    }
    /* The mode and the Sandbox tools first: an open menu takes the number
     * keys, which the lab would otherwise read. q always reaches the switch. */
    if (key != 'q') {
        char line[256] = "";
        const Creature *followed = inspect_creature(&a->inspect, &a->sim);
        bool was_sandbox = a->sandbox.on;
        int took = sandbox_key(&a->sandbox, &a->sim, key, followed,
                               platform_now_ms(), line, sizeof(line));
        if (took == SANDBOX_RESTART) {
            restart_with_setup(a);
            a->redraw = true;
            return;
        }
        if (took == SANDBOX_TAKEN) {
            /* A new mode brings its own camera: on in Documentary. */
            if (a->sandbox.on != was_sandbox) {
                a->look_on = !a->sandbox.on;
            }
            if (line[0] != '\0') {
                say(a, line, 2 * NOTICE_FRAMES);
            }
            a->redraw = true;
            return;
        }
    }
    /* Any key but a second x takes back a first x's question. */
    if (key != 'x' && key != 'X') {
        a->keep_asked = false;
    }
    /* Branching is a Sandbox tool; Documentary never changes a world. */
#ifdef VEREDAS_DEMO
    if (a->sandbox.on &&
        (key == 'x' || key == 'X' || key == '\t' || key == 'v' || key == 'V')) {
        say(a, "not in the browser demo: weather only (w)", NOTICE_FRAMES);
        return;
    }
#endif
    if (a->sandbox.on && (key == 'x' || key == 'X')) {
        branch_key(a);
        a->redraw = true;
        return;
    }
    if (a->sandbox.on && key == '\t') {
        switch_branch(a);
        a->redraw = true;
        return;
    }
    if (a->sandbox.on && (key == 'v' || key == 'V')) {
        if (a->branches.split) {
            a->compare_open = !a->compare_open;
        } else {
            say(a, "nothing to compare: x splits the world in two first",
                NOTICE_FRAMES);
        }
        a->redraw = true;
        return;
    }
    switch (key) {
    case 'q':
        if (!PLATFORM_IS_PAGE) { /* stopping a page would only freeze it */
            a->running = false;
        }
        break;
    case ' ':
        a->paused = !a->paused;
        break;
    case '+':
    case '=': /* the same key as + on most keyboards, without shift */
        if (a->speed < APP_SPEED_COUNT - 1) {
            a->speed++;
        }
        break;
    case '-':
        if (a->speed > 0) {
            a->speed--;
        }
        break;
    case 'r':
        new_world(a);
        break;
    case 'n': /* only here, where it cannot end a living world by a slip */
        if (sim_is_over(&a->sim)) {
            new_world(a);
        }
        break;
    case 's':
        if (!PLATFORM_IS_PAGE) {
            app_save_now(&a->sim, a->opt->save_path, a->notice,
                         sizeof(a->notice));
            a->notice_frames = NOTICE_FRAMES;
        }
        break;
    case ']':
        camera_zoom(&a->camera, 1, a->camera.view_w / 2, a->camera.view_h / 2);
        break;
    case '[':
        camera_zoom(&a->camera, -1, a->camera.view_w / 2, a->camera.view_h / 2);
        break;
    case KEY_LEFT: pan(a, -ARROW_PAN, 0); break;
    case KEY_RIGHT: pan(a, ARROW_PAN, 0); break;
    case KEY_UP: pan(a, 0, -ARROW_PAN); break;
    case KEY_DOWN: pan(a, 0, ARROW_PAN); break;
    case 'l':
    case 'L':
        toggle_page(a);
        break;
    case 'j':
        jump(a);
        break;
    case 'e':
    case 'E':
        a->notices_open = !a->notices_open;
        if (a->notices_open && a->notices.total == 0) {
            a->notices_open = false;
            say(a, "no notices yet", NOTICE_FRAMES);
        }
        break;
    case 'b':
    case 'B':
        a->lab_open = !a->lab_open;
        break;
    case '/': /* the ? key: SDL gives the key, not the character shift makes */
    case '?':
    case 'h':
    case 'H':
        a->keys_open = !a->keys_open;
        break;
    case 'p':
    case 'P':
        if (a->lab_open) {
            a->lab_parts = !a->lab_parts;
        }
        break;
    case 'c':
    case 'C':
        copy_code(a);
        break;
    case 'o':
    case 'O':
        open_code(a);
        break;
    case 'f':
    case 'F':
        a->look_on = !a->look_on;
        say(a, a->look_on ? "camera on: it picks who to follow when you let go"
                          : "camera off: follow by hand", NOTICE_FRAMES);
        break;
    case 'a':
    case 'A':
        /* The drawn frames (art.h) on or off: the pictures kept are
         * forgotten, and the next frame draws them the other way. */
        sprite_cache_set_art(&a->scene.sprites, !a->scene.sprites.art);
        a->redraw = true;
        break;
    case '1': case '2': case '3': case '4': case '5': case '6':
        /* The action keys only mean something on the lab page. Characters are
         * numbers in C, so key - '0' is the key's number: '3' - '0' is 3. */
        if (a->lab_open) {
            a->lab_action = lab_action_of_key(key - '0');
        }
        break;
    case KEY_ESCAPE:
        /* The pages first, so Esc never lets go of a creature behind them. */
        if (a->world_menu_open) {
            a->world_menu_open = false;
        } else if (a->keys_open) {
            a->keys_open = false;
        } else if (a->compare_open) {
            a->compare_open = false;
        } else if (a->lab_open) {
            a->lab_open = false;
        } else if (a->notices_open) {
            a->notices_open = false;
        } else if (a->page.open) {
            life_page_close(&a->page);
        } else {
            inspect_stop(&a->inspect);
        }
        break;
    default: break;
    }
}

/* A click on a button: the key it stands for, as if pressed; the world
 * button opens or closes its menu, and a row of that menu closes it. */
static void press_button(App *a, int key)
{
    playlog_click(&a->playlog);
    if (key == UI_KEY_BLOCK) {
        return; /* a box's background: nothing, and not the world behind */
    }
    /* After the background test: a click on a box beside its buttons is a
     * click, but not on a button. */
    playlog_button(&a->playlog, key);
    if (key == UI_KEY_WORLD_MENU) {
        a->world_menu_open = !a->world_menu_open;
        return;
    }
    a->world_menu_open = false;
    switch (key) {
    case UI_KEY_FOLLOW_TOP_HUNTER:
    case UI_KEY_FOLLOW_OLDEST:
    case UI_KEY_FOLLOW_LARGEST_FAMILY:
        follow_from_panel(a, key);
        break;
    /* Each page's x closes that page, whatever else is open. */
    case UI_KEY_CLOSE_STORY: life_page_close(&a->page); break;
    case UI_KEY_CLOSE_NOTICES: a->notices_open = false; break;
    case UI_KEY_CLOSE_COMPARE: a->compare_open = false; break;
    case UI_KEY_CLOSE_KEYS: a->keys_open = false; break;
    case UI_KEY_CLOSE_LAB: a->lab_open = false; break;
    case UI_KEY_LET_GO: inspect_stop(&a->inspect); break;
    case UI_KEY_MENU_FIRST:
        say(a, sandbox_skipping(&a->sandbox) ? "skipping ahead: Esc stops it"
                                             : "close the menu first (Esc)",
            NOTICE_FRAMES);
        break;
    default: handle_key(a, key); break;
    }
}

static void handle_event(App *a, const Event *e)
{
    a->redraw = true;
    /* Only the player's own doing counts; a resize or a quit does not. */
    if (e->kind == EV_KEY || e->kind == EV_DOWN || e->kind == EV_WHEEL ||
        (e->kind == EV_MOVE && a->pressed)) {
        a->idle_frames = 0;
    }
    switch (e->kind) {
    case EV_QUIT:
        a->running = false;
        break;
    case EV_KEY:
        playlog_key(&a->playlog, e->key);
        handle_key(a, e->key);
        break;
    case EV_WHEEL:
        /* Not over a menu, page or button drawn on the map: the wheel there
         * belongs to the box, as a press does, not to the world behind it. */
        if (over_view(a, e->x, e->y) && ui_key_at(&a->ui, e->x, e->y) == UI_NONE) {
            /* The camera counts from the view's own corner. */
            camera_zoom(&a->camera, e->delta, e->x - a->layout.view_x,
                        e->y - a->layout.view_y);
        }
        break;
    case EV_DOWN:
        a->dragging = false;
        a->press_x = a->mouse_x = e->x;
        a->press_y = a->mouse_y = e->y;
        /* A press on a button is the button's alone: the world and the panel
         * behind it never see it. */
        if (ui_mouse_down(&a->ui, e->x, e->y)) {
            a->pressed = false;
            break;
        }
        /* Anywhere else, the world menu closes, as menus do. */
        a->world_menu_open = false;
        a->pressed = over_view(a, e->x, e->y);
        break;
    case EV_UP: {
        /* Braces: a case may declare a variable only inside a block of its own. */
        int key = ui_mouse_up(&a->ui, e->x, e->y);
        if (key != UI_NONE) {
            press_button(a, key);
        } else if (a->pressed && !a->dragging) {
            click(a, e->x, e->y);
        }
        a->pressed = false;
        a->dragging = false;
        break;
    }
    case EV_MOVE:
        ui_mouse_move(&a->ui, e->x, e->y, platform_now_ms());
        if (a->pressed && !a->dragging &&
            (abs(e->x - a->press_x) > CLICK_SLOP ||
             abs(e->y - a->press_y) > CLICK_SLOP)) {
            a->dragging = true;
        }
        if (a->dragging) {
            /* Dragging pulls the world along: the view moves the other way. */
            pan(a, a->mouse_x - e->x, a->mouse_y - e->y);
            a->mouse_x = e->x;
            a->mouse_y = e->y;
        }
        break;
    case EV_RESIZE:
        apply_window_size(a, e->w, e->h);
        break;
    }
}

/* The top line: boxes for the mode, the date, the animals, the speed, the
 * branch, the weather and who is followed, with the message of the moment. */
static void draw_title(App *a, const Creature *followed)
{
    /* static: a TopState holds 300 bytes of text, and this keeps it off the
     * frame's stack, which the browser keeps small. Filled anew every frame. */
    static TopState st;
    memset(&st, 0, sizeof(st));
    int tpy = a->sim.set.ticks_per_year;
    st.sandbox = a->sandbox.on;
    st.paused = a->paused;
    st.game_over = sim_is_over(&a->sim);
    st.split = a->branches.split;
    st.branch = a->branches.shown == 0 ? 'A' : 'B';
    st.year = years_whole(a->sim.tick, tpy);
    st.season = years_season_of(a->sim.tick, tpy);
    st.animals = a->sim.pool.count;
    st.speed = APP_SPEEDS[a->speed];
    if (weather_active(&a->sim.weather, a->sim.tick)) {
        st.weather_kind = a->sim.weather.kind;
        /* An event lasts a season (actions.c). The cast: ticks_per_year is a
         * positive int, the length a tick count. */
        st.weather_left = weather_left(&a->sim.weather, a->sim.tick,
                                       (uint64_t)tpy / 4u);
        weather_label(&a->sim.weather, a->sim.tick, tpy, st.weather_label,
                      sizeof(st.weather_label));
    }
    if (followed != NULL) {
        name_from_id(followed->id, st.following, sizeof(st.following));
    }
    snprintf(st.notice, sizeof(st.notice), "%s", a->notice);
    bar_top_draw(&a->ui, &a->canvas, a->canvas.w, &st);
}

/* The bar of buttons along the bottom, and the world menu above its right end
 * while open. */
static void draw_bottom(App *a)
{
    BottomState st = {0}; /* every field not set below is 0 or false */
    st.paused = a->paused;
    st.sandbox = a->sandbox.on;
    st.split = a->branches.split;
#ifdef VEREDAS_DEMO
    st.demo = true;
#endif
    st.following = a->inspect.following;
    st.look_on = a->look_on;
    st.speed = APP_SPEEDS[a->speed];
    st.world_menu_open = a->world_menu_open;
    st.menu_open = a->sandbox.menu != SANDBOX_MENU_NONE || sandbox_skipping(&a->sandbox);
    bar_bottom_draw(&a->ui, &a->canvas, a->layout.help_y, a->canvas.w, &st);
    if (a->world_menu_open) {
        bar_world_menu_draw(&a->ui, &a->canvas, a->canvas.w - 2,
                            a->layout.help_y - 2, PLATFORM_IS_PAGE);
    }
}

/* The enlarged body, with a caption above it. */
static void draw_enlarged(App *a, uint32_t lineage, const FamilySample *newest)
{
    const Layout *l = &a->layout;
    char caption[32] = " no family yet";
    if (lineage != FAMILY_NONE) {
        snprintf(caption, sizeof(caption), " largest: #%u", (unsigned)lineage);
    }
    cells_canvas_set(&a->canvas, l->panel_x, l->family_y);
    render_text(0, 0, caption, PANEL_COLS);
    /* In the middle of the panel's width, below the caption. */
    int x = l->panel_x + (SCENE_PANEL_W - SCENE_ENLARGED_W) / 2;
    int y = l->family_y + FONT_H;
    if (newest != NULL) {
        scene_draw_sample(&a->scene, &a->canvas, newest, lineage, a->body_ms, x, y);
    }
}

/* The family's bodies over time, with a caption above them. A family outside the
 * recorded ones has no strip, and the caption says so. */
static void draw_strip(App *a, uint32_t lineage, int samples)
{
    const Layout *l = &a->layout;
    int y = l->family_y + (SCENE_ENLARGED_ROWS * FONT_H);
    cells_canvas_set(&a->canvas, l->panel_x, y);
    char caption[32];
    /* The longest id, 10 digits, would still only lose the end of the words. */
    snprintf(caption, sizeof(caption), samples == 0 ? " #%u: no record" : " family #%u",
             (unsigned)lineage);
    render_text(0, 0, lineage != FAMILY_NONE ? caption : "", PANEL_COLS);
    scene_draw_strip(&a->scene, &a->canvas, &a->families, lineage, l->panel_x,
                     y + FONT_H, SCENE_PANEL_W);
}

/* The bottom of the world panel: the largest family and its newest body.
 * (A followed creature's own family is on its card.) */
static void draw_family(App *a)
{
    if (!a->layout.show_enlarged) {
        return;
    }
    uint32_t lineage = family_largest(&a->families, &a->sim);
    FamilySample samples[FAMILY_STRIP];
    int n = family_strip(&a->families, lineage, samples);
    draw_enlarged(a, lineage, n > 0 ? &samples[n - 1] : NULL);
    if (a->layout.show_strip) {
        draw_strip(a, lineage, n);
    }
}

/* The world panel's lines that follow someone, as buttons. A line that did
 * not fit is not on screen, so it gets none. While following, the panel
 * lists that creature instead, and these lines are not there. */
static void panel_hits(App *a)
{
    const Layout *l = &a->layout;
    if (PANEL_ROW_TOP_HUNTER < l->panel_rows) {
        ui_add_hit_tip(&a->ui, l->panel_x, l->panel_y + PANEL_ROW_TOP_HUNTER * FONT_H,
                       SCENE_PANEL_W, FONT_H, UI_KEY_FOLLOW_TOP_HUNTER,
                       "follow the top hunter");
    }
    if (PANEL_ROW_OLDEST < l->panel_rows) {
        ui_add_hit_tip(&a->ui, l->panel_x, l->panel_y + PANEL_ROW_OLDEST * FONT_H,
                       SCENE_PANEL_W, FONT_H, UI_KEY_FOLLOW_OLDEST,
                       "follow the oldest");
    }
    if (l->show_enlarged) {
        ui_add_hit_tip(&a->ui, l->panel_x, l->family_y, SCENE_PANEL_W,
                       SCENE_ENLARGED_ROWS * FONT_H, UI_KEY_FOLLOW_LARGEST_FAMILY,
                       "follow one of the largest family");
    }
}

static void draw_panel(App *a, const Creature *followed)
{
    const Layout *l = &a->layout;
    if (!l->has_panel) {
        return;
    }
    /* Rows are whole; this clears what is left below the last of them. */
    canvas_rect(&a->canvas, l->panel_x, l->panel_y, SCENE_PANEL_W, l->view_h,
                canvas_rgb(PALETTE_TEXT_BACKGROUND.r, PALETTE_TEXT_BACKGROUND.g,
                           PALETTE_TEXT_BACKGROUND.b));
    if (followed != NULL) {
        /* While following, the panel is that animal's card. */
        card_draw(&a->ui, &a->scene, &a->canvas, l->panel_x, l->panel_y,
                  SCENE_PANEL_W, l->view_h, &a->sim, &a->families, followed,
                  a->body_ms, a->sandbox.on);
        return;
    }
    cells_canvas_set(&a->canvas, l->panel_x, l->panel_y);
    panel_draw(&a->sim, 0, 0, l->panel_rows);
    panel_hits(a);
    draw_family(a);
}

/* A page's box over the map: its background, and a hit over all of it so a
 * click on the page never reaches the world behind. The page's x goes on
 * after its text (ui_close_button_key), so it is drawn over the text and,
 * added later, wins over this hit. */
static void page_box(App *a, int x, int y, int w, int h)
{
    canvas_rect(&a->canvas, x, y, w, h,
                canvas_rgb(PALETTE_TEXT_BACKGROUND.r, PALETTE_TEXT_BACKGROUND.g,
                           PALETTE_TEXT_BACKGROUND.b));
    ui_add_hit(&a->ui, x, y, w, h, UI_KEY_BLOCK);
}

/* Columns of a page's first line the x leaves free: its 16 pixels and the 2
 * of air beside it are 3 characters of 8. */
#define CLOSE_COLUMNS 3

/* The L page over the world's area, inset by a character all round, so the
 * world keeps showing (and running) at its edges. */
static void draw_page(App *a)
{
    const Layout *l = &a->layout;
    life_page_update(&a->page, &a->sim);
    int x = l->view_x + FONT_W, y = l->view_y + FONT_H / 2;
    int w = l->view_w - 2 * FONT_W, h = l->view_h - FONT_H;
    if (w < FONT_W * 20 || h < FONT_H * 4) {
        return; /* too small to read; the world shows instead */
    }
    int cols = w / FONT_W;
    int rows = h / FONT_H;
    if (rows > LIFE_PAGE_MAX_LINES) {
        rows = LIFE_PAGE_MAX_LINES;
    }
    /* static: about 7 KB, kept off the stack, which is small in the web
     * build. Only this function uses it, one frame at a time. */
    static char lines[LIFE_PAGE_MAX_LINES][LIFE_PAGE_LINE_BYTES];
    int n = life_page_text(&a->page, a->sim.set.ticks_per_year, cols, lines,
                           rows);
    page_box(a, x, y, w, h);
    cells_canvas_set(&a->canvas, x, y);
    /* The first line stops short of the x. */
    for (int r = 0; r < n; r++) {
        render_text(0, r, lines[r], r == 0 ? cols - CLOSE_COLUMNS : cols);
    }
    ui_close_button_key(&a->ui, &a->canvas, x, y, w, UI_KEY_CLOSE_STORY,
                        "close the story (l)");
}

/* Columns a line of UTF-8 takes on screen: one per character. A character's
 * first byte is never 10xxxxxx; the bytes that follow it always are, so
 * counting the others counts characters. */
static int text_columns(const char *text)
{
    int count = 0;
    for (const unsigned char *b = (const unsigned char *)text; *b != 0; b++) {
        if ((*b & 0xC0) != 0x80) {
            count++;
        }
    }
    return count;
}

/* The open Sandbox menu, a skip's progress or its summary, in a box at the
 * top left of the world. */
static void draw_sandbox(App *a)
{
    const Layout *lay = &a->layout;
    if (menu_is_drawn(&a->sandbox)) {
        /* The four tool menus as buttons; the skip under way and its summary
         * stay lines of text, below. */
        menu_draw(&a->ui, &a->canvas, lay->view_x, lay->view_y, lay->view_w,
                  lay->view_h, &a->sandbox, &a->sim, PLATFORM_IS_PAGE);
        return;
    }
    static char lines[SKIP_LINES][SKIP_COLS]; /* static: off the small web stack */
    int n = sandbox_menu_lines(&a->sandbox, &a->sim, platform_now_ms(), lines,
                               SKIP_LINES);
    if (n == 0) {
        return;
    }
    const Layout *l = &a->layout;
    int cols = 0;
    for (int r = 0; r < n; r++) {
        int len = text_columns(lines[r]);
        cols = len > cols ? len : cols;
    }
    /* A margin of a character each side, and room for the x on the right. */
    int w = (cols + 2 + CLOSE_COLUMNS) * FONT_W, h = (n + 1) * FONT_H;
    if (w > l->view_w) {
        w = l->view_w;
    }
    int x = l->view_x + FONT_W, y = l->view_y + FONT_H / 2;
    page_box(a, x, y, w, h);
    cells_canvas_set(&a->canvas, x + FONT_W, y + FONT_H / 2);
    for (int r = 0; r < n; r++) {
        if (text_columns(lines[r]) == (int)strlen(lines[r])) {
            render_text(0, r, lines[r], w / FONT_W - 1);
        } else if (text_columns(lines[r]) <= w / FONT_W - 1) {
            render_utf8(0, r, lines[r]); /* the skip summary's chart */
        }
    }
    /* Escape stops a skip under way and closes its summary: the x is Escape. */
    ui_close_button_key(&a->ui, &a->canvas, x, y, w, KEY_ESCAPE,
                        sandbox_skipping(&a->sandbox) ? "stop the skip (Esc)"
                                                      : "close (Esc)");
}

/* Too small for anything else: a minimised window lands here too. */
static void draw_too_small(App *a)
{
    canvas_fill(&a->canvas, canvas_rgb(0, 0, 0));
    cells_canvas_set(&a->canvas, 0, 0);
    render_too_small(a->canvas.w / FONT_W, a->canvas.h / FONT_H, SCENE_MIN_COLS,
                     SCENE_MIN_ROWS);
}

/* The creature being followed, moving the camera; a death here tells the user
 * and drops back to the ordinary panel. */
static const Creature *update_following(App *a, float fraction)
{
    bool was_following = a->inspect.following;
    const Creature *followed =
        scene_follow(&a->scene, &a->camera, &a->inspect, &a->sim, fraction);
    if (was_following && followed == NULL) {
        inspect_stop(&a->inspect);
        snprintf(a->notice, sizeof(a->notice), "the creature you followed died");
        a->notice_frames = NOTICE_FRAMES;
        a->look_now = true;
    }
    return followed;
}

/* Takes a look for new notices and puts the newest in the title line. While
 * the ancestors evolve, the list stays empty: its first look after them takes
 * the world as it is then as known, so their years are not news. */
static void update_notices(App *a)
{
    if (a->prelude.active) {
        notices_reset(&a->notices);
        return;
    }
    uint32_t before = a->notices.total;
    notices_check(&a->notices, &a->sim);
    uint32_t fresh = a->notices.total - before;
    if (fresh == 0) {
        return;
    }
    char line[NOTICE_TEXT + 48];
    const Notice *x = notices_newest(&a->notices);
    if (fresh > 1) {
        snprintf(line, sizeof(line), "%s (and %u more: e lists)", x->text,
                 (unsigned)(fresh - 1));
    } else {
        /* The first notice of the run also tells where the others will be. */
        bool first = (a->hints & HINT_NOTICES) == 0;
        snprintf(line, sizeof(line), "%s%s%s", x->text,
                 x->creature != NOTICE_NOBODY ? " (j follows" : " (",
                 first ? (x->creature != NOTICE_NOBODY ? "; e lists every notice)"
                                                       : "e lists every notice)")
                       : (x->creature != NOTICE_NOBODY ? ")" : "e lists)"));
    }
    a->hints |= HINT_NOTICES;
    say(a, line, RARE_NOTICE_FRAMES);
    a->redraw = true;
}

/* Whether the look-ahead camera has work this frame. Not while the world is
 * split (it would be a third world), nor while the ancestors evolve or a skip
 * runs: those move the world too fast to follow, and the copy is made again
 * afterwards, which is quicker than running it after them. */
static bool look_wanted(const App *a)
{
    return a->look_on && !a->branches.split && !a->prelude.active &&
           !sandbox_skipping(&a->sandbox) && !sim_is_over(&a->sim);
}

/* Nothing on screen the player is reading that a new pick would change. */
static bool look_may_pick(const App *a)
{
    return !a->paused && !a->page.open && !a->lab_open && !a->notices_open &&
           a->sandbox.menu == SANDBOX_MENU_NONE && !a->inspect.following &&
           (a->look_now || a->idle_frames >= LOOK_IDLE_FRAMES);
}

/* Keeps the copy a season ahead and, when the camera may, follows the best
 * coming life. The copy only reads a->sim, so the world shown is the same with
 * the camera on or off. */
static void update_look(App *a)
{
    if (!look_wanted(a)) {
        /* Its memory back: a whole second world is not small. */
        lookahead_free(&a->look);
        a->look_now = false;
        return;
    }
    char err[160];
    if (a->opt->frames > 0) {
        /* Tests and screenshots: to the end, so each run is the same. */
        lookahead_sync_all(&a->look, &a->sim);
    } else if (!lookahead_sync(&a->look, &a->sim, platform_now_ms,
                               APP_PRELUDE_FRAME_MS / 2, err, sizeof(err))) {
        a->look_on = false;
        say(a, "camera off: no memory for its copy of the world", NOTICE_FRAMES);
        return;
    }
    if (!look_may_pick(a)) {
        return;
    }
    char why[LOOKAHEAD_WHY];
    /* The kinds of the last two picks, one bit each: the camera turns to
     * another kind of story when one has something coming. */
    unsigned tired = (1u << lookahead_kind(a->look_last[0])) |
                     (1u << lookahead_kind(a->look_last[1]));
    int reason;
    uint32_t id =
        lookahead_pick(&a->look, &a->sim, tired, &reason, why, sizeof(why));
    if (id == LOOKAHEAD_NOBODY) {
        return; /* not ready yet: look_now stays, and a later frame picks */
    }
    a->look_last[1] = a->look_last[0];
    a->look_last[0] = reason;
    a->look_now = false;
    inspect_follow(&a->inspect, &a->sim, life_find_slot(&a->sim, id));
    char line[LOOKAHEAD_WHY + 32];
    snprintf(line, sizeof(line), "camera: %s (f: camera off)", why);
    say(a, line, 3 * NOTICE_FRAMES);
}

/* The e list, newest first, numbered for the keys that follow. */
static void draw_notices(App *a)
{
    int x, y, w, h;
    int rows = notice_box(a, &x, &y, &w, &h);
    if (!a->notices_open || rows == 0) {
        return;
    }
    page_box(a, x, y, w, h);
    int cols = w / FONT_W - 1;
    cells_canvas_set(&a->canvas, x + FONT_W, y + FONT_H / 2);
    render_text(0, 0, "Notices, newest first: 1-9 or a click follows, e or Esc closes",
                cols - CLOSE_COLUMNS);
    for (int r = 0; r < rows; r++) {
        /* Each row a button pressing its number, the key that follows it.
         * Row r sits below the heading and the half row of margin. r is 0
         * to 8, so '1' + r is a digit character, and a character is an int. */
        /* 48: room for any int, which is what gcc checks, not only 1-9. */
        char tip[48];
        snprintf(tip, sizeof(tip), "follow who it is about (%d)", r + 1);
        ui_add_hit_tip(&a->ui, x, y + FONT_H / 2 + (r + 1) * FONT_H, w, FONT_H,
                       '1' + r, tip);
        const Notice *n = notices_get(&a->notices, (uint32_t)r);
        char line[NOTICE_TEXT + 32];
        int year = years_whole(n->tick, a->sim.set.ticks_per_year) + 1;
        snprintf(line, sizeof(line), "%d  y%-4d %s%s", r + 1, year, n->text,
                 n->creature != NOTICE_NOBODY && life_find_slot(&a->sim, n->creature) < 0
                     ? " (dead)" : "");
        render_text(0, r + 1, line, cols);
    }
    ui_close_button_key(&a->ui, &a->canvas, x, y, w, UI_KEY_CLOSE_NOTICES,
                        "close the notices (e)");
}

/* v: the two branches side by side, in a box at the bottom of the view, so
 * the skip summary at the top stays readable above it. */
static void draw_compare(App *a)
{
    if (!a->compare_open || !a->branches.split) {
        return;
    }
    /* static: about 4 KB, kept off the small web stack. */
    static char lines[COMPARE_LINES][COMPARE_COLS];
    char me = a->branches.shown == 0 ? 'A' : 'B';
    char them = a->branches.shown == 0 ? 'B' : 'A';
    int n = compare_lines(&a->sim, me, &a->branches.other, them,
                          a->branches.split_at, lines, COMPARE_LINES);
    const Layout *l = &a->layout;
    int cols = 0;
    for (int r = 0; r < n; r++) {
        int len = text_columns(lines[r]);
        cols = len > cols ? len : cols;
    }
    /* A last row for the keys; a margin each side and room for the x. */
    int w = (cols + 2 + CLOSE_COLUMNS) * FONT_W, h = (n + 2) * FONT_H;
    if (n == 0 || h > l->view_h || w > l->view_w - 2 * FONT_W) {
        return; /* too small to read */
    }
    int x = l->view_x + FONT_W, y = l->view_y + l->view_h - h - FONT_H / 2;
    page_box(a, x, y, w, h);
    cells_canvas_set(&a->canvas, x + FONT_W, y + FONT_H / 2);
    for (int r = 0; r < n; r++) {
        if (text_columns(lines[r]) == (int)strlen(lines[r])) {
            render_text(0, r, lines[r], cols);
        } else {
            render_utf8(0, r, lines[r]); /* the charts' blocks */
        }
    }
    render_text(0, n, "  Tab shows the other branch | v or Esc closes", cols);
    ui_close_button_key(&a->ui, &a->canvas, x, y, w, UI_KEY_CLOSE_COMPARE,
                        "close the comparison (v)");
}

/* What the ? page lists for the window as it is now. */
static unsigned key_flags(const App *a)
{
    unsigned flags = 0;
    if (a->sandbox.on) {
        flags |= KEYHELP_SANDBOX;
    }
#ifdef VEREDAS_DEMO
    flags |= KEYHELP_DEMO;
#endif
    if (PLATFORM_IS_PAGE) {
        flags |= KEYHELP_PAGE;
    }
    return flags;
}

/* The ? page: every key, two columns side by side when the view is wide
 * enough, else one under the other as far as there is room. */
static void draw_keys(App *a)
{
    if (!a->keys_open) {
        return;
    }
    const Layout *l = &a->layout;
    int x = l->view_x + FONT_W, y = l->view_y + FONT_H / 2;
    int w = l->view_w - 2 * FONT_W, h = l->view_h - FONT_H;
    if (w < FONT_W * 20 || h < FONT_H * 4) {
        return; /* too small to read; the world shows instead */
    }
    page_box(a, x, y, w, h);
    int cols = w / FONT_W - 2;
    int rows = h / FONT_H - 1;
    /* A column's width with a gap of 4 after it. */
    bool side_by_side = cols >= 2 * (KEYHELP_WIDTH + 4);
    cells_canvas_set(&a->canvas, x + FONT_W, y + FONT_H / 2);
    render_text(0, 0, "Every key (? or Esc closes this)", cols);
    int row = 2;
    for (int column = 0; column < 2; column++) {
        const char *lines[KEYHELP_MAX_LINES];
        int n = keyhelp_column(key_flags(a), column, lines, KEYHELP_MAX_LINES);
        int left = side_by_side ? column * (KEYHELP_WIDTH + 4) : 0;
        int top = side_by_side ? 2 : row;
        for (int i = 0; i < n && top + i < rows; i++) {
            render_text(left, top + i, lines[i], cols - left);
        }
        row = top + n + 1; /* one empty row before the second column */
    }
    ui_close_button_key(&a->ui, &a->canvas, x, y, w, UI_KEY_CLOSE_KEYS,
                        "close the key list (?)");
}

static void draw_frame(App *a, float fraction)
{
    /* Every frame records its buttons afresh; ui_end makes them the ones a
     * click is tested against, so a click lands on what was last shown. */
    ui_begin(&a->ui);
    const Creature *followed = update_following(a, fraction);
    if (a->layout.too_small) {
        draw_too_small(a);
        ui_end(&a->ui);
        return;
    }
    const Layout *l = &a->layout;
    scene_draw_world(&a->scene, &a->canvas, &a->camera, &a->sim, fraction,
                     a->last_ms, a->body_ms, l->view_x, l->view_y);
    /* Over the ground and the bodies, under the names and the cursor, so
     * those stay as clear in a snowstorm as without one. */
    weather_fx_draw(&a->canvas, l->view_x, l->view_y, l->view_w, l->view_h,
                    a->weather_fx_kind, a->weather_fx, a->weather_fx_ms);
    scene_draw_cursor(&a->scene, &a->canvas, &a->camera, &a->sim, followed,
                      fraction, l->view_x, l->view_y);
    scene_draw_names(&a->scene, &a->canvas, &a->camera, &a->sim, followed,
                     fraction, l->view_x, l->view_y);
    if (sim_is_over(&a->sim)) {
        scene_draw_game_over(&a->canvas, &a->sim,
                             PLATFORM_IS_PAGE ? "n new world"
                                              : "n new world | q quit",
                             l->view_x, l->view_y, l->view_w, l->view_h);
    }
    if (a->lab_open) {
        lab_draw(&a->scene.sprites, &a->canvas, a->lab_action, a->lab_parts, a->body_ms,
                 l->view_x, l->view_y, l->view_w, l->view_h);
        /* The lab covers the whole map: a click on it reaches nothing behind. */
        ui_add_hit(&a->ui, l->view_x, l->view_y, l->view_w, l->view_h, UI_KEY_BLOCK);
        ui_close_button_key(&a->ui, &a->canvas, l->view_x, l->view_y, l->view_w,
                            UI_KEY_CLOSE_LAB, "close the lab (b)");
    }
    if (a->page.open) {
        draw_page(a);
    }
    draw_sandbox(a);
    draw_notices(a);
    draw_compare(a);
    draw_keys(a);
    /* After the world: a body near the edge may have been drawn over these. */
    draw_title(a, followed);
    draw_panel(a, followed);
    draw_bottom(a);
    ui_tip(&a->ui, &a->canvas, platform_now_ms());
    ui_end(&a->ui);
}

/* The weather's fade and clock, by the wall-clock time since the last frame. */
static void advance_weather_fx(App *a, uint64_t elapsed)
{
    bool active = weather_active(&a->sim.weather, a->sim.tick);
    if (active) {
        a->weather_fx_kind = a->sim.weather.kind;
    }
    float was = a->weather_fx;
    weather_fx_fade(&a->weather_fx, active, elapsed);
    bool still = a->paused || sim_is_over(&a->sim);
    a->weather_fx_ms += still ? elapsed / 2 : elapsed;
    /* A paused window draws only when something changed: a fade under way
     * is a change, so it is seen to its end. Floats compared with != on
     * purpose: any step at all, however small, is a reason to draw. */
    if (a->weather_fx != was) {
        a->redraw = true;
    }
}

static void advance_clocks(App *a)
{
    uint64_t now = platform_now_ms();
    if (!a->paused && !sim_is_over(&a->sim)) {
        a->body_ms += now - a->last_ms;
    }
    advance_weather_fx(a, now - a->last_ms);
    a->last_ms = now;
    if (a->notice_frames > 0 && --a->notice_frames == 0) {
        a->notice[0] = '\0';
        a->redraw = true;
    }
}

/*
 * Whether this frame looks different from the last. Bodies stop moving while
 * paused and only the water, grass and trees go on, and they step only every
 * fifth of a second while paused, so a paused window draws five times a second
 * instead of thirty. Events are still read every frame.
 */
static bool frame_needs_drawing(App *a)
{
    /* A dead world is as still as a paused one, so it costs as little. */
    bool still = a->paused || sim_is_over(&a->sim);
    /* Running, every frame is drawn anyway; the step only matters when still.
     * Switching between the two clocks can cost one extra frame, no more. */
    int ambient = still ? scene_paused_frame(a->last_ms)
                        : scene_ambient_frame(a->last_ms);
    bool needed = !still || a->redraw || ambient != a->last_ambient;
    if (needed) {
        a->last_ambient = ambient;
        a->redraw = false;
    }
    return needed;
}

/* --keys: in the last strlen(keys) frames of --frames, one key a frame, so the
 * frame the screenshot keeps comes after the last of them. A ~ presses
 * nothing: frames of waiting, say for a skip to end. The Escape character
 * (27, $'\e' in a shell) is the Escape key, which is KEY_ESCAPE here. */
static void pretend_keys(App *a)
{
    const char *keys = a->opt->keys;
    if (keys == NULL || a->opt->frames <= 0) {
        return;
    }
    /* strlen gives a size_t; the cast is safe for any text a shell passes. */
    long len = (long)strlen(keys);
    long k = a->frames_done - (a->opt->frames - 1 - len);
    if (k >= 0 && k < len && keys[k] != '~') {
        int key = keys[k] == 27 ? KEY_ESCAPE : keys[k];
        playlog_key(&a->playlog, key);
        handle_key(a, key);
        a->redraw = true;
    }
}

/* One frame. void * because platform_run knows nothing about App. */
static void app_frame(void *arg)
{
    App *a = arg; /* in C, void * converts to any pointer without a cast */
    playlog_frame(&a->playlog, platform_now_ms(), a->sandbox.on);
    Event e;
    while (platform_poll(&e)) {
        handle_event(a, &e);
    }
    pretend_keys(a);
    /* Before any ticks: a quit means the world stays as it was shown. */
    if (!a->running) {
        platform_stop();
        return;
    }
    if (!a->prelude.active && (a->hints & HINT_START) == 0) {
        /* The world is open for the first time: one line on where to start.
         * The camera waits 5 seconds more than it would, so its own line
         * does not replace this one at once. Apart from the chain below, so
         * this frame's ticks run as in any other. */
        a->hints |= HINT_START;
        say(a, a->sandbox.on ? "brown buttons change the world; hover one to see its key"
                             : "buttons along the bottom; hover one for its key; "
                               "Sandbox changes the world",
            3 * NOTICE_FRAMES);
        a->idle_frames = LOOK_IDLE_FRAMES - 5 * FRAMES_PER_SECOND;
    }
    if (a->prelude.active && a->sandbox.menu == SANDBOX_MENU_SETUP) {
        /* The setup screen holds the ancestors: Enter starts them again. */
        a->notice[0] = '\0';
    } else if (a->prelude.active) {
        /* As many prelude ticks as fit in 28 ms of the frame's 33, so the
         * window and the keys still keep up; pausing does not hold it. */
        app_prelude_step(&a->prelude, &a->sim, platform_now_ms,
                         APP_PRELUDE_FRAME_MS);
        if (a->prelude.active) {
            app_prelude_text(&a->prelude, &a->sim, a->notice, sizeof(a->notice));
            a->notice_frames = 0; /* set again every frame */
        } else {
            a->notice[0] = '\0';
        }
        a->redraw = true;
    } else if (sandbox_skipping(&a->sandbox)) {
        /* A skip runs like the prelude, a slice a frame, paused or not. */
        /* Split, the other branch gets half the frame (below), so both
         * move ahead together and the skip covers both. */
        sandbox_skip_step(&a->sandbox, &a->sim, platform_now_ms,
                          a->branches.split ? APP_PRELUDE_FRAME_MS / 2
                                            : APP_PRELUDE_FRAME_MS);
        a->ticks_due = 0;
        a->redraw = true;
    } else if (!a->paused && !sim_is_over(&a->sim)) {
        a->ticks_due += APP_SPEEDS[a->speed];
        /* Checked on every tick: at high speed a frame runs several, and the
         * world may end during the first. */
        while (a->ticks_due >= FRAMES_PER_SECOND && !sim_is_over(&a->sim)) {
            sim_step(&a->sim);
            a->ticks_due -= FRAMES_PER_SECOND;
        }
        if (sim_is_over(&a->sim)) {
            a->ticks_due = 0;  /* nothing is on its way any more */
            a->redraw = true;  /* the frame that shows the screen */
        }
    }
    /* The world behind the one on screen keeps level with it: the ticks the
     * shown world ran this frame, or as many as half a frame allows. */
    if (!branch_caught_up(&a->branches, &a->sim)) {
        branch_catch_up(&a->branches, &a->sim, platform_now_ms,
                        APP_PRELUDE_FRAME_MS / 2);
    }
    /* A skip that ended this frame while split: the comparison is what the
     * player skipped for. */
    bool skipping = sandbox_skipping(&a->sandbox);
    if (a->was_skipping && !skipping && a->branches.split) {
        a->compare_open = true;
    }
    a->was_skipping = skipping;
    update_look(a);
    /* Counted up to the wait and no further: an int that kept growing would
     * pass INT_MAX after about two years of frames, and signed overflow is
     * undefined behaviour, not a wrap round. */
    if (a->idle_frames < LOOK_IDLE_FRAMES) {
        a->idle_frames++;
    }
    family_update(&a->families, &a->sim);
    update_notices(a);
    char weather_line[200];
    if (!a->prelude.active &&
        sandbox_weather_check(&a->sandbox, &a->sim, weather_line, sizeof(weather_line))) {
        say(a, weather_line, 3 * NOTICE_FRAMES);
    }
    /* The bodies' motion is worked out on the body clock as it was before
     * this frame's step, as it always has been. */
    uint64_t body_before = a->body_ms;
    advance_clocks(a);

    if (frame_needs_drawing(a)) {
        /* Only for a frame that is drawn: nothing else reads the motion.
         * Running, every frame is drawn, so this changes nothing there;
         * paused, it saves two updates in three. */
        scene_update(&a->scene, &a->sim, APP_SPEEDS[a->speed], body_before);
        float fraction = (float)a->ticks_due / (float)FRAMES_PER_SECOND;
        draw_frame(a, fraction);
        platform_present(&a->canvas);
    }

    /* The same way out as the window's X button, so --frames tests it. */
    a->frames_done++;
    if (a->opt->frames > 0 && a->frames_done == a->opt->frames) {
        platform_post_quit();
    }
}

/* The zoom step with this name; false when there is none. */
static bool zoom_from_name(const char *name, int *level)
{
    static const char *const NAMES[CAMERA_LEVELS] = {"close", "normal", "far",
                                                     "world"};
    for (int i = 0; i < CAMERA_LEVELS; i++) {
        if (strcmp(name, NAMES[i]) == 0) {
            *level = i;
            return true;
        }
    }
    return false;
}

/* A seed for when none is given: the clock, mixed. time() has only
 * seconds, which is enough for a program a person starts by hand. */
static uint64_t seed_from_clock(void)
{
    uint64_t state = (uint64_t)time(NULL);
    return rng_splitmix64(&state);
}

/* Settings from the file and options; false with err filled in. */
static bool read_settings(const Options *o, Settings *settings, char *err,
                          size_t errlen)
{
    *settings = settings_default();
    if (o->config_path != NULL &&
        !settings_load(settings, o->config_path, err, errlen)) {
        return false;
    }
    if (o->no_floor) {
        settings->min_creatures = 0;
    }
#ifdef VEREDAS_DEMO
    settings->pool_capacity = DEMO_POOL_CAPACITY;
#endif
    return true;
}

/* What happens on leaving, in the order that keeps a failure visible. */
static int app_leave(App *a)
{
    int result = EXIT_FINE;
    char err[512];
    if (a->failed) {
        platform_error("out of memory while building a new world");
        result = EXIT_FAILED;
    }
    if (a->opt->screenshot_path != NULL &&
        !canvas_write_ppm(&a->canvas, a->opt->screenshot_path)) {
        platform_error("could not write the screenshot");
        result = EXIT_FAILED;
    }
    if (!a->failed && a->opt->save_path != NULL &&
        !save_write(&a->sim, a->opt->save_path, err, sizeof(err))) {
        platform_error(err);
        result = EXIT_FAILED;
    }
    if (a->opt->playlog_path != NULL &&
        !playlog_write(&a->playlog, a->opt->playlog_path, err, sizeof(err))) {
        platform_error(err);
        result = EXIT_FAILED;
    }
    if (a->opt->hits) {
        /* x y w h key, one button a line; the key as handle_key takes it. */
        for (int i = 0; i < a->ui.n_shown; i++) {
            const UiHit *h = &a->ui.shown[i];
            printf("hit %d %d %d %d %d\n", h->x, h->y, h->w, h->h, h->key);
        }
    }
    if (a->opt->checksum) {
        printf("checksum 0x%016" PRIx64 "\n", sim_checksum(&a->sim));
    }
    return result;
}

int main(int argc, char **argv)
{
    /* static, like app below: in a browser page main never finishes and its
     * stack is reused, while frames keep reading the options through app.opt.
     * A plain local here would be a pointer to memory that changes under us. */
    static Options opt;
    char err[512];
    if (!options_parse(argc, argv, &opt, err, sizeof(err))) {
        char message[600];
        snprintf(message, sizeof(message), "%s (see veredas-window --help)", err);
        platform_error(message);
        return EXIT_USAGE;
    }
    if (opt.help) {
        fputs(WINDOW_USAGE, stdout);
        return EXIT_FINE;
    }
    if (opt.version) {
        printf("veredas-window %s\n", VEREDAS_VERSION);
        return EXIT_FINE;
    }
    if (opt.once || opt.list_settings) {
        platform_error("--once and --settings are for veredas");
        return EXIT_USAGE;
    }
    if (PLATFORM_IS_PAGE && (opt.save_path != NULL || opt.load_path != NULL)) {
        platform_error("a browser page has nowhere to save or load from");
        return EXIT_USAGE;
    }
    if (PLATFORM_IS_PAGE && opt.playlog_path != NULL) {
        platform_error("a browser page has nowhere to write a playlog");
        return EXIT_USAGE;
    }
    if (opt.code != NULL && opt.load_path != NULL) {
        platform_error("--code and --load each choose the world: give one");
        return EXIT_USAGE;
    }
    int zoom = CAMERA_NORMAL;
    if (opt.zoom_name != NULL && !zoom_from_name(opt.zoom_name, &zoom)) {
        platform_error("--zoom needs close, normal, far or world");
        return EXIT_USAGE;
    }
    Settings settings;
    if (!read_settings(&opt, &settings, err, sizeof(err))) {
        platform_error(err);
        return EXIT_USAGE;
    }

    /* static: this big struct lives for the whole program, not on main's
     * stack, and starts as all zeros. */
    static App app;
    app.opt = &opt;
    ui_init(&app.ui);
    app.paused = opt.paused;
    app.sandbox.on = opt.sandbox;
    /* --frames runs make pictures for tests, which a camera choosing whom to
     * follow would change; f still turns it on there. */
    app.look_on = !opt.sandbox && opt.frames == 0;
    /* Counted as idle from the start: the first pick comes as soon as the
     * copy has run its season ahead. */
    app.idle_frames = LOOK_IDLE_FRAMES;
    uint64_t seed = opt.seed_given ? opt.seed : seed_from_clock();
    int world_w = opt.size_given ? opt.width : DEFAULT_WORLD_W;
    int world_h = opt.size_given ? opt.height : DEFAULT_WORLD_H;
    if (!app_open_world(&app.sim, &opt, &settings, seed, world_w, world_h, err,
                        sizeof(err))) {
        platform_error(err);
        return EXIT_FAILED;
    }
    rng_seed(&app.seed_source, app.sim.seed);
    /* A season ahead. The cast: ticks_per_year is an int, the lead a tick
     * count (uint64_t); settings keep it positive, so nothing is lost. */
    lookahead_init(&app.look, (uint64_t)(app.sim.set.ticks_per_year / 4));
    /* With --frames (tests and screenshots) the prelude runs whole before the
     * first frame, and only when --ancestors asks for it: a picture must come
     * out the same every time, which a prelude cut by a clock would not. */
    app.prelude_years = app_prelude_years(&opt, &settings, opt.frames == 0);
    /* With --code this first world is only a stand-in until the code's world
     * replaces it below, so its ancestors are not run. */
    if (opt.code == NULL) {
        app_prelude_start(&app.prelude, &app.sim, app.prelude_years);
    }
#ifndef VEREDAS_DEMO
    /* --sandbox opens on its setup screen: a new world's settings are chosen
     * before its ancestors run. Not for a loaded world, which has lived, nor
     * for a code's, whose settings come with it. */
    if (opt.sandbox && opt.load_path == NULL && opt.code == NULL) {
        sandbox_open_setup(&app.sandbox, &app.sim.set);
    }
#endif
    if (opt.frames > 0) {
        app_prelude_finish(&app.prelude, &app.sim);
    }
    app.speed = opt.speed > 0 ? app_speed_step(opt.speed) : APP_DEFAULT_SPEED;
    app.running = true;
    app.redraw = true;

    if (!scene_init(&app.scene, &app.sim) ||
        !family_init(&app.families, app.sim.pool.capacity) ||
        !canvas_init(&app.canvas, WINDOW_W, WINDOW_H)) {
        platform_error("out of memory");
        return EXIT_FAILED;
    }
    sprite_cache_set_art(&app.scene.sprites, !opt.no_art);
    scene_layout(&app.layout, WINDOW_W, WINDOW_H);
    camera_init(&app.camera, app.sim.world.w, app.sim.world.h,
                app.layout.view_w, app.layout.view_h);
    app.camera.level = zoom;
    if (opt.code != NULL && !open_code_text(&app, opt.code, err, sizeof(err))) {
#ifdef __EMSCRIPTEN__
        /* A link that will not open should not leave a dead page: the visitor
         * gets a new world, and the reason goes under it, on the page (the
         * ancestors' progress line would write over a line said on screen). */
        char message[200];
        snprintf(message, sizeof(message), "That link did not open (%s), so this "
                                           "is a new world.", err);
        platform_page_note(message);
        app_prelude_start(&app.prelude, &app.sim, app.prelude_years);
#else
        char message[600];
        snprintf(message, sizeof(message), "--code: %s", err);
        platform_error(message);
        return EXIT_USAGE;
#endif
    }
    if (!platform_open("Veredas", WINDOW_W, WINDOW_H, err, sizeof(err))) {
        platform_error(err);
        return EXIT_FAILED;
    }
    app.last_ms = platform_now_ms();
    playlog_init(&app.playlog);

    platform_run(app_frame, &app, opt.frames > 0 ? 0 : FRAMES_PER_SECOND);

    int result = app_leave(&app);
    platform_close();
    canvas_free(&app.canvas);
    family_free(&app.families);
    scene_free(&app.scene);
    sandbox_free(&app.sandbox);
    branch_free(&app.branches);
    lookahead_free(&app.look);
    sim_free(&app.sim);
    return result;
}
