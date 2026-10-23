# FIZZ on the FM-1 itself (Phase 2): feasibility and budget audit

2026-10-23. FIZZ 0.9.5-p1.6g (FMToneSeq 2354eb7). This audit only read and measured. Nothing was flashed or pushed, and no repo
was changed except for this file.

**Labels.** Every number carries one of these:

| label | meaning |
| --- | --- |
| **M** | measured in this audit (the method is given) |
| **U** | measured on the unit by ChoralRoot's work (cited) |
| **D** | read from docs, linker scripts or source (cited) |
| **E** | an estimate (the assumptions are given) |

**Paths.** All paths are relative to `/Volumes/Q7Media-2025/Projects/Github/ChoralRootFM1/`.

| short form | stands for |
| --- | --- |
| `CR/` | ChoralRoot 0.26.1, the newest tree: `ChoralRootFM1/.claude/worktrees/rel-0261-1b7a23/` (7d97982) |
| `FZ/` | `FMToneSeq/` |
| main checkout | `ChoralRootFM1/` itself (rel-24b3). Its BOOT-SAFETY.md, INTEGRATION.md, lcd.c and main.c are byte-identical to `CR/`. |

---

## 0. The answer in one page

**FIZZ can become real FM-1 firmware, but not as Phase 1 builds it.** The sound engine, the sequencer and the
controller logic port almost as they are. Three things do not port:

- the memory shape: everything is held in RAM at once;
- the drawing: plutovg's full-screen redraw every 15 ms;
- the size of the constant data.

| resource | the FM-1 has | FIZZ Phase 1 needs (32-bit) | factor | after the cuts in §5 |
| --- | --- | --- | --- | --- |
| data RAM (RAM 96 KiB + POOL 336 KiB + NOINIT 15 KiB) | **458 032 B**, about 425 KiB for FIZZ after the platform and headroom (D, E) | **~4.5 MB**: 2.89 MB `qd_app_t` + 0.68 MB function statics + 0.94 MB heap peak (M) | **~10x** | ~387 KiB with the device drawing primitives (E); see §4 |
| flash, the app slot | **573 440 B** (D) | **~555 KiB** FIZZ core + ~15 KiB libc/libm + ~55 KiB platform = **~625 KiB** (M + E) | **1.09x** | ~545..560 KiB (E) |
| audio CPU, core 0's worst half | 2902 us, voices shed above 85 % (D) | 16 voices at 8 + 8, WOBBLE-class with DRIVE, CRUSH, AHD and mods: **~2710 us (93 %)** with code in flash; **~2240 (77 %)** with `quad_render` in RAM (E, calibrated) | fits only with code in RAM | 77 % |
| UI CPU, core 0's main loop | ~3..7 ms of CPU per 15 ms frame while audio runs (E) | plutovg full redraw: step view **~44 ms**, idle dance **~148 ms**, TAP ape **~100..160 ms** a frame (E: host time x85, calibrated on ChoralRoot's own renderer) | **6..50x** | strips + device primitives + 20..30 fps: to be measured on the unit |
| SPI to the LCD | 51.4 ms per full frame at 20 MHz (U) | step view: 11 of 120 tiles change a frame (~4 ms); idle dance 103 of 120 (~44 ms: 20 fps at most) (M) | fine with tile blits | same |
| project storage | ~292 KiB of data flash if FIZZ owns 0x97000..0xDFFFF (D) | a demo-density pattern is **2.7 KB** (10.6 pages, of which ~1.9 KB is its 8 track sounds); a project of 64 such patterns is **~163 KB** (M) | 8 projects do not fit | sounds stored by reference: ~0.8 KB a pattern (E) |

**Three findings that lower the risk:**

1. **The whole core already compiles with JieLi's own pi32v2 clang 4.0.1** at ChoralRoot's flags. Only two things
   were needed:
   - `-D__STDC_NO_ATOMICS__`, for plutovg's reference counts;
   - one `-idirafter` include path.

   All 44 objects built (M).
2. **The FM-1 has a single-precision FPU, and Melodee ships with it on.** Melodee's flags are
   `-mcpu=r3 -mfprev1 -ffp-contract=off -Werror=double-promotion` (`melodee/tools/build.py:43`;
   `melodee/README.md:129`). With those flags FIZZ's code is **29 KB smaller**, and plutovg's floats run natively
   instead of through soft-float calls (M).
3. **The flash store (`FZ/core/store/store.c`) is already written for the device.** It is the floating page store of
   PLAN §7: three flash hooks, the commit page last, no erase unless quiet. The page counts above come from it.

**Recommended first milestone, "FIZZ boots":**

- the screen, keys, LEDs and FM TONE in KEYS mode;
- demo A01 playing on one core;
- the same core tree with a device sizing header;
- plutovg kept for now, drawing 40-row strips from a fixed arena, capped at 15..20 fps, with no TAP ape.

After that, measure the unit's real frame cost before deciding on the drawing (§6, §7 Q1).

---

## 1. The FM-1's resources

### 1.1 CPU

**The chip** is a JieLi AC791N / WL82 (`FM-1-transporter/README.md:3`). It has **two pi32v2 cores**
(`CR/docs/DUALCORE.md:62`). The toolchain is clang 4.0.1 with `-target pi32v2` (`CR/tools/build.py:117`). (D)

**The clock is ~360 MHz.** Melodee measured this on units; it is above JieLi's 320 MHz table
(`CR/docs/DUALCORE.md:448`), and the core rails are raised to match (`CR/firmware/hal/fm1_power.h:37,40`). (U, third party)

**Throughput:** code runs at about 205 instructions per us (`CR/docs/FLASH.md:11`). ChoralRoot's emulator converts
with device us = instructions / 258..259 (`CR/docs/EDITOR.md:574`, `CR/docs/DUALCORE.md:291`). (D)

**The FPU:**
- It exists. Melodee uses it with `-mcpu=r3 -mfprev1` (`melodee/tools/build.py:43`), including inside the audio ISR:
  `fm6_render` is "native fp32" (`melodee/tests/target_budget.txt`).
- Melodee's ISR wrapper saves only psr, rets, reti and r0..r3 (`melodee/firmware/hal/fm1_isr.S:6-17`). So the FP
  state needs no extra save. This suggests the FPU works on the general registers (E, an inference).
- ChoralRoot builds without FPU flags (`CR/docs/PROPHET.md:53`), and all its DSP is integer.
- Doubles stay soft even with the FPU, because it is single precision.

**How ChoralRoot splits the work across the two cores** (`CR/docs/DUALCORE.md:150-166`):

| who | what |
| --- | --- |
| core 0 | the audio ISR: tick, events, voice allocation, LFOs and matrix, part 0's render |
| core 1, job 1 | the bass part, plus a share of the P5 / VA chord voices |
| core 0, after join 1 | DIST, level, pan, sends |
| core 1, job 2 | the reverb, while core 0 runs chorus and delay |
| core 0, after join 2 | master, USB tap, click |

- Core 1's stack is **2816 B** (`DUALCORE.md:82`).
- Its idle loop runs from RAM (`.c1_text`, `CR/firmware/app.ld:36`).
- Dual Core has been on by default since 0.22, and a 1-core package also ships (`DUALCORE.md:6,29`).

