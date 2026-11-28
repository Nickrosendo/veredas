#!/bin/bash
# The smooth window check: the pass bar of the smooth window design
# (notes: veredas/specs/2026-11-27-veredas-smooth-window-design.md, section 6),
# run on the machine whose smoothness counts. One table at the end, a PASS or
# FAIL per bar. Nothing else heavy should run meanwhile: every bar is a time.
#
#   1. the same world: make test, make test-slow and make test-threads green
#      (test_same_world, the seed-7 fingerprint and the scene pins are in
#      them), and tick_report's checksums the same as 93db157's on the grown
#      seed-4 world, tick for tick. make test-slow runs only with --slow: its
#      slow_sim steps a full-size world 60,000 ticks under the memory checkers,
#      about two hours on a cloud machine; without it bar 1 says INCOMPLETE;
#   2. a cheaper tick: tick_report on that world, 300 ticks, median, at least
#      20 % faster than 93db157 built on this machine;
#   3. growing a new world: seed 4, 8 ancestor years, 60 s of the prelude
#      screen: at least 28 frames a second, at most 0.5 slow frames (a gap
#      over 40 ms) a second;
#   4. watching a grown world: the saved 8-year world, Documentary, default
#      speed, 120 s at Normal and 120 s at Far: at least 28 frames a second and
#      at most 0.5 late frames a second at each;
#   5. Far within budget: at most 24 ms a frame at Far, 1920x1080, on the
#      surface and in the sky, public and paid builds (--frames 330 minus
#      --frames 30, over 300, the median of three rounds).
#
# Usage: bash tests/smooth_check.sh [--slow] [--skip-tests] [--keep DIR]
#   --slow        also run make test-slow (hours: see bar 1)
#   --skip-tests  leave out bar 1's test runs (bar 1 then says so and does not
#                 pass)
#   --keep DIR    copy every frame log and timing there, for reading later
# Environment:
#   CONTENT  the content pack, for the paid build (default ../veredas-content);
#            without it the paid cells of bar 5 are not run and bar 5 cannot pass
#   SDL_VIDEODRIVER  left as it is: on a desktop the real window opens (what a
#            player sees); "dummy" runs with no screen, as the cloud does
# Takes about 25 minutes on a cloud machine without --slow: 5 of them the live
# runs of bars 3 and 4, which open a window for 60 + 120 + 120 seconds.
SKIP_TESTS=0
SLOW=0
KEEP=""
while [ $# -gt 0 ]; do
    case $1 in
    --slow) SLOW=1 ;;
    --skip-tests) SKIP_TESTS=1 ;;
    --keep) shift; KEEP=${1:-} ;;
    *) echo "usage: bash tests/smooth_check.sh [--slow] [--skip-tests] [--keep DIR]" >&2; exit 2 ;;
    esac
    shift
done

cd "$(dirname "$0")/.." || exit 2
CONTENT=${CONTENT:-../veredas-content}
BEFORE=93db157 # main before the cheaper tick (the design, bar 2)
TMP=$(mktemp -d)
WINDOW=build/release/veredas-window
PAID_WINDOW=build/release-content/veredas-window
SAVE=$TMP/world.save
cleanup() {
    git worktree remove --force "$TMP/before" >/dev/null 2>&1
    rm -rf "$TMP"
}
trap cleanup EXIT

say() { printf '\n== %s\n' "$*"; }

# A clock in milliseconds: GNU date has %N; macOS's date has not, perl has.
now_ms() {
    local ns
    ns=$(date +%s%N 2>/dev/null)
    case $ns in
    *N | '') perl -MTime::HiRes=time -e 'printf "%d\n", time() * 1000' ;;
    *) echo $((ns / 1000000)) ;;
    esac
}

# The middle of its arguments (numbers).
median() {
    printf '%s\n' "$@" | sort -n | awk '{ v[NR] = $1 } END { print v[int((NR + 1) / 2)] }'
}

