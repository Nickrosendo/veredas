#!/bin/sh
# Runs the window program on SDL's dummy video driver, which needs no screen.
# Usage: tests/window_smoke.sh build/test
OUT=$1
export SDL_VIDEODRIVER=dummy
# SDL's drivers keep some memory on purpose, for the life of the program; the
# leak checker would report that as ours. Bad accesses are still caught.
export ASAN_OPTIONS=detect_leaks=0
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
failed=0

# True when two files hold the same bytes. cksum, not cmp: Windows' MSYS2 has
# coreutils (cksum) but not diffutils (cmp). Read from stdin, so the file's
# name is not part of the line.
same_file() {
    [ "$(cksum <"$1")" = "$(cksum <"$2")" ]
}

fail() {
    echo "window_smoke: FAILED: $1" >&2
    failed=1
}

# Windows has no terminal program, so the checks against it have a stand-in.
if [ -x "$OUT/veredas" ]; then
    HAVE_VEREDAS=1
else
    HAVE_VEREDAS=0
fi

# 1. The window's own path gives the same world as a plain run. Without one to
# compare with: the same world as a second run of itself.
window=$("$OUT/veredas-window" --seed 3 --size 64x32 --speed 30 --frames 500 --checksum)
if [ $HAVE_VEREDAS -eq 1 ]; then
    plain=$("$OUT/veredas" --once --seed 3 --size 64x32 --ticks 500 --checksum | grep '^checksum')
else
    plain=$("$OUT/veredas-window" --seed 3 --size 64x32 --speed 30 --frames 500 --checksum)
fi
[ -n "$window" ] && [ "$window" = "$plain" ] ||
    fail "checksums differ: window '$window', plain '$plain'"

# 2. Quitting saves, by the same path as the window's X button. Twice, because
# replacing an existing file is what Windows does differently.
for attempt in 1 2; do
    "$OUT/veredas-window" --seed 3 --size 64x32 --frames 10 --save "$TMP/w.save" ||
        fail "the run with --save failed (attempt $attempt)"
done
if [ $HAVE_VEREDAS -eq 1 ]; then
    "$OUT/veredas" --load "$TMP/w.save" --once --ticks 0 >/dev/null ||
        fail "the saved world did not load"
else
    "$OUT/veredas-window" --load "$TMP/w.save" --frames 1 >/dev/null ||
        fail "the saved world did not load"
fi

# 3. --once is refused, with the command-line mistake exit code.
"$OUT/veredas-window" --once >/dev/null 2>&1
[ $? -eq 2 ] || fail "--once was not refused with exit code 2"

# 4. And the terminal program refuses the window's options.
if [ $HAVE_VEREDAS -eq 1 ]; then
    "$OUT/veredas" --frames 5 >/dev/null 2>&1
    [ $? -eq 2 ] || fail "veredas accepted --frames"
fi

# 5. Every zoom step draws, and writes a picture of the right size.
for zoom in close normal far world; do
    "$OUT/veredas-window" --seed 3 --size 64x32 --frames 5 --zoom "$zoom" \
        --screenshot "$TMP/$zoom.ppm" || fail "the run at $zoom failed"
    [ "$(head -c 16 "$TMP/$zoom.ppm" | tr '\n' ' ')" = "P6 1280 720 255 " ] ||
        fail "the screenshot at $zoom has the wrong header"
done
"$OUT/veredas-window" --zoom sideways >/dev/null 2>&1
[ $? -eq 2 ] || fail "a bad --zoom was not refused"

# 5b. --window sets the size the picture is drawn at (the store page wants
# 1920x1080), and only the picture: the world is the same as at 1280x720.
small=$("$OUT/veredas-window" --seed 3 --size 64x32 --frames 50 --checksum)
big=$("$OUT/veredas-window" --seed 3 --size 64x32 --frames 50 --checksum \
    --window 1920x1080 --screenshot "$TMP/full_hd.ppm") ||
    fail "the run at 1920x1080 failed"
[ "$(head -c 17 "$TMP/full_hd.ppm" | tr '\n' ' ')" = "P6 1920 1080 255 " ] ||
    fail "the 1920x1080 screenshot has the wrong header"
[ "$small" = "$big" ] || fail "--window changed the world"
"$OUT/veredas-window" --window 1920 >/dev/null 2>&1
[ $? -eq 2 ] || fail "a bad --window was not refused"
if [ $HAVE_VEREDAS -eq 1 ]; then
    "$OUT/veredas" --window 1920x1080 >/dev/null 2>&1
    [ $? -eq 2 ] || fail "veredas accepted --window"
