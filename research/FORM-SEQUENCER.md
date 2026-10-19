# FORM sequencer (OMX-27 v1.15.4): mechanics reference

Source: `OMX-27` repo, tag **v1.15.4** (commit d3ba91e, 2026-09-03). Everything below was read
from the code at that tag unless marked **[inferred]** (worked out from the code but not
stated in it) or **[doc]** (taken from the owner's design docs and not cross-checked line by
line).

Main files (all under `OMX-27-firmware/`):

| File | What it holds |
|---|---|
| `src/form/omx_mode_form.cpp` / `.h` | The shell: views, every key/encoder/LED/screen handler, patterns, recording, tools |
| `src/form/machines/form_machine_omni.cpp` / `.h` | The track engine (one per track): step data access, palettes, playback clock, trig evaluation |
| `src/form/machines/omni_structs.h` | Data model: `Step`, `Track`, `OmniSeq` |
| `src/form/machines/omni_transpose_pattern.cpp` | The transpose lane editor |
| `src/form/omx_form_global.h` | Shared flags (shortcut mode, rec arm, note-entry mode, select/edit) |
| `src/utils/aux_macro_manager.cpp` | The AUX MidiFX/arp shortcuts shared with other OMX modes |
| `src/hardware/omx_disp.cpp`, `omx_keypad.cpp` | Screen renderers; key timing |
| `design/form/FORM_USER_GUIDE.md`, `FORM_MENU_MAP.md`, `FORM_DESIGN.md`, `FORM_PRODUCER_REVIEW.md` | The owner's docs (current at this tag) |

## 0. Conventions used here

### Key names

| Name here | OMX key index | Panel |
|---|---|---|
| **AUX** | 0 | top-left key |
| **B1–B10** | 1–10 | the 10 black keys. **B1 = F1, B2 = F2**; F3 = B1+B2 held together |
| **W1–W16** | 11–26 | the 16 white keys = steps 1–16 of the active page |

In FORM, F1/F2 are not separate buttons: they are black keys B1 and B2. Holding them makes a
modifier layer, but while a step is held they become ordinary value keys (see §2). This
matters for the FM-1 port, where OCT−/OCT+ are separate buttons (see §11).

### Timing (from `OMX-27-firmware.ino` and `omx_keypad.cpp`)

| Term | Meaning |
|---|---|
| **tap / quick click** | press and release within **200 ms** (`clickWindow`, `quickClicked()`) |
| **double-tap** | second release within 200 ms of the first (`clicks() == 2`) |
| **hold** | key down **800 ms** (`longPressInterval`, `e.held()`) |
| **hold UI delay** | the "step held" screen and LEDs wait **150 ms** before they show (`loopUpdate`, `stepHoldStartMs_`), so quick taps don't flash them |

Most actions fire on the key-down. Exceptions are noted (AUX transport and rec-arm fire on
release; quick-tap actions fire on release).

### Encoder model (all views)
- **SELECT** (default): turning moves a cursor. **EDIT**: turning changes the value.
- **Click** toggles SELECT ⇄ EDIT (`omxFormGlobal.encoderSelect`). **Holding AUX** makes it
  EDIT for as long as AUX is held (`getEncoderSelect()` = `encoderSelect && !midiAUX`).
- On screen the selected cell is boxed; while editing it is inverted.
- Long-press of the encoder is reserved for the OMX mode picker (leaves FORM).

### Seven views
FORM is one OMX mode with 7 views, switched with **AUX + W3…W9**: MIX, STEP (shown as SEQ),
TRANSPOSE, NOTES, PATTERNS, MI, TOOLS. The step-editing view the questions below refer to is
**STEP/SEQ**. The other views appear only where they own a feature (mute/solo lives in MIX,
live recording in MI/NOTES, page copy in TOOLS, patterns in PATTERNS).

---

## 1. Main sequencer view (STEP/SEQ), nothing held

Code: `onKeyUpdateStep`, `updateStepLEDs`, `paintStepRow`, `onDisplayStep`,
`onDisplaySeqTrackPage` (omx_mode_form.cpp); `dispSeqTrackPage`, `drawStepRow` (omx_disp.cpp).

### Gestures

| Gesture | Effect |
|---|---|
| **Tap B3–B10** | Select the **edit mode** for step holds: B3 NOTE · B4 VELOCITY · B5 LENGTH · B6 REPEAT (ratchet) · B7 CHANCE · B8 MATH (conditions) · B9 FUNCTION · B10 MIDI FX. Mode name pops on screen. Only on the overview page (menu page 0); on the param pages these keys are value keys (§3). |
| **Hold B1 / B2 / B1+B2** | F1 / F2 / F3 layers (§4) |
| **Tap a white key** (<200 ms, only that step held, nothing edited) | **Toggle the step**: a step that is on (notes or ghost) is **cleared** (cut into the step buffer, no message). An empty step is **created** with the last chord entered in Note mode (`lastNotes_`, default middle C = 60) |
| **Hold a white key** | Step hold: the black keys become a value palette for the current edit mode (§2). While stopped, the step's notes play while held (except in Note mode, where the note keys play instead) |
| **Turn encoder (SELECT)** | Walk the Seq menu: overview → param page A (VEL/NUDG/LEN/MFX) → param page B (PROB/COND/FUNC/ACUM) → CC page (5 CC slots, bank, "CC" title) → STEPNOTES (6 note slots and a names/numbers switch) → "TRACK SETUP": SCALE page (MODE/ROOT/SCALE/LOCK/GROUP) → ACTIONS page (QNT/CLR/POTS/NTRY) |
| **Turn encoder (EDIT) on the overview** | Change the **selected track** |
| **Knob K1–K5** | Send the track's pot-bank CC live on the track's channel |

