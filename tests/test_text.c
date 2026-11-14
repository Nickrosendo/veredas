#include "test.h"

#include <string.h>

#include "canvas.h"
#include "text.h"

#define WHITE 0xFFFFFFFFu
#define BLACK 0xFF000000u
#define GREY 0xFF404040u

/* True when every pixel outside the box (x, y, w, h) is still the fill. */
static int only_inked_inside(const Canvas *c, int x, int y, int w, int h,
                             uint32_t fill)
{
    for (int py = 0; py < c->h; py++) {
        for (int px = 0; px < c->w; px++) {
            bool inside = px >= x && px < x + w && py >= y && py < y + h;
            if (!inside && c->px[py * c->w + px] != fill) {
                return 0;
            }
        }
    }
    return 1;
}

/* True when the box holds only the colours a and b, and both of them. */
static int cell_has_only(const Canvas *c, int x, int y, int w, int h,
                         uint32_t a, uint32_t b)
{
    int na = 0, nb = 0;
    for (int py = y; py < y + h; py++) {
        for (int px = x; px < x + w; px++) {
            uint32_t p = c->px[py * c->w + px];
            if (p == a) {
                na++;
            } else if (p == b) {
                nb++;
            } else {
                return 0;
            }
        }
    }
    return na > 0 && nb > 0;
}

static void test_widths(void)
{
    CHECK(text_width("") == 0);
    CHECK(text_width("i") < text_width("W"));
    CHECK(text_width("ab") == text_width("a") + 1 + text_width("b"));
    CHECK(text_width(" ") > 0);
    /* a byte outside ASCII counts as '?' */
    CHECK(text_width("\xC3") == text_width("?"));
}

static void test_draw_stays_in_its_box(void)
{
    Canvas c;
    CHECK(canvas_init(&c, 80, 50));
    canvas_fill(&c, GREY);
    text_draw(&c, 10, 20, "A", WHITE);
    CHECK(only_inked_inside(&c, 10, 20, text_width("A"), TEXT_H, GREY));
    CHECK(text_draw(&c, 10, 20, "Ab", WHITE) == 10 + text_width("Ab"));
    /* see-through: a glyph leaves its background alone where it has no ink */
    canvas_fill(&c, GREY);
    text_draw(&c, 0, 0, "x", WHITE);
    CHECK(cell_has_only(&c, 0, 0, text_width("x"), TEXT_H, WHITE, GREY));
    /* off the edges: clipped, no crash (the memory checkers watch this) */
    text_draw(&c, -5, -7, "Wide words", WHITE);
    text_draw(&c, 75, 45, "Wide words", WHITE);
    canvas_free(&c);
}

static void test_cut(void)
{
    char out[64];
    text_cut("Theloulu", 1000, out, sizeof out);
    CHECK(strcmp(out, "Theloulu") == 0);
    text_cut("Theloulu", text_width("The..."), out, sizeof out);
    CHECK(strcmp(out, "The...") == 0);
    /* one pixel less: one letter fewer */
    text_cut("Theloulu", text_width("The...") - 1, out, sizeof out);
    CHECK(strcmp(out, "Th...") == 0);
    text_cut("Theloulu", 2, out, sizeof out);
    CHECK(strcmp(out, "") == 0);
    text_cut("Theloulu", text_width("..."), out, sizeof out);
    CHECK(strcmp(out, "...") == 0);
    /* exactly its own width: whole */
    text_cut("Theloulu", text_width("Theloulu"), out, sizeof out);
    CHECK(strcmp(out, "Theloulu") == 0);

    char long_line[201];
    memset(long_line, 'm', 200);
    long_line[200] = '\0';
    text_cut(long_line, 300, out, sizeof out);
    CHECK(text_width(out) <= 300);
    CHECK(strlen(out) < sizeof out);
    CHECK(strcmp(out + strlen(out) - 3, "...") == 0);

    /* a small out: still terminated, never written past */
    char small[6];
    memset(small, 'z', sizeof small);
    text_cut("Theloulu", 1000, small, sizeof small);
    CHECK(strlen(small) < sizeof small);
}

static void test_fixed(void)
{
    Canvas c;
    CHECK(canvas_init(&c, 32, 20));
    canvas_fill(&c, GREY);
    text_draw_fixed(&c, 0, 0, "x", WHITE, BLACK);
    CHECK(cell_has_only(&c, 0, 0, TEXT_FIXED_W, TEXT_H, WHITE, BLACK));
    CHECK(only_inked_inside(&c, 0, 0, TEXT_FIXED_W, TEXT_H, GREY));
    /* each letter 8 wide: the second starts at 8 */
    canvas_fill(&c, GREY);
    text_draw_fixed(&c, 0, 0, "ii", WHITE, BLACK);
    CHECK(cell_has_only(&c, 8, 0, TEXT_FIXED_W, TEXT_H, WHITE, BLACK));
    CHECK(only_inked_inside(&c, 0, 0, 2 * TEXT_FIXED_W, TEXT_H, GREY));
    text_draw_fixed(&c, 28, 10, "clipped", WHITE, BLACK);
    canvas_free(&c);
}

int main(void)
{
    test_widths();
    test_draw_stays_in_its_box();
    test_cut();
    test_fixed();
    TEST_SUMMARY();
}
