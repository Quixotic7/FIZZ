// SPDX-License-Identifier: GPL-3.0-only
// Copied unchanged from FIZZ's tools/fizz_sysex.mjs (80f60da) so the published site is self-contained.
// FIZZ's SysEx (docs/SYSEX.md), the host side: the frames a page (or a tool) sends FIZZ, and its replies read.
// Pure: no DOM, no Node APIs. The FIZZ web preview hands the bytes to the engine's midiIn (as USB-MIDI packets:
// usbPackets below); tools/test_sysex_dnsnd.mjs uses it with ChoralRoot's .dnsnd reader.
//
//   identity()                                   -> Uint8Array: F0 7D 46 5A 01 01 7F F7
//   storeSound({name, blob, drive, pool, slot, batch, track, pan, chorus, delay, reverb, tags})
//                                                -> Uint8Array: one STORE SOUND frame (P1.5x: pan and the sends kept
//                                                   with the preset, LOAD or not; tags: FIZZ's tags, a mask or names).
//                                                   E1: a 128-byte (version-6) blob makes it a STORE6 (0x11)
//   fizzTagMask(tags)                            FIZZ's tags as the STORE's mask: a number, or names of FIZZ_TAGS
//   parseReply(bytes)                            -> {kind: "identity", protocol, version} | {kind: "store", status,
//                                                   statusName, pool, slot} | {kind: "error", status, statusName,
//                                                   command} | null (not FIZZ's, or a bad checksum)
//   labelsRequest({subscribe})                   -> P1.6l: LABELS (20): the controls' labels now; subscribe: FIZZ
//                                                   sends LABELS again by itself whenever they change (at most 20 a
//                                                   second), until a request without it
//   parseLabels(bytes)                           -> {count, list: [49 strings], knobs: {SELECT, ALGORITHM, PRESETS,
//                                                   K1..K4, MASTER}, buttons: {FX .. F2}, white: [16], black: [11]}
//                                                   | null (not a LABELS reply)
//   listRequest({factory, pool})                 -> P1.6m: LIST (21): MY PRESETS (and the factory's, the pool's): its
//                                                   replies (61) come one part after another
//   parseList(bytes)                             -> {part, last, total, entries: [{source, slot, id, name, tags,
//                                                   imported, inPool}]} | null
//   dumpRequest({source, slot})                  -> P1.6m: DUMP (22): one preset (source "mine" 1..256, "factory"
//                                                   1..23, "pool" 1..128), or slot 0: all of that source, paced; the
//                                                   reply is a STORE / STORE6 frame with the DUMP flag
//   parseDump(bytes)                             -> {source: "mine" | "pool", slot, name, drive, pan, chorus, delay,
//                                                   reverb, tags, blob (96 or 128 bytes), six, frame} | null
//   restoreFrame(bytes)                          -> the dump as a STORE / STORE6 to send back (the DUMP flag cleared:
//                                                   FIZZ stores it again; {slot} to put it elsewhere, 0 the first free)
//   usbPackets(frame)                            -> [u32]: the frame as USB-MIDI packets (cable 0, CIN 4 / 5 / 6 / 7)
//   pack7(bytes), unpack7(bytes)                 the 7-bit packing (groups of 7 -> 8, the first byte their top bits)

export const FIZZ_HEADER = [0x7d, 0x46, 0x5a];
export const FIZZ_PROTOCOL = 1;
export const CMD = { IDENTITY: 0x01, STORE: 0x10, STORE6: 0x11, LABELS: 0x20, LIST: 0x21, DUMP: 0x22 };   // (E1, P1.6l, P1.6m)
export const REPLY = { IDENTITY: 0x41, STORE: 0x50, LABELS: 0x60, LIST: 0x61, ERROR: 0x7f };
export const SOURCES = ["mine", "factory", "pool"];    // P1.6m: LIST / DUMP's sources
// P1.6l: the labels' order (docs/SYSEX.md): the knobs, the 14 buttons (the FM-1's panel labels), W1..W16, B1..B11
export const LABEL_KNOBS = ["SELECT", "ALGORITHM", "PRESETS", "K1", "K2", "K3", "K4", "MASTER"];
export const LABEL_BUTTONS = ["FX", "TRACKS", "MIX", "LATCH", "EDIT", "SHIFT", "HOME", "SOUNDS", "FILL", "PATTERNS", "PLAY",
  "REC", "F1", "F2"];