**The main loop:** the UI frame comes every **15 ms** on core 0's main loop, never in an ISR
(`CR/firmware/src/main.c:385-406`). TIMER5 runs at 10 kHz, priority 4, and nests above the audio ISR (priority 3)
(`main.c:129-135`, `audio.c:341`).

### 1.2 RAM

On-chip SRAM is 0x01C00000..0x01C80000 (512 KiB), from `CR/firmware/app.ld:15-45`. **RAM and POOL are adjacent**
(0x01C08000..0x01C74000, 432 KiB in one span); the split between them is only a linker section.

ChoralRoot 0.26.1's use comes from `ChoralRootFM1/.claude/worktrees/b-C2.log:34`.

| region | address | size | ChoralRoot 0.26.1 uses | notes |
| --- | --- | --- | --- | --- |
| RAMTEXT | 0x01C00000 | 24 576 | 16 120: flash driver 2888, P5 3792, VA 9046, `.c1_text` 128 | code in RAM. The build demands ≥ 512 B spare (`CR/tools/build.py:87`) |
| (debug gap) | 0x01C06000 | 8 192 | no | reserved for RAM-run debug (`app.ld:8`) |
| RAM (.data + .bss) | 0x01C08000 | 98 304 | **80 544 (81.9 %)** | the largest user is `trk` 9.7 KB |
| POOL | 0x01C20000 | 344 064 | **308 636 (89.7 %)** | static arrays in `.pool`, NOLOAD. The build demands ≥ 8 KiB spare (`build.py:478`) |
| user stack | 0x01C74100..0x01C7A000 | 24 320 | the main stack | `crt0.S:14` |
| system stack | 0x01C7A100..0x01C7C000 | 7 936 | the ISR stack (E: not documented) | `crt0.S:15` |
| NOINIT | 0x01C7C020 | 15 664 | 228 | survives a reset |

- **ChoralRoot has no malloc and no heap:** a grep of `firmware/src` finds none. Every buffer is static. (D)
- **The largest POOL users in ChoralRoot:**
  - `xd_buf` 64 KiB (delay)
  - `grain_buf` 64 KiB
  - `cv_px` 57 600 (the strip canvas)
  - `xr_buf` 32 KiB (reverb)
  - `eng_state` 13.8 KiB

  (D: `nm` on `CR/build/choralroot.elf`)
- **The note "RAM ~80 %, POOL 92 %, flash 60 %"** (2026-10-07) is now RAM 81.9 % and POOL 89.7 %.
- **The app slot is 94.9 % full, not 60 %:** 544 196 of 573 440 B, with ~29 KB free. The 60 % can only refer to the
  whole 1 MiB chip.

### 1.3 PSRAM or external RAM: **no**

- The memory map ends at 0x01C80000 (`app.ld:14`), and no region lies outside on-chip SRAM.
- "psram", "sdram" and "external ram" appear nowhere in ChoralRoot, TransitFM1, RemoteFM1 or the reference docs.
- `RemoteFM1/PLAN.md:30` describes RAM as "96 KiB .data/.bss plus a 336 KiB POOL".

(D)

### 1.4 Flash

**The chip** is 1 MiB SPI NOR, JEDEC 0x856014 (`CR/firmware/hal/fm1_flash_map.h:11,20`). (D)

**Layout** (`CR/docs/BOOT-SAFETY.md:237-245`, `fm1_flash_map.h:106-120`):

| range | what |
| --- | --- |
| 0x0000..0x3FFF | SPL head |
| 0x4000..0x411F | directory |
| 0x4120..0x4FFF | **boot stub** (byte for byte, `stub.lock`) |
| 0x5000..0x90FFF | **app slot**: XIP 0x02001000, **573 440 B** (`app.ld:17`) |
| 0x91000 | commit marker |
| 0x92000 | cfg |
| **0x97000..0xDFFFF** | **data** (FL_DATA, 292 KiB, 73 sectors): ChoralRoot's legacy stores, CZ banks, session, favourites, sound pool 0xB8000..0xC7FFF, loops 0xC8000..0xDBFFF |
| 0xE0000..0xE4FFF | the staged update loader |
| 0xFC000 / 0xFD000 | settings A / B; FM6 bank copy at 0xFE000 |
| 0x93000..0x96FFF and 0xE5000..0xFBFFF | not on ChoralRoot's write list and not documented. V15 stages a loader at 0xE6000 (`RemoteFM1/handoff/TRANSITFM1-FIXES.md`), so this is **not free** |

**Data can be read through XIP.** `fl_plain_window_init` maps flash from 0x93000 up as plain XIP
(`FM1_XIP(off) = 0x02000000 + off - 0x4000`) at **~30 MB/s** (`CR/docs/PRESETS.md:348-352`). (D)

**Erase and program costs:**
- **A 4 KiB sector erase masks every interrupt for 17.8 ms** (U: `irq_gap_us` 17 783 / 17 851; `CR/docs/SETTINGS.md:115,150`).
- A page program is about 1 ms (D, not measured: `CR/docs/FX.md:441`).
- Every flash-write source must sit in RAM. A source in XIP faults: this is the XIP re-stage bug in
  `RemoteFM1/handoff/TRANSITFM1-FIXES.md:6-51`. (D)

### 1.5 The LCD

**The panel:** ST7789-class, 240 x 240, RGB565, on SPI1 at **20 MHz** since 0.22. Transfers are polled DMA, and the
last strip is left sending in the background (`CR/firmware/src/lcd.c:3-17,114,137`). (D)

**ChoralRoot keeps no full framebuffer.** It draws in **six 240 x 40 strips**:
- Each strip is hashed as 4 x 5 tiles of 60 x 8, and only the bounding box of the changed tiles is sent.
- The two DMA buffers are rows 40..119 of `cv_px` (57 600 B in POOL), so the next strip is composed while the last
  one sends (`CR/firmware/src/cr_draw.c:2973-2980`, `CR/docs/EDITOR.md:589-600`).
- Only the strips whose content changed are composed (EDITOR.md:589-592). (D)

**SPI time:**
- **51.4 ms for a full frame (19.5 fps)** at 20 MHz, measured by RemoteFM1's bench (`RemoteFM1/PLAN.md:215`). (U)
- **7.7 ms for a 240 x 40 strip** (`lcd.c:17`). (D)
- One 60 x 8 tile (960 B) is 0.38 ms. (E)

**A full framebuffer fits only without a synth.** RemoteFM1 holds `rf_fb` at 115 200 B in POOL
(`RemoteFM1/firmware/src/remote.c:462`), with no synth beside it. (D)

**The integer AA primitives ChoralRoot already has** (`CR/firmware/src/cr_gfx.c:5-12,165-603`):
- fractional rects;
- discs;
- triangles;
- arcs with round caps and dashes;
- polylines with round joins and dashes (`cr_poly`);
- quadratic curves;
- 4-bit-alpha text faces resampled to any size (CRX 104 px, CRB 40 px, S 12, M 15).

(D)

**ChoralRoot's UI frame cost** (emulator; no figure from the unit):
- a whole screen: 2.0 M instructions = **7.6 ms**, worst 10.5 ms (`CR/docs/INTEGRATION.md:1398`);
- `perf.sh`'s UI frame: average 2.6 ms, max 15.6 ms (`INTEGRATION.md:1400`).

