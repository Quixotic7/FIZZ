// SPDX-License-Identifier: GPL-3.0-only
// The Digitone II's FM TONE track parameters the wizard can set over MIDI: the parameter id (the sound record's
// 28 + 2 x id, FIZZ research/DN2-FORMAT.md), its CC and its NRPN (MSB, LSB), as the Digitone II manual's Appendix C
// prints them, collected by syxgrid-digitone-ii (sysex-visualiser.html LIVE_CC_BY_ID l. 6157-6184, AMP SUS corrected
// to CC 87 from the firmware's own table, LOCK_CC l. 14373-14396) and the FM TONE knob order of DNX
// (packages/core/src/project/machineplock.ts FM_TONE_PLOCKS: SYN pages CC 40-47, 48-55, 56-63, 70-77, NRPN 1:73-104).
// NRPN's data is 14 bits: the stored u16 >> 1 (syxgrid l. 6215-6219, "the firmware's 14-bit form").
//
// kind: "enum" (n values: every CC value is swept), "rpair" (RATIO B: B1 x 19 + B2, stored x 2), "cont" (0..127),
// "bip" (stored + 64); hires: a fine byte beside the coarse one (an NRPN sweep with fractions too).
export const PARAMS = [
  // SYN 1 (CC 40-47, NRPN 1:73-80)
  { id: 33, name: "ALGO", page: "SYN1", cc: 40, nrpn: [1, 73], kind: "enum", n: 8 },
  { id: 34, name: "RATIO C", page: "SYN1", cc: 41, nrpn: [1, 74], kind: "enum", n: 19 },
  { id: 35, name: "RATIO A", page: "SYN1", cc: 42, nrpn: [1, 75], kind: "enum", n: 36 },
  { id: 36, name: "RATIO B", page: "SYN1", cc: 43, nrpn: [1, 76], kind: "rpair", n: 361 },
  { id: 37, name: "HARM", page: "SYN1", cc: 44, nrpn: [1, 77], kind: "bip", hires: true },
  { id: 38, name: "DTUN", page: "SYN1", cc: 45, nrpn: [1, 78], kind: "cont", hires: true },
  { id: 39, name: "FDBK", page: "SYN1", cc: 46, nrpn: [1, 79], kind: "cont" },
  { id: 40, name: "MIX", page: "SYN1", cc: 47, nrpn: [1, 80], kind: "bip" },
  // SYN 2 (CC 48-55, NRPN 1:81-88)
  { id: 43, name: "A ATK", page: "SYN2", cc: 48, nrpn: [1, 81], kind: "cont" },
  { id: 44, name: "A DEC", page: "SYN2", cc: 49, nrpn: [1, 82], kind: "cont" },
  { id: 45, name: "A END", page: "SYN2", cc: 50, nrpn: [1, 83], kind: "cont" },
  { id: 46, name: "A LEV", page: "SYN2", cc: 51, nrpn: [1, 84], kind: "cont" },
  { id: 47, name: "B ATK", page: "SYN2", cc: 52, nrpn: [1, 85], kind: "cont" },
  { id: 48, name: "B DEC", page: "SYN2", cc: 53, nrpn: [1, 86], kind: "cont" },
  { id: 49, name: "B END", page: "SYN2", cc: 54, nrpn: [1, 87], kind: "cont" },
  { id: 50, name: "B LEV", page: "SYN2", cc: 55, nrpn: [1, 88], kind: "cont" },
  // SYN 3 (CC 56-62, NRPN 1:89-95; knob H blank)
  { id: 51, name: "A DELAY", page: "SYN3", cc: 56, nrpn: [1, 89], kind: "cont" },
  { id: 52, name: "A TRIG", page: "SYN3", cc: 57, nrpn: [1, 90], kind: "enum", n: 2 },
  { id: 53, name: "A RESET", page: "SYN3", cc: 58, nrpn: [1, 91], kind: "enum", n: 2 },
  { id: 41, name: "PHRT", page: "SYN3", cc: 59, nrpn: [1, 92], kind: "enum", n: 5 },
  { id: 54, name: "B DELAY", page: "SYN3", cc: 60, nrpn: [1, 93], kind: "cont" },
  { id: 55, name: "B TRIG", page: "SYN3", cc: 61, nrpn: [1, 94], kind: "enum", n: 2 },
  { id: 56, name: "B RESET", page: "SYN3", cc: 62, nrpn: [1, 95], kind: "enum", n: 2 },
  // SYN 4 (CC 70-73, 75-77, NRPN 1:97-100, 102-104; knob E blank)
  { id: 66, name: "RATIO OFS C", page: "SYN4", cc: 70, nrpn: [1, 97], kind: "bip", hires: true },
  { id: 67, name: "RATIO OFS A", page: "SYN4", cc: 71, nrpn: [1, 98], kind: "bip", hires: true },
  { id: 68, name: "RATIO OFS B1", page: "SYN4", cc: 72, nrpn: [1, 99], kind: "bip", hires: true },
  { id: 69, name: "RATIO OFS B2", page: "SYN4", cc: 73, nrpn: [1, 100], kind: "bip", hires: true },
  { id: 70, name: "KEY TRACK A", page: "SYN4", cc: 75, nrpn: [1, 102], kind: "cont" },
  { id: 71, name: "KEY TRACK B1", page: "SYN4", cc: 76, nrpn: [1, 103], kind: "cont" },
  { id: 72, name: "KEY TRACK B2", page: "SYN4", cc: 77, nrpn: [1, 104], kind: "cont" },
  // FILTER (C.4)
  { id: 78, name: "FILTER ATK", page: "FLTR1", cc: 20, nrpn: [1, 16], kind: "cont" },
  { id: 79, name: "FILTER DEC", page: "FLTR1", cc: 21, nrpn: [1, 17], kind: "cont" },
  { id: 80, name: "FILTER SUS", page: "FLTR1", cc: 22, nrpn: [1, 18], kind: "cont" },
  { id: 81, name: "FILTER REL", page: "FLTR1", cc: 23, nrpn: [1, 19], kind: "cont" },
  { id: 74, name: "FILTER FREQ", page: "FLTR1", cc: 16, nrpn: [1, 20], kind: "cont", hires: true },
  { id: 75, name: "FILTER RESO", page: "FLTR1", cc: 17, nrpn: [1, 21], kind: "cont", hires: true },
  { id: 73, name: "FILTER TYPE", page: "FLTR1", cc: 18, nrpn: [1, 22], kind: "enum", n: 128 },
  { id: 76, name: "FILTER ENV", page: "FLTR1", cc: 24, nrpn: [1, 26], kind: "bip", hires: true },
  { id: 77, name: "FILTER DELAY", page: "FLTR2", cc: 19, nrpn: [1, 23], kind: "cont" },
  { id: 82, name: "FILTER KEY TRACK", page: "FLTR2", cc: 26, nrpn: [1, 69], kind: "cont" },
  { id: 83, name: "BASE", page: "FLTR2", cc: 27, nrpn: [1, 24], kind: "cont", hires: true },
  { id: 84, name: "WIDTH", page: "FLTR2", cc: 28, nrpn: [1, 25], kind: "cont" },
  { id: 85, name: "FILTER RESET", page: "FLTR2", cc: 25, nrpn: [1, 68], kind: "enum", n: 2 },
  // AMP (C.5)
  { id: 87, name: "AMP ATK", page: "AMP", cc: 84, nrpn: [1, 30], kind: "cont" },
  { id: 88, name: "AMP HOLD", page: "AMP", cc: 85, nrpn: [1, 31], kind: "cont" },
  { id: 89, name: "AMP DEC", page: "AMP", cc: 86, nrpn: [1, 32], kind: "cont" },
  { id: 90, name: "AMP SUS", page: "AMP", cc: 87, nrpn: [1, 33], kind: "cont" },
  { id: 91, name: "AMP REL", page: "AMP", cc: 88, nrpn: [1, 34], kind: "cont", hires: true },
  { id: 97, name: "AMP MODE", page: "AMP", cc: 91, nrpn: [1, 40], kind: "enum", n: 2 },
  { id: 98, name: "AMP RESET", page: "AMP", cc: 92, nrpn: [1, 41], kind: "enum", n: 2 },
  { id: 95, name: "PAN", page: "AMP", cc: 89, nrpn: [1, 38], kind: "bip" },
  { id: 96, name: "VOL", page: "AMP", cc: 90, nrpn: [1, 39], kind: "cont", hires: true },
  // FX (C.7)
  { id: 101, name: "BIT REDUCTION", page: "FX", cc: 78, nrpn: [1, 5], kind: "enum", n: 128 },
  { id: 102, name: "SRR", page: "FX", cc: 79, nrpn: [1, 6], kind: "cont" },
  { id: 103, name: "SRR ROUTING", page: "FX", cc: 80, nrpn: [1, 7], kind: "enum", n: 2 },
  { id: 104, name: "OVERDRIVE", page: "FX", cc: 81, nrpn: [1, 8], kind: "cont", hires: true },
  { id: 106, name: "OD ROUTING", page: "FX", cc: 82, nrpn: [1, 9], kind: "enum", n: 2 },
  { id: 92, name: "CHORUS SEND", page: "FX", cc: 29, nrpn: [1, 35], kind: "cont", hires: true },
  { id: 93, name: "DELAY SEND", page: "FX", cc: 30, nrpn: [1, 36], kind: "cont", hires: true },
  { id: 94, name: "REVERB SEND", page: "FX", cc: 31, nrpn: [1, 37], kind: "cont", hires: true },
  // TRIG (C.2): portamento
  { id: 99, name: "PORTAMENTO", page: "TRIG", cc: 65, nrpn: [3, 7], kind: "enum", n: 2 },
  { id: 100, name: "PORTA TIME", page: "TRIG", cc: 9, nrpn: [3, 6], kind: "cont" },
];
// LFO 1..3 (C.8): SPD MULT FADE DEST WAVE SPH MODE DEP; ids 4 x slot + lfo; LFO 3 has no CC (NRPN only)
const LFO_SLOTS = [["SPEED", "bip", 0, true], ["MULT", "enum", 24], ["FADE", "bip"], ["DEST", "enum", 105], ["WAVE", "enum", 7],
  ["PHASE", "cont"], ["MODE", "enum", 5], ["DEPTH", "bip", 0, true]];
