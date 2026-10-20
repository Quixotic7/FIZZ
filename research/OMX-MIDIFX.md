# OMX-27 MIDI FX (v1.15.4): mechanics reference

Source: `OMX-27` repo, tag **v1.15.4**, read with `git show v1.15.4:<path>`. Everything below was read from the code
at that tag unless marked **[inferred]** (worked out from the code, not stated in it) or **[doc]** (from the owner's
`Docs.md`, not checked line by line). The companion study of the sequencer is `FORM-SEQUENCER.md`; its key names are
used here.

| Name here | OMX key index | Panel |
| --- | --- | --- |
| **AUX** | 0 | top-left key |
| **B1..B10** | 1..10 | the black keys (B1 = F1, B2 = F2, both = F3) |
| **W1..W16** | 11..26 | the white keys |

Tap = under 200 ms; double-tap = second release within 200 ms; hold = 800 ms (`omx_keypad.cpp`, see
FORM-SEQUENCER.md §0).

Files (all under `OMX-27-firmware/`):

| File | What it holds |
| --- | --- |
| `src/config.h` | `NUM_MIDIFX_GROUPS 5`, `NUM_MIDIFX_SLOTS 8`, the type enum `MIDIFXTYPE`, `MidiNoteGroup`, the type colours (`ColorConfig`) |
| `src/config.cpp` | `kArpRates[] = {1,2,3,4,5,6,8,10,12,16,20,24,32,40,48,64}` (the rate table every FX uses) |
| `src/modes/submodes/submode_midifxgroup.cpp/.h` | **the group**: the chain, routing, note-off tracking, the editor (keys, LEDs, OLED), save / load |
| `src/midifx/midifx_interface.cpp/.h` | the FX base class: encoder model, bypass, output pointer |
| `src/midifx/midifx_*.cpp/.h` | the ten FX types; `midifx_notemaster` (note tracking helper for Arp and Repeat) |
| `src/utils/aux_macro_manager.cpp` | the AUX shortcuts (group select, editor entry, arp quick keys) shared by every mode |
| `src/utils/chord_util.cpp`, `chord_structs.h` | the chord engine the Chord FX borrows from the Chords mode |
| `src/form/omx_mode_form.cpp`, `machines/form_machine_omni.cpp`, `omni_structs.h` | FORM's routing into the groups |
| `OMX-27-firmware.ino` | where the five groups are saved and loaded |
| `Docs.md` (repo root) | the owner's user docs (MidiFX and Arp sections; older than the code: they list 7 types, the code has 10) |

---

## 1. Architecture

`submode_midifxgroup.cpp` (`SubModeMidiFxGroup`), `midifx_interface.h`, `OMX-27-firmware.ino`.

### Groups and slots

| | value | source |
| --- | --- | --- |
| groups | **5** (`subModeMidiFx[5]`, one global array) | `config.h` `NUM_MIDIFX_GROUPS`; `submode_midifxgroup.cpp` |
| slots per group | **8**, an ordered chain ("a pedalboard": slot 1 -> slot 8 -> out) | `NUM_MIDIFX_SLOTS`; Docs.md |
| what a slot holds | nothing, or one FX instance of any of the 10 types; any type can appear more than once | `changeMidiFXType()` |
| scope | **global**: the same 5 groups serve every OMX mode (MI, Chords, Drum, Euclid, FORM). Saved once, after the modes, before FORM | `OMX-27-firmware.ino` (`savePatterns` / `loadPatterns`) |

### The chain (`reconnectInputsOutputs()`, "where the magic happens")

- Walks the slots from 8 down to 1. The last non-empty slot's output goes to the group output; every other non-empty
  slot's output goes to the **next non-empty slot**. The group's input points at the first non-empty slot, or straight
  at the output when the group is empty. **Empty slots are skipped, not "pass-through FX".**
- Rebuilt whenever a slot is added, removed, pasted, cut or moved.
- **Processing order = slot order**, each FX seeing what the slots before it produced (an arp after a harmonizer
  arpeggiates the harmony; a harmonizer after an arp harmonizes every arp note).

### Input

`SubModeMidiFxGroup::noteInput(note)`: stamps `note.prevNoteNumber = note.noteNumber` (the pitch as it entered the
group) and hands it to the first slot.

A note is a `MidiNoteGroup` (`config.h`): channel, noteNumber, prevNoteNumber, velocity, `stepLength` (in steps; 0 for
live keys), sendMidi, sendCV, noteonMicros, **`unknownLength`** (true = a held key, the note-off comes later as its own
event; false = a sequencer note whose length is known), **`noteOff`**.

### Output and note-off tracking (`noteOutputFunc()`, `onPendingNoteOff()`)

- The group keeps a table of **32** sounding notes: (channel, prevNoteNumber, the output noteNumber).
- A note-on leaving the chain is recorded and sent on. **If all 32 entries are full the note-on is dropped silently.**
- A note-off leaving the chain (it carries the original `prevNoteNumber`) is matched against the table by channel +
  prevNoteNumber, and **one note-off goes out for every output note that input produced**. So FX that change pitch or
  add notes (Transpose, Scaler, Harmonizer, Chord) need no note-off logic of their own: the group sends the right
  offs. (Randomizer, which can change the channel, tracks its own.)
- Fixed-length notes (sequencer notes, arp and repeat output) are turned off by the mode's pending-note-off queue; that
  queue calls `onPendingNoteOff(note, channel)` on every group to clear the entry.
