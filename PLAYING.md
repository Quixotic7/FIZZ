# Playing FIZZ (P1.5x)

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
| SEQ (printed) | **SEQ**: hold it for the patterns (they stay open on release; SEQ tapped or HOME closes them); tap it for TOOLS |
| SAVE (printed) | **SAVE**: SAVE PRESET, the selected track's sound into MY PRESETS |
| LFO (printed) | **LATCH**: the step latch, on / off |
| FX (printed) | **FX**: tap it for the effects; hold it to choose the track |
| ENV (printed) | **MIX**: the mixer |
| EDIT (printed) | **EDIT**: the sound editor |
| the black keys, left to right (F#3 .. D#5) | **B1 .. B10** |
| the rightmost black key (F#5) | **B11** |
| the white keys | **W1 .. W16**, the 16 steps of the page you are editing |

The on-screen panel in FM1VST and the web preview labels the buttons by these roles (P1.5r): FX, TRACKS (SEL), MIX
(ENV), LATCH (LFO), EDIT, SHIFT (GLO), HOME, SOUNDS (SAVE), FILL (ARP), PATTERNS (SEQ), PLAY, REC, F1, F2. P1.6l:
FIZZ also tells a page **what every knob, button and key does right now** (the SysEx LABELS, `docs/SYSEX.md`): hold
SHIFT and the labels become its shortcuts, hold a step and the black keys become that mode's values, open the sound
editor and the knobs carry the page's value names. P1.6m: a page can also **export your presets** (LIST and DUMP: MY PRESETS, the factory's, the
pool's) as a library to load back later.

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
- **Main pages and pop-ups (0.9.6).** The main pages are the step view (PIANO ROLL, STEPS, EDIT), the **mixer**
  (ENV, both pages), the **effects** (FX), the **sound editor** (EDIT) and the **MIDI FX** screen (ARP). **F1 never
  leaves them**: they close with **HOME** or their own key (EDIT, ARP; ENV and FX turn their pages), or by opening
  another page. **LFO** keeps them open too. The **pop-ups** keep F1 = back: F3, the track settings (SEL tapped),
  the record settings (SHIFT + REC), the global settings (SHIFT + SEL), the project pages (SHIFT + SAVE), the browser
  (F1 cancels) and the questions; SAVE PRESET and the name editor are the typewriter (F1 deletes, HOME cancels).
- **SHIFT + HOME** cycles **PIANO ROLL** (the mode's lane), **STEPS**, **EDIT** and **GEEK OUT** (SELECT turns its pages: METERS, NUMBERS, VOICES, SCOPE); **HOME** comes back. HOME keeps the view: whatever page you come back from, the step view is the one you chose, and GEEK OUT returns on its last page. STEPS shows all eight
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
  pop-up, the picture shows **the selected track's step as it plays** (the chord, the rider at that velocity, the bar,
  the flag, the ball, the cassette ..), while the head stays still (P1.5z): "TRACK DEFAULT · VEL" with the track's
  default value and colours, as when stopped. Each picture has its own
  "it plays" moment (the rider's kick, the chord's name squeezing in, the ball bouncing through the step). It changes
  when you hear the step, so a step with micro-timing changes late by that much, and a slow track (1/8 ..) changes
  at its pace; a ratchet changes it once. An empty step keeps the last one played, dimmed a little. A muted track is
  followed too. Stop, select a step with the latch (or the cursor), or hold a step and it shows that step again as
  before; a knob's pop-up shows as before and following comes back after it. **One rule everywhere (0.9.5):** the
  step latch on with nothing selected shows the track's values, still (no following; it followed before); the latch
  off and nothing held follows; a step held or selected shows that step. **The sound editor follows the same way**:
  playing, the latch off, nothing held, its values and pictures are the playing step's (its sound lock and its locks,
  marked), shown only (nothing is edited by following).
- **The knobs with no finger on a step** (nothing held, or the step latch's selection), in every view:
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
- **FX held + ALGORITHM** chooses the track: the track's number big in its colour with its name, the six below.
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
| NOTE | SELECT = the whole chord (P1.5o), KNOB 1..4 = notes 2..5 (each alone), ALGORITHM = the cursor | | note 5 |
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
- **With nothing selected** the latch stays on and the knobs edit the track's defaults. In **MODES** (the default) the black
  keys choose the mode. In **VALUES** (0.9.1) they set **the track's default** for the mode, as a held mode key + value
  does (NOTE: its notes; VEL: B10 is 127 ..; B11 resets it); **LFO + a black key** changes the mode. Tap a step
  to select again.
- **Double-tap a white key** (0.9.1, two taps within 200 ms, as everywhere in FIZZ): an empty step gets a trig, a step
  with one loses it. A single tap still only selects; the first tap of a double leaves no edit or undo level of its own.
- **SHIFT + ALGORITHM** the track's scale, **SHIFT + PRESETS** its root (0.9.1: with the latch on too, in VALUES and
  MODES, with or without a selection; no sound lock or browser).
- **F1, F2 and F3 work over the latch** (P1.5m) exactly as without it: F1 + W copies and pastes, F2 + W picks up and
  drops, F1 + B1..B8 the pages, F1 + B9 / B10 / B11 clear, copy, undo, both for F3; the selection stays as it was.
  **F1 / F2 tapped** (nothing with them) are the selection's octave in NOTE and its group of ten in SOUND, as for a
  held step (on the release, so that holding them is still the layer). Its octave (or ten) picture shows its full
  second, in EDIT too (P1.5n), and F1 / F2's map only comes up when they are held longer than a tap.
- **NOTE: SELECT moves all the selected steps' notes** together (P1.5o), as with a finger on a step, in every view;
  ALGORITHM stays the cursor.
- **LFO** again or **HOME** turns it off.
- **Hold LFO + a black key**: that mode, without touching the latch or the selection (a plain tap still toggles).
- **Two latch modes** (P1.5b; P1.5h). **Hold LFO alone** (400 ms, no other key): while you hold it, a screen shows the
  latch mode (two drawings: the keys as values / the keys as modes, the current one lit). **Turn SELECT** to switch it
  (it changes at once); let go of LFO and the screen closes, the mode as shown. Letting go without a turn changes
  nothing. A black key instead closes the screen and picks that hold mode. The mode is kept (a global setting) and
  used every time you turn the latch on.
  - **VALUES**: with the latch on, the black keys set values for the selected steps. LFO lit full.
  - **MODES** (the default): with the latch on, the black keys choose the hold mode (as with nothing held) and **the knobs set the
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
  - **ALGORITHM moves no note**: it is the step cursor.
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
  twice, KNOB 3) and the global settings (SHIFT + SEL, KNOB 1). (ENV is MIX now: PAGE + W is gone.)
