# FM TONE gets the Digitone II's features (a plan, 2026-10-22)

The owner wants FIZZ's FM TONE to gain what the Digitone II importer (`tools/dn2pst.mjs`, `research/DN2-FORMAT.md`)
drops today:

- the **AHD amp envelope**;
- the **filter machines**: MULTI-MODE, EQ, COMB+, COMB−;
- **bit and sample-rate reduction** as "a 4th effect";
- **portamento**;
- **more modulation slots**.

This is a plan only. No code has changed.

**Sources**

| tag | source |
| --- | --- |
| **TR** | `ReferenceManualsAsMarkdown/Digitone/Digitone_II_FM_Tone_Technical_Reference.md` |
| **M1** | the Digitone manual, OS 1.44, same folder |
| **FMT** | `research/DN2-FORMAT.md` |
| **EQ** | FIZZ's engine, `core/voice/quad/eng_quad.c`. The line numbers are today's. |

## 0. In one page

- **Everything new defaults to "off, as today".** The renderer takes today's path, operation for operation, whenever a feature is off:
  - every factory sound, every saved sound and every ChoralRoot `.dnsnd` import renders bit-identically;
  - `test_quad_bitexact` (FIZZ's FM TONE against ChoralRoot v0.25.1's), `test_fx_bitexact` and the goldens stay identical;
  - this is checked at every milestone.
- **The new DSP sits beside ChoralRoot's code, not inside it.** It goes in a new file, `core/voice/quad/quad_ext.c`, included by `quad_voice.c` like `eng_quad.c`. `quad_render` calls it at four marked hook points (`QUADRILLE (e)`), each behind a "feature on?" test:
  1. after the amp envelope's tick;
  2. at the filter;
  3. after the filter;
  4. at the LFO sum.