- Arp and Repeat output carry `prevNoteNumber = noteNumber` (they are new notes) and fixed lengths.

### Pass-through and bypass

| state | what happens | how it is set | saved? |
| --- | --- | --- | --- |
| route **Off** | notes skip the groups entirely | AUX + B5 (a track / the MI keyboard), or a step's MFX = OFF in FORM | (route is mode data) |
| **group empty** | input wired straight to output | no FX in any slot | yes |
| **slot bypassed** ("chain bypass") | `processNoteInput()` sends the note out unchanged; the FX and its settings stay in the chain | AUX + W11 (selected slot, toggles) or, in the editor, hold the slot + B1 (on) / + B2 (off) | **no** [inferred: `chainBypass_` is not written by any `saveToDisk`, so a reload brings every slot back on] |
| FX's own **off** | Arp and Repeat have a MODE = OFF that passes notes through | MODE param, W16 for the arp | yes |
| per-FX **chance** | every type except Chance itself has a CHC% that decides per note whether the FX acts (else the note passes unchanged) | the FX's CHC% | yes |

**The shared chance rule** (Transpose, Randomizer, Selector, Harmonizer, Scaler, Mono, Chord, Repeat, Arp):
`if (chance != 100 && (chance == 0 || random(100) > chance))` pass through. So 100 = always, 0 = never, and a value
c in 1..99 acts with probability (c + 1)/100 [inferred from `random(100)` returning 0..99].

### The Selector branch (`midiFxSelNoteInput()`)

The only non-linear routing. A Selector in slot s with LEN n treats slots s+1 .. s+n as **alternatives**: each note goes
to **one** of them (by MODE: random, up, down), whose output is re-pointed past the block to slot s+n+1 (or the next
non-empty slot after it, or the group output). Details in §2.4.

### Saving (`saveToDisk()` / `loadFromDisk()`)

Per slot: one type byte (0 = empty), then the FX's own packed struct. Bypass state, held notes and the arp's
running state are not saved.

---

## 2. The FX types

The type enum (`config.h` `MIDIFXTYPE`) is also the order of the **type keys** in the editor: hold a slot, press W1..W11.

| W key | type | name / OLED short | LED colour (`ColorConfig`) | file |
| --- | --- | --- | --- | --- |
| W1 | NONE (removes the FX) | `-` | off | |
| W2 | **Chance** | Chance / `CHC` | MEDRED | `midifx_chance` |
| W3 | **Transpose** | Transpose / `TRAN` | PURPLE | `midifx_transpose` |
| W4 | **Randomizer** | Randomizer / `RAND` | RED | `midifx_randomizer` |
| W5 | **Selector** | Selector / `SEL` | ORANGE | `midifx_selector` |
| W6 | **Chord** | Chord / `CHRD` | CYAN | `midifx_chord` |
| W7 | **Harmonizer** | Harmonizer / `HARM` | ROSE | `midifx_harmonizer` |
| W8 | **Scaler** | Scaler / `SCAL` | YELLOW | `midifx_scaler` |
| W9 | **Make Mono** | Make Mono / `MONO` | INDIGO | `midifx_monophonic` |
| W10 | **Repeat** | Repeat / `RPT` | RED | `midifx_repeat` |
| W11 | **Arpeggiator** | Arp / `ARP` | BLUE | `midifx_arpeggiator` |
| W12..W16 | no type: the slot's FX is deleted and nothing replaces it [inferred: `changeMidiFXType` deletes, the `switch` has no case] | | off | |

Every FX edits the same way: the encoder walks its params across pages (SELECT), a click switches to EDIT, AUX held =
EDIT while held (`MidiFXInterface::onEncoderChanged`, `getEncoderSelect()`). Most params use `enc.accel(1)` (1 per
detent, accelerating); percentages and velocities often `accel(5)`. Ranges below are the `constrain()` limits.

### 2.1 Chance (`midifx_chance.cpp`)

Lets a note through or kills it.

| page | param | range | default | step |
| --- | --- | --- | --- | --- |
| 1 (a full-width bar "Trigger / Chance") | Chance % | 0..100 | **`random(100)`** (a fresh Chance FX starts at a random value) | 5 |

- A note-on passes if `random(100) <= chance`: **100 = all, 0 still lets about 1 % through** (r = 0) [inferred from
  the comparison].
- Note-offs always pass (the group drops offs it has no record of).

### 2.2 Transpose (`midifx_transpose.cpp`)

| page | param | OLED | range | default |
| --- | --- | --- | --- | --- |
| 1 | semitones | `ST` (`-`, `+n`, `-n`) | -24..+24 | 0 |
| 1 | octaves | `OCT` | -6..+6 | 0 |
| 1 | (empty cell) | | | |
| 1 | chance | `CHC%` | 0..100 (step 1) | 100 |

- Out = in + ST + 12 x OCT. **A result outside 0..127 is dropped** (not clamped).
- On a chance miss the note passes untransposed.

### 2.3 Randomizer (`midifx_randomizer.cpp`)

