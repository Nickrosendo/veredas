#include "weather_fx.h"

#include <math.h>   /* fmod, sin, lroundf */
#include <string.h> /* memcpy */

#include "gfx_hash.h"
#include "weather.h"

/*
 * One kind's look: the colour the view is washed toward and how far (out of
 * 256, at full strength), and its specks' colour. Speeds are in canvas
 * pixels a second.
 */
typedef struct {
    uint8_t wash_r, wash_g, wash_b;
    int wash; /* 0..256 */
    uint8_t speck_r, speck_g, speck_b;
} Look;

static bool look_of(int kind, Look *out)
{
    /* A switch rather than an array indexed by kind: kind comes from
     * outside, and a value past the end of an array would be read without
     * any check at all. */
    switch (kind) {
    case WEATHER_HARD_WINTER:
        *out = (Look){150, 170, 205, 70, 245, 248, 255};
        return true;
    case WEATHER_DROUGHT:
        *out = (Look){235, 190, 105, 70, 245, 222, 165};
        return true;
    case WEATHER_WARM_YEAR:
        *out = (Look){215, 245, 150, 56, 255, 250, 190};
        return true;
    default:
        return false;
    }
}

/* p moved toward (r, g, b) by a / 256: 0 leaves it, 256 replaces it. */
static uint32_t mixed(uint32_t p, uint8_t r, uint8_t g, uint8_t b, uint32_t a)
{
    uint32_t keep = 256u - a;
    uint32_t pr = (((p >> 16) & 0xFFu) * keep + r * a) >> 8;
    uint32_t pg = (((p >> 8) & 0xFFu) * keep + g * a) >> 8;
    uint32_t pb = ((p & 0xFFu) * keep + b * a) >> 8;
    return 0xFF000000u | (pr << 16) | (pg << 8) | pb;
}

/* The part of the view that is on the canvas, as a clip. */
static CanvasClip view_on_canvas(const Canvas *c, int vx, int vy, int vw, int vh)
{
    int x0 = vx < 0 ? 0 : vx;
    int y0 = vy < 0 ? 0 : vy;
    int x1 = vx + vw > c->w ? c->w : vx + vw;
    int y1 = vy + vh > c->h ? c->h : vy + vh;
    return (CanvasClip){x0, y0, x1 > x0 ? x1 - x0 : 0, y1 > y0 ? y1 - y0 : 0};
}

/*
 * The wash touches every pixel of the view, about 1.8 million at 1920x1080,
 * so it mixes several colour bytes with each multiply. A pixel's red and blue
 * sit 16 bits apart, and each product (at most 255 * 256 = 0xFF00) fits in
 * its 16 bits without reaching the next; green has a whole 32-bit half to
 * itself. Two pixels side by side are read as one 64-bit number, so two
 * multiplies mix six bytes: about four times fewer than mixed() would need.
 */
#define RB2 0x00FF00FF00FF00FFull /* red and blue of two pixels */
#define G2 0x0000FF000000FF00ull  /* green of two pixels */

static void wash(Canvas *c, CanvasClip v, const Look *k, uint32_t a)
{
    uint64_t keep = 256u - a;
    uint64_t one_rb = ((uint64_t)k->wash_r << 16) | k->wash_b;
    uint64_t one_g = (uint64_t)k->wash_g << 8;
    /* The tint's share, already multiplied, for both pixels at once. */
    uint64_t add_rb = ((one_rb << 32) | one_rb) * a;
    uint64_t add_g = ((one_g << 32) | one_g) * a;
    for (int y = v.y; y < v.y + v.h; y++) {
        uint32_t *row = &c->px[y * c->w];
        int x = v.x;
        for (; x + 1 < v.x + v.w; x += 2) {
            uint64_t two;
            /* memcpy, not *(uint64_t *)&row[x]: reading uint32_t memory
             * through a uint64_t pointer breaks C's aliasing rule, and row[x]
             * need not sit on an 8-byte boundary. The compiler turns this
             * memcpy into one plain load. */
            memcpy(&two, &row[x], sizeof(two));
            uint64_t rb = (((two & RB2) * keep + add_rb) >> 8) & RB2;
            uint64_t g = (((two & G2) * keep + add_g) >> 8) & G2;
            two = 0xFF000000FF000000ull | rb | g;
            memcpy(&row[x], &two, sizeof(two));
        }
        if (x < v.x + v.w) { /* an odd width leaves one pixel */
            row[x] = mixed(row[x], k->wash_r, k->wash_g, k->wash_b, a);
        }
    }
}

