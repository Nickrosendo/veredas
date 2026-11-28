# 2026-11-28: The creatures' views cost less, because the processor guesses fewer of their branches wrong

Commits: `729f3e2` (plant_is_bare inline), `fb08ac9` (the walk run by run), `1f0e3c4` (one fan per pool, and the test), `3be38b9` (comment), plus this doc's commit.
Run it: `make release && make ticks && build/release/veredas --once --seed 4 --size 256x144 --ancestors 8 --ticks 0 --save world.save && build/release/tick_report world.save 300`
Reading time: about 25 minutes.

## 1. What was built

Every creature that decides what to do first looks along three *views*
(ahead, left, right; `sense_view`, `src/creature.c:878`). Each view is a
wedge of ground walked outward one step at a time along a *fan* of 7 fine
lines; at each step every line reaches one cell, and the view adds up the
plants, fish, flyers and the clearest creature it sees there. A line stops at
rock or at a creature. After the earlier cheaper-tick work (study
`2026-11-25-cheaper-tick.md`) these views were still the largest part of a
tick: `sense_all`, which holds them, was 40% of it.

This change makes them cheaper **without changing anything the world does**:
every tick gives the same `sim_checksum` and `sim_story_checksum`, bit for
bit. Three changes:

1. `plant_is_bare` became `static inline` in `src/flora.h:27`, so the view's
   plant test no longer calls into another file.
2. Each step of the walk visits each *cell* once, run by run, instead of
   each *line* once with a test to skip lines whose cell was counted
   already (`src/creature.c:938`).
3. The fan of lines is worked out once per world, not once per creature
   (`pool_cache_views`, `src/creature.c:785`).