A press longer than 200 ms with no edit does **nothing** on release (neither toggle nor clear).

### LEDs (overview, nothing held)

| Keys | Shows |
|---|---|
| B1, B2 | dim track colour (full when that modifier is held) |
| B3–B10 | the 8 edit modes. The active mode is in its colour (NOTE white, VEL yellow, LEN cyan, REPEAT orange, CHANCE green, MATH magenta, FUNC blue, MFX red); the others are low white |
| W1–W16 | the **active page's** 16 steps: notes = bright blue-white (`0xC0C0FF`) · ghost trigger (on, no notes) = orange · muted step = dark red · empty = off · **playhead = steady green** (only while playing, and only if the playing step is on the active page) |

The black row does **not** show the track or the page in this state. Those appear only under
F1 (pages) and F2 (tracks).

**[inferred]** The green playhead uses `playingStepIndex()`, which reads `playingStep_`. The
engine advances `playingStep_` straight after a step fires (`onClockTick`), so it names the
**next** step to fire. The MIX view's LED playhead uses `lastTriggeredStepIndex_` instead.
Check this on hardware before copying it.

### Screen (the "track page", `dispSeqTrackPage`)
- **Top row:** 8 track squares (filled = audible, outline = muted or not soloed; the selected
  track is underlined) · transport glyphs ▶ ■ ● (the active one filled; ● = rec armed) · the
  view tag "SEQ" (boxed while AUX is held) · BPM at the right.
- **Middle:** "TRK n" in large type · play-mode icon (FWD/REV/FWD-PONG/REV-PONG/RANDOM) ·
  rate "1:16".
- **Right:** 4 page icons (filled = page in the loop, outline = out; the **active (edit)
  page is underlined**).
- **Bottom:** the 16 steps of the active page: solid = notes · inset block = notes + muted ·
  box with two dots = ghost · outline = empty · steps beyond the page length are drawn as
  short 3 px boxes · a marker under the playhead.
- In EDIT mode with nothing held, "TRK n" is boxed and inverted (the encoder now picks the
  track).
- After a knob moves, a CC meter shows along the top for about 1 s.

There is **no page-follow**: the active (edit) page changes only when you change it (§6).

---

## 2. Step holds and edit modes

Code: `onKeyUpdateStep`, `stepApplyToHeld`, `onEncoderStep`, `onEncoderButtonStep`,
`updateStepLEDs` (shell); `setStepPalette`, `stepPaletteCount`, `stepPaletteSelected`,
`editStepParam`, `resetStepValue`, `stepNotesToGhost`, `setStepJumpTarget`, `stepAddNote` /
`stepRemoveNote` / `stepClearNotes` (machine).

### General rules
- **Several steps can be held at once.** Every palette, encoder or knob edit applies to all
  held steps. The last-pressed held step is the "focus": the screen and LEDs show its value.
- While any step is held, **B1/B2 stop being F1/F2** (`updateShortcutMode` forces
  `FORMSHORTCUT_NONE`). All 10 black keys are value keys.
- **Setting any value through a palette turns the step on.** It sets `trig = 1`, so an empty
  step becomes a **ghost trigger**: it fires (sends its CC locks, runs its function) but plays
  no notes. Palette edits also **set the P-Lock bit** for that parameter (§3).
- **AUX tap while holding step(s)** resets the current mode's value on every held step. In
  Note mode it clears the notes but keeps the step as a ghost. In the other modes it clears
  that parameter's lock and reverts to the track default; Repeat goes back to x1. AUX does
  not open the view layer in this case.
- **Knob K1–K5 while holding step(s)** (any Seq page): CC P-Lock. The step stores the knob's
  absolute value 0–127 for that pot slot and sends it when the step fires (`onPotChanged`).
  No pickup.
- **Pressing another white key** while holding adds it to the held set. The exception is
  FUNCTION mode (below).
- **Release:** if exactly that one step was held, nothing was edited and the press was under
  200 ms, the release toggles the step (§1). Otherwise the release only ends the hold. When
  the last step is released, any note previews are switched off.

### What the black keys do while a step is held (overview page)

