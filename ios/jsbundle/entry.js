// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// FizzData's API (ios/PLAN.md §3 B, phase I2): the web Manager's own data modules, as they are, for the iOS app's
// JavaScriptCore (build.mjs bundles this file and its imports into FizzKit/Sources/FizzData/Resources/FizzData.js as
// globalThis.FizzData). Phase I2: the backup file (.fizzbak) and what it needs. The helpers below are the few lines of
// manager.js (a UI module, not bundled) that turn what the unit gave into makeFizzbak's arguments, so the app passes
// the same ones; and demoStore for the app's demo unit.
//
//   makeUnitFizzbak({kind, image, region, crc, frames, fizz, unit, webTags, stats, date}) -> Uint8Array
//       manager.js backup(): frames are the DUMP replies of MY PRESETS (null: the image's presets, as for a FIZZ that
//       gives no version), stats FizzLink's backup stats (manager.js rounds them into the manifest's "transfer")
//   presetsFromDumps(frames, fizz), presetsFromSummary(summary), webFor(presets, allTags), transferOf(stats)
//   demoStore({presets, projects, layout}) -> a store image (buildImage): MY PRESETS from sounds (js/library.js's
//       shape, a version-6 blob), projects [{slot, name} | name] as their settings and header records only (no
//       patterns, no pool: what readRegion and the BACKUP screen read; for the demo unit, not a unit's store)
//   toPlain(x) -> JSON-able: typed arrays as {u8: hex} (or {u8len, crc32} past 512 bytes) (the tests' comparisons)
import { makeFizzbak, readBackup, backupFileName, crc32, hex8, soundKey, FORMAT as BACKUP_FORMAT } from "../../site/manager/fizzbak.js";
import { readRegion, layoutOf, LAYOUTS, buildImage, writeStore, PRESET_MINE0 } from "../../site/manager/fizzstore.js";
import { RK, rkey } from "../../site/manager/projects.js";
import { soundFromFrame } from "../../site/manager/sound.js";
import { soundFromStoreFrame, soundToFizzsnd, parseFizzsnd, makeFizzlib, parseFizzlib, sameSound } from "../../site/js/library.js";
import { parseDump, FIZZ_TAGS, fizzTagMask } from "../../site/js/fizz_sysex.js";

export { makeFizzbak, readBackup, backupFileName, crc32, hex8, soundKey, BACKUP_FORMAT };
export { readRegion, layoutOf, LAYOUTS, buildImage, writeStore };
export { soundFromFrame, soundFromStoreFrame, soundToFizzsnd, parseFizzsnd, makeFizzlib, parseFizzlib, sameSound };
export { FIZZ_TAGS };

// ---- manager.js readUnit / webFor / backup, as they build makeFizzbak's arguments
export function presetsFromDumps(frames, fizz = "") {
  return Array.from(frames, (f) => {
    const d = parseDump(f);
    if (!d) throw new Error("not a DUMP reply (a STORE / STORE6 frame with the DUMP flag)");
    return { ...soundFromStoreFrame(f), source: { fizz: fizz || "", from: `MY PRESETS ${d.slot}` } };
  });
}
export const presetsFromSummary = (summary) => summary.presets.map((p) => ({ fizz: true, name: p.name, blob: p.blob, drive: p.drive,
  pan: p.pan, sends: p.sends, fizzTags: FIZZ_TAGS.filter((_, k) => (p.tags >> k) & 1), favourite: false,
  source: { from: `MY PRESETS ${p.slot}` }, notes: [], origin: [], mark: "", tags: [] }));
export function webFor(presets, all = {}) {
  const tags = {};
  for (const s of presets) { const k = soundKey(s); if (all[k] && all[k].length) tags[k] = all[k]; }
  return { tags };
}
export const transferOf = (st) => st ? { bytes: st.bytes, ms: Math.round(st.ms), kbps: +st.kbps.toFixed(1), frames: st.frames,
  resent: st.resent, depth: st.depth, gapMs: st.gapMs, perFrameMs: +st.perFrame.toFixed(2),
  rttMs: { median: +st.rtt.median.toFixed(2), p95: +st.rtt.p95.toFixed(2), max: +st.rtt.max.toFixed(2) } } : null;
