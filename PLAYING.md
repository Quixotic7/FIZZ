# Playing QUADRILLE (P1.3c)

The prototype runs in the FM1VST Standalone on the Mac. Turn on **Remote FM-1** there; the FM-1 is the panel and the
sound comes from the Mac. Printed names are in brackets.

## Names used here

| key | name here |
| --- | --- |
| OCT− | **F1** |
| OCT+ | **F2** |
| OCT− and OCT+ together | **F3** |
| GLO | **SHIFT** |
| SEL (printed) | **SEL**: hold it for the tracks |
| SEQ (printed) | **SEQ**: hold it for the patterns |
| the black keys, left to right (F#3 .. D#5) | **B1 .. B10** |
| the rightmost black key (F#5) | **B11** |
| the white keys | **W1 .. W16**, the 16 steps of the page you are editing |

"Held" means down for 400 ms (a white step key: 250 ms). Anything shorter is a tap.

## Getting around

- The unit wakes on the dancing screen. Press anything to get to the **step view**.
- **HOME** always brings you back to the step view.
- After two minutes untouched with nothing playing, it goes back to the dancers.
- **PLAY** plays and stops.

## How do I add and remove steps?

- **Tap** a white key: the step gets a note. Tap it again to remove it (and its locks).
- A new step plays the **track's defaults** (its notes, velocity, length and so on). Change a default and every step
  that still uses it changes too.

## How do I change one step's note, velocity, length …?

1. Pick what to edit with a black key, **nothing held**: **B1 NOTE, B2 VEL, B3 LEN, B4 MICRO, B5 RATCH, B6 CHNC
   (chance), B7 COND (condition), B8 TRIG, B9 SOUND, B10 LOCKS, B11 P-LOCK**. The screen draws the page for that
   choice; steps that have their own value are in colour, steps on the track's default are grey, and the default is a
   dashed line or a grey label.
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

A step has up to **five notes**.

- In **NOTE**, hold the step. **The top five knobs are its notes**, each drawn in its knob's colour:
  - **SELECT = note 1** (yellow): one click is one note of the track's scale (a chromatic track: a semitone). The
    other notes move with it.
  - **KNOB 1..4 = notes 2..5** (blue, green, white, orange), each counted up from note 1: one click is the next note of
    the scale up or down (notes another knob already has are skipped). Turn one all the way down to remove that note.
  - Each note plays as you turn it.
  - In NOTE, SELECT doesn't change pages and KNOB 1..4 don't lock the sound: use P-LOCK for that.
- Or press black keys: they are ten notes of the track's scale from its root (a chromatic track: ten semitones from
  C). The first key replaces the step's notes; keys pressed **while another is still down** add to the chord (up to
  5). Each key plays its note.
- **F1 / F2** (OCT− / OCT+) while holding the step move the ten keys an octave down / up. The screen shows the octave
  big for a second, and the OCT keys' lights tell you where you are (see below).
- **ALGORITHM** moves every note of the step one scale step.

## How do I lock a sound setting on a step? (P-LOCK)

- Press **B11** with nothing held: the **P-LOCK** mode. The page shows the notes, with a padlock and the number of
  locks under each step.
- **Hold a step**: the screen stays as it is. Turn a knob of the screen and it is **locked on the held steps**: KNOB
  1..4 = FREQ, DECAY, HARM, FDBK; PRESETS = the step's sound. The knob's picture shows the lock while you turn.
- **B11 with a step held** clears that step's locks (the sound stays: clear it in SOUND). **F1 + B11** undoes it.
- The black keys B1..B10 do nothing here.
- In the other modes (not NOTE) KNOB 1..4 still lock on a held step as before.

## How do I change the track's defaults?

- With **nothing held**, pick the mode (B1..B10) and turn **ALGORITHM**: it changes the track's default for that mode
  (velocity in VEL, the notes in NOTE, …). The screen shows the default big while you turn.
- Only the steps that don't have their own value follow it.
- In SOUND, ALGORITHM (or PRESETS) chooses the track's own sound.

## How do I work with pages and loops?

- A track has 8 pages of 16 steps. **SELECT** chooses the page you edit. **PAGE (ENV) held + W1..W8** jumps to a page.
- **PAGE tapped** turns page follow on or off (on: the page you see follows the playhead; PAGE lit).
- Hold **F1** to see the pages:
  - **F1 + tap B1..B8**: edit that page.
  - **F1 + double-tap B1..B8** (quickly): loop that page alone.
  - **F1 + hold B1..B8**: that page goes into the loop, or out of it (the loop always keeps one page).
  - **F1 + hold one page key, press another**: loop all the pages between them.
  - Lights: the page you edit lit, the loop's pages dim, the playing page flashing.
- Every page has its own start and end. Hold **F3** (both OCT keys):
  - **F3 + W**: the page ends at that step, and the page joins the loop.
  - **F3 + hold W a, press W b**: the page plays from a to b.
  - **F3 + KNOB 3 / KNOB 4**: the page's start / end (the start never passes the end).
- The loop plays its pages in order, each from its start to its end.

## How do I copy, paste and undo?

Steps:

