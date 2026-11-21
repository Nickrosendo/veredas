#include "card.h"

#include <stdio.h> /* snprintf */
#include <string.h>

#include "actions.h"   /* ACTIONS_GIVABLE, actions_gene_name */
#include "chronicle.h" /* name_from_id */
#include "doing.h"
#include "icons.h"
#include "palette.h"
#include "panel.h"     /* panel_hue_band, panel_band_name */
#include "species.h"
#include "sprite.h"
#include "text.h"
#include "years.h"

#define PAD 4         /* the card's margin, left and right */
#define LINE_H 18     /* a line of text: 16 pixels of letters and 2 of air */
#define BAR_H 6       /* an energy, health or trait bar */
#define TRAIT_H 12    /* one trait's row */
#define TRAIT_LABEL_W 84
#define STRIP_H (SCENE_STRIP_LINES * SCENE_STRIP_CELL_H)

static uint32_t px(Colour k)
{
    return canvas_rgb(k.r, k.g, k.b);
}

/* Room left between y and bottom for something h tall. */
static bool fits(int y, int h, int bottom)
{
    return y + h <= bottom;
}

/* Text in one colour, cut with "..." to w pixels. */
static void line_cut(Canvas *c, int x, int y, int w, const char *text, Colour k)
{
    char cut[128];
    text_cut(text, w, cut, sizeof(cut));
    text_draw(c, x, y, cut, px(k));
}

/* A bar w wide filled to share (0..1) in colour fill; values outside are
 * held at the ends. */
static void bar(Canvas *c, int x, int y, int w, float share, Colour fill)
{
    if (!(share > 0.0f)) { /* also catches NaN, for which every test is false */
        share = 0.0f;
    }
    if (share > 1.0f) {
        share = 1.0f;
    }
    canvas_rect(c, x, y, w, BAR_H, px(PALETTE_BUTTON_FILL));
    /* The float product cut to whole pixels: the cast drops the fraction. */
    canvas_rect(c, x, y, (int)(share * (float)w), BAR_H, px(fill));
}

/* Splits text into at most two lines that each fit w pixels, breaking at a
 * space; a second line still too long is cut with "...". Returns the lines
 * used, 1 or 2. */
static int wrap_two(const char *text, int w, char *one, char *two, size_t size)
{
    snprintf(one, size, "%s", text);
    two[0] = '\0';
    if (text_width(one) <= w) {
        return 1;
    }
    /* The last space whose first part fits. one[k] = '\0' ends the string
     * there for text_width, and is put back to a space if it does not fit. */
    for (size_t k = strlen(one); k > 0; k--) {
        if (one[k] != ' ') {
            continue;
        }
        one[k] = '\0';
        if (text_width(one) <= w) {
            text_cut(text + k + 1, w, two, size);
            return 2;
        }
        one[k] = ' ';
    }
    text_cut(text, w, one, size); /* one long word: cut it */
    return 1;
}

/* The picture, the name, and the kind of body, colour and age. Returns the y
 * below them. */
static int draw_head(Scene *sc, Canvas *c, int x, int y, int w, const Sim *s,
                     const Creature *f, uint64_t body_ms)
{
    scene_draw_followed(sc, c, s, f, body_ms, x + (w - SCENE_ENLARGED_W) / 2, y);
    y += SCENE_ENLARGED_H + 2;
    char name[16];
    name_from_id(f->id, name, sizeof(name));
    line_cut(c, x + PAD, y, w - 2 * PAD, name, PALETTE_TEXT);
    y += LINE_H;
    SpriteBody body = sprite_body(f, &s->set, SPRITE_BASE_DETAIL);
    int band = panel_hue_band(render_display_hue(f->genome.g[GENE_HUE]));
    char age[24], about[96];
    years_format(age, sizeof(age), f->age, s->set.ticks_per_year);
    /* A sea body says so on its card, and a flyer up in the air (the water
     * and air design, 5.7). */
    const char *where = f->up > 0 ? " in the air" : creature_sea_body(f) ? " at sea" : "";
    snprintf(about, sizeof(about), "%s%s, %s, %s", SPECIES[body.species].name,
             where, panel_band_name(band), age);
    line_cut(c, x + PAD, y, w - 2 * PAD, about, PALETTE_TEXT_DIM);
    return y + LINE_H;
}

/* What it is doing, in a highlighted box one or two lines tall. */
static int draw_doing(Canvas *c, int x, int y, int w, const Sim *s,
                      const Creature *f)
{
    char doing[DOING_LEN], one[DOING_LEN], two[DOING_LEN];
    doing_text(s, f, doing, sizeof(doing));
    int lines = wrap_two(doing, w - 4 * PAD, one, two, sizeof(one));
    int h = lines * TEXT_H + 6;
    ui_box(c, x + PAD, y, w - 2 * PAD, h, px(PALETTE_BUTTON_FILL),
           px(PALETTE_BUTTON_EDGE));
    text_draw(c, x + 2 * PAD, y + 3, one, px(PALETTE_BUTTON_TEXT));
    if (lines == 2) {
        text_draw(c, x + 2 * PAD, y + 3 + TEXT_H, two, px(PALETTE_BUTTON_TEXT));
    }
    return y + h + 4;
}