| page | param | OLED | range | default | effect |
| --- | --- | --- | --- | --- | --- |
| 1 | chance | bar "Rand / Chance" | 0..100 (step 1) | 100 | |
| 2 | note down | `NT-` | 0..12 | 0 | pitch = random in [n - NT-, n + NT+] |
| 2 | note up | `NT+` | 0..12 | 0 | |
| 2 | octave down | `OCT-` | 0..12 | 0 | then + 12 x random octave in [-OCT-, +OCT+]; clamped 0..127 |
| 2 | octave up | `OCT+` | 0..12 | 0 | |
| 3 | velocity down | `VEL-` | 0..127 (step 5) | 0 | velocity = random in [v - VEL-, v + VEL+] |
| 3 | velocity up | `VEL+` | 0..127 (step 5) | 0 | |
| 3 | length | `LEN%` | 0..100 (step 5) | 0 | stepLength x `map(random(LEN%), 0, 100, 1, 16)`: x1 .. x15, **lengths only grow**; 0 = unchanged; live keys (length 0) unaffected |
| 3 | channel | `CHAN` (off when 0) | 0..16 | 0 | channel = random in [ch, ch + CHAN - 1], clamped 1..16 |
| 4 | delay min | `DEL-` (`1/n`, off at 0) | 0..16 | 0 | random delay (below) |
| 4 | delay max | `DEL+` | 0..16 | 0 | |

- **Delay**: index 1..16 maps to the rate table reversed (1 = 1/64 .. 16 = 1/1), a fraction of a bar
  (`step_micros x 16`). One of DEL-/DEL+ set: delay = random fraction (0..1) of that length. Both set: a rate is
  interpolated at random between them and the delay is 1/rate of a bar. Delayed notes wait in a queue of **16**; a full
  queue plays the note at once.
- A note-off removes a still-delayed note with the same note number (so a short key press can cancel its own delayed
  note). [inferred edge: the match is on the randomized pitch against the incoming pitch, so a pitch-randomized
  delayed note may not be cancelled and then plays after its key is up.]
- Channel randomization tracks up to **16** notes so the off goes to the same channel; a repeat of a tracked note, or a
  full table, drops the note-on.
- Edge: the note range is `random(min, max)` with max = `min(v + NT+ + 1, 127)` exclusive, so note 127 is never
  produced [inferred].

### 2.4 Selector (`midifx_selector.cpp`, routing in `submode_midifxgroup.cpp` `midiFxSelNoteInput`)

Sends each note to **one** of the next LEN slots.

| page | param | OLED | range | default |
| --- | --- | --- | --- | --- |
| 1 | mode | `MODE`: `RND`, `UP`, `DOWN` | 0..2 | RND |
| 1 | length | `LEN` | 0..7 | 2 |
| 1 | (empty) | | | |
| 1 | chance | `CHC%` | 0..100 (step 1) | 100 |

- RND: slot s + random(1..LEN). UP / DOWN: a cursor walks s+1 .. s+LEN cyclically, one step per note.
- The chosen FX's output is re-pointed to the first non-empty slot at or after s + LEN + 1, else the group output.
  **The unchosen slots are skipped for that note.**
- Chance miss: the note skips the whole block (goes to slot s + LEN + 1). LEN 0: the note goes straight on. LEN 1: always
  slot s+1. A chosen slot that is empty: the note skips the block. A choice past slot 8: group output.
- **Note-offs go to every FX in the block**, each re-pointed past it, so whichever got the note-on also sees the off.
- Changing LEN rebuilds the chain on the next note (`didLengthChange()`).

### 2.5 Chord (`midifx_chord.cpp`, engine in `chord_util.cpp`)

Replaces each note with a chord built on it, using the Chords mode's engine. Two chord types; the pages change with the
type (`getParams()` swaps two `ParamManager`s, keeping the cursor).

| page | BASIC type | INTERVAL type |
| --- | --- | --- |
| 1 NOTES | a keyboard drawing of the last chord with its note names (`dispKeyboard`) | the same |
| 2 CHORD TYPE | `Basic` / `Interval` (`dispOptionCombo`) | the same |
| 3 CHANCE | bar "Chord Trigger / Chance", step 5, default 100 | the same |
| 4 | NOTE 0..11, OCT -5..+4, BALANCE 0..220, CHORD 0..36 (`dispChordBasicPage`) | `GLBL` on / off, `ROOT` 0..11, `SCALE` -1 (off) .. 36 |
| 5 | CUSTOM: six offsets -48..+48 (`RT` / `-` / `+n`); **only when CHORD = Custom** | #NOTES 1..4, DEGREE 0..7, OCT -2..+2, TRANSPOSE -7..+7 |
| 6 | | SPREAD -2..+2, ROTATE 0..4, VOICING 0..7 (`NONE POWR SUS2 SUS4 SU24 +6 +6+9 KB11`) |
| 7 | | SPREAD UP/DOWN on / off, QUARTAL on / off |

- **BASIC**: the input's pitch class **overwrites NOTE** and its octave sets an "auto octave" (input octave - 5), to
  which OCT is added. CHORD is one of 36 shapes (Major, Minor, sus2, sus4, mb5, Mb5, M#5, M9no5, dim7, m7b5, mb6, m6,
  m7, mMaj7, madd9, m7#5, m7b9no5, m9no5, M6add4no5, M7b5, Maj7b5, Madd9b5, Maddb5, M6, M7, Maj7, Madd9, M7#5,
  M7b9no5, Maj9no5, Maj7/6no5, sus4add#5, 7sus4, sus4#5b9, Fourths, Fifth) or Custom. **BALANCE** (23 shapes x 10,
  interpolated) sets how many of the 4 notes play and moves notes up or down an octave (inversions), crossfading the
  velocities between neighbouring shapes; default 40 = four notes.
