// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
// tools/dn2pst.mjs's self-test (research/DN2-FORMAT.md).
//
//   node tools/test_dn2pst.mjs [DN1_DIR] [DN2_DIR]
//
// 1. A synthetic Digitone II FM TONE sound built from the field table (dn2RecordFromParams), wrapped as a .dn2pst
//    (container, LZ4 and raw, ZIP), read back: every value, the name, the tags, the checks; and the refusals.
// 2. DN1_DIR (optional; the owner's version-5 .dnsnd exports, read in place, never copied): each DN1 record carried onto
//    a DN2 record by the FM TONE part of DNX's DN1 -> DN2 map (packages/core/src/project/soundmap.ts, derived from
//    Elektron's own conversions), then imported by dn2pst.mjs, must give the same 73 FM TONE values as ChoralRoot's
//    dnsndToQuad, apart from the differences listed (DN2-only values, and RATIO A's table).
// 3. DN2_DIR (optional; real .dn2pst files): each one read and converted, the result printed.
import { readFileSync, readdirSync, existsSync } from "node:fs";
import { join } from "node:path";
import { parseDn2pst, dn2pstToQuad, dn2RecordFromParams, wrapDn2Record, dn2pstZip, DN2_FIELDS, DN2_TAGS, QP, QX } from "./dn2pst.mjs";
import { parseDnsnd, dnsndToQuad } from "../site/js/fm1dnsnd.js";

let fails = 0, checks = 0;
const check = (ok, msg) => { checks++; if (!ok) { fails++; console.log(`  FAIL ${msg}`); } };
const [dn1Dir, dn2Dir] = process.argv.slice(2);

// ---- 1. synthetic
const want = {
  "lfo1.speed": 23.25, "lfo2.speed": -12.5, "lfo3.speed": 48, "lfo1.mult": 7, "lfo2.mult": 3, "lfo3.mult": 12,
  "lfo1.fade": -10, "lfo2.fade": 20, "lfo3.fade": 0, "lfo1.dest": 74, "lfo2.dest": 1, "lfo3.dest": 2,
  "lfo1.wave": 4, "lfo2.wave": 1, "lfo3.wave": 6, "lfo1.phase": 32, "lfo2.phase": 0, "lfo3.phase": 127,
  "lfo1.mode": 1, "lfo2.mode": 4, "lfo3.mode": 0, "lfo1.depth": 30.5, "lfo2.depth": -24.75, "lfo3.depth": 63.75,
  algo: 5, ratioC: 2, ratioA: 5.5, ratioB: 3 * 19 + 7, harm: -12.5, dtun: 33.25, fdbk: 90, mix: -20, phrt: 2,
  "envA.atk": 1, "envA.dec": 60, "envA.end": 10, "envA.lev": 100, "envB.atk": 2, "envB.dec": 70, "envB.end": 20, "envB.lev": 90,
  "envA.delay": 5, "envA.trig": 1, "envA.reset": 0, "envB.delay": 6, "envB.trig": 0, "envB.reset": 1,
  ofsC: 32, ofsA: -16, ofsB1: 0, ofsB2: -64, keyA: 40, keyB1: 30, keyB2: 20,
  "filter.type": 0, "filter.freq": 80.5, "filter.reso": 20.25, "filter.env": -30, "filter.delay": 3, "filter.atk": 4,
  "filter.dec": 50, "filter.sus": 60, "filter.rel": 127, "filter.keytrack": 50, "filter.base": 10.75, "filter.width": 100,
  "filter.reset": 1, "amp.atk": 8, "amp.hold": 127, "amp.dec": 40, "amp.sus": 90, "amp.rel": 45.5,
  "send.chorus": 10, "send.delay": 20, "send.reverb": 30, "amp.pan": -15, "amp.vol": 100, "amp.mode": 1, "amp.reset": 1,
  "fx.overdrive": 12,
};
const tagBits = (1 << DN2_TAGS.indexOf("BASS")) | (1 << DN2_TAGS.indexOf("DARK"));
const rec = dn2RecordFromParams(want, { name: "SYNTH BASS ÅB", tagBits });
const man = (payload) => ({ FormatVersion: "1.0", ProductType: [], Payload: "SYNTH BASS ÅB", FileType: "Sound",
  FirmwareVersion: "1.10E", MetaInfo: { Tags: ["BASS", "DARK"] } });
