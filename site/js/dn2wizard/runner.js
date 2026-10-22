// SPDX-License-Identifier: GPL-3.0-only
// Runs the plan (plan.js) against the Digitone II: sets each parameter, reads the track's sound back, plays and records
// the audio steps, restores what it changed. Pause, Resume and Stop between steps; a step the device dropped out of
// (unplugged, a port gone) pauses the run, and Resume runs that step again: nothing done before it is lost. Every
// result stays in memory until the zip is made (results()).
import { param, recordOffset } from "./params.js";
import { IDENTITY_REQUEST, isIdentityReply, apiMessage, parseApi, readDevice, readVersion, trackRecord, DUMP } from "./sysex.js";
import { wav16, zipStore } from "./zipwav.js";

export const WIZARD_VERSION = "1.0";
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
const hex = (b) => Array.from(b, (x) => x.toString(16).padStart(2, "0")).join(" ");
const u16 = (r, id) => (r[recordOffset(id)] << 8) | r[recordOffset(id) + 1];
const safe = (s) => String(s).replace(/[^A-Za-z0-9+.-]+/g, "_").replace(/^_+|_+$/g, "").slice(0, 48);
const diff = (a, b) => { const o = []; if (a && b) for (let i = 0; i < a.length; i++) if (a[i] !== b[i]) o.push(i); return o; };

export class Runner {
  // cfg: {track (0-based), pattern (0-based), fast, auto}; ui: {update(), log(line), ask(text) -> "continue" | "skip"}
  constructor({ link, recorder, plan, cfg, ui, mock = null }) {
    Object.assign(this, { link, recorder, plan, cfg, ui, mock });
    this.i = 0; this.state = "idle"; this.files = []; this.steps = []; this.lines = [];
    this.device = {}; this.kind = null; this.base = null; this.ref = null; this.cur = null; this.touched = new Set();
    this.learned = new Map(); this.audioOnly = false; this.fails = 0; this.n = 0; this.t0 = 0; this.avg = {};
    this.pauseMsg = "";
  }
  log(s) { const l = `${new Date().toISOString().slice(11, 23)} ${s}`; this.lines.push(l); this.ui.log(l); }
  file(name, data) { this.files.push({ name, data }); return name; }
  get settle() { return this.cfg.fast ? 0 : 40; }
  ms(x) { return Math.round(this.cfg.fast ? x * 0.15 : x); }
  progress() { return { i: this.i, n: this.plan.length, state: this.state, step: this.plan[this.i], msg: this.pauseMsg, eta: this.eta() }; }
  eta() {
    let ms = 0;
    for (let j = this.i; j < this.plan.length; j++) {
      const s = this.plan[j], a = this.avg[s.k];
      ms += s.k === "audio" ? this.ms(s.ms) + 150 : s.k === "manual" ? (this.mock || this.cfg.auto ? 0 : 20000) : a ? a.t / a.n : 120;
    }
    return ms;
  }

  async start() { if (this.state === "running") return; this.state = "running"; this.t0 = this.t0 || Date.now(); this.ui.update(); await this.loop(); }
  pause(msg = "Paused.") { if (this.state === "running") { this.state = "pausing"; this.pauseMsg = msg; this.ui.update(); } }
  resume() { if (this.state === "paused") { this.pauseMsg = ""; this.start(); } }
  async stop() {
    const was = this.state;
    this.state = "stopping"; this.ui.update();
    if (was !== "running" && was !== "pausing") await this.finish("stopped");
  }
  async finish(how) {
    await this.link.allNotesOff().catch(() => {});
    if (how === "stopped" && this.touched.size && this.link.connected) {
      this.log(`stopped: the ${this.touched.size} parameter(s) this item changed go back to the baseline`);
      await this.restore([...this.touched], false).catch(() => {});
    }
    this.state = how === "stopped" ? "stopped" : "done";
    this.log(`${how}: ${this.i} of ${this.plan.length} steps, ${this.steps.length} results, ${Math.round((Date.now() - this.t0) / 1000)} s`);
    this.ui.update();
  }
  async loop() {
    while (this.i < this.plan.length) {
      if (this.state === "stopping") return this.finish("stopped");
      if (this.state === "pausing") { await this.link.allNotesOff().catch(() => {}); this.state = "paused"; this.ui.update(); return; }
      const s = this.plan[this.i], t = performance.now();
      try {
        await this.exec(s);
        const a = this.avg[s.k] = this.avg[s.k] || { t: 0, n: 0 };
        a.t += performance.now() - t; a.n++;
        this.i++;
        this.ui.update();
      } catch (e) {
        await this.link.allNotesOff().catch(() => {});
        this.log(`step ${this.i + 1} (${s.k} ${s.item}): ${e.message}`);
        if (e.fatal) { this.pauseMsg = e.message; return this.finish("stopped"); }
        this.state = "paused";
        this.pauseMsg = this.link.connected ? `${e.message}. Resume tries this step again.` :
          "The Digitone II is not connected. Plug it back in (choose its ports again if they changed), then Resume.";
        this.ui.update();
        return;
      }
    }
    return this.finish("done");
  }

