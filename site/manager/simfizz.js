// SPDX-License-Identifier: GPL-3.0-only
// A simulated FM-1 running FIZZ behind a fake Web MIDI access, for site/tools/test_manager.mjs and the manager's
// ?sim mode (headless screenshots: headless Chromium's Web MIDI mangles SysEx). Its semantic SysEx (IDENTITY, LIST,
// DUMP, STORE) is the real FIZZ: the wasm (site/fizz.wasm through engine/engine.js), whose flash (1 MiB, the host's
// store layout: site/manager/fizzstore.js LAYOUTS.host) is the region the bulk commands read and write. The bulk
// commands are a model of firmware/app/app.c app_usb_cmd (P2.13) and fm1-base's framing as the unit runs them:
//   - fm1-base's update handshake (cmd 0x11: the identity "FM-1_46..");
//   - one inbound frame held at a time: a frame that arrives while one waits is dropped (bsp/chip/usb.c sysex_byte);
//     the main loop takes one a pass (every `frameMs`), as fm1-base's does between frames;
//   - FZBI (offset, size, CRC-32), FZBR off n (n <= 256; FZER 11 outside), FZSR len crc (FZER 10 playing, 11 another
//     size; the image pulled twice with 0x30 read requests, 3 tries of `tryMs` each; 12 a bad or missing image,
//     nothing changed; 13 cut off while writing: the store cleared at the restart, as the unit's foreign-store wipe;
//     FZOK: FIZZ started again on the restored flash).
// The web manager's Phase 1 commands (STATUS .. PROJECT: docs/SYSEX.md) are the wasm's too: its jobs (a pool swap,
// a delete, a project load) step with the store's step as the unit's do, so the main loop runs on for a while after
// each semantic frame (3 s); sim.tapPlay() starts or stops FIZZ's transport (MIDI START / STOP: the
// commands that need it stopped answer BUSY), sim.counts.cmds[cmd] counts the semantic commands; notes and controllers go
// to the engine at once (sim.counts.notes). sim.hooks.reply(frame) (tests: a unit that loses or delays replies):
// "drop" loses that reply, a number delays it (ms); sim.hooks.midi(bytes) sees each channel message that comes in (PLAY's
// tests: the remote's CCs, the keys' notes).
// No DOM: Node runs it as it is.
//   new SimFizz({createEngine, module, id, playing, frameMs, tryMs, bulk, answers}); await sim.boot()
//   sim.access (navigator.requestMIDIAccess's shape), sim.unplug(), sim.engine, sim.flash(), sim.counts, sim.tapPlay(),
//   sim.press(label) / sim.selectTrack(t) / sim.panel(buttons, keys) (the unit's own panel: OCT- / OCT+ in its browser)
//   new SimPort(name, id) (?sim=two's other gear: a port that answers fm1-base's handshake with `id` and nothing else,
//   as RemoteFM1 or the update loader; id null: never answers); port.counts.hs, port.counts.other
import { pack7, unpack7 } from "../install/fm1lib.js";

