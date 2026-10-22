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
//   await readDnsnd(bytes, fileName) -> { file, name, blob, pan, drive, sends: {chorus, delay, reverb}, notes, origin,
//                                         mark, tags }
//     notes: what did not carry over or was approximated (dnsndToQuad's, and any reader warning not known below);
//     origin: the reader's notes about the file itself (an older payload header, the Digitone OS, the product), which
//     lose nothing; mark: those in one line ("older Digitone file (OS 1.42)"), or "" 
//   await readDn2(bytes, fileName) -> the same, for a Digitone II .dn2pst / .dn2snd (dn2: true)
//   await readFiles([{ name, bytes }]) -> [{ file, ok: true, sound } | { file, ok: false, error }] (by the file's extension)
//   createSender({ sendMidi, onMidiOut }) -> { identify(), send(sounds, { pool, track }, progress) }
import { parseDnsnd, dnsndToQuad } from "./fm1dnsnd.js";
import { parseDn2pst, dn2pstToQuad } from "./dn2pst.js";
import { identity, storeSound, parseReply, NAME_LEN, FIZZ_TAGS, fizzTagMask } from "./fizz_sysex.js";

// ---- tags: the Digitone's (DN1 manifests' MetaInfo.Tags; DN2 the record's tag bits, dn2pst.js DN2_TAGS: the same 32
// words) onto FIZZ's 7 (FIZZ_TAGS). A judgement, as the owner asked; the character tags (SOFT, HARD, DARK, BRIGHT,
// VINTAGE, EPIC, NOISY, METALLIC, ACOUSTIC, EXPRESSIVE, DEEP, LOOP, FAIL, MINE, FAVOURITE) map to nothing. A sound with
// no FIZZ tag sends 0: FIZZ's classifier decides from the sound.
export const TAG_MAP = {
  BASS: "BASS",
  PAD: "PAD", ATMOSPHERIC: "PAD", TEXTURE: "PAD", EVOLVING: "PAD", STRINGS: "PAD",
  LEAD: "LEAD", BRASS: "LEAD",
  CHORD: "KEYS", KEYS: "KEYS", PIANO: "KEYS", EP: "KEYS", ORGAN: "KEYS",
  PLUCK: "PLUCK", BELL: "PLUCK", MALLET: "PLUCK", ARPEGGIO: "PLUCK",
  KICK: "DRUMS", SNARE: "DRUMS", "HI-HAT": "DRUMS", HAT: "DRUMS", CYMBAL: "DRUMS", PERCUSSION: "DRUMS", PERC: "DRUMS", DRUM: "DRUMS",
  "SOUND FX": "FX", SFX: "FX", FX: "FX", NOISE: "FX", GLITCH: "FX",
};
// the Digitone's tags -> FIZZ's, in FIZZ_TAGS order
export const fizzTagsOf = (tags) => FIZZ_TAGS.filter((k) => (tags || []).some((t) => TAG_MAP[String(t).toUpperCase()] === k));

// ChoralRoot's reader says the sends are lost ("reverb send 39.45 not imported: effects are global on the FM-1",
// fm1dnsnd.js dnLost); FIZZ keeps them with the preset (P1.5x): those notes are dropped
const SEND_LOST = /^(reverb|delay|chorus) send -?[\d.]+ not imported: effects are global on the FM-1$/;

export const NAME_MAX = NAME_LEN;          // FIZZ's (and ChoralRoot's SND_NAME): 12

// the name as ChoralRoot's syxName makes it: printable ASCII, trimmed; empty -> "DIGITONE"
export function soundName(raw, fallback = "DIGITONE") {
  const s = Array.from(String(raw || ""), (c) => { const n = c.charCodeAt(0); return n < 32 || n > 126 ? " " : c; }).join("").trim().slice(0, NAME_MAX).trimEnd();
  return s || fallback;
}

// the reader's container / version warnings (fm1dnsnd.js parseDnsnd, by their known starts): nothing is lost by them
const ORIGIN = [/^the payload header is not the 0097 kind/, /^made on Digitone firmware /, /^product /];
export const isOrigin = (w) => ORIGIN.some((re) => re.test(w));
export function originMark(origin) {
  if (!origin.length) return "";
  const os = origin.map((w) => (w.match(/^made on Digitone firmware (\S+):/) || [])[1]).find(Boolean);
  const prod = origin.map((w) => (w.match(/^product (.*): read as a Digitone$/) || [])[1]).find(Boolean);
  const older = origin.some((w) => /^the payload header/.test(w));
  const bits = [os ? `OS ${os}` : "", prod ? `product ${prod}` : ""].filter(Boolean).join(", ");
  return `${older ? "older Digitone file" : "Digitone file"}${bits ? ` (${bits})` : ""}`;
}