  // ---- reading the sound
  async read(kind = this.kind) {
    const d = await this.link.dump(kind, this.cfg.pattern, kind === DUMP.PATTERN_KIT ? 6000 : 2500);
    if (!d) { if (++this.fails >= 3) { this.fails = 0; throw new Error("the Digitone II stopped answering dump requests"); } return null; }
    this.fails = 0;
    return d;
  }
  async readRecord() {
    if (this.audioOnly) return null;
    const d = await this.read();
    return d ? trackRecord(d, this.cfg.track) : null;
  }
  async setParam(id, via, value) {
    const p = param(id);
    this.touched.add(id);
    if (via === "stored") { value = this.ccFor(id, value); via = "cc"; }
    if (via === "cc" && p.cc !== null && p.cc !== undefined) await this.link.cc(p.cc, value);
    else await this.link.nrpn(p.nrpn[0], p.nrpn[1], via === "cc" ? value << 7 : value);
    if (this.settle) await sleep(this.settle);
    return { via, value };
  }
  // the CC whose read-back stored this coarse value (the sweep's own finding; the middle one of a run)
  ccFor(id, coarse) {
    const l = (this.learned.get(id) || []).filter((x) => x.via === "cc" && x.raw >> 8 === coarse).map((x) => x.value);
    return l.length ? l[Math.floor(l.length / 2)] : coarse;
  }
  async restore(ids, verify) {
    if (!this.base) return;
    for (const id of ids) { const p = param(id); await this.link.nrpn(p.nrpn[0], p.nrpn[1], u16(this.base, id) >> 1); }
    if (this.settle) await sleep(this.settle);
    if (!verify || this.audioOnly) return;
    let r = await this.readRecord();
    const off = (rr) => ids.filter((id) => rr && u16(rr, id) !== u16(this.base, id));
    let bad = off(r);
    if (bad.length) {                                     // the NRPN law is not stored >> 1 there: the CC the sweep found
      this.log(`restore: ${bad.map((id) => param(id).name).join(", ")} not back by NRPN; by CC`);
      for (const id of bad) { const p = param(id); if (p.cc !== null && p.cc !== undefined) await this.link.cc(p.cc, this.ccFor(id, u16(this.base, id) >> 8)); }
      if (this.settle) await sleep(this.settle);
      r = await this.readRecord();
      bad = off(r);
      if (bad.length) this.log(`restore: still not at the baseline: ${bad.map((id) => `${param(id).name} ${u16(r, id)} (was ${u16(this.base, id)})`).join(", ")}`);
    }
    this.cur = r;
  }
  saveRecord(r, label) { return r ? this.file(`dumps/${String(this.steps.length).padStart(4, "0")}_${safe(label)}.bin`, r) : null; }

