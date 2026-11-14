# FIZZ release notes

The firmware releases for the M-VAVE FM-1, newest first, written for testers. Install them from
[the install page](https://quixotic7.github.io/FIZZ/install/) and tell us how it goes on
[Discord](https://discord.gg/emf6DmZ9V). `firmware/release.py` turns the "Unreleased" section into the next release:
its text becomes the release's notes on the install page and on GitHub.

## Unreleased

<!-- The next release's notes, for testers, in plain language. firmware/release.py moves them. -->

- **B11's steps / keys no longer flip when you change pages**: turning PRESETS on the keyboard page and then pressing EDIT, ENV, FX, SEQ, SAVE, SHIFT + REC or holding PLAY no longer drops you back on the keyboard page under that page (with the white keys playing notes); and the MIDI FX screen, opened with the step keyboard on, no longer shows B11's label (and SHIFT + white keys) the wrong way round.
- **Waking from the FIZZ screen does nothing else**: the button or knob that wakes the unit only wakes it (PLAY no longer starts playback, a knob no longer changes its value). A button held while it wakes is no SHIFT, no HOME hold, no SEQ map, no REC arm until you let go and press it again.
- The PIANO ROLL in COND, TRIG, FUNC, SLIDE and SOUND is now a 4 x 4 grid of big boxes, a row a beat: conditions as
  rings and names, padlocks and ties, function icons with arrows to where each jump lands, slides as a rising curve,
  and sound locks as their number and name in the sound's own colour.
- **SAVE is a page button**: a click opens the SAVE page you used last, and clicks turn between **SAVE PRESET** and
  **PROJECT**. In the sound editor with a step selected, a click still opens SAVE PRESET for that step's sound.
- **Hold SAVE** (half a second) to save the project now: a big green tick says **PROJECT SAVED** (playing, SAVING
  first, about half a second). **F2 on the PROJECT page** does the same. The project still saves by itself too.
- **F2 saves** on SAVE PRESET (SAVE no longer does), and **F2 keeps** a project's or a track's name. NEW or REPLACE
  is chosen with **PRESETS**.

## 0.12.0-beta.18 (2026-11-13)

One design language across the pages: ALGORITHM selects steps everywhere, the selection, SOUND's bank and the octave stay put as you move around, and MIDI FX, TOOLS and FUNC are redone to match.

- **SHIFT's button blinks while it is stuck** (after a quick click).
- **SOUND, LIVE**: the screen names the sound you play, not only its number.
- **NOTE, LIVE on the step keyboard**: the screen shows the notes you play (and their chord), a second after too.
- **A selected step keeps the scale's roots lit** on the keys and on the screen's keyboard.
- **A stuck SHIFT + HOME, FX, ENV or EDIT** ends SHIFT and goes there; if you're already there, nothing else changes (no next view or page).
- **The sound editor: ALGORITHM selects steps** as on the Home views (fully left: LIVE; LFO the lock). Each page's fifth value is on **PRESETS** now (the filter machine, AMP's mode, the delays, VOICE's priority, MOD's slot); a page without one: PRESETS turns KNOB 1's value. **RATIO**: ALGO on KNOB 1, the ratios C, A, B on KNOB 2..4, MIX on PRESETS.
- **The keyboard page, B11's STEPS**: the white keys are the steps (the row up top, with the playhead), and the keybed gives way to an orange grid of B1..B10, the keys that play, the scale's roots marked (on their LEDs too).
- **SAVE in the sound editor with a step selected** saves that step's sound (its sound lock, or the track's, with its locks): a magenta STEP chip says so; KNOB 4 switches between the TRACK's sound and the STEP's. The track's sound stays as it is: program a kit's pieces on steps and save each one out.
- **B11 is one state everywhere**: the Home views' step selection opens REC's keyboard page in STEPS (white keys the steps, B1..B10 the notes), their step keyboard opens it on the full keyboard, and B11 on the keyboard page changes it for the Home views too. In STEPS, **B1..B10 play the scale from its root** (B1 the root; a 7-note scale: B8 the next one).
- **One look for a selected step**: the sound editor has no frame for the track's sound and the Home views' step frame for a step; its head is the page's name alone (white for a step) with the page's steps under it; the keys' strip is gone (the keys' LEDs and B11's breath show it). The keyboard page shows the same frame when a step is selected.
- **The MIDI FX screen's knobs, as everywhere**: ALGORITHM selects steps (fully left: LIVE), PRESETS the track's group (with steps selected: their MIDI FX). **Hold a slot's key** for the effects' list: KNOB 1 changes the slot's effect, KNOB 2 moves it along the rail (dropped when you let go); the foot shows EFFECT and MOVE.
- **The sound editor's LFO keys (B6..B8) and B1**: only a quick click changes the page; holding one (the mapping) and letting go never takes you to its page.
- **The selection stays put across every mode**: steps selected with ALGORITHM or the step lock's W keys stay selected through HOME, FX, MIX, SOUND, REC (the keyboard page), MIDI FX, TOOLS, the settings and the browser. Only you end it: ALGORITHM fully left, LFO, B11 + F1 (TOOLS: a B11 click). HOME on the Home view with a selection now changes the view as a plain click does.
- **SOUND's bank is remembered, globally**: selecting a step (with a sound lock or not, any lock mode) or another hold mode and back never moves the bank; only OCT- / OCT+ in SOUND (or turning a step's sound into another bank) changes it.
- **One octave everywhere**: NOTE's entry octave (OCT on a selected step) and the keyboard's octave are now the same setting, so changing it in one place carries to the other. Its range is now four octaves each way (the keyboard's was two).
- **FUNC redrawn**: the page's 16 steps as a row (the step filled, trigs as dots) with the function drawn as the playhead's route over it: RESTART back to step 1, REVERSE / FORWARD the flow turning, FLIP both ways, JUMP an arc to its target (another page: that page's row), LEAP the hops, the random ones dashed arcs under a die. The function's name sits centred over the picture, no longer in the corner.
- **ALGORITHM's step flash in every hold mode**: scrolling steps flashes the step's white key once, the value modes too; the selected step's value lights after.
- **TOOLS in the same language as the other pages**: SELECT changes the tool, ALGORITHM selects steps (one at a time; fully left: nothing selected), PRESETS the target (SELECTED STEPS, PAGE, TRACK, PATTERN). The W keys still add and remove steps; **tap a step twice** for that step alone. Selecting steps never changes the target: it stays what you chose. Pages: B11 + B1..B8. **A B11 click clears the selection** (it never turns the white keys into a keyboard in TOOLS). After an F1 / F2 paste, the steps you pasted are selected when you let go. Hold OCT- / OCT+ for the step view's copy / cut and paste; a quick click of OCT+ keeps the change, **OCT- undoes** (a change not yet kept is reverted; otherwise the last change is undone, as SHIFT + F1).
- **MIDI FX follows the selected step**: with a step selected (ALGORITHM), the screen shows and edits that step's group (OFF shows OFF; PRESETS changes it), with the step frame, the step keyboard on too. The black keys stay the slots. **Hold a step's W key** (with a step selected or not) and the black keys set the held steps' group: B1 OFF, B2..B6 groups 1..5, B11 TRACK, lit to show the step's; the other black keys do nothing then. A step no longer stays stuck held when ALGORITHM turns while its key is down. The group popup shows after a moment held (a quick click shows none; a black-key shortcut at once) and closes the moment you let go of the W keys, with a step selected too. **The MIDI FX screen follows B11's one global state**: the step keyboard on opens it on KEYS, the step selection on STEPS; a B11 click there changes it everywhere, and ARP closes it back to the page it opened over.
- **Same rules on every page**: a short white-key press waits a moment (250 ms) on the mixer and the effects too, so a tap no longer locks a value at once; turning the step lock on brings back your last selection on every page; opening a page keeps the lock and clears any popup; clearing says one message, and every "press twice" confirm gives up after 2 seconds.
- **One look for the keys**: one rule lights the keys and one blink rate everywhere; every step row shows the playhead.

## 0.12.0-beta.17 (2026-11-13)

A speed fix for beta.16 on the USB package.

- **Fixed: the Home views and the mixer drew about 10 ms slower** on the USB package in beta.16 (STEPS 60 ms a frame instead of 49), so the screen felt laggy while playing. They draw as fast as in beta.15 again.

## 0.12.0-beta.16 (2026-11-13)

The step keyboard: the white keys play each hold mode's values, and a lot of step-entry polish.

- **The step keyboard** (a B11 click turns the white keys into keys; B11 breathes while it is on). In each hold mode the sixteen white keys are that mode's values, and each press plays the step with it: **VEL** 8 .. 127, **LEN** 1/8 .. 32 steps and INF (the step plays at that length), **MICRO** with W8 on the grid (always a little brighter), **RATCHET** x1 .. x8 then x1 .. x8 rising, **CHANCE** 6 .. 100 %, **COND** A on W1..W8 and B on W9..W16 (1:1 .. 8:8), **TRIG, SLIDE, FUNC** their black keys' values on W1..W10, and **SOUND**: W1..W10 the bank's ten sounds, W11..W16 the pads' velocity (20 .. 127, kept for every sound you play), F1 / F2 clicked change the bank. The key LEDs show the step's value as you move through the steps.
- **With a step selected** the screen shows the page's 16 steps where the heading was (the mode's value stays top right), and the lock's frame goes round the keys too. **Held keys + B11** put that value back to the track's.
- **LIVE (nothing selected)**: the screen shows the value of the key you hold, a second after you let go. **Held keys + B11** make it the track's own (NOTE: the held notes become the track's notes; a band says so). Playing, the key LEDs follow the step that plays.
- **LIVE while playing**: in VEL, LEN, MICRO, RATCHET, CHANCE and COND, holding a white key overrides that value on every step the track plays while you hold it. With REC on it is written into every step it passes (an empty step becomes a ghost step, no trig and no notes). SOUND keys record their sound and velocity at the playhead: finger drumming.
- **Hear the value you set.** Stopped, changing a value on the selected steps plays the step once; ratchets play their hits.
- **LFO is the lock on the step pages, the effects and the sound editor**, with the step keyboard on too: off goes LIVE, on brings back the selection you had (the first time: step 1). **ALGORITHM selects steps in the effects too** (fully left: LIVE; ROUTING's DRY / WET is KNOB 2). ALGORITHM's step flashes its white key; turning SELECT to another page flashes that page's black key, everywhere.
- **F1 / F2 on the step keyboard**: a quick click changes the octave (SOUND: the bank), held they are F1's and F2's step functions, with the white keys as steps. Their screens, and B11's pages screen, show the page's steps along the foot.
- **Recording**: armed while stopped it is always a CAPTURE; armed while playing it is the record settings' TAKE, **OVERDUB** (the default) or **REPLACE**.
- **SHIFT sticks**: a quick click keeps it on (click again to end it); a hold never sticks, and any button ends it after its shortcut.
- **OVERVIEW**: the orange keys no longer cover the tracks, and the selected track's row is tinted in its colour.

## 0.12.0-beta.15 (2026-11-11)

B11 as a second shift key, a new OVERVIEW of all six tracks, a new SHIFT screen, and chords into steps from every page.

- **OVERVIEW, a new view** (**SEL tapped**, at once from anywhere; SEL is lit while it is open): all six tracks' steps on the page at once, each with its own playhead; B1..B6 select, mute (hold) and solo (double tap) tracks as in the mixer, **B9 clears every mute and solo** (lit when there is one), the white keys edit the selected track's steps. **Its knobs with nothing selected: KNOB 1 the track's rate, KNOB 2 its volume, KNOB 3 / 4 the start and end of its loop on the page**, each value shown in a band at the top; the steps outside a track's start..end still show, grey and dim, and a page outside the selected track's loop is greyed and crossed out. B11 works there as on every page; holding a track key + B11 clears every mute and solo, as in the mixer. SEL again or HOME goes back.
- **OVERVIEW's black keys say what they do.** With step entry off (OVERVIEW opened, ALGORITHM fully left, LFO off, B11 + OCT−) they pick the **tracks**, and the track icons light up as a row of keys. With step entry on (LFO, ALGORITHM, B11 + white keys) they are **always the hold modes** (NOTE, VEL ..) whatever LFO's VALUES / MODES setting: the icons go quiet, the mode's name shows at the top, and each change of mode flashes a band with the ten keys and the mode's name. A finger on a step makes them that step's values, as everywhere. **A muted track shows an M, a soloed one an S** in place of its icon.
- **HOME clicks cycle the views** (PIANO ROLL, STEPS, EDIT, GEEK OUT; no SHIFT needed). From any other page the first click brings you back to the view you were on. **Hold HOME for the global settings** (they were SHIFT + SEL), with a new page **HOME**: choose the Home view and **LOCK** it (then HOME always goes there and never cycles).
- **Hold SEL (or SHIFT + SEL) for the track settings** (SEL tapped is OVERVIEW now; the mute and solo map that held SEL showed is gone: mute and solo on the mixer or OVERVIEW). **The track settings and the global settings open on the page you left.** **SHIFT + PLAY opens F3**, as holding PLAY does; **holding PLAY on the keyboard page opens F3** too.
- **A new SHIFT screen, the same on every page** (the keyboard page and the sound editor included): the selected track first (its number in its colour, its kind and name big, the six tracks under SELECT, which turns them while you hold SHIFT), then the root (P) and scale (A), PLAY with the tempo and swing, F1 undo and F2 redo, REC's record MODE, SEL the track settings, and SHIFT's black-key shortcuts at the foot. SHIFT's shortcuts and lights now work on the keyboard page too (its keys no longer play while SHIFT is held).
- **The orange keys write chords into selected steps** on every page that has them (HOME, the sound editor, the effects, the mixer, TOOLS), as on the keyboard page: select steps (B11 + white keys, ALGORITHM, the latch), play keys together, and that chord replaces the steps' notes (an empty step becomes a note step; one undo per chord). The orange keys light the selected step's notes.
- **The steps / keys switch is the same on every page** and stays as you set it: changing pages, views or pressing HOME no longer switches the white keys back to the steps. Only a B11 click turns it.
- **The orange keys.** When the white keys play notes (a B11 click, below) they are drawn as boxy orange keys with a keyboard sign, the scale's roots filled and the keys you play lit, so they never look like the steps: along the foot on HOME, the mixer, SENDS and TOOLS, under the top in the sound editor.
- **Paste after changing pages.** A step cut or copied stays in F2's hand when you let go: change page, hold F2 and tap an empty step to paste it. F1's first step still always copies.
- **ENGINE keeps the other sound.** Switching a track from FM TONE to MIDI and back gives it its own FM TONE sound again (not INIT), and MIDI → FM TONE → MIDI its MIDI sound, until the FM-1 is switched off or a project is loaded.
- **A third ENGINE: FM+MIDI.** The track plays its FM TONE sound and sends its sound editor's moves as CCs on its MIDI OUT channel: knob turns at once, p-locks as their steps play (back to the track's value after), REC + AUTOMATION turns too.
- **MIDI FX routing map.** Turning ALGORITHM on the MIDI FX screen now shows every track wired to its group, each group's eight slots beside it; the selected track's wire moves as you turn.
- **New mode keys.** B8 is FUNC, B9 is TRIG and SLIDE together (from another mode it opens the one you used last; tapped again it switches to the other), B10 is SOUND. SLIDE's B1 OFF, B2 ON, B3 LEGATO stay; the old "B10 again" cycle is gone. Holding B10 (SOUND) + B11 now resets the track's sound to its starting one.
- **B11, a second shift key** (as in Elasticat), **the same on every page where steps can be locked**: the step view, the keyboard page, the sound editor, the effects, the mixer, MIDI FX and TOOLS. **Hold B11**: the pages and their steps over the page (gone the moment you let go); + B1..B8 the pages (as F1 + B1..B8: tap, double tap, hold); + white keys select steps (keys pressed together are one selection, the next press after all are up starts a new one) and turn the step lock on, so the page's knobs lock onto them; **+ F1** leaves the step lock (LIVE); **+ F2** puts the selected steps' value for the current mode back to the track's (in the sound editor, the mixer and the effects: their locks of the page's parameters; MIDI FX: the track's group). **A quick click of B11** switches the white keys between the steps and the orange keys, everywhere alike (it no longer turns the step lock on). With the step lock on B11 stays the hold key. GLO stays SHIFT; B11 keeps its jobs with a step or a mode key held, after F1 (undo), with SHIFT (clear), and with a track key held on the mixer (every mute and solo off, on OVERVIEW too, where B9 does it as well).
- **REC opens the keyboard page** from everywhere; **a REC tap on the keyboard page records** (as holding REC does: the record settings' MODE and TAKE). HOME leaves the keyboard page. The steps / keys switch is B11's click now.
- **Play chords into steps from the keyboard.** On the keyboard page B11 is the hold key too (it no longer plays F#5): hold B11 + white keys to select steps (the pages show while you hold it), let go, and every chord you play is written into the selected steps (outlined over the sound's name), one undo level a chord. **The selected step's notes show**: their keys light and the screen's keyboard fills them in magenta, as ALGORITHM moves through the steps too. ALGORITHM moves the selected step; fully left, LFO or B11 + F1 is LIVE again: the keys just play. A B11 click makes the white keys the steps there (the keyboard stays on the screen). **SELECT** turns the page (no longer the tempo). The keyboard is drawn white again. Recording still takes the notes when REC is armed.
- **F3: SELECT turns the page, PRESETS the tempo** (it was SELECT); ALGORITHM is still the swing.
- **TOOLS: B6 is HUMANIZE and QUANTIZE** (again: the other), **B7 the scope** (it was B11; ALGORITHM still turns it), so B11 is the hold key there too. **MIDI FX: B9 removes** a slot's effect (twice; it was B11).
- **ALGORITHM selects the page's steps only.** The step cursor now stays on the page you edit (steps 1..16; SELECT turns the page), and turned fully left past step 1 it lets go: nothing selected, the knobs edit the track.
- **Clear a track with F2 + B9, twice** (B9 blinks after the first press, CLEAR? shows; the second press clears, like SHIFT + B11). Holding F1 + B9 no longer clears.
- **PLAY closes F3.** With F3 open (PLAY held), a PLAY tap closes it without starting or stopping.
- **The keyboard page's lights with a step selected**: the scale stays dim and the step's notes blink; each key lights for its own note only (F# no longer lights G too).
- **Fixed: GEEK OUT's scope page** could run out of drawing memory and drop its picture.

## 0.12.0-beta.14 (2026-11-08)

FIZZ for the newer FM-1s.

- **FM-1s with the M-VAVE logo on the front** (sold since autumn 2026) have a different flash chip. FIZZ now saves to it and installs updates on it; before, such a unit kept your work only until it was switched off. The flash driver also waits for the slower chip to finish each write (the fix from felucca-bsp and fm1-base). Nothing changes on older units.

## 0.12.0-beta.13 (2026-11-02)

A new look for the Manager, live mix from the Manager, Digitone .syx imports, and one-step installs from The Firmware Hub.

- **Install from The Firmware Hub.** Each package now carries FIZZ's data, so one install is all it takes, from FIZZ's own page, the hub or any FM-1 installer: no separate "writing FIZZ's data" step. The packages are built and signed on GitHub by the hub's shared workflow ("Built from source on GitHub"). The Bluetooth package is the hub's; the USB package is on FIZZ's install page as before. An install cut off by the cable carries on when you plug in and press Install again.
- **A new project starts on A01**, the drum beat on its own. The band is on A02, the polymeter on A03, as before.
- **The Manager's new look (FIZZ NEON).** Every page is redrawn, flat and clean. EDIT shows every parameter on one panel while it fits, with a keyboard you can show or hide; **drag a modulation** from an LFO or the envelope onto a knob. POOL's cards show icons for their tags. LIBRARY has search and **SELECT**: Shift / Ctrl-click many presets, then tag, add to or remove from the pool, or delete them together.
- **PROJECTS:** drag a pattern onto another pattern or project to copy it, hold ALT to move it. DELETE and CUT work on the keyboard. A drop into a project whose pool is full is refused.
- **THIS BROWSER:** the Manager runs a FIZZ in your browser with its own store, so you can make presets and projects without the FM-1. A preset from the browser asks before it is loaded onto the FM-1.
- **PLAY:** a virtual FM-1 panel in the Manager. Shift-click a key to hold it.
- **Live mix.** PAN, the delay, reverb and chorus sends and LEVEL change on the FM-1 as you move them in the Manager.
- **Digitone .syx sound dumps** import in the Manager (and the iOS app), with the newer `.dnsnd` files too.
- **Fixed:** a MOD slot aimed at LFO 1's speed, depth or shape now takes effect. A renamed project, or a sound added to the pool, now shows in the Manager at once.

## 0.12.0-beta.12 (2026-10-27)

- **Record FIZZ over USB.** The FM-1 is now also a USB audio input: pick **FIZZ FM-1** as the input in your DAW (or Audio MIDI Setup) and record FIZZ's master output straight over the cable, 44.1 kHz stereo, at the MASTER knob's level. MIDI and the Manager work as before. (macOS may need the FM-1 unplugged and plugged in once to see the new input.)
- **A Bluetooth package.** The install page now offers two packages: **USB** (the default, as before) and **Bluetooth**, which adds Bluetooth MIDI and keeps everything else, USB audio included. It has room for less: **4 projects, 128 MY presets** and a shorter undo (a whole dense pattern can't be copied or undone in one go). Connect it on a Mac in Audio MIDI Setup (Window → Show MIDI Studio → Bluetooth → Connect, no pairing code), on iPhone or iPad from a MIDI app's Bluetooth settings. SETTINGS (GLO + SEL) has a page 2 (SELECT): **BLUETOOTH** ON / OFF and whether a computer is connected.
- **Over Bluetooth** all of FIZZ's MIDI works (notes, CCs, the remote, clock, MIDI out to the connected computer) and so does the Manager, except RESTORE and the PROJECTS tab's WRITE: those, installing and FIZZ's data need the USB cable, and the Manager says so.
- **Switching packages erases the FM-1's projects and presets.** Back up in the Manager first; the install page checks which package the FM-1 runs, warns before a switch and reminds you to restore after. A USB backup restored onto the Bluetooth package keeps 4 projects and 128 presets: the Manager lets you choose which and lists what is left out; a Bluetooth backup onto the USB package loses nothing. A project moves between the two as a `.fizzprj` (KEEP, then LOAD INTO); when MY PRESETS is full its sounds come along as the project's own copies, so it plays the same.

## 0.12.0-beta.11 (2026-10-27)

Full MIDI: CCs for the sound, a remote control for the whole panel, MIDI out, and MIDI tracks.

- **Channels.** Channels 1 … 6 play tracks 1 … 6 as before (a track's MIDI CH now runs 1 … 15). **Channel 15 plays the selected track** (it was 16). **Channel 16 is the remote** and plays no track.
- **CCs set the sound.** A CC on a track's channel (or 15 for the selected track) turns that track's sound as the editor's knobs do, and the notes sounding follow. One fixed map for every sound: the common numbers where they exist (7 LEVEL, 10 PAN, 91 / 93 / 94 the sends, 74 FREQ, 71 RESO, 73 / 75 / 72 the amp's attack, decay and release, 5 / 65 portamento) and the rest in the editor's page order; the website's new MIDI page lists it. With REC and AUTOMATION on, CCs to the selected track record as locks.
- **The remote (channel 16).** Any controller plays the FM-1's panel: notes 53 … 79 are the 27 keys, CC 20 … 33 the 14 buttons, CC 40 … 46 the 7 encoders (relative: 65 one right, 63 one left), CC 47 … 50 KNOB 1 … 4 for faders and pots, CC 7 MASTER.
- **MIDI out over USB.** Track settings page 3 has **MIDI OUT** (KNOB 4): the track's notes go out on that channel as it plays them, after its MIDI FX and arp, and what you play on it; it still sounds. SETTINGS (GLO + SEL) has **CLOCK OUT** (the clock and START / STOP while FIZZ plays on its own clock) and **CC OUT** (a sound knob's CC goes out as you turn it, so a controller's rings follow), both on at first.
- **MIDI tracks.** Track settings page 3, **ENGINE** (KNOB 3): FM TONE or **MIDI**. A MIDI track makes no sound and sends its notes on its MIDI OUT channel. Its EDIT is **seven pages of four CCs** (SELECT or B1 … B7): four bars, sent as you turn them; a slot never turned is a dashed bar and sends nothing; **SHIFT + a knob** picks the slot's CC number. **Hold a step and turn** for a lock, sent as the step plays (the track's value comes back at its next note); the latch, B11 and REC with AUTOMATION work as in the sound editor. A MIDI track's values save with its sound, so a preset can be a MIDI preset; ENGINE back to FM TONE starts from INIT.
- **The Manager knows MIDI sounds.** A MIDI preset or track opens in EDIT as its seven pages of four bars (drag or scroll a bar, click its CC number to change it, double-click to make it untouched), carries a magenta MIDI chip in LIBRARY, POOL and IMPORT, and keeps being a MIDI sound when it is saved, exported as `.fizzsnd` / `.fizzlib` or imported. Digitone exports leave MIDI sounds out. With TRACK on, an edit reaches the FM-1's track and its changed slots go out as CCs on the track's MIDI OUT, as a knob turn sends them. The browser's FIZZ is rebuilt with MIDI.
- **The website's MIDI page** (`midi.html`, linked from the player, the guide and the install page): the channels, the whole CC map, the remote's map, MIDI out, MIDI tracks and an implementation chart. The browser player's "MIDI keyboard plays the selected track" now sends on channel 15.
- **Geek Out TRACKS draws all six tracks.** In beta.10 the scope's code overwrote the page, so its lines stayed flat or showed scraps of track 1; each sounding track now draws its own wave, and chords no longer clip.

## 0.12.0-beta.10 (2026-10-27)

The Manager's PROJECTS tab, a backup played in the browser, a per-track scope, and a much steadier USB link.

- **The Manager has a PROJECTS tab.** The FM-1's eight project slots on the left: LOAD, RENAME, COPY TO, MOVE TO, KEEP (a copy in this browser) and DELETE. A project's 64 patterns on the right: pick one, a range (SHIFT) or a few (CMD), then COPY, PASTE, CLEAR, or save them as a `.fizzpat` file and bring them back. Patterns can be pasted into another project. Projects kept in the browser save as `.fizzprj` files and load into any slot. Pattern edits wait in a yellow bar (UNDO, DISCARD) and go to the FM-1 in one write, after the store as it was is saved in BACKUP's list.
- **Play a backup in the browser.** A backup the Manager keeps (or a `.fizzbak` you open) plays in the web FIZZ with its projects and presets, for recording videos with Remote FM-1 or trying a set without the unit.
- **The Manager fits the window.** Every page fits the screen without scrolling the whole page; LIBRARY's list scrolls on its own and filters IN POOL / NOT IN POOL; the computer keys play on LIBRARY, POOL and IMPORT.
- **Geek Out has a fifth page: TRACKS.** Six scopes filling the screen, one per track in its colour, each track's own sound before the effects. A silent track is a flat line.
- **The Manager and the tools talk to FIZZ reliably.** Commands sent while the unit was drawing a frame, or two sent back to back, could be lost: the USB side kept just one and FIZZ took most of its own only between frames. The unit now holds two and FIZZ picks them up within a couple of milliseconds, in order. A Manager backup takes about 7 s (was 8 to 16).
- **RESTORE survives a flaky USB link.** A block lost on the way is asked for again every second (for up to ~90 s) instead of failing the restore half way, which could leave the store half written; the data push after an install does the same.
- **Pattern changes are smoother still.** The last ~7 ms the sound waited at a pattern change is gone; the sounds a new pattern needs are read in the background too.

## 0.12.0-beta.9 (2026-10-27)

The cat and the heads, the VOICE page (mono, unison, priority), no pop at a pattern change, HOLD for the live keys, and the owner's rounds of 2026-10-26.

- **SENDS has a cat.** The FX page's four bars are now a big neon cat in sunglasses: the **left ear is CHORUS** and the **right ear DELAY** (each fills from its base toward the tip), the **lens is REVERB** (it fills from the left), and **DRY** sets the whole cat's colour: fully dry it is muted and grey, fully wet bright. The head's outline takes CHORUS's blue on the left and DELAY's green on the right. The values sit at the corners in their colours, without names (CHORUS and DELAY by their ears, REVERB and DRY at the foot); a lock shows as before. While playing, the cat **nods to each step** of the track (its whiskers flick, a glint crosses the lens), and the sends shown **follow the step just heard**, its locks too; not while a step is held, the latch is on or a key is down. Turning REVERB up to **100 %** flashes the sunglasses.
- **MASTER is two heads.** The filter page's curves are now two heads back to back: the **LOW head** (orange, facing left) sits at the left edge with the low cut off and slides right as you turn LOW up; the **HIGH head** (light blue, facing right) sits at the right edge and slides left as you turn HIGH down. A narrow band brings them together, overlapping. SLOPE 12 dB draws a second outline inside each head; FILTER OFF turns them grey. The names and values under them are as before.
- **A sound's VOICE page: MONO / POLY, UNISON, PRIORITY.** A new page in B1's group, after PORT (TRIG, KEYS, PORT, **VOICE**, MOD). **KNOB 1 VOICE**: POLY, or MONO (one note at a time on the track; with PORT LEGATO a new note while one is held moves the voice without a new attack). **KNOB 2 UNISON**: OFF, or 2 … 8 real voices for every note, as the Digitone 1 does; the track's voice LIMIT (track settings) still caps them, the middle voices kept. **KNOB 3 DETUNE** spreads the unison voices' pitch (up to ±50 cents at the outer ones), **KNOB 4 SPREAD** places them across the stereo. **ALGORITHM PRIORITY** (LOW, MID, HIGH): when the unit's eight voices run out, a lower priority sound gives its voices up first, and a sound never takes voices from a higher one. The page draws the voices' waves and their places, the ones over the LIMIT dim. All five are saved with the sound (old sounds: POLY, OFF, MID) and in the Manager's EDIT (PLAY), not lockable on a step.
- **No more pop when changing patterns.** A pattern that was not in memory used to be read from flash while the sound waited (up to ~18 ms: a click or a burst of static); it is read in the background now, playing or stopped. A pattern change also ends the MIDI FX notes the old pattern left going (an empty pattern plays no arp on).
- **The FX pages: PRESETS works** as in the sound editor (nothing held: the track's preset in the browser; a step held or selected: its sound lock from the pool), and the browser brings you back to the FX (or MIX) page you came from, the keyboard still on. **FX opens on the page you left it on.** Opening FX from the live keys keeps the white keys playing.
- **The patterns page**: the knobs other than ALGORITHM do nothing while it is up (they no longer edit the step view behind it).
- **The sound editor, keys playing**: B1 held still makes W1..W8 the modulation source.
- **More headroom: the engine 6 dB lower, BOOST in SETTINGS.** The tracks now play 6 dB under the old level, so the engine and its effects no longer clip inside with many voices. GLO + SEL (SETTINGS), **KNOB 2 BOOST** brings the old level back (+6 dB), for the speaker.
- **HOLD for the live keys (LFO).** Wherever the white keys play live (the keyboard, the sound editor's and the effects' keys), **LFO turns HOLD on**: the notes you play stay after you let go; keep keys down to add more; let go of all of them and the next key starts a new set. LFO again, or leaving the live keys, releases them (LFO lit while it holds).
- **The sound editor remembers each black key group's page**: back to a group, you are on the page you left it on.

## 0.12.0-beta.8 (2026-10-26)

The sequencer clean-up (2026-10-26; docs/SEQ-CLEANUP-PLAN.md, the mock-ups in mockups/fizz-seq-cleanup.html), and the flash diet.

- **One knob table, held or not.** **KNOB 1 is the mode's main value** everywhere: in NOTE the root with nothing held (the whole default chord) and the whole chord with a step held (SELECT did that; B1 held + SELECT still moves the root). **KNOB 2 … 4 are notes 2 … 4**, and **note 5 is on ALGORITHM only while you physically hold the step** (or B1 for the default); in the latch's selection ALGORITHM stays the step cursor. **SELECT is the page** in every mode, held or not. The held NOTE screen draws each note in its knob's colour (1 … 4 blue, green, white, orange; note 5 pink).
- **OCT− / OCT+ tapped with nothing held** move the entry octave (held longer than a tap they are the F1 / F2 layers, whose map waits a tap's time now).
- **The step latch (LFO) starts with nothing selected**: tap a step, or turn ALGORITHM (it lands on the step you last edited). **MODES** is drawn first, on the left, of the latch-mode screen; SELECT turns toward it.
- **B10 is SLIDE, B11 is SOUND; P-LOCK is gone.** SLIDE is the 303's slide on one key: hold a step, **B2 ON** (B1 OFF, B3 LEGATO, or **B10 again** cycles), and the step's note slides into the track's next note; **KNOB 2 TIME, KNOB 3 SLOPE** (TIME / RATE), B11 clears it. ON strikes the next note gliding from this one; **LEGATO** holds this note until the next and moves it without a new attack. The lane shows the slides as ramps; the held screen the note gliding into the next. A slide is the step's PORT / TIME / SLOPE locks (the sound editor shows them; a PORT lock made there is a slide too). For a sound lock on a step hold the step in the sound editor (that is unchanged).
- **TRIG held shows the step's MIDI FX group** under the trig picture (a hexagon with the group's number, GROUP n or TRACK / OFF, the group's eight slots lit where an effect plays). KNOB 2 turns it as before.
- **The effects open on SENDS** (the OVERVIEW is gone): the current track's CHORUS, DELAY and REVERB as three bars (KNOB 1 … 3), the page's steps along the foot and on the white keys as the EDIT home (hold a step: the send is its lock, the bar framed, the value a chip), **REC** the keyboard on / off as the sound editor's. SHIFT + SELECT (or FX held + ALGORITHM) picks the track; W1 … W8 no longer do. On every other effects page and in the mixer the white keys' LEDs show the steps, as the step view lights them.
- **CHAIN routing as wet / dry in series** (the owner, the same day): in CHAIN the effects run in series and **each track's sends are its wet / dry at each effect** (0: the track passes it by, 100: all through, 50: half and half); no separate chain amounts. **DRY is each track's dry / wet in both modes** (SENDS' KNOB 4, ROUTING's ALGORITHM or KNOB 2): 100 % the dry alone, 0 % the effects alone, the middle both full in PARALLEL (as every unit sounded) and half and half in CHAIN; the green wire under the chain shows it.
- **The effects' pages and keys in SENDS' order:** B2 CHORUS, B3 DELAY, B4 REVERB (the pages run SENDS, CHORUS, DELAY, DELAY 2, REVERB, REVERB 2, MASTER, ROUTING); the ROUTING page's block keys follow.
- **The owner's first round on this build (the same day):** the latch in VALUES with nothing selected lights the default's values; REC toggles the keyboard on every effects page; **the latch works on every page as on EDIT** (a selection stays into the mixer, the effects, the track settings, the sound editor and the MIDI FX screen, where a tap selects and several together select many); **a track's group changed while playing** takes effect at once (a HOLD arp in the old group stops); on the MIDI FX screen a held or selected step shows its group popup at once and **B1 OFF, B2 … B6 groups 1 … 5, B11 TRACK** set it; the octave popup shows its number at once over a still 88-key piano.
- **The flash diet:** fonts in two weights (300 and 700: 400 and 500 read light), the drawing's own small sin / cos / exp / log / pow, the editor's and MIDI FX tables and the sound's harmonic table in the data blob, the stats reply by id. fizz.bin 543,568 B (38 KB free) before the clean-up.

## 0.12.0-beta.7 (2026-10-26)

The FX routing redone the way ChoralRoot 0.27 does it.

- **ROUTING** (FX, B6) is now a signal flow of four blocks, CHORUS, DELAY, REVERB and the FILTER. **KNOB 1 MODE:** PARALLEL (the sends into three buses, as every unit has always sounded) or CHAIN (one path through the four in the order on the screen; KNOB 2 … 4 are each effect's amount in the chain, and the tracks' sends rest). **Hold a block's page key** (B4 CHORUS, B2 DELAY, B3 REVERB, B5 FILTER): it lifts, and KNOB 1 moves it a place a click; let go and it lands. In PARALLEL the FILTER can go before the buses (hold B5, KNOB 1 anticlockwise). The old cross-feeds (DELAY > REVERB and the like) are gone. Updating keeps your projects; the routing starts at PARALLEL.

## 0.12.0-beta.6 (2026-10-26)

Fixes from a first-session test of beta.5, and the owner's next round: a global scale, the patterns map that stays, MIDI FX on and off from ARP.

- **A global scale.** SHIFT + ALGORITHM (scale) and SHIFT + PRESETS (root) set the project's global key when the selected track follows it, and the popup says GLOBAL; it stays up while SHIFT is held. New tracks follow the global key; a track overrides it on its settings page (KNOB 1 SCALE, KNOB 2 ROOT) and KNOB 3 FOLLOW KEY puts it back.
- **The track settings page:** the white keys are the steps, or the keyboard when REC's keyboard is on. In the name editor KNOB 2 rolls names like PRESETS.
- **Holding SEQ** opens the patterns map and it stays; SEQ again or HOME closes it. A SEQ tap with no map open is still TOOLS.
- **Holding ARP** turns the track's whole MIDI FX group off and on (MIDI FX OFF / ON), and ARP's light is full while MIDI FX is on.
- **HOME keeps the view** you chose with SHIFT + HOME, and GEEK OUT remembers its page.
- **Saving a new preset no longer stalls the sound:** the save was reading all 64 patterns from flash to pick an id.
- **Track settings, page 1:** KNOB 1 MODE (GLOBAL or LOCAL), KNOB 2 SCALE, KNOB 3 ROOT, KNOB 4 MIDI CH. On GLOBAL the scale and root you turn are the project's, for every track that follows it; on LOCAL they are this track's alone. REC on the page switches the white keys between the steps and the keyboard without leaving it.
- **The SHIFT scale / root popup** goes away a quarter second after SHIFT is released.
- **MIDI FX on / off** (hold ARP, or SHIFT + ARP at once) is a bypass: the group stays and can still be edited, the page says OFF, and the sound skips it. **FILL is SHIFT + FX held.**
- **Any page key leaves the latched patterns map** (SEQ held) and opens its page, as from anywhere else. **MIDI FX off** stops the whole group at once: the track's notes leave the group, so a running arp stops too.
- **Track settings:** MODE reads GLOBAL on the left, every scale name is one size, the scale's notes sit under its name, and the ring marks the knob you turned. **The keyboard's lights:** the root at 75 %, the scale's notes at 40 %, a played note full.
- **REPLACE never changes a pattern's tracks:** every track holds its own copy of a sound, so overwriting a preset from A02 leaves A01 as it was (a test proves it; the pool's entries follow the preset as before).

- **A new project starts on pattern A01**, track 1 (it used to open on the demo's A02, so your first beats went into A02).
- **GEEK OUT is one view:** SHIFT + HOME cycles PIANO ROLL, STEPS, EDIT and GEEK OUT; in GEEK OUT, SELECT turns its pages (METERS, NUMBERS, VOICES, SCOPE). HOME always comes back to the step view.
- **SAVED means saved:** a preset is written to flash at once, so switching off right after SAVE keeps it.
- **MIDI FX:** with the track's group OFF, the page shows that ALGORITHM picks a group.
- **The page lights:** the button of the page you are on is lit: HOME on the step view, EDIT, ENV, FX, SEL, SEQ (TOOLS), ARP (MIDI FX), SAVE and REC (the keyboard). ARP is dim, not lit, while the arp plays with its page closed.

## 0.12.0-beta.5 (2026-10-25)

The owner's beta.4 round: TOOLS on a tap of SEQ, mute and solo in the mixer, finer knobs, and a Manager that no longer slows the FM-1 down. Updating keeps your projects and presets.

- **TOOLS opens with a tap of SEQ** (SEQ again or HOME closes it). Holding SEQ is still the patterns map. **Holding ARP** turns the track's arp on and off. **SHIFT + SAVE** opens the project page (SHIFT + SAVE again or SELECT: the projects).
- **TOOLS:** B9 opens STRETCH and B10 CLIP. STRETCH's KNOB 1 is a ladder of ratios (÷4 ÷3 ÷2 ÷1.5 ÷1.33 ÷1.25 ×1 ×1.25 ×1.33 ×1.5 ×2 ×3 ×4). CLIP labels COPY, PASTE and CLEAR, shows what they act on (PAGE n or PATTERN), COPY and PASTE work in every scope (on the shown page outside PATTERN), and B10 again does the picked action, like OCT+.
- **The mixer (ENV):** tap a black key to select its track, hold it to mute or unmute, double-tap to solo; B11 clears every mute and solo. SELECT turns the page, ALGORITHM picks the track, PRESETS mutes (left) or unmutes (right) the selected track. The keys' lights show it: dim muted, blinking soloed.
- **Finer knobs:** every value kept in hundredths (the LFOs' SPEED and DEPTH, the filter's FREQ, RESO and ENV DEPTH, DTUNE, BASE) moves 0.01 a detent, speeds up when you turn fast (the whole range in about two turns), and SHIFT + knob moves 1.00. The Manager's EDIT page does the same on the wheel and the arrow keys (SHIFT + drag: hundredths).
- **The NOTE hold screen's keybed** (and a layer's little keybed) starts on F, like the keys.
- **Holding LFO:** MODES is now the default latch mode. **The settings page** (SHIFT + SEL) lost the KNOBS layouts: layout A is the only one, PAGE FOLLOW is on KNOB 1.
- **FIZZ Manager:** the FM-1's screen no longer crawls while the Manager is connected (only real transfers slow its frames). LIBRARY's EDIT, LOAD ON FM-1 and EXPORT buttons are their own width.
- **The guide** covers six tracks, TOOLS, the record modes, the Manager and the new SEQ / ARP keys.

## 0.12.0-beta.4 (2026-10-25)

Three ways to record (CAPTURE for quick takes, REPLACE and OVERDUB), STRETCH and CLIP in TOOLS, and a FIZZ Manager that edits the sound on your track. Updating from beta.3 keeps your projects and presets.

- **FIZZ Manager: the sound editor (EDIT).** Pick a preset in LIBRARY and press EDIT (or drop a .fizzsnd or a Digitone sound on the page): every value of the sound is on one page, like a synth's front panel (OPERATORS with the algorithm, OP ENV, PLAY, FILTER with its curve, AMP, DRIVE and CRUSH, the three LFOs, the eight MOD slots; on a phone they stack and scroll), and every value is something you drag, scroll or click; SHIFT drags finely. A keyboard like the FM-1's plays it in the page itself (FIZZ running in your browser, no FM-1 needed; the computer's A..' row plays too, Z and X change the octave). REVERT puts it back, SAVE writes it over the MY preset it came from, SAVE AS NEW into a free place.
- **FIZZ Manager: TRACK, the live editor.** A switch in the Manager's header. Off (as it starts), the Manager only manages your presets and never touches the tracks of the open project, so it is safe while you play. On, it works like the FM-1's sound browser: a preset you pick in LIBRARY plays on the track at once (the selected track, or one you choose), and a bar keeps the track's own sound: KEEP, or CANCEL to put it back. EDIT shows the sound on the track and edits it live, as if you turned the FM-1's knobs; turning a knob on the FM-1 shows in the page too. REVERT puts the track back as it was, SAVE writes the sound into MY PRESETS (the track then plays the saved preset), and the page's keyboard plays the FM-1. Notes the page plays are recorded while REC is on (its keyboard turns red). Needs this FIZZ version; an older one edits in the browser only.
- **FIZZ Manager: EDIT follows the FM-1's track.** With an FM-1 connected, EDIT shows the sound of the track selected on the FM-1 and follows it when you select another track there (or pick one of the six on the editor's track strip). The track is shown big in the editor's header and on the TRACK switch. With TRACK off the sound is only shown (a change nudges the switch); turn TRACK on (in the header, or LIVE on the strip) to edit it. A preset opened from LIBRARY with TRACK off is edited in the browser; SELECTED on the strip goes back to the FM-1's track.
- **FIZZ Manager: the TRACK switch is easier to find**: bigger, with the track's number, and a pulsing ring until you first use it.
- **FIZZ Manager: LOAD ON FM-1.** A button for the selected preset in LIBRARY opens the FM-1's own sound browser on it, as if you had turned PRESETS to it: the track plays it, OCT− on the FM-1 cancels, OCT+ loads it. The button shows OCT− / OCT+ while the FM-1 waits, and the page says what you chose. Works with TRACK off too (you decide on the FM-1). Needs this FIZZ version.
- **FIZZ Manager: EXPORT from LIBRARY.** The selected preset has its own EXPORT button next to EDIT, and EXPORT ALL above the list exports all of MY PRESETS (a .fizzlib, or Digitone files).
- **FIZZ Manager: IMPORT takes FIZZ's own files**: a .fizzlib (as EXPORT ALL writes it), .fizzsnd files and the presets in a .fizzbak backup go into MY PRESETS. Sounds already in MY PRESETS (the same name and sound), or twice in the files, are flagged and left unticked.
- **FIZZ Manager: adding a sound to the pool or taking it out shows at once** (it used to show only later); the page then checks the FM-1's lists until they agree.
- **FIZZ Manager: EXPORT** a preset (or all of MY PRESETS) as a FIZZ .fizzsnd, a Digitone .dnsnd, or a Digitone II .dn2pst or .dn2snd. Each format shows a bar of how much of the sound carries over and lists what it drops or changes (LFO 3, CRUSH, AHD as ADSR ..). The Digitone files are marked EXPERIMENTAL until Digitone owners confirm they load.
- **Saving over a preset from the FIZZ Manager no longer stalls the sound.** Storing over one of MY PRESETS from a page (the Manager's SAVE, a restore of a preset) used to rewrite the other projects' patterns that use it while the FM-1 was making sound; it now does that in the background, a project at a time, and waits while the FM-1 plays (stop it to save over a preset, or save as a new one).
- **The live-recording keyboard starts on F**, like the FM-1's own keys: the 27 keys drawn are the 27 you play (F3 to G5 at OCT 0), so the key lit is the key you press. The plain keyboard (REC tapped) now starts on F too.
- **New take: CAPTURE, for quick takes** (the record settings, SHIFT + REC, KNOB 3 TAKE: CAPTURE, REPLACE or OVERDUB; CAPTURE is the default). Hold REC: nothing changes until you play the first note. Then the track is cleared (steps, locks, back to page 1 with every page 16 steps; its speed stays, so a slower speed fits a longer take) and the take runs from the top with your timing. PLAY or REC ends it: the track becomes exactly as long as you played (36 steps: pages 1 to 3, the third 4 steps long) and loops from there. Playing, hold REC again for the next take: its first note starts everything from the top. REPLACE (a note replaces the step's notes) and OVERDUB (it joins them) record into the sequence as it is.
- **Fixed: holding REC while stopped and then playing a note did not record.** The first note now starts playback and the recording, that note on the first step (PLAY still starts it too).
- **Fixed: REC held + PLAY cancelled the recording and jumped to the step view.** Pressing PLAY while REC is held now starts the recording with playback (CAPTURE: the take from the top).
- **CLEAR TRACK resets the track**: besides its steps and locks, its pages go back to page 1 only, each 16 steps long (as a new track). Its sound and speed stay. (The same for TOOLS' CLEAR on a track or the pattern.)
- **TOOLS: the keys in a new order.** B1 ROTATE, B2 REVERSE, B3 RANDOM, B4 EUCLID, B5 GRIDS, B6 HUMANIZE, B7 QUANTIZE, B8 LENGTH; B11 is still the scope. COPY, PASTE and CLEAR left the keys for a page of their own, **CLIP**: turn PRESETS past STRETCH, pick COPY, PASTE or CLEAR with KNOB 1 (shown big, the other two beside it), and OCT+ does it (lit when it can; CLEAR on a track or the pattern wants OCT+ twice). COPY and PASTE work on a page or the whole pattern, as before.
- **New in TOOLS: STRETCH** (PRESETS, after LENGTH) squeezes or spreads a track in time: KNOB 1 doubles or halves it (x2, x4, x8, /2, /4, /8: an 8-page track onto 1 page, or 1 page over 8), KNOB 2 fits it onto 1..8 pages. Each note lands on the nearest new step, its timing kept as micro-timing (a note between two steps keeps its offset), its p-locks and its own length go with it, and where two notes would meet the one nearer its step stays. On a page or the selected steps it works within the page. The screen shows the loop as it was over the loop as it will be; you hear it at once, OCT+ keeps it (one undo), OCT− puts it back.
- **F1's page map: a double tap on a page loops it alone again.** The second tap now only has to start within 0.2 s of the first one's end; it used to have to end within 0.2 s, which a normal press on the FM-1's keys often missed, so the double tap came out as two taps.
- **F3 (hold PLAY): turning SELECT or ALGORITHM for the tempo or the swing shows the TAP ape** dancing to the new tempo (a new move every two beats, swung with the swing), where the metronome was.

## 0.12.0-beta.3 (2026-10-25)

TOOLS fixed after testing on the FM-1, and a friendlier FIZZ Manager. Updating from beta.2 keeps your projects and presets.

- **FIZZ Manager: backups stay in your browser.** BACK UP no longer asks where to save: the backup is kept in the page's list, and its save button writes it to a folder you pick. A backup not saved to a file yet shows a yellow save button. The list shows how much space the backups use.
- **FIZZ Manager: a refresh button** next to the FM-1 reads the FM-1 again, so presets you saved on it show up. Coming back to the tab does it by itself when something changed.
- **FIZZ Manager: LIBRARY is one list** with ALL, FACTORY and MY PRESETS filters plus the tags, and a big + POOL / − POOL button for the selected sound. Sounds in the pool have a green ring and a badge with their pool number, in LIBRARY and in POOL. POOL's list has the same filters and takes factory presets too.
- **FIZZ Manager asks once** before SORT BY TAG, CLEAR UNUSED, swapping pool places, CLEAR, DELETE and loading a project, and says whether SHIFT + F1 can undo it.
- **FIZZ Manager: Digitone sounds saved on OS 1.42** (such as F186 KICK01) import now; they were refused as "a newer Digitone format (version 4)".
- The FM-1 pill in the Manager's header is cleaner.
- **TOOLS fixes.** The page SELECT picks now stays while the pattern plays (it used to follow the playhead, so a PAGE change went to a page you were no longer looking at), and SELECT pops the pages with "PAGE 3"; a strip over the steps shows the page all the time. The white keys show the page as the change makes it, with the steps it removes dimmed (the ghosts, on the screen faded too, on every page) until OCT+ keeps or OCT− reverts; steps outside a track's loop stay dark, since tools never change them. A selected step rotated now moves along the track (the steps it passes each step back one), and the selection goes with it. RANDOM's four meters have icons, and turning a knob names it (DENSITY, NOTES, VELOCITY, CHANCE). The tool's popup also says which scope it works on, and EUCLID's length goes back to 16 when you leave SELECTED.
- **New in TOOLS: QUANTIZE**, HUMANIZE's opposite. Turn PRESETS past LENGTH to reach it (every B key is taken). KNOB 1 pulls the steps' own timing toward the grid (0..100 %: 50 % halves it, 100 % puts each note back on its step), KNOB 2 picks the grid (a step, 1/2, 1/3 or 1/4 of one: notes never change step), KNOB 3 evens the steps' own velocities toward the track's. It works on the selected steps, the page, the track or the pattern like the other tools: you hear it at once, the bars show where each note was, OCT+ keeps it (one undo), OCT− puts it back. The HUMANIZE screen shows where its notes were too.

## 0.12.0-beta.2 (2026-10-24)

TOOLS for your patterns, and the FIZZ Manager for backups and your library. Updating from beta.1 keeps your projects and presets.

- **New: TOOLS.** Hold ARP to open it; it works on the selected track. B1..B7 (or PRESETS) pick ROTATE, REVERSE, RANDOM, EUCLID, GRIDS, HUMANIZE or LENGTH; they change the selected steps, the page, the track or the whole pattern: B11 or ALGORITHM picks which. SELECT turns the pages, and the white keys show the page's steps as the change will make them; pressing one selects it (with the step lock on, TOOLS starts on the selected steps). You hear every change at once. OCT+ keeps it (one undo), OCT− puts it back, and a big word says which. B8 / B9 copy and paste the page or the pattern, and B10 clears (press it twice for a track or the pattern). HOME closes TOOLS. Turning the arp on and off has moved to the MIDI FX screen (tap ARP). SHIFT + ARP is still FILL, and ARP's light is on while FILL is held or TOOLS or MIDI FX is open.
- **New: the FIZZ Manager** web page backs up your FM-1's whole store to a file and restores it, and shows your library.
- **The FIZZ Manager edits the FM-1 too.** In LIBRARY you rename a preset in place, switch its tags on and off (plus your own tags, kept on your computer and in backups), drag it to another place, add it to the pool, or delete it (a preset a project uses asks a second time; its steps go back to their track's sound, as on the panel). POOL shows the project's 128 places: drop a preset on one, drag one onto another to swap them in every pattern, clear one, sort them by tag, clear the unused, or load another project. IMPORT reads Digitone and Digitone II sounds (.dnsnd, .dn2pst, .dn2snd) and sends them into MY PRESETS. Stop the FM-1 for pool swaps, deletes and project changes. SHIFT + F1 on the FM-1 undoes the page's changes too, except pool swaps and project changes (they clear the undo). While the Manager talks to the FM-1, its screen slows to a frame a second so the transfers go much faster (a whole backup takes about 10 seconds); it is back to normal a moment after.

## 0.12.0-beta.1 (2026-10-24)

Six tracks, and no more sound-browser freezes. **This update clears your projects** (My Presets are kept), so write down or record anything you want to keep first.

- The playhead now moves in half steps, and its red arrow over the open page no longer bounces.
- While playing with the step lock off, the sound editor's values now follow the LFOs too, showing what you hear.
- FIZZ now has six tracks instead of eight, which frees memory for new features. W7 and W8 no longer pick a track and stay dark. **Your projects start fresh:** the first boot of this version clears the old eight-track projects. My Presets are kept.
- The sound browser no longer freezes when you add a sound to the Pool, take one out, or press B9 to switch between Presets, My Presets and the Pool.

## 0.11.0-beta.2 (2026-10-24)

A smoother screen. Updating from beta.1 keeps your projects and presets.

- **Faster screens while playing**, measured with the demo running:
  - piano roll: about 90 → 60 ms a frame;
  - edit: 42 → 35 ms;
  - Geek Out NUMBERS: 45 → 34 ms;
  - VOICES: 47 → 39 ms;
  - step view: 34 → 30 ms.
- **Almost no tearing.** FIZZ now sends only the parts of the screen that changed and times each send to the
  display's refresh. A change too tall for one refresh goes in two whole parts instead of tearing.
- **Closer colours.** Grey and white edges blend smoothly instead of being dithered.
- **Steadier sound under load.** The work is split more evenly between the two cores, and no screen work holds up
  the audio for long.
- **Simpler animations.** The TAP ape is drawn with fewer details, and the MIDI FX glows are gone, so busy screens
  keep up.

Known in this beta:

- The SCOPE page is the slowest screen (about 90 ms a frame), because its whole waveform changes every frame.
- There is no backup of FIZZ's own saves from the install page yet. An update from one FIZZ to the next keeps them.

## 0.11.0-beta.1 (2026-10-24)

The first FIZZ that runs on the FM-1 itself. Until now FIZZ was a preview in the browser (0.10).

- **It replaces the FM-1's firmware.** Install it from the install page in Chrome or Edge. The same page puts
  M-VAVE's firmware back when you want it.
- **8 voices** at once, played across both of the FM-1's cores. If the second core ever misbehaves, FIZZ carries
  on with one until you switch off.
- **USB MIDI only:** no Bluetooth MIDI and no USB audio in this build.
- **Your projects and presets are saved on the FM-1.** The first time FIZZ starts, it clears another firmware's
  saved data if it finds some, so back that up first.
- **Safe mode:** hold SAVE while you switch the FM-1 on to start FIZZ without reading or writing its saves.
  Switch off and on again to leave it. (REC held while switching on is the FM-1's update mode, for installing
  firmware.)
- **Geek Out pages:** SHIFT + HOME past EDIT shows the processor load, frame rate, voices and a scope. Please
  include a photo of METERS or NUMBERS when you report something slow.

Known in this beta:

- Some screens still redraw more than they need to, so the step view and the idle screen can feel slower than the
  rest. A fix is on the way.
- The TAP ape can look choppy while the sequencer plays.
- There is no backup of FIZZ's own saves from the install page yet. An update from one FIZZ to the next keeps them.
