#!/bin/bash
# The air verdict: the whole pass bar of the water and air design, section 6,
# with the sea in place. Runs seeds 1 to 15 side by side to year 23 and applies
# the five bars agreed before running:
#   1. land unharmed: walkers (no sea body, no wings over the line) alive at
#      year 23 in 15 of 15 worlds;
#   2. sea and air life last: sea animals alive at year 23 in at least 12 of
#      15, and flyers alive in at least 12 of 15;
#   3. a sea food chain: sea hunters kill sea animals in years 20 to 23 in at
#      least 10 of 15;
#   4. flyers live up there: in years 20 to 23 they come down to eat, and both
#      diets show (at least MEALS meals of plants and MEALS of meat by flyers)
#      in at least 8 of 15;
#   5. evolution crosses over: in at least 3 of 15 worlds an animal of a line
#      that took to the sea or to the air by itself is alive at year 23.
#
# Usage: bash tests/air_verdict.sh [CONFIG]
#   CONFIG  a settings file passed to every world, for trying a change.
set -eu

CONFIG=${1:-}
SEEDS=15
YEAR_TICKS=600
TICKS=$((23 * YEAR_TICKS))
FROM=$((20 * YEAR_TICKS))
MEALS=5
TOOL=build/release/balance_report

make -s balance >/dev/null
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

EXTRA=""
if [ -n "$CONFIG" ]; then
    EXTRA="--config $CONFIG"
fi

seq 1 "$SEEDS" | xargs -P "$SEEDS" -I{} sh -c \
    "$TOOL --seed {} --ticks $TICKS --sea $FROM $EXTRA > $TMP/{}.txt"

echo "air verdict: $SEEDS seeds to year 23, watched from year 20${CONFIG:+, config $CONFIG}"
echo
printf "%4s %7s %5s %7s %4s %7s %6s %6s %6s %8s %8s\n" seed walkers sea flyers up hunters kills plant meat sea_line air_line
land_ok=0 sea_ok=0 air_ok=0 chain_ok=0 diet_ok=0 cross_ok=0
for seed in $(seq 1 "$SEEDS"); do
    sea_line=$(grep '^SEA ' "$TMP/$seed.txt" || echo "SEA land 0 sea 0 sea_hunters 0 kills_watched 0 kills_all 0 line_births 0 line_alive 0")
    air_line=$(grep '^AIR ' "$TMP/$seed.txt" || echo "AIR walkers 0 flyers 0 up 0 air_hunters 0 line_alive 0 plant_meals 0 meat_meals 0 landings 0")
    read -r _ _ _ _ sea _ _ _ watched _ _ _ _ _ sea_alive <<< "$sea_line"
    read -r _ _ walkers _ flyers _ up _ hunters _ air_alive _ plant _ meat _ _ <<< "$air_line"
    printf "%4s %7s %5s %7s %4s %7s %6s %6s %6s %8s %8s\n" "$seed" "$walkers" "$sea" \
        "$flyers" "$up" "$hunters" "$watched" "$plant" "$meat" "$sea_alive" "$air_alive"
    [ "$walkers" -gt 0 ] && land_ok=$((land_ok + 1))
    [ "$sea" -gt 0 ] && sea_ok=$((sea_ok + 1))
    [ "$flyers" -gt 0 ] && air_ok=$((air_ok + 1))
    [ "$watched" -gt 0 ] && chain_ok=$((chain_ok + 1))
    [ "$plant" -ge "$MEALS" ] && [ "$meat" -ge "$MEALS" ] && diet_ok=$((diet_ok + 1))
    [ $((sea_alive + air_alive)) -gt 0 ] && cross_ok=$((cross_ok + 1))
done

verdict() {
    if [ "$2" -ge "$3" ]; then echo "PASS  $1: $2 of $SEEDS (needs $3)"
    else echo "FAIL  $1: $2 of $SEEDS (needs $3)"; fi
}
echo
verdict "1 walkers alive at year 23" "$land_ok" 15
verdict "2 sea alive at year 23" "$sea_ok" 12
verdict "2 flyers alive at year 23" "$air_ok" 12
verdict "3 sea hunters kill sea animals, years 20-23" "$chain_ok" 10
verdict "4 flyers ate plants and meat, years 20-23" "$diet_ok" 8
verdict "5 a line that took to the sea or the air alive at year 23" "$cross_ok" 3