export function makeUnitFizzbak({ kind = "full", image = null, region = null, crc = null, frames = null, fizz = "", unit = {},
                                  webTags = {}, stats = null, date = new Date() } = {}) {
  if (kind !== "presets") kind = "full";
  if (kind === "presets") image = null;
  const summary = image ? readRegion(image) : null;
  const presets = frames ? presetsFromDumps(frames, fizz) : summary ? presetsFromSummary(summary) : [];
  return makeFizzbak({ kind, image, crc, region, summary, presets, fizz, unit, web: webFor(presets, webTags),
    transfer: transferOf(stats), date });
}

// ---- the demo unit's store
const NAME = 13, BLOB = 128;
const nameBytes = (b, o, s) => { s = String(s || ""); for (let i = 0; i < NAME - 1 && i < s.length; i++) { const c = s.charCodeAt(i); b[o + i] = c >= 0x20 && c <= 0x7E ? c : 0x3F; } };
function libRecord(s, id) {                               // persist.c snd_rec, MY PRESETS ('S' 5), as fizzstore.js preset() reads it
  if (!(s.blob && s.blob.length === BLOB && s.blob[0] === 0x51 && s.blob[1] === 6)) throw new Error(`demoStore: ${s.name}: a version-6 blob (128 bytes)`);
  const b = new Uint8Array(2 + NAME + BLOB + 8), o = 2 + NAME + BLOB, sends = s.sends || {};
  b[0] = 0x53; b[1] = 5;
  nameBytes(b, 2, s.name);
  b.set(s.blob, 2 + NAME);
  if (s.drive !== undefined) b[2 + NAME + 87] = Math.max(0, Math.min(127, s.drive | 0));   // (DRIVE: the blob's byte 87)
  b[o] = id & 0xFF; b[o + 1] = (id >> 8) & 0xFF;
  b[o + 2] = fizzTagMask(s.fizzTags || []);
  b[o + 3] = s.imported ? 1 : 0;
  b[o + 4] = Math.max(-64, Math.min(63, s.pan | 0)) & 0xFF;
  b[o + 5] = (sends.chorus | 0) & 0x7F; b[o + 6] = (sends.delay | 0) & 0x7F; b[o + 7] = (sends.reverb | 0) & 0x7F;
  return b;
}
function hdrRecord(name) {                                // persist.c ser_hdr up to pool_empty, rec_auto (the rest: defaults)
  const b = new Uint8Array(2 + NAME + 1 + 6 + 7 * 6 + 3);
  b[0] = 0x48; b[1] = 1;
  nameBytes(b, 2, name);
  for (let t = 0; t < 6; t++) b[2 + NAME + 7 + 7 * t] = 100;   // each track's level
  b[b.length - 2] = 1; b[b.length - 1] = 1;                 // pool_empty, rec_auto
  return b;
}
const setRecord = (p) => Uint8Array.from([0x47, 2, p, 0, 0, 0, 0, 0, 0, 0, 1]);   // persist.c ser_set, 'G' 2 (six tracks), no effects' values
export function demoStore({ presets = [], projects = [], layout = "device" } = {}) {
  const L = typeof layout === "string" ? LAYOUTS[layout] : layout;
  if (!L) throw new Error(`demoStore: no layout ${layout}`);
  const lib = presets.map((s, k) => {
    const slot = s.slot || k + 1;
    return { key: rkey(RK.LIB, 0, slot - 1), bytes: libRecord(s, s.id ?? PRESET_MINE0 + slot - 1) };
  });
  const ps = projects.map((p, k) => (typeof p === "string" ? { slot: k + 1, name: p } : p));
  const prj = [{ key: rkey(RK.SET, 0, 0), bytes: setRecord(ps.length ? ps[0].slot - 1 : 0) },
    ...ps.map((p) => ({ key: rkey(RK.HDR, p.slot - 1, 0), bytes: hdrRecord(p.name) }))];
  const byKey = (a, b) => a.key - b.key;
  return buildImage(L, { lib: lib.sort(byKey), prj: prj.sort(byKey) });
}