export const LABELS_N = LABEL_KNOBS.length + LABEL_BUTTONS.length + 16 + 11;   // 49
export const FLAGS = { POOL: 0x01, BATCH: 0x02, LOAD: 0x04, DUMP: 0x08 };   // (P1.6m: DUMP: a dump's reply, ignored as it is)
export const MIX = { PAN: 0x01, CHORUS: 0x02, DELAY: 0x04, REVERB: 0x08 };
export const STATUS = ["OK", "FULL", "BAD LENGTH", "BAD CHECKSUM", "BAD PATCH", "BAD SLOT", "BAD VERSION", "BAD COMMAND"];
export const NAME_LEN = 12, BLOB_LEN = 96, BLOB6_LEN = 128;
// P1.5x: FIZZ's tags (core/seq/sounds.h QD_PK_*), bit k: the STORE's optional tags byte
export const FIZZ_TAGS = ["BASS", "KEYS", "PLUCK", "PAD", "LEAD", "DRUMS", "FX"];
export const fizzTagMask = (tags) => typeof tags === "number" ? tags & 0x7f
  : Array.isArray(tags) ? tags.reduce((m, t) => m | (FIZZ_TAGS.includes(String(t).toUpperCase()) ? 1 << FIZZ_TAGS.indexOf(String(t).toUpperCase()) : 0), 0) : 0;

export function pack7(bytes) {
  const out = [];
  for (let i = 0; i < bytes.length; i += 7) {
    const g = bytes.slice(i, i + 7);
    let hi = 0;
    g.forEach((b, j) => { hi |= ((b >> 7) & 1) << j; });
    out.push(hi, ...Array.from(g, (b) => b & 0x7f));
  }
  return out;
}
export function unpack7(bytes) {
  const out = [];
  for (let i = 0; i < bytes.length; ) {
    const hi = bytes[i++];
    for (let j = 0; j < 7 && i < bytes.length; j++) out.push(bytes[i++] | (((hi >> j) & 1) << 7));
  }
  return Uint8Array.from(out);
}
const checksum = (bytes) => (0x80 - (bytes.reduce((s, b) => s + b, 0) & 0x7f)) & 0x7f;
function frame(cmd, body) {
  const b = [cmd, ...body];
  return Uint8Array.from([0xf0, ...FIZZ_HEADER, FIZZ_PROTOCOL, ...b, checksum(b), 0xf7]);
}

export const identity = () => frame(CMD.IDENTITY, []);

