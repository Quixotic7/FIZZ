// SPDX-License-Identifier: GPL-3.0-only
// Digitone sounds into FIZZ: Elektron Digitone .dnsnd files read and converted to FM TONE in this browser (nothing is
// uploaded), then handed to FIZZ through its SysEx (FIZZ docs/SYSEX.md: STORE SOUND) into the engine's MIDI in.
//
// The reader is ChoralRoot FM-1's own (js/fm1dnsnd.js, copied unchanged from ChoralRootFM1 web/fm1dnsnd.js, GPL-3.0-only,
// the same owner): parseDnsnd() reads the container, dnsndToQuad() makes FM TONE's 96-byte patch (version 5), its pan
// and drive, and lists what was lost or approximated. The name is made as ChoralRoot's soundFromDnsnd() makes it
// (fm1sounds.js syxName: printable ASCII, 12 letters); its record step is not needed, FIZZ takes the patch itself.
// The frames are FIZZ's tools/fizz_sysex.mjs (js/fizz_sysex.js, copied).
//
// No DOM: site/tools/test_site.mjs runs it in Node too.
//   await readDnsnd(bytes, fileName) -> { file, name, blob, pan, drive, sends: {chorus, delay, reverb}, notes, tags }
//   await readFiles([{ name, bytes }]) -> [{ file, ok: true, sound } | { file, ok: false, error }]
//   createSender({ sendMidi, onMidiOut }) -> { identify(), send(sounds, { pool, track }, progress) }
import { parseDnsnd, dnsndToQuad } from "./fm1dnsnd.js";
import { identity, storeSound, parseReply, NAME_LEN } from "./fizz_sysex.js";

export const NAME_MAX = NAME_LEN;          // FIZZ's (and ChoralRoot's SND_NAME): 12

// the name as ChoralRoot's syxName makes it: printable ASCII, trimmed; empty -> "DIGITONE"
export function soundName(raw, fallback = "DIGITONE") {
  const s = Array.from(String(raw || ""), (c) => { const n = c.charCodeAt(0); return n < 32 || n > 126 ? " " : c; }).join("").trim().slice(0, NAME_MAX).trimEnd();
  return s || fallback;
}

export async function readDnsnd(bytes, file = "") {
  const parsed = await parseDnsnd(bytes instanceof Uint8Array ? bytes : new Uint8Array(bytes));
  const q = dnsndToQuad(parsed), p = parsed.params || {};
  const send = (k) => (typeof p[k] === "number" ? Math.max(0, Math.min(127, Math.round(p[k]))) : undefined);
  return { file, name: soundName(parsed.name), blob: q.blob, pan: q.pan, drive: q.drive,
           sends: { chorus: send("send.chorus"), delay: send("send.delay"), reverb: send("send.reverb") },
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

// the notes of a sound loaded onto a track with its mix: the sends are no longer lost (the track takes them)
export const notesWithLoad = (sound) => sound.notes.filter((n) => !/send .* not imported/.test(n));

// the STORE SOUND frame of one sound: into the LIBRARY (pool false) or the project's POOL, the first free slot; batch:
// one undo level with the previous message; track 0..7: also loaded onto it, with its pan and sends
export function storeFrame(sound, { pool = false, batch = false, track = -1 } = {}) {
  const load = track >= 0 && track <= 7;
  return storeSound({ name: sound.name, blob: sound.blob, drive: sound.drive, pool, slot: 0, batch,
    ...(load ? { track, pan: sound.pan, chorus: sound.sends.chorus, delay: sound.sends.delay, reverb: sound.sends.reverb } : {}) });
}

// sendMidi(bytes): raw MIDI into the engine's MIDI in (engine.js makes the USB-MIDI packets the core takes);
// onMidiOut(cb): the engine's MIDI out as raw bytes (engine.js reads its packets back), any chunking
export function createSender({ sendMidi, onMidiOut, timeoutMs = 3000 }) {
  let waiting = null, sx = null;
  onMidiOut((bytes) => {
    for (const b of bytes) {                    // (one chunk may hold several messages: each SysEx whole)
      if (b === 0xf0) sx = [b];
      else if (sx && b === 0xf7) {
        sx.push(b);
        const r = parseReply(Uint8Array.from(sx));
        sx = null;
        if (r && waiting && waiting.want(r)) { const w = waiting; waiting = null; clearTimeout(w.t); w.resolve(r); }
      } else if (sx && b < 0x80) sx.push(b);
      else if (sx && b < 0xf8) sx = null;      // (cut by a status byte; real time bytes may sit inside)
    }
  });
  // one message, then its reply (FIZZ keeps at most 4 waiting: one at a time is always within that)
  const ask = (frame, want) => new Promise((resolve) => {
    waiting = { want, resolve, t: setTimeout(() => { waiting = null; resolve(null); }, timeoutMs) };
    sendMidi(frame);
  });
  return {
    ready: true,
    // -> "FIZZ 0.6.4-p1.5r", or null (no answer: a FIZZ before the SysEx, or not running)
    async identify() {
      const r = await ask(identity(), (x) => x.kind === "identity");
      return r ? r.version : null;
    },
    // one after another, each waiting for FIZZ's reply; the messages after the first are a BATCH (one undo level);
    // track: the first sound is also loaded onto it. progress(i, result) after each; result { ok, pool, slot } or
    // { ok: false, error, full }. A FULL stops the rest.
    async send(sounds, { pool = false, track = -1 } = {}, progress = () => {}) {
      const results = [];
      for (let i = 0; i < sounds.length; i++) {
        const r = await ask(storeFrame(sounds[i], { pool, batch: i > 0, track: i === 0 ? track : -1 }), (x) => x.kind === "store" || x.kind === "error");
        const res = !r ? { ok: false, error: "FIZZ did not answer" }
          : r.kind === "store" && r.status === 0 ? { ok: true, pool: r.pool, slot: r.slot, loaded: i === 0 && track >= 0 ? track : -1 }
          : { ok: false, full: r.status === 1, error: r.status === 1 ? (pool ? "the pool is full" : "the library is full") : (r.statusName || `error ${r.status}`).toLowerCase() };
        results.push(res);
        progress(i, res);
        if (res.full) break;
      }
      return results;
    },
  };
}
