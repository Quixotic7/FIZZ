# Playing FIZZ (P1.4a)

The prototype runs in the FM1VST Standalone on the Mac. Turn on **Remote FM-1** there; the FM-1 is the panel and the
sound comes from the Mac. Printed names are in brackets.

**The screens show state, not instructions** (P1.3v): the value you are changing, the mode, the track or pattern, the
scale; everything else is drawing, colour and the LEDs. The gestures are in this guide, not on the screen.

## Names used here

| key | name here |
| --- | --- |
| OCT− | **F1** |
| OCT+ | **F2** |
| OCT− and OCT+ together | **F3** (it stays open; OCT− closes it) |
| GLO | **SHIFT** |
| SEL (printed) | **SEL**: hold it for the tracks (a tap does nothing yet) |
| SEQ (printed) | **SEQ**: hold it for the patterns |
| LFO (printed) | **LATCH**: the step latch, on / off |
| FX (printed) | **FX** (still TRACKS: the clocks screen) |
| EDIT (printed) | **EDIT**: the sound editor |
| the black keys, left to right (F#3 .. D#5) | **B1 .. B10** |
| the rightmost black key (F#5) | **B11** |
| the white keys | **W1 .. W16**, the 16 steps of the page you are editing |

"Held" means down for 400 ms (a white step key: 250 ms). Anything shorter is a tap.

## Getting around

- The unit wakes on the FIZZ logo screen: champagne bubbles rising in the app's colours, the big ones turning with
  tiny bubbles floating inside. Once a loop the word FIZZ gathers out of the bubbles as particles, stands as solid
  coloured letters, then fizzes apart again. While a DAW plays it, a surge of bubbles rises on each beat.
  Press anything to get to the **step view**.
- **HOME** always brings you back to the view you are using (PIANO ROLL, STEPS or EDIT), from anywhere, in one press:
  it closes F3, a settings menu or the step latch and lands on the view at the same time. It never changes the view.
- **SHIFT + HOME** cycles the views: **PIANO ROLL** (the mode's lane), **STEPS** and **EDIT**. STEPS shows all eight
  pages as rows of 16 steps in groups of four; the page you edit is unfolded into taller cells with each step's notes
  drawn inside at their pitch, the other pages are rows above and below. Pages in the loop are bright, the others
  dim. A step's colour says what it plays: **warm white** its own notes, **cyan** the track's notes (a plain trig),
  **violet outline** a ghost (a LOCK step, no note). On the open row a step's fill is as tall as its velocity (a
  ghost's velocity is a tick), and a selected step shows its length as a line under the row (an arrow: to the end).
  Outside NOTE mode, a thin bar in the mode's colour under a step means it has its own value for that mode. Playing,
  a red marker hops along the steps (on a folded row, a red tick under the step) and each step's fill flashes white
  as it plays; only selected or held steps get a white outline. SELECT changes the page (the rows fold and
  unfold). The step cursor (ALGORITHM, below) moves through the rows.
- **EDIT** (SHIFT + HOME from STEPS) shows the page you edit as one row at the bottom (as STEPS draws it) and the hold
  mode's drawing above it at full size, its value large in the top line: the step you hold, select or point at with
  the cursor, or the track's default when nothing is held (a mode key held: its default, "B11 RESET" small top
  right). Every edit moves both at once (the cell's fill rises as the mallet swings); no knob pop-up covers it.
  Holding, the step latch and the cursor all stay in EDIT. Turning **PRESETS** shows the sound picture in the top
  while you turn (with a finger on a step: that step's sound lock).
- **The knobs with no finger on a step** (nothing held, or the step latch's selection), in every view, layout **A**
  (the global settings can switch layouts, see below):
  - **ALGORITHM = the step cursor**: it turns the step latch on and moves its one selected step through all eight
    pages in order (each over its start..end; page 8 runs on to page 1 and back), the page following. The black keys
    and knobs then edit that step; the view stays (in the PIANO ROLL the cursor's step is outlined). LFO or HOME ends it.
  - **SELECT = the page**. With the cursor on, a page change puts it on the new page's first step.
  - **PRESETS = the track's sound** (preset).
  - With a finger on a step: SELECT = the main value (NOTE: note 1), ALGORITHM = the value finely, PRESETS = the
    step's sound lock.
  - What moved: the cursor was PRESETS (in STEPS / EDIT), now ALGORITHM everywhere; ALGORITHM no longer edits a
    track default unless you hold the mode's key; P-LOCK's track chooser is now FX + ALGORITHM.
- **Track or step at a glance**: whenever the screen shows the **track's default** (a mode key held, the default's
  picture after a knob turn, EDIT with nothing held) it has a **magenta frame** round the whole screen and its drawing
  is magenta (the TRACK colour; no mode uses it). A step's screens keep the mode's colours on black, no frame.
- Pop-ups (the held-step and default pictures, the knob takeovers, the octave and sound pickers) no longer show the
  track's box and name: only what is being edited.
- In the PIANO ROLL the small dots over the columns mark the beats (steps 1, 5, 9, 13; the first a little brighter).
- **FX held + ALGORITHM** chooses the track: the track's number big in its colour with its name, the eight below.
- After two minutes untouched with nothing playing, it goes back to the FIZZ screen.
- **PLAY** tapped plays and stops (on the release: about a tenth of a second after the press). **Hold PLAY** to open
  the **F3** menu (the same as both OCT keys).

## How do I add and remove steps?

- **Tap** a white key: the step gets a note. Tap it again to remove it (and its locks).
- A new step plays the **track's defaults** (its notes, velocity, length and so on). Change a default and every step
  that still uses it changes too.

## How do I change one step's note, velocity, length …?

1. Pick what to edit with a black key, **nothing held**: **B1 NOTE, B2 VEL, B3 LEN, B4 MICRO, B5 RATCH, B6 CHNC
   (chance), B7 COND (condition), B8 TRIG, B9 FUNC (the step functions), B10 SOUND, B11 P-LOCK**. The screen draws the page
   for that choice; steps that have their own value are in colour, steps on the track's default are grey, and the
   default is a dashed line or a grey label.
2. **Hold** the step's white key. The screen shows that step (the black keys' ten values are in the table below; the
   screen does not draw them). On NOTE the step's notes are written in one line right over the keyboard,
   each in the colour of the knob that turns it (SELECT yellow, KNOB 1..4 blue, green, white, orange, the sixth pink),
   with the track's scale small at the top left; a chord the unit knows is named big over the notes.
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
| FUNC | (SELECT: the page, for a jump's target) KNOB 1: the kind | LEAP's amount (KNOB 3: JUMP's target) | function, quietly |
| SOUND | the pool sound (PRESETS too) | | sound, quietly |

"Quietly": **KNOB 4** turns the main value **without any overlay**; the screen just changes in place. Knobs with
nothing in the table do nothing. **PRESETS** is always the step's sound.

The ten values (the key with the **nearest** value lights, not only an exact one: VEL 100 lights 102; a tie lights
the lower key; MICRO 0, on the step, lights none; a sound in another group of ten lights the group's end nearest it):

| mode | B1 .. B10 | ALGORITHM |
| --- | --- | --- |
| VEL | 13 25 38 51 64 76 89 102 114 127 | 1..127 |
| LEN | 1/4 1/2 3/4 1 1.5 2 4 8 16 INF steps | every length |
| MICRO | −20 −15 −10 −5 −2 +2 +5 +10 +15 +20 (24ths of a step) | −23..+23; SHIFT + ALGORITHM: the strum |
| RATCH | B1..B8: x1..x8; B9: the ramp (flat, up, down, accent) | x1..x8; SHIFT + ALGORITHM: the ramp |
| CHNC | 10 % .. 100 % | 0..100 % |
| COND | B1 FILL, B2 NOT FILL, B3..B6 "A" = 1..4, B7..B10 "B" = 1..4 (plays on pass A of every B) | every condition |
| TRIG | B1 NOTE, B2 LOCK (changes the sound, no note), B3 TIE (slides a mono note) | the same |
| FUNC | B1 none, B2 RESTART, B3 REVERSE, B4 FORWARD, B5 FLIP, B6 RANDOM (jump), B7 RANDOM FUNCTION, B8 LEAP, B9 JUMP | the nine kinds |
| SOUND | ten sounds of the pool (see below) | any sound |

**FUNC (B9), the step functions** (as the OMX-27's Form): a function runs when its step plays (after its condition
and chance; a LOCK step's runs too).

- **RESTART**: the next step is the loop's start, and the passes count from 1 again (A:B, 1ST).
- **REVERSE / FORWARD**: the track plays backwards / forwards from here (each page from its end to its start, the
  pages in reverse order); **FLIP** turns it round. Micro-timing is mirrored in reverse. **PLAY always starts
  forwards** (a pattern change keeps the direction).
- **RANDOM**: the next step is one of the loop at random. **RANDOM FUNCTION**: none, restart, reverse, forward or flip
  at random, each time.
- **LEAP n** (B8; KNOB 2 the amount, −15..+15, 0 skipped): the next step is n steps on from this one in the playing
  direction, round the loop (LEAP +1 is a normal step, +2 skips one, −1 plays the step before again). B8 uses the
  last amount (+2 at first).
- **JUMP n** (B9; KNOB 3 the target): the next step is step n (on a page outside the loop: the loop's start). B9 uses
  the last target (at first the page's first step). To aim it: **hold B9 and press the target's white key** (it is
  not held; it works with the step latch too). **SELECT** while holding turns the page to target a step on another
  page. KNOB 1 (or ALGORITHM, with a finger on the step) walks the nine kinds.
- The FUNC keys light like the other modes': the step's function's key lit.
- **B11** removes the step's function. The PIANO ROLL's FUNC lane shows each step's function as an icon (an arrow, a
  circular arrow, a die, a jump with its target's number, two hops with LEAP's amount); STEPS marks a step with a
  function with a small blue dot.

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
- **Hold LFO + a black key**: that mode, without touching the latch or the selection (a plain tap still toggles).

## How do I enter notes and chords?

A step has up to **six notes**.

- In **NOTE**, hold the step. **Each knob moves its own note, alone**; each note is drawn in its knob's colour:
  - **SELECT = note 1** (yellow), **KNOB 1..4 = notes 2..5** (blue, green, white, orange). One click is one note of
    the track's scale (a chromatic track: a semitone). The other notes stay put; a pitch another note already has is
    skipped.
  - Notes 2..5 live above note 1: turn one down past note 1 and it is removed; turn it up from nothing and it comes in
    a scale step above note 1.
  - **ALGORITHM moves all the notes together**.
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
  locks under each step as dots.
- **Hold a step**: the screen stays as it is. Turn a knob of the screen and it is **locked on the held steps**: KNOB
  1..4 = FREQ, DECAY, HARM, FDBK; PRESETS = the step's sound; SELECT = the page. The knob's picture shows the lock.
- **B11 with a step held** clears that step's locks (the sound stays: clear it in SOUND). **F1 + B11** undoes it.
- The black keys B1..B10 do nothing here. Only P-LOCK locks the sound.
- **Nothing held** in P-LOCK: KNOB 1..4 turn the track's own FREQ, DECAY, HARM, FDBK; PRESETS its sound; SELECT the
  page; ALGORITHM the cursor (choose the track with **FX + ALGORITHM**).

## How do I change the track's defaults?

- **Hold the mode's black key** (B1..B10): the screen shows the mode's picture marked **TRACK DEFAULT** (NOTE: the
  chord name and the keybed). While you hold it:
  - SELECT and the knobs edit the default, in the same layout as with a step held (NOTE: SELECT = note 1, KNOB 1..4 =
    notes 2..5, each alone); ALGORITHM turns the main default (in NOTE it moves the whole default chord);
  - **B11 (F#5) resets** that default to the track's starting value: NOTE back to a single C4 (clears a chord), VEL 100,
    LEN one step, MICRO 0 (and no strum), RATCH x1 flat, CHNC 100 %, COND always, TRIG NOTE, SOUND the track's
    starting sound. B11 is lit while there is something to reset;
  - the other black keys do nothing. Let go to get the page back.
- With **nothing held at all**, KNOB 1..4 still edit the defaults; the TRACK DEFAULT picture shows for a second.
  KNOB 4 does it quietly (no picture). ALGORITHM is the cursor now: hold the mode's key to turn the main default.
- Only the steps that don't have their own value follow a default.
- In SOUND, KNOB 1 (or ALGORITHM with the SOUND key held, or PRESETS) chooses the track's own sound.

## How do I work with pages and loops?

- A track has 8 pages of 16 steps. **SELECT** chooses the page you edit. **PAGE (ENV) held + W1..W8** jumps to a page.
- **PAGE tapped** turns page follow on or off (on: the page you see follows the playhead; PAGE lit). It is also on
  the global settings page (SHIFT + SEL, KNOB 2).
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

- Hold **SEL**: the screen shows the eight tracks (after a quarter second, or at once when you press a key with it;
  a quick tap shows and does nothing).
  - **SEL + B1..B8**: select that track.
  - **SEL + tap W1..W8**: mute / unmute that track. **SEL + hold W1..W8**: solo / unsolo it. When any track is
    soloed, only soloed tracks play.
  - **SEL + B11**: unmute and unsolo every track.
  - Lights: W1..W8 lit = you hear it, off = muted (or another track is soloed), blinking = soloed; B11 lit while any
    track is muted or soloed.
- **TRACKS (FX)** still shows the eight clocks: KNOB 1..4 set the selected track's length, speed, voice limit and swing;
  W1..W8 select a track, W9..W16 mute tracks 1..8.
- **FX held + ALGORITHM**: choose the track, with the big track picture. FX tapped still opens the clocks screen.

## How do I change patterns?

- Hold **SEQ**: the screen shows one bank's 16 patterns (the playing one white, the queued one blinking orange).
- **SEQ + B1..B4**: bank A..D (64 patterns). The current bank's key is lit, banks with patterns in them dim.
- **SEQ + tap W1..W16**: that pattern of the bank comes in at the next change point. **SEQ + hold W**: it comes in now.
- **SEQ + ALGORITHM**: when a tapped pattern comes in: **END** (the end of the playing pattern, the default), **BAR**
  (the next bar), **BEAT** (the next beat) or **NOW**.
- Stopped, a pattern is chosen at once.

## How do I change speed, tempo and swing?

Press **OCT− and OCT+ together** (or **hold PLAY**): the **F3** menu opens and **stays open** after you let go (a back arrow at the top
right, OCT− lit). **OCT−** (or HOME) closes it. OCT+ does nothing there yet.

From now on every menu or question that stays open works the same way: **OCT− = back / exit / cancel, OCT+ = yes /
confirm**.

A track's speed is a **note value** and a **type**, each kept when you change the other:

- **F3 + B1..B7**: the note value of a step: 1/1, 1/2, 1/4, 1/8, **1/16** (normal speed), 1/32, 1/64.
- **F3 + B8 / B9 / B10**: normal, **dotted** (a step half as long again) or **triplet** (two thirds as long).
  So a dotted track stays dotted when you press F3 + the 1/8 key: dotted 1/8.
- **F3 + KNOB 1**: the note value; **KNOB 2**: the type. The screen shows it as 1/8 with a dot (dotted) or a T
  (triplet); the black keys at the foot of the F3 screen are drawn as the note values, a dot and a triplet.
- **F3 + SELECT**: the tempo; **F3 + ALGORITHM**: the swing. Each turn shows the big metronome picture for a second,
  then the F3 screen again (which also shows both).
- The page whose range you set is the one you edit (F3 + W sets its end, W + W its start..end; KNOB 3 / 4 too).
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
- **SHIFT + REC**: the **record settings** (they stay open; OCT− or HOME back), kept with the project:
  - **KNOB 1 MODE**: LIVE or STEP, what holding REC arms;
  - **KNOB 2 QUANTIZE**: 0..100 % (5 % a click): how much of the played timing is pulled onto the step (0 % as
    played, 100 % exactly on the step);
  - **KNOB 3 TAKE**: **REPLACE** (a step you record into is cleared first) or **OVERDUB** (your notes join the step's;
    its velocity, timing and length stay).
- **Hold REC**: recording armed (in the keyboard) in the MODE chosen.
  **LIVE**: stopped, nothing moves until you play the **first key**:
  playback and recording start together, that note on the loop's first step.
  - Notes go to the nearest step **at the track's speed** (dotted and triplet too), the offset kept as micro-timing,
    the length from how long you held the key. Knob turns become locks on the step playing.
  - An **empty track grows** as you record: it starts as page 1, and each next page joins the loop as the playhead
    gets there; when the recording ends the loop is the pages it reached. After page 8 it goes round and records over
    what is there. A track with steps records into its own loop.
  - **REC or PLAY** while recording ends the recording; playback goes on (PLAY again stops).
  **STEP**: **step recording**, stopped or playing: each note or chord goes into the cursor's step and the cursor moves
  on. F2 leaves a rest, F1 steps back, PRESETS moves the cursor. REC tapped ends it (still the keyboard).
- SHIFT + REC no longer arms recording (it opens the settings).
- **REC + PLAY** starts playing with recording armed, in the keyboard.
- **Hold ARP** for **FILL** (ARP lit): steps with the FILL / NOT FILL condition follow it.
- A **MIDI keyboard** on the track's channel plays, records, or sets the notes of a held step (up to five).

## How do I change sounds?

- The project has a **pool** of sounds: INIT, the 16 factory sounds, and the drums KICK, SNARE, CLAP, GHOST, HAT and
  OPEN HAT (23 so far, up to 128).
- **Nothing held: PRESETS** picks the track's own sound from the pool, in every view (EDIT shows the sound picture
  while you turn). In **P-LOCK**, KNOB 1..4 turn the track's FREQ, DECAY, HARM and FDBK.
- **A step held: PRESETS** gives the held steps a sound from the pool (a sound lock), in any mode. Turn it all the way
  down to go back to the track's sound.
- In **SOUND** mode the ten black keys are ten sounds of the pool (the group of ten that holds the step's sound);
  F1 / F2 show the ten before / after; B11 removes the sound lock.

## How do I edit a sound? (the sound editor, EDIT)

- **EDIT** opens the sound editor for the selected track (EDIT lit). **EDIT again, OCT− or HOME** closes it. It opens
  over the step view from any screen.
- The black keys are its five groups; **press a group's key again for its next page** (round), or turn **SELECT**
  through every page. B1..B5 are lit (the group you are in bright).

  | key | group | pages |
  | --- | --- | --- |
  | **B1** | OPERATORS | RATIO (KNOB 1..4 the ratios of C, A, B1, B2; **ALGORITHM the algorithm**), TONE (HARM, DTUNE, FDBK, MIX), OFFSET (the four offsets) |
  | **B2** | OP ENVELOPES | ENV A, ENV B (attack, decay, end, level; ALGORITHM the delay), TRIG (A / B TRIG and RESET; ALGORITHM PHRT), KEYS (A, B1, B2 key scaling, VEL) |
  | **B3** | FILTERS | FILTER (FREQ, RESO, TYPE, DEPTH), F ENV (attack, decay, sustain, release; ALGORITHM the delay), BASE (BASE, WIDTH, KTRK) |
  | **B4** | ENVELOPES | AMP (attack, decay, sustain, release), LEVEL (LEVEL, **DRIVE**) |
  | **B5** | MODULATION | LFO 1..3 (SPEED, MULT, DEPTH, FADE; ALGORITHM the destination), LFO 1..3 WAVE (WAVE, PHASE, TRIG) |

  Every value of the sound is on exactly one page.
- Each page is one big drawing of that part of FM TONE (the operators wired by the algorithm, the envelopes, the
  filter's curve, the LFO's wave ..). Along the bottom, KNOB 1..4's values in their colours (the one you turned last
  underlined); ALGORITHM's value is in the pink badge top right.
- **Nothing held**: a turn changes the track's sound, live (what is sounding follows). The screen has the magenta TRACK
  frame. **SHIFT + a knob** moves it by the finest step (a hundredth of FREQ, HARM, the LFO speeds ..).
- **A finger on a step** (held, or the step latch's selection: LFO works in the editor): **every turn is a p-lock** on
  that step. The top says STEP n with a padlock; locked values get a padlock, and the envelopes show the track's curve
  dashed behind the step's. **B11** clears the held step's locks on this page (lit while it has some). The white keys
  stay the steps (tap, hold), as in the step view.
- **PRESETS**: the track's preset (nothing held) or the step's sound lock (a step held), as everywhere.
- **DRIVE** (B4, LEVEL page, KNOB 2) is part of the sound now: ChoralRoot's DIST (low cut, the drive, a soft clip, a
  tone low-pass, make-up gain), in each voice, so a step with its own sound or a DRIVE lock has its own. A preset
  loads its drive (WOBBLE: 10; the others 0). FM TONE's LFOs can move it (destination DRIVE).

## The scale (for now)

- **SHIFT + ALGORITHM** sets the track's scale (chromatic, major, minor, the modes, harmonic and melodic minor,
  pentatonics, blues, whole tone). **SHIFT + PRESETS** sets its root.
- This is temporary until the track settings page exists.

## The global settings (SHIFT + SEL)

They stay open (OCT− or HOME back). For now:

- **KNOB 1 KNOBS**: the encoders' layout, **A**, **B**, **C** or **D**: what SELECT, ALGORITHM and PRESETS do with no
  finger on a step (nothing held, or the step latch's selection). With a finger on a step they are the same in every
  layout (the table above; PRESETS the sound lock). The screen draws the three encoders with their jobs under them.

  | layout | SELECT | ALGORITHM | PRESETS |
  | --- | --- | --- | --- |
  | **A** (P1.3y, the start) | the page | the step cursor | the track's sound |
  | **B** (P1.3q's) | the page | the mode's value (the selection's, else the track default) | the track's sound in the PIANO ROLL, the step cursor in STEPS / EDIT |
  | **C** | the step cursor | the page | the track's sound |
  | **D** | the step cursor (it carries the page) | the mode's value (as B) | the track's sound |

  In B and D, "the mode's value" with nothing selected is the track's default (P-LOCK: the track, as in P1.3d); with
  the latch's selection it edits the selection, as a held step would.
- **KNOB 2 PAGE FOLLOW**: on / off (the same as PAGE tapped).

## The maps

- Holding **F1**, **F2**, **SEL** or **SEQ**, or opening **F3**, shows its map: the black keys drawn where they are
  on the panel, each with an icon or a number (a page, a track, clear, copy, undo, a note value), lit by its state.
  No sentences: what each does is in this guide.
- Holding **SHIFT** alone shows the controls SHIFT changes here (a knob, REC, F3, SEQ and a step, HOME, SEL), each
  with its job's icon and its value now (the scale, the root, the swing, the next view, the record MODE, the knob
  layout).
- A page change in the PIANO ROLL with page follow off: a crossed arrow at the top right (was FOLLOW OFF). The small
  dots over the columns mark the beats. The PIANO ROLL's top line no longer shows the tempo (turn it, or open F3).

## Not built yet

- Saving sounds into the pool (the editor changes the track's sound; P1.5a stores it).
- Track settings (scale, root, following the global key, MIDI channel; SEL tapped will open them), pattern chains,
  MIX and the effects screens.
- Saving: closing the app loses everything.
