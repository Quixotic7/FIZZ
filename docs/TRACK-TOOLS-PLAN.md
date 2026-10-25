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

