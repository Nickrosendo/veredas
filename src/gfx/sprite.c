#include "sprite.h"

#include <math.h>   /* floorf */
#include <stdlib.h> /* free, malloc */
#include <string.h> /* memcpy */

#include "art.h"      /* art_clip, art_raster */
#include "canvas.h"   /* canvas_rgb */
#include "gfx_hash.h" /* gfx_mix */
#include "palette.h"  /* render_display_hue, palette_mix */
#include "pose.h"     /* pose_at */
#include "ramp.h"

/* ---- Bodies ---- */

int sprite_tiles_for(float size)
{
    /* Written as "not at least", so a NaN, for which every comparison is
     * false, falls into the first case. */
    if (!(size >= SPRITE_TWO_TILES)) {
        return 1;
    }
    if (!(size >= SPRITE_THREE_TILES)) {
        return 2;
    }
    return 3;
}

SpriteBody sprite_body_of(const BodyTraits *traits, int species, float real_size, int base)
{
    SpriteBody b;
    b.traits = species_round(traits);
    /* The casts: each value is small (species under 32, level under 8, base
     * under 4), so it fits a uint8_t. Out of range ones are brought back to 0. */
    b.species = (uint8_t)(species >= 0 && species < SPECIES_COUNT ? species : 0);
    b.k_level = (uint8_t)species_k_level(real_size);
    b.base = (uint8_t)(base >= 0 && base < SPRITE_BASES ? base : SPRITE_BASE_NORMAL);
    return b;
}

SpriteBody sprite_body(const Creature *c, const Settings *s, int base)
{
    BodyTraits t = species_traits(&c->genome);
    BodyTraits rounded = species_round(&t);
    float size = creature_size(c, s);
    int species = species_pick(c->lineage, species_role(&rounded), size);
    /* Only a body with wings over the line can be up, and only then are they
     * drawn spread. */
    t.aloft = c->up > 0 && t.air > 0.5f ? 1.0f : 0.0f;
    return sprite_body_of(&t, species, size, base);
}

bool sprite_body_equal(const SpriteBody *a, const SpriteBody *b)
{
    const BodyTraits *x = &a->traits, *y = &b->traits;
    return a->species == b->species && a->k_level == b->k_level && a->base == b->base &&
           x->size == y->size && x->speed == y->speed && x->sight == y->sight &&
           x->plant == y->plant && x->meat == y->meat && x->armour == y->armour &&
           x->weapon == y->weapon && x->legs == y->legs && x->swim == y->swim &&
           x->sea == y->sea && x->air == y->air && x->aloft == y->aloft;
}

const int SPRITE_FRAMES[ACTION_COUNT] = {24, 24, 16, 30, 39, 54, 6};

/* K times this, by base: Normal, Far, detail. */
static const float BASE_FACTOR[SPRITE_BASES] = {1.0f, 0.5f, 1.6f};

/*
 * The species' K cap, worked out the first time it is asked for and kept for
 * the rest of the program. skeleton_k_cap costs about 10 ms a species (180 ms
 * for all 17), so doing it lazily spreads that over the first frames that
 * show each species, instead of making start-up wait for all of them. static
 * variables inside a function keep their values between calls, and start at
 * 0 and false. The answer never changes, so one copy for every cache is right;
 * like skeleton_k_cap itself, this is not for two threads at once.
 */
static float cap_of(int species)
{
    static float caps[SPECIES_COUNT];
    static bool known[SPECIES_COUNT];
    if (!known[species]) {
        caps[species] = skeleton_k_cap(species);
        known[species] = true;
    }
    return caps[species];
}

float sprite_k(const SpriteBody *b)
{
    int level = b->k_level < SPECIES_K_LEVELS ? b->k_level : SPECIES_K_LEVELS - 1;
    int species = b->species < SPECIES_COUNT ? b->species : 0;
    int base = b->base < SPRITE_BASES ? b->base : SPRITE_BASE_NORMAL;
    float k = SPECIES_K[level] * SPECIES[species].scale;
    float cap = cap_of(species);
    /* The cap before the base's factor: every detailed picture then fits the
     * large grid, as skeleton's test asks at 1.6 times the cap. */
    k = k < cap ? k : cap;
    return k * BASE_FACTOR[base];
}

/* ---- Palettes ---- */

static uint32_t pixel_of(Colour c)
{
    return canvas_rgb(c.r, c.g, c.b);
}

