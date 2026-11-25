# 2026-11-25: A cheaper tick: the same world, about a quarter faster

Commits: `142faf5` (the measuring tool), `702fc98`, `f5d7053`, `1f267b8`, `47a9f63`, `3773279`, `0e7321f`, `adba5e2` (the speed-ups), `fdfdf2f` (the equivalence test), and this doc, on top of `5f8f6d3`.
Run it: `make release && make ticks && build/release/veredas --once --seed 4 --size 256x144 --ancestors 8 --ticks 0 --save world.save && build/release/tick_report world.save 300`
Reading time: about 45 minutes.

All numbers below come from a cloud machine (4 cores, Linux, gcc 13.3, release
build, `-O2`), not from Nicolas's machine. Its absolute times differ from
yours; the before/after ratios are what carry over, and even those depend a
little on the processor (one speed-up, large pages, only exists on Linux).

## 1. What was built

The window stutters because one tick of a grown world takes longer than a
frame. This change makes the tick cheaper **without changing anything the
world does**: every tick of every world gives the same `sim_checksum` and
`sim_story_checksum` as before, bit for bit. Nothing about the rules, the save
format, the pictures or the camera changed.

Six speed-ups, in the order they were built:

1. The brain's weight lookups are inlined (`src/genome.h:138`).
2. `world_idx`, the "which cell is x, y" function, is inlined (`src/world.h:132`).
3. The creature pool lives in 2 MB pages on Linux (`src/bigmem.c:14`).
4. The views' sideways offsets are rounded once per creature, not at every
   step (`src/creature.c:686`).
5. Wrapping a coordinate round the map uses comparisons, not division
   (`src/world.h:112`), in `world_idx`, in hearing and in `kin_hit_near`.
6. Those offsets are rounded without a call into the maths library
   (`src/creature.c:710`).

And two tools: `tick_report` (`make ticks`), which times ticks of a saved
world and folds every tick's checksums into one number, and a test,
`test_same_world` (plus `slow_same_world` in `make test-slow`), that grows a
world and compares 300 ticks of checksums with the number the old code made.

The numbers (medians of alternating runs, the before program built from
`5f8f6d3` in a separate `git worktree`):

| measure | before | after | change |
|---|---|---|---|
| one tick of the grown seed-4 world, 256x144, 1,027 -> 1,656 animals (`tick_report world.save 300`) | 18.2 ms | 12.9 ms | -29% |
| instructions in `sim_step` per tick (callgrind, 30 ticks of the same world) | 81.9 M | 66.8 M | -18% |
| `speed_report` (seconds per million creature-ticks, seed 1, 10 years) | 9.09 s | 6.59 s | -27% |
| window on that world: frames a second | 30.0 | 30.0 | |
| window: late frames a second (a gap over 40 ms) | 0.1 (8 in 80 s) | 0.04 (3 in 80 s) | |
| window: the gap after a tick frame (median / p90) | 34.1 / 37.4 ms, worst 86 ms | 33.1 / 33.6 ms, worst 53 ms | |

What each step gave: each row is the program before that step against the
program after it, alternated, the median of the runs' medians. The machine
drifts by a few percent over an hour, so the rows do not chain exactly, and
noise is about plus or minus 3%: the small ones are estimates.

| step (commit) | tick, median, before -> after the step | instructions a tick after it |
|---|---|---|
| before (`5f8f6d3`) | | 81.9 M |
| 1. brain weights inline (`702fc98`) | 18.7 -> 15.9 ms (-15%) | 71.5 M |
| 2. `world_idx` inline (`f5d7053`) | 16.1 -> 15.5 ms (-4%) | 70.4 M |
| 3. large pages for the pool (`1f267b8`) | 15.7 -> 14.7 ms (-6%) | 70.4 M (the same: only memory moved) |
| 4. offsets once per creature (`47a9f63`) | 14.7 -> 14.4 ms (-2%, within noise) | 69.0 M |
| 5. wrapping without division (`3773279`) | 14.4 -> 13.0 ms (-10%) | 68.4 M |
| 5b. `kin_hit_near` without division (`0e7321f`) | 13.3 -> 13.0 ms (-2.5%) | 68.4 M |
| 6. rounding without `lroundf` (`adba5e2`) | 13.8 -> 13.5 ms (-2%, within noise) | 66.8 M |

The window numbers need a word: this machine is faster than Nicolas's, so
its tick (18 ms) already fitted in a 33 ms frame before, and the window was
nearly smooth either way (300 frames skipped at the start, 80 s of play, the
dummy video driver). What shows is the tail: the world's slowest ticks in the
window went from 26.5 to 20.5 ms (p99) and from 79 to 46 ms (worst), so fewer
frames were late and the worst gap after a tick frame fell from 86 to 53 ms.
On a machine where a tick is longer than a frame, the same 29% is the
difference between late and on time.

