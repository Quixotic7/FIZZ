// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
// The install page's checks (Node, no browser; the Pages workflow runs them):
//   - site/firmware/releases.json and its files: every package's size, SHA-256, identity (fm1-base's productOf), its
//     update loader, its data blob's CRC-32; the newest first and "latest";
//   - fizz_data.js against a simulated FM-1 running FIZZ on a fake Web MIDI (the fake after fm1-base's
//     tests/installer_boot_test.mjs): the data blob written as fizz_xdata.py push writes it (FZXD, the unit's reads,
//     FZOK), a blob already there, a refused one (FZER), and what readUnit says of FIZZ, stock and update mode;
//   - the release notes' Markdown (escaped, links to http(s) only);
//   - site/install/fm1lib.js is what fm1-base makes (firmware/tools/sync_installer.py --check), where the submodule
//     is checked out (not on the Pages runner).
//   node site/tools/test_install.mjs [FIRMWARE_DIR]     (default site/firmware; a dry run's: firmware/release.py --dry-run)
import fs from "node:fs";
import path from "node:path";
import crypto from "node:crypto";
import { spawnSync } from "node:child_process";
import { fileURLToPath } from "node:url";
import { productOf, pack7, unpack7 } from "../install/fm1lib.js";
import { pushData, readUnit, versionOfId, crc32, parseDataReply, XDATA_OFF } from "../install/fizz_data.js";
import { md } from "../install/notes.js";

const SITE = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");
const ROOT = path.resolve(SITE, "..");
const FW = path.resolve(process.argv[2] || path.join(SITE, "firmware"));
let fails = 0;
const check = (ok, msg) => { console.log(`${msg.padEnd(72)} ${ok ? "ok" : "FAIL"}`); if (!ok) fails++; };

// ---- releases.json
{
  const index = JSON.parse(fs.readFileSync(path.join(FW, "releases.json"), "utf8"));
  const rels = index.releases || [];
  check(Array.isArray(rels), `releases.json: ${rels.length} release(s)`);
  check(rels.length ? index.latest === rels[0].version : !index.latest, "latest is the first listed (the page's default)");
  check(new Set(rels.map((r) => r.version)).size === rels.length, "each version once");
  const dated = rels.map((r) => r.date || "");
  check(dated.every((d, i) => i === 0 || d <= dated[i - 1]), "the newest first");
  for (const r of rels) {
    check(/^\d{1,2}\.\d{1,2}\.\d{1,2}(-beta\.\d{1,3})?(-[a-z0-9]+)?$/.test(r.version) && /^FM-1_46\d{6}$/.test(r.identity),
      `${r.version}: version and identity ${r.identity}`);
    check(versionOfId(r.identity) === r.version.split("-")[0], `${r.version}: identity says ${versionOfId(r.identity)}`);
    check(typeof r.notes === "string" && r.notes.trim().length > 0, `${r.version}: release notes`);
    for (const p of r.packages || []) {
      const get = (f) => {
        const b = fs.readFileSync(path.join(FW, f.path));
        const ok = b.length === f.bytes && crypto.createHash("sha256").update(b).digest("hex") === f.sha256;
        check(ok, `${f.path}: ${b.length} B, SHA-256 as listed`);
        return new Uint8Array(b);
      };
      const raw = get(p.fwsc), data = get(p.data);
      check(productOf(raw) === r.identity, `${p.fwsc.path}: identity ${productOf(raw)}`);
      check(Buffer.from(raw).includes("FELUCCA-LOADER-1"), `${p.fwsc.path}: the update loader in it`);
      check(crc32(data) === p.data.crc32, `${p.data.path}: CRC-32 ${p.data.crc32.toString(16)}`);
      const sums = fs.readFileSync(path.join(FW, r.version, "SHA256SUMS"), "utf8");
      check(sums.includes(`${p.fwsc.sha256}  ${path.basename(p.fwsc.path)}`) && sums.includes(`${p.data.sha256}  ${path.basename(p.data.path)}`),
        `${r.version}/SHA256SUMS lists both`);
    }
  }
}

