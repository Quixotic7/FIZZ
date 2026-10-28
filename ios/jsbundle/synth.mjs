// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// Synthetic inputs for test.mjs and gen_fixtures.mjs (no owner data, no real presets): sounds with fixed-seed blobs,
// their DUMP replies, web tags, backup stats, a fixed date; a ZIP with deflated members; and UTC_DATES, the snippet
// that makes Date's local getters UTC ones in a realm (the ZIP's DOS times and the file names are local time: with it
// Node and JavaScriptCore agree on any machine's time zone; the Swift tests evaluate the same text).
import { deflateRawSync } from "node:zlib";
import { storeSound } from "../../site/js/fizz_sysex.js";
import { crc32 } from "../../site/manager/fizzbak.js";

export const UTC_DATES = 'for (const k of ["FullYear", "Month", "Date", "Day", "Hours", "Minutes", "Seconds", "Milliseconds"]) ' +
  'Date.prototype["get" + k] = Date.prototype["getUTC" + k]; Date.prototype.getTimezoneOffset = function () { return 0; };';

export const DATE = new Date(Date.UTC(2026, 9, 27, 14, 32, 10, 250));

// mulberry32
export function rng(seed) {
  return () => {
    seed = (seed + 0x6d2b79f5) >>> 0;
    let t = seed;
    t = Math.imul(t ^ (t >>> 15), t | 1);
    t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
}
export function blobOf(seed, version = 6, drive = 0) {
  const r = rng(seed), b = new Uint8Array(version === 6 ? 128 : 96);
  for (let i = 2; i < b.length; i++) b[i] = Math.floor(r() * 256);
  b[0] = 0x51; b[1] = version;
  if (version === 6) b[87] = drive;
  return b;
}

// the sounds (js/library.js's shape), as a DUMP of MY PRESETS gives them back
export const SOUNDS = [
  { name: "SYN BASS 1", seed: 11, drive: 12, pan: -20, sends: { chorus: 0, delay: 30, reverb: 40 }, fizzTags: ["BASS"] },
  { name: "A/B KEYS", seed: 22, drive: 0, pan: 0, sends: { chorus: 5, delay: 0, reverb: 0 }, fizzTags: ["KEYS", "PLUCK"] },
  { name: "PAD*WIDE", seed: 33, drive: 127, pan: 63, sends: { chorus: 127, delay: 127, reverb: 127 }, fizzTags: [] },
  { name: "LEAD", seed: 44, drive: 3, pan: -64, sends: { chorus: 1, delay: 2, reverb: 3 }, fizzTags: ["LEAD", "FX"] },
].map((s, k) => ({ fizz: true, name: s.name, blob: blobOf(s.seed, 6, s.drive), drive: s.drive, pan: s.pan, sends: s.sends,
  fizzTags: s.fizzTags, favourite: false, source: { fizz: "0.12.0-beta.11", from: `MY PRESETS ${k + 1}` }, notes: [], origin: [],
  mark: "", tags: [] }));
export const V5 = { fizz: true, name: "OLD FIVE", blob: blobOf(55, 5), drive: 9, pan: 4, sends: { chorus: 1, delay: 0, reverb: 2 },
  fizzTags: ["DRUMS"], favourite: true, source: { fizz: "0.9.0" }, notes: [], origin: [], mark: "", tags: [] };

// a DUMP reply: a STORE6 / STORE frame with the DUMP flag (fizz_sysex.js storeSound, the flag set, the checksum again)
export function dumpFrame(s, slot) {
  const f = Array.from(storeSound({ name: s.name, blob: s.blob, drive: s.drive, slot, pan: s.pan, chorus: s.sends.chorus,
    delay: s.sends.delay, reverb: s.sends.reverb, tags: s.fizzTags.length ? s.fizzTags : undefined }));
  f[6] |= 0x08;
  f[f.length - 2] = (0x80 - (f.slice(5, -2).reduce((a, x) => a + x, 0) & 0x7f)) & 0x7f;
  return Uint8Array.from(f);
}
export const FRAMES = SOUNDS.map((s, k) => dumpFrame(s, k + 1));
export const PROJECTS = [{ slot: 1, name: "NOIR" }, { slot: 3, name: "SUNDAY 2" }];
export const STATS = { bytes: 299008, ms: 4321.678, kbps: 67.5671, frames: 1169, resent: 2, depth: 2, gapMs: 0, perFrame: 3.69691,
  rtt: { avg: 3.3, median: 3.21234, p95: 5.67891, max: 9.87654 } };
export const UNIT = { identity: "FM-1_46001100", port: "FIZZ FM-1" };
export const FIZZ = "0.12.0-beta.11";
export const REGION = [0x97000, 299008];

// a ZIP whose members are deflated (method 8): what a ZIP tool's re-zip of a .fizzlib looks like
export function deflatedZip(entries, date = DATE) {
  const enc = new TextEncoder(), parts = [], dir = [];
  const dt = ((date.getFullYear() - 1980) << 25 | (date.getMonth() + 1) << 21 | date.getDate() << 16 | date.getHours() << 11 | date.getMinutes() << 5 | (date.getSeconds() >> 1)) >>> 0;
  let off = 0;
  for (const e of entries) {
    const name = enc.encode(e.name), raw = e.bytes, data = new Uint8Array(deflateRawSync(raw, { level: 9 })), crc = crc32(raw);
    const h = new DataView(new ArrayBuffer(30));
    h.setUint32(0, 0x04034b50, true); h.setUint16(4, 20, true); h.setUint16(8, 8, true); h.setUint32(10, dt, true);
    h.setUint32(14, crc, true); h.setUint32(18, data.length, true); h.setUint32(22, raw.length, true); h.setUint16(26, name.length, true);
    parts.push(new Uint8Array(h.buffer), name, data);
    const c = new DataView(new ArrayBuffer(46));
    c.setUint32(0, 0x02014b50, true); c.setUint16(4, 20, true); c.setUint16(6, 20, true); c.setUint16(10, 8, true);
    c.setUint32(12, dt, true); c.setUint32(16, crc, true); c.setUint32(20, data.length, true); c.setUint32(24, raw.length, true);
    c.setUint16(28, name.length, true); c.setUint32(42, off, true);
    dir.push(new Uint8Array(c.buffer), name);
    off += 30 + name.length + data.length;
  }
  const dirLen = dir.reduce((n, p) => n + p.length, 0), end = new DataView(new ArrayBuffer(22));
  end.setUint32(0, 0x06054b50, true); end.setUint16(8, entries.length, true); end.setUint16(10, entries.length, true);
  end.setUint32(12, dirLen, true); end.setUint32(16, off, true);
  const all = [...parts, ...dir, new Uint8Array(end.buffer)], out = new Uint8Array(all.reduce((n, p) => n + p.length, 0));
  let o = 0; for (const p of all) { out.set(p, o); o += p.length; }
  return out;
}
// a stored ZIP's members (name, bytes), for re-zipping
export function storedMembers(b) {
  const u32 = (o) => (b[o] | (b[o + 1] << 8) | (b[o + 2] << 16) | (b[o + 3] << 24)) >>> 0, u16 = (o) => b[o] | (b[o + 1] << 8);
  const out = [];
  for (let p = 0; u32(p) === 0x04034b50; ) {
    const cs = u32(p + 18), nl = u16(p + 26), xl = u16(p + 28), name = new TextDecoder().decode(b.subarray(p + 30, p + 30 + nl));
    out.push({ name, bytes: b.slice(p + 30 + nl + xl, p + 30 + nl + xl + cs) });
    p += 30 + nl + xl + cs;
  }
  return out;
}
