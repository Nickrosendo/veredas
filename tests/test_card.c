/* card_draw: its buttons by mode, nothing drawn outside its rectangle, and
 * no card once its animal has died. */
#include "test.h"

#include <stdbool.h>
#include <stdio.h>  /* snprintf */
#include <string.h> /* strlen */

#include "brain.h"
#include "brain_script.h"
#include "card.h"
#include "chronicle.h"
#include "camera.h"
#include "inspect.h"
#include "platform.h" /* KEY_ESCAPE */

#define CW 400
#define CH 800
/* The card's rectangle inside the canvas, with a border all round it. */
#define CX 100
#define CY 40
#define CARD_W 160
#define CARD_H 672

typedef struct {
    Sim sim;
    Scene scene;
    Families families;
    Canvas canvas;
    Ui ui;
    int slot;
} World4;

static bool setup(World4 *t)
{
    Settings set = settings_default();
    snprintf(set.brain_name, sizeof(set.brain_name), "script");
    set.start_share = 0.0f;
    set.founders_per_sea_cell = 0.0f;
    set.min_creatures = 0;
    if (!sim_init(&t->sim, &set, 7, 16, 16)) {
        return false;
    }
    for (int i = 0; i < 16 * 16; i++) {
        t->sim.world.terrain[i] = TERRAIN_SOIL;
        t->sim.world.veg[i] = 0;
    }
    CHECK(scene_init(&t->scene, &t->sim));
    CHECK(family_init(&t->families, t->sim.pool.capacity));
    CHECK(canvas_init(&t->canvas, CW, CH));
    ui_init(&t->ui);
    t->slot = pool_spawn(&t->sim.pool, &t->sim.world, 4, 4);
    CHECK(t->slot >= 0);
    return t->slot >= 0;
}

static void teardown(World4 *t)
{
    canvas_free(&t->canvas);
    family_free(&t->families);
    scene_free(&t->scene);
    sim_free(&t->sim);
}

static void draw(World4 *t, bool sandbox)
{
    ui_begin(&t->ui);
    card_draw(&t->ui, &t->scene, &t->canvas, CX, CY, CARD_W, CARD_H, &t->sim,
              &t->families, &t->sim.pool.items[t->slot], 0, sandbox);
    ui_end(&t->ui);
}

/* How many shown hits press key. */
static int hits_with(const Ui *u, int key)
{
    int n = 0;
    for (int i = 0; i < u->n_shown; i++) {
        n += u->shown[i].key == key;
    }
    return n;
}

static void test_buttons_by_mode(void)
{
    World4 t;
    if (!setup(&t)) {
        return;
    }
    draw(&t, false);
    CHECK(hits_with(&t.ui, 'l') == 1);
    /* let go has its own value: Escape would close an open page first */
    CHECK(hits_with(&t.ui, UI_KEY_LET_GO) == 1);
    CHECK(hits_with(&t.ui, KEY_ESCAPE) == 0);
    CHECK(hits_with(&t.ui, 'g') == 0);
    draw(&t, true);
    CHECK(hits_with(&t.ui, 'g') == 1);
    CHECK(hits_with(&t.ui, 'l') == 1);
    /* Every button inside the card. */
    for (int i = 0; i < t.ui.n_shown; i++) {
        const UiHit *h = &t.ui.shown[i];
        CHECK(h->x >= CX && h->x + h->w <= CX + CARD_W);
        CHECK(h->y >= CY && h->y + h->h <= CY + CARD_H);
    }
    teardown(&t);
}

/* The id with the longest name among the first 200,000 (three syllables
 * with long parts; names are cut at 15 letters, which they never reach). */
static uint32_t long_named_id(void)
{
    char name[16];
    uint32_t best = 0;
    size_t best_len = 0;
    for (uint32_t id = 1; id < 200000; id++) {
        name_from_id(id, name, sizeof(name));
        if (strlen(name) > best_len) {
            best_len = strlen(name);
            best = id;
        }
    }
    printf("  longest name: %zu letters\n", best_len);
    return best;
}