The numbers, on a cloud machine (4 cores, an 8 MB level-2 cache: not
Nicolas's computer, so absolute times differ). The world is the grown seed-4
world of the earlier study, 256x144, 1,027 -> 1,656 animals over 300 ticks:

| measure | before (`b70af4e`) | after | change |
|---|---|---|---|
| one tick, median (`tick_report world.save 300`, before and after alternated 9 times, median of the medians) | 11.61 ms | 10.34 ms | -11% |
| instructions, 30 ticks (cachegrind) | 2,361 M | 2,202 M | -6.7% |
| mispredicted branches, 30 ticks (cachegrind's model) | 18.10 M | 14.64 M | -19% |
| `sense_all`'s share of a tick (perf) | 40.3% | 35.5% | about 4.5 -> 3.7 ms |
| checksums over the 300 ticks | `3fae499e7180bb6f` | `3fae499e7180bb6f` | the same |

Step by step (cachegrind, 30 ticks):

| step | instructions | mispredicted branches |
|---|---|---|
| before | 2,361 M | 18.10 M |
| 1. `plant_is_bare` inline | 2,341 M | 16.84 M |
| 2a. the walk run by run | 2,289 M | 16.85 M (the gain moved into building the runs, see 6.2) |
| 2b. runs built without an `if` | 2,293 M | 15.92 M |
| 3. one fan per pool | 2,209 M | 15.20 M |
| 2c. only runs with an open line visited | 2,202 M | 14.64 M |

(2c was built after 3, and is in the same commit as 2a and 2b.)

## 2. Map of the change

```
 src/creature.c     | 189 +++++++++++++++++++++++++++++++++++++++--------------
 src/creature.h     |   9 +++
 src/flora.c        |   6 --
 src/flora.h        |  10 ++-
 src/sim.c          |   2 +-
 tests/test_views.c |  73 +++++++++++++++++++++
 6 files changed, 231 insertions(+), 58 deletions(-)
(against main at b70af4e; this doc comes on top)
```

Read in this order:

1. `src/creature.c:690-830`: `FanStep` and `Fan`, `fan_fill`, `fan_step`,
   then `struct FanCache`, `pool_cache_views`, `the_fan`, `lowest_bit`.
2. `src/creature.c:878`, `sense_view`, the loop from line 936 to 995.
3. `src/creature.c:1290`, where `sense_all` takes the fan.
4. `src/creature.h:346` and `src/sim.c:219` (the pool's new field and where
   it is made); `src/flora.h:27`.
5. `tests/test_views.c`.

## 3. Decisions

### 3.1 Count branches, not only time

- **Decision:** every step was judged by valgrind's `cachegrind` tool
  (`--branch-sim=yes`) over 30 ticks, as well as by `tick_report`'s clock.
- **Alternatives:** the clock alone; `perf stat`'s hardware counters.
- **Why:** on this machine one program's median moved from 10.8 to 12.6 ms
  between rounds of the same test, so a 2% step is invisible to the clock.
  `cachegrind` runs the program in a simulated processor and counts every
  instruction exactly; its branch predictor is a model, not the real one,
  but the same model for both builds. The virtual machine exposes no hardware
  counters at all (`perf stat` printed `<not supported>` for every one).
  Step 1 shows why both views are needed: the clock said nothing (10.83
  against 10.84 ms), the counts said 7% fewer mispredictions.

### 3.2 Walk cells, not lines

- **Decision:** a fan step holds its *runs* (`FanStep`,
  `src/creature.c:701`): the distinct offsets in order and, for each, which
  lines reach it, one bit per line. The walk visits each run once
  (`src/creature.c:938`); rock or a creature closes all of its lines at once
  (`open &= ~here`, lines 949 and 961).
- **Alternatives:** keep walking lines and make the repeat test cheaper;
  skip the repeat test and count a cell once per line (changes the world:
  plants near the looker would count several times).
- **Why:** the repeat test was the single worst-guessed branch of the whole
  tick, 1.17 M of 16.8 M mispredictions in 30 ticks. With runs there is no
  repeat to test. The world stays the same because the runs are visited in
  the same order as the lines were (a run's offset is its lines' offset, and
  offsets never fall as the line number rises), so every float sum adds the
  same numbers in the same order (section 4.1 of the earlier study: float
  addition gives different last bits in a different order).

### 3.3 One fan per world, in the pool

- **Decision:** `Pool` holds a `struct FanCache *fan_cache`
  (`src/creature.h:346`), made by `pool_cache_views` in `sim_alloc`
  (`src/sim.c:219`) beside the kinship cache, filled to all 48 steps the
  first time a creature looks, and made again only when `view_lines`
  differs (`the_fan`, `src/creature.c:803`).
- **Alternatives:** a `static` table for the whole program; keep one per
  creature (the earlier study's choice, 3.5 there).
- **Why:** a `static` is shared by every thread, and the camera's look-ahead
  copy steps a second world on its own thread (`src/lookthread.c`), so
  nothing `sim_step` reaches may write a non-const `static`. Each world has
  its own pool, so each has its own fan: `sim_copy` goes through the save
  format into a freshly allocated Sim, never copying the pointer. The earlier
  study judged a pool cache "not worth 1-2%" when the fan held only offsets;
  with runs, building it cost about 1,900 instructions a creature a tick
  (3.2% of all instructions), and the cache took it to nothing.

### 3.4 Visit only runs with an open line

- **Decision:** the walk takes the lowest open line not yet looked at
  (`lowest_bit`, `src/creature.c:820`, one instruction with gcc and clang),
  visits its run, and crosses the whole run off (lines 938-943).
- **Alternative:** loop over every run and skip those whose lines have all
  stopped.
- **Why:** that skip was the next worst-guessed branch, 0.8 M in 30 ticks:
  whether a run is still open changes from creature to creature.

### 3.5 Tried, exact, but not kept

- **Prefetching the next steps' cells** (`__builtin_prefetch`, two steps
  ahead): 17% *slower*, 12.17 against 10.42 ms. The whole world (about
  1.6 MB of arrays) fits in this machine's 8 MB level-2 cache, so the loads
  were not waiting for main memory, and working out the addresses twice cost
  more than it saved.
- **The plant test without `if`s** (`plant_has_food` putting its answer
  together with `&` and `|`): 0.24 M fewer mispredictions, but 42 M more
  instructions and 2.2 M more cache misses, because the plant record was then
  read for every cell, empty or not. 13.43 against 12.69 ms: put back.

## 4. Implementation walkthrough

### 4.1 Why a wrong guess costs so much

A modern processor does not wait to know which way an `if` goes: it guesses
(*branch prediction*), runs ahead along the guessed way, and throws that work
away if the guess was wrong, about 15-20 cycles lost each time. It guesses
well when a branch goes the same way most times, or in a pattern; it guesses
badly when the way depends on data that looks random, such as "is there a
plant on this cell" or "did the line before reach the same cell". 240,000
wrong guesses a tick at ~17 cycles is about 4 million cycles, near 2 ms of
an 11 ms tick on this 2.3 GHz machine.

> **C note: an `if` is not always a branch, and a comparison is a number.**
> In TypeScript `a > b` is a `boolean` and `true + 1` is a type error (or 2,
> with a warning). In C a comparison is an `int`, exactly 0 or 1, and can be
> added: `runs += fresh;` (`src/creature.c:755`) counts a new run with no
> `if` at all. The compiler may also turn a short `if` into a *conditional
> move* (`cmov`), which has no guess to get wrong; but it decides that, not
> you. Writing the arithmetic yourself is how you make sure, and the cost is
> that both sides get worked out every time (section 3.5's plant test is
> where that cost was larger than the guesses saved).

### 4.2 The fan as runs

`fan_fill` (`src/creature.c:739`) works out step `d`: line `j`'s offset is
`round(slope_j * d)` as before, and lines with the same offset as the one
before join its run:

```c
int fresh = j == 0 || offset != last; /* 1: a new run starts here */
runs += fresh;
int r = runs - 1;
out->lines[r] = (uint16_t)(out->lines[r] | (1u << j));
out->offset[r] = (int16_t)offset;
out->run_of[j] = (uint8_t)r;
```

For 7 lines at step 2 the offsets are -2 -1 -1 0 1 1 2 (slopes from -0.75 to
0.75, times 2, rounded half away from zero), so the runs are
`{0}`, `{1,2}`, `{3}`, `{4,5}`, `{6}`: five cells, not seven.

> **C note: bit sets in an `unsigned`.** A set of up to 15 lines is one
> `unsigned`, bit `j` for line `j`: `1u << j` is the set holding only `j`,
> `a | b` is the union, `a & b` the lines in both, `a & ~b` the lines of `a`
> not in `b`. `(1u << lines) - 1u` (`src/creature.c:902`) is "all lines":
> for 7, binary 1111111. The `u` matters: `1 << 31` on a signed `int` is
> undefined behaviour, `1u << 31` is fine. In TypeScript you would reach for a
> `Set<number>`; here the whole set fits in a register, and "is any line of
> this run open" is one `&` (`open & step->lines[r]`).

### 4.3 The walk

```c
unsigned left = open;
while (left != 0) {
    int r = step->run_of[lowest_bit(left)];
    unsigned here = open & step->lines[r];
    left &= ~step->lines[r];
    ...visit the run's cell...
}
```

`left` starts as the open lines. The lowest of them names a run; that run is
visited and all its lines leave `left`; the loop ends when none is left. A
closed line is never in `left`, so a run whose lines have all stopped is never
looked at. Rock and creatures close the run's open lines, `open &= ~here`;
when `open` is 0 the view ends (`src/creature.c:992`).

> **C note: `__builtin_ctz` and portable fallbacks.** "Count trailing
> zeros" gives the position of the lowest set bit, one instruction on x86-64
> and ARM. It is a gcc and clang extension, not standard C (C23 adds
> `stdc_trailing_zeros`), so `lowest_bit` (`src/creature.c:820`) uses it only
> under `#if defined(__GNUC__)` (gcc, clang, MinGW and Emscripten all define
> it) and falls back to a plain loop elsewhere. `__builtin_ctz(0)` is
> undefined, which is why the loop tests `left != 0` first.

### 4.4 The pool's fan, through a `const` pointer

`sense_all` gets `const Pool *p`, yet `the_fan` (`src/creature.c:803`) may
fill the pool's fan. It can because `const` applies to the pool's own fields:
`p->fan_cache` is a pointer the function may not change, but the memory it
points at is not `const`. The kinship cache works the same way
(`pool_kinship`). It is honest only because the cache never changes an
answer: whoever reads the pool sees the same world either way.

> **C note: `const` is shallow.** In TypeScript, `Readonly<Pool>` makes the
> fields read-only, but an object a field refers to can still be changed,
> unless you write `DeepReadonly`. C's `const` is the same: a
> `const Pool *` makes `p->fan_cache = x` an error, but
> `p->fan_cache->fan.lines = 7` is allowed, because `fan_cache` is a
> `struct FanCache *`, not a `const struct FanCache *`. Here that is used on
> purpose; elsewhere it is a way for "read-only" code to change things.

> **C note: an incomplete struct type.** `creature.h` says only
> `struct FanCache *fan_cache;`, without saying what is inside: every file
> can hold and pass the pointer, but only `creature.c`, which defines
> `struct FanCache` (`src/creature.c:781`), can look inside or ask
> `sizeof`. It is C's way to keep a type private, like an unexported class
> behind an exported interface.

### 4.5 The test

`tests/test_views.c` grows a 96x54 world for 400 ticks, then for every
living creature calls `creature_sense` twice: once with the Sim's pool, and
once with a copy of the pool struct whose `fan_cache` is `NULL`
(`tests/test_views.c:26`), so that call makes its own fan. The two `Senses`
must be the same bytes (`memcmp`, line 38): not "close", the same. It does
so again with 1, 2, 15 and 7 lines (the pool's fan made again each time)
and with `sight_max` 40, long enough for views to walk past the 48 steps the
fan keeps. That last branch was checked to be reached by running the test
under `gdb` with a breakpoint on it (it stopped at step 49).

The world-level guarantee is the existing `test_same_world` (`make test`)
and `slow_same_world` (`make test-slow`), which compare 300 ticks of
checksums with the numbers the code before the earlier study made, and
`tick_report`'s checksum, the same `3fae499e7180bb6f` on every build here.

## 5. C you need to know

- **A comparison is an `int`, 0 or 1**, and can be added or masked.
- **Bit sets**: `1u << j`, `|`, `&`, `~`; unsigned, so every shift is defined.
- **`__builtin_ctz`**: a compiler extension; undefined for 0.
- **`const` is shallow**: it covers the struct's fields, not what they point at.
- **Incomplete struct types**: `struct X *` can be declared and passed without
  saying what `X` holds.
- **`static inline` in a header**: each file that includes it gets its own copy
  the compiler can inline; a plain function in another `.c` file is always a
  real call (without link-time optimisation).
- **`int16_t` / `uint8_t` in a table**: small types keep a fan step to 80
  bytes instead of 184 with `int`s; the conversion from `int` needs a cast
  under `-Wconversion`.

## 6. Problems hit

### 6.1 The clock could not see a 7% drop in wrong guesses

Step 1 timed 10.83 against 10.84 ms. Counting with cachegrind showed it had
removed 1.26 M of 18.1 M mispredicted branches: a real call has a return to
predict and the checks on each side of it. Lesson: on a noisy machine, count.

### 6.2 The saved guesses moved into building the runs

After the walk went run by run, `sense_all`'s mispredictions fell by
1.3 M, but the program's total did not move: `fan_fill` had 1.04 M of its
own, from its `if (same offset as the line before)`. Every creature built its
fan, so that branch ran for every line of every step reached. Building the
runs with arithmetic (`runs += fresh`) took it to 0.29 M, and the pool's fan
(3.3) to almost nothing.

### 6.3 Prefetching was slower

Built first, on the assumption that the walk waits for memory, as the earlier
study's profile suggested. It made a tick 17% slower (12.17 against
10.42 ms). `lscpu` then showed an 8 MB level-2 cache per core: the world's
arrays (about 1.6 MB) are already close, and the wait the profile showed was
mostly wrong guesses (section 4.1), not memory. Lesson: check the cache
sizes before guessing at memory.

### 6.4 A build flag that replaced all the others

To profile by source line a `-g` build was needed beside the release one.
`make MODE=release OUT=build/prof 'CFLAGS+=-g'` failed with
`src/gfx/anim.h:12:10: fatal error: sim.h: No such file or directory`: a
variable given on make's command line replaces the Makefile's own
assignments, `+=` included, so the `-I` paths were gone. `CC="gcc -g"`
added the flag without touching `CFLAGS`.

### 6.5 Plants without `if`s were slower

See 3.5: removing a branch that guards a memory read makes the read happen
every time. Fewer wrong guesses (-0.24 M) did not pay for 2.2 M more cache
misses.

## 7. Questions to check yourself

1. Why does walking run by run give exactly the same world, when it visits
   cells in a different loop?

> Answer: it visits the same cells in the same order. A run's offset is the
> offset of each of its lines, and offsets never fall as the line number
> rises, so runs in order are lines in order with the repeats left out, which
> the old walk skipped anyway. Every float is then added in the same order,
> so every sum has the same last bits.

2. Why is the pool's fan not a `static` table, which would be simpler?

> Answer: the camera's look-ahead steps a second world on another thread, and
> both threads would fill and read the same `static`: a data race. One fan per
> pool means one per world, and each world is stepped by one thread at a time.

3. `the_fan` fills memory through a `const Pool *`. Why does that compile, and
   why is it acceptable here?

> Answer: `const` covers the pool's fields, so the `fan_cache` pointer cannot
> be changed, but the struct it points at is not `const`. It is acceptable
> because the fan never changes an answer: any reader of the pool sees the
> same senses whether the fan was cached or not, which `test_views` checks.

4. What does `open & step->lines[r]` mean, and when is it 0?

> Answer: the lines of run `r` that are still open. It is 0 when every line
> through that cell has stopped at rock or a creature before this step; the
> walk then never visits it, because those lines are not in `left`.

5. The branch-free plant test removed mispredictions but was slower. Why?

> Answer: the `if (veg == 0)` it removed also skipped reading the plant record
> for empty cells. Without it every cell's 28-byte record was read: more
> instructions and more cache misses, which cost more than the wrong guesses
> saved.

## 8. Worth knowing

- Agner Fog, *The microarchitecture of Intel, AMD and VIA CPUs*, the chapter
  on branch prediction (what makes a branch predictable).
- Ulrich Drepper, *What Every Programmer Should Know About Memory*, sections
  3 (caches) and 6.3 (prefetching, and when it does not help).
- The Valgrind manual, "Cachegrind: a cache and branch-prediction profiler"
  (`--branch-sim=yes`, `cg_annotate`).
- `man perf-record`, `man perf-report` (`--sort srcline`).
- The gcc manual, "Other Built-in Functions" (`__builtin_ctz`,
  `__builtin_prefetch`).
- Jens Gustedt, *Modern C*, the sections on `const` qualification and on
  bitwise operators.

## 9. Glossary

- **View**: one of a creature's three looks (ahead, left, right), a wedge of
  ground walked outward step by step.
- **Fan**: the view's fine lines, fanned from -0.75 to 0.75 cells sideways
  per step.
- **Step**: one distance outward, `d`; each line reaches one cell per step.
- **Offset**: how far sideways a line is at a step, `round(slope * d)`.
- **Run**: the lines that reach the same cell at a step; neighbours, since
  offsets never fall as the line number rises.
- **Branch prediction**: the processor guessing which way an `if` goes and
  running ahead; a *misprediction* throws that work away.
- **Prefetch**: asking for memory before it is needed, so it is there when
  it is.
- **Cache (level 1, 2, 3)**: small fast memories near the processor; level 2
  here is 8 MB per core.
- **cachegrind**: valgrind's tool that runs a program in a simulated
  processor and counts instructions, cache misses and mispredicted branches.
- **Checksum / fingerprint**: `sim_checksum`, a hash of everything that
  decides the world's future; the same number means the same world.
