// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
// The install page's checks (Node, no browser; the Pages workflow runs them):
//   - site/firmware/releases.json and its files: every package's size, SHA-256, identity (fm1-base's productOf), its
//     update loader, its data blob's CRC-32 (the Bluetooth package's two, at their addresses); the newest first and
//     "latest"; the USB package first (older pages install packages[0]); a synthetic two-package release too;
//   - fizz_data.js against a simulated FM-1 running FIZZ on a fake Web MIDI (the fake after fm1-base's
//     tests/installer_boot_test.mjs): the data blob written as fizz_xdata.py push writes it (FZXD, the unit's reads,
//     FZOK), a blob already there, a refused one (FZER), and what readUnit says of FIZZ, stock and update mode; the
//     Bluetooth package: its two blobs at 0xE7000 and 0x97000, its package from STATUS, the Bluetooth MIDI port never
//     used (FZER 15 there: the USB port is asked first, and alone);
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
import { pushData, readUnit, versionOfId, crc32, parseDataReply, XDATA_OFF, BLE_XDATA_OFF, BLE_XDATA2_OFF, usbOnly } from "../install/fizz_data.js";
import { md } from "../install/notes.js";
import { statusRequest } from "../js/fizz_sysex.js";
import os from "node:os";

const SITE = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");
const ROOT = path.resolve(SITE, "..");
const FW = path.resolve(process.argv[2] || path.join(SITE, "firmware"));
let fails = 0;
const check = (ok, msg) => { console.log(`${msg.padEnd(72)} ${ok ? "ok" : "FAIL"}`); if (!ok) fails++; };

// ---- releases.json
function checkReleases(FW, what = "releases.json") {
  const index = JSON.parse(fs.readFileSync(path.join(FW, "releases.json"), "utf8"));
  const rels = index.releases || [];
  check(Array.isArray(rels), `${what}: ${rels.length} release(s)`);
  check(rels.length ? index.latest === rels[0].version : !index.latest, "latest is the first listed (the page's default)");
  check(new Set(rels.map((r) => r.version)).size === rels.length, "each version once");
  const dated = rels.map((r) => r.date || "");
  check(dated.every((d, i) => i === 0 || d <= dated[i - 1]), "the newest first");
  for (const r of rels) {
    check(/^\d{1,2}\.\d{1,2}\.\d{1,2}(-beta\.\d{1,3})?(-[a-z0-9]+)?$/.test(r.version) && /^FM-1_46\d{6}$/.test(r.identity),
      `${r.version}: version and identity ${r.identity}`);
    check(versionOfId(r.identity) === r.version.split("-")[0], `${r.version}: identity says ${versionOfId(r.identity)}`);
    check(typeof r.notes === "string" && r.notes.trim().length > 0, `${r.version}: release notes`);
    const pk = r.packages || [];
    check(pk.length > 0 && !pk[0].kind && !pk[0].data2, `${r.version}: the USB package first (older pages install packages[0])`);
    check(pk.filter((p) => p.kind === "ble").length <= 1 && pk.every((p) => !p.kind || p.kind === "ble"), `${r.version}: packages ${pk.map((p) => p.kind || "usb").join(", ")}`);
    for (const p of pk) {
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
      const listed = (f) => sums.includes(`${f.sha256}  ${path.basename(f.path)}`);
      check(listed(p.fwsc) && listed(p.data) && (!p.data2 || listed(p.data2)), `${r.version}/SHA256SUMS lists ${p.data2 ? "all three" : "both"}`);
      if (p.kind === "ble") {
        const d2 = get(p.data2);
        check(crc32(d2) === p.data2.crc32, `${p.data2.path}: CRC-32 ${p.data2.crc32.toString(16)}`);
        check(p.data.at === BLE_XDATA_OFF && p.data2.at === BLE_XDATA2_OFF && p.projects === 4 && p.presets === 128,
          `${p.name}: its blobs at 0x${(p.data.at || 0).toString(16)} and 0x${(p.data2.at || 0).toString(16)}; ${p.projects} projects, ${p.presets} presets`);
      }
    }
  }
}
checkReleases(FW);

