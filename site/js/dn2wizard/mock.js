// SPDX-License-Identifier: GPL-3.0-only
// A pretend Digitone II for testing the wizard without one (dn2-wizard.html?mock=1): a WebMIDI input / output pair
// and an audio stream. It keeps a kit per pattern (16 track sounds, synthetic records from dn2pst.js's builder),
// applies CC and NRPN to the track whose channel they arrive on (the record's 28 + 2 x id, by the laws in MOCK_LAWS),
// answers the dump requests (0x60, 0x62, 0x63), the API's Device and Version and the identity request, and plays a
// saw wave for the notes. Every message it receives is checked against what the wizard may send (sysex.js allowed).
import { PARAMS } from "./params.js";
import { allowed, dumpMessage, parseDump, parseApi, encode87, IDENTITY_REQUEST, PATTERN_BYTES, KIT_SOUND } from "./sysex.js";
import { dn2RecordFromParams } from "../dn2pst.js";

const ccMap = new Map(PARAMS.filter((p) => p.cc !== null && p.cc !== undefined).map((p) => [p.cc, p]));
const nrpnMap = new Map(PARAMS.map((p) => [(p.nrpn[0] << 7) | p.nrpn[1], p]));
// the mock's own laws (what the analysis must find again): CC v and NRPN v14 -> the stored u16
export const MOCK_LAWS = {
  cc: (p, v) => p.kind === "enum" ? Math.floor(v * p.n / 128) << 8 : p.kind === "rpair" ? Math.floor(v * 361 / 128) * 2 : v << 8,
  nrpn: (p, v) => p.kind === "enum" ? Math.min(v >> 7, p.n - 1) << 8 : p.kind === "rpair" ? Math.min(v, 360) * 2 : v << 1,
};
const INIT = { algo: 1, ratioC: 1, ratioA: 1, ratioB: 3 * 19 + 3, harm: 0, dtun: 0, fdbk: 0, mix: 0, "envA.atk": 0, "envA.dec": 32,
  "envA.end": 0, "envA.lev": 0, "envB.atk": 0, "envB.dec": 32, "envB.end": 0, "envB.lev": 0, "envA.trig": 1, "envA.reset": 1,
  "envB.trig": 1, "envB.reset": 1, phrt: 1, "filter.freq": 127, "filter.reso": 0, "filter.env": 0, "filter.dec": 64, "filter.width": 127,
  "amp.atk": 0, "amp.dec": 64, "amp.sus": 127, "amp.rel": 24, "amp.vol": 100, "amp.mode": 1, "amp.reset": 1, "filter.reset": 1 };

