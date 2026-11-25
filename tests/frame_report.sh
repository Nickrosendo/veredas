#!/bin/sh
# Reads a --frame-log file and says how smooth the window was: frames a
# second, the frame gaps that matter (the slow ones, not the average), and
# what the slow frames were doing.
# Usage: sh tests/frame_report.sh LOG [SKIP]
#   LOG   written by veredas-window --frame-log LOG
#   SKIP  frames to leave out at the start (default 0), to look at steady
#         play apart from the opening
# A frame is "slow" when it starts more than 40 ms after the one before: at
# 30 frames a second each has 33 ms, so a slow one is a picture shown late.
# Each slow gap is put down to what the frame before it spent its time on:
#   tick    the shown world ran a tick (or the prelude's ticks)
#   camera  the camera's look-ahead copy ran on, or was made afresh
#   draw    drawing alone took more than 25 ms
#   other   none of these (another program, the system, a wait)
# With the ancestors' years on their own thread (preludethread.h), a prelude
# frame's ticks are how far the world moved while it waited, not ticks this
# frame ran, so "tick" there only says the world moved.
# The last line is the prelude (the ancestors' years): how many ticks it ran
# and how long it took, from the first frame to the first one after it, or
# "still on" when the log ends first.
LOG=$1
SKIP=${2:-0}
[ -f "$LOG" ] || { echo "usage: frame_report.sh LOG [SKIP]" >&2; exit 2; }

# sort -n puts each column in order for the percentiles; awk reads the log
# once for the counts and once per column through sort.
percentiles() {
    # $1: the column's number in the log
    awk -v skip="$SKIP" -v col="$1" 'NR > 1 && $1 >= skip { print $col }' "$LOG" |
        sort -n |
        awk '{ v[NR] = $1 } END {
            if (NR == 0) { print "no frames"; exit }
            printf "median %6.1f  p90 %6.1f  p99 %6.1f  max %6.1f\n",
                v[int(NR * 0.5) + 1], v[int(NR * 0.9) + 1],
                v[int(NR * 0.99) + 1], v[NR] }'
}

# A frame's gap ends when it starts, so a long gap is the work of the frame
# before it: each line is judged with the one before (ticks, camera, draw kept
# from the last line read).
awk -v skip="$SKIP" 'NR > 1 && $1 >= skip && had {
    frames++; seconds += $2 / 1000
    if ($2 > 40) {
        slow++
        why = ""
        if (ticks > 0) why = why "tick+"
        if (camera > 0) why = why "camera+"
        if (draw > 25) why = why "draw+"
        why = why == "" ? "other" : substr(why, 1, length(why) - 1)
        count[why]++; total[why] += $2
    }
}
NR > 1 { had = 1; ticks = $9; camera = $10 + $11; draw = $7 }
END {
    if (frames == 0) { print "no frames"; exit }
    printf "%d frames in %.0f s: %.1f a second\n", frames, seconds, frames / seconds
    printf "slow frames (gap over 40 ms): %d, %.1f a second\n", slow, slow / seconds
    for (w in count)
        printf "  %-18s %5d frames, average gap %.0f ms\n", w, count[w], total[w] / count[w]
}' "$LOG"
echo "gap (ms)      $(percentiles 2)"
echo "work (ms)     $(percentiles 3)"
echo "world (ms)    $(percentiles 4)"
echo "camera (ms)   $(percentiles 5)"
echo "draw (ms)     $(percentiles 7)"
echo "present (ms)  $(percentiles 8)"
# The prelude: frames with the prelude flag (column 13), and the first frame
# after them, whose gap and ticks are the prelude's last.
awk -v skip="$SKIP" 'NR > 1 && $1 >= skip {
    if ($13 == 1) { frames++; ms += $2; ticks += $9 }
    else if (frames > 0 && !ended) { ended = 1; ms += $2; ticks += $9 }
}
END {
    if (frames == 0) { print "prelude: none"; exit }
    printf "prelude: %d ticks in %.1f s, %d frames%s\n", ticks, ms / 1000, frames,
        ended ? "" : " (still on when the log ends)"
}' "$LOG"