Nicolas measured 38-42 ms a tick on his machine. If his machine gains as this
one did, that becomes about 27-30 ms, under the 33 ms a frame
has. Without large pages (macOS, Windows, or a Linux with transparent huge
pages turned off) the gain is about six points less.

## 2. Map of the change

```
 Makefile                         |  10 +-
 src/bigmem.c                     |  34 ++
 src/bigmem.h                     |  25 ++
 src/creature.c                   |  88 ++++-
 src/genome.c                     |  13 -
 src/genome.h                     |  22 +-
 src/world.c                      |  10 -
 src/world.h                      |  33 +-
 study/2026-11-25-cheaper-tick.md | 694 +++++++++++++++++++++++++++++++++++++++
 tests/same_world.h               |  78 +++++
 tests/slow_same_world.c          |  34 ++
 tests/test_same_world.c          |  34 ++
 tests/test_world.c               |  20 ++
 tests/tick_report.c              |  86 +++++
 14 files changed, 1138 insertions(+), 43 deletions(-)
```

Reading order:

1. `tests/tick_report.c`: the measuring tool. Everything else was decided
   with it.
2. `src/genome.h:126-146`: the brain's weight index, moved from `genome.c`
   into the header as `static inline`.
3. `src/world.h:104-135`: `world_wrap` and the inlined `world_idx`.
4. `src/bigmem.h`, `src/bigmem.c`, and its one caller, `src/creature.c:33`.
5. `src/creature.c:677-734`: the `Fan`, `round_half_away` and `fan_offset`;
   then `sense_view` (`src/creature.c:779`), which uses them at line 846, and
   `sense_all` (`src/creature.c:1141`), which makes the fan at line 1200.
6. `src/creature.c:621` (`kin_hit_near`) and `src/creature.c:1016`
   (`squares_in_reach`): two more wraps without division.
7. `tests/same_world.h`, `tests/test_same_world.c`, `tests/slow_same_world.c`,
   and `tests/test_world.c:26`: how "the same world" is checked.

## 3. Decisions

### 3.1 Measure with a tool that also checks the world

**Decision.** A new tool, `tests/tick_report.c`, loads a save, times each
tick on its own, prints the median, mean and slowest, and folds every tick's
`sim_checksum` and `sim_story_checksum` into one 64-bit number
(`tests/tick_report.c:69`). Two builds that print the same number ran the same
world on every one of those ticks.

**Alternatives.** `speed_report` alone (it has no checksum and starts from a
fresh world, not a grown one); timing inside the window (noisy, and mixes in
drawing).

**Why.** Every speed-up was a candidate for silently changing the world. One
number that changes if any tick differs made checking it as cheap as timing
it, so it was checked after every edit, not once at the end. `--no-checksums`
leaves the checksums out when profiling: they read the whole world and would
otherwise take 40% of the samples (section 6).

### 3.2 Inline across files with `static inline`, not with link-time optimisation

**Decision.** `net_hidden_weight`, `net_out_weight` (`src/genome.h:138`) and
`world_idx` (`src/world.h:132`) became `static inline` functions in their
headers.

**Alternatives.** `-flto` (link-time optimisation, which lets the compiler
inline across files): measured here at about 4% on top of the code at step 3,
within noise, and it changes how every platform builds, the browser one
included. `-O3`: measured, no gain (14.7 against 14.4 ms, noise).

**Why.** Two tiny functions called hundreds of millions of times were the
first big cost, and the fix is local and certain. LTO would do the same for
every function, but for a gain this machine could not separate from noise,
and with more risk to the Windows and browser builds.

### 3.3 Wrap coordinates by comparison, divide only far off the map

**Decision.** `world_wrap(v, n)` (`src/world.h:112`) returns `v` when it is
already on the ring `0 .. n-1`, `v - n` or `v + n` when it is one lap off,
and only otherwise does the old `((v % n) + n) % n`. `world_idx`,
`squares_in_reach` and `kin_hit_near` use it.