// ---- JSON-able, for comparisons (tests) and logs
const hex = (u8) => Array.from(u8, (x) => x.toString(16).padStart(2, "0")).join("");
export function toPlain(x) {
  if (ArrayBuffer.isView(x)) {
    const u8 = new Uint8Array(x.buffer, x.byteOffset, x.byteLength);
    return u8.length <= 512 ? { u8: hex(u8) } : { u8len: u8.length, crc32: hex8(crc32(u8)) };
  }
  if (Array.isArray(x)) return x.map(toPlain);
  if (x && typeof x === "object") {
    const tag = Object.prototype.toString.call(x);
    if (tag === "[object Map]") return { map: [...x.entries()].map(([k, v]) => [toPlain(k), toPlain(v)]) };
    if (tag === "[object Date]") return { date: x.toISOString() };
    const o = {};
    for (const k of Object.keys(x)) o[k] = toPlain(x[k]);
    return o;
  }
  return x;
}

// ---- I5: the EDIT tab (sound.js: the editor's sound and FIZZ's version-6 blob; fizzvalues.js: names, ranges, texts)
//   editTable() -> {values, algos, destOrder, q, np}: the 115 values as FIZZ's pages name and show them
//   soundFromLib(s): a .fizzsnd / library sound as the editor's; soundFromFrame (above): a DUMP reply as the editor's
//   editPack(s) -> {blob, drive}: the editor's sound ({values, midi?}) packed back (sound.js blob6)
//   editTexts([[id, v], ..]) -> [text(id, v), ..] (the tests' comparisons)
import { VALUES, ALGOS, DEST_ORDER, Q, NP, soundFromLib, blob6, text as valueText } from "../../site/manager/sound.js";
// (mock-ups 45, 46) the mix: PAN and the sends under PARAM's ids (MIX_IDS), their rows (info: PAN -64..63, the sends
// 0..127), PAN's texts (panText, -64 first), the defaults (MIX_DEFAULTS); newSound("fm" | "midi"): LIBRARY's + NEW
import { MIX_DEFAULTS, MIX_IDS, newSound, panText, info as mixInfo } from "../../site/manager/sound.js";
export { soundFromLib, newSound };
const editMix = () => ({ ids: MIX_IDS, defaults: MIX_DEFAULTS, rows: Object.values(MIX_IDS).map((id) => mixInfo(id)),
  panTexts: Array.from({ length: 128 }, (_, k) => panText(k - 64)) });
export const editTable = () => ({ values: VALUES, algos: ALGOS, destOrder: DEST_ORDER, q: Q, np: NP, mix: editMix() });
export const editPack = (s) => ({ blob: blob6(s), drive: s.values[Q.DRIVE] });
export const editTexts = (pairs) => pairs.map(([id, v]) => valueText(id, v));

// ---- I4: RESTORE and ADD presets, IMPORT and SEND, EXPORT (importer.js, exporter.js, manager.js openRestore / rsFit /
// restoreFull, projects.js fitModel): the pure halves of those UI modules' mount functions.
//   importRead(files [{name, bytes}]) -> importer.js readImport's items, each with its kind ("fizz", "dn1", "dn2": the
//       row's chip) and midi (sound.js isMidiBlob)
//   importDuplicates(sounds, mineFrames) -> importer.js markDuplicates' marks: "mine", "list" or ""
//   exportCarry(sounds, k) -> exporter.js mountExport rowsFor: { lost, changed, bar, rows: [{what, how, c, w, note, n}],
//       touched, none } (k: fizz, dnsnd, dn2pst, dn2snd)
//   exportFile(sounds, kind, date) -> exporter.js exportFiles (several Digitone sounds: a ZIP; MIDI ones left out)
//   restorePlan(image, unitSize) -> manager.js openRestore's mode ("full" or "presets") and, for another package's
//       backup, the fit: its choice (fitChoice), the projects there and MY PRESETS to choose from; both layouts' limits
//   restoreFit(image, unitSize, choice, withImage) -> projects.js fitModel: { copies, left, image (withImage) };
//       throws its error (code "range") when the choice does not fit
import { readImport, markDuplicates } from "../../site/manager/importer.js";
import { exportFiles, carryOf, MIDI_ONLY as EXPORT_MIDI_ONLY } from "../../site/manager/exporter.js";
import { modelOf as i4ModelOf, imageOf as i4ImageOf, fitModel as i4FitModel, fitChoice as i4FitChoice, projects as i4Projects } from "../../site/manager/projects.js";
import { PACKAGE_NAME as I4_PACKAGE_NAME } from "../../site/manager/fizzstore.js";
import { isMidiBlob as i4IsMidiBlob } from "../../site/manager/sound.js";
import { fizzToDnsnd as i4ToDnsnd, fizzToDn2pst as i4ToDn2pst } from "../../site/js/fizz2dn.js";

