# FIZZ full MIDI (2026-10-27)

This plan builds PLAN.md P1.6b (MIDI tracks) and more, from the owner's request. Built on the `fizz-midi` branch, in
stages, each with its own test. The MIDI track's screens get mockups first.

## The owner's answers (2026-10-27)

1. **The remote:** as drafted. Channel 16's notes 53 … 79 are the panel's **keys**; the CC map of section 3.
2. **The CC map:** a fixed, published map (section 2). A learn mode can come later.
3. **MIDI out:** each track's notes, plus **clock and transport**, plus **knob moves as CCs** (so a controller's rings
   follow).
4. **MIDI track pages:** SHIFT + a knob picks the slot's CC number. **No names**: the screen shows the number.

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
| 16 | **The remote**: the FM-1's panel, as if your hands were on it (section 3). It never plays a track. |

- A track's MIDI CH runs 1 … 15 (16 is the remote's).
- Notes from channels 1–6 and 15 also go into a held step or a recording, as they do today.

### 2. CCs control the sound

A CC on a track's channel, or on 15 for the selected track, sets that track's sound the way the editor's knobs do when
no step is held. The notes already sounding follow the change (`qd_player_live_edit`).

- **The value:** 0 … 127 spread over the value's whole range. A value picked from a list (the filter MACHINE, an LFO's
  DEST, RATIO B's pair) takes its place in the list the knob turns through.
- **When:** on the main loop (the UI part of a MIDI message), never in the audio interrupt.
- **With REC on and AUTOMATION ON:** CCs aimed at the selected track record as p-locks on the steps, the same as its
  knobs do (`qd_auto_put`). Other tracks' CCs are live edits only.
- **Left alone:** 0/32 (bank), 1 and 2 (the modulation sources, as today), 6, 38, 64, 66–69, 96–101 (RPN/NRPN) and
  120–127 (120 and 123 release the notes, as today).
- The modulation slots (the MOD page) have no CCs.

**The map.** Common GM numbers where they exist; everything else in the editor's page order (SELECT's order):

| CC | Value | CC | Value | CC | Value |
|---|---|---|---|---|---|
| 7 | track LEVEL (mixer) | 10 | PAN | 91 | REVERB send |
| 93 | CHORUS send | 94 | DELAY send | 95 | DRY |
| 74 | FILTER FREQ | 71 | RESO | 5 | PORT TIME |
| 65 | PORT (OFF/ON/LEGATO) | 73 | AMP ATTACK | 75 | AMP DECAY |
| 72 | AMP RELEASE | | | | |

| Page | Values → CCs |
|---|---|
| TRIG | A TRIG 3, A RESET 9, B TRIG 12, B RESET 13, PHASE RESET 14 |
| KEYS | A KEY 15, B1 KEY 16, B2 KEY 17, VEL 18 |
| PORT | SLOPE 19 |
| VOICE | VOICE 20, UNISON 21, DETUNE 22, SPREAD 23, PRIORITY 24 |
| RATIO | RATIO C 25, RATIO A 26, RATIO B 27, MIX 28, ALGORITHM 29 |
| TONE | HARM 30, DETUNE 31, FEEDBACK 33 |
| OFFSET | OFS C 34, OFS A 35, OFS B1 36, OFS B2 37 |
| ENV A | ATK 39, DEC 40, END 41, LEV 42, DELAY 43 |
| ENV B | ATK 44, DEC 45, END 46, LEV 47, DELAY 48 |
| FILTER | MACHINE 49, ENV DEPTH 50, SHAPE 51, GAIN 52 |
| F ENV | ATK 53, DEC 54, SUS 55, REL 56, DELAY 57 |
| BASE | BASE 58, WIDTH 59, KEY TRACK 60, BW POST 61 |
| AMP | SUS 62, MODE 63, HOLD 70 |
| LEVEL | LEVEL 76, DRIVE 77, DRIVE PRE 78 |
| CRUSH | BITS 79, SRR 80, ROUTE 81 |
| LFO 1 | SPEED 82, MULT 83, DEPTH 84, DEST 85, WAVE 86, PHASE 87, TRIG 88, FADE 89 |
| LFO 2 | SPEED 90, MULT 92, DEPTH 102, DEST 103, WAVE 104, PHASE 105, TRIG 106, FADE 107 |
| LFO 3 | SPEED 108, MULT 109, DEPTH 110, DEST 111, WAVE 112, PHASE 113, TRIG 114, FADE 115 |

One table in flash, CC → value id (`core/app/midi_map.c`), read both ways (CC in, knob moves out). The guide prints it.