  // ---- one step
  async exec(s) {
    const L = this.link, cfg = this.cfg;
    switch (s.k) {
    case "identify": {
      const id = await L.request(IDENTITY_REQUEST, isIdentityReply, 800);
      this.device.identity = id ? hex(id) : null;
      const dv = await L.request(apiMessage(0x2010, 0x01), (m) => { const a = parseApi(m); return !!a && a.respId === 0x2010; }, 1500);
      if (dv) try { this.device.api = readDevice(parseApi(dv).body); } catch (_) { this.device.api = { raw: hex(dv) }; }
      const vr = await L.request(apiMessage(0x2011, 0x02), (m) => { const a = parseApi(m); return !!a && a.respId === 0x2011; }, 1500);
      if (vr) try { Object.assign(this.device, readVersion(parseApi(vr).body)); } catch (_) { this.device.versionRaw = hex(vr); }
      this.log(`device: ${this.device.api ? this.device.api.name : "(no Device answer)"}, OS ${this.device.version || "?"} (build ${this.device.build || "?"}), identity ${this.device.identity ? "answered" : "no answer"}`);
      return;
    }
    case "baseline": {
      this.full = {};
      for (const k of [DUMP.KIT, DUMP.PATTERN_KIT]) {
        const d = await this.read(k);
        if (!d) { this.log(`baseline: no answer to the ${k === DUMP.KIT ? "kit" : "pattern + kit"} request`); continue; }
        this.full[k] = d;
        this.file(`dumps/full/baseline_${k === DUMP.KIT ? "kit" : "patternkit"}.syx`, d.raw);
      }
      const d = this.full[DUMP.KIT] || this.full[DUMP.PATTERN_KIT];
      if (!d) { const e = new Error("the Digitone II does not answer a kit or pattern request: is SysEx on, and the right ports chosen?"); e.fatal = !this.mock && cfg.auto; throw e; }
      const r = trackRecord(d, cfg.track);
      if (!r) throw new Error(`no sound record for track ${cfg.track + 1} in the answer`);
      this.base = r;
      this.saveRecord(r, "baseline");
      this.steps.push({ n: this.steps.length, item: s.item, kind: "baseline", record: `dumps/${String(this.steps.length).padStart(4, "0")}_baseline.bin`, version: (r[4] << 24 | r[5] << 16 | r[6] << 8 | r[7]) >>> 0 });
      let name = ""; for (let i = 12; i < 28 && r[i]; i++) name += String.fromCharCode(r[i]);
      this.log(`baseline: track ${cfg.track + 1}, "${name}", machine ${r[244]} (0 = FM TONE), filter machine ${r[245]}, record version ${this.steps[this.steps.length - 1].version}`);
      if (r[244] !== 0) throw new Error(`track ${cfg.track + 1} is not on FM TONE (machine ${r[244]}): set it to an INIT FM TONE sound`);
      return;
    }
    case "calibrate": {                                  // which answer shows a live edit: FDBK moved, read back, put back
      for (const k of [DUMP.KIT, DUMP.PATTERN_KIT]) {
        const b = this.full[k];
        if (!b) continue;
        const before = Array.from({ length: 16 }, (_, t) => trackRecord(b, t));
        const v = ((before[cfg.track] ? before[cfg.track][recordOffset(39)] : 0) + 23) & 127;
        await L.cc(param(39).cc, v);
        await sleep(this.cfg.fast ? 5 : 80);
        const a = await this.read(k);
        const changed = a ? before.map((r, t) => (r && diff(r, trackRecord(a, t)).length ? t : -1)).filter((t) => t >= 0) : [];
        await L.nrpn(param(39).nrpn[0], param(39).nrpn[1], u16(trackRecord(b, changed.length === 1 ? changed[0] : cfg.track), 39) >> 1);
        await L.cc(param(39).cc, trackRecord(b, changed.length === 1 ? changed[0] : cfg.track)[recordOffset(39)]);
        if (!changed.length) { this.log(`calibrate: the ${k === DUMP.KIT ? "kit" : "pattern + kit"} answer does not show the edit`); continue; }
        if (!changed.includes(cfg.track)) { this.log(`calibrate: the edit landed on track ${changed[0] + 1}, not ${cfg.track + 1}: using track ${changed[0] + 1}`); cfg.track = changed[0]; }
        this.kind = k;
        this.base = trackRecord(b, cfg.track);
        this.log(`calibrate: the ${k === DUMP.KIT ? "kit (0x62)" : "pattern + kit (0x60)"} answer shows the track's live sound; FDBK CC ${param(39).cc} moved byte ${diff(before[cfg.track], trackRecord(a, cfg.track)).join(", ")}`);
        this.steps.push({ n: this.steps.length, item: s.item, kind: "calibrate", dump: k, track: cfg.track, offsets: diff(before[cfg.track], trackRecord(a, cfg.track)) });
        this.file(`dumps/full/calibrate_${k === DUMP.KIT ? "kit" : "patternkit"}.syx`, a.raw);
        return;
      }
      this.audioOnly = true;
      this.log("calibrate: NEITHER answer shows a live edit: the run goes on with the recordings only (the dumps would all be the same)");
      this.steps.push({ n: this.steps.length, item: s.item, kind: "calibrate", dump: null });
      return;
    }
    case "ref": {
      this.touched = new Set();
      this.ref = await this.readRecord();
      const f = this.saveRecord(this.ref, `${s.item}_ref`);
      this.steps.push({ n: this.steps.length, item: s.item, kind: "ref", record: f });
      return;
    }
    case "probe": {
      if (this.audioOnly) return;
      const p = param(s.id), sent = await this.setParam(s.id, s.via, s.value);
      const r = await this.readRecord();
      const st = { n: this.steps.length, item: s.item, kind: "probe", id: s.id, name: p.name, via: sent.via, value: sent.value,
        cc: sent.via === "cc" ? p.cc : null, nrpn: sent.via === "nrpn" ? p.nrpn : null, ...(s.tag ? { tag: s.tag } : {}) };
      if (r) {
        st.raw = u16(r, s.id);
        st.changed = diff(this.ref, r).slice(0, 24);
        st.record = this.saveRecord(r, `${s.item}_${p.name}_${sent.via}${sent.value}${s.tag ? "_" + s.tag : ""}`);
        (this.learned.get(s.id) || this.learned.set(s.id, []).get(s.id)).push({ via: sent.via, value: sent.value, raw: st.raw });
      } else st.error = "no answer";
      this.steps.push(st);
      return;
    }
    case "set": { const r = await this.setParam(s.id, s.via, s.value); this.steps.push({ n: this.steps.length, item: s.item, kind: "set", id: s.id, name: param(s.id).name, ...r }); return; }
    case "restore": return this.restore(s.ids === "touched" ? [...this.touched] : s.ids, s.verify).then(() => { if (s.ids === "touched") this.touched = new Set(); });
    case "audio": {
      if (!this.recorder || !this.recorder.open_) { this.log(`audio ${s.label}: no audio input, skipped`); return; }
      const ev = [];
      for (const [note, vel, on, off] of s.notes) { ev.push([this.ms(on), 1, note, vel]); ev.push([this.ms(off), 0, note, 0]); }
      ev.sort((a, b) => a[0] - b[0] || a[1] - b[1]);
      this.recorder.start();
      const t0 = performance.now();
      await sleep(this.cfg.fast ? 5 : 60);                // (a little silence before the first note)
      const t1 = performance.now();
      for (const [at, on, note, vel] of ev) {
        const w = t1 + at - performance.now();
        if (w > 0) await sleep(w);
        await (on ? L.noteOn(note, vel) : L.noteOff(note));
      }
      const w = t0 + this.ms(s.ms) - performance.now();
      if (w > 0) await sleep(w);
      const x = this.recorder.stop();
      await L.allNotesOff();
      let peak = 0; for (const v of x) peak = Math.max(peak, Math.abs(v));
      const f = this.file(`audio/${String(this.steps.length).padStart(4, "0")}_${safe(s.item)}_${safe(s.label)}.wav`, wav16(x, this.recorder.rate));
      this.steps.push({ n: this.steps.length, item: s.item, kind: "audio", label: s.label, audio: f, rate: this.recorder.rate,
        notes: s.notes.map(([n, v, on, off]) => ({ note: n, velocity: v, onMs: this.ms(on), offMs: this.ms(off) })), ms: this.ms(s.ms),
        peakDb: peak > 0 ? +(20 * Math.log10(peak)).toFixed(1) : null });
      return;
    }
    case "manual": {
      if (this.mock) this.mock.perform(s.action, cfg.track);
      else if (cfg.auto) { this.log(`skipped (auto): ${s.text}`); this.skipDump = true; return; }
      else if ((await this.ui.ask(s.text)) === "skip") { this.log(`skipped: ${s.text}`); this.skipDump = true; return; }
      this.skipDump = false;
      this.steps.push({ n: this.steps.length, item: s.item, kind: "manual", text: s.text });
      return;
    }
    case "dump": {
      if (this.skipDump) { this.skipDump = false; return; }
      if (this.audioOnly) return;
      const d = await this.read();
      const r = d && trackRecord(d, cfg.track);
      if (d) this.file(`dumps/full/${String(this.steps.length).padStart(4, "0")}_${safe(s.item)}_${safe(s.label)}.syx`, d.raw);
      this.steps.push({ n: this.steps.length, item: s.item, kind: "dump", label: s.label, record: this.saveRecord(r, `${s.item}_${s.label}`),
        changed: r ? diff(this.ref, r).slice(0, 64) : null });
      return;
    }
    default: throw new Error(`unknown step ${s.k}`);
    }
  }

  // the results as a zip: manifest.json, dumps/, audio/, log.txt
  zip() {
    const manifest = {
      wizard: "FIZZ Digitone II harvest", version: WIZARD_VERSION, date: new Date().toISOString(), mock: !!this.mock,
      device: this.device, settings: { channel: this.link.ch + 1, track: this.cfg.track + 1, pattern: this.cfg.pattern,
        dump: this.kind === DUMP.KIT ? "kit 0x62" : this.kind === DUMP.PATTERN_KIT ? "pattern+kit 0x60" : null, audioOnly: this.audioOnly },
      state: this.state, stepsDone: this.i, stepsPlanned: this.plan.length, sent: this.link.count,
      record: { offsetRule: "28 + 2 x id (u16be: coarse, fine)", size: 359 }, steps: this.steps,
    };
    return zipStore([{ name: "manifest.json", data: JSON.stringify(manifest, null, 1) }, ...this.files, { name: "log.txt", data: this.lines.join("\n") + "\n" }]);
  }
}
