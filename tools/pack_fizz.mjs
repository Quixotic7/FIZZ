#!/usr/bin/env node
// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
// pack_fizz: FIZZ's store image (the host's 1 MiB layout, as tools/tour.c's `store PATH` writes it) into the files the
// web Manager imports, and back. Everything goes through the Manager's own code (site/manager/projects.js,
// fizzstore.js, site/js/library.js): the records are FIZZ's own (core/app/persist.c), nothing is re-serialised here.
//
//   node tools/pack_fizz.mjs prj   IMAGE SLOT OUT.fizzprj      project SLOT (1..8) with the MY presets it uses
//                                                              (PROJECTS tab: drop it, then LOAD INTO a slot)
//   node tools/pack_fizz.mjs lib   IMAGE OUT.fizzlib [--project SLOT] [--names A,B,..]
//                                                              MY PRESETS (all; or the ones project SLOT uses; or by
//                                                              name) as a .fizzlib (IMPORT tab)
//   node tools/pack_fizz.mjs image IN.fizzprj OUT.bin [SLOT] [--base IMAGE]
//                                                              a .fizzprj into a blank host image (or a copy of
//                                                              IMAGE) at SLOT (default 1), for the tour's
//                                                              `load OUT.bin SLOT`
//   node tools/pack_fizz.mjs check FILE..                      read back with the Manager's readers (.fizzprj, .fizzlib,
//                                                              .fizzsnd, .fizzbak, a store image .bin): a summary
//   options: --fizz VERSION (the "fizz" field written; default "tour")
import { readFileSync, writeFileSync } from "node:fs";

const M = (p) => new URL(`../site/${p}`, import.meta.url).href;
const { modelOf, imageOf, project, projects, keepProject, loadProject, makeFizzprj, readFizzprj, walkPat, patName, rkey, RK } =
  await import(M("manager/projects.js"));
const { readRegion, buildImage, LAYOUTS } = await import(M("manager/fizzstore.js"));
const { makeFizzlib, parseFizzlib, parseFizzsnd } = await import(M("js/library.js"));
const { readBackup } = await import(M("manager/fizzbak.js"));
const { readImport } = await import(M("manager/importer.js"));
const { FIZZ_TAGS } = await import(M("js/fizz_sysex.js"));

const args = process.argv.slice(2);
const opt = (k, d = null) => { const i = args.indexOf(k); if (i < 0) return d; const v = args[i + 1]; args.splice(i, 2); return v; };
const fizz = opt("--fizz", "tour"), onlyProject = opt("--project"), names = opt("--names"), base = opt("--base");
const [cmd, ...rest] = args;
const die = (m) => { console.error(`pack_fizz: ${m}`); process.exit(1); };
const readImg = (p) => { const b = new Uint8Array(readFileSync(p)); if (b.length !== LAYOUTS.host.size) die(`${p}: ${b.length} bytes, not a host store image (${LAYOUTS.host.size})`); return b; };
const slotOf = (s) => { const n = Number(s); if (!(n >= 1 && n <= 8)) die(`slot ${s}: 1..8`); return n - 1; };

// a MY PRESETS record (fizzstore preset()) as a library.js sound (fizzbak.js fromRecord's shape)
const soundOf = (p) => ({ fizz: true, name: p.name, blob: p.blob, drive: p.drive, pan: p.pan, sends: p.sends,
  fizzTags: FIZZ_TAGS.filter((_, k) => (p.tags >> k) & 1), favourite: false, source: { fizz, from: `MY PRESETS ${p.slot}` },
  notes: [], origin: [], mark: "", tags: [] });

function summary(kept) {
  const pats = kept.recs.filter((r) => r.kind === 3).map((r) => { const w = walkPat(r.bytes); return `${patName(r.i)}${w ? `(${w.trigs} trigs)` : "(?)"}`; });
  return `"${kept.name}"${kept.demo ? " (demo)" : ""}: ${kept.recs.length} records, patterns ${pats.join(" ") || "none"}; ` +
    `pool ${kept.sounds}; MY presets carried ${kept.presets.size}`;
}

