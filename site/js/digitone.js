// SPDX-License-Identifier: GPL-3.0-only
// Digitone sounds into FIZZ: Elektron Digitone .dnsnd files read and converted to FM TONE in this browser (nothing is
// uploaded), then handed to FIZZ through its sound SysEx (FIZZ docs/SYSEX.md) into the engine's MIDI in.
//
// The reader is ChoralRoot FM-1's own (js/fm1dnsnd.js, copied unchanged from ChoralRootFM1 web/fm1dnsnd.js, GPL-3.0-only,
// the same owner): parseDnsnd() reads the container, dnsndToQuad() makes FM TONE's 96-byte patch (version 5) and lists
// what was lost or approximated. ChoralRoot's soundFromDnsnd() (web/fm1sounds.js) wraps that patch into a ChoralRoot
// slot record (its template, PAN and DRIVE in the record); FIZZ takes the patch, the name, PAN and DRIVE directly, so
// only the reader is copied here and the record step is done below as soundFromDnsnd does it.
//
// No DOM: site/tools/test_site.mjs runs it in Node.
//   await readDnsnd(bytes, fileName) -> { file, name, blob: Uint8Array(96), pan, drive, notes: [..], tags }   (throws)
//   await readFiles([{ name, bytes }]) -> [{ file, ok: true, sound } | { file, ok: false, error }]
//   createSender({ sendMidi, onMidiOut }) -> the adapter: send(sounds, "library" | "pool") (see below)
import { parseDnsnd, dnsndToQuad } from "./fm1dnsnd.js";

export const NAME_MAX = 16;               // the Digitone's (FIZZ's own limit applies when the SysEx is defined)

// the name as ChoralRoot's syxName makes it: printable ASCII, trimmed; empty -> "DIGITONE"
export function soundName(raw, fallback = "DIGITONE") {
  const s = Array.from(String(raw || ""), (c) => { const n = c.charCodeAt(0); return n < 32 || n > 126 ? " " : c; }).join("").trim().slice(0, NAME_MAX).trimEnd();
  return s || fallback;
}

export async function readDnsnd(bytes, file = "") {
  const parsed = await parseDnsnd(bytes instanceof Uint8Array ? bytes : new Uint8Array(bytes));
  const q = dnsndToQuad(parsed);
  return { file, name: soundName(parsed.name), blob: q.blob, pan: q.pan, drive: q.drive,
           notes: [...parsed.warnings, ...q.notes], tags: parsed.tags || [] };
}

export async function readFiles(files) {
  const out = [];
  for (const f of files) {
    try { out.push({ file: f.name, ok: true, sound: await readDnsnd(f.bytes, f.name) }); }
    catch (e) { out.push({ file: f.name, ok: false, error: e.message || String(e) }); }
  }
  return out;
}

// ---- the hand-off to FIZZ (the adapter)
// FIZZ's sound SysEx (FIZZ docs/SYSEX.md, being defined): "store a sound" with its name and FM TONE blob into a
// library or pool slot, answered through the engine's MIDI out (OK and the slot, or an error such as full), and an
// identity query. Until it lands, encode() is a stub: send() resolves every sound as { ok: false, error } so the page
// can be built and tested now; wiring it is this function and decode() below, nothing else.
export const SYSEX_READY = false;
export function encode(sound, target) {        // -> Uint8Array (F0 .. F7), or null while the format is not defined
  void sound; void target;
  return null;
}
export function decode(bytes) {                // a reply from FIZZ's MIDI out -> { ok, slot, error } or null (not ours)
  void bytes;
  return null;
}

// sendMidi(bytes): into the engine's MIDI in; onMidiOut(cb): the engine's MIDI out, one call per message
export function createSender({ sendMidi, onMidiOut, timeoutMs = 2000 }) {
  let waiting = null;
  let sx = null;                                // a SysEx split over several midiOut chunks
  onMidiOut((bytes) => {
    for (const b of bytes) {                    // (one chunk may hold several messages: each SysEx whole to decode)
      if (b === 0xf0) sx = [b];
      else if (sx && b === 0xf7) {
        sx.push(b);
        const r = decode(Uint8Array.from(sx));
        sx = null;
        if (r && waiting) { const w = waiting; waiting = null; clearTimeout(w.t); w.resolve(r); }
      } else if (sx && b < 0x80) sx.push(b);
      else if (sx && b < 0xf8) sx = null;      // (cut by a status byte; real time bytes may sit inside)
    }
  });
  const one = (sound, target) => new Promise((resolve) => {
    const msg = encode(sound, target);
    if (!msg) return resolve({ ok: false, error: "FIZZ's sound import arrives with its next build" });
    waiting = { resolve, t: setTimeout(() => { waiting = null; resolve({ ok: false, error: "FIZZ did not answer" }); }, timeoutMs) };
    sendMidi(msg);
  });
  return {
    ready: SYSEX_READY,
    // one after another (each waits for FIZZ's answer); progress(i, result) after each
    async send(sounds, target = "library", progress = () => {}) {
      const results = [];
      for (let i = 0; i < sounds.length; i++) {
        const r = await one(sounds[i], target);
        results.push(r);
        progress(i, r);
        if (!r.ok && /full/i.test(r.error || "")) break;   // (the rest would not fit either)
      }
      return results;
    },
  };
}
