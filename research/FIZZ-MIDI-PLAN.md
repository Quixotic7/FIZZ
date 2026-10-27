# FIZZ full MIDI: draft for the owner (2026-10-27)

This plan builds PLAN.md P1.6b (MIDI tracks) and more, from the owner's request. It is a draft. Nothing is built until the owner says go. The MIDI track's screens get mockups first.

## What FIZZ has today

- **In:** USB and the TRS jack, merged. Each track has a MIDI CH setting (track settings page 1, KNOB 4). Channels 1–6 play tracks 1–6 by default.
  - Channel 16 also plays the selected track.
  - Notes, MOD WHEEL (CC1), BREATH (CC2), aftertouch and pitch bend feed the track's modulation slots.
  - CC120 and CC123 release the track's notes.
  - Clock, START, CONTINUE and STOP drive the transport.
- **Ignored:** every other CC, program change and SPP. There is no remote control.
- **Out:** nothing musical. Only FIZZ's own SysEx replies go out. Bluetooth MIDI is switched off in the build.

## What changes

### 1. Channels

| Channel | Plays |
|---|---|
| 1 … 6 | Tracks 1 … 6. Each track's MIDI CH setting still overrides this. |
| 15 | **The selected track**: notes, controllers and CCs. This moves from 16 to 15. |
| 16 | **The remote**: the FM-1's panel, as if your hands were on it (section 3). |

Notes from channels 1–6 and 15 also go into a held step or a recording, as they do today.

### 2. CCs control the sound

A CC on a track's channel, or on 15 for the selected track, sets that track's sound the way the editor's knobs do when no step is held. The notes already sounding follow the change.

- **Proposed:** a fixed, published map that follows the editor's pages in SELECT order, so a controller template can be built once.
- **Common GM numbers where they exist:**
  - 7 track LEVEL, 10 PAN
  - 91 REVERB, 93 CHORUS, 94 DELAY (the track's sends)
  - 74 FILTER FREQ, 71 RESO
  - 73 AMP ATTACK, 72 AMP RELEASE, 75 AMP DECAY
  - 5 PORTAMENTO TIME, 65 PORTAMENTO
- **Everything else in page order:** CC 14–31, 70–90 and 102–119. That is about 65 numbers, enough for the editor's values: RATIO, TONE, OFFSET, both ENVs, FILTER, F ENV, BASE, AMP, LEVEL, CRUSH, TRIG/KEYS, VOICE and the three LFOs. The modulation-slot settings are left out.
- **Left alone:** 0/32 (bank), 1, 2, 6, 38, 64, 96–101 (RPN/NRPN) and 120–127.
- **With REC on and AUTOMATION ON:** CC moves record as p-locks on the steps, the same as the knobs do.

### 3. The remote on channel 16

It works with any controller. Its own map:

- **Notes 53 … 79 (F3 … G5):** the 27 keys, with velocity. They are the keys, not the notes, so they do whatever the keys do in the current view: steps, playing, the page keys.
- **CC 20 … 33:** the 14 buttons. Value 64 or more presses, below 64 releases. A momentary pad gives taps and holds.
  - In panel order: TRACKS, PATTERN, PAGE, MUTE, SOUND, FUNC, HOME, SAVE, KEYS, MIX, PLAY, REC, OCT−, OCT+.
- **CC 40 … 46:** the 7 encoders (SELECT, ALGORITHM, PRESETS, KNOB 1–4), sent as relative values (65+ is right, 63− is left, steps = distance from 64). This suits endless encoders such as the Roto-Control or a Faderfox.
- **CC 47 … 50:** KNOB 1–4 again, as absolute 0–127. These set a knob's current value directly, for plain pots and faders.
- **CC 7:** MASTER.

### 4. MIDI out on any track

A new setting on track settings page 3: **MIDI OUT**, either OFF or a channel 1–16.

- **What it sends:** the track's notes as it plays them, after its MIDI FX and arp. It also sends the notes you play live on the track. The track still sounds itself, unless it is a MIDI track (section 5).
- **Port:** USB only. The FM-1 has no MIDI out jack; its TRS is input only.

### 5. MIDI tracks

A new setting on track settings page 3: **ENGINE**, either FM TONE or MIDI. A MIDI track has no sound engine:

- Its notes go out on its MIDI OUT channel. Choosing MIDI sets the channel to the track's number if it is OFF.
- **EDIT opens 7 pages of 4 CCs** instead of the sound editor:
  - SELECT, or black keys B1 … B7, picks the page.
  - KNOB 1–4 set the four values, 0–127. Each change is sent at once.
  - A value sends only once it has been touched, so untouched slots stay silent.
  - Holding a step and turning a knob makes a **p-lock**. That step sends the CC as it plays, and the track's own value comes back after it. This works the same as sound locks: LATCH, B11 clears them, and REC with AUTOMATION records them.
  - **Choosing a slot's CC number:** SHIFT + the knob. The page's slots default to 4 numbers running in order: page 1 is CC 16–19, page 2 is 20–23, and so on. Each slot also gets a 6-letter name, kept with the track.
- **Storage:** the 28 values and the CC numbers live in the track's sound record, in place of the FM values. They are saved with the project, and a pool or library preset can be a MIDI preset.

## Questions for the owner

1. **The remote:** is the map in section 3 right? Should channel 16's notes be the **keys** (as proposed), or plain notes on the selected track (which 15 already gives)?
2. **The CC map:** is a fixed map OK? A learn mode could come later.
3. **MIDI out:** should anything else go out, such as clock and transport, or the sound's values as CCs when you turn a knob, so a controller's rings follow?
4. **MIDI track pages:** is "SHIFT + knob picks the CC number" right, and do slots need names?

## Order of work, once agreed

1. Channels 15/16 and CC → sound (core/app/app.c midi_play; a CC table beside ctl_snd.c's pages).
2. The remote: CC and notes into qd_app_button, qd_app_knob and qd_app_key, from the main loop.
3. MIDI OUT: a per-track channel; the player's key events go to an out queue, which fm1core and FIZZ's port send.
4. Mockups of the MIDI track editor. Then the MIDI track itself: the engine switch, 28 lockable values, sending at step time.
5. Tests for each part, then the guide (site/guide.html) and the changelog.
