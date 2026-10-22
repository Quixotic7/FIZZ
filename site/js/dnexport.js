// SPDX-License-Identifier: GPL-3.0-only
// FIZZ presets written as Elektron Digitone files (EXPERIMENTAL until a Digitone owner confirms they load): .dnsnd
// (Digitone) and .dn2pst (Digitone II), with FIZZ's tools/fizz2dn.mjs (js/fizz2dn.js). One preset: its file; several:
// a ZIP of the files. Each preset's notes (what the format cannot hold) go with it, for the page to show first.
// No DOM: the tests run it in Node.
//   digitoneExport(sounds, "dnsnd" | "dn2pst", date) -> { name, bytes, type, items: [{ name, file, notes }] }
import { fizzToDnsnd, fizzToDn2pst, dnName } from "./fizz2dn.js";
import { zipStored } from "./library.js";

export const KINDS = { dnsnd: { label: "Digitone", write: fizzToDnsnd }, dn2pst: { label: "Digitone II", write: fizzToDn2pst } };

export function digitoneExport(sounds, kind, date = new Date()) {
  const k = KINDS[kind];
  if (!k) throw new Error(`unknown Digitone format ${kind}`);
  const used = new Map(), files = [];
  const items = sounds.map((s) => {
    const out = k.write({ blob: s.blob, name: s.name, tags: s.fizzTags || [], pan: s.pan | 0, sends: s.sends || {}, drive: s.drive | 0 }, { date });
    let base = (dnName(s.name) || "FIZZ").replace(/[\\/:*?"<>|]+/g, "_");
    const n = (used.get(base) || 0) + 1; used.set(base, n);
    if (n > 1) base += ` ${n}`;
    const file = `${base}.${kind}`;
    files.push({ name: file, bytes: out.bytes });
    return { name: s.name, file, notes: out.notes };
  });
  if (files.length === 1) return { name: files[0].name, bytes: files[0].bytes, type: "application/zip", items };
  return { name: `FIZZ presets ${date.toISOString().slice(0, 10)} (${k.label}).zip`, bytes: zipStored(files, date), type: "application/zip", items };
}
