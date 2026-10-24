# Smooth screens and steady LEDs on the FM-1 (clean-room notes)

2026-10-23. Written for FIZZ on fm1-base. Sources read: **JIANT 0.9.9** (juanjiant-bit/JIANT `0912abb`, GPL-3.0-only,
the copy at `FM1VST/.claude/worktrees/cores-fw/cores/JIANT`), **fm1-base** `ced9443` with felucca-bsp (MPL-2.0, in
`firmware/fm1-base`), and FIZZ's own port (`firmware/app/app.c`, `core/app/view.c`, `core/gfx/qd_gfx_fast.c`,
`core/app/ctl_leds.c`).

**Clean room.** Section 2 describes what JIANT does, in prose, numbers and my own pseudocode. It contains no JIANT code
and none of its identifiers. Whoever implements this for FIZZ must not open the JIANT files listed in section 5.
Names in `code font` are FIZZ's or fm1-base's (both MPL-2.0). fm1-base and JIANT both descend from Felucca, so a few of
fm1-base's LED and LCD files look like JIANT's. FIZZ takes those files from **fm1-base only**. Their MPL-2.0 copies
are the ones FIZZ may use.

---

## 0. In short

> **Owner's decision (2026-10-23, final): fixes A, B, C and D first** (one instant per frame, the deferred-send
> stash, skipping unchanged strips, the LED fixes), keeping full colour and anti-aliasing. The owner first picked a
> 256-colour palettised full frame, then, once this study showed the tearing is FIZZ's own bug, chose these fixes
> instead. **Section 4.8 (the palettised frame) is the fallback**, to build only if the unit still isn't smooth after
> A–D. Sections 1–3 are unchanged background.

| Symptom | Cause (found in FIZZ's code) | Fix | New RAM |
| --- | --- | --- | --- |
| "Each segment lagging behind": the motorcycle is cut into pieces | Each 40-row strip is drawn at a **different moment**. `view_frame` calls `view_draw` once per strip, and `view_draw` fills the view models from `a->ms`. Between strips, `fz_strip_out` calls `fz_input`, which advances FIZZ's clock (`fz_tick_to(plat_ms())`) and applies presses. So strip 5 shows the bike ~45 ms later than strip 0. | **A. One instant per frame:** latch the frame's time and fill the models once, then draw every strip from that snapshot | ~0 |
| "Slowly updating", strip after strip | The frame takes ~50–60 ms to draw, and each strip goes to the LCD as soon as it is drawn. The screen changes in six steps spread over three or four panel refreshes. | **B. Deferred send:** copy the changed rows into a stash that borrows FIZZ's 27 KB drawing arena (idle during a frame), then send them in one burst of ≤ 5 ms at the end of the frame. **C. Draw less:** skip strips whose draw calls did not change. | ~0.3 KB (B), < 0.1 KB (C) |
| "The LEDs are all flickering" | (1) FIZZ's DIM and MID levels both map to fm1-base's **mid level**: lit on 1 scan frame in 8 (~114 Hz, 12.5 % duty), with **no glow underneath**, and **every** mid LED on the **same** frame. (2) FIZZ's locks use a global interrupt-off (`plat_lock` → `cli`, up to ~1.9 ms measured), and so do the flash page writes (~1 ms each). Both stall the 10 kHz scan, so one column stays lit 10–20× too long while the rest go dark. (3) Breathing levels jump between mid and lit at the 128 threshold. | **D.** Stagger the mid frame by column, put the glow under mid, mask only the audio interrupt in FIZZ's locks, blank the LED lines around flash work, and (optionally) give the scan true per-LED levels by sigma-delta | ~0.1 KB |

JIANT's smoothness does not come from a full frame buffer (it has none) or from tearing-effect sync (the FM-1 has no
TE line). It comes from these:
- every frame is drawn from one consistent state;
- each animated region is composed whole in RAM before any of it is sent;
- only the parts that changed go to the panel;
- drawing is cheap and integer-only;
- motion is driven by the clock, not by the frame count.

FIZZ can have all of these within its budget. The LED steadiness comes from the scan's interrupt priority and from
how the in-between levels are spread over time. fm1-base already has most of that; FIZZ is not using it well.

---

## 1. The FM-1's display and LED hardware (common ground)