**Alternatives.** Keep `%` (the old way); require callers to pass coordinates
within one lap (as `sense_view`'s `wrap_near` does, `src/creature.c:853`),
which would mean checking every caller.

**Why.** This was the single largest gain (10%) and it hid well: the profile
put the time on whatever line used the cell, not on a division. The answer is
the same integer for every input (checked by
`tests/test_world.c:26` for every `v` from `-5n` to `5n`, `n` up to 300, and at
the `int` limits), so callers need no change.

### 3.4 Large pages for the creature pool, on Linux only

**Decision.** `bigmem_alloc` (`src/bigmem.c:14`) asks for a 2 MB-aligned block
and tells Linux it may back it with 2 MB pages (`madvise(MADV_HUGEPAGE)`);
elsewhere it is plain `malloc`. Only the pool uses it (`src/creature.c:33`).

**Alternatives.** Rearrange `Creature` so the fields the senses read sit
together, or split them into their own arrays ("structure of arrays", section
4.4): the real fix for scattered reads, but a large change across every file
that reads a creature. Large pages for the kinship cache too: tried, no gain
(it is 1 MB, which ordinary pages already cover well).

**Why.** 6% for 34 lines, with no change to what is in memory, only where it
is. It does nothing on macOS or Windows, and nothing on a Linux whose
transparent huge pages are set to `never`; the program works the same either
way.

### 3.5 The fan of lines once per creature, not once per pool or per program

**Decision.** `sense_all` makes a `Fan` on the stack (`src/creature.c:1200`)
holding each line's slope and, filled as the views reach them, each step's
offsets. The three views share it.

**Alternatives.** A table made once for the whole program (a `static` array):
not allowed, because the camera's look-ahead steps a second world on another
thread (`src/lookthread.c`), and anything `sim_step` reaches must have no
non-const `static` state. A table kept in the `Pool`, like the kinship cache:
it would save the remaining ~50 roundings per creature, but needs a pointer to
mutable memory inside a `const Pool`, invalidation when `view_lines` changes,
and care in `sim_copy`. Not worth 1-2%.

**Why.** Per creature, the views walked about 117 line-steps, each with a
call to `lroundf`; the fan needs at most `lines x reach` (about 49) roundings,
in a tight loop of its own.

### 3.6 Round without `lroundf`, after checking every float

**Decision.** `round_half_away` (`src/creature.c:710`): cut towards zero,
take the whole part off, add or subtract one by two comparisons.

**Alternatives.** Keep `lroundf`; use `roundf` (still a library call on the
baseline x86-64 the build targets); `(int)(v + 0.5f)` (wrong for negatives and
for 0.49999997, which rounds up when 0.5 is added).

**Why.** It is the same as `lroundf` for every one of the 2,464,696,320
floats whose size is below a million (checked by a throwaway program that
walked all of them, section 6), and the fan's values are below 36. Its own
gain is small (instructions -2%); it mostly finished what 3.5 started.

### 3.7 Check the world against numbers the old code made

**Decision.** `tests/same_world.h` grows a world from a seed, then steps 300
ticks folding both checksums after each one; `test_same_world` (96x54, in
`make test`) and `slow_same_world` (256x144, 1,147 -> 1,486 animals, in
`make test-slow`) compare the result with the number the code at `5f8f6d3`
gave. Both were run on the old code too (same numbers), and the release
builds of gcc and clang agree.

**Alternatives.** Rely on the existing seed-7 fingerprint (one small world,
one checksum at the end: a difference that evens out, or one that only shows
in a crowd, would pass); keep the old code in the tree to run beside the new
(two copies of the rules to maintain).

**Why.** Only a crowd exercises what changed: thousands of views, hearing in
crowds, flyers and swimmers. Folding every tick means a difference on any
tick shows. The pinned numbers depend on the maths library, like the seed-7
one (this machine gives `SEED_7_AFTER_500`, GitHub's gives
`SEED_7_AFTER_500_CI`), so the test compares only where seed 7 gives the
local value, and prints the number elsewhere (`tests/same_world.h:32`).

### 3.8 Speed-ups not built because they would change the world

These would make the tick cheaper, but give different bits, so they are
proposals for Nicolas to decide, each with its measured or estimated gain:

- **The hidden layer's sums in four parts** (`src/brain_net.c:47`). Each
  hidden value is a sum of 65 products added one after another. Adding them
  as four running sums, four products at a time (what SIMD instructions do),
  then adding the four, is perhaps three times faster for that loop, but adds
  in a different order, so the last bits of the sums differ (section 4.1).
  The hidden layer is now about 3.5% of a tick: a gain of about 2%.
- **The network's own `tanh` and `exp`.** `tanhf` (through `expm1f`) and
  `expf` are about 7.5% of a tick, 13 and 7 calls a creature. An own
  implementation (a polynomial, or a table with interpolation) could be two
  to four times cheaper; about 4-5% of a tick. It would also make worlds the
  same on Windows and Linux, which today they are not (the maths libraries
  round differently, `tests/test_sim.c:166`). A rules change: new rules
  version, new fingerprint.
- **Clarity by multiplication** (`src/creature.c:647`): `d / sight` as
  `d * (1 / sight)` with `1 / sight` worked out once per creature. A division
  and a multiplication by the reciprocal differ in the last bit for some
  values. About 1%.

### 3.9 Tried, exact, but not kept

Each of these kept the world the same but showed no gain this machine could
measure, so the simpler code stayed:

- The ten hidden sums built side by side (ten running sums, one input at a
  time; the same order per sum, so exact): slower, 16.2 against 15.5 ms.
- The view's clarities remembered across the three views: no change (14.9
  against 14.6 ms, noise).
