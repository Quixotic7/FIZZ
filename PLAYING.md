# Playing QUADRILLE (P1.3b)

The prototype runs in the FM1VST Standalone on the Mac. Turn on **Remote FM-1** there; the FM-1 is the panel and the
sound comes from the Mac. Printed names are in brackets.

## Names used here

| key | name here |
| --- | --- |
| OCT− | **F1** |
| OCT+ | **F2** |
| OCT− and OCT+ together | **F3** |
| GLO | **SHIFT** |
| the black keys, left to right (F#3 .. D#5) | **B1 .. B10** |
| the rightmost black key (F#5) | **B11** (clear) |
| the white keys | **W1 .. W16**, the 16 steps of the page you are editing |

## Getting around

- The unit wakes on the dancing screen. Press anything to get to the **step view**.
- **HOME** always brings you back to the step view.
- After two minutes untouched with nothing playing, it goes back to the dancers.
- **PLAY** plays and stops.

## How do I add and remove steps?

- **Tap** a white key: the step gets a note. Tap it again to remove it (and its locks).
- A new step plays the **track's defaults** (its note, velocity, length and so on). Change a default and every step
  that still uses it changes too.

## How do I change one step's note, velocity, length …?

1. Pick what to edit with a black key, **nothing held**: **B1 NOTE, B2 VEL, B3 LEN, B4 MICRO, B5 RATCH, B6 CHNC
   (chance), B7 COND (condition), B8 TRIG, B9 SOUND, B10 LOCKS**. The screen draws the page for that choice; steps that
   have their own value are in colour, steps on the track's default are grey, and the default is a dashed line or a
   grey label.
2. **Hold** the step's white key. The screen shows that step; the strip at the bottom shows what the ten black keys do.
3. Press a black key **B1 .. B10** to set one of ten values, or turn **ALGORITHM** for any value.
4. **B11** puts the step back on the track's default.

Hold several white keys to edit them all at once.

The ten values:

| mode | B1 .. B10 | ALGORITHM |
| --- | --- | --- |
| VEL | 13 25 38 51 64 76 89 102 114 127 | 1..127 |
| LEN | 1/4 1/2 3/4 1 1.5 2 4 8 16 INF steps | every length |
| MICRO | −20 −15 −10 −5 −2 +2 +5 +10 +15 +20 (24ths of a step) | −23..+23; SHIFT + ALGORITHM: the strum |
| RATCH | B1..B8: x1..x8; B9: the ramp (flat, up, down, accent) | x1..x8; SHIFT + ALGORITHM: the ramp |
| CHNC | 10 % .. 100 % | 0..100 % |
| COND | B1 FILL, B2 NOT FILL, B3..B6 "A" = 1..4, B7..B10 "B" = 1..4 (plays on pass A of every B) | every condition |
| TRIG | B1 NOTE, B2 LOCK (changes the sound, no note), B3 TIE (slides a mono note) | the same |
| SOUND | ten sounds of the pool (see below) | any sound |
| LOCKS | pick one of the step's locks | that lock's value; B11 removes it |

## How do I enter notes and chords?

- In **NOTE**, hold the step and press black keys: they are ten notes of the track's scale from its root (a chromatic
  track: ten semitones from C).
- The first key replaces the step's notes. Keys pressed **while another is still down** add to the chord (up to 4).
- Each key plays its note so you can hear it.
- **F1 / F2** (OCT− / OCT+) while holding the step move the ten keys an octave down / up.
- **ALGORITHM** moves every note of the step one scale step.

## How do I change the track's defaults?

- With **nothing held**, pick the mode (B1..B10) and turn **ALGORITHM**: it changes the track's default for that mode
  (velocity in VEL, the notes in NOTE, …). The screen shows the default big while you turn.
- Only the steps that don't have their own value follow it.
- In SOUND, ALGORITHM (or PRESETS) chooses the track's own sound.

## How do I work with pages and loops?

- A track has 8 pages of 16 steps. **SELECT** chooses the page you edit. **PAGE (ENV) held + W1..W8** jumps to a page.
- **PAGE tapped** turns page follow on or off (on: the page you see follows the playhead; PAGE lit).
- Hold **F1** to see the pages:
  - **F1 + tap B1..B8**: edit that page (the loop doesn't change).
  - **F1 + double-tap B1..B8**: loop that page alone.
  - **F1 + hold one page key, press another**: loop all the pages between them.
- Every page has its own start and end. Hold **F3** (both OCT keys):
  - **F3 + W**: the page ends at that step, and the page joins the loop.
  - **F3 + hold W a, press W b**: the page plays from a to b.
- The loop plays its pages in order, each from its start to its end.

## How do I copy, paste and undo?

- **F1 + W**: copy that step. **F2 + W**: paste it (tap more keys to paste again; another track works too).
- **SHIFT + F1 + W**: cut the step.
- **F2 + B9**: copy the page you're editing. **F2 + B10**: paste it onto the page you're editing (it joins the loop).
- **F1 + B9**: clear the page. **F1 + B10**: clear the track.
- **F1 + B11**: undo the last clear, paste or cut on this track. Press it again to redo.

## How do I choose, mute and solo tracks?

- Hold **F2** to see the tracks. **F2 + B1..B8** selects a track.
- **F2 + hold a track key + W1**: mute that track; **+ W2**: solo it. When any track is soloed, only soloed tracks play.
- **TRACKS (FX)** still shows the eight clocks: KNOB 1..4 set the selected track's length, speed, voice limit and swing;
  W1..W8 select a track, W9..W16 mute tracks 1..8.

## How do I change speed, tempo and swing?

- **F3 + B1..B9**: the track's speed: 1/8 1/4 1/2 2/3 3/4 1 4/3 3/2 2.
- **F3 + SELECT**: the tempo. **SHIFT + F3 + SELECT**: the swing.

## How do I play the keyboard and record?

- **REC tapped**: the keybed becomes a keyboard for the selected track (REC lit). REC again: back to the steps.
- In a scale, keys outside the scale play the scale note just below.
- **F1 / F2** move the keyboard an octave.
- **SHIFT + REC** (in the keyboard) arms recording; REC blinks.
  - Stopped, it is **step recording**: each note or chord goes into the next step. F2 leaves a rest, F1 steps back,
    PRESETS moves the cursor.
  - Playing, it is **live recording**: notes go in with your timing and length. Knob turns become locks on the step
    playing.
- **REC + PLAY** starts playing with recording armed, in the keyboard.
- **Hold ARP** for **FILL** (ARP lit): steps with the FILL / NOT FILL condition follow it.
- A **MIDI keyboard** on the track's channel plays, records, or sets the notes of a held step.

## How do I change sounds?

- The project has a **pool** of sounds: INIT, the 16 factory sounds, and the drums KICK, SNARE, CLAP, GHOST, HAT and
  OPEN HAT (23 so far, up to 128).
- **Nothing held: PRESETS** picks the track's own sound from the pool. **KNOB 1..4** turn the track's FREQ, DECAY,
  HARM and FDBK.
- **A step held: PRESETS** gives the held steps a sound from the pool (a sound lock), in any mode. Turn it all the way
  down to go back to the track's sound.
- In **SOUND** mode the ten black keys are ten sounds of the pool (the group of ten that holds the step's sound);
  F1 / F2 show the ten before / after; B11 removes the sound lock.
- **A step held: KNOB 1..4** lock FREQ, DECAY, HARM, FDBK on that step (a padlock shows).

## The scale (for now)

- **SHIFT + ALGORITHM** sets the track's scale (chromatic, major, minor, the modes, harmonic and melodic minor,
  pentatonics, blues, whole tone). **SHIFT + PRESETS** sets its root.
- This is temporary until the track settings page exists.

## Help on screen

- Holding **F1**, **F2** or **F3** shows a map of what that layer does.
- Holding **SHIFT** alone shows what SHIFT does on this screen.

## Not built yet

- The sound editor (SOUND), so the pool only has the sounds it starts with.
- Track settings (scale, root, following the global key, MIDI channel), the pattern screen and chains, MIX and the
  effects screens.
- Saving: closing the app loses everything.