fi

# 5c. A --frames run moves its clock a fixed thirtieth of a second a frame, so
# the same command draws the same picture, byte for byte: bodies, water and all.
for run in a b; do
    "$OUT/veredas-window" --seed 3 --size 64x32 --frames 60 --zoom close \
        --screenshot "$TMP/same_$run.ppm" || fail "the run for the same picture failed"
done
same_file "$TMP/same_a.ppm" "$TMP/same_b.ppm" || fail "the same run drew two pictures"

# 5d. --until giant goes on after --frames until a giant is announced, then
# follows it and stops: the frame is printed, the same on a second run, and so is
# the picture. A young world announces one soon (frame 375 here).
for run in a b; do
    "$OUT/veredas-window" --seed 3 --size 64x32 --ancestors 1 --frames 10 \
        --until giant --screenshot "$TMP/giant_$run.ppm" >"$TMP/giant_$run.txt" ||
        fail "the --until giant run failed"
done
grep -q '^until frame [0-9]*$' "$TMP/giant_a.txt" ||
    fail "--until giant printed no frame"
same_file "$TMP/giant_a.txt" "$TMP/giant_b.txt" &&
    same_file "$TMP/giant_a.ppm" "$TMP/giant_b.ppm" ||
    fail "--until giant stopped differently the second time"
"$OUT/veredas-window" --frames 5 --until sunrise >/dev/null 2>&1
[ $? -eq 2 ] || fail "a bad --until was not refused"
"$OUT/veredas-window" --until giant >/dev/null 2>&1
[ $? -eq 2 ] || fail "--until without --frames was not refused"
if [ $HAVE_VEREDAS -eq 1 ]; then
    "$OUT/veredas" --until giant >/dev/null 2>&1
    [ $? -eq 2 ] || fail "veredas accepted --until"
fi

# 5e. --record writes every frame of a --frames run, PPM after PPM, for a video
# tool: three frames at 320x180 are three headers and three pictures' bytes,
# the same both times; without --frames it is refused, and so by veredas.
for run in a b; do
    "$OUT/veredas-window" --seed 3 --size 64x32 --frames 3 --window 320x180 \
        --record "$TMP/clip_$run.ppms" || fail "the --record run failed"
done
frame_bytes=$((15 + 320 * 180 * 3)) # the header "P6\n320 180\n255\n" is 15 bytes
[ "$(wc -c <"$TMP/clip_a.ppms")" -eq $((3 * frame_bytes)) ] ||
    fail "--record did not write three whole frames"
same_file "$TMP/clip_a.ppms" "$TMP/clip_b.ppms" ||
    fail "--record wrote two different clips"
# --after goes on that many frames past --until's moment: the giant run above
# stops on the moment; with --after 4 the clip holds 4 frames more than the
# frame it printed, and without --until --after is refused.
"$OUT/veredas-window" --seed 3 --size 64x32 --ancestors 1 --frames 10 \
    --window 320x180 --until giant --after 4 --record "$TMP/after.ppms" \
    >"$TMP/after.txt" || fail "the --after run failed"
moment=$(sed -n 's/^until frame //p' "$TMP/after.txt")
[ -n "$moment" ] || fail "--after: --until printed no frame"
[ "$(wc -c <"$TMP/after.ppms")" -eq $(((moment + 4) * frame_bytes)) ] ||
    fail "--after did not go on 4 frames past the moment"
"$OUT/veredas-window" --frames 5 --after 4 >/dev/null 2>&1
[ $? -eq 2 ] || fail "--after without --until was not refused"
"$OUT/veredas-window" --record "$TMP/live.ppms" >/dev/null 2>&1
[ $? -eq 2 ] || fail "--record without --frames was not refused"
if [ $HAVE_VEREDAS -eq 1 ]; then
    "$OUT/veredas" --record "$TMP/x.ppms" >/dev/null 2>&1
    [ $? -eq 2 ] || fail "veredas accepted --record"
fi

# 6. A world that dies. A small world has too few founders to last: all six seeds
# tried at 40x20 were empty by tick 5,000. The world stops on the tick it ended,
# so more frames must not change it, and its save loads to the same dead world.
dead_a=$("$OUT/veredas-window" --seed 1 --size 40x20 --speed 120 --frames 2500 \
    --save "$TMP/dead.save" --screenshot "$TMP/dead.ppm" --checksum) ||
    fail "the dying world's run failed"
