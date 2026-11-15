#include "test.h"

#include "actions.h"
#include "canvas.h"
#include "menus.h"
#include "palette.h"
#include "platform.h" /* KEY_ESCAPE */
#include "sandbox.h"
#include "settings.h"
#include "sim.h"
#include "ui.h"

static Ui u;

static int hits_with_key(int key)
{
    int n = 0;
    for (int i = 0; i < u.n_shown; i++) {
        n += u.shown[i].key == key;
    }
    return n;
}

static const UiHit *hit_with_key(int key)
{
    for (int i = 0; i < u.n_shown; i++) {
        if (u.shown[i].key == key) {
            return &u.shown[i];
        }
    }
    return NULL;
}

static void draw(Canvas *c, const Sandbox *b, const Sim *s)
{
    canvas_fill(c, 0xFF000000u);
    ui_begin(&u);
    menu_draw(&u, c, 2, 2, c->w - 2, c->h - 2, b, s, false);
    ui_end(&u);
}

int main(void)
{
    ui_init(&u);
    Sim s;
    Settings set = settings_default();
    CHECK(sim_init(&s, &set, 4, 32, 18));
    Canvas c;
    CHECK(canvas_init(&c, 800, 500));
    static Sandbox b; /* static: starts all zeros */
    b.on = true;

    b.menu = SANDBOX_MENU_NONE;
    draw(&c, &b, &s);
    CHECK(u.n_shown == 0);
    CHECK(!menu_is_drawn(&b));
    b.menu = SANDBOX_MENU_WEATHER;
    draw(&c, &b, &s);
    CHECK(ui_key_at(&u, 12, 12) == UI_KEY_BLOCK); /* inside the box, off any button */

    b.menu = SANDBOX_MENU_TRAIT;
    b.value = 0.5f;
    CHECK(menu_is_drawn(&b));
    draw(&c, &b, &s);
    for (int k = '1'; k < '1' + ACTIONS_GIVABLE_COUNT; k++) {
        CHECK(hits_with_key(k) == 1);
    }
    CHECK(hits_with_key(',') == 1 && hits_with_key('.') == 1);
    CHECK(hits_with_key('\r') == 1 && hits_with_key(KEY_ESCAPE) == 1);
    CHECK(hits_with_key('8') == 0 && hits_with_key('9') == 0 &&
          hits_with_key('0') == 0);

    b.menu = SANDBOX_MENU_WEATHER;
    draw(&c, &b, &s);
    CHECK(hits_with_key('1') == 1 && hits_with_key('2') == 1 &&
          hits_with_key('3') == 1 && hits_with_key(KEY_ESCAPE) == 1);
    CHECK(u.n_shown == 5); /* and the box's own background */

    b.menu = SANDBOX_MENU_SKIP;
    draw(&c, &b, &s);
    CHECK(hits_with_key('1') == 1 && hits_with_key('2') == 1 &&
          hits_with_key('3') == 1 && hits_with_key(KEY_ESCAPE) == 1);
    CHECK(u.n_shown == 5); /* and the box's own background */

    /* setup: the checkbox of a trait whose bit is set is drawn checked */
    sandbox_open_setup(&b, &s.set);
    b.jump_genes = 1 << ACTIONS_GIVABLE[0];
    draw(&c, &b, &s);
    CHECK(hits_with_key(',') == 1 && hits_with_key('.') == 1);
    CHECK(hits_with_key('-') == 1 && hits_with_key('+') == 1);
    CHECK(hits_with_key('\r') == 1 && hits_with_key(KEY_ESCAPE) == 1);
    uint32_t ink = canvas_rgb(PALETTE_BUTTON_TEXT.r, PALETTE_BUTTON_TEXT.g,
                              PALETTE_BUTTON_TEXT.b);
    for (int i = 0; i < ACTIONS_GIVABLE_COUNT; i++) {
        const UiHit *h = hit_with_key('1' + i);
        CHECK(h != NULL);
        if (h != NULL) {
            int mx = h->x + 4 + MENU_CHECK_SIZE / 2, my = h->y + 5 + MENU_CHECK_SIZE / 2;
            bool checked = c.px[my * c.w + mx] == ink;
            CHECK(checked == (i == 0));
        }
    }
    /* every hit inside the canvas */
    for (int i = 0; i < u.n_shown; i++) {
        CHECK(u.shown[i].x >= 0 && u.shown[i].x + u.shown[i].w <= c.w);
        CHECK(u.shown[i].y >= 0 && u.shown[i].y + u.shown[i].h <= c.h);
    }

    /* Placing: near the area's corner when there is room, moved left and up
     * to end inside when there is not, refused when it cannot fit at all. */
    int mx = -1, my = -1;
    CHECK(menu_place(100, 24, 800, 600, MENU_W, 300, &mx, &my));
    CHECK(mx == 100 + MENU_INSET_X && my == 24 + MENU_INSET_Y);
    /* 385 x 305: room for it, but not 8 pixels in from the corner */
    CHECK(menu_place(0, 24, 385, 305, MENU_W, 300, &mx, &my));
    CHECK(mx == 385 - MENU_MARGIN - MENU_W && my == 24 + 305 - MENU_MARGIN - 300);
    CHECK(mx >= MENU_MARGIN && my >= 24 + MENU_MARGIN);
    mx = my = -1;
    CHECK(!menu_place(0, 24, MENU_W, 600, MENU_W, 300, &mx, &my));
    CHECK(!menu_place(0, 24, 800, 200, MENU_W, 300, &mx, &my));
    CHECK(mx == -1 && my == -1);

    /* Every menu in a small window (640 x 360: the map 640 x 312 under the
     * top line), each wholly inside the map: setup is the tallest. */
    Canvas small;
    CHECK(canvas_init(&small, 640, 360));
    const int MENUS[] = {SANDBOX_MENU_TRAIT, SANDBOX_MENU_WEATHER, SANDBOX_MENU_SKIP,
                         SANDBOX_MENU_SETUP};
    for (int m = 0; m < 4; m++) {
        b.menu = MENUS[m];
        canvas_fill(&small, 0xFF000000u);
        ui_begin(&u);
        menu_draw(&u, &small, 0, 24, 640, 312, &b, &s, false);
        ui_end(&u);
        CHECK(hits_with_key(KEY_ESCAPE) == 1);
        for (int i = 0; i < u.n_shown; i++) {
            const UiHit *h = &u.shown[i];
            CHECK(h->x >= 0 && h->x + h->w <= 640);
            CHECK(h->y >= 24 && h->y + h->h <= 24 + 312);
        }
    }
    /* The smallest window: its map is 160 x 48. No menu fits; a line says so,
     * with its x, and nothing else is drawn or clickable. */
    b.menu = SANDBOX_MENU_TRAIT;
    ui_begin(&u);
    menu_draw(&u, &small, 0, 24, 160, 48, &b, &s, false);
    ui_end(&u);
    CHECK(u.n_shown == 2);
    CHECK(hits_with_key(UI_KEY_BLOCK) == 1 && hits_with_key(KEY_ESCAPE) == 1);
    CHECK(hits_with_key('1') == 0);
    for (int i = 0; i < u.n_shown; i++) {
        const UiHit *h = &u.shown[i];
        CHECK(h->x >= 0 && h->x + h->w <= 160);
        CHECK(h->y >= 24 && h->y + h->h <= 24 + 48);
    }
    /* The wheel: the window zooms only where ui_key_at finds nothing, so a
     * box's background must cover the whole box. */
    const UiHit *box = hit_with_key(UI_KEY_BLOCK);
    CHECK(box != NULL);
    if (box != NULL) {
        CHECK(ui_key_at(&u, box->x + 1, box->y + box->h - 2) != UI_NONE);
        CHECK(ui_key_at(&u, box->x + box->w + 1, box->y + 1) == UI_NONE);
    }
    canvas_free(&small);

    b.menu = SANDBOX_MENU_SUMMARY;
    CHECK(!menu_is_drawn(&b));

    canvas_free(&c);
    sim_free(&s);
    sandbox_free(&b);
    TEST_SUMMARY();
}
