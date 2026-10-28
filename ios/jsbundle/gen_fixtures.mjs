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

mkdirSync(OUT, { recursive: true });
writeFileSync(join(OUT, "fixtures.json"), JSON.stringify(fixtures, null, 1) + "\n");
writeFileSync(join(OUT, "presets.fizzbak"), presets);
writeFileSync(join(OUT, "lib.fizzlib"), lib);
writeFileSync(join(OUT, "deflated.fizzlib"), deflated);
console.log(`wrote ${OUT}: fixtures.json, presets.fizzbak (${presets.length} B), lib.fizzlib (${lib.length} B), deflated.fizzlib (${deflated.length} B)`);
