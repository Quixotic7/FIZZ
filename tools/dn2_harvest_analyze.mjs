// SPDX-License-Identifier: GPL-3.0-only
// Reads a Digitone II harvest (the zip site/dn2-wizard.html makes) and reports, for research/DN2-FORMAT.md and
// tools/dn2pst.mjs, what the device's own answers show. It changes nothing; it prints.
//
//   node tools/dn2_harvest_analyze.mjs harvest.zip [--json]
//
// - each parameter swept (by CC, by NRPN): the record bytes it moved (against the rule 28 + 2 x id), and its law:
//   the CC -> stored-value table (an enum's runs), the NRPN -> stored u16 rule;
// - the steps a person did (a filter machine, a modulation slot, the octave, another machine): the bytes they moved;
// - the recordings: length, level, the time to silence, the pitch at the start and the end, the brightness, the
//   number of levels (bit reduction) and the length of the held samples (sample-rate reduction).
import { readFileSync } from "node:fs";
import zlib from "node:zlib";

const u16le = (b, o) => b[o] | (b[o + 1] << 8), u32le = (b, o) => (b[o] | (b[o + 1] << 8) | (b[o + 2] << 16) | (b[o + 3] << 24)) >>> 0;
export function readZip(b) {
  let e = -1;
  for (let i = b.length - 22; i >= Math.max(0, b.length - 65557); i--) if (u32le(b, i) === 0x06054b50) { e = i; break; }
  if (e < 0) throw new Error("not a zip");
  const files = new Map();
  let p = u32le(b, e + 16);
  for (let k = 0, n = u16le(b, e + 10); k < n; k++) {
    const method = u16le(b, p + 10), cs = u32le(b, p + 20), nl = u16le(b, p + 28), xl = u16le(b, p + 30), cl = u16le(b, p + 32), lh = u32le(b, p + 42);
    const name = new TextDecoder().decode(b.subarray(p + 46, p + 46 + nl));
    const start = lh + 30 + u16le(b, lh + 26) + u16le(b, lh + 28), data = b.subarray(start, start + cs);
    files.set(name, method === 8 ? new Uint8Array(zlib.inflateRawSync(data)) : data);
    p += 46 + nl + xl + cl;
  }
  return files;
}

const u16 = (r, o) => (r[o] << 8) | r[o + 1];
// the runs of an enum's CC table: [[cc0, cc1, stored], ...]
function runs(pairs) {
  const out = [];
  for (const [v, s] of pairs.sort((a, b) => a[0] - b[0])) {
    const l = out[out.length - 1];
    if (l && l[2] === s && l[1] === v - 1) l[1] = v; else out.push([v, v, s]);
  }
  return out;
}
// the CC law that fits (coarse = f(v)), or null
function ccLaw(pairs) {
  const coarse = pairs.map(([v, raw]) => [v, raw >> 8]), N = new Set(coarse.map((x) => x[1])).size;
  const laws = [
    ["stored = CC", (v) => v],
    [`stored = floor(CC x ${N} / 128)`, (v) => Math.floor(v * N / 128)],
    [`stored = round(CC x ${N - 1} / 127)`, (v) => Math.round(v * (N - 1) / 127)],
    ["stored = CC x 2 (low byte: the pair)", null],
  ];
  for (const [name, f] of laws) if (f && coarse.every(([v, s]) => f(v) === s)) return name;
  if (pairs.every(([v, raw]) => raw === Math.floor(v * 361 / 128) * 2)) return "u16 = floor(CC x 361 / 128) x 2 (a RATIO B pair)";
  return null;
}
function nrpnLaw(pairs) {
  const M = Math.max(...pairs.map(([, raw]) => raw >> 8)), P = Math.max(...pairs.map(([, raw]) => raw)), fit = [];
  for (const [name, f] of [["u16 = NRPN << 1", (v) => v << 1], ["u16 = NRPN", (v) => v], ["u16 = NRPN x 4", (v) => v << 2],
    ["coarse = NRPN >> 7", (v) => (v >> 7) << 8], [`coarse = min(NRPN >> 7, ${M})`, (v) => Math.min(v >> 7, M) << 8],
    [`u16 = min(NRPN, ${P >> 1}) x 2`, (v) => Math.min(v, P >> 1) * 2]]) if (pairs.every(([v, raw]) => f(v) === raw)) fit.push(name);
  return fit.length ? fit[0] + (fit.length > 1 ? ` (also fits: ${fit.slice(1).join("; ")})` : "") : null;
}

