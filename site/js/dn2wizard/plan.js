// SPDX-License-Identifier: GPL-3.0-only
// What the wizard does, as a list of steps (pure: no MIDI). The items follow FIZZ research/DN2-MEASURE.md, most
// important first, then every other FM TONE parameter the Digitone II takes over MIDI (params.js), then the steps
// that need the person (a filter machine, the modulation menus, the octave, another machine: no CC reaches them).
//
// A step: {k: "identify" | "baseline" | "calibrate" | "ref" | "probe" | "set" | "audio" | "restore" | "manual" |
// "dump", item, ...}. probe: set one parameter (via "cc" or "nrpn", value) and read the track's sound back. restore:
// back to the baseline (ids, or "touched": every parameter the item set since its "ref").
import { PARAMS, param, sweepOf } from "./params.js";

const BRIGHT = [[46, 80], [51, 127], [55, 127]];        // FDBK 80, A LEV 127, B LEV 127 (CC 46, 51, 55)
const set = (item, id, value, via = "cc") => ({ k: "set", item, id, via, value });
const setCc = (item, cc, value) => { const p = PARAMS.find((x) => x.cc === cc); return set(item, p.id, value); };
const sweep = (item, id) => {
  const p = param(id), s = sweepOf(p), out = [];
  for (const v of s.cc) out.push({ k: "probe", item, id, via: "cc", value: v });
  for (const v of s.nrpn) out.push({ k: "probe", item, id, via: "nrpn", value: v });
  out.push({ k: "restore", item, ids: [id], verify: true });
  return out;
};
// an audio step: notes [[note, velocity, onMs, offMs]], recorded for ms
const audio = (item, label, notes, ms) => ({ k: "audio", item, label, notes, ms });
const C3 = 60;

export const ITEMS = [
  { key: "start", title: "Identify the Digitone II and check the wizard can read its sound", person: false },
  { key: "mmtype", title: "MULTI-MODE filter TYPE", person: false },
  { key: "ahd", title: "AMP MODE AHD and HOLD", person: false },
  { key: "porta", title: "Portamento", person: false },
  { key: "crush", title: "Bit and sample-rate reduction", person: false },
  { key: "mult", title: "LFO MULT", person: false },
  { key: "fkt", title: "Filter key track", person: false },
  { key: "ktrk", title: "Key track A, B1, B2", person: false },
  { key: "all", title: "Every other FM TONE parameter", person: false },
  { key: "filters", title: "The other filter machines", person: true },
  { key: "mod", title: "The modulation menus", person: true },
  { key: "octave", title: "The sound's octave", person: true },
  { key: "machines", title: "WAVETONE and FM DRUM", person: true },
];

