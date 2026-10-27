// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
// FIZZ presets written as Elektron Digitone files: .dnsnd (Digitone, OS 1.44: a version-5 sound record) and .dn2pst
// (Digitone II, OS 1.10 and later: a version-2 record, machine FM TONE). Pure JS, no dependencies beyond FIZZ's own
// dn2pst.mjs (its Digitone II tables). The inverse of the readers: ChoralRoot's fm1dnsnd.js dnsndToQuad (the DN1 laws,
// proved by the owner's exports) and dn2pst.mjs dn2pstToQuad; research/DN2-FORMAT.md "Export" says what is proved.
//
//   fizzToDnsnd(sound, opts) -> {bytes: Uint8Array (.dnsnd), notes: [what the format cannot hold], record}
//   fizzToDn2pst(sound, opts) -> {bytes: Uint8Array (.dn2pst), notes, record}
//   sound: {blob (96 bytes 'Q' 5, or 128 'Q' 6) | values (74 or 110 numbers), name, tags (FIZZ kinds: "BASS" ..),
//           dnTags (Digitone tag words, instead of tags), pan (-64..63), sends {chorus, delay, reverb} (0..127),
//           drive (0..127: instead of the blob's byte 87)}
//   opts: {date (the ZIP's time), compress (default true: LZ4, as the instruments write it)}
//
// The files are built the way the instruments and Transfer build them: a ZIP of manifest.json (Transfer's layout: two
// spaces, CRLF) and the payload; the payload Elektron's container (31-byte header, an LZ4 block chain, the 12-byte
// trailer with its CRC-32); the LZ4 block made by Elektron's own compressor's rules (lz4Elektron: it reproduces every
// block of the 462 Digitone files and the Digitone II file on hand, byte for byte).
import { DN2_RATIO_A, DN2_RATIO_CB, DN2_LFO_DEST, DN2_TAGS, multToQuad, crc32, QUAD_RANGES, QUAD_EXT_RANGES, QUAD_VOICE_RANGES } from "./dn2pst.mjs";

// ---- FIZZ's patch (core/voice/quad/quad_voice.h): 0..72 FM TONE (eng_quad.c QP_*), 73 DRIVE, 74..109 the E values
export const Q = { ALGO: 0, RC: 1, RA: 2, RB1: 3, HARM: 4, DTUN: 5, FDBK: 6, MIX: 7, OFSC: 8, OFSA: 9, OFSB1: 10, OFSB2: 11,
  AATK: 12, ADEC: 13, AEND: 14, ALEV: 15, BATK: 16, BDEC: 17, BEND: 18, BLEV: 19, ADLY: 20, ATRIG: 21, ARST: 22, PHRT: 23,
  BDLY: 24, BTRIG: 25, BRST: 26, VEL: 27, AKEY: 28, B1KEY: 29, FATK: 30, FDEC: 31, FSUS: 32, FREL: 33, FREQ: 34, RESO: 35,
  FTYPE: 36, FDEPTH: 37, FDLY: 38, FKTRK: 39, BASE: 40, WIDTH: 41, EATK: 42, EDEC: 43, ESUS: 44, EREL: 45, LEVEL: 46,
  RB2: 71, B2KEY: 72, DRIVE: 73, AMOD: 74, HOLD: 75, PORT: 76, PTIM: 77, PSLOPE: 78, DRPRE: 79, FSHAPE: 80, FGAIN: 81,
  BWPOST: 82, BITS: 83, SRR: 84, CRPOST: 85, MOD0: 86 };
const lfo = (k, f) => 47 + 8 * k + f;                    // f: 0 SPEED 1 MULT 2 FADE 3 DEST 4 WAVE 5 PHASE 6 TRIG 7 DEPTH
const HIRES = [4, 5, 34, 35, 37, 40, 47, 54, 55, 62, 63, 70];
export const NP = 115;                                   // (2026-10-26: + the VOICE page's 110..114)
const RANGES = [...QUAD_RANGES, [0, 127, 0], ...QUAD_EXT_RANGES, ...QUAD_VOICE_RANGES];   // [min, max, default] of the 110
export const defaults = () => RANGES.map((r) => r[2]);