// ---- a simulated FM-1 running FIZZ (fm1-base's FakeFM1, tests/installer_boot_test.mjs, with FIZZ's data writer)
const HS = [0xF0, 0x00, 0x32, 0x45, 0x00, 0x00, 0x00, 0x40, 0x7F, 0xF7];
const FIZZ_ID = [0xF0, 0x7D, 0x46, 0x5A, 0x01, 0x01, 0x7F, 0xF7];
const sxFrame = (cmd, body) => { const b = [cmd, ...body]; return [0xF0, 0x7D, 0x46, 0x5A, 0x01, ...b, (0x80 - (b.reduce((s, x) => s + x, 0) & 0x7F)) & 0x7F, 0xF7]; };
class FakeFIZZ {
  constructor({ id = "FM-1_46001100", version = "FIZZ 0.11.0-beta.1", blob, have = false, refuse = 0, answers = true }) {
    Object.assign(this, { id, version, blob, have, refuse, answers });
    this.served = 0; this.bad = 0; this.reads = [];
    this.input = { name: "FIZZ FM-1", state: "connected", open: async () => {}, onmidimessage: null };
    this.output = { name: "FIZZ FM-1", state: "connected", open: async () => {}, send: (b) => this.rx(Array.from(b)) };
    this.access = { inputs: new Map([["i", this.input]]), outputs: new Map([["o", this.output]]) };
  }
  tx(bytes) { setTimeout(() => this.input.onmidimessage && this.input.onmidimessage({ data: Uint8Array.from(bytes) }), 1); }
  reply(tag, code) {                                   /* app.c fz_usb_reply */
    const m = [0, 0x59, 0x46, 8, 0, 0, ...[...tag].map((c) => c.charCodeAt(0)), code & 0xFF, (code >> 8) & 0xFF, (code >> 16) & 0xFF, code >>> 24];
    let s = 0; for (let i = 6; i < m.length; i++) s += m[i];
    this.tx([0xF0, ...pack7([...m, ~s & 0xFF]), 0xF7]);
  }
  request(addr, len) {
    this.waiting = { addr, len };
    const u = [0, 0x59, 0x30, 8, 0, 0, 0, addr & 0xFF, (addr >>> 8) & 0xFF, (addr >>> 16) & 0xFF, (addr >>> 24) & 0xFF, len & 0xFF, (len >> 8) & 0xFF, 0];
    let s = 0; for (let i = 6; i < 14; i++) s += u[i];
    this.tx([0xF0, ...pack7([...u, ~s & 0xFF]), 0xF7]);
  }
  next() {
    const a = this.reads.length ? this.reads[this.reads.length - 1].addr + 512 : 0;
    if (a >= this.blob.length) { this.have = true; this.reply("FZOK", 1); return; }
    this.reads.push({ addr: a });
    this.request(a, Math.min(512, this.blob.length - a));
  }
  rx(d) {
    if (d.join() === HS.join()) {
      const t = [...this.id].map((c) => c.charCodeAt(0));
      this.tx([0xF0, ...pack7([0, 0x59, 0x11, 0, 0, 0, ...t, ...new Array(28 - t.length).fill(0)]), 0xF7]);
    } else if (d.join() === FIZZ_ID.join()) {
      if (this.answers && this.have) this.tx(sxFrame(0x41, [1, ...[...this.version].map((c) => c.charCodeAt(0))]));
    } else if (this.waiting) {
      const u = unpack7(d.slice(1, -1)), { addr, len } = this.waiting;
      if (u[2] !== 0x30 || u.slice(14, 14 + len).join() !== Array.from(this.blob.subarray(addr, addr + len)).join()) this.bad++;
      this.waiting = null; this.served++;
      this.next();
    } else {
      const u = unpack7(d.slice(1, -1));
      if (u[2] !== 0x46 || String.fromCharCode(...u.slice(6, 10)) !== "FZXD") return;
      const le = (o) => (u[o] | (u[o + 1] << 8) | (u[o + 2] << 16) | (u[o + 3] << 24)) >>> 0;
      this.got = { len: le(10), crc: le(14), off: le(18) };
      if (this.refuse) { this.reply("FZER", this.refuse); return; }
      if (this.got.len !== this.blob.length || this.got.crc !== crc32(this.blob) || this.got.off !== XDATA_OFF) { this.reply("FZER", 1); return; }
      if (this.have) { this.reply("FZOK", 0); return; }
      this.next();
    }
  }
}