export function buildPlan({ person = true } = {}) {
  const S = [], done = new Set();
  const add = (...x) => S.push(...x);
  const sweepOnce = (item, id) => { done.add(id); add(...sweep(item, id)); };
  // 1. identify, the baseline, the calibration (which dump shows the track's live sound)
  add({ k: "identify", item: "start" }, { k: "baseline", item: "start" }, { k: "calibrate", item: "start" });
  // 2. MULTI-MODE TYPE: every CC value read back, then heard at nine
  add({ k: "ref", item: "mmtype" });
  sweepOnce("mmtype", 73);
  for (const [cc, v] of [...BRIGHT, [16, 64], [17, 64]]) add(setCc("mmtype", cc, v));
  for (const v of [0, 16, 32, 48, 64, 80, 96, 112, 127])
    add(set("mmtype", 73, v), audio("mmtype", `TYPE cc${v}`, [[C3, 100, 0, 1500]], 2000));
  add({ k: "restore", item: "mmtype", ids: "touched", verify: true });
  // 3. AHD: the MODE and HOLD laws, then HOLD heard (ATK 0, DEC 0: the hold is the sound's length)
  add({ k: "ref", item: "ahd" });
  sweepOnce("ahd", 97); sweepOnce("ahd", 88);
  for (const [cc, v] of [...BRIGHT, [84, 0], [86, 0]]) add(setCc("ahd", cc, v));
  add({ k: "set", item: "ahd", id: 97, via: "stored", value: 0 });   // AHD (the stored 0: the CC found by the sweep)
  for (const v of [0, 16, 32, 64, 96, 126]) add(set("ahd", 88, v), audio("ahd", `HOLD cc${v}`, [[C3, 100, 0, 100]], 4000));
  add(set("ahd", 88, 127), audio("ahd", "HOLD cc127 held 1.5 s", [[C3, 100, 0, 1500]], 3500));
  add({ k: "restore", item: "ahd", ids: "touched", verify: true });
  // 4. portamento: C2 -> C4, the second note overlapping the first (legato)
  add({ k: "ref", item: "porta" });
  sweepOnce("porta", 99); sweepOnce("porta", 100);
  for (const [cc, v] of BRIGHT) add(setCc("porta", cc, v));
  add(set("porta", 99, 127));
  for (const v of [16, 32, 64, 96, 127])
    add(set("porta", 100, v), audio("porta", `PTIM cc${v}`, [[36, 100, 0, 700], [60, 100, 500, 2200]], 2800));
  add({ k: "restore", item: "porta", ids: "touched", verify: true });
  // 5. bit and sample-rate reduction, their routings and the overdrive
  add({ k: "ref", item: "crush" });
  for (const id of [101, 102, 103, 104, 106]) sweepOnce("crush", id);
  for (const [cc, v] of BRIGHT) add(setCc("crush", cc, v));
  for (const v of [0, 16, 32, 48, 64, 80, 96, 112, 127]) add(set("crush", 101, v), audio("crush", `BR cc${v}`, [[C3, 100, 0, 900]], 1100));
  add(set("crush", 101, 0));
  for (const v of [0, 16, 32, 48, 64, 80, 96, 112, 127]) add(set("crush", 102, v), audio("crush", `SRR cc${v}`, [[C3, 100, 0, 900]], 1100));
  for (const r of [0, 127]) add(set("crush", 102, 64), set("crush", 103, r), audio("crush", `SRR cc64 routing cc${r}`, [[C3, 100, 0, 900]], 1100));
  add({ k: "restore", item: "crush", ids: "touched", verify: true });
  // 6. LFO MULT on the three LFOs (dumps only: the law, every position)
  add({ k: "ref", item: "mult" });
  for (const id of [5, 6, 7]) sweepOnce("mult", id);
  add({ k: "restore", item: "mult", ids: "touched", verify: true });
  // 7. the filter's key track: four notes, at three settings
  add({ k: "ref", item: "fkt" });
  sweepOnce("fkt", 82);
  for (const [cc, v] of [...BRIGHT, [16, 40], [17, 0], [18, 0]]) add(setCc("fkt", cc, v));
  for (const v of [0, 64, 127])
    add(set("fkt", 82, v), audio("fkt", `KEY.T cc${v}`, [[36, 100, 0, 450], [48, 100, 600, 1050], [60, 100, 1200, 1650], [72, 100, 1800, 2250]], 2500));
  add({ k: "restore", item: "fkt", ids: "touched", verify: true });
  // 8. key track A, B1, B2: three notes each, the modulation at 100
  add({ k: "ref", item: "ktrk" });
  for (const id of [70, 71, 72]) sweepOnce("ktrk", id);
  add(setCc("ktrk", 51, 100), setCc("ktrk", 55, 100));
  const ktNotes = [[36, 100, 0, 450], [60, 100, 600, 1050], [84, 100, 1200, 1650]];
  add(audio("ktrk", "key track 0", ktNotes, 1900));
  for (const id of [70, 71, 72]) add(set("ktrk", id, 64), audio("ktrk", `${param(id).name} cc64`, ktNotes, 1900), set("ktrk", id, 0));
  add({ k: "restore", item: "ktrk", ids: "touched", verify: true });
  // 9. everything else the Digitone II takes over MIDI
  add({ k: "ref", item: "all" });
  for (const p of PARAMS) if (!done.has(p.id)) sweepOnce("all", p.id);
  add({ k: "restore", item: "all", ids: PARAMS.map((p) => p.id), verify: true });
  if (!person) return S;
  // 10..13: the person's steps (a manual step, then the sound read back)
  const man = (item, text, action) => ({ k: "manual", item, text, action });
  add({ k: "ref", item: "filters" });
  [["LOWPASS 4", 1], ["EQUALIZER", 2], ["COMB-", 3], ["LEGACY LP/HP", 4], ["COMB+", 5]].forEach(([nm, k]) => {
    add(man("filters", `On the FLTR page, set the filter machine to ${nm}.`, { filterMachine: k }), { k: "dump", item: "filters", label: `machine ${nm}` });
    for (const id of [73, 74, 75]) {
      for (const v of [0, 32, 64, 96, 127]) add({ k: "probe", item: "filters", id, via: "cc", value: v, tag: nm });
      add({ k: "restore", item: "filters", ids: [id], verify: false });
    }
    for (const [cc, v] of [...BRIGHT, [16, 64], [17, 64]]) add(setCc("filters", cc, v));
    for (const v of [0, 64, 127]) add(set("filters", 73, v), audio("filters", `${nm} TYPE cc${v}`, [[C3, 100, 0, 1200]], 1600));
    add({ k: "restore", item: "filters", ids: "touched", verify: false });
  });
  add(man("filters", "Set the filter machine back to MULTI-MODE.", { filterMachine: 0 }), { k: "dump", item: "filters", label: "back to MULTI-MODE" });
  add({ k: "ref", item: "mod" });
  [["VELOCITY", "FILTER FREQ", "+32", [4, 0x2000, 74]], ["VELOCITY", "FILTER FREQ", "-32", [4, 0x6000, 74]], ["MOD WHEEL", "HARM", "+64", [8, 0x4000, 37]],
   ["AFTERTOUCH", "A LEV", "+64", [16, 0x4000, 46]], ["PITCH BEND", "MIX", "+64", [0, 0x4000, 40]], ["BREATH", "FDBK", "+64", [12, 0x4000, 39]],
   ["KEY TRACK (if it is a source)", "FILTER FREQ", "+64", [20, 0x4000, 74]],
   // E4 (FIZZ's import reads the records as six sources x four, depth = (coarse + fine / 256 - 64) x 2: inferred): a
   // source's second slot (is it the next record, k + 1?) and a small odd depth (the fine byte's law); the mock follows
   // that reading (depth / 2 + 64)
   ["VELOCITY (its second slot)", "HARM", "+16", [5, 0x4800, 37]], ["VELOCITY", "FILTER FREQ", "+1", [4, 0x4080, 74]],
   ["MOD WHEEL", "HARM", "-127", [8, 0x0080, 37]]].forEach(([src, dst, dep, mock]) => {
    const which = /second slot/.test(src) ? "the second slot" : "the first slot", srcName = src.replace(/ \(its second slot\)/, "");
    add(man("mod", `In the sound's ${srcName} modulation menu, set ${which} to ${dst} with depth ${dep}. Then press Continue; afterwards, set that slot back to none.`, { mod: mock }),
      { k: "dump", item: "mod", label: `${src} ${dst} ${dep}` }, man("mod", `Set that ${srcName} slot back to none (no destination).`, { mod: [mock[0], 0, 0] }));
  });
  add({ k: "ref", item: "octave" });
  for (const [o, v] of [["-1", -12], ["+1", 12], ["0", 0]])
    add(man("octave", `If the sound has an OCTAVE setting (its settings menu), set it to ${o}.`, { octave: v }), { k: "dump", item: "octave", label: `octave ${o}` });
  add({ k: "ref", item: "machines" });
  add(man("machines", "Change this track's machine to WAVETONE (the SYN machine).", { machine: 1 }), { k: "dump", item: "machines", label: "WAVETONE" });
  add(man("machines", "Change it to FM DRUM.", { machine: 2 }), { k: "dump", item: "machines", label: "FM DRUM" });
  add(man("machines", "Change it back to FM TONE. That is all: the wizard is done with the device.", { machine: 0 }), { k: "dump", item: "machines", label: "FM TONE again" });
  return S;
}

// what the plan will send, for the confirmation before the first write
export function summarize(S, channel) {
  const cc = new Set(), nrpn = new Set(), notes = new Set();
  let probes = 0, audios = 0, manual = 0, recMs = 0;
  for (const s of S) {
    const p = s.id !== undefined ? param(s.id) : null;
    if (s.k === "probe" || s.k === "set") { if (s.via === "nrpn") nrpn.add(`${p.nrpn[0]}:${p.nrpn[1]}`); else if (p.cc !== null && p.cc !== undefined) cc.add(p.cc); else nrpn.add(`${p.nrpn[0]}:${p.nrpn[1]}`); }
    if ((s.k === "probe" || s.k === "set") && p) nrpn.add(`${p.nrpn[0]}:${p.nrpn[1]}`);   // (a restore sends the NRPN)
    if (s.k === "probe") probes++;
    if (s.k === "audio") { audios++; recMs += s.ms; for (const n of s.notes) notes.add(n[0]); }
    if (s.k === "manual") manual++;
  }
  return { channel, cc: [...cc].sort((a, b) => a - b), nrpn: [...nrpn], notes: [...notes].sort((a, b) => a - b), probes, audios, manual, recMs };
}