// a FIZZ sound's 110 values (a version-5 blob or 74 values: the E values their defaults)
export function fizzValues(sound) {
  const v = defaults();
  if (sound.values) { sound.values.forEach((x, i) => { if (i < NP) v[i] = x; }); }
  else {
    const b = sound.blob;
    if (!b || b[0] !== 0x51 || (b[1] !== 5 && b[1] !== 6)) throw new Error("not a FIZZ patch: a version-5 or version-6 FM TONE blob ('Q' 5 / 'Q' 6)");
    let o = 2;
    for (let i = 0; i < 73; i++) {
      let u = b[o++];
      if (HIRES.includes(i)) u |= b[o++] << 8;
      v[i] = u + RANGES[i][0];
    }
    v[Q.DRIVE] = b[87] <= 127 ? b[87] : 0;
    if (b[1] === 6) {
      for (let i = 74; i < 110; i++) v[i] = b[88 + i - 74] + RANGES[i][0];
      v[110] = b[124] & 1; v[114] = ((b[124] >> 1) + 1) & 3;   // (2026-10-26: the VOICE page, zeros its defaults)
      v[111] = b[125] + 1; v[112] = (b[126] + 20) & 255; v[113] = (b[127] + 64) & 255;
    }
  }
  if (sound.drive !== undefined) v[Q.DRIVE] = sound.drive;
  return v.map((x, i) => Math.max(RANGES[i][0], Math.min(RANGES[i][1], Math.round(x))));
}

// ---- the encodings shared by both records
const R = Math.round;
// a hi-res value (hundredths, total >= 0) -> coarse, fine: the readers' coarse + round(fine x 100 / 256) / 100
function hiBytes(h) { h = Math.max(0, Math.round(h)); const c = Math.min(127, Math.floor(h / 100)), f = c === 127 && h >= 12800 ? 99 : h - c * 100; return [c, Math.min(255, R(f * 2.56))]; }
const put = (r, o, b) => { r[o] = b[0]; r[o + 1] = b[1]; };
const be32 = (v) => [(v >>> 24) & 255, (v >>> 16) & 255, (v >>> 8) & 255, v & 255];
const RA_QUARTERS = (ra) => (ra + 1) / 4;                 // FIZZ's RATIO A: 0.25 .. 16 in quarters
const nearestStep = (table, x) => table.reduce((b, t, i) => (Math.abs(t - x) < Math.abs(table[b] - x) ? i : b), 0);
const WAVE_OUT = [0, 1, 2, 3, 5, 4, 6];                   // FM TONE's TRI SINE SQR SAW RAMP EXP RAND -> the Digitone's order
const MULT_OUT = Array.from({ length: 24 }, (_, q) => Array.from({ length: 24 }, (_, n) => n).find((n) => multToQuad(n) === q));
// FM TONE's LFO destination (eng_quad.c QD_*) -> the Digitone's number: DN1 (fm1dnsnd.js DN_LFO_DEST, inverted), DN2 (an id)
const DN1_DEST = { 0: 0, 40: 1, 41: 3, 42: 5, 43: 9, 44: 11, 45: 13, 46: 15, 3: 17, 4: 18, 5: 19, 6: 20, 11: 21, 12: 22, 13: 23, 14: 24,
  17: 27, 18: 28, 19: 29, 15: 30, 20: 31, 21: 32, 22: 33, 16: 34, 23: 35, 24: 38, 1: 42, 2: 43, 47: 44, 48: 45, 49: 46, 50: 47, 51: 48,
  52: 49, 25: 51, 26: 52, 27: 53, 30: 54, 31: 55, 32: 56, 33: 57, 28: 58, 29: 59, 34: 60, 35: 61, 36: 62, 37: 63, 53: 64, 39: 65,
  38: 66, 54: 67, 55: 68, 56: 69, 7: 73, 8: 74, 9: 75, 10: 76, 57: 77 };
