# Playing FIZZ (P1.5w)

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
| SEL (printed) | **SEL**: hold it for the tracks; tap it for the track settings |
| SEQ (printed) | **SEQ**: hold it for the patterns; tap it for the project |
| SAVE (printed) | **SAVE**: SAVE PRESET, the selected track's sound into MY PRESETS |
| LFO (printed) | **LATCH**: the step latch, on / off |
| FX (printed) | **FX**: tap it for the effects; hold it to choose the track |
| ENV (printed) | **MIX**: the mixer |
| EDIT (printed) | **EDIT**: the sound editor |
| the black keys, left to right (F#3 .. D#5) | **B1 .. B10** |
| the rightmost black key (F#5) | **B11** |
| the white keys | **W1 .. W16**, the 16 steps of the page you are editing |

The on-screen panel in FM1VST and the web preview labels the buttons by these roles (P1.5r): FX, TRACKS (SEL), MIX
(ENV), LATCH (LFO), EDIT, SHIFT (GLO), HOME, SOUNDS (SAVE), FILL (ARP), PATTERNS (SEQ), PLAY, REC, F1, F2.

"Held" means down for 400 ms (a white step key: 250 ms). Anything shorter is a tap.

**The knobs accelerate** (P1.5b): turned slowly a value moves one step a click; spun, the steps grow, up to about a
sixteenth of the range a click, so one fast turn sweeps it all. Values with few steps (a list of 24 or fewer: a
type, a mode, a scale) never speed up, and **SHIFT + a knob** is always the finest step.

## Getting around

- The unit wakes on the FIZZ logo screen: champagne bubbles rising in the app's colours, the big ones turning with
  tiny bubbles floating inside. Once a loop the word FIZZ gathers out of the bubbles as particles, stands as solid
  coloured letters, then fizzes apart again. While a DAW plays it, a surge of bubbles rises on each beat.
  Press anything to get to the **step view**, in **EDIT** (P1.5s: the default view at power-on and in a new project;
  the view you used last is kept).
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
  right). Every edit moves both at once (the cell's fill rises as the value changes); no knob pop-up covers it.
  Holding, the step latch and the cursor all stay in EDIT. Turning **PRESETS** opens the library browser (with a
  finger on a step: that step's pool picker).
- **EDIT follows the music** (P1.5q): while playing, with nothing held, the step latch off, no mode key held and no
  pop-up, the top shows **the selected track's step as it plays**: its head ("STEP 7 · VEL"), its value and its
  picture (the chord, the rider at that velocity, the bar, the flag, the ball, the cassette ..), each with its own
  "it plays" moment (the rider's kick, the chord's name squeezing in, the ball bouncing through the step). It changes
  when you hear the step, so a step with micro-timing changes late by that much, and a slow track (1/8 ..) changes
  at its pace; a ratchet changes it once. An empty step keeps the last one played, dimmed a little. A muted track is
  followed too. Stop, select a step with the latch (or the cursor), or hold a step and it shows that step again as
  before; the latch on with nothing selected follows (P1.5s); a knob's pop-up shows as before and following comes
  back after it.
- **The knobs with no finger on a step** (nothing held, or the step latch's selection), in every view, layout **A**
  (the global settings can switch layouts, see below):
  - **ALGORITHM = the step cursor**: it turns the step latch on and moves its one selected step through all eight
    pages in order (each over its start..end; page 8 runs on to page 1 and back), the page following. The black keys
    and knobs then edit that step; the view stays (in the PIANO ROLL the cursor's step is outlined). LFO or HOME ends it.
  - **SELECT = the page**. With the cursor on, a page change puts it on the new page's first step.
  - **PRESETS = the track's sound** (preset).
  - With a finger on a step: SELECT = the main value (NOTE: the whole chord, P1.5o), ALGORITHM = the value finely, PRESETS = the
    step's sound lock.
  - What moved: the cursor was PRESETS (in STEPS / EDIT), now ALGORITHM everywhere; ALGORITHM no longer edits a
    track default unless you hold the mode's key; P-LOCK's track chooser is now FX + ALGORITHM.
- **Track or step at a glance** (P1.5d: the frames the other way round): **a held step's screen has a magenta frame**
  round the whole screen (every hold mode, and the step latch's selection, which stands for a held step); its drawing
  keeps the mode's colours. Whenever the screen shows the **track's default** (a mode key held, the default's picture
  after a knob turn, EDIT with nothing held) there is **no frame**, and its drawing and top line are magenta (the
  TRACK colour; no mode uses it; P1.5o: VEL's value stays VEL's green, as a step's).
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
   On **VEL** (P1.5d) a caped stunt rider rides left: the higher the velocity the higher the wheelie (about the rear
   wheel, flat below about 15, 55 degrees at 127), the faster the wheels, the road and the speed lines, and the
   longer and flatter the cape streams. The value is big at the top right, the ratchet ramp's four ticks at the top
   left. When the step plays, the bike kicks: the front wheel bounces up a little and settles, with a puff of speed
   lines. It is drawn in neon (P1.5e, the owner's reference): every line glows; the bike, the rider and the helmet
   blue, the cape (a forked flag with two sparkles), the tank, the seat and the legs red, the engine, the fork and the
   sparkles cream; the speed lines behind the rear wheel in dashed rows of the three.
   **At VEL 127 it is a stunt show** (P1.5g): the bike stays exactly where it is at 126, in its full wheelie, and the
   world scrolls under it every 2.6 s: a ramp, eight buses in the tracks' colours, a landing ramp; the rear wheel rides
   up, arcs over the buses and lands, with fireworks over them. 126 is the plain wheelie again; when the step plays at
   127, a firework of its own.
3. **Turn the knobs** (table below), or press a black key **B1 .. B10** for one of ten values (shortcuts).
   ALGORITHM also turns the main value.
4. **B11** puts the step back on the track's default.

Hold several white keys to edit them all at once. Each knob turn shows its name and value in a band at the top.
While the band shows, the screen leaves its own top row clear under it (P1.5p): NOTE's scale name and VEL's big
value and ramp ticks step aside and come back when the band goes.

The knobs with a step held:

| mode | SELECT and KNOB 1 | KNOB 2 | KNOB 4 |
| --- | --- | --- | --- |
| NOTE | SELECT = the whole chord (P1.5o), KNOB 1..4 = notes 2..5 (each alone), ALGORITHM = the cursor (its layout's) | | note 5 |
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
- **With the step latch on** (P1.5s) only a finger plays: a white key down on a step sounds while it is down. The
  selection alone is silent, also while you edit it; **ALGORITHM** (the step cursor) plays each step it moves to
  once, briefly.

## How do I edit steps without holding them? (the step latch)

- Press **LFO**: the latch is on (LFO lit). It is as if a step is always held. It starts on the step you last edited.
- **Tap a white key** to select just that step; **hold several together** to select exactly those. Whatever your
  fingers press, from the first key down until all are up, replaces the selection (it never adds or removes a note).
  Tapping the step that is the only one selected unselects it. Selected keys blink.
- Everything you do with a held step works on the selection: black keys, knobs, the screens, OCT for the octave, the
  preview.
- **With nothing selected** the latch stays on but acts as if nothing is held: the black keys choose the mode
  again, the knobs edit the track's defaults. Tap a step to select again.
- **F1, F2 and F3 work over the latch** (P1.5m) exactly as without it: F1 + W copies and pastes, F2 + W picks up and
  drops, F1 + B1..B8 the pages, F1 + B9 / B10 / B11 clear, copy, undo, both for F3; the selection stays as it was.
  **F1 / F2 tapped** (nothing with them) are the selection's octave in NOTE and its group of ten in SOUND, as for a
  held step (on the release, so that holding them is still the layer). Its octave (or ten) picture shows its full
  second, in EDIT too (P1.5n), and F1 / F2's map only comes up when they are held longer than a tap.
- **NOTE: SELECT moves all the selected steps' notes** together (P1.5o), as with a finger on a step, in every knob
  layout and view; ALGORITHM keeps its layout's job (A: the cursor).
- **LFO** again or **HOME** turns it off.
- **Hold LFO + a black key**: that mode, without touching the latch or the selection (a plain tap still toggles).
- **Two latch modes** (P1.5b; P1.5h). **Hold LFO alone** (400 ms, no other key): while you hold it, a screen shows the
  latch mode (two drawings: the keys as values / the keys as modes, the current one lit). **Turn SELECT** to switch it
  (it changes at once); let go of LFO and the screen closes, the mode as shown. Letting go without a turn changes
  nothing. A black key instead closes the screen and picks that hold mode. The mode is kept (a global setting) and
  used every time you turn the latch on.
  - **VALUES** (as before): with the latch on, the black keys set values for the selected steps. LFO lit full.
  - **MODES**: with the latch on, the black keys choose the hold mode (as with nothing held) and **the knobs set the
    selected steps' values for that mode**. While a finger holds a step, the black keys are its values as always.
    LFO breathing slowly between 60 % and full (P1.5n; it was at half, too dim on the unit).

## How do I enter notes and chords?

A step has up to **six notes**.

- In **NOTE**, hold the step (or select it in the latch). Each note is drawn in its knob's colour (P1.5o):
  - **SELECT moves the whole chord** (yellow: note 1 and every other note with it), a note of the track's scale a
    click (chromatic: a semitone), its shape kept; at the top or the bottom of the range it stops (it never squashes).
  - **KNOB 1..4 = notes 2..5** (blue, green, white, orange), **each alone**; the other notes stay put, a pitch another
    note has is skipped. Turned all the way down a note collapses into note 1 and is removed; turned up again it comes
    back from note 1.
  - **ALGORITHM moves no note**: it is the step cursor (its layout's job).
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

- Press **B11** with nothing held: the **P-LOCK** mode. The page shows the notes, with a lock chip and the number of
  locks under each step as dots. (P1.5b: no padlocks anywhere; a lock is a chip: **filled = locked on this step,
  outlined = locked on some step of the track**.)
- **Hold a step**: the screen stays as it is. Turn a knob of the screen and it is **locked on the held steps**: KNOB
  1..4 = FREQ, DECAY, HARM, FDBK; PRESETS = the step's sound; SELECT = the page. The knob's picture shows the lock.
- **B11 with a step held** clears that step's locks (the sound stays: clear it in SOUND). **F1 + B11** undoes it.
- The black keys B1..B10 do nothing here. Only P-LOCK locks the sound.
- **Nothing held** in P-LOCK: KNOB 1..4 turn the track's own FREQ, DECAY, HARM, FDBK; PRESETS its sound; SELECT the
  page; ALGORITHM the cursor (choose the track with **FX + ALGORITHM**).

## How do I change the track's defaults?

- **Hold the mode's black key** (B1..B10): the screen shows the mode's picture marked **TRACK DEFAULT** (NOTE: the
  chord name and the keybed). While you hold it:
  - SELECT and the knobs edit the default, in the same layout as with a step held (NOTE: SELECT = the whole chord,
    KNOB 1..4 = notes 2..5, each alone); ALGORITHM turns the main default (in NOTE it moves the whole default chord);
  - **B11 (F#5) resets** that default to the track's starting value: NOTE back to a single C4 (clears a chord), VEL 100,
    LEN one step, MICRO 0 (and no strum), RATCH x1 flat, CHNC 100 %, COND always, TRIG NOTE, SOUND the track's
    starting sound. B11 is lit while there is something to reset;
  - the other black keys do nothing. Let go to get the page back.
- With **nothing held at all**, KNOB 1..4 still edit the defaults; the TRACK DEFAULT picture shows for a second.
  KNOB 4 does it quietly (no picture). ALGORITHM is the cursor now: hold the mode's key to turn the main default.
- Only the steps that don't have their own value follow a default.
- In SOUND, KNOB 1 (or ALGORITHM with the SOUND key held, or PRESETS) chooses the track's own sound.

## How do I work with pages and loops?

- A track has 8 pages of 16 steps. **SELECT** chooses the page you edit (F1 + B1..B8 too).
- **Page follow** (on: the page you see follows the playhead) is on the track settings' second page (SEL tapped
  twice, KNOB 3) and the global settings (SHIFT + SEL, KNOB 2). (ENV is MIX now: PAGE + W is gone.)
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

Undo and redo of everything (P1.5c):

- **SHIFT + F1 = UNDO, SHIFT + F2 = REDO**, eight levels. A big UNDO / REDO with an arrow shows a moment, and under it
  what was undone: a knob, a step edit, a paste, a clear (an X), a recorded take (a red ring), a sound (a load, the pool or
  MY presets), the dice (a randomise), the snapshot, two pages (double). A dash: nothing left.
- **One level is one gesture**: a knob turned until you pause about half a second, a step held and edited, a paste, a
  clear, a recorded take (all its notes), a preset loaded, a + / − POOL, a preset deleted or saved, a randomise, a
  snapshot recall, a double.
- Undo covers the **pattern you are on** (its steps, locks, track settings and track sounds), **the pool** and **MY
  presets**. It survives page and view changes and a pattern change (each level returns to its own pattern), but a
  **project load or a new project forgets it**.
- A new edit after undoing drops what you could have redone.
- Not undone: a pool sound's delete also clears that sound's locks in the other patterns; undo brings the sound and
  this pattern's locks back, the other patterns' stay cleared.

## How do I choose, mute and solo tracks?

- Hold **SEL**: the screen shows the eight tracks (after a quarter second, or at once when you press a key with it;
  a quick tap shows and does nothing).
  - **SEL + B1..B8**: select that track.
  - **SEL + tap W1..W8**: mute / unmute that track. **SEL + hold W1..W8**: solo / unsolo it. When any track is
    soloed, only soloed tracks play.
  - **SEL + B11**: unmute and unsolo every track.
  - Lights: W1..W8 lit = you hear it, off = muted (or another track is soloed), blinking = soloed; B11 lit while any
    track is muted or soloed.
- **FX held + ALGORITHM** (or FX held + W1..W8): choose the track, with the big track picture.
- The old tracks screen of eight clocks is gone: a track's length and speed are F3's, its voice limit and swing the
  track settings', mutes SEL's.

## How do I change a track's settings? (SEL tapped)

- **Tap SEL**: the selected track's settings in this pattern (its number and name in its colour at the top). **SEL
  again** (or SELECT) for the second page. **OCT− or HOME** closes it. **B1..B8** switch to another track's.
- Page 1: **KNOB 1 SCALE** (its twelve notes shown as dots, the root ringed), **KNOB 2 ROOT**, **KNOB 3 FOLLOW KEY**
  (the project's global key instead of its own), **KNOB 4 MIDI CH** (the channel whose notes play into a held step or
  the recording).
- Page 2: **KNOB 1 VOICES** (the voice limit, 1 = mono; the voices sounding white), **KNOB 2 SWING** on / off,
  **KNOB 3 PAGE FOLLOW**.
- SHIFT + ALGORITHM / PRESETS still set the scale / root from the step view.

## How do I mix? (ENV = MIX)

- **Tap ENV**: the mixer, eight faders, each with its pan dial on top, a level meter beside it, its number (a muted
  track crossed), its delay and reverb sends as two little bars under it. **B1..B8** (or SELECT, ALGORITHM) choose the
  track; its parts take the knobs' colours: **KNOB 1 LEVEL, KNOB 2 PAN, KNOB 3 DELAY, KNOB 4 REVERB**.
- **ENV again**: the **sends**, every track's chorus, delay and reverb as three bars; **KNOB 1 CHORUS, KNOB 2 DELAY,
  KNOB 3 REVERB**. ENV again goes back to the faders. OCT− or HOME closes it.
- Mute and solo stay on **SEL held** (it works over the mixer too).

## How do I change the effects? (FX tapped)

- **Tap FX**: the effects, ChoralRoot's (global: no sound or pattern changes them). It opens on the **OVERVIEW**.
  **FX again** (or SELECT) for the next page; **B1 OVERVIEW, B2 DELAY, B3 REVERB, B4 CHORUS, B5 MASTER, B6 ROUTING**
  (a key again: the effect's second page). OCT− or HOME closes it.
- **OVERVIEW** (P1.5b): three columns, CHORUS, DELAY, REVERB, each with the eight tracks' send bars in the tracks'
  colours (the selected track's bright, an unheard track grey). **W1..W8 choose the track** (lit: the selected one
  bright); **KNOB 1..3 its CHORUS, DELAY, REVERB sends**. FX held + ALGORITHM still chooses the track too.
- **ROUTING** (P1.5b): the chain drawn: the tracks' bus sending into CHORUS, DELAY and REVERB, their returns into the
  mix, the dry path along the foot, the master filter, out. ChoralRoot has no routing of its own, so these are new and
  all start at ChoralRoot's (none): **KNOB 1 DELAY > REVERB**, **KNOB 2 CHORUS > DELAY**, **KNOB 3 CHORUS > REVERB**
  (how much of one effect's return goes into the other's input, drawn as an arrow as thick as the amount), **KNOB 4
  FILTER** AFTER (the master filter on everything, ChoralRoot's) or BEFORE (on the dry mix only, the effects' returns
  past it). Kept with the global settings.

  | page | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
  | --- | --- | --- | --- | --- |
  | OVERVIEW | CHORUS send | DELAY send | REVERB send | |
  | DELAY | TIME (ms, or the note value when synced) | SPREAD (R's time against L's; synced: 1:1 3:4 2:3 1:2) | FEEDBACK | SEND |
  | DELAY 2 | SYNC (FREE / TEMPO) | MODE (STEREO / PING-PONG) | TONE | WOW |
  | REVERB | SIZE | DAMP | DECAY (s, INF) | SEND |
  | REVERB 2 | PRE-DELAY | MOD | LOW CUT | WIDTH |
  | CHORUS | RATE (Hz) | DEPTH | WIDTH | SEND |
  | MASTER | LOW CUT (OFF ..) | HIGH CUT (.. OFF) | SLOPE (6 / 12 dB) | FILTER ON / OFF |
  | ROUTING | DELAY > REVERB | CHORUS > DELAY | CHORUS > REVERB | FILTER AFTER / BEFORE |

  **SEND** is the selected track's send into that effect (its number at the top right; the same value as the
  mixer's).
- The drawings: the delay a ball bouncing between two walls at the delay time, its echoes fading by the feedback; the
  reverb a room as big as its size, its tail as long as its decay; the chorus three wavy lines; the master filter its
  curve.

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

## SHIFT's keys (P1.5c)

Hold **SHIFT**: only its shortcut keys light (the white keys go dark), and SHIFT's map draws the black keys under its
list, each with its picture.

| key | does |
| --- | --- |
| **B1** | store the **snapshot** of the pattern you are on ("SNAPSHOT stored") |
| **B3** | **recall** the snapshot onto it at once (undoable; dim: none stored) |
| **B4** | **copy** the pattern |
| **B5** | **paste** it onto the pattern you are on (undoable; dim: nothing copied) |
| **B6** | **tap tempo**: tap it in time; the tempo is the average of your last four taps (start again after 2 s) |
| **B7** | tap: the **metronome** (CLICK on / off). Hold B7 and turn any knob: its level (0..100, 60 at first) |
| **B9** | **double the track**: its looped pages copied after themselves, the loop twice as long ("FULL": no room) |
| **B11** | **clear the track**: press twice within two seconds ("CLEAR?", B11 blinks; then "CLEARED") |
| **F1 / F2** | **undo / redo** |

- The snapshot and the copied pattern stay until the unit is switched off; they are not saved with the project. One
  snapshot (the last stored), from whichever pattern; recalling puts it on the pattern you are on.
- The metronome clicks on every beat while playing, a higher click on the bar's first beat, mixed after the tracks
  (no effects, no track level). Tap tempo does nothing while an external clock runs (the tempo takeover shows).

## The OCT keys' lights

Where OCT− / OCT+ change an octave (NOTE with a step held, the keyboard) or SOUND's group of ten:

- At the centre (octave 4 in NOTE, octave 0 on the keyboard, the first ten sounds) neither blinks.
- One octave down, OCT− blinks slowly; two down, faster; three or more, faster still. OCT+ the same going up.
- Each change shows the octave (or the group) big for a second, with a keyboard sliding to it.

## How do I play the keyboard and record?

- **REC tapped**: the keybed becomes a keyboard for the selected track (REC lit). REC again: back to the steps.
- In a scale, keys outside the scale play the scale note just below.
- **The keys light the scale** (P1.5b): the root's keys at 80 %, the scale's other notes at 50 %, the rest dark, a
  key playing full.
- **SHIFT + ALGORITHM** the track's scale, **SHIFT + PRESETS** its root, while you play (stopped or playing).
- **F1 / F2** move the keyboard an octave (−2..+2).
- **SHIFT + REC**: the **record settings** (they stay open; OCT− or HOME back), kept with the project:
  - **KNOB 1 MODE**: LIVE or STEP, what holding REC arms;
  - **KNOB 2 QUANTIZE**: 0..100 % (5 % a click): how much of the played timing is pulled onto the step (0 % as
    played, 100 % exactly on the step);
  - **KNOB 3 TAKE**: **REPLACE** (a step you record into is cleared first) or **OVERDUB** (your notes join the step's;
    its velocity, timing and length stay);
  - **KNOB 4 EDIT KEYS** (P1.5b): **SCALE** or **FULL**, the keyboard in the sound editor (below).
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

## How do I change sounds? (P1.5t: presets, the pool, the track's sound)

Three things, one job each:

- **The track's sound** is what a track plays. It is saved with its pattern and edited in EDIT. It remembers the
  preset it came from; once you edit it, a **\*** follows its name ("LEAD 4 \*").
- **The Preset Library** (champagne) holds every preset, shared by every project:
  - **FACTORY**, built in and read only: INIT, the 16 FM TONE sounds and the six drums (KICK, SNARE, CLAP, GHOST,
    HAT, OPEN HAT);
  - **MY PRESETS**, the ones you saved or imported, up to 256.
  - Every preset carries **tags**, one or several of BASS, KEYS, PLUCK, PAD, LEAD, DRUMS, FX (P1.5v). A saved or
    imported preset starts with the one read from the sound; you can change any preset's tags, the factory ones' too
    (their sounds stay read only).
- **The Sound Pool** (lime) is the project's palette of up to 128 sounds, the ones its steps lock. **The pool holds
  references to presets** (P1.5w), not copies: a pool sound is the preset itself, as it is now, so replacing one of MY
  presets changes it in every project's pool. **A new project's pool starts empty** (the demo project keeps its
  sounds). The track's own sound is still a copy (loaded from a preset, edited in EDIT, saved with the pattern).

**PRESETS with nothing held** opens the **library browser**, in every view:

- It opens on the track's current preset. **Every detent moves one preset and the track plays it at once**: playing,
  you hear it in the pattern; stopped, a short note each turn.
- The screen is a rolodex: the chosen preset is the big cassette (its envelope in its first tag's colour, a dot for
  each tag between the reels, a ★ for one of MY presets, ↓ for an imported one; bottom left a **full lime tray** when
  it is in the pool, an **empty tray with +** when it is not), two neighbours above and below, a bar on the right for
  where you are in the list, and "7/31" at the top.
- **SELECT** jumps ten. **The white keys play the chosen preset** in the track's scale.
- **F2 = LOAD**: the preset replaces the track's sound and the browser closes ("LOADED").
- **F1 or HOME = CANCEL**: the track's own sound comes back exactly as it was, edits included ("BACK"). Leaving any
  other way (EDIT, choosing another track) is a cancel too: browsing never loses a sound.
- **B1..B8 filter by tag**: B1 ALL, B2 BASS, B3 KEYS, B4 PLUCK, B5 PAD, B6 LEAD, B7 DRUMS, B8 FX: every preset
  carrying that tag (the chosen filter's key lit, a chip in the head).
- **SHIFT + B2..B8 toggles that tag** on the chosen preset (P1.5v): its chip pops, filled when on, struck when off;
  holding SHIFT lights the chosen preset's tags on B2..B8. Inside the browser SHIFT means tags (no snapshots there).
- **B9 the source**: ALL PRESETS → MY PRESETS → POOL → ALL.
- **B10 = the pool, from any source** (P1.5v): **+ POOL** when the chosen preset is not in the project's pool (B10
  lit; it is copied in without loading it, the cassette shrinks into the tray; the pool full: the tray flashes
  orange), **− POOL** when it is (B10 breathing; in the POOL source always). If steps lock it, B10 first **arms**:
  a big **red X** crosses the cassette, it shakes, the head turns red with **OUT?**, and its users show at the foot
  (the tracks' boxes, ×N steps) **→ TRACK**; B10 again takes it out, and those steps go back to their track's sound.
- **B11 deletes** one of MY presets (B11 is lit only on them; factory presets cannot be deleted): the first press
  arms it (a big **red X**, the cassette shakes, the head red with **DELETE?**, B11 blinking); **B11 again** and it
  breaks up and fizzes away in red. DELETE? shows where it is used: the projects whose pools reference it (a little
  stack and their number) and the steps locking it (×N) **→ TRACK**. Deleting it takes its reference out of every
  pool (this project's at once, another project's as it loads) and sends those steps back to their track's sound.
  **Any other key** (or a turn) disarms OUT? and DELETE?.
- **SHIFT + F1** undoes a load, a + / − POOL, a delete, and a tag change on one of MY presets or a pool sound (a
  factory preset's tags you set back by hand).

**A step held** (or the step latch's selection) **+ PRESETS** chooses the step's sound from **the pool** (a sound
lock), in any mode: the first entry is **TRACK** (no lock: the step plays the track's sound), then the pool's sounds;
the step plays each one. **The picker stays up as long as the step is held** (or the latch's selection lasts; P1.5v:
no time-out), and letting go closes it, keeping the sound. An empty pool offers TRACK only (an empty tray, 0/128). In
**SOUND** mode the ten black keys are still ten sounds of the pool (the group of ten that holds the step's sound);
F1 / F2 show the ten before / after; B11 removes the sound lock. In **P-LOCK**, KNOB 1..4 turn the track's FREQ,
DECAY, HARM and FDBK.

**What happens to the steps** (P1.5w, shown on the screens):
- **Taking a sound out of the pool** (B10) sends the steps that lock it back to their track's sound (OUT? shows them
  going to TRACK).
- **Replacing one of MY presets** (SAVE, REPLACE) changes the preset itself: every pool that references it, in every
  project, and every step locking it, play the new version. **A track's own sound does not change**: it is a copy (a
  track loaded from that preset and not edited keeps the old sound, without a \*, until you load it again).
- **Deleting one of MY presets** takes its references out of every pool and sends the steps that locked them back to
  their track's sound (DELETE? shows how many projects and steps). Tracks keep their own copies. **SHIFT + F1** brings
  the preset back with its references and its locks (this project's, every pattern; another project's were never
  touched before it loads). An unused preset just goes.
- Factory presets can be in any pool; they are never deleted.

## How do I edit a sound? (the sound editor, EDIT)

- **EDIT** opens the sound editor for the selected track (EDIT lit). **EDIT again, OCT− or HOME** closes it. It opens
  over the step view from any screen.
- The black keys are its eight groups (P1.5l); **press a group's key again for its next page** (round), or turn
  **SELECT** through every page (in the table's order). B1..B8 are lit (the group you are in bright). It opens on the
  page you used last (the first time: RATIO).

  | key | group | pages |
  | --- | --- | --- |
  | **B1** | TRIG / KEYS | TRIG (A / B TRIG and RESET; ALGORITHM PHRT), KEYS (A, B1, B2 key scaling, VEL) |
  | **B2** | OPERATORS | RATIO (KNOB 1 RATIO C, KNOB 2 RATIO A, KNOB 3 **RATIO B**, KNOB 4 **MIX**; **ALGORITHM the algorithm**), TONE (HARM, DTUNE, FDBK, **MIX**: the same MIX as RATIO's, P1.5n), OFFSET (the four offsets) |
  | **B3** | OP ENVELOPES | ENV A, ENV B (attack, decay, end, level; ALGORITHM the delay) |
  | **B4** | FILTERS | FILTER (FREQ, RESO, TYPE, DEPTH), F ENV (attack, decay, sustain, release; ALGORITHM the delay), BASE (BASE, WIDTH, KTRK) |
  | **B5** | ENVELOPES | AMP (attack, decay, sustain, release), LEVEL (LEVEL, **DRIVE**) |
  | **B6 / B7 / B8** | LFO 1 / 2 / 3 | the LFO's page (SPEED, MULT, DEPTH, **DEST**), its WAVE page (WAVE, PHASE, TRIG, FADE) |

  Every value of the sound is on exactly one page.
- **The LFOs** (P1.5k). **B6, B7, B8** (F#4, G#4, A#4) are LFO 1, 2, 3: a tap shows that LFO's page, again its WAVE
  page, again back (lit: the LFO on screen bright, the others dim; in the editor's SCALE keyboard too, in FULL they
  play notes). On the LFO page **KNOB 4 is DEST**, the destination, through ChoralRoot's list (NONE, the LFO 1
  values, ALGO, the ratios, HARM .. FREQ, RESO .. DRIVE, PAN, the sends ...); its name is big in the picture with an
  arrow from the wave to it. ALGORITHM does nothing on the LFO pages.
- **Hold an LFO key and turn a knob** on any page to **map** it: the first click makes that value the LFO's
  destination, and turning sets the depth (up or down from 0; on from where it was if it already was the
  destination). While the key is held a picture shows "LFO n" over the value's name and the depth as a bar from the
  centre, and the value's column has a little wave. A value that cannot be a destination (TYPE, a WAVE ..) is
  refused: its name dim, a shake, nothing changed. Let go: you stay on the page (a tap without a turn goes to the LFO's
  page). **With a step held** (or the latch's selection) the destination and the depth are locks of that step.
- A value an LFO moves has the LFO's number in its colour by its name (1 cyan, 2 yellow, 3 violet), on every page.
  **RATIO B** (P1.5i) is one value, as in ChoralRoot and the Digitone: the pair B2 over B1, shown as a fraction. A
  click steps B2 (the fast hand) and carries into B1 after 16.00; **SHIFT + KNOB 3** steps B1. A step's lock of it
  locks both. **MIX** is drawn in the RATIO picture as the balance of the algorithm's two outputs: X's wires and Y's
  as bright and thick as their share (the other faint, never gone), and (P1.5j) a slider of its own under the
  drawing, centred and wide, X's end and Y's marked in their carriers' colours, the handle in KNOB 4's orange, a tick
  at the centre. The operators fade with the mix too: one only in X's chain (or only in Y's) dims and greys as its
  side goes down (never under about 30 %); one that feeds both stays full.
- The foot (P1.5j) is four fixed columns on every page: the name, a line under it for the knob turned last, and the
  value centred in one chip size, drawn only when locked (filled on this step, outlined on some step), all of it clear
  of the frame.
- Each page is one big drawing of that part of FM TONE (the operators wired by the algorithm, the envelopes, the
  filter's curve, the LFO's wave ..). Along the bottom, KNOB 1..4's values in their colours (the one you turned last
  underlined); ALGORITHM's value is in the pink badge top right.
- **Nothing held**: a turn changes the track's sound, live (what is sounding follows). The screen has a **dark orange
  frame** (the track's sound); a step held or selected, a **bright orange frame** (its locks). **SHIFT + a knob** moves it by the finest step (a hundredth of FREQ, HARM, the LFO speeds ..).
- **A finger on a step** (held, or the step latch's selection: LFO works in the editor): **every turn is a p-lock** on
  that step. The top says STEP n; **a value locked on that step is a filled chip** in its knob's colour (black text),
  and the envelopes show the track's curve dashed behind the step's. **Nothing held, a value that some step of the
  track locks is an outlined chip.** **B11** clears the held step's locks on this page (lit while it has some). The
  white keys stay the steps (tap, hold), as in the step view.
- **A step with a sound lock** (P1.5o): its values are the locked pool sound's, its p-locks on top (the track's sound
  < the step's sound lock < its p-locks); a turn p-locks the step, never the pool's sound; a new sound lock keeps the
  p-locks. **F2 held + a knob unlocks that value**: on the held or selected steps, with nothing held on every step of
  the track ("FREQ unlocked" a moment, in the knob's colour). The same in P-LOCK (F2 is then no layer there).
- The top line: the group's page dots first (always in the same place), then the page's name.
- **Live play in the editor** (P1.5b): **tap REC** with the editor open and its keyboard is on (EDIT opened from the
  keyboard keeps it on too). A strip under the top shows the keys, the roots brighter, the ones playing in the track's
  colour. REC again: the white keys are the steps again.
  - **SCALE** (the record settings' EDIT KEYS, the default): **the 16 white keys play the track's scale**, W1 the root
    at the keyboard's octave, then each note of the scale upwards (chromatic: 16 semitones). **The black keys stay the
    editor's pages.**
  - **FULL**: every key plays as the keyboard does; the pages are then on SELECT only.
  - **F1 / F2** the octave (both: every voice off). **SHIFT + ALGORITHM** the scale, **SHIFT + PRESETS** the root;
    SHIFT + KNOB 1..4 stay the fine step. The keys light the scale as the keyboard's do.
  - **EDIT** closes the editor back to the keyboard; HOME to the steps. Playing here is heard, not recorded.
- **PRESETS**: the library browser (nothing held) or the step's sound lock (a step held), as everywhere. F1 or F2
  in the browser brings you back to the editor.
- **Randomise** (P1.5c), drawn in neon lines over the page's picture:
  - **B9**: two dice roll for 0.6 s, then **this page's values** are new.
  - **Hold B10** 1.5 s: a champagne bottle fills with bubbles, the cork pops and it bursts: **the whole sound** is
    new. Let go before the pop: nothing.
  - B9 and B10 are dim, bright while rolling or filling. **SHIFT + F1 undoes** it. **With a step held** (or the
    latch's selection) the new values are that step's p-locks.
  - The values stay playable: ratios only from simple sets (C 1, 3..6; A 1, 3, 7, 11, 15; B1 and B2 1, 3..5), the
    algorithm 1..8, each operator's level at least 40 %, the filter open (FREQ 45..100 %, RESO up to 60 %, its
    envelope depth 40..75 %), the amp's sustain at least 40 %, short attacks, DRIVE up to 25 %, HARM and DTUNE near
    the middle, the offsets near centre; the LFOs gentle (depth up to a sixth of its range either way, no fade, their
    destinations and triggers kept). LEVEL, the filter TYPE and the TRIG / RESET / PHRT switches are never changed.
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
- **KNOB 2 PAGE FOLLOW**: on / off (the same as on the track settings' second page).

## The maps

- Holding **F1**, **F2**, **SEL** or **SEQ**, or opening **F3**, shows its map: the black keys drawn where they are
  on the panel, each with an icon or a number (a page, a track, clear, copy, undo, a note value), lit by its state.
  No sentences: what each does is in this guide.
- Holding **SHIFT** alone shows the controls SHIFT changes here (a knob, REC, F3, SEQ and a step, HOME, SEL), each
  with its job's icon and its value now (the scale, the root, the swing, the next view, the record MODE, the knob
  layout), and under them the black keys with SHIFT's shortcuts (P1.5c).
- A page change in the PIANO ROLL with page follow off: a crossed arrow at the top right (was FOLLOW OFF). The small
  dots over the columns mark the beats. The PIANO ROLL's top line no longer shows the tempo (turn it, or open F3).

## Saving (P1.5a)

- **Nothing to save by hand**: the project is written by itself, a moment (1.5 s) after your last change, while you
  play too (never in a way you could hear). Every pattern keeps its eight tracks' sounds as you edited them:
  switching patterns switches the sounds.
- Switching the unit off and on (in FM1VST: quitting and starting the Standalone) brings back the project you worked
  on, its pattern and the selected track, the effects and the knob layout.
- Eight projects, each with its 64 patterns and its own pool of 128 sounds; **MY PRESETS** (256) are yours, shared
  by every project.

## How do I keep a sound? (SAVE: SAVE PRESET)

- **SAVE** (from anywhere) opens **SAVE PRESET** for the selected track's sound, with a **name made from the sound**
  (a sparkle marks it), **selected** (a shaded band): PLUCK, PAD, BASS, KEYS, LEAD, KICK ... with BRIGHT, DARK, SOFT,
  WIDE ... Name it on the keybed (below), or keep it.
- **SAVE again** saves it as a **NEW** preset in MY PRESETS. When the sound came from one of MY presets, **NEW and
  REPLACE** both show and **F2 switches which one is chosen** (the chosen bright and filled, the other dim; F2 lit;
  NEW first): **SAVE does the chosen one** (P1.5v). NEW saves under a new name such as "DARK BASS 2" when the name is
  taken; REPLACE overwrites that preset. Only NEW possible: only NEW shows, F2 dark. Factory presets are never
  replaced. **HOME** cancels.
- Saved: it drops into MY PRESETS ("SAVED"), and the track now plays that preset, no longer edited.
- **SHIFT + F1** undoes a save.
- The old sounds page (banks, the LIBRARY / POOL tabs, F1 / F2 / B11 + W) is gone: the browser does all of it.

## How do I type a name? (P1.5u: the keybed is a typewriter)

One keyboard for every name: SAVE PRESET's and a project's (B10 on the project page). The screen draws the keybed
with a character on every key; the key you press glows there, and on the panel all 27 keys are lit (the one held
bright).

- **The black keys B1..B11 are A..K, the white keys W1..W16 are L..Z and SPACE** (reading order: the top row, then
  the bottom row). A key types its letter at the cursor and moves the cursor on (it flies up into the name). Upper
  case only; holding a key does not repeat; twelve characters (a key on a full name flashes the row orange and types
  nothing).
- **The suggested name starts selected**: SAVE right away keeps it; **the first key typed replaces it**.
- **SHIFT held**: the black keys are **0..9 and -**, the white keys **. , ' & ! ? # + / ( ) \* : _ ~** and SPACE (the
  screen's keybed changes with it). While a name is typed SHIFT is only this: SHIFT + a button does nothing else (no
  snapshots, undo, settings or views), but **SHIFT + F1 clears** the name.
- **PRESETS**: name ideas from the sound, a rolodex (BRIGHT PLUCK, WARM PING, DEEP TINE, CLEAR HARP ...): the one you
  stop on is the name, selected again. For a project: a few word pairs (NIGHT BUS, SLOW TAPE ...), its own name first.
- **ALGORITHM** moves the cursor (typing inserts there); **SELECT** turns the character under it (A..Z, 0..9, the
  signs, SPACE; at the end of the name it adds one), to fix one letter without looking for its key.
- **F1** deletes the character before the cursor (it drops away in orange; a selected name: all of it); **SHIFT +
  F1** clears.
- **SAVE** confirms (SAVE PRESET: saves NEW; F2 REPLACE where offered; a project: its name kept). **HOME** cancels
  (SAVE PRESET: closed; a project's name: back to the project page, the old name kept).

### Sounds from a computer (P1.5r)

- A page or tool can send FIZZ sounds over MIDI SysEx (the FIZZ web preview loads your Digitone `.dnsnd` presets
  this way): each goes into **MY PRESETS** (marked imported, its kind read; or the project's **POOL**), into the
  first free place or one it names, with its name. The screen shows it a moment: its cassette dropping in, its slot, **imported**. It is saved by
  itself like any change, and **SHIFT + F1 undoes it** (a batch sent together: one undo). The format is in
  `docs/SYSEX.md`.
- **In FM1VST** (the Standalone or the plugin, P1.5s): drop `.dnsnd` files on the window (one or many), or right-click
  the screen > **Import Digitone sounds…**. They go into MY PRESETS as above; a note over the screen says how many
  went in. **PRESETS** then finds them (B9: MY PRESETS).

## How do I manage projects? (SEQ tapped)

- **SEQ tapped** (SEQ held is still the patterns) shows the **project**: its number and name, its 64 patterns (a
  bubble for each one written, the playing one bright), the flash in use and the pool's count.
- **SELECT** or **SEQ again**: the **projects**, W1..W8 (the current one champagne). A W chooses one; **F2** loads it
  (an empty one starts a new project). **F1 + W** picks a project, **another W** copies it there (onto a project: that
  W turns orange, the same W again replaces it). **B11 + W** deletes one (the one you work on: a blank project in
  its place). **B10** names the current project (on the keybed, as above: SAVE keeps it, HOME the old one).
- **F1** or **HOME** leaves.

## Not built yet

- Undo of a pool delete's effect on the other patterns.
- Pattern chains.