- **LCD:** an ST7789-class 240×240 IPS panel on SPI1, written as RGB565 big-endian. There is no TE (tearing-effect)
  line on the FM-1's board. fm1-base notes that reading the scanline back (RDSCL) and timing writes to it did not stop
  tearing on hardware. Nobody on this platform syncs to the panel, so "tear-free" can only mean that a frame's changes
  reach the panel inside one panel refresh (~16.7 ms at the panel's default ~60 Hz).
- **SPI clock:** the divisor counts down from a 60 MHz peripheral clock. **fm1-base runs it at /1, about 44 MHz
  effective (measured), so a full screen (115,200 B) takes ~21 ms.** JIANT runs it at /5 = 12 MHz, ~77 ms a screen.
  Transfers are DMA from RAM, started by writing an address and a count, completed by polling. The CPU is free while
  one runs.
- **CPU:** 360 MHz (fm1-base's clock setup). The audio interrupt (ALNK0) and the key/LED scan (TIMER5, 10 kHz) are the
  two interrupts that matter.
- **LEDs:** an 11-column matrix shared with the keys, driven through a pair of 74HC595 shift registers (column select)
  and four LED row lines. The scan latches one column per TIMER5 tick (100 µs) and lights that column's LEDs until the
  next tick. One scan frame is 11 ticks = 1.1 ms (~909 Hz). A "lit" LED is on for a whole tick of each frame.
  Anything dimmer has to be made by shortening that pulse or skipping frames.

---

## 2. JIANT's techniques, in my own words

### 2.1 Memory map and the display's share

JIANT's linker regions (one 512 KB SRAM):

| Region | Size | Display use |
| --- | --- | --- |
| Main RAM (.data/.bss) | 96 KB | a 240-pixel fill line (480 B), a 64-byte command scratch, the screen-state structs |
| Big zeroed pool | 320 KB (0x50000) | **one 240×150 RGB565 canvas, 72,000 B**; **7,680 B** of cell hashes (1,920 × 32-bit); a few small tables |
| A low buffer below RAM | 28 KB | undo, song staging (not display) |
| RAM code at the top | 24 KB | flash driver, second core's loop, the hottest audio code |
| No-init | ~15 KB | crash and boot records |

So JIANT spends about **80 KB** on the display. It has **no full-frame buffer** (that would be 115 KB in RGB565). It
has no palettised frame either. Everything is drawn in RGB565 straight into the one canvas.

### 2.2 Sending to the LCD

- One DMA transfer per region: set the column and row window, send RAM-write, start the DMA, return without waiting.
  A "busy" flag records that a transfer is in flight. Before the next LCD command, and before anything writes into
  the canvas the DMA reads from, the code waits for the transfer to finish and raises chip-select. The main loop
  therefore runs on while the last region goes out.
- **Fills** do not need a buffer the size of the area. One row of the colour (up to 240 px) is repeated, as many whole
  rows per transfer as fit in 240 pixels. The panel keeps writing through chip-select going high between transfers,
  so one window takes several transfers.
- **A sub-rectangle of a wider buffer** goes out as one window with one transfer per row: the panel's write pointer
  just continues.
- **Overlays with holes.** While a dialog box sits over a live, animated page, up to three rectangles are declared as
  holes. Every send from the page underneath is split into the bands above, below, left and right of each hole, so
  the page keeps moving around the box without painting over it. No second buffer is involved.
- Panel set-up is the usual short ST7789 list: reset, sleep out, 16-bit colour, default scan direction, inversion on
  (IPS), normal mode. There are no gamma, porch or frame-rate commands, and no TE.

### 2.3 Composing a region whole before sending it

The screen is cut into fixed **regions**: header, a row of four cards (one canvas each), the big middle "graph panel"
(240 wide, up to 150 tall), footer, tab bar. Each region is drawn into the canvas in full (cleared, background,
artwork, text) and then sent as one window. The middle panel holds all the large animations (organisms that swim and
pulse with the audio, oscilloscopes, envelopes), and it fits the canvas whole. **An animation is therefore never split
across two separately timed pieces.** That one choice is most of why JIANT's animations look solid.

### 2.4 Change detection at two levels

1. **Per region, before drawing: an input signature.** Each region computes a cheap hash of everything its picture
   depends on: page, track, engine, the parameter values shown, the selection, and, for animated content, a frame or
   time term. If the signature equals last frame's, the region is not drawn and not sent. An idle screen costs almost
   nothing; only the animated regions are drawn each frame.
2. **Per cell, after drawing: a content hash** (used where a region is redrawn every frame because something moves
   under everything, e.g. a full-screen animated backdrop):
   - Cut the canvas into **8×4-pixel cells**. Hash each cell's pixels with a 32-bit FNV-1a, store it, compare with the
     previous frame's.
   - In each row of cells, find the runs of consecutive changed cells and send each run as one window (one transfer
     per pixel row of the run).
   - The hash slots are allocated per region (position + size identify a region). If a region is new, or the table is
     full, it is sent whole and everything is re-hashed on the next frame. A forced redraw (page change, wake) also
     re-hashes and sends everything.
   - The rationale given: at 12 MHz the whole screen took ~77 ms (~12 fps). Sending only changed cells is what made
     the full-screen animated page run at the main loop's pace.
   - 8×4 was chosen over 8×8 (sends ~15 % more pixels) and 4×4 (twice the hash memory).

### 2.5 The frame loop: one state per frame, paced

The main loop does, in order: housekeeping → **read input and apply it** → per-frame logic → **LEDs** → **draw** →
then spin, polling input only (no drawing), until **15 ms** have passed since the loop began (≤ ~66 fps). Input that
arrives while a frame is drawing waits for the next loop, so **a frame is always drawn from one consistent state**.
A frame counter increments once per frame. A user "load" setting with two lighter modes caps the animated panel at
one redraw per **50 ms** or **70 ms** (~20 / ~14 fps), halves or thirds the organisms' vertex counts, and turns off
purely decorative motion (rolling digits, glides). The time goes to keys, knobs and audio instead.

### 2.6 How it draws

- **No stored artwork for the animations.** Organisms, anatomy-chart line art and scopes are drawn by code each frame
  with integer maths: positions in 1/16 px (Q4) so motion is sub-pixel smooth; angles as 16-bit turns through a sine
  table; Bézier curves as a handful of short segments.
- **Anti-aliased lines, the cheap way:** walk the major axis one pixel at a time. At each step the minor coordinate
  (Q8) falls between two pixels; plot both, with coverage `(1 − frac)` and `frac` of the line's alpha, blended over
  what is there. Two pixels per step, no square roots, no area integration. About **400 such lines a frame** for an
  organism page (96 vertices per outline at full load; 64/48 or 32/24 in the lighter modes).
- **Rounded rectangles** with anti-aliased corners from a small coverage table (4×4 samples per corner pixel).
- **Text:** fonts are rasterised **at build time** into 4-bit alpha glyphs trimmed to their ink box, with advances in
  1/16 px. Each glyph is pre-rendered at 4 horizontal sub-pixel phases (2 for the large face), and the drawing picks
  the phase nearest the true pen position. Some faces are Huffman-coded, decoded through a 256-entry table. Blending
  goes through a **16-entry colour ramp per (ink, background) pair**, cached (6 pairs), so a text pixel costs one
  nibble read and one store. There is an optional 2-pixel "halo" outline for text over busy pictures.
- **Theme:** five stored colours per palette; the others are fixed blends computed once when the palette changes.

### 2.7 Keeping motion smooth when frames are slow

- Animations advance by **elapsed time, not by frames.** Each animated thing remembers the time (and frame) it was last
  advanced. Its phase moves by `elapsed_ms × rate`, with elapsed time **clamped to 100 ms** (frame steps clamped to 8),
  so a stall never makes it jump. With motion turned off the step is zero.
- Audio-reactive levels are slewed: fast up, slow down, sampled once per frame.
- Everything inside one frame reads the same frame number and time, so repeated draws within a frame agree.

### 2.8 LEDs

- **Scan in an interrupt that outranks audio.** TIMER5 at 10 kHz has a higher priority (4) than the audio interrupt
  (3), so the scan keeps its 100 µs rhythm while a half-buffer renders. The finding behind this: with the scan below
  audio, the column that was lit when a render started stayed lit for the whole render (0.7 ms idle, several ms
  loaded). That made a ~16 Hz flicker over all the LEDs, beating with the scan, and the encoders lost steps. Nested
  inside the audio interrupt, the scan only touches GPIO and the input state. USB polling waits for the first tick
  outside the render, except the USB-audio packet service, which must run every 1 ms.
- **Levels between dark and lit, all from the scan (no PWM peripheral):**
  - *Lit:* the column's whole tick.
  - *Glow* (~1/30 of lit; ~1/60 in a darker setting): a short pulse of ~3.2 µs (1.6 µs) at the start of the column's
    tick, every frame (~909 Hz, flicker-free). Its width is controlled by **when the LED lines are switched off
    during the 16-bit shift-register write** for the next column. The 24 MHz timer measures each pulse, and the number
    of shifted bits before "off" moves up or down by one per tick to track the target. The width self-calibrates.
  - *Mid:* lit on 1 scan frame in N (N = 8 → ~12.5 %, a pulse every ~8.8 ms, ~114 Hz; 12 in the darker setting), with
    the glow under it on the other frames. **The columns take turns:** column c is lit on the frame where
    `(frame_phase + c) mod N == 0`, so only one or two columns are on the mid level in any frame. Before this,
    every mid LED lit on the same frame. That made a current pulse at ~114 Hz, which was *heard* from the speaker and
    is a visible flicker on a whole keyboard of mid-level keys.
  - *High:* a brighter steady level, 3 of 8 frames (~37 %), on top of the glow.
  - *Breath:* a 1024-frame (~1.13 s) cycle, smoothstep then squared (perceptual), peak ~60 % (30 % in the darker
    setting). Below the glow's brightness it is a share of the glow pulse, dithered over 8 frames. Above it, whole lit
    frames are mixed with glow frames by a **first-order sigma-delta**, which spreads the lit frames as evenly as
    possible. All breathing LEDs share one phase, computed once per scan frame. No divisions in the interrupt.
  - *Per-LED animation* (a power-on sweep): each LED has a level 0..64 and its own sigma-delta accumulator. Once per
    scan frame, add the level; if the sum reaches 64, light that LED this frame and subtract 64. The glow sits under
    the low levels. The longest dark run at the lowest level (4/64) is 16 frames (~18 ms, ~57 Hz).
- **No dark gaps on update.** The UI builds the whole LED picture in local arrays (lit, glow, mid, high, breath per
  column), then copies it out one byte per column, **glow and the in-between levels first, lit last**. An LED moving
  from lit to a dimmer level is never caught dark by the scan.
- **Design rule:** things that invite a press *breathe* rather than hard-blink ("never a hard blink in a dark room").

### 2.9 Audio coexistence

- Audio renders in the ALNK0 interrupt (priority 3) on core 0, 128 frames per half (2.9 ms at 44.1 kHz). Some tracks
  are handed to the **second core** as a job; the interrupt waits for it before returning. The second core's idle
  loop runs from RAM.
- The hottest audio code is **copied to RAM** at boot, so UI code running from flash (XIP) cannot evict it from the
  cache.
- Load guards: past ~65 % of a half the engine tightens; past 80 % it sheds. The lighter "load" modes also thin the
  UI (2.5).
- The LCD uses polled DMA with no SPI interrupt. The only other interrupt is the scan, which nests in audio and
  stays short.
- The UI main loop never masks interrupts for long. Its critical sections only read or clear a few input counters.

---

## 3. What fm1-base already offers (MPL-2.0, FIZZ may use it freely)

| Facility | Where | FIZZ uses it? |
| --- | --- | --- |
| LCD at ~44 MHz, non-blocking DMA (`lcd_blit`, `lcd_fill`, `lcd_sync`), the busy-flag pattern | `device/lcd.c`, `bsp/boards/fm1/fm1_lcd_hw.h` | yes |
| The canvas (`cv_begin`/`cv_blit`, `cv_band` to send several bands in one transfer) | `device/gfx.c` (FIZZ: 240×16, its own screens only) | boot and crash screens only |
| TE sync | none (no TE line) | n/a |
| Scan at **priority 4, above audio** | `device/main_fm1.c` (`fm1_timer5_start(isr_timer5, 4)`) | yes (inherited) |
| Scan-gap statistics (`t5_gap_max`, `t5_late` = gaps > 300 µs, `t5_run_max`) | `device/main_fm1.c`, read by FIZZ's stats line 5 | read them (4.6) |
| LED **glow** (self-calibrating short pulse, two strengths) | `bsp/boards/fm1/fm1_input.h` (`fm1_led_dim`, `fm1_led_dim_level`) | yes, for FAINT |
| LED **mid** (1 frame in 8 / 12) | same file (`fm1_led_mid`), via `plat_lights_mid` | yes, for DIM and MID, but **all columns on the same frame and without the glow under it** |
| LED **breath** (hardware, sigma-delta, ~1.1 s, peak 60 % / 30 %) | same file (`fm1_led_breath`) | **no** (FIZZ breathes in software at 100 Hz, quantised to 4 steps) |
| Power-on sweep and an idle LED animation (per-LED sigma-delta levels) | `bsp/boards/fm1/fm1_led_anim.h`, `fm1_input.h` | no |
| Off-line LED picture copied one byte per column | `device/plat_fm1.c` (`plat_lights`) | yes, but `plat_lights` and `plat_lights_mid` write separately (lit before mid) |
| Per-interrupt mask (`fm1_irq_mask` / `fm1_irq_unmask`, `FM1_IRQ_ALNK0`) | `bsp/chip/fm1_irq.h` | **no**: `plat_lock` is a global `cli` |
| ~60 fps main-loop cadence (`app_frame` every ≥ 16 ms, input between) | `device/main_fm1.c` | FIZZ paces itself (33/50/66/100 ms) |

Missing from fm1-base, compared with JIANT: column-staggered mid, a "high" level, a per-region signature / per-cell
send helper. These are small additions FIZZ can write itself (section 4). Changes to fm1-base's own files must be
published under MPL-2.0.

---

## 4. Recommendation for FIZZ

Budget today (firmware/BUDGET.md, P2.6): main RAM ~2.6 KB free; pool a few KB above fm1-base's demanded spare.
Display RAM already spent:

| Item | Bytes |
| --- | --- |
| strip surface `qf_px` (240×40 RGB565) | 19,200 |
| stroke coverage `scovw` (40 × 256) | 10,240 |
| row hashes `fz_row_hash` | 960 |
| fm1-base canvas (240×16) | 7,680 |
| fill line | 480 |
| **drawing arena `QD_GFX_ARENA`** (fast drawing takes nothing from it; lent to `qd_tmp_borrow` **between frames only**, "never in a drawing") | 27,136 |

A full frame does not fit: RGB565 is 115,200 B, 8-bit 57,600 B, 4-bit 28,800 B. A palettised frame would also lose
FIZZ's anti-aliasing, glows and colour, so it is not recommended (4.7). The plan below adds almost no RAM.

### 4.1 A. One instant per frame (do first: it fixes the "segments lagging behind")

**Change (core, `core/app/view.c`; port, `firmware/app/app.c`):**
1. At the start of `view_frame` (strip mode), latch the frame's time. Use FIZZ's clock at that moment, or better
   (4.4) the expected presentation time.
2. Call every `fill_*` once, into the static models (`lv`, `hv`, … already exist), using the latched time instead of
   `a->ms`.
3. Per strip, only draw from those models. Split today's `view_draw` into `view_fill(a, t, …)` (once) and
   `view_paint(…)` (per strip).
4. Audit the paint functions for reads of live state (`a->…`, `qd_*` getters, `a->ms`). Either move each read into the
   models, or pass the latched time.

The port may keep calling `fz_input` between strips (a press still sounds within a strip's time), because the frame
being drawn no longer sees those changes; they appear in the next frame. Equivalent alternative: between strips, only
*queue* input. Keep the note-sounding part immediate, as already done for MIDI with `qd_app_midi_play`.

**Cost:** RAM ~0, plus a few bytes for the latched time. Flash < 0.5 KB. **CPU: a saving.** The models are filled
once instead of six times, and `qd_app_frame_changed`'s hash pass can reuse the same fill.

### 4.2 B. Deferred send: the whole frame's changes in one burst

The aim is that a frame's changes reach the panel within ≤ 5 ms, so they land inside one panel refresh, instead of
trickling out over the ~50–60 ms of drawing.

```
frame:
  stash = borrow(arena_free − reserve)      // the 27 KB arena, idle during a frame
  rows_in_stash = 0
  for each strip s (top to bottom):
      draw s into the strip surface          // from the frozen models (A)
      for each row r of s:
          h = hash(row r)
          if h != row_hash[y]:
              row_hash[y] = h
              if stash has room for 480 B:
                  copy row r to stash; record y
              else:
                  flush(); copy row r; record y        // overflow: send what we have now
      (input between strips as before, no LCD work)
  flush()                                    // the burst: contiguous y's → one window + one DMA each
  at the end (before anything else borrows the arena): lcd_sync(); give the stash back
```

- **Capacity:** (27,136 − reserve) / 480 ≈ **50 rows**, plus a 240-byte list of row numbers. That covers the velocity
  rider's moving band and most step-view animations. Bigger changes (a screen switch) overflow into two or three
  bursts: still far fewer seams than today's six, and all from one instant (A).
- **Burst time:** 50 rows = 24 KB at ~44 MHz ≈ **4.4 ms**, CPU free during it. Contiguous rows are stored
  contiguously, so a run of rows is one window and one DMA.
- **Copy cost:** ~50 × 480 B memcpy ≈ 0.1–0.2 ms at 360 MHz.
- **The arena's rules:** the stash is a borrow like any other (`qd_tmp_borrow`/`qd_tmp_return`), held only for the
  frame. Anything that borrows *during* a frame gets less room or none: the edits applied between strips (e.g.
  `seq/clip.c`'s undo levels), and a pattern load inside the audio interrupt (`isr_loads`, which must stay 0). Size
  `reserve` to the largest between-strip borrow (measure it with `qd_arena_peak`), or hold edits that borrow until
  after the frame. The DMA source must be in RAM (it is), and the DMA must finish (`lcd_sync`) before
  `qd_persist_step` or anything else borrows the arena.
- **More room, if needed:** shrink the strip to 24 rows. That gives back 7,680 B of surface and 4,096 B of coverage,
  for a stash of ~38 KB ≈ 80 rows. The cost is more strips per frame (10 vs 6); with A and C the extra cost per strip
  is small. Decide on the unit's numbers.
- **Finer granularity (optional):** two hashes per row (left/right half) let a row send only its changed half. That
  costs +960 B, which RAM cannot spare today; pool spare could hold it.

**Cost:** RAM ~0.3 KB (the row list and counters); the stash is borrowed. Flash ~0.5 KB. CPU ≤ 0.2 ms a frame.

### 4.3 C. Draw less: skip strips whose draw calls did not change

FIZZ already hashes a frame's draw calls without drawing (`qd_view_frame_hash`, P2.4). Extend it to hash **per
strip**: in hash mode, bin each call (its arguments + state) into the strips its bounding box touches. For paths, use
the transformed control-point box; the fast drawing already computes boxes for culling. Keep `nstrips` previous
hashes (6 × 4 B). In the drawing pass, a strip whose hash is unchanged is **neither drawn nor hashed per row**.

This is the strip-level form of "redraw a region only when its inputs change". When only the bike's band moves, two or
three strips are drawn instead of six. The frame time drops roughly in proportion, and B's stash rarely overflows.

**Cost:** RAM < 64 B; flash ~1 KB; CPU: the binning adds a little to the existing hash pass and saves whole strips of
drawing.

### 4.4 Smooth motion

- Keep motion time-based (it already is: `secs(a->ms)`, `fmodf(t, …)`). After A, use the **frame's latched time**.
  Optionally latch `t_present = now + expected_draw_and_send` (a running average of the last frames) so moving things
  sit where they will be when the burst lands.
- Clamp per-frame time steps for anything integrated (velocities, slews) to ~100 ms, so a slow frame or a flash erase
  does not make it jump.
- While a screen animates, aim for a **steady cadence** (e.g. 30 fps = 33 ms) rather than "as fast as possible".
  Even spacing reads smoother than a higher, uneven rate. With A + C the velocity screen should fit 33 ms; read
  `frame_avg_us` / `frame_max_us` per screen. If a screen cannot, step it to 20 fps (50 ms) rather than letting it
  wobble between rates.
- Optional, to verify on the unit: the ST7789's normal-mode frame-rate control (FRCTRL2, command 0xC6) can raise the
  panel's own refresh from ~60 Hz to ~100–119 Hz. With no TE line it cannot remove the one tear line per burst, but
  that line then stays on screen half as long. Cost: one init byte. Check that it does not change brightness or add
  flicker on this panel.

### 4.5 D. Steady LEDs

In order of expected effect:

1. **Mask only the audio interrupt in FIZZ's locks.** `fz_lock`/`fz_unlock` (via `plat_lock`) protect FIZZ's state
   from the audio interrupt. Nothing on TIMER5 touches FIZZ's state: FIZZ defines no `APP_TICK`, and the USB MIDI
   queue between TIMER5 and the audio interrupt is fm1-base's own and already safe. So the locks can clear ALNK0's
   enable bit (`fm1_irq_mask(FM1_IRQ_ALNK0)`, restore on unlock, nesting-counted) instead of `cli`. The scan and USB
   then keep their rhythm through every lock. Add it as a new `plat_` call in fm1-base (MPL: publish the change), or
   in the port. Cost: 0 RAM, a few instructions.
2. **Stagger the mid level by column** in fm1-base's scan (`bsp/boards/fm1/fm1_input.h`): column c is on the mid level
   on the scan frame where `(mid_phase + c) mod N == 0`, instead of all columns on `mid_phase == 0`. Each LED still
   gets 1 frame in N; the current per frame becomes even, and the ~114 Hz all-LED pulse goes away. Cost: one add and
   compare per tick.
3. **Put the glow under every mid LED.** In `fz_lights`, add the mid set to the dim set (today `mid_b`/`mid_k` are
   excluded from `dim_b`/`dim_k`). A mid LED then falls to the glow between its lit frames, not to dark: the flicker's
   depth drops from 100 % to ~75 %. Cost: none.
4. **One atomic LED update.** Write the glow, then mid, then lit, per column, in one function: merge `plat_lights` and
   `plat_lights_mid`. Alternatively, keep two LED pictures and let the scan swap them at the start of a scan frame.
   Cost: ≤ 66 B.
5. **Blank the LED lines before any long interrupt-off section** that fm1-base cannot avoid: a flash page write
   (~1 ms), an erase (~18 ms, only when FIZZ is quiet), a crash or update screen. A stalled scan then shows a brief
   dark dip instead of one column flashing bright. One register write in fm1-base's flash wrappers.
6. **Update the LEDs during long frames.** `fz_lights` runs every 10 ms only from `fz_step`, so a 60 ms frame delays
   it and blinks become irregular. Also call it from the strip callback when ≥ 10 ms have passed (it only reads).
7. **Real levels instead of four steps (optional, recommended).** Give the scan a per-LED level (41 LEDs × 6 bits) and
   a per-LED first-order sigma-delta, run once per scan frame, with the glow under any non-zero level. Seed the
   accumulators differently per LED (e.g. `(i × 37) mod 64`) so equal levels do not fire on the same frame. FIZZ's
   five levels (OFF/FAINT/DIM/MID/LIT) and its breaths (`qd_led_breath*`) then show as written, not snapped at 25/128.
   Keep the darkest levels on the glow pulse; the longest dark run is 64/level frames, so use sigma-delta only from
   level ≥ ~8/64 (≤ 8 frames, ≥ ~114 Hz). fm1-base's power-on sweep already does exactly this for its animation, so
   the method is in MPL code FIZZ may reuse. Cost: ~82 B RAM, ~3–5 µs per 1.1 ms scan frame (~0.4 % CPU).
   - Cheaper alternative: map FIZZ's breathing LEDs to fm1-base's hardware breath (`fm1_led_breath`). The shape
     differs (dark→60 % instead of 60→100 %), but it is smooth.
8. **Fewer hard blinks.** Where a blink only means "this can be pressed", breathe instead (a design choice, as in
   JIANT). Keep the fast blinks that carry information.

### 4.6 Order of work and how to check it

1. **A** (core + port). Check on the host: every strip of a frame equals the same frame drawn whole
   (`qd_view_strip_check` already compares strips). On the unit: the rider no longer shears.
2. **D1–D4** (port + a small fm1-base change). On the unit: fm1-base's `t5_gap_max` < ~200 µs and `t5_late` ≈ 0 while
   playing and drawing (stats line 5). Film the keyboard with a phone's 240 fps slow motion: mid keys pulse 1 in 8,
   a column at a time, with no long bright columns.
3. **B**, then **C**. Add to the stats: rows sent per frame, bursts per frame (1 = whole frame at once), stash
   overflows, strips skipped. Target on the velocity screen: 1 burst per frame, ≤ 5 ms per burst, frame ≤ 33 ms.
4. **D7**, if the owner still sees stepped breathing.

Summary of costs: **RAM ≈ +0.5 KB** (A ~0, B ~0.3 KB with the stash borrowed, C < 64 B, D ~0.15 KB). **Flash ≈
+2–3 KB.** **CPU:** A and C save more than B and D cost. The LCD burst is DMA.

### 4.7 Considered and rejected

| Option | Why not |
| --- | --- |
| Full RGB565 back buffer | 115,200 B; nowhere near available |
| 8-bit palettised full frame (57,600 B) | **The fallback (4.8)**, if A–D aren't enough on the unit. It needs ~33 KB of RAM found elsewhere (4.8.2) and a palette built for blending (4.8.3) |
| 4-bit palettised full frame (28,800 B) | Fits only by taking the arena plus ~1.7 KB, and 16 colours cannot show FIZZ's AA text, glows and gradients |
| TE-synced writes | No TE line on the FM-1; scanline read-back did not work on hardware (fm1-base's note) |
| Double-buffering inside the panel's own RAM (scroll start address) | The ST7789's frame memory has only 80 spare rows past a 240-row screen: not enough for a second frame; unverified for this module's offsets |
| Lower SPI clock as in JIANT (12 MHz) | fm1-base's ~44 MHz is already better; transfer time is not FIZZ's bottleneck (~3.5 ms a strip vs ~9 ms drawing it) |
| JIANT-style 8×4 cell hashes | 1,800 cells × 4 B = 7.2 KB for the whole screen; FIZZ's row hashes plus B and C give most of the benefit for 1 KB |

### 4.8 The fallback: an 8-bit palettised full frame

**The design:**
- One 240×240 frame of palette indices, 57,600 B, kept between frames.
- The fast renderer draws the whole frame once into it. The 40-row RGB565 strip goes, and so do the six repeats of
  the screen's logic per frame (P2.4 measured every path being outlined again for each strip).
- Changed rows are converted to RGB565 through a 256-entry table on the way to the LCD and sent in one pass.

What carries over from section 2 and fm1-base:

| Lesson | Applied here |
| --- | --- |
| JIANT composes an animated region whole before sending any of it (2.3) | The whole frame is composed before any of it is sent. A frame cannot shear: draw, then present. |
| JIANT and fm1-base: DMA only from RAM, non-blocking with a busy flag, sync before touching the source (2.2, fm1-base `lcd.c`) | DMA from a small RGB565 **line buffer** filled from the palette; two of them, ping-pong |
| One window can be fed by many transfers; the panel keeps writing across chip-select toggles (JIANT's per-row sends; fm1-base's own `lcd_fill` relies on it) | One window per run of changed rows, one transfer per row |
| Region signatures and cell hashes (2.4) | Bands whose draw calls did not change are not redrawn (fix C) and not sent. Inside redrawn bands, row hashes of the 8-bit rows pick the rows to send. |
| JIANT's 16-step blend ramp per (ink, background) pair (2.6) | Palette ramps laid out so most anti-aliased blends are index arithmetic (4.8.3) |
| No TE line on the FM-1 (fm1-base's note; JIANT doesn't sync either) | Keep each send short; what TE would need is in 4.8.5 |
| fm1-base's SPI at its fastest divisor (~44 MHz effective; the ST7789 tops out near 62 MHz) | No headroom on the clock: send fewer bytes instead |

#### 4.8.1 The output path: palette to line buffer to DMA

```
present():                                    // after the whole frame is drawn
  for each run of changed rows [y0, y1):
      set the window (0, y0) .. (239, y1 − 1); RAM write
      k = 0; convert(row y0 → line[k]); start DMA(line[k], 480 B)
      for y in y0 + 1 .. y1 − 1:
          k ^= 1
          convert(row y → line[k])            // while the previous row is on the wire
          wait for the DMA; start DMA(line[k], 480 B)
      // leave the last DMA running; the next LCD access or conversion syncs first
convert(row → line):  for x in 0..239: line[x] = LUT565be[row[x]]   // the table already byte-swapped
```

- **Numbers.** A row is 480 B, ~87 µs on the wire at ~44 MHz. A full frame is ~21 ms, longer than one panel refresh
  (~16.7 ms), so full-screen changes (screen switches) cannot avoid one tear. Typical animation frames send tens of
  rows, ~2–5 ms. Conversion is ~3 µs per row at 360 MHz (a byte load, a table load, a store per pixel), well under
  the row's 87 µs, so the wire is the limit.
- **RAM.** 2 × 480 B line buffers, plus the 512 B table (`uint16[256]`, kept in big-endian order for the panel).
- **CPU during a send.** The simple version waits on each row's DMA from the main loop: the send's whole duration,
  ~2–5 ms typical. If the profile shows it matters, chain the rows from the SPI1 interrupt (`FM1_IRQ_SPI1` in
  fm1-base's `fm1_irq.h`). Give it a priority below the audio interrupt (a render then only pauses the send) and
  below the scan. Each interrupt converts the next row and restarts the DMA (~3 µs per 87 µs, ~4 % CPU while
  sending), and the main loop is free. This is new port code: fm1-base's driver is polled.
- **Fewer, longer transfers.** If per-transfer overhead shows in the measurements, use 2-row line buffers
  (2 × 960 B).
- **Narrower sends (optional).** Two or four hashes per row (left/right halves or quarters, +960/+2,880 B) let a run
  be sent as a narrower window. Only worth it if wide rows with small changes dominate.
- **Palette effects.** Changing a palette entry changes nothing on the panel until the pixels using it are resent.
  A palette fade is a full resend (~21 ms), not free.

#### 4.8.2 Where the 57.6 KB lives

FIZZ's single-core build today (`firmware/app/build/fizz-size.txt`): RAM 2,596 B free; pool 2,880 B free (this build
lowers fm1-base's spare to 4,096); RAM code region 1,064 B; NOINIT 3,032 B. The fast renderer's unit holds 65,698 B in
RAM: the strip 19,200, the stroke coverage 10,240, the arena 27,136, and ~9.1 KB of path, row and glyph buffers.

| | Bytes |
| --- | --- |
| New: the frame | 57,600 |
| New: RGB565 table 512, line buffers 960, band hashes 15 × 4 × 2 = 120 | 1,592 |
| Freed: the RGB565 strip | −19,200 |
| Freed: stroke coverage cut from 40 rows to a 16-row band (strokes rendered band by band inside the renderer) | −6,144 |
| **Still to find** | **33,848** |

The frame does not need to be contiguous. Split it into two bands, with the row's base address computed once per row
in the pixel loops (`y < split ? A + 240·y : B + 240·(y − split)`).

**Where to find it** (bytes; the owner picks):

| Source | Gives | Cost |
| --- | --- | --- |
| The delay line 64 → 32 KB (pool, `qd_fx.c`) | 32,768 | Half the longest delay (the owner chose 64 KB earlier) |
| The delay line 64 → 48 KB | 16,384 | A quarter of it |
| fm1-base's canvas (240×16 RGB565, 7,680 B in the pool) aliased onto the frame | 7,680 | A small fm1-base change: the app supplies the canvas memory. fm1-base's own screens (boot, crash, update, calibration) never run while FIZZ draws; FIZZ resends everything after them. |
| The comb lines 8 → 4 (`QE_NCOMB`) | 8,192 | E2's comb on fewer voices |
| RAM free today | 2,596 | No RAM margin left |
| The arena trimmed toward its measured peak (22,776 + margin) | ~2,500 | Less room for borrowers |
| The user stack (24 KB) trimmed after measuring its high-water mark | unknown | Measure first |

- **Layout A (simplest):** halve the delay line.
  - Rows 0–99 (24,000 B) in RAM, where the strip and coverage were. RAM keeps ~2.3 KB free.
  - Rows 100–239 (33,600 B) in the pool, from the delay's 32 KB plus 832 B of the pool's free space. The pool keeps
    ~2 KB free.
- **Layout B (keeps ¾ of the delay):** delay to 48 KB + canvas aliased + comb lines to 4 + today's free RAM =
  34,852 B. That fits, with ~1 KB to spare: too tight to recommend without first measuring the arena and the stack.
- **Standard (two-core) build:** comb lines in the RAM code region, 4.7 KB free there. Same arithmetic, with the
  comb-line lever replaced by that region's free space.
- **Both:** the frame is ordinary zeroed memory (pool or `.bss`). Only the line buffers are DMA sources; RAM and the
  pool are both the chip's SRAM.

#### 4.8.3 The palette: built for the blends the renderer makes

The renderer blends in RGB565 values (gamma-encoded, not linear light): `c = dst + (src − dst)·a`. The palette should
make the common blends exact index arithmetic and leave a table for the rest.

**Layout: 16 ramps × 16 levels, index = 16·h + l.**
- Level `l` of ramp `h` is the ramp's top colour × `l/15`, per channel, **in the same RGB space the renderer blends
  in**. Drawing ramp colour (h, L) at coverage `a` over black is then exactly level `round(L·a)` of the same ramp: the
  renderer's own result, quantised to 16 steps.
  - Two colours on one ramp blend to another level of it: `round(Ld + (Ls − Ld)·a)`.
  - Global alpha (fades, fireworks) and anti-aliased edges over black, which cover most of FIZZ's pixels, never leave
    their ramp.
- **Ramp tops:** each hue's *brightest* use, because darker uses (DIM versions, faded trails) are lower levels.
  Suggested assignment, to be fitted by a tool:

  | Ramps | Tops |
  | --- | --- |
  | h = 0 | neutral grey to white |
  | h = 1–8 | the eight track colours (`UI_TRACK_PAL`) |
  | the next few | the other UI hues: the rider's red, blue and cream (`RN_*`), the accents |
  | one | the cool UI greys `UI_G1`–`UI_G3` (`0x8d8d96`, `0x4b4b54`, `0x26262d` sit close to 15/15, 8/15 and 4/15 of one blue-grey ramp; the tool should confirm they land exactly or pick the top so they do) |
  | the rest | **pair ramps** (below) |

- **Level 0 is black in every ramp.** The renderer always writes index 0 for black, so the 15 slots `16·h + 0` for
  h ≥ 1 are spare. Use them for solid colours that sit on no ramp: pure white if no ramp tops at it, tints lighter
  than a hue's top. Ramp arithmetic never produces them; the general path (below) can.
- **Pair ramps.** Anti-aliasing is dominated by a few (ink, background) pairs (the observation behind JIANT's cached
  16-step ramps). FIZZ draws mostly on black, which the ramps above cover. For each common non-black pair, e.g. cream
  or white text on the `UI_G3` panel grey, or a track colour on `UI_G3`, give the pair its own ramp from background
  (l = 0, here *not* black) to ink (l = 15). Edges of that ink on that background are then exact. Find the pairs by
  instrumenting the host renderer: count blended pixels by (source, destination) colour over every screen.
- **Everything else: a nearest-colour table.** Blend the two palette colours in RGB565 exactly as today, then look up
  the result in a **4,096-entry RGB444 → index table** (4 KB, `const` in flash, generated with the palette by a
  build-time tool). Use a perceptual distance (green-weighted, or "redmean"). Before taking the top 4 bits of each
  channel, add a fixed 4×4 ordered-dither offset of ±½ step, chosen by screen position. Translucent glows over
  coloured areas then dither instead of banding, and a static picture stays static, so row hashes stay stable.
  Optional refinement: also try the two on-ramp candidates (the source's ramp and the destination's ramp, at the level
  matching the blend's brightness) and keep the nearest of the three. Edges then tend to stay on one of their two hues.
- **Zero-byte fallback** (if flash is short): for a mixed-hue edge, take the ramp of the larger contributor (`a ≥ ½`
  → source, else destination) and the level whose luminance matches the blended luminance. That uses a 16-entry
  luminance table per ramp, 256 B in all. Brightness at the edge stays right; hue errors stay at sub-pixel edges,
  where they show least.

**Per-pixel logic in the renderer** (per paint: the source index `s`, its ramp and level, its RGB; per pixel:
coverage `c`, 0..64, times global alpha):
```
if c is full:              dst = s                                   // most pixels
elif dst == 0 (black):     dst = 16·h_s + round(l_s · c)
elif ramp(dst) == h_s:     dst = 16·h_s + round(l_d + (l_s − l_d)·c)
else:                      dst = nearest(blend565(pal[dst], pal[s], c), x, y)   // the table, dithered
```
- **Text** from FIZZ's 4-bit atlases: per paint, precompute a 16-entry index ramp (16 B, one entry per atlas
  alpha). Over black it is the ink's own ramp; over a uniform non-black background it comes from that pair's ramp or
  from the table. Cache a few (ink, background) ramps. A text pixel is then a nibble read, a table read and a store.
- **Strokes** keep their "largest coverage of the pieces, blended once" rule, in a 16-row coverage band that the
  renderer steps through for each stroke.

**Quality gate (host):** palettise every screen `render_frames` makes and compare with today's RGB565 frames
(`compare_frames.py`). Per screen, report:
- solid UI colours not exactly in the palette (should be 0);
- PSNR;
- the worst pixel error;
- the share of pixels that took the table path.

Tune ramp tops and pair ramps until the owner can't tell them apart on the unit.

**Theme changes:** FIZZ has one theme today, so there is one palette and one 4 KB table. A runtime theme switch
would need one table per theme in flash (4 KB each), or a rebuild into RAM, which isn't available.

#### 4.8.4 Dirty regions with a persistent frame

- **Bands (fix C, now cheaper and more useful).** Hash each 16-row band's draw calls in the existing hash pass (15
  bands). A band whose hash is unchanged is not cleared, not drawn and not sent: the frame still holds it. On the
  velocity screen only the bike's and the world's bands are redrawn.
- **Rows.** In a redrawn band, hash each 8-bit row (240 B, half today's 480 B). Send only the rows whose hash changed
  (`fz_row_hash`, 960 B, already there).
- **Full sends:** at boot, after fm1-base's own screens, after a palette change, and after a forced redraw.
- **Consistency (fix A) still applies.** Draw the whole frame from one latched instant and one fill of the models,
  then present. Input that arrives while drawing is queued, with note-sounding kept immediate. Because nothing is
  sent until the frame is complete, a frame can no longer mix two moments.

#### 4.8.5 TE sync: not on this board, and what it would take

- The FM-1 has no TE line wired (fm1-base's note). Reading the panel's scanline over SPI and timing writes to it did
  not stop tearing on hardware. So **no sync**. What keeps it clean instead: complete frames (no shear), short sends
  (changed rows only, ~2–5 ms), and a steady frame rate. At most one tear line crosses the moving band, for less than
  one panel refresh.
- **If a TE pin is ever found on a GPIO** (a board probe), the rule is to write faster than the panel scans.
  - Today the write is slower than the scan: ~87 µs a row at ~44 MHz, against the panel's ~70 µs a line at ~60 Hz.
    Started at the TE pulse, the scan would overtake the write.
  - Set the panel's frame rate to ~40 Hz (FRCTRL2, 0xC6): the scan becomes ~100 µs a line. A send started at the TE
    pulse then stays ahead of the scan, and any send of up to one frame is tear-free.
- **12-bit output (experiment only).** The ST7789 accepts 12-bit RGB444 pixels (COLMOD). It has no 8-bit indexed
  mode. A row becomes 360 B (~65 µs), and a full frame ~16 ms, about one refresh. Palette entries converted to 4:4:4
  lose the lower ramp levels' precision (16 steps per channel), so judge it on the unit before adopting it.

#### 4.8.6 Costs and order of work

| | RAM | Flash (estimates) | CPU |
| --- | --- | --- | --- |
| The frame and its output path | +33.8 KB net, from the levers in 4.8.2 | ~1 KB | Conversion ~0.7 ms for a full frame; the send waits on the DMA unless chained |
| The 8-bit paths in the fast renderer | (counted above) | ~3–5 KB | About the same per pixel; the screen logic runs **once** a frame instead of six times |
| Palette, RGB565 table, text ramps | ~0.6 KB | <1 KB + the 4 KB nearest-colour table | — |
| Band skip and row hashes | 120 B (the row hashes exist) | ~1 KB | Saves whole bands of drawing |

Order of work:
1. Palette tool and host quality gate.
2. 8-bit renderer on the host (pixel goldens against the palettised RGB565 renders).
3. Free the RAM (the owner's pick in 4.8.2).
4. Output path on the unit, polled.
5. Band skip.
6. SPI-interrupt chaining, if the send's wait shows in the profile.

Fixes A and D are independent and can go first.

---

## 5. Do not copy: the JIANT files read for this note

JIANT is GPL-3.0-only. The implementer must not open these files, or copy from them or from any other JIANT file.
Copy: JIANT 0.9.9, commit `0912abb`, at `FM1VST/.claude/worktrees/cores-fw/cores/JIANT/`. The same files exist in the
other `cores/JIANT` worktrees and at github.com/juanjiant-bit/JIANT.

- `firmware/app.ld` (memory map)
- `firmware/hal/fm1_lcd_hw.h`, `firmware/src/lcd.c` (LCD transport)
- `firmware/src/gfx.c` (canvas, overlay holes, the cell-hash sender, text blending)
- `firmware/src/main.c` (main loop, timer interrupt, boot LED call)
- `firmware/src/panel.c` (LED settings)
- `firmware/hal/fm1_input.h` (scan, glow, mid, high, breath, sweep)
- `firmware/hal/fm1_led_anim.h` (power-on sweep)
- `firmware/hal/fm1_multicore.h` (header comment only), `firmware/hal/fm1_irq.h`, `firmware/hal/fm1_flash.h`,
  `firmware/src/audio.c`, `firmware/src/storage_hw.c` (searched, a few lines)
- `firmware/src/ui.c`, `firmware/src/ui_draw.c`, `firmware/src/ui_graph.c`, `firmware/src/ui_input.c` (LED picture),
  `firmware/src/ui_being.c`, `firmware/src/ui_lzhome.c`, `firmware/src/ui_engvis.c`, `firmware/src/ui_organic.c`
  (parts)
- Also read (not JIANT code): `FM1VST/research/hub-2026-10-18/jiant.md`.

**Safe sources for the same ideas:** fm1-base and felucca-bsp (MPL-2.0): `device/lcd.c`, `device/gfx.c`,
`device/plat_fm1.c`, `device/main_fm1.c`, `bsp/boards/fm1/fm1_input.h`, `bsp/boards/fm1/fm1_led_anim.h`,
`bsp/chip/fm1_irq.h`. They share ancestry with some JIANT files (Felucca), but these copies are licensed MPL-2.0 to
fm1-base.
