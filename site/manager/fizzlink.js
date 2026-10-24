// SPDX-License-Identifier: GPL-3.0-only
// The manager's link to an FM-1 running FIZZ (docs/WEB-MANAGER-PLAN.md §2, §3): one queue for both of FIZZ's SysEx
// channels, over Web MIDI (or anything shaped like it: the tests' simulated FIZZ, site/manager/simfizz.js).
//   - the semantic one, FIZZ's own frames (F0 7D 46 5A 01 ..: docs/SYSEX.md): IDENTITY, LIST, DUMP, STORE;
//   - the bulk one, the update's framing (F0 pack7(00 59 46 len16 00 body ~sum) F7: firmware/app/app.c app_usb_cmd),
//     as firmware/tools/fizz_xdata.py backup / restore use it: FZBI (the store's offset, size, CRC-32), FZBR off n
//     (n <= 256 bytes of it: FZBD off <bytes>), FZSR len crc (the restore: the unit pulls the image twice with the
//     update's 512-byte read requests, cmd 0x30, then answers FZOK, or FZER and a code).
// One request outstanding at a time (the unit takes one inbound SysEx frame a main-loop pass and drops a frame that
// arrives while one waits: fm1-base bsp/chip/usb.c sysex_byte), except the backup's reads, which may be pipelined
// (depth > 1: a dropped request is sent again; the replies are matched by offset; the CRC-32 checks the whole).
// The link reads all the time (a unit whose replies are not read stalls its main loop: plan §3.3).
//
//   await findFizz(access, {log}) -> FizzLink | throws Error {code: "notfound" | "update" | "notfizz", detail}
//   link.identity() -> "0.11.0-beta.2" | null (FIZZ does not answer: no data blob, or safe mode)
//   link.list({factory, pool}) -> [{source, slot, id, name, tags, imported, inPool}]
//   link.dumpAll(source, total, onProgress) -> [frame]          link.dump(source, slot) -> frame | null
//   link.store(frame) -> {status, statusName, pool, slot}
//   link.storeInfo() -> {lo, size, crc} | null (a FIZZ before P2.13)
//   link.backup({depth, onProgress, signal, onRead}) -> {image, lo, size, crc, stats}       (throws {code})
//   link.restore(image, {onProgress, onPhase, signal}) -> {ok: true} | throws {code: "fzer", fzer, detail}
//   link.close(); link.onLost = () => {}; link.log = (line) => {}
import { Updater, IS_OTA, frame, response, parseRequest, unpack7 } from "../install/fm1lib.js";
import { identity, listRequest, parseList, dumpRequest, parseDump, parseReply, SOURCES } from "../js/fizz_sysex.js";
import { crc32, hex8 } from "./fizzbak.js";

export const FIZZ_PREFIX = "FM-1_46";            // firmware/app/fm1.toml id_prefix
export const BULK = 0x46;                        // app_usb_cmd's command byte
export const CHUNK = 256;                        // app.c FZ_BK_CHUNK
export const FZER = {                            // app.c, fizz_xdata.py --help
  10: "the FM-1 is playing: stop it first", 11: "the backup's size is not this FM-1's store",
  12: "the image did not arrive whole (nothing was changed)", 13: "a write failed: FIZZ cleared its store",
  14: "the store did not read back as the image", 1: "refused",
};
const isFizzId = (id) => !IS_OTA(id) && id.text.startsWith(FIZZ_PREFIX);
const le32 = (v) => [v & 0xFF, (v >>> 8) & 0xFF, (v >>> 16) & 0xFF, (v >>> 24) & 0xFF];
const rd32 = (b, o) => (b[o] | (b[o + 1] << 8) | (b[o + 2] << 16) | (b[o + 3] << 24)) >>> 0;
const ascii = (s) => [...s].map((c) => c.charCodeAt(0));
const err = (code, msg, extra = {}) => Object.assign(new Error(msg), { code }, extra);
const now = () => (typeof performance !== "undefined" ? performance.now() : Date.now());
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

// a reply in the update's framing from FIZZ: F0 pack7(00 59 46 len16 00 tag[4] data ~sum) F7 -> {tag, data} | null
export function parseBulk(p) {
  if (!p || p.length < 3 || p[0] !== 0xF0 || p[p.length - 1] !== 0xF7) return null;
  const u = unpack7(p.slice(1, -1));
  if (u.length < 11 || u[0] !== 0 || u[1] !== 0x59 || u[2] !== BULK) return null;
  const n = u[3] | (u[4] << 8);
  if (u.length < 7 + n || n < 4) return null;
  let s = 0;
  for (let i = 6; i < 6 + n; i++) s += u[i];
  if ((~s & 0xFF) !== u[6 + n]) return null;
  return { tag: String.fromCharCode(...u.slice(6, 10)), data: Uint8Array.from(u.slice(10, 6 + n)) };
}
export const bulkFrame = (tag, ...words) => frame(BULK, [...ascii(tag), ...words.flatMap(le32)]);