dead_b=$("$OUT/veredas-window" --seed 1 --size 40x20 --speed 120 --frames 3000 --checksum) ||
    fail "the longer dying world's run failed"
[ -n "$dead_a" ] && [ "$dead_a" = "$dead_b" ] ||
    fail "a dead world went on changing: '$dead_a', then '$dead_b'"
dead_c=$("$OUT/veredas-window" --load "$TMP/dead.save" --frames 3 --checksum) ||
    fail "the dead save did not load"
[ "$dead_a" = "$dead_c" ] ||
    fail "the dead save came back different: '$dead_a', then '$dead_c'"
if [ $HAVE_VEREDAS -eq 1 ]; then
    text=$("$OUT/veredas" --load "$TMP/dead.save" --once --ticks 100 | tail -6)
    [ $? -eq 0 ] || fail "--once on a dead world did not exit 0"
    case "$text" in
    *"game over"*"last creature died"*"most alive"*"oldest"*"largest"*) ;;
    *) fail "--once did not print the game over lines: $text" ;;
    esac
fi

# 7. The life story page and jumping to a giant are only looking: pressing l, j,
# Esc and l again leaves the world exactly as a run without keys. Each run's
# keys come in its last frames, so the screenshot shows the page open.
keyed=$("$OUT/veredas-window" --seed 3 --size 64x32 --speed 30 --frames 500 \
    --keys "ljlj" --screenshot "$TMP/page.ppm" --checksum) ||
    fail "the run with --keys failed"
[ -n "$keyed" ] && [ "$keyed" = "$window" ] ||
    fail "pressing l and j changed the world: '$keyed' against '$window'"
[ "$(head -c 16 "$TMP/page.ppm" | tr '\n' ' ')" = "P6 1280 720 255 " ] ||
    fail "the screenshot with the page open has the wrong header"
if [ $HAVE_VEREDAS -eq 1 ]; then
    "$OUT/veredas" --keys l >/dev/null 2>&1
    [ $? -eq 2 ] || fail "veredas accepted --keys"
fi

# 8. The species lab is only looking: pressing b leaves the world exactly as a run
# without keys, and the picture is the right size.
lab=$("$OUT/veredas-window" --seed 3 --size 64x32 --speed 30 --frames 500 \
    --keys b --screenshot "$TMP/lab.ppm" --checksum) ||
    fail "the run with the lab open failed"
[ -n "$lab" ] && [ "$lab" = "$window" ] ||
    fail "opening the lab changed the world: '$lab' against '$window'"
[ "$(head -c 16 "$TMP/lab.ppm" | tr '\n' ' ')" = "P6 1280 720 255 " ] ||
    fail "the screenshot with the lab open has the wrong header"

# 9. --paused only looks: the world stays at tick 0, and the terminal program
# refuses the option.
paused=$("$OUT/veredas-window" --seed 3 --size 64x32 --frames 50 --paused --checksum) ||
    fail "the run with --paused failed"
if [ $HAVE_VEREDAS -eq 1 ]; then
    zero=$("$OUT/veredas" --once --seed 3 --size 64x32 --ticks 0 --checksum | grep '^checksum')
    "$OUT/veredas" --paused >/dev/null 2>&1
    [ $? -eq 2 ] || fail "veredas accepted --paused"
    "$OUT/veredas" --no-art >/dev/null 2>&1
    [ $? -eq 2 ] || fail "veredas accepted --no-art"
else
    zero=$("$OUT/veredas-window" --seed 3 --size 64x32 --frames 1 --paused --checksum)
fi
[ -n "$paused" ] && [ "$paused" = "$zero" ] ||
    fail "--paused did not stay at tick 0: '$paused' against '$zero'"

# 10. A world code opens the same world: c prints the code after a year of
# ancestors (paused, so the world stays at that tick), and --code, given the
# code alone or inside a link to the browser page, replays it to the same
# fingerprint. A text that is not a code, and --code with --load, are refused.
copied=$("$OUT/veredas-window" --seed 5 --size 64x32 --ancestors 1 --paused \
    --frames 3 --keys c --checksum) || fail "the run that copies a code failed"
code=$(echo "$copied" | sed -n 's/^world code: //p')
want=$(echo "$copied" | grep '^checksum')
[ -n "$code" ] || fail "c printed no world code"
for given in "$code" "https://example.org/veredas/?seed=9&code=$code#top"; do
    got=$("$OUT/veredas-window" --code "$given" --paused --frames 3 --checksum)
    [ -n "$want" ] && [ "$got" = "$want" ] ||
        fail "--code opened another world: '$got' against '$want'"
