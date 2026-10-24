# FIZZ release notes

The firmware releases for the M-VAVE FM-1, newest first, written for testers. Install them from
[the install page](https://quixotic7.github.io/FIZZ/install/) and tell us how it goes on
[Discord](https://discord.gg/emf6DmZ9V). `firmware/release.py` turns the "Unreleased" section into the next release:
its text becomes the release's notes on the install page and on GitHub.

## Unreleased

<!-- The next release's notes, for testers, in plain language. firmware/release.py moves them.
     Draft for 0.11.0-beta.1 below (the owner reviews it; add the display fix and anything known before releasing). -->

The first FIZZ that runs on the FM-1 itself. Until now FIZZ was a preview in the browser (0.10).

- **It replaces the FM-1's firmware.** Install it from the install page in Chrome or Edge. The same page puts
  M-VAVE's firmware back when you want it.
- **8 voices** at once: this build uses one of the FM-1's two cores.
- **USB MIDI only:** no Bluetooth MIDI and no USB audio in this build.
- **Your projects and presets are saved on the FM-1.** The first time FIZZ starts, it clears another firmware's
  saved data if it finds some, so back that up first.
- **Safe mode:** hold REC while you switch the FM-1 on to start FIZZ without reading or writing its saves.