# ---------------------------------------------------------------------------
say "building the release programs"
# CONTENT= on make's line: a CONTENT set in the environment would otherwise
# send these public builds to build/release-content (the Makefile's ifdef).
make -s MODE=release CONTENT= build/release/veredas build/release/tick_report window || exit 1
HAVE_PAID=0
if [ -d "$CONTENT/src" ]; then
    make -s MODE=release CONTENT="$CONTENT" window && HAVE_PAID=1
fi

say "growing the seed-4 world, 256x144, 8 ancestor years (the benchmark world)"
build/release/veredas --once --seed 4 --size 256x144 --ancestors 8 --ticks 0 \
    --save "$SAVE" >/dev/null || exit 1

# ---------------------------------------------------------------------------
# Bar 1's tests.
T_TEST=skipped T_SLOW=skipped T_THREADS=skipped SAME_WORLD=""
if [ $SKIP_TESTS -eq 0 ]; then
    say "make test"
    make CONTENT= test >"$TMP/test.log" 2>&1 && T_TEST=green || T_TEST=red
    SAME_WORLD=$(grep '^seed 4 at 96x54, ticks 601-900' "$TMP/test.log" | head -1)
    if [ $SLOW -eq 1 ]; then
        say "make test-slow"
        make CONTENT= test-slow >"$TMP/slow.log" 2>&1 && T_SLOW=green || T_SLOW=red
    else
        T_SLOW="not run (--slow)"
    fi
    say "make test-threads"
    make CONTENT= test-threads >"$TMP/threads.log" 2>&1 && T_THREADS=green || T_THREADS=red
    echo "test $T_TEST, test-slow $T_SLOW, test-threads $T_THREADS"
fi