- **INTERVAL**: the input's **scale degree overwrites DEGREE** (scale = global or the FX's own), the octave is
  automatic; the chord is stacked thirds in the scale (#NOTES), then VOICING, QUARTAL, SPREAD UP/DOWN, SPREAD,
  ROTATE (inversion) and TRANSPOSE are applied in that order (`constructChord`).
- **Velocity: every chord note is `ChordSettings.velocity` (100, not editable in the FX) x the balance multiplier;
  the input velocity is lost.** [read; consequence inferred]
- The chord's own MIDI channel (`mchan`) is computed but never applied: notes keep the input channel [inferred:
  `chordNotes_.channel` is set, `inNote.channel` is not changed].
- GLBL on: ROOT / SCALE edit the **global** scale (other modes and Scalers following it change too).
- Notes outside 0..127 are skipped. Up to 6 notes per chord.
- Because NOTE and DEGREE are overwritten by the input, editing them in the FX has no audible effect [inferred].

### 2.6 Harmonizer (`midifx_harmonizer.cpp`)

Adds up to seven fixed-interval notes.

| page | params | range | default |
| --- | --- | --- | --- |
| 1 | `ORIG` (play the input note), `NT 2`, `NT 3`, `NT 4` | ORIG on / off; offsets -126..+127 semitones (`--` = 0 = off) | ON; 0 |
| 2 | `NT 5`, `NT 6`, `NT 7`, `NT 8` | the same | 0 |
| 3 | `CHC%` | 0..100, step 5 | 100 |

- Out: the input (if ORIG), then input + each non-zero offset, clamped to 0..127; duplicates among the added notes are
  skipped (not against the original). Velocity and length are the input's.
- Chance miss: only the input note.
- The Docs' tip: chance under 100 makes "random chords".

### 2.7 Scaler (`midifx_scaler.cpp`)

| page | param | OLED | range | default |
| --- | --- | --- | --- | --- |
| 1 | global | `GLBL` ON / OFF | | ON |
| 1 | root | `ROOT` (note name) | 0..11 | global's |
| 1 | scale | `SCALE` (index; `Off` = chromatic) | -1..36 (37 scales: major ... ) | global's |
| 1 | chance | `CHC%` | 0..100, step 1 | 100 |

- A 12-entry remap table: **every pitch snaps down to the nearest scale note at or below it** (in the root's frame).
  Results outside 0..127 are dropped.
- GLBL on: ROOT / SCALE edit the **global** scale (`scaleConfig`) and the FX re-reads it every loop.

### 2.8 Make Mono (`midifx_monophonic.cpp`)

| page | param | range | default |
| --- | --- | --- | --- |
| 1 | `CHC%` | 0..100, step 5 | 100 |

- Last-note priority **per MIDI channel**: a new note-on first sends a note-off for the channel's previous note.
- No retrigger of the earlier note when the newer one is released.

### 2.9 Repeat (`midifx_repeat.cpp`): note repeat / ratchet

| page | param | OLED | range | default |
| --- | --- | --- | --- | --- |
| 1 | chance | bar "Repeat / Chance" | 0..100, step 1 | 100 |
| 2 | mode | `MODE`: `OFF` `ON` `1-ST` `ONCE` `HOLD` | | **ON** |
| 2 | rate | `RATE`: `HZ` or `1/n` | -1 (Hz), 0..15 = 1/1 .. 1/64 | 6 = **1/8** |
| 2 | rate in Hz | `RTHZ` | 0..255 -> 0..100 = 0.1..1 Hz, 100..255 = 1..50 Hz | 100 = 1 Hz |
| 2 | gate | `Gate` | 2..200 % of the repeat interval, step 5 | 90 |
| 3 | quantize | `QUANT`: off / `GBL` / `1/n` | -2 (off), -1 (global), 0..15 | -1 = GBL (the global quantize, default 1/16) |
| 3 | count | `#RPT` | 1..64 (stored 0..63) | 4 |
| 4 | fade velocity | `FVEL` / `FADE` | on / off | off |
| 4 | start | `STRT` | 0..100 % of the input velocity, step 5 | 50 |
| 4 | end | `END` | 0..100 %, step 5 | 100 |
| 5 | fade rate | `FRAT` / `FADE` | on / off | off |
| 5 | start, end | `STRT`, `END` | 1/1..1/64, or 0.1..50 Hz when RATE = HZ | 1/8, 1 Hz |

- Each input note (up to **16**) repeats at RATE (synced: 1/n of a bar; or free in Hz), each hit's length = GATE % of
  the interval.
- Modes: **ON** repeats while the note is held, **1-ST** plays #RPT hits even after release (a new note-on of the same
  pitch restarts it), **ONCE** plays at most #RPT hits and stops early on release, **HOLD** latches: repeats forever;
  after every key is up, the next note replaces the latched set. OFF passes notes through.
- New notes wait for the QUANT grid (unless off), so repeats start on the beat.
- Fade velocity / rate interpolate from STRT to END over the #RPT hits (meaningful in 1-ST / ONCE; in ON / HOLD the
  counter stops at 0 and the value stays at END) [inferred].
- Sequencer notes (known length) count as held for their length.
- Chance miss: the note passes un-repeated.

### 2.10 Arpeggiator (`midifx_arpeggiator.cpp/.h`)

Seven pages (`params_.addPage`: 1, 4, 4, 4, 4, 17, 17):