static void test_nothing_outside(void)
{
    World4 t;
    if (!setup(&t)) {
        return;
    }
    uint32_t id = long_named_id();
    CHECK(id != 0);
    Creature *c = &t.sim.pool.items[t.slot];
    c->id = id;
    /* The longest doing line: resting beside food, not hungry. */
    c->current = (Action){.kind = ACT_REST, .hold = 1};
    c->energy = 2.0f * creature_mate_needs(c, &t.sim.set);
    t.sim.world.veg[world_idx(&t.sim.world, 4, 5)] = 1;
    uint32_t sentinel = canvas_rgb(255, 0, 255);
    for (int h = 0; h < 2; h++) {
        canvas_fill(&t.canvas, sentinel);
        /* Tall, and too short for all of it: either way, nothing outside. */
        int card_h = h == 0 ? CARD_H : 120;
        ui_begin(&t.ui);
        card_draw(&t.ui, &t.scene, &t.canvas, CX, CY, CARD_W, card_h, &t.sim,
                  &t.families, c, 0, true);
        ui_end(&t.ui);
        int outside = 0, ox = -1, oy = -1;
        for (int y = 0; y < CH; y++) {
            for (int x = 0; x < CW; x++) {
                bool inside = x >= CX && x < CX + CARD_W && y >= CY &&
                              y < CY + card_h;
                if (!inside && t.canvas.px[y * CW + x] != sentinel) {
                    outside++;
                    ox = x;
                    oy = y;
                }
            }
        }
        if (outside > 0) {
            printf("  %d pixels outside, the last at %d,%d (card %d tall)\n",
                   outside, ox, oy, card_h);
        }
        CHECK(outside == 0);
        CHECK(hits_with(&t.ui, 'g') == 1); /* the buttons stay, even short */
    }
    teardown(&t);
}

/* The followed animal dies while its card is shown. The window's next frame
 * asks scene_follow who is followed, as draw_frame does through
 * update_following, and draws the card only for an answer: the answer is
 * nobody, so that frame has none of the card's buttons. card_draw itself,
 * given the dead body's slot, draws nothing. */
static void test_after_death(void)
{
    World4 t;
    if (!setup(&t)) {
        return;
    }
    Camera cam;
    camera_init(&cam, 16, 16, 16, 16);
    Inspect in = {0};
    inspect_follow(&in, &t.sim, t.slot);
    /* frame 1: followed, the card drawn */
    ui_begin(&t.ui);
    const Creature *f = scene_follow(&t.scene, &cam, &in, &t.sim, 0.0f);
    CHECK(f == &t.sim.pool.items[t.slot]);
    if (f != NULL) {
        card_draw(&t.ui, &t.scene, &t.canvas, CX, CY, CARD_W, CARD_H, &t.sim,
                  &t.families, f, 0, true);
    }
    ui_end(&t.ui);
    CHECK(hits_with(&t.ui, UI_KEY_LET_GO) == 1 && hits_with(&t.ui, 'g') == 1);

    pool_kill(&t.sim.pool, &t.sim.world, t.slot, &t.sim.set, DEATH_WOUNDS);
    t.sim.tick++;

    /* frame 2: nobody followed, so no card */
    ui_begin(&t.ui);
    f = scene_follow(&t.scene, &cam, &in, &t.sim, 0.0f);
    CHECK(f == NULL);
    CHECK(!in.following);
    if (f != NULL) {
        card_draw(&t.ui, &t.scene, &t.canvas, CX, CY, CARD_W, CARD_H, &t.sim,
                  &t.families, f, 0, true);
    }
    ui_end(&t.ui);
    CHECK(hits_with(&t.ui, 'l') == 0 && hits_with(&t.ui, UI_KEY_LET_GO) == 0 &&
          hits_with(&t.ui, 'g') == 0);

    /* and the card asked straight for the dead body draws nothing */
    uint32_t sentinel = canvas_rgb(255, 0, 255);
    canvas_fill(&t.canvas, sentinel);
    draw(&t, true);
    CHECK(t.ui.n_shown == 0);
    int inked = 0;
    for (int i = 0; i < CW * CH; i++) {
        inked += t.canvas.px[i] != sentinel;
    }
    CHECK(inked == 0);
    teardown(&t);
}

int main(void)
{
    CHECK(brain_register(&BRAIN_SCRIPT));
    test_buttons_by_mode();
    test_nothing_outside();
    test_after_death();
    TEST_SUMMARY();
}
