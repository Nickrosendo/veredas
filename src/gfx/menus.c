#include "menus.h"

#include <stdio.h> /* snprintf */

#include "actions.h"   /* ACTIONS_GIVABLE, actions_gene_name */
#include "chronicle.h" /* name_from_id */
#include "icons.h"
#include "palette.h"
#include "platform.h"  /* KEY_ESCAPE: only the constant */
#include "text.h"
#include "weather.h"

#define PAD 8
#define ROW (UI_BUTTON_H + 2) /* a row of buttons, with 2 pixels between rows */
#define VALUE_BAR_W 100

static uint32_t px_of(Colour k)
{
    return canvas_rgb(k.r, k.g, k.b);
}

/* The menu's box with its title and, when closable, the close button; returns
 * the y of the first row under the title. The setup screen has its own way
 * out, "Keep this world", which presses the same Escape. */
static int frame(Ui *u, Canvas *c, int x, int y, int h, const char *title,
                 bool closable)
{
    ui_box(c, x, y, MENU_W, h, px_of(PALETTE_TEXT_BACKGROUND),
           px_of(PALETTE_SANDBOX_EDGE));
    ui_add_hit(u, x, y, MENU_W, h, UI_KEY_BLOCK);
    char cut[128];
    /* the title stops short of the close button: 16 pixels and some air */
    text_cut(title, MENU_W - 2 * PAD - 24, cut, sizeof(cut));
    text_draw(c, x + PAD, y + 4, cut, px_of(PALETTE_BUTTON_TEXT));
    if (closable) {
        ui_close_button(u, c, x, y, MENU_W);
    }
    return y + 4 + TEXT_H + 6;
}

static void line(Canvas *c, int x, int y, const char *text)
{
    char cut[160];
    text_cut(text, MENU_W - 2 * PAD, cut, sizeof(cut));
    text_draw(c, x, y + (UI_BUTTON_H - TEXT_H) / 2, cut, px_of(PALETTE_TEXT));
}

/* A plain brown button with a word, no icon. */
static int word_button(Ui *u, Canvas *c, int x, int y, const char *word,
                       const char *tip, int key, bool pressed)
{
    UiButton b = {-1, word, tip, key, false, pressed, true};
    return ui_button(u, c, x, y, &b);
}

/* The value as a bar VALUE_BAR_W wide, filled to v (0..1), with its number. */
static int value_bar(Canvas *c, int x, int y, float v)
{
    int top = y + 6;
    ui_box(c, x, top, VALUE_BAR_W, UI_BUTTON_H - 12, px_of(PALETTE_SANDBOX_FILL),
           px_of(PALETTE_SANDBOX_EDGE));
    float held = v < 0.0f ? 0.0f : v > 1.0f ? 1.0f : v;
    canvas_rect(c, x + 1, top + 1, (int)((float)(VALUE_BAR_W - 2) * held + 0.5f),
                UI_BUTTON_H - 14, px_of(PALETTE_SANDBOX_EDGE));
    char number[16];
    snprintf(number, sizeof(number), "%.2f", (double)v);
    return VALUE_BAR_W + 4 +
           (text_draw(c, x + VALUE_BAR_W + 4, y + 2, number,
                      px_of(PALETTE_BUTTON_TEXT)) - (x + VALUE_BAR_W + 4));
}

/* Each menu's height: the title, its rows, the padding under them. One
 * place for them, so placing a menu and drawing it agree. */
static int menu_height(int menu)
{
    int title = 4 + TEXT_H + 6;
    switch (menu) {
    case SANDBOX_MENU_TRAIT: return title + (ACTIONS_GIVABLE_COUNT + 2) * ROW + PAD;
    case SANDBOX_MENU_WEATHER:
        return title + (WEATHER_COUNT - 1) * (ROW + TEXT_H + 4) + PAD;
    case SANDBOX_MENU_SKIP: return title + 2 * ROW + PAD;
    /* wildness, jumps, the heading, the traits, half a row of air, the two
     * buttons */
    case SANDBOX_MENU_SETUP:
        return title + (3 + ACTIONS_GIVABLE_COUNT) * ROW + ROW / 2 + ROW + PAD;
    default: return 0;
    }
}

