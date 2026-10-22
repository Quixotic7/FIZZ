// SPDX-License-Identifier: GPL-3.0-only
// The wire to the Digitone II: a paced queue of outgoing messages (each checked against what the wizard may send:
// sysex.js allowed), the notes it has on (for all notes off), and requests that wait for their answer with a timeout.
import { allowed, dumpRequest, parseDump, ANSWER } from "./sysex.js";

const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
// one send() may hold several channel messages (an NRPN is four CCs): split them for the check
function split(b) {
  if (b[0] === 0xf0) return [b];
  const out = [];
  for (let i = 0; i < b.length;) { const n = (b[i] & 0xf0) === 0xc0 || (b[i] & 0xf0) === 0xd0 ? 2 : 3; out.push(b.subarray(i, i + n)); i += n; }
  return out;
}

export class Link {
  constructor({ channel, gapMs = 3, log = () => {} }) {
    this.ch = channel; this.gap = gapMs; this.log = log;
    this.input = null; this.output = null; this.waiters = []; this.notes = new Set();
    this.last = 0; this.chain = Promise.resolve(); this.count = { cc: 0, nrpn: 0, note: 0, request: 0, other: 0 };
    this.onMessage = (e) => this.incoming(e.data);
  }
  setPorts(input, output) {
    if (this.input) this.input.removeEventListener("midimessage", this.onMessage);
    this.input = input; this.output = output;
    if (input) input.addEventListener("midimessage", this.onMessage);
  }
  get connected() {
    return !!(this.input && this.output && this.input.state !== "disconnected" && this.output.state !== "disconnected");
  }
  incoming(d) {
    const m = d instanceof Uint8Array ? d : Uint8Array.from(d);
    for (const w of this.waiters.slice()) if (w.match(m)) { this.waiters.splice(this.waiters.indexOf(w), 1); w.resolve(m); }
  }
  // queue bytes (paced: at least gapMs between sends); refused if any part is not something the wizard may send
  send(bytes) {
    const b = bytes instanceof Uint8Array ? bytes : Uint8Array.from(bytes);
    for (const m of split(b)) if (!allowed(m, this.ch)) throw new Error(`refused to send ${Array.from(m.slice(0, 12), (x) => x.toString(16).padStart(2, "0")).join(" ")}`);
    const p = this.chain.catch(() => {}).then(async () => {
      if (!this.connected) throw new Error("the Digitone II is not connected");
      const wait = this.last + this.gap - performance.now();
      if (wait > 0) await sleep(wait);
      this.output.send(Array.from(b));
      this.last = performance.now();
    });
    this.chain = p;
    return p;
  }
  cc(n, v) { this.count.cc++; return this.send([0xb0 | this.ch, n, Math.max(0, Math.min(127, v))]); }
  nrpn(msb, lsb, v14) {
    this.count.nrpn++;
    const v = Math.max(0, Math.min(16383, v14)), c = 0xb0 | this.ch;
    return this.send([c, 99, msb, c, 98, lsb, c, 6, v >> 7, c, 38, v & 127]);
  }
  noteOn(n, vel = 100) { this.count.note++; this.notes.add(n); return this.send([0x90 | this.ch, n, vel]); }
  noteOff(n) { this.notes.delete(n); return this.send([0x80 | this.ch, n, 0]); }
  async allNotesOff() {
    if (!this.connected) return;
    for (const n of [...this.notes]) await this.noteOff(n).catch(() => {});
    await this.send([0xb0 | this.ch, 123, 0]).catch(() => {});
  }
  // send a message and wait for the first incoming one that match() accepts (null on a timeout)
  async request(bytes, match, timeoutMs) {
    let w;
    const got = new Promise((resolve) => { w = { match, resolve }; this.waiters.push(w); });
    await this.send(bytes);
    const t = sleep(timeoutMs).then(() => null);
    const r = await Promise.race([got, t]);
    if (!r) this.waiters.splice(this.waiters.indexOf(w), 1);
    return r;
  }
  // a dump request (0x60 / 0x62 / 0x63, objNr) -> the parsed answer, one retry on a timeout or a bad checksum
  async dump(type, objNr, timeoutMs) {
    for (let k = 0; k < 2; k++) {
      this.count.request++;
      const m = await this.request(dumpRequest(type, objNr), (x) => x[0] === 0xf0 && x[4] === 0x15 && x[6] === ANSWER[type], timeoutMs);
      const d = m && parseDump(m);
      if (d && d.ok) return { ...d, raw: m };
      this.log(m ? `dump 0x${type.toString(16)}: a bad answer (${d ? d.why : "unreadable"}), asking again` : `dump 0x${type.toString(16)}: no answer in ${timeoutMs} ms${k ? "" : ", asking again"}`);
    }
    return null;
  }
}
