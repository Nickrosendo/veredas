#include "icons.h"

#include "text.h"

/*
 * Placeholders until the drawn icons land: a coloured square per group with
 * a letter in the fixed font. The groups: the watching tools, the Sandbox
 * tools, the seasons and weather, and the rest.
 */
enum { GROUP_WATCH, GROUP_SANDBOX, GROUP_SKY, GROUP_OTHER };

typedef struct {
    char letter;
    int group;
} Placeholder;

/* [ICON_PAUSE] = ... is a "designated initializer": each entry says which
 * index it fills, so the table can't silently shift if the enum is reordered. */
static const Placeholder PLACEHOLDERS[ICON_COUNT] = {
    [ICON_PAUSE] = {'P', GROUP_WATCH},
    [ICON_PLAY] = {'>', GROUP_WATCH},
    [ICON_SLOWER] = {'-', GROUP_WATCH},
    [ICON_FASTER] = {'+', GROUP_WATCH},
    [ICON_ZOOM_OUT] = {'[', GROUP_WATCH},
    [ICON_ZOOM_IN] = {']', GROUP_WATCH},
    [ICON_STORY] = {'L', GROUP_WATCH},
    [ICON_NOTICES] = {'E', GROUP_WATCH},
    [ICON_CAMERA] = {'F', GROUP_WATCH},
    [ICON_JUMP] = {'J', GROUP_WATCH},
    [ICON_TRAIT] = {'G', GROUP_SANDBOX},
    [ICON_WEATHER] = {'W', GROUP_SANDBOX},
    [ICON_SKIP] = {'K', GROUP_SANDBOX},
    [ICON_BRANCH] = {'X', GROUP_SANDBOX},
    [ICON_OTHER_BRANCH] = {'T', GROUP_SANDBOX},
    [ICON_COMPARE] = {'V', GROUP_SANDBOX},
    [ICON_NEW_START] = {'U', GROUP_SANDBOX},
    [ICON_MODE_WATCH] = {'D', GROUP_OTHER},
    [ICON_MODE_SANDBOX] = {'S', GROUP_OTHER},
    [ICON_WORLD] = {'O', GROUP_OTHER},
    [ICON_HELP] = {'?', GROUP_OTHER},
    [ICON_CLOSE] = {'x', GROUP_OTHER},
    [ICON_LET_GO] = {'<', GROUP_OTHER},
    [ICON_SPRING] = {'s', GROUP_SKY},
    [ICON_SUMMER] = {'S', GROUP_SKY},
    [ICON_AUTUMN] = {'a', GROUP_SKY},
    [ICON_WINTER] = {'w', GROUP_SKY},
    [ICON_ANIMALS] = {'A', GROUP_OTHER},
    [ICON_DROUGHT] = {'d', GROUP_SKY},
    [ICON_HARD_WINTER] = {'h', GROUP_SKY},
    [ICON_WARM_YEAR] = {'y', GROUP_SKY},
};

static uint32_t group_colour(int group)
{
    switch (group) {
    case GROUP_WATCH:
        return canvas_rgb(0x4d, 0x66, 0x90);
    case GROUP_SANDBOX:
        return canvas_rgb(0xb0, 0x84, 0x4a);
    case GROUP_SKY:
        return canvas_rgb(0x4a, 0x8a, 0x6a);
    default:
        return canvas_rgb(0x6a, 0x6a, 0x7a);
    }
}

void icon_draw(Canvas *c, int x, int y, int icon)
{
    if (icon < 0 || icon >= ICON_COUNT) {
        return;
    }
    const Placeholder *p = &PLACEHOLDERS[icon];
    canvas_rect(c, x + 1, y + 1, ICON_SIZE - 2, ICON_SIZE - 2,
                group_colour(p->group));
    /* A one-letter string: the letter and the terminator. */
    char one[2] = {p->letter, '\0'};
    int w = text_width(one);
    text_draw(c, x + (ICON_SIZE - w) / 2, y, one, canvas_rgb(255, 255, 255));
}
