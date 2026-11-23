#!/bin/bash
# The hunting verdict: the pass bar of the livelier hunting design, section 6.
# Runs seeds 1 to 15 side by side to year 23, watched from year 20, and
# applies the bars agreed before running:
#   1. land unharmed: walkers alive at year 23 in 15 of 15, and walkers' wound
#      deaths other than dives within 30 % of engine 2123688's on the same
#      seed (tests/hunting_baseline.txt) in 15 of 15;
#   2. the sea feeds its hunters: sea hunters alive at year 23 in at least 12
#      of 15, and at least 1,000 sea meat meals (fish and carrion eaten by sea
#      bodies) in years 20 to 23 in at least 10 of 15;
#   3. meat from the air: meat (carrion and fish) at least 20 % of flyers'
#      meals in at least 10 of 15, and at least 100 dive kills of walkers in
#      years 20 to 23 in at least 10 of 15;
#   4. sea and sky still there: tests/air_verdict.sh passes every bar (run
#      separately: bash tests/air_verdict.sh);
#   5. seen: on seeds 1 to 5, run to year 20 and watched for 1,200 ticks, the
#      Documentary camera picks a dive or a hunt at sea at least 3 times on at
#      least 4 of 5 (build/release/camera_picks).
#
# Usage: bash tests/hunting_verdict.sh [CONFIG]
#   CONFIG  a settings file passed to every world, for trying a change.
set -eu

CONFIG=${1:-}
SEEDS=15
YEAR_TICKS=600
TICKS=$((23 * YEAR_TICKS))
FROM=$((20 * YEAR_TICKS))
WATCH=1200
CAMERA_SEEDS=5
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

# The camera runs first in the same batch: 15 + 5 programs side by side.
{
    seq 1 "$SEEDS" | sed 's/^/w /'
    seq 1 "$CAMERA_SEEDS" | sed 's/^/c /'
} | xargs -P $((SEEDS + CAMERA_SEEDS)) -L 1 sh -c '
    if [ "$0" = w ]; then
        '"$TOOL"' --seed "$1" --ticks '"$TICKS"' --sea '"$FROM"' '"$EXTRA"' > '"$TMP"'/"$1".txt
    else
        '"$CAMERA"' --seed "$1" --from '"$FROM"' --watch '"$WATCH"' '"$EXTRA"' > '"$TMP"'/cam"$1".txt
    fi'

echo "hunting verdict: $SEEDS seeds to year 23, watched from year 20${CONFIG:+, config $CONFIG}"
echo
printf "%4s %7s %7s %6s %6s %8s %7s %6s %6s %6s %5s\n" seed walkers wounds base change sea_hunt sea_meat \
    air_meat plant meat% dives
walk_ok=0 wound_ok=0 seah_ok=0 seam_ok=0 share_ok=0 dive_ok=0
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
    [ "$change" -ge -30 ] && [ "$change" -le 30 ] && wound_ok=$((wound_ok + 1))
    [ "$sea_hunters" -gt 0 ] && seah_ok=$((seah_ok + 1))
    [ "$sea_meat" -ge 1000 ] && seam_ok=$((seam_ok + 1))
    [ "$share" -ge 20 ] && share_ok=$((share_ok + 1))
    [ "$dives" -ge 100 ] && dive_ok=$((dive_ok + 1))
done

echo
printf "%4s %6s %8s\n" seed picks sea_sky
cam_ok=0
for seed in $(seq 1 "$CAMERA_SEEDS"); do
    read -r _ _ total _ sea_sky <<< "$(grep '^PICKS ' "$TMP/cam$seed.txt" || echo "PICKS total 0 sea_sky 0")"
    printf "%4s %6s %8s\n" "$seed" "$total" "$sea_sky"
    [ "$sea_sky" -ge 3 ] && cam_ok=$((cam_ok + 1))
done

verdict() {
    if [ "$2" -ge "$3" ]; then echo "PASS  $1: $2 of $4 (needs $3)"
    else echo "FAIL  $1: $2 of $4 (needs $3)"; fi
}
echo
verdict "1 walkers alive at year 23" "$walk_ok" 15 "$SEEDS"
verdict "1 walkers' wound deaths (no dives) within 30% of 2123688" "$wound_ok" 15 "$SEEDS"
verdict "2 sea hunters alive at year 23" "$seah_ok" 12 "$SEEDS"
verdict "2 at least 1,000 sea meat meals, years 20-23" "$seam_ok" 10 "$SEEDS"
verdict "3 meat at least 20% of flyers' meals, years 20-23" "$share_ok" 10 "$SEEDS"
verdict "3 at least 100 dive kills of walkers, years 20-23" "$dive_ok" 10 "$SEEDS"
verdict "5 camera picks a dive or sea hunt 3+ times in 1,200 ticks" "$cam_ok" 4 "$CAMERA_SEEDS"
echo "4 is the air verdict: bash tests/air_verdict.sh${CONFIG:+ $CONFIG}"