- Hearing nearest call first, to skip kinship lookups once nothing farther
  can count: only 12% fewer lookups, no time gained.
- A kinship cache 4 times larger or smaller: no change.
- Skipping a seen creature's colour lookup when it cannot be clearer than one
  seen already: no change.
- Rounding inline at every step of the walk (before the fan): slower, 8.5
  against 8.1 ms for the senses alone.

## 4. Implementation walkthrough

### 4.1 First, why the world is so fragile

The world must come out the same to the last bit, and floating-point
arithmetic is where that is easiest to break.

> **C note: float order changes results.** A `float` has 24 bits of
> precision; every addition rounds its result to the nearest float. So
> `(a + b) + c` and `a + (b + c)` round at different moments and can differ:
> with `a = 1e8f, b = -1e8f, c = 1.0f`, the first is `1.0f` and the second is
> `0.0f` (`b + c` rounds back to `-1e8f`). In TypeScript, `number` is a
> 64-bit double and the same thing happens, just at the 16th digit instead of
> the 7th. A C compiler at `-O2` keeps your order exactly: it may not
> reassociate float sums unless told to with `-ffast-math` (or
> `-fassociative-math`), which is why that flag is forbidden here. It also may
> not fuse `a * b + c` into one fused multiply-add on this build (`-std=c11`
> turns contraction off in gcc, and the baseline x86-64 target has no FMA
> instruction), so inlining a function cannot change its float results either.

That last point is what made the first two speed-ups safe: moving a function
into a header changes where its code is, never what it computes.

### 4.2 Inlining across files

`hidden_layer` (`src/brain_net.c:41`) adds 65 weighted senses into each of 10
hidden values, plus the outputs: about 800 weight lookups a creature a tick.
Each lookup was `g->g[net_hidden_weight(h, i)]`, and `net_hidden_weight` lived
in `genome.c`: `GENE_TRAITS + h * 66 + i`, one multiply and two additions.

> **C note: one file at a time.** Each `.c` file is compiled on its own into
> an object file (`build/release/genome.o`), and the linker joins them
> afterwards. When the compiler builds `brain_net.c` it sees only the
> *declaration* `int net_hidden_weight(int h, int i);` from the header, not
> the body, so it must emit a real `call`: save registers, jump, compute,
> return. That was 367 million calls in 300 ticks. A TypeScript bundler or a
> JIT would inline this for you; a C compiler only can when it sees the body.
> `static inline` in the header gives every file that includes it its own
> copy of the body (`static`: private to that file, so the linker never sees
> two definitions; `inline`: a hint, which at `-O2` it takes for something
> this small). After that, `g->g[net_hidden_weight(h, i)]` compiles to a
> pointer that steps by 4 bytes, and the loop is a few instructions.

Gain: -15%, the largest single step after the division one. `world_idx`
(`src/world.h:132`) was the same story: 58 million calls in 300 ticks from
`creature.c`, `flora.c` and others, now inlined.

### 4.3 The hidden division

The profile after inlining still showed `world_idx` lines and the view's
cell lines as hot. The code was:

```c
int wrapped_x = ((x % w->w) + w->w) % w->w;
```

> **C note: `%` is a division.** On x86-64, `a % b` for a `b` the compiler
> does not know is the `idiv` instruction, which takes tens of cycles where an
> addition takes one, and the processor can start few of them at once. When
> `b` is a constant (`% 8`), the compiler turns it into shifts and multiplies;
> `w->w` is read from memory, so it cannot. `world_idx` did four of them per
> cell, and nearly every cell asked for is already on the map or one step off
> it.

`world_wrap` (`src/world.h:112`) handles those cases with comparisons:

```c
if (v >= 0) {
    if (v < n) return v;
    if (v - n < n) return v - n;   /* not v < 2 * n: that could overflow */
} else if (v >= -n) {
    return v + n;
}
return ((v % n) + n) % n;          /* far off the map: as before */
```

The branches are well predicted (almost always the first), so they are nearly
free. Note `v - n < n` rather than `v < 2 * n`: signed overflow is undefined
behaviour in C (the undefined-behaviour sanitizer would report it, and the
compiler may assume it never happens), so the code never computes a value that
could overflow. In TypeScript, numbers silently become imprecise doubles
instead; in C, they are not allowed to happen at all.

