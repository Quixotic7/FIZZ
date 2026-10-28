// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
// Copied from FIZZ's tools/dn2pst.mjs (e7e6432) so the published site is self-contained.
// Elektron Digitone II presets (.dn2pst) read for FM TONE (FIZZ's patch = ChoralRoot's eng_quad.c QP_*, plus DRIVE).
// A PROTOTYPE: written from other people's reverse engineering (research/DN2-FORMAT.md cites each fact) and checked
// against a synthetic object, against Digitone 1 sounds put through the DN1 -> DN2 map, and against one real file (the
// owner's BASSSINE: the container and the record's head confirmed, the values plausible). It mirrors ChoralRoot's web/fm1dnsnd.js (parseDnsnd, dnsndToQuad): the same value laws, since a DN2 FM TONE
// sound holds the DN1's bytes at new offsets (DNX soundmap.ts: no rescaling). Pure: no DOM, no dependencies; the ZIP's
// deflate uses the browser's DecompressionStream, else node:zlib.
//
// A .dn2pst is a ZIP of manifest.json ({FormatVersion, ProductType, Payload, FileType "Sound", FirmwareVersion,
// MetaInfo.Tags}) and the payload named by Payload: Elektron's stored-file container (31-byte header, the body raw or as
// an LZ4 block chain, a 12-byte trailer) around a 364-byte body: u32be record version, one 0 byte, then the 359-byte
// sound record (BEEFBACE, u32be version, u32be tag bits, the name at 12, then the parameters as u16be at 28 + 2 x id).
//
//   parseDn2pst(bytes, {anyMachine}) -> Promise<{name, tags, tagBits, fizzTags, machine, machineName, filterMachine,
//                                       filterMachineName, version, firmware, params, confidence, warnings, record}>
//       bytes: a .dn2pst, or its payload (a stored container, as a +Drive read gives it), or the 364-byte body, or a
//       bare 359-byte record (a SysEx dump's or a project pool's). Throws on anything else, on a Digitone 1 sound and
//       (unless anyMachine) on a sound that is not FM TONE: WAVETONE, FM DRUM, SWARMER and MIDI have no FM TONE patch.
//   dn2pstToQuad(parsed) -> {blob: Uint8Array(96) FM TONE version 5, values, name (FIZZ's 12 letters), drive, pan,
//                            sends: {chorus, delay, reverb}, notes: [what was lost / approximated],
//                            E1: ext (FIZZ's values 74..109), six (one of them away from its default), blob6:
//                            Uint8Array(128) version 6 (the values, DRIVE, ext; fizz_sysex.mjs sends it as STORE6)}
//   dn2RecordFromParams(params, opts) -> a 359-byte record (the self-test's synthetic sound; a writer would start here)
//   wrapDn2Record(record, {compress}) -> the stored container (payload) around it; dn2pstZip(payload, manifest) -> a .dn2pst

// ---- FM TONE's patch (eng_quad.c QP_*, version 5; the same table as fm1dnsnd.js DN_QUAD_RANGES)
export const QUAD_RANGES = [[1,8,1],[0,18,3],[0,63,3],[0,18,3],[-2600,2600,0],[0,12700,0],[0,127,0],[-64,63,-64],[-100,100,0],[-100,100,0],[-100,100,0],[-100,100,0],[0,127,0],[0,127,32],[0,127,127],[0,127,0],[0,127,0],[0,127,32],[0,127,127],[0,127,0],[0,127,0],[0,1,1],[0,1,1],[0,4,1],[0,127,0],[0,1,1],[0,1,1],[0,127,64],[0,127,0],[0,127,0],[0,127,0],[0,127,64],[0,127,0],[0,127,64],[0,12700,12700],[0,12700,0],[0,7,1],[-6400,6399,0],[0,127,0],[0,127,0],[0,12700,0],[0,127,127],[0,127,8],[0,127,32],[0,127,96],[0,127,24],[0,127,110],[-6400,6399,3200],[0,23,5],[-64,63,0],[0,63,0],[0,6,0],[0,127,0],[0,4,0],[-6400,6399,0],[-6400,6399,3200],[0,23,5],[-64,63,0],[0,63,0],[0,6,0],[0,127,0],[0,4,0],[-6400,6399,0],[-6400,6399,3200],[0,23,5],[-64,63,0],[0,63,0],[0,6,0],[0,127,0],[0,4,0],[-6400,6399,0],[0,18,3],[0,127,0]];
export const QP = { ALGO: 0, RC: 1, RA: 2, RB1: 3, HARM: 4, DTUN: 5, FDBK: 6, MIX: 7, OFSC: 8, OFSA: 9, OFSB1: 10, OFSB2: 11,
  AATK: 12, ADEC: 13, AEND: 14, ALEV: 15, BATK: 16, BDEC: 17, BEND: 18, BLEV: 19, ADLY: 20, ATRIG: 21, ARST: 22, PHRT: 23,
  BDLY: 24, BTRIG: 25, BRST: 26, VEL: 27, AKEY: 28, B1KEY: 29, FATK: 30, FDEC: 31, FSUS: 32, FREL: 33, FREQ: 34, RESO: 35,
  FTYPE: 36, FDEPTH: 37, FDLY: 38, FKTRK: 39, BASE: 40, WIDTH: 41, EATK: 42, EDEC: 43, ESUS: 44, EREL: 45, LEVEL: 46,
  RB2: 71, B2KEY: 72 };
const QL = { SPEED: 0, MULT: 1, FADE: 2, DEST: 3, WAVE: 4, PHASE: 5, TRIG: 6, DEPTH: 7 };
const qlfo = (k, f) => 47 + k * 8 + QL[f];
const QUAD_NP = 73, QUAD_BLOB = 96, QUAD_VER = 5;
// E1: FIZZ's values 74..109 (core/voice/quad/quad_voice.c qe_val): [min, max, default]; the default today's sound
export const QUAD_EXT_RANGES = [[0,1,0],[0,127,127],[0,2,0],[0,127,32],[0,1,0],[0,1,0],[0,127,0],[-64,63,0],[0,1,0],
  [0,127,0],[0,127,0],[0,1,1], ...Array.from({ length: 8 }, () => [[0,8,0],[0,63,0],[-127,127,0]]).flat()];   // (MOD DEST: 64, E4)
export const QX = { AMOD: 74, HOLD: 75, PORT: 76, PTIM: 77, PSLOPE: 78, DRPRE: 79, FSHAPE: 80, FGAIN: 81, BWPOST: 82,
  BITS: 83, SRR: 84, CRPOST: 85, MOD0: 86 };