# ---------------------------------------------------------------------------
# Bar 2: 93db157 has no tick_report (the cheaper tick added it), so today's
# tests/tick_report.c is built against 93db157's own code, in a worktree: it
# uses only save_read, sim_step and the two checksums, which both have.
say "a tick, $BEFORE against this build (3 rounds each, taking turns)"
OLD_TICKS=""
if git worktree add --detach "$TMP/before" "$BEFORE" >/dev/null 2>&1; then
    cp tests/tick_report.c "$TMP/before/tests/"
    if (cd "$TMP/before" && make -s MODE=release CONTENT= build/release/veredas >/dev/null 2>&1 &&
        objs=$(ls build/release/*.o | grep -v '/main\.o$') &&
        ${CC:-gcc} -std=c11 -D_POSIX_C_SOURCE=200809L -Isrc -Isrc/gfx -pthread -O2 \
            tests/tick_report.c $objs -o build/release/tick_report -lm); then
        OLD_TICKS=$TMP/before/build/release/tick_report
    fi
fi
OLD_MS=() NEW_MS=() OLD_SUM="" NEW_SUM=""
if [ -n "$OLD_TICKS" ]; then
    for round in 1 2 3; do
        old=$("$OLD_TICKS" "$SAVE" 300)
        new=$(build/release/tick_report "$SAVE" 300)
        echo "  $BEFORE  $old"
        echo "  this    $new"
        OLD_MS+=("$(echo "$old" | sed 's/.*median \([0-9.]*\) ms.*/\1/')")
        NEW_MS+=("$(echo "$new" | sed 's/.*median \([0-9.]*\) ms.*/\1/')")
        OLD_SUM=$(echo "$old" | sed 's/.*checksums //')
        NEW_SUM=$(echo "$new" | sed 's/.*checksums //')
    done
else
    echo "  could not build $BEFORE's tick_report (is this a git clone with its history?)"
fi

# ---------------------------------------------------------------------------
# Bars 3 and 4: the window live, for a set time, then asked to quit (SIGTERM,
# which SDL turns into a quit, so the frame log is closed properly).
# live LOG SECONDS ARGS...
live() {
    local log=$1 secs=$2
    shift 2
    "$WINDOW" --window 1920x1080 --frame-log "$log" "$@" >/dev/null 2>&1 &
    local pid=$!
    sleep "$secs"
    kill -TERM $pid 2>/dev/null
    wait $pid 2>/dev/null
}

# Frames a second and slow frames a second from a frame log; with PRELUDE=1
# only the prelude's frames (its column 13). Prints "fps slow_per_s seconds".
smooth() {
    awk -v only="${2:-0}" 'NR > 2 && (!only || $13 == 1) {
        frames++; s += $2 / 1000; if ($2 > 40) slow++ }
    END { if (s == 0) { print "0 0 0"; exit }
          printf "%.1f %.2f %.0f\n", frames / s, slow / s, s }' "$1"
}

say "growing a new world: 60 s of the prelude screen (seed 4, 8 ancestor years)"
live "$TMP/prelude.log" 60 --seed 4 --ancestors 8
read -r P_FPS P_SLOW P_SECS <<<"$(smooth "$TMP/prelude.log" 1)"
echo "  $P_FPS frames a second, $P_SLOW slow a second, over $P_SECS s of prelude"

say "watching the grown world: 120 s at Normal"
live "$TMP/normal.log" 120 --load "$SAVE" --zoom normal
read -r N_FPS N_SLOW N_SECS <<<"$(smooth "$TMP/normal.log")"
echo "  $N_FPS frames a second, $N_SLOW late a second, over $N_SECS s"

say "watching the grown world: 120 s at Far"
live "$TMP/far.log" 120 --load "$SAVE" --zoom far
read -r F_FPS F_SLOW F_SECS <<<"$(smooth "$TMP/far.log")"
echo "  $F_FPS frames a second, $F_SLOW late a second, over $F_SECS s"

# ---------------------------------------------------------------------------
# Bar 5: the window speed work's measure, as in the far zoom speed study:
# --frames 330 minus --frames 30, over 300. The sky: five frames of nothing,
# then "<" (a key in the very first frames is lost while the world loads),
# then nothing to the end; --keys presses its keys in a run's last frames, so
# the text is as long as the run.
keys_for() {
    local n=$1 k="~~~~~<"
    while [ ${#k} -lt "$n" ]; do k="$k~"; done
    echo "$k"
}

# far_ms PROGRAM LEVEL: one round's ms a frame.
far_ms() {
    local prog=$1 level=$2 a b t0 t1 t2 short long
    short=() long=()
    if [ "$level" = sky ]; then
        short=(--keys "$(keys_for 30)")
        long=(--keys "$(keys_for 330)")
    fi
    t0=$(now_ms)
    "$prog" --load "$SAVE" --zoom far --window 1920x1080 --frames 30 "${short[@]}" >/dev/null 2>&1
    t1=$(now_ms)
    "$prog" --load "$SAVE" --zoom far --window 1920x1080 --frames 330 "${long[@]}" >/dev/null 2>&1
    t2=$(now_ms)
    a=$((t1 - t0))
    b=$((t2 - t1))
    awk -v a="$a" -v b="$b" 'BEGIN { printf "%.1f\n", (b - a) / 300 }'
}

say "Far at 1920x1080 (3 rounds of each)"
# One plain variable per cell (FAR_public_sky, ...): macOS's bash 3.2 has no
# arrays keyed by name.
for build in public paid; do
    prog=$WINDOW
    if [ $build = paid ]; then
        [ $HAVE_PAID -eq 1 ] || { echo "  paid: no content pack at $CONTENT, not run"; continue; }
        prog=$PAID_WINDOW
    fi
    for level in surface sky; do
        r=()
        for round in 1 2 3; do r+=("$(far_ms "$prog" $level)"); done
        m=$(median "${r[@]}")
        eval "FAR_${build}_$level=$m"
        echo "  $build, $level: ${r[*]} ms -> median $m ms"
    done
done

# ---------------------------------------------------------------------------
if [ -n "$KEEP" ]; then
    mkdir -p "$KEEP" && cp "$TMP"/*.log "$KEEP"/ 2>/dev/null
fi

pass() {
    case $1 in
    1) echo PASS ;;
    incomplete) echo INCOMPLETE ;;
    *) echo FAIL ;;
    esac
}
# at_least X MIN / at_most X MAX: 1 or 0, by awk (the shell has no decimals).
at_least() { awk -v x="$1" -v m="$2" 'BEGIN { print (x + 0 >= m + 0) ? 1 : 0 }'; }
at_most() { awk -v x="$1" -v m="$2" 'BEGIN { print (x != "" && x + 0 <= m + 0) ? 1 : 0 }'; }

# Bar 1
if [ $SKIP_TESTS -eq 1 ]; then
    B1=0; B1_TEXT="tests skipped (--skip-tests)"
else
    B1=$([ $T_TEST = green ] && [ $T_SLOW = green ] && [ $T_THREADS = green ] &&
        [ -n "$OLD_SUM" ] && [ "$OLD_SUM" = "$NEW_SUM" ] && echo 1 || echo 0)
    B1_TEXT="test $T_TEST, slow $T_SLOW, threads $T_THREADS"
    if [ $T_TEST = green ] && [ $T_THREADS = green ] && [ $SLOW -eq 0 ]; then
        B1=incomplete
    fi
fi
if [ -n "$OLD_SUM" ]; then
    if [ "$OLD_SUM" = "$NEW_SUM" ]; then
        B1_TEXT="$B1_TEXT; ticks same as $BEFORE"
    else
        B1=0; B1_TEXT="$B1_TEXT; ticks DIFFER from $BEFORE"
    fi
fi

# Bar 2
if [ ${#OLD_MS[@]} -gt 0 ]; then
    OLD_MED=$(median "${OLD_MS[@]}")
    NEW_MED=$(median "${NEW_MS[@]}")
    CUT=$(awk -v o="$OLD_MED" -v n="$NEW_MED" 'BEGIN { printf "%.0f", (1 - n / o) * 100 }')
    B2=$(at_least "$CUT" 20)
    B2_TEXT="$OLD_MED -> $NEW_MED ms (-$CUT %)"
else
    B2=0; B2_TEXT="not measured"
fi

# Bars 3 and 4
B3=$([ "$(at_least "$P_FPS" 28)" = 1 ] && [ "$(at_most "$P_SLOW" 0.5)" = 1 ] && echo 1 || echo 0)
B4=$([ "$(at_least "$N_FPS" 28)" = 1 ] && [ "$(at_most "$N_SLOW" 0.5)" = 1 ] &&
    [ "$(at_least "$F_FPS" 28)" = 1 ] && [ "$(at_most "$F_SLOW" 0.5)" = 1 ] && echo 1 || echo 0)

# Bar 5
B5=1 B5_TEXT=""
for cell in public_surface public_sky paid_surface paid_sky; do
    eval "v=\${FAR_$cell:-}"
    if [ -z "$v" ]; then
        B5=0; B5_TEXT="$B5_TEXT $cell not run;"
    else
        [ "$(at_most "$v" 24)" = 1 ] || B5=0
        B5_TEXT="$B5_TEXT $cell $v;"
    fi
done

printf '\n%s\n' "The smooth window check, $(git rev-parse --short HEAD), $(date '+%Y-%m-%d %H:%M')"
[ -n "$SAME_WORLD" ] && echo "test_same_world: $SAME_WORLD"
printf '%-4s %-26s %-6s %s\n' "bar" "what" "verdict" "measured"
printf '%-4s %-26s %-6s %s\n' 1 "the same world" "$(pass $B1)" "$B1_TEXT"
printf '%-4s %-26s %-6s %s\n' 2 "a cheaper tick (>= 20 %)" "$(pass $B2)" "$B2_TEXT"
printf '%-4s %-26s %-6s %s\n' 3 "growing a new world" "$(pass $B3)" \
    "$P_FPS fps, $P_SLOW slow/s ($P_SECS s of prelude)"
printf '%-4s %-26s %-6s %s\n' 4 "watching a grown world" "$(pass $B4)" \
    "Normal $N_FPS fps, $N_SLOW late/s; Far $F_FPS fps, $F_SLOW late/s"
printf '%-4s %-26s %-6s %s\n' 5 "Far within 24 ms" "$(pass $B5)" "ms a frame:$B5_TEXT"
echo "Bars 3 and 4 need at least 28 frames a second and at most 0.5 slow frames a second."
if [ "$B4" = 0 ] && [ "$B2" = 1 ] && [ "$B5" = 1 ]; then
    echo "Bar 4 fails with bars 2 and 5 passing: the design says the shown world's tick"
    echo "needs its own thread (approach C), a new talk, not more tuning."
fi