const LFO_CC = [[102, 103, 104, 105, 106, 107, 108, 109], [111, 112, 113, 114, 115, 116, 117, 118], [null, null, null, null, null, null, null, null]];
const LFO_NRPN = [[42, 43, 44, 45, 46, 47, 48, 49], [50, 51, 52, 53, 54, 55, 56, 57], [58, 59, 60, 61, 62, 70, 71, 72]];
for (let l = 0; l < 3; l++) LFO_SLOTS.forEach(([nm, kind, n, hires], s) => PARAMS.push({ id: 4 * s + l + 1, name: `LFO${l + 1} ${nm}`, page: `LFO${l + 1}`,
  cc: LFO_CC[l][s], nrpn: [1, LFO_NRPN[l][s]], kind, ...(n ? { n } : {}), ...(hires ? { hires } : {}) }));

export const byId = new Map(PARAMS.map((p) => [p.id, p]));
export const param = (id) => byId.get(id);
export const recordOffset = (id) => 28 + 2 * id;          // the coarse byte; the fine byte follows

// the values a parameter is swept through. CC: every value for an enum (the CC -> step law, every step), a spread for
// the rest; NRPN: a few steps of an enum, fractions for a hi-res one (14 bits: the stored u16 >> 1)
const CC_SPREAD = [0, 1, 2, 8, 16, 32, 48, 63, 64, 65, 80, 96, 112, 126, 127];
const NRPN_HIRES = [0, 1, 37, 64, 127, 128, 4096, 8192, 8229, 8256, 12345, 16383];
export function sweepOf(p) {
  const cc = p.cc === null || p.cc === undefined ? [] :
    p.kind === "enum" || p.kind === "rpair" ? Array.from({ length: 128 }, (_, i) => i) : CC_SPREAD;
  let nrpn;
  if (p.kind === "rpair") nrpn = [0, 1, 18, 19, 20, 100, 180, 341, 360, 361, 400];
  else if (p.kind === "enum") nrpn = [...new Set([0, 1, 2, 3, Math.min(p.n - 1, 7), p.n - 1].map((i) => i << 7))];
  else if (p.hires) nrpn = NRPN_HIRES;
  else nrpn = [0, 64 << 7, 127 << 7];                       // (the NRPN law of a coarse-only value: three points)
  if (p.cc === null || p.cc === undefined) nrpn = p.kind === "enum" ? Array.from({ length: p.n }, (_, i) => i << 7) : NRPN_HIRES;
  return { cc, nrpn };
}