export function createMockDevice({ product = 0x15, log = () => {} } = {}) {
  const kits = new Map(), pool = dn2RecordFromParams(INIT, { name: "POOL" });
  const kitOf = (n) => {
    if (!kits.has(n)) {
      const k = new Uint8Array(10752);
      for (let t = 0; t < 16; t++) k.set(dn2RecordFromParams(INIT, { name: `T${t + 1} INIT` }), KIT_SOUND(t));
      kits.set(n, k);
    }
    return kits.get(n);
  };
  const dev = { pattern: 0, violations: [], received: 0, notes: new Map(), audio: null, listeners: new Set(), lastNrpn: {} };
  const rec = (t) => kitOf(dev.pattern).subarray(KIT_SOUND(t), KIT_SOUND(t) + 359);
  const put = (t, id, raw) => { const r = rec(t), o = 28 + 2 * id; r[o] = (raw >> 8) & 255; r[o + 1] = raw & 255; };
  const reply = (bytes) => setTimeout(() => {
    const ev = { data: bytes };
    for (const l of dev.listeners) l(ev);
    if (input.onmidimessage) input.onmidimessage(ev);
  }, 1);
  function handle(m) {
    dev.received++;
    const ch = m[0] & 0x0f;
    if (!allowed(m, m[0] === 0xf0 ? 0 : ch)) dev.violations.push(Array.from(m.slice(0, 12)));
    if (m[0] === 0xf0) {
      if (m.length === 6 && IDENTITY_REQUEST.every((b, i) => b === m[i]))
        return reply(Uint8Array.from([0xf0, 0x7e, 0x00, 0x06, 0x02, 0x00, 0x20, 0x3c, 0x00, 0x2b, 0x00, 0x00, 0x01, 0x0b, 0x00, 0x00, 0xf7]));
      const a = parseApi(m);
      if (a) {
        const enc = new TextEncoder();
        const body = a.code === 1 ? [43, 4, 0x01, 0x02, 0x09, 0x10, ...enc.encode("Digitone II (mock)"), 0] :
          a.code === 2 ? [...enc.encode("0059"), 0, ...enc.encode("1.11"), 0] : null;
        if (!body) return;
        const p = Uint8Array.from([0x20, 0x00, a.msgId >> 8, a.msgId & 255, a.code | 0x80, ...body]);
        return reply(Uint8Array.from([0xf0, 0x00, 0x20, 0x3c, 0x10, 0x00, ...encode87(p), 0xf7]));
      }
      const d = parseDump(m);
      if (!d || d.product !== product) return;
      if (d.type === 0x62) return reply(dumpMessage(0x52, d.objNr, kitOf(d.objNr)));
      if (d.type === 0x60) { const pk = new Uint8Array(PATTERN_BYTES + 10752); pk.set(kitOf(d.objNr), PATTERN_BYTES); return reply(dumpMessage(0x50, d.objNr, pk)); }
      if (d.type === 0x63) return reply(dumpMessage(0x53, d.objNr, pool));
      return;
    }
    for (let i = 0; i < m.length; i += 3) {                // (an NRPN arrives as four CCs in one message)
      const st = m[i] & 0xf0, c = m[i] & 0x0f, a = m[i + 1], b = m[i + 2];
      if (st === 0x90 && b > 0) { dev.notes.set(a, b); if (dev.audio) dev.audio.on(a, b); }
      else if (st === 0x80 || (st === 0x90 && b === 0)) { dev.notes.delete(a); if (dev.audio) dev.audio.off(a); }
      else if (st === 0xb0) {
        const s = dev.lastNrpn[c] = dev.lastNrpn[c] || {};
        if (a === 123) { dev.notes.clear(); if (dev.audio) dev.audio.off(-1); }
        else if (a === 99) s.msb = b; else if (a === 98) s.lsb = b; else if (a === 6) s.hi = b;
        else if (a === 38) { const p = nrpnMap.get((s.msb << 7) | s.lsb); if (p) put(c, p.id, MOCK_LAWS.nrpn(p, (s.hi << 7) | b)); }
        else { const p = ccMap.get(a); if (p) put(c, p.id, MOCK_LAWS.cc(p, b)); }
      }
    }
  }
  const input = { id: "mock-in", name: "Elektron Digitone II (mock)", manufacturer: "mock", type: "input", state: "connected",
    connection: "open", onmidimessage: null,
    addEventListener: (k, f) => { if (k === "midimessage") dev.listeners.add(f); },
    removeEventListener: (k, f) => { if (k === "midimessage") dev.listeners.delete(f); } };
  const output = { id: "mock-out", name: "Elektron Digitone II (mock)", manufacturer: "mock", type: "output", state: "connected",
    connection: "open", send: (d) => handle(Uint8Array.from(d)) };
  dev.access = { inputs: new Map([[input.id, input]]), outputs: new Map([[output.id, output]]), sysexEnabled: true,
    onstatechange: null, addEventListener() {}, removeEventListener() {} };
  dev.input = input; dev.output = output;
  dev.record = (pattern, t) => kitOf(pattern).subarray(KIT_SOUND(t), KIT_SOUND(t) + 359);
  // the steps that need a person: done to the pretend device instead
  dev.perform = (action, t) => {
    const r = rec(t);
    if (action.filterMachine !== undefined) r[245] = action.filterMachine;
    if (action.machine !== undefined) r[244] = action.machine;
    if (action.octave !== undefined) r[251] = action.octave & 255;
    if (action.mod) { const [k, depth, id] = action.mod; r[252 + 3 * k] = depth >> 8 & 255; r[253 + 3 * k] = depth & 255; r[254 + 3 * k] = id; }
    log(`mock: ${JSON.stringify(action)} on track ${t + 1}`);
  };
  // the audio: a saw for the notes (the newest one), through a stream the recorder opens
  dev.makeStream = (ctx) => {
    const osc = ctx.createOscillator(), g = ctx.createGain(), dst = ctx.createMediaStreamDestination();
    osc.type = "sawtooth"; g.gain.value = 0;
    osc.connect(g); g.connect(dst); osc.start();
    dev.audio = {
      on: (n, v) => { osc.frequency.setValueAtTime(440 * 2 ** ((n - 69) / 12), ctx.currentTime); g.gain.setTargetAtTime(0.4 * v / 127, ctx.currentTime, 0.005); },
      off: (n) => { if (n < 0 || !dev.notes.size) g.gain.setTargetAtTime(0, ctx.currentTime, 0.05); },
    };
    return dst.stream;
  };
  return dev;
}