/* v minus its floor: a hue brought back round the wheel into 0..1. */
static float wrap01(float v)
{
    return v - floorf(v);
}

/* The mane's ramp (v9:448): the body's base shaded 0.35 towards black, its hue
 * 0.03 back and its saturation 0.15 up, made into a ramp of its own. */
static Ramp mane_ramp(const Ramp *body)
{
    float h, s, l;
    ramp_to_hsl(ramp_shade(body->c[RAMP_BASE], -0.35f), &h, &s, &l);
    s = s + 0.15f > 1.0f ? 1.0f : s + 0.15f;
    return ramp_from_colour(ramp_from_hsl(wrap01(h - 0.03f), s, l));
}

/* Every ink's colour from the body's ramp (v9:469-480). */
static void fill_palette(SpritePalette *out, const Ramp *body, int species)
{
    /* The horn, ivory and shell ramps never change: made once. */
    static bool fixed_made = false;
    static Ramp horn, ivory, shell;
    if (!fixed_made) {
        horn = ramp_fixed(RAMP_FIXED_HORN);
        ivory = ramp_fixed(RAMP_FIXED_IVORY);
        shell = ramp_fixed(RAMP_FIXED_SHELL);
        fixed_made = true;
    }
    Ramp mane = mane_ramp(body);
    const Species *sp = &SPECIES[species];
    out->ink[INK_NONE] = 0;
    /* INK_DEEP..INK_OUTLINE are in the same order as RAMP_DEEP..RAMP_OUTLINE. */
    for (int i = 0; i < RAMP_STEPS; i++) {
        out->ink[INK_DEEP + i] = pixel_of(body->c[RAMP_DEEP + i]);
    }
    out->ink[INK_MOUTH] = canvas_rgb(70, 22, 30);
    out->ink[INK_SHELL_BAND] = pixel_of(shell.c[RAMP_OUTLINE]);
    /* A species with a stripe colour of its own (tiger, zebra) uses it; any
     * other stripe (the lemur's ringed tail) is the body's deepest shade. */
    out->ink[INK_STRIPE] = sp->stripe_freq > 0.0f ? pixel_of(sp->stripe)
                                                   : pixel_of(body->c[RAMP_DEEP]);
    for (int i = 0; i < 5; i++) {
        out->ink[INK_HORN0 + i] = pixel_of(horn.c[i]);
        /* Ivory one step lighter than its index (v9:475), the top step at most. */
        out->ink[INK_IVORY0 + i] = pixel_of(ivory.c[i + 1 < 4 ? i + 1 : 4]);
        out->ink[INK_MANE0 + i] = pixel_of(mane.c[i]);
    }
    for (int i = 0; i < 4; i++) {
        out->ink[INK_SHELL1 + i] = pixel_of(shell.c[1 + i]);
    }
    out->ink[INK_NOSE] = canvas_rgb(28, 18, 22);
    out->ink[INK_EYE] = canvas_rgb(18, 14, 22);
    out->ink[INK_EYE_WHITE] = canvas_rgb(250, 250, 245);
    out->ink[INK_TEETH] = canvas_rgb(246, 240, 226);
}

static int species_in_range(int species)
{
    return species >= 0 && species < SPECIES_COUNT ? species : 0;
}

void sprite_palette(SpritePalette *out, int species, float hue)
{
    species = species_in_range(species);
    float h, s, l;
    ramp_to_hsl(SPECIES[species].look, &h, &s, &l); /* h is not used: the family's hue is */
    Ramp body = ramp_make(render_display_hue(hue), s, l);
    fill_palette(out, &body, species);
}

void sprite_palette_look(SpritePalette *out, int species)
{
    species = species_in_range(species);
    Ramp body = ramp_from_colour(SPECIES[species].look);
    fill_palette(out, &body, species);
}

void sprite_palette_flash(SpritePalette *out, const SpritePalette *in, float flash)
{
    static const Colour WHITE = {255, 255, 255};
    out->ink[INK_NONE] = 0;
    for (int i = 1; i < INK_COUNT; i++) {
        uint32_t p = in->ink[i];
        Colour c = {(uint8_t)((p >> 16) & 0xFFu), (uint8_t)((p >> 8) & 0xFFu),
                    (uint8_t)(p & 0xFFu)};
        out->ink[i] = pixel_of(palette_mix(c, WHITE, flash));
    }
}

/* ---- The cache ---- */

