#!/bin/bash
# The hunting verdict: the pass bar of the livelier hunting design, section 6.
# Runs seeds 1 to 15 side by side to year 23, watched from year 20, and
# applies the bars agreed before running:
#   1. land unharmed: walkers alive at year 23 in 15 of 15, and walkers' wound
#      deaths other than dives, summed over the 15 worlds, within 30 % of the
#      sum of engine 2123688's (tests/hunting_baseline.txt). Judged on the sum,
#      not per seed (Nicolas, 2026-11-23): any new rule replays every world
#      differently, so one seed swings -54 % to +66 % while the average holds;
#   2. the sea feeds its hunters: sea hunters alive at year 23 in at least 12
#      of 15, and at least 1,000 sea meat meals (fish and carrion eaten by sea
#      bodies) in years 20 to 23 in at least 10 of 15;
#   3. meat from the air: meat (carrion and fish) at least 20 % of flyers'
#      meals in at least 10 of 15, and at least 100 dive kills of walkers in
#      years 20 to 23 in at least 10 of 15. If the 100 fails, the bar falls
#      back to 50 dive kills in at least 10 of 15 (the design, section 10.1,
#      Nicolas 2026-11-23: agreed before the run, so there is no third round).
#      Both counts are printed; the fallback is used only when 100 fails;
#   4. sea and sky still there: tests/air_verdict.sh passes every bar (run
#      separately: bash tests/air_verdict.sh);
#   5. seen: on seeds 1 to 15, run to year 20 and watched for 1,200 ticks
#      (build/release/camera_picks), the Documentary camera picks a dive or a
#      hunt at sea at least once on at least 12 of 15, and over the fifteen
#      watches together at least a quarter of all picks are dives or hunts at
#      sea (the design, sections 10.2 and 11, Nicolas 2026-11-23: what a
#      viewer meets; five seeds were mostly luck, so fifteen).
#
# Usage: [KEEP=DIR] bash tests/hunting_verdict.sh [CONFIG]
#   CONFIG  a settings file passed to every world, for trying a change.
#   KEEP    a folder to copy every program's own output into (each world's
#           report and each camera watch, pick by pick), for reading later.
set -eu

CONFIG=${1:-}
SEEDS=15
YEAR_TICKS=600
TICKS=$((23 * YEAR_TICKS))
FROM=$((20 * YEAR_TICKS))
WATCH=1200
CAMERA_SEEDS=15
CAMERA_NEED=12
TOOL=build/release/balance_report
CAMERA=build/release/camera_picks
BASELINE=tests/hunting_baseline.txt

make -s balance camera >/dev/null
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

EXTRA=""
if [ -n "$CONFIG" ]; then
    EXTRA="--config $CONFIG"
fi

# The camera runs in the same batch: 15 + 15 programs side by side (more
# than the cores of a 20-core machine, so the run takes longer than 15 alone).
{
    seq 1 "$SEEDS" | sed 's/^/w /'
    seq 1 "$CAMERA_SEEDS" | sed 's/^/c /'
} | xargs -P $((SEEDS + CAMERA_SEEDS)) -L 1 sh -c '
    if [ "$0" = w ]; then
        '"$TOOL"' --seed "$1" --ticks '"$TICKS"' --sea '"$FROM"' '"$EXTRA"' > '"$TMP"'/"$1".txt
    else
        '"$CAMERA"' --seed "$1" --from '"$FROM"' --watch '"$WATCH"' '"$EXTRA"' > '"$TMP"'/cam"$1".txt
    fi'

