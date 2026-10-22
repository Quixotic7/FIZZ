// SPDX-License-Identifier: GPL-3.0-only
// Copied unchanged from FIZZ's tools/fizz_sysex.mjs (b0d24d4) so the published site is self-contained.
// FIZZ's SysEx (docs/SYSEX.md), the host side: the frames a page (or a tool) sends FIZZ, and its replies read.
// Pure: no DOM, no Node APIs. The FIZZ web preview hands the bytes to the engine's midiIn (as USB-MIDI packets:
// usbPackets below); tools/test_sysex_dnsnd.mjs uses it with ChoralRoot's .dnsnd reader.
//
//   identity()                                   -> Uint8Array: F0 7D 46 5A 01 01 7F F7
//   storeSound({name, blob, drive, pool, slot, batch, track, pan, chorus, delay, reverb, tags})
//                                                -> Uint8Array: one STORE SOUND frame (P1.5x: pan and the sends kept
//                                                   with the preset, LOAD or not; tags: FIZZ's tags, a mask or names)
//   fizzTagMask(tags)                            FIZZ's tags as the STORE's mask: a number, or names of FIZZ_TAGS
//   parseReply(bytes)                            -> {kind: "identity", protocol, version} | {kind: "store", status,
//                                                   statusName, pool, slot} | {kind: "error", status, statusName,
//                                                   command} | null (not FIZZ's, or a bad checksum)
//   usbPackets(frame)                            -> [u32]: the frame as USB-MIDI packets (cable 0, CIN 4 / 5 / 6 / 7)
//   pack7(bytes), unpack7(bytes)                 the 7-bit packing (groups of 7 -> 8, the first byte their top bits)

export const FIZZ_HEADER = [0x7d, 0x46, 0x5a];
export const FIZZ_PROTOCOL = 1;
export const CMD = { IDENTITY: 0x01, STORE: 0x10 };
export const REPLY = { IDENTITY: 0x41, STORE: 0x50, ERROR: 0x7f };
export const FLAGS = { POOL: 0x01, BATCH: 0x02, LOAD: 0x04 };
export const MIX = { PAN: 0x01, CHORUS: 0x02, DELAY: 0x04, REVERB: 0x08 };
export const STATUS = ["OK", "FULL", "BAD LENGTH", "BAD CHECKSUM", "BAD PATCH", "BAD SLOT", "BAD VERSION", "BAD COMMAND"];
export const NAME_LEN = 12, BLOB_LEN = 96;
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

// a sound into FIZZ's MY PRESETS (pool false; slot 1..256, 0 the first free) or the project's POOL (pool true; slot
// 1..128, 0 the first free: P1.5w, a MY preset referenced there). blob: the 96-byte FM TONE patch (ChoralRoot's blob:
// dnsndToQuad(parsed).blob, dn2pstToQuad(parsed).blob, or a sound file's patch); drive 0..127 (FIZZ's DRIVE). batch:
// joins the previous import's undo level. pan (-64..63), chorus, delay, reverb (0..127): whichever are given go with
// the preset (P1.5x; the rest: none, pan centre), and with track 0..7 (LOAD) onto that track of the pattern playing
// too. tags (P1.5x): FIZZ's tags for the preset (a mask, or names of FIZZ_TAGS); none or 0: FIZZ's classifier decides.
export function storeSound({ name = "", blob, drive = 0, pool = false, slot = 0, batch = false, track = -1, pan, chorus,
                             delay, reverb, tags } = {}) {
  if (!(blob instanceof Uint8Array) || blob.length !== BLOB_LEN) throw new Error("blob: a 96-byte FM TONE patch");
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
  return frame(CMD.STORE, [flags, (slot >> 7) & 0x7f, slot & 0x7f, ...nm, c7(drive), load ? track : 0,
                           c7((pan === undefined ? 0 : pan) + 64), c7(chorus), c7(delay), c7(reverb), mask,
                           ...pack7(blob), ...(tm ? [tm] : [])]);
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