/* No key is ever this: a real one uses only the low 38 bits. */
#define KEY_NONE UINT64_MAX

/* A trait of 0, 1/3, 2/3 or 1 as the step 0..3. */
static uint64_t trait_step(float t)
{
    int step = (int)(t * 3.0f + 0.5f);
    return (uint64_t)(step < 0 ? 0 : (step > 3 ? 3 : step));
}

/*
 * Everything a picture depends on, packed into one number: species (5 bits),
 * the nine rounded traits (2 bits each), k_level (3), base (2), action (3) and
 * frame (6), 37 bits in all. << shifts the bits left to make room, | puts the
 * next value into the room made.
 */
static uint64_t key_of(const SpriteBody *b, int action, int frame)
{
    const BodyTraits *t = &b->traits;
    const float traits[9] = {t->size, t->speed, t->sight, t->plant, t->meat,
                             t->armour, t->weapon, t->legs, t->swim};
    uint64_t key = b->species & 31u;
    for (int i = 0; i < 9; i++) {
        key = key << 2 | trait_step(traits[i]);
    }
    key = key << 3 | (b->k_level & 7u);
    key = key << 2 | (b->base & 3u);
    key = key << 3 | ((unsigned)action & 7u);
    key = key << 6 | ((unsigned)frame & 63u);
    /* Wings spread or folded: one more bit, at the top (38 bits in all). The
     * sea and air lines need none, the species already says them. */
    key |= (uint64_t)(t->aloft > 0.5f) << 37;
    return key;
}

/* Which chain a key is in. buckets is a power of two, so & (buckets - 1)
 * keeps the hash's low bits: the same as % buckets, for less work. */
static int bucket_of(const SpriteStore *st, uint64_t key)
{
    uint32_t h = gfx_mix((uint32_t)key ^ gfx_mix((uint32_t)(key >> 32)));
    return (int)(h & (uint32_t)(st->buckets - 1));
}

static void store_reset(SpriteStore *st)
{
    for (int b = 0; b < st->buckets; b++) {
        st->heads[b] = -1;
    }
    st->used = 0;
    st->newest = st->oldest = -1;
}

static bool store_init(SpriteStore *st, int capacity, int slot_bytes)
{
    capacity = capacity < 1 ? 1 : capacity;
    int buckets = 1;
    while (buckets < capacity) {
        buckets *= 2;
    }
    st->capacity = capacity;
    st->buckets = buckets;
    st->slot_bytes = slot_bytes;
    st->entries = malloc((size_t)capacity * sizeof(SpriteEntry));
    /* The inks are only written as pictures are made, so this block is left as
     * malloc gives it: an untouched block costs the system no memory yet. */
    st->inks = malloc((size_t)capacity * (size_t)slot_bytes);
    st->heads = malloc((size_t)buckets * sizeof(int32_t));
    if (st->entries == NULL || st->inks == NULL || st->heads == NULL) {
        return false;
    }
    store_reset(st);
    return true;
}

static void store_free(SpriteStore *st)
{
    free(st->entries); /* free(NULL) does nothing */
    free(st->inks);
    free(st->heads);
    *st = (SpriteStore){0};
}

/* The entry holding key, or -1. */
static int store_find(const SpriteStore *st, uint64_t key, int bucket)
{
    for (int e = st->heads[bucket]; e >= 0; e = st->entries[e].next) {
        if (st->entries[e].key == key) {
            return e;
        }
    }
    return -1;
}

/* Takes entry e out of its chain. */
static void store_unlink(SpriteStore *st, int e)
{
    int bucket = bucket_of(st, st->entries[e].key);
    int32_t *link = &st->heads[bucket];
    /* link points at whatever holds the next index: the bucket's head, or an
     * entry's next. Writing through it skips e, wherever e is in the chain. */
    while (*link >= 0 && *link != e) {
        link = &st->entries[*link].next;
    }
    if (*link == e) {
        *link = st->entries[e].next;
    }
}

/* Takes entry e out of the use order, joining its two neighbours. */
static void use_unlink(SpriteStore *st, int e)
{
    SpriteEntry *x = &st->entries[e];
    if (x->newer >= 0) {
        st->entries[x->newer].older = x->older;
    } else {
        st->newest = x->older;
    }
    if (x->older >= 0) {
        st->entries[x->older].newer = x->newer;
    } else {
        st->oldest = x->newer;
    }
}