const DN2_DEST = Object.fromEntries(Object.entries(DN2_LFO_DEST).map(([id, q]) => [q, +id]));
// FIZZ's kinds -> the nearest Digitone tag words (Transfer's spellings: dn2pst.mjs DN2_TAGS)
export const FIZZ_TO_DN_TAG = { BASS: "BASS", KEYS: "CHORD", PLUCK: "ARPEGGIO", PAD: "PAD", LEAD: "LEAD", DRUMS: "PERCUSSION", FX: "SOUND FX" };
function tagsOf(sound) {
  const words = sound.dnTags ? sound.dnTags.map(String) :
    (Array.isArray(sound.tags) ? sound.tags : []).map((t) => FIZZ_TO_DN_TAG[String(t).toUpperCase()]).filter(Boolean);
  const alias = { ATMOSPHERE: "ATMOSPHERIC" };
  const known = [...new Set(words.map((w) => alias[w] || w))].filter((w) => DN2_TAGS.includes(w));
  let bits = 0;
  for (const w of known) bits |= 1 << DN2_TAGS.indexOf(w);
  return { bits: bits >>> 0, words: DN2_TAGS.filter((w) => known.includes(w)) };   // (in bit order, as Transfer lists them)
}
// the name the Digitone shows: upper case, printable ASCII and the Latin-1 letters (the readers take the bytes as they
// are); at most 16 characters (the record's field; the payload's name in the ZIP is cut to 15, as Transfer cuts it)
export function dnName(s, fallback = "FIZZ") {
  const out = String(s || "").toUpperCase().normalize("NFC").split("").map((c) => {
    const k = c.charCodeAt(0);
    if (k >= 0x20 && k <= 0x7e) return c;               // (printable ASCII, upper case: the factory names use ^ ' & ..)
    if (k >= 0xc0 && k <= 0xff && k !== 0xd7 && k !== 0xf7) return c;   // (ÅÄÖ ..: the factory names use them)
    return "_";
  }).join("").slice(0, 16);
  return out.trim() ? out : fallback;
}
const nameBytes = (r, name) => { for (let i = 0; i < 16; i++) r[12 + i] = i < name.length ? name.charCodeAt(i) & 255 : 0; };

// the notes common to both: what FIZZ has that neither Digitone holds
function commonNotes(v, notes, lfos) {
  if (v[Q.VEL] !== 64) notes.push(`VEL ${v[Q.VEL]} (the operators' velocity sensitivity) has no Digitone field: dropped`);
  if (lfos < 3 && (v[lfo(2, 3)] !== 0 || v[lfo(2, 7)] !== 0)) notes.push("LFO 3: the Digitone has two LFOs: dropped");
  if (v[Q.PSLOPE]) notes.push("portamento SLOPE RATE: the Digitone's slope is a menu setting not in the file: dropped");
  if (v[Q.PORT] === 2) notes.push("portamento LEGATO: written as ON (the Digitone's legato-only type is a menu setting)");
  const mods = Array.from({ length: 8 }, (_, k) => v[Q.MOD0 + 3 * k]).filter((s) => s).length;
  if (mods && lfos < 3) notes.push(`${mods} modulation slot(s): not written (the Digitone 1's modulation records are not decoded)`);
}
function ratioA(v, notes, table) {
  const want = RA_QUARTERS(v[Q.RA]), s = nearestStep(table, want);
  if (table[s] !== want) notes.push(`RATIO A ${want}: the Digitone has no such step: ${table[s]}`);
  return s;
}
function fdec(v, notes) {
  if (v[Q.FDEC] < 127 || v[Q.FSUS] === 127) return v[Q.FDEC];
  notes.push("filter DEC 127: the Digitone's 127 is INF: written as 126");
  return 126;
}
function ampAHD(v, notes) {                               // FIZZ's AHD on a Digitone without one (the DN1): an ADSR like it
  if (!v[Q.AMOD]) return [v[Q.EDEC], v[Q.ESUS], v[Q.EREL]];
  const held = v[Q.HOLD] >= 127;
  notes.push(held ? "amp AHD with HOLD NOTE: written as ADSR, SUS 127, REL = DEC" : `amp AHD HOLD ${v[Q.HOLD]}: written as ADSR (the hold is lost)`);
  return held ? [127, 127, Math.min(126, v[Q.EDEC])] : [v[Q.EDEC], 0, Math.min(126, v[Q.EDEC])];
}