const stats = (rtts) => {
  const s = [...rtts].sort((a, b) => a - b), q = (f) => (s.length ? s[Math.min(s.length - 1, Math.floor(f * s.length))] : 0);
  return { avg: s.length ? s.reduce((a, b) => a + b, 0) / s.length : 0, median: q(0.5), p95: q(0.95), max: s.length ? s[s.length - 1] : 0 };
};

export async function findFizz(access, { log = () => {} } = {}) {
  const up = new Updater(access, log);
  let dev = await up.find(isFizzId);
  if (!dev) {
    dev = await up.find(() => true);
    if (!dev) throw err("notfound", "no FM-1 answered");
    dev.link.close();
    if (IS_OTA(dev.id)) throw err("update", `the FM-1 is in update mode (${dev.id.text})`, { detail: dev.id.text });
    throw err("notfizz", `this FM-1 runs ${dev.id.text}, not FIZZ`, { detail: dev.id.text });
  }
  return new FizzLink(dev.link, dev.id.text, dev.name);
}

const MORE = Symbol("more");

export class FizzLink {
  constructor(link, id, port) {
    Object.assign(this, { link, id, port, closed: false, cur: null, q: Promise.resolve(), log: () => {}, onLost: () => {}, rtoMin: 150 });
    this.reader();
  }

  async reader() {
    while (!this.closed) {
      const p = await this.link.read(400);
      if (this.closed) return;
      if (p) this.dispatch(p);
      else if (this.link.lost) { this.lost(); return; }
    }
  }
  lost() {
    if (this.closed) return;
    this.closed = true;
    if (this.cur) this.cur.fail(err("lost", "the FM-1 was disconnected"));
    this.onLost();
  }
  close() { this.closed = true; try { this.link.close(); } catch (_) { /* (gone) */ } }

  dispatch(p) {
    const c = this.cur;
    if (!c) return;
    let r;
    try { r = c.onFrame(p); } catch (e) { c.fail(e); return; }
    if (r === undefined) return;
    if (r === MORE) { c.arm(); return; }
    c.done(r);
  }

  // one operation: start() sends, onFrame(p) answers undefined (not its), MORE (its, keep waiting: the wait starts
  // again) or the result; on a quiet `timeout` ms, partial() (or a timeout error)
  op(start, onFrame, { timeout = 3000, partial = null } = {}) {
    const run = () => new Promise((resolve, reject) => {
      if (this.closed) { reject(err("lost", "the FM-1 is not connected")); return; }
      let t = null;
      const c = {
        onFrame,
        arm: () => { clearTimeout(t); t = setTimeout(() => { if (this.cur !== c) return; if (partial) c.done(partial()); else c.fail(err("timeout", "the FM-1 did not answer")); }, timeout); },
        done: (v) => { clearTimeout(t); if (this.cur === c) this.cur = null; resolve(v); },
        fail: (e) => { clearTimeout(t); if (this.cur === c) this.cur = null; reject(e); },
      };
      this.cur = c;
      c.arm();
      try { start(c); } catch (e) { c.fail(e); }
    });
    const p = this.q.then(run, run);
    this.q = p.catch(() => {});
    return p;
  }
  send(bytes) {
    if (!this.link.send(Array.from(bytes))) { this.lost(); throw err("lost", "the FM-1 was disconnected"); }
  }

