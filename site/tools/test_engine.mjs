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
// Adapted from RemoteFM1's web/engines/native/test.mjs (GPL-3.0-only).
import fs from "node:fs";
import path from "node:path";
import zlib from "node:zlib";
import { fileURLToPath } from "node:url";
import { createEngine } from "../engine/engine.js";

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

console.log(`\nfizz.wasm ${wasm.length} bytes; PNGs in ${path.relative(process.cwd(), OUT)}`);
console.log(fails ? `${fails} check(s) failed` : "all passed");
process.exit(fails ? 1 : 0);