const QUAD_EXT0 = 74, QUAD_BLOB6 = 128;
const QUAD_HIRES = [4, 5, 34, 35, 37, 40, 47, 54, 55, 62, 63, 70];
const QF = { OFF: 0, LP12: 1, HP12: 2, LP24: 3, MULTI: 4, COMBP: 5, COMBN: 6, EQ: 7 };   // (E2: the machines 4..7)
// FM TONE's LFO destinations (eng_quad.c QD_*; fm1dnsnd.js DN_QD)
const QD = { NONE: 0, PITCH: 1, PAB2: 2, ALGO: 3, RC: 4, RA: 5, RB: 6, OFSC: 7, OFSA: 8, OFSB1: 9, OFSB2: 10, HARM: 11, DTUN: 12,
  FDBK: 13, MIX: 14, ALEV: 15, BLEV: 16, AATK: 17, ADEC: 18, AEND: 19, BATK: 20, BDEC: 21, BEND: 22, ADLY: 23, BDLY: 24, FREQ: 25,
  RESO: 26, FENV: 27, BASE: 28, WIDTH: 29, FATK: 30, FDEC: 31, FSUS: 32, FREL: 33, EATK: 34, EDEC: 35, ESUS: 36, EREL: 37, LEVEL: 38,
  PAN: 39, L1SPD: 40, L1MUL: 41, L1FAD: 42, L1WAV: 43, L1SPH: 44, L1MOD: 45, L1DEP: 46, RALL: 47, ABLEV: 48, ABATK: 49, ABDEC: 50,
  ABEND: 51, ABDLY: 52, DRIVE: 53, SREV: 54, SDLY: 55, SCHO: 56, FDLY: 57,
  HOLD: 58, PTIM: 59, FSHAPE: 60, FGAIN: 61, BITS: 62, SRR: 63 };   // (E4: FIZZ's own)

// ---- the Digitone II's tables
// the machine byte at record +244 and the filter machine at +245 (DNX machine.ts; syxgrid TRACK_MACHINE_REL 232 and
// PRESET_FLTR_MACHINE_REL 233 from the name, = record +244 / +245)
export const DN2_MACHINES = ["FM TONE", "WAVETONE", "FM DRUM", "SWARMER", "MIDI"];
export const DN2_FILTERS = ["MULTI-MODE", "LOWPASS 4", "EQUALIZER", "COMB-", "LEGACY LP/HP", "COMB+"];
// the 32 tag bits at +8, bit t = tag t (DNX tags.ts; Transfer's manifests spell ATMOSPHERIC: syxgrid SOUND_TAGS)
export const DN2_TAGS = ["KICK", "SNARE", "DEEP", "BRASS", "STRINGS", "PERCUSSION", "HI-HAT", "CYMBAL", "EVOLVING", "EXPRESSIVE",
  "BASS", "LEAD", "PAD", "TEXTURE", "CHORD", "SOUND FX", "ARPEGGIO", "METALLIC", "ACOUSTIC", "ATMOSPHERIC", "NOISY", "GLITCH",
  "HARD", "SOFT", "DARK", "BRIGHT", "VINTAGE", "EPIC", "FAIL", "LOOP", "MINE", "FAVOURITE"];
// a Digitone tag -> FIZZ's preset kind (core/seq/sounds.c: BASS KEYS PLUCK PAD LEAD DRUMS FX). A judgement, not a fact
// of the format; no Digitone tag says PLUCK or KEYS outright (CHORD is taken as KEYS)
export const FIZZ_KINDS = ["BASS", "KEYS", "PLUCK", "PAD", "LEAD", "DRUMS", "FX"];
const TAG_TO_FIZZ = { BASS: "BASS", CHORD: "KEYS", PAD: "PAD", LEAD: "LEAD", KICK: "DRUMS", SNARE: "DRUMS", PERCUSSION: "DRUMS",
  "HI-HAT": "DRUMS", CYMBAL: "DRUMS", "SOUND FX": "FX" };
// RATIO C / B1 / B2 (19 steps) and RATIO A (36 steps): the firmware's own strings (syxgrid PLOCK_STRTAB 503.. and
// 522..). RATIO A: quarters to 5, halves to 10, then whole numbers to 16. Every proved DN1 step (fm1dnsnd.js
// DN_RATIO_A_ANCHORS 0 3 9 15 25 35) lands on it; all 36 are quarters, so FM TONE's RATIO A (0.25..16 in quarters) holds
// each one exactly
export const DN2_RATIO_CB = [0.25, 0.5, 0.75, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16];
export const DN2_RATIO_A = Array.from({ length: 36 }, (_, i) => i < 20 ? (i + 1) / 4 : i < 30 ? 5 + (i - 19) / 2 : i - 19);
const DN2_WAVE_TO_QUAD = [0, 1, 2, 3, 5, 4, 6];        // TRI SINE SQR SAW EXPO RAMP RAND (firmware) -> FM TONE's order
// an LFO DESTINATION is a parameter id (DNX soundmap.ts SELECTOR_TABLE carries DN1's numbers onto exactly these ids) ->
// FM TONE's destination. LFO 2's own ids (2, 6, .. 30, LFO 3 aiming at LFO 2) and the ids FM TONE lacks: off, noted
export const DN2_LFO_DEST = { 0: QD.NONE, 1: QD.L1SPD, 5: QD.L1MUL, 9: QD.L1FAD, 17: QD.L1WAV, 21: QD.L1SPH, 25: QD.L1MOD,
  29: QD.L1DEP, 33: QD.ALGO, 34: QD.RC, 35: QD.RA, 36: QD.RB, 37: QD.HARM, 38: QD.DTUN, 39: QD.FDBK, 40: QD.MIX, 43: QD.AATK,
  44: QD.ADEC, 45: QD.AEND, 46: QD.ALEV, 47: QD.BATK, 48: QD.BDEC, 49: QD.BEND, 50: QD.BLEV, 51: QD.ADLY, 54: QD.BDLY,
  58: QD.PITCH, 59: QD.PAB2, 60: QD.RALL, 61: QD.ABLEV, 62: QD.ABATK, 63: QD.ABDEC, 64: QD.ABEND, 65: QD.ABDLY, 66: QD.OFSC,
  67: QD.OFSA, 68: QD.OFSB1, 69: QD.OFSB2, 74: QD.FREQ, 75: QD.RESO, 76: QD.FENV, 77: QD.FDLY, 78: QD.FATK, 79: QD.FDEC,
  80: QD.FSUS, 81: QD.FREL, 83: QD.BASE, 84: QD.WIDTH, 87: QD.EATK, 89: QD.EDEC, 90: QD.ESUS, 91: QD.EREL, 92: QD.SCHO,
  93: QD.SDLY, 94: QD.SREV, 95: QD.PAN, 96: QD.LEVEL, 104: QD.DRIVE,
  73: QD.FSHAPE, 88: QD.HOLD, 100: QD.PTIM, 101: QD.BITS, 102: QD.SRR };   // (E4: FIZZ's destinations for the DN2's own)