if [ -n "${KEEP:-}" ]; then
    mkdir -p "$KEEP"
    cp "$TMP"/*.txt "$KEEP"/
fi

echo "hunting verdict: $SEEDS seeds to year 23, watched from year 20${CONFIG:+, config $CONFIG}"
echo
printf "%4s %7s %7s %6s %6s %8s %7s %6s %6s %6s %5s\n" seed walkers wounds base change sea_hunt sea_meat \
    air_meat plant meat% dives
walk_ok=0 wound_sum=0 base_sum=0 seah_ok=0 seam_ok=0 share_ok=0 dive_ok=0 dive50_ok=0
for seed in $(seq 1 "$SEEDS"); do
    air_line=$(grep '^AIR ' "$TMP/$seed.txt" || echo "AIR walkers 0 flyers 0 up 0 air_hunters 0 line_alive 0 plant_meals 0 meat_meals 0 landings 0")
    hunt_line=$(grep '^HUNT ' "$TMP/$seed.txt" || echo "HUNT land_wounds 0 dive_kills_land 0 dive_kills 0 sea_meat 0 sea_fish 0 air_meat 0 air_plant 0 air_fish 0 sea_hunters 0")
    read -r _ _ walkers _ _ _ _ _ _ _ _ _ _ _ _ _ _ <<< "$air_line"
    read -r _ _ wounds _ dives _ _ _ sea_meat _ _ _ air_meat _ air_plant _ _ _ sea_hunters <<< "$hunt_line"
    base=$(awk -v s="$seed" '$1 == s { print $2 }' "$BASELINE")
    # Whole percentages, in integer arithmetic.
    change=$(( (wounds - base) * 100 / base ))
    meals=$((air_meat + air_plant))
    share=0
    if [ "$meals" -gt 0 ]; then share=$((air_meat * 100 / meals)); fi
    printf "%4s %7s %7s %6s %5s%% %8s %7s %6s %6s %5s%% %5s\n" "$seed" "$walkers" "$wounds" "$base" \
        "$change" "$sea_hunters" "$sea_meat" "$air_meat" "$air_plant" "$share" "$dives"
    [ "$walkers" -gt 0 ] && walk_ok=$((walk_ok + 1))
    wound_sum=$((wound_sum + wounds))
    base_sum=$((base_sum + base))
    [ "$sea_hunters" -gt 0 ] && seah_ok=$((seah_ok + 1))
    [ "$sea_meat" -ge 1000 ] && seam_ok=$((seam_ok + 1))
    [ "$share" -ge 20 ] && share_ok=$((share_ok + 1))
    [ "$dives" -ge 100 ] && dive_ok=$((dive_ok + 1))
    [ "$dives" -ge 50 ] && dive50_ok=$((dive50_ok + 1))
done

# The per-seed change above is shown for reading; bar 1 judges the sum.
sum_change=$(( (wound_sum - base_sum) * 100 / base_sum ))
wound_ok=0
[ "$sum_change" -ge -30 ] && [ "$sum_change" -le 30 ] && wound_ok=1
printf "%4s %7s %7s %6s %5s%%\n" sum "" "$wound_sum" "$base_sum" "$sum_change"

echo
printf "%4s %6s %8s\n" seed picks sea_sky
cam_ok=0 picks_sum=0 sea_sky_sum=0
for seed in $(seq 1 "$CAMERA_SEEDS"); do
    read -r _ _ total _ sea_sky <<< "$(grep '^PICKS ' "$TMP/cam$seed.txt" || echo "PICKS total 0 sea_sky 0")"
    printf "%4s %6s %8s\n" "$seed" "$total" "$sea_sky"
    [ "$sea_sky" -ge 1 ] && cam_ok=$((cam_ok + 1))
    picks_sum=$((picks_sum + total))
    sea_sky_sum=$((sea_sky_sum + sea_sky))
done
printf "%4s %6s %8s\n" sum "$picks_sum" "$sea_sky_sum"
# At least a quarter, in whole numbers: 4 * hunts >= picks. With no picks at
# all the share is 0 and the bar fails.
cam_share=0
[ "$picks_sum" -gt 0 ] && cam_share=$((sea_sky_sum * 100 / picks_sum))
share_pass=0
[ "$picks_sum" -gt 0 ] && [ $((4 * sea_sky_sum)) -ge "$picks_sum" ] && share_pass=1

verdict() {
    if [ "$2" -ge "$3" ]; then echo "PASS  $1: $2 of $4 (needs $3)"
    else echo "FAIL  $1: $2 of $4 (needs $3)"; fi
}
echo
verdict "1 walkers alive at year 23" "$walk_ok" 15 "$SEEDS"
if [ "$wound_ok" -eq 1 ]; then
    echo "PASS  1 walkers' wound deaths (no dives), sum of 15: $sum_change% from 2123688's (needs within 30%)"
else
    echo "FAIL  1 walkers' wound deaths (no dives), sum of 15: $sum_change% from 2123688's (needs within 30%)"
fi
verdict "2 sea hunters alive at year 23" "$seah_ok" 12 "$SEEDS"
verdict "2 at least 1,000 sea meat meals, years 20-23" "$seam_ok" 10 "$SEEDS"
verdict "3 meat at least 20% of flyers' meals, years 20-23" "$share_ok" 10 "$SEEDS"
verdict "3 at least 100 dive kills of walkers, years 20-23" "$dive_ok" 10 "$SEEDS"
if [ "$dive_ok" -lt 10 ]; then
    # The fallback agreed in section 10.1: judged at 50 only because 100 failed.
    verdict "3 fallback: at least 50 dive kills of walkers, years 20-23" "$dive50_ok" 10 "$SEEDS"
else
    echo "      (at least 50 dive kills: $dive50_ok of $SEEDS; the fallback is not needed)"
fi
verdict "5 camera picks a dive or sea hunt at least once in 1,200 ticks" "$cam_ok" "$CAMERA_NEED" "$CAMERA_SEEDS"
if [ "$share_pass" -eq 1 ]; then
    echo "PASS  5 dives and sea hunts of all picks: $sea_sky_sum of $picks_sum, $cam_share% (needs 25%)"
else
    echo "FAIL  5 dives and sea hunts of all picks: $sea_sky_sum of $picks_sum, $cam_share% (needs 25%)"
fi
echo "4 is the air verdict: bash tests/air_verdict.sh${CONFIG:+ $CONFIG}"
