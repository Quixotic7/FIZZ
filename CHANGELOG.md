# FIZZ release notes

The firmware releases for the M-VAVE FM-1, newest first, written for testers. Install them from
[the install page](https://quixotic7.github.io/FIZZ/install/) and tell us how it goes on
[Discord](https://discord.gg/emf6DmZ9V). `firmware/release.py` turns the "Unreleased" section into the next release:
its text becomes the release's notes on the install page and on GitHub.

## Unreleased

<!-- The next release's notes, for testers, in plain language. firmware/release.py moves them. -->

- **FIZZ Manager: backups stay in your browser.** BACK UP no longer asks where to save: the backup is kept in the page's list, and its save button writes it to a folder you pick. A backup not saved to a file yet shows a yellow save button. The list shows how much space the backups use.
- **FIZZ Manager: a refresh button** next to the FM-1 reads the FM-1 again, so presets you saved on it show up. Coming back to the tab does it by itself when something changed.
- **FIZZ Manager: LIBRARY is one list** with ALL, FACTORY and MY PRESETS filters plus the tags, and a big + POOL / − POOL button for the selected sound. Sounds in the pool have a green ring and a badge with their pool number, in LIBRARY and in POOL. POOL's list has the same filters and takes factory presets too.
- **FIZZ Manager asks once** before SORT BY TAG, CLEAR UNUSED, swapping pool places, CLEAR, DELETE and loading a project, and says whether SHIFT + F1 can undo it.
- **FIZZ Manager: Digitone sounds saved on OS 1.42** (such as F186 KICK01) import now; they were refused as "a newer Digitone format (version 4)".
- The FM-1 pill in the Manager's header is cleaner.
- **TOOLS fixes.** The page SELECT picks now stays while the pattern plays (it used to follow the playhead, so a PAGE change went to a page you were no longer looking at), and SELECT pops the pages with "PAGE 3"; a strip over the steps shows the page all the time. The white keys show the page as the change makes it, with the steps it removes dimmed (the ghosts, on the screen faded too, on every page) until OCT+ keeps or OCT− reverts; steps outside a track's loop stay dark, since tools never change them. A selected step rotated now moves along the track (the steps it passes each step back one), and the selection goes with it. RANDOM's four meters have icons, and turning a knob names it (DENSITY, NOTES, VELOCITY, CHANCE). The tool's popup also says which scope it works on, and EUCLID's length goes back to 16 when you leave SELECTED.

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
