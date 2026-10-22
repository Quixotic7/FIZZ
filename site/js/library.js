// SPDX-License-Identifier: GPL-3.0-only
// FIZZ preset files: MY PRESETS exported from FIZZ (its LIST and DUMP SysEx, being defined: docs/SYSEX.md) as files to
// keep, share and send back (js/digitone.js's STORE flow). No DOM: the tests run it in Node.
//
// .fizzsnd: one preset, JSON (readable, robust, diff-able):
//   { "format": "fizz-sound", "version": 1, "name": "PLUCK 2", "tags": ["PLUCK"], "favourite": false,
//     "pan": 0, "sends": { "chorus": 0, "delay": 20, "reverb": 40 }, "drive": 0,
//     "patch": { "blob": "<base64>", "blobVersion": 5 | 6 },
//     "source": { "fizz": "0.7.4-p1.5x", "exported": "<ISO date>", "from": "MY PRESETS 12" } }
//   blob: FIZZ's FM TONE patch exactly as STORE / STORE6 carry it (96 bytes version 5, 128 bytes version 6).
// .fizzlib: several, a ZIP (stored) of "NNN NAME.fizzsnd" files plus library.json:
//   { "format": "fizz-library", "version": 1, "name": "...", "author": "..." (optional), "date": "<ISO>",
//     "fizz": "<the FIZZ version>", "count": n, "files": ["001 PLUCK 2.fizzsnd", ..] }
//
//   soundToFizzsnd(sound, source) -> JSON text          parseFizzsnd(bytes | text, fileName) -> sound (throws)
//   makeFizzlib(sounds, meta) -> Uint8Array (a ZIP)      await parseFizzlib(bytes) -> { meta, items: [{ file, ok, sound | error }] }
//   soundFromStoreFrame(frame) -> sound                  (a DUMP reply is a ready-to-send STORE / STORE6 frame)
//   sameSound(a, b)                                      the duplicate check: the same name and the same patch
//   libraryFileName(date), soundFileName(name)           "FIZZ presets 2026-10-22.fizzlib", "PLUCK 2.fizzsnd"
//   createPresetLink({ sendMidi, onMidiOut })            LIST / DUMP (docs/SYSEX.md sections 21, 22)
//
// A sound here has js/digitone.js's shape: { name, blob, drive, pan, sends: {chorus, delay, reverb}, fizzTags: [..],
// notes: [], origin: [], mark: "", tags: [] } plus { fizz: true, favourite, source } for a FIZZ preset.
import { dnReadZip } from "./fm1dnsnd.js";
import { FIZZ_TAGS, unpack7, parseReply, listRequest, parseList, dumpRequest, parseDump } from "./fizz_sysex.js";

export const FORMAT = "fizz-sound", LIB_FORMAT = "fizz-library", VERSION = 1;
const BLOB = { 5: 96, 6: 128 };
const NOT = "not a FIZZ sound file";

const b64 = (u8) => { let s = ""; for (let i = 0; i < u8.length; i += 0x8000) s += String.fromCharCode(...u8.subarray(i, i + 0x8000)); return btoa(s); };
const unb64 = (s) => Uint8Array.from(atob(s), (c) => c.charCodeAt(0));
const clampInt = (v, lo, hi, what) => {
  if (v === undefined || v === null) return undefined;
  if (!Number.isInteger(v) || v < lo || v > hi) throw new Error(`${NOT}: ${what} ${JSON.stringify(v)} is not ${lo}..${hi}`);
  return v;
};

export function soundToFizzsnd(sound, source = {}) {
  const v = sound.blob.length === 128 ? 6 : 5;
  return JSON.stringify({
    format: FORMAT, version: VERSION, name: sound.name, tags: sound.fizzTags || [], favourite: !!sound.favourite,
    pan: sound.pan | 0, sends: { chorus: sound.sends.chorus | 0, delay: sound.sends.delay | 0, reverb: sound.sends.reverb | 0 },
    drive: sound.drive | 0, patch: { blob: b64(sound.blob), blobVersion: v },
    source: { ...(sound.source || {}), ...source },
  }, null, 2) + "\n";
}

