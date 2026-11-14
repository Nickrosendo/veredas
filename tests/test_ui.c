#include "test.h"

#include <string.h>

#include "canvas.h"
#include "icons.h"
#include "platform.h" /* KEY_ESCAPE */
#include "text.h"
#include "ui.h"

#define GREY 0xFF404040u

static void test_hits(void)
{
    static Ui u; /* static: a Ui holds two lists of 160 hits, ~33 KB */
    ui_init(&u);
    ui_begin(&u);
    ui_add_hit(&u, 0, 0, 50, 24, ' ');
    ui_add_hit(&u, 40, 0, 50, 24, 'g');
    ui_end(&u);
    CHECK(ui_key_at(&u, 45, 10) == 'g'); /* overlap: last added wins */
    CHECK(ui_key_at(&u, 10, 10) == ' ');
    CHECK(ui_key_at(&u, 49, 23) == 'g'); /* edges inside */
    CHECK(ui_key_at(&u, 0, 0) == ' ');
    CHECK(ui_key_at(&u, 90, 10) == UI_NONE); /* x + w is outside */
    CHECK(ui_key_at(&u, 10, 24) == UI_NONE); /* y + h too */
    CHECK(ui_key_at(&u, -1, 5) == UI_NONE);

    /* a click: down and up on the same button */
    CHECK(ui_mouse_down(&u, 10, 10));
    CHECK(ui_mouse_up(&u, 12, 11) == ' ');
    /* Review Focus 1: down on the map, up on a button: nothing */
    CHECK(!ui_mouse_down(&u, 500, 300));
    CHECK(ui_mouse_up(&u, 10, 10) == UI_NONE);
    /* down on one button, up on another: nothing */
    CHECK(ui_mouse_down(&u, 10, 10));
    CHECK(ui_mouse_up(&u, 60, 10) == UI_NONE);
    /* down on a button, up off every button: nothing */
    CHECK(ui_mouse_down(&u, 10, 10));
    CHECK(ui_mouse_up(&u, 500, 300) == UI_NONE);
    /* an up with no down before it: nothing */
    CHECK(ui_mouse_up(&u, 10, 10) == UI_NONE);

    /* hits drawn this frame are not clickable until ui_end */
    ui_begin(&u);
    ui_add_hit(&u, 200, 0, 10, 10, 'x');
    CHECK(ui_key_at(&u, 205, 5) == UI_NONE);
    CHECK(ui_key_at(&u, 10, 10) == ' '); /* the last frame's still are */
    ui_end(&u);
    CHECK(ui_key_at(&u, 205, 5) == 'x');
    CHECK(ui_key_at(&u, 10, 10) == UI_NONE);

    /* full list: false, nothing written past the end */
    ui_begin(&u);
    for (int i = 0; i < UI_MAX_HITS; i++) {
        CHECK(ui_add_hit(&u, 0, 0, 1, 1, 'a'));
    }
    CHECK(!ui_add_hit(&u, 0, 0, 1, 1, 'b'));
    CHECK(u.n_hits == UI_MAX_HITS);
    ui_end(&u);
    CHECK(ui_key_at(&u, 0, 0) == 'a');
}