- Hold **F1** to see the pages:
  - **F1 + tap B1..B8**: edit that page.
  - **F1 + double-tap B1..B8** (the second tap starting within 0.2 s of the first): loop that page alone and edit it.
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
- **Everywhere the white keys are steps (0.9.6)**: the mixer and its sends, the effects (the OVERVIEW's W1..W8 too,
  while F1 or F2 is held), the sound editor (its keyboard off) and the MIDI FX screen on STEPS. F1 + W and F2 + W
  work there exactly as here, and **F1 / F2 held show the same pictures** (the pages and the copy, ringed where it came
  from; the page and the hand). F1 + B1..B8 / B9 / B10 / B11 are the pages, clear, copy, undo there too (MIDI FX: B9
  and B11; its B1..B8 stay the slots' copy and move). The page stays open when you let go. In the sound editor with
  its keyboard on, F1 / F2 stay the octave. F1 + F2 together does nothing on these pages (F3 is the step view's).
- **A copy is the whole step**: its notes and values, its locks (sound, LEVEL, PAN and the sends), its sound and its
  MIDI FX.

Pages and tracks (hold **F1**):

- **F1 + tap B9**: clear the page. **F1 + hold B9** (600 ms): clear the whole track (every page; it is reset too:
  page 1 only, every page 16 steps; its sound and speed stay).
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

- Hold **SEL**: the screen shows the six tracks (after a quarter second, or at once when you press a key with it;
  a quick tap shows and does nothing).
  - **SEL + B1..B6**: select that track.
  - **SEL + tap W1..W6**: mute / unmute that track. **SEL + hold W1..W6**: solo / unsolo it. When any track is
    soloed, only soloed tracks play.
  - **SEL + B11**: unmute and unsolo every track.
  - Lights: W1..W6 lit = you hear it, off = muted (or another track is soloed), blinking = soloed; B11 lit while any
    track is muted or soloed.
- **FX held + ALGORITHM** (or FX held + W1..W6): choose the track, with the big track picture.
- The old tracks screen of eight clocks is gone: a track's length and speed are F3's, its voice limit and swing the
  track settings', mutes SEL's.

## How do I change a track's settings? (SEL tapped)

- **Tap SEL**: the selected track's settings in this pattern (its number, kind and name in its colour at the top).
  **SEL again** (or SELECT) for the next page (three, round). **OCT− or HOME** closes it. **B1..B6** switch to another track's.
- Page 1: **KNOB 1 SCALE** (its twelve notes shown as dots, the root ringed), **KNOB 2 ROOT**, **KNOB 3 FOLLOW KEY**
  (the project's global key instead of its own; **a new track follows it**, the global key starts CHROMATIC C;
  following, SCALE and ROOT show the global key's values, tagged GLOBAL; turning KNOB 1 or 2 gives the track its
  own key, starting from the global one), **KNOB 4 MIDI CH** (the channel that plays the track, 1..6 by
  default; channel 16 always plays the selected track).
- Page 2: **KNOB 1 VOICES** (the voice limit, 1 = mono; the voices sounding white), **KNOB 2 SWING** on / off,
  **KNOB 3 PAGE FOLLOW**, **KNOB 4 MIDI FX**.
- Page 3 (0.9.1): **KNOB 1 KIND**: BASS, KEYS, PLUCK, PAD, LEAD, DRUMS, FX (the presets' seven tags), each with its
  icon; choosing one **renames the track to the kind** unless you typed a name of your own. **KNOB 2 NAME**: a turn
  opens the name keyboard (TRACK NAME; PRESETS or KNOB 2 rolls names from the kind and namesizer: DUSTY BASS, PAD 3 ..); SAVE
  keeps it (a typed name stays when the kind changes; typing the kind's own name lets it follow again), HOME cancels;
  both come back to page 3.
- **The kind's icon** shows wherever the track's name does: the track picker (SEL held), the mixer (over the faders),
  the headers (the keyboard, the lanes, the track settings), the track takeovers (FX + ALGORITHM, P-LOCK's ALGORITHM).
- Names and kinds are **saved with the project** (its header). A project from before 0.9.1 keeps the names it had
  (the default kit's: DRUMS, BASS, KEYS, LEAD, PLUCK, PAD, BELL, NOISE), with the kinds DRUMS, BASS, KEYS, LEAD,
  PLUCK, PAD, KEYS, FX. **A new project** (0.9.5) names its tracks from their sounds: each track's kind from its
  preset's tags, its name from the namesizer and the sound's own ideas (not typed: a kind chosen renames it).
- **The white keys** are the page's steps (a tap makes or removes one); with the keyboard on (REC) they play notes.
- SHIFT + ALGORITHM / PRESETS still set the scale / root from the step view (a following track: the global key).

## How do I mix? (ENV = MIX)

- **Tap ENV**: the mixer, six faders, each with its pan dial on top, a level meter beside it, its number (a muted
  track crossed, a soloed one yellow), its delay and reverb sends as two little bars under it. **ALGORITHM** chooses
  the track; its parts take the knobs' colours: **KNOB 1 LEVEL, KNOB 2 PAN, KNOB 3 DELAY, KNOB 4 REVERB**.
- **Black keys, on both pages**: **tap B1..B6** to select the track, **hold** one to mute / unmute it (at the hold),
  **double-tap** to solo it; **B11** clears every mute and solo. **PRESETS** left mutes the selected track, right
  unmutes it. The keys' lights: playing, dim muted, blinking soloed. **SELECT** turns the page (faders / sends).
- **ENV again**: the **sends**, every track's chorus, delay and reverb as three bars; **KNOB 1 CHORUS, KNOB 2 DELAY,
  KNOB 3 REVERB**. ENV again goes back to the faders. **HOME** closes it (0.9.6: F1 no longer does: F1 / F2 + W copy
  and move steps here, see copy and paste).
- Mute and solo also stay on **SEL held** (it works over the mixer too).
- **Locks on a step** (0.9.1): **hold a W** (or have the latch's selection) and turn a knob: LEVEL, PAN, DELAY, REVERB
  (and CHORUS on the sends' page) become **locks of that step** (its voices play them; the track's values stay). The
  screen gets the step's magenta frame, the locked values as chips in the head and the faders at the step's values;
  let go and the chips are outlined (some step locks it). **F2 + the knob** clears the lock. A W tapped here does
  nothing (no trig).
- **What a knob writes, at a glance (0.9.6)**: the magenta frame comes as soon as a W goes down; the head's top right
  shows **how many steps** are held or selected (magenta, e.g. 3); the fader, pan dial or send bar a knob locks gets
  a **chip in the knob's colour** on it (the effects: on the OVERVIEW's bar, the SEND cell's chip). No frame and no
  count: the knobs move the track.
- **The step latch here (0.9.6)**: a selection made in the step view **is let go** when you open the mixer or the
  effects (the latch stays on), so LEVEL moves the track, not some step you can't see. Select steps **in** the mixer
  (W keys, the latch on) and they stay, into the effects and back. **LFO** in the mixer or the effects turns the latch
  on with nothing selected (the page stays open).

## How do I change the effects? (FX tapped)

- **Tap FX**: the effects, ChoralRoot's (global: no sound or pattern changes them). It opens on the **OVERVIEW**.
  **FX again** (or SELECT) for the next page; **B1 OVERVIEW, B2 DELAY, B3 REVERB, B4 CHORUS, B5 MASTER, B6 ROUTING**
  (a key again: the effect's second page). **HOME** closes it (0.9.6: F1 no longer does).
- **OVERVIEW** (P1.5b): three columns, CHORUS, DELAY, REVERB, each with the six tracks' send bars in the tracks'
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
- **Send locks** (0.9.1): with a step held (a W on any page but the OVERVIEW, whose W1..W8 choose the track) or the
  latch's selection, the OVERVIEW's KNOB 1..3 and each effect's **SEND** write a **lock on the step** instead of the
  track's send: the step's voices send that much (per trig, as Elektron's; a step without the lock keeps the track's).
  The magenta frame, the cell's value as a filled chip (outlined: some step of the track locks it); **F2 + the knob**
  clears it.
- The drawings: the delay a ball bouncing between two walls at the delay time, its echoes fading by the feedback; the
  reverb a room as big as its size, its tail as long as its decay; the chorus three wavy lines; the master filter its
  curve.

## How do I change patterns?

- Hold **SEQ**: the screen shows one bank's 16 patterns; let go and they stay open (SEQ lit) until a SEQ tap or HOME (the playing one white, the queued one blinking orange).
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
- **F3 + SELECT**: the tempo; **F3 + ALGORITHM**: the swing. Each turn shows the TAP ape dancing to it for a second,
  then the F3 screen again (which also shows both).
- The page whose range you set is the one you edit (F3 + W sets its end, W + W its start..end; KNOB 3 / 4 too).
- **EXT CLOCK**: while a MIDI clock (a DAW) is running the unit, the tempo is the clock's: the F3 screen says EXT CLOCK
  with its BPM, and a tempo turn shows EXT CLOCK instead of changing it. A MIDI STOP, or no clock for half a second,
  gives the tempo back to the knob.

## SHIFT's keys (P1.5c)

Hold **SHIFT**: only its shortcut keys light (the white keys go dark), and SHIFT's map draws the black keys under its
list, each with its picture. P1.5z: they work **in the sound editor too** (SHIFT shows no map there: only the
shortcuts act; SHIFT + a knob stays the editor's fine step).

| key | does |
| --- | --- |
| **B1** | store the **snapshot** of the pattern you are on ("SNAPSHOT stored") |
| **B3** | **recall** the snapshot onto it at once (undoable; dim: none stored) |
| **B4** | **copy** the pattern |
| **B5** | **paste** it onto the pattern you are on (undoable; dim: nothing copied) |
| **B6** | **tap tempo**: tap it in time; the tempo is the average of your last four taps (start again after 2 s). The **TAP** screen (0.9.2: **the TAP ape**) shows from the first tap until **2.8 s** after the last: the BPM top left once two taps make one, the taps as dots under the floor (the last four bright), a boombox, and a breakdancing gorilla. The first tap it bounces in pointing at the boombox; **every tap jumps it to its next pose** (step, side lean, knuckle crouch, side kick, handstand, one-arm air freeze, spin, lying freeze on the boombox, headstand, back freeze, then round again) with a hop, a flash, the old pose's ghost, sparkles and the boombox pumping; between taps it **grooves on the beat** (bobs, rocks, spins, the headstand turning). 2 s after the last tap the dots clear and it kneels, fist up. **180 BPM and up**: every tap a headstand flurry (rainbow rings, a sparkle shower). **70 BPM and under** (0.9.5): the moves in slow motion with motion trails, and it moonwalks. An external clock: the ape lounges on the boombox, dimmed, the tempo grey with a clock beside it (tapping does nothing) |
| **B7** | tap: the **metronome** (CLICK on / off). Hold B7 and turn any knob: its level (0..100, 60 at first) |
| **B8** | **PAGE FOLLOW** on / off (P1.5z), its picture (the pages, the playhead, the arrow; OFF crossed) and ON / OFF big. It is the same switch as the global settings' KNOB 1 and the track settings' PAGE FOLLOW: one for all tracks |
| **B9** | **double the track**: its looped pages copied after themselves, the loop twice as long ("FULL": no room) |
| **B11** | **clear the track**: press twice within two seconds ("CLEAR?", B11 blinks; then "CLEARED"): its steps and locks, and its pages reset (page 1 only, each 16 steps); its sound and speed stay |
| **F1 / F2** | **undo / redo** |
| **SELECT** | **the track** (P1.5z): a track a detent, round 1..6, the big track picture (as FX + ALGORITHM). In every view, the sound editor (it shows the new track's sound), the browser (the preview is put back first, then you browse the new track), the mix and the effects; not while typing a name (there SELECT turns the letter). It was the keyboard's swing: the swing is F3 + ALGORITHM |

- The snapshot and the copied pattern stay until the unit is switched off; they are not saved with the project. One
  snapshot (the last stored), from whichever pattern; recalling puts it on the pattern you are on.
- The metronome clicks on every beat while playing, a higher click on the bar's first beat, mixed after the tracks
  (no effects, no track level). Tap tempo does nothing while an external clock runs (the tempo takeover shows).

## The OCT keys' lights

Where OCT− / OCT+ change an octave (NOTE with a step held, the keyboard) or SOUND's group of ten:

- At the centre (octave 4 in NOTE, octave 0 on the keyboard, the first ten sounds) neither blinks.
- One octave down, OCT− blinks slowly; two down, faster; three or more, faster still. OCT+ the same going up.
- Each change shows the octave (or the group) big for a second, with a keyboard sliding to it.
- A pop-up lights OCT− (back) and OCT+ when it confirms something; the main pages (0.9.6) don't: there they are F1 / F2.

## How do I play the keyboard and record?

- **REC tapped**: the keybed becomes a keyboard for the selected track (REC lit). REC again: back to the steps. The
  keyboard drawn is the FM-1's own, its 27 keys from F (F3 to G5 at OCT 0).
- In a scale, keys outside the scale play the scale note just below.
- **The keys light the scale** (P1.5b): the root's keys at 80 %, the scale's other notes at 50 %, the rest dark, a
  key playing full.
- **SHIFT + ALGORITHM** the track's scale, **SHIFT + PRESETS** its root, while you play (stopped or playing).
- **F1 / F2** move the keyboard an octave (−2..+2).
- **SHIFT + REC**: the **record settings** (they stay open; OCT− or HOME back), kept with the project:
  - **KNOB 1 MODE**: LIVE or STEP, what holding REC arms;
  - **KNOB 2 QUANTIZE**: 0..100 % (5 % a click): how much of the played timing is pulled onto the step (0 % as
    played, 100 % exactly on the step);
  - **KNOB 3 TAKE**: **CAPTURE** (the default, below), **REPLACE** (a step you record into is cleared first) or
    **OVERDUB** (your notes join the step's; its velocity, timing and length stay). Step recording treats CAPTURE as
    REPLACE;
  - **KNOB 4 EDIT KEYS** (P1.5b): **SCALE** or **FULL**, the keyboard in the sound editor (below).
- **SHIFT + REC again** (or SELECT): the settings' **page 2**: **KNOB 1 AUTOMATION** ON (the default) / OFF: armed live
  recording also records knob moves (ON) or only notes (OFF).
- **Recording (0.9.5), in every view** (the keyboard, the step view, the sound editor, the MIDI FX screen): notes from
  the keys that play there (the keyboard, the editor's keys, the MIDI FX screen's KEYS) and from MIDI, knobs when
  AUTOMATION is ON.
  - **Stopped: hold REC** (about half a second): **armed**, and it stays armed when you let go (REC blinking).
    **The first note you play starts it**: playback and the recording at once, that note on the first step. **PLAY**
    starts it too (from the top, the first step perhaps a rest), and so does **REC + PLAY** (PLAY pressed while REC is
    held).
  - **CAPTURE** (TAKE, the default) is for quick takes: armed, nothing changes until the first note (or PLAY). Then the
    track is **cleared** (its steps and locks; page 1 only, every page 16 steps long; its speed stays, so a slower
    speed fits a longer take in 128 steps) and the take runs from the top, the loop growing a page at a time as you
    play. **PLAY or REC ends the take** (where you press it): the track becomes exactly as long as you played, to the
    nearest step (36 steps: pages 1 to 3, page 3 four steps long), and it loops from there while playback goes on.
    Past page 8 it goes round and the track stays 128 steps. **Playing, hold REC** (and let go) arms the next take:
    its first note starts the transport again from the top with it, so takes follow one another without stopping.
  - **Playing: hold REC and let go without turning a knob**: the recording starts and **latches** (REC lit; CAPTURE:
    armed, waiting for the first note).
  - **A tap of REC while armed or recording stops it**; playback goes on. Otherwise a tap is the view's own job, as
    before: the keyboard / the steps, the editor's keys, the MIDI FX screen's STEPS / KEYS. (So a tap never switches
    the view while recording: it stops the recording first; the next tap switches.)
  - **Playing: hold REC and turn knobs**: an **automation punch-in**, recording the knobs only while REC is held;
    letting go ends it, nothing latches. While a recording is latched, REC held + a knob records the knob and the
    recording goes on.
  - **PLAY while recording** stops the transport and the recording (REPLACE, OVERDUB); in a CAPTURE take it ends the
    take and playback goes on (PLAY again stops).
  - The page buttons: one is lit, the page you are on (EDIT, ENV, FX, SEL, SEQ, ARP, SAVE, REC); **HOME** is lit on the
    step view with no page open.
  - REC's light: **off** (the steps), **lit** (the keyboard page), **dim** (the editor's keys, the keyboard under MIDI FX), **blinking**
    (armed), **lit** (recording). While it records the screen is framed in red, a red dot on the band under a knob as
    it writes.
  - The record settings' **MODE**: **LIVE** as above (notes to the nearest step at the track's speed, micro-timing
    kept, the length from the hold; with REPLACE or OVERDUB an **empty track grows** a page as the playhead reaches
    it, the loop the pages reached when it ends); **STEP**: step recording, armed by the hold, playing or not: each
    note or chord into the cursor's step (in the keyboard F2 a rest, F1 a step back, PRESETS the cursor). SHIFT + REC
    opens the settings.
  - **Automation** (0.9.1): each knob move is a lock in the step the selected track's playhead is on (the sound
    editor's values, the mix's, the sends'; in the step view and the keyboard KNOB 1..4's quick page). A step with a
    note gets the lock; **an empty step becomes a ghost** (a LOCK trig). Several moves within one step keep the last;
    the next goes on from that value. Heard at once; the track's own sound does not change. **One undo level per
    take.**
- **FILL is SHIFT + ARP held** (0.9.1; it was ARP held): FILL while both are down (ARP blinks). ARP alone: MIDI FX,
  below.
- **MIDI in** (0.9.5): a channel plays **the tracks set to it** (the track settings' MIDI CH: tracks 1..6 on channels
  1..6 by default, as they always were; set by hand, that channel), and **channel 16 always plays the selected track**,
  notes and controllers (MOD WHEEL, BREATH, AFTERTOUCH, PITCH BEND into its modulation slots). On the selected track
  the notes also record, or set the notes of a held step (up to five). (Before 0.9.5 a channel always played the
  track with its number, whatever MIDI CH said; nothing in a project changes.)

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
  each tag between the reels, **three little bars right of the reels for its chorus, delay and reverb sends** (P1.5x:
  blue, green, white, as the SENDS page), a ★ for one of MY presets, ↓ for an imported one; bottom left a **full lime
  tray** when it is in the pool, an **empty tray with +** when it is not), two neighbours above and below, a bar on
  the right for where you are in the list, and "7/31" at the top.
- **A preset carries its mix** (P1.5x): its pan and its chorus / delay / reverb sends. Browsing plays them too (the
  track's pan and sends while you turn; its level stays); **LOAD** keeps them on the track; **CANCEL** gives the
  track its own pan and sends back exactly. The factory FM TONE presets carry ChoralRoot's sends (EP: chorus 30, delay
  20, reverb 40); INIT and the drums the default kit's reverb 20. A step's sound lock (the pool) is the sound only: the
  track's pan and sends stay.
- **SELECT** jumps ten. **The white keys play the chosen preset** in the track's scale.
- **F2 = LOAD**: the preset replaces the track's sound and the browser closes ("LOADED").
- **F1 or HOME = CANCEL**: the track's own sound comes back exactly as it was, edits included ("BACK"). Leaving any
  other way (EDIT, choosing another track) is a cancel too: browsing never loses a sound.
- **B1..B8 filter by tag**: B1 ALL, B2 BASS, B3 KEYS, B4 PLUCK, B5 PAD, B6 LEAD, B7 DRUMS, B8 FX: every preset
  carrying that tag (a chip in the head).
- **The black keys show the tags** (P1.5x, the owner): the **chosen filter's key is solid**; **B2..B8 breathe slowly
  for the tags the chosen preset has** (30 % to full, about 1.5 s); the kinds it does not have are dark (B1 too unless
  it is the filter). They follow the browser as it moves.
- **SHIFT + B2..B8 toggles that tag** on the chosen preset (P1.5v): its chip pops, filled when on, struck when off,
  and its key starts or stops breathing at once. Inside the browser SHIFT means tags (no snapshots there).
- **B9 the source**: ALL PRESETS → MY PRESETS → POOL → ALL.
- **B10 = the pool, from any source** (P1.5v): **+ POOL** when the chosen preset is not in the project's pool (B10
  lit; it is copied in without loading it, the cassette shrinks into the tray; the pool full: the tray flashes
  orange), **− POOL** when it is (B10 breathing; in the POOL source always): P1.5z, the cassette pops back out of the
  lime tray, a burst of lime bubbles fizzes away and the tray's + comes back. **The cassette's outline says it**: lime
  when the preset is in this project's pool, champagne when not, in ALL, MY PRESETS and the POOL alike. If steps lock it, B10 first **arms**:
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

- **EDIT** opens the sound editor for the selected track (EDIT lit). **EDIT again or HOME** closes it (0.9.6: F1 no
  longer does; with the keyboard off its W keys are the steps, and F1 / F2 + W copy and move them). It opens over the
  step view from any screen.
- The black keys are its eight groups (P1.5l); **press a group's key again for its next page** (round), or turn
  **SELECT** through every page (in the table's order). B1..B8 are lit (the group you are in bright). It opens on the
  page you used last (the first time: RATIO).

  | key | group | pages |
  | --- | --- | --- |
  | **B1** | PLAY / MOD | TRIG (A / B TRIG and RESET; ALGORITHM PHRT), KEYS (A, B1, B2 key scaling, VEL), **PORT** (PORT, TIME, SLOPE), **MOD** (the slot's SOURCE, DEST, DEPTH; **ALGORITHM the slot**) |
  | **B2** | OPERATORS | RATIO (KNOB 1 RATIO C, KNOB 2 RATIO A, KNOB 3 **RATIO B**, KNOB 4 **MIX**; **ALGORITHM the algorithm**), TONE (HARM, DTUNE, FDBK, **MIX**: the same MIX as RATIO's, P1.5n), OFFSET (the four offsets) |
  | **B3** | OP ENVELOPES | ENV A, ENV B (attack, decay, end, level; ALGORITHM the delay) |
  | **B4** | FILTERS | FILTER (FREQ, KNOB 2 and 3 by the machine, DEPTH; **ALGORITHM the MACHINE**), F ENV (attack, decay, sustain, release; ALGORITHM the delay), BASE (BASE, WIDTH, KTRK, **ROUTE**: the base-width before or after the filter) |
  | **B5** | AMP / FX | AMP (attack, decay, sustain, release; **ALGORITHM the MODE**, ADSR / AHD: in AHD attack, **HOLD**, decay), LEVEL (LEVEL, **DRIVE**, **ROUTE**), **CRUSH** (BITS, SRR, ROUTE) |
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
  click steps B2 (the fast hand) and carries into B1 after 16.00; **SHIFT + KNOB 3** steps B1. A fast spin
  accelerates as the other values do (0.9.5), so it sweeps B1 too in a few turns. A step's lock of it
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
  - **Hold B9** 1 s (0.9.1): the dice shake, harder and harder, then roll at 1 s and **this page's values** are new.
    Let go before: nothing changes.
  - **Hold B10** 1.5 s: a champagne bottle fills with bubbles, the cork pops and it bursts: **the whole sound** is
    new. Let go before the pop: nothing.
  - B9 and B10 are dim, bright while rolling or filling. **SHIFT + F1 undoes** it. **With a step held** (or the
    latch's selection) the new values are that step's p-locks.
  - The values stay playable: ratios only from simple sets (C 1, 3..6; A 1, 3, 7, 11, 15; B1 and B2 1, 3..5), the
    algorithm 1..8, each operator's level at least 40 %, the filter open (FREQ 45..100 %, RESO up to 60 %, its
    envelope depth 40..75 %), the amp's sustain at least 40 %, short attacks, DRIVE up to 25 %, HARM and DTUNE near
    the middle, the offsets near centre; the LFOs gentle (depth up to a sixth of its range either way, no fade, their
    destinations and triggers kept). LEVEL, the filter TYPE and the TRIG / RESET / PHRT switches are never changed.
- **DRIVE** (B5, LEVEL page, KNOB 2) is part of the sound now: ChoralRoot's DIST (low cut, the drive, a soft clip, a
  tone low-pass, make-up gain), in each voice, so a step with its own sound or a DRIVE lock has its own. A preset
  loads its drive (WOBBLE: 10; the others 0). FM TONE's LFOs can move it (destination DRIVE).
  - **ROUTE** (KNOB 3, 0.8.0): **POST** (as before) drives the voice after its filter; **PRE** drives it before the
    filter, so the filter tames the grit (the Digitone II's OD ROUTING). The picture's chain shows the order.
- **Portamento** (B1, PORT page, 0.8.0), in the sound, so a step can lock it:
  - **PORT**: OFF (as before), **ON** (every note glides from the track's last), **LEGATO** (only when the last note
    is still held: tied or overlapping steps, or a key still down).
  - **TIME** (0..127) and **SLOPE**: TIME glides in the same time whatever the interval; RATE at the same speed, so a
    wider leap takes longer.
  - **Chords** glide note by note: the lowest from the last chord's lowest, and so on up; a note more than the last
    chord had glides from the nearest. A single note after a chord glides from its top note.
  - **The 303 slide**: on a MONO track (limit 1), hold the step to slide into and turn PORT to ON (a lock); its note
    glides in from the one before. A TIE glides too when PORT is on.
- **The filter machines** (B4, FILTER page, ALGORITHM = MACHINE, 0.8.1), the Digitone II's six, in its menu's order
  (0.8.3-e4.1):
  - **MULTI**: KNOB 3 TYPE morphs low-pass (0) to band-pass (64) to high-pass (127);
  - **LOWPASS 4** (24 dB, the old LP24), **LEGACY** (the filter as before: KNOB 3 TYPE OFF / LP12 / HP12; turned to, LP12);
  - **COMB-** / **COMB+**: FREQ is the comb's pitch (43 Hz and up), KNOB 2 **FDBK** how much it rings, KNOB 3 **LPF**
    how bright the ring stays (COMB+ every harmonic, string-like; COMB- the odd ones, hollow);
  - **EQ**: a bell at FREQ, KNOB 2 **GAIN** (-18 .. +18 dB), KNOB 3 **Q** (wide .. narrow).
  - **Hold B4** (the FILTERS key, 400 ms; 0.9.1): a popup of the six machines, each with its curve, the current one
    lit. While held, **ALGORITHM or any knob** turns the choice (a detent a machine), or **B1..B6** picks one. Let go:
    closed, the choice kept (through the dip). A tap of B4 still turns the group's pages; ALGORITHM on the FILTER page
    still turns the machine, and shows the same popup for a second after its last detent (0.9.5).
  - The picture draws each machine's response; the filter envelope, DEPTH, key tracking and the base-width work with
    every machine. Changing the machine on a playing note takes a short dip (no click). BASE's KNOB 4 **ROUTE** puts
    the base-width filter before (as before) or after the machine.
- **CRUSH** (B5, CRUSH page, 0.8.2), the Digitone II's bit and sample-rate reduction, in each voice (a step can lock
  it): **BITS** (OFF, then 16 bits down to 1: the steps get taller), **SRR** (OFF, then the sample rate divided by up
  to 64: each value held longer, aliasing on purpose), **ROUTE** after the filter (as the Digitone's default) or
  before it. The picture is a sine as stairs, as wide as SRR's hold and as tall as BITS' step, and the FILTER / CRUSH
  chain in ROUTE's order. Both OFF: the sound as before.
- **The modulation slots** (B1, MOD page, 0.8.3), the Digitone II's modulation and two more: eight slots, each a
  **SOURCE**, a **DEST** (the LFOs' list) and a **DEPTH** (-127 .. +127). The sources: **VEL** (the note's velocity),
  **KEY** (the note: above middle C up, below it down), **MW** (the mod wheel, CC 1), **AT** (aftertouch, channel pressure),
  **PB** (pitch bend), **BR** (breath, CC 2), **RAND** (a new value each note) and **F ENV** (the filter envelope's
  level). MW, AT, PB and BR come from MIDI on the track's channel (channel 1 is track 1), in FM1VST from the host or
  a keyboard. The page draws the eight as rows: the source's glyph, its destination, the depth as a bar from the
  centre; ALGORITHM picks the slot, KNOB 1 .. 3 set it.
  - **Hold B1 and turn a knob** on any page to map, as with an LFO key: the first click puts the source on that value
    (the slot with the same source and value, else the first empty one) and turning sets the depth. While B1 is held
    **W1 .. W8 choose the source** (VEL KEY MW AT PB BR RAND F ENV, the chosen one lit; VEL at first) and a picture
    shows the source's glyph over the value's name with the depth. A value that cannot be a destination, or all eight
    slots taken: refused (a shake). **With a step held** the slot is locks of that step. A tap of B1 still steps
    through its pages.
  - A value a slot moves has the source's glyph by its name, on every page.
  - The LFOs and the slots can also move **HOLD**, **PORT TIME**, the filter's **SHAPE** and **GAIN**, **BITS** and
    **SRR** (0.8.3).
- **AHD** (B5, AMP page, ALGORITHM = MODE, 0.8.0): **attack, hold, decay** to silence, the Digitone II's other amp
  envelope. HOLD is a time (the gate does nothing: one-shot, for drums and plucks), or **NOTE** (127): held while the
  key is down, then DEC is the release. DEC INF holds for ever. Switching the MODE while a note plays carries on from
  where the envelope is. In AHD, SUS and REL are not used (KNOB 4 is empty).

## TOOLS: change a whole page, track or pattern (SEQ tapped, 0.12.0)

- **Tap SEQ**: TOOLS opens on the selected track (SHIFT + SELECT still changes it). **SEQ** again or **HOME** closes it.
- **B1..B8**: ROTATE (the steps round the loop), REVERSE, RANDOM (new steps: density, notes, velocity, chance),
  EUCLID (N hits spread evenly), GRIDS (a drum part from a map: X, Y, density, chaos), HUMANIZE (timing and velocity
  jitter), QUANTIZE (the timing pulled back to the grid), LENGTH (the loop x2 / /2). **B9 STRETCH** squeezes or
  spreads the track in time (x2..x8, /2../8, or onto 1..8 pages). **B10 CLIP**: COPY, PASTE or CLEAR (KNOB 1 picks,
  OCT+ does it; CLEAR on a track or the pattern wants OCT+ twice). **PRESETS** steps through them all.
- **B11 or ALGORITHM**: the scope, SELECTED STEPS, PAGE, TRACK or PATTERN. **W1..W16** put steps in or out of the
  selection (the scope turns SELECTED); **SELECT** the page shown. **KNOB 1..4** the tool's values.
- Every change plays at once; the white keys and the screen show the result, the steps it removes as ghosts.
  **OCT+ keeps it** (one undo level), **OCT− puts it back**; **SHIFT + F1** undoes a kept change later.

## MIDI FX: the arpeggiator and friends (ARP, 0.9.0; 0.9.1's gestures)

- **Five groups of eight slots**, kept with the project. A slot holds one of ten effects or nothing; a track's notes
  pass the slots in order (an empty or bypassed slot is skipped), then sound on the track. **A track plays through
  one group or none** (OFF, the default); **two tracks may share a group** (their notes meet in one arp, each still
  sounding on its own track). Notes the group makes count against the track's voices.
- **The keys and a MIDI keyboard** play through the track's group too (stopped as well: the arp runs on the tempo);
  recording keeps what you **played**, before the effects.
- **ARP tapped** (0.9.1; it was SHIFT + ARP) opens the **MIDI FX** screen of the selected track's group: its number in a hexagon, the tracks on it
  as small squares, the eight slots as a rail (notes run along it as they pass), the selected effect's picture, its
  values at the foot. **ARP again** or **HOME** close it (0.9.6: F1 no longer does, not even a tap). The step latch's
  selection stays while it is open (0.9.3). **ARP's light breathes** while the screen is open (0.9.4): low (5 .. 25 %)
  when the group's arp is off, high (60 .. 100 %) when it is on; outside the screen it is steady when the arp is on.
  - **B1 .. B8** select slot 1 .. 8; the selected one again: its next page (the dots, on the release). **SELECT** turns
    the page too.
  - **Hold a slot key and turn any knob** (0.9.1): that slot moves along the rail, a place a detent, the others
    making room. 0.9.3: held past a tap, the tile **lifts with arrows either side** (it can move); turning only
    **previews** the move (the chain plays as it was) and **it drops into its new place when you let go of the key**
    (one undo level). A plain tap still selects. (0.9.3 fixes a move that stayed stuck on the knob after the release.)
  - **PRESETS** chooses the slot's effect (NONE, then the ten, a rolodex while turning); it is heard at once.
  - **KNOB 1 .. 4** the page's values; **KNOB 4 is CHANCE** on every effect's first page (the effect acts on that share
    of the notes; the others pass untouched).
  - **ALGORITHM** puts the track on a group (OFF, 1 .. 5); the screen follows, **keeping the slot number** (0.9.1).
    While you turn, the five groups' rails stay up and the track's square glides on from where it is (no restart a
    detent). **SHIFT + SELECT** goes to the next track (its group).
  - **B10** bypasses the slot (and back; saved). **B11** removes its effect: press it twice (the first arms a red X).
    **SHIFT + F1** undoes.
  - **F1 / F2 as in the sequencer** (0.9.1): **F1 + B**: the first slot pressed is copied, every other slot pressed
    while F1 is held gets it pasted (the copy stays for F2). **F2 + B**: pick up and drop, as F2 + W on steps (the first
    press cuts into the hand, an empty slot gets a paste, a full one alternates; the slot flies to where it lands).
    0.9.3: **held, they show what they do**: F1 the copy glyph, then the copied effect (its slot marked on the rail)
    and where the next B puts it; F2 the hand, empty (the next B picks up) or with the effect in it (the next B drops).
    0.9.6, on **STEPS**: **F1 + W / F2 + W** copy and move **steps** as in the step view (whole, every lock), the
    picture then the step as a card (its notes, a LOCK step a lock); **F1 + B9** clears the page, **F1 + B11** undoes.
  - **REC** (0.9.3) switches the white keys between **KEYS** (as before: the track through the group; REC lit) and
    **STEPS** (REC dark): the track's 16 steps of the page you edit, lit as in the step view. 0.9.4: this is the same
    switch as the main view's REC keyboard: opened from the step view the screen starts on STEPS, from the keyboard on
    KEYS, and when you leave it you are in the keyboard if it was on KEYS, in the step view if on STEPS. REC held arms
    recording there as in the keyboard (the screen stays, on KEYS); SHIFT + REC opens the record settings. **Hold steps and turn
    ALGORITHM**: their MIDI FX lock (TRACK, OFF, group 1 .. 5), the same as TRIG's KNOB 2, with its picture and the
    step's frame; several held: all of them; the step latch's selection counts too. Nothing held: ALGORITHM is the
    track's group again. On the arp's lanes (pages 3, 4) the white keys stay the lane's steps.
  - **The white keys** play the track through the group (its scale, as the sound editor's keyboard).
- **ARP held** (400 ms alone, anywhere): the track's **MIDI FX off** (as the group OFF; a popup says MIDI FX OFF), held
  again: **back on** with the group it had (MIDI FX ON). **SEQ tapped** opens **TOOLS** (SEQ again or HOME closes it, SEQ
  lit while open). **ARP is lit** while the selected track's MIDI FX is on (a group, not turned off), while its page is
  open and while FILL (SHIFT + ARP held) is down; off otherwise.
- **The stopped preview** (0.9.3): a step held while stopped plays **through its MIDI FX group** (its own, else the
  track's): you hear the arp, the chord, the harmony; letting go ends every note (a HOLD arp's too).
- **A step's own group**: hold the step in **TRIG** (B8): **KNOB 2** is its MIDI FX (TRACK: the track's, the default;
  OFF; group 1 .. 5), or **B4 TRACK, B5 OFF, B6 .. B10 groups 1 .. 5**. With nothing held KNOB 2 is the track's group.
  The track settings' page 2, **KNOB 4**, is the track's group too.
- **The effects** (page by page, KNOB 1 .. 4):
  - **ARP**: PATTERN (UP, DOWN, UP DN, DN UP, U+D, D+U, CONV, DIV, CON DIV, HI UP, HI UPDN, LO UP, LO UPDN, RAND,
    RAND OTH, SHUFFLE, AS PLAYED), RATE (1/1 .. 1/64 of a bar), RANGE (octaves), CHANCE; MODE (OFF, ON while held,
    1-ST once through even after release, ONCE at most once, HOLD latched: the next chord after every key is up
    replaces it), GATE, OCT DIST (the octaves' step: +12, -12 goes down, +7 climbs in fifths), QUANT (when a run
    starts). Its picture (0.9.5) draws **the PATTERN's real walk** over RANGE octaves, on the notes in play (or C E G,
    dim, with none): DOWN falls, UP DN rises then falls, CONV comes in from the outside, the random ones scatter and
    change, AS PLAYED follows the order played; the MOD lane's rests are gaps in it, the TRANSPOSE lane's steps
    offsets; changing PATTERN morphs it; each step a clean line, the playing one's dot running along it; at the left
    a small keyboard on the same scale, its keys lit for the notes the arp plays from. Page 3 the **MOD lane** and page 4
    the **TRANSPOSE lane**: the white keys are their 16 steps (a tap
    selects one; **SHIFT + W** a random value), KNOB 1 the step's value, KNOB 4 the lane's length (B1 .. B8 stay the
    slots). The MOD lane: x as played, . rest, - tie (the note before lasts longer), R repeat, < lowest -oct, > highest
    +oct, " power chord, # every held note, 1 .. 6 the n-th note played. RESET (page 3, KNOB 2): NORM, NOTE (a new
    note restarts it), MPAT / TPAT (restarts as the MOD / TRANSPOSE lane wraps). Every arp note at the first held
    note's velocity. A new arp starts ON.
  - **CHANCE**: lets a note through or drops it.
  - **REPEAT**: RATE, HITS, FADE (the velocity over the hits), CHANCE; MODE (ON while held, 1-ST, ONCE, HOLD), GATE,
    QUANT.
  - **HARM**: NOTE 2 .. 7 added at fixed intervals (duplicates skipped), ORIG (the played note on / off).
  - **TRANSPOSE**: SEMI, OCT (a note outside the range is dropped). **SCALER**: SCALE, ROOT, KEY (OWN, or TRACK: the
    track's key): each note down onto the scale.
  - **RANDOM**: PITCH +-, VEL +-, LENGTH + (a sequencer note's end later, up to that much), CHANCE; OCTAVE +-, DELAY
    (a random wait, up to 1/64 .. 1/1 of a bar).
  - **CHORD**: the note as a chord's root: CHORD (its shape), NOTES (more than the shape: doubled an octave up), INVERT,
    CHANCE; SPREAD, VOICING (CLOSE, DROP 2, DROP 3, OPEN). The played velocity is kept.
  - **MONO**: one note at a time per track (the newest cuts the one before).
  - **SELECTOR**: each note to ONE of the next LENGTH slots (RND, UP, DOWN); the others are skipped for it.

## The scale (for now)

- **SHIFT + ALGORITHM** sets the track's scale (chromatic, major, minor, the modes, harmonic and melodic minor,
  pentatonics, blues, whole tone). **SHIFT + PRESETS** sets its root. Its picture (the scale's name, its keys) shows
  over the PIANO ROLL, STEPS and EDIT alike (P1.5z), and **stays while SHIFT is held** (it fades once you let go).
- A track that **follows the global key** (FOLLOW KEY, the default): SHIFT + ALGORITHM / PRESETS set the
  **project's global key** (every following track plays it); the picture says **GLOBAL** under the name.
- This is temporary until the track settings page exists.

## The global settings (SHIFT + SEL)

They stay open (OCT− or HOME back). For now:

- **KNOB 1 PAGE FOLLOW**: on / off (the same as on the track settings' second page).

## The maps

- Holding **F1**, **F2**, **SEL** or **SEQ**, or opening **F3**, shows its map: the black keys drawn where they are
  on the panel, each with an icon or a number (a page, a track, clear, copy, undo, a note value), lit by its state.
  No sentences: what each does is in this guide.
- Holding **SHIFT** alone shows the controls SHIFT changes here (a knob, REC, F3, SEQ and a step, HOME, SEL), each
  with its job's icon and its value now (the scale, the root, the swing, the next view, the record MODE; SEL
  the settings), and under them the black keys with SHIFT's shortcuts (P1.5c).
- A page change in the PIANO ROLL with page follow off: a crossed arrow at the top right (was FOLLOW OFF). The small
  dots over the columns mark the beats. The PIANO ROLL's top line no longer shows the tempo (turn it, or open F3).

## Saving (P1.5a)

- **Nothing to save by hand**: the project is written by itself, a moment (1.5 s) after your last change, while you
  play too (never in a way you could hear). Every pattern keeps its six tracks' sounds as you edited them:
  switching patterns switches the sounds.
- Switching the unit off and on (in FM1VST: quitting and starting the Standalone) brings back the project you worked
  on, its pattern and the selected track, the effects and the latch mode.
- Eight projects, each with its 64 patterns and its own pool of 128 sounds; **MY PRESETS** (256) are yours, shared
  by every project.

## Keys held while switching on (the FM-1, P2.14)

- **SAVE held**: FIZZ's **safe mode**. FIZZ starts without its store: nothing is read from it or written to it, so a
  store that keeps FIZZ from starting can be wiped or restored over USB (`firmware/tools/fizz_xdata.py wipe`,
  `restore`). Switch off and on again to leave it.
- **REC held**: the FM-1's **update mode** (fm1-base's, the same key as m-bootloader's): for installing a firmware.
- **OCT- and OCT+ held**: the panel's own setup (fm1-base's).

A back-up of your projects and MY PRESETS: `fizz_xdata.py backup FILE` (and `restore FILE`, stopped).

## How do I keep a sound? (SAVE: SAVE PRESET)

- **SAVE** (from anywhere) opens **SAVE PRESET** for the selected track's sound, with a **suggested name** (a sparkle
  marks it), **selected** (a shaded band): since P1.5y a pair from the owner's **namesizer** (SPICY WALRUS, RAPID
  CAR, COSMIC TREE ...), a new one each time. Name it on the keybed (below), or keep it.
- **SAVE again** saves it as a **NEW** preset in MY PRESETS. When the sound came from one of MY presets, **NEW and
  REPLACE** both show and **F2 switches which one is chosen** (the chosen bright and filled, the other dim; F2 lit;
  NEW first): **SAVE does the chosen one** (P1.5v). NEW saves under a new name such as "DARK BASS 2" when the name is
  taken; REPLACE overwrites that preset. Only NEW possible: only NEW shows, F2 dark. Factory presets are never
  replaced. **HOME** cancels. The track's **pan and sends are saved with the sound** (P1.5x), NEW and REPLACE alike.
- Saved: it drops into MY PRESETS ("SAVED"), and the track now plays that preset, no longer edited.
- **SHIFT + F1** undoes a save.
- The old sounds page (banks, the LIBRARY / POOL tabs, F1 / F2 / B11 + W) is gone: the browser does all of it.

## How do I type a name? (P1.5u: the keybed is a typewriter)

One keyboard for every name: SAVE PRESET's, a project's (B10 on the project page) and a track's (0.9.1: the track
settings' page 3, KNOB 2). The screen draws the keybed
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
- **PRESETS (or KNOB 4, or KNOB 2) right rolls a new name, every detent, for ever** (P1.5z): mostly namesizer pairs, now and
  then a made-up word (phonic nonsense like NIRE, cosmic words like FM-GX) and, about one time in five, one of the
  sound's own names (BRIGHT PLUCK, WARM PING ...). A neon die tumbles in beside each new one and lands. The name
  shown is the name, selected again (the sparkle stays).
- **Left walks back** through the last ten names you saw (dots under the name show where you are among them; no
  die); it stops at the oldest. Right then goes forward through them first, and past the newest rolls new ones
  again. A name you typed or edited is kept among them. A project's name works the same (its own name the oldest).
- **ALGORITHM** moves the cursor (typing inserts there); **SELECT** or **KNOB 1** (P1.5z) turns the character under it (A..Z, 0..9, the
  signs, SPACE; at the end of the name it adds one), to fix one letter without looking for its key.
- **F1** deletes the character before the cursor (it drops away in orange; a selected name: all of it); **SHIFT +
  F1** clears.
- **SAVE** confirms (SAVE PRESET: saves NEW; F2 REPLACE where offered; a project: its name kept). **HOME** cancels
  (SAVE PRESET: closed; a project's name: back to the project page, the old name kept).

### Sounds from a computer (P1.5r)

- A page or tool can send FIZZ sounds over MIDI SysEx (the FIZZ web preview loads your Digitone `.dnsnd` presets
  this way): each goes into **MY PRESETS** (marked imported, its kind read or the tags the page sends; or the
  project's **POOL**), into the first free place or one it names, with its name, its **pan and sends** (P1.5x). The screen shows it a moment: its cassette dropping in, its slot, **imported**. It is saved by
  itself like any change, and **SHIFT + F1 undoes it** (a batch sent together: one undo). The format is in
  `docs/SYSEX.md`.
- **In FM1VST** (the Standalone or the plugin, P1.5s): drop `.dnsnd` (Digitone) or **`.dn2pst` (Digitone II, P1.5x)**
  files on the window (one or many), or right-click the screen > **Import Digitone sounds…**. They go into MY PRESETS
  as above, with their pan and sends (and a Digitone II preset's tags: BASS, CHORD as KEYS, PAD, LEAD, the drums as
  DRUMS, SOUND FX as FX); a note over the screen says how many went in. A Digitone II sound of another machine
  (WAVETONE, FM DRUM, SWARMER, MIDI) is refused, the note says why. **PRESETS** then finds them (B9: MY PRESETS).
- **0.8.0:** a Digitone II sound keeps its **AHD** amp envelope (HOLD too), its **portamento** (ON and its time) and
  its overdrive **before the filter** (its OD ROUTING; how the Digitone II stores that is still a guess), as FIZZ's
  own (the SysEx's STORE6).
- **0.8.3:** a Digitone II sound's **modulation** (velocity, key, mod wheel, aftertouch, pitch bend, breath, four
  destinations each) comes into the eight slots, the first eight that FIZZ has a destination for; how the Digitone
  II stores the depth is still a guess, the note over the screen says so.

## How do I manage projects? (SHIFT + SAVE)

- **SHIFT + SAVE** (it was SEQ tapped; SEQ tapped is TOOLS now) shows the **project**: its number and name, its 64 patterns (a
  bubble for each one written, the playing one bright), the flash in use and the pool's count.
- **SELECT** or **SHIFT + SAVE again**: the **projects**, W1..W8 (the current one champagne). A W chooses one; **F2** loads it
  (an empty one starts a new project). **F1 + W** picks a project, **another W** copies it there (onto a project: that
  W turns orange, the same W again replaces it). **B11 + W** deletes one (the one you work on: a blank project in
  its place). **B10** names the current project (on the keybed, as above: SAVE keeps it, HOME the old one).
- **F1** or **HOME** leaves.

## Not built yet

- Undo of a pool delete's effect on the other patterns.
- Pattern chains.