/* Puts entry e at the newest end of the use order: it was just asked for. */
static void use_push_newest(SpriteStore *st, int e)
{
    SpriteEntry *x = &st->entries[e];
    x->newer = -1;
    x->older = st->newest;
    if (st->newest >= 0) {
        st->entries[st->newest].newer = e;
    } else {
        st->oldest = e;
    }
    st->newest = e;
}

/* An entry for a new picture: the next unused one, or once all are in use the
 * one asked for longest ago, the oldest end of the use order, taken out of its
 * hash chain and out of the use order. Either way it is not on the use order
 * when returned; sprite_picture puts it at the newest end. */
static int store_take(SpriteStore *st)
{
    if (st->used < st->capacity) {
        return st->used++;
    }
    int oldest = st->oldest;
    use_unlink(st, oldest);
    store_unlink(st, oldest);
    return oldest;
}

/* Empties the standing measures: every key KEY_NONE, which no body has. */
static void standing_reset(SpriteCache *c)
{
    for (int i = 0; i < SPRITE_STANDING_ENTRIES; i++) {
        c->standing[i].key = KEY_NONE;
    }
}

/* The place body key `key` is kept in. & (SPRITE_STANDING_ENTRIES - 1) keeps
 * the hash's low bits, as bucket_of does. */
static SpriteStanding *standing_slot(SpriteCache *c, uint64_t key)
{
    uint32_t h = gfx_mix((uint32_t)key ^ gfx_mix((uint32_t)(key >> 32)));
    return &c->standing[h & (SPRITE_STANDING_ENTRIES - 1)];
}

bool sprite_cache_init(SpriteCache *c, int entries, int detail_entries)
{
    *c = (SpriteCache){0}; /* every pointer NULL: free is safe on a half-made cache */
    c->art = true;
    c->raster = malloc(sizeof(SkeletonRaster));
    c->standing = malloc(SPRITE_STANDING_ENTRIES * sizeof(SpriteStanding));
    int whole = entries / 6 < 1 ? 1 : entries / 6;
    if (c->raster == NULL || c->standing == NULL ||
        !store_init(&c->trimmed, entries, SPRITE_TRIMMED_BYTES) ||
        !store_init(&c->small, whole, SKELETON_W * SKELETON_H) ||
        !store_init(&c->large, detail_entries, SKELETON_DETAIL_W * SKELETON_DETAIL_H)) {
        sprite_cache_free(c);
        return false;
    }
    standing_reset(c);
    return true;
}

void sprite_cache_free(SpriteCache *c)
{
    store_free(&c->trimmed);
    store_free(&c->small);
    store_free(&c->large);
    free(c->raster);
    c->raster = NULL;
    free(c->standing);
    c->standing = NULL;
}

void sprite_cache_reset(SpriteCache *c)
{
    if (c->trimmed.heads != NULL) {
        store_reset(&c->trimmed);
    }
    if (c->small.heads != NULL) {
        store_reset(&c->small);
    }
    if (c->large.heads != NULL) {
        store_reset(&c->large);
    }
    if (c->standing != NULL) {
        standing_reset(c);
    }
}

void sprite_cache_set_art(SpriteCache *c, bool on)
{
    if (c->art != on) {
        c->art = on;
        sprite_cache_reset(c);
    }
}

/* A float rounded into 0..255, for a shadow radius. */
static uint8_t radius_byte(float v)
{
    if (!(v > 0.0f)) {
        return 0;
    }
    return (uint8_t)(v >= 255.0f ? 255.0f : v + 0.5f);
}

/* The art's scale, lowered when its standing pose would be wider or taller
 * than the largest standing body allowed at this base (SPRITE_MAX_W by
 * SPRITE_MAX_H at Normal, times the base's factor). Matching areas makes a
 * long low drawing (the elephant's) wider than its skeleton, so a giant's
 * frames could pass the cap its skeleton keeps to. */
static float capped_scale(const SpriteBody *b, float scale)
{
    const ArtSet *set = art_set(b->species);
    int base = b->base < SPRITE_BASES ? b->base : SPRITE_BASE_NORMAL;
    float max_w = (float)SPRITE_MAX_W * BASE_FACTOR[base];
    float max_h = (float)SPRITE_MAX_H * BASE_FACTOR[base];
    if (set == NULL) {
        return scale;
    }
    if ((float)set->stand_w * scale > max_w) {
        scale = max_w / (float)set->stand_w;
    }
    if ((float)set->stand_h * scale > max_h) {
        scale = max_h / (float)set->stand_h;
    }
    return scale;
}