async function check(file) {
  const b = new Uint8Array(readFileSync(file));
  if (/\.fizzprj$/i.test(file)) { const k = await readFizzprj(b, file); console.log(`${file}: project ${summary(k)}`); return; }
  if (/\.fizzsnd$/i.test(file)) { const s = parseFizzsnd(b, file); console.log(`${file}: sound "${s.name}" blob v${s.blob[1]}`); return; }
  if (/\.fizzlib$/i.test(file)) {
    const l = await parseFizzlib(b, file), bad = l.items.filter((x) => !x.ok);
    const imp = await readImport([{ name: file, bytes: b }]);              // (the IMPORT tab's own reader)
    console.log(`${file}: library "${l.meta.name}", ${l.items.length - bad.length} sounds (${l.items.filter((x) => x.ok).map((x) => x.sound.name).join(", ")})` +
      `${bad.length ? `; BAD ${bad.map((x) => `${x.file}: ${x.error}`).join("; ")}` : ""}; IMPORT reads ${imp.filter((x) => x.ok).length} ok`);
    if (bad.length || imp.some((x) => !x.ok)) process.exitCode = 1;
    return;
  }
  const r = await readBackup(b, file);                                     // (.fizzbak, or a raw image)
  if (r.summary) {
    console.log(`${file}: store image, layout ${r.summary.layout}, ${r.summary.presets.length} MY presets; projects ${r.summary.projects.map((p) => `${p.slot} "${p.name}"`).join(", ")}`);
    if (r.image) for (const p of projects(modelOf(r.image)).filter((x) => x.there))
      console.log(`  slot ${p.p + 1} "${p.name}"${p.demo ? " (demo)" : ""}: ${p.used} patterns, pool ${p.pool}`);
  } else console.log(`${file}: ${r.kind}, ${r.presets.length} presets`);
}

if (cmd === "prj") {
  const [img, slot, out] = rest;
  if (!out) die("prj IMAGE SLOT OUT.fizzprj");
  const m = modelOf(readImg(img)), p = slotOf(slot);
  if (!project(m, p).there) die(`${img}: slot ${slot} is empty`);
  const kept = keepProject(m, p);
  writeFileSync(out, makeFizzprj(kept, { fizz }));
  console.log(`${out}: ${summary(kept)}`);
  await check(out);
} else if (cmd === "lib") {
  const [img, out] = rest;
  if (!out) die("lib IMAGE OUT.fizzlib [--project SLOT] [--names A,B]");
  const image = readImg(img);
  let ps = readRegion(image).presets;
  if (onlyProject) { const k = keepProject(modelOf(image), slotOf(onlyProject)); ps = ps.filter((p) => k.presets.has(p.id)); }
  if (names) { const want = names.split(",").map((s) => s.trim().toUpperCase()); ps = ps.filter((p) => want.includes(p.name.toUpperCase())); }
  if (!ps.length) die("no MY presets to write");
  writeFileSync(out, makeFizzlib(ps.map(soundOf), { fizz }));
  console.log(`${out}: ${ps.length} MY presets (${ps.map((p) => `${p.slot}:${p.name}`).join(", ")})`);
  await check(out);
} else if (cmd === "image") {
  const [inp, out, slot = "1"] = rest;
  if (!out) die("image IN.fizzprj OUT.bin [SLOT]");
  const kept = await readFizzprj(new Uint8Array(readFileSync(inp)), inp);
  const m = modelOf(base ? readImg(base) : buildImage("host", {})), q = slotOf(slot);
  const r = loadProject(m, q, kept);
  // the settings record (persist.c ser_set / de_set): FIZZ wipes a projects' store without one ('G' QD_PRJ_FORMAT 2:
  // prj_format_ok). A blank image gets the shortest one de_set takes: project q, A01, track 1, no effects' values
  // (their defaults), the Home view EDIT. A --base image keeps its own
  if (!m.prj.has(rkey(RK.SET, 0, 0))) m.prj.set(rkey(RK.SET, 0, 0), Uint8Array.of(0x47, 2, q, 0, 0, 0, 0, 0, 0));
  writeFileSync(out, imageOf(m));
  console.log(`${out}: "${kept.name}" in slot ${q + 1}, ${r.presetsAdded} MY presets added, ${r.copies} copies`);
  await check(out);
} else if (cmd === "check") {
  if (!rest.length) die("check FILE..");
  for (const f of rest) await check(f);
} else {
  console.error(readFileSync(new URL(import.meta.url)).toString().split("\n").filter((l) => l.startsWith("//   ")).join("\n"));
  process.exit(2);
}
