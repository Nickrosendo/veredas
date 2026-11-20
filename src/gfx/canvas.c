#include "canvas.h"

#include <stddef.h> /* size_t */
#include <stdio.h>  /* fopen, fprintf, fwrite, fclose */
#include <stdlib.h> /* malloc, free */
#include <string.h> /* memcpy */

uint32_t canvas_rgb(uint8_t r, uint8_t g, uint8_t b)
{
    /* The casts come first: a uint8_t in an expression becomes a signed
     * int, and shifting into an int's top bit is undefined behaviour. */
    return 0xFF000000u | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

/* How many pixels w by h is, or -1 when that does not fit an int. In size_t,
 * 64 bits wide here, the product of two ints cannot wrap around, so the check
 * sees the true answer; in int, the multiplication itself could overflow. */
static int pixel_count(int w, int h)
{
    size_t n = (size_t)w * (size_t)h;
    return n > (size_t)2147483647 ? -1 : (int)n;
}

bool canvas_init(Canvas *c, int w, int h)
{
    *c = (Canvas){0}; /* px NULL, sizes 0: an empty canvas */
    return canvas_resize(c, w, h);
}

bool canvas_resize(Canvas *c, int w, int h)
{
    w = w < 0 ? 0 : w;
    h = h < 0 ? 0 : h;
    int needed = pixel_count(w, h);
    if (needed < 0) {
        return false;
    }
    if (needed > c->capacity) {
        /* Not realloc: the old pixels need not survive, and asking for the
         * new block first means a failure leaves the old one in place. */
        uint32_t *block = malloc((size_t)needed * sizeof(*block));
        if (block == NULL) {
            return false;
        }
        free(c->px);
        c->px = block;
        c->capacity = needed;
    }
    c->w = w;
    c->h = h;
    canvas_fill(c, canvas_rgb(0, 0, 0));
    return true;
}

void canvas_free(Canvas *c)
{
    free(c->px); /* free(NULL) is allowed and does nothing */
    *c = (Canvas){0};
}

void canvas_fill(Canvas *c, uint32_t colour)
{
    canvas_rect(c, 0, 0, c->w, c->h, colour);
}

void canvas_rect(Canvas *c, int x, int y, int w, int h, uint32_t colour)
{
    /* Clip to the block. C checks nothing: writing to px[-1] or past the
     * end would overwrite whatever lives there, with no error. */
    int x0 = x < 0 ? 0 : x;
    int y0 = y < 0 ? 0 : y;
    int x1 = x + w > c->w ? c->w : x + w;
    int y1 = y + h > c->h ? c->h : y + h;
    for (int py = y0; py < y1; py++) {
        uint32_t *row = c->px + py * c->w; /* pointer sum: counts in pixels */
        for (int px = x0; px < x1; px++) {
            row[px] = colour;
        }
    }
}

/*
 * A mask at scale 1 that lies wholly on the canvas: no clipping is possible, so
 * it writes pixels directly. This is the common case by far (most of a frame is
 * symbols), and it is worth having: going through canvas_rect once per pixel
 * cost a third of the program's time.
 */
static void mask_unclipped(Canvas *c, const uint16_t *rows, int size, int x,
                           int y, bool flip, uint32_t colour)
{
    uint32_t *line = c->px + y * c->w + x; /* a pointer to the mask's corner */
    for (int r = 0; r < size; r++) {
        unsigned bits = rows[r];
        if (bits != 0) {
            for (int i = 0; i < size; i++) {
                if ((bits >> (size - 1 - i)) & 1u) {
                    line[flip ? size - 1 - i : i] = colour;
                }
            }
        }
        line += c->w; /* adding to a uint32_t * moves by whole pixels */
    }
}

void canvas_mask(Canvas *c, const uint16_t *rows, int size, int x, int y,
                 int scale, bool flip, uint32_t colour)
{
    int extent = size * scale;
    if (scale == 1 && x >= 0 && y >= 0 && x + extent <= c->w &&
        y + extent <= c->h) {
        mask_unclipped(c, rows, size, x, y, flip, colour);
        return;
    }
    for (int r = 0; r < size; r++) {
        /* The cast keeps the shift below unsigned: a uint16_t would become
         * a signed int first. */
        unsigned bits = rows[r];
        if (bits == 0) {
            continue; /* most rows of most symbols are short or empty */
        }
        for (int i = 0; i < size; i++) {
            if ((bits >> (size - 1 - i)) & 1u) {
                int column = flip ? size - 1 - i : i;
                canvas_rect(c, x + column * scale, y + r * scale, scale, scale,
                            colour);
            }
        }
    }
}

void canvas_rect_clip(Canvas *c, int x, int y, int w, int h, uint32_t colour,
                      CanvasClip clip)
{
    /* The overlap of the two rectangles; canvas_rect then clips to the canvas. */
    int left = x > clip.x ? x : clip.x;
    int top = y > clip.y ? y : clip.y;
    int right = x + w < clip.x + clip.w ? x + w : clip.x + clip.w;
    int bottom = y + h < clip.y + clip.h ? y + h : clip.y + clip.h;
    if (left < right && top < bottom) {
        canvas_rect(c, left, top, right - left, bottom - top, colour);
    }
}

/* One channel, 0..255, of over mixed with under: keep / 256 of over. The
 * products stay under 2^16, so unsigned 32-bit arithmetic never overflows. */
static uint32_t mix_channel(uint32_t over, uint32_t under, uint32_t keep)
{
    return (over * keep + under * (256u - keep)) >> 8;
}

void canvas_mix_rect_clip(Canvas *c, int x, int y, int w, int h, uint32_t colour,
                          int keep, CanvasClip clip)
{
    keep = keep < 0 ? 0 : (keep > 256 ? 256 : keep);
    if (keep == 256) {
        canvas_rect_clip(c, x, y, w, h, colour, clip);
        return;
    }
    /* The overlap of the rectangle, the clip and the canvas. */
    int left = x > clip.x ? x : clip.x;
    int top = y > clip.y ? y : clip.y;
    int right = x + w < clip.x + clip.w ? x + w : clip.x + clip.w;
    int bottom = y + h < clip.y + clip.h ? y + h : clip.y + clip.h;
    left = left < 0 ? 0 : left;
    top = top < 0 ? 0 : top;
    right = right > c->w ? c->w : right;
    bottom = bottom > c->h ? c->h : bottom;
    uint32_t k = (uint32_t)keep;
    uint32_t cr = (colour >> 16) & 0xFFu, cg = (colour >> 8) & 0xFFu, cb = colour & 0xFFu;
    for (int py = top; py < bottom; py++) {
        uint32_t *row = &c->px[py * c->w];
        for (int px = left; px < right; px++) {
            uint32_t old = row[px];
            uint32_t r = mix_channel(cr, (old >> 16) & 0xFFu, k);
            uint32_t g = mix_channel(cg, (old >> 8) & 0xFFu, k);
            uint32_t b = mix_channel(cb, old & 0xFFu, k);
            row[px] = (colour & 0xFF000000u) | (r << 16) | (g << 8) | b;
        }
    }
}

void canvas_mask_wide(Canvas *c, const uint64_t *rows, int size, int x, int y,
                      int scale, bool flip, uint32_t colour, CanvasClip clip)
{
    size = size > 64 ? 64 : size;
    for (int r = 0; r < size; r++) {
        uint64_t bits = rows[r];
        int i = 0;
        while (bits != 0 && i < size) {
            /* Column i is bit (size - 1 - i): skip clear pixels, then count
             * the run of set ones. */
            if (((bits >> (size - 1 - i)) & 1u) == 0) {
                i++;
                continue;
            }
            int start = i;
            while (i < size && ((bits >> (size - 1 - i)) & 1u)) {
                i++;
            }
            /* Mirrored, the run [start, i) lands at [size - i, size - start). */
            int column = flip ? size - i : start;
            canvas_rect_clip(c, x + column * scale, y + r * scale,
                             (i - start) * scale, scale, colour, clip);
        }
    }
}

void canvas_pixel(Canvas *c, int x, int y, int scale, uint32_t colour,
                  CanvasClip clip)
{
    canvas_rect_clip(c, x, y, scale, scale, colour, clip);
}

void canvas_picture(Canvas *c, const uint8_t *ink, int stride, int w, int h,
                    int x, int y, int scale, bool flip, const uint32_t *colours,
                    CanvasClip clip)
{
    for (int r = 0; r < h; r++) {
        const uint8_t *row = ink + r * stride;
        for (int i = 0; i < w; i++) {
            uint8_t index = row[i];
            if (index != 0) {
                int column = flip ? w - 1 - i : i;
                canvas_pixel(c, x + column * scale, y + r * scale, scale,
                             colours[index], clip);
            }
        }
    }
}

/* One 0xFFRRGGBB pixel with each colour channel times keep / 256, whole bytes. */
static uint32_t darkened(uint32_t p, int keep)
{
    uint32_t k = (uint32_t)keep;
    uint32_t r = (((p >> 16) & 0xFFu) * k) >> 8;
    uint32_t g = (((p >> 8) & 0xFFu) * k) >> 8;
    uint32_t b = ((p & 0xFFu) * k) >> 8;
    return (p & 0xFF000000u) | (r << 16) | (g << 8) | b;
}

void canvas_darken_clip(Canvas *c, int keep, CanvasClip clip)
{
    keep = keep < 0 ? 0 : (keep > 256 ? 256 : keep);
    if (keep == 256) {
        return; /* nothing would change */
    }
    int x0 = clip.x < 0 ? 0 : clip.x;
    int y0 = clip.y < 0 ? 0 : clip.y;
    int x1 = clip.x + clip.w > c->w ? c->w : clip.x + clip.w;
    int y1 = clip.y + clip.h > c->h ? c->h : clip.y + clip.h;
    /* Every pixel of the view, twice a frame at night: about 4 ms a pass at
     * 1920x1080 one pixel at a time (2026-11-17). So two pixels side by side
     * are read as one 64-bit number and darkened with two multiplies, the
     * way weather_fx.c washes them. Red and blue sit 16 bits apart and each
     * product is at most 255 * 256 = 0xFF00, so it fits its 16 bits without
     * reaching the next byte's; green has its 32-bit half to itself. The
     * results are the same bytes darkened() gives, pixel for pixel. */
    const uint64_t rb2 = 0x00FF00FF00FF00FFull; /* red and blue of two pixels */
    const uint64_t g2 = 0x0000FF000000FF00ull;  /* green of two pixels */
    const uint64_t top2 = 0xFF000000FF000000ull; /* the top bytes, kept */
    uint64_t k = (uint64_t)keep;
    for (int py = y0; py < y1; py++) {
        uint32_t *row = &c->px[py * c->w];
        int px = x0;
        for (; px + 1 < x1; px += 2) {
            uint64_t two;
            /* memcpy, not *(uint64_t *)&row[px]: reading uint32_t memory
             * through a uint64_t pointer breaks C's aliasing rule, and
             * row[px] need not sit on an 8-byte boundary. The compiler turns
             * the memcpy into one plain load. */
            memcpy(&two, &row[px], sizeof(two));
            uint64_t rb = (((two & rb2) * k) >> 8) & rb2;
            uint64_t g = (((two & g2) * k) >> 8) & g2;
            two = (two & top2) | rb | g;
            memcpy(&row[px], &two, sizeof(two));
        }
        if (px < x1) { /* an odd width leaves one pixel */
            row[px] = darkened(row[px], keep);
        }
    }
}

void canvas_darken_ellipse(Canvas *c, int cx, int cy, int rx, int ry, int keep,
                           CanvasClip clip)
{
    if (rx <= 0 || ry <= 0) {
        return;
    }
    keep = keep < 0 ? 0 : (keep > 256 ? 256 : keep);
    /* Only the part of the ellipse's box that is on the canvas and in the clip. */
    int x0 = cx - rx > clip.x ? cx - rx : clip.x;
    int y0 = cy - ry > clip.y ? cy - ry : clip.y;
    int x1 = cx + rx + 1 < clip.x + clip.w ? cx + rx + 1 : clip.x + clip.w;
    int y1 = cy + ry + 1 < clip.y + clip.h ? cy + ry + 1 : clip.y + clip.h;
    x0 = x0 < 0 ? 0 : x0;
    y0 = y0 < 0 ? 0 : y0;
    x1 = x1 > c->w ? c->w : x1;
    y1 = y1 > c->h ? c->h : y1;
    /* A pixel is inside when (dx/rx)^2 + (dy/ry)^2 <= 1, multiplied out so it
     * needs whole numbers only. long long is at least 64 bits: no overflow. */
    long long rx2 = (long long)rx * rx, ry2 = (long long)ry * ry;
    for (int py = y0; py < y1; py++) {
        long long dy = py - cy;
        for (int px = x0; px < x1; px++) {
            long long dx = px - cx;
            if (dx * dx * ry2 + dy * dy * rx2 <= rx2 * ry2) {
                uint32_t *p = &c->px[py * c->w + px];
                *p = darkened(*p, keep);
            }
        }
    }
}

void canvas_mix_ellipse(Canvas *c, int cx, int cy, int rx, int ry, uint32_t colour,
                        int keep, CanvasClip clip)
{
    keep = keep < 0 ? 0 : (keep > 256 ? 256 : keep);
    if (rx <= 0 || ry <= 0 || keep == 0) {
        return;
    }
    /* The ellipse's box cut to the clip and the canvas, as darkening does. */
    int x0 = cx - rx > clip.x ? cx - rx : clip.x;
    int y0 = cy - ry > clip.y ? cy - ry : clip.y;
    int x1 = cx + rx + 1 < clip.x + clip.w ? cx + rx + 1 : clip.x + clip.w;
    int y1 = cy + ry + 1 < clip.y + clip.h ? cy + ry + 1 : clip.y + clip.h;
    x0 = x0 < 0 ? 0 : x0;
    y0 = y0 < 0 ? 0 : y0;
    x1 = x1 > c->w ? c->w : x1;
    y1 = y1 > c->h ? c->h : y1;
    uint32_t k = (uint32_t)keep;
    uint32_t cr = (colour >> 16) & 0xFFu, cg = (colour >> 8) & 0xFFu, cb = colour & 0xFFu;
    long long rx2 = (long long)rx * rx, ry2 = (long long)ry * ry;
    for (int py = y0; py < y1; py++) {
        long long dy = py - cy;
        for (int px = x0; px < x1; px++) {
            long long dx = px - cx;
            if (dx * dx * ry2 + dy * dy * rx2 > rx2 * ry2) {
                continue;
            }
            uint32_t *p = &c->px[py * c->w + px];
            uint32_t old = *p;
            uint32_t r = mix_channel(cr, (old >> 16) & 0xFFu, k);
            uint32_t g = mix_channel(cg, (old >> 8) & 0xFFu, k);
            uint32_t b = mix_channel(cb, old & 0xFFu, k);
            *p = (old & 0xFF000000u) | (r << 16) | (g << 8) | b;
        }
    }
}

void canvas_copy(Canvas *c, const Canvas *from, int x, int y)
{
    /* The part of from that lands on c, worked out once for all rows. */
    int left = x < 0 ? -x : 0;
    int top = y < 0 ? -y : 0;
    int right = x + from->w > c->w ? c->w - x : from->w;
    int bottom = y + from->h > c->h ? c->h - y : from->h;
    if (left >= right) {
        return; /* also means nothing is read through a NULL px below */
    }
    for (int r = top; r < bottom; r++) {
        /* memcpy counts bytes, not pixels. */
        memcpy(c->px + (y + r) * c->w + x + left, from->px + r * from->w + left,
               (size_t)(right - left) * sizeof(c->px[0]));
    }
}

bool canvas_write_ppm_to(const Canvas *c, FILE *f)
{
    fprintf(f, "P6\n%d %d\n255\n", c->w, c->h);
    /* A row at a time into a buffer, then one fwrite: --record writes about
     * six million bytes a frame at 1920x1080, and a call per byte was the
     * slow part. The buffer is on the stack, so the widest canvas it takes
     * is fixed; a wider one falls back to a byte at a time. */
    unsigned char row[3 * 4096];
    bool fits = (size_t)c->w * 3 <= sizeof(row);
    for (int y = 0; y < c->h; y++) {
        const uint32_t *line = c->px + (size_t)y * (size_t)c->w;
        for (int x = 0; x < c->w; x++) {
            uint32_t p = line[x];
            unsigned char rgb[3] = {(unsigned char)((p >> 16) & 0xFFu),
                                    (unsigned char)((p >> 8) & 0xFFu),
                                    (unsigned char)(p & 0xFFu)};
            if (fits) {
                memcpy(row + (size_t)x * 3, rgb, 3);
            } else {
                fwrite(rgb, 1, 3, f);
            }
        }
        if (fits) {
            fwrite(row, 1, (size_t)c->w * 3, f);
        }
    }
    return !ferror(f);
}

bool canvas_write_ppm(const Canvas *c, const char *path)
{
    /* "wb": binary, or Windows would turn each byte 10 into two bytes. */
    FILE *f = fopen(path, "wb");
    if (f == NULL) {
        return false;
    }
    /* Writes are buffered, so a full disk may only show up at fclose. */
    bool wrote = canvas_write_ppm_to(c, f);
    return fclose(f) == 0 && wrote;
}