/* Draws the picture of b, in that frame of that action, into the scratch
 * raster. Nothing is kept yet: keep() does that, once the size is known. */
static void draw_raster(SpriteCache *c, const SpriteBody *b, int action, int frame,
                        Anatomy *a)
{
    species_anatomy(a, b->species, &b->traits, sprite_k(b));
    const ArtClip *clip = c->art ? art_clip(b->species, action, &b->traits) : NULL;
    SkeletonRaster *r = c->raster;
    /* With drawn frames, the skeleton still draws the body standing first:
     * its width, middle, lowest row and shadow say how big and where the
     * frames go, so the genes keep deciding the size. That is the same for
     * every pose, so it is drawn once a body and its measures kept
     * (SpriteStanding); a later picture of the body takes them from there.
     * Any action and pose give the body's key: ACTION_REST, pose 0. */
    uint64_t body_key = key_of(b, ACTION_REST, 0);
    SpriteStanding *st = clip != NULL ? standing_slot(c, body_key) : NULL;
    if (st != NULL && st->key == body_key) {
        r->w = st->w;
        r->h = st->h;
        r->ox = st->ox;
        r->oy = st->oy;
        r->x0 = st->x0;
        r->y0 = st->y0;
        r->x1 = st->x1;
        r->y1 = st->y1;
        r->shadow_rx = st->shadow_rx;
        r->shadow_ry = st->shadow_ry;
    } else {
        SpritePose pose;
        pose_at(&pose, b, clip != NULL ? ACTION_REST : action, clip != NULL ? 0 : frame);
        skeleton_raster(r, a, &pose, b->base != SPRITE_BASE_FAR,
                        b->base == SPRITE_BASE_DETAIL);
        if (clip == NULL) {
            return;
        }
    }
    float scale = art_scale_for(b->species, r->x1 - r->x0, r->y1 - r->y0);
    if (!(scale > 0.0f)) {
        /* Nothing standing to measure: the skeleton's picture stays. Its
         * measures are not kept, so the skeleton is drawn every time, as the
         * picture it is. */
        return;
    }
    if (st != NULL && st->key != body_key) {
        *st = (SpriteStanding){body_key, r->w, r->h, r->ox, r->oy,
                               r->x0, r->y0, r->x1, r->y1, r->shadow_rx, r->shadow_ry};
    }
    scale = capped_scale(b, scale);
    /* The middle pixel of the standing body, and its lowest row. */
    float anchor_x = (float)(r->x0 + r->x1 - 1) * 0.5f;
    float anchor_y = (float)(r->y1 - 1);
    /* The parts the body shows, painted over the frames: plates, a fin, a horn. */
    const ArtClip *layers[ART_PART_COUNT];
    art_layers(b->species, action, &b->traits, layers);
    art_raster(r, clip, layers, frame * clip->frames / SPRITE_FRAMES[action], scale,
               anchor_x, anchor_y);
    if (b->base == SPRITE_BASE_FAR) {
        r->eye_x = r->eye_y = -1;  /* Far pictures have no eyes, drawn or art */
    }
}

/* The store a picture of this many inks belongs in. */
static SpriteStore *store_for(SpriteCache *c, const SpriteBody *b, int cells)
{
    if (b->base == SPRITE_BASE_DETAIL) {
        return &c->large;
    }
    return cells <= SPRITE_TRIMMED_BYTES ? &c->trimmed : &c->small;
}

/* Copies the raster's inked box into entry e of st, as picture p, every
 * position moved by the box's corner (x0, y0) so the ground point, the eye and
 * the bounds keep their places on the body. */
