// SPDX-License-Identifier: MPL-2.0
// FIZZ's browser emulator (firmware/build.py --web) run headless in Node, with the same imports fm1-base's
// worklet gives it (web/emu/worklet.js): boot, the idle screen, PLAY, the demo pattern, the screen as raw RGB565 and
// the sound's level. A check that the wasm runs, not of the page (its "Switch on", the AudioWorklet).
//   node firmware/tools/test_emu.mjs firmware/app/build/emu/app.wasm OUTDIR
// writes OUTDIR/emu-idle.raw and emu-play.raw (240 x 240, as fm1-base's fbdump: high byte first) and prints the
// level; exits 1 if the screen stayed black or the sound silent.
import fs from "fs";

const [wasmPath, outDir] = process.argv.slice(2);
let mem, t = 0n;
const wasi = {
  clock_time_get: (id, prec, out) => { new DataView(mem.buffer).setBigUint64(out, t, true); return 0; },
  fd_write: (fd, iov, n, out) => { new DataView(mem.buffer).setUint32(out, 0, true); return 0; },
  proc_exit: () => {},
};
const stub = new Proxy(wasi, { get: (o, k) => (k in o ? o[k] : () => 0) });
const env = new Proxy({}, { get: () => () => 0 });
const { instance } = await WebAssembly.instantiate(fs.readFileSync(wasmPath), { wasi_snapshot_preview1: stub, env });
const ex = instance.exports;
mem = ex.memory;
if (ex._initialize) ex._initialize();
ex.web_master(2800);
ex.web_boot();
let peak = 0;
function run(ms, measure) {                    // the device's time, 128 frames at a time (as the worklet asks)
  for (let done = 0; done < Math.round(ms * 44.1); done += 128) {
    ex.web_render(128);
    t += 2902494n;
    if (measure)
      for (const v of new Float32Array(mem.buffer, ex.web_out_l(), 128)) peak = Math.max(peak, Math.abs(v));
  }
}
function shot(name) {
  const px = new Uint8Array(mem.buffer, ex.web_fb(), 240 * 240 * 2);
  fs.writeFileSync(`${outDir}/${name}.raw`, Buffer.from(px));
  return px.some((b) => b !== 0);
}
run(600);
const idle = shot("emu-idle");
ex.web_buttons(1 << 10);                       // PLAY (plat.h B_PLAY)
run(100);
ex.web_buttons(0);
run(1500);
const play = shot("emu-play");
run(3000, true);
const db = peak > 0 ? 20 * Math.log10(peak) : -Infinity;
console.log(`device ${ex.web_now()} ms, ${ex.web_blits()} blits, screen ${idle && play ? "drawn" : "BLACK"}, ` +
            `peak ${db.toFixed(1)} dBFS`);
process.exit(idle && play && db > -40 ? 0 : 1);