Gain: -10%, plus -2.5% for the same change in `kin_hit_near`
(`src/creature.c:621`), which wrapped 14 coordinates per creature with two `%`
each.

### 4.4 Memory: cache lines, pages and the TLB

A `Creature` is 4,388 bytes (its genome alone is 3,244), and the pool holds
8,000 of them: 35 MB. A creature's senses read a few fields of each
neighbour and each creature it sees: its id, health, colour gene, recent hits.

> **C note: memory is not flat.** The processor reads memory in 64-byte cache
> lines, and keeps recently used lines in caches of a few KB (L1) to a few MB
> (L3); a read that misses them all waits about a hundred nanoseconds. Above
> that sits address translation: memory is mapped in 4 KB pages, and turning
> an address into a physical one needs a page-table lookup, cached in the
> TLB (translation lookaside buffer), which holds about 1,500 pages. Reading
> one field from each of 1,600 creatures 4.4 KB apart touches 1,600 different
> pages: more than the TLB holds, so many reads first walk the page table.
> In TypeScript you never see any of this, and an array of objects is an
> array of pointers anyway; in C, an array of structs is the structs
> themselves, one after another, so their size decides how far apart the
> fields you read are.

The cache-friendly fix would be a layout where the fields the senses read are
close together: a small "hot" struct per creature, or one array per field
(structure of arrays), so 1,600 health values fill 100 cache lines instead of
1,600. That is a large change. The cheap fix is bigger pages: with 2 MB pages
the pool is 18 pages, which the TLB always holds.

```c
size_t whole = (bytes + large - 1) / large * large;   /* whole 2 MB pages */
if (posix_memalign(&block, large, whole) != 0) return NULL;
(void)madvise(block, whole, MADV_HUGEPAGE);
```

`posix_memalign` (`src/bigmem.c:23`) is `malloc` with a promise about where
the block starts (here on a 2 MB boundary), and its memory is freed with plain
`free`, so `pool_free` did not change. `madvise` is advice: if the kernel has
no large page to give, the block stays ordinary memory.

> **C note: feature-test macros.** The Makefile compiles everything with
> `-D_POSIX_C_SOURCE=200809L`, which tells the C library "show me POSIX and
> nothing more". `madvise` and `MADV_HUGEPAGE` are Linux extras, so they were
> hidden (section 6). `#define _DEFAULT_SOURCE` at the very top of
> `src/bigmem.c`, before any `#include`, asks glibc for its extras in that
> file alone. The `#if defined(__linux__) && !defined(__EMSCRIPTEN__)` around
> them keeps the Windows and browser builds on `malloc`.

How to see it worked: while `tick_report` ran, `/proc/<pid>/smaps_rollup`
said `AnonHugePages: 34816 kB`. Gain: -6%.

### 4.5 The fan, and calls that cost more than they look

Each view walks `view_lines` (7) lines fanned from -0.75 to 0.75 cells
sideways per step; line `j` at step `d` is `round(slope_j * d)` cells to the
side. The old code rounded with `lroundf` at every step of every line of
every view.

> **C note: a library call clobbers registers.** By the x86-64 calling
> convention, every `xmm` register (where floats live) may be overwritten by a
> called function. So around a call to `lroundf`, the compiler must store
> every float it is holding (the view's running sums of plants, fish, the
> best clarity) to the stack and load them back: the disassembly of the walk
> showed `movss %xmm6,0x50(%rsp)` and friends around each `call lroundf`.
> `lroundf` itself cannot be inlined: it lives in `libm.so`, and must also
> handle errors (a value too large for a `long`).

The `Fan` (`src/creature.c:687`) keeps each slope and each offset row once
computed; `fan_offset` (`src/creature.c:718`) fills rows as the views reach
them. It lives on `sense_all`'s stack, so each creature gets a fresh one and
no state survives between creatures or threads. Then `round_half_away`
(`src/creature.c:710`) replaced the remaining `lroundf`:

```c
int t = (int)v;                          /* cut towards zero */
float rest = v - (float)t;               /* exactly the fraction */
return t + (rest >= 0.5f) - (rest <= -0.5f);
```

> **C note: why `v - (float)t` is exact.** `t` is `v` with its fraction cut
> off, so `v - t` keeps only bits `v` already had: no rounding happens.
> Halves round away from zero: `2.5` gives `t = 2`, `rest = 0.5`, so 3;
> `-2.5` gives `t = -2`, `rest = -0.5`, so -3; this is what `lroundf` does.
> A comparison in C is an `int`, 1 or 0, so adding comparisons needs no
> branch (TypeScript would give you a boolean, and `true + true` is 2 only by
> coercion).

