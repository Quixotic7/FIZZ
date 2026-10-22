# Digitone II presets (`.dn2pst`) for FIZZ

What a Digitone II preset file holds, and how its FM TONE sound becomes FIZZ's FM TONE patch (the 73 values of
ChoralRoot's `eng_quad.c` `QP_*`, plus DRIVE). The prototype reader is `tools/dn2pst.mjs`; its test is
`tools/test_dn2pst.mjs`.

**One real `.dn2pst` has been read:** the owner's `BASSSINE` (§7). Everything else comes from other people's reverse
engineering, cross-checked against each other and against the Digitone 1 reader we already trust. §7 says what that
file confirms. §8 lists what still needs real files, and which files to save.

## Sources

| tag | source | version read |
| --- | --- | --- |
| **DNX** | github.com/NoiseAndMatter/DNX (MIT): device captures, firmware reads, and a DN1 → DN2 sound map derived from Elektron's own conversions | `a940275`, 2026-10-17 |
| **SG** | github.com/xrcstrecords/syxgrid-digitone-ii, `sysex-visualiser.html`: its `.dn2pst` writer was checked against 1,309 `.dn2pst` files that Transfer wrote; its parameter names and display laws come from the OS 1.10E firmware | `52b018f`, 2026-10-11 |
| **EK** | github.com/dagargo/elektroid (GPL-3): the transfer tool's Elektron connector and package writer | `fde26a5`, 2026-10-18 |
| **#94** | DNX issue 94: "`28 + 2 × id`, u16be, value × 256, the same law as the preset record" | — |
| **CR** | ChoralRoot `web/fm1dnsnd.js`: the DN1 `.dnsnd` reader, proved by the owner's controlled exports | `9b69580` (FIZZ's copy: `site/js/fm1dnsnd.js`) |

These were read as data only: nothing from them was run, built or installed. Line numbers below are for the versions
listed. The Digitone II manual's MIDI appendix was not needed. The firmware laws in SG cover the same ground and are
more exact, and the modbang randomiser's page names only the FX CCs.

## 1. The file

A `.dn2pst` is a **ZIP** with two entries, the same shape as a DN1 `.dnsnd`:

- `manifest.json`: `{"FormatVersion": "1.0", "ProductType": [], "Payload": "<name, ≤ 15 chars>", "FileType": "Sound", "FirmwareVersion": "1.10E" | "1.11", "MetaInfo": {"Tags": [..]}}`.
  - SG `buildDn2pst` (l. 18309) and `pstPayloadName` (l. 18303): Transfer cuts the payload's name to 15 characters.
  - SG l. 18263–18271: on all 1,309 files, `Tags` equals the record's tag bits, in bit order.
  - `ProductType` is empty for a DN2 file from Transfer (DNX `device-storage.md` l. 263) and `["24","30"]` on a DN1. Elektroid writes the device id instead (EK `elektron_pkg.c` l. 249–270).
  - `FileType` is `"Sound"` (EK `elektron_pkg.c` l. 40; EK `elektron.c` l. 4054–4067: the DN2's `preset-takt-ii` filesystem uses the *sound* package).
- `<Payload>`: the **stored container**, byte for byte what a `+Drive` read of `/soundbanks/<A..H>/<1..256>` returns (EK `elektron.c` l. 50; DNX `device-storage.md` l. 1007–1044).

## 2. The container (31-byte header, body, 12-byte trailer)

| offset | size | field | value in a DN2 preset | source |
| --- | --- | --- | --- | --- |
| 0x00 | 4 | magic | `AC 11 D3 03` | DNX `container.ts` l. 28; EK `elektron.c` l. 124 |
| 0x04 | 1 | header version | 2 | EK l. 125 (DNX read `02 00 05 00` as two u16le) |
| 0x05 | 2 | family, u16be | 5 | EK l. 126 |
| 0x07 | 2 | device, u16be | **15** DN2, 9 DN1 | EK l. 127; DNX `container.ts` l. 30, 185 |
| 0x09 | 4 | build, ASCII | `0049` (1.10D: BASSSINE), `0050` (1.10E), `0059` (1.11) | DNX l. 31; SG l. 18281–18284 |
| 0x0D | 4 | content kind, u32be | **3** = sound | DNX l. 32, 82 (the firmware checks it) |
| 0x11 | 4 | object version, u32be | the record's version (2, 3 on 1.11) | DNX l. 33, 93; SG l. 18269 |
| 0x15 | 4 | index, u32be | bank × 256 + slot − 1 (the device stamps it) | DNX l. 34, 129–148; SG l. 18269 |
| 0x19 | 4 | body length, u32be | **364** | DNX l. 35; `device-storage.md` l. 241 |
| 0x1D | 1 | compressed | 1 = LZ4 chain, 0 = raw | DNX l. 36, 106–121; EK l. 134 |
| 0x1E | 1 | trailer length | 12 | DNX l. 37; EK l. 135 |
| 0x1F | — | body | raw 364 bytes, or an LZ4 chain: u32be size, block, …, u32be 0 | DNX `dn2codec.ts` l. 20–33 |
| end−12 | 4 | check, u32be | CRC-32, **zero initial register**, reflected, xorout FFFFFFFF, over `[0x1F, end−12)` | DNX `checksum.ts` l. 1–40 |
| end−8 | 4 | length, u32be | payload length − 43 | DNX `container.ts` l. 41 |
| end−4 | 4 | footer | `AA A1 DA AA` | DNX l. 42 |

- **Compression.** A preset in a file is LZ4-compressed (SG l. 18264: "LZ4 chunks"). A `+Drive` raw read is not (DNX `device-storage.md` l. 989–1003, 1044). The reader takes both forms.
  - The LZ4 blocks share one dictionary and decode to 32,768 bytes each, the last one short. A preset is one block.
- **A DN1-reader footnote.** `fm1dnsnd.js` reads "the decompressed size" as a u16le at 0x1C. That works only by luck: 0x1C is the low byte of the u32be length at 0x19, and 0x1D is the compression flag (1). For 302 the result is `0x2E | 1 << 8`. A raw (flag 0) `.dnsnd` would break it. `dn2pst.mjs` reads the u32be at 0x19.

## 3. The body and the record

The 364-byte body is a 5-byte prefix and then the 359-byte record:

- The prefix is `00 00 00 <ver> 00`: the record's version as a u32be, then a 0 (DNX `device-storage.md` l. 1309–1343; SG `soundStoredBody` l. 18330).
- The record has no prefix in a SysEx sound dump or a project's pool. A DN1 stored body has no prefix either.
- A body that opens `DN1S` is a DN1 sound kept on a DN2's `+Drive`. Refuse it (SG `recordFromSoundBody` l. 18296).

| record offset | field | source |
| --- | --- | --- |
| 0 | `BE EF BA CE` | DNX `soundmap.ts` l. 63, 190 |
| 4 | version, u32be: 1, 2 (OS ≤ 1.10E), 3 (1.11) | SG l. 18281–18284; DNX `soundmap.ts` l. 58 |
| 8 | tag bits, u32be, bit t = tag t (§5) | DNX `tags.ts` l. 1–74 |
| 12 | name, 16 bytes, NUL-padded, **Windows-1252** | DNX `soundmap.ts` l. 61–67; brief / DNX `device-storage.md` |
| 28 + 2·id | parameter `id`: u16be, **coarse byte then fine byte** (value × 256, i.e. 8.8) | #94; DNX `soundparams.ts`; SG l. 4246–4249 |
| 244 | **machine**: 0 FM TONE, 1 WAVETONE, 2 FM DRUM, 3 SWARMER, 4 MIDI | DNX `machine.ts` l. 21–38; SG l. 1466–1488 |
| 245 | **filter machine**: 0 MULTI-MODE, 1 LOWPASS 4, 2 EQUALIZER, 3 COMB−, 4 LEGACY LP/HP, 5 COMB+ | SG l. 4262–4268, 5597 |
| 251 | the sound's octave, signed semitones (*inferred*: the DN1 v5's +280, copied) | DNX `soundmap.ts` l. 106 |
| 252 + 3k, k < 20 | twenty 3-byte modulation records, the third byte a parameter id (velocity / mod wheel / aftertouch / breath / key-track routings?) | DNX `soundmap.ts` l. 128–131, 212–218 |
| 331..353 | arpeggiator: MODE, SPD, RNG, N.LEN, LEN, step enable u16, 16 offsets | DNX `arp.ts` l. 18–26 |
| 355 | `BA CE F0 0C` | DNX `soundmap.ts` l. 195 |

