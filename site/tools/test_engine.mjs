// SPDX-License-Identifier: GPL-3.0-only
// FIZZ's wasm in Node, through engine/engine.js exactly as the page's AudioWorklet drives it:
//
//   node site/tools/test_engine.mjs            (after site/build_wasm.sh)
//
//   1. boot from an erased flash; info() filled; the first screens (PNGs in site/.build/test/)
//   2. sound: REC tapped (the keybed becomes the selected track's keyboard), D4 held through setPanel: not silent
//   3. the sequencer: PLAY tapped on the power-on pattern, 4 s rendered (reported)
//   4. the flash: a step added (W1 tapped in the step view), FIZZ's autosave (1.5 s after the last change) moves
//      flash().seq; a second unit booted from flash().bytes shows the step, a fresh unit does not
//   5. the real-time factor: 10 s at 48 kHz (through the resampler) with the sequencer playing
//   6. FIZZ's SysEx (docs/SYSEX.md) through engine.midiIn / midiOut as raw bytes, as the page sends it: IDENTITY, three
//      Digitone sounds (--dnsnd DIR, the owner's; never copied here) stored in the LIBRARY (slots 1..3, a batch), the
//      first loaded onto track 2; after FIZZ's autosave a unit booted from the flash puts the next sound in slot 4
// Adapted from RemoteFM1's web/engines/native/test.mjs (GPL-3.0-only).
import fs from "node:fs";
import path from "node:path";
import zlib from "node:zlib";
import { fileURLToPath } from "node:url";
import { createEngine } from "../engine/engine.js";
import { identity, parseReply, parseDump, restoreFrame } from "../js/fizz_sysex.js";
import { encodeRequest as labelsReq, decodeLabels } from "../js/labels.js";
import { listRequest, parseList, dumpRequest } from "../js/fizz_sysex.js";
import { readDnsnd, readFiles, storeFrame } from "../js/digitone.js";
import { fizzTagMask } from "../js/fizz_sysex.js";
import { makeFizzlib, parseFizzlib, soundFromStoreFrame } from "../js/library.js";
import { digitoneExport } from "../js/dnexport.js";
import { fizzValues } from "../js/fizz2dn.js";
import { dnReadZip } from "../js/fm1dnsnd.js";
import { execFileSync } from "node:child_process";
import { dn2RecordFromParams, wrapDn2Record, dn2pstZip } from "../js/dn2pst.js";

const HERE = path.dirname(fileURLToPath(import.meta.url));
const SITE = path.resolve(HERE, "..");
const OUT = path.join(SITE, ".build/test");
const W = 240, H = 240;
const BTN = { FX: 0, SEL: 1, ENV: 2, LFO: 3, EDIT: 4, GLO: 5, HOME: 6, SAVE: 7, ARP: 8, SEQ: 9, PLAY: 10, REC: 11, "OCT-": 12, "OCT+": 13 };

function crc32(b) {
    let c = -1;
    for (let i = 0; i < b.length; i++) { c ^= b[i]; for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1; }
    return (c ^ -1) >>> 0;
}
function chunk(type, data) {
    const t = Buffer.from(type, "ascii"), len = Buffer.alloc(4), crc = Buffer.alloc(4);
    len.writeUInt32BE(data.length);
    crc.writeUInt32BE(crc32(Buffer.concat([t, data])));
    return Buffer.concat([len, t, data, crc]);
}
export function writePng(file, px) {
    const raw = Buffer.alloc((W * 3 + 1) * H);
    for (let y = 0; y < H; y++) for (let x = 0; x < W; x++) {
        const c = px[y * W + x], o = y * (W * 3 + 1) + 1 + 3 * x;
        raw[o] = Math.floor(((c >> 11) & 31) * 255 / 31);
        raw[o + 1] = Math.floor(((c >> 5) & 63) * 255 / 63);
        raw[o + 2] = Math.floor((c & 31) * 255 / 31);
    }
    const ihdr = Buffer.alloc(13);
    ihdr.writeUInt32BE(W, 0); ihdr.writeUInt32BE(H, 4); ihdr[8] = 8; ihdr[9] = 2;
    fs.writeFileSync(file, Buffer.concat([Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]), chunk("IHDR", ihdr),
                                          chunk("IDAT", zlib.deflateSync(raw)), chunk("IEND", Buffer.alloc(0))]));
}

