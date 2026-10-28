# FIZZ on fm1-base: the real budget (P2.2)

2026-10-23. fm1-base `ced9443` (felucca-bsp `4bd6d0b`), FIZZ's core as of FMToneSeq `508bfea` plus the MPL-2.0
relicensing in progress (comments only). Every number below is from a real link with JieLi's pi32v2 clang 4.0.1
(`firmware/build.py`, the map `firmware/app/build/fizz.map`, the report `firmware/app/build/fizz-size.txt`), unless
marked E (an estimate).

## How it is built

| what | command | output |
| --- | --- | --- |
| the unit's package | `python3 firmware/build.py` | `firmware/app/build/fizz.fwsc` (611,354 B, identity `FM-1_4600`), `fizz-xdata.bin` (69,572 B), `fizz-size.txt` |
| the host simulator | `python3 firmware/build.py --host` | `firmware/app/build/host/fizz_host SCRIPT OUTDIR` |
| the browser emulator | `source ~/emsdk/emsdk_env.sh; python3 firmware/build.py --web` | `firmware/app/build/emu/` (678 KB wasm) |
| the simulator's tests | `firmware/app/build/host/fizz_host firmware/app/tests/boot.txt OUT` | boot, idle, step view, the demo played, SysEx |
| the screens against FIZZ's own | `python3 firmware/tools/compare_frames.py` | 4 of 4 frames identical, pixel for pixel |

`firmware/build.py` runs fm1-base's `tools/build.py` unchanged, with six small patches of its module (its docstring
lists them): the toolchain container sees the whole repository; newlib's headers and `libm` / `libc` /
`libcompiler_rt` (r3) are linked; `firmware/app/fizz.ld` replaces `device/app.ld`; the data blob is cut out and stamped;
soft doubles are a note, not a failure; the browser build uses WebAssembly's own setjmp.

## The platform's own cost (the template, measured)