| page | params (OLED) | range | default |
| --- | --- | --- | --- |
| 1 | chance bar "Arpeggiator / Chance" | 0..100, step 5 | 100 |
| 2 | `MODE` | `OFF` `ON` `1-ST` `ONCE` `HOLD` | **OFF** |
| 2 | `PAT` | 17 patterns (below) | UP |
| 2 | `RSET` | `NORM` `NOTE` `MPAT` `TPAT` | NORM |
| 2 | `CHC%` (the same chance) | 0..100, step 5 | 100 |
| 3 | `RATE` | 1/1 .. 1/64 (the 16 rates) | **1/8** |
| 3 | `RANG` octave range | 1..8 (stored 0..7) | **2** |
| 3 | `GATE` | 2..200 %, step 5 | 90 |
| 3 | `BPM` (the **global** tempo) | 40..300 | |
| 4 | `ODIST` octave distance | -24..+24 semitones | +12 |
| 4 | `QUANT` start quantize | off / `GBL` / 1/n | GBL |
| 5 | inspect, read only: `VEL`, `CHAN`, `MIDI`, `CV` | | |
| 6 | **mod pattern**: 16 steps + its length (param 17) | 14 mods per step; length 1..16 | all "as played"; 16 |
| 7 | **transpose pattern**: 16 steps + its length | -48..+48 semitones per step; length 1..16 | all 0; 16 |

**Modes** (`changeArpMode`, `sortNotes`, `arpNoteTrigger`):

| mode | note list | runs | stops |
| --- | --- | --- | --- |
| OFF | | notes pass through | |
| ON | the keys held now | while any key is held | at release |
| 1-ST (one-shot) | the latched list | once through pattern x octave range, even after release | at the end of the range |
| ONCE | the keys held now | once through pattern x range | at the end, or at release |
| HOLD | latched: notes add while any key is down; the first key after all are up starts a new list | forever | MODE off / W16 |

**Patterns** (`ArpPattern`; the order on the screen; examples for C E G B):

| # | OLED | name | order |
| --- | --- | --- | --- |
| 0 | UP | Up | C E G B |
| 1 | DN | Down | B G E C |
| 2 | UPDN | UpDown | C E G B G E (ends not repeated) |
| 3 | DNUP | DownUp | B G E C E G |
| 4 | U&D | Up & Down | C E G B B G E C (ends repeated) |
| 5 | D&U | Down & Up | B G E C C E G B |
| 6 | CON | Converge | C B E G |
| 7 | DIV | Diverge | G E B C |
| 8 | C-V | Con-Div | C B E G E B |
| 9 | HI 1 | Hi-Up | B G B E B C (the top note between each) |
| 10 | HI 2 | Hi-UpDown | B G B E B C B E |
| 11 | LO 1 | Low-Up | C E C G C B |
| 12 | LO 2 | Low-UpDown | C E C G C B C G |
| 13 | RAND | Random | any note, repeats allowed |
| 14 | ROTH | Rand Other | random, never the same note twice in a row |
| 15 | RONC | Rand Once | a shuffle, every note once, reshuffled each octave pass |
| 16 | ASP | As Played | the order the keys were pressed |

**Octaves**: after each pass through the list the octave counter moves on; a note is played + octave x ODIST, for
RANG octaves. ODIST -12 runs the octaves downwards; other values give non-octave steps (+7 climbs in fifths).

**Reset** (RSET): NORM = never restarts mid-run; NOTE = restarts on every new note; MPAT = restarts when the mod pattern
wraps; TPAT = restarts when the transpose pattern wraps.

**Mod pattern** (`ModPattern`, `applyModPattern`): one mod per arp step, its own length, so it runs as a polymeter
against the note list.

| # | OLED char | mod | does |
| --- | --- | --- | --- |
| 0 | `×` | As Played | the arp's note |
| 1 | `.` | Rest | silence |
| 2 | `-` | Tie | silence; and lengthens the previous note by one arp step (`findStepLength`) |
| 3 | `R` | Repeat | the last played note again (or the last chord / power chord) |
| 4 | `<` | LoPitch -Oct | the lowest held note - 12 |
| 5 | `>` | HiPitch +Oct | the highest held note + 12 |
| 6 | `"` | PwrChord | lowest and highest note together |
| 7 | `#` | Chord | every held note together |
| 8..13 | `1`..`6` | Note 1..6 | the n-th note as played (rest if fewer are held) |

**Transpose pattern**: a semitone offset per arp step, its own length; added before the octave.

**Timing and output** (`loopUpdate`, `onClockTick`, `playNote`):
- One arp step every `step_micros x 16 / RATE` (RATE = 1/8 -> two 16th steps).
- The start waits for the QUANT grid (`currentClockTick % (384 / rate) == 0`; off = at once).
- Each note: length = GATE % x the arp step x the tie count; **velocity = the first held note's velocity** (all
  notes); channel = **each note's own input channel**; sendMidi / sendCV from the first note.
- Up to **8** notes in the list. Sequencer notes (known length) count as held for their length.

**Arp edge cases**
- A new Arp FX starts in **MODE OFF**: it passes notes until turned on (W16 or MODE).
- PwrChord and Chord shift by octave x 12, not by ODIST; LoPitch / HiPitch ignore the octave counter.
- With one held note, "highest" is taken as that note + 12.
- `swing_` is stored and saved but never used.
- `getClone()` copies RATE into QUANT (a bug: a pasted arp gets a different quantize).
- The W14 shortcut cycles RANG 1..4 only; the param reaches 8.
- Docs.md says "the arpeggiator sends clock when in use"; at this tag `seqConfig.numOfActiveArps` is counted but
  nothing reads it [inferred].
