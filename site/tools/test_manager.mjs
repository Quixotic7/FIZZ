// SPDX-License-Identifier: GPL-3.0-only
// The manager's checks (site/manager/, docs/WEB-MANAGER-PLAN.md Phase 0), in Node, no browser, no unit:
//   site/manager/fizzlink.js against a simulated FM-1 running FIZZ (site/manager/simfizz.js: the wasm FIZZ for its
//   own SysEx, a model of app.c's FZBI / FZBR / FZSR and fm1-base's framing for the store) on a fake Web MIDI:
//   - connect: the update handshake (FM-1_46..), FIZZ's IDENTITY; a stock FM-1, update mode, a FIZZ without its
//     data (safe mode) and one without the store commands (an older FIZZ) each refused or told apart;
//   - the library: LIST (MY PRESETS and the factory's), DUMP of everything, the same as the store's records read
//     offline (fizzstore.js);
//   - the backup: the region byte for byte, its CRC-32 the unit's, one request at a time and pipelined (the unit
//     drops what arrives while a frame waits: sent again, matched by offset), an autosave while it is read (read
//     again), the timing; a .fizzbak written, read back, its members' CRC-32s checked, opened as a .fizzlib too;
//   - the restore: the unit changed, the backup put back (FZSR): FZOK and the unit's presets as backed up; refused
//     while playing (FZER 10) and for another size (11); cut off: cancelled before anything is erased (12, nothing
//     changed), unplugged while it writes (lost; the unit's store cleared when plugged in again);
//   - the owner's saved images (../FIZZ-saves, outside the repo; skipped when absent) read offline.
//   node site/tools/test_manager.mjs
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { createEngine } from "../engine/engine.js";
import { storeSound, parseDump } from "../js/fizz_sysex.js";
import { soundFromStoreFrame, parseFizzlib } from "../js/library.js";
import { SimFizz } from "../manager/simfizz.js";
import { findFizz, parseBulk, bulkFrame } from "../manager/fizzlink.js";
import { makeFizzbak, readBackup, crc32, soundKey } from "../manager/fizzbak.js";
import { readRegion, layoutOf, LAYOUTS } from "../manager/fizzstore.js";

const SITE = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");
const module = await WebAssembly.compile(fs.readFileSync(path.join(SITE, "fizz.wasm")));
let fails = 0;
const check = (ok, msg) => { console.log(`${ok ? "ok  " : "FAIL"}  ${msg}`); if (!ok) fails++; };
const note = (msg) => console.log(`      ${msg}`);
const same = (a, b) => a.length === b.length && a.every((x, i) => x === b[i]);
const sim = (o = {}) => new SimFizz({ createEngine, module, frameMs: 0, tryMs: 150, ...o }).boot();
const blob = (k) => { const b = new Uint8Array(96); b[0] = 0x51; b[1] = 5; b[2] = k & 7; b[3] = 2 * k; b[50] = 10 * k; return b; };
const settle = (u, ms = 2500) => u.run(ms);                // FIZZ's autosave: 1.5 s after the last change
const mine = (rows) => rows.filter((r) => r.source === "mine");

// ---- a unit with three presets
const unit = await sim();
const NAMES = ["GLASS BELL", "SUB DROP", "NIGHT PAD"];
for (let k = 0; k < 3; k++) { unit.engine.midiIn(storeSound({ name: NAMES[k], blob: blob(k + 1), drive: 3 * k, pan: k - 1, reverb: 20 * k, tags: 1 << (k * 2) })); unit.run(80); }
settle(unit);

// ---- connect
let link = await findFizz(unit.access);
check(link.id === "FM-1_46001100" && link.port === "FIZZ FM-1", `connect: ${link.id} on "${link.port}" (the update handshake)`);
const ver = await link.identity();
check(typeof ver === "string" && ver.length > 0, `IDENTITY: FIZZ ${ver}`);
const info = await link.storeInfo();
check(info && info.size === LAYOUTS.host.size && info.crc === crc32(unit.flash()), `FZBI: region ${info && info.size} B at ${info && info.lo}, CRC-32 the region's`);
check(layoutOf(info.size).name === "host" && layoutOf(299008).name === "device", "the region's layout by its size (the unit's 299,008 B, the wasm's 1 MiB)");