The proof is not this argument but the check in section 6: every float below
a million, against `lroundf`.

### 4.6 What `-O2` does and does not do

Several dead ends make sense once you know what the optimiser does:

- **It does** inline small functions it can see (same file, or a header),
  keep values in registers, hoist loop-invariant loads when it can prove
  nothing in the loop writes them, and, since gcc 12, vectorise simple loops
  that need no leftover step: `genome_kinship` (`src/genome.c:229`) compares
  four genes at once with SSE instructions (`subps`, `andps`, `cmpltps`).
- **It does not** inline across files, reassociate float arithmetic, turn
  `x / y` into `x * (1 / y)` for floats (different bits), use instructions the
  target may lack (no AVX or FMA on the baseline x86-64), or rearrange your
  structs.
- **So** "ten hidden sums side by side" (section 3.9) gave it nothing new: it
  could not vectorise across sums whose weights are 66 floats apart without
  gather instructions, and the original loop was already limited by memory,
  not by the chain of additions.

### 4.7 Reading a profile

> **C note: how to read a profile.** `perf record` interrupts the program
> about 4,000 times a second and notes where it was; `perf report` counts
> where. "Self" (or `--no-children`) is time in that function's own code;
> "children" adds what it called. Three traps: (1) at `-O2`, inlined
> functions vanish into their callers, so `sense_view` shows up as
> `creature_sense` or `sense_all`; build with `-g` (it does not change the
> code, checked with `objdump`) and sort by source line
> (`--sort srcline`) to see inside. (2) A sample lands on the instruction
> *after* the slow one ("skid"), and a stalled load shows up on whatever
> uses its value: the 11% on `if (plant_offers_fruit(plant))` is the wait for
> `w->plant[i]` to arrive, not the comparison. (3) A division or a cache miss
> is spread over many lines; no single line says "division". Counting
> instructions with `valgrind --tool=callgrind` is a second view: exact and
> repeatable, but blind to waiting, so it under-rates memory and division.

The top of the profile before (`perf record -F 4000`, 300 ticks, checksums
off; `creature_sense` is the senses with `sense_view` inlined):

```
    30.17%  tick_report           [.] creature_sense
    12.01%  tick_report           [.] hidden_layer
     5.15%  tick_report           [.] world_idx
     4.81%  tick_report           [.] genome_kinship
     4.66%  libm.so.6             [.] __expm1f
     4.26%  tick_report           [.] pool_kinship
     3.98%  tick_report           [.] sim_step
     3.03%  tick_report           [.] flora_step
     2.38%  tick_report           [.] net_hidden_weight
     2.12%  libm.so.6             [.] __lroundf
     2.12%  tick_report           [.] chances_from_hidden
     1.89%  tick_report           [.] climate_region
     1.74%  tick_report           [.] hear_one
     1.70%  tick_report           [.] sight_reach
     1.68%  tick_report           [.] creature_bite_choice
```

and after:

```
    39.97%  tick_report           [.] sense_all
     5.80%  libm.so.6             [.] __expm1f
     4.82%  tick_report           [.] pool_kinship
     4.81%  tick_report           [.] genome_kinship
     4.46%  tick_report           [.] flora_step
     3.90%  tick_report           [.] hidden_layer
     3.41%  tick_report           [.] sim_step
     2.45%  tick_report           [.] creature_bite_choice
     2.26%  tick_report           [.] climate_region
     2.23%  tick_report           [.] chances_from_hidden
     2.19%  tick_report           [.] hear_one
     2.01%  tick_report           [.] net_learn
     1.81%  tick_report           [.] plant_mass
     1.69%  tick_report           [.] sight_reach
     1.66%  libm.so.6             [.] __tanhf
```

The senses are now a larger share of a smaller tick. What is left there is
mostly waiting for memory: the view's loads of `terrain`, `occupant`, `sky`,
`veg` and `plant` for each cell, and other creatures' fields.

## 5. C you need to know

- **Translation units.** Each `.c` file is compiled alone; the compiler sees
  other files only through the declarations in headers. A body in a header
  must be `static inline` (or the linker sees one copy per file).
- **Undefined behaviour.** Signed overflow, reading outside an array, and a
  few more are not errors at run time but "anything may happen"; the
  compiler optimises assuming they never occur. Write `v - n < n`, not
  `v < 2 * n`, when `2 * n` might not fit.
- **Floats are not real numbers.** Every operation rounds; order matters;
  `x / y` and `x * (1 / y)` differ. Keep the order and the operations, and the
  bits stay the same.
- **Integer division is slow; `%` is a division.** By a constant it is
  cheap (the compiler rewrites it); by a variable it is `idiv`.
