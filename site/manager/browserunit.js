// SPDX-License-Identifier: GPL-3.0-only
// FIZZ in this browser as the Manager's FM-1 (the owner, 2026-10-27: "play and edit presets without the FM-1, hear
// them from the emulator"): the wasm FIZZ (site/fizz.wasm) in an AudioWorklet (js/audio.js's Player, its sound out of
// the computer) behind a fake Web MIDI access, so every tab works on it as on a unit: LIBRARY, EDIT and TRACK live,
// POOL, IMPORT, PROJECTS, BACK UP and RESTORE. The semantic SysEx is the wasm's own; the update handshake and the bulk
// commands (FZBI / FZBR / FZSR) are simfizz.js's model of app.c, over the browser FIZZ's flash. Its store is shown in
// the FM-1's layout (fizzstore.js convertImage: the same records), so a backup of it restores onto the FM-1 and the
// FM-1's restores onto it. The flash is kept in this browser (onFlash: the page stores it) and comes back next time.
// Before the page's first click (the autoplay rule: no sound yet) the same FIZZ runs quietly on the page's own thread
// (engine.js, as simfizz.js runs it); wake() on the first click moves it into the worklet with its flash, and the frames
// sent meanwhile wait for it.
//   const u = new BrowserUnit({ log, onFlash(hostImage) }); await u.start() (in a click: the autoplay rule);
//   await u.boot(image | null) (an FM-1 image or the browser's 1 MiB one: quiet if the sound has not started);
//   await u.wake() (in a click: the sound on, FIZZ moved into the worklet); u.quiet; u.access (requestMIDIAccess's shape)
// PLAY (play.js) plays its panel: u.panel(p) (panel.js's Controls snapshot, once it sounds), u.view = { frame(pixels),
// leds(levels, playGreen) } (its screen and LEDs as they come), u.screenNow() (the last of each; quiet: the engine's own)
// nf-speed (the owner, 2026-10-29: "loading and writing to browser take the same long time as writing to the FM-1"): the
// Manager reads and writes this store here, in the page, not over the SysEx paced for the FM-1:
//   await u.readStore() -> {image, lo, size, crc, stats: null} (what a backup over the link reads: FZBI's snapshot, the
//     FM-1's layout); await u.writeStore(image) -> {ok, ms} (what FZSR leaves: FIZZ started again on the image, its
//     flash kept in this browser), or throws as the link's restore does ({code: "fzer", fzer: 10 | 11, written: false})
import { SimFizz } from "./simfizz.js";
import { Player } from "../js/audio.js";
import { ver } from "../js/build.js";
import { convertImage, LAYOUTS } from "./fizzstore.js";
import { crc32 } from "./fizzbak.js";

export const BROWSER_ID = "FM-1_46999999";            // (FIZZ's id prefix, digits only: fm1lib.js parseIdentity)
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

