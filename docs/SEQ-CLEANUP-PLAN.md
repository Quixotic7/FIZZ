# The sequencer clean-up (plan, 2026-10-26)

The owner's problems with the core sequencer after beta.7, discussed and agreed on 2026-10-26; nothing built yet.
Mock-ups: `mockups/fizz-seq-cleanup.html` (`mockups/png/fizz-seq-cleanup-sheet.png`).

## 1. The knobs, nothing held or a step held: one table

- **KNOB 1 is the mode's main value** in every case: NOTE's root with nothing held (today it is only on SELECT
  with B1 held); with a step held, KNOB 1 moves the whole chord (SELECT did). **SELECT is the page always.**
- KNOB 2..4 take the chord's notes 2..4; **note 5 only while the step (or B1) is physically held**, on ALGORITHM.
  In the latch ALGORITHM stays the step cursor (navigating the selection).
- OCT- / OCT+ are the entry octave whenever the knobs are on note values, held or not, latched or not.
- B1 held + SELECT keeps working as a second way to the root.
- Held, latched-with-a-selection and latched-empty (VALUES) share one knob table, the held one, so "latch, VALUES,
  nothing selected" is exactly "a key held with the latch off".

## 2. The latch (LFO)

- Starts with an **empty selection** (today it selects and locks step 1).
- MODES is the default (done 2026-10-25) and is drawn **first, on the left**; SELECT turns toward it.

## 3. The black keys

- **B11 SOUND** (the step's sound from the pool; was B10). P-LOCK goes: the sound editor's hold-a-step lock covers
  FREQ, DECAY, HARM and FDBK and stays exactly as it is.
- **B10 SLIDE**: a step's portamento. The key cycles PORT OFF / ON / LEGATO, KNOB 2 TIME, KNOB 3 SLOPE (TIME / RATE);
  the picture: the step's note gliding into the next step's pitch along a ramp (mock-up 6, 7). The 303 slide in one key.
- **MIDI FX stays in TRIG** (KNOB 2); the TRIG hold screen draws the group under the trig picture: the hexagon in
  KNOB 2's green with the group's number and slots, or TRACK / OFF in grey (mock-up 4, 5).

## 4. FX and MIX with steps

- The FX layer's first page becomes **SENDS**: the current track (the badge; SHIFT + SELECT changes it), its three
  sends as bars with their values (KNOB 1..3), the steps on the lane and on the white keys as the EDIT home, a held
  step's send a lock (the chip, the bar framed), REC switching the white keys between the steps and the keyboard as the
  sound editor does (mock-up 1..3). The OVERVIEW's W1..W8 track picking and its every-track bars go.
- Every other FX page shows the steps on the white keys' LEDs (send locks are never blind).
- The mixer keeps its faders; the steps show on the LEDs only.

## Open

- Nothing: the owner approved the plan's points on 2026-10-26; the mock-up's screens are the ones to build.