static void test_buttons(void)
{
    static Ui u;
    ui_init(&u);
    Canvas c;
    CHECK(canvas_init(&c, 400, 40));
    canvas_fill(&c, GREY);
    UiButton with_word = {ICON_WEATHER, "weather", "weather (w)", 'w', false,
                          false, true};
    UiButton icon_only = {ICON_WEATHER, NULL, "weather (w)", 'w', false, false,
                          true};
    UiButton word_only = {-1, "Give", NULL, '\r', false, false, false};
    CHECK(ui_button_width(&with_word) > ui_button_width(&icon_only));
    CHECK(ui_button_width(&icon_only) >= ICON_SIZE);
    CHECK(ui_button_width(&word_only) > text_width("Give"));

    ui_begin(&u);
    CHECK(ui_button(&u, &c, 5, 0, &with_word) == ui_button_width(&with_word));
    int w = ui_button_width(&with_word);
    ui_end(&u);
    CHECK(ui_key_at(&u, 5, 0) == 'w');
    CHECK(ui_key_at(&u, 5 + w - 1, UI_BUTTON_H - 1) == 'w');
    CHECK(ui_key_at(&u, 5 + w, 5) == UI_NONE);
    CHECK(ui_key_at(&u, 5, UI_BUTTON_H) == UI_NONE);
    /* drawn only inside its own rectangle */
    for (int y = 0; y < c.h; y++) {
        for (int x = 0; x < c.w; x++) {
            bool inside = x >= 5 && x < 5 + w && y < UI_BUTTON_H;
            if (!inside) {
                CHECK(c.px[y * c.w + x] == GREY);
            }
        }
    }
    /* and it did draw something */
    CHECK(c.px[(UI_BUTTON_H / 2) * c.w + 5] != GREY);

    /* a dim button is darker than the same button lit */
    Canvas lit, dark;
    CHECK(canvas_init(&lit, 100, UI_BUTTON_H));
    CHECK(canvas_init(&dark, 100, UI_BUTTON_H));
    UiButton dim = with_word;
    dim.dim = true;
    ui_begin(&u);
    ui_button(&u, &lit, 0, 0, &with_word);
    ui_button(&u, &dark, 0, 0, &dim);
    ui_end(&u);
    long sum_lit = 0, sum_dark = 0;
    for (int i = 0; i < lit.w * lit.h; i++) {
        sum_lit += (long)(lit.px[i] & 0xFF) + (long)((lit.px[i] >> 8) & 0xFF);
        sum_dark += (long)(dark.px[i] & 0xFF) + (long)((dark.px[i] >> 8) & 0xFF);
    }
    CHECK(sum_dark < sum_lit);
    /* a dim button still presses its key: the key says why it can't */
    CHECK(ui_key_at(&u, 2, 2) == 'w');

    /* the close button presses Escape, inside the box's top right corner */
    ui_begin(&u);
    ui_close_button(&u, &c, 100, 0, 200);
    ui_end(&u);
    CHECK(ui_key_at(&u, 100 + 200 - 6, 6) == KEY_ESCAPE);
    CHECK(ui_key_at(&u, 110, 6) == UI_NONE);

    canvas_free(&lit);
    canvas_free(&dark);
    canvas_free(&c);
}

static void test_tip(void)
{
    static Ui u;
    ui_init(&u);
    Canvas c;
    CHECK(canvas_init(&c, 300, 100));
    UiButton b = {ICON_PAUSE, "pause", "pause (space)", ' ', false, false, false};
    ui_begin(&u);
    ui_button(&u, &c, 10, 70, &b);
    ui_end(&u);

    canvas_fill(&c, GREY);
    ui_mouse_move(&u, 15, 75, 1000);
    CHECK(u.hover_key == ' ');
    ui_tip(&u, &c, 1000 + UI_TIP_DELAY_MS - 1); /* too soon: nothing */
    int changed = 0;
    for (int i = 0; i < c.w * c.h; i++) {
        changed += c.px[i] != GREY;
    }
    CHECK(changed == 0);
    ui_tip(&u, &c, 1000 + UI_TIP_DELAY_MS);
    for (int i = 0; i < c.w * c.h; i++) {
        changed += c.px[i] != GREY;
    }
    CHECK(changed > 0);
    /* off the button: no tip, however long */
    canvas_fill(&c, GREY);
    ui_mouse_move(&u, 250, 10, 5000);
    CHECK(u.hover_key == UI_NONE);
    ui_tip(&u, &c, 99000);
    changed = 0;
    for (int i = 0; i < c.w * c.h; i++) {
        changed += c.px[i] != GREY;
    }
    CHECK(changed == 0);
    /* a tip near the right edge stays on the canvas (the checkers watch) */
    ui_begin(&u);
    ui_button(&u, &c, 280, 0, &b);
    ui_end(&u);
    ui_mouse_move(&u, 299, 5, 0);
    ui_tip(&u, &c, 10000);
    canvas_free(&c);
}

static void test_icons(void)
{
    Canvas c;
    CHECK(canvas_init(&c, 40, 40));
    for (int i = 0; i < ICON_COUNT; i++) {
        canvas_fill(&c, GREY);
        icon_draw(&c, 10, 10, i);
        int inked = 0, outside = 0;
        for (int y = 0; y < c.h; y++) {
            for (int x = 0; x < c.w; x++) {
                bool in = x >= 10 && x < 10 + ICON_SIZE && y >= 10 &&
                          y < 10 + ICON_SIZE;
                if (c.px[y * c.w + x] != GREY) {
                    inked += in;
                    outside += !in;
                }
            }
        }
        CHECK(inked > 0);
        CHECK(outside == 0);
    }
    canvas_fill(&c, GREY);
    icon_draw(&c, 0, 0, ICON_COUNT); /* out of range: nothing */
    icon_draw(&c, 0, 0, -1);
    CHECK(c.px[0] == GREY);
    icon_draw(&c, 35, -5, ICON_PAUSE); /* clipped */
    canvas_free(&c);
}

int main(void)
{
    test_hits();
    test_buttons();
    test_tip();
    test_icons();
    TEST_SUMMARY();
}