for (const compress of [true, false]) {
  const pst = dn2pstZip(wrapDn2Record(rec, { compress }), man());
  const s = await parseDn2pst(pst);
  check(s.name === "SYNTH BASS ÅB" && s.machineName === "FM TONE" && s.filterMachineName === "MULTI-MODE" && s.version === 2,
        `head: ${s.name} ${s.machineName} ${s.filterMachineName} v${s.version}`);
  check(s.tags.join() === "BASS,DARK" && s.fizzTags.join() === "BASS", `tags ${s.tags} -> ${s.fizzTags}`);
  check(s.warnings.length === 0, `no warnings (${s.warnings.join("; ")})`);
  for (const [k, v] of Object.entries(want)) check(s.params[k] === v, `${compress ? "lz4" : "raw"} ${k}: ${s.params[k]} != ${v}`);
  const q = dn2pstToQuad(s), p = q.values;
  check(q.blob.length === 96 && q.blob[0] === 0x51 && q.blob[1] === 5, "the blob's head");
  check(q.name === "SYNTH BASS A" && q.drive === 12 && q.pan === -15, `name ${q.name}, drive ${q.drive}, pan ${q.pan}`);
  check(q.sends.chorus === 10 && q.sends.delay === 20 && q.sends.reverb === 30, `sends ${JSON.stringify(q.sends)}`);
  check(p[QP.ALGO] === 5 && p[QP.RC] === 4 && p[QP.RB1] === 3 && p[QP.RB2] === 7 && p[QP.HARM] === -1250 && p[QP.DTUN] === 3325,
        "SYN 1 values");
  check(p[QP.OFSC] === 50 && p[QP.OFSA] === -25 && p[QP.OFSB2] === -100, "ratio offsets");
  check(p[QP.AKEY] === 40 && p[QP.B1KEY] === 30 && p[QP.B2KEY] === 20 && p[QP.FKTRK] === 64, "key tracks");
  check(p[QP.FTYPE] === 4 && p[QP.FREQ] === 8050 && p[QP.FDEPTH] === -3000 && p[QP.FREL] === 127, "filter (E2: MULTI-MODE as FIZZ's MULTI, TYPE 0)");
  check(p[QP.EDEC] === 40 && p[QP.ESUS] === 90 && p[QP.EREL] === 46 && p[QP.LEVEL] === 100, "amp (ADSR)");
  check(p[47 + 1] === 13 && p[55 + 1] === 3 && p[63 + 1] === 6, `MULT 7 / 3 / 12 -> ${p[48]} ${p[56]} ${p[64]} (fixed x2, BPM x8, BPM x64)`);
  check(p[47 + 3] === 25 && p[55 + 3] === 40 && p[63 + 3] === 0, `LFO dests ${p[50]} ${p[58]} ${p[66]} (FREQ, L1SPD, none)`);
  check(p[QP.RA] === 21, `RATIO A 5.5 -> ${p[QP.RA]} (21: 22 quarters)`);
  check(q.notes.some((n) => /LFO 3 destination LFO2 SPD/.test(n)), "LFO 3 -> LFO 2: noted");
}
{   // the other inputs: a +Drive read (the payload), the body, a bare record; AHD; machines refused
  const pl = wrapDn2Record(rec);
  check((await parseDn2pst(pl)).params.algo === 5, "a bare payload");
  check((await parseDn2pst(pl.subarray(31 + 4 + 1 + 2, 31 + 4 + 1 + 2 + 364))).params.fdbk === 90, "a 364-byte body");
  check((await parseDn2pst(rec)).params.mix === -20, "a bare 359-byte record");
  const ahd = dn2RecordFromParams({ ...want, "amp.mode": 0, "amp.hold": 127, "amp.dec": 70 });
  const qa = dn2pstToQuad(await parseDn2pst(ahd));
  check(qa.ext[QX.AMOD - 74] === 1 && qa.ext[QX.HOLD - 74] === 127 && qa.values[QP.EDEC] === 70 && qa.six &&
        qa.blob6.length === 128 && qa.blob6[1] === 6 && qa.blob6[88] === 1 && qa.blob6[89] === 127,
        "E1: AHD, HOLD NOTE -> FIZZ's AHD (AMOD 1, HOLD 127, DEC 70), a version-6 blob");
  const pt = dn2pstToQuad(await parseDn2pst(dn2RecordFromParams({ ...want, portamento: 64, "portamento.time": 50 })));
  check(pt.ext[QX.PORT - 74] === 1 && pt.ext[QX.PTIM - 74] === 50 && !pt.notes.some((n) => /PORTAMENTO/.test(n)),
        "E1: PORTAMENTO on, TIME 50 -> PORT ON, TIME 50");
  const od = dn2RecordFromParams(want); od[240] = 1;
  const qod = dn2pstToQuad(await parseDn2pst(od)), qpre = dn2pstToQuad(await parseDn2pst(dn2RecordFromParams(want)));
  check(!qod.ext[QX.DRPRE - 74] && qpre.ext[QX.DRPRE - 74] === 1 && qpre.blob6[87] === 12,
        "E1: OD ROUTING 1 -> DRIVE after the voice, 0 -> before the filter; DRIVE at byte 87");
  // the OCTAVE -12 halves each ratio: C 2 -> 1 (3), A 5.5 -> 2.75 (10), B1 1 -> 0.5 (1)
  const qo = dn2pstToQuad(await parseDn2pst(dn2RecordFromParams({ ...want, octave: -12 })));
  check(qo.values[QP.RC] === 3 && qo.values[QP.RA] === 10 && qo.values[QP.RB1] === 1 && qo.notes.some((n) => /OCTAVE -1/.test(n)),
        `octave: C ${qo.values[QP.RC]} A ${qo.values[QP.RA]} B1 ${qo.values[QP.RB1]}; ${qo.notes.join(" | ")}`);
  const lp4 = dn2RecordFromParams(want); lp4[245] = 1;
  check(dn2pstToQuad(await parseDn2pst(lp4)).values[QP.FTYPE] === 3, "LOWPASS 4 -> LP24");
  const comb = dn2RecordFromParams(want); comb[245] = 5;
  {   // E4: the modulation records into the slots: VEL (records 4..7) -> FREQ +32, PB (0) -> HARM -20; nine: one lost
    const mr = dn2RecordFromParams(want);
    mr[252 + 3 * 4] = 80, mr[252 + 3 * 4 + 2] = 74;        // VEL -> FREQ, (80 - 64) x 2 = +32
    mr[252] = 54, mr[254] = 37;                             // PB -> HARM, (54 - 64) x 2 = -20
    const qm = dn2pstToQuad(await parseDn2pst(mr)), M = (s, f) => qm.ext[QX.MOD0 + 3 * s + f - 74];
    check(M(0, 0) === 5 && M(0, 1) === 11 && M(0, 2) === -20 && M(1, 0) === 1 && M(1, 1) === 25 && M(1, 2) === 32 && qm.six,
          `MOD: PB -> HARM -20, VEL -> FREQ +32 (${[0, 1].map((s) => [0, 1, 2].map((f) => M(s, f)).join("/")).join(", ")})`);
    for (let k = 0; k < 9; k++) mr[252 + 3 * (k + 8)] = 70, mr[252 + 3 * (k + 8) + 2] = 74;
    const q9 = dn2pstToQuad(await parseDn2pst(mr));
    check(q9.notes.some((n) => /not imported/.test(n)), "more than eight: noted");
  }
  {   // E2: every machine: COMB+ 5, COMB- 6 (LPF FSHAPE, FDBK RESO), EQ 7 (Q FSHAPE, GAIN RESO's coarse - 64), BW ROUTING
    const qc = dn2pstToQuad(await parseDn2pst(comb));
    check(qc.values[QP.FTYPE] === 5 && qc.ext[QX.FSHAPE - 74] === 0 && qc.values[QP.RESO] === 2025 && qc.six, "COMB+ -> 5");
    const cn = dn2RecordFromParams({ ...want, "filter.type": 90 }); cn[245] = 3;
    const qn = dn2pstToQuad(await parseDn2pst(cn));
    check(qn.values[QP.FTYPE] === 6 && qn.ext[QX.FSHAPE - 74] === 90, "COMB- -> 6, LPF 90");
    const eq = dn2RecordFromParams({ ...want, "filter.type": 40, "filter.reso": 80 }); eq[245] = 2; eq[238] = 1;
    const qe = dn2pstToQuad(await parseDn2pst(eq));
    check(qe.values[QP.FTYPE] === 7 && qe.ext[QX.FSHAPE - 74] === 40 && qe.ext[QX.FGAIN - 74] === 16 &&
          qe.ext[QX.BWPOST - 74] === 1, `EQUALIZER -> 7, Q 40, GAIN 80 - 64 = ${qe.ext[QX.FGAIN - 74]}; BW ROUTING 1: after`);
    const mm = dn2RecordFromParams({ ...want, "filter.type": 64 });
    const qm = dn2pstToQuad(await parseDn2pst(mm));
    check(qm.values[QP.FTYPE] === 4 && qm.ext[QX.FSHAPE - 74] === 64, "MULTI-MODE TYPE 64 -> MULTI, its morph 64");
  }
  const drum = dn2RecordFromParams(want); drum[244] = 2;
  let err = null;
  try { await parseDn2pst(drum); } catch (e) { err = e; }
  check(err && err.code === "DN2_MACHINE" && /FM DRUM/.test(err.message), `FM DRUM refused: ${err && err.message}`);
  check((await parseDn2pst(drum, { anyMachine: true })).params === null, "anyMachine: the head, no params");
  const dn1 = wrapDn2Record(rec); dn1[8] = 9;
  err = null; try { await parseDn2pst(dn1); } catch (e) { err = e; }
  check(err && /Digitone 1/.test(err.message), "a DN1 container refused");
  const bad = wrapDn2Record(rec); bad[40] ^= 1;
  check((await parseDn2pst(bad)).warnings.some((w) => /check field/.test(w)), "a damaged payload: the CRC warns");
}