export function parseFizzsnd(input, file = "") {
  let j;
  try { j = JSON.parse(typeof input === "string" ? input : new TextDecoder().decode(input)); }
  catch (_) { throw new Error(`${NOT} (not JSON)`); }
  if (!j || j.format !== FORMAT) throw new Error(`${NOT} (format ${JSON.stringify(j && j.format)}, not "${FORMAT}")`);
  if (j.version !== VERSION) throw new Error(`a newer FIZZ sound file (version ${j.version}): not supported`);
  if (typeof j.name !== "string" || !j.name.trim()) throw new Error(`${NOT}: no name`);
  const p = j.patch || {};
  if (!(p.blobVersion in BLOB) || typeof p.blob !== "string") throw new Error(`${NOT}: no patch (blob and blobVersion 5 or 6)`);
  let blob;
  try { blob = unb64(p.blob); } catch (_) { throw new Error(`${NOT}: the patch is not base64`); }
  if (blob.length !== BLOB[p.blobVersion] || blob[0] !== 0x51 || blob[1] !== p.blobVersion)
    throw new Error(`${NOT}: the patch is not an FM TONE version ${p.blobVersion} blob (${blob.length} bytes)`);
  const tags = Array.isArray(j.tags) ? j.tags.map((t) => String(t).toUpperCase()) : [];
  const bad = tags.filter((t) => !FIZZ_TAGS.includes(t));
  if (bad.length) throw new Error(`${NOT}: unknown tags ${bad.join(", ")}`);
  const s = j.sends || {};
  return {
    file, fizz: true, name: j.name.trim().slice(0, 12), blob, drive: clampInt(j.drive ?? 0, 0, 127, "drive"),
    pan: clampInt(j.pan ?? 0, -64, 63, "pan"),
    sends: { chorus: clampInt(s.chorus ?? 0, 0, 127, "chorus"), delay: clampInt(s.delay ?? 0, 0, 127, "delay"), reverb: clampInt(s.reverb ?? 0, 0, 127, "reverb") },
    fizzTags: FIZZ_TAGS.filter((t) => tags.includes(t)), favourite: !!j.favourite, source: j.source || {},
    notes: [], origin: [], mark: j.source && j.source.fizz ? `FIZZ ${j.source.fizz}` : "", tags: [],
  };
}

// ---- a ZIP, stored (no compression: the files are small JSON; any ZIP tool opens it)
const CRC = new Int32Array(256).map((_, n) => { let c = n; for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1; return c; });
const crc32 = (b) => { let c = -1; for (let i = 0; i < b.length; i++) c = CRC[(c ^ b[i]) & 0xff] ^ (c >>> 8); return (c ^ -1) >>> 0; };
export function zipStored(entries /* [{ name, bytes }] */, date = new Date()) {
  const enc = new TextEncoder(), parts = [], dir = [];
  const dt = ((date.getFullYear() - 1980) << 25 | (date.getMonth() + 1) << 21 | date.getDate() << 16 | date.getHours() << 11 | date.getMinutes() << 5 | (date.getSeconds() >> 1)) >>> 0;
  let off = 0;
  for (const e of entries) {
    const name = enc.encode(e.name), data = e.bytes, crc = crc32(data);
    const h = new DataView(new ArrayBuffer(30));
    h.setUint32(0, 0x04034b50, true); h.setUint16(4, 20, true); h.setUint16(6, 0x0800, true); h.setUint16(8, 0, true);
    h.setUint32(10, dt, true); h.setUint32(14, crc, true); h.setUint32(18, data.length, true); h.setUint32(22, data.length, true);
    h.setUint16(26, name.length, true); h.setUint16(28, 0, true);
    parts.push(new Uint8Array(h.buffer), name, data);
    const c = new DataView(new ArrayBuffer(46));
    c.setUint32(0, 0x02014b50, true); c.setUint16(4, 20, true); c.setUint16(6, 20, true); c.setUint16(8, 0x0800, true); c.setUint16(10, 0, true);
    c.setUint32(12, dt, true); c.setUint32(16, crc, true); c.setUint32(20, data.length, true); c.setUint32(24, data.length, true);
    c.setUint16(28, name.length, true); c.setUint32(42, off, true);
    dir.push(new Uint8Array(c.buffer), name);
    off += 30 + name.length + data.length;
  }
  const dirLen = dir.reduce((n, p) => n + p.length, 0);
  const end = new DataView(new ArrayBuffer(22));
  end.setUint32(0, 0x06054b50, true); end.setUint16(8, entries.length, true); end.setUint16(10, entries.length, true);
  end.setUint32(12, dirLen, true); end.setUint32(16, off, true);
  const all = [...parts, ...dir, new Uint8Array(end.buffer)], out = new Uint8Array(all.reduce((n, p) => n + p.length, 0));
  let o = 0; for (const p of all) { out.set(p, o); o += p.length; }
  return out;
}