  // ---- the semantic channel
  identity() {
    return this.op(() => this.send(identity()), (p) => {
      const r = parseReply(p);
      return r && r.kind === "identity" ? r.version.replace(/^FIZZ\s*/, "") : undefined;
    }, { timeout: 1500, partial: () => null });
  }
  list({ factory = false, pool = false } = {}) {
    const rows = [];
    return this.op(() => this.send(listRequest({ factory, pool })), (p) => {
      const l = parseList(p);
      if (!l) return undefined;
      rows.push(...l.entries);
      return l.last ? rows : MORE;
    });
  }
  dump(source, slot) {
    return this.op(() => this.send(dumpRequest({ source, slot })), (p) => {
      const d = parseDump(p);
      if (d && d.slot === slot) return d.frame;
      const r = parseReply(p);
      return r && r.kind === "error" && r.command === 0x22 ? null : undefined;
    });
  }
  // every preset of a source (DUMP slot 0), as many as LIST counted; paced by the unit's reply ring
  dumpAll(source, total, onProgress = () => {}) {
    const out = [];
    if (!total) return Promise.resolve(out);
    return this.op(() => this.send(dumpRequest({ source, slot: 0 })), (p) => {
      const d = parseDump(p);
      if (!d || (source !== "factory" && d.source !== source)) return undefined;
      out.push(d.frame);
      onProgress(out.length, total);
      return out.length >= total ? out : MORE;
    }, { timeout: 4000, partial: () => out });
  }
  store(f) {
    return this.op(() => this.send(f), (p) => {
      const r = parseReply(p);
      if (r && r.kind === "store") return r;
      return r && r.kind === "error" && (r.command === 0x10 || r.command === 0x11) ? r : undefined;
    });
  }

  // ---- the bulk channel
  bulk(tag, words, want, timeout) {
    return this.op(() => this.send(bulkFrame(tag, ...words)), (p) => {
      const r = parseBulk(p);
      return r && (r.tag === want || r.tag === "FZER") ? r : undefined;
    }, { timeout, partial: () => null });
  }
  async storeInfo() {
    const r = await this.bulk("FZBI", [], "FZBI", 15000);   // (the unit's CRC of ~300 KB, bitwise: tens of ms)
    if (!r || r.tag !== "FZBI" || r.data.length < 12) return null;
    return { lo: rd32(r.data, 0), size: rd32(r.data, 4), crc: rd32(r.data, 8) };
  }

  // the store read whole: FZBR, `depth` requests outstanding (1: one at a time), each matched by its offset, sent
  // again when its answer does not come (rto: 4 x the slowest of the recent round trips, 150 ms .. 3 s); the copy's
  // CRC-32 against FZBI's, and once more from the start when they differ (an autosave while it was read)
  async backup({ depth = 1, gapMs = 0, onProgress = () => {}, signal = null, onRead = null, tries = 2 } = {}) {
    for (let attempt = 1; ; attempt++) {
      const info = await this.storeInfo();
      if (!info) throw err("nobulk", "this FIZZ has no backup over USB (update FIZZ)");
      const r = await this.readRegion(info, { depth, gapMs, onProgress, signal, onRead });
      const crc = crc32(r.image);
      r.stats.attempt = attempt;
      this.log(`backup: ${info.size} B in ${r.stats.ms.toFixed(0)} ms, ${r.stats.kbps.toFixed(1)} KB/s, depth ${depth}, ` +
        `${r.stats.frames} frames, ${r.stats.resent} sent again, round trip avg ${r.stats.rtt.avg.toFixed(1)} / median ${r.stats.rtt.median.toFixed(1)} / ` +
        `p95 ${r.stats.rtt.p95.toFixed(1)} / max ${r.stats.rtt.max.toFixed(1)} ms; CRC-32 ${hex8(crc)} (the unit's ${hex8(info.crc)})`);
      if (crc === info.crc) return { image: r.image, lo: info.lo, size: info.size, crc, stats: r.stats };
      if (attempt >= tries) throw err("changed", "the FM-1 changed its store while it was read: stop it and back up again");
      this.log("backup: the store changed while it was read (an autosave?): reading it again");
    }
  }
  readRegion(info, { depth = 1, gapMs = 0, onProgress = () => {}, signal = null, onRead = null, from = 0, to = info.size }) {
    const k0 = Math.floor(from / CHUNK), n = Math.ceil(to / CHUNK) - k0, image = new Uint8Array(info.size);
    const fly = new Map(), rtts = [];
    let next = k0, done = 0, resent = 0, frames = 0, tick = null, wait = null, rto = 1500, sent = -Infinity;
    const t0 = now();
    const want = (k) => Math.min(CHUNK, info.size - k * CHUNK);
    const ask = (k, again) => {
      const e = fly.get(k) || { tries: 0 };
      e.t = sent = now(); e.tries++; e.again = !!again || e.tries > 1;
      fly.set(k, e);
      frames++;
      if (again) resent++;
      this.send(bulkFrame("FZBR", k * CHUNK, want(k)));
    };
    // the next requests, `gapMs` apart at least (the unit drops a frame that arrives before it took the last one)
    const fill = () => {
      clearTimeout(wait);
      while (fly.size < depth && next < k0 + n) {
        const left = gapMs - (now() - sent);
        if (fly.size && left > 0) { wait = setTimeout(fill, left); return; }
        ask(next++, false);
      }
    };
    return this.op((c) => {
      fill();
      tick = setInterval(() => {                       // one overdue request sent again a tick (more would collide)
        if (signal && signal.aborted) { c.fail(err("cancel", "cancelled")); return; }
        const t = now();
        let old = null;
        for (const [k, e] of fly) if (t - e.t >= rto && (!old || e.t < old[1].t)) old = [k, e];
        if (!old) return;
        if (old[1].tries >= 10) { c.fail(err("timeout", `the FM-1 stopped answering at ${(old[0] * CHUNK).toString(16)}`)); return; }
        ask(old[0], true);
      }, 10);
    }, (p) => {
      const r = parseBulk(p);
      if (!r) return undefined;
      if (r.tag === "FZER") throw err("fzer", `the FM-1 refused a read (FZER ${rd32(r.data, 0)})`);
      if (r.tag !== "FZBD" || r.data.length < 4) return undefined;
      const off = rd32(r.data, 0), k = off / CHUNK, e = fly.get(k);
      if (!e || off % CHUNK || r.data.length !== 4 + want(k)) return MORE;   // (a late duplicate)
      image.set(r.data.subarray(4), off);
      if (!e.again) rtts.push(now() - e.t);
      if (rtts.length >= 8) rto = Math.max(this.rtoMin, Math.min(3000, 4 * stats(rtts.slice(-64)).max));
      fly.delete(k);
      done++;
      onProgress(done / n, off);
      if (onRead) onRead(off);
      if (done === n) {
        const ms = now() - t0, bytes = Math.min(to, info.size) - k0 * CHUNK;
        return { image, stats: { bytes, frames, resent, depth, gapMs, ms, kbps: bytes / 1024 / (ms / 1000), perFrame: ms / n, rtt: stats(rtts) } };
      }
      fill();
      return MORE;
    }, { timeout: 8000 }).finally(() => { clearInterval(tick); clearTimeout(wait); });
  }