(E)

### 1.6 Audio and the CPU budget per block

**The format** (D):
- **44 100 Hz**, a half buffer of **128 frames = 2902 us** (`CR/firmware/src/core.h:25`), with control blocks of
  32 samples;
- ALNK0 I2S, double-buffered DMA (`hal/fm1_audio.h:3-5`);
- the shed drops voices above 85 % of a half (`audio.c:158`).

**ChoralRoot 0.22 on the unit**, worst halves in us of 2902, Dual Core On (`CR/docs/INTEGRATION.md:1098-1110`). (U)

| 8 notes + bass | block | tick | part 0 | fx | master | TIMER5 |
| --- | --- | --- | --- | --- | --- | --- |
| idle | 233 | 63 | 50 | 48 | 80 | 25 |
| **FM TONE: WOBBLE + BASS** | **2345 (81 %)** | 237 | **1509** | **642** | 103 | 170 |
| FM TONE, 6 notes | 1993 (69 %) | | 1157 | 645 | | 145 |
| FM6 TINE EP | 2580 | 353 | 1647 | 649 | 111 | 197 |

What else is known:
- **A WOBBLE voice costs 176 us** on core 0 from flash (1509 − 1157 over 2 voices; PLAN §3). Core 1 renders FM6
  voices at about 140 us (PLAN §3). (U)
- **The fx stage averages ~488 us** and peaks at 600..720 while recording. It includes the parts' DIST, level, pan and
  sends; the reverb alone is 193..198 (`INTEGRATION.md:1159-1186`). (U)
- **TIMER5 costs 150..230 us a half**, and 350..410 while recording over USB (`INTEGRATION.md:1126-1129`). (U)
- **Code in RAM:** `P5_RAM` gave −26 % on the chord part, and `CR_RAM_VA` gave the block 2389 → 2103. (U:
  `CR/docs/FX.md:479-502`)
- **`quad_render` is not in RAM in ChoralRoot.** It is 17 552 B (`CR/docs/FLASH.md:78`), and RAMTEXT has only ~8.4 KB
  left (`FX.md:502`). (D)

---

## 2. FIZZ's needs today, by module

### 2.1 How this was measured

All three builds go to the audit's scratch folder. Nothing was written into the repos. Docker was already running.