export async function readDnsnd(bytes, file = "") {
  const parsed = await parseDnsnd(bytes instanceof Uint8Array ? bytes : new Uint8Array(bytes));
  const q = dnsndToQuad(parsed), p = parsed.params || {};
  const send = (k) => (typeof p[k] === "number" ? Math.max(0, Math.min(127, Math.round(p[k]))) : undefined);
  const origin = parsed.warnings.filter(isOrigin);
  return { file, name: soundName(parsed.name), blob: q.blob, pan: q.pan, drive: q.drive,
           sends: { chorus: send("send.chorus"), delay: send("send.delay"), reverb: send("send.reverb") },
           notes: [...parsed.warnings.filter((w) => !isOrigin(w)), ...q.notes.filter((n) => !SEND_LOST.test(n))], origin, mark: originMark(origin),
           tags: parsed.tags || [], fizzTags: fizzTagsOf(parsed.tags) };
}

// a Digitone II .dn2pst (or .dn2snd, the same stored container without the ZIP: research/DN2-FORMAT.md §10), read
// with FIZZ's tools/dn2pst.mjs (js/dn2pst.js). A sound that is not FM TONE (WAVETONE, FM DRUM, SWARMER, MIDI) throws
// the reader's message: not importable. The reader's notes about the file itself (dn2pstToQuad prefixes them "file: ")
// are the quiet mark; the rest are what was lost or approximated.
export async function readDn2(bytes, file = "") {
  const parsed = await parseDn2pst(bytes instanceof Uint8Array ? bytes : new Uint8Array(bytes), { anyMachine: true });
  const q = dn2pstToQuad(parsed);
  const origin = q.notes.filter((n) => n.startsWith("file: ")).map((n) => n.slice(6));
  const firmware = parsed.firmware ? `OS ${parsed.firmware}` : "";
  const mark = origin.length || firmware
    ? `Digitone II file${firmware ? ` (${firmware})` : ""}${origin.length ? ` · ${origin.length} note${origin.length === 1 ? "" : "s"} on the file` : ""}` : "";
  return { file, dn2: true, name: soundName(q.name, "DIGITONE II"), blob: q.blob, pan: q.pan, drive: q.drive,
           sends: { chorus: q.sends.chorus, delay: q.sends.delay, reverb: q.sends.reverb },
           notes: q.notes.filter((n) => !n.startsWith("file: ") && !SEND_LOST.test(n)), origin, mark, tags: parsed.tags || [],
           fizzTags: fizzTagsOf(parsed.tags) };
}
export const isDn2Name = (name) => /\.dn2(pst|snd)$/i.test(name || "");
export const IMPORTABLE = /\.(dnsnd|dn2pst|dn2snd)$/i;

export async function readFiles(files) {
  const out = [];
  for (const f of files) {
    try { out.push({ file: f.name, ok: true, sound: await (isDn2Name(f.name) ? readDn2(f.bytes, f.name) : readDnsnd(f.bytes, f.name)) }); }
    catch (e) { out.push({ file: f.name, ok: false, error: e.message || String(e) }); }
  }
  return out;
}

// the notes of a sound loaded onto a track with its mix: the sends are no longer lost (the track takes them)
export const notesWithLoad = (sound) => sound.notes.filter((n) => !SEND_LOST.test(n));

// the STORE SOUND frame of one sound: into the LIBRARY (pool false) or the project's POOL, the first free slot; batch:
// one undo level with the previous message; track 0..7: also loaded onto it, with its pan and sends
// MIX_WITH_EVERY_STORE: FIZZ's presets keep their pan and sends (P1.5x): every STORE carries them, LOAD or not
export const MIX_WITH_EVERY_STORE = true;
export const mixOf = (sound) => ({ pan: sound.pan, chorus: sound.sends.chorus, delay: sound.sends.delay, reverb: sound.sends.reverb });
export function storeFrame(sound, { pool = false, batch = false, track = -1 } = {}) {
  const load = track >= 0 && track <= 7;
  return storeSound({ name: sound.name, blob: sound.blob, drive: sound.drive, pool, slot: 0, batch,
    ...(load ? { track } : {}), ...(load || MIX_WITH_EVERY_STORE ? mixOf(sound) : {}),
    tags: fizzTagMask(sound.fizzTags || []) });
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
          : { ok: false, full: r.status === 1, error: r.status === 1 ? (pool ? "the pool is full" : "MY PRESETS is full") : (r.statusName || `error ${r.status}`).toLowerCase() };
        results.push(res);
        progress(i, res);
        if (res.full) break;
      }
      return results;
    },
  };
}