const safe = (s) => String(s || "").replace(/[\\/:*?"<>|\x00-\x1f]+/g, "_").trim() || "PRESET";
export const soundFileName = (name) => `${safe(name)}.fizzsnd`;
export const libraryFileName = (date = new Date()) => `FIZZ presets ${date.toISOString().slice(0, 10)}.fizzlib`;

export function makeFizzlib(sounds, { name, author, fizz, date = new Date() } = {}) {
  const enc = new TextEncoder();
  const files = sounds.map((s, i) => `${String(i + 1).padStart(3, "0")} ${safe(s.name)}.fizzsnd`);
  const meta = { format: LIB_FORMAT, version: VERSION, name: name || libraryFileName(date).replace(/\.fizzlib$/, ""),
    ...(author ? { author } : {}), date: date.toISOString(), fizz: fizz || "", count: sounds.length, files };
  return zipStored([{ name: "library.json", bytes: enc.encode(JSON.stringify(meta, null, 2) + "\n") },
    ...sounds.map((s, i) => ({ name: files[i], bytes: enc.encode(soundToFizzsnd(s, { fizz: fizz || (s.source && s.source.fizz) || "" })) }))], date);
}

export async function parseFizzlib(bytes, file = "") {
  let zip;
  try { zip = await dnReadZip(bytes); } catch (_) { throw new Error("not a FIZZ library (not a ZIP archive)"); }
  const mt = await zip.get("library.json");
  if (!mt) throw new Error("not a FIZZ library (no library.json)");
  let meta;
  try { meta = JSON.parse(new TextDecoder().decode(mt)); } catch (_) { throw new Error("not a FIZZ library (library.json is not JSON)"); }
  if (!meta || meta.format !== LIB_FORMAT) throw new Error(`not a FIZZ library (format ${JSON.stringify(meta && meta.format)})`);
  if (meta.version !== VERSION) throw new Error(`a newer FIZZ library (version ${meta.version}): not supported`);
  const names = zip.names.filter((n) => /\.fizzsnd$/i.test(n)).sort();
  const items = [];
  for (const n of names) {
    const label = `${file ? file + ": " : ""}${n.replace(/^.*\//, "")}`;
    try { items.push({ file: label, ok: true, sound: parseFizzsnd(await zip.get(n), label) }); }
    catch (e) { items.push({ file: label, ok: false, error: e.message || String(e) }); }
  }
  if (meta.count !== undefined && meta.count !== names.length) items.push({ file: file || "library", ok: false, error: `library.json says ${meta.count} presets, the archive holds ${names.length}` });
  return { meta, items };
}

// ---- a DUMP: a STORE (0x10) / STORE6 (0x11) frame, as docs/SYSEX.md lays out its body
export function soundFromStoreFrame(f) {
  const b = Array.from(f);
  if (b[0] !== 0xf0 || b[1] !== 0x7d || b[2] !== 0x46 || b[3] !== 0x5a || b[b.length - 1] !== 0xf7) throw new Error("not a FIZZ frame");
  const cmd = b[5], body = b.slice(6, -2);
  if (cmd !== 0x10 && cmd !== 0x11) throw new Error(`not a STORE frame (command ${cmd})`);
  const packed = cmd === 0x11 ? 147 : 110, n = 22 + packed;
  if (body.length !== n && body.length !== n + 1) throw new Error(`a STORE body of ${body.length} bytes`);
  const name = String.fromCharCode(...body.slice(3, 15)).replace(/[\s\0]+$/, "");
  const mask = body[21];
  const blob = unpack7(body.slice(22, 22 + packed)).slice(0, cmd === 0x11 ? 128 : 96);
  const tm = body.length > n ? body[n] : 0;
  return { fizz: true, name, blob, drive: body[15], pan: mask & 1 ? body[17] - 64 : 0,
    sends: { chorus: mask & 2 ? body[18] : 0, delay: mask & 4 ? body[19] : 0, reverb: mask & 8 ? body[20] : 0 },
    fizzTags: FIZZ_TAGS.filter((_, k) => (tm >> k) & 1), favourite: false, notes: [], origin: [], mark: "", tags: [] };
}

export function sameSound(a, b) {
  if (!a || !b || a.name !== b.name || a.blob.length !== b.blob.length) return false;
  for (let i = 0; i < a.blob.length; i++) if (a.blob[i] !== b.blob[i] && !(i === 87 && a.blob.length === 96)) return false;   // (v5 byte 87: ignored)
  return true;
}

// ---- LIST / DUMP (docs/SYSEX.md sections 21, 22, P1.6m): fizz_sysex.js's listRequest / parseList, dumpRequest /
// parseDump. LIST answers in parts (16 entries each) until "last"; DUMP slot 0 sends every preset of the source, one
// STORE / STORE6 frame each (with the DUMP flag), as FIZZ's reply ring has room.
// list -> [{ slot, id, name, tags: [FIZZ tag names], imported, inPool }]; dump(slot) -> a frame; dumpAll -> [frames]
export const PRESETS_READY = true;
const tagNames = (m) => FIZZ_TAGS.filter((_, k) => (m >> k) & 1);

// the link: one question at a time; collect(frame) gets each FIZZ frame and returns undefined (not its answer yet)
// or the result (done); the wait restarts with every frame FIZZ sends (a long DUMP is paced, not slow)
export function createPresetLink({ sendMidi, onMidiOut, timeoutMs = 3000 }) {
  let waiting = null, sx = null;
  onMidiOut((bytes) => {
    for (const b of bytes) {
      if (b === 0xf0) sx = [b];
      else if (sx && b === 0xf7) {
        sx.push(b);
        const f = Uint8Array.from(sx); sx = null;
        if (waiting) {
          const w = waiting, r = w.collect(f);
          clearTimeout(w.t);
          if (r !== undefined) { waiting = null; w.resolve(r); }
          else w.t = setTimeout(() => { if (waiting === w) { waiting = null; w.resolve(w.partial ? w.partial() : null); } }, timeoutMs);
        }
      } else if (sx && b < 0x80) sx.push(b);
      else if (sx && b < 0xf8) sx = null;
    }
  });
  const ask = (msg, collect, partial) => new Promise((resolve) => {
    const w = { collect, partial, resolve };
    w.t = setTimeout(() => { if (waiting === w) { waiting = null; resolve(partial ? partial() : null); } }, timeoutMs);
    waiting = w;
    sendMidi(msg);
  });
  return {
    get ready() { return PRESETS_READY; },
    // MY PRESETS' directory
    async list() {
      const rows = [];
      return ask(listRequest({}), (f) => {
        const l = parseList(f);
        if (!l) return undefined;
        for (const e of l.entries) if (e.source === "mine") rows.push({ slot: e.slot, id: e.id, name: e.name, tags: tagNames(e.tags), favourite: false, imported: e.imported, inPool: e.inPool });
        return l.last ? rows : undefined;
      });
    },
    // one preset as its STORE / STORE6 frame (DUMP flag set), or null (an empty place: BAD SLOT)
    async dump(slot) {
      return ask(dumpRequest({ source: "mine", slot }), (f) => {
        const d = parseDump(f);
        if (d && d.slot === slot) return d.frame;
        const r = parseReply(f);
        return r && r.kind === "error" && r.command === 0x22 ? null : undefined;
      });
    },
    // every preset (DUMP slot 0), as many as expected; progress(n, total) after each
    async dumpAll(total, progress = () => {}) {
      const out = [];
      return ask(dumpRequest({ source: "mine", slot: 0 }), (f) => {
        const d = parseDump(f);
        if (!d || d.source !== "mine") return undefined;
        out.push(d.frame);
        progress(out.length, total);
        return out.length >= total ? out : undefined;
      }, () => out);
    },
  };
}