// the page's side: held keys and buttons (button labels -> matrix ids through the core's own panel table)
class Panel {
    constructor(e) { this.e = e; this.t = e.info().panelTable; this.keys = 0; this.buttons = 0; }
    bit(name) { return 1 << this.t.btnId[BTN[name]]; }
    set(vel = null) { this.e.setPanel({ keys: this.keys >>> 0, buttons: this.buttons >>> 0, enc: new Int32Array(7), keyVel: vel || new Uint8Array(27) }); }
    btn(name, down) { this.buttons = down ? this.buttons | this.bit(name) : this.buttons & ~this.bit(name); this.set(); }
    key(k, down) { this.keys = down ? this.keys | (1 << k) : this.keys & ~(1 << k); this.set(); }
}
const L = new Float32Array(128), R = new Float32Array(128);
const peakOf = () => { let p = 0; for (let i = 0; i < 128; i++) p = Math.max(p, Math.abs(L[i]), Math.abs(R[i])); return p; };
function run(e, ms) { let p = 0; const end = e.deviceMs() + ms; while (e.deviceMs() < end) { e.render(L, R); p = Math.max(p, peakOf()); } return p; }
function tapBtn(e, p, name, ms = 80) { p.btn(name, true); run(e, ms); p.btn(name, false); run(e, 60); }
function tapKey(e, p, k, ms = 80) { p.key(k, true); run(e, ms); p.key(k, false); run(e, 60); }
const blank = (px) => px.every((v) => v === px[0]);
const diffPx = (a, b) => { let n = 0; for (let i = 0; i < a.length; i++) if (a[i] !== b[i]) n++; return n; };

const wasm = fs.readFileSync(path.join(SITE, "fizz.wasm"));
const module = await WebAssembly.compile(wasm);
const mk = (flash = null, sampleRate = 44100) => createEngine({ sampleRate, files: { wasm: module }, flash, options: {} });
fs.mkdirSync(OUT, { recursive: true });
let fails = 0;
const check = (ok, what) => { console.log(`${ok ? "ok  " : "FAIL"}  ${what}`); if (!ok) fails++; };
const note = (what) => console.log(`      ${what}`);

// 1. boot
let e = await mk();
const info = e.info();
check(info.id === "fizz" && info.name && info.version && info.buttonNames.length === 14, `info: ${info.name} ${info.version} (${info.license}, ${info.family})`);
let px = e.screen().pixels;
while (blank(px) && e.deviceMs() < 3000) { e.runMs(1); px = e.screen().pixels; }
check(!blank(px), `screen drawn at ${e.deviceMs()} ms`);
writePng(path.join(OUT, "boot-0.png"), px);
for (const t of [500, 1500, 3000]) { run(e, t - e.deviceMs()); writePng(path.join(OUT, `boot-${t}.png`), e.screen().pixels); }
e.dispose();

// 2. the keyboard: REC tapped, D4 held
e = await mk();
let p = new Panel(e);
run(e, 1000);
tapBtn(e, p, "HOME");                     // (wakes it from the logo screen to the step view)
tapBtn(e, p, "REC");
const quiet = run(e, 300);
const vel = new Uint8Array(27); vel[9] = 100;
p.keys = 1 << 9; p.set(vel);
const loud = run(e, 500);
p.keys = 0; p.set();
run(e, 300);
writePng(path.join(OUT, "keyboard.png"), e.screen().pixels);
check(loud > 64 / 32768, `REC + D4 held: peak ${Math.round(loud * 32768)} (before the key ${Math.round(quiet * 32768)})`);
e.dispose();

// 3. the sequencer: PLAY on the power-on pattern
e = await mk();
p = new Panel(e);
run(e, 500);
tapBtn(e, p, "HOME");
tapBtn(e, p, "PLAY");
const seqPeak = run(e, 4000);
writePng(path.join(OUT, "playing.png"), e.screen().pixels);
note(`PLAY on the power-on pattern: 4 s, peak ${Math.round(seqPeak * 32768)}`);
e.dispose();