// ---- the Digitone's record (version 5, 302 bytes: fm1dnsnd.js's offsets; DNX soundmap.ts names the rest)
// INIT values of the bytes no FIZZ value writes (research/DN2-FORMAT.md "Export": each the value the INIT sound and 99 %
// of the corpus's version-5 records hold; their meaning, where known, from DNX's DN1 -> DN2 map)
export const DN1_INIT = [
  [112, 64], [114, 64], [116, 64], [118, 64], [120, 64], [122, 64], [124, 64], [126, 64],   // 8 x u16 0x0040 (DN2 144..158)
  [168, 1],                                              // AMP ENV RESET on
  [170, 63], [172, 39],                                  // portamento OFF (64 = ON), its time
  [188, 2],                                              // (DN2 +246; meaning unknown)
  [251, 9], [253, 14], [254, 15], [255, 255], [256, 255], // the arpeggiator: SPD, N.LEN, LEN 16, all steps on (MODE 0 off)
  [279, 2], [283, 100],                                  // (DN2 +250, +326; meaning unknown)
  [298, 0xba], [299, 0xce], [300, 0xf0], [301, 0x0c],    // the record's end mark
];
export function dn1Record(v, { name, tagBits, pan = 0, sends = {} }, notes) {
  const r = new Uint8Array(302);
  r.set([0xbe, 0xef, 0xba, 0xce, 0, 0, 0, 5, ...be32(tagBits)], 0);
  nameBytes(r, name);
  for (const [o, x] of DN1_INIT) r[o] = x;
  for (let k = 0; k < 2; k++) {
    put(r, 30 + 2 * k, hiBytes(v[lfo(k, 0)] + 6400));
    r[34 + 2 * k] = v[lfo(k, 1)];                          // MULT: 1:1 (ChoralRoot's reading of the DN1)
    r[38 + 2 * k] = v[lfo(k, 2)] + 64;
    const d = DN1_DEST[v[lfo(k, 3)]];
    if (d === undefined) notes.push(`LFO ${k + 1} destination #${v[lfo(k, 3)]}: the Digitone has none: off`);
    r[42 + 2 * k] = d ?? 0;
    r[46 + 2 * k] = WAVE_OUT[v[lfo(k, 4)]];
    r[50 + 2 * k] = v[lfo(k, 5)];
    r[54 + 2 * k] = v[lfo(k, 6)];
    put(r, 58 + 2 * k, hiBytes(v[lfo(k, 7)] + 6400));
  }
  r[62] = v[Q.ALGO] - 1; r[64] = v[Q.RC]; r[66] = ratioA(v, notes, DN2_RATIO_A);
  const rb = 2 * (v[Q.RB1] * 19 + v[Q.RB2]); r[68] = rb >> 8; r[69] = rb & 255;
  put(r, 70, hiBytes(v[Q.HARM] + 6300)); put(r, 72, hiBytes(v[Q.DTUN]));
  r[74] = v[Q.FDBK]; r[76] = v[Q.MIX] + 64; r[78] = v[Q.PHRT];
  [Q.AATK, Q.ADEC, Q.AEND, Q.ALEV, Q.BATK, Q.BDEC, Q.BEND, Q.BLEV].forEach((i, k) => { r[82 + 2 * k] = v[i]; });
  [Q.ADLY, Q.ATRIG, Q.ARST, Q.BDLY, Q.BTRIG, Q.BRST].forEach((i, k) => { r[98 + 2 * k] = v[i]; });
  // the filter: FM TONE's OFF LP12 HP12 LP24 as they are; FIZZ's E2 machines (4 MULTI, 5 COMB+, 6 COMB-, 7 EQ) the
  // Digitone 1 has none of: the nearest legacy type, noted. MULTI by its morph (FSHAPE 0 LP12 .. 64 BP .. 127 HP12): the
  // nearer end; the combs and the EQ's bell have no counterpart: off
  let ft = v[Q.FTYPE];
  if (ft > 3) {
    const t = ft === 4 ? (v[Q.FSHAPE] < 64 ? 1 : 2) : 0, name = ["MULTI", "COMB+", "COMB-", "EQ"][ft - 4];
    notes.push(`filter ${name}${ft === 4 ? ` (morph ${v[Q.FSHAPE]})` : ""}: the Digitone has no such filter: approximated as ${t === 1 ? "LP2" : t === 2 ? "HP2" : "off"}`);
    ft = t;
  }
  r[128] = ft;
  put(r, 130, hiBytes(v[Q.FREQ])); put(r, 132, hiBytes(v[Q.RESO])); put(r, 134, hiBytes(v[Q.FDEPTH] + 6400));
  r[136] = v[Q.FATK]; r[138] = fdec(v, notes); r[140] = v[Q.FSUS]; r[142] = v[Q.FREL];
  put(r, 144, hiBytes(v[Q.BASE])); r[146] = v[Q.WIDTH];
  const [ed, es, er] = ampAHD(v, notes);
  r[148] = v[Q.EATK]; r[150] = ed; r[152] = es; put(r, 154, hiBytes(er * 100));
  put(r, 156, hiBytes(v[Q.DRIVE] * 100)); r[158] = pan + 64; put(r, 160, hiBytes(v[Q.LEVEL] * 100));
  put(r, 162, hiBytes((sends.reverb | 0) * 100)); put(r, 164, hiBytes((sends.delay | 0) * 100)); put(r, 166, hiBytes((sends.chorus | 0) * 100));
  if (v[Q.PORT]) { r[170] = 64; r[172] = v[Q.PTIM]; }
  [Q.OFSC, Q.OFSA, Q.OFSB1, Q.OFSB2].forEach((i, k) => put(r, 174 + 2 * k, hiBytes(v[i] * 64 + 6400)));
  r[182] = v[Q.FDLY];
  r[274] = v[Q.AKEY]; r[275] = v[Q.B1KEY]; r[276] = v[Q.B2KEY];
  r[277] = R(v[Q.FKTRK] * 100 / 127);
  if (v[Q.DRIVE] && !v[Q.DRPRE]) notes.push("DRIVE: the Digitone's drive is before the filter (FIZZ's is after it)");
  for (const [i, what] of [[Q.BITS, "BIT REDUCTION"], [Q.SRR, "SAMPLE RATE REDUCTION"]]) if (v[i]) notes.push(`${what} ${v[i]}: the Digitone has none: dropped`);
  if (v[Q.BWPOST]) notes.push("base-width after the filter: the Digitone's is before it");
  commonNotes(v, notes, 2);
  return r;
}

