# 2026-11-25: The ancestors' years now run on a thread of their own, so the "Growing a new world" screen stays smooth

Commits: `818f4ef` (panel facts), `29ce50b` (prelude line and settle rule), `cd7b793` (the prelude thread and its test), `fb0c8f5` (family strip from samples), `fda4c05` (the window uses the thread), `ebeb55d` (frame report prelude line), plus this doc's commit.
Run it: `make MODE=release window && build/release/veredas-window --seed 4 --ancestors 8` (and `--frame-log frames.txt`, then `sh tests/frame_report.sh frames.txt`, to measure).
Reading time: about 30 minutes.

## 1. What was built

Before a new world opens, the window runs its "ancestors' years" (the
*prelude*: a few years of evolution run as fast as possible, so the world
opens on creatures that have already adapted) and shows a screen with a
turning ring, a progress bar, the year and a few walking animals.

Until now those ticks ran on the window's own thread (the *main thread*, the
one that reads the keys and draws). Each frame ran ticks until 28 ms had
passed, but it checked the clock only after each tick. One tick of a grown
world takes about 38 ms on Nicolas's machine, so every frame ran one or two
ticks and came late: about 23 frames a second, and the ring and the animals
jerked.

Now a second thread, the *worker*, gets the world and runs the ticks one
after another. The main thread never runs a tick and never waits for one
during the prelude. It draws the screen from a small summary, the
*glance*, that the worker publishes after each tick. A key press takes the
world back: the main thread waits for the tick under way to end (at most one
tick), and the prelude ends where it got to, as before.

The measurement, as in the brief: `make MODE=release window`, then
`SDL_VIDEODRIVER=dummy timeout -s TERM 60 build/release/veredas-window --seed 4 --ancestors 8 --frame-log frames.txt`,
then `sh tests/frame_report.sh frames.txt`. "Before" is the program built
from `007355c` in a separate `git worktree`; "after" is this branch. The two
ran one after the other on the same machine, nothing else running. A second
pair of runs, 150 seconds long, timed the whole prelude (4,800 ticks), which
neither program finishes in 60 seconds here.

**This was a cloud machine (4 cores), not Nicolas's computer.** A tick here
takes about 12 ms, not 38, so absolute numbers differ, and the gain in
smoothness is smaller here than it will be on a machine where a tick is
longer than a frame.

| seed 4, 8 ancestor years, `SDL_VIDEODRIVER=dummy` | before (`007355c`) | after |
|---|---|---|
| frames a second, 60 s run (all prelude) | 27.2 | 30.0 |
| slow frames (gap over 40 ms), 60 s run | 335 (5.6 a second) | 2 (0.0 a second) |
| gap median / p90 / p99 / max (ms) | 35.9 / 42.5 / 53.9 / 91.4 | 33.4 / 34.4 / 34.8 / 58.4 |
| work per frame, median (ms) | 35.8 | 2.5 |
| prelude ticks done in the 60 s | 4,437 | 4,532 |
| the whole prelude, 4,800 ticks (150 s run) | 65.6 s | 61.4 s |

What the numbers say:

- **Smoothness** is the real gain. Before, 335 of 1,626 of the frames came more
  than 40 ms after the one before ("slow frames"); after, 2 of 1,795. The
  90th-percentile gap went from 42.5 ms to 34.4 ms: every frame
  now comes on time, because a frame's work is drawing (about 2-3 ms), not
  ticks.
- **Speed of the years** improved less, on this machine: the 4,800 ticks took 61.4 s instead of 65.6 s (6% sooner).
  Here a tick (12 ms) is shorter than a frame (33 ms), so the old prelude
  already ticked about 90% of the time; the worker ticks 100% of the time.
  On Nicolas's machine, where one tick filled a whole frame and the clock
  check came after it, the old prelude also spent a part of each frame
  drawing and waiting, so the years should finish sooner there too; that is
  not measured here.

## 2. Map of the change

