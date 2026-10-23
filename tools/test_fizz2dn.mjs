// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
// tools/fizz2dn.mjs's checks (research/DN2-FORMAT.md "Export").
//
//   node tools/test_fizz2dn.mjs [DN1_DIR] [DN2_DIR]      (the owner's files, read in place, never copied)
//
// 1. Elektron's LZ4: lz4Elektron makes the very block of every .dnsnd under DN1_DIR and every .dn2pst under DN2_DIR.
// 2. Every DN1 file: imported (ChoralRoot's dnsndToQuad), exported, imported again: the same FIZZ patch; and, for a
//    version-5 original, the exported record and file against the original, byte for byte (the differences listed).
// 3. Random FIZZ patches through both formats and both readers: the same patch (within what each format holds).
// 4. The files themselves: the container (CRC, lengths, the LZ4 chain) read by both readers, the ZIP checked by
//    `unzip -t`, the structure against BASSSINE.dn2pst.
import { readFileSync, readdirSync, statSync, writeFileSync, mkdtempSync } from "node:fs";
import { join } from "node:path";
import { tmpdir } from "node:os";
import { execFileSync } from "node:child_process";
import { fizzToDnsnd, fizzToDn2pst, lz4Elektron, fizzValues, Q, NP, defaults } from "./fizz2dn.mjs";
import { parseDn2pst, dn2pstToQuad, readContainer, DN2_RATIO_A, DN2_TAGS, QUAD_RANGES, QUAD_EXT_RANGES } from "./dn2pst.mjs";
import { parseDnsnd, dnsndToQuad, dnReadZip, dnLz4Block } from "../site/js/fm1dnsnd.js";

const [dn1Dir = "../DigitonePresets", dn2Dir = "../Digitone2Presets"] = process.argv.slice(2);
let fails = 0, checks = 0;
const check = (ok, msg) => { checks++; console.log(`  ${ok ? "ok  " : "FAIL"} ${msg}`); if (!ok) fails++; };
const walk = (d, ext) => { try { return readdirSync(d).flatMap((f) => { const p = join(d, f); return statSync(p).isDirectory() ? walk(p, ext) : f.endsWith(ext) ? [p] : []; }); } catch (_) { return []; } };
const eq = (a, b) => a.length === b.length && a.every((x, i) => x === b[i]);
const u32 = (b, o) => (b[o] << 24 | b[o + 1] << 16 | b[o + 2] << 8 | b[o + 3]) >>> 0;
const DATE = new Date(2026, 9, 22, 12, 0, 0);
const tmp = mkdtempSync(join(tmpdir(), "fizz2dn-"));
const unzipOk = (bytes, name) => { const f = join(tmp, name); writeFileSync(f, bytes); try { execFileSync("unzip", ["-tq", f], { stdio: "pipe" }); return true; } catch (_) { return false; } };

// ---- 1. the LZ4 compressor against the instruments' own blocks
const dn1Files = walk(dn1Dir, ".dnsnd"), dn2Files = walk(dn2Dir, ".dn2pst");
let lzOk = 0, lzN = 0;
for (const f of [...dn1Files, ...dn2Files]) {
  const z = await dnReadZip(new Uint8Array(readFileSync(f))), man = JSON.parse(new TextDecoder().decode(await z.get("manifest.json")));
  const pl = await z.get(man.Payload), len = u32(pl, 25), size = u32(pl, 31);
  const { data } = dnLz4Block(pl, 35, len);
  lzN++;
  if (eq(lz4Elektron(data), pl.subarray(35, 35 + size))) lzOk++; else console.log(`  LZ4 differs: ${f}`);
}
check(lzN > 0 && lzOk === lzN, `Elektron's LZ4: ${lzOk} of ${lzN} blocks made byte for byte`);