/* One speck of w by h pixels at (x, y), blended in by a / 256, inside clip. */
static void speck(Canvas *c, int x, int y, int w, int h, const Look *k, uint32_t a,
                  CanvasClip clip)
{
    for (int py = y; py < y + h; py++) {
        if (py < clip.y || py >= clip.y + clip.h) {
            continue;
        }
        for (int px = x; px < x + w; px++) {
            if (px >= clip.x && px < clip.x + clip.w) {
                uint32_t *p = &c->px[py * c->w + px];
                *p = mixed(*p, k->speck_r, k->speck_g, k->speck_b, a);
            }
        }
    }
}

/* n wrapped into 0 .. size - 1. fmod keeps the sign of n, so a negative
 * result needs size added once; doubles because seconds times a speed
 * grow past what a float holds to the pixel after a few days of running. */
static double wrapped(double n, double size)
{
    double m = fmod(n, size);
    return m < 0.0 ? m + size : m;
}

/*
 * Where speck i is at t seconds, in view pixels, and its size. Three hashes
 * of the speck's number give it a start, a speed and a sway that never
 * change, so it moves smoothly from frame to frame with nothing remembered.
 */
static void place(int kind, int i, double t, int vw, int vh, int *x, int *y,
                  int *w, int *h)
{
    double sx = (double)gfx_hash(i, kind, 1) * vw; /* float * int would turn vw into a float */
    double sy = (double)gfx_hash(i, kind, 2) * vh;
    double speed = gfx_hash(i, kind, 3);
    double phase = gfx_hash(i, kind, 4) * 6.2832; /* a whole turn, in radians */
    double fx, fy;
    *w = 1;
    *h = 1;
    switch (kind) {
    case WEATHER_HARD_WINTER: /* falling slowly, swaying and drifting a little */
        fy = sy + t * (18.0 + 22.0 * speed);
        fx = sx + t * 6.0 + 5.0 * sin(t * 0.9 + phase);
        *w = *h = speed < 0.6 ? 2 : 3; /* the faster ones nearer, so larger */
        break;
    case WEATHER_DROUGHT: /* dashes blowing left to right, bobbing */
        fx = sx + t * (90.0 + 90.0 * speed);
        fy = sy + 3.0 * sin(t * 2.0 + phase);
        *w = 4 + (int)(speed * 5.0); /* 4 to 8 long */
        *h = speed < 0.5 ? 1 : 2;
        break;
    default: /* WEATHER_WARM_YEAR: rising slowly, wavering */
        fy = sy - t * (8.0 + 10.0 * speed);
        fx = sx + 4.0 * sin(t * 0.7 + phase);
        *w = *h = speed < 0.75 ? 2 : 3;
        break;
    }
    /* (int) cuts toward zero; wrapped() has made both values at least 0. */
    *x = (int)wrapped(fx, vw);
    *y = (int)wrapped(fy, vh);
}

void weather_fx_draw(Canvas *c, int vx, int vy, int vw, int vh, int kind,
                     float strength, uint64_t ms)
{
    Look k;
    /* !(strength > 0) rather than strength <= 0: it is true for NaN too, since
     * NaN fails every comparison. */
    if (!(strength > 0.0f) || !look_of(kind, &k) || vw <= 0 || vh <= 0) {
        return;
    }
    if (strength > 1.0f) {
        strength = 1.0f;
    }
    CanvasClip v = view_on_canvas(c, vx, vy, vw, vh);
    if (v.w == 0 || v.h == 0) {
        return;
    }
    wash(c, v, &k, (uint32_t)lroundf((float)k.wash * strength));
    /* Counted from the whole view, on the canvas or not, so a view sliding off
     * the edge keeps its specks where they were. long: a big view's w * h
     * is past what an int is sure to hold (32767 is all C promises). */
    long count = (long)vw * vh / WEATHER_FX_PIXELS_PER_SPECK;
    count = lroundf((float)count * strength);
    uint32_t a = (uint32_t)lroundf(230.0f * strength);
    double t = (double)ms / 1000.0;
    for (long i = 0; i < count; i++) {
        int x, y, w, h;
        place(kind, (int)i, t, vw, vh, &x, &y, &w, &h);
        speck(c, vx + x, vy + y, w, h, &k, a, v);
    }
}

void weather_fx_fade(float *strength, bool active, uint64_t elapsed_ms)
{
    float step = (float)elapsed_ms / (float)WEATHER_FX_FADE_MS;
    float s = *strength + (active ? step : -step);
    /* Also catches a strength that was outside 0..1 before the step, and NaN,
     * which fails s > 0 and so becomes 0. */
    *strength = s > 1.0f ? 1.0f : (s > 0.0f ? s : 0.0f);
}
