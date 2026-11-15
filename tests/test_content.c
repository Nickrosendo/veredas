/*
 * The content pack switch (content.h): what the build has, which worlds it
 * opens, and the art lookup that puts a pack's frames before the built-in
 * ones. Passes in the public build and in a build with a pack
 * (make test CONTENT=...), checking what each should have.
 */
#include "test.h"

#include <string.h> /* strlen */

#include "art.h"
#include "content.h"
#include "species.h"

static void test_what_this_build_has(void)
{
    const Content *pack = content_get();
    CHECK(pack != NULL);
    CHECK(pack->name != NULL);
    CHECK(strlen(pack->name) <= CONTENT_NAME_MAX);
    /* A count without its table, or the other way round, would be a broken pack. */
    CHECK((pack->art_sets == NULL) == (pack->art_set_count == 0));
    CHECK(pack->art_set_count >= 0);
    if (pack->name[0] == '\0') {
        /* The public build: no pack, nothing added. */
        CHECK(pack->art_sets == NULL);
    }
}

static void test_which_worlds_open(void)
{
    CHECK(content_has(""));                     /* needs none: opens anywhere */
    CHECK(content_has(content_get()->name));    /* needs this build's own pack */
    CHECK(!content_has("no-such-pack"));
}

/* art_set_in on a made-up table, so it needs no pack to test. */
static void test_lookup_in_a_table(void)
{
    const ArtSet sets[2] = {{.species = SPECIES_FOX}, {.species = SPECIES_WOLF}};
    CHECK(art_set_in(sets, 2, SPECIES_WOLF) == &sets[1]);
    CHECK(art_set_in(sets, 2, SPECIES_FOX) == &sets[0]);
    CHECK(art_set_in(sets, 2, SPECIES_GOAT) == NULL);
    CHECK(art_set_in(sets, 1, SPECIES_WOLF) == NULL); /* only count are looked at */
    CHECK(art_set_in(NULL, 0, SPECIES_WOLF) == NULL);
}

/* Every species answers with the pack's frames when it has some, the built-in
 * ones otherwise: in the public build, always the built-in ones. */
static void test_pack_frames_come_first(void)
{
    const Content *pack = content_get();
    for (int sp = 0; sp < SPECIES_COUNT; sp++) {
        const ArtSet *mine = art_set_in(pack->art_sets, pack->art_set_count, sp);
        const ArtSet *built_in = art_set_in(ART_SETS, ART_SET_COUNT, sp);
        CHECK(art_set(sp) == (mine != NULL ? mine : built_in));
    }
    /* The public build draws all 17 species from the built-in frames. */
    if (pack->art_sets == NULL) {
        for (int sp = 0; sp < SPECIES_COUNT; sp++) {
            CHECK(art_set(sp) != NULL);
        }
    }
}

int main(void)
{
    test_what_this_build_has();
    test_which_worlds_open();
    test_lookup_in_a_table();
    test_pack_frames_come_first();
    TEST_SUMMARY();
}