export class BrowserUnit extends SimFizz {
  constructor({ log = () => {}, onFlash = null } = {}) {
    super({ id: BROWSER_ID, name: "FM-1 (FIZZ in this browser)", frameMs: 0, tryMs: 2000 });   // (fm1lib.js probes ports named FM-1 only)
    this.log = log; this.onFlash = onFlash;
    this.host = null; this.region = null; this.waitFlash = null; this.booted = null; this.saveT = null;
    this.quiet = false; this.holding = null; this.waking = null; this.fseq = -1;
    this.view = null; this.lastFrame = null; this.lastLeds = null; this.version = "";
    this.player = new Player({
      frame: (px) => { this.lastFrame = px; if (this.view) this.view.frame(px); },
      leds: (levels, playGreen) => { this.lastLeds = { levels, playGreen }; if (this.view) this.view.leds(levels, playGreen); },
      midiOut: (b) => this.out(b),
      info: (m) => { if (m && m.version) this.version = m.version; if (this.booted) { const f = this.booted; this.booted = null; f(true); } },
      flash: (m) => this.flashMsg(m),
      error: (m) => this.log(`browser FIZZ: ${m}`),
    }, { root: "../" });
  }
  start() { return this.player.start(); }
  get running() { return this.player.running && !this.quiet; }
  setVolume(v) { this.player.setVolume(v); }
  async boot(image = null, { settle = 600 } = {}) {
    const host = image && image.length !== LAYOUTS.host.size ? convertImage(image, "host") : image;
    if (!this.player.running) return this.bootQuiet(host);
    if (this.quiet && this.engine) this.engine.dispose();
    this.quiet = false;
    this.engine = { midiIn: (d) => this.player.midi(d), dispose: () => {} };
    const up = new Promise((r) => { this.booted = r; });
    await this.player.load(ver("../fizz.wasm"), host);
    await Promise.race([up, sleep(8000)]);
    this.host = host; this.region = null;
    if (settle) await sleep(settle);                   // (FIZZ mounts its store and draws its first screen)
    return this;
  }
  // quiet: FIZZ on this thread (simfizz.js's loop), no sound
  async bootQuiet(host) {
    if (!this.module) {
      const [{ createEngine }, module] = await Promise.all([import("../engine/engine.js"), fetch(ver("../fizz.wasm")).then((r) => r.arrayBuffer()).then((b) => WebAssembly.compile(b))]);
      this.createEngine = createEngine; this.module = module;
    }
    this.quiet = true;
    await super.boot(host);
    this.fseq = this.engine.flash().seq;
    try { this.version = this.engine.info().version || this.version; } catch (_) { /* (no info) */ }
    this.host = host; this.region = null;
    return this;
  }
  run(ms) {                                           // (the worklet runs FIZZ in real time)
    if (!this.quiet || !this.engine) return;
    super.run(ms);
    const q = this.engine.flash().seq;
    if (q !== this.fseq) { this.fseq = q; this.flashMsg({}); }
  }
  pass() {
    if (this.quiet) { super.pass(); return; }
    const d = this.slot; this.slot = null; if (d) { this.counts.frames++; this.take(d); }
  }
  rx(d) { if (this.holding) this.holding.push(d); else super.rx(d); }
  // the first click: the sound on; a quiet FIZZ moved into the worklet (its flash as it is now). -> true if it plays
  wake() {
    if (this.waking) return this.waking;
    this.waking = (async () => {
      const ok = await this.player.start().catch(() => false);
      if (ok && this.quiet && this.engine) await this.move();
      return ok && !this.quiet;
    })().finally(() => { this.waking = null; });
    return this.waking;
  }
  async move() {
    while (this.restoring || this.storing) await sleep(100);
    this.holding = [];                                // (frames sent meanwhile: kept, then given to the worklet's FIZZ)
    clearTimeout(this.timer); this.timer = null;
    if (this.slot) { this.holding.push(this.slot); this.slot = null; }
    SimFizz.prototype.run.call(this, 300);            // (its replies out, its saves written)
    const host = this.engine.flash().bytes;
    await this.boot(host);                            // (the quiet FIZZ let go there)
    this.log("browser FIZZ: the sound on (moved into the audio worklet)");
    const held = this.holding;
    this.holding = null;
    for (const d of held) { this.rx(d); await sleep(20); }
  }
  // PLAY: the panel (keys, buttons, detents, MASTER) into the worklet's FIZZ; -> false while it is quiet or waking
  panel(p) {
    if (this.quiet || this.waking || !this.player.node) return false;
    this.player.panel(p);
    return true;
  }
  screenNow() {
    if (this.quiet && this.engine && this.engine.screen) return { pixels: this.engine.screen().pixels, leds: this.engine.leds() };
    return { pixels: this.lastFrame, leds: this.lastLeds };
  }
  // the flash: asked of the worklet when the bulk commands need it; each change of FIZZ's own stored for next time
  flashMsg(m) {
    if (m.requested && this.waitFlash) { const f = this.waitFlash; this.waitFlash = null; f(m.bytes); return; }
    if (this.onFlash && !m.requested) { clearTimeout(this.saveT); this.saveT = setTimeout(() => this.save(), 1500); }
  }
  hostFlash() {
    if (this.quiet) return Promise.resolve(this.engine ? this.engine.flash().bytes : null);
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
  // nf-speed: the store read and written here (FZBI + FZBR's image, FZSR's end), FIZZ's SysEx not paced for this
  async readStore() {
    while (this.restoring || this.storing || this.waking) await sleep(20);
    const b = await this.hostFlash();
    if (!b) throw Object.assign(new Error("the browser's FIZZ did not answer"), { code: "timeout" });
    this.host = b; this.region = convertImage(b, "device");
    const image = this.region.slice();
    return { image, lo: 0, size: image.length, crc: crc32(image), stats: null };
  }
  async writeStore(image) {
    const no = (code, msg) => Object.assign(new Error(msg), { code: "fzer", fzer: code, written: false });
    if (this.playing) throw no(10, "the FM-1 is playing: stop it first");               // (as simfizz.js's FZSR)
    if (image.length !== LAYOUTS.device.size) throw no(11, "the backup's size is not this FM-1's store");
    while (this.restoring || this.storing || this.waking) await sleep(20);
    const t0 = Date.now();
    this.storing = true;
    clearTimeout(this.timer); this.timer = null;      // (no pass into the FIZZ let go)
    try { await this.boot(Uint8Array.from(image), { settle: 0 }); }   // (fz_restart on the image: the same boot FZSR's FZOK gives)
    finally { this.storing = false; if (this.slot) this.loop(); }
    if (this.hooks.restored) this.hooks.restored(0);
    clearTimeout(this.saveT);
    await this.save();                                // (kept in this browser at once, not on FIZZ's next write)
    return { ok: true, ms: Date.now() - t0 };
  }
  dispose() { clearTimeout(this.saveT); this.onFlash = null; this.unplug(); if (this.quiet && this.engine) this.engine.dispose(); this.engine = null; try { this.player.ctx && this.player.ctx.close(); } catch (_) { /* (gone) */ } }
}
