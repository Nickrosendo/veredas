#include "test.h"

#include <string.h>

#include "bars.h"
#include "canvas.h"
#include "scene.h" /* SCENE_MIN_W */
#include "ui.h"

static Ui u; /* static: a Ui is ~33 KB */

/* Draws the bar w wide into a fresh canvas and finishes the frame. */
static void draw_bar(Canvas *c, int w, const BottomState *st)
{
    canvas_resize(c, w, UI_BAR_H);
    canvas_fill(c, 0xFF000000u);
    ui_begin(&u);
    bar_bottom_draw(&u, c, 0, w, st);
    ui_end(&u);
}

static bool bar_has_key(int key)
{
    for (int i = 0; i < u.n_shown; i++) {
        if (u.shown[i].key == key) {
            return true;
        }
    }
    return false;
}

/* No two hits overlap, and every one is inside 0 .. w. */
static bool no_overlaps_and_inside(int w)
{
    for (int i = 0; i < u.n_shown; i++) {
        const UiHit *a = &u.shown[i];
        if (a->x < 0 || a->x + a->w > w || a->w <= 0) {
            return false;
        }
        for (int j = i + 1; j < u.n_shown; j++) {
            const UiHit *b = &u.shown[j];
            if (a->x < b->x + b->w && b->x < a->x + a->w) {
                return false;
            }
        }
    }
    return true;
}

static void test_documentary(void)
{
    Canvas c;
    CHECK(canvas_init(&c, 1, 1));
    BottomState st = {0};
    st.speed = 2;
    draw_bar(&c, 1280, &st);
    CHECK(ui_key_at(&u, 3, UI_BAR_H / 2) == ' ');      /* pause at the left */
    CHECK(ui_key_at(&u, 1280 - 4, UI_BAR_H / 2) == '?'); /* help at the right */
    CHECK(bar_has_key('m'));
    CHECK(bar_has_key(UI_KEY_WORLD_MENU));
    CHECK(bar_has_key('-') && bar_has_key('+'));
    CHECK(bar_has_key('[') && bar_has_key(']'));
    CHECK(bar_has_key('l') && bar_has_key('e') && bar_has_key('f') &&
          bar_has_key('j'));
    CHECK(!bar_has_key('g')); /* no Sandbox tools */
    CHECK(!bar_has_key('w'));
    CHECK(no_overlaps_and_inside(1280));
    /* the words fit at 1280: a button is wider than its icon */
    for (int i = 0; i < u.n_shown; i++) {
        if (u.shown[i].key == 'l') {
            CHECK(u.shown[i].w > 30);
        }
    }
    canvas_free(&c);
}

static void test_sandbox(void)
{
    Canvas c;
    CHECK(canvas_init(&c, 1, 1));
    BottomState st = {0};
    st.sandbox = true;
    st.speed = 2;
    draw_bar(&c, 1280, &st);
    CHECK(bar_has_key('g') && bar_has_key('w') && bar_has_key('k') &&
          bar_has_key('x') && bar_has_key('u'));
    CHECK(!bar_has_key('v') && !bar_has_key('\t'));
    CHECK(no_overlaps_and_inside(1280));
    st.split = true;
    draw_bar(&c, 1280, &st);
    CHECK(bar_has_key('\t') && bar_has_key('v'));
    CHECK(no_overlaps_and_inside(1280));
    /* each tip names its key */
    for (int i = 0; i < u.n_shown; i++) {
        if (u.shown[i].key == 'w') {
            CHECK(strstr(u.shown[i].tip, "(w)") != NULL);
        }
    }
    canvas_free(&c);
}

/* Review Focus 2: narrow windows never overlap or leave the bar. */
static void test_narrow(void)
{
    Canvas c;
    CHECK(canvas_init(&c, 1, 1));
    BottomState st = {0};
    st.sandbox = true;
    st.split = true;
    st.speed = 2;
    for (int w = 1280; w >= SCENE_MIN_W; w -= 7) {
        draw_bar(&c, w, &st);
        CHECK(no_overlaps_and_inside(w));
        CHECK(bar_has_key(' ')); /* pause stays */
        CHECK(bar_has_key('m')); /* and the mode switch */
    }
    draw_bar(&c, SCENE_MIN_W, &st);
    CHECK(no_overlaps_and_inside(SCENE_MIN_W));
    /* a middle width: the words go before any button does */
    st.split = false;
    st.sandbox = false;
    draw_bar(&c, 600, &st);
    CHECK(bar_has_key('j') && bar_has_key('['));
    CHECK(no_overlaps_and_inside(600));
    canvas_free(&c);
}

/* Review Focus 5: the demo shows the tools it lacks, dim, still pressing
 * their keys, which say "not in the demo". */
static void test_demo(void)
{
    Canvas c;
    CHECK(canvas_init(&c, 1, 1));
    BottomState st = {0};
    st.sandbox = true;
    st.demo = true;
    st.speed = 2;
    draw_bar(&c, 1280, &st);
    CHECK(bar_has_key('g') && bar_has_key('k') && bar_has_key('x') &&
          bar_has_key('u') && bar_has_key('w'));
    canvas_free(&c);
}

static void test_world_menu(void)
{
    Canvas c;
    CHECK(canvas_init(&c, 400, 300));
    ui_begin(&u);
    bar_world_menu_draw(&u, &c, 390, 290, false);
    ui_end(&u);
    CHECK(bar_has_key('r') && bar_has_key('c') && bar_has_key('o') &&
          bar_has_key('s') && bar_has_key('q'));
    CHECK(u.n_shown == 5);
    for (int i = 0; i < u.n_shown; i++) {
        CHECK(u.shown[i].x + u.shown[i].w <= 390);
        CHECK(u.shown[i].y + u.shown[i].h <= 290);
    }
    ui_begin(&u);
    bar_world_menu_draw(&u, &c, 390, 290, true);
    ui_end(&u);
    CHECK(bar_has_key('r') && bar_has_key('c') && bar_has_key('o'));
    CHECK(!bar_has_key('s') && !bar_has_key('q'));
    canvas_free(&c);
}

int main(void)
{
    ui_init(&u);
    test_documentary();
    test_sandbox();
    test_narrow();
    test_demo();
    test_world_menu();
    TEST_SUMMARY();
}
