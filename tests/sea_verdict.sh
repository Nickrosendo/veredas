#!/bin/bash
# The sea verdict: the sea half of the water and air design, section 6. Runs
# seeds 1 to 15 side by side to year 23 and applies the four bars agreed for
# the sea half before running:
#   1. land unharmed: land animals alive at year 23 in 15 of 15 worlds;
#   2. the sea lasts: sea animals alive at year 23 in at least 12 of 15;
#   3. a sea food chain: sea hunters kill sea animals in years 20 to 23 in at
#      least 10 of 15;
#   5. evolution crosses over: in at least 3 of 15 worlds an animal of a line
#      that took to the sea by itself (born to land parents, or descended from
#      one who was) is alive at year 23. Births of such lines over the whole
#      run are printed beside it.
#
# Usage: bash tests/sea_verdict.sh [CONFIG]
#   CONFIG  a settings file passed to every world, for trying a change.
set -eu

CONFIG=${1:-}
SEEDS=15
YEAR_TICKS=600
TICKS=$((23 * YEAR_TICKS))
FROM=$((20 * YEAR_TICKS))
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

echo "sea verdict: $SEEDS seeds to year 23, food chain watched from year 20${CONFIG:+, config $CONFIG}"
echo
printf "%4s %6s %5s %8s %7s %6s %11s %10s\n" seed land sea hunters watched kills line_births line_alive
land_ok=0 sea_ok=0 chain_ok=0 cross_ok=0
for seed in $(seq 1 "$SEEDS"); do
    # The SEA line's values, by name: "SEA land 12 sea 3 ..." pairs.
    line=$(grep '^SEA ' "$TMP/$seed.txt" || echo "SEA land 0 sea 0 sea_hunters 0 kills_watched 0 kills_all 0 line_births 0 line_alive 0")
    read -r _ _ land _ sea _ hunters _ watched _ kills _ births _ alive <<< "$line"
    printf "%4s %6s %5s %8s %7s %6s %11s %10s\n" "$seed" "$land" "$sea" "$hunters" \
        "$watched" "$kills" "$births" "$alive"
    [ "$land" -gt 0 ] && land_ok=$((land_ok + 1))
    [ "$sea" -gt 0 ] && sea_ok=$((sea_ok + 1))
    [ "$watched" -gt 0 ] && chain_ok=$((chain_ok + 1))
    [ "$alive" -gt 0 ] && cross_ok=$((cross_ok + 1))
done

verdict() {
    if [ "$2" -ge "$3" ]; then echo "PASS  $1: $2 of $SEEDS (needs $3)"
    else echo "FAIL  $1: $2 of $SEEDS (needs $3)"; fi
}
echo
verdict "1 land alive at year 23" "$land_ok" 15
verdict "2 sea alive at year 23" "$sea_ok" 12
verdict "3 sea hunters kill sea animals, years 20-23" "$chain_ok" 10
verdict "5 a line that took to the sea alive at year 23" "$cross_ok" 3
