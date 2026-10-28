// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
// FIZZ's backup file, .fizzbak (docs/WEB-MANAGER-PLAN.md §4.1): a ZIP (stored, as .fizzlib) that can be opened and
// inspected with any ZIP tool. No DOM: the tests run it in Node.
//
//   manifest.json            { "format": "fizz-backup", "version": 1, "kind": "full" | "presets", "date": ISO,
//                              "fizz": "0.11.0-beta.2", "unit": { "identity": "FM-1_46001100", "port": "FIZZ FM-1" },
//                              "store": { "layout": "device" (the USB package's; "ble" the Bluetooth package's, magic
//                                         "Z2FB", region [0xAF000, 200704]), "magic": "Z2FS", "format": 2, "tracks": 6,
//                                         "region": [0x97000, 299008], "crc32": "1a2b3c4d", "file": "store.bin" },
//                              "projects": [{ "slot": 1, "name": "NOIR" }], "presets": 143,
//                              "web": { "tags": { "<sound key>": ["SHIMMER"] } }, "transfer": { .. the timing .. } }
//   store.bin                the store's region exactly as the unit holds it (FZBI / FZBR)            (kind "full")
//   presets/NNN NAME.fizzsnd every MY preset as .fizzsnd (DUMP), the portable layer, with "web": {"tags"} (every kind)
//   library.json             the same presets as a .fizzlib's index, so the preview's Import reads a .fizzbak too
//
//   makeFizzbak({kind, image, region, crc, presets, fizz, unit, web, transfer, date}) -> Uint8Array
//   await readBackup(bytes, fileName) -> {kind, manifest, image | null, summary (fizzstore readRegion) | null,
//                                         presets: [sound], web, source: "fizzbak" | "fizzlib" | "bin"} (throws)
//   soundKey(sound)          a sound's key for the web tags: its name and its FM TONE values (blob bytes 2..86, the
//                            same in a version-5 and a version-6 blob), as library.js sameSound compares them
//   backupFileName(date, kind)
import { zipStored, soundToFizzsnd, parseFizzsnd, parseFizzlib, LIB_FORMAT, VERSION as LIB_VERSION } from "../js/library.js";
import { dnReadZip } from "../js/fm1dnsnd.js";
import { readRegion, layoutOf } from "./fizzstore.js";
import { FIZZ_TAGS } from "../js/fizz_sysex.js";

export const FORMAT = "fizz-backup", VERSION = 1;
const enc = new TextEncoder(), dec = new TextDecoder();