| fm1-base's template, `--no-ble` | flash (image) | RAM (.data + .bss) | pool |
| --- | --- | --- | --- |
| USB audio in + out (fm1-base's default) | 81,012 | 27,844 | 69,384 |
| USB audio out off | 80,100 | 19,396 | 69,384 |
| USB audio off | 78,468 | 17,076 | 69,384 |
| USB audio off, TRS MIDI off | 78,032 | 16,916 | 69,384 |
| screen mirroring on (`mirror = true`) | +3,320 | +5,480 | +9,800 |

The template's pool is its 240 x 144 canvas (69,120). USB audio in costs 2,320 B of RAM, out 8,448 more.

## FIZZ's build: USB only, no USB audio, no mirroring

Where everything went (bytes):

| region | size | platform (fm1-base + felucca-bsp + the port's app.c) | FIZZ's core + plutovg + fz_port.c | C library | free |
| --- | --- | --- | --- | --- | --- |
| app slot (XIP image) | 581,564 | 54,214 + 3,130 RAM code | 434,934 | 19,489 | **69,640** |
| RAM (.data + .bss) | 98,304 | 14,701 | 70,748 | 0 (impure data is in .data) | **12,820** |
| pool | 344,064 | 11,524 (fm1-base's canvas, 240 x 24) | 323,548 | 0 | **8,992** (fm1-base demands 8,192) |
| RAM code region (after the code) | 32,768 | 3,130 code + 8,192 (the sound's ring) | 16,418 | | **5,024** |
| NOINIT (after fm1-base's records) | 15,696 | 484 | 12,140 | | **3,048** |
| data blob, PLAT_XDATA1 | 94,208 | | 69,572 + 12 trailer | | **24,624** |
| data region 0x97000..0xDFFFF (stores) | 299,008 | (the panel's calibration is at 0xFC000, not here) | not used yet (the store is off: below) | | 299,008 |

FIZZ's static data needs **422,854 B** (its own BUDGET-LINE says 424,020 from the arm stand-in link: they agree).
RAM and the pool alone cannot hold it: with the platform's 34,901 and the pool's 8,192 spare they leave FIZZ
**399,275 B, 23,579 short**. It fits because `fizz.ld` also uses the RAM code region's spare (29,638 B: FIZZ uses no RAM
code yet) and NOINIT's (15,212 B), unit by unit:

| FIZZ's unit | its .bss | placed in |
| --- | --- | --- |
| `fz_port.c` (the one `qd_app_t`) | 209,820 | pool |
| `qd_fx.c` (delay 64 KB, reverb, chorus) | 113,728 | pool |
| `qd_gfx_plutovg.c` (the 40 KB arena, the 20-row ARGB strip) | 60,572 | RAM |
| `quad_voice.c` (8 comb lines and the voice tables) | 16,418 | RAM code region |
| `view.c` | 9,284 | NOINIT |
| `app.c` (FIZZ's) | 8,252 | RAM |
| `ui_idle.c`, `ui_keys.c` | 2,856 | NOINIT |
| the rest (ctl_latch, player, sounds ..) | 1,924 | RAM |

`app.c` (the port's) clears the RAM code region's and NOINIT's parts before FIZZ starts; crt0 clears only .bss and the
pool. The pool is the tightest: 800 B above fm1-base's demanded spare.

## The numbers `core/qd_config.h` and `tools/budget` should use

Today's `tools/budget/budget.py` estimates the platform from TransitFM1: RAM 425,032 B for FIZZ, flash 468,553 B. On
fm1-base, measured:

| | today's (TransitFM1, E) | fm1-base, measured | FIZZ now | FIZZ is |
| --- | --- | --- | --- | --- |
| `FM1_RAM` (data RAM the chip gives) | 458,032 (RAM + POOL + NOINIT) | **487,218** = RAM 98,304 + POOL 344,064 + NOINIT 15,212 + the RAM code region's spare 29,638 | | |
| `PLATFORM_RAM` | 25,000 | **34,901** (fm1-base 14,701 in RAM and 11,524 canvas and 484 NOINIT; the port's ring 8,192, its other buffers ~2,200 are inside the 14,701) | | |
| `HEADROOM_RAM` | 8,000 | **8,192** (fm1-base's build refuses less in the pool) | | |
| **`BUDGET_RAM`** | 425,032 | **444,125** | 422,854 | **21,271 under** |
| with ~21 KB of the RAM code region given to code (P2.5: `quad_render` + `quad_ext`, E) | | **~423,100** | 422,854 | ~250 under: no margin; P2.5 needs RAM back first |
| `APP_SLOT` | 573,440 | **581,564** (fm1-base's XIP region, 0x8DFBC) | | |
| `PLATFORM_FLASH` | 55,000 | **57,344** (54,214 + 3,130 RAM code; fm1-base's fonts S, B, M ~29 KB, E) | | |
| `MARGIN_FLASH` | 8,192 | 8,192 | | |
| **flash for FIZZ + C library** | 468,553 | **516,028** | 454,423 (434,934 + 19,489) | **61,605 under** |
| **`XDATA_BUDGET`** | 98,304 (24 sectors of the data region) | **94,196** (PLAT_XDATA1 0xE5000..0xFBFFF, less the 12-byte trailer) | 69,572 | **24,624 under** |
| projects' flash | ~292 KiB | **299,008** (0x97000..0xDFFFF, 73 sectors, all FIZZ's) | 0 (store off) | |

With USB audio in and out the same build links (checked): RAM 96,236 of 98,304, `PLATFORM_RAM` 45,653, so
`BUDGET_RAM` 433,373 (10,519 under). USB audio in alone: +2,304 RAM, +1,504 flash.

The budget gate should also check each region, not only the sum: FIZZ's units are placed whole (no `-fdata-sections`
in JieLi's clang), and the pool's 800 spare bytes are what a growing `qd_app_t` or effect buffer eats first.

## What can be turned off to make room

| | saves | the cost |
| --- | --- | --- |
| USB audio (off in this build) | 10,768 RAM, 2,544 flash | the computer can't record FIZZ over the cable (in) or play through the FM-1 (out). In alone is cheap: 2,320 RAM |
| screen mirroring (off) | 5,480 RAM, 9,800 pool, 3,320 flash | no screen on the computer (RemoteFM1 does this another way) |
| the screen reader | small (always in fm1-base: announce.c and the words link; not measured apart) | not separable without changing fm1-base |
| fm1-base's canvas (240 x 24 here, 240 x 144 in the template) | 11,524 pool at most | fm1-base's own screens (boot, crash, update, calibration) need ~16..24 rows: could share FIZZ's strip surface |
| fm1-base's fonts (S, B, M) | ~29 KB flash (E) | `APP_SYS_TEXT`: FIZZ would draw the crash and update screens itself; not with plutovg in a crash handler, so a small bitmap face (~4 KB) would be needed |
| `APP_BIG_FONT FONT_M` (done) | 19,776 flash (FONT_L) | the splash's name in 24 px |
| the sound's ring 8,192 → 4,096 | 4,096 (RAM code region) | the ring's maximum depth 40 → 18 ms (see the risks) |
| delay 64 KB → 32 KB (the owner chose 64) | 32,768 pool | half the longest delay |
| comb lines 8 → 4 | 8,192 | E2's comb on fewer voices |

## CPU

The host simulator says nothing useful about the unit's CPU: it runs the platform's audio interrupt, but in this port
the interrupt only copies the ring (FIZZ renders on the main loop). The firmware measures itself instead:
`python3 firmware/tools/fizz_xdata.py stats` reads `frame_avg_us`, `frame_max_us`, `strip_max_us` (one 20-row strip),
`blk32_avg_us` / `blk32_max_us` (one 32-frame audio block: 726 us of sound), `underruns`, `ring_min`, `ring_target`,
`cpu_isr`, `xruns` from the unit (`--reset` starts them again). The audit's estimates stand until then: the step view
~44 ms a frame and the idle screen ~148 ms with plutovg (E, x85 of the Mac), FM TONE ~176 us a voice a 128-frame half.

## P2.4: the sound on its own, MASTER, the drawing's profile (2026-10-23)

The unit's first numbers (P2.2, the idle screen): a frame 0.56 s (max 1.07 s), a 32-frame audio block 57 us, 1,463
underruns in 15 s: the sound was made on the main loop and every frame starved it.

**The sound now comes from the audio interrupt** (`app_render`, 256 frames a half, 5.8 ms), as ChoralRoot makes it:
FIZZ's sequencer, voices and effects, and MIDI's real-time messages (clock, START, CONTINUE, STOP). The main loop does
the rest: the panel, MIDI's channel messages (queued by the interrupt with their time), FIZZ's clock, SysEx, each call
with the interrupts off around it (`plat_lock`, the clock 4 ms at a time). Those calls run between frames *and between
the strips of a frame*, each press after FIZZ's clock has been given up to the press's own time (a hold stays a hold
however slow the frame). The drawing reads FIZZ's state with the interrupts on. A slow frame can no longer make the
sound late; the longest a main-loop call holds the interrupt is in the counters (`lock_max_us`, and fm1-base's own
`fm1_remote.py log`). The ring is gone (8 KB of the RAM code region back).

**MASTER** is fm1-base's: the render's gain is `APP_GAIN(pot)` (the square of the position, Q12), as in the template,
for the speaker, the headphones and USB audio alike. FIZZ's own volume stays at its default. Host test: MASTER at 4096
gives −8.0 dBFS peak, at 600 −41.5 dBFS.

**Frames**: 30 a second for 1.5 s after a press or a turn, 15 while playing, 10 on the idle screen, 4 otherwise. Only
the rows whose pixels changed are sent (as before). Skipping a frame whose content did not change needs the core to
say so (below).

**The FPU flags are in effect** for FIZZ's units: `firmware/app/build/o2/` objects are compiled with fm1-base's
`-mcpu=r3 -mfprev1 -ffp-contract=off` (its `build_app` flags, which the o2 units share), the link has no single-float
soft routines (`__addsf3`, `__mulsf3`, `__divsf3` .. are absent from `fizz.dis`), and plutovg's float code is FPU
instructions (`plutovg_matrix_multiply`: `r7 = r8 * r3 (f)`, `r11 = r4 + r7 (f)`). What is soft is the doubles
(`__adddf3` ..: the list in fizz-size.txt) and the 64-bit divisions (`__divdi3`, `__udivdi3`, `__moddi3`: FreeType's
`PVG_FT_MulDiv` / `DivFix` in plutovg's stroker and rasteriser, the sequencer).

**The profile** (`python3 firmware/tools/fizz_xdata.py stats`, `--reset` to start again), three lines:

| line | fields |
| --- | --- |
| 1 | `isr_n`, `isr_avg_us` / `isr_max_us` (one half: 5,805 us of sound), `cpu_isr`, `xruns` (fm1-base's late halves), `lock_max_us` / `lock_kind` (1 a press, 2 the clock, 3 a knob, 4 MIDI, 5 SysEx in, 6 SysEx out), `midi_drops`, `frames`, `frame_avg_us`, `frame_max_us` |
| 2 | a frame's average by phase (us): `raster` (plutovg_rasterize: outlines, strokes, flattening, spans), `blend` (painting the spans), `glyph` (glyph outlines from the TTF), `rgb565` (the strip's conversion), `lcd` (row hashes, blits, the DMA wait), `input` (the main loop's calls between strips), `isr_in_frame` (the audio interrupt inside the frame), `rest` (the core's own: its screens' logic, the paths built, plutovg's state); and the calls a frame: `rasters`, `blends`, `glyphs` |
| 3 | by screen (mode/view: idle, step, keys, tracks / roll, steps, edit): frames, average and longest frame (us) |

On the Mac (the host simulator, -Os, the step view while playing): a frame 1.48 ms, of which raster 1.13 ms (304
outlines a frame: every path is outlined again for each of the 12 strips), rest 0.26, blend 0.02, glyphs 0.015; the
idle screen 2.4 ms. The unit's idle screen took 562 ms: ~240x, not the audit's ~85x. The soft 64-bit divisions and
doubles in plutovg's stroker and rasteriser are the first suspects; the unit's line 2 will say.

**Core changes this asks for** (none made):
1. *A frame worth drawing*: `int qd_app_frame_changed(const qd_app_t *a)` (or a counter the state's every change
   bumps, the clock's animations included), so the port can skip frames whose picture would be the same.
2. *Taller strips*: `QD_STRIP_DEV` 20 → 40 (or 60) in `core/qd_config.h`. Each strip draws the whole screen's logic
   and outlines every path again (304 outlines a frame at 20 rows); 40 rows halves that for +19,200 B of RAM (RAM has
   8,532 free, the RAM code region 13,216 since the ring went: it fits).
3. *MIDI notes in the audio interrupt*: `qd_app_midi` split into the part that sounds (the player's key on and off,
   the controllers: audio-safe, no undo, no recording) and the part for the UI (recording into a held step, undo,
   the preview), so notes from a keyboard sound at once whatever the screen does. Today they wait for the main loop
   (up to a strip).
4. *The store's flush outside the clock*: when the store goes on (P2.6), `qd_persist_ms`'s erases and writes must not
   run inside `qd_app_tick` while the interrupts are off around it: a `qd_persist_step()` the port calls on its own.
5. *(Still open from P2.2)*: the stores sized to the data region; the doubles; `qd_arena.c`'s `free` / `realloc`
   fallbacks on the unit.

## P2.5: the fast drawing, the core's seams, the store on (2026-10-23)

The unit's P2.4 numbers: 0 xruns, the interrupt 538 us a 5.8 ms half (9 %), the longest lock 1.9 ms; the frames slow
(idle 660 ms, the step view playing 284 ms, mostly plutovg's rasteriser).

**The drawing is the fast one** (`core/gfx/qd_gfx_fast.c`): fm1.toml's gen line says `--gfx fast`, and
`tools/gen_units.py` then builds what FIZZ's `tools/Makefile` builds with `GFX=fast` (`CORE_SRC`, `DEV_PVG_SRC`: no
plutovg) and defines `QD_GFX_FAST` for every unit of FIZZ's (`build/gen/fz_gfx.h`, through `fizz_prefix.h`). `--gfx
plutovg` still builds, but **with 40-row strips plutovg no longer fits the unit's RAM** (its unit's .bss is 85 KB, the
RAM region has 79 KB after the platform's: 16 KB over). `compare_frames.py` builds render_frames with the same drawing:
the simulator's frames still equal it pixel for pixel.

**Sizes** (fast drawing, USB only): image 481,904 of 581,564 (99,660 free); RAM 87,064 of 98,304 (11,240 free); pool
335,616 of 344,064 (8,448 free, fm1-base demands 8,192); the RAM code region 4,964 free (FIZZ's voices and its app.c's
data there now); NOINIT 3,032 free; the blob 84,116 of 94,196 (flash 0xE5000..0xF9894, 21 sectors of PLAT_XDATA1,
none of the store's). FIZZ's static data 428,504 B. No double left anywhere: fm1-base's soft-double check is back
on (the exemption is gone, and fz_libc.c has no %f and no atof).

**The seams adopted:**
- *Frames*: looked at 30 a second for 1.5 s after a touch, 20 while playing, 15 on the idle screen, 10 otherwise, and
  drawn only when `qd_app_frame_changed` says the picture would differ (`checks`, `skipped`, `check_avg_us` in the
  stats).
- *MIDI*: `qd_app_midi_play` in the audio interrupt (a note sounds at once, whatever the screen does),
  `qd_app_midi_ui` on the main loop with the message's time, in the same order.
- *The store is on* (`FZ_FLASH 1`): `qd_persist_attach` on the data region (offset 0 = 0x97000, 73 sectors),
  `qd_persist_own_step(a, 1)`, and `qd_persist_step` every 15 ms on the main loop, between frames (never between strips:
  it borrows the drawing's arena) and outside the port's locks (the flash driver masks the interrupts a 256-byte page
  at a time, ~1 ms, inside a half's slack). The store erases only when FIZZ is quiet (stopped, silent, untouched 10 s);
  an erase masks everything for ~18 ms, and fm1-base silences the DMA meanwhile, so `xruns` counts a few late halves
  then, in silence. The first quiet spells after the first boot erase what ChoralRoot or RemoteFM1 left in the region.
  `store_steps`, `store_avg_us`, `store_max_us` in the stats. Host test: the selected track written by itself and back
  after a restart.
- *`qd_app_show` / `qd_app_seed`*: the simulator's `fzshow MODE VIEW` and `fzseed N` (compare.txt uses `fzshow 0 0`).
- *The profile* times the fast drawing's texts (`qd_text`) and shapes (`qd_line` .. `qd_fill_rect`, `qd_path_*`),
  the RGB565 conversion, the LCD, the input; `isr_loads` counts patterns the sound had to load inside the interrupt
  (the prefetch's misses: it must stay 0, see below). On the Mac (simulator, -Os): a frame 0.17 ms (plutovg's 1.08),
  shapes 0.10, text 0.01, 538 shapes and 28 texts a frame.

**What remains to watch (core):** the store can be read and written from inside the audio interrupt if the sequencer
asks for a pattern the main loop's prefetch has not brought: `qd_proj_pat` then loads it there, and saves the one it
evicts (a synchronous store write in the interrupt, concurrent with the main loop's own steps). `isr_loads` shows it;
the host tests (a pattern change while playing) keep it at 0. A core guarantee would be better: the audio path never
pages (a miss plays the pattern's default or nothing, counted), and an eviction's save queued for the store's step
instead of written at once. Today a changed pattern leaving RAM during the main loop's prefetch is written inside the
port's lock, and the flash driver turns the interrupts back on after each page (`irq_restore` is an unconditional
`sti`): the sound then runs in the middle of that main-loop call (a torn read at worst: the evicted slot is one the
sequencer does not use).

## P2.5b: a safe start, MASTER, the store's recovery (2026-10-23)

The unit after P2.5 (30a27b9): the speaker at full level with MASTER doing nothing, a garbled sound, the audio
interrupt at ~90 %. The overload was the core's (a pattern out of RAM re-blanked 32 times a half: fixed in bcc550f).
With the interrupt near 100 % fm1-base's main loop, which reads and smooths the MASTER pot (`master_pos`), ran a pass
in seconds, so MASTER followed the knob seconds late or not at all; and the late halves (xruns) were the "missing
slices". The port's output path is fm1-base's own (render_block: `APP_GAIN(master_pos)` into `app_render`, as the
template), and the simulator checks it (MASTER 4096: −14.1 dBFS peak, 600: −47.5).

What the port now does against it:
- **A safe start**: the sound ramps from silence to MASTER's level over 0.5 s whenever it starts (the boot, after an
  update's screens), and the gain glides across each half from the last one's (no step when MASTER moves).
- **ChoralRoot's ceiling**: 16 bits to 24 with ChoralRoot's −6 dBFS headroom (its `OUT_SHIFT 7`), 6 dB under P2.4's.
- **A guard**: 8 halves in a row over 80 % of their time (4.6 ms of 5.8) let every voice go (all sound off on the 16
  channels); `over_halves` and `sheds` count it.
- **The store's recovery**: a region with nothing of FIZZ's in it and something else written is erased at the first
  screen before the store is mounted ("FIZZ: CLEARING ITS STORE", ~1.3 s, once); **SAVE held at power-on is safe mode** (P2.14; REC held is now fm1-base's update mode)
  (the store not mounted: nothing read or written); `F0 7D 46 50 57 49 50 45 F7` ("WIPE",
  `fizz_xdata.py wipe`) erases the region and starts FIZZ again on it without a restart. The watchdog is fed at each
  flash operation of the store's long flows (project load / new / copy, migrate).
- **The stats** add line 4 (inside the interrupt by part: sequencer, voices, effects, the rest; voices sounding,
  `pat_miss`, `snd_miss`, `sw_late`, `snd_uncached`, the stores' sectors used / blank / to erase, keys, the pages)
  and line 5 (the guard, MIDI and SysEx in, TIMER5's longest run and gap, fm1-base's longest lock and where).
- The simulator (`tests/store.txt`): a region of noise, then of zeros, cleared and used; the wipe; safe mode. The
  core's own handling of a foreign region (`fznowipe`) was also measured: no extra cost in the sound, the sectors
  erased once quiet.
- fm1-base's canvas is 240 x 16 now (`APP_BIG_FONT FONT_B`): the pool was 32 B short after the core's sound scratch.

## P2.6 (device side, first part): audio code in RAM, the shed, the remote, the boot lock (2026-10-23)

The unit with P2.5b, playing the first-boot demo: the interrupt 3.8 ms a half on average (59 %), 98 xruns; by part
voices 2,546 us with 4..7 voices, effects 856, sequencer 301; idle voices 392 us with none sounding. Against the host
(idle voices ~2 us a half) the unit is ~200x slower, not the ~85x the audit calibrated on ChoralRoot's code: the
audio code runs from the flash through the XIP cache, and the fast drawing's code (in the flash too) runs between the
halves and evicts it.

- **Audio code in RAM**: the effects' unit (qd_fx.c: the reverb, the delay, the chorus, the output) and the mix
  (mix.c), whole, 11.9 KB, in the RAM code region (fizz.ld change 3). FIZZ's units are compiled for long calls
  (`-pi32v2-large-program`, as fm1-base's Bluetooth build): the flash and the RAM code are 4 MiB apart, out of a
  direct call's reach. FIZZ's app.c data moved from the RAM code region to RAM. The region: 15,092 B of code (fm1-base's
  3.1 KB with it), 16,420 B of the voices' data, 1,256 B free. **`quad_render` (19,210 B) does not fit yet**: the
  voices' 16 KB of comb lines (`QE_NCOMB 8`) must leave the region first, and RAM (2,896 B free), the pool
  (3,808 B above fm1-base's spare) and NOINIT (3,032 B) cannot take them whole.
- **The shed** (ChoralRoot's): a half over 85 % (4.93 ms) lets one voice go (its key up first, else the oldest; never
  one already fading: the fade steal's one-block ramp), at most every second half; `over_halves`, `sheds`.
- **The remote**: fm1-base keeps one SysEx frame at a time and drops a frame that arrives while one waits; a tap's
  release came while a slow frame kept the press waiting, so the button stuck (the next tap seemed ignored). The
  port now takes fm1-base's remote frames (7D 4C 43) between strips too.
- **The ~104 ms lock** was the port's own: the store's mount (73 sectors read, the project read) inside a lock at the
  first screen. The sound is not on then, so no lock: the panel's scan and USB go on.

**For `quad_render` in RAM (core):** ~19 KB of the RAM code region back: the comb lines out of it (`QE_NCOMB 8 -> 4`
saves 8 KB; or the comb lines lent from the drawing's arena, 27 KB at GFX=fast, when a voice runs a comb), plus ~3 KB
elsewhere (the arena 27 KB -> 24 KB, or the delay 64 -> 56 KB). **The second core** (8 + 8 voices) needs, from the core,
a voice render split (`qd_mix_block` on a range of the pool's voices into its own buses, the two summed before the
effects), and from the platform a working second core: felucca-bsp's `fm1_cpu1.h` says "EXPERIMENTAL: compiles, has
not run on a device", and fm1-base has no `plat_cpu2_*` yet; ChoralRoot's second core works (DUALCORE.md) but its code
is ChoralRoot's (GPL), so FIZZ's would be written for fm1-base anew.

## P2.6: 8 voices, the voices' render in RAM (the single-core package)

The owner (2026-10-23): 8 voices on one core (`firmware/build.py --single-core`, `QD_VOICES_DEV=8`, `fizz-1core.fwsc`),
16 when core 1 renders half (Standard, `QD_VOICES_DEV=16`). Code in RAM is placed by function now (build.py `HOT`:
the section renamed `.fzrt.*` in the object, fizz.ld's window), not by unit.

| | Standard (16 voices) | single-core (8 voices) |
| --- | --- | --- |
| RAM code region: code | 11,620 B (fm1-base 3.1 KB, mix 1.1 KB, the effects' hot seven 7.3 KB) | 31,592 B (+ quad_render 19.4 KB, qd_quad_voice_render, quad_lfo_incb) |
| RAM code region: data | 16,428 B (the comb lines) | 0 (the comb lines in the pool) |
| RAM code region free | 4,708 B | 1,176 B |
| pool | 332,240 B (11,824 spare) | 338,144 B (5,920 spare: 8 voices free ~10.5 KB, the comb lines take 16.4 KB; FM1B_POOL_SPARE=4096 for this build) |
| RAM .data + .bss | 95,708 B (2,596 free) | same |

## USB audio in (2026-10-27)

The owner: the computer records FIZZ over the cable (fm1-base's `usb_audio_in`: the master output after MASTER, 16-bit
44.1 kHz stereo, `uac_tap` after each block in fm1-base's render). Not `usb_audio_out` (the computer playing through
the FM-1: 8,928 B more RAM than there is). fm1-base moved to upstream 934e40d (from 42dc630: Bluetooth's ble_trim,
ble_no_pairing, ble_heap_kb, ble_stack_kb; the USB audio ring in the pool, FM1B_UAC_RING_POOL, which app.h sets).
After: app slot 572,040 of 581,564 (9,524 free), RAM 96,768 of 98,304 (1,536 free), pool 330,096 of 344,064 (5,776
above fm1-base's 8,192). The single-core package and the host simulator build.

Bluetooth MIDI (`ble = true`, a store object at 0xFA000, USB audio off), the overflow:

| fm1-base | app slot | RAM | pool | RAM code region | NOINIT |
| --- | --- | --- | --- | --- | --- |
| 42dc630, defaults | 116,080 | 44,624 | 8,560 | 3,864 | 1,096 |
| 934e40d, ble_trim, ble_no_pairing, heap 12 KB, stacks 5 KB | 82,372 | 27,040 | 604 | 4,136 | 1,096 |

Bluetooth's own memory at the defaults: a 24 KB heap (RAM), three 8 KB task stacks (pool), four 2 KB queues, ~11 KB
of the radio library's. LOOM-8 measured heap 7.1 KB and stacks 1.3 KB deep (fm1-base 3c647b2); FIZZ's audio
interrupt nests on the radio's stacks (stack_audio 3,032), so 5 KB stacks are a guess the unit's log must confirm.

## The Bluetooth package (2026-10-27)

`python3 firmware/build.py --ble` (`--release X` too; `--ble --host`: `build/host/fizz-ble_host`; `--ble --web` builds, into the USB one's `build/emu/`)
builds a second package beside the USB one: fm1-base's Bluetooth MIDI peripheral (`ble_trim`, `ble_no_pairing`; heap
and stacks from fm1.toml's `fizz_ble_heap_kb` 12 and `fizz_ble_stack_kb` 5, or `FZ_BLE_HEAP_KB` / `FZ_BLE_STACK_KB`:
the unit's log, radio.c "heap" and "stacks", says what they reach) with USB audio in kept. Its files:
`fizz-ble.fwsc`, `fizz-ble-xdata.bin` (the first blob), `fizz-ble-xdata2.bin` (the second), `fizz-ble-size.txt`,
`fizz-ble.map`; its link script `build/fizz-ble.ld` (build.py `ble_ld`). The USB package (`python3 firmware/build.py`)
and the single-core one are as they were: the same image (the USB one's 8 bytes of fm1-base's `FM1B_BUILD`, a hash of
the sources, aside), the same blob, the same sizes; nothing below changes them (QD_PKG_BLE 0, the code under `#if`).
The package's identity stays FM-1_46 (release.py stamps versions); the unit says which package it runs in STATUS (42):
the Bluetooth package appends `01 04 01 00` (a package byte, bit 0 Bluetooth; 4 projects; 128 MY presets as 14 bits)
after the selected track; the USB package's reply is byte for byte as before (docs/SYSEX.md "The package").

Where it started, measured (Bluetooth on, USB audio in, the screens' unit `ui_layer.o` already in a second blob that
did not fit; FZ_MEASURE=1 links with every region at 1 MiB and reports the overflow): the app slot 84,710 B over,
RAM 28,588 over, the pool 4,464 over (12,656 short of fm1-base's 8,192 spare), the RAM code region 4,136 over, NOINIT
1,096 over (the radio's log), the first blob 4,612 too long for XDATA1 less OBJ_BLE's two sectors.

| | USB package | Bluetooth package |
| --- | --- | --- |
| app slot (of 581,564) | 572,040 (9,524 free) | 580,992 (572 free) |
| RAM .data + .bss (of 98,304) | 96,768 (1,536 free) | 98,176 (128 free) |
| pool (of 344,064; 8,192 kept spare) | 330,096 (5,776 above the spare) | 335,232 (640 above the spare) |
| RAM code region (of 32,768) | 24 free | 168 free |
| NOINIT, FIZZ's (of 15,696 with fm1-base's) | 200 free | 356 free |
| first blob | 90,616 at 0xE5000 (of 94,196) | 84,544 at 0xE7000 (of 86,004: 1,460 free) |
| second blob | none | 96,672 at 0x97000 (of 98,292: 1,620 free) |
| store | 73 sectors 0x97000..0xDFFFF, magic Z2FS | 49 sectors 0xAF000..0xDFFFF, magic Z2FB |

Every cut, the Bluetooth package only, and what it saved:

| cut | saved | where | the cost |
| --- | --- | --- | --- |
| 4 projects, 128 MY presets (persist.h), the stores' indexes sized to what each store can hold (QD_PRJ_KEYS 576, QD_LIB_KEYS 136: an index can never overflow) | 11,480 (+512 of the browser's list in RAM) | pool | the owner's choice; a full 64-pattern project (413 pages) leaves room for little else |
| the reverb's lines 12-bit, two samples in three bytes (cr_fx.c QD_FX_REVERB_PACKED; the delay as before) | 10,240 | pool | its error a floor at −65..−71 dBFS out (34..41 dB under a loud tail); quiet tails end sooner (a 12-bit unit, then exactly 0); ~185 more instructions a reverb sample, ~2..2.5 % of a core (~120 µs a half) E; test_fx_packed |
| fm1-base's canvas in FIZZ's 8-bit frame (BLE_OVERLAY: gfx.c's cv_px is app.h's APP_CANVAS_AT, the frame's rows in the pool) | 7,680 | pool | none: every platform screen clears the panel and FIZZ draws its whole frame after it (app_ui_init); the crash screen's FIZZ is gone |
| the radio's three task stacks 5 KB, not 6 | 3,072 | pool | the unit's log must confirm 5 KB (LOOM-8: 1.3 KB deep; FIZZ's audio interrupt nests on them, stack_audio 3,032) |
| the undo log 7 KB, not 16 (qd_config.h QD_UNDO_POOL_BLE; 8 KB first, 7 KB for SysEx over Bluetooth's queues) | 9,216 | pool | a full pattern's snapshot is refused ("too big"), a dense pattern's copy (the clipboard) is refused, fewer levels fit beside a big snapshot; ordinary patterns undo as before (test_ctl, test_tools check both sizes) |
| the 8-bit frame's dirty rectangles: one segment a row, not four (fz_fb.c FZ_FB_SEGS, as the single-core package) | 1,920 + 1,920 | pool, NOINIT | a tall narrow change (ROLL's playhead, a meter) goes as whole rows, not a narrow window: longer sends, a split pass more often |
| five RAM functions back in the flash (build.py BLE_COLD): xq_block 664 (the master filter, only while on), xc_block 608 (the chorus, while it holds something), xf_delay 1,940 (while the delay holds something), qd_player_event 886 (a trig), fz_real_qd_fx_block 808 (every block) | 4,906 | RAM code region | the flash cache's misses when they run: P2.15g measured the effects' seven (7.3 KB) out of RAM at +10..16 points of the interrupt; these ~5 KB: +5..11 points E with the delay, the chorus and the filter all on, ~1..2 with none of them. The radio's trim (3.6 KB) must run from RAM |
| moved, not cut: app.o's .bss (8,147) and 82 of the frame's 88 rows in RAM (19,680; FZ_FB_LO 6) to the pool, ctl_latch.o's .bss (748) to NOINIT | — | RAM → pool, NOINIT | none (all SRAM) |
| the screens' code out of the app slot: ui_layer.o but ui_geek_draw and ui_menu_draw (their sections renamed .fzsl.*, build.py BLE_SLOT), 89,926 B, and the 18-px light face of the first blob (6,070) into the second blob | 95,996 | app slot, first blob → second blob | none in speed (the same flash through the same cache). Code executes from the data region's plain window (fl_plain_window_init's unencrypted range from 0x93000 up); main loop only, called only once both blobs are checked (fz_init). **The CPU's PC limit** (felucca-bsp fm1_guard.h: a fetch outside [0x02000120, _etext] and the RAM code faults, vector 1, DBG_MSG bit 12) must cover it: fizz-ble.ld's `_etext` is the second blob's end (the slot's own end `_etext_slot`; the core-1 save/restore of the limits keeps it), and build.py's pc_window_check fails a link with a function outside the window (2026-10-27: until then every screen drawn from the blob crashed on the unit: EDIT, SHIFT, the menus) |

OBJ_BLE (fm1-base's Bluetooth store object, two sectors) is at **0xE5000** (XDATA1's first two sectors, Felucca's
autosave pair), not 0xFA000: fm1-base's storage writes and erases only inside FL_STORE_OK (felucca-bsp fm1_flash.h:
0x97000..0xDFFFF, 0xE5000..0xE6FFF, 0xFC000..0xFEFFF), and 0xFA000 is outside it (st_erase would refuse it, -8: the
radio's address never kept). The first blob starts at 0xE7000 instead; the room is the same.

**SysEx over Bluetooth** (2026-10-27, the owner: yes). FIZZ's own SysEx, the Manager's 7D 46 5A whole (LIST, DUMP,
STORE, LABELS, the jobs) and the port's (7D 46 50 stats, the backup's FZBI / FZBR in the update framing), from a
computer on Bluetooth, answered over Bluetooth; a request over USB answered over USB:
- fm1-base 934e40d drops SysEx in le_midi.c. `firmware/patches/fm1-base-ble-sysex.patch` (RemoteFM1's 0001, the owner's,
  MPL-2.0: its le_midi.c and radio.c parts and the simulator's `sysex PORT ..` command) is applied by build.py with
  `patch` to copies in build/overlay (BLE_PATCHED; the submodule stays clean): SysEx both ways as USB-MIDI packets (CIN
  4, then 5 / 6 / 7), a frame whole or cut short. radio.c's queues sized in app.h (BLE_OUT_Q 256 packets: a whole reply
  at once, LABELS ~560 B; BLE_IN_Q 64) and moved to the pool (software rings; the radio's DMA buffers stay in its heap).
- In: app.c fz_bsx (the audio interrupt, which reads plat_midi_in) gathers a frame's bytes into one of two 320-byte
  slots (the Manager's longest frame to the unit: a STORE6, ~190 B); the main loop moves it into fz_hq with its port
  (fz_bsx_hold) and serves it as a USB frame (fz_route: the port being served).
- Out: the core tags each reply with its request's port (fizz_sx.c, QD_PKG_BLE: the ring's length's top bit; a LIST
  / DUMP job, a STORE's work, a LABELS subscription keep their requester's port); app.c sends a Bluetooth one whole as
  packets (fz_ble_sx_send: the audio interrupt held off meanwhile, so no note lands inside it; when the queue has room,
  else the next look; up to 300 ms for the port's own replies). FZSR (restore) and FZXD (a blob) over Bluetooth answer
  FZER 15: they pull from the host over USB only.
- The cost: ~2.5 KB of pool (the frame slots 640, the out queue 256 x 6 B, the in queue 256; the stock queues' 640 B
  of RAM back), 1 KB of undo log (8 -> 7 KB), the frame's rows in RAM 2 -> 6 (960 B of the pool to RAM), ~1.4 KB of
  code (the page 2 drawing went to the second blob: menu_set2 noinline). The simulator: tests/ble_sysex.txt (STATUS,
  IDENTITY, LIST, a DUMP of 23 frames and a STORE over Bluetooth, answered there and never on USB; one over USB on USB).
  The update framing's FZBI / FZBR over Bluetooth run on the unit only (the simulator has no update framing).
- Not taken: RemoteFM1's 0002 (ATT MTU 247: Bluetooth SysEx ~4x faster) costs 183 B of the radio's RAM, which is not
  there.

**SETTINGS page 2** (the owner, 2026-10-27; the Bluetooth package only): GLO + SEL, then SELECT, as the track settings'
pages (two dots by the title). KNOB 1 BLUETOOTH ON / OFF (the settings record QD_RK_SET byte 5 bit 4, set = OFF: an
older record reads ON); KNOB 2 the radio's state, read-only: OFF, WAITING, CONNECTED. app.h APP_BLE_AT_BOOT is
app.c fz_ble_at_boot (the store started a moment early, at main_fm1.c's ble_start, to read it); a change is applied
every 10 ms (fz_ble_look: plat_ble_enable) and the state told to the page (qd_app_ble_state). The simulator:
tests/ble_settings.txt (the shots, OFF switching the radio off and ON on again).

**Bluetooth's turns** (2026-10-27, the unit: a Mac's link through Audio MIDI Setup lost, reason 8, right after its
connection updates; a plain CoreBluetooth link at 30 ms held). fm1-base runs the radio's tasks, its link layer's
control procedures among them (each due at an "instant" ~6 connection events on: ~45 ms at CoreMIDI's 7.5 ms), only
in ble_poll: once a main-loop pass and in plat_yield. FIZZ's pass held it for a whole frame (the instant, every band,
the scan's wait, the send: ~25..90 ms), a store step (up to 8 pages; an erase ~40 ms with the interrupts off, the
radio's too), a project load or a SysEx job. Now (the Bluetooth package only):
- app.c fz_ble_give: plat_yield when 2 ms went by without a ble_poll, from fz_input (between bands: FZ_FB_STEP 1, a band
  at a time; the scan's wait), the send's every transfer (fz_fb.c), fz_watchdog (the store's every flash operation, its
  every read: fz_fread). Never with the audio interrupt masked (fz_audio_off's depth), inside it, or inside a turn.
- The gaps timed: fm1-base's app_loop and app_yield (right after ble_poll): stats line 17 (`fizz_xdata.py stats`,
  `--reset` starts them again): ble_gap_max_us, gaps over 5 and 10 ms, the turns given; Bluetooth's SysEx in / out /
  lost, MIDI in / out; the store's erases and their longest (us), its pages and their longest, erases with a computer
  linked, erases held.
- The store's erases held while a computer is linked (persist.c erase_hold): only when a store is down to its last
  blank sector (else a record would wait); GC and the blank sectors ahead wait for the link to go. A page (256 B,
  ~1 ms E with the interrupts off) is not held.
- le_midi.c (BLE_OVERLAY): the FM-1 asks for a new interval only when the link is outside BLE MIDI's 7.5..15 ms (a Mac at
  7.5 ms keeps it: one update procedure less), its first range now 7.5..22.5 ms, the old ones after it.
- The unit (da2e512, the owner's Mac on Audio MIDI Setup): the link held at 7.5 ms; idle on HOME, the longest gap 14.8 ms
  (185 over 10 ms a minute); one reading 93 ms. The 93 ms was the stats' own reply: 16 lines over USB through
  fm1-base's ota_wire_send, which waits ~1 ms a 48 bytes for its ring with nothing else run (and the next reading shows
  the gap). Now every USB SysEx FIZZ sends (its replies, the stats, the backup's) goes through app.c fz_usb_sx_send:
  the same ring, Bluetooth's turn while it waits. The idle gaps: the frame's instant then its first band ran together;
  a turn between them now, and before the store's step. Line 17 also says where the longest gap was: its phases
  (ble_gap_phases, a bit each: app.c FZ_PH_*), the one it began in and the one it ended in.
- What still holds Bluetooth back for longer than a band: a call under the audio lock (a SysEx command's work, a key's,
  a project load's reads: the stats' in_ms_max), one instant of the drawing (the screen's logic and its recording,
  fz_fbs latch), a held erase a store needed. The unit's line 17 says how long.

MIDI over Bluetooth: in, as USB's and the TRS jack's (app.c fz_midi_isr reads every port: the channels, 15 the selected
track, 16 the remote, the clock and transport); out, the core's queue to USB and, a computer connected, to Bluetooth
(fz_port.c fz_midi_out: `plat_midi_out_room(TO_USB | TO_BLE_HOST)`, the fuller queue's room, never a wait; the
simulator's `firmware/app/tests/ble_midi.txt` with `fizz-ble_host`).

### What the other tools need for the second package

- **fizz_xdata.py**: `push --ble` writes `fizz-ble-xdata.bin` to 0xE7000 and `fizz-ble-xdata2.bin` to 0x97000 (one
  FZXD each, the unit found again for each; the unit takes a blob whose length, CRC-32 and offset match one of its two
  stamps, "FZXDSTMP" and "FZX2STMP", and starts FIZZ once both are there); `info --ble`. `backup` / `restore` / `wipe`
  unchanged: the unit reports its store's region in FZBI (the Bluetooth package: 0xAF000, 200,704 B = 49 sectors).
- **The install page / release.py** (not changed yet): build with `--ble --release X`; publish `fizz-ble-X.fwsc` and
  both blobs; check each blob against its stamp in the image (`FZXDSTMP` for the first, `FZX2STMP` = bytes
  46 5A 58 32 53 54 4D 50 for the second: len, CRC-32 after it); after the install, write the first blob at 0xE7000 and
  the second at 0x97000 with the same FZXD command (46 "FZXD" len crc off), in either order; the first can be FZOK 0
  (already there). Writing the second blob erases the USB package's MY PRESETS (0x97000..0xAEFFF): a backup first
  (`fizz_xdata.py backup` on the USB package) if they matter. Back to the USB package: its blob at 0xE5000 again; its
  store starts fresh (the Z2FB sectors are another's).
- **The web Manager** (site/manager/fizzstore.js, fizzbak.js, parseStatus in tools/fizz_sysex.mjs and site/js): a
  store image's layout by its sectors' magic (each sector's first four bytes, little-endian; blank = all FF):
  "Z2FS" 5A 32 46 53 = USB (73 sectors from 0x97000: LIBRARY at 0, 22 sectors; projects at 22 x 4096, 51 sectors);
  "Z2FB" 5A 32 46 42 = Bluetooth (49 sectors from 0xAF000: LIBRARY at 0, 11 sectors; projects at 11 x 4096, 38
  sectors). Records and pages are the same format. fizzstore.js today checks byte 3 == 'S' (it refuses Z2FB) and
  writes "Z2FS" in writeStore / buildImage: a Bluetooth image must be written back with Z2FB. 4 projects, 128 MY
  PRESETS there: from STATUS's package bytes (absent: 8 and 256). Over Bluetooth MIDI (the FM-1's own port, when
  a computer is connected) everything but a restore (FZSR) and a blob's write (FZXD) works as over USB: replies come
  back on the port the request came on.
