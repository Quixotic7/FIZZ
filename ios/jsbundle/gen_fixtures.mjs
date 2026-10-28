// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// FizzDataTests' fixtures: Node's own results (entry.js and the site modules imported directly, not the bundle) for
// synthetic inputs (synth.mjs: no owner data, no real presets), which the Swift tests ask JavaScriptCore for again.
//   node ios/jsbundle/gen_fixtures.mjs        (from the worktree root)
// Writes FizzKit/Tests/FizzDataTests/Resources/: fixtures.json (the inputs, the expected hashes and results as
// toPlain JSON), presets.fizzbak, lib.fizzlib, deflated.fizzlib (the same library, its members deflated). The full
// .fizzbak (300 KB) is not kept: the Swift test makes it in JavaScriptCore, checks its SHA-256 against Node's, then reads it.
import { createHash } from "node:crypto";
import { mkdirSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import * as E from "./entry.js";
import * as S from "./synth.mjs";

const here = dirname(fileURLToPath(import.meta.url));
const OUT = join(here, "../FizzKit/Tests/FizzDataTests/Resources");
new Function(S.UTC_DATES)();

const sha = (b) => createHash("sha256").update(b).digest("hex");
const hex = (b) => Buffer.from(b).toString("hex");
const b64 = (b) => Buffer.from(b).toString("base64");
const plain = (x) => JSON.stringify(E.toPlain(x));
const result = async (f) => { try { return plain(await f()); } catch (e) { return `throws: ${e.message}`; } };

const webTags = { [E.soundKey(S.SOUNDS[0])]: ["SHIMMER", "DARK"], [E.soundKey(S.SOUNDS[2])]: [], deadbeef: ["UNUSED"] };
const image = E.demoStore({ presets: S.SOUNDS, projects: S.PROJECTS });
const full = E.makeUnitFizzbak({ kind: "full", image, region: S.REGION, crc: null, frames: S.FRAMES, fizz: S.FIZZ, unit: S.UNIT, webTags, stats: S.STATS, date: S.DATE });
const presets = E.makeUnitFizzbak({ kind: "presets", image, region: S.REGION, frames: S.FRAMES, fizz: S.FIZZ, unit: S.UNIT, webTags, date: S.DATE });
const noDump = E.makeUnitFizzbak({ kind: "full", image, region: S.REGION, frames: null, fizz: "", unit: S.UNIT, webTags: {}, date: S.DATE });
const lib = E.makeFizzlib([...S.SOUNDS, S.V5], { name: "SYN", author: "test", fizz: S.FIZZ, date: S.DATE });
const deflated = S.deflatedZip(S.storedMembers(lib));

const r = S.rng(99);
const crcCases = [0, 1, 7, 64, 257, 4096].map((n) => { const b = Uint8Array.from({ length: n }, () => Math.floor(r() * 256)); return { hex: hex(b), crc: E.crc32(b) }; });

const fixtures = {
  utcDates: S.UTC_DATES,
  date: S.DATE.toISOString(),
  fizz: S.FIZZ, unit: S.UNIT, region: S.REGION, stats: S.STATS, webTags, projects: S.PROJECTS,
  sounds: S.SOUNDS.map((s) => ({ name: s.name, blob: b64(s.blob), drive: s.drive, pan: s.pan, sends: s.sends, fizzTags: s.fizzTags, source: s.source })),
  frames: S.FRAMES.map(hex),
  expect: {
    crc32: crcCases,
    backupFileName: { full: E.backupFileName(S.DATE, "full"), presets: E.backupFileName(S.DATE, "presets") },
    demoStore: { length: image.length, sha256: sha(image) },
    readRegion: plain(E.readRegion(image)),
    fullFizzbak: { length: full.length, sha256: sha(full) },
    presetsFizzbak: { length: presets.length, sha256: sha(presets) },
    noDumpFizzbak: { length: noDump.length, sha256: sha(noDump) },
    readBackup: {
      full: await result(() => E.readBackup(full, "a.fizzbak")),
      presets: await result(() => E.readBackup(presets, "presets.fizzbak")),
      fizzlib: await result(() => E.readBackup(lib, "lib.fizzlib")),
      deflated: await result(() => E.readBackup(deflated, "deflated.fizzlib")),
      bin: await result(() => E.readBackup(image, "store.bin")),
      notBackup: await result(() => E.readBackup(new Uint8Array(1000), "x.bin")),
    },
    soundFromStoreFrame: plain(S.FRAMES.map((f) => E.soundFromStoreFrame(f))),
    soundKeys: S.SOUNDS.map((s) => E.soundKey(s)),
  },
};

// ---- I5: the EDIT tab (sound.js): the table, texts across every value's range, the DUMPs as the editor's sounds,
// their blobs packed back, STORE6 frames (SAVE, a track's AUDITION) and a MIDI sound
{
  const SND = await import("../../site/manager/sound.js");
  const texts = [];
  SND.VALUES.forEach(([, lo, hi, def], id) => {
    const xs = new Set([lo, hi, def, lo + 1, hi - 1, Math.round((lo + hi) / 2), 0, 1, -1, 99, 100, 101, -100, -101, 1234, -1234, 127]);
    for (const x of xs) if (x >= lo && x <= hi) texts.push([id, x, SND.text(id, x)]);
  });
  const sounds = S.FRAMES.map((f) => SND.soundFromFrame(f));
  const midi = SND.newMidi();
  midi.slots[0] = { cc: 74, value: 100, touched: true };
  midi.slots[5] = { cc: 7, value: 0, touched: true };
  midi.slots[27] = { cc: 127, value: 127, touched: false };
  const midiSound = { name: "MIDI TEST", values: SND.defaults(), midi, pan: 3, sends: { chorus: 1, delay: 2, reverb: 3 }, tags: 5 };
  const midiFrame = SND.storeFrameOf(midiSound, { slot: 0, own: true });
  const asDump = (f) => { const b = Array.from(f); b[6] |= 0x08; b[b.length - 2] = (0x80 - (b.slice(5, -2).reduce((a, x) => a + x, 0) & 0x7f)) & 0x7f; return Uint8Array.from(b); };
  fixtures.edit = {
    table: plain(E.editTable()),
    texts,
    lib: plain(S.SOUNDS.map((s) => E.soundFromLib(s))),
    sounds: plain(sounds),
    blobs: sounds.map((s) => hex(SND.blob6(s))),
    save: sounds.map((s, k) => hex(SND.storeFrameOf(s, { slot: k, own: true }))),
    audition: sounds.map((s) => hex(SND.storeFrameOf(s, { track: 2, audition: true, extras: { id: 0x1234, sound: 0, edited: true } }))),
    midi: { sound: plain(midiSound), blob: hex(SND.blob6(midiSound)), frame: hex(midiFrame), dump: hex(asDump(midiFrame)), read: plain(SND.soundFromFrame(asDump(midiFrame))) },
  };
}

// ---- I4: IMPORT's reads (files EXPORT wrote from the synthetic sounds, and two that do not read), the duplicates,
// EXPORT's carry and files, the restore's plan and fit for a host-layout backup onto the Bluetooth package
{
  const kinds = ["fizz", "dnsnd", "dn2pst", "dn2snd"];
  const one = S.SOUNDS.slice(0, 1), files = [];
  const exportFile = {};
  for (const k of kinds) for (const [label, ss] of [["one", one], ["all", S.SOUNDS]]) {
    const f = E.exportFile(ss, k, S.DATE);
    exportFile[`${k}.${label}`] = { name: f.name, type: f.type, length: f.bytes.length, sha256: sha(f.bytes), notes: plain(f.notes), skipped: plain(f.skipped || null) };
    files.push({ name: f.name, bytes: f.bytes });
  }
  const bad = [{ name: "x.dnsnd", bytes: new Uint8Array(10) }, { name: "y.fizzbak", bytes: new Uint8Array(10) }];
  const read = await E.importRead([...files, ...bad]);
  const host = E.demoStore({ presets: S.SOUNDS, projects: S.PROJECTS, layout: "host" }), ble = 49 * 4096;
  const plan = E.restorePlan(host, ble);
  fixtures.expect.i4 = {
    exportFile,
    exportCarry: Object.fromEntries(kinds.flatMap((k) => [[`${k}.one`, plain(E.exportCarry(one, k))], [`${k}.all`, plain(E.exportCarry(S.SOUNDS, k))]])),
    importRead: plain(read),
    importDuplicates: plain(E.importDuplicates(read.filter((x) => x.ok).map((x) => x.sound), S.FRAMES.slice(0, 2))),
    restorePlan: { host: plain(plan), same: plain(E.restorePlan(image, image.length)), presets: plain(E.restorePlan(null, image.length)) },
    restoreFit: await result(() => E.restoreFit(host, ble, plan.fit.choice, true)),
    restoreFitTooMany: await result(() => E.restoreFit(host, ble, { projects: [0, 1, 2, 3, 4], presets: [] })),
  };
}

// ---- I6: PROJECTS (a session's calls in order, as FizzDataTests replays them: the numbers of sessions and clips too)
{
  const projects = [{ slot: 1, name: "NOIR", pats: 18 }, { slot: 3, name: "FIZZ TOUR", pats: 16, demo: true }, { slot: 4, name: "SKETCH", pats: 3 }];
  const img = E.demoPatterns(E.demoStore({ presets: S.SOUNDS, projects: projects.map(({ slot, name }) => ({ slot, name })) }), { projects });
  const opts = { fizz: S.FIZZ, date: S.DATE };
  const o = E.pjOpen(img), clip = E.pjCopy(o.h, 0, [4, 2, 3]);
  const paste = E.pjPaste(o.h, 3, 16, clip.c), clearDemo = E.pjClear(o.h, 2, [0]), clear = E.pjClear(o.h, 0, [5]);
  const keep = E.pjKeep(o.h, 0, opts), pat = E.pjFizzpat(clip.c, opts);
  fixtures.i6 = {
    projects,
    demoPatterns: { length: img.length, sha256: sha(img) },
    open: plain(o), copy: plain(clip), paste: plain(paste), clearDemo: plain(clearDemo), clear: plain(clear),
    keep: plain(keep), keptInfo: plain(await E.pjKeptInfo(keep.bytes, keep.fileName)),
    copyKept: plain(await E.pjCopyKept(o.h, keep.bytes, keep.fileName, [1, 0])),
    loadKept: plain(await E.pjLoadKept(o.h, 4, keep.bytes, keep.fileName)),
    fizzpat: { fileName: pat.fileName, length: pat.bytes.length, sha256: sha(pat.bytes) },
    readFizzpat: plain(await E.pjReadFizzpat(pat.bytes, pat.fileName)),
    renameKept: plain(await E.pjRenameKept(keep.bytes, keep.fileName, "noir two", opts)),
    image: (() => { const r = E.pjImage(o.h); return { ok: r.ok, length: r.image.length, sha256: sha(r.image) }; })(),
    undo: plain(E.pjUndo(o.h)), rebase: plain(E.pjRebase(o.h, img)), discard: plain(E.pjDiscard(o.h)),
  };
}

// ---- KEPT: the phone's presets as files (a preset without web tags is the web Manager's .fizzsnd byte for byte; a
// library of them makeFizzlib's), read back with their web tags
{
  const same = (a, b) => a.length === b.length && a.every((x, i) => x === b[i]);
  const s = S.SOUNDS[0], plainFile = E.keptFile(s), tagged = E.keptFile(S.SOUNDS[1], ["SHIMMER", "DARK"], { from: "KEPT" });
  if (!same(plainFile, new TextEncoder().encode(E.soundToFizzsnd(s)))) throw new Error("keptFile without web tags is not soundToFizzsnd");
  const entries = [...S.SOUNDS, S.V5].map((x) => ({ sound: x, webTags: [] }));
  const opts = { name: "SYN", fizz: S.FIZZ, date: S.DATE };
  if (!same(E.keptLib(entries, opts), E.makeFizzlib([...S.SOUNDS, S.V5], opts))) throw new Error("keptLib without web tags is not makeFizzlib");
  const lib = E.keptLib([{ sound: S.SOUNDS[0], webTags: ["SHIMMER"] }, { sound: S.SOUNDS[2], webTags: [] }], opts);
  fixtures.kept = {
    plain: { length: plainFile.length, sha256: sha(plainFile) },
    tagged: { b64: b64(tagged), sha256: sha(tagged) },
    readTagged: plain(E.keptRead(tagged, "B.fizzsnd")),
    readPlain: plain(E.keptRead(plainFile, "A.fizzsnd")),
    lib: { length: lib.length, sha256: sha(lib) },
    libRead: plain(await E.parseFizzlib(lib, "kept.fizzlib")),
    fileNames: ["PLUCK 2", "A/B:C", "  ", "BASS*"].map((n) => E.keptFileName(n)),
    notSound: await result(() => E.keptRead(new TextEncoder().encode("{\"format\":\"fizz-library\"}"), "x.fizzsnd")),
  };
}

mkdirSync(OUT, { recursive: true });
writeFileSync(join(OUT, "fixtures.json"), JSON.stringify(fixtures, null, 1) + "\n");
writeFileSync(join(OUT, "presets.fizzbak"), presets);
writeFileSync(join(OUT, "lib.fizzlib"), lib);
writeFileSync(join(OUT, "deflated.fizzlib"), deflated);
console.log(`wrote ${OUT}: fixtures.json, presets.fizzbak (${presets.length} B), lib.fizzlib (${lib.length} B), deflated.fizzlib (${deflated.length} B)`);