export { EXPORT_MIDI_ONLY };
const i4Kind = (it) => (it.sound ? (it.sound.dn2 ? "dn2" : it.sound.fizz ? "fizz" : "dn1")
  : /\.fizz/i.test(it.file) ? "fizz" : /dn2/i.test(it.file) ? "dn2" : "dn1");
export async function importRead(files) {
  const got = await readImport(Array.from(files, (f) => ({ name: f.name, bytes: f.bytes })));
  return got.map((it) => ({ ...it, kind: i4Kind(it), midi: !!(it.sound && i4IsMidiBlob(it.sound.blob)) }));
}
export function importDuplicates(sounds, mineFrames = []) {
  const items = sounds.map((s) => ({ ok: true, sound: s }));
  markDuplicates(items, Array.from(mineFrames, (f) => soundFromStoreFrame(f)));
  return items.map((it) => it.dup);
}
const i4Plain = (s) => ({ blob: s.blob, name: s.name, tags: s.fizzTags || [], pan: s.pan | 0, sends: s.sends || {}, drive: s.drive | 0 });
const I4_MIDI_ROW = { what: "MIDI SOUND", how: "not written", c: "--midi", w: 1, note: EXPORT_MIDI_ONLY };
export function exportCarry(sounds, k) {
  if (k === "fizz") return carryOf([]);
  const midi = sounds.filter((s) => i4IsMidiBlob(s.blob)).length;
  if (midi === sounds.length) return { lost: midi, changed: 0, bar: 0, rows: [{ ...I4_MIDI_ROW, n: midi }], touched: midi, none: true };
  const all = sounds.filter((s) => !i4IsMidiBlob(s.blob)).map((s) => (k === "dnsnd" ? i4ToDnsnd(i4Plain(s)) : i4ToDn2pst(i4Plain(s))).notes);
  const merged = (one, by) => {
    for (const r of one.rows) { const key = `${r.what}|${r.how}`; const e = by.get(key) || { ...r, n: 0 }; e.n++; by.set(key, e); }
    return [...by.values()];
  };
  if (midi) {
    const one = carryOf(all.flat()), touched = midi + all.filter((n) => n.length).length;
    return { ...one, lost: one.lost + midi, rows: merged(one, new Map([["MIDI", { ...I4_MIDI_ROW, n: midi }]])), touched,
      bar: Math.max(1, Math.min(9, Math.round(10 - 9 * touched / sounds.length))) };
  }
  if (all.length === 1) return carryOf(all[0]);
  const one = carryOf(all.flat()), touched = all.filter((n) => n.length).length;
  return { ...one, rows: merged(one, new Map()), touched, bar: touched ? Math.max(1, Math.min(9, Math.round(10 - 9 * touched / all.length))) : 10 };
}
export const exportFile = (sounds, kind, date = new Date()) => exportFiles(sounds, kind, date);
const i4Limits = (l) => l && { name: l.name, package: l.package, packageName: I4_PACKAGE_NAME[l.package] || l.package, nproj: l.nproj, nlib: l.nlib, size: l.size };
export function restorePlan(image, unitSize) {
  const to = layoutOf(unitSize), sum = image ? readRegion(image) : null, src = sum && sum.layout;
  const plan = { mode: image && to && src ? "full" : "presets", to: i4Limits(to), from: i4Limits(src ? LAYOUTS[src] : null), fit: null, error: "" };
  if (plan.mode === "full" && src !== to.name) {
    try {
      const m = i4ModelOf(image);
      plan.fit = { choice: i4FitChoice(m, to), projects: i4Projects(m).filter((x) => x.there).map((x) => ({ p: x.p, name: x.name })),
        presets: sum.presets.map((x) => ({ i: x.slot - 1, slot: x.slot, name: x.name, tags: x.tags })) };
    } catch (e) { plan.mode = "presets"; plan.error = e.message || String(e); }
  }
  return plan;
}
export function restoreFit(image, unitSize, choice, withImage = false) {
  const r = i4FitModel(i4ModelOf(image), layoutOf(unitSize), choice);
  return { copies: r.copies, left: r.left, ...(withImage ? { image: i4ImageOf(r.model) } : {}) };
}

