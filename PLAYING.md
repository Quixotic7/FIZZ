# Playing FIZZ (P1.3p)

The prototype runs in the FM1VST Standalone on the Mac. Turn on **Remote FM-1** there; the FM-1 is the panel and the
sound comes from the Mac. Printed names are in brackets.

## Names used here

| key | name here |
| --- | --- |
| OCT− | **F1** |
| OCT+ | **F2** |
| OCT− and OCT+ together | **F3** (it stays open; OCT− closes it) |
| GLO | **SHIFT** |
| SEL (printed) | **SEL**: hold it for the tracks |
| SEQ (printed) | **SEQ**: hold it for the patterns |
| LFO (printed) | **LATCH**: the step latch, on / off |
| the black keys, left to right (F#3 .. D#5) | **B1 .. B10** |
| the rightmost black key (F#5) | **B11** |
| the white keys | **W1 .. W16**, the 16 steps of the page you are editing |

"Held" means down for 400 ms (a white step key: 250 ms). Anything shorter is a tap.

## Getting around

- The unit wakes on the FIZZ logo screen: champagne bubbles rising in the app's colours, the big ones turning with
  tiny bubbles floating inside. Once a loop the word FIZZ gathers out of the bubbles as particles, stands as solid
  coloured letters, then fizzes apart again. While a DAW plays it, a surge of bubbles rises on each beat.
  Press anything to get to the **step view**.
- **HOME** in the sequencer switches its view: **PIANO ROLL** (the mode's lane) and **STEPS**. STEPS shows all eight
  pages as rows of 16 steps in groups of four; the page you edit is unfolded into taller cells with each step's notes
  drawn inside at their pitch, the other pages are rows above and below. Pages in the loop are bright, the others
  dim; a LOCK step is a ring, a coloured bar under a step means it has its own value for the current mode. Playing, a
  red marker hops along the steps and each step pops as it plays. SELECT changes the page (the rows fold and unfold).
  From any other screen HOME comes back to the sequencer in the view you used last. If F3 or the step latch is open,
  HOME closes that first.
- After two minutes untouched with nothing playing, it goes back to the FIZZ screen.
- **PLAY** plays and stops.

## How do I add and remove steps?

- **Tap** a white key: the step gets a note. Tap it again to remove it (and its locks).
- A new step plays the **track's defaults** (its notes, velocity, length and so on). Change a default and every step
  that still uses it changes too.

## How do I change one step's note, velocity, length …?

1. Pick what to edit with a black key, **nothing held**: **B1 NOTE, B2 VEL, B3 LEN, B4 MICRO, B5 RATCH, B6 CHNC
   (chance), B7 COND (condition), B8 TRIG, B9 SOUND, B11 P-LOCK** (B10 is empty for now). The screen draws the page
   for that choice; steps that have their own value are in colour, steps on the track's default are grey, and the
   default is a dashed line or a grey label.
2. **Hold** the step's white key. The screen shows that step; the strip at the bottom shows what the ten black keys do.
3. **Turn the knobs** (table below), or press a black key **B1 .. B10** for one of ten values (shortcuts).
   ALGORITHM also turns the main value.
4. **B11** puts the step back on the track's default.

Hold several white keys to edit them all at once. Each knob turn shows its name and value in a band at the top.

The knobs with a step held:

| mode | SELECT and KNOB 1 | KNOB 2 | KNOB 4 |
| --- | --- | --- | --- |
| NOTE | SELECT = note 1, KNOB 1..4 = notes 2..5 (each alone), ALGORITHM = all the notes | | note 5 |
| VEL | velocity | | velocity, quietly |
| LEN | length | TIE on / off | length, quietly |
| MICRO | micro-timing | strum | micro, quietly |
| RATCH | ratchet count | ramp | count, quietly |
| CHNC | chance | condition | chance, quietly |
| COND | condition | chance | condition, quietly |
| TRIG | NOTE / LOCK / TIE | | type, quietly |
| SOUND | the pool sound (PRESETS too) | | sound, quietly |

"Quietly": **KNOB 4** turns the main value **without any overlay**; the screen just changes in place. Knobs with
nothing in the table do nothing. **PRESETS** is always the step's sound.

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

## How do I hear a step while I edit it?

- With the transport **stopped**, a held step plays: its notes, sound, locks and velocity. Every edit plays it again.
  Letting go stops it. Hold several and they play together. While playing there is no preview.

## How do I edit steps without holding them? (the step latch)

- Press **LFO**: the latch is on (LFO lit). It is as if a step is always held. It starts on the step you last edited.
- **Tap a white key** to select just that step; **hold several together** to select exactly those. Whatever your
  fingers press, from the first key down until all are up, replaces the selection (it never adds or removes a note).
  Tapping the step that is the only one selected unselects it. Selected keys blink.
- Everything you do with a held step works on the selection: black keys, knobs, the screens, OCT for the octave, the
  preview.
- **With nothing selected** the latch stays on but acts as if nothing is held: the black keys choose the mode
  again, the knobs edit the track's defaults. Tap a step to select again.
- **LFO** again or **HOME** turns it off.

## How do I enter notes and chords?

A step has up to **six notes**.

- In **NOTE**, hold the step. **Each knob moves its own note, alone**; each note is drawn in its knob's colour:
  - **SELECT = note 1** (yellow), **KNOB 1..4 = notes 2..5** (blue, green, white, orange). One click is one note of
    the track's scale (a chromatic track: a semitone). The other notes stay put; a pitch another note already has is
    skipped.
  - Notes 2..5 live above note 1: turn one down past note 1 and it is removed; turn it up from nothing and it comes in
    a scale step above note 1.
  - **ALGORITHM moves all the notes together** ("A = ALL" on the screen).
  - A sixth note (pink) comes only from the black keys.
  - Each note plays as you turn it.
  - In NOTE, SELECT doesn't change pages and KNOB 1..4 don't lock the sound: use P-LOCK for that. (In every mode
    except P-LOCK, SELECT and the knobs edit the step while it is held.)
- Or press black keys: they are ten notes of the track's scale from its root (a chromatic track: ten semitones from
  C). The first key replaces the step's notes; keys pressed **while another is still down** add to the chord (up to
  6). Each key plays its note.
- **F1 / F2** (OCT− / OCT+) while holding the step move the ten keys an octave down / up. The screen shows the octave
  big for a second, and the OCT keys' lights tell you where you are (see below).
- The notes are kept in the order you entered them: the first key pressed is note 1 (SELECT's), the second note 2
  (KNOB 1's), and so on.

## How do I lock a sound setting on a step? (P-LOCK)

- Press **B11** with nothing held: the **P-LOCK** mode. The page shows the notes, with a padlock and the number of
  locks under each step.
- **Hold a step**: the screen stays as it is. Turn a knob of the screen and it is **locked on the held steps**: KNOB
  1..4 = FREQ, DECAY, HARM, FDBK; PRESETS = the step's sound; SELECT = the page. The knob's picture shows the lock.
- **B11 with a step held** clears that step's locks (the sound stays: clear it in SOUND). **F1 + B11** undoes it.
- The black keys B1..B10 do nothing here. Only P-LOCK locks the sound.
- **Nothing held** in P-LOCK: KNOB 1..4 turn the track's own FREQ, DECAY, HARM, FDBK; PRESETS its sound;
  **ALGORITHM chooses the track** (1..8, the screen lists them); SELECT the page.

## How do I change the track's defaults?

- **Hold the mode's black key** (B1..B9): the screen shows the mode's picture marked **TRACK DEFAULT** (NOTE: the
  chord name and the keybed). While you hold it:
  - SELECT and the knobs edit the default, in the same layout as with a step held (NOTE: SELECT = note 1, KNOB 1..4 =
    notes 2..5, each alone); ALGORITHM turns the main default (in NOTE it moves the whole default chord);
  - **B11 (F#5) resets** that default to the track's starting value: NOTE back to a single C4 (clears a chord), VEL 100,
    LEN one step, MICRO 0 (and no strum), RATCH x1 flat, CHNC 100 %, COND always, TRIG NOTE, SOUND the track's
    starting sound. B11 is lit while there is something to reset;
  - the other black keys do nothing. Let go to get the page back.
- With **nothing held at all**, the knobs and ALGORITHM still edit the defaults; the TRACK DEFAULT picture shows for a
  second. KNOB 4 does it quietly (no picture).
- Only the steps that don't have their own value follow a default.
- In SOUND, KNOB 1 or ALGORITHM (or PRESETS) chooses the track's own sound.

## How do I work with pages and loops?

- A track has 8 pages of 16 steps. **SELECT** chooses the page you edit. **PAGE (ENV) held + W1..W8** jumps to a page.
- **PAGE tapped** turns page follow on or off (on: the page you see follows the playhead; PAGE lit).
- Hold **F1** to see the pages:
  - **F1 + tap B1..B8**: edit that page.
  - **F1 + double-tap B1..B8** (quickly): loop that page alone.
  - **F1 + hold B1..B8**: that page goes into the loop, or out of it (the loop always keeps one page).
  - **F1 + hold one page key, press another**: loop all the pages between them.
  - Lights: the page you edit lit, the loop's pages dim, the playing page flashing.
- Every page has its own start and end. Open **F3** (both OCT keys; see the speed section):
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

- Hold **SEQ**: the screen shows one bank's 16 patterns (the playing one white, the queued one blinking orange).
- **SEQ + B1..B4**: bank A..D (64 patterns). The current bank's key is lit, banks with patterns in them dim.
- **SEQ + tap W1..W16**: that pattern of the bank comes in at the next change point. **SEQ + hold W**: it comes in now.
- **SEQ + ALGORITHM**: when a tapped pattern comes in: **END** (the end of the playing pattern, the default), **BAR**
  (the next bar), **BEAT** (the next beat) or **NOW**.
- Stopped, a pattern is chosen at once.

## How do I change speed, tempo and swing?

Press **OCT− and OCT+ together**: the **F3** menu opens and **stays open** after you let go ("OCT− EXIT" on the
screen, OCT− lit). **OCT−** (or HOME) closes it. OCT+ does nothing there yet.

From now on every menu or question that stays open works the same way: **OCT− = back / exit / cancel, OCT+ = yes /
confirm**.

A track's speed is a **note value** and a **type**, each kept when you change the other:

- **F3 + B1..B7**: the note value of a step: 1/1, 1/2, 1/4, 1/8, **1/16** (normal speed), 1/32, 1/64.
- **F3 + B8 / B9 / B10**: normal, **dotted** (a step half as long again) or **triplet** (two thirds as long).
  So a dotted track stays dotted when you press F3 + the 1/8 key: dotted 1/8.
- **F3 + KNOB 1**: the note value; **KNOB 2**: the type. The screen shows it as "1/8." (dotted) or "1/8T" (triplet).
- **F3 + ALGORITHM**: the tempo. **SHIFT + F3 + ALGORITHM**: the swing. Both show live on the F3 screen.
- **F3 + SELECT**: the page you edit (the F3 screen follows it).
- **EXT CLOCK**: while a MIDI clock (a DAW) is running the unit, the tempo is the clock's: the F3 screen says EXT CLOCK
  with its BPM, and a tempo turn shows EXT CLOCK instead of changing it. A MIDI STOP, or no clock for half a second,
  gives the tempo back to the knob.

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
- **Nothing held: PRESETS** picks the track's own sound from the pool. In **P-LOCK**, KNOB 1..4 turn the track's
  FREQ, DECAY, HARM and FDBK.
- **A step held: PRESETS** gives the held steps a sound from the pool (a sound lock), in any mode. Turn it all the way
  down to go back to the track's sound.
- In **SOUND** mode the ten black keys are ten sounds of the pool (the group of ten that holds the step's sound);
  F1 / F2 show the ten before / after; B11 removes the sound lock.

## The scale (for now)

- **SHIFT + ALGORITHM** sets the track's scale (chromatic, major, minor, the modes, harmonic and melodic minor,
  pentatonics, blues, whole tone). **SHIFT + PRESETS** sets its root.
- This is temporary until the track settings page exists.

## Help on screen

- Holding **F1**, **F2**, **SEL** or **SEQ**, or opening **F3**, shows a map of what it does.
- Holding **SHIFT** alone shows what SHIFT does on this screen.

## Not built yet

- The sound editor (SOUND), so the pool only has the sounds it starts with, and P-LOCK locks only the step view's
  knobs. B10 is empty.
- Track settings (scale, root, following the global key, MIDI channel), pattern chains, MIX and the effects screens.
- Saving: closing the app loses everything.
