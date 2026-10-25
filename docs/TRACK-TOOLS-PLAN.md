# TOOLS: the track tools (plan, 2026-10-24; T1 built the same day, §10)

Mock-up: `mockups/fizz-tools.html` (11 screens), PNGs `mockups/png/fizz-tools-01..11-*.png` and
`mockups/png/fizz-tools-sheet.png` (`python3 mockups/render_png.py tl-1 .. tl-11 sheet-tl`).

## 1. What OMX-27 offers (research)

- **Form sequencer TOOLS view** (research/FORM-SEQUENCER.md §9, v1.15.4): SHUFFLE, HUMANIZE, VEL RANDOM (min/max),
  CHANCE RND, EUCLID, GRIDS, QUANTIZE (a live-previewed morph), the PAGE tool (copy / cut / paste a page), one undo
  level for the track before every destructive tool.
- **OMX mainline** (okyeron/OMX-27, read 2026-10-24 at its 2026-04-12 head) has **Euclidean** and **Grids** as whole
  modes (`omx_mode_euclidean.cpp`, `euclidean_sequencer.cpp`, `omx_mode_grids.cpp`, `retro_grids.cpp`).
  `retro_grids.cpp` is Mutable Instruments Grids' data copied in: 25 nodes × 96 bytes (3 parts × 32 steps), read by
  bilinear mixing of four nodes (`ReadDrumMap`: x, y >> 6 picks the cell, `U8Mix` blends), a trig when the level
  passes 255 − density, plus perturbation (chaos) and accent.
- **Licences:** OMX-27 has no licence file (PROVENANCE.md already treats it as reference-only). MI Grids' firmware
  (pichenettes/eurorack) is **GPL-3.0**, and the node tables are part of that source. FIZZ is MPL-2.0, so **neither
  the tables nor OMX's copy may be used**. FIZZ gets a clean-room map (§4.5). Only the idea is used (an XY map
  between pattern styles, with density thresholds). Ideas aren't copyrightable.

## 2. Opening it and the controls (owner's answers, 2026-10-24)