// ---- I6: PROJECTS (projtab.js's model in this JSContext; the store's records stay here, held by a number, and the
// app gets what it draws). A session is projtab.js's J: the store as read (base), the changes waiting (ops, each
// applied again to a copy of base, as its model()), their results. The patterns' clips (copyPatterns, a .fizzpat, a
// kept project's patterns) are held by a number too.
//   pjOpen(image) -> {h, view}     pjClose(h)     pjView(h) -> view     pjImage(h) -> {ok, image} | {ok: false, code, message}
//   view: {layout, nproj, nlib, package, projects: [{p, there, name, demo, used, pool, changed, pats: [tile | null] x 64}],
//          ops: [{kind, q, at, ks, name, poolAdded, presetsAdded, copies}]}
//   tile: {r: [6 x 16-bit trig masks]} | {demo: true} | {bad: true}
//   pjPaste(h, q, j, c) / pjClear(h, q, ks) / pjLoadKept(h, q, bytes, file) -> {ok, view, result} | {ok: false, code, message}
//   pjUndo(h) / pjDiscard(h) -> view      pjRebase(h, image) -> {ok, view} (the ops again on the store read again)
//   pjCopy(h, p, ks) / pjReadFizzpat(bytes, file) / pjCopyKept(h | 0, bytes, file, ks) -> clip {c, n, from, names, ks}
//   pjFree(c)      pjFizzpat(c, {fizz, date}) -> {bytes, fileName}
//   pjKeep(h, p, {fizz, date}) -> {bytes, fileName, name, pats, sounds, colours}
//   pjKeptInfo(bytes, file) -> {ok, name, demo, pats, sounds, colours, fizz, date, used: [bool x 64]}
//   pjRenameKept(bytes, file, name, {fizz, date}) -> {bytes, fileName, name}
//   demoPatterns(image, {prev, projects: [{slot, name, pats, demo}]}) -> image with the demo unit's patterns (synthetic)
import { modelOf, imageOf, cloneModel, project as pjProject, copyPatterns, pastePatterns, clearPatterns, keepProject,
  loadProject, renameKept, makeFizzprj, readFizzprj, makeFizzpat, readFizzpat, patFileName, prjFileName, NPAT, DEMO_N,
  limits as pjLimits } from "../../site/manager/projects.js";

