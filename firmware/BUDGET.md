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
  screen before the store is mounted ("FIZZ: CLEARING ITS STORE", ~1.3 s, once); **REC held at power-on is safe mode**
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
