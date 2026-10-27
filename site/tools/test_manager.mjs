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
//   - Phase 1: STATUS, PROJECTS, TAGS, RENAME, SWAP (MY PRESETS now, the pool a job), POOL REF, USERS, DELETE (IN USE
//     first, then with confirm), PROJECT, FLUSH; BUSY while playing; the import of synthetic Digitone II files;
//   - Phase 2-A (docs/MANAGER-P2-PLAN.md): the sound editor's model (sound.js) through the real FIZZ: every factory
//     preset and random sounds DUMPed, edited, stored with STORE6 and DUMPed back the same; the values' texts the
//     core's; EXPORT's files (.fizzsnd read back, .dnsnd / .dn2pst ZIPs, .dn2snd the payload) and their carry rows;
//   - Phase 2-B: a track's DUMP (its extras), PARAM (ACK, clamped, BAD TRACK, while playing), AUDITION (nothing
//     stored) and the track's own DUMP sent back (the same DUMP again, byte for byte), OWN, a STORE over a used place
//     (BUSY while playing; a job, its reply when done);
//   - the owner's fixes (2026-10-25): the unit's panel selects a track (a track's DUMP and STATUS follow), BROWSE (the
//     unit's browser on a preset: OCT- cancels, OCT+ loads, NOT FOUND, BAD TRACK), POOL REF in the next LIST, IMPORT of
//     FIZZ's .fizzlib / .fizzsnd / .fizzbak with the duplicates flagged and only the new ones sent;
//   - FIZZ MIDI (e): a MIDI track's sound (STORE6 of a MIDI blob, the core's rules) DUMPed as MIDI, its blob byte for
//     byte; edited (a value, a CC number, a slot untouched), STOREd and DUMPed back the same, still MIDI; on a track
//     (LOAD, AUDITION; PARAM takes a slot's value, not its CC number); .fizzsnd / .fizzlib / .fizzbak round trips and
//     IMPORT keep it MIDI; the Digitone exports leave it out; an FM TONE sound unchanged; the lists' MIDI chip;
//   - the owner's saved images (../FIZZ-saves, outside the repo; skipped when absent) read offline.
//   node site/tools/test_manager.mjs
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { createEngine } from "../engine/engine.js";
import { storeSound, parseDump, parseReply, trackFrame, listRequest, usersRequest, projectRequest, projectsRequest, dumpRequest } from "../js/fizz_sysex.js";
import { readFiles, storeFrame } from "../js/digitone.js";
import { dn2RecordFromParams, wrapDn2Record, dn2pstZip } from "../js/dn2pst.js";
import { soundFromStoreFrame, parseFizzlib } from "../js/library.js";
import { SimFizz } from "../manager/simfizz.js";
import { findFizz, parseBulk, bulkFrame } from "../manager/fizzlink.js";
import { makeFizzbak, readBackup, crc32, soundKey } from "../manager/fizzbak.js";
import { readRegion, layoutOf, LAYOUTS, regionRecords, buildImage, convertImage, writeStore, RESERVE } from "../manager/fizzstore.js";
import { modelOf, imageOf, projects, project, walkPat, rkey, RK, patName, copyPatterns, pastePatterns, clearPatterns, keepProject, loadProject,
  renameKept, makeFizzprj, readFizzprj, makeFizzpat, readFizzpat, patFileName, prjFileName } from "../manager/projects.js";
import { backupFlash, isBackupFile } from "../js/backup-play.js";
import { poolPlaces, shownRows, newFilter, listHtml } from "../manager/presets.js";
import { dnsndV4 } from "./dnsnd_fixture.mjs";
import { soundFromFrame, storeFrameOf, blob6, libSound, text, info as vinfo, VALUES, Q, lfo, LF, copySound, tickStep, stepOf,
  soundFromLib, sameValues, isMidiBlob, midiBlob, newMidi } from "../manager/sound.js";
import { exportFiles, carryOf, noteRow, MIDI_ONLY } from "../manager/exporter.js";
import { readImport, markDuplicates } from "../manager/importer.js";
import { fizzToDn2pst } from "../js/fizz2dn.js";
import { parseFizzsnd } from "../js/library.js";

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