const HS = [0xF0, 0x00, 0x32, 0x45, 0x00, 0x00, 0x00, 0x40, 0x7F, 0xF7];
const PANEL_BUTTONS = ["FX", "SEL", "ENV", "LFO", "EDIT", "GLO", "HOME", "SAVE", "ARP", "SEQ", "PLAY", "REC", "OCT-", "OCT+"];
const CRC_T = (() => { const t = new Uint32Array(256); for (let n = 0; n < 256; n++) { let c = n; for (let k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320 ^ (c >>> 1) : c >>> 1; t[n] = c >>> 0; } return t; })();
const crc32 = (b) => { let c = 0xFFFFFFFF; for (let i = 0; i < b.length; i++) c = CRC_T[(c ^ b[i]) & 0xFF] ^ (c >>> 8); return (c ^ 0xFFFFFFFF) >>> 0; };
const le32 = (v) => [v & 0xFF, (v >>> 8) & 0xFF, (v >>> 16) & 0xFF, (v >>> 24) & 0xFF];
const rd32 = (b, o) => (b[o] | (b[o + 1] << 8) | (b[o + 2] << 16) | (b[o + 3] << 24)) >>> 0;
const later = (fn, ms) => (!ms && typeof setImmediate === "function" ? setImmediate(fn) : setTimeout(fn, ms));
const cancel = (t) => { if (t && typeof clearImmediate === "function") clearImmediate(t); clearTimeout(t); };

// fm1-base's framing: F0 pack7(00 59 cmd len16 00 body ~sum) F7 (felucca-bsp ota.c ota_send_msg)
function msg(cmd, body) {
  let s = 0;
  for (const b of body) s += b;
  return [0xF0, ...pack7([0, 0x59, cmd, body.length & 0xFF, body.length >> 8, 0, ...body, ~s & 0xFF]), 0xF7];
}

export class SimFizz {
  constructor({ createEngine = null, module = null, flash = null, id = "FM-1_46001100", name = "FIZZ FM-1", playing = false,
                frameMs = 1, tryMs = 300, bulk = true, answers = true, sampleRate = 44100 } = {}) {
    Object.assign(this, { createEngine, module, id, playing, frameMs, tryMs, bulk, answers, sampleRate, initFlash: flash });
    this.engine = null; this.slot = null; this.restoring = null; this.gone = false; this.hooks = {};
    this.counts = { frames: 0, dropped: 0, fzbr: 0, fzsrReads: 0, sent: 0, cmds: {} };
    this.L = new Float32Array(128); this.R = new Float32Array(128);
    this.input = { name, state: "connected", open: async () => {}, onmidimessage: null };
    this.output = { name, state: "connected", open: async () => {}, send: (b) => this.rx(Array.from(b)) };
    this.access = { inputs: new Map([["in", this.input]]), outputs: new Map([["out", this.output]]), onstatechange: null, sysexEnabled: true };
    this.cache = { seq: -1, bytes: null };
    this.timer = null; this.busyUntil = 0;
  }
  async boot(flash = this.initFlash) {
    if (this.engine) this.engine.dispose();
    this.engine = this.createEngine && this.module
      ? await this.createEngine({ sampleRate: this.sampleRate, files: { wasm: this.module }, flash, options: {} }) : null;
    this.cache = { seq: -1, bytes: null };
    if (this.engine) this.run(400);
    return this;
  }
  // device time with its audio (the engine takes MIDI in its render), its SysEx replies to the host
  run(ms) {
    const e = this.engine;
    if (!e || this.restoring) return;
    const end = e.deviceMs() + ms;
    while (e.deviceMs() < end) e.render(this.L, this.R);
    this.out(e.midiOut());
  }
  out(bytes) {
    let sx = this.sx || null;
    for (const b of bytes) {
      if (b === 0xF0) sx = [b];
      else if (sx) { sx.push(b); if (b === 0xF7) { this.tx(sx); sx = null; } }
    }
    this.sx = sx;
  }
  tx(bytes) {
    if (this.gone) return;
    this.counts.sent++;
    const data = Uint8Array.from(bytes);
    const h = this.hooks.reply ? this.hooks.reply(data) : 0;   // (tests: "drop" loses the reply, a number delays it, ms)
    if (h === "drop") { this.counts.lost = (this.counts.lost || 0) + 1; return; }
    later(() => { if (!this.gone && this.input.onmidimessage) this.input.onmidimessage({ data }); }, +h || 0);
  }
  flash() {                                          // the region now (cached between the engine's flash writes)
    const f = this.engine.flash();
    if (f.seq !== this.cache.seq || !this.cache.bytes) this.cache = { seq: f.seq, bytes: f.bytes };
    return this.cache.bytes;
  }
  unplug() {
    this.gone = true;
    this.input.state = this.output.state = "disconnected";
    cancel(this.timer);
    this.timer = null;
    if (this.restoring) this.restoring.cut();
    if (this.access.onstatechange) this.access.onstatechange({ port: this.input });
  }
  // plugged in again: a unit cut off while it wrote its store starts on a cleared one (app.c: foreign, wiped)
  async replug() {
    if (!this.engine) await this.boot(null);
    this.gone = false; this.slot = null;
    this.input.state = this.output.state = "connected";
    if (this.access.onstatechange) this.access.onstatechange({ port: this.input });
  }
  dispose() { this.unplug(); if (this.engine) this.engine.dispose(); this.engine = null; }
  // FIZZ's transport started or stopped (MIDI START / STOP into the engine, as a clock source would)
  tapPlay() {
    if (!this.engine) return;
    this.engine.midiIn([this.playing ? 0xFC : 0xFA]);
    this.run(80);
    this.playing = !this.playing;
  }

  // the unit's own panel (the owner's hands): buttons held by their FM-1 labels (FX SEL ENV LFO EDIT GLO HOME SAVE ARP
  // SEQ PLAY REC OCT- OCT+: FIZZ's FX, TRACKS, MIX, LATCH, EDIT, SHIFT, HOME, SOUNDS, FILL, PATTERNS, PLAY, REC, F1, F2)
  // and keys (0..26: the panel's key bits) for a moment, then let go; press(label) one button; selectTrack(t) TRACKS + white key t
  panel(buttons = [], keys = [], ms = 40) {
    if (!this.engine) return;
    const t = this.engine.info().panelTable;
    let b = 0, k = 0;
    for (const l of buttons) { const i = PANEL_BUTTONS.indexOf(l); if (i < 0) throw new Error(`no button ${l}`); b |= 1 << t.btnId[i]; }
    for (const x of keys) k |= 1 << x;
    this.engine.setPanel({ buttons: b, keys: k });
    this.run(ms);
  }
  press(label) { this.panel([label]); this.panel([]); }
  selectTrack(t) { this.panel(["SEL"]); this.panel(["SEL"], [[1, 3, 5, 8, 10, 13][t]]); this.panel(["SEL"]); this.panel([]); }   // (white keys 1..6's bits)

  // ---- inbound: one frame held (sysex_byte: sx_collect = !sx_ready), taken by the main loop's next pass
  rx(d) {
    if (this.gone) return;
    if (d[0] !== 0xF0 && d[0] >= 0x80 && this.hooks.midi) this.hooks.midi(Uint8Array.from(d));
    if (d[0] !== 0xF0 && d[0] >= 0x80 && this.engine && !this.restoring) {   // (a note, a controller: the USB-MIDI parser's, not
      this.engine.midiIn(d);                          // the one SysEx frame held; P2-B: the editor's keybed)
      this.counts.notes = (this.counts.notes || 0) + 1;
      return;
    }
    if (this.restoring) { this.restoring.frame(d); return; }   // (inside ota_read: it takes each frame at once)
    if (this.slot) { this.counts.dropped++; return; }
    this.slot = d;
    this.loop();
  }
  loop() {
    if (this.timer) return;
    this.timer = later(() => { this.timer = null; this.pass(); }, this.frameMs);
  }
  pass() {
    const d = this.slot;
    this.slot = null;
    if (d) { this.counts.frames++; this.take(d); }
    if (this.engine && !this.restoring) {             // the frame's device time, then a little more while replies flow
      this.run(Math.max(2, this.frameMs));
      if (Date.now() < this.busyUntil || this.slot) this.loop();
    }
  }
  take(d) {
    if (d.join() === HS.join()) { this.tx(msg(0x11, [...this.id].map((c) => c.charCodeAt(0)).concat(new Array(27).fill(0)).slice(0, 27))); return; }
    if (d[1] === 0x7D && d[2] === 0x46 && d[3] === 0x5A) {   // FIZZ's own SysEx: the wasm
      if (!this.engine || !this.answers) return;
      if (this.hooks.semantic) this.hooks.semantic(d);
      this.counts.cmds[d[5]] = (this.counts.cmds[d[5]] || 0) + 1;
      this.engine.midiIn(d);
      this.busyUntil = Date.now() + 3000;                // (a job: its steps, a store step every 15 ms of device time)
      return;
    }
    const u = unpack7(d.slice(1, -1));
    if (u.length < 7 || u[0] !== 0 || u[1] !== 0x59 || (u[3] | (u[4] << 8)) + 7 !== u.length) return;
    let s = 0;
    for (let i = 6; i + 1 < u.length; i++) s += u[i];
    if ((~s & 0xFF) !== u[u.length - 1]) return;      // (ota_take: a frame with a bad sum is not one)
    if (u[2] === 0x46 && this.bulk && this.engine) this.usbCmd(u);
  }
  reply(tag, data) { this.tx(msg(0x46, [...tag].map((c) => c.charCodeAt(0)).concat(Array.from(data)))); }

  // ---- app.c app_usb_cmd
  usbCmd(u) {
    const tag = String.fromCharCode(...u.slice(6, 10)), n = u.length;
    const region = this.flash(), size = region.length;
    if (tag === "FZBI") { this.reply("FZBI", [...le32(0), ...le32(size), ...le32(crc32(region))]); return; }
    if (tag === "FZBR") {
      if (n < 7 + 12) return;
      const off = rd32(u, 10), len = rd32(u, 14);
      this.counts.fzbr++;
      if (this.hooks.read) this.hooks.read(off);
      if (len > 256 || off > size || len > size - off) { this.reply("FZER", le32(11)); return; }
      this.reply("FZBD", [...le32(off), ...this.flash().subarray(off, off + len)]);
      return;
    }
    if (tag === "FZSR") {
      if (n < 7 + 12) return;
      const len = rd32(u, 10), crc = rd32(u, 14);
      if (this.playing) { this.reply("FZER", le32(10)); return; }
      if (len !== size) { this.reply("FZER", le32(11)); return; }
      this.restore(len, crc);
    }
  }

  // fz_restore: the image pulled once (its CRC checked), then again while each sector is erased and written
  restore(len, crc) {
    const img = new Uint8Array(len);
    let pass = 1, addr = 0, tries = 0, wait = null, c = 0xFFFFFFFF;
    const st = this.restoring = {};
    const finish = async (code) => {
      clearTimeout(wait);
      this.restoring = null;
      if (code === 0 || code >= 13) await this.boot(code === 0 ? img : null);   // fz_restart (13, 14: wiped as foreign)
      this.reply(code ? "FZER" : "FZOK", le32(code || 1));
      if (this.hooks.restored) this.hooks.restored(code);
    };
    const ask = () => {
      const k = Math.min(512, len - addr);
      const q = [0, ...le32(addr), k & 0xFF, k >> 8, 0];
      this.counts.fzsrReads++;
      this.tx(msg(0x30, q));
      clearTimeout(wait);
      wait = later(() => {
        if (this.restoring !== st) return;
        if (++tries >= 3) { finish(pass === 1 ? 12 : 13); return; }
        ask();
      }, this.tryMs);
    };
    st.frame = (d) => {
      if (d.join() === HS.join()) { this.tx(msg(0x11, [...this.id].map((x) => x.charCodeAt(0)).concat(new Array(27).fill(0)).slice(0, 27))); return; }
      const u = unpack7(d.slice(1, -1)), k = Math.min(512, len - addr);
      if (u.length !== k + 15 || u[2] !== 0x30 || rd32(u, 7) !== addr || (u[11] | (u[12] << 8) | (u[13] << 16)) !== k) return;
      const data = u.slice(14, 14 + k);
      if (this.hooks.restoreRead && this.hooks.restoreRead(pass, addr) === "cut") { this.unplug(); return; }
      tries = 0;
      if (pass === 1) {
        for (const b of data) c = CRC_T[(c ^ b) & 0xFF] ^ (c >>> 8);
      } else img.set(data, addr);
      addr += k;
      if (addr < len) { ask(); return; }
      if (pass === 1) {
        if (((c ^ 0xFFFFFFFF) >>> 0) !== crc) { finish(12); return; }
        pass = 2; addr = 0; ask(); return;            // (app_panic, "RESTORING ITS STORE")
      }
      finish(crc32(img) === crc ? 0 : 14);
    };
    st.cut = () => { clearTimeout(wait); if (pass === 2 && this.engine) { this.engine.dispose(); this.engine = null; } this.restoring = null; };
    ask();
  }
}

export class SimPort {
  constructor(name, id = null) {
    this.id = id; this.gone = false; this.counts = { hs: 0, other: 0 }; this.access = null;
    this.input = { name, state: "connected", open: async () => {}, onmidimessage: null };
    this.output = { name, state: "connected", open: async () => {}, send: (b) => this.rx(Array.from(b)) };
  }
  rx(d) {
    if (this.gone) return;
    if (d.join() !== HS.join()) { this.counts.other++; return; }
    this.counts.hs++;
    if (!this.id) return;
    const data = Uint8Array.from(msg(0x11, [...this.id].map((c) => c.charCodeAt(0)).concat(new Array(27).fill(0)).slice(0, 27)));
    setTimeout(() => { if (!this.gone && this.input.onmidimessage) this.input.onmidimessage({ data }); }, 2);
  }
  unplug() { this.gone = true; this.input.state = this.output.state = "disconnected"; if (this.access && this.access.onstatechange) this.access.onstatechange({ port: this.input }); }
  replug() { this.gone = false; this.input.state = this.output.state = "connected"; if (this.access && this.access.onstatechange) this.access.onstatechange({ port: this.input }); }
}