// ---- audio
function readWav(b) {
  const v = new DataView(b.buffer, b.byteOffset, b.byteLength), rate = v.getUint32(24, true), n = (b.length - 44) >> 1, x = new Float32Array(n);
  for (let i = 0; i < n; i++) x[i] = v.getInt16(44 + 2 * i, true) / 32768;
  return { rate, x };
}
// the pitch at sample `at` (YIN: the cumulative-mean-normalized difference, its first dip under 0.15)
function f0(x, rate, at, win = 2048) {
  const s = Math.max(0, Math.min(x.length - 2 * win, at)), W = win, maxT = Math.min(W, Math.floor(rate / 30)), d = new Float32Array(maxT);
  let e = 0; for (let i = 0; i < W; i++) e += x[s + i] * x[s + i];
  if (e < 1e-4) return null;
  let run = 0;
  for (let t = 1; t < maxT; t++) {
    let v = 0; for (let i = 0; i < W; i++) { const q = x[s + i] - x[s + i + t]; v += q * q; }
    run += v; d[t] = run ? v * t / run : 1;
  }
  for (let t = Math.max(2, Math.floor(rate / 2000)); t < maxT - 1; t++)
    if (d[t] < 0.15) { while (t + 1 < maxT && d[t + 1] < d[t]) t++; return +(rate / t).toFixed(1); }
  return null;
}
function centroid(x, rate, at, win = 1024) {
  const s = Math.max(0, Math.min(x.length - win, at));
  let num = 0, den = 0;
  for (let k = 1; k < win / 2; k += 2) {
    let re = 0, im = 0;
    for (let i = 0; i < win; i++) { const w = x[s + i] * (0.5 - 0.5 * Math.cos(2 * Math.PI * i / win)), a = 2 * Math.PI * k * i / win; re += w * Math.cos(a); im -= w * Math.sin(a); }
    const m = Math.hypot(re, im); num += m * k * rate / win; den += m;
  }
  return den ? Math.round(num / den) : null;
}
export function audioStats(b) {
  const { rate, x } = readWav(b);
  let peak = 0, pi = 0, ss = 0;
  for (let i = 0; i < x.length; i++) { const a = Math.abs(x[i]); ss += x[i] * x[i]; if (a > peak) { peak = a; pi = i; } }
  const env = (i, w = 256) => { let m = 0; for (let j = i; j < Math.min(x.length, i + w); j++) m = Math.max(m, Math.abs(x[j])); return m; };
  let start = 0; while (start < x.length && Math.abs(x[start]) < peak * 0.05) start++;
  let end = x.length - 256; while (end > start && env(end) < peak * 0.01) end -= 256;
  const levels = new Set(), held = [];
  let run = 1;
  for (let i = start; i < Math.min(end, start + rate); i++) { levels.add(Math.round(x[i] * 32768)); if (x[i] === x[i - 1]) run++; else { if (run > 1) held.push(run); run = 1; } }
  held.sort((a, b) => a - b);
  return { rate, seconds: +(x.length / rate).toFixed(2), peakDb: peak ? +(20 * Math.log10(peak)).toFixed(1) : null, rmsDb: ss ? +(10 * Math.log10(ss / x.length)).toFixed(1) : null,
    soundMs: Math.round((end - start) * 1000 / rate), f0Start: f0(x, rate, start + Math.floor(rate * 0.05)), f0End: f0(x, rate, Math.max(start, end - 3000)),
    centroidHz: centroid(x, rate, start + Math.floor(rate * 0.1)), levels: levels.size, heldMedian: held.length ? held[held.length >> 1] : 1, peakAtMs: Math.round(pi * 1000 / rate) };
}