const PJ = new Map(), CLIPS = new Map();
let pjNext = 1, clipNext = 1;
const pjGet = (h) => { const s = PJ.get(h); if (!s) throw new Error(`no projects session ${h}`); return s; };
const pjFail = (e) => ({ ok: false, code: e.code || "error", message: e.message });
function pjApply(m, op) {
  if (op.kind === "paste") return pastePatterns(m, op.q, op.j, op.clip);
  if (op.kind === "clear") return clearPatterns(m, op.q, op.ks);
  if (op.kind === "load") return loadProject(m, op.q, op.kept);
  return null;
}
function pjModel(s) {
  if (!s.cache) {
    const m = cloneModel(s.base);
    for (const op of s.ops) op.result = pjApply(m, op);
    s.cache = m;
  }
  return s.cache;
}
const pjMasks = (rows) => rows.map((r) => r.reduce((a, on, x) => (on ? a | (1 << x) : a), 0));
const pjTile = (t) => (!t ? null : t.demo ? { demo: true } : t.bad ? { bad: true } : { r: pjMasks(t.rows) });
function pjViewOf(s) {
  const m = pjModel(s), L = pjLimits(m);
  const projects = Array.from({ length: L.nproj }, (_, p) => {
    const x = pjProject(m, p);
    return { p, there: x.there, name: x.name, demo: x.demo, used: x.used, pool: x.pool, changed: s.ops.some((o) => o.q === p),
      pats: x.pats.map(pjTile) };
  });
  const ops = s.ops.map((o) => {
    const r = o.result || {};
    return { kind: o.kind, q: o.q, at: r.at || [], ks: o.ks || [], name: o.kind === "load" ? o.kept.name : o.kind === "paste" ? o.clip.from : "",
      poolAdded: r.poolAdded || 0, presetsAdded: r.presetsAdded || 0, copies: r.copies || 0 };
  });
  return { layout: m.layout.name, nproj: L.nproj, nlib: L.nlib, package: L.package, projects, ops };
}
// an op tried on a copy and the store image built from it (it fits: the page finds that out only at WRITE), then kept
function pjPush(s, op) {
  const m = cloneModel(pjModel(s));
  try { op.result = pjApply(m, op); imageOf(m); } catch (e) { return pjFail(e); }
  s.ops.push(op);
  s.cache = m;
  return { ok: true, result: op.result || null, view: pjViewOf(s) };
}
export function pjOpen(image) {
  const h = pjNext++;
  PJ.set(h, { base: modelOf(image), ops: [], cache: null });
  return { h, view: pjViewOf(PJ.get(h)) };
}
export const pjClose = (h) => PJ.delete(h);
export const pjView = (h) => pjViewOf(pjGet(h));
export function pjImage(h) { try { return { ok: true, image: imageOf(pjModel(pjGet(h))) }; } catch (e) { return pjFail(e); } }
export function pjRebase(h, image) {
  const s = pjGet(h);
  s.base = modelOf(image); s.cache = null;
  try { imageOf(pjModel(s)); return { ok: true, view: pjViewOf(s) }; }
  catch (e) { s.ops = []; s.cache = null; return { ...pjFail(e), view: pjViewOf(s) }; }
}
export function pjPaste(h, q, j, c) {
  const clip = CLIPS.get(c);
  return clip ? pjPush(pjGet(h), { kind: "paste", q, j, clip }) : pjFail(new Error("nothing copied"));
}
export const pjClear = (h, q, ks) => pjPush(pjGet(h), { kind: "clear", q, ks: [...ks] });
export async function pjLoadKept(h, q, bytes, file = "") {
  let kept;
  try { kept = await readFizzprj(bytes, file); } catch (e) { return pjFail(e); }
  return pjPush(pjGet(h), { kind: "load", q, kept });
}
export function pjUndo(h) { const s = pjGet(h); s.ops.pop(); s.cache = null; return pjViewOf(s); }
export function pjDiscard(h) { const s = pjGet(h); s.ops = []; s.cache = null; return pjViewOf(s); }
function pjClipOut(clip) {
  if (!clip.pats.length) return { c: 0, n: 0, from: clip.from, names: [], ks: [] };
  const c = clipNext++;
  CLIPS.set(c, clip);
  return { c, n: clip.pats.length, from: clip.from, names: clip.names, ks: clip.pats.map((x) => x.k) };
}
export const pjCopy = (h, p, ks) => pjClipOut(copyPatterns(pjModel(pjGet(h)), p, [...ks].sort((a, b) => a - b)));
export const pjFree = (c) => CLIPS.delete(c);
export async function pjReadFizzpat(bytes, file = "") {
  try { return { ok: true, ...pjClipOut(await readFizzpat(bytes, file)) }; } catch (e) { return pjFail(e); }
}
// a kept project as a store of its own (slot 1 of an empty store of the session's layout, with the session's MY
// PRESETS, so its presets keep their ids where the FM-1 has them): what its card draws, its patterns copied out
function pjKeptModel(h, kept) {
  const s = h ? PJ.get(h) : null, b = s ? s.base : null;
  const m = b ? { layout: b.layout, magic: b.magic, lib: new Map(b.lib), prj: new Map() }
    : { layout: LAYOUTS.device, magic: LAYOUTS.device.magic, lib: new Map(), prj: new Map() };
  loadProject(m, 0, kept);
  return m;
}
export async function pjCopyKept(h, bytes, file, ks) {
  try {
    const kept = await readFizzprj(bytes, file);
    return { ok: true, ...pjClipOut(copyPatterns(pjKeptModel(h, kept), 0, [...ks].sort((a, b) => a - b))) };
  } catch (e) { return pjFail(e); }
}
export function pjFizzpat(c, { fizz = "", date = new Date() } = {}) {
  const clip = CLIPS.get(c);
  if (!clip) throw new Error("nothing copied");
  return { bytes: makeFizzpat(clip, { fizz, date }), fileName: patFileName(clip) };
}
export function pjKeep(h, p, { fizz = "", date = new Date() } = {}) {
  const kept = keepProject(pjModel(pjGet(h)), p);
  return { bytes: makeFizzprj(kept, { fizz, date }), fileName: prjFileName(kept), name: kept.name, pats: kept.pats,
    sounds: kept.sounds, colours: kept.colours };
}
export async function pjKeptInfo(bytes, file = "") {
  try {
    const k = await readFizzprj(bytes, file), x = pjProject(pjKeptModel(0, k), 0);
    return { ok: true, name: k.name, demo: k.demo, pats: k.pats, sounds: k.sounds, colours: k.colours, fizz: k.fizz,
      date: k.date || "", used: x.pats.map((t) => !!t) };
  } catch (e) { return pjFail(e); }
}
export async function pjRenameKept(bytes, file, name, { fizz = "", date = new Date() } = {}) {
  const k = renameKept(await readFizzprj(bytes, file), name);
  return { bytes: makeFizzprj(k, { fizz, date }), fileName: prjFileName(k), name: k.name };
}