// ---- 2. every DN1 file through FIZZ and back
const sendsOf = (P) => ({ reverb: Math.round(P["send.reverb"] || 0), delay: Math.round(P["send.delay"] || 0), chorus: Math.round(P["send.chorus"] || 0) });
const byteDiff = new Map(), why = {};
let n2 = 0, same = 0, raSkip = 0, v5n = 0, v5same = 0, manSame = 0, hdrSame = 0, old132 = 0;
for (const f of dn1Files) {
  const bytes = new Uint8Array(readFileSync(f)), a = await parseDnsnd(bytes), qa = dnsndToQuad(a);
  const bits = u32(a.record, 8), recTags = DN2_TAGS.filter((_, t) => (bits >>> t) & 1);   // (the record's own tag bits)
  const sound = { values: qa.values, drive: qa.drive, pan: qa.pan, sends: sendsOf(a.params), name: a.name, dnTags: recTags };
  const out = fizzToDnsnd(sound, { date: DATE }), b = await parseDnsnd(out.bytes), qb = dnsndToQuad(b);
  n2++;
  // the RATIO A steps ChoralRoot's reader interpolates (16..24, 26..34) are not the firmware's: compared apart
  const interp = (st) => st > 15 && st !== 25 && st !== 35;
  const ra = interp(a.record[66]) || interp(out.record[66]);
  if (ra) raSkip++;
  const diff = qa.values.map((x, i) => (x === qb.values[i] || (ra && i === Q.RA) ? -1 : i)).filter((i) => i >= 0);
  if (!diff.length && qb.drive === qa.drive && qb.pan === qa.pan && b.name === a.name) same++;
  else if (n2 - same <= 5) console.log(`  ${f.split("/").pop()}: values ${diff.join(",")} differ (${diff.map((i) => `${qa.values[i]}->${qb.values[i]}`).join(" ")})`);
  if (a.version === 5) {                                  // byte for byte against the original
    v5n++;
    if (a.firmware !== "1.42A") old132++;
    const r0 = a.record, r1 = out.record;
    let d = 0;
    for (let o = 0; o < 302; o++) if (r0[o] !== r1[o]) { d++; byteDiff.set(o, (byteDiff.get(o) || 0) + 1); }
    if (!d) v5same++;
    const z = await dnReadZip(bytes), m0 = new TextDecoder().decode(await z.get("manifest.json"));
    const m1 = new TextDecoder().decode(await (await dnReadZip(out.bytes)).get("manifest.json"));
    if (m0 === m1) manSame++; else if (a.firmware === "1.42A") console.log(`  manifest differs: ${JSON.stringify(m0)} vs ${JSON.stringify(m1)}`);
    const p0 = await z.get(JSON.parse(m0).Payload);
    if (eq(p0.subarray(0, 21), out.payload.subarray(0, 21)) && eq(p0.subarray(25, 31), out.payload.subarray(25, 31))) hdrSame++;
  }
}
check(same === n2, `DN1 files: ${same} of ${n2} come back as the same FIZZ patch (${raSkip} with a RATIO A step ChoralRoot's reader interpolates, compared without it)`);
check(manSame === v5n - old132 && hdrSame === v5n - old132, `version-5 originals from Transfer 1.42A: the manifest identical in ${manSame} of ${v5n - old132}, the payload header (but its slot) in ${hdrSame} (${old132} from an older Transfer: "1.32A", no tags, build 0070)`);
console.log(`  version-5 originals: ${v5same} of ${v5n} records byte-identical; the bytes that differ, and in how many files:`);
const groups = [[[31, 33, 59, 61, 71, 73, 131, 133, 135, 145, 155, 157, 161, 163, 165, 167, 175, 177, 179, 181], "a fine byte: FIZZ keeps hundredths, the Digitone 1/256ths"],
  [[110, 170, 172, 173, 188], "not in FIZZ (DN1 +110; PORT / PTIM read by no importer; +173, +188)"], [[...Array.from({ length: 60 }, (_, i) => 190 + i)], "the modulation records (not decoded)"],
  [[...Array.from({ length: 23 }, (_, i) => 250 + i)], "the arpeggiator"], [[273, 278, 279, 280, 281, 282, 283, 284, 285, 286, 287, ...Array.from({ length: 10 }, (_, i) => 288 + i)], "the tail: octave, unknown settings"],
  [[274, 275, 276, 277], "key tracks (not read by ChoralRoot's importer, so 0 in FIZZ)"], [[...Array.from({ length: 16 }, (_, i) => 12 + i)], "the name: bytes after its end, or folded"],
  [[162, 164, 166], "the sends (FIZZ's are whole numbers)"],
  [[64, 66, 68, 69, 174, 176, 178, 180], "the ratios: the OCTAVE folded into them, RATIO B's low bit, the offsets in FIZZ's hundredths of a ratio"],
  [[154, 156, 160], "AMP REL, DRIVE, VOL as FIZZ's whole numbers"], [[140], "filter SUS 127 for a DEC INF (the importer's rule)"],
  [[168], "AMP ENV RESET off (FIZZ always resets)"], [[8, 9, 10, 11], "the tag bits"]];
