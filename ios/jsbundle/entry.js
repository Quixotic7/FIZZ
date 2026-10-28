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
export { soundFromLib };
export const editTable = () => ({ values: VALUES, algos: ALGOS, destOrder: DEST_ORDER, q: Q, np: NP });
export const editPack = (s) => ({ blob: blob6(s), drive: s.values[Q.DRIVE] });
export const editTexts = (pairs) => pairs.map(([id, v]) => valueText(id, v));