// ---- the library
const rows = await link.list({ factory: true });
const my = mine(rows), fac = rows.filter((r) => r.source === "factory");
check(my.length === 3 && my.map((r) => r.name).join() === NAMES.join() && fac.length === 23, `LIST: MY PRESETS ${my.map((r) => `${r.slot} ${r.name}`).join(", ")}; factory ${fac.length}`);
const t0 = performance.now();
const dumps = await link.dumpAll("mine", my.length);
const dumpMs = performance.now() - t0;
const parsed = dumps.map(parseDump);
check(parsed.length === 3 && parsed.every((d, k) => d.name === NAMES[k] && d.blob[2] === (k + 1) % 8 && d.pan === k - 1 && d.reverb === 20 * k),
  `DUMP everything: ${parsed.map((d) => `${d.slot} ${d.name}`).join(", ")} (${dumpMs.toFixed(0)} ms)`);
const off = readRegion(unit.flash());
check(off.presets.length === 3 && off.presets.every((p, k) => p.name === NAMES[k] && p.slot === my[k].slot && p.id === my[k].id &&
  p.tags === my[k].tags && same(p.blob.subarray(2, 87), parsed[k].blob.subarray(2, 87)) && p.pan === parsed[k].pan && p.sends.reverb === parsed[k].reverb),
  `the store read offline: the same presets as LIST and DUMP (slots, ids, tags, patches, mix); ${off.projects.length} project(s), format ${off.format} (${off.tracks} tracks), ${off.magic}`);
check(off.tracks === 6, "the wasm is the six-track FIZZ (its projects' format 2)");

// ---- the backup, one request at a time
unit.counts.dropped = 0;
let bk = await link.backup({ depth: 1 });
check(same(bk.image, unit.flash()) && bk.crc === info.crc && bk.stats.resent === 0 && unit.counts.dropped === 0,
  `backup, depth 1: ${bk.size} B byte for byte, CRC-32 the unit's; ${bk.stats.frames} FZBR, none sent again, none dropped`);
note(`timing (simulated unit, frameMs 0): ${bk.stats.ms.toFixed(0)} ms, ${bk.stats.kbps.toFixed(0)} KB/s, ${bk.stats.perFrame.toFixed(2)} ms a frame, round trip median ${bk.stats.rtt.median.toFixed(2)} ms`);
// pipelined (64 KB of it): the unit holds one inbound frame, so a second request sent at once is mostly dropped and
// sent again (matched by offset, the bytes still right); spaced by a gap after the unit took the last one, none are
link.rtoMin = 20;
for (const [depth, gapMs] of [[2, 0], [3, 0], [2, 1]]) {
  unit.counts.dropped = 0;
  const r = await link.readRegion(info, { depth, gapMs, from: 0, to: 0x10000 });
  const ok = same(r.image.subarray(0, 0x10000), unit.flash().subarray(0, 0x10000));
  check(ok && (gapMs ? true : r.stats.resent > 0 && unit.counts.dropped > 0),
    `pipelined, depth ${depth}, gap ${gapMs} ms: 64 KB right; the unit dropped ${unit.counts.dropped}, ${r.stats.resent} sent again (${r.stats.ms.toFixed(0)} ms)`);
}
link.rtoMin = 150;
// an edit on the unit just after FZBI: FIZZ's autosave changes the store; the copy's CRC-32 differs; read once more
{
  let poked = false;
  unit.hooks.read = (o) => {
    if (o === 0 && !poked) { poked = true; unit.engine.midiIn(storeSound({ name: "WHILE READ", blob: blob(6) })); unit.run(2500); }
  };
  const r = await link.backup({ depth: 1 });
  unit.hooks.read = null;
  settle(unit);
  const rows2 = mine(await link.list());
  check(poked && r.stats.attempt === 2 && rows2.some((x) => x.name === "WHILE READ") && readRegion(r.image).presets.some((p) => p.name === "WHILE READ"),
    `an autosave while it was read: the CRC-32 differed, read again (attempt ${r.stats.attempt}), the copy has the new preset`);
}

// ---- the .fizzbak
bk = await link.backup();
const sounds = (await link.dumpAll("mine", 4)).map((f) => ({ ...soundFromStoreFrame(f), source: { fizz: ver } }));
const webTags = { [soundKey(sounds[0])]: ["SHIMMER", "BELLS"] };
const file = makeFizzbak({ kind: "full", image: bk.image, crc: bk.crc, region: [bk.lo, bk.size], presets: sounds, fizz: ver,
  unit: { identity: link.id, port: link.port }, web: { tags: webTags }, transfer: { kbps: bk.stats.kbps } });
