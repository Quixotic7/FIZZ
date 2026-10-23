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
