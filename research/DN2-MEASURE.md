# A favour for a Digitone II owner: some test presets

We are teaching a small FM synth (FIZZ, on the M-VAVE FM-1) to load Digitone II presets. We can read the preset files,
but a few settings can only be pinned down by someone with a real Digitone II. It is about an hour of turning one knob
and saving. Thank you!

## How it works

- **Start every preset from the same base.** Make a new **FM TONE** sound from INIT and save it unchanged as `INIT`.
  - For items 1, 4, 5 and 6 you need a bright version of it: INIT with **A LEV 127, B LEV 127, FDBK 80**. Save that as `BRIGHT`.
- **For each preset below, change only what the line says.** Then save it to the +Drive preset library under the name given.
  - Leave the tags empty.
  - Go back to the base before the next one.
- **Note what the screen shows** where the line asks: a word, a number or a symbol. A phone photo of the screen is perfect, named like the preset.
- **Recordings** are WAV files, 44.1 or 48 kHz, from the main outputs, dry: chorus, delay and reverb sends at 0. Name each one like its preset (`MM TYPE 064.wav`). Play from a trig or the keys at **120 BPM**.

## The list, most important first

| # | change (from the base) | save as | note on the screen | record |
| --- | --- | --- | --- | --- |
| 1 | **Filter machine MULTI-MODE**, FREQ 64, RESO 64. Then **TYPE** at 0, 32, 64, 96, 127 (base `BRIGHT`) | `MM TYPE 000` … `MM TYPE 127` | what TYPE shows at each value (LP, BP, HP, or a number) | 3 s of C3 for each |
| 2 | Filter machine **LEGACY LP/HP**, then each TYPE it offers (base `BRIGHT`) | `LEG LP`, `LEG HP`, `LEG OFF` (if there is an OFF) | the list of TYPE choices | no |
| 3 | **AMP MODE AHD**, ATK 0, DEC 0. Then **HOLD** at 0, 32, 64, 96, 126, and NOTE (the top) | `AHD HOLD 000` … `AHD HOLD 126`, `AHD HOLD NOTE` | what HOLD shows at each | one short tap of C3, recorded until silent (up to 20 s). For NOTE: hold C3 for 2 s, then let go |
| 4 | **Portamento** on (PORT ON), **PTIM** at 32, 64, 127. Two trigs: C2 on step 1, C4 on step 5 (base `BRIGHT`) | `PORT 032`, `PORT 064`, `PORT 127` | also photograph the sound's PORTAMENTO settings menu (type, slope …) if it has one | 4 s for each |
| 5 | Same as `PORT 064`, but with the portamento type set to **LEGATO ONLY**, if it exists | `PORT LEGATO` | — | no |
| 6 | **BIT REDUCTION** at 0, 32, 64, 96, 127. Then **SAMPLE RATE REDUCTION** at 32 and 127, and SRR at 64 with its routing **before** and then **after** the filter (base `BRIGHT`) | `BR 000` … `BR 127`, `SRR 032`, `SRR 127`, `SRR PRE`, `SRR POST` | what BR shows at each (bits?) | 2 s of C3 for each |
| 7 | **LFO 1 MULT**: turn it all the way down, then save at the 1st, 7th, 12th, 13th, 18th and 24th position (counting clicks from the bottom) | `MULT 01`, `MULT 07`, `MULT 12`, `MULT 13`, `MULT 18`, `MULT 24` | exactly what MULT shows at each (e.g. "BPM 64", "x 2") | no |
| 8 | Filter **KEY TRACK** at 0, 50 and 100, with the filter at LOWPASS 4, FREQ 40 (base `BRIGHT`) | `FKT 000`, `FKT 050`, `FKT 100` | — | 1 s each of C2, C3, C4, C5 |
| 9 | **Key track A** at 64 (with A LEV 100). Then **B1** at 64 and **B2** at 64 (with B LEV 100), one per preset | `KTRK A 64`, `KTRK B1 64`, `KTRK B2 64` | — | no |
| 10 | **Modulation** (the sound's velocity / mod wheel / … menus): VELOCITY → FILTER FREQ, depth +32; again with −32. MOD WHEEL → HARM +64. AFTERTOUCH → A LEV +64. PITCH BEND → MIX +64. BREATH → FDBK +64. KEY TRACK (if it is a source) → FREQ +64. Finally all four velocity slots used. | `VEL FREQ P32`, `VEL FREQ M32`, `MW HARM`, `AT LEVA`, `PB MIX`, `BR FDBK`, `KT FREQ`, `VEL 4 SLOTS` | what each depth shows | no |
| 11 | The sound's **OCTAVE** setting (if the Digitone II has one) at −1 and +1 | `OCT M1`, `OCT P1` | where the setting is | no |
| 12 | Save `INIT` once on **OS 1.11** (and, if you still have it, once on 1.10) | `INIT 111`, `INIT 110` | the OS version | no |
| 13 | Any one **WAVETONE** and one **FM DRUM** preset, as they are | their own names | — | no |

## Sending them back

1. Open **Elektron Transfer**, connect the Digitone II, and copy these presets from the +Drive library to a folder on the computer. They arrive as `.dn2pst` files.
2. Put the WAVs and the screen photos in the same folder.
3. Write the OS version in a short note.txt.
4. Zip the folder and send it to us.

## And one more: a preset made by FIZZ

FIZZ can now write `.dn2pst` files. We will send a few (the harvest page offers three: `FIZZ TEST 1..3`). Please load them with Transfer into empty preset slots, put each on a track, and tell us: does it load, are its name and tags right, and does it sound like its description?