| build | compiler and flags | what it gives |
| --- | --- | --- |
| **pi32v2** (the device's own compiler) | JieLi clang 4.0.1 `-target pi32v2`, ChoralRoot's `-Os -ffunction-sections -fno-builtin` (run in Docker, `debian:bookworm-slim`, as `CR/tools/build.py:111` does). Then again with **Melodee's FPU flags** `-mcpu=r3 -mfprev1 -ffp-contract=off` | per-object `.text` / `.rodata` / `.bss` sizes. pi32v2 has no `-fdata-sections`, and no pi32v2 link was attempted |
| **Cortex-M4** (a 32-bit stand-in) | `~/arm-gnu-toolchain-14.2` `arm-none-eabi-gcc -mcpu=cortex-m4 -mthumb -Os -ffunction-sections -fdata-sections`, **linked** with `--gc-sections` from the glue's one exported symbol (`fm1core_get`), with newlib-nano | which code survives the link, the struct layouts (`gdb ptype /o`), and per-function stack (`-fstack-usage`) |
| **host** (this Mac, arm64, `-O2`) | the repo's `tools/Makefile` objects with malloc / calloc / realloc / free counted per call site (a force-included header), and every `qd_app_frame` timed | the heap and the CPU |

**Calibration** of host time against the device:
- ChoralRoot's own renderer, compiled from `CR/tests/cr_draw_test.c` on this Mac.
- FIZZ's copy of FM TONE, against the unit's 176 us WOBBLE voice.

**`store.c` is counted once.** It is built twice in the tools (once by itself, once through `app.c → persist.c`).

### 2.2 Static RAM (`.data` + `.bss`), by module (M)

One static `qd_app_t` holds almost everything (`FZ/core/glue/core_quadrille.c:36`). On 32-bit it is
**2 890 296 B** (Cortex-M4 layout; 2 893 648 on the 64-bit host).

| member (`FZ/core/app/app.h:119-132`) | 32-bit bytes | what it is |
| --- | --- | --- |
| `proj` | **1 752 604** | **64 patterns x 26 864** (`seq/pattern.h:235`). A step is 20 B, not PLAN's 12: `note[6]`, `ovr`, `func`, `mfx`… A pattern is 8 x 128 steps (20 480) + 1024 locks (4096) + 8 track sounds (1904) + track settings (344). Also 128 pool sounds x 242 (30 976) and 5 MIDI FX groups (2320) |
| `seq` | **521 768** | event ring `ev[4096]` x 48 B = 196 608 (`seq/seq.h:80,190`); trig records `rec[512]` x **608 B** = 311 296 (`seq.h:81,193`; each holds a whole 110-value patch and 115 locks); per-track cursors 10 752 |
| `undo` | **251 768** | `pat0` 26 864, `snd0[128]` 30 976, `lib0[256]` 61 952, `xl` 10 240, `pool[65536]`, `snap` + `pclip` 2 x 26 864 (`app/undo.h:33,54-70`) |
| `per` | **194 236** | two `qd_store_t` at 63 208 each (index `ent[2048]` x 16 = 32 KB, plus a 28.8 KB `job_buf`: `store/store.h:41-90`); the library `lib[256]` x 242 = 61 952 (`app/persist.h:33,55`); change hashes 5.4 KB |
| `ui` | 64 312 | `clip` 58 624, of which **`undo[8]` x 6712 = 53 696** (`seq/clip.h:54`) |
| `pool` | 57 920 | **64 voices** x 904 (`voice/pool.h:38,80`; 16 are used by default) |
| `player` | 36 248 | MIDI FX run-time 29 408 (5 groups x 5872: `seq/mfx.h:189`); `last_on[8][128]` 4096; group maps 2048 |
| `sx` 4152, `model` 3008, `midi_on` 2048, `mix` 1924, the rest ~500 | 11 632 | |

**Function-level statics** add another **679 882 B** (pi32v2 `.bss`, M):

| what | bytes | where |
| --- | --- | --- |
| comb arena `qe_comb[64][1024]` int16 | **131 072** | `voice/quad/quad_ext.c:142-145` (the comment already says "the FM-1: 16") |
| effects (as ChoralRoot's): `xd_buf` 65 536, `xr_buf` 32 768, `xr_pbuf` 8192, `cho_buf` 4096, `xf` 2356 | 113 920 | `voice/fx/cr_fx.c:201,213,227`, `fx.c:21` (already marked `.pool`) |
| **7 whole-pattern scratch copies** (`static qd_pattern_t tmp / sp`) | **188 048** | `app/view.c:293,462,1133,1657`, `app/ctl_step.c:603,1318`, `app/persist.c:466` |
| record buffers `pr_buf`, `pr_buf2`, store `buf` (3 x 28 800) | 86 400 | `app/persist.c:45`, `store/store.c:357` |
| undo's temporary `tmp[QD_UNDO_POOL]` | 65 536 | `app/undo.c:159` |
| clip `now` / `old` x 2 (6712 each), plus lock pools of 4096 in `clip.c`, `pattern.c` (x 2), `ctl_shift.c`, `ctl_latch.c` | ~40 600 | `seq/clip.c:51,75,88,128`, `seq/pattern.c:998,1022`, `app/ctl_shift.c:47` |
| idle screen masks `mask[240*60]` + `wd` | 19 104 | `ui/ui_idle.c:84,96` |
| `keys[2048]` (persist), factory sound cache 5566, lists, … | ~35 000 | `app/persist.c:840`, `seq/sounds.c:79` |

**Host-only, not counted:** the glue's LCD copy (115 200, `glue/core_quadrille.c:38`), the panel struct (115 384), the
NOR image, and the SysEx ring (16 400). On the device the platform has these.

### 2.3 Heap: every allocation (M)

There are allocation calls in only two places: **`FZ/core/gfx/qd_gfx_plutovg.c`** (lines 34, 37-38, 312-315,
422-423) and **plutovg itself**. Every other module is static.

Measured with `bench`: the app driven as the glue drives it (a tick a ms, audio as it falls due, a frame every 15 ms),
through the idle screen, demo A01 playing, a held step, knob turns, takeovers, all eight button screens held and
tapped, eight sound-editor pages, then SHIFT + B6 tapped 10 times (the TAP ape).

| allocation | after init | after all the screens | after the TAP ape (peak) |
| --- | --- | --- | --- |
| the screen surface, 240 x 240 ARGB32 (`qd_gfx_plutovg.c:34`) | 230 424 | 230 424 | 230 424 |
| **the ape's 6 layers**: 3 poses x (175 x 158 ARGB + a half-size copy) (`qd_gfx_plutovg.c:312`, `ui/ui_ape.c:196`) | – | – | **383 624** |
| glyph outlines cached by stb_truetype and plutovg (`plutovg-stb-truetype.h:1701`, `plutovg-font.c:219,230`) | 2 500 | 65 000 | 71 100 |
| span buffers, which grow and never shrink (`plutovg-rasterize.c:51,115,359`) | 8 200 | 86 000 | 159 700 |
| path arrays (`plutovg-path.c:79,235`) | 250 | 8 200 | 26 600 |
| transient per draw: the stroker's borders (`plutovg-ft-stroker.c:256`: peak 109 744, **one block 65 792**), and the outline per fill or stroke (`plutovg-rasterize.c:174`: peak 79 208) | ~10 000 | ~190 000 | ~85 000 |
| **live / peak** | 242 292 / 296 420 | 392 868 / 522 340 | **877 050 / 935 254** |

**Allocation churn: ~3.06 M allocations in ~2600 frames, ~1200 a frame.** Each fill or stroke allocates an outline;
each stroke reallocates its borders. On a device with no heap this has to be an arena, with bounded arrays.

**Text masks** (`qd_text_mask`, `qd_gfx_plutovg.c:422`) create and free a surface each call: up to 240 x 60 x 4 for
the idle screen.

### 2.4 Flash, by module (M + E)

- **Code:** pi32v2 with FPU flags. The share that survives `--gc-sections` is estimated per object from the ARM link.
- **Constants:** the ARM link's kept `.rodata`. pi32v2's are the same within 1 %, except for the dead stb_image tables.

| module | code | constants | total | what is in it |
| --- | --- | --- | --- | --- |
| voice: FM TONE + E1..E4, pool, mix (`quad_voice.c` includes `eng_quad.c`, `quad_ext.c`) | 29.7 KB | 31.9 KB | 61.6 KB | FM TONE's tables (`quad_tables.h`, `quad_times.h`, `cr_tables.h`) |
| effects (`qd_fx.c` includes `cr_fx.c`, `fx.c`, `master.c`) | 11.8 | 3.5 | 15.4 | |
| sequencer: model, pattern, seq, player + mfx, chord, scale, sounds + names, clip, demo | 40.7 | 8.3 | 49.0 | factory sounds, `names_words.h` 2 KB, the demos |
| app: app + persist + undo + SysEx, `ctl*`, view | 102.9 | 14.9 | 117.8 | |
| ui: 10 screen units (`ui_layer.c` alone is 84 KB of code: it includes `ui_snd`, `ui_lib`, `ui_presets`, `ui_type`, `ui_mfx`) | 140.3 | 21.7 | 162.0 | **`ape_poses.h` 16 808 B** (`APE_NBYTES`) |
| gfx glue + **fonts** | 3.2 | **105.6** | 108.8 | **6 Inter Tight weights (200..700) x ~17.5 KB TTF subsets** (`FZ/core/gfx/fonts_gen.h:7,888,…,5308`) |
| plutovg (stb_image, stb_image_write and the font cache loader are dropped by `--gc-sections`) | 38.6 | 2.0 | 40.6 | rasteriser (FreeType's), stroker, blend, stb_truetype |
| **FIZZ core** | **367** | **188** | **~555 KB** | without FPU flags the code is **394 KB** (+27) |
| libc + libm (snprintf, sinf / cosf / atan2f / sqrtf / powf, strtod) (E, from the ARM link) | ~15 | | ~15 | `snprintf("%.1f", double)` in `quad_voice.c:387` pulls in float printf |
| platform: HAL, ISR, cpu1, USB MIDI + UAC, LCD driver, OTA / loader, stub glue (E) | ~55 | | ~55 | TransitFM1 is 53 560 B of XIP including its own gfx, fonts and a 7.5 KB loader copy (`TransitFM1/README.md:104`). ChoralRoot's `fm1_*` symbols are 26.8 KB and its USB / UAC ~9 KB. FIZZ replaces the gfx and adds the audio ISR |
| **total** | | | **~625 KB** | **slot 573 440 B: ~52 KB over** |

### 2.5 Stack (M, from the ARM build's `-fstack-usage`)

**The UI is the deepest path:**

| frame | bytes |
| --- | --- |
| `qd_view_frame` | 1216 |
| `ui_snd_draw` | 1952 |
| `sd_crush` | 3040 |
| plutovg's `PVG_FT_Raster_Render` (a stack pool) | **12 640** |
| `plutovg_blend_gradient` (separate from the raster pool) | 8368 |
| `blend_transformed_*` (separate from the raster pool) | ~4200 |

The worst chain is **~20 KB** against a 24 KB main stack.

**The audio path:**

| function | bytes |
| --- | --- |
| `qd_app_block` | 1072 |
| `quad_render` | 840 |
| `qd_seq_step_rec` | 1464 |

That is ~3..4 KB on the ISR stack (7.9 KB), and it must fit core 1's 2816 B when voices render there. ChoralRoot's
`quad_render` does already.

### 2.6 The summary table

| module | RAM today (32-bit) | flash | Phase 1 note | Phase 2 note |
| --- | --- | --- | --- | --- |
| voices (pool, FM TONE, E1..E4) | 57.9 KB pool + 128 KB comb | 61.6 KB | 64 voices, 64 comb lines | 16 voices (14.5 KB), 16 comb lines (32 KB) or 8 (16 KB); `quad_render` + `quad_ext` hot paths into RAMTEXT (~21 KB free: FIZZ has no P5 / VA RAM code) |
| effects | 113.9 KB | 15.4 | ChoralRoot's, already `.pool` | the same; the delay could be 32 KB at half the time |
| sequencer queues | 508 KB | in seq | 4096 events, 512 trig records | **measured peak over 40 s of each demo: 25 events, 13 trig records** (`bench seqpeak`); 512 x 24 B events + 64 records → ~33 KB |
| patterns and sounds | 1.75 MB | – | all 64 patterns in RAM | the playing and the queued pattern in RAM (54 KB, or 37 KB at a 12 B step); the rest read from the store; pool sounds read through XIP |
| player / MIDI FX | 36.2 KB | in seq | 5 groups x 5.9 KB | one held-note pool shared (PLAN §13): ~19 KB |
| persist and store | 194 KB + 95 KB buffers | in app | two stores, 2048 keys each, library in RAM | one store, an index sized to the region (~16 KB), page-streamed records, the library read from flash: ~24 KB |
| undo | 252 KB + 54 KB clip undo + 64 KB temp | in app | full copies | a ~16..24 KB log; snapshot and clipboard in a flash scratch sector |
| UI state and statics | ~60 KB + 188 KB of pattern copies | 162 KB ui + 118 KB app | | one shared scratch, or none |
| gfx | 230 KB surface + 384 KB ape + 120..300 KB plutovg | 109 KB fonts + 41 KB plutovg | full ARGB redraw, any size of TTF | 40-row strips (§5); device primitives or an arena; fewer weights |

---

## 3. CPU

### 3.1 Calibrating host time against the device (M + U)

| anchor | this Mac | the device | device / host |
| --- | --- | --- | --- |
| a FM TONE WOBBLE voice, a 128-frame block, through FIZZ's copy of ChoralRoot's code (16 voices 37.1 us − 8 voices 20.6, over 8) | 2.06 us | 176 us (U) | **~85x** |
| ChoralRoot's whole chord view, a forced redraw of all six strips (`CR/tests/cr_draw_test.c` built at `-O2`, 200 draws a state) | 95..125 us (94 states: mean 154) | 7.6 ms (emulator: 2.0 M instructions / 258) | **~69x**; ~85x if the device runs at 205 instructions per us rather than 258 |
| the effects alone: chorus, delay, reverb, Output Clean (`render_audio`) | 3.0 us | the fx stage averages ~488 us, but it also holds the parts' loops (U) | ~100..160x (a loose anchor) |

**Both anchors agree on ~70..85x for integer code.** Below, UI and DSP times are scaled at **85x**.

That assumes plutovg's float code runs with the FPU flags. **Without them (soft-float), the UI figures would be several
times higher** (E: plutovg's blend, canvas and stroke code calls `__addsf3`, `__mulsf3` and friends in the build
without them, M).

### 3.2 Audio (M on the host, E on the device)

**Per voice, by feature.** `bench`, 16 voices, sends 40, the app's whole block. Each change is against the same
preset without the feature.

| feature (patch values) | host, over preset 0 (no filter, no drive) | host, over WOBBLE (LP24, drive 10) | device, scaled x85 |
| --- | --- | --- | --- |
| E2 filter machine MULTI / COMB+ / EQ | +0.45..0.53 us a voice | ±0 (it replaces LP24) | ~0 over a filtered voice; +40 us over an unfiltered one |
| E1 DRIVE pre or post | +0.50 | ±0 (DRIVE moved) | ~+32 (ChoralRoot's measured DIST loop) |
| E3 CRUSH (BITS 60, SRR 30) | +0.08 | +0.04 | +4..7 |
| E1 AHD + portamento | +0.10 | +0.06 | +5..8 |
| E4 8 mod slots | +0.03 | +0.03 | +2 |

**FIZZ's worst voice:** WOBBLE + DRIVE 90 + CRUSH + AHD / PORT + 8 mods comes to about **176 + 32 + 7 + 8 + 2 ≈
225 us**, +28 % (E).

**Core 0's worst half**, PLAN §3's table with FIZZ's voices, 8 a core, in us of 2902 (E):

| | tick + seq | 8 voices | mix | fx | master | TIMER5 | **half** |
| --- | --- | --- | --- | --- | --- | --- | --- |
| WOBBLE-class (PLAN §3), flash | 250 | 1408 | 80 | 300 | 110 | 170 | 2318 (80 %) |
| **FIZZ's worst voice, flash** | 250 | 1800 | 80 | 300 | 110 | 170 | **2710 (93 %: sheds)** |
| FIZZ's worst voice, `quad_render` + `quad_ext` in RAM (x0.74, P5's measured gain) | 250 | 1332 | 80 | 300 | 110 | 170 | **2242 (77 %)** |
| plus recording over USB | | | | | | +150..240 | 82..85 % |

**The sequencer and MIDI FX** (M on the host, `render_audio`'s `demo.txt`):
- The sequencer plus the player cost **0.054..0.070 us per 32-sample block** on average for demos A01..A03, which is
  0.2..0.3 us a half.
- At 85..160x that is **~25..45 us a half on the device** on average (E).
- The worst single block, a busy step, is below the host clock's 1 us step. The 11..21 us the tool printed are
  scheduler noise.
- PLAN §3 already reserves 250 us for the tick and sequencer (ChoralRoot's tick peaked at 237..354 at a chord change).
- **Two things are not known:** the cost of 32 note-ons with steals in one tick, and an ARP group at 1/32 on 8 tracks.

**64-bit sequencer arithmetic:** the sequencer keeps every tick as `int64_t` (`qd_sev_t` 48 B, `qd_tcur_t`). On a
32-bit core that is slower but block-rate. Packing the event to 24 B halves its RAM.

### 3.3 The UI (M on the host, E on the device)

These are the frame times of `qd_app_frame` (plutovg, full redraw, ARGB → RGB565) on this Mac. The dirty area is the
change between consecutive frames, in 60 x 8 tiles, ChoralRoot's grain.

| scene (`bench`) | host frame, mean / max | **device, x85** | tiles changed a frame (of 120), mean / max | SPI (0.38 ms a tile) |
| --- | --- | --- | --- | --- |
| idle screen, the four operators dancing | 1735 / 2336 us | **~148 / 199 ms** | 103 / 111 | ~39 ms |
| demo A01 playing, step view | 516 / 765 | **~44 / 65 ms** | 11.3 / 115 | ~4.3 ms |
| a step held | 510 / 687 | ~43 / 58 | 2.8 / 92 | ~1 |
| KNOB 1 turned (a p-lock) | 508 / 577 | ~43 / 49 | 9.1 / 32 | ~3.5 |
| knob takeovers | 535 / 734 | ~45 / 62 | 9.4 / 84 | ~3.6 |
| the button screens (TRACKS, SEL, PAGE, MUTE, SOUND, SAVE, KEYS, SEQ) held / tapped | 232..650 / up to 924 | ~20..55 / up to 79 | 1.4..28 | 0.5..11 |
| sound editor, pages 1..8 | 430 / 1128 | ~37 / 96 | 19.5 / 118 | ~7.4 |
| **the TAP ape**: 10 taps + finale, with the 3-pose cache | 1200 / 1881 | **~102 / 160** | 59 / 106 | ~23 |
| `render_frames`, every scenario (40 988 frames) | p50 434, p90 1516, p99 3571, max 5430 | p50 ~37, p90 ~129, p99 ~300 ms | | |

**For comparison, ChoralRoot's busiest whole screen** costs 7.6..10.5 ms on the device (emulator), and its editor
detent costs ~13 ms after composing only the changed strips (EDITOR.md:581).

**FIZZ's frames cost 3..11x ChoralRoot's whole screens on the host**, and FIZZ redraws all of it every frame.

**The time left for the UI** (E):
- Core 0's main loop gets what the audio ISR leaves.
- At an 80 % worst half that is ~3 ms of each 15 ms frame. At the 54 % average ChoralRoot measured it is ~7 ms.
- Core 1 is idle 40..50 % of each half, but nothing runs there outside the audio jobs today.

**So the UI is 6..50x over at 66 fps.** Even at 20 fps (50 ms, the SPI's own limit for a full-screen motion), the
idle dance and the ape are 3..10x over. The step view comes within ~2x once only its 1..2 changed strips are
composed.

---

## 4. The gap

| | have | need today (Phase 1 as it is) | over | need after the "device shape" cuts (no change of look) | after the cuts that change look or feel |
| --- | --- | --- | --- | --- | --- |
| data RAM | ~425 KiB for FIZZ (458 032 − ~25 KB platform − 8 KB headroom) | ~4.5 MB | **~10x** | plutovg kept with strips and an arena: **~622 KB** (over by ~200 KB); device primitives: **~538 KB** (over by ~113 KB) | **~387 KB with the device primitives** (fits with ~38 KB margin; ChoralRoot itself uses 389 KB); ~470 KB with plutovg (still over by ~45 KB) |
| app slot flash | 573 440 B | ~625 KB | ~52 KB (1.09x) | ~600 KB (FPU flags, plutovg's dead code gone, `%f` out) | **~545..560 KB**: 3 font weights, or fonts and the ape in data flash; plutovg → primitives |
| audio, core 0 worst half | 2902 us (85 % shed line = 2467) | 2710 us in flash | ~240 us | 2242 us (77 %) with the render in RAM | |
| UI per frame | ~3..7 ms of CPU per 15 ms frame | ~44 ms (step view), ~148 ms (idle), ~100..160 ms (ape) | 6..50x | strips: step view ~10..15 ms; at 20..30 fps → about fits | the idle and the ape redrawn as device primitives at 15..20 fps, or simplified |
| project flash | ~292 KiB (FIZZ owning the data region) or ~120 KiB (a guest beside ChoralRoot, PLAN §7) | 2.7 KB a pattern, 163 KB a full project of 64 (M) | 8 projects x 64 do not fit | track sounds stored by reference when unedited: ~0.8 KB a pattern, ~50 KB a project (E) | fewer projects (4), or 32 patterns |

**How the RAM column is built.** "Device shape" means: patterns and sounds paged from flash, queues sized to need, one
scratch, one store, a 24 KB undo log, 16 voices, 16 comb lines, strips. The extra look-or-feel cuts are:

| cut | saves |
| --- | --- |
| the delay at 32 KB | −32 |
| 8 comb lines | −16 |
| 24 B events and 32 records | −31 |
| a 12 B step | −16 |
| the sound pool from flash | −29 |
| a tighter scratch | −20 |
| shared MIDI FX pools | −7 |

All figures are E, built from the M sizes in §2.

---

## 5. Ways to close it, ranked by value per effort

Value is bytes or ms recovered; effort is agent-days of work plus the risk to Phase 1. **"Look or feel"** is what the
owner gives up.

| # | change | recovers | effort | look or feel |
| --- | --- | --- | --- | --- |
| 1 | **Size the containers to need**, one device sizing header (`qd_config.h`): `QD_POOL_MAX` 16, `QE_NCOMB` 16, `QD_SEQ_NEV` 512, `QD_SEQ_NREC` 64, `QDS_MAXKEY` 1024, one store | **~1 MB RAM** (pool 43 KB, comb 96 KB, seq 445 KB, store 63 KB…) | small: constants and overflow counters; the tests pin the behaviour | none at these sizes (the demos peak at 25 events and 13 records). Overflow → a counter, like ChoralRoot's shed |
| 2 | **One scratch pattern instead of seven, page-streamed records instead of three 28.8 KB buffers** (`view.c`, `ctl_step.c`, `persist.c`, `store.c`) | ~250 KB | small to medium | none |
| 3 | **Patterns paged from the store**: the playing and the queued pattern in RAM; the others read on selection (XIP ~30 MB/s: a 2.7 KB record in ~0.1 ms) | **~1.67 MB** | medium: the "every pattern in RAM" assumption runs through `seq.c` (chains, queueing), `persist.c` and the overview screens. They need a small per-pattern summary (used, length, a thumbnail of steps) kept in RAM: 64 x ~32 B | a queued pattern from the same bank switches at once; a jump while the store writes waits for the page in flight (≤ 1 ms) |
| 4 | **Melodee's FPU flags** (`-mcpu=r3 -mfprev1 -ffp-contract=off -Werror=double-promotion`); doubles → float in `view.c`, `seq.c:1039-1065`, `ctl_rec.c`, `ui_turn.c` | −27 KB flash; the UI's float math native, not soft (several x) | small | none. Note: the metronome's `expf` / `sinf` per sample in `qd_app_block` (`app.c:322-337`) is the only float in the audio path → a table |
| 5 | **Strip rendering**: draw the screen in 40-row strips, clipped, into a 240 x 40 buffer; hash the strips and tiles as ChoralRoot does; only changed strips composed, only changed tiles sent | RAM 230 KB → 38 KB (ARGB strip) or 57.6 KB (ChoralRoot's `cv_px` with its DMA buffers); CPU ÷3..6 on mostly-static screens | medium: `qd_begin` / `qd_end` as a strip loop calling the screen's draw once a strip. The draw must be deterministic for a given frame (it is: state, not time); text masks (`qd_text_mask`) need a clip | none, if the frame rate holds |
| 6 | **Undo as a log**: the pool already logs diffs (`undo.h:65`); drop the full copies `snd0` / `lib0` / `pat0`, merge clip undo into it, snapshot and clipboard to a flash scratch sector written when quiet | **~370 KB** → ~24 KB | medium | depth bounded by the log (e.g. 16 KB ≈ dozens of step edits, a few page clears); undo of a library delete becomes "re-save" |
| 7 | **The library and the pool sounds read from flash** (256 + 128 x 242 B); 8 track sounds per pattern stored by reference when unedited | RAM ~93 KB; **flash per pattern 2.7 → ~0.8 KB** | medium | browsing reads flash (fast through XIP) |
| 8 | **plutovg kept, in a fixed arena** (bounded span and outline arrays, an LRU glyph cache of ~16 KB) | heap 935 KB → ~64 KB | medium: plutovg's `plutovg_array_ensure` / malloc → an arena allocator; the 65 KB stroker block must go (long paths split) | none; but CPU stays at plutovg's cost (§3.3) |
| 9 | **The device's own AA primitives instead of plutovg** (PLAN §9a): `qd_gfx.h` on ChoralRoot's `cr_gfx.c` (`cr_poly` round caps and joins, arcs, discs, rects, quadratic curves, AA text faces) | flash −40 KB plutovg (+~10 KB); RAM: no heap at all; CPU: ChoralRoot draws whole screens in 7.6 ms where FIZZ needs ~44 | **large**: `qd_gfx.h`'s API (line, polyline, circle, arc, ellipse, rounded rect, fills, alpha, clip, transforms, layers, any-size TTF text) needs device versions; text becomes pre-rasterised faces | subtle: stroke AA is ChoralRoot's look (exact, 16 samples); text only at the built sizes (e.g. 12, 15, 24, 40, 64 px) instead of any size; no arbitrary transforms or gradients |
| 10 | **Frame-rate targets**: 30 fps for interaction, 15..20 fps for full-screen motion (idle dance, ape, page slides); one draw per input burst | CPU ÷2..4 | small | motion less smooth (the SPI already caps full-screen motion at 19.5 fps) |
| 11 | **`quad_render` + `quad_ext` hot paths + DIST in RAMTEXT** (~17.5 + ~3 KB; FIZZ has none of ChoralRoot's P5 / VA RAM code) | audio −26 %: worst half 93 → 77 % | small, as ChoralRoot did for P5 / VA (`tools/ram_code.py`) | none |
| 12 | **TAP ape**: cache 1 pose (138 KB ARGB) / 3 poses as **4-bit palette layers** (5 colours x 3 coverage levels = 16 entries: 13.8 + 3.5 KB a pose) / draw live from the strokes (0 KB, ~+3 ms host on a tap frame) / pre-rendered RLE sprites in flash | 384 KB → 0..52 KB | small to medium | 1 pose: the ghost of the last pose mostly gone; 4-bit: no difference at 1:1 |
| 13 | **Fonts**: 6 weights (105 KB) → 3 (300, 500, 700: ~52 KB); or ChoralRoot's 4-bit faces at the used sizes; or fonts, ape and tables in the data flash through the XIP window | −52 KB to −125 KB of slot | small (weights); medium (data flash: a second payload in the installer and the update path) | weight nuance (200 / 400 / 600 snap to their neighbours) |
| 14 | **MIDI FX held-note pools shared** (PLAN §13) | ~17 KB | small | groups share 64 timed notes |
| 15 | **Idle screen**: the 19 KB band masks → strokes, or fewer dancing elements | 19 KB RAM; idle frame ÷2..5 | small | idle motion simpler |
| 16 | **Fewer voices** (12, or 8 single-core) | audio −25..50 % | trivial | polyphony |
| 17 | **Compress tables** (FM TONE's 32 KB are ChoralRoot's and already dieted; `ape_poses.h` 16.8 KB is already compact) | < 5 KB | – | not worth it |
| 18 | **The size-built main loop** (ChoralRoot's `tools/size_fns.py`, minsize on non-audio functions) | ~3..5 % of 300 KB of UI and app code: 10..15 KB | small (a copy of the tool) | none |

**What to do first:**
- **Items 1, 2, 4 and 11 are free** in look or feel and cheap.
- **Items 3, 5, 6 and 7 are the structural work.** Each can be done on the host first, under the existing tests.
- **The drawing is the one real fork** (items 8 / 9 / 10). It should be decided on the unit's numbers, not the
  estimate (§7 Q1).

---

## 6. A revised Phase 2 step list (PLAN §11 updated for what Phase 1 became)

**The principle: one core tree, two targets.** `FZ/core/` stays the only source:

| target | what it is |
| --- | --- |
| **host** | the Mac `.fm1core` and the tools |
| **device** | a new `FZ/device/`: TransitFM1's chassis with RemoteFM1's fixes, and ChoralRoot's audio ISR and HAL. Built with the pi32v2 toolchain in Docker, with Melodee's flags |

- A **sizing header** `core/app/qd_config.h` holds every container size. `QD_DEVICE_SIZES=1` gives the device's.
- The **host tests are the acceptance tests.** `make -C tools test` runs twice, at the host's sizes and at the
  device's.
- A **size gate** prints the device build's XIP / RAM / POOL / RAMTEXT line on every build and fails over budget, as
  `CR/tools/build.py:87,478` does.
- **Phase 1 keeps going.** The owner prototypes on the Mac through RemoteFM1. Features land in Phase 1 first, and
  they must pass the device-size tests before they merge.

| # | step | proves | its test |
| --- | --- | --- | --- |
| **P2.0** | **The device build of the core, no platform**: `qd_config.h`; the pi32v2 compile in Docker (it already compiles, M); a link with a stub platform; the size report | the core links for the chip; the numbers of §2 confirmed by the real linker | the size gate; `make -C tools test` at `QD_DEVICE_SIZES=1` |
| **P2.1** | **The device shape, on the host** (§5 items 1, 2, 3, 6, 7, 14): queues to need, one scratch, patterns paged from the store, the undo log, one store, the library and pool in flash, the MIDI FX pools shared | RAM ≤ budget with the same behaviour | every host test and golden unchanged (`test_seq` timelines, `test_ctl`, `test_persist`, `test_store` power cuts); `bench`'s RAM report under budget; the owner plays the Mac build for a session |
| **P2.2** | **The chassis**: a copy of TransitFM1 with `TRANSITFM1-FIXES.md` (XIP sources copied to RAM before `fl_write`, the MIDI-in ring drained, the LCD in `fm1_fault`); FIZZ's identity and version; an empty screen; keys and LEDs echoed | it boots and leaves safely; the stub byte for byte | TransitFM1's `tests/run_tests.sh` (stub, ota, ldr, package); the interruption model FIZZ ↔ ChoralRoot ↔ TransitFM1 ↔ RemoteFM1 |
| **P2.3** | **Milestone 1, "FIZZ boots"**: ChoralRoot's audio ISR, master and limiter; FIZZ's core on **one core**, 8 voices; KEYS mode; demo A01 playing; the idle screen and the step view drawn through **plutovg in a fixed arena, 40-row strips, ≤ 20 fps**, the TAP ape off; the store read-only (or RAM) | the screen, the keys and the sound on the unit | unit: `stale_halves` 0, `pk_block_us`, **`ui_frame_avg / max_us`** read; the screens equal to the host's frame dumps |
| **P2.4** | **The drawing decision**, from P2.3's readings: keep plutovg (arena + strips + fps caps), or port `qd_gfx` to `cr_gfx` primitives screen by screen (§5 item 9). A one-day spike first: the step view through `cr_gfx` on the unit | the frame budget | unit: `ui_frame_max_us` ≤ the budget at 16 voices; host: pixel goldens (exact for primitives, a tolerance for plutovg) |
| **P2.5** | **Two cores and code in RAM**: 16 voices 8 + 8, the reverb on core 1; `quad_render` + `quad_ext` + DIST in RAMTEXT | §3.2's 77 % | `cr_c1_thread_test`-style bit-exact (inline = thread = unsplit); unit: 16 FIZZ-worst voices on 8 tracks, three sends, a 16th at 160 BPM: `pk_c0_busy_us`, `pk_c1_busy_us`, `shed_voices` |
| **P2.6** | **The store on the NOR**: the region chosen (§7 Q6); the RAM flash driver; erase only when quiet; one page a frame while playing | saving while playing never drops audio | host: `test_store` power cuts (exists); unit: save with 16 voices playing, `erases_near` 0, `stale_halves` 0, `flash_prog_us` read |
| **P2.7** | **All screens**, fonts (§7 Q3), the TAP ape (§7 Q4), LEDs | the gestures of §8 / 8b on the unit | the host's `render_frames` dumps against the unit's (RemoteFM1 capture or `--debug` screenshots) |
| **P2.8** | **MIDI and clock, MIDI FX, SysEx** (the DN import, `fizz_sx`) | it plays with other gear | host MIDI tests (exist); unit: external clock at 90..180 BPM |
| **P2.9** | **USB audio** (if wanted) | as ChoralRoot's | `uac_check.py`; the +150..240 us of TIMER5 within budget |
| **P2.10** | **Content and release**: factory sounds, demos, the web installer's identity | the owner hears it | the owner, on the unit |

**How the prototype loop carries on beside it:**

- **Phase 1 is unchanged for the owner.** FM1VST Standalone with Remote FM-1, `fm1remote`, the headless tools.
- **After P2.0 every Phase 1 change is checked twice:** at host sizes (free design) and at device sizes. A feature
  that cannot fit shows up the day it is written, not at the end. This is PLAN §13's "budget meter", made a gate.
- **The unit switches between firmwares.** RemoteFM1 is on it for prototyping, and FIZZ is flashed for device steps.
  Only the orchestrator flashes, with one `--info` between flashes and a backup first.
- **Device-only code** (the platform, the device `qd_gfx` if chosen, the store's NOR hooks) **lives in `FZ/device/`.**
  The seams of PLAN §13 hold: nothing in `seq/`, `voice/` or `ui/` calls the platform.

---

## 7. Open questions for the owner

1. **The drawing on the device** (decide after P2.3's readings):
   - (a) keep plutovg: strips, a fixed arena, 15..20 fps for full-screen motion;
   - (b) port to ChoralRoot's integer AA primitives: fastest, text only at fixed sizes, weeks of work;
   - (c) a hybrid: primitives for the step view, held steps and animations; plutovg for rare screens.

   *Recommend (a) for Milestone 1, then (c) where the unit says so.*
2. **The frame rate:**
   - (a) 60 fps everywhere, which is not possible;
   - (b) 30 fps for interaction, 15..20 for full-screen motion;
   - (c) 20 fps everywhere.

   *Recommend (b).*
3. **Fonts:**
   - (a) all 6 Inter Tight weights (105 KB);
   - (b) 3 weights: 300, 500, 700 (~52 KB);
   - (c) ChoralRoot-style pre-rasterised faces at the sizes used;
   - (d) fonts in the data flash through the XIP window (installer change).

   *Recommend (b).*
4. **The TAP ape on the device:**
   - (a) 3 poses as 4-bit layers (~52 KB);
   - (b) 1 pose cached (the ghost mostly gone);
   - (c) drawn live at 15 fps, no cache;
   - (d) left out of the first device release.

   *Recommend (d) for Milestone 1, then (a).*
5. **Undo depth on the device:**
   - (a) a ~16 KB log: dozens of edits, a few big ones;
   - (b) ~32 KB;
   - (c) one level only, "the same gesture again" (PLAN Q17).

   *Recommend (a).*
6. **Flash for projects:**
   - (a) FIZZ owns the whole data region, 0x97000..0xDFFFF (~292 KiB; ChoralRoot's data goes, with a backup first);
   - (b) a guest beside ChoralRoot in its legacy sectors (~120 KiB, PLAN §7);
   - (c) like (a), plus 4 projects instead of 8.

   *Recommend (a) with track sounds stored by reference (~50 KB a full project).*
7. **Patterns in RAM:**
   - (a) the playing and the queued pattern, the rest from flash;
   - (b) a bank of 16 in RAM (430 KB: does not fit).

   *Recommend (a).*
8. **Polyphony:**
   - (a) 16 voices, 8 + 8, the render in RAM;
   - (b) 12;
   - (c) 8 on one core.

   *Recommend (a).*
9. **Comb lines:**
   - (a) 16 (32 KB: every voice can comb);
   - (b) 8 (16 KB: the 9th comb voice falls back to MULTI).

   *Recommend (a) if RAM allows after P2.1, else (b).*
10. **Effects RAM:**
    - (a) the delay as ChoralRoot's (64 KB);
    - (b) half the maximum delay time (32 KB).

    *Recommend (a) unless P2.1 runs short.*
11. **The FPU:**
    - (a) Melodee's flags: the FPU for the UI, the audio path kept integer;
    - (b) ChoralRoot's soft-float.

    *Recommend (a).*
12. **The library:**
    - (a) 256 MY presets, in flash only;
    - (b) 128.

    *Recommend (a).*
13. **Phase 1 from now on:**
    - (a) free design; the device-size tests advise but do not block;
    - (b) device sizes enforced on every merge after P2.0.

    *Recommend (b) for containers and memory, (a) for screens until Q1 is settled.*

---

## Appendix: how to repeat the measurements

The audit's tools are in its scratch folder, which is temporary. Each line below is enough to rebuild them.

- **pi32v2 sizes.** Run `debian:bookworm-slim` in Docker with `~/.jieli/toolchain` mounted at `/opt/jieli` and the
  FMToneSeq tree read-only. Then, for each core source (the `CORE_SRC` of `tools/Makefile`) and each `plutovg/source/*.c`:

  ```
  /opt/jieli/pi32v2/bin/clang -target pi32v2 -Os -ffunction-sections -fno-builtin -w -idirafter /opt/jieli/pi32v2/include -D__STDC_NO_ATOMICS__ -DPLUTOVG_BUILD -DPLUTOVG_BUILD_STATIC -DPLUTOVG_DISABLE_FONT_FACE_CACHE_LOAD
  ```

  Add `-mcpu=r3 -mfprev1 -ffp-contract=off` for the FPU build. Read the results with `arm-none-eabi-readelf -S`
  (section sizes) and `-nm` (soft-float calls).
- **Kept code and layouts.** The same sources with `arm-none-eabi-gcc -mcpu=cortex-m4 -mthumb -Os -ffunction-sections
  -fdata-sections`, the glue and `core-glue/common/nor.c`. Link with `--specs=nano.specs --specs=nosys.specs
  -Wl,--gc-sections` from `fm1core_get`, then read the map. Struct layouts come from `arm-none-eabi-gdb -batch -ex
  'ptype /o qd_app_t'`; stack from `-fstack-usage`.
- **Heap and CPU.** Build `tools/Makefile`'s objects with a force-included header that maps malloc, calloc, realloc
  and free onto a per-call-site counter. Build `app.c` with `-Dqd_app_frame=qd_app_frame_real`, so every frame is
  timed. A bench drives `qd_app_*` as the glue does, scene by scene.
  - `bench seqpeak` gives the queues' high water.
  - `bench flash N` copies N patterns and writes them through `qd_persist` into a RAM NOR, then reads the page count.
- **The calibration.** Build `CR/tests/cr_draw_test.c` with a timing `main` (`#define main crd_main` and
  `#include` it), `cc -O2 -I CR/build/gen -I CR/firmware/src -I CR/tests … CR/firmware/src/cr_engine.c`, and time 200
  forced `render()` calls per state.