```
 Makefile                   |   2 +-
 src/app.c                  |  19 +++-
 src/app.h                  |   8 ++
 src/gfx/scene.c            |   6 ++
 src/gfx/scene.h            |   5 +
 src/panel.c                |  43 +++++---
 src/panel.h                |  21 ++++
 src/preludethread.c        | 216 +++++++++++++++++++++++++++++++++++++++
 src/preludethread.h        | 105 +++++++++++++++++++
 src/window/window_main.c   | 247 +++++++++++++++++++++++++++++++++++++--------
 tests/frame_report.sh      |  17 ++++
 tests/test_preludethread.c | 198 ++++++++++++++++++++++++++++++++++++
 study/2026-11-25-prelude-thread.md  (this doc)
 12 files changed, 824 insertions(+), 63 deletions(-)
(code only, against main at 5f8f6d3; this doc comes on top)
```

Read in this order:

1. `src/preludethread.h` (the whole file: the rule of who owns the world,
   then the six calls).
2. `src/preludethread.c` (`worker` first, then `preludethread_give`,
   `preludethread_glance`, `preludethread_take`).
3. `tests/test_preludethread.c` (what must hold: the same world as the
   main-thread prelude).
4. `src/window/window_main.c`: `Glance` and `WorldTop` (near line 180),
   `glance_fill`, `world_away`, `world_top`, `take_world` (lines 321-377),
   then `prelude_frame` (line 2231), then every `take_world(a)` call.
5. `src/panel.h` / `src/panel.c`: the split of `panel_draw` into
   `panel_facts` and `panel_draw_facts`.
6. `src/app.c`: `app_prelude_settle` and `app_prelude_line`; `src/gfx/scene.c`:
   `scene_draw_strip_samples`; `tests/frame_report.sh`: the new last line.

## 3. Decisions

### 3.1 Hand the world itself to the worker, not a copy

- **Decision:** the worker steps `a->sim`, the world on screen, through a
  pointer (`preludethread_give(&a->prelude_thread, &a->sim, until)`,
  `src/window/window_main.c:2234`; the worker's `sim_step(t->sim)`,
  `src/preludethread.c:54`).
- **Alternatives:** (a) the worker steps a copy and the copy replaces the
  world at the end, as the camera's look-ahead does (`src/lookthread.c`);
  (b) the main thread keeps ticking but each frame gives the clock back
  sooner.
- **Why:** a copy costs about 30 ms and a second world's memory, and a key
  press would then need the copy handed back as the new world (a struct
  assignment plus freeing the old one). The prelude's world is not watched
  while it runs, so nothing needs a second one. (b) cannot work: a single
  tick is longer than a frame, and a tick cannot be cut in half.
- **Cost:** while the world is away the main thread may not read it at all.
  That is what 3.2 and 3.3 are about.

### 3.2 The screen draws from a "glance" that the worker publishes

- **Decision:** after each tick the worker calls a function the window gave
  it (`glance_fill`, `src/window/window_main.c:341`), which fills a plain
  struct, `Glance` (`src/window/window_main.c:204`). It holds the tick, the
  year's length, the animal count, whether the world is over, the weather,
  everything the panel shows (`PanelFacts`, `src/panel.h:102`) and the
  largest family's strip. The worker then copies it, under the lock, to where
  the main thread reads it (`src/preludethread.c:60`). The main thread copies
  it out once a frame (`preludethread_glance`, `src/preludethread.c:130`).
