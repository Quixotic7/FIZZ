# Text audit (P1.3v, PLAN §9a "The clarity rules")

Every text string the screens drew before P1.3v (FMToneSeq b1da3fe, `core/ui/*.c`; the strings handed in by
`core/app/view.c` and `ctl*.c` are listed with the call that draws them), with a verdict and what replaced it.
Sizes are the canvas font size in px and the Inter Tight weight. **keep**: stays as it was (a big value, a name);
**enlarge**: made 12 px or more, weight 500 or more; **shape**: replaced by a drawing; **remove**: gone (tutorial
text, or said by the drawing already).

The rule now (§9a): no text under 12 px (the tests fail under 11: `qd_text_stats_min`, checked over every frame
test_ctl, test_app and render_frames draw); a short label is 12 px 600 (`ui_lab`); the big values stay thin.

## Exceptions (text kept where a shape would not do)

- **COND lane**: a named condition (FILL, !FIL, PRE, 1ST ...) as its letters stacked in the step's column, 12 px.
  A:B conditions are circles only.
- **SOUND lane**: a step's pool number as digits stacked in its column, 12 px.
- **TRACKS**: a track's length and rate ("16 · 1/16") under its clock, 12 px, squeezed to the clock's width.
- **SEL map and TRACKS**: track names squeezed to their cell (never narrower than 70 %).
- **LATCH** in the top line beside the mode's name (a mode, not a hint).
- **DEFAULT 100** at the PIANO ROLL's foot: the track's default value the lane is drawn around.
- **TEMPO takeover**: "BPM" and "SWING" as units of the big values.
- **The keyboard**: the octave and scale line; step recording's step number.

## Strings from view.c / ctl

| where | strings | verdict | now |
| --- | --- | --- | --- |
| confirmations (`qd_ctl_confirm`) | big: PAGE CLEARED, TRACK CLEARED, LOCKS CLEARED, UNDONE, NO UNDO, NO ROOM, ONE PAGE, FOLLOW, NO FOLLOW, LIVE REC, STEP REC, REC OFF, EXT CLOCK | keep | 40 px |
| confirmations | small: "F1 + B11 undoes it", "again to redo", "for locks: the pattern has 1024", "the loop is never empty", "the page follows the playhead", "the page stays where you put it", "notes go in as played", "the keyboard plays", "the tempo follows MIDI clock" | remove | not drawn (`ui_confirm_draw` ignores them) |
| SHIFT map (`fill_shift`) | "+ ALGORITHM → the track's scale", "+ PRESETS → the track's root", "+ F3 + ALGORITHM → swing", "+ SEQ + W → change pattern now", "+ REC → step / live recording", "+ SELECT → swing", "+ ALGORITHM → the ratchet's ramp / the strum", context STEP VIEW / A STEP HELD / KEYBOARD | shape | controls drawn, icons, values (D DORIAN, D, 56%, STEP / LIVE / OFF) |
| octave pop-up sub (`draw_octave`) | "B1 = C4 · D DORIAN" | enlarge | "C4 · D DORIAN" 12 px |
| knob band (`fill_band`) | STEP 5 · VEL, TRACK DEFAULT; VELOCITY, NOTE 3, STRUM ...; the value | enlarge | 12 px; the value 26 px |
| turn takeovers (`fill_turn`) | FREQ DECAY HARM FDBK ALGO TEMPO SWING LEN RATE LIMIT PAGE SCALE ROOT TRACK; ctx STEP n | enlarge | 12 px 600 |
| PIANO ROLL (`fill_lane`) | DEFAULT 100, DEFAULT C4, DEFAULT ALWAYS, TRACK SOUND | enlarge | 12 px 600 |
| EDIT / held (`fill_held`) | the chord name, the note names, the scale name, the condition names, the sound names, the length names | keep | as above |

## Every call, by screen