// E4: FIZZ's eight modulation slots into the Digitone II's modulation records, the inverse of dn2pst.mjs's reading
// (+252 + 3k, k < 24: six sources x four records, PB VEL MW BR AT KEY; the first two bytes the depth, depth / 2 + 64 as a
// coarse byte and a 1/256 fine byte; the third the destination's id; inferred, not verified on a real sound). RAND and
// F ENV have no record, a destination the Digitone II lacks has no id, a source's fifth slot has no room: noted, dropped
const SRC_NAMES = ["", "VEL", "KEY", "MW", "AT", "PB", "BR", "RAND", "F ENV"];
const SRC_GROUP = { 5: 0, 1: 1, 3: 2, 6: 3, 4: 4, 2: 5 };   // FIZZ's source -> the record group (dn2pst.mjs SRC, inverted)
function modRecords(r, v, notes) {
  const used = [0, 0, 0, 0, 0, 0];
  for (let k = 0; k < 8; k++) {
    const src = v[Q.MOD0 + 3 * k], dest = v[Q.MOD0 + 3 * k + 1], depth = v[Q.MOD0 + 3 * k + 2];
    if (!src || !dest) continue;
    const g = SRC_GROUP[src], id = DN2_DEST[dest];
    if (g === undefined) { notes.push(`MOD ${k + 1}: ${SRC_NAMES[src] || `source ${src}`} has no Digitone II modulation record: dropped`); continue; }
    if (!id) { notes.push(`MOD ${k + 1} (${SRC_NAMES[src]}): destination #${dest}: the Digitone II has none: dropped`); continue; }
    if (used[g] >= 4) { notes.push(`MOD ${k + 1}: a fifth ${SRC_NAMES[src]} slot: the Digitone II has four records a source: dropped`); continue; }
    const o = 252 + 3 * (g * 4 + used[g]++), x = depth / 2 + 64, c = Math.floor(x);
    r[o] = c; r[o + 1] = Math.round((x - c) * 256); r[o + 2] = id;
  }
}