// ---- 2. DN1 sounds through DNX's DN1 -> DN2 map (the FM TONE part), against ChoralRoot's dnsndToQuad
// DNX soundmap.ts SELECTOR_TABLE (an LFO destination: DN1's number -> DN2's); a gap between two equal deltas is filled
const SEL = new Map([[0, 0], [1, 1], [2, 2], [3, 5], [4, 6], [5, 9], [9, 17], [10, 18], [11, 21], [15, 29], [16, 30], [17, 33],
  [18, 34], [19, 35], [20, 36], [21, 37], [22, 38], [23, 39], [24, 40], [27, 43], [29, 45], [30, 46], [32, 48], [34, 50],
  [42, 58], [43, 59], [44, 60], [45, 61], [46, 62], [49, 65], [51, 74], [52, 75], [53, 76], [54, 78], [55, 79], [57, 81],
  [58, 83], [59, 84], [60, 87], [61, 89], [62, 90], [63, 91], [64, 104], [65, 95], [66, 96], [67, 94], [68, 93], [69, 92], [73, 66]]);
// + what that table leaves open and the parameter ids settle: DN1's LFO slots 1 + 2s / 2 + 2s are ids 1 + 4s / 2 + 4s
// (C098 turns LFO 1's MODE, DN1 13 -> id 25), and RATIO OFFSET A, B1, B2 follow C's 73 -> 66 (C070..C072: 74..76 -> 67..69)
for (let s = 0; s < 8; s++) { SEL.set(1 + 2 * s, 1 + 4 * s); SEL.set(2 + 2 * s, 2 + 4 * s); }
[[74, 67], [75, 68], [76, 69], [77, 77]].forEach(([a, b]) => SEL.set(a, b));
const sel = (v) => {
  if (SEL.has(v)) return SEL.get(v);
  const ks = [...SEL.keys()], b = Math.max(...ks.filter((k) => k < v)), a = Math.min(...ks.filter((k) => k > v));
  return Number.isFinite(a) && SEL.get(b) - b === SEL.get(a) - a ? v + SEL.get(b) - b : v;
};
function dn1ToDn2(d1) {
  const d2 = dn2RecordFromParams({}, { version: 2 });
  for (let i = 8; i < 28; i++) d2[i] = d1[i];
  let z = false; for (let i = 12; i < 28; i++) { if (!d1[i]) z = true; if (z) d2[i] = 0; }   // the name to its NUL only
  const cp = (a, b, n) => { for (let i = 0; i < n; i++) d2[a + i] = d1[b + i]; };
  for (let s = 0; s < 8; s++) for (let k = 0; k < 2; k++) cp(30 + 8 * s + 2 * k, 30 + 4 * s + 2 * k, 2);
  d2[54] = sel(d1[42]); d2[56] = sel(d1[44]); d2[55] = 0; d2[57] = 0;
  cp(94, 62, 50); cp(160, 174, 8); d2[168] = d1[274]; d2[170] = d1[275]; d2[172] = d1[276];
  cp(176, 130, 6); cp(182, 182, 2); cp(184, 136, 8); d2[192] = d1[277]; cp(194, 144, 4);
  cp(202, 148, 2); cp(206, 150, 6); cp(236, 156, 2); cp(218, 158, 2); cp(220, 160, 2);
  cp(216, 162, 2); cp(214, 164, 2); cp(212, 166, 2); cp(224, 168, 6);
  d2[174] = [3, 0, 1, 0][d1[128]] ?? d1[128]; d2[245] = [4, 4, 4, 1][d1[128]] ?? 4;
  d2[250] = d1[279]; d2[251] = d1[280];
  return d2;
}
if (dn1Dir && existsSync(dn1Dir)) {
  const files = readdirSync(dn1Dir).filter((f) => f.endsWith(".dnsnd")).sort();
  const diff = new Map(), DN2_ONLY = new Set([QP.AKEY, QP.B1KEY, QP.B2KEY, QP.FKTRK, ...Array.from({ length: 8 }, (_, i) => 63 + i)]);
  let n = 0, raSteps = 0, multDiff = 0;
  for (const f of files) {
    const a = await parseDnsnd(new Uint8Array(readFileSync(join(dn1Dir, f))));
    if (a.version !== 5) continue;                     // DNX's map is of version-5 DN1 sounds
    n++;
    const qa = dnsndToQuad(a), b = await parseDn2pst(dn2pstZip(wrapDn2Record(dn1ToDn2(a.record)), man()));
    const qb = dn2pstToQuad(b);
    check(b.name === a.name, `${f}: the name ${b.name}`);
    const raInterp = a.record[66] > 15 && ![25, 35].includes(a.record[66]);
    if (raInterp) raSteps++;
    for (let i = 0; i < 73; i++) {
      if (qa.values[i] === qb.values[i] || DN2_ONLY.has(i) || (i === QP.RA && raInterp)) continue;
      if ((i === 48 || i === 56) && qa.values[i] >= 6) { multDiff++; continue; }   // MULT 6..20: the two readers differ
      diff.set(i, (diff.get(i) || []).concat(`${f}: ${qa.values[i]} / ${qb.values[i]}`));
    }
    check(qa.drive === qb.drive && qa.pan === qb.pan, `${f}: drive ${qa.drive}/${qb.drive}, pan ${qa.pan}/${qb.pan}`);
  }
  for (const [i, l] of diff) check(false, `value ${i}: ${l.length} sound(s) differ, e.g. ${l.slice(0, 3).join("; ")}`);
  console.log(`  DN1 -> DN2: ${n} version-5 sounds compared (${raSteps} with a RATIO A step the DN1 reader interpolates, ${multDiff} LFO MULT ${multDiff === 1 ? "value" : "values"} 6..20 read differently)`);
}

// ---- 3. real .dn2pst files
if (dn2Dir && existsSync(dn2Dir)) {
  for (const f of readdirSync(dn2Dir).filter((x) => /\.dn2pst$/i.test(x)).sort()) {
    try {
      const s = await parseDn2pst(new Uint8Array(readFileSync(join(dn2Dir, f))), { anyMachine: true });
      if (s.machine !== 0) { console.log(`  ${f}: ${s.machineName} (not imported) "${s.name}" [${s.tags}]`); continue; }
      const q = dn2pstToQuad(s);
      console.log(`  ${f}: "${q.name}" v${s.version} ${s.filterMachineName} [${s.tags}] -> ${s.fizzTags}; drive ${q.drive}; ${q.notes.join(" | ")}`);
    } catch (e) { console.log(`  ${f}: REFUSED ${e.message}`); }
  }
}
console.log(`test_dn2pst: ${checks} checks, ${fails} failed`);
process.exit(fails ? 1 : 0);