// the demo unit's patterns: each project's taken from the image before (the same name, else the same slot) or made up
// from its name ('P' 6 records: six tracks on page 1, their sounds MY presets by reference; a few steps lock pool
// places that reference MY presets); a demo project's A01..A03 left to the demos. Synthetic: never a unit's data.
const DEMO_TSET = 43;                                     // (QD_TSET_SIZE_MFX: a 'P' 4..6's track settings)
function demoPat(rnd, ids, places) {
  const b = [0x50, 6, 20, DEMO_TSET];
  const p16 = (v) => { b.push(v & 0xFF, (v >> 8) & 0xFF); };
  p16(100); b.push(0, 1); p16(120); p16(0);              // master, swing, used, bpm, nlock
  const refs = Array.from({ length: 6 }, () => ids[Math.floor(rnd() * ids.length)] || 0);
  refs.forEach(p16);
  for (let i = 0; i < DEMO_TSET * 6 + 4 * 6; i++) b.push(0);   // the track settings, lock_start, lock_n
  const dens = [0, 0.38, 0.2, 0.3, 0.16, 0.24];
  for (let t = 0; t < 6; t++) {
    let sm = 0;
    if (t === 0) { for (let x = 0; x < 16; x += 4) sm |= 1 << x; if (rnd() < 0.5) sm |= 1 << 14; }
    else if (rnd() >= 0.25) for (let x = 0; x < 16; x++) if (rnd() < dens[t]) sm |= 1 << x;
    if (!sm) { b.push(0); continue; }
    b.push(1);
    p16(sm);
    for (let x = 0; x < 16; x++) {
      if (!((sm >> x) & 1)) continue;
      const w = new Array(20).fill(0);
      w[2] = 1; w[4] = 36 + t * 7 + (x % 5); w[10] = 90 + Math.floor(rnd() * 37); w[11] = 4; w[14] = 100;
      if (places.length && rnd() < 0.12) w[16] = places[Math.floor(rnd() * places.length)];
      b.push(...w);
    }
  }
  refs.forEach(() => { b.push(0, 0); });                 // (each track sound by reference: its pool sound, edited)
  return Uint8Array.from(b);
}
export function demoPatterns(image, { prev = null, projects = [] } = {}) {
  const m = modelOf(image);
  let pm = null;
  try { pm = prev ? modelOf(prev) : null; } catch (_) { pm = null; }
  const at = 2 + NAME + BLOB;
  const ids = [...m.lib.values()].filter((r) => r[0] === 0x53 && r[1] === 5 && r.length >= at + 2).map((r) => r[at] | (r[at + 1] << 8));
  const own = (mm, p) => [...mm.prj.keys()].filter((k) => k >>> 24 !== RK.SET && k >>> 24 !== RK.HDR && ((k >>> 16) & 0xFF) === p);
  for (const sp of projects) {
    const p = sp.slot - 1, h = m.prj.get(rkey(RK.HDR, p, 0));
    if (!h) continue;
    if (sp.demo) { const g = h.slice(); g[2 + NAME] = 1; m.prj.set(rkey(RK.HDR, p, 0), g); }
    let from = -1;
    if (pm) {
      const there = Array.from({ length: pjLimits(pm).nproj }, (_, q) => pjProject(pm, q));
      from = there.findIndex((x) => x.there && x.name === sp.name);
      if (from < 0 && there[p] && there[p].there && sp.pats) from = p;
    }
    if (from >= 0) { for (const k of own(pm, from)) m.prj.set(rkey(k >>> 24, p, k & 0xFFFF), pm.prj.get(k)); continue; }
    let seed = 7;
    for (const ch of sp.name) seed = (seed * 31 + ch.charCodeAt(0)) >>> 0;
    const rnd = () => {
      seed = (seed + 0x6d2b79f5) >>> 0;
      let t = seed;
      t = Math.imul(t ^ (t >>> 15), t | 1);
      t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
      return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
    };
    const places = [];
    for (let n = 1; ids.length && n <= 12; n++) {
      const id = ids[Math.floor(rnd() * ids.length)];
      m.prj.set(rkey(RK.SND, p, n), Uint8Array.from([0x52, 1, id & 0xFF, (id >> 8) & 0xFF]));
      places.push(n);
    }
    const k0 = sp.demo ? DEMO_N : 0;
    for (let i = 0; i < (sp.pats || 0) - k0 && k0 + i < NPAT; i++) m.prj.set(rkey(RK.PAT, p, k0 + i), demoPat(rnd, ids, places));
  }
  return imageOf(m);
}