// the names of the ids (DNX plockparams.ts / machineplock.ts, syxgrid PLOCK_NAMES), for the notes
const DN2_ID_NAMES = { 2: "LFO2 SPD", 6: "LFO2 MULT", 10: "LFO2 FADE", 14: "LFO2 DEST", 18: "LFO2 WAVE", 22: "LFO2 SPH",
  26: "LFO2 MODE", 30: "LFO2 DEP", 41: "PHRT", 52: "A TRIG", 53: "A RESET", 55: "B TRIG", 56: "B RESET", 70: "KEY TRACK A",
  71: "KEY TRACK B1", 72: "KEY TRACK B2", 73: "FILTER TYPE", 82: "FILTER KEY TRACK", 85: "FILTER RESET", 86: "AMP DELAY",
  88: "AMP HOLD", 97: "AMP MODE", 98: "AMP RESET", 99: "PORTAMENTO", 100: "PORTAMENTO TIME", 101: "BIT REDUCTION",
  102: "SAMPLE-RATE REDUX", 103: "SRR ROUTING", 105: "BW ROUTING", 106: "OVERDRIVE ROUTING" };

// ---- the record's parameters: [key, offset, hi-res, confidence, decode(d, o) -> the Digitone's value, apply(v, out)]
// offset = 28 + 2 x id (DNX soundparams.ts measured them; issue #94 states the rule). confidence: "confirmed" (the offset
// measured on a DN2 by DNX, or a DN1 byte copied there by Elektron's own conversions, DNX soundmap.ts, with the DN1's
// law proved by fm1dnsnd.js's controlled exports), "fw" (offset and law from the firmware's parameter tables, read by
// syxgrid; no export behind it), "inferred", "guess"; "info" fields are decoded for the caller but not imported
const lo = (d, o) => d[o];
const hi = (d, o) => (d[o] * 100 + Math.round(d[o + 1] * 100 / 256)) / 100;   // coarse + fine / 256, in hundredths
const R = Math.round;
const to = (qp, f = (v) => R(v)) => (v, out) => out.set(qp, f(v));
const to100 = (qp) => to(qp, (v) => R(v * 100));
const ofs = (qp) => (v, out) => out.set(qp, R(v * 100 / 64));                // ratio offset: -64..63.99 = -1.00..+0.99
// MULT: four runs of six (syxgrid's H46 read 0..11 off a DN2: 0..5 BPM x1..x32, 6..11 the same at 120 BPM; 12..23 by
// the manual's range: BPM x64..x2k, then fixed); FM TONE: 0..11 x1..x2k at the BPM, 12..23 the same at 120 (eng_quad.c)
export const multToQuad = (n) => { const f = Math.floor(n / 6); return (n % 6) + 6 * (f >> 1) + 12 * (f & 1); };
const lfo = (k) => [
  [`lfo${k + 1}.speed`, 30 + 2 * k, 1, "confirmed", (d, o) => hi(d, o) - 64, to100(qlfo(k, "SPEED"))],
  [`lfo${k + 1}.mult`, 38 + 2 * k, 0, "fw", lo, to(qlfo(k, "MULT"), multToQuad)],
  [`lfo${k + 1}.fade`, 46 + 2 * k, 0, "confirmed", (d, o) => d[o] - 64, to(qlfo(k, "FADE"))],
  [`lfo${k + 1}.dest`, 54 + 2 * k, 0, "confirmed", lo, (v, out) => {
    let q = DN2_LFO_DEST[v];
    if (k === 0 && q >= QD.L1SPD && q <= QD.L1DEP) q = undefined;            // LFO 1 on itself: FM TONE has none
    out.set(qlfo(k, "DEST"), q ?? QD.NONE);
    if (q === undefined) out.note(`LFO ${k + 1} destination ${DN2_ID_NAMES[v] ?? `#${v}`}: FM TONE has none, off`);
  }],
  [`lfo${k + 1}.wave`, 62 + 2 * k, 0, "confirmed", lo, (v, out) => out.set(qlfo(k, "WAVE"), DN2_WAVE_TO_QUAD[v] ?? 0)],
  [`lfo${k + 1}.phase`, 70 + 2 * k, 0, "confirmed", lo, to(qlfo(k, "PHASE"))],
  [`lfo${k + 1}.mode`, 78 + 2 * k, 0, "confirmed", lo, to(qlfo(k, "TRIG"))],
  // DEPTH: the DN1's bytes (DNX copies them unscaled); the DN2 shows them doubled, -128.00..127.99 (syxgrid law 6)
  [`lfo${k + 1}.depth`, 86 + 2 * k, 1, "confirmed", (d, o) => hi(d, o) - 64, to100(qlfo(k, "DEPTH"))],
];
const env = (x, base) => ["atk", "dec", "end", "lev"].map((f, i) => [`env${x}.${f}`, base + 2 * i, 0, "confirmed", lo,
  to(QP[`${x}${f.toUpperCase()}`])]);
export const DN2_FIELDS = [
  ...lfo(0), ...lfo(1), ...lfo(2),
  ["algo", 94, 0, "confirmed", (d, o) => d[o] + 1, to(QP.ALGO)],                              // id 33: 0..7 -> 1..8
  ["ratioC", 96, 0, "confirmed", (d, o) => DN2_RATIO_CB[Math.min(18, d[o])], (v, out) => out.set(QP.RC, Math.max(0, DN2_RATIO_CB.indexOf(v)))],
  ["ratioA", 98, 0, "confirmed", (d, o) => DN2_RATIO_A[Math.min(35, d[o])], (v, out) => out.set(QP.RA, R(v * 4) - 1)],
  // RATIO B (id 36): u16be = 2 x (B1 step x 19 + B2 step) + a low bit (fm1dnsnd.js; syxgrid's firmware "rpair" law
  // disagrees about B1 and is marked derived there)
  ["ratioB", 100, 1, "confirmed", (d, o) => Math.min(360, (d[o] << 8 | d[o + 1]) >> 1), (v, out) => {
    out.set(QP.RB1, Math.floor(v / 19)); out.set(QP.RB2, v % 19);
  }],
  ["harm", 102, 1, "confirmed", (d, o) => hi(d, o) - 63, to100(QP.HARM)],                    // centre 63: -26..+26
  ["dtun", 104, 1, "confirmed", hi, to100(QP.DTUN)],
  ["fdbk", 106, 0, "confirmed", lo, to(QP.FDBK)],
  ["mix", 108, 0, "confirmed", (d, o) => d[o] - 64, to(QP.MIX)],
  ["phrt", 110, 0, "confirmed", lo, to(QP.PHRT)],                                            // OFF ALL C A+B1+B2 A+B2
  ...env("A", 114), ...env("B", 122),
  ["envA.delay", 130, 0, "confirmed", lo, to(QP.ADLY)],
  ["envA.trig", 132, 0, "confirmed", lo, to(QP.ATRIG)],
  ["envA.reset", 134, 0, "confirmed", lo, to(QP.ARST)],
  ["envB.delay", 136, 0, "confirmed", lo, to(QP.BDLY)],
  ["envB.trig", 138, 0, "confirmed", lo, to(QP.BTRIG)],
  ["envB.reset", 140, 0, "confirmed", lo, to(QP.BRST)],
  ["unk142", 142, 0, "info", lo, null],                       // id 57: DN1 +110's byte (fm1dnsnd.js unk110); FM TONE: no id
  ["ofsC", 160, 1, "confirmed", (d, o) => hi(d, o) - 64, ofs(QP.OFSC)],                      // ids 66..69 RATIO OFFSET
  ["ofsA", 162, 1, "confirmed", (d, o) => hi(d, o) - 64, ofs(QP.OFSA)],
  ["ofsB1", 164, 1, "confirmed", (d, o) => hi(d, o) - 64, ofs(QP.OFSB1)],
  ["ofsB2", 166, 1, "confirmed", (d, o) => hi(d, o) - 64, ofs(QP.OFSB2)],
  ["keyA", 168, 0, "inferred", lo, to(QP.AKEY)],                                             // ids 70..72 KEY TRACK
  ["keyB1", 170, 0, "inferred", lo, to(QP.B1KEY)],
  ["keyB2", 172, 0, "inferred", lo, to(QP.B2KEY)],
  ["filter.type", 174, 0, "fw", lo, null],                                                   // id 73: per machine
  ["filter.freq", 176, 1, "confirmed", hi, to100(QP.FREQ)],
  ["filter.reso", 178, 1, "confirmed", hi, to100(QP.RESO)],
  ["filter.env", 180, 1, "confirmed", (d, o) => hi(d, o) - 64, to100(QP.FDEPTH)],
  ["filter.delay", 182, 0, "confirmed", lo, to(QP.FDLY)],
  ["filter.atk", 184, 0, "confirmed", lo, to(QP.FATK)],
  ["filter.dec", 186, 0, "confirmed", lo, to(QP.FDEC)],                                      // 127 = INF (dn2pstToQuad)
  ["filter.sus", 188, 0, "confirmed", lo, to(QP.FSUS)],
  ["filter.rel", 190, 0, "confirmed", lo, to(QP.FREL)],                                      // 127 = INF = FM TONE's
  // KEY.T 0..100 % (syxgrid "pct"); FM TONE's KTRK 0..127: the scale is a guess
  ["filter.keytrack", 192, 0, "guess", lo, to(QP.FKTRK, (v) => R(Math.min(100, v) * 127 / 100))],
  ["filter.base", 194, 1, "confirmed", hi, to100(QP.BASE)],
  ["filter.width", 196, 0, "confirmed", lo, to(QP.WIDTH)],
  ["filter.reset", 198, 0, "fw", lo, null],
  ["amp.atk", 202, 0, "confirmed", lo, to(QP.EATK)],
  ["amp.hold", 204, 0, "confirmed", lo, null],                                               // 127 = NOTE (dn2pstToQuad)
  ["amp.dec", 206, 0, "confirmed", lo, null],
  ["amp.sus", 208, 0, "confirmed", lo, null],
  ["amp.rel", 210, 1, "confirmed", hi, null],
  ["send.chorus", 212, 1, "confirmed", hi, null],
  ["send.delay", 214, 1, "confirmed", hi, null],
  ["send.reverb", 216, 1, "confirmed", hi, null],
  ["amp.pan", 218, 0, "confirmed", (d, o) => d[o] - 64, (v, out) => { out.pan = Math.max(-64, Math.min(63, v)); }],
  ["amp.vol", 220, 1, "confirmed", hi, to(QP.LEVEL)],
  ["amp.mode", 222, 0, "confirmed", lo, null],                                               // 0 AHD, 1 ADSR
  ["amp.reset", 224, 0, "confirmed", lo, null],
  ["portamento", 226, 0, "fw", lo, null],
  ["portamento.time", 228, 0, "fw", lo, null],
  ["fx.bitreduction", 230, 0, "confirmed", lo, null],
  ["fx.srr", 232, 0, "confirmed", lo, null],
  ["fx.srrRouting", 234, 0, "fw", lo, null],
  ["fx.overdrive", 236, 1, "confirmed", hi, (v, out) => { out.drive = Math.max(0, Math.min(127, R(v))); }],
  ["fx.bwRouting", 238, 0, "fw", lo, null],
  ["fx.odRouting", 240, 0, "fw", lo, null],
  ["machine", 244, 0, "confirmed", lo, null],
  ["filter.machine", 245, 0, "confirmed", lo, null],
  ["octave", 251, 0, "inferred", (d, o) => (d[o] << 24) >> 24, null],                       // DN1 v5 +280's byte
];

// ---- the container (research/DN2-FORMAT.md "Container")
export const DN2_RECORD = 359, DN2_BODY = 364;
const HDR = 31, TRL = 12;
const NOT = "not a Digitone II preset";
const u16le = (b, o) => b[o] | b[o + 1] << 8, u32le = (b, o) => (b[o] | b[o + 1] << 8 | b[o + 2] << 16 | b[o + 3] << 24) >>> 0;
const u32be = (b, o) => (b[o] << 24 | b[o + 1] << 16 | b[o + 2] << 8 | b[o + 3]) >>> 0;
const be32 = (v) => [v >>> 24 & 255, v >>> 16 & 255, v >>> 8 & 255, v & 255];
const isMagic = (b, o, m) => o + 4 <= b.length && b[o] === m[0] && b[o + 1] === m[1] && b[o + 2] === m[2] && b[o + 3] === m[3];
const BEEFBACE = [0xbe, 0xef, 0xba, 0xce], CONTAINER = [0xac, 0x11, 0xd3, 0x03], FOOTER = [0xaa, 0xa1, 0xda, 0xaa];

// CRC-32 with a zero initial register (reflected, poly EDB88320, xorout FFFFFFFF): the trailer's check over the bytes
// from 0x1F to the trailer (DNX checksum.ts). crc32(.., 0xffffffff) is the ordinary ZIP CRC-32
const CRC_T = Array.from({ length: 256 }, (_, n) => { let c = n; for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1; return c >>> 0; });
export function crc32(b, init = 0) {
  let c = init >>> 0;
  for (let i = 0; i < b.length; i++) c = CRC_T[(c ^ b[i]) & 255] ^ (c >>> 8);
  return (c ^ 0xffffffff) >>> 0;
}
async function inflateRaw(bytes) {
  if (typeof DecompressionStream !== "undefined") {
    const s = new Blob([bytes]).stream().pipeThrough(new DecompressionStream("deflate-raw"));
    return new Uint8Array(await new Response(s).arrayBuffer());
  }
  const zlib = await import("node:zlib");
  return new Uint8Array(zlib.inflateRawSync(bytes));
}
// the ZIP's entries (central directory): name -> Promise<bytes> (as fm1dnsnd.js dnReadZip)
async function readZip(b) {
  let e = -1;
  for (let i = b.length - 22; i >= Math.max(0, b.length - 22 - 65535); i--) if (u32le(b, i) === 0x06054b50) { e = i; break; }
  if (e < 0) throw new Error(`${NOT} (not a ZIP archive)`);
  const n = u16le(b, e + 10), out = new Map();
  let p = u32le(b, e + 16);
  for (let k = 0; k < n; k++) {
    if (p + 46 > b.length || u32le(b, p) !== 0x02014b50) throw new Error(`${NOT} (a damaged ZIP directory)`);
    const method = u16le(b, p + 10), cs = u32le(b, p + 20), nl = u16le(b, p + 28), xl = u16le(b, p + 30), cl = u16le(b, p + 32), lh = u32le(b, p + 42);
    const name = new TextDecoder().decode(b.subarray(p + 46, p + 46 + nl));
    if (lh + 30 > b.length || u32le(b, lh) !== 0x04034b50) throw new Error(`${NOT} (a damaged ZIP entry ${name})`);
    const start = lh + 30 + u16le(b, lh + 26) + u16le(b, lh + 28), data = b.subarray(start, start + cs);
    if (data.length !== cs) throw new Error(`${NOT} (the ZIP entry ${name} is cut short)`);
    if (method !== 0 && method !== 8) throw new Error(`${NOT} (ZIP method ${method})`);
    out.set(name, { data, deflated: method === 8 });
    p += 46 + nl + xl + cl;
  }
  return { names: [...out.keys()], get: async (name) => { const d = out.get(name); return d && (d.deflated ? inflateRaw(d.data) : d.data.slice()); } };
}
// the LZ4 block chain from src[p]: u32be compressed size, the block, .., u32be 0. The blocks share one dictionary
// (DNX dn2codec.ts), so all decode into one buffer. -> {data, end}
export function lz4Chain(src, p, want) {
  const o = new Uint8Array(want); let n = 0;
  for (;;) {
    if (p + 4 > src.length) throw new Error(`${NOT} (the sound data is cut short)`);
    const size = u32be(src, p); p += 4;
    if (!size) break;
    const end = p + size;
    if (end > src.length) throw new Error(`${NOT} (the sound data is cut short)`);
    const byte = () => { if (p >= end) throw new Error(`${NOT} (the sound data does not decompress)`); return src[p++]; };
    while (p < end) {
      const t = byte(); let l = t >> 4;
      if (l === 15) { let b; do { b = byte(); l += b; } while (b === 255); }
      if (n + l > want) throw new Error(`${NOT} (the sound data does not decompress)`);
      for (let i = 0; i < l; i++) o[n++] = byte();
      if (p >= end) break;                                     // the block's last sequence: literals only
      const off = byte() | byte() << 8;
      let m = (t & 15) + 4;
      if ((t & 15) === 15) { let b; do { b = byte(); m += b; } while (b === 255); }
      if (!off || off > n || n + m > want) throw new Error(`${NOT} (the sound data does not decompress)`);
      for (let i = 0; i < m; i++, n++) o[n] = o[n - off];
    }
  }
  if (n !== want) throw new Error(`${NOT} (the sound data decompresses to ${n} bytes, not ${want})`);
  return { data: o, end: p };
}
// a stored container -> {body, header fields, warnings}
export function readContainer(pl, warnings = []) {
  if (pl.length < HDR + TRL || !isMagic(pl, 0, CONTAINER)) throw new Error(`${NOT} (the payload's magic)`);
  const h = { headerVersion: pl[4], family: pl[5] << 8 | pl[6], device: pl[7] << 8 | pl[8],
    build: String.fromCharCode(...pl.subarray(9, 13)), kind: u32be(pl, 13), objectVersion: u32be(pl, 17),
    index: u32be(pl, 21), length: u32be(pl, 25), compressed: pl[29] !== 0, trailer: pl[30] };
  if (h.device === 9) throw new Error("a Digitone 1 sound (device 9): read it with the .dnsnd reader");
  if (h.device !== 15) warnings.push(`the container's device is ${h.device}, not 15 (Digitone II): read as one`);
  if (h.kind !== 3) throw new Error(`${NOT} (the container holds content kind ${h.kind}, not 3: a sound)`);
  let body, end;
  if (h.compressed) ({ data: body, end } = lz4Chain(pl, HDR, h.length));
  else { body = pl.slice(HDR, HDR + h.length); end = HDR + h.length; if (body.length !== h.length) throw new Error(`${NOT} (the body is cut short)`); }
  if (pl.length - end !== TRL || !isMagic(pl, pl.length - 4, FOOTER)) warnings.push("the payload's trailer is not the usual 12 bytes ending AA A1 DA AA");
  else {
    if (u32be(pl, pl.length - 12) !== crc32(pl.subarray(HDR, pl.length - TRL))) warnings.push("the payload's check field does not match (CRC-32, zero seed): read anyway");
    if (u32be(pl, pl.length - 8) !== pl.length - 43) warnings.push("the trailer's length field is not the payload's length - 43");
  }
  return { body, header: h };
}

// Windows-1252 -> text (the name: DNX device-storage.md)
const CP1252 = "€\u0081‚ƒ„…†‡ˆ‰Š‹Œ\u008dŽ\u008f\u0090‘’“”•–—˜™š›œ\u009džŸ";
const cp1252 = (bytes) => Array.from(bytes, (c) => c >= 0x80 && c < 0xa0 ? CP1252[c - 0x80] : String.fromCharCode(c)).join("");
// FIZZ's name: 12 letters of ASCII 0x20..0x7E (docs/SYSEX.md), accents folded (EÅ -> EA)
export const fizzName = (s, fallback = "DIGITONE II") => {
  const a = String(s || "").normalize("NFD").replace(/[̀-ͯ]/g, "").replace(/[^\x20-\x7e]/g, "?").trim().slice(0, 12).trimEnd();
  return a || fallback;
};

// ---- the sound
export async function parseDn2pst(bytes, opts = {}) {
  if (!(bytes instanceof Uint8Array)) bytes = Uint8Array.from(bytes || []);
  const warnings = [];
  let man = null, rec, header = null, body = null;
  if (bytes.length >= 4 && u32le(bytes, 0) === 0x04034b50) {                       // a .dn2pst (ZIP)
    const zip = await readZip(bytes), mt = await zip.get("manifest.json");
    if (!mt) throw new Error(`${NOT} (no manifest.json)`);
    try { man = JSON.parse(new TextDecoder().decode(mt)); } catch (_) { throw new Error(`${NOT} (manifest.json is not JSON)`); }
    if (!man || (man.FileType !== "Sound" && man.FileType !== "Preset")) throw new Error(`${NOT} (FileType ${man && JSON.stringify(man.FileType)}, not "Sound")`);
    if (man.FileType !== "Sound") warnings.push(`FileType "${man.FileType}": read as a sound`);
    const pname = typeof man.Payload === "string" && zip.names.includes(man.Payload) ? man.Payload : zip.names.find((n) => n !== "manifest.json");
    if (pname !== man.Payload) warnings.push(`the payload ${JSON.stringify(man.Payload)} is not in the ZIP: read ${JSON.stringify(pname)}`);
    const pl = pname && await zip.get(pname);
    if (!pl) throw new Error(`${NOT} (no payload)`);
    ({ body, header } = readContainer(pl, warnings));
  } else if (isMagic(bytes, 0, CONTAINER)) ({ body, header } = readContainer(bytes, warnings));   // a +Drive read
  else body = bytes;                                                               // a body or a bare record
  if (body.length >= 4 && body[0] === 0x44 && body[1] === 0x4e && body[2] === 0x31 && body[3] === 0x53)
    throw new Error("a Digitone 1 sound kept on a Digitone II (its body opens \"DN1S\"): load it on the device once to convert it, or import the .dnsnd");
  if (isMagic(body, 5, BEEFBACE) && body.length === DN2_BODY) {
    rec = body.slice(5);
    if (u32be(body, 0) !== u32be(rec, 4) || body[4] !== 0) warnings.push("the body's 5-byte prefix is not the record's version and a 0");
  } else if (isMagic(body, 0, BEEFBACE)) {
    if (body.length === 302) throw new Error("a Digitone 1 sound record (302 bytes): read it with the .dnsnd reader");
    rec = body.slice(0, DN2_RECORD);
  } else throw new Error(`${NOT} (no BEEFBACE in a ${body.length}-byte body)`);
  if (rec.length !== DN2_RECORD) throw new Error(`${NOT} (a ${rec.length}-byte record, not ${DN2_RECORD})`);
  const version = u32be(rec, 4);
  if (version < 1 || version > 3) throw new Error(`a newer Digitone II sound (record version ${version}): not supported`);
  if (version !== 2) warnings.push(`record version ${version}: read with version 2's layout (${version === 3 ? "OS 1.11's version; " : ""}not verified on a real file)`);
  if (!isMagic(rec, 355, [0xba, 0xce, 0xf0, 0x0c])) warnings.push("the record does not end BA CE F0 0C (DNX soundmap.ts DEFAULT_BYTES)");
  if (header && header.objectVersion !== version) warnings.push(`the container says version ${header.objectVersion}, the record ${version}`);
  let n = 0;
  while (n < 16 && rec[12 + n]) n++;
  const name = cp1252(rec.subarray(12, 12 + n));
  const tagBits = u32be(rec, 8), bitTags = DN2_TAGS.filter((_, t) => (tagBits >>> t) & 1);
  const manTags = man && man.MetaInfo && Array.isArray(man.MetaInfo.Tags) ? man.MetaInfo.Tags.map(String) : null;
  if (manTags && manTags.join() !== bitTags.join()) warnings.push(`the manifest's tags (${manTags.join(", ") || "none"}) are not the record's (${bitTags.join(", ") || "none"}): the record's kept`);
  const tags = bitTags;
  const fizzTags = FIZZ_KINDS.filter((k) => tags.some((t) => TAG_TO_FIZZ[t] === k));
  const machine = rec[244], filterMachine = rec[245];
  const machineName = DN2_MACHINES[machine] ?? `machine ${machine}`, filterMachineName = DN2_FILTERS[filterMachine] ?? `filter ${filterMachine}`;
  const res = { name, tags, tagBits, fizzTags, machine, machineName, filterMachine, filterMachineName, version,
    firmware: man ? man.FirmwareVersion ?? null : null, product: man ? man.ProductType ?? null : null,
    container: header, params: null, confidence: null, warnings, record: rec };
  if (machine !== 0) {
    if (opts.anyMachine) return res;
    const e = new Error(`"${name}" is a ${machineName} sound: FIZZ imports FM TONE sounds only`);
    e.code = "DN2_MACHINE"; e.machine = machineName;
    throw e;
  }
  res.params = {}; res.confidence = {};
  for (const [k, o, , c, dec] of DN2_FIELDS) { res.params[k] = +dec(rec, o).toFixed(2); res.confidence[k] = c; }
  return res;
}

// the sound's OCTAVE into the four ratios (as fm1dnsnd.js dnOctave): each ratio x 2^(oct / 12) as a step + OFS
const RA_Q = Array.from({ length: 64 }, (_, i) => (i + 1) / 4);
function octave(p, oct) {
  const f = 2 ** (oct / 12);
  const ops = [[DN2_RATIO_CB, QP.RC, QP.OFSC], [RA_Q, QP.RA, QP.OFSA], [DN2_RATIO_CB, QP.RB1, QP.OFSB1], [DN2_RATIO_CB, QP.RB2, QP.OFSB2]];
  const set = [];
  for (const [tab, qs, qo] of ops) {
    const want = (tab[p[qs]] + p[qo] / 100) * f;
    let best = null;
    tab.forEach((x, i) => {
      const o = Math.max(-100, Math.min(100, R((want - x) * 100))), got = x + o / 100, e = Math.abs(got - want) / want;
      if (got > 0 && e <= 0.003 && (!best || Math.abs(o) < Math.abs(best[1]) || (Math.abs(o) === Math.abs(best[1]) && e < best[2]))) best = [i, o, e];
    });
    if (!best) return false;
    set.push([qs, qo, best[0], best[1]]);
  }
  for (const [qs, qo, i, o] of set) { p[qs] = i; p[qo] = o; }
  return true;
}

// the parsed sound -> FM TONE's blob, FIZZ's name and DRIVE, the track's PAN and sends, and what was lost
export function dn2pstToQuad(parsed) {
  if (!parsed || parsed.machine !== 0 || !parsed.params) throw new Error(`"${parsed && parsed.name}" is a ${parsed && parsed.machineName} sound: FIZZ imports FM TONE sounds only`);
  const P = parsed.params, p = QUAD_RANGES.map((r) => r[2]), notes = [];
  const ext = QUAD_EXT_RANGES.map((r) => r[2]);                                          // (E1: 74..109)
  const xset = (i, v) => { const [mn, mx] = QUAD_EXT_RANGES[i - QUAD_EXT0]; ext[i - QUAD_EXT0] = Math.max(mn, Math.min(mx, v)); };
  const out = { pan: 0, drive: 0, note: (s) => notes.push(s),
    set: (i, v) => { const [mn, mx, df] = QUAD_RANGES[i]; p[i] = Math.max(mn, Math.min(mx, Number.isFinite(v) ? v : df)); } };
  for (const [k, , , c, , apply] of DN2_FIELDS) if (apply && c !== "info") apply(P[k], out);
  // the filter: machine + TYPE (E2: FIZZ's machines: LEGACY OFF / LP12 / HP12, LOWPASS 4, MULTI, COMB+, COMB-, EQ; TYPE
  // their third control, FSHAPE: MULTI's morph, the combs' LPF, EQ's Q; EQ's GAIN is RESO's coarse - 64)
  const fm = parsed.filterMachine, ty = P["filter.type"];
  if (fm === 1) p[QP.FTYPE] = QF.LP24;                                                   // LOWPASS 4
  else if (fm === 4) {                                                                   // LEGACY LP/HP: LP HP OFF (+ 3: DN1's OFF)
    p[QP.FTYPE] = ty === 0 ? QF.LP12 : ty === 1 ? QF.HP12 : QF.OFF;
    if (ty > 3) notes.push(`LEGACY LP/HP type ${ty}: not a known value, the filter off`);
  } else if (fm === 0 || fm === 5 || fm === 3 || fm === 2) {                             // MULTI, COMB+, COMB-, EQ
    p[QP.FTYPE] = fm === 0 ? QF.MULTI : fm === 5 ? QF.COMBP : fm === 3 ? QF.COMBN : QF.EQ;
    xset(QX.FSHAPE, ty | 0);
    if (fm === 2) xset(QX.FGAIN, R(P["filter.reso"] || 0) - 64);
    if (fm === 0 && ty) notes.push(`MULTI-MODE TYPE ${ty} taken 1:1 as FIZZ's morph (the law is not verified)`);
  } else {
    p[QP.FTYPE] = QF.OFF;
    notes.push(`${parsed.filterMachineName} filter: not a known machine, the filter off`);
  }
  if (P["fx.bwRouting"]) xset(QX.BWPOST, 1);                                            // (E2: BW ROUTING 1: after)
  // the Digitone's filter DEC 127 is INF: FM TONE's filter DEC has none: SUS 127 (as dnsndToQuad)
  if (P["filter.dec"] === 127) { p[QP.FSUS] = 127; notes.push("filter DEC INF: FM TONE's filter SUS set to 127"); }
  if (P["filter.reset"] === 0) notes.push("FILTER ENV RESET off not imported");
  // the amp: ADSR as it is; AHD (MODE 0): FIZZ's AHD (E1: AMOD, HOLD 1:1, 127 NOTE; DEC 127 INF, as FIZZ's)
  const adec = P["amp.dec"], rel = P["amp.rel"];
  xset(QX.HOLD, P["amp.hold"]);                       // (HOLD is stored whatever the mode: kept)
  if (P["amp.mode"] === 0) {
    xset(QX.AMOD, 1);
    p[QP.EDEC] = Math.min(127, adec);
    p[QP.ESUS] = P["amp.sus"];                         // (stored though AHD has none: kept, so a switch to ADSR has them)
    p[QP.EREL] = rel >= 127 ? 127 : Math.min(126, R(rel));
  } else {
    p[QP.EDEC] = adec >= 127 ? 127 : Math.min(126, adec);
    p[QP.ESUS] = P["amp.sus"];
    p[QP.EREL] = rel >= 127 ? 127 : Math.min(126, R(rel));
  }
  if (P["amp.reset"] === 0) notes.push("AMP ENV RESET off not imported (FM TONE's amp envelope always resets)");
  if (P["fx.bitreduction"] || P["fx.srr"]) {          // E3's CRUSH values (1:1: the laws are not verified; FIZZ plays them from E3)
    xset(QX.BITS, P["fx.bitreduction"]); xset(QX.SRR, P["fx.srr"]); xset(QX.CRPOST, P["fx.srrRouting"] ? 1 : 0);
    notes.push(`BIT REDUCTION ${P["fx.bitreduction"]}, SAMPLE RATE REDUCTION ${P["fx.srr"]}: taken 1:1 (the laws are not verified)`);
  }
  if (P.portamento === 64) { xset(QX.PORT, 1); xset(QX.PTIM, P["portamento.time"]); }   // (E1: PORT ON, TIME 1:1)
  if (P["fx.odRouting"] === 0) {                       // E1: OD ROUTING 0 read as before the filter (a guess)
    xset(QX.DRPRE, 1);
    if (out.drive) notes.push("OVERDRIVE before the filter (OD ROUTING 0 read as PRE: the encoding is not verified)");
  }
  // the sound's OCTAVE (inferred: record +251, the DN1's +280)
  const oct = P.octave | 0;
  if (oct) {
    notes.push(octave(p, oct) ? `the sound's OCTAVE ${oct > 0 ? "+" : ""}${oct / 12}: every ratio x ${2 ** (oct / 12)}` :
      `the sound's OCTAVE ${oct > 0 ? "+" : ""}${oct / 12} not imported (a ratio has no such value in FM TONE)`);
  }
  {   // E4: the modulation records (+252 + 3k, k < 24: six sources x four, PB VEL MW BR AT KEY; the third byte the
      // destination's id, the first two the depth, bipolar about 64: inferred, not verified) into the eight slots
    const rec = parsed.record, SRC = [5, 1, 3, 6, 4, 2], SRCN = ["PB", "VEL", "MW", "BR", "AT", "KEY"];
    let slot = 0, lost = 0;
    if (rec && rec.length >= 252 + 72)
      for (let k = 0; k < 24; k++) {
        const o = 252 + 3 * k, id = rec[o + 2];
        if (!id) continue;
        const q = DN2_LFO_DEST[id];
        if (q === undefined || q === QD.NONE) { notes.push(`MOD ${SRCN[k >> 2]} -> ${DN2_ID_NAMES[id] ?? `#${id}`}: FIZZ has no such destination`); continue; }
        if (slot >= 8) { lost++; continue; }
        xset(QX.MOD0 + 3 * slot, SRC[k >> 2]);
        xset(QX.MOD0 + 3 * slot + 1, q);
        xset(QX.MOD0 + 3 * slot + 2, Math.round((rec[o] + rec[o + 1] / 256 - 64) * 2));
        slot++;
      }
    if (slot) notes.push(`MOD: ${slot} slot(s) from the Digitone's modulation records (their layout and depth law are inferred, not verified)`);
    if (lost) notes.push(`MOD: ${lost} more record(s) not imported (FIZZ has eight slots)`);
  }
  if (parsed.warnings && parsed.warnings.length) notes.push(...parsed.warnings.map((w) => `file: ${w}`));
  const send = (v) => Math.max(0, Math.min(127, R(v || 0)));
  const six = ext.some((v, k) => v !== QUAD_EXT_RANGES[k][2]);
  return { blob: quadPack(p), values: p, name: fizzName(parsed.name), drive: out.drive, pan: out.pan,
    sends: { chorus: send(P["send.chorus"]), delay: send(P["send.delay"]), reverb: send(P["send.reverb"]) }, notes,
    ext, six, blob6: quadPack6(p, out.drive, ext) };
}

// E1: version 6 (FIZZ's quad_voice.c qd_quad_pack): version 5's bytes, 'Q' 6, DRIVE at 87, the values 74..109 a byte
// each (value - min) at 88..123; 2026-10-26: ext's 36..40 (the VOICE page's 110..114), when given, in 124..127 (zeros:
// their defaults): VOICE | (PRIORITY - 1) & 3 << 1, UNISON - 1, DETUNE - 20, SPREAD - 64 (mod 256)
export const QUAD_VOICE_RANGES = [[0, 1, 0], [1, 8, 1], [0, 127, 20], [0, 127, 64], [0, 2, 1]];   // (110..114)
export function quadPack6(p, drive, ext) {
  const blob = new Uint8Array(QUAD_BLOB6);
  blob.set(quadPack(p).subarray(0, 87));
  blob[1] = 6;
  blob[87] = Math.max(0, Math.min(127, drive | 0));
  ext.forEach((v, k) => { if (k < QUAD_EXT_RANGES.length) blob[88 + k] = (v - QUAD_EXT_RANGES[k][0]) & 255; });
  if (ext.length > QUAD_EXT_RANGES.length) {
    const c = (k) => { const [mn, mx] = QUAD_VOICE_RANGES[k]; return Math.max(mn, Math.min(mx, Math.round(ext[QUAD_EXT_RANGES.length + k] ?? QUAD_VOICE_RANGES[k][2]))); };
    blob[124] = (c(0) & 1) | (((c(4) - 1) & 3) << 1);
    blob[125] = c(1) - 1;
    blob[126] = (c(2) - 20) & 255;
    blob[127] = (c(3) - 64) & 255;
  }
  return blob;
}

// FM TONE's values (version 5) -> its blob: 'Q' 5, value - min a byte, the hi-res ones u16le, zeros (eng_quad.c quad_pack)
export function quadPack(p) {
  const blob = new Uint8Array(QUAD_BLOB);
  let o = 2;
  blob[0] = 0x51; blob[1] = QUAD_VER;
  for (let i = 0; i < QUAD_NP; i++) {
    const u = p[i] - QUAD_RANGES[i][0];
    blob[o++] = u & 255;
    if (QUAD_HIRES.includes(i)) blob[o++] = u >> 8;
  }
  return blob;
}

// ---- writing (the self-test's synthetic sounds; the inverse of the table)
// params: the Digitone's values by DN2_FIELDS key (absent: the DN1 conversion's defaults, DNX soundmap.ts DEFAULT_BYTES)
export function dn2RecordFromParams(params = {}, { name = "INIT", version = 2, tagBits = 0 } = {}) {
  const d = new Uint8Array(DN2_RECORD);
  d.set(BEEFBACE, 0); d.set(be32(version), 4); d.set(be32(tagBits >>> 0), 8);
  for (let i = 0; i < 16 && i < name.length; i++) d[12 + i] = name.charCodeAt(i) & 255;
  for (const [o, v] of [[34, 0x70], [42, 3], [50, 0x40], [90, 0x40], [144, 0x40], [146, 0x40], [148, 0x40], [150, 0x40],
    [152, 0x40], [154, 0x40], [156, 0x40], [158, 0x40], [198, 1], [204, 0x7f], [222, 1], [249, 1], [355, 0xba], [356, 0xce],
    [357, 0xf0], [358, 0x0c]]) d[o] = v;
  const wHi = (o, v) => { const c = Math.floor(v + 1e-9); d[o] = c; d[o + 1] = Math.min(255, R((v - c) * 256)); };
  const ENC = {
    "algo": (o, v) => { d[o] = v - 1; }, "ratioC": (o, v) => { d[o] = DN2_RATIO_CB.indexOf(v); },
    "ratioA": (o, v) => { d[o] = DN2_RATIO_A.indexOf(v); }, "ratioB": (o, v) => { d[o] = (v * 2) >> 8; d[o + 1] = (v * 2) & 255; },
    "octave": (o, v) => { d[o] = v & 255; },
  };
  const zero = new Uint8Array(DN2_RECORD);
  for (const [k, o, h, , dec] of DN2_FIELDS) {
    if (params[k] === undefined) continue;
    const v = params[k];
    if (ENC[k]) { ENC[k](o, v); continue; }
    const bias = -dec(zero, o);                    // the law's offset: 64 (bipolar), 63 (HARM), -1 (ALGO), 0
    if (h) wHi(o, v + bias); else d[o] = (v + bias) & 255;
  }
  return d;
}
// a record -> the stored container: header (Elektroid elektron.c elektron_data_header), the body (prefix + record), raw
// or as one literal LZ4 block, the trailer (CRC-32 zero seed, the length, AA A1 DA AA)
export function wrapDn2Record(rec, { compress = true, slot = 0, build = "0050" } = {}) {
  const ver = u32be(rec, 4), body = new Uint8Array(DN2_BODY);
  body.set(be32(ver), 0); body.set(rec, 5);
  let chain = body;
  if (compress) {                                    // one block of literals only: valid LZ4, not small
    const l = body.length, ext = [];
    for (let r = l - 15; ; r -= 255) { if (r >= 255) ext.push(255); else { ext.push(r); break; } }
    const blk = Uint8Array.from([0xf0, ...ext, ...body]);
    chain = Uint8Array.from([...be32(blk.length), ...blk, 0, 0, 0, 0]);
  }
  const h = [...CONTAINER, 2, 0, 5, 0, 15, ...Array.from(build, (c) => c.charCodeAt(0)), ...be32(3), ...be32(ver), ...be32(slot),
    ...be32(DN2_BODY), compress ? 1 : 0, TRL];
  const pl = new Uint8Array(HDR + chain.length + TRL);
  pl.set(h, 0); pl.set(chain, HDR);
  pl.set(be32(crc32(chain)), HDR + chain.length); pl.set(be32(pl.length - 43), HDR + chain.length + 4); pl.set(FOOTER, pl.length - 4);
  return pl;
}
// a payload + manifest -> a .dn2pst (a ZIP, entries stored)
export function dn2pstZip(payload, man) {
  const enc = new TextEncoder(), files = [["manifest.json", enc.encode(JSON.stringify(man, null, 2))], [man.Payload, payload]];
  const loc = [], cen = [];
  let off = 0;
  const le16 = (v) => [v & 255, v >> 8 & 255], le32 = (v) => [v & 255, v >>> 8 & 255, v >>> 16 & 255, v >>> 24 & 255];
  for (const [name, data] of files) {
    const nb = enc.encode(name), c = crc32(data, 0xffffffff);
    const fix = [...le16(20), ...le16(0), ...le16(0), ...le16(0), ...le16(0x21), ...le32(c), ...le32(data.length), ...le32(data.length), ...le16(nb.length), ...le16(0)];
    loc.push(Uint8Array.from([...le32(0x04034b50), ...fix, ...nb, ...data]));
    cen.push(Uint8Array.from([...le32(0x02014b50), ...le16(20), ...fix, ...le16(0), ...le16(0), ...le16(0), ...le32(0), ...le32(off), ...nb]));
    off += loc[loc.length - 1].length;
  }
  const cl = cen.reduce((a, b) => a + b.length, 0);
  const end = Uint8Array.from([...le32(0x06054b50), 0, 0, 0, 0, ...le16(files.length), ...le16(files.length), ...le32(cl), ...le32(off), 0, 0]);
  const zip = new Uint8Array(off + cl + end.length);
  let p = 0;
  for (const b of [...loc, ...cen, end]) { zip.set(b, p); p += b.length; }
  return zip;
}