// ---- the analysis
export function analyze(zipBytes) {
  const files = readZip(zipBytes), man = JSON.parse(new TextDecoder().decode(files.get("manifest.json")));
  const rec = (f) => (f && files.get(f)) || null;
  const out = { device: man.device, settings: man.settings, mock: man.mock, params: [], manual: [], audio: [], notes: [] };
  let ref = null;
  const groups = new Map();
  for (const s of man.steps) {
    if (s.kind === "ref") ref = rec(s.record);
    if (s.kind === "probe" && s.record) {
      const key = `${s.id}|${s.via}|${s.tag || ""}`;
      if (!groups.has(key)) groups.set(key, { id: s.id, name: s.name, via: s.via, tag: s.tag || "", cc: s.cc, nrpn: s.nrpn, ref, probes: [] });
      groups.get(key).probes.push({ value: s.value, r: rec(s.record) });
    }
    if (s.kind === "dump" && s.record) {
      const r = rec(s.record), ch = [];
      if (r && ref) for (let i = 0; i < r.length; i++) if (r[i] !== ref[i]) ch.push(`+${i}: ${ref[i]} -> ${r[i]}`);
      out.manual.push({ item: s.item, label: s.label, changed: ch });
    }
    if (s.kind === "audio") out.audio.push({ item: s.item, label: s.label, file: s.audio, ...(files.get(s.audio) ? audioStats(files.get(s.audio)) : { missing: true }) });
  }
  for (const g of groups.values()) {
    const rs = g.probes.filter((p) => p.r), vary = [];
    for (let i = 0; i < 359; i++) if (new Set(rs.map((p) => p.r[i])).size > 1) vary.push(i);
    const o = 28 + 2 * g.id, rule = vary.length > 0 && vary.every((i) => i === o || i === o + 1);
    const at = vary.length ? (vary[0] % 2 ? vary[0] - 1 : vary[0]) : o;
    const pairs = rs.map((p) => [p.value, u16(p.r, at)]);
    const res = { id: g.id, name: g.name, via: g.via, tag: g.tag, cc: g.cc, nrpn: g.nrpn, n: rs.length, offsets: vary, ruleOffset: o, matchesRule: rule,
      law: g.via === "cc" ? ccLaw(pairs) : nrpnLaw(pairs), fine: pairs.some(([, raw]) => raw & 255) };
    if (g.via === "cc") { const R = runs(pairs.map(([v, raw]) => [v, raw >> 8])); if (R.length <= 40) res.runs = R; res.values = new Set(pairs.map((x) => x[1] >> 8)).size; }
    else res.pairs = pairs;
    out.params.push(res);
  }
  out.params.sort((a, b) => a.id - b.id || a.via.localeCompare(b.via) || a.tag.localeCompare(b.tag));
  return out;
}

export function report(a) {
  const L = [];
  L.push(`# Digitone II harvest${a.mock ? " (MOCK DEVICE)" : ""}`, "");
  L.push(`- Device: ${a.device.api ? a.device.api.name : "?"}, OS ${a.device.version || "?"} (build ${a.device.build || "?"})`);
  L.push(`- Track ${a.settings.track}, channel ${a.settings.channel}, pattern ${a.settings.pattern}, read back with ${a.settings.dump || "nothing (audio only)"}`, "");
  L.push("## Parameters", "", "| id | name | via | moved bytes | rule 28+2·id | law | detail |", "| --- | --- | --- | --- | --- | --- | --- |");
  for (const p of a.params) {
    const detail = p.runs ? (p.runs.length <= 12 ? p.runs.map(([x, y, s]) => (x === y ? `${x}` : `${x}-${y}`) + `→${s}`).join(" ") : `${p.values} values`) :
      p.pairs ? p.pairs.slice(0, 6).map(([v, r]) => `${v}→${r}`).join(" ") : "";
    L.push(`| ${p.id} | ${p.name}${p.tag ? ` (${p.tag})` : ""} | ${p.via === "cc" ? `CC ${p.cc}` : `NRPN ${p.nrpn.join(":")}`} | ${p.offsets.join(", ") || "none"} | ${p.matchesRule ? "yes" : `**no** (${p.ruleOffset})`} | ${p.law || "**?**"}${p.fine ? ", fine byte used" : ""} | ${detail} |`);
  }
  if (a.manual.length) {
    L.push("", "## Steps a person did", "");
    for (const m of a.manual) L.push(`- ${m.item} / ${m.label}: ${m.changed.length ? m.changed.slice(0, 16).join("; ") + (m.changed.length > 16 ? ` (+${m.changed.length - 16})` : "") : "no byte moved"}`);
  }
  if (a.audio.length) {
    L.push("", "## Recordings", "", "| item | label | sound ms | peak dB | f0 start / end | centroid Hz | levels | held |", "| --- | --- | --- | --- | --- | --- | --- | --- |");
    for (const r of a.audio) L.push(r.missing ? `| ${r.item} | ${r.label} | (missing) |` :
      `| ${r.item} | ${r.label} | ${r.soundMs} | ${r.peakDb} | ${r.f0Start ?? "-"} / ${r.f0End ?? "-"} | ${r.centroidHz ?? "-"} | ${r.levels} | ${r.heldMedian} |`);
  }
  return L.join("\n");
}

if (import.meta.url === `file://${process.argv[1]}`) {
  const f = process.argv[2];
  if (!f) { console.error("usage: node tools/dn2_harvest_analyze.mjs harvest.zip [--json]"); process.exit(2); }
  const a = analyze(new Uint8Array(readFileSync(f)));
  console.log(process.argv.includes("--json") ? JSON.stringify(a, null, 1) : report(a));
}