// 4. the flash round trip
e = await mk();
p = new Panel(e);
run(e, 500);
tapBtn(e, p, "HOME");
run(e, 300);
const s0 = e.flash().seq;
tapKey(e, p, 2);                          // W2 (G3): a step
tapKey(e, p, 4);                          // W3 (A3)
run(e, 3000);                             // (FIZZ saves 1.5 s after the last change)
const s1 = e.flash().seq;
const bytes = e.flash().bytes;
const edited = e.screen().pixels.slice();
writePng(path.join(OUT, "flash-edited.png"), edited);
e.dispose();
check(s1 !== s0 && bytes && bytes.length === 1 << 20, `the edit moved flash().seq (${s0} -> ${s1}); image ${bytes ? bytes.length : 0} bytes`);
const restored = await mk(bytes);
p = new Panel(restored);
run(restored, 500);
tapBtn(restored, p, "HOME");
run(restored, 300);
const back = restored.screen().pixels.slice();
writePng(path.join(OUT, "flash-restored.png"), back);
restored.dispose();
const fresh = await mk();
p = new Panel(fresh);
run(fresh, 500);
tapBtn(fresh, p, "HOME");
run(fresh, 300);
const plain = fresh.screen().pixels.slice();
fresh.dispose();
const dRestored = diffPx(back, edited), dFresh = diffPx(plain, edited);
check(dFresh > 0 && dRestored < dFresh, `booted from the image it shows the edit (${dRestored} px differ from the edited unit; a fresh unit: ${dFresh})`);

// 5. the real-time factor at 48 kHz, sequencer playing
e = await mk(null, 48000);
p = new Panel(e);
run(e, 300);
tapBtn(e, p, "HOME");
tapBtn(e, p, "PLAY");
const N = 48000 * 10 / 128;
let pk = 0;
const t0 = performance.now();
for (let q = 0; q < N; q++) { e.render(L, R); pk = Math.max(pk, peakOf()); }
const wall = (performance.now() - t0) / 1000;
check(pk > 64 / 32768, `48 kHz, sequencer playing: 10 s in ${wall.toFixed(2)} s, real-time factor ${(10 / wall).toFixed(1)}x, ${(wall / N * 1000).toFixed(3)} ms per 128-frame quantum (budget 2.667 ms), peak ${Math.round(pk * 32768)}`);
e.dispose();

