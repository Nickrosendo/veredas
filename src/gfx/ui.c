#include "ui.h"

#include <stdio.h>  /* snprintf */
#include <string.h> /* memcpy */

#include "icons.h"
#include "palette.h"
#include "platform.h" /* KEY_ESCAPE: only the constant, nothing of SDL */
#include "text.h"

#define PAD 4          /* air inside a button, left and right */
#define ICON_GAP 4     /* between the icon and the word */
#define CLOSE_SIZE 16  /* the x button */

static uint32_t to_pixel(Colour k)
{
    return canvas_rgb(k.r, k.g, k.b);
}

/* One step lighter, for the button under the mouse. */
static Colour lighter(Colour k)
{
    Colour white = {255, 255, 255};
    return palette_mix(k, white, 0.15f);
}

/* Half as bright, for a tool that can't be used now. Dividing an int by 2
 * drops the remainder; the value fits a uint8_t, which the cast says. */
static Colour halved(Colour k)
{
    Colour out = {(uint8_t)(k.r / 2), (uint8_t)(k.g / 2), (uint8_t)(k.b / 2)};
    return out;
}

void ui_init(Ui *u)
{
    /* (Ui){0} would build a whole 33 KB zeroed Ui as a temporary; memset
     * zeroes the one we have in place. */
    memset(u, 0, sizeof(*u));
    u->down_key = UI_NONE;
    u->hover_key = UI_NONE;
    u->mouse_x = -1;
    u->mouse_y = -1;
}

void ui_begin(Ui *u)
{
    u->n_hits = 0;
}

void ui_end(Ui *u)
{
    /* Copying an array needs memcpy (or a loop): C has no assignment for
     * arrays, though a struct holding one copies with plain = . Only the
     * hits in use are copied. */
    memcpy(u->shown, u->hits, (size_t)u->n_hits * sizeof(u->hits[0]));
    u->n_shown = u->n_hits;
}

/* Adds a hit with its tip; tip may be NULL. */
static bool add_hit_tip(Ui *u, int x, int y, int w, int h, int key,
                        const char *tip)
{
    if (u->n_hits >= UI_MAX_HITS) {
        return false;
    }
    UiHit *hit = &u->hits[u->n_hits++];
    hit->x = x;
    hit->y = y;
    hit->w = w;
    hit->h = h;
    hit->key = key;
    /* snprintf always terminates and never writes past the size given, so a
     * tip longer than UI_TIP_LEN - 1 is cut, not overflowed. */
    snprintf(hit->tip, sizeof(hit->tip), "%s", tip != NULL ? tip : "");
    return true;
}

bool ui_add_hit(Ui *u, int x, int y, int w, int h, int key)
{
    return add_hit_tip(u, x, y, w, h, key, NULL);
}

/* The topmost shown hit under (x, y), or NULL. Searched from the end, since
 * the last added is drawn on top. */
static const UiHit *hit_at(const Ui *u, int x, int y)
{
    for (int i = u->n_shown - 1; i >= 0; i--) {
        const UiHit *h = &u->shown[i];
        if (x >= h->x && x < h->x + h->w && y >= h->y && y < h->y + h->h) {
            return h;
        }
    }
    return NULL;
}

int ui_key_at(const Ui *u, int x, int y)
{
    const UiHit *h = hit_at(u, x, y);
    return h != NULL ? h->key : UI_NONE;
}

int ui_button_width(const UiButton *b)
{
    int w = 2 * PAD;
    if (b->icon >= 0) {
        w += ICON_SIZE;
    }
    if (b->word != NULL && b->word[0] != '\0') {
        if (b->icon >= 0) {
            w += ICON_GAP;
        }
        w += text_width(b->word);
    }
    return w;
}

void ui_box(Canvas *c, int x, int y, int w, int h, uint32_t fill, uint32_t edge)
{
    canvas_rect(c, x, y, w, h, edge);
    canvas_rect(c, x + 1, y + 1, w - 2, h - 2, fill);
}

static bool mouse_over(const Ui *u, int x, int y, int w, int h)
{
    return u->mouse_x >= x && u->mouse_x < x + w && u->mouse_y >= y &&
           u->mouse_y < y + h;
}

