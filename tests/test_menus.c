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
    menu_draw(&u, c, 10, 10, b, s, false);
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

    b.menu = SANDBOX_MENU_SUMMARY;
    CHECK(!menu_is_drawn(&b));

    canvas_free(&c);
    sim_free(&s);
    sandbox_free(&b);
    TEST_SUMMARY();
}
