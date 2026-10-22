// SPDX-License-Identifier: GPL-3.0-only
// The Digitone II's SysEx, read-only parts: the dump requests and their answers, the Elektron API's Device and
// Version requests, the universal identity request. Pure: bytes in, bytes out.
//
// Dump framing (DNX packages/core/src/sysex/container.ts, device/dumprequest.ts; verified there on a Digitone II
// 2026-07-28): F0 00 20 3C <product 0x15> <dev 0> <type> <ver 1 1> <objNr> <8-in-7 payload> <chk 14-bit>
// <len 14-bit> F7. A request is the answer's type + 0x10 with an empty payload: 0x60 pattern + kit (99,840 bytes),
// 0x62 kit (10,752), 0x63 a pool sound (359). They read; they cannot write (their body is empty).
// API framing (DNX device/api.ts): F0 00 20 3C 10 00 <8-in-7: msgId u16, respId u16, code> F7; Device 0x01,
// Version 0x02, the answer's code + 0x80.
export const DN2_PRODUCT = 0x15;
export const DUMP = { PATTERN_KIT: 0x60, KIT: 0x62, SOUND: 0x63 };
export const ANSWER = { 0x60: 0x50, 0x62: 0x52, 0x63: 0x53 };
export const SIZE = { 0x50: 99840, 0x52: 10752, 0x53: 359 };
export const PATTERN_BYTES = 89088;                      // a pattern + kit answer: the pattern, then the kit
export const KIT_SOUND = (t) => 60 + 359 * t;            // track t's sound record in a kit (syxgrid PRESET_NAME_BASE)
export const RECORD = 359;

export function encode87(raw) {
  const out = [];
  for (let s = 0; s < raw.length; s += 7) {
    const n = Math.min(7, raw.length - s);
    let msb = 0;
    const g = [];
    for (let i = 0; i < n; i++) { msb |= (raw[s + i] >> 7) << (6 - i); g.push(raw[s + i] & 0x7f); }
    out.push(msb, ...g);
  }
  return Uint8Array.from(out);
}
export function decode87(enc) {
  const out = [];
  for (let s = 0; s < enc.length; s += 8) {
    const n = Math.min(8, enc.length - s) - 1, msb = enc[s];
    for (let i = 0; i < n; i++) out.push(enc[s + 1 + i] | (((msb >> (6 - i)) & 1) << 7));
  }
  return Uint8Array.from(out);
}
const sum14 = (b) => { let s = 0; for (const x of b) s += x; return s & 0x3fff; };

// a dump message (request: an empty payload)
export function dumpMessage(type, objNr = 0, payload = new Uint8Array(0), product = DN2_PRODUCT) {
  const enc = encode87(payload), total = 10 + enc.length + 5, out = new Uint8Array(total);
  out.set([0xf0, 0x00, 0x20, 0x3c, product, 0x00, type, 0x01, 0x01, objNr & 0x7f], 0);
  out.set(enc, 10);
  const c = sum14(enc), l = (total - 10) & 0x3fff;
  out.set([c >> 7, c & 0x7f, l >> 7, l & 0x7f, 0xf7], total - 5);
  return out;
}
export const dumpRequest = (type, objNr) => dumpMessage(type, objNr);

// an answer -> {type, objNr, payload, ok, why}
export function parseDump(m) {
  if (!m || m.length < 15 || m[0] !== 0xf0 || m[1] !== 0x00 || m[2] !== 0x20 || m[3] !== 0x3c || m[4] === 0x10) return null;
  const enc = m.subarray(10, m.length - 5), payload = decode87(enc);
  const chk = (m[m.length - 5] << 7) | m[m.length - 4], len = (m[m.length - 3] << 7) | m[m.length - 2];
  const ok = chk === sum14(enc) && len === ((m.length - 10) & 0x3fff) && m[m.length - 1] === 0xf7;
  return { product: m[4], type: m[6], objNr: m[9], payload, ok, why: ok ? "" : "checksum or length" };
}

// track t's record out of an answer (null: not there or not a sound record)
export function trackRecord(dump, t) {
  if (!dump) return null;
  const base = dump.type === 0x50 ? PATTERN_BYTES : dump.type === 0x52 ? 0 : -1;
  if (base < 0) return null;
  const o = base + KIT_SOUND(t), r = dump.payload.subarray(o, o + RECORD);
  return r.length === RECORD && r[0] === 0xbe && r[1] === 0xef && r[2] === 0xba && r[3] === 0xce ? Uint8Array.from(r) : null;
}

// ---- the API (Device, Version)
export function apiMessage(msgId, code) {
  const enc = encode87(Uint8Array.from([msgId >> 8, msgId & 255, 0, 0, code]));
  return Uint8Array.from([0xf0, 0x00, 0x20, 0x3c, 0x10, 0x00, ...enc, 0xf7]);
}
export function parseApi(m) {
  if (!m || m.length < 8 || m[0] !== 0xf0 || m[1] !== 0x00 || m[2] !== 0x20 || m[3] !== 0x3c || m[4] !== 0x10) return null;
  const p = decode87(m.subarray(6, m[m.length - 1] === 0xf7 ? m.length - 1 : m.length));
  if (p.length < 5) return null;
  return { msgId: (p[0] << 8) | p[1], respId: (p[2] << 8) | p[3], code: p[4], body: p.subarray(5) };
}
const str0 = (b, at) => { let s = ""; while (at < b.length && b[at]) s += String.fromCharCode(b[at++]); return [s, at + 1]; };
export function readDevice(body) {          // u8 product, u8 n + n message codes, string
  const n = body[1] || 0, [name] = str0(body, 2 + n);
  return { productId: body[0], supported: Array.from(body.subarray(2, 2 + n)), name };
}
export function readVersion(body) {
  const [build, at] = str0(body, 0), [version] = str0(body, at);
  return { build, version };
}
export const IDENTITY_REQUEST = Uint8Array.from([0xf0, 0x7e, 0x7f, 0x06, 0x01, 0xf7]);
export const isIdentityReply = (m) => m && m.length > 5 && m[0] === 0xf0 && m[1] === 0x7e && m[3] === 0x06 && m[4] === 0x02;

// ---- what the wizard may send (the safety check the page and the test both use)
export function allowed(m, ch) {
  if (!m || !m.length) return false;
  const s = m[0];
  if (s === 0xf0) {
    if (m.length === 6 && IDENTITY_REQUEST.every((b, i) => b === m[i])) return true;
    const a = parseApi(m);
    if (a) return a.respId === 0 && (a.code === 0x01 || a.code === 0x02) && a.body.length === 0;
    const d = parseDump(m);
    return !!d && d.ok && d.product === DN2_PRODUCT && d.payload.length === 0 && [0x60, 0x62, 0x63].includes(d.type);
  }
  const st = s & 0xf0;
  if ((s & 0x0f) !== ch) return false;
  return st === 0x80 || st === 0x90 || st === 0xb0;      // notes and controllers (CC, NRPN, all notes off)
}
