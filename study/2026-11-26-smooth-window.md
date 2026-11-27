# 2026-11-26: a smooth window before the playtest

## What was built

Two problems were left before outside players try Veredas: the window still
stuttered (worst in the first seconds), and Far zoom at 1920x1080 had crept
back over its 24 ms budget (about 26 ms on Nicolas's machine). Both are fixed
without changing what the game draws or how the world unfolds: every
screenshot and every world fingerprint is the same as before, byte for byte.

Six commits on `claude/upbeat-tesla-sky6im`:

1. **Art samples looked up** (`src/gfx/art.c`). Shrinking a drawn animal to
   Far size took up to 64 samples per pixel, each doing two divisions. Now
   each picture works out where its samples land once per column and once per
   row, and the pixels only look those up.
2. **A body's standing measures kept, cache doubled** (`src/gfx/sprite.c`).
   Every new picture of a drawn animal used to draw the whole skeleton first,
   just to measure it. The measures are the same for every pose of the body,
   so they are now kept per body. The picture cache doubled (24,576 to 49,152
   pictures, 12 MB more at the peak).
3. **Tile lookups worked out once** (`src/gfx/tiles.c`). Building a ground
   tile asked "which of the nine cells does this point show?" about 19 times
   a pixel. The answer never depends on the tile's terrain, only on the point,
   so a table answers it now. Tiles build about 4 times faster.
4. **A cheaper tick** (`src/brain_net.c`, `src/creature.c`). The brain read
   each weight through a function call in another file (a million calls a
   tick), and the senses rounded with a maths-library call per cell. Both are
   now plain code. A tick is about 16% cheaper.
5. **The prelude on its own thread** (`src/preludethread.c`, new). A new
   world's first years used to run in 28 ms slices on the window's thread,
   each slice ending a tick late. Now a worker thread grows the world while
   the window animates its "Growing a new world" screen at a steady 30 frames
   a second. In its spare time the window prepares the first world frame's
   work ahead.
6. **Ticks after the picture** (`src/window/window_main.c`). In the open
   window, a frame's ticks now run after its picture has been shown. A tick
   and the redraw it causes then land in different frames instead of the
   same one.

### Numbers

Far zoom, 1920x1080, `--frames 330` minus `--frames 30`, over 300 frames.
The old and new builds ran at the same moment on two cores, then swapped;
median ratio of 5 pairs:

| view    | old (ms here) | new (ms here) | new / old | his machine (26 ms × ratio) |
|---------|---------------|---------------|-----------|-----------------------------|
| surface | 18.6–19.8     | 16.4–16.8     | 0.862     | about 22.4 ms (budget 24)   |
| sky     | 14.3–15.4     | 12.6–13.2     | 0.872     | (was within budget)         |

Stutter, from `--frame-log` and `tests/frame_report.sh`. A frame counts as
late when it starts more than 40 ms after the one before. Seed 4, 1920x1080,
default speed, old and new at the same moment:

| run                                           | late frames a second, old | new  |
|-----------------------------------------------|---------------------------|------|
| first 50 s (prelude and after), this machine  | 2.7                       | 0.1  |
| first 70 s, machine made ~2× slower           | 10.4                      | 0.7  |
| after the prelude only, ~2× slower            | 2.4                       | 1.3  |
| first world frame's work                      | 273–525 ms                | 87 ms|

"Made ~2× slower" means a busy loop sharing each core with the window. That
is harsher than Nicolas's machine, which is about 1.5× slower than this one
at drawing.

## Decisions

- **Speed only, no visible trade-off.** The brief said to stop and ask if a
  budget could only be met by drawing less. It never came to that: each
  change keeps the pictures byte-identical. This was checked by comparing
  PPM screenshots at every zoom, the sky and the lab against a build of
  `main`.
- **The prelude's worker gets its own copy of the world.** The alternative
  was letting the worker step the window's world in place. But the window
  reads the world all through a frame (panel, title, family strip,
  notices), so every read would have needed a lock, or careful code to skip
  it during the prelude. With a copy, the worker's world is touched by
  nobody else, and the only shared things sit under one lock: a stop flag,
  the tick reached, and the newest copy to show.
- **A copy once a second, so the panel stays alive.** Without it, the panel
  and the title would have frozen at year 0 for the whole prelude (the
  screenshot of the old prelude shows them moving). A copy costs the worker
  about 30 ms, so this slows the prelude by about 3%.
- **The finished world is moved, not copied.** `*shown = t->world` is a
  struct assignment: it copies the struct's fields, its pointers included,
  so the window's `Sim` takes over the worker's memory. `branch.c` already
  swaps worlds this way, and the `Pool` comments say its pointers into the
  `Sim` are set again each tick for exactly this reason.
- **`--frames` runs keep the old order.** Tests and screenshots must come out
  the same on every run, so they still run the prelude whole before the first
  frame, and their ticks before drawing. Only the open window changed.
- **Ticks after the picture, not a ticking thread.** A thread for the shown
  world's ticks would need a copy of the world to draw from while the worker
  ticks (`sim_copy` takes about 30 ms, too slow every tick), and every key
  that changes the world would have to go to the worker's copy. Moving the
  ticks to the end of the frame needed 20 lines and gets most of the benefit
  on his machine. The threaded version is written up below as the next step,
  if it is ever needed.
- **The cache doubled again.** 4% at Far for 12 MB is the same trade the
  2026-11-21 doubling made (6% for 7 MB).

## Walkthrough

### 1. Samples by axis — `src/gfx/art.c:158`

`SampleAxis` holds, for one axis of the grid, the art pixel each sample lands
on. `sample_axis` (`src/gfx/art.c:163`) fills it once per picture, using
exactly the arithmetic `sample_ink` used to do per sample. `count_samples`
(`src/gfx/art.c:179`) then only indexes:

```c
const int *ax = &xs->at[(gx - xs->lo) * samples];
const int *ay = &ys->at[(gy - ys->lo) * samples];
...
uint8_t ink = art_ink(f, ax[sx], ay[sy]);
```

*C note:* `&xs->at[k]` is a pointer to element k, so `ax[sx]` reads element
`k + sx`. A TypeScript analogue would be `xs.at.subarray(k)`, but in C it
costs nothing: it is just an address.

*C note:* the result is the same only because the float expressions are
written the same way. C (without `-ffast-math`) evaluates `a + b * c` in the
order written, and on x86-64 every `float` operation rounds the same way
every time. Rewriting `(p - anchor - 0.5f) / scale` as `p * (1/scale) - ...`
would have changed the last bit of some results, and with it some pictures.

### 2. Standing measures — `src/gfx/sprite.h:128`, `src/gfx/sprite.c:473`

`draw_raster` (`src/gfx/sprite.c:473`) first looks for the body's measures
in `c->standing`, a direct-mapped table: each body has exactly one slot,
picked by its key's hash (`standing_slot`, `src/gfx/sprite.c:380`). A hit
(`src/gfx/sprite.c:487`) copies ten numbers into the raster, and the
skeleton is not drawn at all. A miss draws it and stores the measures
(`src/gfx/sprite.c:515`):

```c
*st = (SpriteStanding){body_key, r->w, r->h, r->ox, r->oy,
                       r->x0, r->y0, r->x1, r->y1, r->shadow_rx, r->shadow_ry};
```

*C note:* `(SpriteStanding){...}` is a *compound literal*: a struct value
written in place, here assigned whole. The fields are filled in declaration
order.

To know what to keep, I read what the art path takes from the skeleton.
`art_raster` resets the inks, the box, the eye and `body_dx`; `keep()` then
reads the grid, the ground point and the shadow. Only those survive, so only
those are kept.

### 3. The tile table — `src/gfx/tiles.c:382`

```c
static uint8_t resolved[2][RESOLVE_SPAN][RESOLVE_SPAN]; /* [0]: size 16, [1]: 8 */
static bool resolved_ready[2];
```

`source_at` (`src/gfx/tiles.c:387`) fills a size's table on its first use,
then answers from it. Points outside the table fall back to `resolve`.

*C note:* `static` outside a function means "private to this file" and
"lives for the whole program, starting at zero". So `resolved_ready` starts
`false` without any setup code.

### 4. The cheaper tick — `src/brain_net.c:50`, `src/creature.c:727`

```c
const float *row = &g->g[net_hidden_weight(h, 0)];
float sum = row[NET_IN]; /* the bias */
for (int i = 0; i < NET_IN; i++) {
    sum += row[i] * in->v[i];
}
```

Before, each weight was `g->g[net_hidden_weight(h, i)]`. `net_hidden_weight`
lives in `genome.c`, so the compiler could not see that it is just
`base + i`. It made a real call for every weight and could not keep the loop
tight.

*C note:* the compiler optimises one `.c` file at a time. A function in
another file is a black box unless it is `static inline` in a header (or the
build uses link-time optimisation, which Veredas does not).

`round_half_away` (`src/creature.c:727`) replaces `lroundf`. `(int)a` cuts
the fraction off, and for a small non-negative float `a - (float)whole` is
exact, so comparing it with `0.5f` decides exactly as `lroundf` does.

### 5. The prelude thread — `src/preludethread.c`

The struct (`src/preludethread.h:68`) keeps the worker's things apart from
the shared ones; only the fields after "Under the lock" are shared.

- `worker` (`src/preludethread.c:43`) loops: read `stop` under the lock;
  tick without the lock; publish `tick` under the lock; about once a second
  `hand_over_copy` (`src/preludethread.c:17`). It never holds the lock
  during a tick, so the window never waits just to read progress.
- `hand_over_copy` makes the copy *without* the lock (30 ms), then swaps it
  in under the lock. An older copy the window never took is freed after the
  lock is let go (`src/preludethread.c:27`).
- `preludethread_end` (`src/preludethread.c:135`) sets `stop`, waits with
  `pthread_join` (`src/preludethread.c:145`; at most one tick), then moves the
  world over (`src/preludethread.c:151`).

In the window:

- `step_prelude` (`src/window/window_main.c:492`) starts the thread on a
  prelude's first frame, takes copies, takes the world at the end, and warms
  caches with the spare time (`src/window/window_main.c:515`).
- `hold_prelude` (`src/window/window_main.c:479`) is called before every key
  (`src/window/window_main.c:967`), before a new world (`:525`), before
  opening a world code (`:758`) and before leaving (`hold_prelude(&app)` near
  the end of `main`). Whatever the window then does to `a->sim`, it does to
  the real world, never to a copy that the next hand-over would throw away.

*C note:* `pthread_create(&t->thread, NULL, worker, t)` starts `worker(t)` on
a new thread. The `void *` argument is C's "pointer to anything", and the
worker casts it back to `PreludeThread *`. TypeScript has nothing quite like
this: a Web Worker shares no memory unless you use `SharedArrayBuffer`.
Here, both threads see the same struct, and the mutex is the only thing that
stops them from reading and writing it at the same moment.

### 6. Ticks after the picture — `src/window/window_main.c:2273`, `:2355`, `:2448`

The frame's `if / else if` chain now ends with
`else { ticks_after = true; }` (`:2355`) where it used to run the ticks.
After the picture has been shown, `if (ticks_after) run_frame_ticks(a);`
(`:2448`) runs them. Deciding inside the chain keeps every case as it was:
the setup screen, the prelude, a skip, a dead world, a pause. `--frames`
runs still tick inside the chain.

`run_due_ticks` (`src/window/window_main.c:548`) did not need to change. It
gives the ticks "the frame's end minus the rest of its work", and the rest
of the frame's work has all happened by now.

## C to know

- **Struct assignment moves ownership by convention, not by language.**
  `*shown = t->world;` copies the fields. Afterwards both structs point at
  the same memory, so the code then wipes `t` (`preludethread_init(t)`) to
  make sure only one owner frees it. C has no move semantics: you make
  them yourself.
- **Mutex discipline.** Lock, touch the shared fields, unlock, all within a
  few lines. Never do slow work (a tick, a 30 ms copy) while holding the
  lock.
- **`pthread_join` waits for a thread to end.** Until it returns, the
  worker may still be touching its struct, so nothing may be freed before
  it.
- **Float results depend on the order of operations.** That is why every
  speed-up here keeps the same expressions in the same order.
- **Function calls across `.c` files block optimisation.** Inside a hot loop
  that matters; elsewhere it doesn't.
- **`static` has two meanings.** Inside a function, it keeps a value between
  calls; outside, it makes a name private to the file. Both kinds start at
  zero.

## Problems met

- **Measuring.** `/usr/bin/time` is not installed here, so the timing script
  uses `date +%s%N` before and after. Each pair of builds ran at the same
  moment on cores 1 and 2, then swapped (scripts in the session's scratch
  folder, not committed).
- **Where the Far time went.** gprof showed about 30% of the frame building
  animal pictures, about 110 a frame. A test with a 4× larger cache showed
  most of those were genuinely new pictures, not ones thrown out too early.
  So the win had to come from making each picture cheaper. The bigger cache
  added the last 4%.
- **"A tick takes 37–40 ms".** Here a tick alone takes about 10 ms, while a
  frame holding a tick took about 30 ms. That fits the brief's number as
  "a frame with a tick in it", scaled to his machine. The tick and the
  picture building it causes were sharing a frame, and the reordering
  splits them.
- **The test run and a `git stash` collided.** I stashed to build the old
  code while `make test` was compiling in the background. The suite was
  run again from clean afterwards, and it passed.
- **A slip caught before pushing.** The first version of the reordering also
  stopped the world while the Sandbox setup screen was open after the start.
  Deciding inside the old `if / else` chain fixed that.

## Questions for you

1. The prelude now shows a copy that is up to a second old in the panel and
   the title, while the bar and the year line move every frame. Is that good
   enough for the first impression, or should the panel be hidden during the
   prelude?
2. After the prelude, on a machine 2× slower than this one, 1.3 late frames
   a second remain: half from building pictures after a tick, half from the
   tick itself. On your machine it should be well below that. If your
   `--frame-log` still shows late `tick` frames, the next step is the shown
   world's ticks on a thread. That is a bigger change: the window would draw
   from a copy, and every key that changes the world would have to wait for
   the worker. Do you want it before or after the playtest?
3. 117 MB at the peak at Far, 1080p (was 105). Fine for the playtest
   machines?

## Glossary

- **Prelude**: a new world's first years (2 by default), run fast before
  the world is shown, so it opens on animals that have already evolved.
- **Late frame**: a frame that starts more than 40 ms after the one before;
  at 30 frames a second each has 33 ms.
- **Worker thread**: a second line of execution in the same program, sharing
  its memory.
- **Mutex (lock)**: a token only one thread can hold at a time. Code that
  touches shared fields holds it.
- **Direct-mapped table**: a cache where each key has exactly one slot, so a
  newer key simply replaces an older one there.
- **Struct assignment move**: handing a struct's memory to another variable
  by copying its fields, then forgetting the old one.
- **Fingerprint**: `sim_checksum`, a hash of the whole world. If it is the
  same, the world unfolded the same.
- **Pinned picture**: a screenshot the tests compare byte for byte.