// a synthetic release with both packages (firmware/release.py's shape), checked as the real ones
{
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), "fizz-rel-")), v = "0.13.0-beta.1", id = "FM-1_46001300";
  const fwsc = (salt) => {
    const b = new Uint8Array(4096).map((_, i) => (i * 13 + salt) & 0xFF);
    for (let i = 0; i < 20; i++) b[i * 0x30 + 0x2F] = i < id.length ? (id.charCodeAt(i) + i + 1) & 0xFF : 0x7D;
    b.set(Buffer.from("FELUCCA-LOADER-1"), 2000);
    return b;
  };
  const blob = (n, k) => Uint8Array.from({ length: n }, (_, i) => (i * k) & 0xFF);
  const files = { "fizz.fwsc": fwsc(1), "fizz-xdata.bin": blob(900, 3), "fizz-ble.fwsc": fwsc(2), "fizz-ble-xdata.bin": blob(800, 5), "fizz-ble-xdata2.bin": blob(700, 7) };
  fs.mkdirSync(path.join(dir, v));
  const sha = (b) => crypto.createHash("sha256").update(b).digest("hex");
  for (const [n, b] of Object.entries(files)) fs.writeFileSync(path.join(dir, v, n), b);
  fs.writeFileSync(path.join(dir, v, "SHA256SUMS"), Object.entries(files).map(([n, b]) => `${sha(b)}  ${n}`).join("\n") + "\n");
  const f = (n, extra = {}) => ({ path: `${v}/${n}`, bytes: files[n].length, sha256: sha(files[n]), ...(/\.bin$/.test(n) ? { crc32: crc32(files[n]) } : {}), ...extra });
  const rel = { version: v, date: "2026-10-28", channel: "beta", identity: id, notes: "Bluetooth.", packages: [
    { name: "fizz", label: "USB", fwsc: f("fizz.fwsc"), data: f("fizz-xdata.bin") },
    { name: "fizz-ble", kind: "ble", label: "Bluetooth", projects: 4, presets: 128, fwsc: f("fizz-ble.fwsc"), data: f("fizz-ble-xdata.bin", { at: BLE_XDATA_OFF }), data2: f("fizz-ble-xdata2.bin", { at: BLE_XDATA2_OFF }) }] };
  fs.writeFileSync(path.join(dir, "releases.json"), JSON.stringify({ name: "FIZZ", latest: v, releases: [rel] }, null, 2));
  const before = fails;
  checkReleases(dir, "a two-package release");
  check(fails === before, "a two-package release: the USB package first, the Bluetooth one with both blobs, all checked");
  fs.rmSync(dir, { recursive: true });
}