// ---- KEPT: the phone's own presets (Documents/Presets, one .fizzsnd each, seen in the Files app): js/library.js's
// .fizzsnd as it is, the web tags beside FIZZ's tags as "webTags" (parseFizzsnd passes over it; written only when a
// preset has some, so a preset without is the web Manager's file byte for byte)
//   keptFile(sound, webTags, source) -> Uint8Array        keptRead(bytes, file) -> { sound, webTags }
//   keptLib(entries [{sound, webTags}], {name, fizz, date}) -> makeFizzlib's ZIP, each member a keptFile
//   keptFileName(name) -> soundFileName ("PLUCK 2.fizzsnd")
import { zipStored, soundFileName, libraryFileName, LIB_FORMAT as KEPT_LIB, VERSION as KEPT_VERSION } from "../../site/js/library.js";

const keptText = (sound, webTags = [], source = {}) => {
  const t = soundToFizzsnd(sound, source), tags = Array.from(webTags || [], (x) => String(x)).filter((x) => x);
  if (!tags.length) return t;
  const j = JSON.parse(t);
  j.webTags = tags;
  return JSON.stringify(j, null, 2) + "\n";
};
export const keptFile = (sound, webTags = [], source = {}) => new TextEncoder().encode(keptText(sound, webTags, source));
export function keptRead(bytes, file = "") {
  const sound = parseFizzsnd(bytes, file);
  const j = JSON.parse(new TextDecoder().decode(bytes));
  return { sound, webTags: Array.isArray(j.webTags) ? j.webTags.map((x) => String(x)) : [] };
}
export function keptLib(entries, { name, fizz = "", date = new Date() } = {}) {
  const enc = new TextEncoder(), list = Array.from(entries);
  const files = list.map((e, i) => `${String(i + 1).padStart(3, "0")} ${soundFileName(e.sound.name)}`);
  const meta = { format: KEPT_LIB, version: KEPT_VERSION, name: name || libraryFileName(date).replace(/\.fizzlib$/, ""),
    date: date.toISOString(), fizz: fizz || "", count: list.length, files };
  return zipStored([{ name: "library.json", bytes: enc.encode(JSON.stringify(meta, null, 2) + "\n") },
    ...list.map((e, i) => ({ name: files[i], bytes: enc.encode(keptText(e.sound, e.webTags, { fizz: fizz || (e.sound.source && e.sound.source.fizz) || "" })) }))], date);
}
export const keptFileName = (name) => soundFileName(name);