// 6. the SysEx
{
  const arg = process.argv.indexOf("--dnsnd");
  const DIR = arg > 0 ? process.argv[arg + 1] : "/Volumes/Q7Media-2025/Projects/Github/ChoralRootFM1/DigitonePresets";
  const replies = (eng) => {        // every whole SysEx in the engine's MIDI out since the last call
    const out = [], b = eng.midiOut();
    let sx = null;
    for (const x of b) { if (x === 0xf0) sx = [x]; else if (sx) { sx.push(x); if (x === 0xf7) { out.push(parseReply(sx)); sx = null; } } }
    return out;
  };
  const ask = (eng, frame) => { eng.midiIn(frame); for (let i = 0; i < 40; i++) { run(eng, 5); const r = replies(eng); if (r.length) return r[0]; } return null; };
  e = await mk();
  p = new Panel(e);
  run(e, 500);
  const bn = e.info().buttonNames;
  check(bn.includes("TRACKS") && bn.includes("MIX") && bn.includes("LATCH") && bn.includes("SOUNDS"), `the panel's names: ${bn.join(" ")}`);
  const id = ask(e, identity());
  check(id && id.kind === "identity" && id.version.includes(info.version), `IDENTITY: ${JSON.stringify(id)}`);
  const files = fs.existsSync(DIR) ? fs.readdirSync(DIR).filter((f) => f.endsWith(".dnsnd")).sort().slice(0, 4) : [];
  if (files.length === 4) {
    const sounds = await Promise.all(files.map((f) => readDnsnd(fs.readFileSync(path.join(DIR, f)), f)));
    const got = [];
    for (let i = 0; i < 3; i++) got.push(ask(e, storeFrame(sounds[i], { batch: i > 0, track: i === 0 ? 1 : -1 })));
    check(got.every((r, i) => r && r.kind === "store" && r.status === 0 && !r.pool && r.slot === i + 1),
      `STORE x3 (${sounds.slice(0, 3).map((s) => s.name).join(", ")}): ${got.map((r) => r && `${r.statusName} LIBRARY ${r.slot}`).join(", ")}`);
    run(e, 300);
    writePng(path.join(OUT, "sysex-imported.png"), e.screen().pixels);
    run(e, 3000);
    const img = e.flash().bytes;
    e.dispose();
    const e2 = await mk(img);
    run(e2, 500);
    const r4 = ask(e2, storeFrame(sounds[3]));
    check(r4 && r4.status === 0 && r4.slot === 4, `after a reboot from the flash the library keeps them: the next sound goes to LIBRARY ${r4 && r4.slot}`);
    const rp = ask(e2, storeFrame(sounds[3], { pool: true }));
    check(rp && rp.status === 0 && rp.pool && rp.slot >= 2, `STORE into the POOL: ${rp && `${rp.statusName} POOL ${rp.slot}`}`);
    // an older Digitone file (DN1, OS 1.42, the "0095" payload header; the owner's, read in place): its file notes
    // are a mark, not losses, and it imports
    const OLD = "/Volumes/Q7Media-2025/Projects/Github/ChoralRootFM1/Digitone2Presets/H003 BRITE2.dnsnd";
    if (fs.existsSync(OLD)) {
      const s = await readDnsnd(fs.readFileSync(OLD), path.basename(OLD));
      check(s.notes.length === 0 && s.origin.length === 2 && s.mark === "older Digitone file (OS 1.42)",
        `${path.basename(OLD)}: mark "${s.mark}", ${s.notes.length} lost (${s.notes.join("; ")})`);
      const ro = ask(e2, storeFrame(s));
      check(ro && ro.status === 0, `${path.basename(OLD)} imports: ${ro && `${ro.statusName} LIBRARY ${ro.slot}`}`);
    } else note(`no ${OLD}: the older-file check skipped`);
    // a Digitone II FM TONE preset (the owner's BASSSINE.dn2pst, read in place) and a WAVETONE one (synthetic)
    const DN2 = "/Volumes/Q7Media-2025/Projects/Github/ChoralRootFM1/Digitone2Presets/BASSSINE.dn2pst";
    const wave = dn2RecordFromParams({}, { name: "WAVE TEST" }); wave[244] = 1;
    const waveZip = dn2pstZip(wrapDn2Record(wave), { FormatVersion: 1, ProductType: ["24", "38"], Payload: "WAVE TEST.dn2snd", FileType: "Sound", FirmwareVersion: "1.10D", MetaInfo: { Tags: [] } });
    const list = [{ name: "WAVE TEST.dn2pst", bytes: waveZip }];
    if (fs.existsSync(DN2)) list.unshift({ name: path.basename(DN2), bytes: fs.readFileSync(DN2) });
    const got2 = await readFiles(list);
    const wv = got2[got2.length - 1];
    check(!wv.ok && /WAVETONE sound: FIZZ imports FM TONE sounds only/.test(wv.error), `a WAVETONE .dn2pst is refused: ${wv.error}`);
    if (got2.length === 2) {
      const s = got2[0].sound;
      check(got2[0].ok && s.dn2 && s.blob.length === (s.six ? 128 : 96), `${path.basename(DN2)}: "${s.name}", DRIVE ${s.drive}, mark "${s.mark}", ${s.notes.length} lost or approximated`);
      const r2 = ask(e2, storeFrame(s));
      check(r2 && r2.status === 0, `${path.basename(DN2)} imports: ${r2 && `${r2.statusName} LIBRARY ${r2.slot}`}`);
    } else note(`no ${DN2}: the Digitone II check skipped`);
    // what FIZZ keeps of an import (P1.5x): the preset's pan, sends and tags, read back by FIZZ's own test_sysex --feed
    // (tools/build/test_sysex, built by make -C tools test; the wasm has no read-back command)
    const FEED = process.env.FIZZ_TEST_SYSEX || path.resolve(SITE, "../tools/build/test_sysex");   // (FIZZ_TEST_SYSEX: a build of the same FIZZ)
    const mixed = got.length ? [sounds[0], sounds[1]] : [];
    try { const tx = path.join("/Volumes/Q7Media-2025/Projects/Github/ChoralRootFM1/DigitonePresets", "A036 TX BASS 1 MF.dnsnd"); if (fs.existsSync(tx)) mixed.push(await readDnsnd(fs.readFileSync(tx), path.basename(tx))); } catch (_) {}
    if (got2.length === 2) mixed.push(got2[0].sound);
    if (fs.existsSync(FEED) && mixed.length) {
      const tmp = fs.mkdtempSync(path.join(OUT, "feed-"));
      for (const s of mixed) {
        const f = path.join(tmp, "one.syx");
        fs.writeFileSync(f, storeFrame(s));
        const out = execFileSync(FEED, ["--feed", f], { encoding: "utf8" });
        const mix = ((out.match(/^mix (.*)$/m) || [])[1] || "").split(" ").map(Number), tags = Number((out.match(/^tags (.*)$/m) || [])[1]);
        const want = [s.pan, s.sends.chorus, s.sends.delay, s.sends.reverb], mask = fizzTagMask(s.fizzTags);
        check(want.every((v, i) => v === mix[i]) && (mask ? tags === mask : true),
          `${s.file}: FIZZ keeps pan / chorus / delay / reverb ${mix.join(" ")} (sent ${want.join(" ")}), tags ${tags}${mask ? ` (sent ${mask}: ${s.fizzTags.join(", ")})` : " (none mapped: FIZZ's classifier)"}`);
      }
    } else note(`no ${FEED}: the kept mix and tags not checked (make -C tools test builds it)`);
    // a Digitone 1 sound with drive (CHAPPET DL, DRIVE 44): the Digitone's drive is before its filter, so the import
    // goes as STORE6 with DRIVE ROUTE PRE, which FIZZ keeps (test_sysex --feed)
    if (fs.existsSync(FEED)) {
      const p = path.join("/Volumes/Q7Media-2025/Projects/Github/ChoralRootFM1/DigitonePresets", "A002 CHAPPET DL.dnsnd");
      if (fs.existsSync(p)) {
        const s = await readDnsnd(fs.readFileSync(p), path.basename(p)), f = storeFrame(s);
        const tmpd = fs.mkdtempSync(path.join(OUT, "feedd-"));
        fs.writeFileSync(path.join(tmpd, "one.syx"), f);
        const vals = ((execFileSync(FEED, ["--feed", path.join(tmpd, "one.syx")], { encoding: "utf8" }).match(/^values (.*)$/m) || [])[1] || "").split(" ").map(Number);
        const plain = await readDnsnd(fs.readFileSync(path.join(path.dirname(p), "A001 DIGIT-ONE.dnsnd")), "A001");
        check(s.six && f[5] === 0x11 && vals[73] === 44 && vals[79] === 1 && !plain.six && storeFrame(plain)[5] === 0x10,
          `${s.file}: STORE6, FIZZ keeps DRIVE ${vals[73]} ${vals[79] ? "PRE" : "POST"}; DIGIT-ONE (DRIVE 0) stays a plain STORE`);
      }
    }
    // E1: BASSSINE uses FIZZ's extension (its drive before the filter): it goes as STORE6, and FIZZ keeps AMP MODE /
    // HOLD, PORT / its time and slope, and the drive's routing (values 74..79) and DRIVE (73) as the reader made them
    if (got2.length === 2 && fs.existsSync(FEED)) {
      const s = got2[0].sound, f6 = storeFrame(s);
      const tmp6 = fs.mkdtempSync(path.join(OUT, "feed6-"));
      fs.writeFileSync(path.join(tmp6, "one.syx"), f6);
      const out = execFileSync(FEED, ["--feed", path.join(tmp6, "one.syx")], { encoding: "utf8" });
      const vals = ((out.match(/^values (.*)$/m) || [])[1] || "").split(" ").map(Number);
      const r6 = parseReply(Uint8Array.from((out.match(/^reply(.*)$/m) || ["", ""])[1].trim().split(/\s+/).map((x) => parseInt(x, 16))));
      check(s.six && f6[5] === 0x11 && r6 && r6.status === 0 && [0, 1, 2, 3, 4, 5].every((k) => vals[74 + k] === s.ext[k]) && vals[73] === s.drive,
        `${s.file} as STORE6 (0x${f6[5].toString(16)}): AMP MODE ${vals[74]}, HOLD ${vals[75]}, PORT ${vals[76]} / ${vals[77]} / ${vals[78]}, DRIVE ${vals[73]} ${vals[79] ? "PRE" : "POST"} kept (the reader's ${s.ext.slice(0, 6).join(" ")})`);
    }
    // Digitone export (experimental): FIZZ presets written as .dnsnd / .dn2pst (js/dnexport.js, FIZZ's fizz2dn), read
    // back by the site's own readers: the same FIZZ values (DRIVE, the E values too for .dn2pst), name, tags and sends;
    // the files pass the writer's checks (unzip -t, the readers' warnings: none, the trailer's length field)
    {
      const src = [];
      for (const f of ["A001 DIGIT-ONE.dnsnd", "A002 CHAPPET DL.dnsnd", "A036 TX BASS 1 MF.dnsnd"]) {
        const p = path.join("/Volumes/Q7Media-2025/Projects/Github/ChoralRootFM1/DigitonePresets", f);
        if (fs.existsSync(p)) src.push(soundFromStoreFrame(storeFrame(await readDnsnd(fs.readFileSync(p), f))));
      }
      if (got2.length === 2) src.push(soundFromStoreFrame(storeFrame(got2[0].sound)));
      const unzipOk = (bytes) => { const f = path.join(fs.mkdtempSync(path.join(OUT, "unz-")), "t.zip"); fs.writeFileSync(f, bytes); try { execFileSync("unzip", ["-tq", f], { stdio: "pipe" }); return true; } catch (_) { return false; } };
      for (const kind of ["dnsnd", "dn2pst"]) {
        for (const s of src) {
          // (a Digitone 1 file: two LFOs and no E values but its drive's place, before the filter, which the import sets
          // again whenever DRIVE > 0; so the E values are compared too, LFO 3 only when it is heard)
          const dn1Only = kind === "dnsnd";
          const ex = digitoneExport([s], kind);
          const back = (await readFiles([{ name: ex.name, bytes: ex.bytes }]))[0];
          const a = fizzValues({ blob: s.blob, drive: s.drive }), b = back.ok ? fizzValues({ blob: back.sound.blob, drive: back.sound.drive }) : [];
          // (a Digitone 1 file has two LFOs and no E values: for a version-6 sound, LFO 3 at depth 0 (inaudible) and the E
          // values are not compared; the writer notes what it drops of them that is heard)
          const silentL3 = a[70] === 0;
          const diff = a.map((v, i) => (v !== b[i] ? i : -1)).filter((i) => i >= 0 && !(dn1Only && silentL3 && i >= 63 && i <= 70));
          const pl = (await (await dnReadZip(ex.bytes)).get(JSON.parse(new TextDecoder().decode(await (await dnReadZip(ex.bytes)).get("manifest.json"))).Payload));
          const lenOk = pl.length - 43 === ((pl[pl.length - 8] << 24 | pl[pl.length - 7] << 16 | pl[pl.length - 6] << 8 | pl[pl.length - 5]) >>> 0);
          check(back.ok && !diff.length && back.sound.name === s.name && back.sound.fizzTags.join() === s.fizzTags.join() &&
                JSON.stringify(back.sound.sends) === JSON.stringify(s.sends) && unzipOk(ex.bytes) && !back.sound.origin.length && lenOk,
            `${s.name} -> ${ex.name} -> read back: ${diff.length ? "values differ at " + diff.join(" ") : "the same values"}${s.blob.length === 128 ? ` (DRIVE ${b[73]} ${b[79] ? "PRE" : "POST"})` : ""}, tags ${back.ok && back.sound.fizzTags.join("+")}, unzip -t ${unzipOk(ex.bytes) ? "OK" : "FAIL"}, reader warnings ${back.ok ? back.sound.origin.length : "-"}`);
        }
        const many = digitoneExport(src, kind);
        const z = await dnReadZip(many.bytes);
        check(unzipOk(many.bytes) && z.names.length === src.length && z.names.every((n) => n.endsWith("." + kind)), `${src.length} presets -> "${many.name}" (${z.names.join(", ")})`);
      }
    }
    // a .fizzlib of 20 presets (20 of the owner's .dnsnd, as FIZZ's DUMP would give them: STORE frames): written, read
    // back, stored into a blank unit; every frame the same, and FIZZ's test_sysex reads them the same
    const DIR20 = "/Volumes/Q7Media-2025/Projects/Github/ChoralRootFM1/DigitonePresets";
    const twenty = fs.readdirSync(DIR20).filter((f) => f.endsWith(".dnsnd")).sort().slice(10, 30);
    if (twenty.length === 20) {
      const frames = [];
      for (const f of twenty) frames.push(storeFrame(await readDnsnd(fs.readFileSync(path.join(DIR20, f)), f)));
      const lib = makeFizzlib(frames.map((f) => soundFromStoreFrame(f)), { fizz: info.version, author: "test" });
      const back = await parseFizzlib(lib, "twenty.fizzlib");
      const again = back.items.map((it) => it.ok ? storeFrame(it.sound) : null);
      const same = again.length === 20 && again.every((f, i) => f && f.join() === frames[i].join());
      check(same && back.meta.count === 20, `a .fizzlib of 20 presets (${lib.length} bytes): read back, the same 20 STORE frames`);
      const e3 = await mk();
      run(e3, 300);
      const slots = again.map((f) => { const r = f && ask(e3, f); return r && r.status === 0 ? r.slot : 0; });
      e3.dispose();
      check(slots.join() === Array.from({ length: 20 }, (_, i) => i + 1).join(), `the 20 into a blank unit: MY PRESETS ${slots[0]}..${slots[19]}`);
      if (fs.existsSync(FEED)) {
        const tmp = fs.mkdtempSync(path.join(OUT, "feed20-"));
        const read = (f) => { fs.writeFileSync(path.join(tmp, "one.syx"), f); return execFileSync(FEED, ["--feed", path.join(tmp, "one.syx")], { encoding: "utf8" }).split("\n").filter((l) => /^(name|values|mix|tags) /.test(l)).join("\n"); };
        check(frames.every((f, i) => read(f) === read(again[i])), "FIZZ's test_sysex reads the 20 round-tripped presets as the originals");
      }
    }
    e2.dispose();
  } else { note(`SysEx: no .dnsnd files (--dnsnd ${DIR}): only IDENTITY checked`); e.dispose(); }
}

