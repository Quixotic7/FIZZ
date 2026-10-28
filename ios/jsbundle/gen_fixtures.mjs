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

mkdirSync(OUT, { recursive: true });
writeFileSync(join(OUT, "fixtures.json"), JSON.stringify(fixtures, null, 1) + "\n");
writeFileSync(join(OUT, "presets.fizzbak"), presets);
writeFileSync(join(OUT, "lib.fizzlib"), lib);
writeFileSync(join(OUT, "deflated.fizzlib"), deflated);
console.log(`wrote ${OUT}: fixtures.json, presets.fizzbak (${presets.length} B), lib.fizzlib (${lib.length} B), deflated.fizzlib (${deflated.length} B)`);