- **Memory has a shape.** Cache lines (64 bytes), pages (4 KB, or 2 MB large
  pages), and the TLB that caches page lookups. An array of large structs
  spreads the fields you read across many lines and pages.
- **Calls cost more than the call.** Besides the jump, a call into another
  file or library makes the compiler save and reload registers around it.
- **Feature-test macros.** `_POSIX_C_SOURCE`, `_DEFAULT_SOURCE` and friends
  decide which functions the C library's headers declare; define them before
  the first `#include`.
- **`posix_memalign` and `madvise`.** Aligned allocation, freed with `free`;
  advice to the kernel about a range of memory.

## 6. Problems hit

**The profile was mostly the checksums.** The first `perf` run of
`tick_report` said:

```
    39.82%  tick_report  tick_report           [.] sim_checksum
    17.66%  tick_report  tick_report           [.] creature_sense
```

The checksums read the whole world every tick, and `tick_report` computes
two of them per tick. Fix: `--no-checksums` (`tests/tick_report.c:45`) for
profiling; the timed part never included them.

**gprof misled.** `gcc -pg` with `-O2` attributed everything to whichever
function the code had been inlined into; with `-fno-inline` the program took
57 ms a tick instead of 18 and the shares shifted. `perf` (installed from
`linux-tools`) sampled the real release build. It had no hardware counters
in this virtual machine (`perf stat -e instructions` said
`<not supported> instructions`), so time samples (`cpu-clock`) it was, with
callgrind for instruction counts.

**Noise.** Running the same program five times gave medians from 14.25 to
15.30 ms. Fix: alternate the before and after programs, pin each run to one
core (`taskset -c 2`), take the median of seven medians, and confirm small
changes with callgrind's instruction counts, which do not vary. Changes
smaller than about 3% are reported as "within noise".

**Ten sums side by side were slower.**

```
300 ticks, animals 1027 -> 1656: median 16.22 ms, mean 16.52 ms, slowest 31.38 ms; checksums 3fae499e7180bb6f
```

against 15.5 ms for the plain loop. Cause: the weights of the ten sums are
66 floats apart, so each step of the inner loop read ten scattered values
instead of one stream; the chain of additions was never the bottleneck.
Fix: reverted (section 3.9).

**`MADV_HUGEPAGE` was not declared.**

```
src/creature.c:37:9: warning: implicit declaration of function 'madvise' [-Wimplicit-function-declaration]
src/creature.c:37:27: error: 'MADV_HUGEPAGE' undeclared (first use in this function)
```

Cause: `-D_POSIX_C_SOURCE=200809L` hides non-POSIX declarations. Fix: a file
of its own, `src/bigmem.c`, starting with `#define _DEFAULT_SOURCE`.

**Rounding inline in the walk was slower.** Replacing `lroundf` by a branchy
rounding at every step made the senses alone 8.5 ms instead of 8.1: the
branches on the fraction were unpredictable there. Fix: the fan first (fewer
roundings, in a loop of their own), then a branch-free rounding.

**`lroundf` was still in the profile after the fan.** callgrind counted
2,357,133 calls in 30 ticks: the fan still rounds each row once per creature.
Fix: `round_half_away`, after checking it against `lroundf` for every float:

```
2464696320 floats with |v| < 1e6 checked, 0 differ
```

**Pinned values differ between machines.** `tests/test_sim.c` pins two
seed-7 fingerprints, this machine's `0x8898b6ba0272fbb8` and GitHub's
`0x40f36572f36ef983`: the maths library takes different paths on different
processors. A test pinning a new value from this machine would fail on
GitHub. Fix: `same_world_maths_as_pinned` (`tests/same_world.h:32`) compares
only where seed 7 gives this machine's value, and prints the number
elsewhere.

**clang's sanitizers were not installed here.**

```
/usr/bin/ld: cannot find /usr/lib/llvm-18/lib/clang/18/lib/linux/libclang_rt.asan_static-x86_64.a: No such file or directory
```

So the clang check was done on release builds only: `tick_report` gave the
same checksums (`3fae499e7180bb6f`) and the 96x54 world the same number as
gcc. GitHub's CI runs the clang test build.

## 7. Questions to check yourself

1. Why could moving `net_hidden_weight` into a header make the program
   faster but never change a single bit of the world?

   > Answer: The function only computes an integer index; inlining changes
   > where its instructions sit, not what they compute. And no float
   > operation was reordered or fused: gcc at `-std=c11` does not contract
   > `a * b + c`, the target has no FMA, and `-O2` never reassociates float
   > sums.