// ---- the Digitone II's record (version 2, 359 bytes: dn2pst.mjs DN2_FIELDS's offsets, 28 + 2 x id)
// INIT values of the bytes no FIZZ value writes: Elektron's DN1 -> DN2 conversion of the DN1 INIT (DNX soundmap.ts
// DEFAULT_BYTES and its copies of DN1_INIT's bytes), research/DN2-FORMAT.md "Export"
export const DN2_INIT = [
  [144, 64], [146, 64], [148, 64], [150, 64], [152, 64], [154, 64], [156, 64], [158, 64],   // ids 58..65 (PITCH ALL ..)
  [198, 1], [224, 1],                                    // FILTER and AMP ENV RESET on
  [226, 63], [228, 39],                                  // portamento OFF (64 = ON), its time
  [246, 2], [249, 1], [250, 2], [326, 100],              // (copied from the DN1's +188, constant, +279, +283)
  [332, 13], [334, 14], [335, 15], [336, 255], [337, 255],   // the arpeggiator (DN1 +251 9 -> 13 by DNX's table ..)
  [355, 0xba], [356, 0xce], [357, 0xf0], [358, 0x0c],
];
export function dn2Record(v, { name, tagBits, pan = 0, sends = {} }, notes) {
  const r = new Uint8Array(359);
  r.set([0xbe, 0xef, 0xba, 0xce, 0, 0, 0, 2, ...be32(tagBits)], 0);
  nameBytes(r, name);
  for (const [o, x] of DN2_INIT) r[o] = x;
  for (let k = 0; k < 3; k++) {
    put(r, 30 + 2 * k, hiBytes(v[lfo(k, 0)] + 6400));
    r[38 + 2 * k] = MULT_OUT[v[lfo(k, 1)]];
    r[46 + 2 * k] = v[lfo(k, 2)] + 64;
    let d = DN2_DEST[v[lfo(k, 3)]];
    if (d === undefined) { notes.push(`LFO ${k + 1} destination #${v[lfo(k, 3)]}: the Digitone II has none: off`); d = 0; }
    r[54 + 2 * k] = d;
    r[62 + 2 * k] = WAVE_OUT[v[lfo(k, 4)]];
    r[70 + 2 * k] = v[lfo(k, 5)];
    r[78 + 2 * k] = v[lfo(k, 6)];
    put(r, 86 + 2 * k, hiBytes(v[lfo(k, 7)] + 6400));
  }
  r[94] = v[Q.ALGO] - 1; r[96] = v[Q.RC]; r[98] = ratioA(v, notes, DN2_RATIO_A);
  const rb = 2 * (v[Q.RB1] * 19 + v[Q.RB2]); r[100] = rb >> 8; r[101] = rb & 255;
  put(r, 102, hiBytes(v[Q.HARM] + 6300)); put(r, 104, hiBytes(v[Q.DTUN]));
  r[106] = v[Q.FDBK]; r[108] = v[Q.MIX] + 64; r[110] = v[Q.PHRT];
  [Q.AATK, Q.ADEC, Q.AEND, Q.ALEV, Q.BATK, Q.BDEC, Q.BEND, Q.BLEV].forEach((i, k) => { r[114 + 2 * k] = v[i]; });
  [Q.ADLY, Q.ATRIG, Q.ARST, Q.BDLY, Q.BTRIG, Q.BRST].forEach((i, k) => { r[130 + 2 * k] = v[i]; });
  [Q.OFSC, Q.OFSA, Q.OFSB1, Q.OFSB2].forEach((i, k) => put(r, 160 + 2 * k, hiBytes(v[i] * 64 + 6400)));
  r[168] = v[Q.AKEY]; r[170] = v[Q.B1KEY]; r[172] = v[Q.B2KEY];
  // the filter machine (+245) and its TYPE (+174): FM TONE's OFF LP12 HP12 -> LEGACY LP/HP 3 0 1 (Elektron's own DN1
  // conversion), LP24 -> LOWPASS 4; FIZZ's E2 machines (plan §3: 4 MULTI, 5 COMB+, 6 COMB-, 7 EQ) to theirs
  const ft = v[Q.FTYPE];
  const [mach, type] = ft === 0 ? [4, 3] : ft === 1 ? [4, 0] : ft === 2 ? [4, 1] : ft === 3 ? [1, 0] :
    ft === 4 ? [0, v[Q.FSHAPE]] : ft === 5 ? [5, v[Q.FSHAPE]] : ft === 6 ? [3, v[Q.FSHAPE]] : [2, v[Q.FSHAPE]];
  r[245] = mach; r[174] = type;
  put(r, 176, hiBytes(v[Q.FREQ]));
  if (ft === 7) r[178] = v[Q.FGAIN] + 64; else put(r, 178, hiBytes(v[Q.RESO]));
  put(r, 180, hiBytes(v[Q.FDEPTH] + 6400));
  r[182] = v[Q.FDLY]; r[184] = v[Q.FATK]; r[186] = fdec(v, notes); r[188] = v[Q.FSUS]; r[190] = v[Q.FREL];
  r[192] = R(v[Q.FKTRK] * 100 / 127);
  if (v[Q.FKTRK] && R(R(v[Q.FKTRK] * 100 / 127) * 127 / 100) !== v[Q.FKTRK]) notes.push(`filter KEY TRACK ${v[Q.FKTRK]}: the Digitone II's is in percent: ${r[192]} %`);
  put(r, 194, hiBytes(v[Q.BASE])); r[196] = v[Q.WIDTH];
  // the amp: ADSR (MODE 1) or AHD (MODE 0, HOLD 127 = NOTE)
  r[222] = v[Q.AMOD] ? 0 : 1;
  r[202] = v[Q.EATK]; r[204] = v[Q.HOLD]; r[206] = v[Q.EDEC]; r[208] = v[Q.ESUS]; put(r, 210, hiBytes(v[Q.EREL] * 100));
  put(r, 212, hiBytes((sends.chorus | 0) * 100)); put(r, 214, hiBytes((sends.delay | 0) * 100)); put(r, 216, hiBytes((sends.reverb | 0) * 100));
  r[218] = pan + 64; put(r, 220, hiBytes(v[Q.LEVEL] * 100));
  if (v[Q.PORT]) { r[226] = 64; r[228] = v[Q.PTIM]; }
  r[230] = v[Q.BITS]; r[232] = v[Q.SRR]; r[234] = v[Q.CRPOST];
  put(r, 236, hiBytes(v[Q.DRIVE] * 100));
  r[238] = v[Q.BWPOST];
  r[240] = v[Q.DRPRE] ? 0 : 1;                           // OD ROUTING: 0 before the filter (dn2pst.mjs's reading), 1 after
  r[244] = 0;                                            // the machine: FM TONE
  modRecords(r, v, notes);
  commonNotes(v, notes, 3);
  return r;
}