- **F1 + W**: the first step you press is **copied**; every other step you press while F1 is held gets it **pasted**.
- When you let go of F1 the copy stays: the next time you hold **F2**, the first step you press gets it.
- **F2 + W** is a **pick-up and drop** (the OMX-27 Form's):
  - the first step you press is **cut** into your hand, even an empty one (so it also erases);
  - then an **empty step** always gets what's in your hand **pasted**;
  - a **full step** alternates: paste over it, then the next full step is cut into your hand, and so on;
  - letting go of F2 empties your hand. The screen shows the page and what's in your hand.

Pages and tracks (hold **F1**):

- **F1 + tap B9**: clear the page. **F1 + hold B9** (600 ms): clear the whole track (every page).
- **F1 + B10**: copy the page you're editing. Then **B1..B8** paste it onto that page (press again to paste again;
  the page joins the loop). The copied page's key blinks.
- **F1 + B11**: undo. Everything you did during one F1 or F2 hold is undone together. Press it again to redo.

## How do I choose, mute and solo tracks?

- Hold **SEL**: the screen shows the eight tracks.
  - **SEL + B1..B8**: select that track.
  - **SEL + tap W1..W8**: mute / unmute that track. **SEL + hold W1..W8**: solo / unsolo it. When any track is
    soloed, only soloed tracks play.
  - **SEL + B11**: unmute and unsolo every track.
  - Lights: W1..W8 lit = you hear it, off = muted (or another track is soloed), blinking = soloed; B11 lit while any
    track is muted or soloed.
- **TRACKS (FX)** still shows the eight clocks: KNOB 1..4 set the selected track's length, speed, voice limit and swing;
  W1..W8 select a track, W9..W16 mute tracks 1..8.

## How do I change patterns?

- Hold **SEQ**: the screen shows A01..A16 (the playing one white, the queued one blinking orange).
- **SEQ + tap W1..W16**: that pattern comes in at the next change point. **SEQ + hold W**: it comes in now.
- **SEQ + ALGORITHM**: when a tapped pattern comes in: **END** (the end of the playing pattern, the default), **BAR**
  (the next bar), **BEAT** (the next beat) or **NOW**.
- Stopped, a pattern is chosen at once.

## How do I change speed, tempo and swing?

Hold **F3** (both OCT keys). A track's speed is a **note value** and a **type**, each kept when you change the other:

- **F3 + B1..B7**: the note value of a step: 1/1, 1/2, 1/4, 1/8, **1/16** (normal speed), 1/32, 1/64.
- **F3 + B8 / B9 / B10**: normal, **dotted** (a step half as long again) or **triplet** (two thirds as long).
  So a dotted track stays dotted when you press F3 + the 1/8 key: dotted 1/8.
- **F3 + KNOB 1**: the note value; **KNOB 2**: the type. The screen shows it as "1/8." (dotted) or "1/8T" (triplet).
- **F3 + SELECT**: the tempo. **SHIFT + F3 + SELECT**: the swing.

## The OCT keys' lights

Where OCT− / OCT+ change an octave (NOTE with a step held, the keyboard) or SOUND's group of ten:

- At the centre (octave 4 in NOTE, octave 0 on the keyboard, the first ten sounds) neither blinks.
- One octave down, OCT− blinks slowly; two down, faster; three or more, faster still. OCT+ the same going up.
- Each change shows the octave (or the group) big for a second, with a keyboard sliding to it.

## How do I play the keyboard and record?

- **REC tapped**: the keybed becomes a keyboard for the selected track (REC lit). REC again: back to the steps.
- In a scale, keys outside the scale play the scale note just below.
- **F1 / F2** move the keyboard an octave (−2..+2).
- **SHIFT + REC** (in the keyboard) arms recording; REC blinks.
  - Stopped, it is **step recording**: each note or chord goes into the next step. F2 leaves a rest, F1 steps back,
    PRESETS moves the cursor.
  - Playing, it is **live recording**: notes go in with your timing and length. Knob turns become locks on the step
    playing.
- **REC + PLAY** starts playing with recording armed, in the keyboard.
- **Hold ARP** for **FILL** (ARP lit): steps with the FILL / NOT FILL condition follow it.
- A **MIDI keyboard** on the track's channel plays, records, or sets the notes of a held step (up to five).

## How do I change sounds?

- The project has a **pool** of sounds: INIT, the 16 factory sounds, and the drums KICK, SNARE, CLAP, GHOST, HAT and
  OPEN HAT (23 so far, up to 128).
- **Nothing held: PRESETS** picks the track's own sound from the pool. **KNOB 1..4** turn the track's FREQ, DECAY,
  HARM and FDBK.
- **A step held: PRESETS** gives the held steps a sound from the pool (a sound lock), in any mode. Turn it all the way
  down to go back to the track's sound.
- In **SOUND** mode the ten black keys are ten sounds of the pool (the group of ten that holds the step's sound);
  F1 / F2 show the ten before / after; B11 removes the sound lock.

## The scale (for now)

- **SHIFT + ALGORITHM** sets the track's scale (chromatic, major, minor, the modes, harmonic and melodic minor,
  pentatonics, blues, whole tone). **SHIFT + PRESETS** sets its root.
- This is temporary until the track settings page exists.

## Help on screen

- Holding **F1**, **F2**, **F3**, **SEL** or **SEQ** shows a map of what it does.
- Holding **SHIFT** alone shows what SHIFT does on this screen.

## Not built yet

- The sound editor (SOUND), so the pool only has the sounds it starts with, and P-LOCK locks only the step view's
  knobs.
- Track settings (scale, root, following the global key, MIDI channel), pattern chains, MIX and the effects screens.
- Saving: closing the app loses everything.
