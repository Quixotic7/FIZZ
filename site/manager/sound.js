// SPDX-License-Identifier: GPL-3.0-only
// The sound editor's model (docs/MANAGER-P2-PLAN.md §1): a sound as FIZZ's 110 values (fizzvalues.js, generated from
// the core: names, ranges, texts), its name, tags, pan and sends; read from a DUMP / STORE frame or a .fizzsnd sound,
// packed back into FIZZ's version-6 blob and a STORE6 frame. No DOM: the tests run it in Node.
//   soundFromFrame(frame) -> sound | null       a DUMP reply (MY PRESETS, the factory's, the pool's, a track's)
//   soundFromLib(s)        -> sound              js/library.js's shape (a .fizzsnd, a backup's preset)
//   blob6(sound), storeFrameOf(sound, opts), libSound(sound) (js/library.js's shape: for EXPORT)
//   text(id, v), clampV(id, v), stepOf(id, fine), info(id)
import { VALUES, ALGOS, DEST_ORDER } from "./fizzvalues.js";
import { fizzValues, Q } from "../js/fizz2dn.js";
import { quadPack6 } from "../js/dn2pst.js";
import { parseDump, storeSound, unpack7, FIZZ_TAGS } from "../js/fizz_sysex.js";

export { VALUES, ALGOS, DEST_ORDER, Q };
export const NP = VALUES.length;                           // 115
export const lfo = (k, f) => 47 + 8 * k + f;             // f: 0 SPEED 1 MULT 2 FADE 3 DEST 4 WAVE 5 PHASE 6 TRIG 7 DEPTH
export const mod = (k, f) => 86 + 3 * k + f;             // f: 0 SOURCE 1 DEST 2 DEPTH
export const LF = { SPEED: 0, MULT: 1, FADE: 2, DEST: 3, WAVE: 4, PHASE: 5, TRIG: 6, DEPTH: 7 };

export const info = (id) => {
  const r = VALUES[id];
  return r && { id, name: r[0], min: r[1], max: r[2], def: r[3], rule: r[4], ex: r[4] === "list" ? null : r[5], list: r[4] === "list" ? r[5] : null };
};
export const clampV = (id, v) => { const r = VALUES[id]; return Math.max(r[1], Math.min(r[2], Math.round(+v || 0))); };
export function text(id, v) {
  const r = VALUES[id];
  if (!r) return String(v);
  if (r[4] === "list") return r[5][v - r[1]] ?? String(v);
  const ex = r[5][String(v)];
  if (ex !== undefined) return ex;
  const a = Math.abs(v), f2 = `${Math.floor(a / 100)}.${String(a % 100).padStart(2, "0")}`;
  return r[4] === "fix2" ? f2 : r[4] === "fix2s" ? `${v < 0 ? "-" : "+"}${f2}` : r[4] === "ofs" && v > 0 ? `+${v}` : String(v);
}
// a hi-res value (hundredths) steps by 1.00, SHIFT by 0.01; the rest by 1
export const isHi = (id) => VALUES[id][2] - VALUES[id][1] >= 1000;
export const stepOf = (id, fine = false) => (isHi(id) && !fine ? 100 : 1);
// a wheel tick / an arrow press: the two-decimal values (fix2, fix2s) step 0.01, accelerated by n quick ticks in a row
// (x1.6 a tick, at most 1.00 a tick); SHIFT exactly 1.00. Everything else: stepOf (SHIFT fine).
export const isHundredths = (id) => VALUES[id][4] === "fix2" || VALUES[id][4] === "fix2s";
export const tickStep = (id, shift = false, n = 0) =>
  isHundredths(id) ? (shift ? 100 : Math.min(100, Math.round(1.6 ** Math.max(0, n)))) : stepOf(id, shift);
// how far through its range (0..1), and back
export const frac = (id, v) => { const r = VALUES[id]; return (v - r[1]) / Math.max(1, r[2] - r[1]); };
export const fromFrac = (id, f) => { const r = VALUES[id]; return clampV(id, r[1] + f * (r[2] - r[1])); };
export const defaults = () => VALUES.map((r) => r[3]);