{
  const blob = Uint8Array.from({ length: 84116 }, (_, i) => (i * 7 + (i >> 9)) & 0xFF);
  const dev = new FakeFIZZ({ blob });
  let r = null, err = null, last = 0;
  try { r = await pushData(dev.access, blob, { waitMs: 3000, onProgress: (f) => { last = f; } }); } catch (e) { err = e; }
  check(!err && r && r.written, `data: written (${err ? err.code + " " + err.message : "FZOK 1"})`);
  check(dev.got && dev.got.off === 0xE5000 && dev.got.len === blob.length && dev.got.crc === crc32(blob), "data: FZXD with the blob's length, CRC-32 and 0xE5000");
  check(dev.served === Math.ceil(blob.length / 512) && dev.bad === 0, `data: ${dev.served} reads, each answered with the blob's bytes`);
  check(last === 1, "data: progress to the end");
  const u = await readUnit(dev.access);
  check(u && u.kind === "fizz" && u.version === "0.11.0-beta.1" && u.fromId === "0.11.0", `readUnit: FIZZ ${u && u.version} (identity ${u && u.fromId})`);
  const again = await pushData(dev.access, blob, { waitMs: 3000 });
  check(!again.written && dev.served === Math.ceil(blob.length / 512), "data already there: FZOK 0, nothing read");
}
{
  const blob = new Uint8Array(1000).fill(3);
  const dev = new FakeFIZZ({ blob, refuse: 1 });
  let err = null;
  try { await pushData(dev.access, blob, { waitMs: 3000 }); } catch (e) { err = e; }
  check(err && err.code === "dataerr" && /another build/.test(err.detail), `data refused: ${err && err.code} (${err && err.detail})`);
  const u = await readUnit(dev.access);
  check(u && u.kind === "fizz" && u.version === null, "readUnit: FIZZ without its data does not say its version");
}
{
  const stock = new FakeFIZZ({ id: "FM-1_015", blob: new Uint8Array(1) });
  const u = await readUnit(stock.access);
  check(u && u.kind === "stock" && u.id === "FM-1_015", `readUnit: stock (${u && u.id})`);
  const ota = new FakeFIZZ({ id: "ota-FM-1_0", blob: new Uint8Array(1) });
  check((await readUnit(ota.access))?.kind === "update", "readUnit: update mode");
  let err = null;
  try { await pushData(stock.access, new Uint8Array(4), { waitMs: 1500 }); } catch (e) { err = e; }
  check(err && err.code === "nofizz", "data: no FIZZ to write to (stock): nofizz");
  check(versionOfId("FM-1_4600") === "" && versionOfId("FM-1_46011203") === "1.12.3", "versionOfId");
  check(parseDataReply([0xF0, 0xF7]) === null, "parseDataReply: not a reply");
}

// ---- the release notes' Markdown
{
  const h = md("Intro <b>x</b> & more.\n\n- one **bold**\n  continued\n- `code` [link](https://example.com) [bad](javascript:alert(1))\n\n### Known");
  check(h.includes("&lt;b&gt;x&lt;/b&gt; &amp; more") && h.includes("<li>one <b>bold</b> continued</li>"), "md: escaped, a list item continued");
  check(h.includes('<a href="https://example.com"') && !h.includes("javascript:alert(1)\"") && !h.includes('href="javascript'), "md: http(s) links only");
  check(h.includes("<h4>Known</h4>") && h.includes("<code>code</code>"), "md: headings and code");
}

// ---- fm1lib.js as fm1-base makes it
if (fs.existsSync(path.join(ROOT, "firmware", "fm1-base", "web", "fm1ota.js"))) {
  const r = spawnSync("python3", [path.join(ROOT, "firmware", "tools", "sync_installer.py"), "--check"], { encoding: "utf8" });
  check(r.status === 0, "site/install/fm1lib.js: fm1-base's installer code, unchanged");
} else console.log("(fm1-base not checked out: fm1lib.js's comparison skipped)");

console.log(fails ? `INSTALL PAGE TEST FAILED (${fails})` : "install page test passed");
process.exit(fails ? 1 : 0);