static void keep(SpriteCache *c, SpriteStore *st, int e, const Anatomy *a)
{
    const SkeletonRaster *r = c->raster;
    int w = r->x1 > r->x0 ? r->x1 - r->x0 : 0;
    int h = r->y1 > r->y0 ? r->y1 - r->y0 : 0;
    uint8_t *ink = st->inks + (size_t)e * (size_t)st->slot_bytes;
    /* Row by row: a row of the box is w inks in a row of r->w. */
    for (int y = 0; y < h; y++) {
        memcpy(ink + (size_t)y * (size_t)w,
               r->ink + (size_t)(r->y0 + y) * (size_t)r->w + (size_t)r->x0, (size_t)w);
    }
    SpritePicture *p = &st->entries[e].picture;
    /* The casts to int16_t: every value is a grid position, under 128. */
    p->ink = ink;
    p->w = (int16_t)w;
    p->h = (int16_t)h;
    p->ox = (int16_t)(r->ox - r->x0);
    p->oy = (int16_t)(r->oy - r->y0);
    p->x0 = 0;
    p->y0 = 0;
    p->x1 = (int16_t)w;
    p->y1 = (int16_t)h;
    /* -1, -1 means no eye (Far), and stays so. */
    p->eye_x = (int16_t)(r->eye_x >= 0 ? r->eye_x - r->x0 : -1);
    p->eye_y = (int16_t)(r->eye_y >= 0 ? r->eye_y - r->y0 : -1);
    p->shadow_dx = (int16_t)floorf(r->body_dx + 0.5f);
    p->shadow_rx = radius_byte(r->shadow_rx);
    p->shadow_ry = radius_byte(r->shadow_ry);
    p->eye_big = r->eye_big;
    p->eye_detail = a->k > 1.6f;
}

/*
 * With drawn frames, pose `frame` of n shows art frame frame * frames / n
 * (draw_raster), and nothing else about the pose changes the picture: the
 * skeleton under the art is always posed standing. So every pose showing the
 * same art frame is handed the first of them, and they share one key and one
 * picture: a wolf's 24 walking poses become its 4 drawn frames.
 * The first pose showing art frame k is the smallest f with
 * f * frames >= k * n, which is k * n / frames rounded up; adding frames - 1
 * before dividing rounds an integer division up. Without art, or for a clip
 * that has more frames than poses, the pose stays as it is.
 */
static int first_pose_of_art_frame(const SpriteCache *c, const SpriteBody *b,
                                   int action, int frame)
{
    const ArtClip *clip = c->art ? art_clip(b->species, action, &b->traits) : NULL;
    int n = SPRITE_FRAMES[action];
    if (clip == NULL || clip->frames >= n) {
        return frame;
    }
    int k = frame * clip->frames / n;
    return (k * n + clip->frames - 1) / clip->frames;
}

SpritePicture sprite_picture(SpriteCache *c, const SpriteBody *b, int action, int frame)
{
    action = action >= 0 && action < ACTION_COUNT ? action : ACTION_REST;
    int n = SPRITE_FRAMES[action];
    /* % keeps the sign of a negative number, so n is added before the second %. */
    frame = (frame % n + n) % n;
    frame = first_pose_of_art_frame(c, b, action, frame);
    uint64_t key = key_of(b, action, frame);
    /* A small picture may be in either small store, by its size. */
    SpriteStore *found = b->base == SPRITE_BASE_DETAIL ? &c->large : &c->trimmed;
    int e = store_find(found, key, bucket_of(found, key));
    if (e < 0 && b->base != SPRITE_BASE_DETAIL) {
        found = &c->small;
        e = store_find(found, key, bucket_of(found, key));
    }
    if (e < 0) {
        Anatomy a;
        draw_raster(c, b, action, frame, &a);
        const SkeletonRaster *r = c->raster;
        int cells = (r->x1 - r->x0) * (r->y1 - r->y0);
        found = store_for(c, b, cells > 0 ? cells : 0);
        e = store_take(found);
        keep(c, found, e, &a);
        int bucket = bucket_of(found, key);
        found->entries[e].key = key;
        found->entries[e].next = found->heads[bucket];
        found->heads[bucket] = e;
        c->builds++;
    } else {
        use_unlink(found, e);  /* found: it moves from where it was to newest */
    }
    use_push_newest(found, e);
    return found->entries[e].picture;
}

/* The scars' places, as shares of the body's extent: middle across, middle
 * down. */
static const float SCAR_AT[3][2] = {{0.45f, 0.40f}, {0.65f, 0.55f}, {0.28f, 0.60f}};

static bool inked(const SpritePicture *p, int x, int y)
{
    return x >= p->x0 && x < p->x1 && y >= p->y0 && y < p->y1 &&
           p->ink[y * p->w + x] != INK_NONE;
}

/* Whether grid pixel (x, y) is the body itself, not its outline: inked, not
 * the outline's ink, and with ink on all four sides. (The soft top outline
 * shares the shadow's ink, so the ink alone cannot tell it from the body.) */