const tagMask = (tags) => (Array.isArray(tags) ? tags.reduce((m, t) => m | (FIZZ_TAGS.includes(t) ? 1 << FIZZ_TAGS.indexOf(t) : 0), 0) : tags & 0x7f);
export const tagNames = (mask) => FIZZ_TAGS.filter((_, k) => (mask >> k) & 1);

// the extras a track's DUMP carries after its tags byte (Manager P2-B): the preset id (3 x 7 bits), the pool sound it
// came from (2 x 7), flags (01 edited, 02 the selected track, 04 recording, 08 playing, 10 the unit's browser previews
// on it), its MIDI channel, the selected track
function extrasOf(b, at) {
  if (b.length < at + 8) return null;
  return { id: (b[at] << 14) | (b[at + 1] << 7) | b[at + 2], sound: (b[at + 3] << 7) | b[at + 4], edited: !!(b[at + 5] & 1),
    selected: !!(b[at + 5] & 2), rec: !!(b[at + 5] & 4), playing: !!(b[at + 5] & 8), browse: !!(b[at + 5] & 16), chan: b[at + 6],
    sel: b[at + 7] };
}

// a DUMP reply (or any STORE / STORE6 frame) -> the editor's sound
export function soundFromFrame(frame) {
  const d = parseDump(frame);
  if (!d) return null;
  const values = fizzValues({ blob: d.blob, drive: d.drive });
  const b = Array.from(frame), body = b.slice(6, -2), n7 = d.six ? 147 : 110;
  const track = body[0] & 0x04 ? body[16] : -1;
  return { name: d.name, values, pan: d.pan, sends: { chorus: d.chorus, delay: d.delay, reverb: d.reverb }, tags: d.tags,
    source: d.source, slot: d.slot, track, extras: extrasOf(body, 22 + n7 + 1) };
}
// js/library.js's shape (a .fizzsnd, a backup's preset) -> the editor's sound
export function soundFromLib(s) {
  return { name: s.name, values: fizzValues({ blob: s.blob, drive: s.drive }), pan: s.pan | 0, sends: { ...(s.sends || {}) },
    tags: tagMask(s.fizzTags || []), source: "file", slot: 0, track: -1, extras: null };
}
export const copySound = (s) => ({ ...s, values: s.values.slice(), sends: { ...s.sends }, extras: s.extras && { ...s.extras } });

export const blob6 = (s) => quadPack6(s.values.slice(0, 73), s.values[Q.DRIVE], s.values.slice(74));
// back to js/library.js's shape (EXPORT writes it)
export const libSound = (s, source = {}) => ({ fizz: true, name: s.name, blob: blob6(s), drive: s.values[Q.DRIVE], pan: s.pan,
  sends: { ...s.sends }, fizzTags: tagNames(s.tags), favourite: false, source, notes: [], origin: [], mark: "", tags: [] });
// a STORE6 frame of the sound: slot (MY PRESETS 1..256, 0 the first free), track (LOAD), its mix and tags
// P2-B: own (the owner's preset), audition (with track: onto the track only), extras (the track's preset id, pool
// sound, edited mark)
export function storeFrameOf(s, { slot = 0, track = -1, batch = false, own = false, audition = false, extras = null } = {}) {
  return storeSound({ name: s.name, blob: blob6(s), drive: s.values[Q.DRIVE], slot, batch, track, pan: s.pan,
    chorus: s.sends.chorus, delay: s.sends.delay, reverb: s.sends.reverb, tags: s.tags, own, audition, extras });
}
// the same sound? (the values, the name)
export const sameValues = (a, b) => a && b && a.values.length === b.values.length && a.values.every((v, i) => v === b.values[i]);

// FIZZ's version-6 blob back to the values (the tests' round trip)
export const valuesOfBlob = (blob, drive) => fizzValues({ blob, drive });
export { unpack7 };