### 3. The remote on channel 16

It works with any controller. Its own map:

- **Notes 53 … 79 (F3 … G5):** the 27 keys, with velocity. They are the keys, not the notes, so they do whatever the
  keys do in the current view: steps, playing, the page keys.
- **CC 20 … 33:** the 14 buttons. Value 64 or more presses, below 64 releases. A momentary pad gives taps and holds.
  - In panel order (printed names): FX, SEL, ENV, LFO, EDIT, GLO, HOME, SAVE, ARP, SEQ, PLAY, REC, OCT−, OCT+.
- **CC 40 … 46:** the 7 encoders (SELECT, ALGORITHM, PRESETS, KNOB 1–4), relative: 65 + n is n detents right, 63 − n
  is n + 1 detents left (64 is nothing). This suits endless encoders such as the Roto-Control or a Faderfox.
- **CC 47 … 50:** KNOB 1–4 for plain pots and faders: each move turns the knob by how far the fader moved since its
  last message (the first message only sets where it is).
- **CC 7:** MASTER (absolute).
- All of it goes into `qd_app_key`, `qd_app_button`, `qd_app_knob` and `qd_app_master` from the main loop, as the
  panel does. A button or key the remote left down is let go when the remote sends CC 123 (all notes off).

### 4. MIDI out (USB only)

The FM-1 has no MIDI out jack; its TRS is input only. Everything goes to USB.

- **A track's notes:** a new setting on track settings page 3, **MIDI OUT**: OFF or a channel 1–16. The track's notes
  as it plays them, after its MIDI FX and arp, and the notes you play live on the track. The track still sounds
  itself, unless it is a MIDI track (section 5). A stop, a mute or a pattern change sends the notes' offs.
- **Clock and transport:** 24 PPQN clock, START, CONTINUE, STOP, while FIZZ runs on its own clock (not while it
  follows an outside clock). SETTINGS (GLO + SEL) **KNOB 3 CLOCK OUT**: ON / OFF, ON by default.
- **Knob moves as CCs:** when a sound value changes from the panel or the remote (the editor, the mixer), its CC of
  section 2 goes out on the track's MIDI CH, so a controller's rings follow. CCs that came in are not sent back; the
  steps' p-locks playing are not sent. SETTINGS **KNOB 4 CC OUT**: ON / OFF, ON by default.
- **How it goes out:** the app fills a small out queue (single producer, single consumer); on the unit FIZZ's port sends
  it from the audio render, right after the block that played it (`plat_midi_send(…, TO_USB)` may be called there), so
  notes keep the sequencer's timing; on the desktop `fm1core_qd_midi_out_take` serves it with the SysEx replies.
  Before a note-on the port checks `plat_midi_out_room` (room for the offs it owes).

### 5. MIDI tracks

A new setting on track settings page 3: **ENGINE**, either FM TONE or MIDI. A MIDI track has no sound engine:

- Its notes go out on its MIDI OUT channel. Choosing MIDI sets the channel to the track's number if it is OFF.
- **EDIT opens 7 pages of 4 CCs** instead of the sound editor:
  - SELECT, or black keys B1 … B7, picks the page.
  - KNOB 1–4 set the four values, 0–127. Each change is sent at once.
  - A value sends only once it has been touched, so untouched slots stay silent.
  - Holding a step and turning a knob makes a **p-lock**. That step sends the CC as it plays, and the track's own value
    comes back after it. This works the same as sound locks: LATCH, B11 clears them, and REC with AUTOMATION records
    them.
  - **Choosing a slot's CC number:** SHIFT + the knob. The page's slots default to 4 numbers running in order: page 1 is
    CC 16–19, page 2 is 20–23, and so on. No names: the screen shows the number.
- **Storage:** the 28 values and the CC numbers live in the track's sound record, in place of the FM values. They are
  saved with the project, and a pool or library preset can be a MIDI preset.

## Order of work

1. (a) Channels 15/16 and CC → sound (core/app/app.c midi_play / qd_app_midi_ui; the map in core/app/midi_map.c).
2. (b) The remote: CC and notes into qd_app_button, qd_app_knob and qd_app_key, from the main loop.
3. (c) MIDI OUT: a per-track channel; notes, clock and transport, knob CCs into an out queue that fm1core and FIZZ's
   port send.
4. (d) Mockups of the MIDI track editor, for the owner. (e) Then the MIDI track itself: the engine switch, 28 lockable
   values, sending at step time.
5. Tests for each part (core/tests/test_midi.c), then the guide (site/guide.html) and the changelog.