// 7. P1.6l LABELS and P1.6m LIST / DUMP against the wasm, raw MIDI in and out as the page sends them
{
  const frames = (eng) => { const out = [], b = eng.midiOut(); let sx = null; for (const x of b) { if (x === 0xf0) sx = [x]; else if (sx) { sx.push(x); if (x === 0xf7) { out.push(Uint8Array.from(sx)); sx = null; } } } return out; };
  const pump = (eng, ms) => { const got = []; for (let t = 0; t < ms; t += 5) { run(eng, 5); got.push(...frames(eng)); } return got; };
  // labels: subscribed; the step view, then SHIFT held
  e = await mk();
  p = new Panel(e);
  run(e, 500); tapBtn(e, p, "HOME"); run(e, 300); frames(e);
  e.midiIn(labelsReq(true));
  let ls = pump(e, 200).map(decodeLabels).filter(Boolean);
  const base = ls[ls.length - 1];
  const W = [0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26], BK = [1, 3, 5, 8, 10, 13, 15, 17, 20, 22, 25];
  check(base && base.keys[W[0]] && base.keys[BK[0]], `LABELS, the step view: SELECT "${base && base.enc[0]}", W1 "${base && base.keys[W[0]]}", B1 "${base && base.keys[BK[0]]}", HOME "${base && base.buttons[8]}"`);
  p.btn("GLO", true);
  ls = pump(e, 300).map(decodeLabels).filter(Boolean);
  const sh = ls[ls.length - 1];
  check(sh && sh.enc[0] === "TRACK" && sh.keys[BK[7]] === "FOLLOW" && sh.count !== base.count, `SHIFT held, FIZZ sends a new set by itself: SELECT "${sh && sh.enc[0]}", B8 "${sh && sh.keys[BK[7]]}", F1 "${sh && sh.buttons[0]}"`);
  p.btn("GLO", false);
  ls = pump(e, 300).map(decodeLabels).filter(Boolean);
  check(ls.length >= 1 && ls[ls.length - 1].keys[W[0]] === base.keys[W[0]], "SHIFT let go: the step view's labels again");
  e.dispose();
  // presets: 3 stored, LIST, DUMP everything, restored into a blank unit, dumped again: the same frames
  const DIR3 = "/Volumes/Q7Media-2025/Projects/Github/ChoralRootFM1/DigitonePresets";
  const three = fs.existsSync(DIR3) ? fs.readdirSync(DIR3).filter((f) => f.endsWith(".dnsnd")).sort().slice(0, 3) : [];
  if (three.length === 3) {
    e = await mk(); run(e, 300); frames(e);
    for (let i = 0; i < 3; i++) { e.midiIn(storeFrame(await readDnsnd(fs.readFileSync(path.join(DIR3, three[i])), three[i]), { batch: i > 0 })); pump(e, 50); }
    e.midiIn(listRequest({}));
    const parts = pump(e, 200).map(parseList).filter(Boolean);
    const rows = parts.flatMap((x) => x.entries).filter((x) => x.source === "mine");
    check(parts.length && parts[parts.length - 1].last && rows.length === 3, `LIST: ${parts.length} part(s), MY PRESETS ${rows.map((r) => r.slot + " " + r.name).join(", ")}`);
    e.midiIn(dumpRequest({ source: "mine", slot: 0 }));
    const dumps = pump(e, 300).map(parseDump).filter(Boolean);
    e.dispose();
    check(dumps.length === 3 && dumps.every((d, i) => d.slot === i + 1), `DUMP everything: ${dumps.map((d) => `${d.slot} ${d.name} (${d.six ? "STORE6" : "STORE"})`).join(", ")}`);
    const e4 = await mk(); run(e4, 300); frames(e4);
    for (const d of dumps) { e4.midiIn(restoreFrame(d.frame)); pump(e4, 50); }
    e4.midiIn(dumpRequest({ source: "mine", slot: 0 }));
    const again = pump(e4, 300).map(parseDump).filter(Boolean);
    e4.dispose();
    check(again.length === 3 && again.every((d, i) => d.frame.join() === dumps[i].frame.join()), "restoreFrame into a blank unit, dumped again: the same 3 frames, byte for byte");
    // and through the site's own files: the dumps -> .fizzlib -> read back -> STORE (first free) -> dumped again
    const lib = makeFizzlib(dumps.map((d) => soundFromStoreFrame(d.frame)), { fizz: info.version });
    const items = (await parseFizzlib(lib, "x.fizzlib")).items;
    const e5 = await mk(); run(e5, 300); frames(e5);
    for (let i = 0; i < items.length; i++) { e5.midiIn(storeFrame(items[i].sound, { batch: i > 0 })); pump(e5, 50); }
    e5.midiIn(dumpRequest({ source: "mine", slot: 0 }));
    const viaFile = pump(e5, 300).map(parseDump).filter(Boolean);
    e5.dispose();
    check(viaFile.length === 3 && viaFile.every((d, i) => d.frame.join() === dumps[i].frame.join()), "dumps -> .fizzlib -> imported into a blank unit -> dumped again: the same 3 frames");
  }
}

console.log(`\nfizz.wasm ${wasm.length} bytes; PNGs in ${path.relative(process.cwd(), OUT)}`);
console.log(fails ? `${fails} check(s) failed` : "all passed");
process.exit(fails ? 1 : 0);