2. `world_wrap(v, n)` writes `v - n < n` rather than `v < 2 * n`. What could
   go wrong with the second?

   > Answer: `2 * n` overflows an `int` when `n` is above `INT_MAX / 2`, and
   > signed overflow is undefined behaviour: the compiler may assume it never
   > happens and the sanitizer reports it. `v - n` cannot overflow there,
   > since `v >= n >= 0`.

3. The profile put 11% of the senses' time on `if (plant_offers_fruit(plant))`,
   a single comparison. What was really taking the time?

   > Answer: Loading `w->plant[i]` from memory: the comparison is the first
   > instruction that needs the loaded value, so the wait (and the sample's
   > skid) lands there.

4. Large pages made the tick 6% faster without changing a single
   instruction (callgrind counted the same 70.4 M). Where did the time go
   before?

   > Answer: Into address translation: reading a few fields from each of
   > ~1,600 creatures spread over 35 MB touched more 4 KB pages than the TLB
   > holds, so many reads waited for a page-table walk first. In 2 MB pages
   > the pool is 18 pages.

5. Why is summing the hidden layer in four running sums a rules change, when
   it adds exactly the same products?

   > Answer: Float addition rounds after every step, so the order of the
   > additions decides the last bits of the result; four sums added at the
   > end round at different moments than one sum built left to right.

6. Why does the `Fan` live on `sense_all`'s stack rather than in a `static`
   table made once?

   > Answer: The camera's look-ahead steps a second world on another thread,
   > and two threads filling one `static` table would race; the project's rule
   > is no non-const `static` state reachable from `sim_step`. A stack value
   > belongs to one call on one thread.

## 8. Worth knowing

- `man 3 posix_memalign`, `man 2 madvise` (`MADV_HUGEPAGE`), and the kernel's
  "Transparent Hugepage Support" page
  (`Documentation/admin-guide/mm/transhuge.rst`), which explains
  `/sys/kernel/mm/transparent_hugepage/enabled`.
- `man 7 feature_test_macros`: `_POSIX_C_SOURCE`, `_DEFAULT_SOURCE`.
- `man 1 perf-record`, `man 1 perf-report`, `man 1 perf-annotate`; the
  valgrind manual's callgrind chapter.
- Bryant and O'Hallaron, *Computer Systems: A Programmer's Perspective*, 3rd
  ed.: chapter 5 (optimising program performance: inlining, loop-carried
  dependences), chapter 6 (the memory hierarchy, cache-friendly code),
  chapter 9 (virtual memory, pages and the TLB), section 2.4 (floating point).
- David Goldberg, "What Every Computer Scientist Should Know About
  Floating-Point Arithmetic" (1991): why order changes results.
- Ulrich Drepper, "What Every Programmer Should Know About Memory" (2007):
  caches, TLBs and large pages.
- Agner Fog's instruction tables: how many cycles `idiv` takes on each
  processor.
- The GCC manual, "Options That Control Optimization": what `-O2` turns on,
  and `-ffp-contract`.

## 9. Glossary

- **Tick**: one step of the world: every creature senses, decides and acts,
  then plants, weather and the rest update.
- **Checksum / fingerprint**: a 64-bit number made from the whole world
  (`sim_checksum`) or its stories (`sim_story_checksum`); equal numbers mean,
  in practice, equal worlds.
- **Inline / inlining**: the compiler pasting a function's body where it is
  called, removing the call.
- **`static inline`**: a function defined in a header, private to each file
  that includes it, that the compiler may inline.
- **Translation unit**: one `.c` file with everything it includes, compiled
  on its own.
- **Link-time optimisation (LTO)**: optimising across files when they are
  joined, with `-flto`.
- **`idiv`**: the x86-64 instruction for signed integer division (and `%`).
- **Cache line**: the 64 bytes the processor moves between memory and its
  caches at once.
- **Page / large page**: the unit memory is mapped in: 4 KB normally, 2 MB
  for a large ("huge") page.
- **TLB (translation lookaside buffer)**: the processor's cache of recent
  page lookups.
- **Transparent huge pages (THP)**: Linux backing ordinary memory with 2 MB
  pages, always or only where `madvise` asks.
- **Structure of arrays**: one array per field instead of one array of
  structs, so the same field of many records sits together.
- **SIMD / SSE**: instructions that do the same operation on several values
  at once (four floats for SSE).
- **FMA (fused multiply-add)**: `a * b + c` with one rounding instead of two;
  different bits from doing it in two steps.
- **Skid**: a profiler sample landing a little after the instruction that
  was really slow.
- **Self time / children**: time in a function's own code / plus what it
  called.
- **Fan**: the seven lines a view is walked along, here also the struct that
  keeps their slopes and offsets.
- **Look-ahead copy**: the second world the Documentary camera steps ahead on
  its own thread to choose what to film.