// ---- the container, the LZ4 block, the ZIP
// Elektron's LZ4 block compressor, by its observable rules: the reference LZ4 fast compressor (acceleration 1, a
// 4096-entry hash table of positions, MINMATCH 4, MFLIMIT 12, LASTLITERALS 5, the skip trigger 6), hashing the next four
// bytes read big-endian times 2654435761, >> 20. It reproduces all 462 Digitone and the Digitone II file on hand.
export function lz4Elektron(src) {
  const n = src.length, out = [];
  const be = (i) => ((src[i] << 24) | (src[i + 1] << 16) | (src[i + 2] << 8) | src[i + 3]) >>> 0;
  const hash = (i) => Math.imul(be(i), 2654435761 | 0) >>> 20;
  const table = new Int32Array(4096), mflimit1 = n - 11, matchlimit = n - 5;
  let anchor = 0, ip = 0;
  const lit = (from, to, tokenExtra = 0) => {
    const l = to - from, t = out.length;
    if (l >= 15) { out.push(0xf0 | tokenExtra); let x = l - 15; for (; x >= 255; x -= 255) out.push(255); out.push(x); } else out.push((l << 4) | tokenExtra);
    for (let i = from; i < to; i++) out.push(src[i]);
    return t;
  };
  const last = () => { lit(anchor, n); return Uint8Array.from(out); };
  if (n < 13) return last();
  table[hash(0)] = 0; ip = 1;
  let fh = hash(ip);
  for (;;) {
    let match, fip = ip, step = 1, nb = 64;
    do {
      const h = fh;
      ip = fip; fip += step; step = nb++ >> 6;
      if (fip > mflimit1) return last();
      match = table[h]; fh = hash(fip); table[h] = ip;
    } while (be(match) !== be(ip));
    while (ip > anchor && match > 0 && src[ip - 1] === src[match - 1]) { ip--; match--; }
    let t = lit(anchor, ip);
    for (;;) {
      out.push((ip - match) & 255, ((ip - match) >> 8) & 255);
      let ml = 0;
      while (ip + 4 + ml < matchlimit && src[ip + 4 + ml] === src[match + 4 + ml]) ml++;
      ip += ml + 4;
      if (ml >= 15) { out[t] += 15; ml -= 15; for (; ml >= 255; ml -= 255) out.push(255); out.push(ml); } else out[t] += ml;
      anchor = ip;
      if (ip >= mflimit1) return last();
      table[hash(ip - 2)] = ip - 2;
      const h = hash(ip); match = table[h]; table[h] = ip;
      if (be(match) === be(ip)) { t = out.length; out.push(0); continue; }
      break;
    }
    fh = hash(++ip);
  }
}
// Elektron's stored-file container (DNX container.ts; Elektroid elektron.c elektron_data_header): 31 bytes of header,
// the body raw or as an LZ4 chain (u32be size, the block, u32be 0), the CRC-32 (zero seed) over everything after the
// header, the body's length in the payload (payload - 43), the footer AA A1 DA AA
export function container({ device, build, objVersion, body, index = 0, compress = true }) {
  let chain = body;
  if (compress) { const blk = lz4Elektron(body); chain = Uint8Array.from([...be32(blk.length), ...blk, 0, 0, 0, 0]); }
  const pl = new Uint8Array(31 + chain.length + 12);
  pl.set([0xac, 0x11, 0xd3, 0x03, 0x02, 0x00, 0x05, 0x00, device, ...Array.from(build, (c) => c.charCodeAt(0)), ...be32(3), ...be32(objVersion),
    ...be32(index), ...be32(body.length), compress ? 1 : 0, 12], 0);
  pl.set(chain, 31);
  pl.set([...be32(crc32(chain)), ...be32(pl.length - 43), 0xaa, 0xa1, 0xda, 0xaa], 31 + chain.length);
  return pl;
}
// manifest.json as Transfer writes it (two spaces, CRLF, no newline at the end)
export const manifestText = (m) => JSON.stringify(m, null, 2).replace(/\n/g, "\r\n");
// a ZIP as Transfer's (version 2.0, UTF-8 names, deflate): each entry one stored deflate block (method 8, BTYPE 00)
export function zipTransfer(files, when = new Date()) {
  const enc = new TextEncoder(), parts = [], cen = [];
  const le16 = (v) => [v & 255, (v >> 8) & 255], le32 = (v) => [v & 255, (v >>> 8) & 255, (v >>> 16) & 255, (v >>> 24) & 255];
  const dt = ((when.getHours() << 11) | (when.getMinutes() << 5) | (when.getSeconds() >> 1)) & 0xffff;
  const dd = (((when.getFullYear() - 1980) << 9) | ((when.getMonth() + 1) << 5) | when.getDate()) & 0xffff;
  let off = 0;
  for (const [name, data0] of files) {
    const data = typeof data0 === "string" ? enc.encode(data0) : data0, nb = enc.encode(name), c = crc32(data, 0xffffffff);
    const defl = Uint8Array.from([0x01, ...le16(data.length), ...le16(~data.length & 0xffff), ...data]);
    const fix = [...le16(0x0a), ...le16(0x0800), ...le16(8), ...le16(dt), ...le16(dd), ...le32(c), ...le32(defl.length), ...le32(data.length), ...le16(nb.length), ...le16(0)];
    const head = Uint8Array.from([...le32(0x04034b50), ...fix, ...nb]);
    parts.push(head, defl);
    cen.push(Uint8Array.from([...le32(0x02014b50), ...le16(0x14), ...fix, ...le16(0), ...le16(0), ...le16(0), ...le32(0), ...le32(off), ...nb]));
    off += head.length + defl.length;
  }
  const cl = cen.reduce((a, b) => a + b.length, 0);
  const end = Uint8Array.from([...le32(0x06054b50), 0, 0, 0, 0, ...le16(files.length), ...le16(files.length), ...le32(cl), ...le32(off), 0, 0]);
  const z = new Uint8Array(off + cl + end.length);
  let p = 0;
  for (const b of [...parts, ...cen, end]) { z.set(b, p); p += b.length; }
  return z;
}
const payloadName = (name) => name.slice(0, 15).replace(/[\\/]/g, "_").trim() || "SOUND";