| Mode (select with B3–B10) | Black keys while a step is held | Encoder while a step is held |
|---|---|---|
| **NOTE** | **B1–B10 = scale degrees 0–9** of the track's scale, starting at the root, at the current octave (`paletteScale()->getNoteByDegree(degree, octave)`; root = middle C + root + 12×octave). On a chromatic track they are **10 semitones** up from the root. Chord entry: see "Note entry" below | Shift **all notes of the step ±1 semitone** per detent (chromatic, not by scale degree), clamped 0–127 |
| **VELOCITY** | B1–B10 = velocity 12, 25, 38, 50, 63, 76, 88, 101, 114, 127 (`(n·127)/10`) | velocity ±1, with acceleration |
| **LENGTH** | B1–B10 = note length **0.5, 0.75, 1, 2, 4, 6, 8, 16 (1 bar), 32 (2 bars), 64 (4 bars) steps** (len index {2,3,4,5,7,9,11,19,20,22}) | step through all 23 length values (0.125, 0.25, 0.5, 0.75, then 1–16 steps, then 2, 3, 4 bars), ±1 |
| **REPEAT** | B1–B4 = ratchet **×1, ×2, ×3, ×4** | ±1 within 0–3. **Doesn't set the lock bit and doesn't turn the step on** (`editStepRepeat`) |
| **CHANCE** | B1–B10 = probability 10, 20, … 100 % | ±1 % with acceleration (0–100) |
| **MATH** (condition) | B1 = **FILL** · B2 = **!FILL** · B3–B6 = ratio **A = 1–4** · B7–B10 = ratio **B = 1–4**. A ratio key keeps the other half and forces A ≤ B (e.g. on 1:2, pressing A=3 gives 3:3). LEDs: Fill orange, !Fill pink, A green, B cyan | walks the whole condition list (0–43): `--`, FILL, !FIL, PRE, !PRE, NEI, !NEI, 1ST, !1ST, then 1:2, 2:2, 1:3 … 8:8. **PRE/NEI/1ST and A/B values above 4 can only be reached with the encoder** |
| **FUNCTION** | B1–B7 = `--` (none) · RSET (restart) · `<<` (set reverse) · `>>` (set forward) · `<>` (flip direction) · **J?** (random jump) · `???` (random function) | walks 0–70: the 7 functions, then **J1…J64** (jump to step n) |
| **MIDI FX** | **B5 = Off · B6–B10 = MidiFX group 1–5** (same layout as the AUX MidiFX keys). "TRK" (follow the track's group) is the default and isn't on the palette; get back to it with AUX-reset | walks Off / TRK / FX1–5 |

**FUNCTION extra gesture:** while holding a step whose function is already a jump (J? or Jn),
**pressing another white key** sets that key as the jump target (`setStepJumpTarget`: func =
7 + absolute step index). It does not add the second step to the held set. To make a jump:
hold the step, press B6 (J?), then press the target step.

**Note entry (NOTE mode)** (`omxFormGlobal.noteEntryToggle`, set by **NTRY** on any ACTIONS
page; a global setting):
- **PR (Pressed, default):** pressing a note key while **no other note key is held**
  **replaces** the held step's notes. Note keys pressed while another is still held **add**
  to the chord. Up to **6 notes** per step, no duplicates; extra notes are dropped silently.
- **TG (Toggle):** each press adds the note, or removes it if the step already has it
  (drum-style).
- On a **monophonic** track every entry replaces (one note per step).
- While stopped, each note key plays its note (on the track's channel, at the track's default
  velocity) until released. While playing there is no preview.
- The chord is remembered as `lastNotes_` (a tap on an empty step stamps it).
- Out-of-range notes (octave extremes) are ignored.
- **Note-key LEDs:** chromatic track: keys that land on white piano notes are periwinkle,
  black piano notes amber. Scale track: root periwinkle, other degrees blue. Notes in the
  chord (or held) are bright, the rest dim.

**There is no "black key clears the override" palette key.** The only way to clear an
override (lock) is the AUX tap (current mode), the encoder click on a param page (§3), or
long-press-and-release of W4 in the Notes view.

### Screen while holding (overview page, after 150 ms or after the first edit)
- NOTE mode: a compact piano keyboard showing the focus step's chord, with the page's step
  strip under it (`dispStepNoteKeyboard`).
- Other modes: the value as text ("VEL 101", "0.75", "x3", "50%", "FILL" / "3:4", "JMP > 5",
  "FX2" / "TRK" / "OFF") and the step strip with a marker on the focus step
  (`dispStepOverview`, `stepValueString`).
- LEDs: held steps blink white on the step row; the other steps go dark. Palette keys:
  "bar" modes (VEL/LEN/CHANCE) light every key up to the current value with the selected key
  near-white. "Choice" modes light only the current key. Keys outside the palette stay off.

---

## 3. Track defaults and step overrides (P-Locks)

Code: `Step`, `Track::paramDefaults` (omni_structs.h); `editParamDefault`,
`setParamDefaultPalette`, `editStepParam`, `clearStepParamLock`, `stepParamLocked`
(form_machine_omni.cpp); `onDisplayStepMenu`, `onEncoderStep`, `onEncoderButtonStep` (shell).

### The model
Each step **stores a concrete value** for every parameter, plus a **9-bit lock mask**
(`Step::locks`: VEL, NUDGE, LEN, MFX, PROB, COND, FUNC, ACCUM, REPEAT). Each track has
`paramDefaults[8]` for the 8 lockable parameters (pid 0–7):

| pid | Param | Range | Track default |
|---|---|---|---|
| 0 | VEL velocity | 0–127 | 100 |
| 1 | NUDG micro-timing | −60…+60 (±1 step) | 0 |
| 2 | LEN note length | index 0–22 (0.125 step … 4 bars) | 3 (= 0.75 step) |
| 3 | MFX MidiFX route | 0 Off · 1 Track · 2–6 FX1–5 | 1 (track) |
| 4 | PROB chance | 0–100 % | 100 |
| 5 | COND condition | 0–43 | 0 (none) |
| 6 | FUNC step function | 0–70 | 0 (none) |
| 7 | ACUM accumulate | 0–4 | 0 |
| — | REPEAT ratchet | ×1–×4 | none (built-in ×1). The lock bit exists but has no track default |
| — | notes | up to 6, −1 = empty | not a default; per step |
| — | CC P-Locks | 5 slots, −1 = unlocked | not a default; unlocked slots send nothing |
| — | step mute | on/off | set only from MIX (F1 + white key) |

**Editing a default pushes it into every step that is not locked for that parameter** (all 64
steps, `editParamDefault`). Unlocked steps therefore always equal the default. **Editing a
step's value sets its lock bit** (editing is locking). Locks are **per parameter, per step**.

### Gestures (STEP view param pages A and B; walk to them with the encoder in SELECT)

| Gesture | Effect |
|---|---|
| Encoder (SELECT) | choose the parameter cell (4 per page) |
| **Nothing held, B3–B10 (press)** | set the **track default** of the selected parameter from its palette (same values as §2; Nudge uses B1–B9 = −60, −45, −30, −15, 0, +15, +30, +45, +60; Accum B1–B5 = 0–4; MFX starts at B5) |
| **Nothing held, quick tap B1/B2** | palette keys 1 and 2 for the default. A hold stays F1/F2. Not for MFX |
| Nothing held, encoder in EDIT | edit the default (±1 with acceleration) |
| **Holding step(s), B1–B10** | set the held steps' value from the palette, which locks it |
| **Holding step(s), turn encoder** | edit the held steps' value (**regardless of SELECT/EDIT**), which locks it. COND/FUNC also pop the full value text |
| **Holding step(s), click encoder** | **clear the lock** for the selected parameter: the step reverts to the track default |
| Holding step(s), AUX tap | same as the overview AUX reset, but for the **edit mode chosen on the overview**, not the selected cell **[inferred]** (the AUX handler uses `stepEditMode_`) |

Other places that edit defaults: MIX → LEVELS (8-bar velocity mixer = default VEL of each
track), MI → MIDI → VEL.

### How a lock is shown
- On the param pages (`dispStepParams`): while a step is held, each cell shows that step's
  value, and **the label of a locked parameter is drawn inverted**. With nothing held the
  cells show the track defaults (never inverted).
- On the step row and LEDs, locks are **not** shown. A lock on an empty step only shows as a
  ghost (orange LED, two-dot box), because locking turned it on. The owner chose not to tint
  P-Locked steps ("Won't build", FORM_DESIGN.md §6).
- CC page while holding: slots show that step's locks ("CC LOCK" title; unlocked slots have no
  bar). Encoder edits the lock; **click clears that slot's lock**.

