// SPDX-License-Identifier: GPL-3.0-only
// FIZZ in this browser as the Manager's FM-1 (the owner, 2026-10-27: "play and edit presets without the FM-1, hear
// them from the emulator"): the wasm FIZZ (site/fizz.wasm) in an AudioWorklet (js/audio.js's Player, its sound out of
// the computer) behind a fake Web MIDI access, so every tab works on it as on a unit: LIBRARY, EDIT and TRACK live,
// POOL, IMPORT, PROJECTS, BACK UP and RESTORE. The semantic SysEx is the wasm's own; the update handshake and the bulk
// commands (FZBI / FZBR / FZSR) are simfizz.js's model of app.c, over the browser FIZZ's flash. Its store is shown in
// the FM-1's layout (fizzstore.js convertImage: the same records), so a backup of it restores onto the FM-1 and the
// FM-1's restores onto it. The flash is kept in this browser (onFlash: the page stores it) and comes back next time.
//   const u = new BrowserUnit({ log, onFlash(hostImage) }); await u.start() (in a click: the autoplay rule);
//   await u.boot(image | null) (an FM-1 image or the browser's 1 MiB one); u.access (navigator.requestMIDIAccess's shape)
import { SimFizz } from "./simfizz.js";
import { Player } from "../js/audio.js";
import { ver } from "../js/build.js";
import { convertImage, LAYOUTS } from "./fizzstore.js";

export const BROWSER_ID = "FM-1_46999999";            // (FIZZ's id prefix, digits only: fm1lib.js parseIdentity)
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

export class BrowserUnit extends SimFizz {
  constructor({ log = () => {}, onFlash = null } = {}) {
    super({ id: BROWSER_ID, name: "FM-1 (FIZZ in this browser)", frameMs: 0, tryMs: 2000 });   // (fm1lib.js probes ports named FM-1 only)
    this.log = log; this.onFlash = onFlash;
    this.host = null; this.region = null; this.waitFlash = null; this.booted = null; this.saveT = null;
    this.player = new Player({
      midiOut: (b) => this.out(b),
      info: () => { if (this.booted) { const f = this.booted; this.booted = null; f(true); } },
      flash: (m) => this.flashMsg(m),
      error: (m) => this.log(`browser FIZZ: ${m}`),
    }, { root: "../" });
  }
  start() { return this.player.start(); }
  get running() { return this.player.running; }
  setVolume(v) { this.player.setVolume(v); }
  async boot(image = null) {
    const host = image && image.length !== LAYOUTS.host.size ? convertImage(image, "host") : image;
    this.engine = { midiIn: (d) => this.player.midi(d), dispose: () => {} };
    const up = new Promise((r) => { this.booted = r; });
    await this.player.load(ver("../fizz.wasm"), host);
    await Promise.race([up, sleep(8000)]);
    this.host = host; this.region = null;
    await sleep(600);                                  // (FIZZ mounts its store and draws its first screen)
    return this;
  }
  run() {}                                            // (the worklet runs FIZZ in real time)
  pass() { const d = this.slot; this.slot = null; if (d) { this.counts.frames++; this.take(d); } }
  // the flash: asked of the worklet when the bulk commands need it; each change of FIZZ's own stored for next time
  flashMsg(m) {
    if (m.requested && this.waitFlash) { const f = this.waitFlash; this.waitFlash = null; f(m.bytes); return; }
    if (this.onFlash && !m.requested) { clearTimeout(this.saveT); this.saveT = setTimeout(() => this.save(), 1500); }
  }
  hostFlash() {
    return new Promise((res) => { this.waitFlash = res; this.player.flashReq(); setTimeout(() => { if (this.waitFlash === res) { this.waitFlash = null; res(null); } }, 3000); });
  }
  async save() { const b = await this.hostFlash(); if (b && this.onFlash) { this.host = b; this.onFlash(b); } }
  flash() { return this.region; }
  async usbCmd(u) {
    const tag = String.fromCharCode(...u.slice(6, 10));
    if (tag === "FZBI" || !this.region) {             // (a backup starts with FZBI: the store as it is now)
      const b = await this.hostFlash();
      if (b) { this.host = b; this.region = convertImage(b, "device"); }
    }
    if (this.region) super.usbCmd(u);
  }
  dispose() { this.unplug(); this.engine = null; try { this.player.ctx && this.player.ctx.close(); } catch (_) { /* (gone) */ } }
}