- **Alternatives:** (a) lock the world during drawing, so the main thread
  waits for the tick to end before it draws (that is the stutter again);
  (b) publish only the tick and draw the panel empty during the prelude (a
  visible change, and the panel's numbers are part of what makes the wait
  interesting); (c) a fixed struct inside `preludethread.c` (the core code
  would have to include the window's family header, `src/gfx/family.h`,
  which the core may not depend on, see the Makefile's groups).
- **Why:** the screen stays exactly as it was, and `preludethread.c` stays
  generic: it knows the glance's size and the function that fills it, nothing
  more (`PreludeGlanceFn`, `src/preludethread.h:51`). The glance is about
  1.5 KB, and filling it costs about 0.3 ms against a 12 ms tick (measured
  with a throwaway program over ticks 2,000-3,000 of seed 4).

### 3.3 Anything that may read the world takes it back first

- **Decision:** `take_world` (`src/window/window_main.c:369`) is called at
  the top of `handle_key` for every key but F3 (line 906), in `press_button`
  for every button but the world menu's (line 1145), in `click` (line 444),
  in `zoom_or_dive` (line 422) and before the world is saved on quitting
  (line 2721). The next frame gives the world back to the worker if the
  prelude is still on (`prelude_frame`, line 2231).
- **Alternatives:** look at each key and take the world only for those that
  read it; or queue the keys and handle them when the prelude ends.
- **Why:** there are dozens of keys and buttons, and new ones get added; a
  rule that needs an audit of each would break silently. Taking it back costs
  at most one tick of waiting, only when the player does something. The rule
  "any key ends the prelude where it got to" needs exactly this wait anyway.
  Mouse movement, dragging and resizing touch only the camera and the layout,
  so they never wait.

### 3.4 The family records go with the world

- **Decision:** while the world is away the worker also runs the family
  look (`family_update`, every 500 ticks) inside `glance_fill`, and the main
  thread does not touch `a->families` (the guard at the `family_update` call
  in `app_frame`, and `draw_family` reading the glance, line 1403).
- **Alternative:** skip the family looks during the prelude.
- **Why:** the strip of the largest family's bodies over time would then
  start empty when the world opens, missing the nine looks of the ancestors'
  years that it had before. To draw the strip from the glance,
  `scene_draw_strip_samples` (`src/gfx/scene.c:2938`) takes samples instead
  of the records.

### 3.5 While somebody is followed, the ticks stay on the main thread

- **Decision:** `prelude_frame` gives the world only when nobody is followed
  (`!a->inspect.following`, line 2233). Otherwise it runs
  `app_prelude_step` as before.
- **Alternative:** publish the followed creature's card in the glance too.
- **Why:** following during the prelude is rare (clicking a panel line
  while waiting), and the card reads a great deal of the world. Falling back
  to the old way keeps it correct at no cost; the browser build uses the same
  fallback all the time.

### 3.6 `--frames` runs and the browser build keep the old way

- **Decision:** the worker is started only when `opt.frames == 0`
  (`src/window/window_main.c:2633`), and `--frames` runs the prelude whole
  before the first frame with `app_prelude_finish` (line 2653), as before.
  In the browser build `PRELUDETHREAD_THREADS` is 0
  (`src/preludethread.h:40`), `preludethread_start` says no and
  `preludethread_give` always returns false (`src/preludethread.c:197`).
- **Why:** test pictures must come out the same on every run, and the
  browser page has no threads (they would need a different page setup and
  special server headers). This is the same choice the camera's look-ahead
  made.

### 3.7 Split `panel_draw`, and why it had to be split

- **Decision:** `panel_draw` became `panel_facts` (gather from the world,
  `src/panel.c:365`) then `panel_draw_facts` (draw, line 386).
- **Why:** the worker must gather the panel's numbers, but it must not draw
  (drawing code uses globals, such as the canvas that `cells_canvas_set`
  points at). The old `panel_draw` also kept its stats in a `static Stats now`,
  which the worker and the main thread would both have written: see 6.1.

## 4. Implementation walkthrough

### 4.1 Who owns the world: `away` and `busy`

The header's first comment states the rule (`src/preludethread.h:1-28`).
Two flags decide who may touch `*sim`:

- `away` (`src/preludethread.h:57`): set by `preludethread_give`, cleared by
  `preludethread_take`. Only the main thread changes it, so the main thread
  may read it without the lock (`world_away`, `src/window/window_main.c:354`).
  The worker reads it only under the lock.
- `busy`: set by the worker, under the lock, just before a tick, and cleared
  just after (`src/preludethread.c:52`). While busy, the worker is touching
  the world without holding the lock.

> **C note: handing a whole world between threads.** In TypeScript a Web
> Worker cannot share an object with the page: `postMessage` copies it (a
> "structured clone"), or *transfers* an `ArrayBuffer`, after which the page's
> copy is unusable and touching it throws. C threads share all memory.
> `preludethread_give` passes the address of `a->sim`, eight bytes, and from
> then on both threads *can* reach the same 100+ megabytes of world. Nothing
> in the language stops the main thread from reading `a->sim.tick` while the
> worker writes it. "Handing over" is a convention the code keeps: the
> `away` flag, and the rule that every reader checks `world_away(a)` first.
> It is like a transferred `ArrayBuffer`, except the old reference still works
> and silently gives wrong answers.

### 4.2 The worker's loop

`worker` (`src/preludethread.c:42`) holds the lock all the time except
while it ticks and while it sleeps:

```c
while (!t->quit) {
    if (!t->away || t->finished) {
        pthread_cond_wait(&t->wake, &t->lock);  /* sleep until given a world */
        continue;
    }
    t->busy = true;
    pthread_mutex_unlock(&t->lock);
    sim_step(t->sim);                 /* the tick, without the lock */
    bool end = at_end(t);
    fill_glance(t, t->glance_next);   /* the glance, still the worker's own */
    pthread_mutex_lock(&t->lock);
    t->busy = false;
    t->finished = end;
    memcpy(t->glance, t->glance_next, t->glance_size);   /* publish */
    pthread_cond_broadcast(&t->idle); /* wake a waiting take or wait */
}
```

There are two glance buffers. `glance_next` belongs to the worker and is
filled without the lock, while the worker still owns the world. `glance`
is the published one, and both threads touch it only under the lock. The
copy between them is a `memcpy` of 1.5 KB, a microsecond, so the lock is
never held long.

> **C note: `void *` and function pointers instead of generics.** The
> glance's type is the window's business, so `preludethread.c` sees it as
> bytes: `unsigned char *glance` and a `size_t glance_size`, filled by a
> `PreludeGlanceFn`, declared at `src/preludethread.h:51` as
> `typedef void (*PreludeGlanceFn)(void *ctx, const Sim *sim, void *out);`.
> In TypeScript you would write `PreludeThread<G>` with
> `fill: (ctx: C, sim: Sim) => G`. C has no generics, so the type is erased by
> hand: the window's `glance_fill` receives `void *` and converts back
> (`Families *f = ctx; Glance *g = out;`, `src/window/window_main.c:343`). In
> C a `void *` converts to any object pointer without a cast; nothing checks
> it is the right type, so `sizeof(Glance)`, passed once to
> `preludethread_init` (line 2630), must match what `glance_fill` writes.
> `memcpy` copies those bytes without knowing what they mean, which is fine
> because a `Glance` holds no pointers.

### 4.3 Giving the world

`preludethread_give` (`src/preludethread.c:104`) runs on the main thread
while the worker sleeps. It stores the pointer and the end tick, fills the
first glance (so the screen has something to draw before the first tick
ends), then under the lock sets `away = true` and signals `wake`:

> **C note: why the signal is sent under the lock.** A condition variable
> does not remember signals. If the worker had just checked `away` (false)
> and was about to call `pthread_cond_wait`, a signal sent between the two
> would be lost and the worker would sleep forever. Because the worker holds
> the lock from its check until `pthread_cond_wait` atomically releases it,
> and `give` takes the lock before setting `away` and signalling, the signal
> can only come before the check (the worker then sees `away`) or after the
> worker is asleep (the signal wakes it).

### 4.4 Each frame of the prelude

`prelude_frame` (`src/window/window_main.c:2231`) replaces the old
"run ticks for 28 ms" block:

1. If the world is here and nobody is followed, give it.
2. If it is away, copy the latest glance into `a->glance`. If the worker
   says it has finished, take the world back. That never waits, because a
   finished worker is asleep.
3. Otherwise (no worker, or somebody followed) run ticks here as before.
4. Write the prelude's line ("evolving ancestors: year 3 of 8...") from
   `world_top(a)` (line 360): the glance's numbers while away, the world's
   own otherwise.

The rest of the frame was checked for every read of `a->sim`. Each one now
either runs only when the world is here (`family_update`, `scene_update`,
`branch_catch_up`, `update_following`, line 1952), or reads `world_top`
(the title line, the weather's fade, `advance_clocks`,
`frame_needs_drawing`, the frame log's tick count). Some needed no change:
they sit behind `!a->prelude.active` (the camera's `look_wanted`, the weather
notices, `update_notices`), and the world can only be away while the
prelude is active.

> **C note: why reading the world during a step is a data race, and why that
> is worse than a stale number.** In JavaScript, the page and a worker
> never touch the same object at the same time, so a value is always some
> value that was really there. C11 is stricter: if one thread writes a
> memory location and another reads it with no lock or atomic between them,
> that is a *data race*, and the whole program's behaviour is undefined, not
> only that one value's. In practice: a 64-bit read can see half of an old
> value and half of a new one; a pointer read while the pool grows can point
> at freed memory; and the compiler, which may assume no other thread
> changes plain variables, can read a value once and keep it in a register.
> That last one really happened while writing this doc: a throwaway program
> that waited with `while (seen < 100) { seen = s.tick; }` during a step
> never ended, built with `-O1`. The compiler read `s.tick` once, before the
> loop (see 6.2).

### 4.5 Taking the world back on a key press

`preludethread_take` (`src/preludethread.c:142`):

```c
pthread_mutex_lock(&t->lock);
t->away = false;            /* the worker runs no further step after this one */
while (t->busy) {
    pthread_cond_wait(&t->idle, &t->lock);
}
pthread_mutex_unlock(&t->lock);
```

If the worker is between ticks (it holds the lock or is waiting for it),
`take` gets the lock, clears `away`, and the worker's next look at `away`
sends it to sleep: no wait. If the worker is mid-tick, `busy` is true and
`take` sleeps on `idle` until the worker's broadcast at the end of the tick.
The window's `take_world` (`src/window/window_main.c:369`) then applies
`app_prelude_settle` (`src/app.c:74`), so a tick that happened to be the
last one ends the prelude exactly as `prelude_tick` would have.

> **C note: waiting for a step is a loop, not an `await`.** In TypeScript you
> would `await` a promise that the worker's message resolves. A condition
> variable is lower level: `pthread_cond_wait` releases the lock, sleeps,
> and takes the lock again before returning, and it may return when nobody
> signalled (a *spurious wakeup*, allowed by POSIX). So the condition,
> `t->busy`, is always checked again in a `while`. The lock also makes the
> worker's writes visible: everything the worker wrote during its tick
> happens before its `pthread_mutex_lock` after the tick, which happens
> before `take` returns from `pthread_cond_wait` holding that lock. After
> `take`, the main thread sees the world exactly as the worker left it.
> Without the lock this ordering ("happens-before") is not guaranteed even on
> one machine.

### 4.6 What the thread checker prints

`make test-threads` builds with `-fsanitize=thread` (ThreadSanitizer, the
thread checker) and runs `tests/test_lookthread.c` and now
`tests/test_preludethread.c`. The checker watches every memory access and
reports any two from different threads that no lock orders. To see what a
mistake looks like, a throwaway program gave a world to the worker, slept
50 ms and read `s.tick` (the bug). Its real output:

```
==================
WARNING: ThreadSanitizer: data race (pid=5009)
  Read of size 8 at 0x7ffc94b66dc0 by main thread:
    #0 main race_demo.c:18

  Previous write of size 8 at 0x7ffc94b66dc0 by thread T1:
    #0 sim_step src/sim.c:822
    #1 worker src/preludethread.c:54

  As if synchronized via sleep:
    #0 nanosleep ../../../../src/libsanitizer/tsan/tsan_interceptors_posix.cpp:397 (libtsan.so.2+0x58cc6) (BuildId: 2a13a7710e361d06f7babbea53065ca2be93f738)
    #1 main race_demo.c:17

  Location is stack of main thread.

  Location is global '<null>' at 0x000000000000 ([stack]+0x1edc0)

  Thread T1 (tid=5011, running) created by main thread at:
    #0 pthread_create ../../../../src/libsanitizer/tsan/tsan_interceptors_posix.cpp:1022 (libtsan.so.2+0x5ac1a) (BuildId: 2a13a7710e361d06f7babbea53065ca2be93f738)
    #1 preludethread_start src/preludethread.c:93
    #2 main race_demo.c:14

SUMMARY: ThreadSanitizer: data race race_demo.c:18 in main
==================
ThreadSanitizer: reported 1 warnings
saw tick 2, ended at 200
```

Read it from the top: the main thread read 8 bytes at line 18 of the demo
(`seen = s.tick`), and before that the worker had written the same 8 bytes
in `sim_step` (`src/sim.c:822`, `s->tick++`) called from `worker`
(`src/preludethread.c:54`), with no lock between the two. "As if
synchronized via sleep" is the checker noticing that the only thing between
the two accesses was the 50 ms `nanosleep`: a sleep orders nothing in C, the
race is real, and the read could as well have come mid-tick. The thread that
did the write is named at the end: T1, made by `preludethread_start`. The
demo printed `saw tick 2`, a number that happened to be right, which is what
makes races hard to find by looking at results.

Run five times in a row, the same program was reported once. The checker
sees only races that really happen in a run, and a sleep between two
accesses makes it unsure enough that by default it often stays quiet. A
quiet run proves less than a report does. That is why the test makes the
two threads really run at once, and why the design is checked by reasoning
(who owns the world, and when) and not only by the checker. The window's
own build passes the same checker: a ThreadSanitizer build of the window ran
the prelude for 20 seconds (seed 4, 8 years, quit mid-prelude) and for 40
seconds (a 64x32 world, 1 year, past the prelude's end, with `--save`), with
no report.

### 4.7 The test

`tests/test_preludethread.c` builds a small world (64x32, 40-tick years,
3 years) and checks:

- `test_same_world_as_finish` (line 75): a prelude run on the thread, with
  the main thread reading the glance every millisecond meanwhile, ends on the
  same tick and the same `sim_checksum` as `app_prelude_finish` (line 108).
  The glance function also runs `panel_facts`, so the stats scratch buffer is
  written between ticks just as in the window.
- `test_taken_back_part_way` (line 123): taken back after tick 5, the world
  equals a world stepped to the same tick on one thread (line 150); given
  again and run to the end, it still equals the main-thread prelude
  (line 163), as after the setup screen's Esc.
- `test_give_says_no` (line 173): no worker, or a world already at its end,
  and give says no.

It is in `THREAD_TESTS` (`Makefile:218`), so CI runs it under the thread
checker for gcc and clang, as well as under the memory checkers in
`make test`.

## 5. C you need to know

- **`pthread_mutex_t`**: a lock. `pthread_mutex_lock` blocks until no other
  thread holds it. Everything between lock and unlock is ordered with every
  other thread's locked sections.
- **`pthread_cond_t`**: a place to sleep until another thread says something
  changed. Always used with a mutex and a condition checked in a `while`.
  `signal` wakes one sleeper, `broadcast` all of them.
- **`pthread_create` / `pthread_join`**: start a thread running a function
  `void *f(void *)`, and wait for it to return. `pthread_create` returns an
  error number (0 for success), not -1 and `errno`.
- **Data race = undefined behaviour** (C11, section 5.1.2.4): not "a stale
  value", but anything at all, including the compiler removing your loop's
  reads.
- **`void *`**: a pointer to anything. It converts to and from any object
  pointer without a cast in C (not in C++). The compiler cannot check the
  type on the other side.
- **Function pointer typedefs**: `typedef void (*Fn)(void *ctx, ...)` names
  the type "pointer to a function taking these and returning void". Passing
  `ctx` alongside is how C does closures: the function plus the data it
  needs.
- **`memcpy(dst, src, n)`**: copies n bytes. Safe for structs that hold no
  pointers; for one that does, both copies would point at the same memory.
- **Compound literals**: `(WorldTop){.tick = s->tick, ...}`
  (`src/window/window_main.c:326`) builds a struct value in an expression,
  like an object literal; fields not named are zero.
- **`static` local variables are shared by every thread**: `static Stats now;`
  inside a function is one variable for the whole program, not one per call
  and not one per thread (see 6.1).
- **`malloc(0)`** may return NULL or a unique pointer; code that treats NULL
  as "out of memory" should ask for at least one byte (`src/preludethread.c:83`).

## 6. Problems hit

### 6.1 A `static` inside the panel would have been written by two threads

`panel_draw` began with:

```c
static Stats now;
sim_stats_now(s, &now);
```

`static` here kept a large struct off the browser's small stack. It is one
variable for the whole program. Had the worker gathered the panel through
`panel_draw` while the main thread drew a panel (the terminal, or a future
caller), both would write `now` at once. The fix was the split in 3.7:
`panel_facts` writes into a struct the caller owns (the worker's own
`glance_next`), and the `static` that remains (`static PanelFacts facts`,
`src/panel.c:381`) is used only by `panel_draw`, which only the drawing
thread calls. The audit of every non-const `static` reachable from the
worker (see 6.5) is how this kind of problem is found before the thread
checker happens to see it.

### 6.2 A demo of the race hung instead of crashing

To capture the thread checker's report for 4.6, the first demo waited in a
loop:

```c
uint64_t seen = 0;
while (seen < 100) {
    seen = s.tick; /* the bug: the world is away */
}
```

Built with `-O1 -fsanitize=thread`, it never ended (killed after 5
minutes). The cause: with no lock or atomic in the loop, the compiler may
assume nothing else changes `s.tick`, reads it once, and loops on the
register forever. This is the "undefined behaviour" of a data race, seen for
real. The fix, for the demo, was one read after a 50 ms sleep, which the
checker reported as shown in 4.6. In real code the fix is the lock (or a C11
`_Atomic`), never `volatile`.

### 6.3 A measurement script read the wrong column

The first count of prelude frames used column 12 of the frame log, which is
`drawn`, not `prelude` (column 13, see the header line in
`src/frame_log.c:22`). It made a 1-year prelude look as if it had run past
its 600 ticks. The world was checked to be sure: the window was quit with
`--save`, and a throwaway program loaded the save (tick 658), built the same
seed and size fresh, stepped it to tick 658 on one thread, and compared:
`saved tick 658 checksum 1e6f88f5907be796; fresh 1e6f88f5907be796 same`. The
prelude had ended at tick 600 and the world had then run at normal speed.
The lesson went into `tests/frame_report.sh`, which now prints the prelude's
ticks and time itself (its last line), so nobody has to count columns.

### 6.4 The test counted 15 million checks

The first version of the test read the glance in a tight loop with a
`CHECK` inside: `tests/test_preludethread.c: 15390077 checks, 0 failed`. A
loop that never waits also fights the worker for the lock. The fix: the loop
waits a millisecond between looks (as a frame would) and checks once after
the loop. Now: `39 checks, 0 failed`.

### 6.5 The static audit

`grep -nP '^\s*static\s+(?!const\b)(?!inline\b)[^(]*[;=\[]' src/*.c src/gfx/family.c`
lists every non-const `static` variable. The worker runs `sim_step`, then
`glance_fill`: `family_update`, `panel_facts` (with `sim_stats_now`,
`stats_measure`, `panel_colour_groups`, `render_display_hue`,
`tactics_list`), `family_largest` and `family_strip`. The list holds the
brain registry (`src/brain.c:17`, written only while starting up),
`compare.c` and `notices.c` (window thread only), `render.c` and `term.c`
(the terminal), and the `static PanelFacts` in `panel_draw` (6.1). None is
reached by the worker. `src/gfx/family.c` and `src/palette.c` have none.

### 6.6 The machine's ticks are short

On this cloud machine a tick of seed 4 takes about 12 ms (11.8 ms over
ticks 2,000-3,000), not 38, so the old prelude was not as bad here as on
Nicolas's machine (27 frames a second, not 23), and the years sped up only
a little. The numbers in section 1 are this machine's.

## 7. Questions to check yourself

1. While the world is away, why may the main thread read `t->away` without
   the lock, but not `a->sim.tick`?

> Answer: only the main thread ever writes `away` (in give and take), and a
> thread always sees its own writes in program order, so its reads cannot
> race. `a->sim.tick` is written by the worker in every `sim_step`; a read on
> the main thread with no lock between the two is a data race.

2. A key is pressed while the worker is in the middle of a 38 ms tick. What
   happens, step by step, and how long does the main thread wait?

> Answer: `handle_key` calls `take_world`, which locks, sets `away = false`,
> and, since `busy` is true, sleeps in `pthread_cond_wait(&t->idle, ...)`. The
> worker finishes the tick, locks, clears `busy`, publishes the glance and
> broadcasts `idle`; seeing `away` false, it then goes to sleep. `take`
> wakes, sees `busy` false and returns: the main thread waited for the rest
> of that tick, at most 38 ms. `app_prelude_settle` runs, then the key's
> normal handling ends the prelude (`app_prelude_stop`) where it got to.

3. Why does the worker fill the glance into `glance_next` without the lock,
   and only copy it into `glance` under the lock? What would go wrong if it
   filled `glance` directly?

> Answer: filling needs the world, which the worker owns while away, and
> takes about 0.3 ms; holding the lock that long is harmless but needless.
> Filling `glance` directly without the lock would race with the main thread's
> `memcpy` out of it: the screen could draw a glance that is half one tick's
> and half the next one's (and it would be undefined behaviour).

4. Why does the test compare `sim_checksum` and the tick, rather than, say,
   the animal count?

> Answer: the fingerprint covers the whole state that decides what happens
> next (every creature, every cell, the random number generator), so two
> worlds with the same fingerprint and tick go on identically. Many different
> worlds have the same animal count.

5. `--frames` runs never start the worker. What would break if they did?

> Answer: with a worker the number of ticks per frame depends on how fast
> the machine is, so the frame on which the world opens, and so every
> picture after it, would differ from run to run and machine to machine. The
> pinned scene pictures and the window smoke test depend on the same frames
> every time.

6. Why is the signal in `preludethread_give` sent while holding the lock?

> Answer: a condition variable keeps no memory of signals. Holding the lock
> means the worker is either before its check of `away` (it will see it set)
> or already inside `pthread_cond_wait` (it will be woken). Unlocked, the
> signal could fall between the worker's check and its sleep and be lost.

## 8. Worth knowing

- `man 3 pthread_cond_wait` (the spurious wakeup paragraph),
  `man 3 pthread_mutex_lock`, `man 3 pthread_create`, `man 7 pthreads`.
- David R. Butenhof, *Programming with POSIX Threads*, chapter 3
  (synchronization: mutexes and condition variables, the "predicate loop").
- Michael Kerrisk, *The Linux Programming Interface*, chapters 29 and 30
  (threads, and thread synchronization).
- Jens Gustedt, *Modern C*, the chapter on threads and atomic access: what a
  data race is in C11 and why it is undefined behaviour.
- The C11 standard (draft N1570), section 5.1.2.4, "Multi-threaded
  executions and data races".
- The ThreadSanitizer manual (clang.llvm.org/docs/ThreadSanitizer.html):
  how to read its reports.
- Hans Boehm, "Threads Cannot Be Implemented as a Library" (2005): why the
  compiler must know about threads, and the loop-hoisting of 6.2.

## 9. Glossary

- **Prelude / ancestors' years**: the years of evolution run fast before a
  new world is shown (`src/app.h`, `Prelude`).
- **Main thread**: the window's thread, which reads keys and draws every
  frame.
- **Worker (thread)**: the second thread, which here only runs prelude ticks.
- **Tick**: one step of the world (`sim_step`).
- **Frame**: one picture; the window aims at 30 a second, 33 ms each.
- **Gap**: the time from one frame's start to the next one's; over 40 ms is a
  "slow frame", a picture shown late.
- **Away**: the world has been given to the worker and not yet taken back.
- **Busy**: the worker is in the middle of a tick, without the lock.
- **Glance**: the summary of the world the worker publishes after each tick,
  so the screen never reads the world while it is away.
- **Mutex (lock)**: lets one thread at a time into a section of code.
- **Condition variable**: lets a thread sleep, holding no lock, until another
  thread says something changed.
- **Spurious wakeup**: a return from `pthread_cond_wait` with no signal,
  which POSIX allows; the reason for the `while`.
- **Data race**: two threads touch the same memory, at least one writing,
  with no lock or atomic ordering them; undefined behaviour in C.
- **Happens-before**: the ordering a lock (or atomic) gives: writes before
  an unlock are visible after the next lock of the same mutex.
- **ThreadSanitizer (thread checker)**: a compiler option
  (`-fsanitize=thread`) that reports data races as they happen in a run
  (`make test-threads`).
- **Fingerprint**: `sim_checksum`, a hash of everything that decides the
  world's future.
- **Worktree**: a second checkout of the same repository in another folder
  (`git worktree add`), used here to build the "before" program.