- Docs.md: keep to about 3 arps per group for performance; the save code notes the unit froze at 29 arps.

---

## 3. The editor

`SubModeMidiFxGroup::onKeyUpdate`, `updateLEDs`, `onEncoderChanged`, `onDisplayUpdateMidiFX`;
`AuxMacroManager::onKeyUpdateAuxMFXShortcuts`, `onKeyHeldAuxMFXShortcuts`, `UpdateAUXLEDS`; FORM's
`onEncoderButtonDown` (the FX cell).

### 3.1 Opening it

| gesture | opens |
| --- | --- |
| **AUX + B6..B10 double-tap** | the editor of group 1..5 |
| **AUX + B6..B10 held** (800 ms) | the same |
| AUX + B6..B10 tap | only routes the track (MI: the keyboard) to that group; pops `TRK MFX n` (FORM) / `MidiFX n` (MI) |
| AUX + B5 | route off |
| **AUX + W10** | **pass-through quick edit** of the selected group (`MFX PASS`): the keybed keeps playing |
| **AUX + W12** | quick edit of the group's **arp** (creates one in the first empty slot if there is none) |
| FORM: click the **FX cell** on the TRACK page | the routed group's editor (`MFX OFF` if the track is unrouted) |

On entry the editor selects the first non-empty slot if the selected one is empty.

### 3.2 The full editor (keys)

| keys | does |
| --- | --- |
| **B3..B10 press** | select slot 1..8 (its params show) **and hold it** |
| B3..B10 held + **W1..W16** | set the slot's type (table §2); W1 deletes the FX; the OLED pops the type's name |
| B3..B10 held + **encoder** | **move the slot** one place per detent left / right (wraps round; the others close up) |
| B3..B10 held + **B1** | slot **on**; + **B2** = slot **off** (bypass; the slot's LED turns red; pops `n TYPE On/Off`) |
| **B1 held + B3..B10** | **copy** that slot (`Copy` label while B1 is held) |
| **B2 held + B3..B10** | **paste** into that slot (replaces what is there) |
| **B1 + B2 held + B3..B10** | **cut** (copy, then empty the slot) |
| encoder turn | walk the selected FX's params (SELECT) or change the value (EDIT) |
| encoder click | SELECT <-> EDIT |
| **AUX held + turn** | EDIT while AUX is held |
| **AUX tap** | exit (pops `Exit`). If the editor was opened with AUX still down, the first release does not exit (`auxReleased_`) |
| any key's hold event | swallowed (nothing) |

Adding = hold an empty slot + a type key. Removing = hold + W1, or cut. Reordering = hold + encoder, or cut / paste.
Changing = hold + another type key (**the old FX and its settings are deleted**; the same type again does nothing).
The copy buffer holds one FX and belongs to the group (`copyBuffer` is a member of `SubModeMidiFxGroup`), so a slot
cannot be copied from one group into another [inferred from that].

### 3.3 The full editor (OLED, 128 x 32)

| state | screen |
| --- | --- |
| a slot held | **the 8 slots as short names** (`ARP`, `RAND`, `-` ...), the held one rising out of the row over about 140 ms (`dispSlots`, `heldAnimPos_`) |
| B1 / B2 / both held | the label `Copy` / `Paste` / `Cut` with the page dots |
| selected slot empty | `No FX` |
| otherwise | the selected FX's own page: usually four labelled cells (`dispGenericMode2`, the cursor boxed, inverted in EDIT, page dots); some pages are one full-width bar (chance), a keyboard (Chord NOTES), a 16-character strip (arp mod pattern), a 16-bar graph (arp transpose pattern) |
| popups | the type's name on a type change; `n TYPE On/Off` on a bypass change |

(The group also has a slot-list menu, "FX 1..FX 8" over two pages and an "Exit" page, but every way in sets the
param view, so it is never shown [inferred: `midiFXParamView_` is set true on entry and only cleared on exit].)

### 3.4 The full editor (LEDs)

| key | colour |
| --- | --- |
| AUX | orange, slow blink |
| B1, B2 | the F1 / F2 colours (`FUNKONE`, `FUNKTWO`); the held one blinks; both held: both blink in `FUNKTHREE` |
| B3..B10 | each slot in its type's colour (§2); empty `0x080808`; bypassed **red**; the selected slot blinks |
| W1..W16 | only while a slot is held: the 11 type colours (W12..W16 off), the slot's current type blinking |

### 3.5 Pages that use the keys (the arp's patterns)

When the selected FX's page "uses keys" (`usesKeys()`: only the Arp, on pages 6 and 7), the slot keys stop selecting
slots and the arp takes the keybed (`MidiFXArpeggiator::onKeyUpdate`, `updateLEDs`). To get the slot keys back, turn the
encoder to an earlier page.

| keys | mod pattern (page 6) | transpose pattern (page 7) |
| --- | --- | --- |
| **W1..W16 press / hold** | select that step and hold it (the cursor jumps there) | the same |
| W held + **B1..B10** | set the step's mod to 0..9 (As Played .. Note 2; Notes 3..6 only by encoder) | set the step's offset to 0..+9 |
| **B3 held + W** (or the length param selected + W) | **the pattern's length = that step** (`Set Length` label) | the same |
| B1 (F1) held + W | reset the step (to As Played / 0); `Reset: n` | the same |
| B2 (F2) held + W | paste the last value; `Pasted: n` | the same |
| B1 + B2 (F3) held + W | **a random value** (a random mod of the 14 / a random offset 0..11); `Random: n` | the same |
| encoder | walk the 16 steps and the length; EDIT changes the value | the same |

LEDs on these pages: W keys within the length show the step (mod page: As Played orange, Rest dim red `0x100000`, Tie
`0x801000`, Repeat red, the others magenta; transpose page: 0 blue, positive `0x8080FF`, negative dim `0x000020`),
beyond the length off, the cursor blinking; B3 in the F1 colour (blinks while it is the length key); with a W held,
B1..B10 show the step's value (mod: the current one blinks; transpose: a bar up to the value). OLED: the 16 steps as
characters or bars with `LEN: n  SEL: n  MOD: x` / `OFS: n`, or the action label.

### 3.6 Pass-through quick edit (AUX + W10) and arp quick edit (AUX + W12)

- The group is active but **does not take the keys**: the keybed plays the instrument through the FX while the encoder
  edits the selected FX (`passthroughQuickEdit`). No group LEDs are drawn except AUX.
- **AUX + B1 / B2**: the previous / next non-empty slot (pops `n TYPE`).
- AUX + B6..B10: move the quick edit to another group; AUX + B5: leave it. AUX tap: exit.
- AUX + W12 does the same for the arp (pops the arp edit message), creating it if needed.

### 3.7 AUX shortcuts in the playing modes (MI, Chords, FORM ...)

| AUX + | does | LED |
| --- | --- | --- |
| B5 | route off | red (`SALMON` when selected) |
| B6..B10 | route to group 1..5; hold / double-tap: its editor | blue (light cyan for the selected group) |
| W10 | pass-through quick edit | red (blinks while active) |
| **W11** | **bypass the group's selected slot** (toggle; `No FX` if empty) | not lit |
| W12 | arp params (creates the arp) | dark orange |
| W13 | next arp pattern (creates the arp); the pattern's name pops | dark purple |
| W14 | next octave range 1..4 (creates the arp); `OctRange: n` | dark purple, blinking the range count |
| W15 | arp hold on / off (creates the arp) | yellow when hold is on, dark yellow off |
| W16 | arp on / off (creates the arp; turns every arp in the group on / off together, except in the editor or quick edit with an arp selected, where only that one) | mint blinking when on, dark cyan off |

The arp keys act on the selected slot if it is an arp, else on the first arp in the group.

---

## 4. With the sequencer and the keyboard

`form_machine_omni.cpp` (the trig loop, `previewNote`, `selectMidiFx`, `flushNotes`); `omx_mode_form.cpp`
(`seqNoteOn`, `seqNoteOff`, `onNotePostFX`, `onPendingNoteOff`, `onModeActivated`, playback stop, `killAllNotes`);
`omni_structs.h`.

### Routing in FORM

| level | field | values | default |
| --- | --- | --- | --- |
| track | `Track.midiFx` (3 bits) | 0 = off, 1..5 = group 1..5 | **0 (off)** |
| step | `Step.mfxIndex` (3 bits, a step lock) | 0 = OFF, 1 = TRK (follow the track), 2..6 = FX1..FX5 | **1 (TRK)** |

- When a step fires, each of its notes goes to the step's group (or the track's for TRK, or straight out for OFF). The
  group used is stored with the sounding note, so its note-off goes to the same group.