- **It stays integer** (Q15 / Q24, as FM TONE is), so the Mac and the FM-1 render the same samples.
- **The patch grows from 74 to 109 values**, in FIZZ's own **blob version 6** (128 bytes).
  - Version 1..5 blobs (96 / 80 bytes, ChoralRoot's) still load. Their new values take the defaults, which are all off.
  - The pattern's mix lock ids move up by 33, renumbered when an older pattern record loads.
  - SysEx gets a STORE for the 128-byte blob. The 96-byte STORE stays.
- **Recommendations:**
  - Bit / sample-rate reduction as a **per-sound insert** (CRUSH), as the Digitone II has it, rather than a global send. Both are laid out in §5.
  - The modulation as **eight free slots** (source, destination, depth), rather than the Digitone's fixed four per source.
  - Four milestones:
    1. **M1** the plumbing, AHD and portamento;
    2. **M2** the filter machines;
    3. **M3** CRUSH;
    4. **M4** the modulation slots.
- **Cost on the FM-1:**
  - All of it on, on every voice: about +30 µs a voice per 2.9 ms half, about +17 % on a WOBBLE-class voice.
  - Each feature costs nothing when it is off.
  - With `quad_render` in RAM, the worst half goes from 67 % to about 76 %.
  - COMB needs 2 KB of RAM per voice (32 KB for 16), which fits PLAN §7's 66 KB spare.

## 1. What the Digitone II does, feature by feature

### 1.1 The amp envelope: AHD and ADSR

- TR §13.6: the amp envelope has two modes, AHD and ADSR, and "AHD can have fixed or note-controlled hold timing".
- FMT §4, the stored values:
  - **AMP MODE**: id 97, 0 = AHD, 1 = ADSR (the default).
  - **HOLD**: id 88, 0..126, and **127 = NOTE**: held while the key is down.
  - ATK, DEC, SUS, REL as before.
- **Inferred.** AHD is attack, then hold, then decay to silence. SUS and REL do not apply.
  - A timed HOLD runs its time whatever the gate does: one-shot, percussive.
  - HOLD = NOTE holds the peak until the key is released, then decays on DEC. That makes DEC the release, an organ-like gate.
  - The hold's time law is not known. Measure it on a Digitone II (§11, question 7).

### 1.2 The filter machines

TR §13.1 lists six machines. FMT §4 gives their stored fields (ids 73..75 depend on the machine):

| machine | id 73 | id 74 | id 75 | what it is |
| --- | --- | --- | --- | --- |
| MULTI-MODE | TYPE (0..127) | FREQ | RESO | "Morphable low-pass, band-pass, and high-pass" |
| LOWPASS 4 | — | FREQ | RESO | 24 dB/oct low-pass (FM TONE's LP24) |
| LEGACY LP/HP | TYPE (LP, HP, OFF) | FREQ | RESO | 12 dB/oct, FM TONE's LP12 / HP12 / OFF |
| COMB− | LPF | FREQ | FDBK | "Negative-feedback comb filter" |
| COMB+ | LPF | FREQ | FDBK | "Positive-feedback comb filter" |
| EQUALIZER | Q | FREQ | GAIN (−64..63) | "Parametric EQ" |

- **All six** share the filter envelope (ATK DEC SUS REL, ENV depth, DELAY), KEY TRACK and the base-width filter (TR §13.2).
- **Routing.** BW ROUTING (id 105) puts base-width before or after the machine. TR §13.3 / OD ROUTING (id 106) puts overdrive before or after the filter.
- **Inferred:**
  - MULTI-MODE's TYPE morphs LP (0) → BP (64) → HP (127). One real file has TYPE 127 on a bass (FMT §7), so this needs a check.
  - The combs' FREQ is the comb's pitch, so a delay of 1 / FREQ.
  - FDBK is the feedback gain, and LPF a low-pass in the feedback loop that damps the high teeth.
  - COMB+ feeds back in phase: every harmonic, string-like.
  - COMB− feeds back inverted: odd harmonics, a hollow tube or clarinet.

### 1.3 Bit and sample-rate reduction

- TR §13.4: bit reduction is "from 16 down to 1 bit".
- TR §13.5: sample-rate reduction "can be placed before or after the selected filter machine".
- FMT §4, stored per sound:
  - **BR**: id 101, OFF and then a firmware table of 127 steps;
  - **SRR**: id 102, 0..127;
  - **SRR ROUTING**: id 103, pre or post filter.
- The Digitone II has them **per track, in the voice**, beside OVERDRIVE (id 104). They are not send effects.

### 1.4 Portamento

M1 §9.5.5 and §11.2.7–8; on the Digitone II, ids 99 and 100:

- PORT (on / off) and PTIM (1..127) are trig-page values, lockable per step.
- The SOUND SETUP menu adds:
  - **TYPE**: TRACK (from the track's last note), VOICE (from that voice's last note), or LEGATO ONLY (only from a note still held);
  - **SLOPE**: constant rate or constant time;
  - **AMOUNT**: a partial glide, "with polyphonic portamento on chords, this results in a shorter and less messy glide";
  - **STYLE**: glide or glissando, in semitones;
  - **GATING**: whether the glide stops at the key-up.

The Digitone II's stored sound has PORT and PTIM. The menu settings are probably in the record's unread bytes 324..330 (FMT §3).

### 1.5 The modulation slots

- M1 §9.5.11–15, the Digitone 1: PITCH BEND, VELOCITY MOD, MOD WHEEL (CC 1), BREATH (CC 2) and AFTERTOUCH each "assign up to four PARAMETER page parameters". The depth is "an offset of the original track parameter value".
- That is **5 sources × 4 slots = 20 records**: DNX's twenty 3-byte records at DN1 190 / DN2 252.
- TR §15.2 adds **key tracking** as a source on the Digitone II. Its record has room for **24** records (252..323): **6 × 4**.
  - *Inferred:* 312..323 hold the six-source set's four new records. DN1 conversions leave them zero.
- Each record is 3 bytes: two bytes of depth (*inferred*: they vary like a coarse / fine pair) and a destination parameter id. The ids are the same as the LFOs' destinations (FMT §4).

## 2. What FIZZ has now

**The voice** (`eng_quad.c` `quad_render`, l. 1133–1497, ChoralRoot's line for line), in order:

1. Four operators in eight algorithms.
2. The X/Y MIX.
3. A DC blocker.
4. Base-width: a one-pole high-pass and a one-pole low-pass.
5. The multimode SVF (`QP_FTYPE` OFF / LP12 / HP12 / LP24), with its ADSR, depth, delay and key track.
6. The amp ADSR × LEVEL².

DRIVE (`QD_QP_DRIVE`, value 73) is ChoralRoot's DIST. It runs per voice **after** the voice, in the mix (`mix.c` → `qd_quad_dist`).

**The modulation** is three LFOs per voice at control rate (every 32 samples), summed into `dst[QD_*]`. That is ChoralRoot's 58 destinations, including PAN, DRIVE and the three sends through the owner's offsets. Velocity reaches:

- the operator levels (`QP_VEL`);
- the amplitude (fixed, half);
- the A, B1 and B2 key scaling.

Nothing else is a source.

**The pool** (`pool.c`) has 16 shared voices, a per-track limit (1 = mono choke) and reserve, and fade steals.

- `qd_pool_legato` moves a sounding voice to a new note: TIE on a MONO track.
- `qd_quad_voice_t` has `pitch16` and `pitch_cur`, but **no glide**: `quad_voice.c` l. 107 says "no glide: QUADRILLE has none yet".
- ChoralRoot's `voice.c` has one (`glide_set`, l. 571–586, and l. 1035–1040).

**The effects** (`qd_fx.c`) are ChoralRoot's chorus, delay and reverb on three mono sends, then the master filter and Output Clean.

- The P1.5b routing adds DELAY > REVERB, CHORUS > DELAY, CHORUS > REVERB, and the master filter before or after.
- The FX pages are B1 OVERVIEW, B2 DELAY, B3 REVERB, B4 CHORUS, B5 MASTER, B6 ROUTING (`ctl_mix.c` l. 20–32).

**The editor** (`ctl_snd.c` l. 21–56): 18 pages in eight groups, each value on exactly one page.

| key | group | pages |
| --- | --- | --- |
| B1 | TRIG / KEYS | TRIG, KEYS |
| B2 | OPERATORS | RATIO, TONE, OFFSET |
| B3 | OP ENVELOPES | ENV A, ENV B |
| B4 | FILTERS | FILTER, F ENV, BASE |
| B5 | ENVELOPES | AMP, LEVEL |
| B6..B8 | LFO 1..3 | page, WAVE |

- KNOB 1..4 are the page's values, and ALGORITHM a fifth where the page has one. FILTER, BASE, AMP and LEVEL have no fifth today.
- P1.5k's hold-to-map: hold an LFO key and turn a value's knob to make it the destination and set the depth. The owner wants it kept unchanged.

## 3. The patch: values 74..108, blob version 6

The new values are appended after DRIVE (73). Each one's default is the value at which today's sound comes out
unchanged. Every new value is a patch value, so it is lockable per step like the rest. Lock ids are patch indexes.

| id | name | range | default | lockable | LFO / MOD destination | milestone |
| --- | --- | --- | --- | --- | --- | --- |
| 74 | AMOD (amp mode) | 0 ADSR, 1 AHD | **0** | yes (a routing change: the dip) | no | M1 |
| 75 | HOLD | 0..126, 127 NOTE | 127 | yes | yes (new destination) | M1 |
| 76 | PORT | 0 OFF, 1 ON, 2 LEGATO | **0** | yes (a step's slide) | no | M1 |
| 77 | PTIM | 0..127 | 32 | yes | yes (new) | M1 |
| 78 | PSLOPE | 0 TIME, 1 RATE | 0 | yes | no | M1 |
| 79 | FSHAPE | 0..127 | 0 | yes | yes (new) | M2 |
| 80 | FGAIN | −64..63 | 0 | yes | yes (new) | M2 |
| 81 | BWPOST | 0 base-width before the filter, 1 after | **0** | yes (dip) | no | M2 |
| 82 | BITS (bit reduction) | 0 OFF, 1..127 | **0** | yes | yes (new) | M3 |
| 83 | SRR | 0 OFF, 1..127 | **0** | yes | yes (new) | M3 |
| 84 | CRPOST | 0 CRUSH before the filter, 1 after | 1 | yes (dip) | no | M3 |
| 85..108 | MOD 1..8: slot k is SRC 82 + 3k, DST 83 + 3k, DEPTH 84 + 3k (§6) | SRC 0 NONE, 1..8; DST as the LFOs'; DEPTH −127..127 | **SRC 0** | yes | no (a slot does not modulate a slot) | M4 |

- That is **109** values (0..108): slot 1 is 85..87, slot 8 is 106..108.
- **The filter machine is `QP_FTYPE`, extended**, not a new value. Today it is 0 OFF, 1 LP12, 2 HP12, 3 LP24; it gains 4 MULTI, 5 COMB+, 6 COMB−, 7 EQ.
  - An old patch is one of 0..3, the same as before.
  - A p-lock of the machine is one value, and a change already takes the dip (`pool.h`: "the filter TYPE").
  - The editor shows it as six machines (§4.4).
- **FSHAPE** is the machine's own third control, as the Digitone's id 73 is: MULTI's TYPE morph, the combs' LPF, the EQ's Q. RESO is the combs' FDBK. FGAIN is the EQ's gain.
- **The LFO destinations grow** after ChoralRoot's 58 (QD 0..57): QD 58 HOLD, 59 PTIM, 60 FSHAPE, 61 FGAIN, 62 BITS, 63 SRR.
  - `N_QUAD_DEST` / `QUAD_DEST_ORDER` gain them, marked `QUADRILLE (e)`.
  - A sound with any of these is FIZZ-only (§10).

**Blob version 6** (`'Q'` 6, **128 bytes**):

- version 5's layout up to byte 86;
- DRIVE at 87, as FIZZ already packs it;
- then values 74..108 one byte each (value − min);
- no new hi-res values: PTIM and the depths are whole steps;
- 88 + 35 = 123 bytes used, then zeros.

The places that change:

- `qd_quad_unpack` reads versions 1..5 as today, with the new values at their defaults, and 6 directly.
- `qd_quad_pack` always writes 6.
- **The persist records:**
  - 'P' 3 and 'S' 5 carry 128-byte blobs; 'P' 2 / 'S' 4 and older are read with 96.
  - The library and pool flash grow by 32 bytes a sound: 256 MY presets + 128 pool = 12 KB.
- **The lock ids:**
  - `QD_LK_PAN..QD_LK_LEVEL` follow the patch, so they move from 74..78 to 109..113.
  - A 'P' 2 record's locks with id ≥ 74 are renumbered (+35) as it loads.
  - `qd_lock_t.id` is a `uint8_t`, so there is room.
- **SysEx:**
  - STORE (0x10, a 96-byte blob packed to 110) stays, and fills in the defaults.
  - A new **STORE6** (0x11) carries the 128-byte blob (147 packed, a 169-byte body). That is under the 640-byte frame limit.
  - `docs/SYSEX.md` and `tools/fizz_sysex.mjs` gain it, and `dn2pstToQuad` returns a v6 blob.
- **Memory:**
  - A patch goes from 146 to 218 bytes (109 × 2).
  - 16 voices + 81 unpacked sounds + 64 pending trigs: about +12 KB of POOL.
  - Together with COMB's 32 KB (§4.3): 44 KB of the 66 KB spare (PLAN §7). See §8.

## 4. The DSP design, feature by feature

These rules hold throughout:

- All arithmetic is integer, as the voice is.
- The new state lives in `quad_voice_t`. Its `qstate` is 256 bytes and about 124 are used, so about 40 more fit.
- Each hook tests its feature first. Off, `quad_render` executes exactly today's code, so the bit-exact tests are untouched.

### 4.1 AHD (M1)

- **Where:** in `quad_env_tick` (l. 910) for envelope 3, the amp, when AMOD = 1.
- **The stages:**
  1. ATK, the amp attack table, unchanged.
  2. A new stage, HOLD:
     - for HOLD 0..126: a tick counter against a hold-time table;
     - for 127 (NOTE): held until the gate falls.
  3. DEC: exponential to **0** on the amp decay table, then the voice ends (stage 0).
- SUS and REL are not read. A gate falling during a timed HOLD changes nothing, which is what makes it one-shot.
- **The hold time table:** until it is measured, it is the amp attack table's time for the same value, so HOLD 40 lasts as long as ATK 40 takes. HOLD's control is drawn as a time, like ATK.
- **The state:** one `uint32_t` hold counter.
- **`quad_done` and `quad_endless`** work as they are. With HOLD = NOTE, `endless` is true while the gate is down; that is AMP REL INF's case today.
- **Tests:** `test_quad` gains these cases:
  - the AHD envelope's timing against the tables;
  - NOTE hold with the gate up and down;
  - a p-lock switching ADSR to AHD mid-note, which takes the dip;
  - `quad_done` at the decay's end.

### 4.2 Portamento (M1)

- **Where:** in `quad_voice.c`. It glides `pitch_cur` toward `pitch16` once per control block, before `quad_render` reads `m.pitch16 = v->pitch_cur` (l. 143). This is ChoralRoot's `voice.c` law (l. 571–586, 1035–1040), so FIZZ and ChoralRoot glide alike:
  - **TIME**: constant time. The glide takes `1 + PTIM² / 8` blocks whatever the interval: PTIM 32 is 129 blocks, about 94 ms; PTIM 127 is about 1.5 s.
  - **RATE**: a fixed speed in 1/16 semitones a block.
- **Where a glide starts:**
  - **A mono track (limit 1), and TIE:** the voice moves, through `qd_pool_legato` or a fresh voice, from the pitch it had. This covers the 303-style slide: lock PORT ON on the step to slide into.
  - **A poly track, one note:** from the **track's last note**, the Digitone's TRACK type. The owner keeps `last_pitch` (set at every note-on), and a fresh voice starts at it.
  - **A poly track, a chord after a chord:** each new note glides from the **previous chord's note of the same rank** (lowest from lowest, and so on). Extra notes start from the nearest one. This is the Digitone's "less messy" goal without its AMOUNT setting.
  - The pool already knows a trig's notes. The owner keeps the last chord's pitches (8 bytes).
- **LEGATO (2):** it glides only when the track still has a voice in gate at the new note's start: an overlapping LEN, or a TIE. Otherwise it jumps.
- **The pool's choice of voice is unchanged.** The glide is a start pitch, not a voice choice, so steals and reserves behave as today.
- **The state:** `gstep` in `qd_quad_voice_t` (4 bytes), and `last_pitch` plus the last chord in the owner.
- **Tests:**
  - in `test_pool`: a glide's start, its end and its time; a chord pairing; LEGATO with and without an overlap; TIE on mono;
  - in `test_seq`: a step locking PORT ON slides;
  - nothing changes with PORT = 0.

### 4.3 The filter machines (M2)

These are hook 2. FTYPE 0..3 runs the SVF exactly as today; 4..7 go to `quad_ext.c`.

- **MULTI (4), LP → BP → HP:**
  - The trapezoidal SVF already makes `v2` (LP), `v1` (BP) and `x − k·v1 − v2` (HP) in one pass.
  - The output crossfades them by FSHAPE: 0..64 is LP→BP, 64..127 is BP→HP, with Q15 weights computed once a block.
  - BP is scaled by k so that it peaks at unity.
  - The cost is LP12's plus two multiplies a sample. The state is the SVF's (`f1`, `f2`).
- **LOWPASS 4 / LEGACY** are FTYPE 3 / 1 / 2 / 0, as today.
- **COMB+ (5), COMB− (6):**
  - The delay line is `int16`, **1024 samples** a voice: the lowest comb pitch is 43 Hz, and a lower FREQ clamps there.
  - The lines live in a pool arena, `int16 comb[16][1024]`, 32 KB, indexed by the voice's pool index.
  - The delay length is `44100 / f`, from FREQ plus ENV, key track and the LFOs, through a FREQ → Hz table (the SVF's own law), with a Q16 fraction.
  - The read is linearly interpolated. The length is per block, its change ramped over the block.
  - In the loop: `y = x ± g·lp(d[n − L])`, where `g` comes from RESO (0..127 → 0..0.985, kept below 1 so it is stable), and `lp` is a one-pole from FSHAPE (127 = open).
  - The output is scaled by (1 − g/2), as a gain law, then the existing soft knee.
  - The state: the write index (`uint16_t`), the one-pole (`int32_t`), and the last length (`int32_t`).
  - **A fresh voice clears only the samples it will read.** The first block reads 0s, because the write index starts at the line's head and the line is zeroed when the voice is taken.
- **EQ (7):**
  - A peaking bell built from the same SVF: `y = x + G·k·v1`. FREQ is the centre and FSHAPE the Q, with k = 1/Q from a 128-entry table.
  - FGAIN −64..63 maps to −18..+18 dB through a 128-entry Q12 table, so G = 10^(dB/20) − 1.
  - The cost is LP12's.
- **BWPOST (1):** base-width runs after the machine instead of before it. The two loops swap order, at no cost.
- **Switching machines** goes through the dip (a routing change), so there is no click. The machine's states are cleared at the dip.
- **Tests:**
  - in `test_quad`:
    - MULTI at 0 matches LP12 within 1 LSB, and at 127 matches HP12;
    - the comb's pitch: autocorrelation on noise is within 1 % of the asked frequency at 110, 440 and 1760 Hz;
    - COMB− has odd harmonics only;
    - the EQ's gain at f0 is within 0.3 dB of its table;
    - no NaN or overflow under `-fsanitize`;
  - FTYPE 0..3 sounds are unchanged in `test_quad_bitexact`.

### 4.4 CRUSH, bit and sample-rate reduction (M3)

This is hook 3, per voice: CRPOST = 1 puts it after the filter, 0 before. Both BITS and SRR at 0 skip it entirely.

- **SRR:**
  - A phase accumulator steps at `1 / N` of the sample rate, where N is 1..64 from SRR on a log table.
  - The held sample is replaced when the phase wraps. That is a sample-and-hold, aliasing on purpose, as the Digitone's does.
  - The state is the phase (`uint32_t`) and the held value (`int32_t`).
- **BITS:**
  - The sample is quantized to `b` bits of the voice's ±65535 range by a mask with a rounding offset. `b` runs from 16 down to 1, from a 128-entry table like the Digitone's (FMT: "OFF, then a table").
  - The state is none.
- **The cost** is about 6 operations a sample.
- **Tests:**
  - a sine through SRR N gives steps N samples long;
  - BITS b gives at most 2^b levels;
  - both at 0 leave the block bit-identical.

### 4.5 The modulation slots (M4)

This is hook 4, at control rate, in the LFO block (l. 1149–1235). It runs after the LFOs fill `dst[]`, and adds the slots
to the same array. **Every LFO destination is a slot destination**, and the existing destination code applies them,
PAN, DRIVE and the sends among them. `dst[d] += src(k) × DEPTH / 127` in Q15. A slot with SRC 0 is skipped.

**Sources, all per voice:**

| # | source | Q15 value | from |
| --- | --- | --- | --- |
| 1 | VELOCITY | vel / 127, unipolar | the trig, or a played note |
| 2 | KEY | (note − 60) / 64, bipolar | the voice's pitch |
| 3 | MOD WHEEL | CC 1 / 127 | the track's MIDI state |
| 4 | AFTERTOUCH | channel pressure / 127 | the track |
| 5 | PITCH BEND | bend / 8192, bipolar | the track |
| 6 | BREATH | CC 2 / 127 | the track |
| 7 | RANDOM | a value per note-on | `quad_xs`, seeded by the pool index and the note serial, so deterministic |
| 8 | F ENV | the filter envelope's level, unipolar | the voice |

- Sources 1–6 are the Digitone II's six. RANDOM and F ENV are FIZZ's, both nearly free.
- The track's MIDI state (CC 1, CC 2, pressure, bend) is 8 bytes in `qd_quad_owner_t`. It is set from:
  - `midi_in` on the track's channel, or the selected track (FM1VST and the web page forward the host's MIDI);
  - on the FM-1, its own controls, if it has any (§11, question 3).
- **The state:** none per voice, beyond the RANDOM value (2 bytes). The cost is 8 slots × about 4 operations a block, which is nothing.
- **Tests:**
  - each source moves its destination by its depth: a VEL → FREQ slot at velocities 20 and 127 against a FREQ lock;
  - KEY across octaves;
  - a CC 1 ramp, through `midi_in`;
  - an empty matrix leaves the render identical.

## 5. "A 4th effect": two ways, and the recommendation

| | **A. a global send effect, CRUSH** | **B. a per-sound insert (recommended)** |
| --- | --- | --- |
| where | `qd_fx_block` gets a 4th mono send `sx`. One crusher runs on it, its return into the mix, before the master filter. | In every voice, before or after its filter (§4.4), as on the Digitone II |
| values | FX parameters, global: BITS, RATE, a tone low-pass, the return level. Each track gets a 4th send, CRUSH. A voice gets a CRUSH mix lock. | Sound values BITS, SRR, CRPOST (74..), lockable and modulatable per step |
| FX pages | B7 CRUSH (BITS, RATE, TONE, SEND). OVERVIEW grows to four columns. | No change. CRUSH is in the sound. |
| ROUTING | gains CRUSH > DELAY and CRUSH > REVERB, so the crushed return can feed them | No change |
| mixer | the sends page gets a 4th bar (KNOB 4 CRUSH). The track's mix in the HDR record gains a byte. | No change |
| editor | No change | B5 group: a CRUSH page (§7) |
| import | Not faithful. Two imported sounds with different BITS would share one crusher; at best the importer sets the track's send. | 1:1 (§9) |
| cost on the FM-1 | one instance, about 10 µs a half, flat | about 6–8 µs a half per voice that uses it, nothing otherwise |
| bit-exact | `test_fx_bitexact` must skip the 4th bus when every send is 0 | untouched |

- **Recommendation: B.** It is what the Digitone II does: the crush is part of the sound, and it sits before or after the filter, which a send cannot do.
  - It imports exactly, and it can be p-locked or modulated per step, for example a glitchy SRR lock on one step.
  - A crusher is an insert sound: on a send, the dry signal under it masks most of the lo-fi character.
  - On the FX pages it can still be presented as the 4th effect: OVERVIEW's foot could show which tracks' sounds crush. That is optional.
- **If the owner meant a shared effect** (one crusher that every track can send into, like the delay), A is cheap and the two can coexist later. A is about 150 lines in `qd_fx.c` plus the pages above.

## 6. The modulation slots: the Digitone II, FIZZ today, the design

- **The Digitone II:** six sources (VELOCITY, PITCH BEND, MOD WHEEL, BREATH, AFTERTOUCH, KEY TRACK), each with **four fixed destination / depth pairs**. That is 24 records, most of them empty in any one preset.
- **FIZZ today:** three LFOs with one destination each. Velocity drives the operator levels (VEL) and the amplitude. Key scaling drives A, B1 and B2.
- **The design: eight free slots** of SOURCE, DESTINATION, DEPTH (−127..127), values 85..108.
  - **Why not 6 × 4:**
    - Eight slots are 24 values, against 48 for the Digitone's layout.
    - Any source can take as many slots as it needs: four on velocity is fine.
    - The editor shows one list instead of six menus.
  - **The import** packs a Digitone II preset's non-empty records into the slots in order. Over eight is noted ("MOD: 3 slots not imported"); presets rarely use more than four.
- **On the editor** (§7): a MOD page lists the eight slots as rows: a source icon → the destination's name, and the depth as a bar from the centre.
  - ALGORITHM chooses the slot.
  - KNOB 1 is SOURCE, KNOB 2 DESTINATION (the LFOs' list and order), KNOB 3 DEPTH.
  - KNOB 4 is the chosen slot's depth's fine twin. *Alternative:* empty.
  - The page's lock table follows the chosen slot. `qd_snd_lock_screen` is rebuilt when ALGORITHM moves.
- **Hold-to-map, extended** (P1.5k's gesture, unchanged for the LFOs): **hold B1** (the group key, now PLAY / MOD) and turn any value's knob on any page.
  - The first detent takes the **next empty slot** and gives it the MOD page's chosen source (VELOCITY until another is chosen) and that destination. Turning sets the depth.
  - If a slot already has that source and destination, it turns that slot's depth.
  - While B1 is held, **W1..W8 choose the source**: VEL, KEY, MW, AT, PB, BR, RAND, F ENV, drawn as their icons on the keys' places. The white keys are the steps otherwise, so this is a layer, like the LFO keys'.
  - A value that cannot be a destination is refused with P1.5k's shake. A tap of B1 without a turn goes to its next page, as today.
- **Marks:** a modulated value shows a small source glyph by its label, next to the LFO's numbered dot. The glyph is a shape, never under 12 px.
- **Velocity:** `QP_VEL`, the operators' velocity, stays. A VEL slot is the general route; the Digitone keeps both too.

## 7. The sound editor

The changes to `QD_SND_PAGES` and `PLAYING.md`'s table:

| key | group | pages (new in **bold**) |
| --- | --- | --- |
| **B1** | **PLAY / MOD** (was TRIG / KEYS) | TRIG, KEYS, **GLIDE**, **MOD** |
| B2 | OPERATORS | RATIO, TONE, OFFSET |
| B3 | OP ENVELOPES | ENV A, ENV B |
| B4 | FILTERS | FILTER (**MACHINE on ALGORITHM**), F ENV, BASE (**+ BWPOST**) |
| B5 | **AMP / FX** (was ENVELOPES) | AMP (**ADSR / AHD on ALGORITHM**), LEVEL, **CRUSH** |
| B6..B8 | LFO 1..3 | page, WAVE (DEST's list +6 new destinations) |

The pages, each with one big picture (the clarity rules: no hint text, labels 12 px or more, the value big):

- **GLIDE** (B1): KNOB 1 PORT (OFF / ON / LEGATO), KNOB 2 TIME, KNOB 3 SLOPE.
  - The picture: two note bars, the second higher, joined by a curve as long as TIME.
  - OFF draws a hard step.
  - LEGATO draws the bars overlapping, and the curve only where they overlap.
- **MOD** (B1): §6. Eight rows, the chosen one big. Source icons:
  - VEL: a wedge;
  - KEY: a key;
  - MW: a wheel;
  - AT: a finger pressing;
  - PB: a bent arrow;
  - BR: a breath puff;
  - RAND: a die;
  - F ENV: a small ADSR.
- **FILTER** (B4):
  - **ALGORITHM is the MACHINE**, its name in the pink badge: LEGACY, LOWPASS 4, MULTI, COMB+, COMB−, EQ. LEGACY covers FTYPE 0..2 and LOWPASS 4 is FTYPE 3.
  - KNOB 1 FREQ and KNOB 4 DEPTH, as today. KNOB 2 and KNOB 3 change with the machine:

    | machine | KNOB 2 | KNOB 3 |
    | --- | --- | --- |
    | LEGACY | RESO | TYPE: LP / HP / OFF, written to FTYPE 0..2 |
    | LOWPASS 4 | RESO | — |
    | MULTI | RESO | TYPE morph, written to FSHAPE |
    | COMB+ / COMB− | FDBK (RESO) | LPF (FSHAPE) |
    | EQ | GAIN (FGAIN) | Q (FSHAPE) |

  - The picture is the response, already drawn as a curve (`UI_SD_FILTER`):
    - MULTI: the curve morphs LP → BP → HP;
    - LOWPASS 4: a steeper skirt;
    - COMB+: evenly spaced teeth at FREQ's harmonics, as tall as FDBK, the high ones bowed down by LPF;
    - COMB−: the teeth at the odd harmonics only;
    - EQ: one bell or dip at FREQ, as wide as Q, as tall as GAIN.
- **BASE** (B4): BASE, WIDTH, KTRK, and KNOB 4 **BWPOST** (BEFORE / AFTER). The window is drawn on the left or the right of a small filter icon.
- **AMP** (B5):
  - **ALGORITHM is MODE** (ADSR / AHD).
  - In AHD: KNOB 1 ATK, KNOB 2 **HOLD**, KNOB 3 DEC. KNOB 4 is dim.
  - The picture in AHD: the attack, a flat top as long as HOLD, then the decay to 0. With HOLD at NOTE, the flat top sits over a note bar and ends where the bar ends.
- **CRUSH** (B5): KNOB 1 BITS, KNOB 2 SRR, KNOB 3 BEFORE / AFTER.
  - The picture: one sine cycle drawn as stairs. The step width follows SRR and the step height BITS; both OFF draws a smooth sine.
  - A small FILTER→CRUSH or CRUSH→FILTER chain sits under it.

Every value stays on exactly one page, and the tests that find pages by name (`qd_snd_page_of`) gain the new names.

## 8. Cost

**On the Mac, measured** (2026-10-22, a scratch bench of `quad_voice.c` at -O2: one voice, a held note, 20,000 blocks,
the best of five):

| sound | ns a block (32 samples) |
| --- | --- |
| INIT | 205 |
| FEEDBACK | 195 |
| EP | 215 |
| BASS | 298 |
| STRINGS | 290 |
| WOBBLE | 319 |
| NOISE-ISH | 344 |
| DRIVE (`qd_quad_dist`), alone | 142 |

A block is 726 µs of audio, so 16 WOBBLE voices take under 1 % of a core. **The Mac is no constraint.** The new stages
would add about 0–60 ns a block each.

**On the FM-1, Phase 2.** These are estimates scaled from PLAN §3's measurements: a WOBBLE voice is 176 µs per 2.9 ms
half, and DIST is 32 µs. They are µs per half, per voice, with the feature on.

| feature | per voice | how it scales | note |
| --- | --- | --- | --- |
| AHD | ~0 | one envelope tick a block, as today | |
| portamento | ~0 | one step of `pitch_cur` a block | |
| MULTI | +3 over LP12 | two MACs a sample | |
| EQ | ≈ LP12 (~15) | instead of the SVF, not on top | |
| COMB± | ~18 | a read, interpolate, one-pole, write | **plus 2 KB RAM a voice** |
| CRUSH | 6–8 | a phase add and a mask a sample | |
| MOD slots | ~1 | eight multiply-adds a block | |
| **all on, worst** (COMB + CRUSH + 8 slots) | **~+28 (+16 %)** | | |

**Against PLAN §3's worst half**, with eight WOBBLE-class voices a core and every voice using COMB and CRUSH:

| | today | with every feature on every voice |
| --- | --- | --- |
| `quad_render` in flash | 2318 µs (80 %) | ~2540 µs (88 %) |
| `quad_render` in RAM | 1950 µs (67 %) | ~2170 µs (75 %) |
| one core, 8 voices | 86 % | about 94 % |

**To trim for the hardware:**

- **The allocator's units.** PLAN §3 question 5 already asks whether a voice should be weighed by its patch. A voice with COMB or CRUSH on should count **3 units** instead of 2, so the pool steals before the shed has to. In practice that is a cap of about 11 comb/crush voices.
- **COMB's RAM.** The 1024-sample lines are 32 KB for 16 voices. If the POOL is tight, there are two fallbacks:
  - 512-sample lines: 16 KB, with the lowest comb pitch 86 Hz;
  - a comb arena shared by 8 voices: the 9th comb voice falls back to LP12, which is noted.
- **The patch's growth.** About 12 KB more of POOL (§3). Together with COMB that is 44 of PLAN §7's 66 KB spare, which leaves 22 KB.
- **RAMTEXT.** `quad_ext.c`'s loops are about 3–4 KB, inside RAMTEXT's 24 KB budget with `quad_render`'s 6..16 KB. COMB and CRUSH are the hot ones.

## 9. Import: the new values from Digitone presets

| FIZZ value | from a Digitone II (`.dn2pst`, FMT offsets) | from a Digitone 1 (`.dnsnd`, `fm1dnsnd.js` offsets) |
| --- | --- | --- |
| AMOD | +222: 0 AHD → 1, 1 ADSR → 0 | 0 (the DN1 has ADSR only) |
| HOLD | +204, 1:1 (127 NOTE) | 127 |
| PORT | +226 (64 = ON → 1; the TYPE menu, if it is in +324..330, → 2 for LEGATO ONLY) | +170 (`unk170`), the same law |
| PTIM | +228, 1:1, *the time law to verify* | +172 (`unk172`) |
| FTYPE (machine) | +245 machine, +174 type: MULTI → 4, LOWPASS 4 → 3, LEGACY → 1 / 2 / 0, COMB+ → 5, COMB− → 6, EQ → 7 | +128 (v5: 0 OFF, 1 LP2, 2 HP2, 3 LP4), as today |
| FSHAPE | +174: MULTI's TYPE, the combs' LPF, EQ's Q | 0 |
| RESO / FGAIN | +178: RESO (MULTI, LEGACY, LP4), FDBK → RESO (combs), GAIN → FGAIN (EQ, coarse − 64) | RESO, as today |
| BWPOST | +238 (BW ROUTING) | 0 |
| BITS, SRR | +230, +232 (BITS through the firmware's table: OFF, 16.0 bits … ) | 0 |
| CRPOST | +234 (SRR ROUTING) | 1 |
| MOD 1..8 | +252 + 3k, k = 0..23: each record with a destination id ≠ 0 → the next slot. The source is k / 4 in the record's order (PB, VEL, MW, BR, AT, KEY: *inferred from the DN1 menu order*). The destination comes through `DN2_LFO_DEST`. The depth is the record's first two bytes (*law to verify*). | +190 + 3k, k = 0..19, five sources × 4, the same way. The destination is DN1's number → id (DNX's selector table) → FM TONE's. |
| DRIVE (73) | +236, as now | +156, as now |

- With these, the importer's notes for AHD, the filter machines, BR / SRR, portamento and the modulation go away.
- What remains approximated or dropped:
  - the arpeggiator;
  - OD ROUTING (FIZZ's DRIVE stays after the voice, §10);
  - PORT's AMOUNT / STYLE / GATING settings;
  - MULTI's TYPE law, until it is verified.
- The importer needs the record's modulation and portamento layout pinned first. The diagnostic saves in FMT §8 gain:
  - one preset with a VEL → FREQ depth of +32, and one with −32;
  - one with MOD WHEEL → HARM;
  - one with PORT on, PTIM 64 and TYPE LEGATO;
  - a sine through BR at 8 bits, and one with SRR at 16.

## 10. What stays ChoralRoot's, and what does not

- **The sound engine.** FIZZ's `eng_quad.c` is ChoralRoot's line for line. These features go in `quad_ext.c` behind hooks; `eng_quad.c` changes only at the four marked call sites and in the destination list. So:
  - `test_quad_bitexact` keeps comparing FIZZ's FM TONE with ChoralRoot v0.25.1's on all 17 factory sounds and their scenarios;
  - a new `test_quad_ext` covers the new paths.
- **A FIZZ sound that uses a new value is FIZZ-only.**
  - ChoralRoot's `quad_blob_ok` refuses version 6, and FTYPE > 3.
  - FM1VST's drop of a ChoralRoot sound is unaffected.
  - FIZZ exporting to ChoralRoot would need a v6 → v5 step, like `quadToV3`: the new values dropped, with notes.
- **Whether ChoralRoot should gain these too** is the owner's call (question 5). The `quad_ext.c` split makes a later port to ChoralRoot one file and four hook lines.
- **Not in this plan:**
  - the Digitone II's **overdrive routing**: DRIVE before the filter;
  - **VELOCITY TO VOL** curves;
  - **MONO NOTE PRIO**.

  DRIVE before the filter would move DIST into the voice (PLAN §3: +32 µs a driven voice). It could join M3 as one more value if the owner wants it.

## 11. Milestones

Each one ships with its tests, the bit-exact tests unchanged, the screens rendered and the guide updated.

1. **M1: the plumbing, AHD and portamento.**
   - Blob v6 with migration from v1..v5.
   - The persist records 'P' 3 / 'S' 5, and the lock-id renumbering of 'P' 2.
   - SysEx STORE6, with `fizz_sysex.mjs` and `dn2pst.mjs` returning v6.
   - `quad_ext.c` with the hooks.
   - AHD (AMOD, HOLD) and its AMP page; GLIDE (PORT, PTIM, PSLOPE), its page, the pool's chord pairing and LEGATO.
   - The import of AHD / HOLD / PORT / PTIM.
   - Tests: `test_quad_ext` AHD, `test_pool` glide, `test_persist` v5 → v6 and the lock ids, `test_sysex` STORE6, `test_dn2pst` BASSSINE.
2. **M2: the filter machines.**
   - FTYPE 4..7, FSHAPE, FGAIN, BWPOST, the comb arena.
   - The FILTER page's MACHINE, the per-machine knobs and drawings; BASE's BWPOST.
   - The import of every filter machine.
   - Tests: MULTI against LP12 / HP12, the comb's pitch and its odd harmonics, the EQ's gain, the dip when the machine changes, RAM within budget.
3. **M3: CRUSH.**
   - BITS, SRR, CRPOST and the CRUSH page.
   - The import of BR / SRR / SRR ROUTING.
   - (Option A, the global send, only if the owner chooses it.)
   - Tests: steps and levels, off is bit-identical, the cost on the bench.
4. **M4: the modulation slots.**
   - Eight slots and the MOD page; the B1 hold-to-map with W1..W8 as sources; the glyph marks.
   - The track's MIDI state from `midi_in`: CC 1, CC 2, pressure, bend.
   - The six new LFO destinations.
   - The import of the modulation records.
   - Tests: each source against its destination; hold-to-map's slot choice, refusals and locks; an empty matrix is identical.

The order follows dependencies. M1 carries the format change every later milestone needs, and its features are small.
M2 is the biggest DSP job. M3 is small. M4 changes the editor's gestures most, so it comes last, when the owner has
played M1–M3.

## 12. Questions for the owner

1. **The 4th effect:** a per-sound CRUSH, as the Digitone II has (recommended, §5 B), or a shared send effect beside chorus, delay and reverb (§5 A), or both?
2. **The modulation:** eight free slots (recommended), or the Digitone's six sources × four?
3. **Performance sources:** does the FM-1 have a mod wheel, pitch bend or aftertouch? Should FIZZ take them from USB MIDI in (FM1VST, the web page)? Without them, VEL, KEY, RANDOM and F ENV are the sources that work on the unit.
4. **Portamento on chords:** pair by rank (recommended), or every note from the track's last note? And LEGATO: should "held" mean an overlapping step LEN, as proposed?
5. **ChoralRoot:** FIZZ-only (recommended for now), or port to ChoralRoot as well (its blob version and bit-exact references would move)?
6. **COMB's floor:** 1024-sample lines (43 Hz, 32 KB), or 512 (86 Hz, 16 KB)?
7. **Measurements on the Digitone II** (they pin laws that are guessed here):
   - the AHD HOLD time at a few values;
   - MULTI's TYPE at 0, 64 and 127, by ear or on a scope;
   - PTIM's glide time at 32 and 127;
   - the BR table's steps.
8. **DRIVE before the filter** (the Digitone II's OD ROUTING): add it in M3 as one more value?
9. **The B1 / B5 group names:** PLAY / MOD and AMP / FX, or the owner's words?