---

## 4. F1 / F2 / F3 in the STEP view

Code: `onKeyUpdateStep`, `handlePageGesture`, `handleF1PageActions`, `updateShortcutMode`,
`onEncoderChanged` (shell); `copyStep` / `cutStep` / `pasteStep`, `setRateShortcut`,
`setPageLen` (machine).

`updateShortcutMode` decides the layer: both B1 and B2 held = **F3**, only B1 = **F1**, only
B2 = **F2**. Keys pressed as part of the AUX layer never count. In the MI view B1/B2 are notes,
so there are no F-layers there.

### F1 (hold B1): pages, copy, clear, undo

| + key | Effect |
|---|---|
| **B3–B6** (pages 1–4) | see the page gestures in §6 (tap = edit that page · double-tap = solo · hold one + press another = loop range) |
| **B8** | **clear the active page** (all 16 steps reset to init), undoable |
| **B9** | **clear every page** of the track, undoable |
| **B10** | **undo / redo** (one level, shared with TOOLS key B10) |
| **W1–W16** | **COPY** that step into the track's step buffer (all of it: notes, values, locks, CC locks, ratchet, mute). This also puts F2 into "holding" state, so the next F2 press **pastes** |
| **Encoder** | change the **active page** (1–4, clamped). Large page icons pop for 0.9 s |

LEDs: B3–B6 = pages (active = green, or **bright red if the active page is out of the loop**
· in the loop = blue · out = very dim · the **playing page flashes yellow**). B8 orange, B9
red, B10 blue (flashes for 2 s after any destructive action, dim when nothing to undo). Step
row unchanged. Screen: the track page with the page-icon section inverted and a "COPY" label
box over the step row.

### F2 (hold B2): track select and pick-up/drop

| + key | Effect |
|---|---|
| **B3–B10** | **select track 1–8**. Holding that track key turns the white keys into its controls: **W1 mute · W2 solo · W3–W7 play mode** (FWD / REV / FWD-PONG / REV-PONG / RANDOM) **· W9–W16 colour presets** (W8, copy-arm, works only in MIX) |
| **W1–W16** (no track key held) | **pick-up / drop**: |
| | 1st press of this F2 hold, hand empty → **CUT** that step (even an empty one, so it can act as an eraser) |
| | hand loaded, **empty** step → **PASTE** (always) |
| | hand loaded and "holding", **filled** step → **PASTE** over it, hand becomes empty-handed |
| | hand loaded but empty-handed, **filled** step → **CUT** it |

Releasing F2 resets the hand (the next F2 hold starts with a fresh grab). F1 + step pre-loads
the hand. The step buffer is **per track** (`bufferedStep_` lives in each track engine), and
changing track resets the F2 state. So **steps cannot be copied between tracks** **[inferred
from code]**. Use the TOOLS PAGE clipboard to move material between tracks.

Screen: "TRK n" boxed, label box "PASTE / CUT" (or "MUTE / PLAY MODE" while a track key is
held). LEDs: B3–B10 = tracks (selected white, muted red, otherwise the track colour).

### F3 (hold B1+B2): rate, page length, tempo

| + key | Effect |
|---|---|
| **B3–B10** | **track rate**: 1:1, 1:2, 1:3, 1:4, 1:8, **1:16** (default), 1:32, 1:64 (steps per bar; `kRateShortcuts`). The 16-value rate list (1, 2, 3, 4, 5, 6, 8, 10, 12, 16, 20, 24, 32, 40, 48, 64) is reachable in the MIX menu |
| **W1–W16** | **length of the active page = n steps** (1–16). **This also adds the page to the loop** (`setPageLen` sets its enable bit). Pops "Pn LEN m" |
| **Encoder** | **BPM** 40–300 (acceleration ×5), in any view |
| **AUX tap** | **tap tempo** (rolling average; a gap over 2 s starts a new run) |