done
"$OUT/veredas-window" --code hello --frames 1 >/dev/null 2>&1
[ $? -eq 2 ] || fail "a --code that is not a code was not refused"
"$OUT/veredas-window" --code "$code" --load "$TMP/w.save" --frames 1 >/dev/null 2>&1
[ $? -eq 2 ] || fail "--code with --load was not refused"

# 11. --playlog writes the sitting's record on leaving: keys counted, the
# Sandbox reached, no button clicked (--keys are keys, not buttons); and the
# terminal program refuses it.
"$OUT/veredas-window" --seed 3 --size 64x32 --frames 20 --keys "mgg" \
    --playlog "$TMP/play.txt" >/dev/null || fail "the run with --playlog failed"
grep -q '^key g 2$' "$TMP/play.txt" && grep -q '^key m 1$' "$TMP/play.txt" &&
    ! grep -q '^first_sandbox_second -1$' "$TMP/play.txt" &&
    grep -q '^button other 0$' "$TMP/play.txt" && ! grep -q '^button g' "$TMP/play.txt" &&
    grep -q '^version ' "$TMP/play.txt" && grep -q '^first_verb_second -1$' "$TMP/play.txt" ||
    fail "the playlog is not as expected: $(tr '\n' '|' < "$TMP/play.txt")"
# A verb: m, then w 1 starts a drought; and x splits the world. Both are
# counted, with the moment each was first done.
"$OUT/veredas-window" --seed 3 --size 64x32 --frames 30 --keys "mw1x" \
    --playlog "$TMP/verb.txt" >/dev/null || fail "the run with a verb failed"
grep -q '^verb weather 1 ' "$TMP/verb.txt" && grep -q '^verb split 1 ' "$TMP/verb.txt" &&
    ! grep -q '^first_verb_second -1$' "$TMP/verb.txt" ||
    fail "the playlog missed the verbs: $(tr '\n' '|' < "$TMP/verb.txt")"
if [ $HAVE_VEREDAS -eq 1 ]; then
    "$OUT/veredas" --playlog "$TMP/x.txt" >/dev/null 2>&1
    [ $? -eq 2 ] || fail "veredas accepted --playlog"
fi

# 12. Every page drawn over the map has a close button at its top right, and
# the world panel's lines are buttons. At 1280x720 the map is x 0-1119 and
# y 24-695; a page is inset 8 pixels, so its 16-pixel x sits 2 pixels inside
# its corner, at (1094, 34). --hits prints "hit x y w h key" per button.
# Keys: 3001 follow the top hunter, 3020 close the story, 3021 the notices,
# 3022 the comparison, 3023 the key list (ui.h).
hits() {
    "$OUT/veredas-window" --seed 3 --size 64x32 --speed 30 --hits "$@"
}
hits --frames 20 | grep -q '^hit 1120 120 160 16 3001$' ||
    fail "the panel's top hunter line is not a button"
hits --frames 20 --keys l | grep -q '^hit 1094 34 16 16 3020$' ||
    fail "the story page has no close button at its top right"
hits --frames 20 --keys '?' | grep -q '^hit 1094 34 16 16 3023$' ||
    fail "the key list has no close button at its top right"
# Sandbox opens on its setup screen: Escape keeps the world, x splits it.
hits --frames 20 --sandbox --keys "$(printf '\033')xv" | grep -q '^hit [0-9]* [0-9]* 16 16 3022$' ||
    fail "the comparison has no close button"
hits --frames 3000 --speed 120 --keys e | grep -q '^hit 1094 34 16 16 3021$' ||
    fail "the notice list has no close button at its top right"
# While following (l opens the most notable life, j follows it, Esc closes
# the page), the panel is the animal's card: story (l, 108) and, in Sandbox,
# give trait (g, 103) are buttons on it, right of the map's 1120 pixels.
esc=$(printf '\033')
card=$(hits --frames 500 --sandbox --keys "${esc}lj${esc}~")
echo "$card" | grep -q '^hit 11[2-9][0-9] [0-9]* [0-9]* 20 108$' &&
    echo "$card" | grep -q '^hit 1[12][0-9][0-9] [0-9]* [0-9]* 20 103$' ||
    fail "the card has no story or give trait button"
if [ $HAVE_VEREDAS -eq 1 ]; then
    "$OUT/veredas" --hits >/dev/null 2>&1
    [ $? -eq 2 ] || fail "veredas accepted --hits"
fi

[ $failed -eq 0 ] && echo "window_smoke: all passed"
exit $failed