static void draw_trait(Ui *u, Canvas *c, int x, int y, const Sandbox *b)
{
    int h = menu_height(SANDBOX_MENU_TRAIT);
    char name[16], title[96];
    name_from_id(b->target, name, sizeof(name));
    snprintf(title, sizeof(title), "Give %s a trait. Its young inherit it.", name);
    int row = frame(u, c, x, y, h, title, true);
    /* One button per trait, its digit in the word so the key is learnt too. */
    for (int i = 0; i < ACTIONS_GIVABLE_COUNT; i++) {
        char word[40], tip[48];
        const char *gene = actions_gene_name(ACTIONS_GIVABLE[i]);
        snprintf(word, sizeof(word), "%d %s", i + 1, gene);
        snprintf(tip, sizeof(tip), "%s (%d)", gene, i + 1);
        UiButton btn = {-1, word, tip, '1' + i, false, i == b->trait, true};
        ui_button_sized(u, c, x + PAD, row, 160, &btn);
        row += ROW;
    }
    /* The value: - bar + */
    int at = x + PAD;
    at += word_button(u, c, at, row, "-", "less (,)", ',', false) + 4;
    at += value_bar(c, at, row, b->value) + 4;
    word_button(u, c, at, row, "+", "more (.)", '.', false);
    row += ROW;
    at = x + PAD;
    at += word_button(u, c, at, row, "Give", "give it (Enter)", '\r', false) + 8;
    word_button(u, c, at, row, "s sea body", "a sea body, or a land body back (s)",
                's', false);
}

static void draw_weather(Ui *u, Canvas *c, int x, int y)
{
    /* What each event does, from weather.h's own comments. */
    static const char *const WHAT[WEATHER_COUNT] = {
        [WEATHER_DROUGHT] = "plants grow less, soil takes less rain",
        [WEATHER_HARD_WINTER] = "colder: more cold ground, cold costs more",
        [WEATHER_WARM_YEAR] = "milder: less cold ground, plants grow more",
    };
    static const int ICONS[WEATHER_COUNT] = {
        [WEATHER_DROUGHT] = ICON_DROUGHT,
        [WEATHER_HARD_WINTER] = ICON_HARD_WINTER,
        [WEATHER_WARM_YEAR] = ICON_WARM_YEAR,
    };
    int h = menu_height(SANDBOX_MENU_WEATHER);
    int row = frame(u, c, x, y, h, "Weather for one season", true);
    for (int k = WEATHER_NONE + 1; k < WEATHER_COUNT; k++) {
        char word[32], tip[48];
        snprintf(word, sizeof(word), "%d %s", k, weather_name(k));
        snprintf(tip, sizeof(tip), "%s (%d)", weather_name(k), k);
        UiButton btn = {ICONS[k], word, tip, '0' + k, false, false, true};
        ui_button_sized(u, c, x + PAD, row, MENU_W - 2 * PAD, &btn);
        row += ROW;
        line(c, x + PAD + 4, row - 2, WHAT[k]);
        row += TEXT_H + 4;
    }
}

static void draw_skip(Ui *u, Canvas *c, int x, int y)
{
    int h = menu_height(SANDBOX_MENU_SKIP);
    int row = frame(u, c, x, y, h, "Skip ahead", true);
    int at = x + PAD;
    for (int i = 0; i < 3; i++) {
        int years = sandbox_skip_years(i);
        char word[24], tip[40];
        snprintf(word, sizeof(word), "%d %s", years, years == 1 ? "year" : "years");
        snprintf(tip, sizeof(tip), "skip %s (%d)", word, i + 1);
        UiButton btn = {ICON_SKIP, word, tip, '1' + i, false, false, true};
        at += ui_button(u, c, at, row, &btn) + 6;
    }
    line(c, x + PAD, row + ROW, "20 years takes a minute or two");
}

/* A row of the setup's traits: a button with a checkbox drawn in its left end. */
static void check_row(Ui *u, Canvas *c, int x, int y, int i, bool checked)
{
    char word[48], tip[48];
    const char *gene = actions_gene_name(ACTIONS_GIVABLE[i]);
    /* Six spaces: the checkbox's room, the font's space being 4 pixels. */
    snprintf(word, sizeof(word), "      %d %s", i + 1, gene);
    snprintf(tip, sizeof(tip), "%s may jump (%d)", gene, i + 1);
    UiButton btn = {-1, word, tip, '1' + i, false, false, true};
    ui_button_sized(u, c, x, y, 200, &btn);
    int bx = x + 4, by = y + 5;
    ui_box(c, bx, by, MENU_CHECK_SIZE, MENU_CHECK_SIZE, px_of(PALETTE_TEXT_BACKGROUND),
           px_of(PALETTE_SANDBOX_EDGE));
    if (checked) {
        canvas_rect(c, bx + 2, by + 2, MENU_CHECK_SIZE - 4, MENU_CHECK_SIZE - 4,
                    px_of(PALETTE_BUTTON_TEXT));
    }
}

