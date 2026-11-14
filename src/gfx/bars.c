#include "bars.h"

#include <stdio.h> /* snprintf */

#include "icons.h"
#include "palette.h"
#include "text.h"

#define EDGE 2        /* air at the bar's two ends */
#define BUTTON_GAP 2  /* between buttons of one group */
#define GROUP_GAP 10  /* between groups */
#define LABEL_PAD 4   /* each side of the speed's "2/s" */
#define MAX_ITEMS 24

enum { GROUP_WATCH, GROUP_SANDBOX, GROUP_RIGHT };

/*
 * One thing on the bar: a button, or (label) the speed's words between its
 * - and + buttons, which are not a button. keep orders what goes when the
 * window is too narrow: the lowest keep goes first.
 */
typedef struct {
    UiButton b;
    int group;
    int keep;
    bool label;
    bool shown; /* still on the bar after the narrowing */
} Item;

typedef struct {
    Item items[MAX_ITEMS];
    int n;
} Items;

static void add(Items *list, int group, int keep, UiButton b)
{
    if (list->n >= MAX_ITEMS) {
        return;
    }
    /* A compound literal, (Item){...}, is an unnamed struct value built on the
     * spot; assigning it copies every field. Fields not named become 0. */
    list->items[list->n++] = (Item){.b = b, .group = group, .keep = keep,
                                    .shown = true};
}

/* The buttons for this state, left to right. The strings are literals or
 * live in speed_word, which the caller keeps alive while the bar is drawn. */
static void list_items(Items *list, const BottomState *st, char *speed_word)
{
    list->n = 0;
    add(list, GROUP_WATCH, 100,
        (UiButton){st->paused ? ICON_PLAY : ICON_PAUSE, st->paused ? "play" : "pause",
                   st->paused ? "play (space)" : "pause (space)", ' ', false,
                   st->paused, false});
    add(list, GROUP_WATCH, 55,
        (UiButton){ICON_SLOWER, NULL, "slower (-)", '-', false, false, false});
    add(list, GROUP_WATCH, 50, (UiButton){-1, speed_word, NULL, UI_NONE, false, false, false});
    list->items[list->n - 1].label = true;
    add(list, GROUP_WATCH, 55,
        (UiButton){ICON_FASTER, NULL, "faster (+)", '+', false, false, false});
    add(list, GROUP_WATCH, 30,
        (UiButton){ICON_ZOOM_OUT, NULL, "zoom out ([)", '[', false, false, false});
    add(list, GROUP_WATCH, 30,
        (UiButton){ICON_ZOOM_IN, NULL, "zoom in (])", ']', false, false, false});
    add(list, GROUP_WATCH, 58,
        (UiButton){ICON_STORY, "story", "life story (l)", 'l', false, false, false});
    add(list, GROUP_WATCH, 57,
        (UiButton){ICON_NOTICES, "notices", "notices (e)", 'e', false, false, false});
    add(list, GROUP_WATCH, 40,
        (UiButton){ICON_CAMERA, "camera", "camera picks who to follow (f)", 'f',
                   false, st->look_on, false});
    add(list, GROUP_WATCH, 20,
        (UiButton){ICON_JUMP, "jump", "jump to the newest notice (j)", 'j', false,
                   false, false});
    if (st->sandbox) {
        add(list, GROUP_SANDBOX, 80,
            (UiButton){ICON_TRAIT, "give trait",
                       st->following ? "give a trait (g)"
                                     : "give a trait (g): follow an animal first",
                       'g', st->demo || !st->following, false, true});
        add(list, GROUP_SANDBOX, 79,
            (UiButton){ICON_WEATHER, "weather", "weather (w)", 'w', false, false, true});
        add(list, GROUP_SANDBOX, 70,
            (UiButton){ICON_SKIP, "skip", "skip ahead (k)", 'k', st->demo, false, true});
        add(list, GROUP_SANDBOX, 69,
            (UiButton){ICON_BRANCH, st->split ? "keep this" : "branch",
                       st->split ? "keep this branch (x twice)" : "split in two (x)",
                       'x', st->demo, false, true});
        if (st->split) {
            add(list, GROUP_SANDBOX, 68,
                (UiButton){ICON_OTHER_BRANCH, "other", "other branch (Tab)", '\t',
                           false, false, true});
            add(list, GROUP_SANDBOX, 67,
                (UiButton){ICON_COMPARE, "compare", "compare the branches (v)", 'v',
                           false, false, true});
        }
        add(list, GROUP_SANDBOX, 60,
            (UiButton){ICON_NEW_START, "new start", "new start (u)", 'u', st->demo,
                       false, true});
    }
    add(list, GROUP_RIGHT, 95,
        (UiButton){st->sandbox ? ICON_MODE_SANDBOX : ICON_MODE_WATCH,
                   st->sandbox ? "Sandbox > Documentary" : "Documentary > Sandbox",
                   st->sandbox ? "switch to Documentary (m)" : "switch to Sandbox (m)",
                   'm', false, false, st->sandbox});
    add(list, GROUP_RIGHT, 85,
        (UiButton){ICON_WORLD, "world", "world: new, code, save, quit", UI_KEY_WORLD_MENU,
                   false, st->world_menu_open, false});
    add(list, GROUP_RIGHT, 90,
        (UiButton){ICON_HELP, NULL, "every key (?)", '?', false, false, false});
}