Screen: rate "1:16" over a 16-cell length bar. LEDs: B3–B10 rate keys (current cyan), step
row = page length (lit up to the length, last step green, rest off).

### Other copy / cut / paste scopes (outside the STEP view)

| What | Where / gesture |
|---|---|
| **Step** | STEP/TOOLS: F1+W = copy, F2+W = cut/paste (above), quick tap = clear (also cuts into the buffer). NOTES view: **quick tap B1 = copy** the selected step, **quick tap B2 = paste** |
| **Page** (16 steps + page length) | TOOLS → **PAGE** tool: **B6 COPY · B7 CUT · B8 PASTE** on the active page (choose it with F1). The buffer lives in the shell, so it **crosses pages and tracks**. Paste also adds the page to the loop |
| **Clear page / track** | F1+B8 / F1+B9 (undoable). Also AUX + hold B3 → "Clear Track?" YES/NO, and CLR on any ACTIONS page (not undoable) |
| **Track** | MIX: hold a track key (B3–B10), **tap W8 to arm** (1st tap = "COPY PAT TO?" = steps/pages/play mode/defaults · 2nd tap = "COPY ALL TO?" = whole track incl. MIDI, scale and colour), then tap destination track key(s). Releasing the held track cancels |
| **Pattern** | PATTERNS: F1+slot = copy · F2+slot = cut (filled slot) / paste (empty slot) · F3+slot = clear · quick tap B1/B2 = copy/paste the active pattern |
| **Transpose lane slot** | TRANSPOSE: F1+W = copy, then the next F1+W pastes · F2+W = cut, then paste · F3+W = lane length |

---

## 5. AUX layer (shift)

Code: `onKeyUpdate` (AUX block), `onKeyHeldUpdate`, `updateAuxViewLEDs`, `doStopOrKill`,
`togglePlayback`, `resetPlayback` (shell); `onKeyUpdateAuxMFXShortcuts` (aux_macro_manager.cpp).

**While AUX is held, the layer is modal.** Every key-down goes to the AUX layer, keys with no
AUX function do nothing, and their releases are swallowed too (`auxSwallowMask_`), so nothing
leaks into the view. The layer is the same in every view.