**The id rule is exact.** Every offset DNX measured on a DN2 sound (`soundparams.ts`) is `28 + 2 × id`, using the lock
ids measured independently (`plockparams.ts`, `machineplock.ts`). For example:

- ALGO is id 33, offset 94.
- PHRT is id 41, offset 110.
- FREQ is id 74, offset 176.
- AMP ATK is id 87, offset 202.
- OVER is id 104, offset 236.
- LFO slot s of LFO l is id `4s + l`, offset `30 + 8s + 2(l − 1)`.

**The value laws are DN1's.** DNX built its DN1 → DN2 converter from 5,760 sound pairs in Elektron's own converted
projects. It reproduces all of them byte for byte, and **no copied field was rescaled** (`soundmap.ts` l. 9–43,
91–116). So each FM TONE value a DN1 sound has is the DN1's bytes at a new offset. ChoralRoot's `fm1dnsnd.js` already
proved those laws on the owner's exports. SG's firmware display laws (l. 4281–4282; renderer `plockRenderLaw`
l. 4330) agree with every one, for example:

- HARM `(raw − 16128) / 256` = coarse − 63.
- MIX and the filter ENV: coarse − 64.
- DTUN and FREQ: `raw / 256` to 2 places.
- The DEC and REL times: 127 = INF.

## 4. FM TONE: DN2 ↔ DN1 ↔ FIZZ

The record's FM TONE parameters, in record order. The columns:

- **DN2 off.**: the offset in the 359-byte record.
- **enc.**: `c` is the coarse byte alone; `cf` is coarse + fine/256 in hundredths (as `fm1dnsnd.js` reads it).
- **DN1 off.**: the offset in the 302-byte DN1 version-5 record (DNX `soundmap.ts` `VERIFIED_COPIES` / `VALIDATION_COPIES`).
- **FIZZ**: the `QP_*` index.
- **conf.**:
  - **C**: confirmed. The DN2 offset was measured by DNX, *and* the law was proved on DN1 exports and carried over by a verified byte copy.
  - **F**: from the firmware tables (SG), with no capture behind it here.
  - **I**: inferred.
  - **G**: guessed.

