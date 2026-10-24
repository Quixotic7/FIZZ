# FIZZ release notes

The firmware releases for the M-VAVE FM-1, newest first, written for testers. Install them from
[the install page](https://quixotic7.github.io/FIZZ/install/) and tell us how it goes on
[Discord](https://discord.gg/emf6DmZ9V). `firmware/release.py` turns the "Unreleased" section into the next release:
its text becomes the release's notes on the install page and on GitHub.

## Unreleased

<!-- The next release's notes, for testers, in plain language. firmware/release.py moves them. -->

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