/* A label and a bar under it. */
static int labelled_bar(Canvas *c, int x, int y, int w, const char *label,
                        float share, Colour fill)
{
    line_cut(c, x + PAD, y, w - 2 * PAD, label, PALETTE_TEXT);
    bar(c, x + PAD, y + TEXT_H + 1, w - 2 * PAD, share, fill);
    return y + TEXT_H + BAR_H + 4;
}

static int draw_bars(Canvas *c, int x, int y, int w, const Sim *s,
                     const Creature *f)
{
    /* Energy over what mating needs, the measure its own brain senses. */
    float need = creature_mate_needs(f, &s->set);
    float health_max = creature_max_health(f, &s->set);
    y = labelled_bar(c, x, y, w, "energy", need > 0.0f ? f->energy / need : 0.0f,
                     PALETTE_ENERGY);
    return labelled_bar(c, x, y, w, "health",
                        health_max > 0.0f ? f->health / health_max : 0.0f,
                        PALETTE_HEALTH);
}

static int draw_counts(Canvas *c, int x, int y, int w, const Creature *f)
{
    line_cut(c, x + PAD, y, w - 2 * PAD,
             creature_is_hunter(f) ? "eats meat" : "eats plants", PALETTE_TEXT);
    y += LINE_H;
    char text[64];
    /* %u with an explicit unsigned: the tally's counts are uint32_t. */
    snprintf(text, sizeof(text), "%u young, %u kills",
             (unsigned)f->tally.children, (unsigned)f->tally.kills);
    line_cut(c, x + PAD, y, w - 2 * PAD, text, PALETTE_TEXT);
    return y + LINE_H;
}

/* One row per trait a player can give: its word, then its gene as a bar. */
static int draw_traits(Canvas *c, int x, int y, int w, int bottom,
                       const Creature *f)
{
    for (int k = 0; k < ACTIONS_GIVABLE_COUNT && fits(y, TRAIT_H, bottom); k++) {
        int gene = ACTIONS_GIVABLE[k];
        line_cut(c, x + PAD, y - 3, TRAIT_LABEL_W - PAD, actions_gene_name(gene),
                 PALETTE_TEXT_DIM);
        bar(c, x + TRAIT_LABEL_W, y + 3, w - TRAIT_LABEL_W - PAD,
            f->genome.g[gene], PALETTE_TRAIT);
        y += TRAIT_H;
    }
    return y + 4;
}

/* The width of a row of n buttons, 2 pixels apart. */
static int row_width(const UiButton *b, int n)
{
    int total = 0;
    for (int k = 0; k < n; k++) {
        total += ui_button_width(&b[k]) + 2;
    }
    return total;
}

/* The row of buttons. When the words do not all fit, the first buttons lose
 * theirs until the row fits: the last, give trait in Sandbox, keeps its word
 * longest, as the reason Sandbox is there. */
static void draw_buttons(Ui *u, Canvas *c, int x, int y, int w, bool sandbox)
{
    UiButton b[3] = {
        {ICON_STORY, "story", "its life story (l)", 'l', false, false, false},
        {ICON_LET_GO, "let go", "let go (Esc)", UI_KEY_LET_GO, false, false, false},
        {ICON_TRAIT, "give", "give it a trait (g)", 'g', false, false, true},
    };
    int n = sandbox ? 3 : 2;
    for (int k = 0; k < n && row_width(b, n) > w - 2 * PAD; k++) {
        b[k].word = NULL;
    }
    int bx = x + PAD;
    for (int k = 0; k < n; k++) {
        bx += ui_button(u, c, bx, y, &b[k]) + 2;
    }
}

void card_draw(Ui *u, Scene *sc, Canvas *c, int x, int y, int w, int h,
               const Sim *s, const Families *f, const Creature *followed,
               uint64_t body_ms, bool sandbox)
{
    /* A dead body's slot keeps its old contents until a newborn takes it
     * (pool_kill): drawn, it would show a card for an animal that is gone,
     * with buttons for it. The window never asks, but nothing stops it. */
    if (followed == NULL || !followed->alive) {
        return;
    }
    /* Room is kept for the buttons at the bottom; everything else must end
     * above that. They are drawn right under the last part that fit. */
    int lowest_buttons_y = y + h - UI_BUTTON_H - 2;
    int bottom = lowest_buttons_y - 2;
    int at = y + 2;
    if (fits(at, SCENE_ENLARGED_H + 2 + 2 * LINE_H, bottom)) {
        at = draw_head(sc, c, x, at, w, s, followed, body_ms);
    }
    if (fits(at, 2 * TEXT_H + 10, bottom)) {
        at = draw_doing(c, x, at, w, s, followed);
    }
    if (fits(at, 2 * (TEXT_H + BAR_H + 4), bottom)) {
        at = draw_bars(c, x, at, w, s, followed);
    }
    if (fits(at, 2 * LINE_H, bottom)) {
        at = draw_counts(c, x, at, w, followed);
    }
    at = draw_traits(c, x, at, w, bottom, followed);
    if (fits(at, LINE_H + STRIP_H, bottom)) {
        line_cut(c, x + PAD, at, w - 2 * PAD, "its family over time",
                 PALETTE_TEXT_DIM);
        scene_draw_strip(sc, c, f, followed->lineage, x, at + LINE_H, w);
        at += LINE_H + STRIP_H + 4;
    }
    int buttons_y = at + 2 < lowest_buttons_y ? at + 2 : lowest_buttons_y;
    if (buttons_y >= y) {
        draw_buttons(u, c, x, buttons_y, w, sandbox);
    }
}