static void draw_setup(Ui *u, Canvas *c, int x, int y, const Sandbox *b)
{
    int h = menu_height(SANDBOX_MENU_SETUP);
    int row = frame(u, c, x, y, h, "A new start: how bodies change", false);
    char text[64];
    int at = x + PAD;
    at += word_button(u, c, at, row, "-", "less wild (,)", ',', false) + 4;
    at += word_button(u, c, at, row, "+", "wilder (.)", '.', false) + 8;
    snprintf(text, sizeof(text), "wildness x%.1f (1 is nature's own)",
             (double)sandbox_wild_scale(b->wild_step));
    line(c, at, row, text);
    row += ROW;
    at = x + PAD;
    at += word_button(u, c, at, row, "-", "fewer jumps (-)", '-', false) + 4;
    at += word_button(u, c, at, row, "+", "more jumps (+)", '+', false) + 8;
    snprintf(text, sizeof(text), "jumps: %.0f %% of changes",
             (double)(sandbox_jump_chance(b->jump_step) * 100.0f));
    line(c, at, row, text);
    row += ROW;
    line(c, x + PAD, row, "traits that may jump to any value:");
    row += ROW;
    for (int i = 0; i < ACTIONS_GIVABLE_COUNT; i++) {
        /* >> before &: shift the gene's bit down to the lowest place, then
         * keep only that bit. */
        bool on = ((b->jump_genes >> ACTIONS_GIVABLE[i]) & 1) != 0;
        check_row(u, c, x + PAD, row, i, on);
        row += ROW;
    }
    row += ROW / 2;
    at = x + PAD;
    at += word_button(u, c, at, row, "Start again", "start again from the first tick "
                      "(Enter)", '\r', false) + 6;
    word_button(u, c, at, row, "Keep this world", "keep this world (Esc)", KEY_ESCAPE,
                false);
}

bool menu_is_drawn(const Sandbox *b)
{
    if (sandbox_skipping(b)) {
        return false;
    }
    return b->menu == SANDBOX_MENU_TRAIT || b->menu == SANDBOX_MENU_WEATHER ||
           b->menu == SANDBOX_MENU_SKIP || b->menu == SANDBOX_MENU_SETUP;
}

bool menu_place(int area_x, int area_y, int area_w, int area_h, int w, int h,
                int *x, int *y)
{
    if (w > area_w - 2 * MENU_MARGIN || h > area_h - 2 * MENU_MARGIN) {
        return false;
    }
    /* Where it wants to be, then moved left and up just enough to end inside
     * the area. It fits, so moving never takes it past the area's left or top. */
    int right = area_x + area_w - MENU_MARGIN, bottom = area_y + area_h - MENU_MARGIN;
    *x = area_x + MENU_INSET_X;
    *y = area_y + MENU_INSET_Y;
    if (*x + w > right) {
        *x = right - w;
    }
    if (*y + h > bottom) {
        *y = bottom - h;
    }
    return true;
}

/* In place of a menu the area cannot hold: one line saying so, with its x.
 * The menu's keys still work; only its buttons are missing. */
static void draw_too_small(Ui *u, Canvas *c, int area_x, int area_y, int area_w)
{
    int w = area_w - 2 * MENU_MARGIN < MENU_W ? area_w - 2 * MENU_MARGIN : MENU_W;
    int h = 4 + TEXT_H + 4;
    int x = area_x + MENU_MARGIN, y = area_y + MENU_MARGIN;
    ui_box(c, x, y, w, h, px_of(PALETTE_TEXT_BACKGROUND), px_of(PALETTE_SANDBOX_EDGE));
    ui_add_hit(u, x, y, w, h, UI_KEY_BLOCK);
    char cut[64];
    text_cut("too small for the menu: widen or Esc", w - 2 * PAD - 24, cut,
             sizeof(cut));
    text_draw(c, x + PAD, y + 4, cut, px_of(PALETTE_BUTTON_TEXT));
    ui_close_button(u, c, x, y, w);
}

void menu_draw(Ui *u, Canvas *c, int area_x, int area_y, int area_w, int area_h,
               const Sandbox *b, const Sim *s, bool demo)
{
    (void)s;    /* the menus read the Sandbox's own copy of what they show */
    (void)demo; /* the demo opens only the weather menu, which is the same */
    if (!menu_is_drawn(b)) {
        return;
    }
    int x, y;
    if (!menu_place(area_x, area_y, area_w, area_h, MENU_W, menu_height(b->menu),
                    &x, &y)) {
        draw_too_small(u, c, area_x, area_y, area_w);
        return;
    }
    switch (b->menu) {
    case SANDBOX_MENU_TRAIT: draw_trait(u, c, x, y, b); break;
    case SANDBOX_MENU_WEATHER: draw_weather(u, c, x, y); break;
    case SANDBOX_MENU_SKIP: draw_skip(u, c, x, y); break;
    case SANDBOX_MENU_SETUP: draw_setup(u, c, x, y, b); break;
    default: break;
    }
}
