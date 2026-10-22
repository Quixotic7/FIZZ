// SPDX-License-Identifier: GPL-3.0-only
// The page's side of the audio: one AudioContext, the "fm1-host" AudioWorkletNode (worklet/host.js) and FIZZ's engine
// added to its worklet. From RemoteFM1's web/site/js/audio.js, cut to one engine (the native one, engine/engine.js)
// and one firmware (fizz.wasm, fetched here: an engine can't fetch inside the worklet).
//
//   const p = new Player(events);            events: { frame, leds, info, flash, halted, midiOut, error }
//   await p.start();                         // in a user gesture (the autoplay policy)
//   await p.load(wasmUrl, flashBytes | null)
//   p.panel(msg); p.midi(bytes); p.flashReq(); p.setVolume(0..1)

const abs = (p) => new URL(p, document.baseURI).href;

async function fetchBytes(url, what) {
  let r;
  try { r = await fetch(url); } catch (e) { throw new Error(`${what} could not be downloaded (${e.message || e}).`); }
  if (!r.ok) throw new Error(`${what} could not be downloaded (HTTP ${r.status}).`);
  return new Uint8Array(await r.arrayBuffer());
}

export class Player {
  constructor(events = {}) {
    this.ev = events;
    this.ctx = null; this.node = null; this.out = null; this.volume = 1;
    this.engineAdded = null;
    this.wasm = null;            // { url, bytes } the last fetched module
  }
  emit(name, ...a) { if (this.ev[name]) this.ev[name](...a); }
  get running() { return !!this.ctx && this.ctx.state === "running"; }

  // the context and the host node; resolves once audio runs (call it from a user gesture; resolves false if the
  // browser still holds the audio back)
  async start() {
    if (!this.ctx) {
      const AC = window.AudioContext || window.webkitAudioContext;
      if (!AC || !("audioWorklet" in AC.prototype)) throw new Error("This browser has no AudioWorklet: use a current Chrome, Edge, Firefox or Safari.");
      this.ctx = new AC({ latencyHint: "interactive" });
      await this.ctx.audioWorklet.addModule(abs("worklet/host.js"));
      this.node = new AudioWorkletNode(this.ctx, "fm1-host", { numberOfInputs: 0, numberOfOutputs: 1, outputChannelCount: [2] });
      this.node.port.onmessage = (e) => this.onMessage(e.data);
      this.out = this.ctx.createGain();
      this.out.gain.value = this.volume;
      this.node.connect(this.out);
      this.out.connect(this.ctx.destination);
    }
    if (this.ctx.state !== "running") { try { await this.ctx.resume(); } catch (_) { /* (no gesture yet) */ } }
    return this.ctx.state === "running";
  }
  // a context that runs without a gesture (a test browser, a page the visitor already clicked): try quietly
  async tryAutostart() {
    try {
      const AC = window.AudioContext || window.webkitAudioContext;
      const probe = new AC();
      const ok = probe.state === "running";
      await probe.close();
      return ok ? this.start() : false;
    } catch (_) { return false; }
  }

  onMessage(m) {
    if (!m || !m.type) return;
    if (m.type === "frame") this.emit("frame", m.pixels, m.seq);
    else if (m.type === "leds") this.emit("leds", m.levels, m.playGreen);
    else if (m.type === "info") this.emit("info", m);
    else if (m.type === "flash") this.emit("flash", m);
    else if (m.type === "halted") this.emit("halted", m.code);
    else if (m.type === "midiOut") this.emit("midiOut", m.bytes);
    else if (m.type === "error") this.emit("error", m.message);
    else if (m.type === "load") this.emit("load", m);
  }

  // the engine's module in the worklet: a shim that imports it statically and registers it with host.js
  addEngine() {
    if (this.engineAdded) return this.engineAdded;
    const src = `import * as E from ${JSON.stringify(abs("engine/engine.js"))};\n(globalThis.FM1_ENGINES = globalThis.FM1_ENGINES || {}).native = E;\n`;
    const url = URL.createObjectURL(new Blob([src], { type: "text/javascript" }));
    const p = this.ctx.audioWorklet.addModule(url).finally(() => URL.revokeObjectURL(url));
    this.engineAdded = p;
    p.catch(() => { this.engineAdded = null; });
    return p;
  }

  // load FIZZ (wasmUrl: fizz.wasm, versioned so a new build is never a cached old one) from a flash image or erased
  async load(wasmUrl, flash = null) {
    if (!this.ctx) throw new Error("start() first");
    await this.addEngine();
    if (!this.wasm || this.wasm.url !== wasmUrl) this.wasm = { url: wasmUrl, bytes: await fetchBytes(abs(wasmUrl), "FIZZ") };
    this.node.port.postMessage({ type: "load", engine: "native", files: { wasm: this.wasm.bytes.slice().buffer },
                                 flash: flash ? new Uint8Array(flash) : null, options: {} });
  }
  setVolume(v) {
    this.volume = Math.max(0, Math.min(1, +v || 0));
    if (this.out) this.out.gain.setTargetAtTime(this.volume, this.ctx.currentTime, 0.015);
  }
  get sampleRate() { return this.ctx ? this.ctx.sampleRate : 0; }
  panel(p) { if (this.node) this.node.port.postMessage({ type: "panel", ...p }); }
  midi(bytes) { if (this.node) this.node.port.postMessage({ type: "midi", bytes: Uint8Array.from(bytes) }); }
  flashReq() { if (this.node) this.node.port.postMessage({ type: "flashReq" }); }
}