| parameter | id | DN2 off. | enc. | DN2 value | DN1 off. | FIZZ | conversion | conf. |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| LFO1..3 SPEED | 1/2/3 | 30/32/34 | cf | −64.00..63.99 (coarse − 64) | 30/32/— | 47/55/63 | × 100 | C |
| LFO MULT | 5/6/7 | 38/40/42 | c | 0..23 | 34/36/— | 48/56/64 | **re-ordered**, see below | F |
| LFO FADE | 9/10/11 | 46/48/50 | c | −64..63 | 38/40/— | 49/57/65 | 1:1 | C |
| LFO DEST | 13/14/15 | 54/56/58 | c | a parameter id, 0..104 | 42/44 (DN1 numbering) | 50/58/66 | id → FM TONE destination (below) | C |
| LFO WAVE | 17/18/19 | 62/64/66 | c | TRI SINE SQR SAW EXPO RAMP RAND | 46/48 | 51/59/67 | → FM TONE order `0 1 2 3 5 4 6` | C |
| LFO SPH | 21/22/23 | 70/72/74 | c | 0..127 | 50/52 | 52/60/68 | 1:1 | C |
| LFO MODE | 25/26/27 | 78/80/82 | c | FREE TRIG HOLD ONE HALF | 54/56 | 53/61/69 | 1:1 | C |
| LFO DEPTH | 29/30/31 | 86/88/90 | cf | coarse − 64 (the DN2 *shows* it × 2, −128..127.99: SG law 6) | 58/60 | 54/62/70 | × 100 | C |
| ALGO | 33 | 94 | c | 0..7 → 1..8 | 62 | 0 | + 1 | C |
| RATIO C | 34 | 96 | c | step 0..18: .25 .5 .75 1 2 … 16 | 64 | 1 | 1:1 | C |
| RATIO A | 35 | 98 | c | step 0..35: .25…5 by ¼, 5.5…10 by ½, 11…16 | 66 | 2 | ratio × 4 − 1 (exact) | C (table F) |
| RATIO B | 36 | 100 | u16 | `2 × (B1·19 + B2)` (+ a low bit) | 68 | 3, 71 | B1 = n / 19, B2 = n % 19 | C |
| HARM | 37 | 102 | cf | coarse − 63: −26..+26 | 70 | 4 | × 100 | C |
| DTUN | 38 | 104 | cf | 0..127 | 72 | 5 | × 100 | C |
| FDBK | 39 | 106 | c | 0..127 | 74 | 6 | 1:1 | C |
| MIX | 40 | 108 | c | −64..63 | 76 | 7 | 1:1 | C |
| PHRT | 41 | 110 | c | OFF ALL C A+B1+B2 A+B2 | 78 | 23 | 1:1 | C |
| A ATK DEC END LEV | 43–46 | 114–120 | c | 0..127 | 82–88 | 12–15 | 1:1 | C |
| B ATK DEC END LEV | 47–50 | 122–128 | c | 0..127 | 90–96 | 16–19 | 1:1 | C |
| A DELAY, TRIG, RESET | 51–53 | 130–134 | c | 0..127, off/on, off/on | 98–102 | 20–22 | 1:1 | C |
| B DELAY, TRIG, RESET | 54–56 | 136–140 | c | as A | 104–108 | 24–26 | 1:1 | C |
| (id 57) | 57 | 142 | c | ? (DN1's `unk110`) | 110 | — | not imported | — |
| (PITCH ALL … AB DELAY) | 58–65 | 144–158 | — | LFO destinations only; `0x40` in every record | — | — | — | — |
| RATIO OFFSET C A B1 B2 | 66–69 | 160–166 | cf | coarse − 64 = −1.00..+0.99 (DNX measured −0.5 → 32) | 174–180 | 8–11 | × 100 / 64 | C |
| KEY TRACK A B1 B2 | 70–72 | 168/170/172 | c | 0..127 | **274/275/276** | 28, 29, 72 | 1:1 | I |
| FILTER TYPE | 73 | 174 | c | per filter machine (below) | 128 (DN1 type) | 36 | below | F |
| FILTER FREQ | 74 | 176 | cf | 0..127.99 | 130 | 34 | × 100 | C |
| FILTER RESO | 75 | 178 | cf | 0..127 | 132 | 35 | × 100 | C |
| FILTER ENV | 76 | 180 | cf | −64..63 | 134 | 37 | × 100 | C |
| FILTER ENV DELAY | 77 | 182 | c | 0..127 | 182 | 38 | 1:1 | C |
| FILTER ATK DEC SUS REL | 78–81 | 184–190 | c | 0..127; DEC, REL 127 = INF | 136–142 | 30–33 | 1:1; DEC INF → SUS 127 | C |
| FILTER KEY TRACK | 82 | 192 | c | 0..100 % | **277** | 39 | % × 127 / 100 | G (scale) |
| BASE | 83 | 194 | cf | 0..127 | 144 | 40 | × 100 | C |
| WIDTH | 84 | 196 | c | 0..127 | 146 | 41 | 1:1 | C |
| FILTER ENV RESET | 85 | 198 | c | off/on (1 by default) | — | — | noted if off | F |
| AMP DELAY? | 86 | 200 | c | ? (SG names it; DNX lists 86 as unmapped) | — | — | — | — |
| AMP ATK | 87 | 202 | c | 0..127 | 148 | 42 | 1:1 | C |
| AMP HOLD | 88 | 204 | c | 0..126, 127 = NOTE | — (DN2 only; 127 by default) | — | AHD only, below | C |
| AMP DEC | 89 | 206 | c | 0..126, 127 = INF | 150 | 43 | 1:1 (≤ 126, 127 INF) | C |
| AMP SUS | 90 | 208 | c | 0..127 | 152 | 44 | 1:1 | C |
| AMP REL | 91 | 210 | cf | 127 = INF | 154 | 45 | rounded, ≤ 126, 127 INF | C |
| CHORUS / DELAY / REVERB send | 92/93/94 | 212/214/216 | cf | 0..127 (0 = OFF) | 166/164/162 | — | `sends`, track settings | C |
| PAN | 95 | 218 | c | −64..63 | 158 | — | `pan`, a track setting | C |
| VOL | 96 | 220 | cf | 0..127 | 160 | 46 LEVEL | rounded | C |
| AMP MODE | 97 | 222 | c | 0 AHD, 1 ADSR (1 by default) | — | — | below | C |
| AMP ENV RESET | 98 | 224 | c | off/on | 168 | — | noted if off | C |
| PORTAMENTO, TIME | 99/100 | 226/228 | c | SG: 64 = ON; time + 1 | 170/172 (DN1's `unk170`/`unk172`) | — | noted | F |
| BIT REDUCTION | 101 | 230 | c | OFF, then a table | — | — | noted | C |
| SRR, SRR ROUTING | 102/103 | 232/234 | c | 0..127; pre/post | — | — | noted | C / F |
| OVERDRIVE | 104 | 236 | cf | 0..127 | **156 (DN1 DRIVE)** | 73 DRIVE | rounded | C |
| BW / OD ROUTING | 105/106 | 238/240 | c | pre/post filter | — | — | not imported | F |
| octave | — | 251 | s8 | semitones | 280 (v5) | — | every ratio × 2^(oct/12), as `dnsndToQuad` | I |

### The value conversions

- **LFO MULT: re-ordered, not 1:1.** SG measured MULT on a DN2 in runs of six (H46, l. 4367–4377): 0..5 are ×1..×32 at the BPM, and 6..11 are ×1..×32 at a fixed 120.
  - By the manual's range, 12..17 are ×64..×2k at the BPM, and 18..23 the same fixed.
  - FM TONE has 0..11 at the BPM and 12..23 fixed (`eng_quad.c` `QUAD_NMULT`).
  - So `fm = n % 6 + 6·⌊n / 12⌋ + 12·(⌊n / 6⌋ odd)` (`multToQuad`).
  - This agrees with `fm1dnsnd.js` on the steps it proved (0, 5, 21–23) and differs on 6..20, which nothing has proved for DN1. **See §9.**
- **RATIO A: the firmware's table.** It is 0.25..5 by quarters, then 5.5..10 by halves, then 11..16 (SG `PLOCK_STRTAB` l. 4278, law 25).
  - All six DN1 steps proved by exports land on it: 0 = .25, 3 = 1, 9 = 2.5, 15 = 4, 25 = 8, 35 = 16.
  - `fm1dnsnd.js` interpolates steps 16..24 and 26..34 instead (4.4, 4.8, …). **See §9.**
  - Every step is a multiple of ¼, so FM TONE's RATIO A holds each one exactly.
- **RATIO B.** SG's firmware "rpair" renderer (l. 4379) reads B1 as `n / 361` and marks it as not measured. ChoralRoot's law (`n / 19`, `n % 19`) was proved on DN1 exports, and the byte is a verified copy, so the reader keeps ChoralRoot's.
- **LFO DEST is a parameter id.** DNX's selector table, which maps DN1 destination numbers to DN2 ones (`soundmap.ts` l. 212–235), lands every entry on the id of the same parameter. Examples: DN1 17 ALGO → 33, 51 FREQ → 74, 64 DRIVE → 104 OVER.
  - The id → FM TONE destination table is `DN2_LFO_DEST` in `dn2pst.mjs`.
  - Off, with a note: LFO 3 aiming at **LFO 2** (ids 2, 6, … 30), since FM TONE's LFO 2 and 3 can only move LFO 1.
  - Off, with a note: any DN2-only target, such as KEY.T, HOLD, BR or SRR.
  - The test checks the parts DNX's table leaves open against the owner's DN1 exports: DN1 13 → 25 (LFO 1 MODE), and 74..76 → 67..69 (RATIO OFFSET A, B1, B2).
- **The filter.** The DN1 type became a machine plus a type. DNX `SCALAR_FIELDS` (l. 250–252): DN1 OFF → LEGACY type 3, LP2 → LEGACY 0, HP2 → LEGACY 1, LP4 → LOWPASS 4.

| filter machine | FIZZ `FTYPE` |
| --- | --- |
| LOWPASS 4 | LP24 |
| LEGACY LP/HP | type 0 → LP12, 1 → HP12, 2 / 3 → OFF (the firmware lists `LP HP OFF`; Elektron's converter writes 3 for OFF) |
| MULTI-MODE | TYPE ≥ 96 → HP12, else LP12; 33..95 noted as band-pass, which FM TONE lacks (*inferred*: TYPE taken as an LP→BP→HP morph) |
| EQUALIZER, COMB−, COMB+ | OFF, noted |

- **AMP AHD.** In ADSR (MODE 1) the import is 1:1, and HOLD sits at 127, unused. FM TONE has only ADSR, so AHD is approximated:
  - HOLD = NOTE (127) → SUS 127, REL = DEC.
  - A timed HOLD → DEC, SUS 0, REL = DEC, and the hold time is lost (noted).
- **DRIVE.** The DN2's FX OVERDRIVE is the DN1's DRIVE: Elektron copied DN1 156 to DN2 236. It becomes FIZZ's DRIVE (value 73).
- **FM TONE has no equivalent** for these, so they are dropped and noted: BIT REDUCTION, SRR, the three routings, portamento, FILTER ENV RESET off, AMP ENV RESET off, the modulation records, and the arpeggiator.
- **FM TONE's VEL (27) stays at its default.** The DN2 keeps velocity routing in the modulation records, which are not decoded.

## 5. Name and tags

- **The name.** It is 16 bytes, Windows-1252, NUL-padded. The reader decodes it in full, then FIZZ's 12 ASCII letters fold accents, so `PLUCKY EÅ` becomes `PLUCKY EA`.
- **The tags.** There are 32 bits at +8, a closed set: KICK SNARE DEEP BRASS STRINGS PERCUSSION HI-HAT CYMBAL EVOLVING EXPRESSIVE BASS LEAD PAD TEXTURE CHORD SOUND FX ARPEGGIO METALLIC ACOUSTIC ATMOSPHERIC NOISY GLITCH HARD SOFT DARK BRIGHT VINTAGE EPIC FAIL LOOP MINE FAVOURITE.
  - Sources: DNX `tags.ts` l. 41–74, confirmed from the DN2's TAGS screen.
  - Transfer spells bit 19 `ATMOSPHERIC` (SG l. 18273).
  - The manifest's `Tags` repeat the bits. The reader keeps the record's bits and warns if the two disagree.
- **The FIZZ kind** is `fizzTags`, in FIZZ's order. It is a judgement, not a fact of the format:

| Digitone tag | FIZZ kind |
| --- | --- |
| BASS | BASS |
| CHORD | KEYS |
| PAD | PAD |
| LEAD | LEAD |
| KICK SNARE PERCUSSION HI-HAT CYMBAL | DRUMS |
| SOUND FX | FX |
| nothing | PLUCK |

FIZZ's STORE frame has no tag field today: FIZZ guesses the kind from the patch (`qd_preset_kind`). See §10.

## 6. Machines FIZZ cannot import

The machine byte at +244 decides:

- **FM TONE (0)** imports.
- **WAVETONE (1), FM DRUM (2), SWARMER (3) and MIDI (4)** reuse ids 33..72 for different parameters (DNX `machineplock.ts` l. 4–19, 84–216; SG `PLOCK_NAMES.syn`). They have no FM TONE patch.
  - `parseDn2pst` refuses them with `"<name>" is a <MACHINE> sound: FIZZ imports FM TONE sounds only` (`e.code = "DN2_MACHINE"`).
  - With `{anyMachine: true}`, it returns the name, tags and machine and no parameters, so a page can list them greyed out.
- **The filter machine** matters as well:
  - LOWPASS 4 imports exactly.
  - MULTI-MODE and LEGACY LP/HP import approximately.
  - EQUALIZER and both COMBs import with the filter OFF.

## 7. What the real file `BASSSINE.dn2pst` confirms (2026-10-22)

The owner's preset, read in place, never copied. It is 582 bytes, a ZIP of two deflated entries. `tools/dn2pst.mjs`
reads it with **no warnings**.

**The container: confirmed in every field.**

- **The manifest:**

  ```json
  {"FormatVersion": "1.0", "ProductType": [], "Payload": "CATN_BASSSINE", "FileType": "Sound", "FirmwareVersion": "1.10D", "MetaInfo": {"Tags": ["BASS"]}}
  ```

  - The payload's name is the sound's name, 13 characters, so it is not cut.
- **The payload** is 253 bytes. Its header bytes are `AC 11 D3 03 | 02 | 00 05 | 00 0F | "0049" | 00 00 00 03 | 00 00 00 02 | 00 00 00 AE | 00 00 01 6C | 01 | 0C`:
  - header version 2, family 5, device 15;
  - build `0049` for 1.10D;
  - kind 3, object version 2;
  - index 174 (bank A, slot 175), body length 364;
  - compressed, trailer 12.
- **The body** is **one LZ4 block of 202 bytes**, then the u32 0 terminator.
- **The trailer** is `2A 2C AD 02 | 00 00 00 D2 | AA A1 DA AA`:
  - The CRC-32 (zero seed) over `[0x1F, end − 12)` **matches**.
  - The length 210 = 253 − 43.
- **The body's prefix** is `00 00 00 02 00`. **The record** is 359 bytes: `BEEFBACE`, version 2, and it ends `BA CE F0 0C`.

**The record's head: confirmed.**

- The tag word `00 00 04 00` is bit 10, BASS, which equals the manifest's `Tags`.
- The name `CATN_BASSSINE` sits at +12.
- The machine at +244 is **0, FM TONE**. The filter machine at +245 is **0, MULTI-MODE**.

**The defaults the DN1 conversion predicts are all there:**

- LFO SPEED 0x70 (+48) on all three LFOs, LFO 3's MULT 3 (and LFO 2's).
- `0x40` at 144..166: ids 58..65 and the four RATIO OFFSETs, which are 0.
- AMP HOLD 127 at 204, AMP MODE 1 (ADSR) at 222, 249 = 1.

**The arpeggiator**, at DNX's offsets: MODE 0 (off) at 331, then 0D 00 0E 0F, and an enable word of FFFF at 336 (all 16 steps).

**The values decode plausibly for a sine bass.** Plausible does not prove an offset: one file cannot single a field out.

| field | read | comment |
| --- | --- | --- |
| ALGO | 6 | |
| RATIO C / A / B1 / B2 | 1 / 3 / 11 / 1 | B from `01 F5`: n = 250 = 13·19 + 3 |
| HARM | +6.21 | |
| DTUN | 0 | |
| FDBK | 127 | |
| MIX | −64 | fully one side |
| A LEV, B LEV | 1, 63 | |
| A END, B END | 127, 127 | |
| PHRT | ALL | |
| FREQ, RESO | 37.42, 36 | |
| filter ENV | 0 | |
| amp ATK DEC SUS REL | 21 32 96 36 | |
| VOL | 127 | |
| OVERDRIVE | 1 | |
| octave (+251) | −24 | two octaves down: what a bass preset would carry |
| sends, pan | 0 | |
| LFOs | depth 0 | |

**Three readings are questionable:**

- **MULTI-MODE TYPE = 127.** The reader takes it as HP12, assuming TYPE morphs LP → HP. A high-pass at FREQ 37 on a sine bass is odd; a low-pass is the likelier sound. **This decides how every MULTI-MODE preset imports.**
- **PORTAMENTO (+226) = 63, and TIME (+228) = 39.** SG's law shows 64 as ON, so 63 reads OFF. Why the time is set is unexplained.
- **FILTER ENV RESET (+198) = 0.** The DN1 conversion's default is 1.

## 8. Unknowns, and the files that settle them

### What is still open

1. **The record's version.** A preset saved on 1.11 should be **version 3**. The reader reads it with version 2's layout and warns. A diff of the same sound saved on 1.10E and 1.11 would settle what changed.
2. **The filter.**
   - MULTI-MODE TYPE: is it 0..127, and is it an LP→BP→HP morph? BASSSINE has 127 (§7).
   - LEGACY LP/HP: does type 3 mean OFF?
3. **FILTER KEY TRACK.** Confirm the 0..100 % law and the scale to FM TONE's KTRK. FM TONE adds `semitones × KTRK × 2.34 / 256` of a FREQ step, so 100 % ≈ 109 if a FREQ step is a semitone. The reader uses × 1.27.
4. **KEY TRACK A, B1, B2.** These are offsets 168..172 at 0..127. Is the law the same as FM TONE's `AKEY` (level scaling −octaves × KEY / 127)?
5. **LFO MULT 6..23.** Check on a DN2 export with MULT at ×64, ×2k and fixed ×1 (§9).
6. **The octave at +251.** Is it the sound's OCTAVE, in semitones? Does the DN2 still have one? BASSSINE's −24 suits a bass.
7. **AMP AHD / HOLD.** Check against an AHD preset: HOLD at 127 = NOTE, and a timed hold.
8. **Ids 57, 86 and the bytes 246..250.** These are unidentified (BASSSINE: id 57 = 127, 246 = 4, 250 = 2). DNX copies DN1 188/189 to 246/247, 279 to 250, and holds 249 at 1.
9. **The modulation records (252..311).** These may be velocity, mod wheel, aftertouch, breath and key-track routings: 20 records could be 5 sources × 4 destinations. Not decoded.

### Files to save

Save **single-parameter saves from one baseline**, as ChoralRoot's DN1 `PresetFormat` exports were. Each one is the
same FM TONE preset saved again with one control moved, named after the move. A save then differs from the baseline in
exactly one field, which proves both the offset and the law.

- **Baseline.** Init an FM TONE sound and save it.
- **Filter TYPE** (MULTI-MODE): TYPE 0, 32, 64, 96 and 127, plus a note of what the screen calls each.
- **Filter machines:** the baseline with LOWPASS 4, then LEGACY LP/HP set to LP, HP and its third value.
- **Filter KEY TRACK:** 0, 50 % and 100 %.
- **KEY TRACK** A 64, B1 64 and B2 64, one per save.
- **LFO MULT** on LFO 1: its 7th, 12th, 13th, 18th and 24th positions (counting the knob from the bottom), each with the value the screen shows.
- **AMP:** MODE AHD with HOLD at NOTE, then AHD with HOLD at 32.
- **Octave:** the sound's octave (if the DN2 has one) at −1 and +1.
- **OS 1.11:** the baseline saved on 1.11, for record version 3.
- **More presets** of other machines (WAVETONE, FM DRUM, SWARMER), to check that the refusal message reads well.

To check, put the files in `Digitone2Presets/` and run:

```
node tools/test_dn2pst.mjs "" ../Digitone2Presets
```

For each file it prints:

- its name, version, filter machine and tags;
- the FIZZ kind, the DRIVE and every note.

A diff of two files' records names the field that moved.

## 9. Findings for ChoralRoot's DN1 reader

These hold for `fm1dnsnd.js` as well, because the DN1 → DN2 conversion copies these bytes unchanged. They are not
fixed here: the ChoralRoot repo is read-only from this task.

- **RATIO A, steps 16..24 and 26..34.** The firmware's table says 4.25, 4.5, … 5, 5.5, … 10, 11, … 15. `DN_RATIO_A` interpolates 4.4, 4.8, … instead.
- **LFO MULT 6..20.** The DN2 reading (SG H46) puts the fixed-120 BPM run at 6..11, not at 12..17. `fm1dnsnd.js` maps MULT 1:1 onto FM TONE's order, where 6..11 are ×64..×2k at the BPM. Its own docs say "which half is which is not proved".
- **KEY TRACK.** The DN1 keeps KEY TRACK A, B1, B2 at 274, 275, 276 and the FILTER KEY TRACK at 277 (DNX copy map). The reader leaves FM TONE's AKEY / B1KEY / B2KEY / FKTRK at 0.
- **Two bytes the reader marks unknown.** `unk170` / `unk172` are PORTAMENTO on and time.
- **The size field.** `fm1dnsnd.js` reads it from the wrong field; it works only by luck (§2).

## 10. What a DN2 import needs beyond the parser

- **FIZZ's web page** (`site/js/digitone-ui.js`):
  - Accept `.dn2pst` beside `.dnsnd`: the file input's `accept`, and the drop filter.
  - Copy `tools/dn2pst.mjs` to `site/js/`, then call `parseDn2pst(bytes, {anyMachine: true})`.
  - List non-FM-TONE files as not importable.
  - Send `dn2pstToQuad`'s `{blob, name, drive}` through `storeSound` exactly as `.dnsnd` does. `pan` and `sends` go with LOAD.
  - Add a `.dn2pst` case to `site/tools/test_site.mjs`.
- **The FIZZ kind from the tags** needs a protocol change, for example a STORE body byte carrying a FIZZ kind mask. Without one, FIZZ keeps guessing from the patch.
- **FM1VST's drop**: the same reader run in the plugin's web view, or a C port.
  - A C port needs a small LZ4 block decoder (about 40 lines), a ZIP reader with inflate (the plugin already links zlib through JUCE), and this table.
  - The parameter laws are simple byte reads. A table-driven C version is the same size as the JS.
- **A DN2 Transfer/backup folder** (DNX's `.dn2snd` in backups, `soundbanks/A/001 NAME.dn2snd`) holds the same stored container without the ZIP.
  - `parseDn2pst` already reads a bare container. A page could accept `.dn2snd` too.
- **Project files** (`.dn2prj`) hold up to 128 pool sounds and 16 kit sounds, each a bare 359-byte record.
  - The geometry is in DNX `dn2image.ts` / `soundmap.ts` `DN2_POOL_OFFSET`.
  - `parseDn2pst(record)` reads each one. A project import would need only the LZ4 chain and the offsets.

## 11. Export: FIZZ presets as Digitone files (2026-10-22)

`tools/fizz2dn.mjs` writes a FIZZ sound as a Digitone file:

- `fizzToDnsnd(sound)` writes a `.dnsnd` for the Digitone, as OS 1.44 saves it, with a version-5 record.
- `fizzToDn2pst(sound)` writes a `.dn2pst` for the Digitone II, with a version-2 record and machine FM TONE.
- Each returns `{bytes, notes}`. The notes say what the format cannot hold.
- The input is a FIZZ patch: a version-5 or version-6 blob, or the 110 values. The name, tags, pan, sends and DRIVE come with it.
- The wizard publishes a copy as `site/js/dn2wizard/fizz2dn.js`, with three FIZZ-made test presets for the owner to load (`testpresets.js`).
- The tests are in `tools/test_fizz2dn.mjs`. They read the owner's files in place.

### The file, as the instruments and Transfer write it

- **The ZIP.** It has the same entries and the same header fields as Transfer's:
  - version made by 2.0, version needed 1.0, the UTF-8 flag, method 8, no extra fields.
  - Each entry is one *stored* deflate block, a valid deflate stream that `unzip -t` accepts. Transfer compresses its entries for real. This is the only difference in structure.
- **`manifest.json`.** It is written exactly as Transfer writes it: two-space indents, CRLF line endings, no final newline, the keys in Transfer's order. That is `JSON.stringify(m, null, 2)` with each `\n` turned into `\r\n`.
  - A Digitone file says `ProductType ["24","30"]` and `FirmwareVersion "1.42A"`. Every Transfer 1.42A file in the corpus says that, the OS 1.44 sounds included.
  - A Digitone II file says `ProductType []` and `"1.10E"`.
  - The tags are the record's tag bits, in bit order.
  - The manifest is identical in **146 of 146** version-5 corpus files written by Transfer 1.42A. The other 60, from an older Transfer, have `"1.32A"`, product `["20","28"]`, no tags and build `0070`.
- **The container** matches the instrument's, field for field:
  - the device byte: 9 on the Digitone, 15 on the Digitone II;
  - the build: `0097` on the Digitone, `0050` on the Digitone II;
  - content kind 3, the record's version, index 0 (the slot: the device stamps it), the body length, the compressed flag 1, trailer length 12;
  - the CRC-32 (zero seed) and the trailer.
  - The header is identical, slot aside, in **146 of 146** Transfer-1.42A files. Against `BASSSINE.dn2pst`, the magic, family, device, kind, lengths and flags all match.
- **Elektron's LZ4, found.** The instruments compress with the reference LZ4 fast compressor at acceleration 1, with one difference: the four bytes to hash are read **big-endian**. The hash is `(be32(p) × 2654435761) >> 20`, a 4096-entry table of positions, MINMATCH 4, MFLIMIT 12, LASTLITERALS 5, skip trigger 6.
  - `lz4Elektron` rebuilds **every one of the 463 blocks on hand**, byte for byte: all 462 Digitone files and BASSSINE.
  - The usual little-endian read gives 448, or 457 with the 8,192-entry table.
  - So a FIZZ export's payload is what the instrument itself would write for the same record.

### The records

- **The Digitone's record** (302 bytes, version 5) is written from the field table of `fm1dnsnd.js` (§4's DN1 offsets), inverted.
  - Key tracks A, B1, B2 and the filter's go to +274..+277 (DNX's map).
  - Portamento goes to +170 / +172: ON = 64 (inferred, as on the DN2), OFF = 63.
  - Every other byte takes its INIT value from `DN1_INIT`:
    - the eight `40 00` at +112..+127;
    - AMP ENV RESET on at +168;
    - portamento OFF at +170 and its time at +172;
    - +188 = 2;
    - the arpeggiator's defaults at +251 / +253..+256;
    - +279 = 2 and +283 = 100;
    - the end mark at +298.
  - Each of these is the value in the owner's INIT-based export (`BASE`) and in most of the 206 version-5 records: 83 % for +188, 91 % for +168, 97 % or more for the rest. Their meanings, where DNX knows them, are in §3.
  - **No Elektron or owner file is embedded:** the table holds a value per byte, as DNX's `DEFAULT_BYTES` does.
- **The Digitone II's record** (359 bytes, version 2) is written from §4's table (`28 + 2 × id`) with these mappings:

| FIZZ value | Digitone II field |
| --- | --- |
| FTYPE OFF / LP12 / HP12 | LEGACY LP/HP type 3 / 0 / 1 (Elektron's own conversion of the DN1's) |
| LP24 | LOWPASS 4 |
| FIZZ's planned MULTI, COMB+, COMB−, EQ | their machines; FSHAPE to TYPE (+174), FGAIN to the EQ's GAIN (+178) |
| AMOD / HOLD | AMP MODE (+222: 0 AHD, 1 ADSR) / HOLD (+204) |
| PORT / PTIM | +226 (64 ON, 63 OFF) / +228 |
| BITS / SRR / CRPOST | +230 / +232 / +234 |
| BWPOST | +238 |
| DRPRE | OD ROUTING (+240: 0 before the filter) |
| LFO 3 | its own fields |
| MULT | in the Digitone II's order (`multToQuad` inverted) |

  - The other bytes come from `DN2_INIT`, Elektron's DN1 → DN2 conversion of the DN1 INIT: DNX's `DEFAULT_BYTES` and its copies (+246 = 2, +250 = 2, +326 = 100, the arpeggiator at +332..+337).
  - BASSSINE, a third party's sound, has +246 = 4 and +324 = 2 there. **The Digitone II's own INIT may differ in these bytes.** The wizard's baseline dump of an INIT FM TONE sound will tell, and `DN2_INIT` should then take its values.
- **The name** is upper-cased printable ASCII (Latin-1 letters kept), at most 16 characters. The ZIP's payload entry is cut to 15, as Transfer cuts it.
- **FIZZ's tags** map to the nearest Digitone words:

| FIZZ | Digitone |
| --- | --- |
| BASS | BASS |
| KEYS | CHORD |
| PLUCK | ARPEGGIO |
| PAD | PAD |
| LEAD | LEAD |
| DRUMS | PERCUSSION |
| FX | SOUND FX |

- **Pan, the sends and DRIVE** go into their fields. DRIVE goes to the DN1's DRIVE and the DN2's OVERDRIVE.

### What the formats cannot hold (the `notes`)

| | Digitone | Digitone II |
| --- | --- | --- |
| VEL (the operators' velocity) | dropped | dropped |
| LFO 3 | dropped | kept |
| AHD, HOLD | as an ADSR like it | kept |
| CRUSH (BITS, SRR), base-width after the filter | dropped | kept (the laws are not verified) |
| DRIVE after the voice | the DN1's drive is before the filter (noted) | OD ROUTING |
| the modulation slots | not written (the records are not decoded) | not written |
| portamento LEGATO, SLOPE RATE | written as ON; the slope dropped | the same |
| RATIO A off the 36 steps | the nearest step | the nearest step |
| filter DEC 127 with SUS < 127 | 126 (127 is INF there) | 126 |
| filter key track | percent: 101 steps for FIZZ's 128 | the same |

### Checked (`node tools/test_fizz2dn.mjs`)

- **Elektron's LZ4:** 463 of 463 blocks rebuilt.
- **All 462 Digitone files.** Each is imported (`dnsndToQuad`), exported and imported again, and comes back as **the same FIZZ patch**.
  - In 36 of them, RATIO A sits on a step that ChoralRoot's reader interpolates (§9). Those are compared without RATIO A.
- **The 206 version-5 originals, byte for byte:**
  - 11 records come out identical.
  - The bytes that differ all fall into these groups (the test lists each offset and how many files):
    - the fine bytes: FIZZ keeps hundredths, where the Digitone keeps 1/256ths;
    - the sends, AMP REL, DRIVE and VOL, which are whole numbers in FIZZ;
    - the octave, folded into the ratios on import;
    - RATIO B's low bit;
    - AMP ENV RESET off (FIZZ always resets);
    - the importer's DEC-INF → SUS 127 rule;
    - the bytes FIZZ has no value for: +110, +188, the modulation records, the arpeggiator, the tail +273..+297;
    - key tracks and portamento, which ChoralRoot's importer does not read;
    - name bytes after the name's end.
  - **No other byte differs.**
- **400 random FIZZ patches through each format and its reader** come back as they were:
  - through the Digitone: within what it holds;
  - through the Digitone II: AHD, portamento, DRIVE's place, CRUSH, LFO 3 and key tracks included.
- `unzip -t` passes on both kinds of file, and the payloads read clean (CRC, lengths, the LZ4 chain).

The import side moved with the export (`dn2pst.mjs`): HOLD and AHD's SUS / REL are kept, and BITS / SRR / CRPOST are taken 1:1, so a FIZZ → Digitone II → FIZZ round trip keeps them.

### How sure, and what is not proved

- **Digitone (`.dnsnd`): high confidence.**
  - The payload is what the instrument writes, given the record, and the manifest is Transfer's own.
  - The record differs from real ones only in the fields listed.
  - Not proved: that Transfer and OS 1.44 accept a ZIP whose entries are *stored* deflate blocks. It is standard deflate, but no device has seen one.
- **Digitone II (`.dn2pst`): good on the container, moderate on the record.**
  - The container and LZ4 match BASSSINE's exactly.
  - The FM TONE fields are §4's confirmed table.
  - Not proved:
    - the INIT bytes FIZZ has no value for (+246, +324, the arpeggiator: see above);
    - the encodings of PORT / PTIM, AHD's HOLD, BITS / SRR and their routings, BWPOST and OD ROUTING (§8);
    - that OS 1.11 takes a version-2 record, as syxgrid's notes say it does, upgrading it on load.
- **The next proof** is a Digitone II owner loading the wizard's `FIZZ TEST 1..3` (DN2-MEASURE.md), and a Digitone owner loading an exported `.dnsnd`.