| screen | file:line (before) | function | string | before | verdict | now |
| --- | --- | --- | --- | --- | --- | --- |
| every main screen | ui_kit.c:76 | ui_box | the track's box number (10 px) | 10 500 | **enlarge** | 12 px 600 in a 16 px box |
| every main screen | ui_kit.c:87 | ui_head | the top line's left (11 px 500) | 11 500 | **enlarge** | 12 px 600 |
| every main screen | ui_kit.c:89 | ui_head | the top line's right (11 px 400) | 11 400 | **enlarge** | 12 px 500 |
| PIANO ROLL | ui_lane.c:659 | def_name | DEFAULT 100 at the foot (8 px) | 8 px 600 | **enlarge** | 12 px 600 (the default is the value the lane draws around) |
| PIANO ROLL / STEPS | ui_lane.c:677 | ui_lane_draw | the mode's name after the track (8 px) | 8 px 600 | **enlarge** | 12 px 600 in the mode's colour |
| PIANO ROLL / STEPS | ui_lane.c:679 | ui_lane_draw | the tempo in the top line (11 px 400) | 11 400 | **remove** | not the subject here: the tempo takeover and F3 show it |
| PIANO ROLL / STEPS | ui_lane.c:716 | ui_lane_draw | LATCH (8 px) | 8 px 600 | **enlarge** | 12 px 600 (a mode, beside the mode's name) |
| PIANO ROLL / STEPS | ui_lane.c:722 | ui_lane_draw | FOLLOW OFF (8 px) | 8 px 600 | **shape** | a crossed arrow left of the pattern |
| PIANO ROLL / STEPS | ui_lane.c:724 | ui_lane_draw | NO FOLLOW (8 px) | 8 px 600 | **shape** | (the same icon) |
| PIANO ROLL · COND | ui_lane.c:350 | l_cond | A:B over its circles (8 px) | 8 600 | **remove** | the circles (b of them, the a-th filled) say it |
| PIANO ROLL · COND | ui_lane.c:357 | l_cond | a named condition's letters stacked (10 px) | 10 600 | **enlarge** | 12 px 600, 13 px apart (exception: the value as text in a column) |
| PIANO ROLL · P-LOCK | ui_lane.c:165 | l_note | the lock count under a padlock (7 px) | 7 600 | **shape** | a dot a lock (up to 4) |
| PIANO ROLL · SOUND | ui_lane.c:413 | l_sound | the pool number under a dot (8 px squeezed) | 8 600 | **enlarge** | its digits stacked down the column, 12 px 600 (exception: the value) |
| STEPS / EDIT | ui_lane.c:530 | steps_row | the page numbers (folded 8 px) | g > 0.5 ? 12 : 8 ed ? 700 : 500 | **enlarge** | 12 px 600 / 700 edited |
| EDIT | ui_held.c:727 | head_value | the value in the head, right | size 200 | **keep** | 24 px thin, shrinking to 12 at least (else left to the drawing) |
| EDIT | ui_held.c:829 | ui_edit_draw | the sub-label: the scale; "LENGTH IN STEPS", "/24 OF A STEP", "TO THE PAGE'S END" | 8 px 600 | **enlarge / remove** | the scale only, 12 px; the others removed |
| EDIT · P-LOCK | ui_held.c:737 | ed_plock | "THE KNOBS LOCK" | 15 300 | **remove** | tutorial; a padlock |
| EDIT · P-LOCK | ui_held.c:738 | ed_plock | "THE STEPS HELD" | 15 300 | **remove** | tutorial |
| EDIT · P-LOCK | ui_held.c:742 | ed_plock | "THE KNOBS LOCK" / "NO P-LOCKS YET" | 15 300 | **remove** | the padlock (coloured with locks, grey without); the count in the head |
| EDIT · P-LOCK | ui_held.c:743 | ed_plock | "B11 CLEARS THEM" / "TURN A KNOB" | 15 300 | **remove** | tutorial |
| EDIT · VEL | ui_held.c:774 | ui_edit_draw | the velocity big beside the mallet | 60 200 | **keep** | 60 px |
| held / EDIT | ui_held.c:104 | big | EMPTY STEP (in place of the value) | 11 600 | **enlarge** | 14 px 600 |
| held / EDIT | ui_held.c:106 | big | the value big (VEL, LEN, MICRO, RATCH) | size 200 | **keep** | big, thin |
| held / EDIT | ui_held.c:635 | held_head | the head: STEP 5 · VEL, TRACK DEFAULT · NOTE (11 px) | 11 500 | **enlarge** | 12 px 600 |
| held / EDIT | ui_held.c:643 | held_head | DEFAULT / EMPTY STEP tag (7 px in a box) | 7 600 | **enlarge** | a 12 px word, DEFAULT / EMPTY, no box (none in P-LOCK) |
| held / EDIT · CHNC | ui_held.c:487 | f_cond | the chance big | 60 200 | **keep** | 60 px |
| held / EDIT · COND | ui_held.c:453 | cond_draw | "plays every pass", "plays while FILL is held" ... | 11 400 | **remove** | tutorial; the passes' circles stay |
| held / EDIT · COND | ui_held.c:485 | f_cond | the chance under the dots (60%) | 34 200 | **keep** | 34 px |
| held / EDIT · COND / CHNC | ui_held.c:443 | cond_draw | the condition big (3:4, FILL, ALWAYS) | sz 200 | **keep** | big |
| held / EDIT · LEN | ui_held.c:284 | f_len | the ruler's step numbers (8 px, ten of them) | 8 i == 2 ? 600 : 500 | **shape** | only the held step's number, 12 px; the ticks carry the rest |
| held / EDIT · MICRO | ui_held.c:349 | f_micro | the step before's number (8 px) | base + 14 8 | **remove** | the tape's big ticks |
| held / EDIT · MICRO | ui_held.c:351 | f_micro | the step's number (8 px) | base + 14 8 | **enlarge** | 12 px 600 |
| held / EDIT · MICRO | ui_held.c:353 | f_micro | the step after's number (8 px) | base + 14 8 | **remove** | the tape's big ticks |
| held / EDIT · NOTE | ui_held.c:153 | note_line | the note line (C4 E4 G4) over the keybed | size 500 | **keep** | 15 px 500, never under 12 |
| held / EDIT · NOTE | ui_held.c:168 | chord_name | the chord root big | size 200 | **keep** | big |
| held / EDIT · NOTE | ui_held.c:170 | chord_name | the chord quality (superscript) | qs 300 | **keep** | 20..30 px |
| held / EDIT · SOUND | ui_held.c:500 | cassette | the cassette's pool number / TRACK (8 px) | 8 600 | **enlarge** | 12 px 600 |
| held / EDIT · SOUND | ui_held.c:502 | cassette | the sound's name in the cassette | 13 500 | **keep** | 13 px 500 |
| held / EDIT · SOUND | ui_held.c:528 | f_sound | the sound before (003  BELL, 10 px) | 10 500 | **enlarge** | 12 px 500 |
| held / EDIT · SOUND | ui_held.c:537 | f_sound | the sound after (10 px) | 10 500 | **enlarge** | 12 px 500 |
| held / EDIT · TRIG | ui_held.c:590 | f_trig | NOTE / LOCK / TIE under the icons (10 px) | 10 600 | **enlarge** | 12 px 600 |
| held / EDIT · TRIG | ui_held.c:592 | f_trig | "strikes its notes", "slides the held note, legato" ... | 11 400 | **remove** | tutorial |
| held / EDIT · VEL | ui_held.c:262 | f_vel | RAMP (under the ramp ticks) | 8 px 600 | **remove** | the ticks in RATCH's colour say it |
| held · LEN | ui_held.c:277 | f_len | STEPS (after the value) | 8 px 600 | **remove** | the ruler says it |
| held · MICRO | ui_held.c:343 | f_micro | /24 (after the value) | 18 300 | **remove** | the tape's ticks are the 24ths |
| held · NOTE | ui_held.c:181 | f_note | EMPTY STEP | 11 600 | **enlarge** | 14 px 600 |
| held · NOTE | ui_held.c:210 | f_note | the scale name (CHROMATIC, D DORIAN) | 8 px 600 | **enlarge** | 12 px 600 (ui_lab) |
| held · SOUND | ui_held.c:541 | f_sound | EMPTY STEP (8 px) | 8 600 | **enlarge** | 12 px 600 |
| held · SOUND | ui_held.c:554 | f_sound | "+2 LOCKS ON TOP" | 8 px 600 | **shape** | a padlock a lock (up to 4) |
| held · SOUND | ui_held.c:560 | f_sound | "BANK 11–20" | 8 px 600 | **remove** | the bank pop-up (OCT) and the OCT LEDs show it |
| defaults (mode key held) | ui_held.c:652 | reset_word | B11 RESET (8 px) | 8 px 600 | **shape** | a reset arrow icon, lit when the default differs |
| takeovers | ui_turn.c:603 | ui_turn_draw | the knob's job (FREQ, 11 px) | 11 600 | **enlarge** | 12 px 600 |
| takeovers | ui_turn.c:607 | ui_turn_draw | the context at the right (STEP 5; was the track's name) | 11 400 | **enlarge** | 12 px 500; the track's name gone since P1.3r |
| takeovers | ui_turn.c:638 | ui_turn_draw | the value big | 58 200 | **keep** | 58 px |
| takeovers (a lock) | ui_turn.c:640 | ui_turn_draw | the track's own value under a lock (track 0.0, 8 px) | 8 500 | **enlarge** | 12 px 500, bottom right |
| knob band | ui_turn.c:564 | ui_band_draw | the knob's badge letter (10 px) | 10 700 | **enlarge** | 12 px 700 in a 10 px circle |
| knob band | ui_turn.c:576 | ui_band_draw | STEP 5 · VEL / TRACK DEFAULT (8 px) | 8 px 600 | **enlarge** | 12 px 600 |
| knob band | ui_turn.c:577 | ui_band_draw | the job's name (VELOCITY, 11 px) | 11 600 | **enlarge** | 12 px 600 |
| knob band | ui_turn.c:589 | ui_band_draw | the value (26 px, could shrink to 9) | size 300 | **keep** | 26 px, never under 12 |
| TEMPO / SWING takeover | ui_turn.c:378 | d_tempo | the swing | sw ? 40 : 32 200 | **keep** | 32..40 px |
| TEMPO / SWING takeover | ui_turn.c:379 | d_tempo | SWING | 8 px 600 | **enlarge** | 12 px 600 (the value's name) |
| TEMPO takeover | ui_turn.c:372 | d_tempo | the tempo big | 66 200 | **keep** | 66 px |
| TEMPO takeover | ui_turn.c:373 | d_tempo | BPM | 8 px 600 | **enlarge** | 12 px 600 (a unit) |
| RATE takeover | ui_turn.c:409 | d_rate | PATTERN / TRACK n under the clocks (8 px) | 8 600 | **enlarge** | 12 px 600 |
| RATE takeover | ui_turn.c:412 | d_rate | the rate before (16 px 300) | 16 300 | **enlarge** | 16 px 500 |
| RATE takeover | ui_turn.c:414 | d_rate | the rate after (16 px 300) | 16 300 | **enlarge** | 16 px 500 |
| RATE takeover | ui_turn.c:415 | d_rate | the rate big | 52 200 | **keep** | 52 px |
| LEN takeover | ui_turn.c:434 | d_len | each page's first step (1 17 33 ..., 8 px) | 8 500 | **enlarge** | the page's number 1..8, 12 px 600 |
| LEN takeover | ui_turn.c:436 | d_len | PAGES | 8 px 600 | **remove** |  |
| LIMIT takeover | ui_turn.c:458 | d_limit | n SOUNDING | 8 px 600 | **remove** | the filled dots are the voices sounding |
| PAGE takeover | ui_turn.c:474 | d_page | the page numbers (10 px) | 10 ed ? 600 : 500 | **enlarge** | 12 px 600 |
| SCALE / ROOT takeover | ui_turn.c:500 | d_scale | the scale before (12 px 400) | 12 400 | **enlarge** | 12 px 500 |
| SCALE / ROOT takeover | ui_turn.c:503 | d_scale | the scale big | w > 216 ? 40 * 216 / w : 40 200 | **keep** | 40 px |
| SCALE / ROOT takeover | ui_turn.c:506 | d_scale | the scale after (12 px 400) | 12 400 | **enlarge** | 12 px 500 |
| SCALE / ROOT takeover | ui_turn.c:523 | d_scale | the root under the keybed (9 px) | 9 600 | **enlarge** | 12 px 600 |
| ALGO takeover | ui_turn.c:328 | d_algo | the operators' names C A B1 B2 (9 px) | 9 600 | **enlarge** | 12 px 600 |
| TRACK takeover (P-LOCK) | ui_turn.c:539 | d_track | the track numbers (10 px) | 10 sel ? 600 : 500 | **enlarge** | 12 px 600 |
| TRACK takeover (P-LOCK) | ui_turn.c:540 | d_track | the track names (10 px) | 10 sel ? 600 : 400 | **enlarge** | 12 px 600 |
| TRACK takeover (P-LOCK) | ui_turn.c:542 | d_track | M (muted, 8 px) | 8 700 | **shape** | an orange cross |
| TRACK takeover (P-LOCK) | ui_turn.c:551 | d_track | "ALGORITHM: THE TRACK · P-LOCK" | 8 px 600 | **remove** | tutorial |
| PRESETS picker | ui_picker.c:122 | ui_picker_draw | the bank's range 11–20 under the pool line (8 px) | 8 px 600 | **enlarge** | 12 px 600 |
| PRESETS picker | ui_picker.c:125 | ui_picker_draw | FM TONE (8 px) | 8 px 600 | **remove** |  |
| PRESETS picker | ui_picker.c:36 | neighbour | the neighbours (004  PLUCK, 10 px) | 10 500 | **enlarge** | 12 px 500 |
| PRESETS picker | ui_picker.c:66 | ui_picker_draw | SOUND / STEP n · SOUND (11 px) | 11 500 | **enlarge** | 12 px 600 |
| PRESETS picker | ui_picker.c:81 | ui_picker_draw | TRACK SOUND (8 px) | 8 600 | **enlarge** | TRACK, 12 px 600 |
| PRESETS picker | ui_picker.c:83 | ui_picker_draw | NO LOCK (8 px) | 8 600 | **remove** | the grey cassette says it |
| PRESETS picker | ui_picker.c:86 | ui_picker_draw | the pool number (9 px) | 9 600 | **enlarge** | 12 px 600 |
| PRESETS picker | ui_picker.c:90 | ui_picker_draw | the sound's name | tw > 164 ? 22 * 164 / tw : 22 300 | **keep** | 22 px 400, never under 12 |
| octave / bank pop-up | ui_octave.c:116 | ui_octave_draw | ENTRY OCTAVE / KEYBOARD OCTAVE / SOUND BANK (11 px) |  | **enlarge** | 12 px 600 |
| octave / bank pop-up | ui_octave.c:125 | ui_octave_draw | the number rolling out | 92 200 | **keep** | 92 px |
| octave / bank pop-up | ui_octave.c:129 | ui_octave_draw | the number rolling in | 92 200 | **keep** | 92 px |
| octave / bank pop-up | ui_octave.c:131 | ui_octave_draw | "B1 = C4 · D DORIAN", "F3–G5 · ...", "SOUNDS 11–20" (8 px) | 8 px 600 | **enlarge** | 12 px; "B1 =" removed (the note and the scale) |
| octave / bank pop-up | ui_octave.c:150 | ui_octave_draw | OCT− (8 px) | 8 px 600 | **remove** | the dots on that side, as the LEDs |
| octave / bank pop-up | ui_octave.c:151 | ui_octave_draw | OCT+ (8 px) | 8 px 600 | **remove** |  |
| octave / bank pop-up | ui_octave.c:157 | ui_octave_draw | SLOW BLINK / FASTER / FASTEST / THE CENTRE: NO BLINK | 8 px 600 | **remove** | tutorial; the dots are the LEDs' distance |
| bank pop-up | ui_octave.c:78 | pool_strip | the pool strip's bank numbers (7 px) | 7 600 | **remove** | the sub line names the bank's sounds |
| layer maps | ui_kit.c:126 | ui_head_track | the top line's left (11 px) | 11 500 | **enlarge** | 12 px 600 |
| layer maps | ui_kit.c:127 | ui_head_track | the track's name at the right (11 px) | 11 500 | **enlarge** | 12 px 500 |
| layer maps | ui_kit.c:139 | key_label | key_label: a key's legend (7 px, squeezed) |  | **shape** | gone with the strip: ui_keymap draws icons or numbers |
| layer maps | ui_kit.c:143 | key_label | (key_label) | 7 weight | **shape** |  |
| layer maps | ui_kit.c:149 | key_label | (key_label, squeezed) | 7 weight | **shape** |  |
| layer maps | ui_kit.c:165 | ui_strip | ui_strip: the eleven keys with words in them (CLR, COPY, UNDO, 1/16, NRM, DOT ...) |  | **shape** | ui_keymap: the keys where they are on the panel, icons (clear, copy, undo, notes, dot, triplet) and numbers 12 px |
| layer maps | ui_layer.c:57 | legend | legend(): the key of a legend line (8 px) | 8 px 600 | **remove** | every legend gone; the key map's icons |
| layer maps | ui_layer.c:58 | legend | legend(): what the key does (8 px) | 8 px 600 | **remove** |  |
| F1 | ui_layer.c:86 | f1 | a page's number (18 px) | 18 ed ? 400 : 300 | **keep** | 20 px |
| F1 | ui_layer.c:89 | f1 | a page's range 5–12 (7 px) | 7 600 | **shape** | the dots: outside the range only a speck |
| F2 | ui_layer.c:131 | hand_card | HAND EMPTY (9 px) | 9 600 | **shape** | a dashed empty card |
| F2 | ui_layer.c:135 | hand_card | IN THE HAND (8 px) | 8 px 600 | **remove** | the card is the hand |
| F2 | ui_layer.c:137 | hand_card | EMPTY STEP in the card (11 px) | 11 600 | **shape** | an empty cell in the card |
| F2 | ui_layer.c:140 | hand_card | LOCK STEP (8 px) | 8 px 600 | **shape** | the padlock alone, bigger |
| F2 | ui_layer.c:171 | f2 | CUT / PASTE under the last W (8 px) | 8 700 | **shape** | scissors (orange) / paste (green) icon |
| F3 | ui_layer.c:197 | f3 | PAGE n (8 px) | 8 px 600 | **enlarge** | 12 px 600 |
| F3 | ui_layer.c:198 | f3 | "W: THE END · W + W: START–END" | 8 px 600 | **remove** | tutorial |
| F3 | ui_layer.c:211 | f3 | the range's start under its cell (8 px) | 8 600 | **enlarge** | 12 px 600 in KNOB 3's green |
| F3 | ui_layer.c:213 | f3 | the range's end under its cell (8 px) | 8 600 | **enlarge** | 12 px 600 in KNOB 4's orange |
| F3 | ui_layer.c:217 | f3 | the range big | 34 200 | **keep** | 36 px |
| F3 | ui_layer.c:218 | f3 | STEPS (8 px) | 8 px 600 | **remove** |  |
| F3 | ui_layer.c:222 | f3 | the rate big (1/16) | 34 200 | **keep** | 36 px |
| F3 | ui_layer.c:226 | f3 | T (triplet mark) | 14 500 | **keep** | 16 px |
| F3 | ui_layer.c:227 | f3 | DOTTED / TRIPLET / RATE (8 px) | 8 px 600 | **remove** | the dot / T mark says it |
| F3 | ui_layer.c:232 | f3 | VALUE TYPE START END by the knob marks (8 px) | 8 px 600 | **remove** | the colours (blue rate, green type, green / orange bracket) say it |
| F3 | ui_layer.c:245 | f3 | TEMPO 120 · SWING 56% (11 px) | 11 500 | **enlarge** | "120 BPM · 56%" 14 px 500, lit after a turn |
| F3 | ui_layer.c:247 | f3 | "SELECT: PAGE · ALGO: TEMPO · SHIFT: SWING" | 8 px 600 | **remove** | tutorial |
| F3 latched | ui_layer.c:320 | ui_layer_draw | OCT− EXIT badge (7 px) | 7 600 | **shape** | a back arrow in a box |
| SEL | ui_layer.c:290 | tracks | a track's number (9 px) | 9 600 | **enlarge** | 12 px 600 |
| SEL | ui_layer.c:291 | tracks | a track's name (8 px) | 8 500 | **enlarge** | 12 px 600, squeezed to the cell (no narrower than 0.7) |
| SEL | ui_layer.c:294 | tracks | S in the solo ring (8 px) | 8 700 | **enlarge** | 12 px 700 in a bigger ring |
| SEQ | ui_layer.c:351 | ui_patterns_draw | the bank letter big | 40 200 | **keep** | 40 px |
| SEQ | ui_layer.c:352 | ui_patterns_draw | the pattern number big | 64 200 | **keep** | 64 px |
| SEQ | ui_layer.c:354 | ui_patterns_draw | QUEUED / NEXT AT PLAY (8 px) | 8 px 600 | **shape** | the orange number and the line emptying to the change; PLAY / STOP icon |
| SEQ | ui_layer.c:359 | ui_patterns_draw | PLAYING / STOPPED (8 px) | 8 px 600 | **shape** | a PLAY triangle / STOP square |
| SEQ | ui_layer.c:379 | ui_patterns_draw | the banks A B C D (11 px) | 11 on ? 600 : 400 | **enlarge** | 13 px |
| SEQ | ui_layer.c:383 | ui_patterns_draw | LAUNCH (8 px) | 8 px 600 | **remove** | the knob mark beside the four words |
| SEQ | ui_layer.c:387 | ui_patterns_draw | END BAR BEAT NOW (13 px 300) | 13 i == v->launch ? 500 : 300 | **enlarge** | 14 px 500, the one in use white and underlined |
| SHIFT | ui_layer.c:414 | ui_shift_draw | the key (+ ALGORITHM, 11 px) | 11 600 | **shape** | the control drawn: a knob's badge, REC, F3's two keys, SEQ and a W |
| SHIFT | ui_layer.c:415 | ui_shift_draw | the arrow (11 px text) | 11 400 | **shape** | a drawn arrow |
| SHIFT | ui_layer.c:416 | ui_shift_draw | what it does ("the track's scale", 11 px 400) | 11 400 | **shape** | the job's icon and its value now (D DORIAN, D, 56%), 12..20 px |
| SHIFT | ui_layer.c:419 | ui_shift_draw | "nothing here" | 11 400 | **remove** |  |
| TRACKS | ui_tracks.c:20 | ui_tracks_draw | the pattern after the title (11 px 400) | 11 400 | **enlarge** | 12 px 500 |
| TRACKS | ui_tracks.c:59 | ui_tracks_draw | the track's number in its clock (8 px; 12 muted) | tr->mute ? 12 : 8 500 | **enlarge** | 12 px 600 |
| TRACKS | ui_tracks.c:61 | ui_tracks_draw | the track's name (8 px) | 8 600 | **enlarge** | 12 px 600, squeezed to the clock (no narrower than 0.7) |
| TRACKS | ui_tracks.c:72 | ui_tracks_draw | length · rate (16 · 1/16, 8 px) | 8 500 | **enlarge** | 12 px 500, squeezed to the clock (exception: two values) |
| keyboard | ui_keys.c:106 | ui_keys_draw | SOUND (over the name) | 8 px 600 | **remove** | the name is the sound |
| keyboard | ui_keys.c:112 | ui_keys_draw | the pool number (9 px) | 9 600 | **enlarge** | 12 px 600 |
| keyboard | ui_keys.c:113 | ui_keys_draw | the sound's name | 26 300 | **keep** | 26 px |
| keyboard | ui_keys.c:133 | ui_keys_draw | octave · scale (8 px) | 8 px 600 | **enlarge** | 12 px 600 |
| keyboard | ui_keys.c:135 | ui_keys_draw | the octave (8 px) | 8 px 600 | **enlarge** | 12 px 600 |
| keyboard · live REC | ui_keys.c:42 | live | RECORDING in the red badge (9 px 700) | 9 700 | **enlarge** | REC, 12 px 700 |
| keyboard · live REC | ui_keys.c:44 | live | Q (quantize, 9 px) | 9 700 | **enlarge** | 12 px 700 |
| keyboard · live REC | ui_keys.c:85 | live | octave · scale · STEP n (8 px) | 8 px 600 | **enlarge / remove** | octave · scale, 12 px; the step count removed |
| keyboard · step REC | ui_keys.c:145 | ui_keys_draw | STEP REC (8 px, red) | 8 px 600 | **shape** | a red record dot |
| keyboard · step REC | ui_keys.c:147 | ui_keys_draw | STEP n (8 px) | 8 px 600 | **enlarge** | the number alone, 12 px 600 |
| confirmations | ui_turn.c:648 | ui_confirm_draw | the word (CLEARED, UNDONE, EXT CLOCK ..., 34 px) | w > 224 ? 34 * 224 / w : 34 200 | **keep** | 40 px |
| confirmations | ui_turn.c:651 | ui_confirm_draw | the sentence under it ("F1 + B11 undoes it", "the tempo follows MIDI clock" ...) | 11 400 | **remove** | short and big only |
| logo | ui_idle.c:152 | draw_word_solid | FIZZ | WD_SIZE WD_WEIGHT | **keep** | 56 px |
| (kit) | ui_kit.c:30 | ui_T | ui_T: every text | float size int weight | **—** | qd_text records the smallest size drawn (the tests' guard) |
| (kit) | ui_kit.c:38 | ui_T | qd_text |  | **—** |  |
| (kit) | ui_kit.c:54 | ui_lab | ui_lab: every label (8 px 600) | 8 600 | **enlarge** | 12 px 600 |

(158 calls.)

## Addendum: the screens built after the pass (P1.3w, P1.3x)

P1.3v landed before P1.3w and P1.3x; their screens were built under the rules and run under the same guard
(test_ctl's clarity gesture visits FUNC in all three views; render_frames checks every render): the smallest text
drawn is still 12 px.

| screen | string | size / weight | verdict |
| --- | --- | --- | --- |
| held / EDIT · FUNC | the function's name (RESTART, REVERSE, FORWARD, FLIP, RANDOM, RND FUNC, JUMP 20, NONE) | 34 px 200 (EDIT: the head's 24 px) | keep (the value) |
| held / EDIT · FUNC | a jump's target page number beside its strip | 12 px 600 | keep (the subject's page) |
| PIANO ROLL · FUNC | a JUMP's target number, digits stacked in the column | 12 px 600 | exception (as the SOUND lane's) |
| held / EDIT · FUNC | the function as an icon (arrow, circular arrow, die, jump arc) | — | shape |
| STEPS | a step with a function: a blue dot | — | shape |
| knob band | FUNCTION, the function's name | 12 px 600 / 26 px | keep |
| confirmation | LIVE REC (REC held) | 40 px | keep |
| F3 (P1.3x) | the metronome takeover over the map (TEMPO / SWING, the values) | as the takeovers | keep |