// a sound into FIZZ's MY PRESETS (E1: a 128-byte blob, FM TONE version 6 with FIZZ's values 74.., goes as STORE6, the
// same body with the longer blob) (pool false; slot 1..256, 0 the first free) or the project's POOL (pool true; slot
// 1..128, 0 the first free: P1.5w, a MY preset referenced there). blob: the 96-byte FM TONE patch (ChoralRoot's blob:
// dnsndToQuad(parsed).blob, dn2pstToQuad(parsed).blob, or a sound file's patch); drive 0..127 (FIZZ's DRIVE). batch:
// joins the previous import's undo level. pan (-64..63), chorus, delay, reverb (0..127): whichever are given go with
// the preset (P1.5x; the rest: none, pan centre), and with track 0..7 (LOAD) onto that track of the pattern playing
// too. tags (P1.5x): FIZZ's tags for the preset (a mask, or names of FIZZ_TAGS); none or 0: FIZZ's classifier decides.
export function storeSound({ name = "", blob, drive = 0, pool = false, slot = 0, batch = false, track = -1, pan, chorus,
                             delay, reverb, tags } = {}) {
  if (!(blob instanceof Uint8Array) || (blob.length !== BLOB_LEN && blob.length !== BLOB6_LEN))
    throw new Error("blob: a 96-byte FM TONE patch (version 5), or a 128-byte one (version 6: STORE6)");
  if (!(slot >= 0 && slot <= (pool ? 128 : 256))) throw new Error(`slot: 0 (the first free) .. ${pool ? 128 : 256}`);
  const nm = Array.from({ length: NAME_LEN }, (_, i) => {
    const c = i < name.length ? name.charCodeAt(i) : 0x20;
    return c >= 0x20 && c <= 0x7e ? c : 0x3f;
  });
  const load = track >= 0 && track <= 7;
  const mask = (pan !== undefined ? MIX.PAN : 0) | (chorus !== undefined ? MIX.CHORUS : 0) |
               (delay !== undefined ? MIX.DELAY : 0) | (reverb !== undefined ? MIX.REVERB : 0);   // (P1.5x: LOAD or not)
  const tm = fizzTagMask(tags);
  const c7 = (v) => Math.max(0, Math.min(127, Math.round(v || 0)));
  const flags = (pool ? FLAGS.POOL : 0) | (batch ? FLAGS.BATCH : 0) | (load ? FLAGS.LOAD : 0);
  return frame(blob.length === BLOB6_LEN ? CMD.STORE6 : CMD.STORE, [flags, (slot >> 7) & 0x7f, slot & 0x7f, ...nm, c7(drive), load ? track : 0,
                           c7((pan === undefined ? 0 : pan) + 64), c7(chorus), c7(delay), c7(reverb), mask,
                           ...pack7(blob), ...(tm ? [tm] : [])]);
}

export const labelsRequest = ({ subscribe = false } = {}) => frame(CMD.LABELS, [subscribe ? 1 : 0]);

export function parseLabels(bytes) {
  const b = Array.from(bytes);
  if (b.length < 10 || b[0] !== 0xf0 || b[b.length - 1] !== 0xf7 || b[1] !== 0x7d || b[2] !== 0x46 || b[3] !== 0x5a ||
      b[5] !== REPLY.LABELS) return null;
  if (b.slice(5, -1).reduce((s, x) => s + x, 0) & 0x7f) return null;
  const count = (b[6] << 7) | b[7], n = b[8], list = [];
  let o = 9;
  for (let i = 0; i < n && o < b.length - 2; i++) {
    const len = b[o++];
    list.push(String.fromCharCode(...b.slice(o, o + len)));
    o += len;
  }
  if (list.length !== n) return null;
  const at = (k) => list[k] || "";
  const nk = LABEL_KNOBS.length, nb = LABEL_BUTTONS.length;
  return { count, list,
    knobs: Object.fromEntries(LABEL_KNOBS.map((k, i) => [k, at(i)])),
    buttons: Object.fromEntries(LABEL_BUTTONS.map((k, i) => [k, at(nk + i)])),
    white: Array.from({ length: 16 }, (_, i) => at(nk + nb + i)),
    black: Array.from({ length: 11 }, (_, i) => at(nk + nb + 16 + i)) };
}