const seen = new Set();
for (const [offs, label] of groups) {
  const hit = offs.filter((o) => byteDiff.has(o));
  hit.forEach((o) => seen.add(o));
  if (hit.length) console.log(`    ${label}: ${hit.map((o) => `+${o} (${byteDiff.get(o)})`).join(" ")}`);
}
const other = [...byteDiff.keys()].filter((o) => !seen.has(o)).sort((x, y) => x - y);
check(!other.length, `no other byte differs${other.length ? ": " + other.map((o) => `+${o} (${byteDiff.get(o)})`).join(" ") : ""}`);

// ---- 3. random FIZZ patches
let seed = 7;
const rnd = (a, b) => { seed = (seed * 1103515245 + 12345) & 0x7fffffff; return a + (seed % (b - a + 1)); };
const RANGES = [...QUAD_RANGES, [0, 127, 0], ...QUAD_EXT_RANGES];
function randomPatch({ dn1 }) {
  const v = defaults();
  for (let i = 0; i < 73; i++) v[i] = rnd(RANGES[i][0], RANGES[i][1]);
  v[Q.DRIVE] = rnd(0, 127);
  // the values each format holds exactly (the rest are the notes' cases, checked below)
  const raSteps = DN2_RATIO_A.map((x, i) => [x, i]).filter(([x, i]) => !dn1 || i <= 15 || i === 25 || i === 35);
  v[Q.RA] = Math.round(raSteps[rnd(0, raSteps.length - 1)][0] * 4) - 1;
  if (v[Q.FDEC] === 127) v[Q.FSUS] = 127;
  for (const k of [0, 1, 2]) { const d = 47 + 8 * k + 3; if (k === 0 && v[d] >= 40 && v[d] <= 46) v[d] = 0; }
  if (dn1) {
    v[Q.VEL] = 64; v[Q.AKEY] = v[Q.B1KEY] = v[Q.B2KEY] = v[Q.FKTRK] = 0;
    for (let f = 0; f < 8; f++) v[47 + 16 + f] = RANGES[47 + 16 + f][2];   // LFO 3 at its defaults
  } else {
    v[Q.VEL] = 64;
    v[Q.FKTRK] = Math.round(rnd(0, 100) * 127 / 100);
    v[Q.AMOD] = rnd(0, 1); v[Q.HOLD] = rnd(0, 127); v[Q.DRPRE] = rnd(0, 1);
    if (rnd(0, 1)) { v[Q.PORT] = 1; v[Q.PTIM] = rnd(0, 127); }
    if (rnd(0, 1)) { v[Q.BITS] = rnd(1, 127); v[Q.SRR] = rnd(0, 127); v[Q.CRPOST] = rnd(0, 1); }
  }
  for (let i = 0; i < NP; i++) v[i] = Math.max(RANGES[i][0], Math.min(RANGES[i][1], v[i]));
  return v;
}
let r1 = 0, r2 = 0;
const N = 400;
for (let t = 0; t < N; t++) {
  const v = randomPatch({ dn1: true }), pan = rnd(-64, 63), sends = { reverb: rnd(0, 127), delay: rnd(0, 127), chorus: rnd(0, 127) };
  const o = fizzToDnsnd({ values: v, name: `RND ${t}`, tags: ["BASS"], pan, sends }, { date: DATE }), b = await parseDnsnd(o.bytes), q = dnsndToQuad(b);
  const d = v.slice(0, 73).map((x, i) => (x === q.values[i] ? -1 : i)).filter((i) => i >= 0);
  const ok = !d.length && q.drive === v[Q.DRIVE] && q.pan === pan && Math.round(b.params["send.delay"]) === sends.delay && b.tags.join() === "BASS";
  if (ok) r1++; else if (t - r1 < 3) console.log(`  random DN1 ${t}: ${d.map((i) => `${i}: ${v[i]} -> ${q.values[i]}`).join(", ")}`);
}
check(r1 === N, `random patches, Digitone: ${r1} of ${N} back as they were`);
for (let t = 0; t < N; t++) {
  const v = randomPatch({ dn1: false }), pan = rnd(-64, 63), sends = { reverb: rnd(0, 127), delay: rnd(0, 127), chorus: rnd(0, 127) };
  const o = fizzToDn2pst({ values: v, name: `RND ${t}`, tags: ["PAD", "FX"], pan, sends }, { date: DATE });
  const b = await parseDn2pst(o.bytes), q = dn2pstToQuad(b), back = [...q.values, q.drive, ...q.ext];
  const skip = (i) => (i === Q.PTIM && !v[Q.PORT]) || (i === Q.CRPOST && !v[Q.BITS] && !v[Q.SRR]);
  const d = v.map((x, i) => (x === back[i] || skip(i) ? -1 : i)).filter((i) => i >= 0);
  const ok = !d.length && q.pan === pan && q.sends.reverb === sends.reverb && q.sends.chorus === sends.chorus && b.tags.join() === "PAD,SOUND FX" && !b.warnings.length;
  if (ok) r2++; else if (t - r2 < 3) console.log(`  random DN2 ${t}: ${d.map((i) => `${i}: ${v[i]} -> ${back[i]}`).join(", ")} ${b.warnings.join("; ")}`);
}
check(r2 === N, `random patches, Digitone II (AHD, portamento, DRIVE's place, CRUSH, LFO 3, key tracks too): ${r2} of ${N} back as they were`);

