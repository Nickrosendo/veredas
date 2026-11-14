/*
 * ui: buttons drawn in pixels that remember where they were drawn.
 *
 * "Immediate mode": nothing about a button is kept from one frame to the
 * next. Each frame the window draws its buttons again, and drawing one also
 * records its rectangle and the key a click on it presses (a "hit"). When the
 * frame is finished (ui_end), those hits become the ones the mouse is tested
 * against, so a click always lands on what the player sees. A click is turned
 * into a key, and the key goes through the window's ordinary key handling:
 * a button can do nothing its key does not.
 *
 * Knows nothing about SDL or the simulation: it draws onto a Canvas.
 */
#ifndef AGON_UI_H
#define AGON_UI_H

#include <stdbool.h>
#include <stdint.h>

#include "canvas.h"

/* UI_NONE: no button. Keys are what the window's handle_key takes (ASCII, or
 * the KEY_ values of platform.h, 1000 and up). From 3000 up, buttons that
 * have no key of their own: the window answers to them directly. */
enum { UI_NONE = -1, UI_KEY_WORLD_MENU = 3000 };
/* A box's background (a menu, a page): a click there presses nothing, but
 * never reaches the world behind the box. Added before the box's buttons, so
 * they, added later, win. */
enum { UI_KEY_BLOCK = 3010 };
/* The world panel's clickable lines (nobody followed): each follows who it
 * names. They had no key before the buttons came and have none now. */
enum {
    UI_KEY_FOLLOW_TOP_HUNTER = 3001,
    UI_KEY_FOLLOW_OLDEST = 3002,
    UI_KEY_FOLLOW_LARGEST_FAMILY = 3003
};
/* The close button of each page drawn over the map. Each closes its own
 * page and no other: Escape closes whichever comes first in its order, which
 * is the wrong one when two pages are open and the player clicked the x of
 * the one behind. */
enum {
    UI_KEY_CLOSE_STORY = 3020,
    UI_KEY_CLOSE_NOTICES = 3021,
    UI_KEY_CLOSE_COMPARE = 3022,
    UI_KEY_CLOSE_KEYS = 3023,
    UI_KEY_CLOSE_LAB = 3024
};

#define UI_MAX_HITS 160
#define UI_TIP_LEN 48
#define UI_BAR_H 24     /* a bar of buttons, the top line too */
#define UI_BUTTON_H 20  /* a button inside a bar: 2 pixels of air above and below */
#define UI_TIP_DELAY_MS 500

typedef struct {
    int x, y, w, h;
    int key;               /* what a click presses */
    char tip[UI_TIP_LEN];  /* the hover label, "" for none */
} UiHit;

typedef struct {
    UiHit hits[UI_MAX_HITS];  /* this frame's, being drawn */
    UiHit shown[UI_MAX_HITS]; /* the last finished frame's: what the player sees */
    int n_hits, n_shown;
    int mouse_x, mouse_y;
    int down_key;             /* the button the left button went down on, or UI_NONE */
    uint64_t hover_since_ms;  /* when the mouse came onto hover_key */
    int hover_key;
} Ui;

typedef struct {
    int icon;          /* an ICON_ value (icons.h), or -1 for none */
    const char *word;  /* NULL: icon only */
    const char *tip;   /* the hover label, e.g. "weather (w)"; NULL for none */
    int key;
    bool dim;          /* drawn at half brightness: the tool can't be used now */
    bool pressed;      /* drawn pushed in: a state that is on (paused, camera) */
    bool sandbox;      /* the brown look of the Sandbox tools */
} UiButton;

void ui_init(Ui *u);
/* Starts a frame: empties this frame's hits. */
void ui_begin(Ui *u);
/* Ends a frame: its hits become the shown ones the mouse is tested against. */
void ui_end(Ui *u);
/* Records a rectangle that presses key when clicked. False when the list is
 * full; nothing is written then. */
bool ui_add_hit(Ui *u, int x, int y, int w, int h, int key);
/* The same with a hover label; tip may be NULL. */
bool ui_add_hit_tip(Ui *u, int x, int y, int w, int h, int key, const char *tip);
/* The key of the topmost shown hit under (x, y): the last one added wins, so
 * a menu drawn after the bar beats the bar. UI_NONE when there is none. */
int ui_key_at(const Ui *u, int x, int y);

/* A button's width: padding, the 16-pixel icon, the word. */
int ui_button_width(const UiButton *b);
/* Draws b with its top left corner at (x, y), UI_BUTTON_H tall, records its
 * hit with its tip, and returns its width. */
int ui_button(Ui *u, Canvas *c, int x, int y, const UiButton *b);
/* The same, w pixels wide (at least its own width): rows of a menu, all
 * as wide as the widest. */
int ui_button_sized(Ui *u, Canvas *c, int x, int y, int w, const UiButton *b);
/* A filled box with a one-pixel edge. */
void ui_box(Canvas *c, int x, int y, int w, int h, uint32_t fill, uint32_t edge);
/* A small x button inside the top right corner of a box: presses Escape. */
void ui_close_button(Ui *u, Canvas *c, int box_x, int box_y, int box_w);
/* The same x pressing key instead, with its own tip. */
void ui_close_button_key(Ui *u, Canvas *c, int box_x, int box_y, int box_w,
                         int key, const char *tip);
/* After UI_TIP_DELAY_MS over a button with a tip, draws the tip near the
 * mouse, kept inside the canvas. Drawn last, over everything. */
void ui_tip(const Ui *u, Canvas *c, uint64_t now_ms);

/* The left button went down. True when it is over a shown button: the world
 * and the panel must not see this press then. */
bool ui_mouse_down(Ui *u, int x, int y);
/* The left button came up: the key to press, or UI_NONE. A click needs the
 * button to go down and come up over the same key, so a drag that starts on
 * the map and ends over a button presses nothing. */
int ui_mouse_up(Ui *u, int x, int y);
void ui_mouse_move(Ui *u, int x, int y, uint64_t now_ms);

#endif