const back = await readBackup(file, "x.fizzbak");
check(back.kind === "full" && same(back.image, bk.image) && back.presets.length === 4 && back.presets.every((s, k) => s.name === sounds[k].name && same(s.blob, sounds[k].blob)),
  `.fizzbak: ${file.length} B; read back: store.bin byte for byte, ${back.presets.length} .fizzsnd presets`);
check(back.manifest.store.tracks === 6 && back.manifest.projects.length === back.summary.projects.length && back.manifest.fizz === ver &&
  back.web.tags[soundKey(sounds[0])].join() === "SHIMMER,BELLS", `manifest: ${back.manifest.store.layout} ${back.manifest.store.magic}, ${back.manifest.store.tracks} tracks, CRC-32 ${back.manifest.store.crc32}; the web tags kept (${back.web.tags[soundKey(sounds[0])].join(", ")})`);
const asLib = await parseFizzlib(file, "x.fizzbak");
check(asLib.items.length === 4 && asLib.items.every((x) => x.ok), "a .fizzbak opens as a .fizzlib (the preview's Import reads it)");
{
  const bad = file.slice();
  const at = Buffer.from(bad).indexOf(Buffer.from("store.bin")) + 9 + 5000;   // (a byte inside store.bin's data)
  bad[at] ^= 0x55;
  let e = null;
  try { await readBackup(bad, "bad.fizzbak"); } catch (x) { e = x; }
  check(e && /damaged|CRC/.test(e.message), `a damaged .fizzbak refused: ${e && e.message}`);
  const raw = await readBackup(bk.image, "store.bin");
  check(raw.source === "bin" && raw.presets.length === 4 && raw.presets[0].name === NAMES[0], "a raw image (fizz_xdata.py backup) read as a full backup, its presets from the store's records");
  const pres = await readBackup(makeFizzbak({ kind: "presets", presets: sounds, fizz: ver }), "p.fizzbak");
  check(pres.kind === "presets" && !pres.image && pres.presets.length === 4, "a presets-only .fizzbak: no store.bin, the four presets");
}

// ---- the restore
{
  for (const n of ["EXTRA ONE", "EXTRA TWO"]) { unit.engine.midiIn(storeSound({ name: n, blob: blob(7) })); unit.run(80); }
  settle(unit);
  const before = mine(await link.list()).map((r) => r.name);
  let phases = [], last = 0, restored = null;
  unit.hooks.restored = () => { restored = unit.flash().slice(0); };
  const r = await link.restore(bk.image, { onPhase: (p) => phases.push(p), onProgress: (f) => { last = f; } });
  const after = mine(await link.list()).map((r2) => r2.name);
  check(r.ok && phases.join() === "check,write" && last === 1 && unit.counts.fzsrReads >= 2 * bk.size / 512,
    `restore (FZSR): FZOK after ${r.served} reads, both passes (${phases.join(" then ")}), ${(2 * bk.size / 1024 / (r.ms / 1000)).toFixed(0)} KB/s`);
  check(before.length === 6 && after.join() === sounds.map((s) => s.name).join() && restored && readRegion(restored).presets.length === 4,
    `the unit as backed up: MY PRESETS ${before.length} -> ${after.length} (${after.join(", ")})`);
  link.log = () => {};
}
{
  unit.playing = true;
  let e = null;
  try { await link.restore(bk.image); } catch (x) { e = x; }
  unit.playing = false;
  check(e && e.code === "fzer" && e.fzer === 10 && !e.written, `restore while playing: refused, FZER ${e && e.fzer} (${e && e.message})`);
  e = null;
  try { await link.restore(bk.image.subarray(0, 299008)); } catch (x) { e = x; }
  check(e && e.fzer === 11, `restore of another size: refused, FZER ${e && e.fzer}`);
  const bad = Uint8Array.from([0xF0, ...new Array(40).fill(0x11), 0xF7]), f = bulkFrame("FZBI", 7).slice(0);
  f[f.length - 2] ^= 1;
  check(parseBulk(bad) === null && parseBulk(f) === null && parseBulk(bulkFrame("FZBD", 5)).tag === "FZBD", "parseBulk: garbage and a bad sum refused");
}