const CRC_T = (() => { const t = new Uint32Array(256); for (let n = 0; n < 256; n++) { let c = n; for (let k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320 ^ (c >>> 1) : c >>> 1; t[n] = c >>> 0; } return t; })();
export function crc32(b, c = 0) {
  c = (c ^ 0xFFFFFFFF) >>> 0;
  for (let i = 0; i < b.length; i++) c = CRC_T[(c ^ b[i]) & 0xFF] ^ (c >>> 8);
  return (c ^ 0xFFFFFFFF) >>> 0;
}
export const hex8 = (n) => (n >>> 0).toString(16).padStart(8, "0");

export function soundKey(s) {
  const b = new Uint8Array(s.name.length + 1 + 85);
  b.set(enc.encode(s.name), 0);
  b.set(s.blob.subarray(2, 87), s.name.length + 1);
  return hex8(crc32(b));
}

const safe = (s) => String(s || "").replace(/[\\/:*?"<>|\x00-\x1f]+/g, "_").trim() || "PRESET";
const pad = (n) => String(n).padStart(2, "0");
export function backupFileName(date = new Date(), kind = "full") {
  const d = `${date.getFullYear()}-${pad(date.getMonth() + 1)}-${pad(date.getDate())} ${pad(date.getHours())}${pad(date.getMinutes())}`;
  return `FIZZ ${kind === "presets" ? "presets" : "backup"} ${d}.fizzbak`;
}

export function makeFizzbak({ kind = "full", image = null, region = null, crc = null, summary = null, presets = [], fizz = "",
                              unit = {}, web = {}, transfer = null, date = new Date() } = {}) {
  if (kind === "full" && !(image instanceof Uint8Array)) throw new Error("a full backup needs the store's image");
  const sum = summary || (image ? readRegion(image) : null);
  const files = presets.map((s, i) => `presets/${String(i + 1).padStart(3, "0")} ${safe(s.name)}.fizzsnd`);
  const tags = web.tags || {};
  const manifest = { format: FORMAT, version: VERSION, kind, date: date.toISOString(), fizz,
    unit: { identity: unit.identity || "", port: unit.port || "" },
    ...(image ? { store: { layout: sum.layout, magic: sum.magic, format: sum.format, tracks: sum.tracks,
      region: region || [0, image.length], crc32: hex8(crc ?? crc32(image)), file: "store.bin",
      used: Math.round(sum.used.all * 1000) / 1000 } } : {}),
    projects: sum ? sum.projects.map(({ slot, name }) => ({ slot, name })) : [],
    presets: presets.length, files, web: { tags }, ...(transfer ? { transfer } : {}) };
  const lib = { format: LIB_FORMAT, version: LIB_VERSION, name: backupFileName(date, kind).replace(/\.fizzbak$/, ""),
    date: date.toISOString(), fizz, count: presets.length, files };
  const snd = presets.map((s) => {
    const j = JSON.parse(soundToFizzsnd(s, { fizz: fizz || (s.source && s.source.fizz) || "" }));
    const t = tags[soundKey(s)];
    if (t && t.length) j.web = { tags: t };
    return enc.encode(JSON.stringify(j, null, 2) + "\n");
  });
  return zipStored([
    { name: "manifest.json", bytes: enc.encode(JSON.stringify(manifest, null, 2) + "\n") },
    ...(image ? [{ name: "store.bin", bytes: image }] : []),
    ...presets.map((_, i) => ({ name: files[i], bytes: snd[i] })),
    { name: "library.json", bytes: enc.encode(JSON.stringify(lib, null, 2) + "\n") },
  ], date);
}

// the central directory's CRC-32 of each member (dnReadZip reads the members, not their CRCs)
function zipCrcs(b) {
  const u32 = (o) => (b[o] | (b[o + 1] << 8) | (b[o + 2] << 16) | (b[o + 3] << 24)) >>> 0, u16 = (o) => b[o] | (b[o + 1] << 8);
  let e = -1;
  for (let i = b.length - 22; i >= Math.max(0, b.length - 22 - 65535); i--) if (u32(i) === 0x06054b50) { e = i; break; }
  const out = new Map();
  if (e < 0) return out;
  let p = u32(e + 16);
  for (let k = 0; k < u16(e + 10) && p + 46 <= b.length && u32(p) === 0x02014b50; k++) {
    const nl = u16(p + 28);
    out.set(dec.decode(b.subarray(p + 46, p + 46 + nl)), u32(p + 16));
    p += 46 + nl + u16(p + 30) + u16(p + 32);
  }
  return out;
}

const fromRecord = (p, fizz) => ({ fizz: true, name: p.name, blob: p.blob, drive: p.drive, pan: p.pan, sends: p.sends,
  fizzTags: FIZZ_TAGS.filter((_, k) => (p.tags >> k) & 1), favourite: false, source: { fizz, from: `MY PRESETS ${p.slot}` },
  notes: [], origin: [], mark: "", tags: [] });

export async function readBackup(bytes, file = "") {
  if (!(bytes instanceof Uint8Array)) bytes = new Uint8Array(bytes);
  const isZip = bytes.length > 4 && bytes[0] === 0x50 && bytes[1] === 0x4B && bytes[2] === 3 && bytes[3] === 4;
  if (!isZip) {                                       // a raw image (firmware/tools/fizz_xdata.py backup FILE)
    if (!layoutOf(bytes.length)) throw new Error(`${file || "this file"}: not a FIZZ backup`);
    const summary = readRegion(bytes);
    return { kind: "full", source: "bin", manifest: { format: FORMAT, version: VERSION, kind: "full", fizz: "", date: null,
      store: { layout: summary.layout, magic: summary.magic, format: summary.format, tracks: summary.tracks, crc32: hex8(crc32(bytes)) } },
    image: bytes, summary, presets: summary.presets.map((p) => fromRecord(p, "")), web: { tags: {} } };
  }
  const zip = await dnReadZip(bytes).catch(() => { throw new Error(`${file || "this file"}: a damaged ZIP`); });
  const crcs = zipCrcs(bytes);
  const member = async (n) => {
    const d = await zip.get(n);
    if (d && crcs.has(n) && crc32(d) !== crcs.get(n)) throw new Error(`${file || "the backup"}: ${n} is damaged (its CRC-32)`);
    return d;
  };
  const mt = await member("manifest.json");
  if (!mt) {                                          // a .fizzlib: presets only
    const lib = await parseFizzlib(bytes, file);
    const bad = lib.items.filter((x) => !x.ok);
    if (bad.length && bad.length === lib.items.length) throw new Error(bad[0].error);
    return { kind: "presets", source: "fizzlib", manifest: { format: LIB_FORMAT, fizz: lib.meta.fizz || "", date: lib.meta.date || null },
      image: null, summary: null, presets: lib.items.filter((x) => x.ok).map((x) => x.sound), web: { tags: {} }, skipped: bad };
  }
  let m;
  try { m = JSON.parse(dec.decode(mt)); } catch (_) { throw new Error(`${file || "the backup"}: manifest.json is not JSON`); }
  if (!m || m.format !== FORMAT) throw new Error(`${file || "this file"}: not a FIZZ backup (format ${JSON.stringify(m && m.format)})`);
  if (!(m.version >= 1) || m.version > VERSION) throw new Error(`a newer FIZZ backup (version ${m.version}): update this page`);
  let image = null, summary = null;
  if (m.store) {
    image = await member(m.store.file || "store.bin");
    if (!image) throw new Error(`${file || "the backup"}: store.bin is missing`);
    if (m.store.crc32 && hex8(crc32(image)) !== m.store.crc32) throw new Error(`${file || "the backup"}: store.bin is not the image the unit gave (CRC-32)`);
    summary = readRegion(image);
  }
  const names = (m.files && m.files.length ? m.files : zip.names.filter((n) => /\.fizzsnd$/i.test(n)).sort());
  const presets = [], tags = { ...((m.web && m.web.tags) || {}) };
  for (const n of names) {
    const d = await member(n);
    if (!d) throw new Error(`${file || "the backup"}: ${n} is missing`);
    const s = parseFizzsnd(d, n);
    presets.push(s);
    try { const j = JSON.parse(dec.decode(d)); if (j.web && Array.isArray(j.web.tags)) tags[soundKey(s)] = j.web.tags.map(String); } catch (_) { /* (parsed above) */ }
  }
  if (m.presets !== undefined && m.presets !== presets.length) throw new Error(`${file || "the backup"}: says ${m.presets} presets, holds ${presets.length}`);
  return { kind: m.kind || (image ? "full" : "presets"), source: "fizzbak", manifest: m, image, summary, presets, web: { tags } };
}