/* An item's width with or without its word. */
static int item_width(const Item *it, bool words)
{
    if (it->label) {
        return text_width(it->b.word) + 2 * LABEL_PAD;
    }
    UiButton b = it->b; /* a copy: changing its word leaves the list alone */
    if (!words && b.icon >= 0) {
        b.word = NULL;
    }
    return ui_button_width(&b);
}

/* The width the shown items of one side take: the left groups, or the right. */
static int side_width(const Items *list, bool right, bool words)
{
    int w = 0, last_group = -1;
    for (int i = 0; i < list->n; i++) {
        const Item *it = &list->items[i];
        if (!it->shown || (it->group == GROUP_RIGHT) != right) {
            continue;
        }
        if (last_group >= 0) {
            w += it->group == last_group ? BUTTON_GAP : GROUP_GAP;
        }
        w += item_width(it, words);
        last_group = it->group;
    }
    return w;
}

static bool fits(const Items *list, int w, bool words)
{
    return EDGE + side_width(list, false, words) + GROUP_GAP +
               side_width(list, true, words) + EDGE <= w;
}

/* Hides the shown item with the lowest keep. False when none is left to hide
 * but the last few, which always stay. */
static bool hide_least_needed(Items *list)
{
    int least = -1;
    for (int i = 0; i < list->n; i++) {
        const Item *it = &list->items[i];
        if (it->shown && it->keep < 90 &&
            (least < 0 || it->keep < list->items[least].keep)) {
            least = i;
        }
    }
    if (least < 0) {
        return false;
    }
    list->items[least].shown = false;
    return true;
}

/* Draws the shown items of one side from x, returning the x after them. */
static int draw_side(Ui *u, Canvas *c, int x, int y, const Items *list, bool right,
                     bool words)
{
    int last_group = -1;
    for (int i = 0; i < list->n; i++) {
        const Item *it = &list->items[i];
        if (!it->shown || (it->group == GROUP_RIGHT) != right) {
            continue;
        }
        if (last_group >= 0) {
            x += it->group == last_group ? BUTTON_GAP : GROUP_GAP;
        }
        last_group = it->group;
        if (it->label) {
            text_draw(c, x + LABEL_PAD, y + (UI_BUTTON_H - TEXT_H) / 2, it->b.word,
                      canvas_rgb(PALETTE_BUTTON_TEXT.r, PALETTE_BUTTON_TEXT.g,
                                 PALETTE_BUTTON_TEXT.b));
            x += item_width(it, words);
            continue;
        }
        UiButton b = it->b;
        if (!words && b.icon >= 0) {
            b.word = NULL;
        }
        x += ui_button(u, c, x, y, &b);
    }
    return x;
}

void bar_bottom_draw(Ui *u, Canvas *c, int y, int w, const BottomState *st)
{
    canvas_rect(c, 0, y, w, UI_BAR_H,
                canvas_rgb(PALETTE_TEXT_BACKGROUND.r, PALETTE_TEXT_BACKGROUND.g,
                           PALETTE_TEXT_BACKGROUND.b));
    char speed_word[16];
    snprintf(speed_word, sizeof(speed_word), "%d/s", st->speed);
    /* static: an Items is ~2 KB, fine on the stack too, but this keeps the
     * frame's stack small in the browser, whose stack is 1 MB. */
    static Items list;
    list_items(&list, st, speed_word);
    bool words = fits(&list, w, true);
    while (!words && !fits(&list, w, false) && hide_least_needed(&list)) {
        /* each pass hides one more button */
    }
    int top = y + (UI_BAR_H - UI_BUTTON_H) / 2;
    draw_side(u, c, EDGE, top, &list, false, words);
    if (fits(&list, w, words)) {
        draw_side(u, c, w - EDGE - side_width(&list, true, words), top, &list, true,
                  words);
    }
}

void bar_world_menu_draw(Ui *u, Canvas *c, int right_x, int bottom_y, bool page)
{
    /* A struct array initialised in place; page has the first three only. */
    static const struct {
        const char *word, *tip;
        int key;
    } ROWS[] = {
        {"new world", "a new world (r)", 'r'},
        {"copy code", "copy this world's code (c)", 'c'},
        {"open code", "open a world's code (o)", 'o'},
        {"save", "save (s)", 's'},
        {"quit", "quit (q)", 'q'},
    };
    int n = page ? 3 : 5;
    int widest = 0;
    for (int i = 0; i < n; i++) {
        UiButton b = {-1, ROWS[i].word, ROWS[i].tip, ROWS[i].key, false, false, false};
        int bw = ui_button_width(&b);
        widest = bw > widest ? bw : widest;
    }
    int box_w = widest + 8;
    int box_h = n * (UI_BUTTON_H + 2) + 6;
    int x = right_x - box_w, y = bottom_y - box_h;
    ui_box(c, x, y, box_w, box_h,
           canvas_rgb(PALETTE_TEXT_BACKGROUND.r, PALETTE_TEXT_BACKGROUND.g,
                      PALETTE_TEXT_BACKGROUND.b),
           canvas_rgb(PALETTE_BUTTON_EDGE.r, PALETTE_BUTTON_EDGE.g,
                      PALETTE_BUTTON_EDGE.b));
    for (int i = 0; i < n; i++) {
        UiButton b = {-1, ROWS[i].word, ROWS[i].tip, ROWS[i].key, false, false, false};
        ui_button_sized(u, c, x + 4, y + 4 + i * (UI_BUTTON_H + 2), widest, &b);
    }
}