- The track's group: AUX + B5..B10 with the track selected, or the TRACK page's MFX cell. The step's: the MFX edit mode
  (B10) with a step held, palette **B5 = OFF, B6..B10 = FX1..FX5**; TRK comes back with AUX-reset (FORM-SEQUENCER.md
  §2, §3). The step param page walks OFF / TRK / FX1..5.
- Ratchet hits each go through the group as separate notes [inferred: each hit calls `seqNoteOn` with the same index].
- **Tracks share the groups.** Two tracks routed to group 2 feed one chain, so their notes meet in one arp's note list
  (one pattern across both tracks' notes, each note keeping its own channel) [inferred from one instance per group].
- Sequencer notes enter with a known length; Arp and Repeat treat them as held for that length, so a step of length 4
  arpeggiates for 4 steps; the sequencer also sends explicit note-offs when a note is retriggered or the track is
  flushed (mute, stop).
- Stopping playback or KILL calls `resync()` on every group (arps and repeats stop) and flushes pending notes.

### Live keyboard and recording

| mode | the keybed's notes | MFX |
| --- | --- | --- |
| **MI** (MIDI keyboard) | go to the **selected group** (AUX + B5..B10), as held notes (unknown length) | yes: this is where the arp is played live; Repeat only runs in the selected group there |
| Chords, Drum, Euclid | routed per chord / drum key / euclid track | yes |
| **FORM** (preview, MI view, recording) | `previewNote` sends with index **255** | **no: FORM's live keys bypass the MFX** |

- FORM records the keys as played (pre-FX) into steps (`recordNoteToStep`); the MFX output is never recorded
  [read for the routing; that nothing post-FX is recorded is inferred from there being no path back].

---

## 5. What is generative

| source | what it generates | where |
| --- | --- | --- |
| Chance | drops notes | §2.1 |
| every FX's CHC% | applies the FX to some notes only (random harmony, random arps, random repeats) | §1 |
| Randomizer | random pitch (+-12 st and +-12 oct), velocity, length (x1..x15), channel, delay | §2.3 |
| Selector RND | a random FX per note out of up to 7 | §2.4 |
| Harmonizer / Chord | several notes from one | §2.5, §2.6 |
| Repeat | a stream of hits from one note, fading velocity and rate | §2.9 |
| Arp | a stream from a chord: Random / Rand Other / Rand Once patterns, mod pattern (rests, ties, chords, octave jumps), transpose pattern | §2.10 |
| arp pattern editor, F3 + W | fills a mod / transpose step with a random value | §3.5 |
| a new Chance FX | its own value starts random | §2.1 |

Nothing generates notes without input (no Turing-machine or probability-sequence FX): every FX needs an incoming
note.

---

## Notes for FIZZ

FIZZ's panel (PLAN.md §8b to version 2.3): OCT− / OCT+ = F1 / F2 (both = F3), GLO = SHIFT, B1..B10 the hold modes
(B9 FUNC, B10 SOUND), B11 (F#5) P-LOCK / CLEAR, W1..W16 the steps; buttons FX, SEL, ENV (MIX), LFO (step latch), EDIT,
HOME, SAVE, ARP (KEYS / FILL), SEQ, PLAY, REC; knobs SELECT, PRESETS, ALGORITHM, KNOB 1..4; one LED colour in three
levels plus blinking; a 240 x 240 screen under the LINE and clarity rules. P1.6a plans ARP tap = on / off, ARP held =
FILL, SHIFT + ARP = the arp's settings; P1.6c is this editor.

**What to take from the OMX**

1. **The chain model**: a short ordered chain of FX between the sequencer and the voices, the group tracking note-offs
   by the pitch that entered (so FX never manage their own offs). This is the part worth copying exactly.
2. **The type set**, in roughly this order of value for an FM step sequencer: Arp (with its mod and transpose
   patterns), Chance, Repeat (ratchet streams beyond FIZZ's x1..x8), Harmonizer, Transpose, Scaler (FIZZ tracks
   already have scales), Randomizer, Chord (FIZZ steps already hold chords: lower priority), Mono, Selector.
3. **Per-FX chance** as the generative knob: every FX "sometimes".
4. **The arp's two 16-step lanes edited on the white keys** with values on the black keys, which is FIZZ's own
   held-step idiom already (hold W + B value; F1 / F2 / F3 + W = reset / paste / random).

**What to change for the FM-1**

1. **Groups**: the OMX's 5 global, shared groups make two tracks share one arp (surprising) and need a routing
   field per step. Recommend **one chain per track** (4 slots: enough for Arp + three others) plus a per-step
   "GEN on / off" lock, the step's notes otherwise going through the track's chain. Shared groups can come later as a
   chain "source" option if wanted.
2. **Colour**: the OMX tells types apart by LED colour; the FM-1 cannot. The **screen** carries the types (one LINE
   drawing per FX: the arp as a staircase, Repeat as the bouncing ball FIZZ already uses for ratchets, Chance as the dot
   field, Harmonizer as stacked bars ...). LEDs: slot lit = selected, dim = holds an FX, faint = empty, blinking =
   bypassed.
3. **Entry**: **SHIFT + ARP** (already planned for the arp's settings) opens the track's GEN chain, landing on its arp
   if it has one. ARP tap stays the arp's on / off (the OMX's W16), ARP held stays FILL.
4. **Editor gestures** (FIZZ's conventions in place of the OMX's):

   | OMX | FIZZ (proposed) |
   | --- | --- |
   | B3..B10 = slots 1..8 | **B1..B4 = slots 1..4** (B5..B10 free for the selected FX's quick values) |
   | slot held + W1..W11 = type | slot held + **W1..W10 = type** (W1 empty), the screen showing the type picker |
   | slot held + encoder = move | slot held + **SELECT** = move |
   | slot held + B1 / B2 = on / off | slot held + **B11** = bypass toggle (B11 = CLEAR's place: "take it out of play") |
   | F1 / F2 / F3 + slot = copy / paste / cut | **F1 + slot** = copy then paste (FIZZ's F1 rule), **F2 + slot** = pick-up / drop (FIZZ's F2 rule = move) |
   | encoder walks 4-cell pages | **KNOB 1..4 = the FX's four main values** on one drawing; a second page by pressing the slot again (as the sound editor's P1.4a pages) |
   | AUX tap = exit | **OCT− = back** (the latched-menu rule), HOME too |
   | arp page 6 / 7: W = step, B = value, B3 + W = length | the same on W / B; **F3 + W = the lane's length** (FIZZ's page-end gesture), F1 + W reset, F2 + W paste, SHIFT + W random |

5. **Live keys**: the OMX FORM bypasses the MFX for live keys; FIZZ should **send the keyboard (REC's keyboard view)
   through the track's chain**, since the arp is meant to be played (P1.6a), and **record what was played, pre-FX**,
   as the OMX does. A later "print" (render the chain's output into steps) would be FIZZ's own addition.
6. **Voices**: Chord / Harmonizer / Repeat multiply notes against FIZZ's 16 shared voices; the per-track voice limit
   (§2) should apply after the chain, and the chain's output should be counted on the overview's voice dots.
7. **Defaults to fix on the way**: a new arp should start **on** (the OMX's starts OFF), chance should start at 100 %
   (the OMX's Chance FX starts random), the bypass state should be saved, and Chord should keep the input velocity.

**Open questions for the owner**
- One chain per track (recommended) or OMX-style shared groups?
- How many slots (4 recommended) and which types first (Arp, Chance, Repeat, Harmonizer suggested)?
- Should the per-step route be a lock in an existing hold mode (FUNC or TRIG) or a new place, since B1..B11 are taken?