// ---- cut off
{
  // a backup, the cable pulled half way
  const u = await sim();
  const l = await findFizz(u.access);
  u.hooks.read = (o) => { if (o >= 0x80000) u.unplug(); };
  let e = null;
  try { await l.backup(); } catch (x) { e = x; }
  check(e && (e.code === "lost" || e.code === "timeout"), `backup, unplugged half way: ${e && e.code} (${e && e.message}); nothing kept`);
  u.dispose();
}
{
  // a restore cancelled while the unit still checks the image: the rest answered with zeros, its CRC fails: FZER 12
  const u = await sim();
  for (const n of ["KEEP ME"]) { u.engine.midiIn(storeSound({ name: n, blob: blob(3) })); u.run(80); }
  settle(u);
  const l = await findFizz(u.access);
  const ac = new AbortController();
  let e = null;
  try { await l.restore(bk.image, { onProgress: (f) => { if (f > 0.2) ac.abort(); }, signal: ac.signal }); } catch (x) { e = x; }
  const names = mine(await l.list()).map((r) => r.name);
  check(e && e.code === "cancel" && e.fzer === 12 && !e.written && names.join() === "KEEP ME", `restore cancelled in the check pass: FZER ${e && e.fzer}, nothing erased (${names.join(", ")})`);
  // unplugged while it writes: the link says so; plugged in again, the unit starts on a cleared store
  u.hooks.restoreRead = (pass, a) => (pass === 2 && a >= 0x40000 ? "cut" : undefined);
  e = null;
  try { await l.restore(bk.image); } catch (x) { e = x; }
  check(e && (e.code === "lost" || e.code === "timeout"), `restore, unplugged while writing: ${e && e.code} (${e && e.message})`);
  u.hooks.restoreRead = null;
  await u.replug();
  const l2 = await findFizz(u.access);
  const n2 = mine(await l2.list());
  check(n2.length === 0, `plugged in again: FIZZ cleared its half-written store (MY PRESETS ${n2.length}); a restore from the file is the way back`);
  const r = await l2.restore(bk.image);
  check(r.ok && mine(await l2.list()).length === 4, "restored from the file afterwards: the four presets back");
  l2.close(); u.dispose();
}

// ---- the wrong device
{
  const stock = await new SimFizz({ id: "FM-1_015" }).boot();
  let e = null;
  try { await findFizz(stock.access); } catch (x) { e = x; }
  check(e && e.code === "notfizz" && e.detail === "FM-1_015", `a stock FM-1 refused: ${e && e.code} (${e && e.detail})`);
  const ota = await new SimFizz({ id: "ota-FM-1_0" }).boot();
  e = null;
  try { await findFizz(ota.access); } catch (x) { e = x; }
  check(e && e.code === "update", `an FM-1 in update mode refused: ${e && e.code}`);
  const other = await new SimFizz({ id: "FM-1_45001000" }).boot();   // (another fm1-base app: ChoralRoot's prefix)
  e = null;
  try { await findFizz(other.access); } catch (x) { e = x; }
  check(e && e.code === "notfizz", `another fm1-base firmware (${other.id}) refused: ${e && e.code}`);
  const none = { inputs: new Map(), outputs: new Map() };
  e = null;
  try { await findFizz(none); } catch (x) { e = x; }
  check(e && e.code === "notfound", "nothing plugged in: notfound");
  const old = await sim({ bulk: false });
  const lo = await findFizz(old.access);
  e = null;
  try { await lo.backup(); } catch (x) { e = x; }
  check((await lo.storeInfo()) === null && e && e.code === "nobulk" && (await lo.identity()), "a FIZZ without FZBI (before P2.13): no backup, its library still read");
  lo.close(); old.dispose();
  const safe = await sim({ answers: false });
  const ls = await findFizz(safe.access);
  check((await ls.identity()) === null && (await ls.storeInfo()) !== null, "FIZZ not answering (safe mode, no data): IDENTITY none, the store's backup still there");
  ls.close(); safe.dispose();
}

// ---- the owner's saved images (outside the repo), read offline
{
  const dir = path.resolve(SITE, "../../FIZZ-saves");
  const imgs = fs.existsSync(dir) ? fs.readdirSync(dir).filter((f) => f.endsWith(".bin")).sort() : [];
  if (!imgs.length) note("no ../FIZZ-saves: the owner's images not read");
  for (const f of imgs) {
    const b = new Uint8Array(fs.readFileSync(path.join(dir, f)));
    let r = null, e = null;
    try { r = readRegion(b); } catch (x) { e = x; }
    check(r && r.magic && !e, `${f}: ${r ? `${r.layout}, ${r.magic}, ${r.tracks || "?"} tracks, projects ${r.projects.map((p) => p.name).join(", ") || "none"}, ${r.presets.length} presets` : e.message}`);
  }
}

link.close();
unit.dispose();
console.log(fails ? `MANAGER TEST FAILED (${fails})` : "manager test passed");
process.exit(fails ? 1 : 0);