  // the store put back (stopped only: the unit refuses while playing, FZER 10). The unit reads the image twice, 512
  // bytes a request: first all of it, to check its CRC-32 before anything is erased (pass "check"), then again while
  // it erases and writes each sector (pass "write"). signal (cancel) is honoured in the check pass only: the reads
  // still to come are answered with zeros, so the unit's CRC check fails and nothing is erased (FZER 12)
  restore(image, { onProgress = () => {}, onPhase = () => {}, signal = null, idleMs = 30000 } = {}) {
    const len = image.length, crc = crc32(image);
    let pass = 1, last = -1, served = 0, bytes = [0, 0], cancelled = false;
    const t0 = now();
    return this.op(() => { onPhase("check"); this.send(bulkFrame("FZSR", len, crc)); }, (p) => {
      const q = parseRequest(p);
      if (q) {
        if (q.len > 512 || q.addr + q.len > len) throw err("badreq", `the FM-1 asked for ${q.addr.toString(16)}+${q.len}, outside the image`);
        if (pass === 1 && q.addr < last) { pass = 2; onPhase("write"); }
        last = q.addr;
        const fake = pass === 1 && signal && signal.aborted;
        if (fake && !cancelled) { cancelled = true; this.log("restore: cancelled before anything was erased"); }
        this.send(response(q.addr, fake ? new Array(q.len).fill(0) : Array.from(image.subarray(q.addr, q.addr + q.len)), q.fl));
        served++;
        bytes[pass - 1] = Math.max(bytes[pass - 1], q.addr + q.len);
        onProgress((bytes[0] + bytes[1]) / (2 * len), pass);
        return MORE;
      }
      const r = parseBulk(p);
      if (!r || (r.tag !== "FZOK" && r.tag !== "FZER")) return undefined;
      const ms = now() - t0, code = rd32(r.data, 0);
      this.log(`restore: ${r.tag} ${code} after ${served} reads, ${ms.toFixed(0)} ms (${(2 * len / 1024 / (ms / 1000)).toFixed(1)} KB/s both passes)`);
      if (r.tag === "FZOK") return { ok: true, ms, served };
      throw err(cancelled && code === 12 ? "cancel" : "fzer", FZER[code] || `FZER ${code}`, { fzer: code, written: pass === 2 });
    }, { timeout: idleMs });
  }
}

// FIZZ's sources as LIST and DUMP name them
export { SOURCES, sleep };
