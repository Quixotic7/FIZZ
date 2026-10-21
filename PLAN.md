# FIZZ: a step sequencer for the FM TONE engine on the M-VAVE FM-1 (plan, 2026-10-17; named FIZZ 2026-10-19)

**The name is FIZZ** (the owner, 2026-10-19; "QUADRILLE" below is the working name it replaces). **Status
(2026-10-19): Phase 1 in progress, the prototype plays on the unit (§13's Phase 1 log).** The original status line:
**Status: a plan and mock-ups. Nothing is built.** No firmware code, no device access, no remote. The folder name is a
placeholder (names in §0). **How it gets built (2026-10-18): Phase 1 is a prototype on the computer with the real
FM-1 as its panel through RemoteFM1; Phase 2 makes it firmware (§13).**

The owner's brief: "I want a firmware based on the new transit one that brings in just the FM-tone engine. The whole
firmware focus is going to be on step sequencing this engine. We can have 4 to 8 tracks that all use this engine, the
voices shared between the tracks. I want to be able to step sequence each track using the 16 white keys on the keyboard
and I want to be able to hold a step and use the top row to set notes, velocity, conditions, chance, I want all the
engine parameters to be fully p lockable, I also want to be able to fully lock a preset on to a step so each step can
have a different preset on 1 track. So you could do a whole drum beat on 1 track. Steps should support multiple notes
and microtiming. Each track should be able to support 8 pages of 16 steps. Each track should be able to have its own
rate. For now I just want to plan and mock up ideas, no actual building."

Mock-ups: `mockups/index.html` (open it in a browser; every screen is a 240 x 240 canvas, drawn pixel for pixel and
shown at 2x), PNGs in `mockups/png/` (rendered headless by `mockups/render_png.py`). Three directions: **MOD**
(ChoralRoot's), **RISO** (this plan's own) and **LINE** (after the OP-1's screens, added the same day at the owner's
request: "fully featured but very clean and simple and fun like these OP-1 UI's"; `mockups/line.html`, twenty animated
screens, §9a). Open questions in §10, the build plan in §11.

---

## 0. In one page

| | recommended | why |
| --- | --- | --- |
| Name | **FIZZ** (the owner, 2026-10-19; the working name was QUADRILLE) | the repo is github.com/Quixotic7/FIZZ; the local folder stays `FMToneSeq/` for now |
| Tracks | **8**, all FM TONE | tracks cost almost nothing by themselves (voices are the cost, mixed per voice, §2); 8 fits the 8 black keys that select them and the 8 pages |
| Voices | **16, shared**: 8 rendered on each core; a per-track limit 1..16 (1 = mono); the single-core package 8 | FM TONE's worst preset measured 176 us a voice on the unit; 8 a core with all three effects is about 76 % of the worst half (§3) |
| Step | 12 bytes: trig type, up to **4 notes**, velocity, length, micro-timing (±23/24 of a step), ratchet x1..x8 with a velocity ramp, condition, chance 0..100 %, a **sound (preset) lock**, a strum, a has-locks bit | §4 |
| P-locks | every one of FM TONE's 73 values and the per-voice mix (level, pan, three sends): 4 bytes a lock, a pool of 1024 a pattern | §5 |
| Preset lock + p-locks | the step's sound replaces the track's sound in the voice, then the step's locks go on top, then the LFOs and the track's perform offsets at render | §5 |
| Pattern | 8 tracks x up to 128 steps (8 pages of 16), per-track rate, length, swing on/off; 16.3 KB in RAM, about 2 KB typical in flash | §6, §7 |
| Saving | never an erase while anything sounds: page programs only (each under one audio half) into sectors erased ahead when the unit is quiet | §7 |
| "Hold a step and use the top row" | **the black keys** (the keybed's top row): 11 fields while a step is held | §8 |
| Development | **Phase 1** on the Mac: an `.fm1core` with no firmware limits, the unit as its panel through RemoteFM1 (FM1VST's Standalone or `fm1remote`); **Phase 2** the firmware, once the design has settled | §13 |
| The look | **LINE**: black, thin round-ended lines, one drawing a screen, **colour = the knob that turns it** (KNOB 1 blue, 2 green, 3 white, 4 orange), red only for the playhead and recording | §9a |

## 1. The base

**TransitFM1 is the chassis; ChoralRoot gives the engine, the audio and the parts around it.** TransitFM1 is
ChoralRoot's boot stub and update order on Dinghy's platform with the sound taken out (56 KB). QUADRILLE puts the sound
back, but only FM TONE's.

### From TransitFM1 (`../TransitFM1`, GPL-3.0-only)

| what | files | why keep it as is |
| --- | --- | --- |
| the boot stub, byte for byte (version 3, sha256 `baee55ed…`, `stub.lock`) | `firmware/stub/`, `firmware/crt0.S` | the same sector 0x4000 as ChoralRoot and TransitFM1: hops between the three have no unprotected cut (TransitFM1 README's table) |
| the update loader and its order (staged at 0xE0000, the foreign order: 0x4000 last) | `firmware/loader/`, `firmware/src/ota.c` (own loader staged, kept, restaged) | a QUADRILLE install never bricks; leaving it for any firmware is TransitFM1's safe path |
| the platform (Dinghy's: boot guard, USB rescue, updater, crash screen, keys, LCD) | `firmware/src/platform.c`, `usb.c`, `recovery.c`, `lcd.c`, `gfx.c`, `libc.c`, `bootguard.h`, `firmware/hal/*` | already the minimal set; TransitFM1's `FELUCCA_UAC 0` / `FELUCCA_UART 0` are switched back on |
| the build and the package | `tools/build.py`, `tools/fm1pkg_make.py`, `tools/lz4blk.py`, `tools/fm1_install.py`, `tests/` (stub, ota, ldr, package) | the marker, the CRC compensation, the host interruption model |

What changes in it: the identity (TransitFM1 answers `FM-1_015` so that every installer takes it; QUADRILLE should
answer as a Felucca-family firmware with its own version string: question 15), `transit.c` is replaced by the app,
the flash windows (`FL_RANGE_OK`) open the data sectors of §7.

### From ChoralRoot (`../ChoralRootFM1`, GPL-3.0-only; read only, nothing changed there)

| what | files | notes |
| --- | --- | --- |
| **FM TONE** (code name QUAD) | `firmware/src/eng_quad.c`, `quad_tables.h` (HARM halves, 13.5 KB), `quad_times.h` (the Digitone's measured times), `tests/quad_ref.py`, `tests/cr_quad_test.c`, `tests/quad_pops.c`, `tests/quad_goldens/` | one change of substance: the voice reads **its own patch** (`p = s->patch` instead of `quad_patch[part]`), §5. Everything else (algorithms, HARM mirror, LFO destinations, the pops fixes, levels) as on ChoralRoot main |
| the patch format and the store | `quad_store.c`, the blob version 5 (96 B, `quad_pack` / `quad_unpack`, versions 1..4 read) | sounds move between ChoralRoot and QUADRILLE as blobs; QUADRILLE can read ChoralRoot's FM TONE sounds (question 24) |
| the `.dnsnd` import | `web/fm1dnsnd.js`, docs/SOUNDS.md | the owner's Digitone sounds, through the web tool, as today |
| dsp | `dsp.c` (the trapezoidal SVF, `tsvf`) | used by QUAD's filters |
| the audio ISR and the shed | `audio.c` (half buffers of 128 frames, `shed_check`), `fx.c` (mix, DIST, master), `master.c` | the shed is the guard behind the voice budget |
| the voice envelope and fade helpers | parts of `voice.c` (`env_tick`, `voice_kill`, the fade steal) | the allocator itself is new (a pool shared by tracks, §2); Melodee's voice model credited as in ChoralRoot |
| **effects** (recommended): chorus (stereo), delay at the half rate, reverb (its lines on core 1), the Clean output chain with its look-ahead limiter, DIST | `cr_fx.c`, `fx.c` | FM TONE's LFO destinations already include DRIVE, REV, DEL, CHO: they keep their meaning |
| **the second core** | `hal/fm1_cpu1.{h,S}`, `hal/fm1_power.h`, `hal/fm1_guard.h`, `cr_cpu1.h`, `fx.c mix_parts_split`; the voice split of branch `claude/reverent-burnell-8b8084` (HISTORY 3y177, 3y187: `engine_t.split`, `p5x_assign`, the scratch chosen by core) | QUADRILLE renders every voice of one engine: the split is simpler than ChoralRoot's (no bass part) |
| code in RAM | `cr_ram.c`, `tools/ram_code.py`, the `.ram_text` section (RAMTEXT 24 KB) | `quad_render` in RAM (§3): P5 lost 26 % of its part time that way on the unit |
| velocity and aftertouch from the keys | `cr_sense.c`, `hal/fm1_input.h`, `hal/fm1_adc.h` (Velo / MONK-1 by Charles Vestal) | the KEYS mode plays with velocity |
| storage | `storage.c` (A/B objects, the commit record last, **erase ahead**), the floating banks of the sound pool (`sp_*`) | the pattern store is a floating page store in the same spirit (§7) |
| MIDI | `midi_uart.c`, `midi_clock.c`, `midi_control.c`, `cr_midi.c` (parts) | clock in / out, a channel a track |
| USB audio (optional) | `usb_audio.c`, `usb_audio_stream.c`, `usb_audio_desc.h`, `uac_fir.h` | recording over USB; stereo first (question 20) |
| the font and the draw primitives | `tools/gen_aa_font.py`, Inter Tight (OFL), `cr_gfx.c` (parts), `cr_draw.c` (bands, stripe meters) | the MOD direction is ChoralRoot's look |
| the Mac emulator and the browser emulator | `tools/emu/`, `tools/emu/web/` | adapted for the app: headless scripts as ChoralRoot's |

### Licences and credits (to carry into LICENSING.md)

All GPL-3.0-only, as Felucca, SLOOP, Dinghy, ChoralRoot and TransitFM1. Credits, as the code comes in:

- **Felucca**, Leo Kuroshita (@kurogedelic), Hügelton Instruments: the platform, the loader, `storage.c`, `fx.c`.
- **SLOOP** (isod89): the platform as it carries it; USB audio at 48 kHz; the designs of the look-ahead limiter and the
  stereo chorus.
- **Dinghy** (zvenson): `platform.c` as TransitFM1 has it. Dinghy's logo is not part of the tree.
- **ChoralRoot FM-1 contributors** (Quixotic7): the boot stub, the order, FM TONE (QUAD), the effects as they are now,
  erase ahead, the dual-core voice split, code in RAM.
- **TransitFM1 contributors**: the foreign order, the restaged loader.
- **Melodee** (Kerem Kilic, Ellic Studio): the voice model (slots, budget in units), USB audio (UAC1 recording), the
  dual-core reliability work (PR 21) and the ideas of the split.
- **fm1-x0x**: the second core's bring-up (after Melodee and JieLi's `EnableOtherCpu`, Apache-2.0, restated).
- **Velo / MONK-1** (Charles Vestal): velocity and aftertouch from the keys.
- **Inter Tight** (OFL 1.1), **Terminus** (OFL 1.1) if the small font stays.
- **JieLi AC79 SDK** (Apache-2.0): three files read into the package at build time.
- **Elektron**: the Digitone is the model of FM TONE (its manual, our measurements, no Elektron code) and the Elektron
  sequencer is the model of the step grammar (locks, conditions, trigless trigs). Names on the device are our own;
  "Digitone" is never shown.

Not taken: every other ChoralRoot engine, the chord engine and its keys, the looper (its step model's vocabulary is
reused: ratchet, condition, chance, as the owner already knows them), the settings v15 record, the session, the
favourites, the sound pool's floating banks' format (their idea is reused).

## 2. Tracks and voices

### Recommendation: 8 tracks, 16 shared voices

**8 tracks.** What a track costs:

| per track | cost |
| --- | --- |
| CPU, the sequencer | per tick (96 a beat at the track's rate): a compare against the next step's time; per step: the trig's resolution, done ahead in the main loop (§5). Under 1 us a track a half |
| CPU, the audio | nothing per track: voices are mixed **per voice** into the stereo bus and the three sends (level x pan x send gains, about 5 us a voice a half). A track has a bus only while its DRIVE is above 0 (DIST then runs once for the track: 30 .. 50 us a half, as ChoralRoot's `track_dist`) |
| RAM / POOL | its sound (73 x int16 = 146 B), its settings (32 B), its free LFO phases (24 B); in a pattern its 128 steps (1.5 KB) and its share of the lock pool |

So 4 or 8 tracks cost the same CPU; what 8 costs is 6 KB more of each pattern in POOL and a wider overview screen.
Eight also fits the controls: the eight leftmost black keys select the tracks in one press (§8), the mute mode has
white keys 1..8 to mute and 9..16 to solo, and a drum track with preset locks can sit beside seven melodic ones.

### Voice allocation (new code; the pool is shared, the tracks own their voices while they sound)

A voice is `{owner track, note, velocity, gate, the voice's patch (146 B), its "own" mask (73 bits), QUAD's voice
state (124 B), core}`. A trig on track t with n notes asks for n voices:

1. **The track's limit** L (1..16; the track setup). If t already sounds L voices, the new note takes t's own voice:
   first a released one (the quietest), then the oldest held one. **L is also the choke**: L = 1 cuts the previous
   note at every trig (a one-block fade), as a mono drum track wants; L = 2..4 lets tails overlap.
2. **A free voice** of the pool, on the core with fewer voices (the cores stay balanced 8 + 8).
3. **A steal across tracks**: a released voice first (the quietest of all), then the oldest held note of the track
   that is furthest over its **reserve** (a per-track minimum, 0 by default, 1 for a bass you never want to lose).
   A track at or under its reserve is not robbed.
4. A stolen voice fades over one control block (32 samples, 0.73 ms: ChoralRoot's fade steal; the kink is the
   one remaining click source docs/QUAD.md "Pops" names) while the new note starts in the same voice slot at the next
   block. **Question 25**: a 3 ms fade for stolen voices (four blocks; the new note waits 2 ms) for fewer clicks.

**Same note again, same track**: with a voice still sounding on that note, the voice is re-struck in place (QUAD's
PHRT mask avoids the pops, docs/QUAD.md "Pops") **only if the new step plays the same sound** (the same preset lock,
or none). A step with another sound never re-strikes a sounding voice: it takes a fresh one and releases the old one
by its own envelope, so the patch of a sounding voice is never swapped (the click risk of §5).

**Mono / poly** is the limit: MONO = L 1 with legato on TIE steps (the next note moves the voice, `quad_legato`, the
operator envelopes restart only with TRIG on, as the Digitone's "every note"); POLY = L 2..16. Unison is not planned
(question 21).

## 3. The voice budget

**Measured on the unit** (docs/INTEGRATION.md "Measured on the unit", 2026-10-14, 0.22, Dual Core On with the bass on
core 1; us of the 2902 us half):

| FM TONE: WOBBLE (the heaviest preset: LP24, two LFOs) + BASS | block | part0 (the chord's render) | fx | tick | master | TIMER5 |
| --- | --- | --- | --- | --- | --- | --- |
| 8 notes + bass | 2345 (81 %) | 1509 | 642 | 237 | 103 | 170 |
| 6 notes + bass | 1993 (69 %) | 1157 | 645 | | | 145 |

So a WOBBLE voice costs **176 us** on core 0 (1509 - 1157 over 2 voices; the part's own work about 100). The 16
presets average 79 % of WOBBLE's host cost (`tests/run_quad_test.sh`: 1491 against 1880 instructions a sample), about
**140 us** a voice. The voice split on the unit (branch claude/reverent-burnell-8b8084, HISTORY 3y187, package C):
FM6 TINE EP's 8 voices 4 + 4 took its worst half from 2468 to 2012 us dry (85 -> 69 %), and core 1 rendered its four
voices at about 140 us each against about 190 on core 0. Core 1 is not slower per voice.

**QUADRILLE's half, estimated** (16 voices, 8 a core, three effects with the reverb's lines on core 1, dry, no USB
recording). The window in which voices overlap is the same on both cores (hand-over to join 1, DUALCORE.md "What each
core runs"), so the half is core 0's fixed work plus the longer of the two voice sides:

| core 0 | WOBBLE-class, flash | typical, flash | WOBBLE-class, `quad_render` in RAM |
| --- | --- | --- | --- |
| tick, sequencer, events, voice starts (ChoralRoot's tick peaked 237 .. 354 at a chord change; the trigs here are resolved ahead) | 250 | 250 | 250 |
| 8 voices | 1408 | 1120 | 1040 (x 0.74, P5's measured gain) |
| per-voice mix (16 x 5) | 80 | 80 | 80 |
| chorus, delay, the up-sampler (the fx stage 480 .. 640 less the reverb's ~200 and the parts' loops) | 300 | 300 | 300 |
| master and limiter | 110 | 110 | 110 |
| TIMER5's ticks nested in the half | 170 | 170 | 170 |
| **the worst half** | **2318 (80 %)** | **2030 (70 %)** | **1950 (67 %)** |
| core 1: 8 voices, then the reverb's lines (~250) | 1660 | 1370 | 1290 |

- **16 voices fit** at about 80 % of the worst half with eight WOBBLE-class voices a core in flash, 70 % typical, 67 %
  with the render in RAM. **Recommendation: 16 voices, `quad_render` in RAM** (build step 6), the shed as the guard.
- **Recording over USB at 44.1 kHz** adds 150 .. 240 us on core 0 (TIMER5's endpoint service; INTEGRATION.md): 85 %
  worst in flash, 75 % in RAM.
- **Per-voice DRIVE** (the Digitone's order, FM -> drive -> filters) would add about 32 us a driven voice (DIST's loop):
  eight driven voices a core +256 us. Recommendation: DRIVE as a **track** insert (§2), question 13.
- **The single-core package** (and Dual Core Off): one core renders all voices and all effects: 250 + n x 176 + 80 +
  480 + 110 + 170 = 2498 us (86 %) for n = 8 WOBBLE-class, 2210 (76 %) typical. **8 voices** on one core, the shed
  thinning the rare worst case.
- **What the estimate does not know yet**: the sequencer's own peak at a busy step (8 tracks x 4 notes = 32 note-ons
  in one tick are capped at 16 voices, but the steals and fades of a full pool all land in that block), the ghosts of
  released voices after a big step, and how the shed behaves when every voice is one engine. Build step 5 reads them
  (`perf_unit`-style scenario: 16 WOBBLE voices on 8 tracks, three sends up, a step every 16th at 160 BPM).

**The budget in the allocator**: voices are counted in units, a QUAD voice 2 units as ChoralRoot counts it, 16 units a
core; a p-lock that switches a voice to LP24 or turns all three LFOs on does not change its units (the shed covers the
excess). **Question 5**: whether to weigh a voice by its patch (filter type, LFOs on, HARM in use: each known at the
trig) so the allocator steals before the shed has to.

## 4. The step model

```c
typedef struct {            /* 12 bytes, a step */
    uint8_t flags;          /* bit 0-1 type: 0 none, 1 NOTE, 2 LOCK (trigless: locks only), 3 TIE (legato into
                             * the sounding voice); bit 2-4 ratchet - 1 (x1..x8); bit 5-6 ratchet ramp (flat,
                             * up, down, accent first); bit 7 has locks */
    uint8_t note[4];        /* note 1 absolute (1..127, 0 = none); notes 2..4 as offsets +1..+48 from note 1
                             * (0 = none): a chord keeps its shape when note 1 is moved */
    uint8_t vel;            /* 1..127 */
    uint8_t len;            /* an index: 1/8, 1/4, 1/2, 3/4 of a step, 1, 1.5, 2, 3, 4, 6, 8 .. 128 steps, INF */
    int8_t  micro;          /* -23 .. +23: 24ths of a step, at the track's rate */
    uint8_t cond;           /* 0 ALWAYS, FILL, NOT FILL, PRE, NOT PRE, NEI, NOT NEI, 1ST, NOT 1ST, A:B for B = 2..8 */
    uint8_t chance;         /* 0 .. 100 % */
    uint8_t sound;          /* the sound lock: 0 none, 1 INIT, 2..17 the factory sounds, 18.. the user sounds */
    uint8_t strum;          /* the chord's notes this many 24ths apart (0 together; ratchets strum every hit) */
} step_t;
```

| field | range | what it does | as on |
| --- | --- | --- | --- |
| type | NOTE, LOCK, TIE | NOTE strikes; LOCK changes the track's sounding voices at that time without striking (the trigless trig); TIE moves a MONO track's voice to the new notes, legato | the Elektron sequencer |
| notes | up to 4 | a chord on one step; voices as notes, within the track's limit | the Digitone's NOTE + 3 offsets |
| velocity | 1..127 | into FM TONE's VEL scaling and the amplitude | |
| length | 1/8 step .. 128 steps, INF | the gate; INF holds until the track's next NOTE | the Digitone's LEN |
| micro-timing | ±23/24 of a step | the step's notes earlier or later; a negative one on step 1 fires in the last step of the pass before (the trig is resolved two steps ahead) | Elektron's micro timing |
| strum | 0..23 | spreads a chord's notes in 24ths after the micro-timed start | new |
| ratchet | x1..x8, a ramp | ChoralRoot's ratchet: the hits evenly inside one step, the last one holds to the length (the owner's rule, LOOPER.md); the ramp shapes the hits' velocities | ChoralRoot's loop editor (it has x2..x4) |
| condition | 44 values | `A:B` = pass A of every B passes **of the track's own loop** (tracks of different lengths count their own passes); 1ST / NOT 1ST; PRE / NOT PRE = the last conditional step of this track played / did not; NEI / NOT NEI = the same for the track to its left; FILL / NOT FILL while FILL is held | ChoralRoot's (1:2 .. 4:4, 1st) and Elektron's |
| chance | 0..100 % | after the condition: one roll per step per pass (all notes and every ratchet hit come or stay away together) | ChoralRoot's, in 1 % steps |
| sound lock | any sound of the pool | the whole patch for this step's voices (§5) | the brief; the Digitakt's "sound lock" |
| locks | a count | the step's p-locks in the pattern's lock pool | |

The **track defaults** (in the track's settings, 32 B): the default note, velocity and length a new trig gets (the
last ones entered), the MIDI channel, the limit and reserve, the rate, the length, swing on / off, transpose.

## 5. P-locks

### What can be locked

**Every value of FM TONE's patch: all 73** (ALGO, the four ratios and their offsets, HARM, DTUNE, FDBK, MIX, both
operator envelopes with delays, trig and reset, PHRT, VEL, the three KEYs, the filter's envelope, FREQ, RESO, TYPE,
DEPTH, delay and key track, BASE, WIDTH, the amp envelope, LEVEL, and the three LFOs' eight values each), plus **the
voice's mix**: PAN, the delay, reverb and chorus sends, and the track-level DRIVE (a track insert: a lock on it sets the
track's drive at that step for all its voices). Lock ids 0..72 are the patch's value indices (`QP_*`), 73..79 the mix.

```c
typedef struct { uint8_t step, id; int16_t value; } lock_t;   /* 4 bytes; the hi-res values in hundredths as stored */
```

A pattern has **one pool of 1024 locks**, kept sorted by track then step (a track's range: a start and a count in the
pattern header). That is 128 locks a track on average and far more on one: a drum track with a sound lock on each of
32 steps and four locks on each uses 128. The pool is full: `no room for locks` on screen, and the lock is not made.

### How a step's sound, locks and live edits combine

```
the voice's patch at the trig  =  the step's SOUND if it has one, else the track's sound (as it is now)
                                  then every lock of the step, value by value
the voice's "own" mask         =  all 73 bits with a SOUND lock, else the bits of its locks
while the voice sounds         =  a live edit of the track's sound reaches the voice's values that are not "own"
                                  a LOCK (trigless) step of the track writes its locks into the track's sounding voices
at the render (each 32-sample block)
                               =  the patch values + the LFOs' contributions + the track's perform offsets (dst[])
```

- **A p-lock wins over the sound lock** (it is applied after it): a whole drum kit on one track is SOUND locks, and a
  ghost note is the same SOUND with a LEVEL and FREQ lock.
- **A live knob turn on the track changes unlocked values of sounding voices too** (as on the Digitone: a filter sweep
  is heard on a held pad). A sound-locked step is not changed by the track's edits: its sound is its own. To sweep a
  whole drum track, the track has **perform offsets**: four relative offsets (FREQ, DECAY, HARM, FDBK by default),
  added at the render like an LFO's contribution, so they move every voice of the track, sound-locked or not (question
  12). The engine already sums 57 destinations a voice (`dst[]` in `quad_render`); the offsets are four more addends.
- **The hi-res values** (HARM, DTUNE, FREQ, RESO, the depths, BASE, SPEED) lock in hundredths, as the patch stores them.

### How a lock reaches a voice, and what it costs

FM TONE already reads its patch every control block (32 samples): the envelopes' times, the levels, the filter
coefficients, the LFOs. So a voice whose patch is its own needs nothing new in the render: `p = s->patch`.

1. **Ahead of time, in the main loop** (not the audio ISR): the sequencer resolves the trigs two steps ahead (micro-
   timing can be -23/24): for each trig it fills a pending record: the 146 B patch (a copy of the sound, unpacked
   sounds are kept as int16 arrays in POOL so a sound lock is a `memcpy`), the locks written over it (a few
   instructions each), the mask, the notes, velocity, gate, its sample time. About 2 .. 5 us a trig.
2. **In the audio ISR**, at the trig's control block: the voice is taken (§2), the pending patch copied into it (146 B,
   under 1 us) and `quad_note_on` runs as today. A step of 8 tracks x 4 notes is at most 16 voice starts.
3. A live edit or a trigless LOCK step: the value is written into each sounding voice of the track whose bit is not
   "own" (LOCK: and sets the bit). At most 16 stores a value.

### Click risk, and what keeps it away

| change on a voice that sounds | risk | what happens |
| --- | --- | --- |
| a new trig with another sound (preset lock) | the routing, phases and filter state of one patch under another's: a step | never done: a fresh voice takes it, the old one releases (§2) |
| a level (A LEV, B LEV, MIX, LEVEL) | a step | none: QUAD ramps the operator levels and MIX per sample over the block (docs/QUAD.md "Pops") |
| FREQ, RESO, BASE, WIDTH | a zipper of one step a block | as any knob turn today; the filter switching in or out crossfades its block (`fon`) |
| ratios, offsets, DTUNE, HARM | timbre jumps, no wave step (the phases run on) | accepted: it is what the lock asks for |
| ALGO, PHRT, the filter TYPE, an LFO's WAVE / TRIG (a LOCK step or a live edit) | a step: the routing changes under the sound | the voice dips: its output ramps to 0 over one block and back over the next (1.5 ms), **new; question 26** |
| an envelope time | none | the envelope reads its times each tick: the stage in progress continues at the new rate |

### Locks and the LFOs and envelopes

- **An LFO moves around the locked value**, as around the track's: the lock is the patch value, the LFO's
  contribution is added at the render. A lock on an LFO's own values (speed, depth, destination, wave) applies to that
  voice's LFO from its trig: TRIG / ONE / HALF LFOs start at the note, so a per-step LFO is exact.
- **FREE LFOs run per track** (their phase in the track, as `quad_lfo[part]` is per part today). A lock on a FREE
  LFO's SPEED gives that voice its own lead (the `lof` mechanism LFO 1 already has for its modulated speed): it drifts
  from the track's phase. **Question 23**: accept, or make a locked SPEED switch that voice's LFO to TRIG.
- **Envelopes start at the trig** with the voice's locked times and levels. A LOCK (trigless) step changes a sounding
  voice's envelope times from that moment (the stage continues at the new rate), never restarts it.
- **Velocity** goes through VEL as today; a lock on VEL (the patch's velocity sensitivity) works per step.

## 6. Patterns

- **8 tracks x 8 pages x 16 steps**. A track's **length** is 1..128 steps (pages beyond it are dark); its **rate** is
  1/8, 1/4, 1/2, 3/4, 1, 3/2 or 2 times the pattern's (16th notes at 1; the Digitone's seven; question 14 for 1/3,
  2/3, 4/3); each track loops on its own length (polymeter).
- **The pattern's master length** (16 .. 1024 steps of 1, or INF) sets when a queued pattern takes over and when a
  chain moves on; by default the longest track at its rate.
- **Swing** 50..80 % for the pattern (GLO + SELECT), on or off a track; it delays every second step of the track at
  its own rate.
- **Tempo** 30..300 BPM for the pattern (SELECT), or MIDI clock in.
- **Pattern change**: queued to the end of the master length (the screen counts down), or at once with GLO held.
- **Chains**: hold PATTERN and press pattern keys in order: up to 16 patterns, looped.
- **Songs** (later, question 19): rows of (pattern, repeats, mute mask, tempo) in an object of their own.
- **Banks**: 4 banks (A..D) of 16 = 64 patterns in a project (question 8).
- **FILL**: REC held in the step view (§8); FILL / NOT FILL conditions follow it.

## 7. Memory

### RAM and POOL

| what | bytes | where |
| --- | --- | --- |
| a step | 12 | |
| a track's steps (128) | 1536 | |
| a pattern: 8 tracks' steps 12 288 + track settings 8 x 32 + the lock pool 1024 x 4 + header 64 + lock index 32 | **16 736** | |
| the playing pattern + the next one (queued, loaded ahead) | 33 472 | POOL |
| a clipboard (a pattern, or less) | 16 736 | POOL |
| the dirty-pattern cache (four patterns kept until written: §7 "Saving") | 66 944 | POOL |
| 16 voices: QUAD state 124 + patch 146 + mask 10 + `voice_t` ~40 | ~5 120 | POOL |
| the sounds unpacked (INIT, 16 factory, up to 64 user: 81 x 146) | 11 826 | POOL |
| pending trigs (2 steps ahead, up to 64) x ~170 | ~10 900 | POOL |
| the effects as ChoralRoot's (the delay lines at the half rate 64 KB, the reverb 32 + 8 KB, chorus 4 KB) | ~108 000 | POOL |
| **POOL total** | **~270 KB of 336** | about 66 KB spare |
| RAM (96 KB): stacks, both cores' (core 1 2.8 KB), the platform, the UI's state, the frame's scratch | ~45 .. 60 KB (ChoralRoot's 80 KB holds far more UI) | RAM |
| RAMTEXT (24 KB) | `quad_render` (6 .. 16 KB), the voice mix | RAM |
| XIP code: the platform ~56 KB, USB audio ~15, FM TONE ~15 + HARM 13.5 + times 2, effects ~20, audio and voices ~15, the sequencer ~25, the UI and the draw ~40, the font tables ~50 | **~250 .. 300 KB of 573** | flash |

### Flash for patterns and sounds

**Sparse in flash, fixed in RAM.** A pattern record: the header (64 B), per track its settings (32 B), a page mask
(1 B), per used page a step mask (2 B) and only the non-empty steps (12 B each), then the track's locks (4 B each).
A typical pattern (8 tracks, 1 .. 2 pages each, about 160 trigs, 100 locks) is **about 2 KB**; a full one (1024 steps
and 1024 locks) 16.8 KB. Records go into a **floating page store**: sectors of 4 KB (a 256 B sector header and 15
pages of 256 B), a record a run of pages, the commit page last, an older version stale once the new one is committed.

| | coexist with ChoralRoot (recommended first) | the whole data region |
| --- | --- | --- |
| sectors | 0x97000..0xB3FFF (29) and 0xDC000..0xDFFFF (4): ChoralRoot's **legacy** stores, read once by its migration, never written since 0.22 (to be confirmed: question 6) | 0x97000..0xDFFFF (73) |
| what is left of ChoralRoot | its settings (0xFC000, the panel calibration), session, favourites, sound pool, loops: untouched; QUADRILLE can read its FM TONE sounds | nothing (QUADRILLE asks first; a backup with the web manager before) |
| settings, user sounds | 0xDC000 / 0xDD000 settings, 0xDE000 / 0xDF000 64 user sounds (96 B blobs, 2 objects) | the same, elsewhere |
| pattern store | 29 sectors, 5 kept blank for the largest record: 24 x 3840 = 92 KB | 66 x 3840 = 253 KB |
| patterns | **about 45 typical**, 5 full ones | about 128 typical, 15 full |

A flash meter on the save screen shows the store's use; a save that does not fit says `pattern store full` and keeps
the pattern in RAM.

### Saving with no flash writes that could cause drop-outs while playing

docs/SETTINGS.md "When flash is written": a **sector erase masks every interrupt for 17.8 ms** (six audio halves
nobody serves); **a page program masks them for about 1 ms** (not yet measured on the unit: `flash_prog_us`), under one
half. So:

1. **No erase while anything sounds or the transport runs.** Sectors are made blank ahead: at power-on before the audio
   starts, and after 10 s with the transport stopped, no voice sounding and no input (ChoralRoot's `ST_AHEAD_IDLE_MS`).
   The store always keeps 5 blank sectors (a full pattern's record) when it can.
2. **A save while playing is page programs only**, into those blank sectors, **one page per UI frame, right after an
   audio half has been served**: a typical pattern is 8 .. 10 pages (about 0.3 s), a full one 66 pages (about 2 s).
   Core 1 is idle in RAM outside the audio ISR (ChoralRoot's rule), so the flash going off under XIP touches no audio.
3. **Garbage collection** (copying live records out of a mostly stale sector, then erasing it) runs only under rule 1.
4. **When the blank sectors run out while playing**, the pattern stays in the dirty-pattern cache in POOL (up to four
   patterns) and is written at the next quiet moment; the screen says `saving when stopped`. Never an erase to make
   room.
5. **The counters** (as ChoralRoot's 78 PEAK): `erases_near` (an erase while sounding or within 2 s of an input: 0 is
   the rule), `saves_playing`, `pages_playing`, `flash_prog_us`, `stale_halves`. Build step 8 checks them on the unit
   after saving while 16 voices play.

**Power cuts**: the record's commit page last; a record without one is stale; a sector half erased is erased again at
power-on. The host test walks a cut after every erase and every page (as `tests/pool_test.c`).

**What is saved when** (question 7): recommended, **the working copy is written by itself** (1.5 s after the last edit,
by the rules above), and **SAVE** makes a restore point of the pattern; GLO + SAVE held reverts to it (ChoralRoot's
gesture). The alternative is ChoralRoot's manual model (nothing written until SAVE held).

## 8. The controls

### Decision: the "top row" is the black keys

| | black keys (11, the keybed's top row) | the top button row (6) |
| --- | --- | --- |
| reach while a white step is held | the same hand or the other, on the keybed, no travel | the other hand, across the panel |
| room | 11: every field gets its own key, and the sound editor's 11 screens fit them one for one | 6: fields share buttons, or the buttons lose their own jobs |
| feedback | each key has an LED: the field shown, the fields with a value set | the buttons' LEDs only |
| cost | the keybed is not a keyboard in the step view (it is in KEYS mode) | none |

**The black keys** it is. The six top buttons keep jobs that must work with nothing held (tracks, patterns, pages,
mutes, the sound, FUNC). This also follows ChoralRoot's decision of 2026-10-06 in spirit: the printed black-key labels
(OP1..OP6, PIT, GLO, MONO, POLY) are not used; what a key does is on the screen.

### The keybed in the step view

```
black   T1    T2    T3          T4    T5          T6    T7    T8          pg-   pg+          (nothing held)
black   NOTE  VEL   LEN         MICRO RATCH       COND  CHNC  TRIG        SOUND LOCKS        CLEAR   (a step held)
black   SYN1  SYN2  ENV A/B     ENV2  ENV3        FILT  FILT2 AMP         LFO1  LFO2   LFO3          (the sound editor)
        F#3   G#3   A#3         C#4   D#4         F#4   G#4   A#4         C#5   D#5   F#5
white  1     2     3     4     5     6     7     8     9    10    11    12    13    14    15    16   the page's steps
       F3    G3    A3    B3    C4    D4    E4    F4    G4   A4    B4    C5    D5    E5    F5    G5
```

The black keys' groups of 3 | 2 | 3 | 2 | 1 carry the fields' groups: what (NOTE VEL LEN), when (MICRO RATCHET),
whether (COND CHANCE TRIG), sound (SOUND LOCKS), and CLEAR alone at the right end. In the sound editor the same keys
are FM TONE's 11 screens in signal order (ChoralRoot's QUAD screen plan has exactly 11: SYN 1, SYN 2, ENV A/B, ENV 2,
ENV 3, FILTER, FILTER 2, AMP, LFO 1, LFO 2, LFO 3).

**White keys (the steps)**:

| gesture | does |
| --- | --- |
| tap (under 250 ms, nothing turned) | a NOTE trig with the track's default note, velocity and length; on a trig: removes it and its locks |
| hold | the step is **held**: the screen is the step's editor, the black keys its fields, every knob turn edits the step (a knob of a sound value makes a p-lock); release: back |
| hold two or more | the edits go to all of them |
| hold, then OCT+ | **latches** the held step(s): they stay selected after release (their keys blink) so both hands are free; OCT- or HOME lets go (question 10) |
| GLO + tap | a LOCK (trigless) step, or back to empty |
| turn a sound knob while an empty step is held | makes a LOCK step with that lock (Elektron's habit) |

**Black keys with a step held** (each opens one big field; SELECT and KNOB 1 turn the big value):

| key | field | the screen | KNOB 1 .. 4 |
| --- | --- | --- | --- |
| F#3 | **NOTE** | the chord name big, its notes under it | note 1, note 2, note 3, note 4 (2..4 as offsets; SELECT picks a chord shape) |
| G#3 | **VEL** | the velocity big, a stripe meter | velocity, ratchet ramp |
| A#3 | **LEN** | the length big, the bar over the next steps | length, TIE on / off |
| C#4 | **MICRO** | the step's span in 24ths with the note's start on it | micro, strum |
| D#4 | **RATCH** | x1..x8, the hits drawn in the step | count, ramp |
| F#4 | **COND** | `3:4` big, the passes as a ring | condition, chance |
| G#4 | **CHANCE** | the percent big, a dot field that thins | chance, condition |
| A#4 | **TRIG** | NOTE / LOCK / TIE | type |
| C#5 | **SOUND** | the sound lock as a picker (one name big, neighbours peeking) | sound; PRESETS does the same with a step held |
| D#5 | **LOCKS** | the step's locks as a picker: one lock big (name, value, the track's value ghosted), the others peeking | the lock's value; GLO + KNOB 1 removes it |
| F#5 | **CLEAR** | tap: the held step(s) cleared (notes, sound, locks); the same tap again within 4 s: undone | |

LEDs with a step held: the current field lit, the fields that hold a value other than the default dim (a condition, a
chance under 100 %, locks, a sound), the rest faint. In the sound editor with a step held: the current screen lit,
the screens that have a lock on the held step dim.

### KEYS mode (ARP, printed)

The keybed is a chromatic keyboard for the selected track, F3..G5 shifted by OCT- / OCT+, with velocity (Velo's
sensing). With the transport stopped and REC on: **step recording**: each chord played goes into the cursor's step and
the cursor moves on (OCT+: a rest; PRESETS: the cursor). Playing with REC on: **live recording** into the steps, with
the micro-timing played (GLO + REC: quantize to the step), knob turns recorded as locks. MIDI notes in on the
track's channel do the same; **MIDI notes in while a step is held become the step's notes** (an external keyboard
for step entry).

### Buttons (printed -> QUADRILLE)

| printed | role | tap | hold | with GLO (FUNC) |
| --- | --- | --- | --- | --- |
| FX | **TRACKS** | the track overview (8 lanes; KNOB 1..4 the selected track's LEN, RATE, LIMIT, SWING) | the overview while held | GLO + TRACKS + black key: copy the track's sound |
| SEL | **PATTERN** | the pattern screen (latched) | + white key: queue that pattern; + several in order: a chain; + black key 1..4: bank A..D | + white key: change at once |
| ENV | **PAGE** | the next page of the track | + white key 1..8: that page | page follow on / off |
| LFO | **MUTE** | mute mode, latched: white 1..8 mute the tracks, 9..16 solo them | the same while held | clear every mute |
| EDIT | **SOUND** | FM TONE's editor for the track: black keys = its 11 screens; a held step turns every edit into its lock | the sound picker | |
| GLO | **FUNC** | latch (the button lit) | the shift | |
| HOME | **HOME** | the step view; from the step view: the next view (steps / overview) | | GLO + HOME: CLEAR what is held (steps, PAGE, TRACK, PATTERN; none: the page), OCT+ to confirm a track or a pattern |
| SAVE | **SAVE** | the pattern's restore point; in the sound editor: save the sound | save the project now | GLO + SAVE held 1 s: revert the pattern |
| ARP | **KEYS** | KEYS mode on / off (lit) | | |
| SEQ | **MIX** | the mix: the tracks' levels, pans, sends; again: the effects (delay, reverb, chorus, master) | | |
| PLAY | **PLAY** | play / stop | | GLO + PLAY: PASTE |
| REC | **REC** | KEYS mode: record on / off. Step view: nothing | step view: **FILL** while held | GLO + REC: COPY |
| OCT- / OCT+ | | step view: page - / +; a step held: its notes an octave; KEYS: octave; menus: back / OK | | both: PANIC |

**Copy / paste / clear** act on what is held, as one rule: steps held, PAGE held, TRACKS held, PATTERN held + a key;
nothing held: the page. COPY = GLO + REC, PASTE = GLO + PLAY, CLEAR = GLO + HOME. The same gesture again within 4 s
undoes a paste or a clear (question 17).

### Knobs

| knob | the step view | a step held | in the sound editor |
| --- | --- | --- | --- |
| MASTER | the volume | | |
| SELECT | tempo; GLO: swing | the big field's value | the lane (as ChoralRoot's editor) |
| PRESETS | the track's sound (a picker) | the step's **sound lock** | the sound |
| ALGORITHM | the track's ALGO | an ALGO lock | ALGO |
| KNOB 1..4 | the track's quick page: FREQ, AMP DEC, HARM, FDBK (question 11); GLO: the perform offsets of the same four | the field's cells (above) | the screen's four values; with a step held: locks |

A knob turn shows its value big for a second (ChoralRoot's read-out), never on a step's own screen.

### LEDs

The FM-1 has one colour of LED and three levels (lit, the glow, the backlight; `hal/fm1_input.h`), plus blinking in
software. The mock-ups' keybed diagrams use: **off, faint** (the backlight), **dim** (the glow), **lit, blink, fast
blink**.

| state | white keys | black keys | buttons |
| --- | --- | --- | --- |
| step view, stopped | trigs lit, LOCK steps dim, beyond the length off, others faint | the selected track lit, the others faint | PLAY off |
| step view, playing | as stopped; the playhead's key inverts for the step (a lit one goes dark, a dark one lit) | a track's key flashes on each of its trigs | PLAY blinks on the beat |
| a step held | the held step fast blink | the fields (above) | |
| PATTERN held | patterns with content dim, the current lit, the queued blink | banks A..D: the current lit | PATTERN lit |
| mute mode | 1..8 lit = heard, 9..16 lit = soloed | | MUTE lit; blinks while any track is muted |
| KEYS mode | the notes sounding lit, C keys faint | | KEYS lit; REC lit while recording |

## 8b. The sequencer controls, version 2 (2026-10-18, after the OMX-27 Form sequencer)

**This replaces §8's step view, its held-step fields, the PAGE and REC rows and the KEYS row.** The owner, after
playing P1.3: the black keys choose the hold mode, not the track; with a step held the black keys set that mode's
values, the last one (F#5) clears it back to the track's; a track has default notes, velocity and so on that a step
uses unless it overrides them; OCT− / OCT+ are function keys F1 / F2 (both = F3), GLO is SHIFT; REC switches between
the keyboard and the sequencer, as on Elektron boxes; every page has its own start and end, with shortcuts to loop
one page or several. The model is the owner's Form sequencer for the OMX-27 (v1.15.4), studied in
`research/FORM-SEQUENCER.md`. Order of work: the core sequencer and its UI first, then the sound editor, then track and
pattern management.

### Names

| FM-1 key | role here |
| --- | --- |
| OCT− | **F1** |
| OCT+ | **F2** |
| OCT− and OCT+ together | **F3** |
| GLO | **SHIFT** (Form's AUX) |
| black keys F#3 G#3 A#3, C#4 D#4, F#4 G#4 A#4, C#5 D#5 | **B1..B10** |
| black key F#5 (the rightmost) | **B11, CLEAR** |
| white keys | **W1..W16**, the steps of the page being edited |

### Track defaults and step overrides (the model change)

- A track has **defaults** for: notes (up to 4, a chord), velocity, length, micro, ratchet and ramp, chance, condition,
  trig type, and sound (its own sound).
- **The sound pool** (the owner, 2026-10-18): a project holds up to **128 sounds**. A step's sound lock is one byte,
  a pool index (0 = none: the track's sound). The pool starts with INIT, the 16 factory sounds and the demo drums;
  the sound editor adds to it. A track's own sound is its own patch, which can be stored into the pool.
- A step stores a value **and an override bit** per parameter. A parameter without its bit takes the track's default
  **when the step plays**, so changing a track's default changes every step that does not override it, at once.
- A tapped step has no overrides: it plays the track's defaults.
- Setting a value on a held step overrides that parameter; B11 clears the override (back to the track's).
- p-locks (§5) are unchanged: they are the sound's overrides, per step.

### The step view, nothing held

| control | does |
| --- | --- |
| W tap | an empty step: a trig with no overrides; a trig: removed with its locks |
| W hold (250 ms) | the step is held (below); several can be held |
| **B1..B10** | **the hold mode**: NOTE, VEL, LEN, MICRO, RATCH, CHNC, COND, TRIG, SOUND, LOCKS |
| B11 | nothing held: (question 37) |
| **ALGORITHM** | **the track's default for the current mode** (velocity in VEL, length in LEN, ...; the notes in NOTE) |
| **SELECT** | **the page being edited** (the owner: SELECT changes pages) |
| KNOB 1..4, PRESETS | the track's sound, as in P1.3 (FREQ, DECAY, HARM, FDBK; the sound). The track's ALGO moves to the sound editor |

**The screen shows the page as the current mode's lane**: NOTE the piano roll (as now), VEL bars, LEN bars over the
steps, MICRO offsets from the grid, RATCH the hits, CHNC dot columns, COND the conditions' names, TRIG the three
icons, SOUND each step's sound as a dot of its kit colour, LOCKS the count. **Steps that override the mode are drawn
bright, steps on the track's default grey, and the track's default is a dashed line or a grey label across the
page**, so what a default change will move is visible.

### A step held

| control | does |
| --- | --- |
| **B1..B10** | **the current mode's ten values** (table below); the value is set on every held step and overridden |
| **B11 (CLEAR)** | the held steps go back to the track's value for this mode (the override bit cleared) |
| **ALGORITHM** | the current mode's value, finely (every value, not only the ten) |
| KNOB 1..4 | p-locks, as in P1.3 |
| **PRESETS** | **the sound lock**: any of the pool's 128 sounds, in every mode (the owner, 2026-10-18: PRESETS is the sound-lock knob while a step is held, overriding its usual job); in SOUND mode B1..B10's bank follows it |
| SELECT | the page being edited (as with nothing held) |
| OCT− / OCT+ | in NOTE: the entry octave down / up; in SOUND: the pool's previous / next ten. Otherwise F1 / F2 are off while a step is held (as in Form) |
| other W keys | more held steps |

| mode | B1 .. B10 | ALGORITHM adds |
| --- | --- | --- |
| NOTE | ten notes from the track's scale and root at the entry octave (a chromatic track: ten semitones); keys pressed together make a chord (up to 4); a note sounds when pressed | the notes up / down a scale step |
| VEL | 13 25 38 51 64 76 89 102 114 127 | 1..127 |
| LEN | 1/4 1/2 3/4 1 1.5 2 4 8 16 INF steps | the whole length table |
| MICRO | −20 −15 −10 −5 −2 +2 +5 +10 +15 +20 (24ths; 0 is the default, B11) | −23..+23 |
| RATCH | B1..B8 = x1..x8; B9 = the ramp (flat, up, down, accent, cycling); B10 nothing | the same |
| CHNC | 10 20 .. 100 % | 0..100 % |
| COND | B1 FILL, B2 NOT FILL, B3..B6 A = 1..4, B7..B10 B = 1..4 (A:B, as Form) | the whole list (PRE, NEI, 1ST, A:B to 8:8) |
| TRIG | B1 NOTE, B2 LOCK (trigless), B3 TIE | the same |
| SOUND | B1..B10 = ten sounds of the pool: the bank of ten that holds the step's sound (sound 15 chosen: the keys are 11..20); OCT− / OCT+ the bank before / after; B11 clears the lock (the track's sound) | any of the 128 sounds (as PRESETS), and the keys' bank follows |
| LOCKS | B1..B10 = the step's locks (up to ten, in order) | the chosen lock's value; B11 removes that lock |

Entering notes is Form's "pressed" rule: the first key pressed replaces the step's notes, keys held together add to
them. With several steps held, every value goes to all of them.

### F1 (OCT−), F2 (OCT+), F3 (both), SHIFT (GLO)

| layer | B1..B8 | B9 | B10 | B11 | W | SELECT |
| --- | --- | --- | --- | --- | --- | --- |
| **F1** | **pages 1..8**: tap = edit that page; double-tap = loop that page alone; hold page A + press page B = loop A..B | clear the page | clear the track | undo / redo | **copy the step** | |
| **F2** | **tracks 1..8**: tap = select; hold a track + W1 = mute, + W2 = solo | copy the page | paste the page | | **paste the copied step** (each tap pastes again) | |
| **F3** | the rate: 1/8 1/4 1/2 2/3 3/4 1 4/3 3/2 (B9 = 2) | rate 2 | | | **the page's end** = that step (and the page joins the loop); hold W a, press W b = start a, end b | tempo (SHIFT: swing) |
| **SHIFT** | | | | | (question 38) | |

- **Cut** = SHIFT + F1 + W (copy, then clear the step).
- **Undo**: one level per track, covering clears, pastes and cuts (Form's is narrower).
- While F1, F2 or F3 is held, **the screen shows that layer's map**: F1 the eight pages (being edited, in the loop,
  playing, each one's start..end), F2 the eight tracks (selected, muted, soloed), F3 the page's range and the rate. So
  every layer explains itself while it is held.

### Pages: each with its own start and end

- A track has 8 pages of 16 steps. **Each page has a start and an end** (1..16, start <= end; default 1..16) and an
  "in the loop" flag (default: page 1 only).
- Playback walks the pages in the loop in ascending order, each from its start to its end. A track's length is the
  sum of its pages' ranges (this replaces §6's track length).
- Loop one page: F1 + double-tap its key. Loop several: F1 + hold one page key + press another (A..B, contiguous).
  Add one page to the loop without removing the others: F3 + W on that page (sets its end).
- **PAGE (printed ENV)** is free: tap = page follow on / off (the edited page follows the playhead; default on),
  held + W1..W8 = edit that page (as now).

### Keyboard and recording

- **REC (tap)** switches the keybed between **the sequencer** and **the keyboard** (the selected track, chromatic or in
  its scale; the KEYS screen). Its LED is lit in the keyboard.
- **Recording**: REC + PLAY starts playing with recording armed (Elektron's). In the keyboard, SHIFT + REC arms or
  disarms recording: stopped = step recording (as P1.3), playing = live recording (as P1.3: micro-timing kept, the
  length from how long the key is held).
- **FILL**: **hold ARP** (decided for now; the owner expects to move it as the design grows).


### What stays

TRACKS (FX) and its screen (white 1..8 select, 9..16 mute: the owner's "track shortcuts on the track page"), PATTERN
(SEL), HOME, PLAY, the takeovers (§9a rule 5), §5's p-locks, every held-step screen of P1.3 (they become the screens
of a held step in each mode, with the B1..B10 values drawn as a strip).

### Version 2.1 (2026-10-19, the owner after playing P1.3b)

These override the tables above where they differ.

| what | now |
| --- | --- |
| **tracks** | **hold SEL** (printed; was PATTERN): B1..B8 select tracks 1..8; **W1..W8 tap = mute / unmute** track 1..8, **W1..W8 held (400 ms) = solo / unsolo** it; **B11 (F#5) = unmute and unsolo every track**. The screen shows the track map while SEL is held. F2 no longer does tracks (the owner, 2026-10-19) |
| **patterns** | **hold SEQ** (printed; was MIX): **W1..W16 tap = queue A01..A16** by the launch mode, **held (400 ms) = at once**; **ALGORITHM turned while SEQ is held = the launch mode**: END (the end of the master length, default), BAR, BEAT, NOW. The SEQ map shows the mode (the owner: SHIFT + W would be hard to hold) |
| **F1 held** | quick **copy / paste**: the first W pressed copies that step, every further W while F1 is held pastes it (B1..B11 keep §8b's page jobs) |
| **F2 held** | quick **cut / paste**, **Form's pick-up / drop exactly** (research/FORM-SEQUENCER.md §4): the first W always cuts, even an empty step; once something is in the hand, an empty step is always pasted into and a filled step alternates paste and cut; releasing F2 empties the hand. F2 + B9 / B10 keep the page copy / paste |
| **NOTE, a step held** | **the top five knobs are its notes: SELECT = note 1** (absolute), **KNOB 1..4 = notes 2..5** (offsets from note 1, 0 = none). A step has **up to 5 notes** (was 4). B1..B10 pressed entry also takes up to 5 |
| **B11 (F#5), nothing held** | the **P-LOCK** hold mode: holding a step **does not change the screen**; whatever the screen is (the step view's quick page now, a sound editor page later) its knobs p-lock the held steps' values. B11 with a step held in P-LOCK clears the steps' p-locks (undoable). Answers question 37 |
| **octave feedback** | a change of octave (NOTE held, the keyboard) shows a **full-screen octave animation** (the octave number big, a keyboard sliding to it) for about a second; **the OCT keys' LEDs show the octave**: no blink at the centre (octave 4 in NOTE, 0 in the keyboard), OCT− blinking slowly one octave down, faster two down, OCT+ the same upwards. The SOUND mode's banks of ten work the same way (the first bank is the centre) |

In NOTE with a step held, KNOB 1..4 are notes, so p-locks there go through P-LOCK. In the other modes KNOB 1..4 stay
p-locks as §8b.

**The F1 page layer, version 2.1** (the owner, 2026-10-19; replaces §8b's F1 row and F2's B9 / B10):

| F1 + | does |
| --- | --- |
| B1..B8 tap | select that page for editing |
| B1..B8 double-tap (second release within 200 ms) | **loop that page alone** (only it plays; the owner kept Form's gesture) |
| B1..B8 held (400 ms) | that page on / off in the loop (the loop is never left empty) |
| B1..B8: hold page A, press page B | loop A..B |
| B9 tap | clear the page being edited (undoable) |
| B9 held (600 ms) | clear the track: every page (undoable) |
| B10 tap | copy the page being edited; then B1..B8 pastes it onto that page (each press pastes again; undoable) |
| B11 | undo / redo |
| W | quick copy / paste of steps (above) |

F2 no longer copies or pastes pages.

**F3's knobs, version 2.1** (the owner, 2026-10-19): while F3 is held, **KNOB 1 = the rate's note value** (1/2, 1/4,
1/8, 1/16, 1/32; 1/16 = rate 1), **KNOB 2 = normal / dotted / triplet** (dotted x 2/3, triplet x 3/2: every rate of
§6 is one of these, e.g. 2/3 = dotted 1/16, 3/2 = triplet 1/16; the base set may later grow), **KNOB 3 = the edited
page's start**, **KNOB 4 = its end** (start never passes end, end never below start). F3 + B1..B9 stay as quick rate
picks, F3 + W as the page's end / range, F3 + SELECT as tempo. The F3 map shows the rate as a note value with its
dot or triplet mark.

**The rate's type is remembered** (the owner, 2026-10-19): a track's rate is a **base note value** and a **type**
(normal / dotted / triplet), stored separately. Choosing a base keeps the type, so with a track on dotted, F3 + the
1/8 key gives dotted 1/8. **F3 + B1..B7 = the base: 1/1, 1/2, 1/4, 1/8, 1/16, 1/32, 1/64; F3 + B8 / B9 / B10 =
normal / dotted / triplet.** KNOB 1 and KNOB 2 do the same. (This replaces "F3 + B1..B9 = the nine rates".)

### Version 2.2 (2026-10-19, the owner after playing P1.3c)

These override the tables above where they differ.

**The knobs take over in the hold modes** (every mode but P-LOCK). The black keys stay as shortcuts.

| | nothing held | a step held |
| --- | --- | --- |
| **a hold mode** (NOTE .. LOCKS) | **KNOB 1..4 = the track's defaults for the mode** (the same layout as when a step is held); SELECT = the page; ALGORITHM = the mode's main default (as P1.3b) | **SELECT and KNOB 1 = the step's main value** (NOTE: SELECT = note 1, KNOB 1..4 = notes 2..5); KNOB 2..4 = the mode's extras; ALGORITHM = the step's value (as P1.3b, a duplicate the owner keeps) |
| **P-LOCK** (B11) | KNOB 1..4 = the track's sound (FREQ, DECAY, HARM, FDBK); PRESETS = its sound; **ALGORITHM = the track** (choose 1..8); SELECT = the page | KNOB 1..4 = p-locks (the screen stays); PRESETS = the sound lock |

| mode | SELECT / KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
| --- | --- | --- | --- | --- |
| NOTE | SELECT note 1, KNOB 1..4 notes 2..5 | | | |
| VEL | velocity | | | |
| LEN | length | TIE on / off | | |
| MICRO | micro | strum | | |
| RATCH | count | ramp | | |
| CHNC | chance | condition | | |
| COND | condition | chance | | |
| TRIG | type | | | |
| SOUND | the pool sound (PRESETS too) | | | |
| LOCKS | the chosen lock's value | which lock | | |

Knobs without a job in a mode do nothing there (p-locks are P-LOCK's).

**Other changes:**
- **Banks**: SEQ held + **B1..B4 = bank A..D** (64 patterns).
- **Preview**: with the transport stopped, **holding a step plays it** (its notes, sound, locks; re-struck when an edit
  changes it), released with the key.
- **F3 locks on**: pressing OCT− and OCT+ together opens the F3 menu and it stays; **OCT− exits**. The general rule for
  every latched menu or confirmation from now on: **OCT− = back / exit / cancel, OCT+ = yes / confirm / enter**.
- **Colours**: each mode keeps its own colour (the owner's choice over colour = knob); the knobs' parts within a
  mode's screen may still use the knob colours where they help.
- **Step latch on LFO** (printed LFO; the owner): LFO toggles the latch (its LED lit). While it is on, **it is as if a
  step is always held**: a W tap **selects** that step (it neither adds nor removes trigs), holding more W keys adds
  them to the selection; the black keys, the knobs, the screens and the preview act on the selection. It starts on the
  last step edited (else step 1). LFO again, or HOME, turns it off. (OCT− cannot be its exit: OCT− is the octave while
  a step is held.)
- **LOCKS is dropped** (P-LOCK on B11 covers it): **B10 is empty**, reserved. The "last entered" note / velocity are
  not coming back: the track's defaults do that job.
- The owner's answers (2026-10-19): the demo drums wait for the sound editor; no saving yet; SHIFT + ALGORITHM /
  PRESETS for scale and root stay; the dancers stay.
- **Editing a track's default** (the owner, after P1.3d): with the step latch off, **holding a mode key (B1..B9)**
  holds "the track" for that mode: the screen is the mode's full picture marked TRACK DEFAULT (as a held step's: NOTE
  shows the chord name and the keybed), SELECT and KNOB 1..4 edit the default by `KNOB_JOB`, ALGORITHM the main
  default, and **B11 (F#5) resets that default** to the track's initial value (NOTE: back to its single note, which
  clears a default chord). With nothing held at all, ALGORITHM and KNOB 1..4 still edit the defaults, now showing that
  same full picture instead of P1.3d's band (the owner liked the full NOTE screen). The band stays for a held step.
- **Six notes** (the owner): in NOTE with a step held, **ALGORITHM = note 6** (SELECT note 1, KNOB 1..4 notes 2..5),
  so a step has **up to 6 notes** (keys too). For the track's default chord ALGORITHM keeps moving every note
  together (the owner likes it); a held step's notes no longer have that transpose.
  **Superseded the same day (the owner):** in NOTE, with a step held **or the NOTE key held**, **ALGORITHM moves
  every note together, SELECT moves only note 1, KNOB 1..4 move only notes 2..5**, each on its own (the other notes
  stay where they are). Notes are therefore stored as pitches, not as offsets from note 1. A knob turned all the way
  down removes its note; turned up from none it adds one. A sixth note comes only from the black keys.
- **KNOB 4, the quiet knob** (the owner): in every mode where KNOB 4 has no job (all but NOTE, where it is note 5),
  KNOB 4 sets the mode's main value **without any overlay**: the lane (nothing held) or the held step's screen just
  changes in place. With nothing held it sets the track's default, with a step held the step's value.
- **HOME cycles the sequencer's views** (the owner): **PIANO ROLL** (the mode lanes, as built) and **STEPS** (just the
  page's 16 steps: two rows of eight cells like the white keys, on / off, LOCK steps, p-lock and condition marks, the
  playhead, the page's start..end and the loop). HOME first closes an open latch (F3, the step latch); from any other
  screen it returns to the last view used.
  **STEPS, redrawn** (the owner, after P1.3e): **16 steps a row, in groups of four, one row a page, all 8 pages
  stacked**: the edited page highlighted, the loop's pages marked, other pages dim, each page's out-of-range steps dark,
  the playhead moving across the rows, trigs flashing.
- **The F3 menu's big knobs** (the owner): **SELECT = the edited page** (the F3 map follows it), **ALGORITHM = the
  tempo, SHIFT + ALGORITHM = the swing** (they were on SELECT).
- **The step latch, refined** (the owner): while latched, **holding several W keys selects them all** to edit
  together; **tapping a selected step unselects it**. With **nothing selected** the latch stays on but behaves as
  nothing held: the black keys choose the hold mode, the knobs edit the track's defaults; a W tap selects again (a tap
  still never adds or removes a trig while latched).
  **Selection by gesture, not toggles** (the owner, after P1.3e): the W keys pressed while at least one is still down
  form one gesture; when every W key is up, **that set replaces the selection** (a single tap selects just that
  step). Tapping the step that is already the whole selection empties it (the mode keys come back).

### Questions for the redesign

35. **SOUND's ten keys**: **decided: the sound pool** (128 sounds, a one-byte lock; ten at a time on the keys, OCT
    for the next ten). **PRESETS picks any of the 128 with a step held, in every mode** (the owner: "use the Preset
    knob for sound locking, overriding its default functionality"); ALGORITHM does the same in SOUND mode; KNOB 1..4
    stay p-locks.
36. **FILL**: **decided: hold ARP**, for now.
37. **B11 with nothing held**: nothing; or "clear every override of the current mode on this page" (with undo)?
    *Recommend nothing for now.*
38. **SHIFT + W**: free for now (views later: the sound editor, mix, patterns). *Recommend free.*
39. **Scales**: **decided: per track (chromatic by default) with a global key the tracks can follow.**
40. **The knobs**: **decided: ALGORITHM turns the mode's value** (the track's default with nothing held, the step's with
    a step held), **SELECT changes pages**, KNOB 1..4 are the sound (p-locks with a step held). §9a rule 1's "SELECT
    turns what KNOB 1 turns" no longer holds.

## 9. The screens and the two directions

`mockups/index.html`. Every screen is a 240 x 240 canvas, so what is drawn is what the panel would show, pixel for
pixel (the type is Inter Tight, as ChoralRoot's firmware rasterises it). Screens, in both directions:

1. **The step view**: the track, the pattern, the 16 steps of the page as 16 columns (each a key), the page strip,
   the knob row. A column: its velocity as its height, a chord as up to four bars, a lock as a notch, a sound lock as
   stripes, a condition / chance strip, micro-timing as the column's shift. *Motion*: the playhead sweeps; a trig
   flashes its column white for 60 ms; a page change slides the columns sideways.
2. **A held step: NOTE**: the chord name big (`Cm7`), its notes, the four note cells; the strip of the 11 black-key
   fields at the foot (the one held lit). *Motion*: a new chord name squeezes in (ChoralRoot's chord-name squeeze).
3. **A held step: COND / CHANCE**: `3:4` with the four passes as a ring (the third lit), the chance as a dot field.
   *Motion*: the ring's lit pass follows the playing pass.
4. **Setting a p-lock**: the sound editor's FILTER screen with step 5 held: the track's curve ghosted, the locked
   curve bright, FREQ's cell marked LOCK. *Motion*: turning, the bright curve slides away from the ghost.
5. **A sound locked on a step**: the sound's name big as a picker (neighbours peeking), "on step 9", the step's other
   locks counted. *Motion*: a turn of PRESETS rolls the names like a drum.
6. **The track overview**: eight lanes (colour, sound, a mini page with the playhead, rate, length, the voices in use
   as dots against the limit), the selected one bright, muted ones dark; the knob row LEN RATE LIMIT SWING. *Motion*:
   the voice dots light and fade with the voices.
7. **Pattern select**: the queued pattern big (`A07`), a countdown ring to the change, the 16 patterns of the bank as a
   strip, the banks. *Motion*: the ring empties to the change; then the big name drops in.
8. **The micro-timing editor**: the step's span magnified (24 ticks either side), the grid line, the note's start, the
   strummed chord notes after it, the neighbours faint; `+7/24` big. *Motion*: the start slides with the knob.
9. **Keybed diagrams** with the LED states of §8 (the panel, the six views).

The PNGs (`python3 mockups/render_png.py`, headless Chromium): `mockups/png/mod-01..08-*.png` and
`riso-01..08-*.png` (240 x 240, 1:1), `sheet-mod.png` and `sheet-riso.png` (each direction's eight at 2x),
`panel-map.png` (every control's role), `keybed-1..6.png` (the step view playing, a step held, the sound editor with a
step held, PATTERN held, mute mode, KEYS mode) and `led-legend.png`.

**MOD** (ChoralRoot's): black, heavy white type, flat blocks and stripes, the roundel; role colours kept for their
jobs (red = playhead / REC / clear, yellow = chance, blue = condition, white = the value, orange and green as the knob
row's), eight track colours from the same flat palette.

**RISO** (new): a print look: warm paper ground, two or three inks per screen (fluorescent pink, blue, yellow, green,
orange, black) with a one-pixel misregistration, halftone dots for chance and velocity, condensed numbers, stamped
labels. Bright in daylight, distinct from ChoralRoot. On an IPS panel at night a light ground glares: a dark-paper
variant would be its night mode (question 16).

## 9a. LINE: the OP-1 direction

`mockups/line.html` (open it in a browser: every screen runs live at 122 BPM), the stills `mockups/png/line-01..25-*.png`
(240 x 240, 1:1), `sheet-line.png` (all twenty-five at 2x) and `line-motion.gif` (eight of them moving, the knob-turn
takeover among them). After the OP-1's
screens in spirit; nothing is copied from them, and the device never names the OP-1.

### The rules

1. **Colour means a knob, everywhere.** Whatever KNOB 1 turns is blue, KNOB 2 green, KNOB 3 white, KNOB 4 orange:
   the dot on a curve, the operator's circle, the note on the little keybed, the fader of the selected track. The
   player turns the knob of the colour of the thing they want to move. **SELECT always turns what KNOB 1 turns** (the
   big value). PRESETS and ALGORITHM keep their printed jobs (the sound, the algorithm).
2. **Red is only time and recording**: the playhead, a clock's hand, RECORDING. Greys are structure (outlines, rulers,
   what is off).
3. **Thin lines, round ends, outlines first**: 1 .. 1.5 px strokes; a fill means "on" (the playing pattern, the field
   on screen, a lit trig). Type is Inter Tight at weight 200 .. 300 for the big values, 500 .. 600 small caps for
   labels.
4. **No rows of values.** (The owner, 2026-10-17: "move away from the 4 parameter boxes per page ... a full visual,
   then turning the knob adjusts it, and if needed the whole screen could temporarily update to show the parameter.")
   A screen is one drawing and each knob moves a part of it. At most one or two big numbers, and only when the number
   is the subject (a held step's field, the tempo, the queued pattern).
5. **A knob whose value the drawing does not show takes the whole screen while it turns** (screens 21..25): its name
   and a little knob mark in its colour at the top, the context grey at the right, a full-screen drawing of what it
   does (FREQ the filter's curve, DECAY the envelope, HARM the wave growing harmonics, FEEDBACK a spiral), the value
   big at the foot. A value with steps (RATE, a condition, a sound) is a picker: the value big, its neighbours small.
   The first detent shows it at once; one second after the last detent the screen fades back (120 ms). On the step
   view this is every knob (it draws no sound values); on a held step's field it never happens (the field is drawn).
6. **One drawing a screen, and the drawing is the value**: velocity is how high the mallet rises, a ratchet is a ball
   bouncing as many times as the count with each bounce as high as its hit's velocity, a chance is a field of dots
   that reshuffles every pass, a sound lock is a cassette, the delay is a ping-pong ball between two walls.
7. **A held step's screens share one layout**: `STEP n · FIELD` top left, the track top right; KNOB 1's value big top
   left; the drawing in the middle, where KNOB 2 has its own green part (the strum's staircase, the ratchet's ramp, the
   tie's slur); at the foot **the black-key strip** (eleven little keys
   grouped 3 | 2 | 3 | 2 | 1 like the keybed's black keys): the lit one is the field on screen, an outlined one holds a
   value (a condition, locks, a sound). In the sound editor the same strip is its eleven pages.
8. **Everything moves with the music** (motion for each screen in line.html's notes): the four operators dance on the
   idle screen, the track clocks turn at their own rates, the mallet strikes when the step plays, the reels turn.

### The twenty-five screens

| # | screen | the drawing |
| --- | --- | --- |
| 1 | idle | the four operators (C, A, B1, B2) as four dancers in a square, a quarter turn each bar: the quadrille |
| 2 | the step view | the 16 keys as tall outlines, the page as a little piano roll inside them (chords stacked, lengths as bars, micro-timing off the column, ratchets dashed); markers under a column: lock, condition, chance, sound |
| 3 | tracks | eight rings, each with its trigs round it and its own hand at its own length and rate (polymeter at a glance), its voices as dots, the 16-voice pool top right; the selected track's parts in its knobs' colours (trigs blue = LEN, length and rate green = RATE, voices white = LIMIT, an orange arc = SWING) |
| 4 | held: NOTE | the chord name big (it squeezes in), the four notes on a keybed drawing in the four knob colours, their names under them |
| 5 | held: VEL | a mallet over a drum, raised by the velocity; it strikes when the step plays |
| 6 | held: LEN | the note as a bar stretching over the next steps on a ruler; TIE as a slur to the next note |
| 7 | held: MICRO | the step magnified as a tape measure (24 ticks), a flag at the start, the strum as a green staircase |
| 8 | held: RATCHET | a ball bouncing x1..x8 along a dotted path, the bounce heights the ramp |
| 9 | held: COND / CHANCE | `3:4` with four circles (the pass ring moves each pass), 75 % as thirty dots that reshuffle each pass |
| 10 | held: SOUND | the locked preset as a cassette (reels turn), the neighbours peeking above and below; the page's sounds as dots |
| 11 | a p-lock | the FILTER 2 envelope: the track's curve dashed, the step's solid, padlocks on the locked values, the track's values under them |
| 12 | the engine, SYN 1 | the algorithm wired up: the operators as circles with their waves scrolling at their ratios, dots flowing to the speaker; the ratios written under the operators in their knobs' colours, HARM an orange comb |
| 13 | patterns | the queued pattern big, bank A as 16 dots, the chain drawn as a line through them, a line that empties to the change |
| 14 | mix | eight faders with pan dials, meters and two send lines each; the selected track's fader, pan and sends in the four knob colours |
| 15 | delay | a ping-pong ball between two walls: the ball blue (TIME), the echoes green (FEEDBACK), the walls white (WIDTH), the send an orange arrow (reverb: a room; chorus: three wavy lines) |
| 16 | tempo / swing | a metronome swinging at the tempo, unequal swings with swing on |
| 17 | mute | a speaker a track, crossed when muted, its waves pulsing with its trigs |
| 18 | KEYS recording | the notes scrolling in from the red line as they are played, the held keys on the little keybed |
| 19 | held: TRIG | NOTE (a struck dot), LOCK (a padlock: trigless), TIE (a slur), one line saying what the chosen one does |
| 20 | held: LOCKS | the step's locks as a picker: one big in blue, a line from the track's value to the lock's |
| 21 | a knob turned | the step view, KNOB 1 turned: FREQ takes the screen (the filter's curve sweeping), then hands it back (the canvas loops it) |
| 22 | a turn: DECAY | the amp envelope's decay as the curve itself, a red dot running down it each beat |
| 23 | a turn: HARM | the wave growing harmonics, the comb of harmonics under it |
| 24 | a turn: FEEDBACK | a spiral winding back into itself, more turns with more feedback |
| 25 | a turn: RATE | a picker (1/2x big, 1/4x and 1x small) over the pattern's clock and the track's clock side by side |

### FUNC on B9, SOUND on B10 (the owner, 2026-10-19)

**B9 becomes FUNC**, the step functions of the owner's OMX-27 Form (research/FORM-SEQUENCER.md: FUNCTION); **SOUND
moves to B10** (B11 stays P-LOCK).

| B (a step held) | function |
| --- | --- |
| B1 | none |
| B2 | RESTART: the next step is the loop's start; the passes, conditions' counters reset |
| B3 | REVERSE: the track plays backwards from here |
| B4 | FORWARD |
| B5 | FLIP the direction |
| B6 | RANDOM JUMP: the next step is a random step of the loop |
| B7 | RANDOM FUNCTION: one of none / restart / reverse / forward / flip, rolled each time |

- The value knob walks the seven, then **JUMP 1 .. 128** (a step of the track, page by page). While a held step's
  function is a jump, **pressing another white key makes it the target** (Form's gesture; it is not added to the held
  set).
- A function runs only when its step fires (after its condition and chance). A jump to a page outside the loop goes to
  the loop's start. The direction is the track's playing state, back to forward on PLAY (Form keeps it: decided here
  for now, open to the owner).
- Screens: the FUNC lane shows each step's function as an icon (arrows, a dice, J with its target); the held and EDIT
  screens draw it big. Under the clarity rules.

### PLAY, F3 and live recording (the owner, 2026-10-19)

- **Holding PLAY opens F3** (latched, as both OCT keys do). A tap still plays / stops; to tell the two apart the
  transport acts on a tap's release (about 0.1 s later than on the press; if that is felt, the fallback is SHIFT +
  PLAY for F3 and PLAY on the press).
- **In F3: SELECT = the tempo, ALGORITHM = the swing**, each turn showing the **full-screen metronome picture** (the
  tempo / swing takeover of P1.3c) for a second over the F3 page. The page is reached in F3 through F3 + W and KNOB 3
  / 4 (replaces P1.3h's SELECT = page, ALGORITHM = tempo).
- **Live recording in the keyboard (REC):**
  - **holding REC arms live recording** (replaces SHIFT + REC);
  - **stopped, nothing moves until the first key**: that key starts playback and recording at once, the note on the
    loop's first step;
  - **an empty track grows**: recording runs on through pages 1..8, each joining the loop as it is reached; at the
    stop the loop is the pages reached; at page 8's end it loops and records over what is there; a track with steps
    records into its loop;
  - **REC or PLAY while recording ends the recording, playback goes on** (PLAY again stops);
  - notes land on the nearest step **at the track's rate** (base and type), their offset kept as **micro-timing**, the
    length from how long the key was held.

### Version 2.3 (2026-10-19, the owner after playing 0.2.2 on the unit)

Live recording worked as expected; no hint text is missed; **EDIT is the view the owner uses most**. Their answers:
P-LOCK stays on B11 for now; the direction stays until PLAY.

**P1.3y, the control fixes:**
1. **FUNC:** a new **LEAP n** (skip n steps; ±). **KNOB 1 = the function, KNOB 2 = the leap, KNOB 3 = the jump target.**
   **B8 = LEAP, B9 = JUMP** join B1..B7. The jump target is set **holding B9 + a white key** (or KNOB 3), not by a
   second white key (which clashed with the step latch). FUNC's value keys light as other modes' do.
2. **HOME returns to your view and never cycles; SHIFT + HOME cycles** PIANO ROLL / STEPS / EDIT.
3. **One meaning a knob:** **ALGORITHM = the step cursor** (nothing held, every view; it was PRESETS); **PRESETS =
   the track's preset**, or with a finger on a step its sound lock (in EDIT the sound picture shows while it turns);
   **SELECT = the page whenever no finger is on a step** (also with the latch or the cursor); a page change puts the
   cursor on that page's first step. The mode's main default stays on KNOB 1..4 and a held mode key.
4. **LFO held + a black key = the hold mode** without toggling the latch (a tap toggles it).
5. **TRACK vs STEP at a glance:** editing a track's default has its own look, a bold coloured frame around the
   whole screen and the drawing in that colour; editing steps keeps the mode colours on plain black. No reading
   needed.
6. **FX held + ALGORITHM = the track** (a big track picture while turning).

**P1.4a, the sound editor on EDIT** (printed EDIT): the black keys jump to the engine's pages, **Operators, Op
envelopes, Filters, Envelopes, Modulation**; each page a big LINE drawing of that part of FM TONE with KNOB 1..4 on its
values (a second half by pressing the page's key again); with a finger on a step every turn is a p-lock (the P-LOCK
hook of P1.3c). Under the clarity rules.

**P1.4b, track settings and the mixer:** **FX tapped = the track settings page** (scale, root, follow the global key,
MIDI channel, voice limit, swing on / off, page follow); **ENV (printed) = MIX**: eight faders with pan and sends, the
black keys the tracks, the knobs level / pan / delay / reverb of the selected one; MIX again = the effects (delay,
reverb, chorus). PAGE's jobs move: page follow to the settings, pages to SELECT and F1.

**Version 2.3, the owner's additions (same day):**
- **Record settings on SHIFT + REC** (a latched menu, OCT− back): the recording mode **LIVE / STEP**, quantize, overdub /
  replace. Holding REC arms whichever mode is chosen (SHIFT + REC no longer toggles step recording).
- **The value keys light the nearest value** in every hold mode (velocity 100 lights the 102 key; a length between two
  keys the nearer), not only an exact match.
- **Knob layouts to try:** a **global settings page on SHIFT + SEL** with **KNOBS = A / B / C (/ D)**, complete
  self-consistent layouts (A: ALGORITHM the step cursor, PRESETS the preset, SELECT the page; B: PRESETS the cursor
  (P1.3q's); C: SELECT the cursor, ALGORITHM the page; ...), so the owner can compare them on the unit.
- **Buttons:** **FX (printed) = the effects page**: ChoralRoot's stereo effects as FIZZ already runs them (the stereo
  delay, the large modulated reverb, the stereo chorus; docs/FX.md in ChoralRoot), every parameter on pages with
  drawings. **SEL tapped = the track settings page** (SEL held stays the track map). **ENV = MIX** (the owner: fine).
  FX held + ALGORITHM still chooses the track.
- Order: P1.3y gains the light rounding, the global settings page with the knob layouts, and the record settings;
  P1.4b becomes track settings (SEL tap), MIX (ENV) and FX (FX).

### The roadmap after P1.4b (the owner, 2026-10-19)

- **P1.4a gains DRIVE**: drive is part of the sound (loads with the preset, p-lockable), on a sound editor page.
- **P1.5a, saving, the library and the pool:** the project written by itself into the core's flash image (FM1VST keeps
  it between launches; the floating page store of §7, the device's own way), eight projects; SAVE stores the track's
  sound in the user library, F2 adds it to the project's pool; the sounds page and the project page (below).
- **P1.5b:** version 2.4 (below: live play in the editor, the scale keys, lock marks, FX overview and routing, knob
  acceleration, two latch modes). **P1.5c:** **8 undo levels**, **snapshots of the pattern** (the owner: all 8 tracks' steps, sounds and settings;
  store, recall: a performance tool), **randomise a sound** (in the sound editor).
- **P1.6a, the arpeggiator:** ARP tapped = on / off (the keyboard), ARP held = FILL, SHIFT + ARP = its settings (mode,
  rate, octaves, gate, latch).
- **P1.6b, MIDI tracks:** a track can also send its notes out over MIDI (a channel); a track's engine can be switched
  from FM TONE to **MIDI**, its knobs and p-locks then sending CCs.
- **P1.6c, a MIDI generation editor** after the OMX-27's MIDI FX (research first: research/OMX-MIDIFX.md).
- **Held:** pattern chains. **Phase 2** (the firmware) waits until the owner is happy with the design and the UX.
- **FM1VST:** improvements of general use (the screen copy, FM1_ONLY_CORES) go to FM1VST main; FIZZ's glue stays on
  `core-quadrille`. The owner: push FM1VST main once its tests pass.

### P1.5a, saving, the library and the pool (mock-up version 2, 2026-10-20)

`mockups/fizz-save-pool.html` (sheet `mockups/png/fizz-save-pool-sheet.png`, GIFs `fizz-lib-store.gif`,
`fizz-pool-add.gif`, `fizz-pool-move.gif`): fourteen screens, each with the FM-1's keys under it. The library is
champagne (0xf3d9a4), the pool SOUND's lime, a track its own colour.

**The owner's answers to version 1** (which had SAVE saving the project, SHIFT + EDIT for the pool and holds to
confirm): eight projects; a track's modified sound is saved with its pattern, and can also be saved to a **user
library**, then optionally added to the project's **pool**; the project is **saved automatically**, the **SAVE key
saves the sound to the library**; no SHIFT + EDIT, **single clicks rather than holds** (SHIFT + a button is kept for
the snapshots of P1.5b); names **from the sound**.

**The model**
- **The project is written by itself** (every edit, a moment after the last; page programs only while playing, §7).
  Each pattern keeps its eight tracks' sounds as edited. **Eight projects**, each with its own 64 patterns and pool.
- **The LIBRARY**: the user's sounds, 128 (8 banks of 16), shared by every project (on the device a store of its own).
- **The POOL**: the project's 128 sounds, what the steps' sound locks and PRESETS choose from (INIT, the factory sounds,
  the drums to start).

**The sounds page** (opened by SAVE)
- **SAVE tapped** (anywhere) opens it on the LIBRARY to **store the selected track's sound**: the first free slot
  chosen (blinking), a **name from the sound** (envelope and spectrum: PLUCK, PAD, BASS, KEYS, BELL, LEAD, KICK, HAT ...
  with BRIGHT, DARK, SOFT, WIDE ...). **SAVE again stores it.** A W chooses another slot; onto a slot that holds a sound
  the first SAVE only arms it (orange, the old and new silhouettes in turn), **SAVE again replaces it**; anything else
  cancels. OCT− / HOME leave.
- Once stored, the pool tray blinks with **F2: F2 adds the sound to the project's pool** (its first free slot; the page
  turns to the POOL tab).
- **B1..B8 the bank, W1..W16 its sounds**, SELECT / PRESETS through all 128, **B9 LIBRARY / POOL**. A tile: the amp
  envelope as a silhouette coloured by the sound's kind (blue bass, white keys, violet pads, green leads, orange drums)
  and the name. **W tapped** chooses and plays it once; **W held** plays it while held and it takes the screen.
- **F2**: in the library, add the chosen sound to the pool; in the pool, put it on the selected track (a step held:
  lock it on the step). The pool shows a sound's users (the tracks' boxes; a padlock and the count of steps).
- **B10** names (SELECT the letter on a drum, ALGORITHM along, B11 deletes a letter, B10 again another name from the
  sound, F2 keeps). **F1 + W** copy then paste (B9 between them copies across library and pool), **F2 + W** pick up /
  drop (a filled slot swaps; in the pool every lock and track follows, renumbered in all patterns), **B11 + W** deletes
  (its users flash orange, it fizzes away; tracks keep their copy, locks go back to the track's sound; undo brings it
  back; INIT cannot go).

**The project page** (**SEQ tapped**; SEQ held stays the patterns): its number and name, its 64 patterns (a few
bubbles rise off one when it is written), the flash meter and the pool's count. SELECT or SEQ again: **the projects**,
eight on W1..W8: W chooses, **F2 loads** (an empty one starts a new project), F1 + W copies one, B11 + W deletes, B10
names.

### Version 2.4 (2026-10-21, the owner after playing 0.4.1): P1.5b

The owner's eight points, each with how it is to be built (P1.5b; the undo / snapshots / randomise milestone becomes
P1.5c).

1. **Live play in the sound editor.** The owner: "You can only use the sound editor while programming steps. I would
   like to be able to live play." REC tapped (the keyboard) must work with the editor open. In the editor with the
   keyboard on, **the black keys stay the editor's page shortcuts and the 16 white keys play the track's scale**
   (W1 the root at the keyboard's octave, then each scale degree upwards; chromatic = 16 semitones; F1 / F2 the octave).
   The owner also wants **the full keyboard** there: a record setting **KNOB 4 EDIT KEYS: SCALE / FULL** (SHIFT + REC;
   FULL = every key plays as the keyboard does now, the pages then only on SELECT). Outside the editor the keyboard
   stays as it is (full).
2. **The scale while playing live.** With the keyboard on (playing or stopped), **SHIFT + PRESETS (scale) and SHIFT +
   ALGORITHM (root) work**, in the editor too: there SHIFT + ALGORITHM / PRESETS mean the scale and root, and the fine
   step of P1.4a stays on SHIFT + KNOB 1..4 and SELECT. **The keys light the scale**: the root's keys at 80 %, the
   other in-scale keys at 50 %, out-of-scale keys dark (in the SCALE layout every white key is in the scale: roots 80,
   the rest 50); a key playing at 100 %.
3. **Locks without the padlock.** The owner dislikes the padlock. **A value locked on the held step is highlighted**:
   its value drawn as a filled chip in its knob's colour with black text, and its part of the drawing at full
   strength. **With no step held, a value that any step of the track locks gets a quieter mark**: the same chip
   outlined (not filled) in its knob's colour. One rule: outline = locked somewhere, filled = locked here. The padlock
   goes everywhere (the editor, P-LOCK, the pool's lock count can keep a small chip instead).
4. **The editor's header.** The page dots move with the page name's width: **dots first (fixed, left), then the
   name**.
5. **FX: an overview page with the sends** (where FX opens): three columns, CHORUS, DELAY, REVERB, each with the eight
   tracks' send bars in the tracks' colours; the selected track's bars bright. **KNOB 1..3 the selected track's
   chorus, delay and reverb sends**; **W1..W8 choose the track** on this page (FX held + ALGORITHM still too).
6. **FX: a routing page**: the chain drawn (the tracks' bus, CHORUS, DELAY, REVERB, the master filter, out) with what
   can be rerouted on the knobs. First read ChoralRoot's fx (cr_fx.c, fx.c) for any routing it has and use that;
   otherwise add cross-feeds (KNOB 1 DELAY → REVERB, KNOB 2 CHORUS → DELAY, KNOB 3 CHORUS → REVERB, KNOB 4 the master
   filter before or after the effects), **every default ChoralRoot's routing so the bit-exact tests stay identical**.
   FX's black keys: B1 OVERVIEW, B2 DELAY, B3 REVERB, B4 CHORUS, B5 MASTER, B6 ROUTING (a key again = its next page).
7. **Knob acceleration** on every encoder: detents close together move further (×1 slow, rising to about a
   sixteenth of the range per detent when spun), so a full sweep is one fast turn and single steps stay easy; values
   with few steps (enumerations, ≤ 24 steps) never accelerate; SHIFT + a knob stays the finest step.
8. **Two latch modes.** **Hold LFO (alone, released after 400 ms with no other key) switches the latch mode**, kept as a
   global setting and reused whenever the latch is turned on again:
   - **VALUES** (as now): with the latch on, the black keys set values for the selected steps.
   - **MODES**: with the latch on, the black keys choose the hold mode (as with nothing held), and **the knobs set the
     selected steps' values for that mode** (as the held-step screen's knobs do). **Holding a step**, the black keys
     are the value shortcuts as always.
   A takeover shows the mode on the switch (two drawings: keys as values / keys as modes); LFO is lit full in VALUES,
   at half in MODES. LFO held + a black key still selects the hold mode.

### Version 2.5 (2026-10-21, the owner after playing 0.5.0): P1.5d

- **The frames the other way round.** The owner: the frame round a track's values (the TRACK frame of P1.3y) "should be
  the other way around". **The sequencer's held-step screens get the frame** (in 0xf03cff); **the track defaults get
  none**. **In the sound editor: a dark orange frame for the track's sound, a bright orange frame for a step's locks.**
- **VEL as a caped stunt motorcyclist** (the owner, from a photo of a caped wheelie rider; it replaced a first idea, a
  flying squirrel skydiver, `mockups/fizz-vel-squirrel.html`): `mockups/fizz-vel-rider.html`,
  `mockups/png/fizz-vel-rider.png`, in place of the mallet and drum. Side view, **riding to the left**; both wheels on
  the road at low velocity, the front lifting into a **wheelie** as the velocity rises (about the rear axle, 0 below
  VEL 15, 55 degrees at 127); the cape (orange, a white stripe and a star) droops when slow and streams flat and waves
  when fast; the wheels spin, the road dashes and the speed lines run right with the velocity. The value big top
  right, the ramp's ticks top left.
  **Neon (the owner's second reference, `mockups/png/fizz-vel-rider-reference.png`)**: blue bike, wheels (two rings, a
  red hub) and helmet (a star, a visor), red cape (a forked flag streaming back at shoulder height, two cream sparkles)
  and legs (tubes: a line with a black core), cream engine fins, exhaust, fork and lamp; every line glows (two wide
  faint strokes under it); dashed speed lines in blue, cream and red behind the rear wheel.
- **VEL 127, the stunt show** (the owner: "Would be fun if something special happened at maxed value of 127"):
  `mockups/fizz-vel-rider.html` (`show()`), `mockups/png/fizz-vel-127-jump.gif`. Version 2 (the owner: "a bit too
  different from the full wheelie ... the motorcycle should visually stay in the same spot"): the bike keeps the full
  wheelie in the same place and size as at 126; **the world scrolls under it** (200 px/s, every 2.6 s): a takeoff ramp
  (10 high), **eight buses in the tracks' colours**, a landing ramp; the rear wheel rides up, flies a small arc (16)
  over the buses with the nose a little higher (keeps the helmet clear of the value), lands; **neon fireworks** burst
  over the buses as their middle passes.

### The clarity rules (the owner, 2026-10-19: "strong visually, powerful gesture based, and not require the user to be squinting all the time")

These override anything above that conflicts with them.

1. **No tutorial text on the screen.** No hints, instructions or key legends ("W TAP: QUEUE · HOLD: AT ONCE", "OCT−
   EXIT", "B11 CLEARS THEM", "PLAYS EVERY PASS", "/24 OF A STEP" ...). Gestures are learned from PLAYING.md and from
   the screens' shapes; the screens show state, not instructions. The layer maps (F1, F2, F3, SEL, SEQ, SHIFT) are
   **visual**: the keys drawn where they are on the panel, lit or coloured by what they do, with icons, not sentences.
2. **Nothing to squint at.** Text is either **big** (the value being changed, chord names, the logo) or a **short
   label at 12 px or more** (never under 11 px), weight 500 or more. What cannot be that size is removed or becomes a
   shape (a tick, a dot, a bar, an icon).
3. **Text only for what must be read at a glance:** the value being changed, the mode's name, the track or pattern
   where it is the subject, the scale name. Everything else is drawing, colour, motion or LEDs.
4. Every new screen is checked against 1..3 before it is built.

### What LINE asks of the firmware

- **Anti-aliased thin lines, circles, arcs, ellipses and round rectangles** at 1 .. 1.5 px with round caps, drawn at run
  time (no bitmaps): the drawings are a few dozen strokes each. To check in ChoralRoot's `cr_gfx.c` / `cr_draw.c` what
  exists (its look is flat blocks); a Wu-style AA line and a midpoint circle with coverage are about 2 KB of code.
- **The redraw cost**: the animated screens redraw at the UI's frame rate; a full 240 x 240 redraw of a screen of
  strokes is well inside a frame on core 0's main loop, but it is to be measured with 16 voices playing (the UI never
  runs in the audio ISR). Only the changed rectangles need pushing to the LCD.
- **Thin type at big sizes**: Inter Tight 200 at 50 .. 70 px for the big values means more font tables (digits and a
  few letters only at the biggest sizes: about 6 .. 10 KB a size with `gen_aa_font.py`). The alternative is a
  stroke font for the big digits, drawn with the same AA lines (smaller, any size, more OP-1 in spirit): question 28.

## 10. Open questions (numbered; a recommendation each)

1. **The name.** **Decided 2026-10-19: FIZZ** (the owner). The code keeps its `qd_` prefix inside.
2. **"The top row"** = the black keys (11 fields while a step is held), not the top buttons. *Recommend the black keys*
   (§8 has the comparison).
3. **Black keys select the tracks** in the step view (one press), instead of TRACKS + a white key. *Recommend yes*;
   TRACKS + white key 1..8 also works, for players who look for it.
4. **8 tracks.** *Recommend 8* (the same CPU as 4; 6 KB more a pattern).
5. **16 voices, 8 a core**, the render in RAM; the single-core package 8. Weigh voices by their patch? *Recommend 16,
   units as ChoralRoot's (2 a voice), no weighing until the unit's reading says so.*
6. **Flash: a guest beside ChoralRoot** (its legacy sectors, ~45 typical patterns, its sounds readable) **or the whole
   region** (~128, ChoralRoot's data gone)? *Recommend the guest first*, after checking that ChoralRoot never writes
   or migrates from 0x97000..0xB3FFF and 0xDC000..0xDFFFF once its pool exists.
7. **Saving**: the working copy written by itself (page programs only while playing) with SAVE as a restore point, or
   ChoralRoot's manual model? *Recommend by itself.*
8. **Patterns in a project**: 64 (A..D x 16) with a flash meter, or 128 (A..H)? *Recommend 64* (the guest's flash).
9. **Notes a step**: 4, or 6 / 8? *Recommend 4* (the Digitone's; 16 voices go fast with chords on 8 tracks).
10. **Latching a held step** with OCT+. *Recommend yes* (ChoralRoot's layers latch the same way).
11. **The step view's KNOB 1..4**: FREQ, AMP DEC, HARM, FDBK? ChoralRoot's HOME for QUAD was RATIO A, HARM, FDBK, MIX.
    *Recommend FREQ, AMP DEC, HARM, FDBK* (the sequencer's usual locks), assignable later.
12. **Sound-locked steps and live tweaks**: untouched by the track's edits, moved only by the perform offsets?
    *Recommend yes.*
13. **DRIVE**: a track insert (lockable per step at the track level), or per voice (the Digitone's order, +32 us a
    voice)? *Recommend the track insert.*
14. **Rates**: the Digitone's seven (1/8 .. 2), or with 1/3, 2/3, 4/3 for triplets? *Recommend the seven plus 2/3 and
    4/3.*
15. **Identity**: a Felucca-family identity and version string of its own (installers then treat it as ChoralRoot
    treats Felucca's family), or V15's as TransitFM1? *Recommend its own*, so the web installer and the Hub can tell it.
16. **The look**: MOD, RISO, or LINE? *Recommend LINE* (the owner asked for the OP-1's clean and fun; §9a), MOD's draw
    code as the fallback if the AA primitives cost too much, RISO dropped.
17. **Undo**: "the same gesture again within 4 s" for paste and clear, nothing more? *Recommend yes.*
18. **FILL on REC held** in the step view. *Recommend yes.*
19. **Songs**: chains in 0.1, songs in 0.2? *Recommend yes.*
20. **USB audio**: stereo recording only, or a pair per track group? *Recommend stereo first* (each extra pair costs
    core 0 TIMER5 time while streaming).
21. **Unison** (several voices on one note, detuned)? *Recommend not in 0.1*: DTUNE inside a voice covers much of it.
22. **The condition list**: Elektron's (PRE, NEI, FILL, 1ST, A:B to 8:8) with ChoralRoot's. *Recommend yes.*
23. **A locked SPEED on a FREE LFO**: drifts from the track (accept) or switches the voice's LFO to TRIG? *Recommend
    accept.*
24. **ChoralRoot's FM TONE sounds**: readable in QUADRILLE's picker (read only, from its pool)? *Recommend yes.*
25. **The steal's fade**: one block (0.73 ms, ChoralRoot's) or four (3 ms, a 2 ms later start)? *Recommend one, then
    listen.*
26. **The dip** (1.5 ms) for ALGO / TYPE / PHRT / WAVE changes on a sounding voice. *Recommend yes.*
27. **MIDI tracks** (a track that sends notes instead of playing FM TONE)? *Recommend later.*
28. **LINE's big numbers**: Inter Tight 200 as font tables, or a stroke font drawn with the AA lines? *Recommend the
    font tables for 0.1* (the toolchain exists), a stroke font later if flash runs short.
29. **The black-key strip** at the foot of a held step's screens (LINE rule 5)? *Recommend yes*: it is the only place
    the eleven fields are shown, since the printed black-key labels are not used.
## 11. The firmware build plan (Phase 2, in order; nothing built)

This is Phase 2 (§13): the device. Steps 3, 4, 5 and 9 start from Phase 1's code instead of from nothing. Each step ends with its test passing; device steps are the owner's (only the orchestrator flashes).

| # | step | what it proves | its test |
| --- | --- | --- | --- |
| 1 | **The chassis**: a copy of TransitFM1 as the new repo's base; the app replaces `transit.c` with an empty screen and the keys / LEDs echoed; its identity and version string | the base builds; the stub is byte for byte (`stub.lock`); installs and leaves safely | `tests/run_tests.sh` (stub, ota, ldr, package); the build's stub hash; the interruption model for QUADRILLE <-> ChoralRoot <-> TransitFM1 |
| 2 | **Audio out**: ChoralRoot's audio ISR, the half buffers, the master and limiter, a test tone; the shed counters | the DAC path and the deadline on this chassis | host: a WAV of the tone bit for bit against ChoralRoot's; unit: `--debug` `stale_halves` 0, `pk_block_us` |
| 3 | **FM TONE as a library with a patch per voice** (`p = s->patch`), the 16 presets | the extraction and the per-voice patch change no sample | `cr_quad_test` ported (21 166 checks); the 16 presets' goldens equal to ChoralRoot's renders, one voice per patch; `quad_pops check` |
| 4 | **The voice pool**: tracks as owners, limit, reserve, steal, fade, the per-voice mix; MONO legato | allocation and stealing do what §2 says, without pops | host unit tests of the allocator (limits, reserve, choke, same note with another sound); `quad_pops` scenarios with preset-locked steps; a render of 8 tracks equal to the sum of 8 one-track renders |
| 5 | **The sequencer core, host first**: steps, rates, lengths, swing, micro and strum, ratchets, conditions, chance (seeded), sound locks, p-locks, trigless steps, the trigs resolved two steps ahead | the timing and the lock semantics, deterministic | golden event timelines (text: sample time, track, note, vel, the patch's hash) for a dozen patterns; locks resolved = a reference that applies them by hand; a negative micro-timing on step 1 |
| 6 | **Two cores**: the 16 voices split 8 + 8, the reverb's lines on core 1 | the split is bit-exact and fits | `cr_c1_thread_test`-style: inline = thread = unsplit, ThreadSanitizer 0; unit: 16 WOBBLE voices on 8 tracks, three sends, a 16th at 160 BPM: `pk_block_us`, `pk_c0_busy_us`, `pk_c1_busy_us`, `shed_voices` |
| 7 | **`quad_render` in RAM** | the gain P5 had (-26 %) | `ram_code.py cmp`; goldens unchanged; unit: the same scenario's `pk_c0_busy_us` |
| 8 | **Storage**: settings, user sounds, the pattern store (floating pages, commit last, erase ahead, GC when quiet), the dirty cache | saving while playing has no erase and no audible cost; power cuts are safe | host: a cut after every erase and page (`pool_test.c`-style); a counter test: a save while voices sound programs only; unit: save while 16 voices play, `erases_near` 0, `stale_halves` 0, `flash_prog_us` read |
| 9 | **The UI**: the step view, the held step's fields, the sound editor with locks, the overview, patterns, mute, mix, KEYS mode, LEDs (the chosen direction) | the gestures of §8 | the Mac emulator headless: scripts per gesture with frame dumps compared to the mock-ups; the key matrix to LED states |
| 10 | **MIDI and clock**: a channel a track, clock in / out, notes into a held step, program change -> pattern | it plays with other gear | host MIDI tests; unit: an external clock at 90 .. 180 BPM, drift |
| 11 | **USB audio** (if wanted): stereo recording | as ChoralRoot's | `uac_check.py` on the unit |
| 12 | **Content**: demo patterns (a drum beat on one track with sound locks, a chord track, a bass with ties), the factory sounds by ear | the owner hears it | the owner, on the unit |

## 12. Risks and what is not known

- **The unit's cost of a full pool** is an estimate from 8-voice readings; the steal-and-fade storm of a busy step is
  not measured. Step 6 reads it before the UI is built on it.
- **Flash page program time** (`flash_prog_us`) is not measured on the unit; the saving rule assumes about 1 ms.
- **The legacy sectors** (question 6) are free only if ChoralRoot's migration never reads foreign bytes there as its
  own: its loads check magic and CRC, to be confirmed in `upool.c` / `storage.c`.
- **ALGO and TYPE switches on a sounding voice** are rare (a LOCK step, a live turn) and the dip is new: to be heard.
- **Sixteen voices of one engine** make the shed's choice simpler (all weigh the same) but a shed is a voice dropping
  out; the allocator should never need it in normal patterns.

## 13. How it gets built: Phase 1 on the computer, Phase 2 the firmware

The owner (2026-10-18): build a version that runs on the computer and uses RemoteFM1 for the panel's input, LEDs and
screen. "We can go fast locally with no real firmware constraints and be able to quickly prototype, iterate, do large
changes and reorgs without ever having to make this work as a real firmware. Then once happy with it, Phase 2 would be
taking that and making it work as real firmware."

### What already exists (read 2026-10-18)

- **RemoteFM1** (`../RemoteFM1`, branch `m1`): the FM-1 as a USB panel. Keys, buttons, encoders and MASTER go up as
  MIDI; the host sends the LEDs (five levels) and the screen as tiles (FILL / PAL / RLE / RAW / COPY) into a shadow
  frame buffer. Measured on 5BD75BE3: bulk 830 KB/s and SysEx 375 KB/s to the unit, 20 fps sent, a UI frame 0.4 to
  2 KB. **M1 is done on the unit**: the owner played ChoralRoot through it ("Screen is good, input good").
- **`fm1remote`** (`RemoteFM1/host`): runs any `.fm1core` on the Mac, audio out of the Mac, the unit as its panel.
- **FM1VST** (`../FM1VST`): the core ABI (`core-api/fm1core.h`, ABI 5: `init`, `tick(ms)`, `frame`, `audio`, `midi_in`,
  a panel struct with keys, buttons, encoders, velocity, LEDs and the 240 x 240 LCD), cores for ChoralRoot, Felucca and
  Melodee, a conformance test every core passes, headless script and replay tests, and the plugin and Standalone with
  an on-screen panel. **Its "Remote FM-1" switch (branch `remote-fm1`) works on the unit** ("Everything works").
- Not built yet: RemoteFM1's browser back end (its M4: Web MIDI, WebUSB, the core in an AudioWorklet).

### Recommendation: Phase 1 is an `.fm1core`, run on the Mac

QUADRILLE's prototype is a core like ChoralRoot's: the same ABI, but built only for the computer, with none of the
device's limits. It then runs today, with nothing new to build around it:

| where it runs | what for |
| --- | --- |
| FM1VST Standalone, **Remote FM-1 on** | everyday playing and testing on the real unit: its keys, knobs, LEDs and screen, the sound from the Mac |
| FM1VST Standalone, on-screen panel | working without the unit (an agent, or the owner away from it) |
| `fm1remote --core quadrille --device` | the shortest loop, with `--stats` for the screen's bytes per frame |
| FM1VST's headless script runner | the tests: key presses in, LCD frames and audio out, compared against goldens and the mock-ups |
| FM1VST in Live | the sequencer against a DAW's MIDI clock |
| the browser, later | after RemoteFM1's M4 (its browser back end) and a WASM build of the core, as ChoralRoot's web emulator is built |

**Why a core and not a browser app written from scratch:** the whole chain from the unit to the core exists and has
been played on; a browser app would first need RemoteFM1's M4 and its own audio, panel and test harness. And Phase 2
stays short: the sequencer, the voices and the engine are already C, and the core ABI is the device's own shape
(a 1 ms tick, 128-frame audio blocks, a panel). What is fast in a browser (live reload, canvas drawing) Phase 1 gets
another way: a rebuild of the core takes seconds and a restart of `fm1remote` or the Standalone picks it up (a reload
button in FM1VST is a small addition), and the screens draw with a canvas-like library (below).

### No firmware limits in Phase 1, by design

- **C99 with floats, `malloc` and big tables allowed.** No RAM, POOL, flash or CPU budget is enforced. Fixed sizes that
  are part of the design (8 tracks, 128 steps a track, 4 notes a step, 16 voices) are constants that can change.
- **The engine is ChoralRoot's FM TONE** (`eng_quad.c`, copied with its history noted, the per-voice patch of §5 added),
  its effects (chorus, delay, reverb, the output chain) likewise. So what the prototype plays is what the unit will
  play. The voice count is a setting (16 to start; more for experiments).
- **The screen draws through one small drawing interface** (`qd_gfx.h`: line, polyline, circle, arc, ellipse, rounded
  rectangle, fill, text at a size and weight, a clip, an alpha), built in Phase 1 on an anti-aliased vector library
  (candidates: plutovg, MIT, C; canvas_ity, ISC, C++ with the HTML canvas's own calls), drawing into the panel's
  RGB565 LCD. line.html's drawings move across almost call for call. Text: Inter Tight through stb_truetype at any
  size and weight.
- **Saving is a file on the Mac** (the project as one versioned binary or JSON file in the core's folder), not the
  flash store of §7.
- **A budget meter instead of limits**: a hidden page (and `fm1remote --stats`) shows what the design would cost on
  the device: voices in use against 16, the estimated core-0 time from §3's per-voice costs, the lock pool against
  1024, the pattern's bytes as §7 would store it, the screen's bytes per frame over RemoteFM1. Nothing is refused; a
  design that would not fit is seen early.

### Seams kept clean for Phase 2

The prototype is built in layers, so Phase 2 replaces the bottom of each and keeps the rest:

| layer | Phase 1 | Phase 2 |
| --- | --- | --- |
| `seq/` the model and the sequencer: patterns, tracks, steps, locks, conditions, timing, trigs resolved ahead | plain C, deterministic, sample-accurate, tested headless | the same code; `malloc` → static pools |
| `voice/` the pool, the steal, the per-voice patch; FM TONE; the effects | ChoralRoot's C, floats where it has them | the same code; split across the two cores (§3) |
| `ui/` screens, gestures, the knob takeover, LEDs | C over `qd_gfx.h` | the same code |
| `qd_gfx` | the vector library | the device's own AA primitives (§9a "What LINE asks of the firmware") |
| `store/` | a file | the floating page store (§7) |
| `glue/` | the core ABI (FM1VST's `core-glue/` pattern) | TransitFM1's platform (the HAL, the boot stub, the update path) |

The one rule that keeps Phase 2 honest: **nothing in `seq/`, `voice/` or `ui/` calls the operating system** (no
files, threads, clocks or printing); all of that goes through `glue/`, as a firmware's would.

### Phase 1 milestones

| # | milestone | done when |
| --- | --- | --- |
| P1.0 | **The empty core**: QUADRILLE loads in FM1VST and `fm1remote`, passes the core conformance test, draws the LINE idle screen through `qd_gfx`, keys light their LEDs | the owner sees the quadrille dance on the unit's screen through Remote FM-1 |
| P1.1 | **The sound**: FM TONE with a patch per voice, the voice pool, the effects; KEYS mode plays the selected track with velocity | the 16 presets sound as on ChoralRoot (its goldens, one voice per patch) |
| P1.2 | **The sequencer, headless first**: steps, rates, lengths, swing, micro-timing, strum, ratchets, conditions, chance, sound locks, p-locks, trigless steps; MIDI clock in | golden event timelines for a dozen patterns (§11 step 5's tests, moved here) |
| P1.3 | **The step view and the held step**: the white keys as steps, the black keys as the fields, every field screen of §9a, the knob takeover, the LEDs of §8 | scripted presses produce the mock-ups' screens; the owner programs a beat on the unit |
| P1.4 | **Sound and pattern work**: the sound editor with p-locks, tracks, patterns and chains, mute, mix, the effects screens | a whole demo song made on the unit |
| P1.5 | **Keep it**: save and load the project, copy / paste / clear, undo, the budget meter | the owner uses it for a session without losing work |
| P1.6 | **Settle**: the owner's changes, as many rounds as it takes; then PLAN.md is rewritten from what the prototype is | the owner says the design is right |

Agents write the code from briefs (the ChoralRoot rule); the core is built through FM1VST's `fm1_add_core`
(QUADRILLE as a submodule under `cores/`, its glue in `core-glue/quadrille/`), with no change to FM1VST's other
cores. Phase 1 never flashes anything: the unit runs RemoteFM1 throughout.

### Phase 2

§11's steps, on TransitFM1's chassis as planned (with the XIP re-stage fix RemoteFM1 found: its
`handoff/TRANSITFM1-FIXES.md`), now
starting from Phase 1's `seq/`, `voice/` and `ui/` and its tests. What is new work there: the device's `qd_gfx`, the
two-core voice split, `quad_render` in RAM, the flash store, `malloc` replaced by pools, and whatever the budget meter
said would not fit. The headless tests of Phase 1 stay the acceptance tests: the device build must produce the same
event timelines and the same screens.

### Questions for Phase 1

31. **A core run on the Mac or a browser app first?** **Decided 2026-10-18: the core on the Mac** (the owner: "Ok, lets
    go with mac core then"). Everything around it works today, and it can be tested headless; the browser can follow
    from RemoteFM1's M4.
32. **C99 for the prototype (recommended) or C++?** *Recommend C99*: the code moves to the device as it is; C++ would be
    quicker to write only for the drawing, which the library covers.
33. **The drawing library**: plutovg (C) or canvas_ity (C++, the canvas's API)? *Recommend plutovg*, behind
    `qd_gfx.h`, after a one-day trial of both drawing screen 2 and screen 12.
34. **Where the code lives**: in this repo (`FMToneSeq/`, beside the plan and the mock-ups) as FM1VST's submodule.
    *Recommend yes*; rename the repo when the name is chosen (question 1).

### Phase 1 log

**2026-10-18, P1.0 done** (FMToneSeq ea6f8bd; FM1VST branch `core-quadrille` b1a444b in the worktree
`FM1VST/.claude/worktrees/quadrille`, from `ui-device-only`, core ABI 6):
- The core: `core/glue` (the only code that knows the ABI), `core/app`, `core/ui` (`ui_idle.c`, the idle screen ported
  from line.html's s1; `ui_kit.c`, line.html's helpers), `core/gfx` (`qd_gfx.h` on plutovg 1.3.3, vendored in
  `third_party/`), Inter Tight at 200..700 made by `tools/make_fonts.py`. PLAY starts and stops the dance, SELECT and
  KNOB 1 set the tempo, held keys and buttons light, PLAY's green LED blinks on the beat. A sine per held key stands in
  for FM TONE until P1.1, because FM1VST's conformance test needs a key to sound.
- FM1VST: one new file, `core-glue/quadrille/core.cmake`, building from the live FMToneSeq tree (`QUADRILLE_DIR`), and
  the option `FM1_ONLY_CORES` (empty by default: every core) so the worktree builds only QUADRILLE.
- Tests: `ctest -R 'quadrille|common'` 5 passed, 1 skipped (no parameter map); the conformance test passes with the
  flash and reboot checks declared skipped. `make -C tools frames` renders headless frames; the idle frame matches
  `mockups/png/line-01-idle.png` (mean difference 1.2 / 255, only anti-aliased edges).
- **On the unit** (5BD75BE3 running RemoteFM1, the worktree's Standalone with Remote FM-1 on), the owner: "QUADRILLE
  shows on the unit; PLAY and the knobs work."
- Found on the way: `fm1remote` (RemoteFM1 `m1`) still speaks ABI 4 and does not build against ABI 6; until it is
  ported, the Standalone is the way onto the unit.

**2026-10-18, P1.1 built** (FMToneSeq 0d0e334; FM1VST `core-quadrille` 4084c06; version 0.0.2-p1.1):
- FM TONE lifted out of ChoralRoot v0.25.1 into `core/voice/quad/` with a patch per voice and the per-part state
  (FREE LFO phases, tempo) moved into a per-track owner; every change marked "QUADRILLE" in the copied code.
  **Bit-exact**: INIT + the 16 presets x 10 scenarios, 8,622,880 samples identical to ChoralRoot's own engine built
  from the tag. ChoralRoot's FM TONE checks ported: 26,570 pass (ASan, signed-overflow sanitizer).
- Found in ChoralRoot's code: `quad_bwk(12700)` reads `QUAD_BW_K[128]`, one past the table (multiplied by 0, so no
  audible effect); guarded here, worth fixing upstream.
- The voice pool (§2: limit, reserve, choke, steals with ChoralRoot's one-block fade; 73 checks), the per-voice mix,
  ChoralRoot's chorus, delay, reverb and Output Clean (impulses and the whole chain identical to ChoralRoot's code).
  DRIVE left out for now.
- Eight tracks (`core/seq/model`): DRUMS (MARIMBA), BASS (BASS, mono), KEYS (EP), LEAD (SQUARE LEAD), PLUCK, PAD
  (GLASS PAD), BELL, NOISE (NOISE-ISH); reverb send 20 each.
- Playing: the keybed plays the selected track (OCT -2..+2, velocity), MIDI channels 1..8 play tracks 1..8, TRACKS +
  white key 1..8 selects, PRESETS picks the sound with the full-screen picker, KEYS shows the keybed screen.
- Host CPU for 16 voices + effects: 21.5 us mean, 62 us worst per 128-frame block (0.7 % of the device's 2902 us).

**2026-10-18, P1.2 built** (FMToneSeq 91f111d; FM1VST `core-quadrille` 561d59a; version 0.0.3-p1.2):
- The sequencer (`core/seq/`): §4's 12-byte step, the sorted lock pool (1024, `QD_EFULL` when full), 64 patterns, an
  editing API for P1.3. Time in 50400 ticks a 16th (every rate, micro, strum, ratchet and swing is whole) and an exact
  sample transport; steps resolved two of their track's steps ahead. Rates 1/8..2 plus 2/3 and 4/3, lengths, swing,
  micro, strum, ratchets with ramps, the length table (filled: 12 16 24 32 48 64 96, INF), every condition, seeded
  chance, sound locks, p-locks and mix locks (ids 73..78: PAN DELAY REVERB CHORUS DRIVE LEVEL), LOCK steps, TIE legato
  on mono tracks, live edits, the 1.5 ms dip (filter TYPE change: largest step 15,548 -> 452), pattern change and
  chains, MIDI clock in. The choices where §4..§6 were open are in the commit and the P1.2 report.
- Tests: 16 golden timelines, each matched by an independent plain-loop reference; test_seq 1,071 checks; the engine
  and effects still bit-exact.
- Demos: A01 a beat on DRUMS alone (kick from INIT with a pitch-drop LFO, snare / clap / ghosts / hats from NOISE-ISH,
  all sound locks + p-locks, made by measurement: to be tuned by ear), A02 the beat with bass, chords and a lead, A03
  polymeter on all 8 tracks. On the unit: PLAY, PATTERN + white key, TRACKS shows the eight clocks (white 9..16 mute),
  REC held = FILL.
- Open: MIDI clock resolved to the 32-sample block (not tried with real gear); A03 steals across tracks (82 in 32 s).

**2026-10-18, P1.3 built** (FMToneSeq 9a26749; FM1VST `core-quadrille` 0ccf9eb; version 0.0.4-p1.3):
- Programmable on the unit: the step view (s2 from the real pattern, page follow), white-key taps and holds (250 ms),
  several held, the latch, FUNC + tap = LOCK step, the eleven black-key fields with their LINE screens, CLEAR with
  undo, the knob takeovers (FREQ, DECAY, HARM, FDBK, ALGO, tempo / swing, the tracks screen's LEN, RATE, LIMIT, SWING),
  p-locks from any unused knob with a step held, KEYS step and live recording, MIDI notes into a held step, §8's LEDs.
- The controller is table-driven (`core/app/ctl*.c`), the screens draw only from the model and the UI state
  (`core/ui`, `view.c`). Choices where §8 was open: the field memory, latch = OCT+ (FUNC + OCT+ in NOTE, where the
  OCT keys move the notes), unused field knobs make quick-page locks, the chord shapes on SELECT, power-on keys only
  wake. The table of every field's knobs is in the P1.3 commit; `PLAYING.md` is the owner's guide.
- Tests: test_ctl 146 checks (16 gesture scenarios, 45 field mappings, LEDs, determinism); "program a beat" from
  presses and turns alone equals the same pattern built through the API, byte for byte and in its timeline. The
  conformance test turns KEYS on first (`sound btn ARP 60`).
- Not yet: the sound editor, pattern screen, MUTE, MIX, SAVE, copy / paste, the perform offsets (P1.4, P1.5).

**2026-10-18 evening, onto the latest FM1VST and RemoteFM1:** `core-quadrille` merged FM1VST `main` a424a24 (the
FM-1-alone editor, settings behind a corner button) as 0d307c0, and its `remote/RemoteFM1` moved to RemoteFM1 `main`
56dccd4 (0.1.1 and the web side; protocol and host library unchanged) as 9758888. Rebuilt; `ctest -R
'quadrille|common'` passes; the Standalone relaunched with P1.3 on the new editor.

**2026-10-19, P1.3b built: the controls of §8b** (FMToneSeq 1390e98; FM1VST `core-quadrille` f64a372; version
0.0.5-p1.3b; the Standalone relaunched with it):
- The model: a 16-byte step with an override bit per parameter (NOTE VEL LEN MICRO RATCH CHANCE COND TRIG SOUND
  STRUM), track defaults resolved when the step is resolved, 8 pages per track with their own start / end and a loop
  mask (the track length is gone), the sound pool (INIT, the 16 factory sounds, KICK SNARE CLAP GHOST HAT OPEN HAT),
  14 scales per track with a global key, solo, a step and a page clipboard, one undo level per track.
- The controls as §8b: the black keys as hold modes and as ten values, F#5 clear, ALGORITHM the mode's value,
  SELECT pages, PRESETS the sound lock with a step held, F1 / F2 / F3 / SHIFT layers with their on-screen maps, the
  mode lanes, REC = keyboard, REC + PLAY, SHIFT + REC, ARP = FILL, PAGE = follow. Temporary until the track settings
  page: SHIFT + ALGORITHM = scale, SHIFT + PRESETS = root.
- Tests: test_seq 2,306 (the 16 old goldens unchanged, 6 new: pages, loops, defaults, pool, scales, solo; the
  reference walks pages and resolves defaults itself), test_ctl 201 (21 scenarios, every §8b gesture), program-a-beat
  by presses alone equal to the API build. The demos render byte-identical to P1.2.
- A change the agent made to the look: each mode has its own colour (NOTE blue, VEL green, LEN cyan, MICRO violet,
  RATCH orange, CHNC yellow, COND magenta, TRIG white, SOUND lime, LOCKS sand) instead of §9a's colour = knob. To be
  judged by the owner.
- Removed from P1.3: the latch, FUNC + tap LOCK steps (TRIG B2 makes a LOCK step), the "last entered" defaults, the
  quantize toggle.

**2026-10-19, P1.3c built: §8b version 2.1** (FMToneSeq dff2194; FM1VST `core-quadrille` 6da47b1; version
0.0.6-p1.3c; the Standalone relaunched with it): tracks on SEL (tap mute, hold solo, F#5 clears all), patterns on SEQ
(tap queue, hold now, ALGORITHM = launch END / BAR / BEAT / NOW), F1 quick copy / paste and its page layer (tap
edit, double-tap alone, hold in / out, A..B, B9 clear page / hold clear track, B10 copy a page to others), F2 Form's
pick-up / drop (checked against omx_mode_form.cpp v1.15.4), five notes a step (SELECT note 1, KNOB 1..4 notes 2..5),
P-LOCK on F#5 (the screen stays; a per-screen table of lockable knobs for the sound editor), the octave and bank
animation with the OCT LEDs blinking 500 / 250 / 125 ms, F3's knobs, rates as a base (1/1..1/64) and a remembered
type (normal, dotted, triplet). The step is 18 bytes; ticks doubled to 100800 a 16th so every one of the 21 rates
divides by micro, strum and ratchets. test_seq 2,761 (27 goldens, p01..p22 unchanged), test_ctl 296; the demos
render identical. One undo level per F1 / F2 hold.

**2026-10-19, P1.3d built: §8b version 2.2** (FMToneSeq f06a422; FM1VST `core-quadrille` b8ca2ac; version
0.0.7-p1.3d; the Standalone relaunched): the knobs take over the hold modes (one table `KNOB_JOB[mode][knob]`; held =
the step, nothing held = the track's defaults; a band over the top of the screen names each turn), P-LOCK's
ALGORITHM chooses the track, banks on SEQ + B1..B4, the preview while stopped (`qd_player_preview`), F3 latched with
a reusable latch table (OCT− back, OCT+ confirm, HOME closes), the step latch on LFO, LOCKS dropped (B10 reserved).
No model change (the defaults already had every extra). test_ctl 452; old goldens unchanged.

**2026-10-19, P1.3e built: §8b 2.2's later bullets** (FMToneSeq 4523ac3; FM1VST `core-quadrille` 88ef5fb; version
0.0.8-p1.3e; the Standalone relaunched): a held mode key edits the track's default on a full TRACK DEFAULT picture
(after 250 ms or a knob; B11 resets to C4 / 100 / 1 step / ...), nothing-held default turns show the same picture,
six notes a step (ALGORITHM = note 6 on a held step, the default chord still transposed by ALGORITHM), the quiet
KNOB 4, the step latch (tap toggles, hold several, an empty selection gives the mode keys back), HOME cycles PIANO
ROLL / STEPS. The step stays 18 bytes (the pad byte became note 6). test_seq 2,794 (p28-six new), test_ctl 517.

**2026-10-19, P1.3f built** (FMToneSeq 9ae7dd2; FM1VST `core-quadrille` 8ce3283; version 0.0.9-p1.3f; the Standalone
relaunched): notes stored as pitches (slot k = note k; the step still 18 bytes), NOTE's knobs each move their own note
and ALGORITHM all of them (held step, latch selection, NOTE key), the step latch selects by gesture (the pressed set
replaces the selection; a short tap on the sole selected step empties it), STEPS redrawn as an overview of all 8
pages, 16 cells a row in fours. test_ctl 528, test_seq 2,795; all 28 goldens unchanged.

**2026-10-19, P1.3g** (FMToneSeq 9d05475; FM1VST `core-quadrille` 6c298f4; version 0.0.10-p1.3g; relaunched):
SELECT in the STEPS view changes the page with no pop-up (the edited row moves); PIANO ROLL keeps its page picker.

**2026-10-19, P1.3h** (FMToneSeq 8f72460; FM1VST `core-quadrille` 498c992; version 0.0.11-p1.3h; relaunched): in the
F3 menu SELECT = the page, ALGORITHM = the tempo, SHIFT + ALGORITHM = the swing, none with a pop-up.

**2026-10-19, P1.3i** (FMToneSeq 90647e0; FM1VST `core-quadrille` d00b906; version 0.0.12-p1.3i; relaunched): the
owner found ALGORITHM not setting the tempo in F3. The cause: a MIDI START + STOP (Ableton Live was running) left the
sequencer following an external clock until PLAY was pressed, and every tempo turn was ignored silently. Now STOP ends
it, half a second without a pulse ends it (the transport stops), and a turn while a clock really runs shows EXT CLOCK.
A new ABI-level test (`make -C tools abi`) loads the built module as the host does.
- The owner (2026-10-19): when an external clock stops arriving, the unit stops (kept as P1.3i built it).

**2026-10-19, P1.3j and P1.3k: FIZZ** (FMToneSeq ab8bc8b, d606920, ec5f7f2; FM1VST `core-quadrille` ce24407, c6195df,
merge 4894c6b of FM1VST main bf10c77 = 0.5.0; version 0.1.0-p1.3k; the Standalone relaunched with `fizz.fm1core`):
- The owner's tempo report ("ALGORITHM in F3 changes nothing", with SELECT and KNOB 1..4 working) is **not
  reproduced**: through the ABI (`make -C tools abi`) and through FM1VST's own Remote FM-1 path with the fake unit
  (`make -C tools remote`) ALGORITHM moves the tempo. Open: to watch on the unit with `tools/build/midimon`. Fixed on
  the way: PRESETS in F3 leaked to the step view; the F3 tempo label is bigger and lights when turned.
- STEPS: the edited page unfolds into tall cells with its notes inside, the other pages folded, a hopping marker,
  trigs popping, a breathing open row.
- **The name is FIZZ**: the core id `fizz`, FM1VST `core-glue/fizz`, `FIZZ_DIR`, README.md and LICENSE (GPL-3.0); the
  idle screen is bubbles rising through the drink and bursting on the beats (`core/tests/out/fizz-idle.gif`).
- FM1VST 0.5.0 merged without conflicts and with no ABI change (two new optional symbols FIZZ doesn't need).

**2026-10-19, P1.3l** (FMToneSeq a713a21; FM1VST `core-quadrille` c6d488c; version 0.1.1-p1.3l; relaunched): the idle
screen is a glass filling and fizzing over (the owner's cola photo): a 9.4 s loop of pour, foam, overflow with drips and
spray, FIZZ rising from the foam, settle, fade; the spray pulses on the beats when a MIDI START plays it
(`core/tests/out/fizz-glass.gif`).

**2026-10-19, P1.3m** (FMToneSeq 71c3500; FM1VST `core-quadrille` 2903c71; version 0.1.2-p1.3m; relaunched): the logo
screen per the owner: no footer, zoomed on the top of the glass, everything outlined in greys on black (no colour,
checked by a test), the same loop (`core/tests/out/fizz-logo.gif`).

**2026-10-19, P1.3n** (FMToneSeq 3fc0d21; FM1VST `core-quadrille` 49a10a4; version 0.1.3-p1.3n; relaunched): the logo
screen without the glass and straw: a thin wavy surface line, fine outlined bubbles popping with tiny splashes, the
fizz peak with FIZZ rising above the line.

**2026-10-19, P1.3o** (FMToneSeq 257504a; FM1VST `core-quadrille` 36df912; version 0.1.4-p1.3o; relaunched): the logo
screen from the owner's champagne close-ups (ReferenceImages/FizzyBubbles/Champagne01): submerged, in depth (bokeh,
specks, crisp middle bubbles, near bubbles with a lit rim and tiny bubbles inside), a bed feeding the rise, a swell
that brings FIZZ in; greys on black, an 11 s seamless loop.

**2026-10-19, P1.3p: the FIZZ logo** (FMToneSeq c280073; FM1VST `core-quadrille` 97793a2 on the merge 08b4825 of FM1VST's
local `screen-copy` (copy the screen to the clipboard, x2 / x4, PNG and raw files, Freeze); version 0.1.5-p1.3p;
relaunched): after five logo mock-ups (`mockups/fizz-*.html`) the owner picked the champagne in the app's colours with
the word from particles: big bubbles turning with tiny bubbles floating inside (no white highlight), FIZZ gathering
from 248 particles in ~0.5 s, solid while it holds, breaking back into particles that fizz away. Letters sampled
through a new `qd_text_mask` behind qd_gfx. STEPS: the edited page's row at half height (62 px).

**2026-10-19, P1.3q** (FMToneSeq 48258cf; FM1VST `core-quadrille` f885e13; version 0.1.6-p1.3q; relaunched): STEPS colours
steps by kind (own notes warm white, track notes cyan, ghost / LOCK a violet outline), fills the open row too with the
notes drawn dark on it, velocity as the fill height, a length line under held / selected steps, PRESETS as a step
cursor (the latch, STEPS stays on screen), the NOTE-mode override bars dropped. Also 81f5718: the logo's inner bubbles
orbit in whole turns (they jumped at the 11 s wrap).
- The owner (2026-10-19): with the PRESETS cursor, STEPS stays on screen (kept as P1.3q built it).

**2026-10-19, P1.3r** (FMToneSeq 51ffc81; FM1VST `core-quadrille` 273195f; version 0.1.7-p1.3r; relaunched): HOME cycles
PIANO ROLL, STEPS and **EDIT** (the edited page as one row at the bottom, the hold mode's picture re-laid-out above it,
the value strip between; both update together; no pop-ups over it); the PRESETS cursor walks all 8 pages in order (8
wraps to 1); pop-ups and held screens drop the track from their headers.

**2026-10-19, P1.3s** (FMToneSeq 78a5c2a; FM1VST `core-quadrille` bb52f8d; version 0.1.8-p1.3s; relaunched): EDIT draws
each mode at full held-screen size with the value in the header and the row below; the black-key value strips are
gone everywhere (a small B11 RESET word with a mode key held); "KEYS OCT n"; beat dots over steps 1 5 9 13 in the
lanes instead of step numbers; the playhead no longer outlines cells (only held / latched / selected steps do).

**2026-10-19, P1.3t** (FMToneSeq 1a8f2ca; FM1VST `core-quadrille` 3bf77d9; version 0.1.9-p1.3t; relaunched): the NOTE
drawing (held and EDIT): the keybed low, the notes as one readable line in their knob colours directly above it, no
tiny names, no "A = ALL", no "SELECTED" anywhere, no "KEYS OCT"; only the scale name; the header value is the chord
name when there is one.

**2026-10-19, P1.3u..P1.3x** (FMToneSeq b1da3fe, 149e79f + 2b8bea1, 3c2ac64, 6c30f5b; FM1VST `core-quadrille` 194297c,
2a4521a, b56cdee, 3251026; version 0.2.2-p1.3x; relaunched):
- P1.3u: EDIT · NOTE's chord name big and centred; VEL's value big beside the mallet.
- P1.3v, the clarity pass: `research/TEXT-AUDIT.md` (158 text calls, each kept, enlarged, made a shape or removed);
  labels at 12 px 600; the layer maps drawn as the panel's keys with icons; no hint sentences; a test guard that fails
  under 11 px (the smallest text drawn is now 12 px). Exceptions listed in the audit.
- P1.3w: FUNC on B9 (none, RESTART, REVERSE, FORWARD, FLIP, RANDOM JUMP, RANDOM FUNCTION, JUMP 1..128; Form's target
  gesture), SOUND on B10; the engine applies functions in resolution order with a per-track direction (forward on PLAY);
  goldens p29, p30 with the reference's own walker. The step is 20 bytes.
- P1.3x: PLAY held opens F3 (a tap acts on release); F3 SELECT = tempo, ALGORITHM = swing with the metronome picture;
  REC held arms live recording, the first note starts it, an empty track grows to 8 pages, REC / PLAY end it with
  playback going on; placement at the track's rate with micro-timing. SHIFT + REC stays the step-record toggle.
- 2026-10-19 evening: the owner asked for RemoteFM1 back on the unit. It ran ChoralRoot 0.25.1 (FM-1_920): data backed up
  first (`fm1-backups/before-remotefm1-20261019/choralroot-data-backup.json`: settings, 128 user sounds, favourites, loop
  slot 1), then RemoteFM1 0.1.1 installed (`remote-0.1.1.fwsc` sha256 a7408d6c..., matching its SHA256SUMS); the unit
  answers FM-1_015 with its vendor interface; the FIZZ Standalone relaunched.

**2026-10-19/20, P1.3y, P1.4a, P1.4b** (FMToneSeq 3154357, d353c7e, 79256ba; FM1VST `core-quadrille` 6bb6f4b + the
merge of main ffdbb7e 189e2be, 647073c, ea052de; versions 0.2.3-p1.3y, 0.3.0-p1.4a, 0.3.1-p1.4b; each relaunched):
- P1.3y: FUNC's LEAP n (QD_FN_LEAP, golden p31-leap) on B8, JUMP on B9 with B9 held + W as the target; HOME returns to
  the view in one press, SHIFT + HOME cycles; with no step held SELECT = page, ALGORITHM = cursor, PRESETS = preset
  (defaults only with the mode key held); LFO held + a black key = the hold mode; the TRACK frame (magenta 0xf03cff) on
  every track-default screen; FX held + ALGORITHM = the track. The owner's additions: SHIFT + REC record settings (MODE
  LIVE / STEP, QUANTIZE, TAKE REPLACE / OVERDUB, in `qd_project_t`); nearest-value key lights in every mode; SHIFT + SEL
  global settings with KNOBS layouts A..D (D added: SELECT cursor, ALGORITHM value) and PAGE FOLLOW; a SEL tap shows
  nothing (the map after 250 ms). SHIFT + REC / SEL open over the step view.
- P1.4a: the sound editor on EDIT (latched): B1..B5 groups, a group key again = its next page, 18 pages (the 74 values
  do not fit five groups of two), KNOB 1..4 the page's values, ALGORITHM a fifth where a page has one, SHIFT + a knob =
  the finest step, PRESETS the preset / sound lock. Nothing held = the track (TRACK frame); a step held = p-locks
  (padlocks, the track's envelope dashed behind, B11 clears the page's locks). DRIVE = patch value 73 (QD_QUAD_NP 74),
  ChoralRoot's `track_dist` per voice (bit-exact test), loaded with the preset, p-lockable. Open for the owner: each
  page's foot row of four values vs the "no knob-value rows" rule.
- P1.4b: a SEL tap = the track's settings (SCALE, ROOT, FOLLOW KEY, MIDI CH; page 2 VOICES, SWING, PAGE FOLLOW; B1..B8
  other tracks); ENV = MIX (faders, pan, meters, sends; ENV again = the sends page); FX = ChoralRoot's effects, every
  parameter (DELAY, DELAY 2, REVERB, REVERB 2, CHORUS, MASTER; B1..B4, again = the second page). The clocks screen is
  retired (its values now live on F3, the track settings and SEL); PAGE + W is gone.

**2026-10-20, P1.5a** (FMToneSeq 277ccf8, 4cabcb5; FM1VST `core-quadrille` 89e4272, 4934b52; versions 0.4.0-p1.5a,
0.4.1-p1.5a; relaunched; RemoteFM1 0.1.1 back on 5BD75BE3 after a data backup to
`fm1-backups/before-remotefm1-20261020/`):
- Part 1: each pattern carries its 8 tracks' sounds (`snd[8]`: patch, pool source, edited, name); `core/store`, §7's
  floating page store (CRC'd 256 B pages, commit last, tombstones, GC, §7's playing rules with counters; test_store:
  958 power cuts); `core/app/persist`: the library (32 sectors) and the projects (224 sectors) in the core's 1 MiB NOR
  image, written by itself 1.5 s after the last change; eight projects; the restart restores project, pattern, track.
  The Standalone's image: `~/Library/Application Support/fm1emu/fizz/flash.bin`, written on quit.
- Part 2: SAVE = the sounds page (LIBRARY / POOL on B9, store with arm / replace, F2 into the pool or onto the track,
  W audition, B10 the name editor, F1 / F2 / B11 + W copy, move, delete), a sound classifier for the auto-names; SEQ
  tapped = the project page and the projects. Deviations: delete has no undo yet (P1.5b), project copy works as sound
  copy, "x6" for the lock count (the font has no ×), names 12 characters.

**2026-10-21, P1.5b** (FMToneSeq 59abb7f; FM1VST `core-quadrille` e3189d5; version 0.5.0-p1.5b; relaunched; RemoteFM1
0.1.1 reinstalled on 5BD75BE3 over ChoralRoot 0.26.1 1-core after a backup to `fm1-backups/before-remotefm1-20261021/`):
version 2.4's eight points. The editor's keyboard (REC; SCALE: W1 the root at octave 3, black keys the pages; FULL:
pages on SELECT; EDIT KEYS on SHIFT + REC KNOB 4); SHIFT + ALGORITHM = scale, SHIFT + PRESETS = root with the keyboard
on (the step view's mapping kept everywhere); the scale's LEDs (roots 80 %, scale 50 %); lock chips (filled = this
step, outlined = some step) in place of the padlock; the editor head's dots first; FX OVERVIEW (opens there; W1..W8 the
track, KNOB 1..3 its sends) and ROUTING (ChoralRoot has none: DELAY > REVERB, CHORUS > DELAY, CHORUS > REVERB, the
filter AFTER / BEFORE; all 0 = ChoralRoot's exact path, bit-exact unchanged; DELAY > REVERB one block late); knob
acceleration (`qd_knob_accel`: x1 at 120 ms per detent, up to 1/16 of the range at 15 ms; <= 24 steps and SHIFT
never); latch modes VALUES / MODES (LFO held alone; saved).

**2026-10-21, P1.5d** (FMToneSeq 2c0735e; FM1VST `core-quadrille` ba521e0; version 0.5.1-p1.5d; relaunched): version
2.5. The frames swapped: held-step screens framed in `UI_STEP_FRAME` 0xf03cff, track defaults unframed; the sound
editor framed dark orange `UI_SND_FRAME_TRACK` 0x8a3412 (the track's sound) or bright `UI_SND_FRAME_STEP` 0xff6a2c (a
step's locks). VEL is the caped stunt rider (the mock-up ported to ui_held.c; a kick of up to 9 degrees and a puff when
the step plays; in EDIT the value sits in the head row). `qd_path_paint` added to qd_gfx.

**2026-10-21, P1.5e** (FMToneSeq 8918b4e; FM1VST `core-quadrille` e2cdb69; version 0.5.2-p1.5e; relaunched): the VEL
rider in the owner's neon style (mock-up version 2): blue / red / cream, each line glowing (three strokes, `rn_glow`
with `qd_get_alpha()`), tubes, ringed wheels, the forked cape. A held VEL frame takes about 0.55 ms on the Mac against
0.28 ms for NOTE: the screen to measure first on the FM-1 in Phase 2. The default keeps its magenta value and head.

**2026-10-21, P1.5f** (FMToneSeq 65804b8; FM1VST `core-quadrille` a1798be; version 0.5.3-p1.5f; relaunched): VEL 127's
stunt show (`rd_jump`, `rd_bus`, `rd_ramp`, `rd_fireworks` in ui_held.c, the mock-up's constants): the bus jump with
fireworks wherever the VEL picture shows 127, a burst of its own when the step plays there. A jump frame 0.9 ms on the
Mac against 0.53 ms for the wheelie.

**2026-10-21, P1.5g** (FMToneSeq 72858f1; FM1VST `core-quadrille` faa6032; version 0.5.4-p1.5g; relaunched): VEL 127's
show version 2 (`rd_show`): the bike stays as at 126 (rear axle x 138, scale 1.4, the full wheelie), the ramps and the
eight buses scroll under it, a small arc, three bursts as the buses' middle passes; a burst of its own at (70, 92) when
the step plays at 127. A show frame about 1.0 ms on the Mac (the wheelie 0.53 ms).

**2026-10-21, P1.5h, P1.5i** (FMToneSeq 9bf7cb7, 0f04d4a; FM1VST `core-quadrille` 30f7aa8, 551ca5c; versions
0.5.5-p1.5h, 0.5.6-p1.5i; relaunched):
- P1.5h: holding LFO alone opens the latch-mode screen at 400 ms while held; SELECT sets VALUES / MODES at once; the
  release keeps it (no toggle on release); any key closes it, a black key still picks the hold mode.
- P1.5i: RATIO B as ChoralRoot's one value (the pair index B1 x 19 + B2, 0..360, default 60; one step per detent, never
  accelerated; SHIFT steps B1; the readout B2 over B1; a lock takes both halves); the RATIO page is C, A, B, MIX (MIX
  moved from TONE, whose KNOB 4 is now empty); the mix drawn as the two outputs' wires (thick and bright by their share,
  never gone) and a crossfader in KNOB 4's orange by the speaker.

**2026-10-21, P1.5j** (FMToneSeq 5e6791a; FM1VST `core-quadrille` 90c7151; version 0.5.7-p1.5j; relaunched): the
editor's foot as four fixed 56.5 px columns, one 52.5 x 32 chip each (drawn only when locked, the value centred), 4 px
clear of the frame on every page (a margin test over all 18 pages); a shorter RATIO picture (the bus at y 150); the
mix in its own row, 180 px, centred, its ends the outputs' carriers; operators fading (brightness and saturation, 30 %
floor) by the share of the output they reach (one reaching both stays full).

**2026-10-21, P1.5k** (FMToneSeq dbb21ad; FM1VST `core-quadrille` d19f76b; version 0.5.8-p1.5k; relaunched): the LFOs in
the sound editor. B6 / B7 / B8 = LFO 1 / 2 / 3 (again: the WAVE page); the LFO page SPEED, MULT, DEPTH, DEST
(ChoralRoot's list and order, the name big with an arrow), the WAVE page WAVE, PHASE, TRIG, FADE (ALGORITHM idle there);
hold an LFO key + turn a value's knob = map it (the first detent) and set the signed depth, a takeover while held,
invalid values refused with a shake, release stays on the page; with a step held these are p-locks; LFO marks (a
numbered dot, `UI_LFO_COL` cyan / yellow / violet) by the label of every modulated value.
- 2026-10-21, the owner on P1.5k: hold-to-map (hold an LFO key, turn a value's knob to make it the destination and set
  the depth) is "quite good": keep it as it is; carry it into Phase 2 unchanged.