| AUX + | Effect |
|---|---|
| **B1** (release) | **Play / Pause** (pause keeps the position) |
| **B2** (release) | **Reset** to the loop start (sends MIDI Start if playing) |
| **B1 + B2** (either order) | **STOP** (stop and reset). **STOP again while stopped = KILL**: flush all notes, CC 123 + CC 120 on every track channel |
| **B1 held 800 ms while paused** | STOP (reset after a pause) |
| **B3 tap** (release) | **Rec arm** on/off (the record glyph and B3 LED turn red) |
| **B3 hold** | **Clear Track?** confirm (YES/NO on the encoder, click to confirm, AUX cancels) |
| **B4** | Record mode **OVERDUB ⇄ REPLACE** |
| **B5** | selected track's MidiFX **off** |
| **B6–B10** | selected track's MidiFX group **1–5**. **Hold** or **double-tap** opens that group's editor (the chord/arp engine) |
| **W1 / W2** | **Octave − / +** (−5…+4). Applies to the note palette, the MI keyboard and the Notes piano |
| **W3–W9** | **View**: W3 MIX · W4 STEP · W5 TRANSPOSE · W6 NOTES · W7 PATTERNS · W8 MI · W9 TOOLS. The switch is live (the view renders while AUX is still held) and the name pops. **Double-tap** = also send that view back to its first page ("HOME"). Views otherwise **remember their menu position** |
| **W10** | MidiFX pass-through quick edit (B1/B2 then step through the group's FX slots) |
| **W11** | bypass toggle of the group's selected FX slot |
| **W12** | jump to the arp params |
| **W13 / W14** | next arp pattern / next arp octave range |
| **W15 / W16** | arp hold / arp on-off |
| **turn encoder** | temporary EDIT of the selected cell |
| **AUX double-click** (if a macro is selected in MI → MACROS) | enter or exit the macro (M8 / NRN / DEL) |

Special cases:
- **AUX tap while holding step(s)** in STEP/TOOLS = reset value (§2). The layer doesn't open.
- **F3 held, then AUX** = tap tempo (§4).
- AUX in an open Quant/Clear submenu = cancel.

LEDs while AUX is held: W3–W9 = views (current white, others low white), B1 green while
playing, B3 red when armed / dark red otherwise, B4 orange in REPLACE. The MidiFX keys
(B5–B10) show the group colours.

**Page changes are not on the AUX layer.** Pages live on F1 (owner's deliberate deviation,
FORM_DESIGN.md §6).

---

## 6. Pages

Code: `Track` page fields and `positionToStep` / `stepToPosition` / `syncLen` (omni_structs.h);
`setPageLen`, `setEnabledPages`, `setTrackLen`, `setActivePage`, `key16toStep` (machine);
`handlePageGesture` (shell).

### Data
- **4 pages × 16 steps = 64 steps** per track (`steps[64]`, page p = steps p·16 … p·16+15).
- Each page has **its own length 1–16** (`pageLen[4]`, default 16). A page always starts at
  its step 1: there is **no per-page start step** (`Track::startstep` is reserved and never
  read).
- `enabledPages` (4-bit mask) = which pages are **in the loop**. Default: **page 1 only**.
  The mask can never be empty (falls back to page 1).
- **Active page** (`activePage_`, 0–3) = the page shown and edited on W1–W16. It is
  independent of the loop: you can edit a page that doesn't play (its F1 LED is bright red).

### Playback
- The track plays in "position space": the **enabled pages in ascending order**, each for
  its own length, concatenated. Total length = sum of the enabled page lengths (max 64). The
  order can't be changed or repeated (no page chain).
- Example: pages 1+3 enabled, lengths 16 and 5 → 21-step loop: page 1 steps 1–16, then page 3
  steps 1–5.
- Track length, direction, play modes, jumps and conditions all work on this concatenated
  loop. A **jump target on a disabled page goes to the loop start**.
- Different tracks with different lengths and rates give polymeter for free.

### Gestures

| Gesture | Effect |
|---|---|
| **F1 + tap B3–B6** | make page 1–4 the **active (edit) page**. The loop is unchanged |
| **F1 + double-tap B3–B6** | **solo that page**: loop = only that page, and it becomes active (**loop one page**) |
| **F1 + hold page A + press page B** | **loop range**: enables **every page from A to B** (contiguous) and disables the rest. Active page = A. The anchor is the **lowest-numbered** held page (`for i in 0..3: first held`) (**loop several pages**) |
| **F1 + encoder** | active page −/+ (clamped 1–4) |
| **F3 + W(n)** | active page length = n, and **adds the active page to the loop** without removing others. This is the only way to build a **non-contiguous** loop (e.g. pages 1 and 3) in the STEP view |
| MIX: **F3 + W(n)** | **flat track length** = n + 16·(active page index): rebuilds the pages (pages 1…k enabled, all full except the last) (`setTrackLen`). Note that this differs from STEP F3 |
| MIX menu → TRACK → **LEN** 1–64 | same flat rebuild |
| TOOLS → PAGE paste | pastes a page and adds it to the loop |

Select vs solo timing: the first tap of a double-tap already selects the page on its release.
The second release then solos it. A range gesture suppresses the tap/solo actions until all
page keys are released (`pageGestureDone_`).

### Page display and follow
- **No page-follow.** The edit page never moves on its own. While playing on another page, the
  step row shows no playhead.
- Track page screen: the 4 page icons (filled = in the loop, outline = out, underline = active
  page). The icons don't show the playing page.
- F1 LEDs: the **playing page flashes yellow** on B3–B6.
- The MI view and armed NOTES view draw **page bars with a playhead across all pages**
  (`drawPageBars`).
- NOTES view encoder (keyboard page) auto-advances the selected step across pages (step 16 →
  next page's step 1, clamped at page 1 step 1 and page 4 step 16).

---

## 7. Playing notes vs sequencing; recording

Code: `onKeyUpdateMI`, `onKeyUpdateNotes`, `recordPlayedNote`, `recordNoteReleased`,
`commitRecHeld`, `flushRecHeld`, `quantEnterSubmenu` / `quantMorphPreview` /
`quantExitSubmenu`, `togglePlayback` (shell); `recordResolveStep`, `recordNoteToStep`,
`recordNoteLen`, `lenFromSteps` (machine).

**There is no record/keyboard toggle inside the STEP view.** Playing and sequencing are
separate views:
- **MI view (AUX+W8)**: the **whole keybed** (B1–B10 and W1–W16) is a playable keyboard on
  the selected track's channel, using OMX's normal key→note map with the track's scale (LOCK
  and GROUP apply). B1/B2 are notes here, so there are no F-layers. The sequencer keeps
  running. The encoder (EDIT on page 0) selects the track.
- **NOTES view (AUX+W6)**: a fixed piano (B3–B10 = sharps F#4…A#5 · W5–W16 = naturals
  F4…C6, plus octave) for **editing the selected step**. When **armed** it becomes a live
  keyboard like MI.
- **Incoming MIDI notes are ignored** (not played, not recorded).

### Live recording

| Step | Mechanics |
|---|---|
| Arm | **AUX + tap B3** in any view. Recording only happens from the **MI** and **NOTES** keyboards |
| Start | If armed and stopped, the **first note played resets and starts the transport** (start-on-note) |
| Capture | At **press**, the note resolves to the nearest step: if under halfway into the current step → the step that just fired with a **positive nudge**, otherwise the next step with a **negative nudge**. Nudge = offset × 60 × (100 − quantize %)/100. The track is fixed at press time |
| Commit | **On release**: the note is added to that step (overdub dedups), the nudge is written, and **LEN = how long it was held** (snapped to 0.125/0.25/0.5/0.75, whole steps up to 16, then 1/2/3/4 bars). Writing on release stops the sequencer replaying and cutting the held note |
| Velocity | Not velocity-sensitive. A newly filled step gets the **track default velocity** |
| Overdub / Replace | **AUX+B4**. Replace clears a step's notes the first time it's hit in each pass (resets when the selected track's loop wraps) |
| Limit | 8 notes held at once. Beyond that: "REC FULL" popup and a red flash on AUX |
| Mono track | each recorded note replaces the step's note |
| Stop | stopping commits held notes and **disarms** |
| Quantize | `recQuantize_` starts at **0 %** (keep played timing). The **QNT/QUANT** action (any ACTIONS page) opens a submenu: the encoder scrubs 0–100 % in steps of 5 and **previews the morph live** on the track's nudges; click applies it and stores the % for later recording; AUX cancels and restores |

**Step recording:** no auto-advancing step-record mode. The closest is the NOTES view: choose a
step (W1/W2 quick taps = prev/next step, F1 + W = jump to step, encoder on page 0 =
next/prev step across pages) and play a note or chord on the piano to write it.

NOTES-view extras (`onKeyUpdateNotes`): **hold W1** = velocity palette · **hold W2** = length
palette · **hold W1+W2** = math palette · **hold W3** = chance palette (B1–B10 set the value on
the selected step) · **W4 tap** = clear the step · **W4 held then released** (on a step-param
cursor) = clear that param's lock · quick tap B1/B2 = copy/paste step · F1/F2/F3 as in STEP
(F1 + W = jump to step).

---

## 8. Tracks

Code: `OmniSeq`, `Track` (omni_structs.h); `editPage` / `drawPage` (machine menu pages);
`onKeyUpdateMix*`, `onEncoderMix`, `selectMachine` (shell).

- **Count:** 8 (4 on Teensy 3.1). Each is the same polyphonic engine. MIDI channels default to
  1–8.
- **Select:** STEP/NOTES/TOOLS: **F2 + B3–B10**. MIX: **tap B3–B10**. MIX/STEP/MI overview:
  encoder in EDIT.
- **Mute / solo:** in **MIX**: **F1 + B3–B10 = mute toggle** · **F2 + B3–B10 = solo toggle**
  (neither changes the selection) · hold a track key → **W1 mute, W2 solo** · TRACK grid
  cells. Also from STEP with F2 + hold track key + W1/W2.
  - Muted or not-soloed tracks **keep advancing silently**, and their ringing notes are cut
    at once.
  - Any solo silences all non-soloed tracks.
  - LEDs in MIX: muted tracks go dark, soloed tracks flash.
- **Step mute:** MIX, **F1 + W** toggles that step's mute on the selected track.
- **FILL:** **holding F2 in MIX** turns Fill on for all tracks (FILL steps play, !FILL steps
  don't). This is the only place Fill can be triggered.

### Per-track settings (MIX → TRACK grid and the machine menu; F3 shortcuts)

| Setting | Range / default |
|---|---|
| RATE | 1:1 … 1:64 (16 values), default 1:16 |
| LEN | 1–64 flat, or per-page lengths (§6) |
| GATE | 0–200 % (stored 0–100), default 80 %. Note length = step LEN × gate (default 0.75 × 0.8 = 0.6 step) |
| SWING / S-DV | −100…+100 / 16th or 8th. Delays (or advances) every other 16th (or 8th) by swing % of a step. Counted per played step, not per bar position |
| TRIP | triplet: step time ×4/3 **and every 4th position is skipped** |
| DIR / MODE | forward/reverse · NONE, PONG, RAND, RND2 (random, no repeat), SHUF (reshuffle every loop), SHLD (shuffle once) |
| CHAN, MONO, MIDI, CV | channel 1–16 · monophonic (last note of a chord only; a new note cuts held ones) · send MIDI / CV |
| MFX | track MidiFX group (steps set to TRK follow it) |
| TPOS / TYPE / TPAT | transpose ±64 · GI (scale intervals) / SE (semitones) / LI (local intervals) · apply the transpose lane |
| Scale MODE / ROOT / SCALE | GL (global) / CH (chromatic) / LO (own root and scale). LOCK and GROUP stay global |
| Defaults | the 8 step defaults (§3), incl. default velocity (LEVELS) |
| Pot bank | which CC map the 5 knobs use |
| Colour | 8 presets (hold track + W9–W16) or K5. Global, not per pattern |

---

## 9. Everything else

| Feature | How it works in FORM |
|---|---|
| **Ratchets** | REPEAT ×1–×4. The step's tick span is split into equal sub-hits measured from the actual fire time (nudge included). Each hit re-sends the note (note-off then note-on of the same number) at the same velocity and length, and resends the CC locks. A step that doesn't fire (chance/condition) cancels its ratchet (`onClockTick` ratchet block) |
| **Probability** | rolled first: prob N fires N % of the time; 0 never fires |
| **Conditions** | after the roll. FILL / !FILL (MIX F2 hold) · PRE / !PRE (previous evaluated step on this track fired) · NEI / !NEI (the track to the left's last step fired; tracks evaluate 1→8) · 1ST / !1ST (first loop since reset) · **A:B** (plays on loop A of every B; B up to 8; the loop counter counts full track lengths) |
| **Step functions** | only run if the step fired. RSET = next step is the loop start and loop counters, the lane and shuffles reset · `<<` / `>>` **permanently** set the track's direction · `<>` flips it · J? = random step · `???` = random of none/restart/rev/fwd/flip · Jn = jump to step n. A step function overrides the play mode for that step |
| **Micro-timing** | NUDG −60…+60 = ±1 step. Encoder, or the param-page palette (9 keys, zero on B5), or recorded. Reverse playback mirrors nudges. TOOLS HUMANIZE / QUANTIZE and the QUANT morph edit nudges in bulk |
| **Accent** | none (velocity only) |
| **Slide / tie** | none as such. Long lengths (up to 4 bars) and gate up to 200 % give overlaps. A repeated note number is cut before it retriggers. MONO cuts too. No glide/legato flag |
| **Ghost trigs** | steps with no notes but `trig = 1`: fire CC locks, functions and conditions without notes (made by any palette lock on an empty step, or AUX-reset in Note mode) |
| **CC P-Locks** | 5 per step (hold step + knob, or the CC page encoder). Sent once when the step fires, on the track's pot-bank CC numbers |
| **Per-step MidiFX** | each step can route to Off / track group / group 1–5 |
| **Randomise** | TOOLS: SHUFFLE, HUMANIZE, VEL RANDOM (min/max), CHANCE RND, EUCLID, GRIDS. Play modes RAND/RND2/SHUF/SHLD. Functions J? / `???`. TRANSPOSE lane: B8 random values, B9 random values + length, hold B3 + W = random value on that slot |
| **Transpose** | track TPOS · the **transpose lane** (16 slots ±48, length 1–16; it advances one slot per played step position when TPAT is on) · per-step **ACUM 1–4**: each time the step fires it moves its own position in the lane by ACUM, so the step **walks** the lane loop by loop · TOOLS TRANSPOSE (oct/semi, destructive) and SCALE SNAP. Chromatic tracks always transpose in semitones |
| **Scale / key** | per-track GL/CH/LO. The SCALE page in STEP has palette keys: **MODE B3/B4/B5 = GL/CH/LO · ROOT B3–B9 = C D E F G A B · SCALE B3–B10 = first 8 scales · LOCK/GROUP B6 = off, B7 = on**. ROOT/SCALE edit the track's own scale when LO, else the global one, and are inert on CH. The note palette always uses scale degrees |
| **Undo** | **one level**, for the selected track's whole data. Snapshot taken before every destructive TOOLS action and before F1+B8/B9. **F1+B10** or **TOOLS B10** swaps it back; pressing again redoes. Lost on a pattern switch. **Not** for step edits, cut/paste, recording, CLR-confirm or pattern ops |
| **Patterns** | 16 slots, each a snapshot of all 8 tracks. Switch styles (PATTERNS B3–B6): Finish Loop (at the selected track's loop end) / Next Bar / **Instant** (default) / Chained (tapping slots builds a chain that advances every loop). A progress bar shows when a queued switch will land. No song mode |
| **Tempo** | F3 + encoder (any view), F3 + AUX tap tempo, TOOLS → BPM, MIX TIMINGS |

---

## 10. Screen summary per state

| State | Screen |
|---|---|
| STEP overview, idle | track page (§1): track squares, transport, "SEQ", BPM, "TRK n", play icon, rate, 4 page icons, 16-step row with playhead |
| STEP overview, EDIT mode | the same, with "TRK n" boxed |
| Step held, NOTE mode | mini piano with the chord + step strip |
| Step held, other modes | value text (e.g. "VEL 101", "x3", "3:4", "JMP > 5") + step strip with a marker on the focus step |
| Param pages A/B | 4 cells (label over value). Nothing held = track defaults; held = step values, **locked labels inverted**. Value inverted while editing |
| CC page | 5 bars + big bank digit + "CC" title. Held step: "CC LOCK" + that step's locks |
| STEPNOTES | 6 note slots (names or numbers) for the step chosen by tapping a white key |
| SCALE page | 5 cells MODE/ROOT/SCALE/LOCK/GROUP. Inactive cells greyed with a checkerboard |
| ACTIONS page | QNT @ · CLR µ · POTS @ · NTRY PR/TG |
| F1 held | track page with the page section inverted + "COPY" label. F1 + encoder: large page icons for 0.9 s |
| F2 held | "TRK n" boxed + "PASTE / CUT" (or "MUTE / PLAY MODE" with a track key held) |
| F3 held | "1:16" rate + length bar of the active page. F3 + encoder: "BPM n" popup |
| AUX held | the current view with its tag boxed. A view name pops on switching |
| Transient popups | mode names, "CUT"/"PASTE"/"COPY", "P2 LEN 12", "RATE 1:8", "PLAY"/"PAUSE"/"RESET"/"STOP"/"KILL", "REC ARM"/"REC OFF", "OVERDUB"/"REPLACE", "Octave: n", "UNDO"/"REDO"/"NO UNDO", "CLR P2"/"CLR TRACK", "REC FULL", group names ("TRACK SETUP", "STEP") |
| MI page 0 | track page without page icons or step row, plus page bars with the playhead across all pages |
| PATTERNS | active slot, switch style, queued ">Pn" or "CHn", progress bar. F1/F2/F3 show "COPY SLOT" / "CUT PASTE" / "CLEAR SLOT" |

---

## 11. Notes for the QUADRILLE / FM-1 port (my commentary, not FORM behaviour)

- **F1/F2 are black keys B1/B2 in FORM.** That is why every palette is "B1–B10 while a step
  is held" and why F-layers switch off during a step hold. On the FM-1 (F1/F2 → OCT−/OCT+,
  F3 = both), the 10 black keys stay free for palettes, but these FORM gestures then need
  the OCT buttons as modifiers instead: F1+step copy, F2 pick-up/drop, F1+B3–B6 pages,
  F3+step page length, F3+B3–B10 rate.
- FORM leans on **10 black keys = 10 values** almost everywhere: velocity tenths, length
  0.5…64, chance tenths, Fill/!Fill + A(1–4):B(1–4), 7 functions, MFX Off+5, 9-step nudge,
  10 scale degrees. Larger ranges are encoder-only.
- **The override model is concrete-value + lock bit, and editing a default rewrites the
  unlocked steps.** Clearing is a separate gesture (AUX tap, or encoder click). No palette
  key means "back to default".
- **Loop one page** = F1 + double-tap the page key. **Loop several** = F1 + hold one page key
  + press another (contiguous). **Add a page** without removing others = select it, then
  F3 + step (sets its length and enables it). There is no page-follow and no per-page start.
- **Live recording is outside the step view** (MI/NOTES), and it is release-committed with
  nudge + held length.
