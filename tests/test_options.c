#include "test.h"

#include <string.h> /* strstr */

#include "app.h"
#include "options.h"

static bool size_ok(const char *text, int w, int h)
{
    int pw = -1, ph = -1;
    return parse_size(text, &pw, &ph) && pw == w && ph == h;
}

static bool size_rejected(const char *text)
{
    int w = -1, h = -1;
    /* Rejected, and the outputs left alone. */
    return !parse_size(text, &w, &h) && w == -1 && h == -1;
}

static void test_parse_size(void)
{
    CHECK(size_ok("80x40", 80, 40));
    CHECK(size_ok("1x1", 1, 1));
    CHECK(size_ok("4096x4096", OPTIONS_MAX_SIDE, OPTIONS_MAX_SIDE));
    CHECK(size_rejected("0x10"));
    CHECK(size_rejected("-5x10"));
    CHECK(size_rejected("80"));
    CHECK(size_rejected("80x"));
    CHECK(size_rejected("x40"));
    CHECK(size_rejected("80x40z"));
    CHECK(size_rejected("99999999x99999999"));
    CHECK(size_rejected(""));
    CHECK(size_rejected(" 80x40"));
    CHECK(size_rejected("80x 40"));
    CHECK(size_rejected("+80x40"));
    CHECK(size_rejected("4097x10"));
    CHECK(size_rejected("80X40"));
}

static void test_parse_seed(void)
{
    uint64_t seed = 0;
    CHECK(parse_seed("123", &seed) && seed == 123);
    CHECK(parse_seed("0x7b", &seed) && seed == 0x7b);
    CHECK(parse_seed("18446744073709551615", &seed) && seed == UINT64_MAX);
    CHECK(!parse_seed("18446744073709551616", &seed)); /* 2^64 */
    CHECK(!parse_seed("", &seed));
    CHECK(!parse_seed("-1", &seed));
    CHECK(!parse_seed(" 5", &seed));
    CHECK(!parse_seed("12a", &seed));
    CHECK(!parse_seed("banana", &seed));
}

static void test_parse_long(void)
{
    long v = 0;
    CHECK(parse_long("10", 1, 100, &v) && v == 10);
    CHECK(parse_long("-3", -5, 5, &v) && v == -3);
    CHECK(!parse_long("101", 1, 100, &v));
    CHECK(!parse_long("0", 1, 100, &v));
    CHECK(!parse_long("", 1, 100, &v));
    CHECK(!parse_long("5x", 1, 100, &v));
    CHECK(!parse_long("99999999999999999999", 1, 100, &v));
}

/* Runs options_parse on a NULL-ended list of arguments. The arguments
 * are string literals; argv is an array of pointers to them, which
 * getopt_long may reorder without ever writing into the strings. */
static bool parse(char **argv, Options *o, char *err, size_t size)
{
    int argc = 0;
    while (argv[argc] != NULL) {
        argc++;
    }
    return options_parse(argc, argv, o, err, size);
}

static bool fails_with(char **argv, const char *want);

static void test_defaults(void)
{
    char *argv[] = {"veredas", NULL};
    Options o;
    char err[128];
    CHECK(parse(argv, &o, err, sizeof(err)));
    CHECK(!o.seed_given && !o.size_given && !o.once && !o.no_floor);
    CHECK(o.speed == 0);
    CHECK(o.ticks == OPTIONS_DEFAULT_TICKS);
    CHECK(!o.help && !o.version);
    CHECK(o.ancestors == -1); /* not given: the setting decides */
}

static void test_ancestors_option(void)
{
    char *argv[] = {"veredas", "--ancestors", "12", NULL};
    Options o;
    char err[128];
    CHECK(parse(argv, &o, err, sizeof(err)));
    CHECK(o.ancestors == 12);
    char *none[] = {"veredas", "--ancestors", "0", NULL};
    CHECK(parse(none, &o, err, sizeof(err)) && o.ancestors == 0);
    char *negative[] = {"veredas", "--ancestors", "-1", NULL};
    CHECK(fails_with(negative, "--ancestors"));
    char *huge[] = {"veredas", "--ancestors", "1001", NULL};
    CHECK(fails_with(huge, "--ancestors"));
    char *word[] = {"veredas", "--ancestors", "many", NULL};
    CHECK(fails_with(word, "--ancestors"));
}