int ui_button(Ui *u, Canvas *c, int x, int y, const UiButton *b)
{
    int w = ui_button_width(b);
    int h = UI_BUTTON_H;
    bool hover = mouse_over(u, x, y, w, h);
    /* Held down: the left button went down on this key and is still over it. */
    bool held = hover && u->down_key == b->key;
    Colour fill = b->sandbox ? PALETTE_SANDBOX_FILL : PALETTE_BUTTON_FILL;
    Colour edge = b->sandbox ? PALETTE_SANDBOX_EDGE : PALETTE_BUTTON_EDGE;
    Colour ink = PALETTE_BUTTON_TEXT;
    if (b->pressed || held) {
        fill = PALETTE_PRESSED_FILL;
        edge = PALETTE_PRESSED_EDGE;
    }
    if (hover && !b->dim) {
        fill = lighter(fill);
        edge = lighter(edge);
    }
    if (b->dim) {
        fill = halved(fill);
        edge = halved(edge);
        ink = halved(ink);
    }
    ui_box(c, x, y, w, h, to_pixel(fill), to_pixel(edge));
    int at = x + PAD;
    int icon_y = y + (h - ICON_SIZE) / 2;
    if (b->icon >= 0) {
        icon_draw(c, at, icon_y, b->icon);
        if (b->dim) {
            /* the icon dimmed with the rest: darken its square by half */
            CanvasClip clip = {at, icon_y, ICON_SIZE, ICON_SIZE};
            canvas_darken_clip(c, 128, clip);
        }
        at += ICON_SIZE + ICON_GAP;
    }
    if (b->word != NULL && b->word[0] != '\0') {
        /* The font is 16 tall in a 20-tall button: 2 pixels above. */
        text_draw(c, at, y + (h - TEXT_H) / 2, b->word, to_pixel(ink));
    }
    add_hit_tip(u, x, y, w, h, b->key, b->tip);
    return w;
}

void ui_close_button(Ui *u, Canvas *c, int box_x, int box_y, int box_w)
{
    int x = box_x + box_w - CLOSE_SIZE - 2;
    int y = box_y + 2;
    bool hover = mouse_over(u, x, y, CLOSE_SIZE, CLOSE_SIZE);
    Colour fill = hover ? lighter(PALETTE_BUTTON_FILL) : PALETTE_BUTTON_FILL;
    ui_box(c, x, y, CLOSE_SIZE, CLOSE_SIZE, to_pixel(fill),
           to_pixel(PALETTE_BUTTON_EDGE));
    /* the x: two diagonals, 8 pixels long, in the middle of the square */
    uint32_t ink = to_pixel(PALETTE_BUTTON_TEXT);
    for (int i = 0; i < 8; i++) {
        canvas_rect(c, x + 4 + i, y + 4 + i, 1, 1, ink);
        canvas_rect(c, x + 11 - i, y + 4 + i, 1, 1, ink);
    }
    add_hit_tip(u, x, y, CLOSE_SIZE, CLOSE_SIZE, KEY_ESCAPE, "close (Esc)");
}

void ui_tip(const Ui *u, Canvas *c, uint64_t now_ms)
{
    /* Both are unsigned: now_ms - since would wrap round to a huge number if
     * now_ms were the smaller, so that case is ruled out first. */
    if (u->hover_key == UI_NONE || now_ms < u->hover_since_ms ||
        now_ms - u->hover_since_ms < UI_TIP_DELAY_MS) {
        return;
    }
    const UiHit *h = hit_at(u, u->mouse_x, u->mouse_y);
    if (h == NULL || h->key != u->hover_key || h->tip[0] == '\0') {
        return;
    }
    int w = text_width(h->tip) + 2 * PAD;
    int bh = TEXT_H + 4;
    /* Above the button when there is room, else below it; moved left when it
     * would leave the canvas on the right. */
    int x = u->mouse_x + 8;
    int y = h->y - bh - 2;
    if (y < 0) {
        y = h->y + h->h + 2;
    }
    if (x + w > c->w) {
        x = c->w - w;
    }
    if (x < 0) {
        x = 0;
    }
    ui_box(c, x, y, w, bh, to_pixel(PALETTE_TEXT_BACKGROUND),
           to_pixel(PALETTE_BUTTON_EDGE));
    text_draw(c, x + PAD, y + 2, h->tip, to_pixel(PALETTE_BUTTON_TEXT));
}

bool ui_mouse_down(Ui *u, int x, int y)
{
    u->mouse_x = x;
    u->mouse_y = y;
    u->down_key = ui_key_at(u, x, y);
    return u->down_key != UI_NONE;
}

int ui_mouse_up(Ui *u, int x, int y)
{
    u->mouse_x = x;
    u->mouse_y = y;
    int key = ui_key_at(u, x, y);
    int pressed = (u->down_key != UI_NONE && key == u->down_key) ? key : UI_NONE;
    u->down_key = UI_NONE;
    return pressed;
}

void ui_mouse_move(Ui *u, int x, int y, uint64_t now_ms)
{
    u->mouse_x = x;
    u->mouse_y = y;
    int key = ui_key_at(u, x, y);
    if (key != u->hover_key) {
        u->hover_key = key;
        u->hover_since_ms = now_ms;
    }
}
