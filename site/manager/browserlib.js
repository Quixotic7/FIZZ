// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
// LIBRARY's IN THIS BROWSER (FIZZ NEON): the MY PRESETS of the browser's own FIZZ, read from and written into its
// flash (the 1 MiB image the page keeps in IndexedDB, "browserFlash") without booting it. No DOM: the tests run it in
// Node.
//   browserRows(img) -> [{source: "browser", slot, id, name, tags, imported, rec, lib}]   (lib: js/library.js's shape)
//   recordOf({name, blob (128), pan, chorus, delay, reverb, tags}) -> a MY PRESETS record ('S' 5, id 0)
//   keepIn(img, recs) -> {image, added, same, full}     (each into the first free place; one already there skipped)
//   dropFrom(img, slots) -> image
//   replaceIn(img, slot, rec) -> image   (the record at that place swapped for rec: its id and flags kept, so a project
//                                          that plays it plays the new sound: RENAME, TAGS, EDIT's SAVE)
//   uniqueName(name, taken) -> a name not taken: NAME, else NAME 2, NAME 3 .. (12 characters at most; DUPLICATE)
//   blankImage() -> an empty store in the browser FIZZ's layout
import { readRegion, buildImage, LAYOUTS, layoutOf, convertImage } from "./fizzstore.js";
import { modelOf, imageOf, addPreset, removePreset, rkey, RK } from "./projects.js";
import { FIZZ_TAGS } from "../js/fizz_sysex.js";

const NAME = 13, BLOB = 128, LIBLEN = 2 + NAME + BLOB + 8;

export const blankImage = () => buildImage(LAYOUTS.host, {});
// any store image (a backup's, the unit's layout) as the browser FIZZ's
export const asHost = (img) => (img && img.length === LAYOUTS.host.size ? img : convertImage(img, "host"));

export const libOf = (p, from = "") => ({ fizz: true, name: p.name, blob: p.blob, drive: p.drive, pan: p.pan, sends: { ...p.sends },
  fizzTags: FIZZ_TAGS.filter((_, k) => (p.tags >> k) & 1), favourite: false, source: { from }, notes: [], origin: [], mark: "", tags: [] });

export function browserRows(img) {
  if (!img || !layoutOf(img.length)) return [];
  let sum;
  try { sum = readRegion(img); } catch (_) { return []; }
  return sum.presets.map((p) => ({ source: "browser", slot: p.slot, id: p.id, name: p.name, tags: p.tags, imported: p.imported,
    blob: p.blob, lib: libOf(p, `IN THIS BROWSER ${p.slot}`) }));
}

export function recordOf({ name = "", blob, pan = 0, chorus = 0, delay = 0, reverb = 0, tags = 0 }) {
  if (!(blob instanceof Uint8Array) || blob.length !== BLOB) throw new Error("a 128-byte sound");
  const r = new Uint8Array(LIBLEN);
  r[0] = 0x53; r[1] = 5;
  const nm = String(name).slice(0, NAME - 1);
  for (let i = 0; i < nm.length; i++) { const c = nm.charCodeAt(i); r[2 + i] = c >= 0x20 && c <= 0x7e ? c : 0x3f; }
  r.set(blob, 2 + NAME);
  const o = 2 + NAME + BLOB;
  r[o + 2] = tags & 0x7F;
  r[o + 4] = Math.max(-64, Math.min(63, pan | 0)) & 0xFF;
  r[o + 5] = chorus & 0x7F; r[o + 6] = delay & 0x7F; r[o + 7] = reverb & 0x7F;
  return r;
}

export function keepIn(img, recs) {
  const m = modelOf(img || blankImage());
  let added = 0, same = 0, full = 0;
  const at = [];
  for (const rec of recs) {
    try {
      const x = addPreset(m, rec);
      if (x.same) same++; else added++;
      at.push(x.i + 1);
    } catch (e) { if (e.code === "libfull") full++; else throw e; }
  }
  return { image: added ? imageOf(m) : img || blankImage(), added, same, full, at };
}

export function dropFrom(img, slots) {
  const m = modelOf(img);
  for (const s of slots) removePreset(m, s - 1);
  return imageOf(m);
}

export function replaceIn(img, slot, rec) {
  const m = modelOf(img), k = rkey(RK.LIB, 0, slot - 1), old = m.lib.get(k);
  if (!old || old.length !== LIBLEN) throw new Error(`no preset at ${slot}`);
  const r = rec.slice(), o = 2 + NAME + BLOB;
  r[o] = old[o]; r[o + 1] = old[o + 1]; r[o + 3] = old[o + 3];
  m.lib.set(k, r);
  return imageOf(m);
}

// (the iOS app's KeptPresets.uniqueName: a trailing number counted on)
export function uniqueName(name, taken) {
  const t = new Set(taken);
  if (!t.has(name)) return name;
  let base = name, n = 2;
  const m = / (\d+)$/.exec(name);
  if (m) { base = name.slice(0, m.index); n = +m[1] + 1; }
  for (;; n++) {
    const tail = ` ${n}`, c = base.slice(0, NAME - 1 - tail.length).trim() + tail;
    if (!t.has(c)) return c;
  }
}
