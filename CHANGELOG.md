# FIZZ release notes

The firmware releases for the M-VAVE FM-1, newest first, written for testers. Install them from
[the install page](https://quixotic7.github.io/FIZZ/install/) and tell us how it goes on
[Discord](https://discord.gg/emf6DmZ9V). `firmware/release.py` turns the "Unreleased" section into the next release:
its text becomes the release's notes on the install page and on GitHub.

## Unreleased

<!-- The next release's notes, for testers, in plain language. firmware/release.py moves them. -->

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
