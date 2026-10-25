// SPDX-License-Identifier: GPL-3.0-only
// The browser's own FIZZ for the sound editor (docs/MANAGER-P2-PLAN.md §1.4 "without a unit"): site/fizz.wasm, the
// same core, in an AudioWorklet (js/audio.js's Player) with its sound out of the computer. The editor plays its sound
// there when no FM-1 edits live: the sound loaded onto track 1 (STORE6 with LOAD into a scratch MY PRESETS place of
// this browser FIZZ; a FIZZ with the P2-B commands: AUDITION, and each drag a PARAM), notes on channel 16 (the
// selected track: track 1). Nothing here is kept: a fresh store each time the page opens it.
//   const bf = new BrowserFizz({ log });  await bf.start()  (in a click: the autoplay rule)
//   bf.ask(frame, want) -> the first reply want(parsed) accepts (3 s), bf.send(bytes), bf.note(n, vel), bf.ready
import { Player } from "../js/audio.js";
import { ver } from "../js/build.js";
import { parseReply, identity } from "../js/fizz_sysex.js";

export class BrowserFizz {
  constructor({ log = () => {} } = {}) {
    this.log = log;
    this.ready = false; this.starting = null; this.version = "";
    this.waits = []; this.sx = null;
    this.player = new Player({
      info: () => { if (!this.ready && this.onInfo) this.onInfo(); },
      midiOut: (bytes) => this.rx(bytes),
      error: (m) => this.log(`browser FIZZ: ${m}`),
    }, { root: "../" });
  }
  // the AudioContext and FIZZ in it (from a click); true once FIZZ answers
  start() {
    if (this.ready) return Promise.resolve(true);
    if (this.starting) return this.starting;
    this.starting = (async () => {
      const ok = await this.player.start();
      if (!ok) { this.starting = null; return false; }
      const booted = new Promise((r) => { this.onInfo = r; });
      await this.player.load(ver("../fizz.wasm"), null);
      await Promise.race([booted, new Promise((r) => setTimeout(r, 8000))]);
      for (let t = 0; t < 20 && !this.version; t++) {      // (FIZZ answers once it runs)
        const r = await this.ask(identity(), (x) => x.kind === "identity", 400).catch(() => null);
        if (r) this.version = r.version.replace(/^FIZZ\s*/, "");
      }
      this.ready = !!this.version;
      this.log(this.ready ? `browser FIZZ ${this.version} plays the editor's sound` : "browser FIZZ did not answer");
      if (!this.ready) this.starting = null;
      return this.ready;
    })();
    return this.starting;
  }
  get running() { return this.ready && this.player.running; }
  send(bytes) { this.player.midi(bytes); }
  note(n, vel = 100, ch = 15) { this.send(vel ? [0x90 | ch, n & 127, vel & 127] : [0x80 | ch, n & 127, 0]); }
  setVolume(v) { this.player.setVolume(v); }
  rx(bytes) {
    for (const b of bytes) {
      if (b === 0xf0) this.sx = [b];
      else if (this.sx) {
        this.sx.push(b);
        if (b === 0xf7) { const f = this.sx; this.sx = null; this.frame(f); }
      }
    }
  }
  frame(f) {
    const r = parseReply(f);
    if (!r) return;
    const i = this.waits.findIndex((w) => w.want(r));
    if (i >= 0) { const w = this.waits.splice(i, 1)[0]; clearTimeout(w.t); w.res({ ...r, frame: Uint8Array.from(f) }); }
  }
  // a frame sent, the first reply want() accepts (null after ms)
  ask(frame, want, ms = 3000) {
    return new Promise((res) => {
      const w = { want, res, t: setTimeout(() => { this.waits.splice(this.waits.indexOf(w), 1); res(null); }, ms) };
      this.waits.push(w);
      this.send(frame);
    });
  }
}