// ---- Phase 1: the library, the pool, the projects edited (the wasm's commands, its jobs stepped by its store)
{
  const u = await sim();
  for (const [k, n] of ["ALPHA", "BETA", "GAMMA"].entries()) { u.engine.midiIn(storeSound({ name: n, blob: blob(k + 2), tags: 1 << k })); u.run(80); }
  settle(u);
  const l = await findFizz(u.access);
  const st = await l.status();
  check(st && st.tracks === 6 && !st.playing && !st.noStore && st.project === 1 && st.projects === 1 && st.mine === 3 && st.lib[1] > 0 && st.prj[1] > st.lib[1],
    `STATUS: ${st && `${st.tracks} tracks, project ${st.project} of ${st.projects}, ${st.mine} my presets, ${st.pool} pool sounds, store pages ${st.lib.join("/")} + ${st.prj.join("/")}`}`);
  const pj = await l.projects();
  check(pj && pj.current === 1 && pj.list.length === 8 && pj.list[0].there && pj.list[0].current && !pj.list[1].there,
    `PROJECTS: ${pj && pj.list.filter((p) => p.there).map((p) => `${p.slot} ${p.name} (${p.pages} pages)`).join(", ")}`);
  let rows = mine(await l.list({ pool: true }));
  const A = rows.find((r) => r.name === "ALPHA"), B = rows.find((r) => r.name === "BETA");
  let r = await l.tags("mine", A.slot, ["KEYS", "FX"]);
  rows = mine(await l.list());
  check(r.status === 0 && rows.find((x) => x.slot === A.slot).tags === ((1 << 1) | (1 << 6)), `TAGS: ALPHA -> KEYS + FX (${r.statusName})`);
  r = await l.rename("mine", A.slot, "ALPHA PRIME");
  rows = mine(await l.list());
  check(r.status === 0 && rows.find((x) => x.slot === A.slot).name === "ALPHA PRIME" && rows.find((x) => x.slot === A.slot).id === A.id, "RENAME: ALPHA PRIME, its id kept");
  r = await l.swap("mine", A.slot, B.slot);
  rows = mine(await l.list());
  check(r.status === 0 && rows.find((x) => x.slot === A.slot).id === B.id && rows.find((x) => x.slot === B.slot).id === A.id, "SWAP MY PRESETS: the places swapped, the ids with them");
  r = await l.swap("mine", B.slot, 200);
  rows = mine(await l.list());
  check(r.status === 0 && rows.find((x) => x.slot === 200) && rows.find((x) => x.slot === 200).id === A.id, "SWAP into an empty place: a move (to 200)");
  const pr = await l.poolRef(0, A.id);
  const pool = (await l.list({ pool: true })).filter((x) => x.source === "pool");
  check(pr.status === 0 && pool.some((x) => x.slot === pr.a && x.id === A.id && x.name === "ALPHA PRIME"), `POOL REF: pool ${pr.a} references ALPHA PRIME (no copy)`);
  const users = await l.users("mine", 200);
  check(users.projects && users.projects.length === 1 && users.projects[0].project === 1, `USERS: ${JSON.stringify(users.projects)}`);
  u.tapPlay();
  const st2 = await l.status();
  r = await l.swap("pool", pr.a, 100);
  check(st2.playing && r.status === 8, `while playing: STATUS says so, a pool SWAP answers BUSY (${r.statusName})`);
  u.tapPlay();
  r = await l.swap("pool", pr.a, 100);
  const pool2 = (await l.list({ pool: true })).filter((x) => x.source === "pool");
  check(r.status === 0 && pool2.some((x) => x.slot === 100 && x.id === A.id), `SWAP the pool (a job): pool ${pr.a} moved to 100`);
  r = await l.remove("mine", 200);
  check(r.status === 10 && r.a === 1, `DELETE a MY preset the pool uses, no confirm: IN USE (${r.a} project, ${r.b} steps)`);
  r = await l.remove("mine", 200, { confirm: true });
  rows = mine(await l.list({ pool: true }));
  check(r.status === 0 && !rows.some((x) => x.slot === 200), "DELETE with confirm: gone");
  r = await l.remove("mine", 77);
  check(r.status === 9, `DELETE of an empty place: ${r.statusName}`);
  r = await l.project("new", 2);
  const st3 = await l.status();
  check(r.status === 0 && st3.project === 2 && st3.projects === 2, `PROJECT NEW 2: worked on now (${st3.projects} projects)`);
  r = await l.rename("project", 2, "SUNDAY");
  r = await l.project("load", 1);
  const pj2 = await l.projects();
  check(r.status === 0 && pj2.current === 1 && pj2.list[1].name === "SUNDAY", `PROJECT LOAD 1, project 2 named ${pj2.list[1].name}`);
  const fl = await l.flush();
  check(fl.kind === "status" && !fl.dirty, "FLUSH: a STATUS when everything is written");
  // IMPORT: synthetic Digitone II sounds (.dn2pst, .dn2snd: never real files), into MY PRESETS by STORE
  const rec = dn2RecordFromParams({ algo: 3 }, { name: "SYNTH TEST" });
  const files = [{ name: "SYNTH TEST.dn2pst", bytes: dn2pstZip(wrapDn2Record(rec), { FormatVersion: 1, ProductType: ["24", "38"], Payload: "SYNTH TEST.dn2snd", FileType: "Sound", FirmwareVersion: "1.10D", MetaInfo: { Tags: ["BASS", "SHIMMER"] } }) },
    { name: "RAW TEST.dn2snd", bytes: wrapDn2Record(dn2RecordFromParams({}, { name: "RAW TEST" })) }];
  const items = await readFiles(files);
  const before = mine(await l.list()).length;
  let ok = items.length === 2 && items.every((x) => x.ok);
  for (const it of items) { const s = await l.store(storeFrame(it.sound)); ok = ok && s.status === 0; }
  rows = mine(await l.list());
  check(ok && rows.length === before + 2 && rows.some((x) => x.name === "SYNTH TEST") && rows.some((x) => x.name === "RAW TEST"),
    `IMPORT: a synthetic .dn2pst and .dn2snd read and stored into MY PRESETS (${rows.length} now)`);
  {   // a Digitone 1 sound saved on OS 1.42 with record version 4 (the shape of the owner's F186 KICK01, synthetic): it
      // was refused as "a newer Digitone format (version 4)"; read with version 5's layout
    const [v4] = await readFiles([{ name: "SYNTH KICK.dnsnd", bytes: dnsndV4() }]);
    const s4 = v4.ok && await l.store(storeFrame(v4.sound));
    check(v4.ok && v4.sound.name === "SYNTH KICK" && v4.sound.blob[2] === 2 && v4.sound.fizzTags.join() === "DRUMS" && /record 4/.test(v4.sound.mark) && s4 && s4.status === 0,
      `IMPORT: a synthetic version-4 .dnsnd (OS 1.42) read (${v4.ok ? `${v4.sound.name}, ${v4.sound.mark}` : v4.error}) and stored`);
  }
  {   // the page's lists: the pool's places by preset id (a factory id too: POOL REF takes one), the filters
    const fac1 = (await l.list({ factory: true })).find((x) => x.source === "factory");
    const pf = await l.poolRef(0, fac1.id);
    const all = await l.list({ factory: true, pool: true }), pp = poolPlaces(all);
    check(pf.status === 0 && (pp.get(fac1.id) || []).includes(pf.a) && !mine(all).some((x) => x.id === fac1.id),
      `POOL REF of a factory preset (id ${fac1.id}, ${fac1.name}): pool ${pf.a}; its id no MY preset's`);
    const f = newFilter();
    const a0 = shownRows(all, f); f.src = "factory"; const a1 = shownRows(all, f); f.src = "mine"; f.tags.add("BASS"); const a2 = shownRows(all, f);
    check(a0.mine.length === mine(all).length && a0.fac.length > 0 && !a1.mine.length && a1.fac.length === a0.fac.length && !a2.fac.length && a2.mine.every((x) => x.tags & 1),
      `the lists' filters: ALL ${a0.mine.length} + ${a0.fac.length}, FACTORY ${a1.fac.length}, MY PRESETS + BASS ${a2.mine.length}`);
  }
  {   // an older FIZZ answers a new command 7F 07 <cmd> (BAD COMMAND): read as such (the page says: update FIZZ)
    const e = [0x7F, 0x07, 0x12], f = Uint8Array.from([0xF0, 0x7D, 0x46, 0x5A, 0x01, ...e, (0x80 - (e.reduce((a, b) => a + b) & 0x7F)) & 0x7F, 0xF7]);
    const pr2 = parseReply(f);
    check(pr2 && pr2.kind === "error" && pr2.status === 7 && pr2.command === 0x12, "an older FIZZ's BAD COMMAND for TAGS read as such");
  }
  check(u.counts.dropped === 0, `no frame dropped (one request at a time): ${JSON.stringify(u.counts.cmds)}`);
  l.close(); u.dispose();
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

// ---- Phase 2-A: the sound editor's model and EXPORT
{
  const hi = VALUES.findIndex((r) => r[4] === "fix2s"), lo = VALUES.findIndex((r) => r[2] - r[1] < 1000);
  check(hi >= 0 && tickStep(Q.FREQ) === 1 && tickStep(Q.FREQ, true) === 100 && tickStep(Q.FREQ, false, 10) === 100 && tickStep(hi) === 1 && tickStep(hi, true) === 100 && tickStep(hi, false, 5) > 1 && tickStep(hi, false, 10) === 100 && tickStep(hi, false, 40) === 100 && tickStep(hi, true, 10) === 100 && tickStep(lo, false, 10) === stepOf(lo) && tickStep(lo, true) === stepOf(lo, true),
    `EDIT steps: LFO DEPTH / SPEED and FREQ 0.01 a tick, quick ticks up to 1.00, SHIFT 1.00; the rest as before`);
}
{
  const u = await sim();
  const lk = await findFizz(u.access);
  const rows = await lk.list({ factory: true });
  const fac = rows.filter((r) => r.source === "factory");
  let ok = 0, bad = [];
  for (const r of fac) {
    const f = await lk.dump("factory", r.slot);
    const s = soundFromFrame(f);
    const st = await lk.store(storeFrameOf(s, { slot: 0 }));
    const back = st && st.status === 0 ? soundFromFrame(await lk.dump("mine", st.slot)) : null;
    if (back && back.values.every((x, i) => x === s.values[i]) && back.name === s.name && back.pan === s.pan && back.sends.reverb === s.sends.reverb) ok++;
    else bad.push(r.name);
  }
  check(ok === fac.length, `EDIT: every factory preset (${fac.length}) DUMPed into the editor's 110 values, stored back (STORE6), DUMPed the same${bad.length ? `: not ${bad.join(", ")}` : ""}`);
  // random sounds: each value anywhere in its range
  let seed = 12345;
  const rnd = () => (seed = (seed * 1103515245 + 12345) & 0x7fffffff) / 0x7fffffff;
  let rok = 0;
  const base = soundFromFrame(await lk.dump("factory", 1));
  for (let t = 0; t < 24; t++) {
    const s = copySound(base);
    s.values = VALUES.map((r) => r[1] + Math.floor(rnd() * (r[2] - r[1] + 1)));
    s.name = `RAND ${t}`;
    const st = await lk.store(storeFrameOf(s, { slot: 200 }));
    const back = st && st.status === 0 ? soundFromFrame(await lk.dump("mine", 200)) : null;
    if (back && back.values.every((x, i) => x === s.values[i])) rok++;
  }
  check(rok === 24, `EDIT: 24 random sounds (every value anywhere in its range) through STORE6 and DUMP unchanged (${rok})`);
  check(VALUES.length === 115 && text(Q.FREQ, 12700) === "127.00" && text(Q.HARM, -150) === "-1.50" && text(Q.MIX, 5) === "+5" && text(33, 127) === "INF" &&
    text(lfo(0, LF.WAVE), 1) === "SINE" && text(Q.FTYPE, 7) === "EQ" && text(Q.HOLD, 127) === "NOTE" && vinfo(Q.ALGO).max === 8,
    "EDIT: the values' names, ranges and texts are the core's (fizzvalues.js: make -C tools values-check)");
  // EXPORT
  const s = copySound(base);
  s.name = "EXPORT ME";
  s.values[lfo(2, LF.DEST)] = 33; s.values[lfo(2, LF.DEPTH)] = -3000; s.values[Q.BITS] = 40; s.values[Q.AMOD] = 1; s.values[Q.HOLD] = 60;
  const lib = libSound(s, { fizz: "test" });
  const fz = exportFiles([lib], "fizz"), back = parseFizzsnd(fz.bytes, fz.name);
  check(fz.name === "EXPORT ME.fizzsnd" && same(back.blob, blob6(s)) && back.name === "EXPORT ME", `EXPORT .fizzsnd: ${fz.name}, its blob the editor's`);
  const d1 = exportFiles([lib], "dnsnd"), d2 = exportFiles([lib], "dn2pst"), d3 = exportFiles([lib], "dn2snd");
  const pay = fizzToDn2pst({ blob: lib.blob, name: lib.name, tags: lib.fizzTags, pan: lib.pan, sends: lib.sends, drive: lib.drive }).payload;
  check(d1.name === "EXPORT ME.dnsnd" && d1.bytes[0] === 0x50 && d1.bytes[1] === 0x4b && d2.name === "EXPORT ME.dn2pst" && d2.bytes[0] === 0x50 &&
    d3.name === "EXPORT ME.dn2snd" && same(d3.bytes, pay) && d3.bytes[0] === 0xac,
    `EXPORT: .dnsnd and .dn2pst ZIPs, .dn2snd the stored container (${d3.bytes.length} B: fizzToDn2pst's payload)`);
  const c1 = carryOf(d1.notes[0]), c2 = carryOf(d2.notes[0]);
  const has = (c, what, how) => c.rows.some((r) => r.what.startsWith(what) && r.how === how);
  check(has(c1, "LFO 3", "dropped") && has(c1, "BIT REDUCTION", "dropped") && has(c1, "AMP AHD", "as ADSR") && c1.bar < 10 && c1.lost >= 2 &&
    !has(c2, "LFO 3", "dropped") && c2.bar > c1.bar, `EXPORT's carry rows: Digitone ${c1.bar}/10 (${c1.rows.map((r) => `${r.what} ${r.how}`).join("; ")}), Digitone II ${c2.bar}/10`);
  check(noteRow("RATIO A 3.75: the Digitone has no such step: 3.5").how === "nearest step" && noteRow("filter DEC 127: the Digitone's 127 is INF: written as 126").how === "written as 126",
    "EXPORT: the notes' verdicts (nearest step, written as ..)");
  const many = exportFiles([lib, { ...lib, name: "EXPORT ME" }], "dnsnd");
  check(/\.zip$/.test(many.name) && many.notes.length === 2, `EXPORT: several: one ZIP (${many.name})`);
  lk.close(); u.dispose();
}

// ---- Phase 2-B: the live editor's commands
{
  const u = await sim();
  const lk = await findFizz(u.access);
  const f0 = await lk.dumpTrack(3), t3 = soundFromFrame(f0);
  check(t3 && t3.track === 2 && t3.extras && typeof t3.extras.chan === "number" && t3.extras.sel === 0 && !t3.extras.selected,
    `a track's DUMP: track 3 "${t3 && t3.name}", preset ${t3 && t3.extras.id}, channel ${t3 && t3.extras.chan + 1}, the selected track ${t3 && t3.extras.sel + 1}`);
  const fs0 = await lk.dumpTrack(0);
  check(soundFromFrame(fs0).track === 0 && soundFromFrame(fs0).extras.selected, "a track's DUMP, slot 0: the selected one (track 1)");
  let r = await lk.param(2, Q.FREQ, 4321);
  const t3b = soundFromFrame(await lk.dumpTrack(3));
  check(r.status === 0 && r.a === 3 && r.b === Q.FREQ && t3b.values[Q.FREQ] === 4321 && t3b.extras.edited, "PARAM with its ACK: track 3's FREQ 43.21, the track marked edited");
  r = await lk.param(2, Q.FDBK, 500);
  check(r.status === 0 && soundFromFrame(await lk.dumpTrack(3)).values[Q.FDBK] === 127, "PARAM: a value past its range clamped (FDBK 127)");
  r = await lk.param(6, Q.FREQ, 1);
  check(r.status === 11, `PARAM onto track 7: ${r.statusName}`);
  u.tapPlay();
  r = await lk.param(2, Q.RESO, 2000);
  check(r.status === 0 && soundFromFrame(await lk.dumpTrack(3)).values[Q.RESO] === 2000, "PARAM while playing: taken");
  u.tapPlay();
  // AUDITION: nothing stored; then the track's own DUMP back: the same DUMP again
  const before = await lk.dumpTrack(3), n0 = (await lk.list()).length;
  const other = soundFromFrame(await lk.dump("factory", 7));
  const x = await lk.store(storeFrameOf(other, { track: 2, audition: true, extras: { id: 7, sound: 0, edited: false } }));
  const aud = soundFromFrame(await lk.dumpTrack(3));
  check(x.status === 0 && x.slot === 0 && (await lk.list()).length === n0 && aud.name === other.name && aud.extras.id === 7 && !aud.extras.edited &&
    aud.values.every((v, i) => v === other.values[i]), `AUDITION: track 3 plays ${other.name} (preset 7), nothing stored (${n0} presets)`);
  const y = await lk.store(trackFrame(before));
  const after = await lk.dumpTrack(3);
  check(y.status === 0 && same(Array.from(after), Array.from(before)), "AUDITION of the track's own DUMP: its DUMP the same again, byte for byte (CANCEL)");
  // OWN; a STORE over a used place: BUSY while playing, else a job (its reply when done)
  const own = await lk.store(storeFrameOf({ ...other, name: "OWN ONE" }, { own: true }));
  let rows = await lk.list();
  const ownRow = rows.find((q) => q.source === "mine" && q.slot === own.slot);
  check(own.status === 0 && ownRow && !ownRow.imported, `STORE6 OWN: MY PRESETS ${own.slot} not marked imported`);
  u.tapPlay();
  const busy = await lk.store(storeFrameOf({ ...other, name: "OVER" }, { slot: own.slot, own: true }));
  u.tapPlay();
  const over = await lk.store(storeFrameOf({ ...other, name: "OVER" }, { slot: own.slot, own: true }));
  rows = await lk.list();
  check(busy.status === 8 && over.status === 0 && over.slot === own.slot && rows.find((q) => q.source === "mine" && q.slot === own.slot).name === "OVER" &&
    rows.find((q) => q.source === "mine" && q.slot === own.slot).id === ownRow.id, `a STORE over a used place: BUSY while playing; stopped: the job, OK, the id kept (${ownRow.id})`);
  lk.close(); u.dispose();
}

// ---- FIZZ MIDI (e): a MIDI track's sound through the Manager's model, the wasm FIZZ's SysEx and the files
{
  const u = await sim();
  const lk = await findFizz(u.access);
  // made by the core's rules (qd_msnd_init: every slot untouched, CC 16 + k), two slots turned, then STORE6 into MY PRESETS
  const m0 = newMidi();
  m0.slots[0] = { cc: 16, value: 64, touched: true };
  m0.slots[9] = { cc: 74, value: 100, touched: true };
  m0.slots[27] = { cc: 43, value: 12, touched: true };
  const mb = midiBlob(m0);
  const st = await lk.store(storeSound({ name: "MIDI SYNTH", blob: mb, tags: 0 }));
  const f = st && st.status === 0 ? await lk.dump("mine", st.slot) : null, d = f && parseDump(f), s = f && soundFromFrame(f);
  check(st.status === 0 && d.six && isMidiBlob(d.blob) && same(Array.from(d.blob), Array.from(mb)) && s.midi && s.midi.slots.length === 28 &&
    s.midi.slots[9].cc === 74 && s.midi.slots[9].value === 100 && s.midi.slots[9].touched && !s.midi.slots[1].touched && s.midi.slots[1].cc === 17,
    `MIDI: a MIDI sound STORE6d (MY PRESETS ${st.slot}) DUMPs as its own blob, byte for byte; the editor reads 28 slots (CC 74 = 100, CC 17 untouched)`);
  check(same(Array.from(blob6(s)), Array.from(d.blob)) && same(Array.from(libSound(s).blob), Array.from(d.blob)) &&
    same(Array.from(soundFromStoreFrame(storeFrameOf(s, { slot: st.slot })).blob), Array.from(d.blob)),
    "MIDI: unedited, blob6 / libSound / storeFrameOf write the DUMP's blob back exactly");
  // edited: a value, a CC number, a slot untouched; STORE over its place (OWN), DUMPed back the same, still MIDI
  const e = copySound(s);
  e.midi.slots[3] = { cc: 19, value: 77, touched: true };
  e.midi.slots[9] = { ...e.midi.slots[9], cc: 1 };
  e.midi.slots[27] = { cc: 43, value: 0, touched: false };
  check(!sameValues(e, s) && sameValues(copySound(e), e) && s.midi.slots[3].value === 0 && s.midi.slots[9].cc === 74, "MIDI: copySound copies the slots (the original untouched), sameValues sees the edits");
  const st2 = await lk.store(storeFrameOf(e, { slot: st.slot, own: true }));
  const e2 = st2.status === 0 ? soundFromFrame(await lk.dump("mine", st.slot)) : null;
  check(st2.status === 0 && e2 && e2.midi && sameValues(e2, e) && e2.midi.slots[3].value === 77 && e2.midi.slots[9].cc === 1 && !e2.midi.slots[27].touched &&
    same(Array.from(parseDump(await lk.dump("mine", st.slot)).blob), Array.from(blob6(e))),
    "MIDI: edited (a value, a CC number, a slot untouched), STOREd and DUMPed back the same, still MIDI");
  // on a track: LOAD; PARAM takes a slot's value (its value id), not its CC number (NOT FOUND); AUDITION the whole sound
  const ld = await lk.store(storeFrameOf(e2, { slot: st.slot, track: 1, own: true }));
  const t2 = soundFromFrame(await lk.dumpTrack(2));
  const pv = await lk.param(1, Q.FDBK, 99), pc = await lk.param(1, Q.AMOD, 75), t2b = soundFromFrame(await lk.dumpTrack(2));
  check(ld.status === 0 && t2.midi && sameValues(t2, e2) && pv.status === 0 && t2b.midi.slots[0].value === 99 && pc.status === 9 && t2b.midi.slots[0].cc === 16,
    `MIDI on track 2 (LOAD): its DUMP MIDI; PARAM slot 1's value: OK (99), its CC number: ${pc.statusName} (the editor sends the whole sound)`);
  const a = copySound(t2b);
  a.midi.slots[4] = { cc: 99, value: 5, touched: true };
  const ax = await lk.store(storeFrameOf(a, { track: 1, audition: true, extras: { id: t2b.extras.id, sound: 0, edited: true } }));
  const t2c = soundFromFrame(await lk.dumpTrack(2));
  check(ax.status === 0 && t2c.midi && sameValues(t2c, a) && t2c.extras.edited, "MIDI on a track: an edit as an AUDITION (the whole sound): the track has it, marked edited");
  // the files: .fizzsnd, .fizzlib (with an FM TONE sound), .fizzbak; IMPORT keeps them MIDI
  const fm = soundFromFrame(await lk.dump("factory", 3));
  const libM = libSound(e2, { fizz: "test" }), libF = libSound(fm, { fizz: "test" });
  const fz = exportFiles([libM], "fizz"), back = soundFromLib(parseFizzsnd(fz.bytes, fz.name));
  check(back.midi && sameValues(back, e2) && same(Array.from(blob6(back)), Array.from(blob6(e2))), `MIDI: .fizzsnd round trip (${fz.name}): still MIDI, the same slots`);
  const fl = exportFiles([libF, libM], "fizz"), bak = makeFizzbak({ kind: "presets", presets: [{ ...libM, name: "MIDI BAK" }], fizz: "test" });
  const items = await readImport([{ name: fl.name, bytes: fl.bytes }, { name: "m.fizzbak", bytes: bak }]);
  const sent = [];
  for (const it of items) sent.push(await lk.store(storeFrame(it.sound, { batch: sent.length > 0 })));
  const got = await Promise.all(sent.map(async (r) => soundFromFrame(await lk.dump("mine", r.slot))));
  check(items.length === 3 && items.every((x) => x.ok) && !isMidiBlob(items[0].sound.blob) && isMidiBlob(items[1].sound.blob) && isMidiBlob(items[2].sound.blob) &&
    sent.every((r) => r.status === 0) && !got[0].midi && got[1].midi && sameValues(got[1], e2) && got[2].midi && got[2].name === "MIDI BAK",
    "MIDI: IMPORT of a .fizzlib (FM TONE + MIDI) and a .fizzbak: the MIDI sounds stored as MIDI sounds");
  // EXPORT to the Digitones: a MIDI sound left out (alone: refused)
  let why = "";
  try { exportFiles([libM], "dnsnd"); } catch (x) { why = x.message; }
  let why2 = "";
  try { exportFiles([libM], "dn2pst"); } catch (x) { why2 = x.message; }
  const mix = exportFiles([libF, libM], "dnsnd");
  check(why === MIDI_ONLY && why2 === MIDI_ONLY && mix.skipped.length === 1 && mix.skipped[0] === libM.name && mix.notes.length === 1 && /\.dnsnd$/.test(mix.name),
    `MIDI: the Digitone exports refuse a MIDI sound ("${why}"); with an FM TONE sound: only that one written (${mix.name})`);
  // an FM TONE sound: no midi, its values as before; never the same as a MIDI sound
  const fm2 = soundFromFrame(await lk.dump("factory", 3));
  check(!fm.midi && sameValues(fm, fm2) && !sameValues(fm, e2) && !sameValues(e2, fm) && !isMidiBlob(blob6(fm)) && !copySound(fm).midi,
    "MIDI: an FM TONE sound is read and written as before (no midi; never the same as a MIDI sound)");
  // the lists: LIBRARY / POOL's cassettes carry a MIDI chip for a MIDI preset only
  const rowsM = await lk.list({ factory: true }), frames = new Map();
  for (const r of rowsM.filter((q) => q.source === "mine")) frames.set(r.slot, await lk.dump("mine", r.slot));
  const html = listHtml(rowsM, newFilter(), { frameOf: (r) => (r.source === "mine" ? frames.get(r.slot) : null), places: poolPlaces(rowsM), on: () => false });
  const chips = (html.match(/class="mchip"/g) || []).length, nMidi = [...frames.values()].filter((x) => isMidiBlob(parseDump(x).blob)).length;
  check(chips === nMidi && nMidi === 3, `the lists: a MIDI chip on each MIDI preset (${chips} of ${frames.size} MY PRESETS)`);
  lk.close(); u.dispose();
}

// ---- the owner's fixes (2026-10-25): BROWSE (LIBRARY's LOAD ON FM-1), the selected track followed, POOL REF read
// back at once, IMPORT of FIZZ's own files with their duplicates
{
  const u = await sim();
  for (let k = 0; k < 3; k++) { u.engine.midiIn(storeSound({ name: NAMES[k], blob: blob(k + 1), tags: 1 << k })); u.run(80); }
  settle(u);
  const lk = await findFizz(u.access);
  const trackOf = async (slot) => soundFromFrame(await lk.dumpTrack(slot));
  // the unit's own panel: TRACKS + a white key selects a track; a track's DUMP (slot 0) and STATUS follow
  u.selectTrack(2);
  const s2 = await trackOf(0), st2 = await lk.status();
  check(s2.track === 2 && s2.extras.sel === 2 && st2.sel === 2, `the unit selects track 3 on its panel: the selected track's DUMP is track ${s2.track + 1}, STATUS sel ${st2.sel + 1} (EDIT follows it)`);
  u.selectTrack(0);
  check((await trackOf(0)).track === 0, "and back to track 1");
  // BROWSE: the unit's browser on track 4 at MY PRESETS 2 (its id); OCT- cancels, OCT+ loads
  const rows0 = await lk.list({ factory: true });
  const p2 = rows0.find((q) => q.source === "mine" && q.slot === 2), f5 = rows0.find((q) => q.source === "factory" && q.slot === 5);
  const own4 = await lk.dumpTrack(4);
  const b1 = await lk.browse(3, p2.id), st = await lk.status(), t4 = await trackOf(4);
  check(b1.status === 0 && b1.a === 4 && st.browse && st.sel === 3 && t4.extras.browse && t4.extras.id === p2.id && t4.name === p2.name,
    `BROWSE track 4, ${p2.name} (id ${p2.id}): ACK OK, the unit's browser open (STATUS), track 4 selected and previewing it`);
  u.press("OCT-");
  const st3 = await lk.status(), back4 = await lk.dumpTrack(4);
  check(!st3.browse && same(Array.from(back4).slice(0, -10), Array.from(own4).slice(0, -10)),
    "OCT- on the unit: the browser closed, track 4 its own sound again (its DUMP the same, the selection apart)");
  const b2 = await lk.browse("selected", f5.id);
  u.press("OCT+");
  const st4 = await lk.status(), t4b = await trackOf(4);
  check(b2.status === 0 && b2.a === 4 && !st4.browse && t4b.extras.id === f5.id && t4b.name === f5.name && !t4b.extras.browse,
    `BROWSE the selected track (4), FACTORY 5 ${f5.name}; OCT+ on the unit: loaded (the track plays preset ${t4b.extras.id})`);
  const nf = await lk.browse(0, 4000), bt = await lk.browse(6, f5.id);
  check(nf.status === 9 && bt.status === 11 && !(await lk.status()).browse, "BROWSE: NOT FOUND for no such preset, BAD TRACK for track 7; nothing opened");
  // POOL REF: the next LIST has it (the page shows it at once, then reads back until the unit agrees)
  const pr = await lk.poolRef(0, p2.id), afterRef = await lk.list({ pool: true });
  check(pr.status === 0 && afterRef.some((q) => q.source === "pool" && q.slot === pr.a && q.id === p2.id), `POOL REF: pool ${pr.a} in the very next LIST`);
  // IMPORT: a .fizzlib of MY PRESETS (as EXPORT ALL writes it), a .fizzsnd twice, a .fizzbak's presets
  const mineSnd = (await lk.dumpAll("mine", 3)).map((f) => ({ ...soundFromStoreFrame(f), source: { fizz: ver } }));
  const lib = exportFiles(mineSnd, "fizz");
  const extra = { ...mineSnd[0], name: "NEW ONE", blob: blob(6) };
  const one = exportFiles([extra], "fizz");
  const bak = makeFizzbak({ kind: "presets", presets: [mineSnd[1], { ...extra, name: "FROM BAK", blob: blob(7) }], fizz: ver, web: { tags: { [soundKey({ ...extra, name: "FROM BAK", blob: blob(7) })]: ["DUSTY"] } } });
  const items = await readImport([{ name: lib.name, bytes: lib.bytes }, { name: one.name, bytes: one.bytes }, { name: "again.fizzsnd", bytes: one.bytes }, { name: "old.fizzbak", bytes: bak }]);
  markDuplicates(items, mineSnd);
  const dups = items.map((x) => `${x.sound ? x.sound.name : x.file}:${x.dup || "new"}`);
  check(/\.fizzlib$/.test(lib.name) && items.length === 7 && items.every((x) => x.ok) && dups.join() ===
    "GLASS BELL:mine,SUB DROP:mine,NIGHT PAD:mine,NEW ONE:new,NEW ONE:list,SUB DROP:mine,FROM BAK:new" && items[6].web && items[6].web[0] === "DUSTY",
    `IMPORT reads FIZZ's files: ${lib.name} (3, all already in MY PRESETS), a .fizzsnd twice (the second flagged), a .fizzbak's presets (its web tags): ${dups.join(", ")}`);
  const sent = [];
  for (const it of items.filter((x) => !x.dup)) sent.push(await lk.store(storeFrame(it.sound, { batch: sent.length > 0 })));
  const rowsI = await lk.list();
  check(sent.every((r) => r.status === 0) && ["NEW ONE", "FROM BAK"].every((n) => mine(rowsI).filter((q) => q.name === n).length === 1) && mine(rowsI).length === 5,
    "IMPORT: only the new ones sent: MY PRESETS 5, each once");
  lk.close(); u.dispose();
}

// ---- PROJECTS (the owner's requests, 2026-10-26): the store written in the browser (fizzstore.js), patterns
// between projects and a project kept and brought back (projects.js), checked by the wasm FIZZ's own replies; a backup
// as the browser FIZZ's flash (js/backup-play.js); LIBRARY's pool filter
{
  const L = new Float32Array(128), R = new Float32Array(128);
  const fizz = async (flash = null) => {
    const e = await createEngine({ sampleRate: 44100, files: { wasm: module }, flash, options: {} });
    const run = (ms) => { const end = e.deviceMs() + ms; while (e.deviceMs() < end) e.render(L, R); };
    const replies = () => { const out = [], b = e.midiOut(); let sx = null; for (const x of b) { if (x === 0xf0) sx = [x]; else if (sx) { sx.push(x); if (x === 0xf7) { out.push(parseReply(sx)); sx = null; } } } return out; };
    const ask = (f, n = 800) => { e.midiIn(f); const got = []; for (let i = 0; i < n; i++) { run(5); got.push(...replies().filter(Boolean)); if (got.length && got[got.length - 1].last !== false) return got; } return got; };
    const t = e.info().panelTable, BT = { HOME: 6 };
    let keys = 0, buttons = 0;
    const set = () => e.setPanel({ keys: keys >>> 0, buttons: buttons >>> 0, enc: new Int32Array(7), keyVel: new Uint8Array(27) });
    const tapB = (n) => { buttons |= 1 << t.btnId[BT[n]]; set(); run(80); buttons = 0; set(); run(60); };
    const tapK = (k) => { keys |= 1 << k; set(); run(80); keys = 0; set(); run(60); };
    run(1500);
    const open = (p) => { ask(projectRequest({ op: "load", p: p + 1 })); run(600); };
    const byId = () => {
      const pool = ask(listRequest({ pool: true })).flatMap((r) => r.entries || []).filter((x) => x.source === "pool");
      const m = {};
      for (const x of pool) { const u = ask(usersRequest({ source: "pool", slot: x.slot })).find((y) => y.kind === "users"); m[x.name] = (m[x.name] || 0) + (u ? u.projects.reduce((a, y) => a + y.steps, 0) : 0); }
      for (const k of Object.keys(m)) if (!m[k]) delete m[k];
      return m;
    };
    const dev = () => convertImage(e.flash().bytes, "device");
    return { e, run, ask, tapB, tapK, open, byId, dev };
  };
  const eq = (a, b) => Object.keys({ ...a, ...b }).every((k) => (a[k] || 0) === (b[k] || 0));
  // the writer: an image's records written again (both layouts) read back the same; too many for the region refused
  const A = await fizz();
  A.tapB("HOME"); A.tapK(2); A.tapK(4);                       // the demo's A01 edited: its record, with the demo's sound locks
  A.ask(storeSound({ name: "KEPT ONE", blob: blob(5), track: 0 }));   // a MY preset loaded on track 1 (a 'P' 6 reference)
  A.run(2500);
  A.ask(projectRequest({ op: "new", p: 2 })); A.run(800);
  A.ask(projectRequest({ op: "new", p: 3 })); A.run(800);
  A.ask(projectRequest({ op: "load", p: 1 })); A.run(2500);
  const img = A.dev(), rr = regionRecords(img);
  const back = regionRecords(buildImage("device", rr)), host = regionRecords(convertImage(img, "host"));
  const sameRecs = (x, y) => x.length === y.length && x.every((r, i) => r.key === y[i].key && same(r.bytes, y[i].bytes));
  check(sameRecs(rr.lib, back.lib) && sameRecs(rr.prj, back.prj) && sameRecs(rr.prj, host.prj) && sameRecs(rr.lib, host.lib),
    `the store written again: ${rr.lib.length} + ${rr.prj.length} records the same, packed and in the browser FIZZ's layout`);
  let full = null;
  try { writeStore(new Uint8Array(LAYOUTS.device.size), 0, 8, Array.from({ length: 40 }, (_, i) => ({ key: i + 1, bytes: new Uint8Array(240) }))); } catch (x) { full = x; }
  check(full && full.code === "full", `a store with too little room refused (${full && full.message}): the GC's ${RESERVE} blank sectors kept`);
  // the patterns: A01 pasted into the new projects 2 (at B01) and 3 (at C05); FIZZ counts the same steps a sound in both
  const m = modelOf(img), ps = projects(m);
  const k0 = [...m.prj.keys()].filter((k) => k >>> 24 === RK.PAT && ((k >>> 16) & 0xFF) === 0).map((k) => k & 0xFFFF)[0] ?? -1;   // (the pattern edited)
  const w = walkPat(m.prj.get(rkey(RK.PAT, 0, k0)));
  check(ps[0].there && ps[0].demo && ps[1].there && ps[2].there && w && w.trigs > 0 && ps.every((x) => x.pats.every((s) => !s || !s.bad)),
    `the projects read: ${ps.filter((x) => x.there).map((x) => `${x.name}${x.demo ? " (demo)" : ""} ${x.used} patterns ${x.pool} sounds`).join(", ")}; the edited ${k0 >= 0 ? patName(k0) : "?"} ${w ? w.trigs : 0} trigs, 'P' ${w ? w.v : "?"}`);
  const clip = copyPatterns(m, 0, [0, 1, 2]);                 // (the demo's others, compiled in: nothing to copy)
  const r2 = pastePatterns(m, 1, 16, clip), r3 = pastePatterns(m, 2, 36, clip);
  check(clip.pats.length === 1 && clip.pool.size > 0 && r2.poolAdded === clip.pool.size && same(r2.at, [16]) && same(r3.at, [36]),
    `COPY ${clip.names.join(", ")} (pool places ${[...clip.pool.keys()].join(", ")}; presets ${[...clip.presets.keys()].join(", ") || "none"}) PASTE onto 2 B01 (+${r2.poolAdded} pool), 3 C05 (+${r3.poolAdded})`);
  const fizzpat = await readFizzpat(makeFizzpat(clip, { fizz: "test" }), "t.fizzpat");
  const m4 = modelOf(img); pastePatterns(m4, 1, 16, fizzpat);
  check(same(m4.prj.get(rkey(RK.PAT, 1, 16)), m.prj.get(rkey(RK.PAT, 1, 16))), `a .fizzpat read back pastes the same record (${patFileName(clip)})`);
  const B = await fizz(convertImage(imageOf(m), "host"));
  B.open(1); const u2 = B.byId(); B.open(2); const u3 = B.byId();
  check(Object.keys(u2).length > 0 && eq(u2, u3), `FIZZ plays the pasted pattern the same in both: steps a sound ${JSON.stringify(u2)}`);
  let refused = null;
  try { clearPatterns(m, 0, [k0 ? 0 : 1]); } catch (x) { refused = x; }
  clearPatterns(m, 1, [16]);
  check(refused && refused.code === "demo" && !m.prj.has(rkey(RK.PAT, 1, 16)) && !project(m, 1).pats[16], "CLEAR: 2 B01 gone; a demo pattern refused");
  // a project kept, written to a .fizzprj, read back and loaded into slot 8 of a FIZZ that has none of its presets
  const kept = keepProject(modelOf(img), 0);
  const prj = await readFizzprj(makeFizzprj(kept, { fizz: "test" }), prjFileName(kept));
  renameKept(prj, "brought back");
  const C = await fizz();
  const mc = modelOf(C.dev()), lr = loadProject(mc, 7, prj);
  const D = await fizz(convertImage(imageOf(mc), "host"));
  const pl = D.ask(projectsRequest()).find((x) => x.kind === "projects");
  A.open(0); const a0 = A.byId();
  D.open(7); const d7 = D.byId();
  const lrec = mc.prj.get(rkey(RK.PAT, 7, k0)), lw = walkPat(lrec);   // (its track 1 names the preset by its new id)
  const ref = lw && lw.refAt.length ? lrec[lw.refAt[0]] | (lrec[lw.refAt[0] + 1] << 8) : 0;
  const refName = (readRegion(imageOf(mc)).presets.find((x) => x.id === ref) || {}).name;
  check(kept.presets.size === 1 && lr.presetsAdded === 1 && pl && pl.list[7].there && pl.list[7].name === "BROUGHT BACK" && project(mc, 7).demo,
    `KEEP ${kept.name} (${kept.recs.length} records, ${kept.presets.size} MY preset), .fizzprj read back, loaded into slot 8 of another FIZZ as ${pl ? JSON.stringify(pl.list[7]) : "?"}: its preset added (${lr.presetsAdded})`);
  check(Object.keys(a0).length > 0 && eq(a0, d7) && refName === "KEPT ONE",
    `FIZZ plays it there as here: steps a sound ${JSON.stringify(d7)}${eq(a0, d7) ? "" : ` (here ${JSON.stringify(a0)})`}; track 1 names ${refName} (id ${ref})`);
  // a backup as the browser FIZZ's flash
  const bak = makeFizzbak({ kind: "full", image: img, fizz: "test" });
  const bf = await backupFlash(bak, "FIZZ backup 2026-10-26 2140.fizzbak");
  const E = await fizz(bf.flash);
  const pe = E.ask(projectsRequest()).find((x) => x.kind === "projects");
  let pre = null;
  try { await backupFlash(makeFizzbak({ kind: "presets", presets: [] }), "p.fizzbak"); } catch (x) { pre = x; }
  check(bf.flash.length === 1 << 20 && bf.name === "FIZZ backup 2026-10-26 2140" && pe && same(pe.list.filter((x) => x.there).map((x) => x.name), readRegion(img).projects.map((x) => x.name)) &&
    pre && isBackupFile(bak) && isBackupFile(img), `a .fizzbak as the browser FIZZ's flash: it boots with ${pe ? pe.list.filter((x) => x.there).map((x) => x.name).join(", ") : "?"}; a presets-only one refused`);
  // LIBRARY's filter: IN POOL and NOT IN POOL split the list
  const rows = [...A.ask(listRequest({ factory: true, pool: true })).flatMap((r) => r.entries || [])];
  const f = newFilter(), all = shownRows(rows, f);
  f.pool = "in"; const pin = shownRows(rows, f); f.pool = "out"; const pout = shownRows(rows, f);
  const n = (x) => x.mine.length + x.fac.length;
  check(n(pin) > 0 && n(pout) > 0 && n(pin) + n(pout) === n(all) && pin.fac.every((r) => poolPlaces(rows).has(r.id)), `IN POOL ${n(pin)} + NOT IN POOL ${n(pout)} = ${n(all)}`);
  for (const u of [A, B, C, D, E]) u.e.dispose();
  // the owner's dumps (FIZZ_SAVES or ../FIZZ-saves; skipped when absent): every pattern record read
  const dir = process.env.FIZZ_SAVES || path.resolve(SITE, "../../FIZZ-saves");
  const own = fs.existsSync(dir) ? fs.readdirSync(dir).filter((x) => x.endsWith("-store.bin")).sort().slice(-3) : [];
  for (const x of own) {
    const mo = modelOf(new Uint8Array(fs.readFileSync(path.join(dir, x)))), po = projects(mo).filter((y) => y.there);
    check(po.every((y) => y.pats.every((s) => !s || !s.bad)), `${x}: ${po.map((y) => `${y.name} ${y.used}/${y.pool}`).join(", ")}, every pattern read`);
  }
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
