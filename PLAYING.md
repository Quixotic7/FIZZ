# Playing QUADRILLE (P1.3)

The prototype runs in the FM1VST Standalone on the Mac. Turn on **Remote FM-1** there; the FM-1 is the panel and the
sound comes from the Mac. Printed names in brackets.

## Getting around

- The unit wakes on the dancing screen. Press anything to get to the **step view**, the home screen.
- **HOME** always brings you back to the step view.
- After two minutes untouched with nothing playing, it goes back to the dancers.

## The step view

- The **16 white keys** are the 16 steps of the page; the screen draws them as a little piano roll.
- **Tap** a white key to put a note on that step; tap again to remove it.
- **FUNC (GLO) + tap** makes a lock step: it changes the sound without playing a note.
- The first eight **black keys** pick tracks 1–8. **C#5 / D#5** go to the page before / after.
- **PAGE (ENV)** goes to the next page. **PAGE + white key 1–8** jumps to that page.
- **FUNC + PAGE** turns "follow the playhead" on or off.
- **KNOB 1–4**: the track's filter (FREQ), decay, harmonics (HARM) and feedback (FDBK).
- **PRESETS** picks the track's sound, **ALGORITHM** its algorithm, **SELECT** the tempo, **FUNC + SELECT** the swing.
- Each knob turn fills the screen for a second.

## Editing a step

- **Hold** a white key for a moment: the screen becomes that step.
- The **black keys** are now its settings, left to right: NOTE, VEL, LEN, MICRO, RATCH, COND, CHANCE, TRIG, SOUND,
  LOCKS, and CLEAR on the last one.
- **KNOB 1** (or SELECT) turns the big number; **KNOB 2** turns the green part of the drawing.
- In **NOTE**, the four knobs are the four notes, and SELECT walks through chord shapes. OCT− / OCT+ move the notes an
  octave.
- A knob the screen doesn't use **locks** that sound setting on this step only (a padlock shows).
- **PRESETS** gives the step its own sound (a sound lock).
- **Hold several** white keys to edit them all at once.
- **CLEAR** wipes the held steps; press it again within 4 seconds to undo.
- **OCT+** keeps the held steps selected so both hands are free (on the NOTE screen: FUNC + OCT+). OCT− or HOME lets
  go.
- New notes take the last note, velocity and length you set.

## KEYS (ARP)

- **KEYS** makes the keybed a keyboard for the selected track.
- **Stopped, REC** records step by step. Play a note or chord and it goes into the next step. OCT+ leaves a rest,
  OCT− goes back a step, PRESETS moves the cursor.
- **Playing, REC** records live with your timing. **FUNC + REC** snaps notes to the steps.
- Knob turns while recording are written into the step that is playing.

## Elsewhere

- **TRACKS (FX)** shows the eight clocks.
  - KNOB 1–4 set the selected track's length, speed, voice limit and swing on/off.
  - White keys 1–8 select a track; 9–16 mute tracks 1–8.
- **PATTERN (SEL) + white key** switches to A01–A16. A01..A03 are the demos.
- **REC held** in the step view is FILL.
- A **MIDI keyboard** on the track's channel can play, record, or set the notes of a held step.

## Not built yet (P1.4 and P1.5)

- The sound editor (SOUND), the pattern screen and chains, MUTE, MIX and the effects screens.
- SAVE, copy / paste, and the perform offsets (FUNC + KNOB 1–4).
- Nothing is saved yet: closing the app loses the pattern.