static bool on_body(const SpritePicture *p, int x, int y)
{
    return inked(p, x, y) && p->ink[y * p->w + x] != INK_OUTLINE &&
           inked(p, x - 1, y) && inked(p, x + 1, y) && inked(p, x, y - 1) &&
           inked(p, x, y + 1);
}

/* How many pixels of a cut centred on (mid_x, mid_y), length long and
 * slanting down to the right, land on the body. */
static int cut_hits(const SpritePicture *p, int mid_x, int mid_y, int length)
{
    int hits = 0;
    for (int d = 0; d < length; d++) {
        if (on_body(p, mid_x - length / 2 + d, mid_y - length / 2 + d)) {
            hits++;
        }
    }
    return hits;
}

/* Writes that cut's body pixels into out from n, no more than max in all;
 * returns the new count. */
static int add_cut(const SpritePicture *p, int mid_x, int mid_y, int length,
                   int16_t out[][2], int n, int max)
{
    /* A cut slanting down to the right, one pixel wide. */
    for (int d = 0; d < length; d++) {
        int x = mid_x - length / 2 + d, y = mid_y - length / 2 + d;
        if (n < max && on_body(p, x, y)) {
            /* The casts: x and y are inside the picture, which is small. */
            out[n][0] = (int16_t)x;
            out[n][1] = (int16_t)y;
            n++;
        }
    }
    return n;
}

int sprite_scar_pixels(const SpritePicture *p, int count, int16_t out[][2], int max)
{
    count = count > 3 ? 3 : count;
    int w = p->x1 - p->x0, h = p->y1 - p->y0;
    if (count <= 0 || w <= 0 || h <= 0) {
        return 0;
    }
    int length = w / 4 < 3 ? 3 : w / 4;
    int n = 0;
    for (int k = 0; k < count; k++) {
        int mid_x = p->x0 + (int)(SCAR_AT[k][0] * (float)w);
        int mid_y = p->y0 + (int)(SCAR_AT[k][1] * (float)h);
        /* A cut that misses the body (it falls between a hare's tall ears)
         * slides down a row at a time until it lands on fur, at most half
         * the picture's height. */
        int lowest = mid_y + h / 2;
        while (mid_y < lowest && cut_hits(p, mid_x, mid_y, length) == 0) {
            mid_y++;
        }
        n = add_cut(p, mid_x, mid_y, length, out, n, max);
    }
    return n;
}

/* Adds one eye pixel to out at n, and returns the new count. */
static int eye_pixel(SpriteEyePixel *out, int n, int dx, int dy, uint8_t ink)
{
    out[n] = (SpriteEyePixel){(int8_t)dx, (int8_t)dy, ink};
    return n + 1;
}

int sprite_eye_pixels(const SpritePicture *p, float eyes,
                      SpriteEyePixel out[SPRITE_EYE_PIXELS_MAX])
{
    if (p->eye_x < 0 || p->eye_y < 0) {
        return 0;
    }
    int n = 0;
    if (eyes <= 0.0f) {
        /* Shut: a line of the darkest shade, longer on a detailed body. */
        n = eye_pixel(out, n, 0, 0, INK_DEEP);
        n = eye_pixel(out, n, 1, 0, INK_DEEP);
        if (p->eye_detail) {
            n = eye_pixel(out, n, -1, 0, INK_DEEP);
        }
    } else if (p->eye_big && p->eye_detail) {
        /* Three rows open with a glint; one row half shut. */
        bool open = eyes >= 1.0f;
        int rows = open ? 3 : 1;
        for (int a = -1; a <= 1; a++) {
            for (int b = 0; b < rows; b++) {
                n = eye_pixel(out, n, a, b - (open ? 1 : 0), INK_EYE);
            }
        }
        if (open) {
            n = eye_pixel(out, n, 0, -1, INK_EYE_WHITE);
        }
    } else if (p->eye_big || p->eye_detail) {
        n = eye_pixel(out, n, 0, 0, INK_EYE);
        n = eye_pixel(out, n, 1, 0, INK_EYE);
        if (eyes >= 1.0f) {
            n = eye_pixel(out, n, 1, -1, INK_EYE);
            n = eye_pixel(out, n, 0, -1, INK_EYE_WHITE);
        }
    } else {
        n = eye_pixel(out, n, 0, 0, INK_EYE); /* one pixel: a blink cannot show */
    }
    return n;
}
