# FIZZ on two cores: findings and a plan (P2.6)

2026-10-23. Research first; then the owner's decisions and the manager (core 1 not started yet): see "The owner's
decisions" and "What is built" below. Sources:
- ChoralRoot 0.26.1 (`ChoralRootFM1/.claude/worktrees/rel-0261-1b7a23`, its `docs/DUALCORE.md`) and the main checkout's
  git history;
- fm1-x0x (`../fm1-x0x`, fe6b0ae) and Melodee (`../melodee`, 16daf99), checked out locally;
- Felucca 1.0 (`../Felucca`);
- felucca-bsp and fm1-x0x on GitHub (fetched 2026-10-23).

## Why

The unit with P2.6 part 1, playing the first-boot demo: the audio interrupt takes 3,987 us of a 5,805 us half (69 %).
The voices take 2,849 us for 6 of them; effects in RAM saved only ~10 %. The plan has always been 8 + 8 voices across
the two cores (PLAN §11 step 6). ChoralRoot renders its bass part and a share of its chord voices on core 1 this way.

## Who wrote the second-core code, and under what licence

| code | where | written by | licence | reusable under MPL-2.0? |
| --- | --- | --- | --- | --- |
| felucca-bsp `chip/fm1_cpu1.h`, `.S` | fm1-base's submodule | Leo Kuroshita, from JieLi's SDK `EnableOtherCpu` | MPL-2.0 | yes, but it is "EXPERIMENTAL: compiles, has not run on a device". It has no mailbox validation, no fault path and no fallback |
| fm1-x0x `firmware/hal/fm1_cpu1.h`, `.S` (~200 lines; used by X0X, NuSaw, SPH-16, LOOM-8) | charlesvestal/fm1-x0x | Charles Vestal (all commits); design and fixes from Melodee (keremimo) and Jangada (zednaked) | GPL-3.0-only in fm1-x0x | **pending**: felucca-bsp issue #5 (2026-10-18) proposes this core as felucca-bsp's MPL-2.0 `chip/fm1_cpu1.*`. Charles licenses his part MPL; **keremimo granted** MPL for his parts on 2026-10-18 (incl. fixes and fallback logic); **zednaked has not answered** (the core-1 hold in `fm1_cstart`, the fault path that halts core 1, the job checks, the cache-line mailbox layout, the supply order) |
| Melodee `firmware/hal/fm1_multicore.h`, `fm1_core1_*.h`, `fm1_cache.h`, `fm1_power.h`, `main.c`'s start | keremimo/melodee | Kerem Kilic (all commits) | GPL-3.0-only | his contributions: MPL-2.0 for felucca-bsp by his comment on issue #5 |
| ChoralRoot `firmware/hal/fm1_cpu1.S` | ChoralRootFM1 | Michael P Jones (copied from fm1-x0x's; 10 lines of whitespace differ) and genkigenki (header-only commit 8221b7c) | GPL-3.0-only | it is Charles Vestal's file: through issue #5, not as ChoralRoot's |
| ChoralRoot `firmware/hal/fm1_cpu1.h` (326 lines) | ChoralRootFM1 | Michael P Jones, on fm1-x0x's (Charles Vestal) and Melodee's PR 21 port (Kerem Kilic); genkigenki headers | GPL-3.0-only | **mixed**: the owner's additions are his to relicense; the base is Charles's, the validation and fault path Kerem's (both now MPL-willing), and anything from Jangada needs zednaked |
| ChoralRoot `firmware/src/cr_cpu1.h` (706 lines: the job split, the budgets, the escalation, the persistent bar, the profile) | ChoralRootFM1 | Michael P Jones (15 commits), genkigenki (2, headers) | GPL-3.0-only in ChoralRoot | **yes: relicensed MPL-2.0 by its author for FIZZ, 2026-10-23** (the owner's decision below). Its design follows Melodee PR 21 (credited); its code is ChoralRoot's. Only the owner's lines: genkigenki's two commits touched headers only, and nothing of `hal/fm1_cpu1.*` comes with it |
| Felucca 1.0 | hugelton/Felucca | Leo Kuroshita | GPL-3.0-only | has no second-core code |

**Conclusion.** The right base is the shared core that felucca-bsp issue #5 is turning into felucca-bsp's MPL-2.0
`chip/fm1_cpu1.*`: X0X's, with Melodee's and Jangada's fixes. It runs on four firmwares. One answer is missing,
zednaked's. Until it comes there are two choices for the owner:
- **(a)** wait for issue #5 and use felucca-bsp's file (fm1-base would then add `plat_cpu2_*`, as DESIGN.md says);
- **(b)** write FIZZ's start from Charles's and Kerem's parts only, which is the start, the job post and wait, the
  check word and the fallback, leaving out the Jangada-specific pieces (the `fm1_cstart` hold, the halt-on-fault
  record, the cache-line layout).

  The `fm1_cstart` hold matters: fm1-base already calls `APP_CSTART` at the very start for exactly this ("a second core
  a reset left running, held"), so FIZZ would write its own hold.

I recommend (a), with (b) only if issue #5 stalls. The owner's ChoralRoot `cr_cpu1.h` can be relicensed by him, but
FIZZ needs only its ideas: one job a block, the 8 ms stall limit, the bar.

## The plan: FIZZ's voices 8 + 8

**Shape (as ChoralRoot's).** Each 32-frame control block in the audio interrupt (eight a 256-frame half) runs these
steps:
1. **Core 0, shared state first:**
   - the sequencer's events of the block, the voice allocation and steals;
   - every voice's per-block shared updates (the owners' FREE LFO phases, glides, the pool's `block_begin`);
   - nothing that core 1 reads after this point changes until the join.
2. **Core 0 posts one job:** "voices 8..15 into bus B" (`fm1_cpu1_run(fn, arg)`).
3. **Core 0 renders voices 0..7 into bus A,** then waits (`fm1_cpu1_wait`, 8 ms at most).
4. **Core 0 sums A then B in a fixed order,** then runs the effects (reverb, delay, chorus) and the output. The result
   is the samples of one core, bit for bit.
5. **Failure:**
   - **Stall, bad job or fault:** core 1 is held and barred for the session, and core 0 renders 8..15 itself in that
     block (late once, then single-core: the shed works on 16 again).
   - **Persistent bar:** a `.noinit` bar keeps core 1 off after a crash or watchdog reset that happened while it ran
     (ChoralRoot's, X0X's).
   - **Packages:** ship a Standard and a Single-core package, as ChoralRoot and Melodee do.

**The handshake.**
- **Cost:** eight post-and-wait pairs a half: a few us each (csync, a sequence number, a check word).
- **Effects on core 1 too?** Possible later, but it needs another job: the effects would run after the join, the
  reverb on core 1 while core 0 runs the chorus and the delay. Only if the voices' split leaves core 0 the busier one.

**Flash and XIP rules.**
- **Core 1's idle loop runs from RAM** (`.c1_text`, the window fm1-base's build exempts).
- **Its jobs run from the flash.** That is safe because core 1 works only while core 0 waits inside the audio
  interrupt, and core 0 erases or programs the flash only with interrupts off: never while core 1 runs.
  - FIZZ's store keeps to this: `qd_persist_step` on the main loop; each flash operation is fm1-base's, with the
    interrupts off around it.
  - The blob writer (`app_usb_cmd`) runs with the sound off.
- **The caches are shared:** the two cores fetching `quad_render` from the flash at once contend for the XIP cache.
  So `quad_render` in RAM (P2.6 part 1's open item) matters more with two cores, not less.

**Memory.**

| item | size | where |
| --- | --- | --- |
| core 1's stack | 2,816 B (X0X's, ChoralRoot's) | the pool or NOINIT |
| the mailbox | 64-byte aligned, each core's words on their own cache line | RAM |
| bus B (l, r and the three sends for 32 frames) | 5 x 32 x 4 = 640 B | RAM |
| core 1's loop | well under 1 KB | `.c1_text` |
| **total** | **~3.6 KB** | RAM has 2,896 B free and the pool 3,808 above its spare: it needs the core's savings below, or the stack in NOINIT (3,032 B free) |

**Supplies.**
- fm1-base already raises VDC14 and SYSVDD to M-VAVE's levels at the start (`main_fm1.c`: `fm1_vdc14_raise(7)`,
  `fm1_sysvdd_raise(15)`).
- ChoralRoot's DUALCORE.md "Reliability" lists the units that misread with both cores and the levels that cured them
  (Jangada's runs: 15 / 5 clean for 15 minutes).
- **Check with the issue #5 owners whether fm1-base's levels are the dual-core-safe ones before core 1 starts on the
  owner's unit.** The owner's unit ran ChoralRoot dual core from 0.16.

**What the FIZZ agent must provide (core API).**
1. A block in three parts (or the same split behind one function the port gives a job-runner to):
   - `qd_app_block_begin(a)`: the events, allocation and steals, the per-block shared updates of every voice;
   - `qd_app_block_voices(a, v0, v1, qd_bus_t *bus)`: voices `[v0, v1)` rendered into `bus` (cleared first). It
     writes nothing outside those voices' own state and the bus: no pool counters, no owner state, no arena, no flash;
     its stack stays under ~2 KB (core 1's stack is 2,816 B; today's audio stack measures 2,256 B in all);
   - `qd_app_block_end(a, bus A, bus B, out)`: sums the buses in a fixed order, adds the metronome, runs the
     effects and the output.

   `qd_app_audio` keeps working as today by calling the three in turn (the host, FM1VST, the simulator).
2. **Voices balanced between the halves of the pool:** the allocator spreads new notes over 0..7 and 8..15, so each
   core gets about half (ChoralRoot gives each part its own budget instead).
3. **A test:** the split rendered as two calls equals `qd_app_audio`'s samples bit for bit (as ChoralRoot's
   `cr_c1_thread_test`), and core 1's half run before or after core 0's gives the same.

**What the port does (fm1-base side), once the core 1 code is settled.**
1. **The start:** `APP_CSTART` (core 1 held at boot), then core 1 started at `app_init`, before the sound. On a 20 ms
   timeout it is held and the unit runs single-core.
2. **The job runner** in `app_render`.
3. **The bar.**
4. **Stats:** core 1 state (on, failed, gave up, barred), misreads, stalls, the per-half wait, and each core's voice
   time.
5. **A build switch** for the single-core package (`FZ_SINGLE_CORE=1`).
6. **The simulator** runs core 1's job inline (as ChoralRoot's emulator), and the host tests compare.

**Expected gain.** Voices are ~70 % of the interrupt now (2,849 of 3,987 us for 6 voices). Halved, the interrupt
playing that demo would be ~2,560 us (44 %), before the core's own per-voice work and `quad_render` in RAM. Sixteen
voices need both.

## The owner's decisions (2026-10-23)

1. **Core 1's start waits for felucca-bsp issue #5**: FIZZ uses felucca-bsp's MPL-2.0 `chip/fm1_cpu1.*` when it lands
   (choice (a) above). Nothing of fm1-x0x's, Melodee's, Jangada's or ChoralRoot's `hal/fm1_cpu1.*` /
   `fm1_multicore.h` is copied into FIZZ, before or after.
2. **ChoralRoot's `firmware/src/cr_cpu1.h` is relicensed MPL-2.0 for FIZZ by its author** (Michael P Jones, the
   owner). Its design and its code may be reused: the job split, the budgets, the escalation, the persistent "core 1
   off" record and bar, the stage profile.

### LICENSING note (for the FIZZ agent and anyone copying from ChoralRoot)

- What may be copied: **the owner's lines of ChoralRoot's `firmware/src/cr_cpu1.h`**, nothing else of ChoralRoot's
  second-core code. Never `hal/fm1_cpu1.h` / `.S` (Charles Vestal's, with Melodee's and Jangada's parts: GPL-3.0 until
  felucca-bsp issue #5 makes it MPL-2.0 there, and then it comes from felucca-bsp, not from ChoralRoot), never
  Melodee's `fm1_multicore.h` / `fm1_core1_*.h`, never Jangada's code.
- Every file holding such a copy starts with `SPDX-License-Identifier: MPL-2.0` and names where the copy is from:
  > from ChoralRoot's cr_cpu1.h, Michael P Jones, relicensed MPL-2.0 by its author for FIZZ, 2026-10-23
  (with the functions it took, when it is part of a file). `cr_cpu1.h` credits its design to Melodee PR 21 (Kerem
  Kilic); keep that credit beside the line.
- Copy from the owner's ChoralRoot checkout (`ChoralRootFM1/firmware/src/cr_cpu1.h`, or the 0.26.1 worktree), not from
  a fork.
- FIZZ's own files that already do this: `firmware/app/fz_dual.h`, `fz_dual.c` (the escalation `cr_c1_assess` /
  `cr_c1_give_up`, the record's decision `cr_c1_bar_decide` / `cr_c1_bar_boot`, the power-on gating
  `cr_c1_power_on` / `cr_c1_boot`, rewritten for FIZZ's names). The record itself (`fz_c1_bar_t`, its magic) is FIZZ's
  own, not `hal/fm1_cpu1.h`'s `struct fm1_c1_bar`.

## What is built (core 1 not started)

**The manager** (`app/fz_dual.h`, `app/fz_dual.c`), in the unity build (app.c includes it) and in a host test:
- **A back end** (`fz_c1_backend_t`: start, run a job, wait, hold, its misread and fault counters). Today the
  **simulated** core 1: core 0 runs the job through the same mailbox a real core will read. `FZ_C1_BACKEND` in app.c
  picks it.
- **The mailbox**: 64-byte aligned, core 0's request on one cache line (sequence, job, argument, a check word over the
  three), core 1's answer on the next (the sequence done, the status, the counters). A request with a wrong check
  word is refused and counted, never run. `fz_c1_serve()` is core 1's side (a FIZZ idle loop or the simulator).
- **Each block**: `posted = fz_dual_post(job, arg)`; core 0's own part; `fz_dual_join(posted, job, arg)`. The join
  waits, and on a refusal, a stall or a fault runs the job on core 0 in that same block, so the block is never short.
- **The escalation** (ChoralRoot's): a first misread is forgiven, a second (a later block) or any stall or fault gives
  core 1 up for the session and sets the record.
- **The record** (`fz_c1_bar_t` in `.noinit`, 12 B): a give-up, a crash beside core 1 (`APP_FAULT` -> `app_fault` ->
  `fz_dual_crashed`, only with a real back end), or a watchdog reset while core 1 ran keep core 1 off the next boot.
  A power-on clears it.
- **Power-on** (`app_init`, before the sound): not in the single-core package, not when the setting is off, not after a
  failed boot (fm1-base's `bootguard.failed`: a crash or the watchdog in the 30 s of the boot before: one core this
  boot, nothing written), not when the record bars it; otherwise the back end starts, and if it does not answer it is
  held and the session is single-core ("failed").
- **Stats**: a sixth line on `7D 46 50` (`fizz_xdata.py stats`): `c1=` on / off / failed / gave up / single, the back
  end, why it is barred, the start time, the jobs handed over, the jobs core 0 ran instead, stalls, refusals,
  misreads, the longest wait.
- **The build switch**: `firmware/build.py --single-core` (also with `--host`, `--web`, `--release`) builds with
  `FZ_SINGLE_CORE=1` into `fizz-1core.fwsc` (`fizz-1core_host`); core 1's start is compiled out and every job runs on
  core 0. The Standard package is the plain build.
- **The test** (`app/tests/dual_test.c`, both builds): the power-on rules and the record (garbage, setting off, safe
  mode, a counted boot, no answer, a watchdog hang, a crash, a give-up and the boots after them), and 400 blocks of
  16 stand-in voices rendered 0..7 on core 0 and 8..15 as the job: bit for bit one core's samples with the job run
  first, run last, refused once, refused twice, and stalled in the middle.

  ```
  cc -O2 -Wall -Iapp app/tests/dual_test.c -o /tmp/dual_test && /tmp/dual_test
  cc -O2 -Wall -Iapp -DFZ_SINGLE_CORE=1 app/tests/dual_test.c -o /tmp/dual_test1 && /tmp/dual_test1
  ```

**Cost so far**: RAM 300 B (the mailbox's 128 B and its alignment, the counters), NOINIT 12 B, flash ~1 KB.
Nothing is handed over yet: no block calls `fz_dual_post` until FIZZ's core has the split render.

### Waiting for

1. **The FIZZ agent's split render** (`qd_app_block_begin` / `qd_app_block_voices(a, v0, v1, bus)` /
   `qd_app_block_end`) and its per-voice and effects profiling. Then, in `fz_audio`'s place: per block, begin; post
   "voices 8..15 into bus B"; voices 0..7 into bus A; join; end. Proven on the host first: the simulated core 1
   (`fz_c1_sim_late` 0 and 1) gives `qd_app_audio`'s samples bit for bit, and on the unit with the simulated back end
   (both halves on core 0: the handshake's cost alone, ~0 voices' gain).
2. **felucca-bsp issue #5** (`chip/fm1_cpu1.*`, MPL-2.0). Then FIZZ adds `fz_c1_felucca` (a back end over its start,
   run, wait, hold and counters, with the PC guard opened around the start as ChoralRoot does), `app_cstart` with its
   hold (fm1-base's `APP_CSTART`: core 1 left running by a reset, held before the RAM is cleared), core 1's stack
   (2,816 B: NOINIT has ~3 KB free) and sets `FZ_C1_BACKEND`. fm1-base may by then have `plat_cpu2_*` (its
   DESIGN.md), which would replace the back end's direct calls.
3. **The supplies**: whether fm1-base's VDC14 / SYSVDD levels are the dual-core-safe ones (above), before core 1 runs
   on the owner's unit.
