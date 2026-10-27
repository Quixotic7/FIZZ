// SPDX-License-Identifier: GPL-3.0-only
// FIZZ presets written as Elektron Digitone files (EXPERIMENTAL until a Digitone owner confirms they load): .dnsnd
// (Digitone) and .dn2pst (Digitone II), with FIZZ's tools/fizz2dn.mjs (js/fizz2dn.js). One preset: its file; several:
// a ZIP of the files. Each preset's notes (what the format cannot hold) go with it, for the page to show first.
// No DOM: the tests run it in Node.
//   digitoneExport(sounds, "dnsnd" | "dn2pst", date) -> { name, bytes, type, items: [{ name, file, notes }] }
import { fizzToDnsnd, fizzToDn2pst, dnName } from "./fizz2dn.js";
import { zipStored } from "./library.js";

export const KINDS = { dnsnd: { label: "Digitone", write: fizzToDnsnd }, dn2pst: { label: "Digitone II", write: fizzToDn2pst } };

// FIZZ MIDI (e): a MIDI track's sound ('Q' 6 ending in "MIDI", core/voice/quad/quad_voice.h QD_MSND_*) has no FM TONE
// to write: it is left out, with a note (manager/sound.js isMidiBlob)
const isMidi = (b) => !!b && b.length === 128 && b[0] === 0x51 && b[1] === 6 && b[124] === 0x4d && b[125] === 0x49 && b[126] === 0x44 && b[127] === 0x49;

export function digitoneExport(sounds, kind, date = new Date()) {
  const k = KINDS[kind];
  if (!k) throw new Error(`unknown Digitone format ${kind}`);
  const midi = sounds.filter((s) => isMidi(s.blob));
  sounds = sounds.filter((s) => !isMidi(s.blob));
  if (!sounds.length) throw new Error("a MIDI sound cannot become a Digitone sound");
  const used = new Map(), files = [];
  const items = sounds.map((s) => {
    const out = k.write({ blob: s.blob, name: s.name, tags: s.fizzTags || [], pan: s.pan | 0, sends: s.sends || {}, drive: s.drive | 0 }, { date });
    let base = (dnName(s.name) || "FIZZ").replace(/[\\/:*?"<>|]+/g, "_");
    const n = (used.get(base) || 0) + 1; used.set(base, n);
    if (n > 1) base += ` ${n}`;
    const file = `${base}.${kind}`;
    files.push({ name: file, bytes: out.bytes });
    return { name: s.name, file, notes: out.notes };
  }).concat(midi.map((s) => ({ name: s.name, file: `${s.name} (MIDI)`, notes: ["A MIDI sound: not written"] })));
  if (files.length === 1) return { name: files[0].name, bytes: files[0].bytes, type: "application/zip", items };
  return { name: `FIZZ presets ${date.toISOString().slice(0, 10)} (${k.label}).zip`, bytes: zipStored(files, date), type: "application/zip", items };
}