**ARP held 400 ms opens TOOLS**, latched. **HOME closes it.** (Owner's final map, 2026-10-24.)

> **Superseded (T1b, 2026-10-25, the owner):** **SEQ tapped** opens TOOLS (SEQ tapped again or HOME closes it; SEQ lit while open); SEQ held stays the patterns; ARP held toggles the track's arp bypass; the project page moved to **SHIFT + SAVE**.

| gesture | does |
| --- | --- |
| ARP tapped (under `QD_ARP_HOLD_MS`) | the MIDI FX screen (unchanged) |
| **ARP held 400 ms** | **TOOLS**. This replaces the arp on / off toggle (`qd_mfx_arp_tap` / `QD_TK_MFX_ARP` at the hold) |
| SHIFT + ARP | FILL, at once, while held (unchanged, `b_fill`) |

- **Arp on / off** stays reachable on the MIDI FX screen (add / remove the ARP effect in the group's slots, or B10 bypass on its slot).
- **ARP's LED:** lit while FILL is held, and lit while TOOLS or the MIDI FX screen is open. Otherwise it shows the arp's state, as now.
- **Firmware edits** (for the coding agent): `b_fill`'s hold branch, the LED rule in `ctl_leds.c`, and the takeover is no longer raised on the hold.
- TOOLS opens the moment the hold reaches 400 ms (the existing `qd_ctl_ms` timer), so there's no wait for the release.

| control | in TOOLS |
| --- | --- |
| B1..B7 | the tool: ROTATE, REVERSE, RANDOM, EUCLID, GRIDS, HUMANIZE, LENGTH. The same key again: RANDOM / GRIDS reroll, ROTATE back to 0 |
| B8 / B9 / B10 | COPY / PASTE / CLEAR, for PAGE and PATTERN only (no whole-track copy, the owner's choice). CLEAR is pressed twice for TRACK or PATTERN |
| B11 | the scope: PAGE → TRACK → PATTERN |
| W1..W8 | the track. In GRIDS + PATTERN, a tap gives a track the next part |
| KNOB 1..4, SELECT | the tool's values. ROTATE's offset is on SELECT; GRIDS' chaos is on SELECT |
| **OCT+** | **keep** the live change: one undo level |
| **OCT−** | **revert** to the original. With nothing pending: close TOOLS |
| HOME | close; a pending change is **kept** (decided) |
| switching tool | keeps the pending change (decided); the new tool starts from the result |

**The tool's name pops up on a switch** (the owner's exception to the values-only rule):
- It's big (34 px), framed, and fades after ~0.8 s (mock-up screen 12).
- It reuses the existing takeover system (`ui_turn_t` / `UI_TK_*`, the same machinery as the TAP and FOLLOW takeovers): one new `UI_TK_TOOL` kind that draws a name from a 7-entry const table.
- Cost: 0 B RAM (the takeover state already exists), ~0.3 KB flash.

**Live preview: seen and heard.** It works like loading a sound:
- Every knob detent rewrites the pattern in place, so the player plays it with no change to the player.
- The orange dot by the tool icons means a change is pending.
- How it works:
  1. On the first change, the tool opens an **undo gesture and holds it open**: a flag stops `QD_UNDO_QUIET_MS` from closing it. The gesture's "before" (the compressed pattern at the top of the 16 KB log) is the original.
  2. Each later change runs *revert, then apply with the current values*. Revert is a new `qd_undo_revert_open()`: XOR the region back from the gesture's copy, the same code undo uses. Detents are combined, at most once per frame.
  3. OCT+ closes the gesture: one level.
  4. OCT− reverts and drops it.
- **Player hot path: zero added cost.** The player only sees ordinary edits, as with any knob edit. The player resolves steps two ahead, so a change can be heard one or two steps late.
- **RAM: 0 new**, because the "before" is the log's existing gesture copy.
- **CPU:** a revert plus apply is a few hundred µs for PAGE or TRACK, on the UI side, at frame rate.
- **Limit:** if the gesture's copy doesn't fit the log (PATTERN scope with 128-step tracks), live preview is off for that scope. The tool then draws its preview only, and OCT+ applies without an undo level; the crossed-out undo arrow shows this.

## 3. Scope

- **PAGE**: the edited page's start..end on the selected track.
- **TRACK**: the track's looped steps in play order (the looped pages, each start..end). Steps outside the loop
  aren't touched.
- **PATTERN**: every track, each on its own loop. EUCLID and RANDOM use the same values on every track, and RANDOM
  takes a different seed for each track. GRIDS hands out the parts LOW, MID and HIGH to the tracks chosen with W (by
  default the first three tracks of kind DRUM; failing that, tracks 1..3). LENGTH, ROTATE and REVERSE act on each
  track.

All of the code walks a **position list** `pos(i) → physical step` (qd_track_loop_step already does this), so the
ops see one array of n steps whatever the scope.

## 4. The ops

Generated steps are made with `qd_step_on`, so they have **no overrides** and take the track's defaults. GRIDS also
sets a velocity override from the level. Steps the generator removes are removed with their locks (the existing
remove path).

1. **ROTATE (B1).** SELECT sets the offset from the original, live and heard; OCT+ keeps it, OCT− reverts. B1 again sets the offset back to 0.
   The steps are permuted in place by **cycle-following with one 16 B temporary** (gcd(n, k) cycles). Locks: every
   lock of the track inside the scope has its step renumbered through the same map, then the track's lock range is
   re-sorted in place (insertion sort; it's already almost sorted, so it runs in about n·k). The micro offsets move
   with their steps.
2. **REVERSE (B2).** B2 again flips the pending result, so it's heard; OCT+ keeps it, OCT− reverts. Steps i and n−1−i are swapped in place. Micro becomes
   −micro (the timing mirrors, as Form's reverse playback does). Locks are renumbered and re-sorted as in ROTATE.
3. **RANDOM (B3).** B3 again rolls a new seed (the dice animation). KNOB 1 is the density 0..100 %, KNOB 2 the note
   spread in scale degrees around the track's default note (0 = keep the notes), KNOB 3 the velocity spread
   (0 = none), **KNOB 4 the chance spread** (CHANCE RND: each trig gets a chance override of 100 − spread·h). The values are a pure function of the seed: `h = hash32(seed, track, i)` (a 32-bit integer mix such as
   xorshift-multiply). A trig is made when `h & 0xFF < density`, the note is chosen from `(h >> 8)`, the velocity
   from `(h >> 16)`. The preview therefore needs **no buffer**.
4. **EUCLID (B4).** KNOB 1 sets the hits (0..len), KNOB 2 the length (1..16 for PAGE, up to the track's length
   otherwise; the pattern repeats after it), KNOB 3 the rotation, KNOB 4 how the result lands: NEW (replace), ADD (OR
   with the existing trigs), MASK (AND: keep only the trigs that are hits). Step i is a hit when
   `((i + rot) * hits) % len < hits`. That's the Bresenham form, which gives the same spacing as Bjorklund up to
   rotation. No table and no RAM.
5. **GRIDS (B5), clean-room.** KNOB 1 is X, KNOB 2 Y, KNOB 3 the density, KNOB 4 the part. FIZZ's own map has
   **3 × 3 nodes × 3 parts × 32 steps = 864 B of const flash** (decided: 32 steps, 3 × 3 to start). The nodes are **authored by FIZZ**: a script
   `tools/grids_map.py` (MPL-2.0) builds each node from stated rules. The rules are a metric weight (downbeats >
   backbeats > 8ths > 16ths), a per-node syncopation shift and swing emphasis, and part roles (LOW on the downbeats,
   MID on the backbeats, HIGH on the off-beats). The owner can then hand-tune it. The script writes
   `core/seq/grids_map.h`. No Mutable data is read or consulted while this is written. A level is the bilinear blend
   of the four nodes around (x, y), and a trig is made when `level > 255 − density`. The velocity is
   `40 + level·87/255` (an accent). **Chaos (SELECT)**: each step's level gets a seeded offset of ±chaos·(hash − 0.5), and B5 again rerolls the seed. It costs 4 B (the seed and the amount).
6. **HUMANIZE (B6).** KNOB 1 sets the micro jitter (±0..12 24ths), KNOB 2 the velocity jitter (±0..40). It works from the original each time (revert, then apply), with a seeded hash. 4 B RAM, ~0.4 KB flash.
7. **LENGTH (B7).** OCT+ ×2 calls the existing double (SHIFT + B9's code). OCT− ÷2 takes the later half of the
   looped pages out of the loop mask. It doesn't delete anything, so ×2 is a plain undo or a re-loop. PAGE scope
   halves the page's end instead.
8. **COPY / PASTE / CLEAR (B8..B10).** These are the existing functions: PAGE uses the F1 + B10 page clipboard, PATTERN uses SHIFT + B4 / B5. At TRACK scope, COPY and PASTE do nothing (dim). CLEAR is pressed twice for TRACK or PATTERN, as SHIFT + B11 is.

**Preview** (superseded by §2's live preview, which is both seen and heard): the screen draws the result live from the values (the lane: solid = kept, ring = new, faded = going,
plus the ring / map / dice picture). Nothing is written until OCT+. Every value is computed per step, per frame, from
the values, so a preview costs no RAM. Hearing the preview before applying is phase 3 (open question 2).

## 5. Memory budget (dual: RAM ~1.2 KB free, pool margin ~368 B; 1-core ~0.7 KB)

| item | RAM | flash (estimate) | where it comes from |
| --- | --- | --- | --- |
| UI state (now ~32 B with HUMANIZE, chance and chaos): tool, scope, track, part map (8 × 2 bits), ROTATE's running offset, RANDOM's density / notes / vel / seed (32-bit), EUCLID ×4, GRIDS ×4 | **~24 B** in `qd_ui_t` | 0 | the ~1.2 KB free (0.7 KB single-core: fits) |
| ROTATE / REVERSE temporary | 16 B on the stack, while the op runs | ~0.8 KB | in place, nothing persistent |
| lock renumber + re-sort | 6 B (one lock_t) on the stack | ~0.4 KB | in place over the existing pool |
| RANDOM / EUCLID | 0 | ~0.6 KB | pure functions |
| GRIDS map | 0 | **864 B const** | const data in flash, never copied to RAM |
| TOOLS screens (six drawings, the scope takeover, the fizz-in) | 0 (they draw into the existing frame buffer / arena) | ~5–7 KB | ~61 KB under the flash budget |
| **undo** | **0 new**: the existing 16 KB undo log | 0 | each apply is "an operation, a level of its own" (undo.h) |
| pool | 0 | | the pool isn't touched |

- **Total: about 32 B RAM, about 10–11 KB flash.** That includes the name popup (~0.3 KB), the held-open gesture and `qd_undo_revert_open` (~0.4 KB), and HUMANIZE (~0.4 KB). No new buffers. The preview isn't a copy.
- **Undo risk:** a PATTERN-scope apply on 128-step tracks can change up to 8 × 128 × 16 B = 16 KB. That plus the
  compressed "before" may not fit in the log, and then undo.h makes no level. Mitigation: before applying, check
  `qd_undo` for room (a new query, about 0.1 KB of code). If there's no room, the apply screen draws the undo arrow
  crossed out in yellow and OCT+ must be pressed twice. Typical cases (16..32-step loops) are a few hundred bytes.
- **Lock pool:** generated steps make no locks. GRIDS' velocity is an override, not a lock. ROTATE and REVERSE keep
  the count. So QD_EFULL can't happen.
- **The audio thread:** an apply edits the pattern under the same guard the other bulk edits (clear, paste, double)
  use. The worst case (PATTERN, 1024 locks re-sorted) is a few ms on core 0's UI side, never in the ISR. Measure it
  with the Geek Out NUMBERS "longest lock".

## 6. Test plan

- **Core unit tests** (`make -C tools test`, one file `core/tests/test_tools.c`):
  - rotate by k then by −k is the identity (steps and locks, byte for byte), for n = 1, 7, 16, 37 (pages with ranges)
    and k = 0, 1, n−1, n;
  - reverse applied twice is the identity, and micro is negated once;
  - the lock pool stays sorted, and its count is unchanged after rotate and reverse;
  - euclid hit counts equal `hits` for every (hits, len), and E(3,8) gives `x..x..x.` up to rotation;
  - random is deterministic for a given seed, and the density over 128 steps is within ±10 %;
  - the grids map's corners equal its nodes, the density thresholds are monotonic, and `grids_map.h` regenerates
    byte-identical from the script;
  - every op is one undo level, and undo restores the pattern byte-exactly.
- `make -C tools budget-gate` with the feature built: RAM delta ≤ 32 B; the flash figure is reported.
- **Firmware:** `--host` simulator tests for the latch (open, close, B11 cycling), then on the unit (only the main
  session flashes). Apply PATTERN-scope EUCLID while playing; check NUMBERS for no late halves and the longest lock.
- **Mock-up approval** before any code.

## 7. Phases

1. **T1 (core, no screen polish):** the held-open gesture + qd_undo_revert_open (live preview), position lists, ROTATE, REVERSE, EUCLID, LENGTH, scope, latch, undo integration,
   simple screens. The unit tests above.
2. **T2:** RANDOM (with chance), HUMANIZE, and GRIDS (with chaos) with `tools/grids_map.py` (the owner tunes the nodes on the Standalone, through
   RemoteFM1).
3. **T3:** the full LINE-style screens and animations of the mock-up, the undo-room warning, COPY / PASTE / CLEAR
   wiring, the name popup.
4. **T4:** port to firmware, measure RAM and flash, flash a test build.

## 8. Decisions (owner, 2026-10-24)

- **ARP:** tapped = MIDI FX, held 400 ms = TOOLS, SHIFT + ARP = FILL (immediate). The ARP-held arp toggle is removed, and arp on / off is set from the MIDI FX screen. The LED is lit for FILL held and for TOOLS / MIDI FX open; otherwise it shows the arp's state.
- **Pending changes:** HOME and switching tools keep a pending change.
- **PATTERN scope:** a preview that's drawn but not heard is OK there when the log is too small.
- **GRIDS map:** 32 steps, 3 × 3 nodes.
- **Latency:** hearing a live change 1–2 steps late is OK.
- **Extras:** wanted are chaos, HUMANIZE and the random chance spread. A whole-track copy is not wanted.

No open questions remain beyond tuning the GRIDS nodes by ear (T2).

## 9. Fitting it in

Measured margins (2026-10-24):

| build | app slot (flash) | RAM | pool | RAM code | NOINIT |
| --- | --- | --- | --- | --- | --- |
| dual | 44,628 B free | 1,196 B | 8,560 (fm1-base needs 8,192: 368 spare) | 500 B | 104 B |
| single-core | (similar) | ~0.7 KB | | | |

TOOLS uses no pool, no RAM code and no NOINIT. All of its code runs from flash on the UI side, never in the audio ISR. Its RAM is only `qd_ui_t` fields.

| phase | content | flash (est.) | RAM | builds | if it doesn't fit |
| --- | --- | --- | --- | --- | --- |
| T1 | latch and ARP map, scope and position lists, held-open gesture + `qd_undo_revert_open`, ROTATE, REVERSE, EUCLID, LENGTH, lock renumber / re-sort, plain screens, name popup | ~4.5 KB | ~16 B | dual + single | drop the name popup's frame (draw text only), and share one ring drawing between ROTATE and EUCLID |
| T2 | RANDOM (+ chance), HUMANIZE, GRIDS (+ chaos), the 864 B map | ~3 KB | ~16 B | dual + single | GRIDS map to 16 steps (432 B), or HUMANIZE folded into RANDOM's knobs |
| T3 | LINE-style drawings and animations, the undo-room check, COPY / PASTE / CLEAR wiring | ~3 KB | 0 | dual; single if it fits | static drawings (no dice roll or fizz-in), or keep T1's plain screens on the single-core build |
| T4 | the firmware port, measured with `budget-gate` and `firmware/build.py` (dual and `--single-core`) | — | — | both | — |
| **total** | | **~10.5 KB of 44,628 (24 %)** | **~32 B of 1,196 (single-core: of ~700)** | | |

- **RAM:** ~32 B is 2.7 % of dual's free RAM and ~4.6 % of single-core's. No buffer depends on scope, and the preview and revert use the existing 16 KB undo log.
- **The one stack peak:** a 16 B step plus a 6 B lock temporary during an apply, inside the UI task's existing stack.
- **Acceptance gate for each phase:**
  - the RAM delta is at most 32 B;
  - dual flash stays more than 30 KB free after T3;
  - single-core RAM free stays at least 0.6 KB;
  - the pool margin is unchanged.
- **If single-core RAM is the limit,** pack the tool values into a union: only one tool is live at a time, which brings ~32 B down to ~12 B.

## 10. T1 as built (2026-10-24)

- **Code:** `core/seq/tools.c` (the ops, built through `pattern.c`), `core/app/ctl_tools.c` (the latch, built through
  `ctl_latch.c`), `core/ui/ui_tools.c` (the screens, built through `ui_layer.c`), `undo.c`'s `hold` and
  `qd_undo_revert_open`. Tests: `core/tests/test_tools.c` (host and device sizes). Renders: `render_frames --only t1`
  (`core/tests/out/.../t1-*.png`, `t1-sheet.png`).
- **Where it differs from the plan above:**
  - LENGTH follows the latch's one OCT rule (§2): SELECT / KNOB 1 is ×2 / ÷2 a detent, live; OCT+ keeps it and OCT−
    reverts it. §4.7's "OCT+ ×2, OCT− ÷2" would have clashed with keep / revert.
  - Switching the tool, the scope or the track keeps a pending change as its own undo level, then switches. The orange
    dot is the current tool's live change.
  - EUCLID changes nothing until a knob is turned.
  - The drawn-only fallback applies whenever the log can't keep the gesture's compressed copy, in any scope (it's the
    whole pattern's copy whatever the scope).
  - The tool's name also pops when TOOLS opens. The scope's takeover stays the usual second.
  - The arp on / off takeover (`qd_mfx_arp_tap`, `QD_TK_MFX_ARP`, `ui_mfx_arp_draw`) is gone. Its three `qd_ui_t` ints
    pay for TOOLS' 12-byte state, so the RAM delta is 0. ARP's LED is steady lit for FILL and the MIDI FX screen (no
    blink or breath).
  - Flash: about 9 KB (ui 4.7, app 2.7, seq 1.2), against the 4.5 KB estimated.

### 10.1 The owner's revision after trying T1 on the unit (2026-10-24)

| control | in TOOLS now |
| --- | --- |
| W1..W16 | the visible page's steps. Their LEDs show the steps as the pending change makes them (selected ones blink). A press puts a step in or out of the selection and turns the scope to SELECTED STEPS (another scope's pending change is kept first; SELECTED's own follows the selection live). The selection is the step lock's (LFO): no RAM of its own |
| B11, KNOB 4 | the scope, four now: SELECTED STEPS, PAGE, TRACK, PATTERN. With the step lock on, TOOLS opens on SELECTED STEPS |
| SELECT | the page shown and edited, round all eight (PAGE scope: a pending change is kept first) |
| PRESETS | the tools, round the built ones |
| KNOB 1..3, ALGORITHM | the tool's values. KNOB 4's old job moved to ALGORITHM (EUCLID's landing; RANDOM's chance spread, GRIDS' part in T2) |
| OCT+ / OCT- | keep / revert, each with a popup (the tool's name and KEPT / REVERTED). OCT- never leaves |
| HOME | the only way out |

- The track: the one selected when TOOLS opened. SHIFT + SELECT (FIZZ's track change everywhere) still works inside
  TOOLS; a pending change is kept first.
- No glows in the TOOLS screens: the ROTATE sweep, the ring's dots and the REVERSE bars lost theirs. One ring drawing
  serves ROTATE and EUCLID; the popups have no frame (a dark band under the words).
- The tools' values are a union (a tool chosen starts from its defaults: EUCLID 4 of 16).
- LENGTH in SELECTED STEPS works as PAGE (on the page the change began on).

### 10.2 T2 as built

- **RANDOM (B3):** KNOB 1 the density (0..100 %), KNOB 2 the notes' spread (0..12 degrees of the track's scale round
  its default note), KNOB 3 the velocity spread (0..64), **ALGORITHM the chance spread** (0..100: was KNOB 4). B3 again
  rolls a new seed. Every value a pure function of the seed (`qd_tl_hash`), so the preview needs no buffer.
- **HUMANIZE (B6):** KNOB 1 the timing jitter (+-0..12 24ths), KNOB 2 the velocity jitter (+-0..40), from the original
  each time; B6 again a new seed.
- **GRIDS (B5):** KNOB 1 X, KNOB 2 Y, KNOB 3 the density, **ALGORITHM the chaos** (was SELECT: SELECT turns the pages
  now). **B5 again: the next part** (LOW -> MID -> HIGH) with a new chaos seed (KNOB 4 is the scope, ALGORITHM chaos,
  so the part went on the key). New trigs take the level as a velocity override.
- **GRIDS over the PATTERN:** the parts LOW, MID, HIGH go to the DRUMS tracks first, then the other tracks in order
  (the default kit: tracks 1, 2, 3). The plan's "W taps a track for the next part" is gone: W are the steps now.
- **The map:** `tools/grids_map.py` (MPL-2.0, its rules in its header) writes `core/seq/grids_map.h`, 3 x 3 nodes x 3
  parts x 32 steps (864 B const); `make test` checks the header is what the rules make. Nothing of Mutable's read.
- **The values** share the union (RANDOM 6 B, GRIDS 8 B): qd_ui_t is still smaller than before TOOLS.

### 10.3 T3 as built

- **B8 COPY, B9 PASTE** for PAGE (the F1 + B10 page clipboard: copied from the page shown, pasted onto the page
  shown) and PATTERN (SHIFT + B4 / B5's pattern clipboard). Dark in SELECTED and TRACK (no whole-track copy, the
  owner's choice); PASTE dim with nothing to paste.
- **B10 CLEAR:** SELECTED STEPS and PAGE at once; TRACK and PATTERN pressed twice ("CLEAR?", B10 blinking, 2 s).
- Each is an undo level of its own, a pending change kept first, and pops the scope's name with COPIED / PASTED /
  CLEAR? / CLEARED (TOO BIG, FULL when the clipboard or the lock pool has no room).
- **The undo-room check** (`qd_undo_room`): after every live change the level's size is counted against the log. Too
  big: the crossed-out arrow; OCT+ once says NO UNDO, OCT+ again keeps it without a level.
- Not built from T3's list: the LINE-style animations (the dice roll, the fizz-in, the growing pages). The flash went
  to the tools instead.

### 10.4 The owner's swap (after T3)

ALGORITHM sets the scope (SELECTED / PAGE / TRACK / PATTERN; B11 still cycles it) and KNOB 4 is the tool's fourth
value again: EUCLID's landing, RANDOM's chance spread, GRIDS' chaos (the orange meter and pictures: KNOB 4's colour).
Where §10.1 and §10.2 say "ALGORITHM" for those values or "KNOB 4" for the scope, read it the other way round.

### 10.5 The owner's findings on the unit (0.12.0-beta.2, 2026-10-24) and the fixes

- **What was found (reproduced at the device's sizes, `test_tools` t_owner / t_sel_rotate, `firmware/app/tests/tools.txt`):**
  the ops write every position of their scope (a 64-step TRACK: all four pages; a pattern read back in from the store:
  the same). What went wrong was what the panel showed. (1) **Page follow ran inside TOOLS**: playing, the page shown
  jumped to the playhead's, so SELECT's page did not stay, a PAGE change went on writing the page it began on while
  another was shown, and the ghosts (kept for that one page only) vanished. (2) The W LEDs lit every trig of the page,
  the steps **outside the loop / the page's start..end** too (a 5-step track: 11 keys the tools never touch), so "only
  some steps change". (3) **SELECTED + ROTATE** turned the selected steps round among themselves: one step (or steps
  alike) cannot move, so nothing happened. (4) EUCLID kept a length clamped to the selection's few steps after
  leaving SELECTED.
- **Fixes:** no page follow while TOOLS is open; SELECT pops the step view's PAGE takeover (the eight pages, "PAGE 3");
  a page strip over the lane (the page shown in the track's colour, looped pages grey). The lane and the W LEDs come
  from `qd_tools_page`: now (lit), the ghosts (dim keys, faded bars) read from the held gesture's "before"
  (`qd_undo_before`, the compressed copy read in place: any page, no RAM), steps that do not play dark (dots on the
  lane). SELECTED + ROTATE moves the selected steps k places along the track's loop, the others filling the places
  left in order (`qd_tl_move`; one permutation helper now serves ROTATE, REVERSE and the move), the selection follows
  on OCT+. RANDOM: an icon under each meter (trigs, a note, a wedge, a pie) and KNOB 1..4 pop DENSITY / NOTES /
  VELOCITY / CHANCE (20 px, the owner's words). The tool's popup names the scope under it.
- **Cost:** RAM +0 (dual 1,148 B free, single-core 540 B); flash about +1.8 KB (budget model 465,925 -> 467,709 B;
  dual 18,956 B free in the app slot).


### 10.6 QUANTIZE (2026-10-25, the owner's request)

HUMANIZE's opposite, the eighth tool, on every scope with the same live preview, ghosts, LEDs, OCT+ keep / OCT− revert,
popups and undo as the others (`qd_tl_quantize`, `tools.c`).

- **What it moves:** the only off-grid timing FIZZ stores per step is `micro` (−23..+23 24ths of a step, an override).
  Ratchets are evenly spaced counts, strum is a chord's spread, swing is the track's on / off with the pattern's amount
  (the player adds it on top of micro): none is off-grid feel, so none is touched. The track's default micro is the
  track's nudge (a setting), left alone: QUANTIZE works on the steps' own micro only. A value that lands on the track's
  default drops its override.
- **KNOB 1, the amount** 0..100 % (5 % a detent; it starts at 0, so nothing moves until it is turned): each trig's micro
  goes that far to its line, rounded toward the line (50 % halves it, 100 % puts it on the line).
- **KNOB 2, the grid:** a step, 1/2, 1/3, 1/4 of one (24ths 24, 12, 8, 6). The lines are the step's own (|line| <= 23,
  a tie goes to the step's side), so a note never changes step: no collisions, no locks to move.
- **KNOB 3, the velocity** 0..100 %: "humanize off" for HUMANIZE's other half: the steps' own velocities that far back
  to the track's default.
- **KNOB 4:** nothing. No swing knob: swing is already the track's and the player's, applied on top of a quantized step.
- **Where it sits: PRESETS only** (round past LENGTH; the eighth icon in the head, which now spaces eight at 19 px).
  Every B key is taken (B1..B7 the tools, B8..B10 the clipboard, B11 the scope), "B6 again" is HUMANIZE's new seed, and
  SHIFT + B is kept for snapshots, so no single click was free. With QUANTIZE chosen B1..B7 are all dim.
- **The screen** is HUMANIZE's, shared (`tl_timing`): the steps as bars off their lines, heights the velocities; a
  moved step's ghost (as it was) outlined behind it (HUMANIZE gets the ghosts too; its bars are 7 px wide now, were 10);
  KNOB 2's grid as green ticks in each step and as a step cut in its parts at the foot, KNOB 1 and KNOB 3 as bars. No new
  words: the name popup only. `qd_tools_timing` gives the bars and ghosts: live from the gesture's before
  (`qd_undo_before`), drawn only worked out through the ops' own per-step functions (`qd_tl_quantize_one`,
  `qd_tl_humanize_one`).
- **Cost:** RAM +0 (the values in the tools' union: 3 B of its 8); flash dual +1,888 B (17,068 free), single-core
  +1,872 B (13,276 free); budget model 467,709 -> 469,481 B (the ratchet raised for it).
- **Tests:** `test_tools` t_quantize (the lines, 100 % zeroes the micro, 50 % halves it, the velocity) and t_quant_live
  (PRESETS, each scope, the ghosts, OCT− byte for byte, OCT+ one level, undo byte for byte, drawn only) at host and
  device sizes; `firmware/app/tests/tools.txt` (HUMANIZE kept, then 50 %, 100 %, OCT−, OCT+, undo: the host-only probe
  `fz_micabs_T`); renders `render_frames --only t1` (`t1-quantize-*.png`).

### 10.7 The owner's layout, CLIP, STRETCH (2026-10-25, branch tools-layout)

- **The keys:** B1 ROTATE, B2 REVERSE, B3 RANDOM, B4 EUCLID, B5 GRIDS, B6 HUMANIZE, **B7 QUANTIZE**, **B8 LENGTH** (the enum
  reordered, `QD_TL_NKEY` 8); B9 and B10 do nothing (dark); B11 and ALGORITHM the scope as before. PRESETS rounds all ten:
  the eight, then STRETCH, then CLIP. The head draws ten icons, 16 px apart at 80 % (the dot, the track's box and the
  scope moved right).
- **CLIP** (COPY / PASTE / CLEAR off the B keys, a page of their own): KNOB 1 picks the action (the one picked big in the
  middle, its neighbours small at the sides, three dots for KNOB 1's place), **OCT+ does it** (the latch's confirm, as
  KEEP is for the others; its LED lit when the action can be done here, blinking while CLEAR waits for its second
  press). COPY / PASTE: PAGE and PATTERN only (grey elsewhere, PASTE grey with nothing copied); CLEAR: SELECTED and PAGE
  at once, TRACK and PATTERN twice (the red cross blinking, "CLEAR?"). The functions, popups and undo are T3's.
- **STRETCH** (`qd_tl_stretch`, `qd_tl_stretch_track`, tools.c): remaps the track's loop (TRACK; PATTERN: each track;
  PAGE / SELECTED: the page's start..end) in time. KNOB 1 doubles / halves (x2..x8, /2../8; as far as there is room:
  128 steps, or the page's end; the factor shown is the one done), KNOB 2 fits it onto 1..8 pages (a page: 2..16 steps;
  turning KNOB 1 again leaves fit). Each trig at i + micro / 24 (the track's nudge off) goes to the nearest step of the
  new time t x num / den, the rest its micro (|micro| <= 12 inside, up to 23 at the ends); two trigs on one step: the
  nearer to its line kept; a step's own length scaled to the nearest of the table; locks move with their steps (one
  permutation of the 128 steps through the tools' `tl_permute`, then the steps that took no trig cleared with their
  locks). The new loop: whole pages from the first looped one (moved down if it would pass page 8), the last page's
  end the rest. x8 then /8 is the identity (steps, micro, locks); `test_tools` t_stretch, t_stretch_live (KNOB 1 live,
  the clamp, OCT- byte for byte, fit 1 page, OCT+ one level, undo byte for byte, drawn only worked out) at host and
  device sizes; `firmware/app/tests/tools.txt` (x2 live, OCT-, CLIP's COPY / PASTE).
- **Its screen:** the loop as it was (grey ticks, its trigs) over the loop as it is (the track's colour), on one time line
  (the eight pages across; PAGE: the page's sixteen steps), a fan between them (KNOB 1's blue, KNOB 2's green when
  fitting), the factor big (x2 / a drawn division sign and 4) or the pages it fits onto. `qd_tools_stretch` gives the
  128 steps now and before (live: the gesture's before, a page at a time; drawn only: the result's loop, no trigs).
- **Cost:** RAM +0 (the values in the tools' union: `st.fit`, `cl.act`; `lpow` is STRETCH's factor too); the pool +32 B
  dual / +48 B single-core (the HOME-flash counter, fz_port.c); flash dual 15,788 -> 10,860 B free (-4,928, with the F3
  ape and the counter), single-core 11,980 -> 7,052; budget model 470,827 -> 474,842 B (the ratchet raised for it).

**The same batch (not TOOLS):**
- **F1's page map, the double tap** (`k_f1_black`, ctl.c): the code had not changed since P1.5c and passes on the host;
  the window was the second RELEASE within 200 ms of the first release, so it included the second press's length. At the
  unit's pace (a press ~80..120 ms on its keys, input sampled between the frame's bands) a natural double tap's releases
  are ~250 ms apart: two taps. Now the second PRESS within 200 ms of the first release marks it (`pg_taps` 2), decided at
  its release (a hold still toggles). `test_ctl` g_f1_pages (110 ms presses, 150 ms apart; the second press 210 ms after:
  two taps) and `tools.txt`'s F1 part (fails on master's build, passes here).
- **F3's tempo / swing takeover: the TAP ape** (`ui_ape_tempo`, ui_ape.c): the TAP screen's scene (`ap_draw`, shared)
  grooving to the tempo, a new pose every two beats (one under 64 BPM) with its hop and sparkles, the bob on each beat,
  the pairs of beats swung by the swing; the BPM big (the knob's colour turning TEMPO), the swing under it (big, green,
  turning SWING); no sound arcs (the swing sits there). The metronome stays for TSWING only. Renders `p13x-f3-tempo*.png`,
  `p13x-f3-swing.png`.
- **The HOME screen flashing in menus:** not reproduced. 35,000 random presses, holds and turns on the host (stopped and
  playing) never showed a menu's screen going to EDIT and back on the clock alone; every knob in MIX, FX, SOUND, MIDI FX,
  TOOLS, F3, REC, SETTINGS and SAVE raises no takeover that draws the step view; every simulator script runs with no
  blip. So it is the unit's alone (the frame's path, the panel). A counter now watches every latched frame
  (`fz_blip_look`, fz_port.c): a menu's screen, then EDIT, then the same menu within 400 ms is counted with the menu, the
  latch, the mode, the takeover and its length: stats line 16 `blips= blip_scr= blip_latch= blip_mode= blip_tk=
  blip_ms=` (`fizz_xdata.py stats`), the host's `expect fz_blips`.