// ---- the two files
export function fizzToDnsnd(sound, opts = {}) {
  const v = fizzValues(sound), notes = [], name = dnName(sound.name), tg = tagsOf(sound);
  const record = dn1Record(v, { name, tagBits: tg.bits, pan: sound.pan | 0, sends: sound.sends || {} }, notes);
  const payload = container({ device: 9, build: "0097", objVersion: 5, body: record, compress: opts.compress !== false });
  const man = { FormatVersion: "1.0", ProductType: ["24", "30"], Payload: payloadName(name), FileType: "Sound", FirmwareVersion: "1.42A",
    MetaInfo: { Tags: tg.words } };
  return { bytes: zipTransfer([["manifest.json", manifestText(man)], [man.Payload, payload]], opts.date), notes, record, payload, manifest: man };
}
export function fizzToDn2pst(sound, opts = {}) {
  const v = fizzValues(sound), notes = [], name = dnName(sound.name), tg = tagsOf(sound);
  const record = dn2Record(v, { name, tagBits: tg.bits, pan: sound.pan | 0, sends: sound.sends || {} }, notes);
  const body = new Uint8Array(364);
  body.set(be32(2), 0); body.set(record, 5);              // the stored body: the record's version, a 0, the record
  const payload = container({ device: 15, build: "0050", objVersion: 2, body, compress: opts.compress !== false });
  const man = { FormatVersion: "1.0", ProductType: [], Payload: payloadName(name), FileType: "Sound", FirmwareVersion: "1.10E",
    MetaInfo: { Tags: tg.words } };
  return { bytes: zipTransfer([["manifest.json", manifestText(man)], [man.Payload, payload]], opts.date), notes, record, payload, manifest: man };
}