// ---- a simulated FM-1 running FIZZ (fm1-base's FakeFM1, tests/installer_boot_test.mjs, with FIZZ's data writer)
const HS = [0xF0, 0x00, 0x32, 0x45, 0x00, 0x00, 0x00, 0x40, 0x7F, 0xF7];
const FIZZ_ID = [0xF0, 0x7D, 0x46, 0x5A, 0x01, 0x01, 0x7F, 0xF7];
const sxFrame = (cmd, body) => { const b = [cmd, ...body]; return [0xF0, 0x7D, 0x46, 0x5A, 0x01, ...b, (0x80 - (b.reduce((s, x) => s + x, 0) & 0x7F)) & 0x7F, 0xF7]; };
class FakeFIZZ {
  // blobs: {offset: bytes} (the Bluetooth package's two; blob: the USB package's one at XDATA_OFF); pkg: "ble" appends
  // STATUS's package bytes; port: the port's name ("FM-1 1234 Bluetooth": a Bluetooth link, FZER 15 for FZXD)
  constructor({ id = "FM-1_46001100", version = "FIZZ 0.11.0-beta.1", blob, blobs = null, have = false, refuse = 0, answers = true, pkg = "usb", port = "FIZZ FM-1" }) {
    Object.assign(this, { id, version, blobs: blobs || { [XDATA_OFF]: blob }, have, refuse, answers, pkg, port });
    this.blob = Object.values(this.blobs)[0];
    this.served = 0; this.bad = 0; this.reads = []; this.wrote = [];
    this.input = { name: port, state: "connected", open: async () => {}, onmidimessage: null };
    this.output = { name: port, state: "connected", open: async () => {}, send: (b) => this.rx(Array.from(b)) };
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
    if (a >= this.blob.length) { this.wrote.push(this.got.off); if (this.wrote.length >= Object.keys(this.blobs).length) this.have = true; this.reply("FZOK", 1); return; }
    this.reads.push({ addr: a });
    this.request(a, Math.min(512, this.blob.length - a));
  }
  rx(d) {
    if (d.join() === HS.join()) {
      const t = [...this.id].map((c) => c.charCodeAt(0));
      this.tx([0xF0, ...pack7([0, 0x59, 0x11, 0, 0, 0, ...t, ...new Array(28 - t.length).fill(0)]), 0xF7]);
    } else if (d.join() === FIZZ_ID.join()) {
      if (this.answers && this.have) this.tx(sxFrame(0x41, [1, ...[...this.version].map((c) => c.charCodeAt(0))]));
    } else if (d.join() === Array.from(statusRequest()).join()) {   /* STATUS: 23 bytes; the Bluetooth package's 4 more */
      if (this.answers && this.have) this.tx(sxFrame(0x42, [1, 6, 0, 0, 0, 1, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, ...(this.pkg === "ble" ? [1, 4, 1, 0] : [])]));
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
      if (/ bluetooth$/i.test(this.port)) { this.reply("FZER", 15); return; }   /* (app.c: a blob pulls over USB only) */
      if (this.refuse) { this.reply("FZER", this.refuse); return; }
      this.blob = this.blobs[this.got.off];
      if (!this.blob || this.got.len !== this.blob.length || this.got.crc !== crc32(this.blob)) { this.reply("FZER", 1); return; }
      if (this.have || this.wrote.includes(this.got.off)) { this.reply("FZOK", 0); return; }
      this.reads = [];
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

// ---- the Bluetooth package: two blobs, its package from STATUS, never over its Bluetooth MIDI port
{
  const b1 = Uint8Array.from({ length: 5000 }, (_, i) => (i * 3) & 0xFF), b2 = Uint8Array.from({ length: 7000 }, (_, i) => (i * 5 + 1) & 0xFF);
  const dev = new FakeFIZZ({ blobs: { [BLE_XDATA_OFF]: b1, [BLE_XDATA2_OFF]: b2 }, pkg: "ble" });
  const r1 = await pushData(dev.access, b1, { off: BLE_XDATA_OFF, waitMs: 3000 });
  const r2 = await pushData(dev.access, b2, { off: BLE_XDATA2_OFF, waitMs: 3000 });
  check(r1.written && r2.written && dev.wrote.join() === [BLE_XDATA_OFF, BLE_XDATA2_OFF].join() && dev.bad === 0,
    `Bluetooth package: both blobs written (0x${BLE_XDATA_OFF.toString(16)}, 0x${BLE_XDATA2_OFF.toString(16)}), each read right`);
  const u = await readUnit(dev.access);
  check(u && u.kind === "fizz" && u.package === "ble", `readUnit: the Bluetooth package (STATUS's package bytes): ${u && u.package}`);
  const usb = new FakeFIZZ({ blob: new Uint8Array(600).fill(9) });
  await pushData(usb.access, usb.blob, { waitMs: 3000 });
  check((await readUnit(usb.access))?.package === "usb", "readUnit: the USB package (STATUS without the package bytes)");
  // the Bluetooth MIDI port alone: not asked (an install, a blob need USB); with the USB port beside it: USB
  const bt = new FakeFIZZ({ blob: b1, have: true, pkg: "ble", port: "FM-1 1234 Bluetooth" });
  check(await readUnit(bt.access) === null, "the Bluetooth MIDI port alone: not asked (readUnit: no FM-1)");
  let err = null;
  try { await pushData(bt.access, b1, { off: BLE_XDATA_OFF, waitMs: 1500 }); } catch (e) { err = e; }
  check(err && err.code === "nofizz", "a blob over the Bluetooth port alone: not sent (nofizz: connect the USB cable)");
  const both = new FakeFIZZ({ blobs: { [BLE_XDATA_OFF]: b1, [BLE_XDATA2_OFF]: b2 }, pkg: "ble" });
  const access = { inputs: new Map([["b", bt.input], ["i", both.input]]), outputs: new Map([["b", bt.output], ["o", both.output]]) };
  check([...usbOnly(access).inputs.values()].map((x) => x.name).join() === "FIZZ FM-1", "usbOnly: the Bluetooth port left out");
  const r3 = await pushData(access, b1, { off: BLE_XDATA_OFF, waitMs: 3000 });
  check(r3.written && !bt.got && both.got && both.got.off === BLE_XDATA_OFF, "USB and Bluetooth ports both there: the blob goes over USB");
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