static void test_every_option(void)
{
    char *argv[] = {"veredas",    "--seed",  "0x10",       "--size", "80x40",
                    "--speed", "30",      "--once",     "--ticks", "500",
                    "--no-floor", NULL};
    Options o;
    char err[128];
    CHECK(parse(argv, &o, err, sizeof(err)));
    CHECK(o.seed_given && o.seed == 16);
    CHECK(o.size_given && o.width == 80 && o.height == 40);
    CHECK(o.speed == 30);
    CHECK(o.once && o.ticks == 500 && o.no_floor);

    /* --name=value works too, and so does -h for --help. */
    char *argv2[] = {"veredas", "--seed=7", "-h", NULL};
    CHECK(parse(argv2, &o, err, sizeof(err)));
    CHECK(o.seed == 7 && o.help);
}

/* True when parsing fails with a message containing want. */
static bool fails_with(char **argv, const char *want)
{
    Options o;
    char err[128] = "";
    bool ok = parse(argv, &o, err, sizeof(err));
    return !ok && strstr(err, want) != NULL;
}

static void test_mistakes(void)
{
    char *unknown[] = {"veredas", "--bogus", NULL};
    CHECK(fails_with(unknown, "--bogus"));
    char *unknown_short[] = {"veredas", "-z", NULL};
    CHECK(fails_with(unknown_short, "-z"));
    char *missing[] = {"veredas", "--seed", NULL};
    CHECK(fails_with(missing, "--seed"));
    char *bad_seed[] = {"veredas", "--seed", "banana", NULL};
    CHECK(fails_with(bad_seed, "banana"));
    char *bad_size[] = {"veredas", "--size", "0x0", NULL};
    CHECK(fails_with(bad_size, "0x0"));
    char *bad_speed[] = {"veredas", "--speed", "0", NULL};
    CHECK(fails_with(bad_speed, "speed"));
    char *bad_ticks[] = {"veredas", "--ticks", "-1", NULL};
    CHECK(fails_with(bad_ticks, "ticks"));
    char *extra[] = {"veredas", "world", NULL};
    CHECK(fails_with(extra, "world"));
}

static void test_file_options(void)
{
    char *argv[] = {"veredas", "--config", "a.conf", "--save", "b.save", NULL};
    Options o;
    char err[128];
    CHECK(parse(argv, &o, err, sizeof(err)));
    CHECK(strcmp(o.config_path, "a.conf") == 0);
    CHECK(strcmp(o.save_path, "b.save") == 0);
    CHECK(o.load_path == NULL);

    char *resume[] = {"veredas", "--load", "b.save", "--speed", "5", NULL};
    CHECK(parse(resume, &o, err, sizeof(err)));
    CHECK(strcmp(o.load_path, "b.save") == 0);
    CHECK(o.config_path == NULL);

    char *list[] = {"veredas", "--settings", NULL};
    CHECK(parse(list, &o, err, sizeof(err)) && o.list_settings);

    char *with_seed[] = {"veredas", "--load", "x", "--seed", "1", NULL};
    CHECK(fails_with(with_seed, "--load"));
    char *with_size[] = {"veredas", "--load", "x", "--size", "8x8", NULL};
    CHECK(fails_with(with_size, "--load"));
    char *with_config[] = {"veredas", "--load", "x", "--config", "y", NULL};
    CHECK(fails_with(with_config, "--load"));
    char *with_floor[] = {"veredas", "--no-floor", "--load", "x", NULL};
    CHECK(fails_with(with_floor, "--load"));
    char *no_path[] = {"veredas", "--config", NULL};
    CHECK(fails_with(no_path, "--config"));
}

static void test_window_options(void)
{
    Options o;
    char err[128];
    char *none[] = {"veredas", NULL};
    CHECK(parse(none, &o, err, sizeof(err)));
    CHECK(!o.checksum && o.frames == 0);
    CHECK(o.screenshot_path == NULL && o.zoom_name == NULL);

    char *all[] = {"veredas", "--checksum", "--frames", "500", "--screenshot",
                   "a.ppm", "--zoom", "far", NULL};
    CHECK(parse(all, &o, err, sizeof(err)));
    CHECK(o.checksum);
    CHECK(o.frames == 500);
    CHECK(strcmp(o.screenshot_path, "a.ppm") == 0);
    CHECK(strcmp(o.zoom_name, "far") == 0);

    char *zero[] = {"veredas", "--frames", "0", NULL};
    CHECK(fails_with(zero, "--frames"));
    char *word[] = {"veredas", "--frames", "many", NULL};
    CHECK(fails_with(word, "--frames"));
    char *no_path[] = {"veredas", "--screenshot", NULL};
    CHECK(fails_with(no_path, "--screenshot"));
}