// ---- P1.6m: LIST, DUMP
export const listRequest = ({ factory = false, pool = false } = {}) => frame(CMD.LIST, [(factory ? 1 : 0) | (pool ? 2 : 0)]);
export const dumpRequest = ({ source = "mine", slot = 0 } = {}) => {
  const src = SOURCES.indexOf(source);
  if (src < 0) throw new Error(`source: one of ${SOURCES.join(", ")}`);
  return frame(CMD.DUMP, [src, (slot >> 7) & 0x7f, slot & 0x7f]);
};
const sumOk = (b) => !(b.slice(5, -1).reduce((s, x) => s + x, 0) & 0x7f);
const isFizz = (b) => b.length >= 8 && b[0] === 0xf0 && b[b.length - 1] === 0xf7 && b[1] === 0x7d && b[2] === 0x46 && b[3] === 0x5a;
export function parseList(bytes) {
  const b = Array.from(bytes);
  if (!isFizz(b) || b[5] !== REPLY.LIST || !sumOk(b)) return null;
  const entries = [];
  for (let e = 0, o = 11; e < b[10] && o + 20 <= b.length - 2; e++, o += 20)
    entries.push({ source: SOURCES[b[o]] || String(b[o]), slot: (b[o + 1] << 7) | b[o + 2],
      id: (b[o + 3] << 14) | (b[o + 4] << 7) | b[o + 5], name: String.fromCharCode(...b.slice(o + 6, o + 18)).trimEnd(),
      tags: b[o + 18], imported: !!(b[o + 19] & 1), inPool: !!(b[o + 19] & 2) });
  return { part: b[6], last: !!b[7], total: (b[8] << 7) | b[9], entries };
}
export function parseDump(bytes) {
  const b = Array.from(bytes);
  if (!isFizz(b) || (b[5] !== CMD.STORE && b[5] !== CMD.STORE6) || !sumOk(b) || !(b[6] & FLAGS.DUMP)) return null;
  const six = b[5] === CMD.STORE6, n7 = six ? 147 : 110, body = b.slice(6, -2);
  const blob = unpack7(body.slice(22, 22 + n7));
  return { source: body[0] & FLAGS.POOL ? "pool" : "mine", slot: (body[1] << 7) | body[2],
    name: String.fromCharCode(...body.slice(3, 15)).trimEnd(), drive: body[15], pan: body[17] - 64, chorus: body[18],
    delay: body[19], reverb: body[20], tags: body.length > 22 + n7 ? body[22 + n7] : 0, blob, six, frame: Uint8Array.from(b) };
}
export function restoreFrame(bytes, { slot } = {}) {
  const b = Array.from(bytes);
  b[6] &= ~FLAGS.DUMP & 0x7f;
  if (slot !== undefined) { b[7] = (slot >> 7) & 0x7f; b[8] = slot & 0x7f; }
  b[b.length - 2] = (0x80 - (b.slice(5, -2).reduce((s, x) => s + x, 0) & 0x7f)) & 0x7f;
  return Uint8Array.from(b);
}

export function parseReply(bytes) {
  const b = Array.from(bytes);
  if (b.length < 8 || b[0] !== 0xf0 || b[b.length - 1] !== 0xf7 || b[1] !== 0x7d || b[2] !== 0x46 || b[3] !== 0x5a)
    return null;
  const body = b.slice(5, -1);
  if (body.reduce((s, x) => s + x, 0) & 0x7f) return null;
  const [kind, ...rest] = body.slice(0, -1);
  if (kind === REPLY.IDENTITY) return { kind: "identity", protocol: rest[0], version: String.fromCharCode(...rest.slice(1)) };
  if (kind === REPLY.STORE)
    return { kind: "store", status: rest[0], statusName: STATUS[rest[0]], pool: rest[1] === 1, slot: (rest[2] << 7) | rest[3] };
  if (kind === REPLY.ERROR) return { kind: "error", status: rest[0], statusName: STATUS[rest[0]], command: rest[1] };
  if (kind === REPLY.LABELS) return { kind: "labels", ...parseLabels(bytes) };
  if (kind === REPLY.LIST) return { kind: "list", ...parseList(bytes) };
  if ((kind === CMD.STORE || kind === CMD.STORE6) && (rest[0] & FLAGS.DUMP)) return { kind: "dump", ...parseDump(bytes) };
  return null;
}

// a SysEx frame as USB-MIDI event packets (cable 0): CIN 4 three bytes that go on, CIN 5 / 6 / 7 the last one, two,
// three bytes ending at F7; each packet a u32: CIN | b0 << 8 | b1 << 16 | b2 << 24 (FM1VST's fm1core midi_in)
export function usbPackets(f) {
  const out = [];
  for (let i = 0; i < f.length; i += 3) {
    const k = Math.min(3, f.length - i), end = i + k >= f.length && f[f.length - 1] === 0xf7;
    const cin = end ? 4 + k : 4;
    out.push((cin | (f[i] << 8) | ((k > 1 ? f[i + 1] : 0) << 16) | ((k > 2 ? f[i + 2] : 0) << 24)) >>> 0);
  }
  return out;
}