// ---- 4. the files themselves
{
  const v = defaults(); v[Q.LEVEL] = 100;
  const s = { values: v, name: "fizz test 1", tags: ["KEYS"], pan: -10, sends: { reverb: 20, delay: 0, chorus: 5 } };
  const d1 = fizzToDnsnd(s, { date: DATE }), d2 = fizzToDn2pst(s, { date: DATE });
  check(unzipOk(d1.bytes, "t.dnsnd") && unzipOk(d2.bytes, "t.dn2pst"), "unzip -t: both ZIPs test OK (CRC, deflate)");
  const w = []; readContainer(d2.payload, w);
  check(!w.length, `the Digitone II payload reads clean (CRC, lengths, the LZ4 chain)${w.length ? ": " + w.join("; ") : ""}`);
  check(d1.payload.length - 43 === u32(d1.payload, d1.payload.length - 8) && d2.payload.length - 43 === u32(d2.payload, d2.payload.length - 8), "the trailers' length fields");
  check(d1.manifest.Payload === "FIZZ TEST 1" && d1.manifest.MetaInfo.Tags.join() === "CHORD", `the name folded (${d1.manifest.Payload}), KEYS -> ${d1.manifest.MetaInfo.Tags}`);
  if (dn2Files.length) {                                   // the structure against a real Digitone II file
    const z = await dnReadZip(new Uint8Array(readFileSync(dn2Files[0]))), m = JSON.parse(new TextDecoder().decode(await z.get("manifest.json")));
    const pl = await z.get(m.Payload), mine = d2.payload;
    const fields = [[0, 9, "magic, header version, family, device"], [13, 17, "content kind"], [25, 31, "body length, compressed, trailer length"]];
    for (const [a, b, what] of fields) check(eq(pl.subarray(a, b), mine.subarray(a, b)), `like ${dn2Files[0].split("/").pop()}: ${what}`);
    check(Object.keys(m).join() === Object.keys(d2.manifest).join() && JSON.stringify(m.ProductType) === "[]", "the manifest's keys, in Transfer's order");
    const b0 = (await parseDn2pst(new Uint8Array(readFileSync(dn2Files[0])))).record, b1 = d2.record;
    const same = [0, 1, 2, 3, 144, 146, 158, 249, 355, 356, 357, 358].every((o) => b0[o] === b1[o]);
    check(same, "the record's fixed bytes (BEEFBACE, the 0x40 ids 58..65, +249, the end mark) as BASSSINE's");
  }
  // what the formats cannot hold is said
  const x = defaults(); x[Q.VEL] = 90; x[47 + 16 + 3] = 25; x[47 + 16 + 7] = 3000; x[Q.AMOD] = 1; x[Q.BITS] = 40; x[Q.MOD0] = 1; x[Q.MOD0 + 1] = 25; x[Q.MOD0 + 2] = 50;
  const n1 = fizzToDnsnd({ values: x, name: "N" }).notes, nn2 = fizzToDn2pst({ values: x, name: "N" }).notes;
  check(n1.some((s) => /VEL/.test(s)) && n1.some((s) => /LFO 3/.test(s)) && n1.some((s) => /AHD/.test(s)) && n1.some((s) => /BIT/.test(s)) && n1.some((s) => /modulation/.test(s)),
    `Digitone notes: ${n1.join(" | ")}`);
  check(nn2.some((s) => /VEL/.test(s)) && !nn2.some((s) => /LFO 3|AHD|BIT|modulation|MOD/.test(s)), `Digitone II notes (E4: the slot written, no note): ${nn2.join(" | ")}`);
  check(fizzValues({ blob: Uint8Array.from([0x51, 5, ...new Array(94).fill(0)]) })[Q.ALGO] === 1, "a version-5 blob is read");
  // the wizard's three test presets (site/js/dn2wizard/testpresets.js, through the site's copy of this exporter)
  const { testPresetFiles } = await import("../site/js/dn2wizard/testpresets.js");
  for (const t of testPresetFiles()) {
    const p = await parseDn2pst(t.bytes), q = dn2pstToQuad(p);
    check(p.machine === 0 && !p.warnings.length && p.name === t.name && p.tags.join() === t.tags.map((x) => ({ KEYS: "CHORD", PAD: "PAD", FX: "SOUND FX", BASS: "BASS" })[x]).join() &&
      unzipOk(t.bytes, "w.dn2pst") && q.values.every((x, i) => x === t.values[i]) && q.ext.every((x, k) => x === t.values[74 + k] || (74 + k === Q.CRPOST)),
      `the wizard's ${t.name}: a clean Digitone II preset that reads back as itself`);
  }
}
// ---- 5. E2: FIZZ's filter machines (FTYPE 4 MULTI, 5 COMB+, 6 COMB-, 7 EQ; FSHAPE, FGAIN, BWPOST) written as the
// Digitone II's machines and read back by dn2pst.mjs: every value the same (on EQ, RESO aside: the machine has none,
// its place holds the GAIN); a Digitone 1 file says each one is approximated
{
  const cases = [[4, 0, 0, 0], [4, 64, 0, 1], [4, 127, 0, 0], [5, 40, 0, 0], [6, 90, 0, 1], [7, 30, -20, 0], [7, 100, 17, 1], [3, 0, 0, 1]];
  for (const [ft, shape, gain, bw] of cases) {
    const v = defaults(); v[Q.LEVEL] = 100; v[Q.RESO] = 4200; v[Q.FREQ] = 8800;
    v[Q.FTYPE] = ft; v[Q.FSHAPE] = shape; v[Q.FGAIN] = gain; v[Q.BWPOST] = bw;
    const d2 = fizzToDn2pst({ values: v, name: "MACHINE" }, { date: DATE });
    const q = dn2pstToQuad(await parseDn2pst(d2.bytes)), back = [...q.values, q.drive, ...q.ext];
    const diff = v.map((x, i) => (x !== back[i] && !(ft === 7 && i === Q.RESO) ? i : -1)).filter((i) => i >= 0);
    const what = ft === 4 ? "MULTI" : ft === 5 ? "COMB+" : ft === 6 ? "COMB-" : ft === 7 ? "EQ" : "LOWPASS 4";
    check(!diff.length && q.six === (shape !== 0 || gain !== 0 || bw !== 0), `${what} (TYPE ${shape}, GAIN ${gain}, BW ${bw ? "after" : "before"}) round-trips through dn2pst.mjs${diff.length ? ": differs at " + diff.join(" ") : ""}`);
    if (ft > 3) {
      const n1 = fizzToDnsnd({ values: v, name: "MACHINE" }).notes;
      check(n1.some((s) => /approximated as (LP2|HP2|off)/.test(s)), `Digitone 1: ${n1.find((s) => /approximated/.test(s))}`);
    }
  }
}
// ---- 6. E4: the eight modulation slots into the Digitone II's modulation records and back through dn2pst.mjs (the
// reader fills the slots in the records' order, PB VEL MW BR AT KEY: compared as a set); FIZZ's own LFO destinations
// (FSHAPE HOLD PTIM BITS SRR) as the DN2's ids 73 88 100 101 102; RAND / F ENV, a destination the DN2 lacks and a fifth
// slot of one source noted and dropped; the Digitone 1 notes the slots
{
  const QDN = { FREQ: 25, HARM: 11, PAN: 39, RESO: 26, FSHAPE: 60, HOLD: 58, PTIM: 59, BITS: 62, SRR: 63, L1DEP: 46 };
  const slotsOf = (vals) => Array.from({ length: 8 }, (_, k) => vals.slice(Q.MOD0 + 3 * k, Q.MOD0 + 3 * k + 3)).filter((s) => s[0] && s[1]).map((s) => s.join(",")).sort();
  const v = defaults(); v[Q.LEVEL] = 100;
  const want = [[1, QDN.FREQ, 32], [1, QDN.HARM, -32], [3, QDN.HARM, 127], [2, QDN.PAN, -127], [4, QDN.RESO, 1], [5, QDN.FSHAPE, -5], [6, QDN.BITS, 64], [3, QDN.SRR, -1]];
  want.forEach(([s, d, x], k) => { v[Q.MOD0 + 3 * k] = s; v[Q.MOD0 + 3 * k + 1] = d; v[Q.MOD0 + 3 * k + 2] = x; });
  [QDN.FSHAPE, QDN.HOLD, QDN.PTIM].forEach((d, k) => { v[47 + 8 * k + 3] = d; v[47 + 8 * k + 7] = 1000 * (k + 1); });
  const d2 = fizzToDn2pst({ values: v, name: "MODS" }, { date: DATE });
  const q = dn2pstToQuad(await parseDn2pst(d2.bytes)), back = [...q.values, q.drive, ...q.ext];
  check(slotsOf(back).join(" ") === slotsOf(v).join(" ") && !d2.notes.some((s) => /MOD|modulation/.test(s)),
    `8 slots (VEL KEY MW AT PB BR, depths -127..127, odd and even) round-trip through dn2pst.mjs: ${slotsOf(back).join(" ")}`);
  check([0, 1, 2].every((k) => back[47 + 8 * k + 3] === v[47 + 8 * k + 3]), `LFO 1..3 aimed at FSHAPE, HOLD, PTIM (ids 73, 88, 100) come back so: ${[0, 1, 2].map((k) => back[47 + 8 * k + 3]).join(" ")}`);
  const w = defaults(); w[Q.LEVEL] = 100;
  [[7, QDN.FREQ, 10], [8, QDN.HARM, 10], [1, 61, 10], [1, QDN.FREQ, 1], [1, QDN.HARM, 2], [1, QDN.PAN, 3], [1, QDN.RESO, 4], [1, QDN.L1DEP, 5]]
    .forEach(([s, d, x], k) => { w[Q.MOD0 + 3 * k] = s; w[Q.MOD0 + 3 * k + 1] = d; w[Q.MOD0 + 3 * k + 2] = x; });
  const n2 = fizzToDn2pst({ values: w, name: "LOST" }).notes;
  check(n2.some((s) => /RAND has no/.test(s)) && n2.some((s) => /F ENV has no/.test(s)) && n2.some((s) => /destination #61: the Digitone II has none/.test(s)) && n2.some((s) => /fifth VEL/.test(s)),
    `what the Digitone II cannot hold is noted: ${n2.filter((s) => /MOD/.test(s)).join(" | ")}`);
  const n1 = fizzToDnsnd({ values: v, name: "MODS" }).notes;
  check(n1.some((s) => /8 modulation slot\(s\): not written/.test(s)), `the Digitone 1: ${n1.find((s) => /modulation/.test(s))}`);
}
console.log(`test_fizz2dn: ${checks} checks, ${fails} failed`);
process.exit(fails ? 1 : 0);