/* --window sets the window's starting size, read like --size, so a --frames
 * run can draw a 1920x1080 screenshot for the store page. Not given: both
 * sides stay 0 and the window program uses its own default. */
static void test_window_size_option(void)
{
    Options o;
    char err[128];
    char *none[] = {"veredas", NULL};
    CHECK(parse(none, &o, err, sizeof(err)));
    CHECK(o.window_w == 0 && o.window_h == 0);

    char *full_hd[] = {"veredas", "--window", "1920x1080", NULL};
    CHECK(parse(full_hd, &o, err, sizeof(err)));
    CHECK(o.window_w == 1920 && o.window_h == 1080);
    /* The world's size is a different thing, and stays not given. */
    CHECK(!o.size_given);

    char *bad[] = {"veredas", "--window", "1920", NULL};
    CHECK(fails_with(bad, "1920"));
    char *too_big[] = {"veredas", "--window", "9000x100", NULL};
    CHECK(fails_with(too_big, "9000x100"));
    char *no_value[] = {"veredas", "--window", NULL};
    CHECK(fails_with(no_value, "--window"));
}

/* --until names a moment the window program waits for after --frames; the
 * parser only keeps the name, and the window program checks it, as for
 * --zoom. */
static void test_until_option(void)
{
    Options o;
    char err[128];
    char *none[] = {"veredas", NULL};
    CHECK(parse(none, &o, err, sizeof(err)) && o.until_name == NULL);
    char *given[] = {"veredas", "--frames", "10", "--until", "pounce", NULL};
    CHECK(parse(given, &o, err, sizeof(err)));
    CHECK(o.until_name != NULL && strcmp(o.until_name, "pounce") == 0);
    char *no_value[] = {"veredas", "--until", NULL};
    CHECK(fails_with(no_value, "--until"));
}

static void test_paused_option(void)
{
    Options o;
    char err[128];
    char *none[] = {"veredas", NULL};
    CHECK(parse(none, &o, err, sizeof(err)) && !o.paused);
    char *given[] = {"veredas", "--paused", NULL};
    CHECK(parse(given, &o, err, sizeof(err)) && o.paused);
    CHECK(parse(none, &o, err, sizeof(err)) && !o.no_art);
    char *no_art[] = {"veredas", "--no-art", NULL};
    CHECK(parse(no_art, &o, err, sizeof(err)) && o.no_art);
    /* The parser accepts them; the terminal program's main refuses it, like
     * --frames (window_smoke.sh checks the exit code). */
}

static void test_parse_twice(void)
{
    /* getopt_long keeps its place in global variables. A second parse
     * must start from the beginning again, not where the first ended. */
    char *first[] = {"veredas", "--seed", "1", NULL};
    char *second[] = {"veredas", "--seed", "2", NULL};
    Options o;
    char err[128];
    CHECK(parse(first, &o, err, sizeof(err)) && o.seed == 1);
    CHECK(parse(second, &o, err, sizeof(err)) && o.seed == 2);
}

/* With no --speed both programs start at APP_DEFAULT_SPEED: 2 ticks a
 * second, slow enough to follow one creature. --speed still picks any step. */
static void test_default_speed(void)
{
    CHECK(APP_SPEEDS[APP_DEFAULT_SPEED] == 2);
    CHECK(app_speed_step(10) == 3); /* the old default is still a step */
}

int main(void)
{
    test_parse_size();
    test_parse_seed();
    test_parse_long();
    test_defaults();
    test_every_option();
    test_mistakes();
    test_file_options();
    test_window_options();
    test_window_size_option();
    test_until_option();
    test_paused_option();
    test_parse_twice();
    test_default_speed();
    test_ancestors_option();
    TEST_SUMMARY();
}
