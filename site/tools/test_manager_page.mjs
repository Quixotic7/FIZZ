// SPDX-License-Identifier: GPL-3.0-only
// The manager page (site/manager/) in headless Chromium over the DevTools protocol (as test_site.mjs: no Playwright,
// never a window), against its simulated FM-1 (?sim: the browser FIZZ behind a fake Web MIDI; headless Chromium's own
// Web MIDI mangles SysEx, so the unit is never reached from here):
//   - every tab at 1280 x 800 and 390 x 844: no console error, no horizontal scroll; the screenshots beside the
//     mock-ups' (mockups/png/fizz-web-manager-0N-*.png) for the eye;
//   - BACKUP: the STATUS numbers without a backup; BACK UP keeps the backup in this browser (no download), its save
//     button writes it to a file (showSaveFilePicker, stubbed here), DELETE asks first; a stored backup and a dropped
//     file open the restore;
//   - REFRESH: a preset saved on the unit shows after the header's button, and after the tab comes back (focus);
//   - LIBRARY: one list, the filters ALL / FACTORY / MY PRESETS and the tags; a tag toggled, a preset renamed, moved,
//     + POOL then − POOL (the pool's badge with its place), DELETE (a confirm, then gone);
//   - POOL: the sounds beside with the same filters, a MY preset and a factory one put on empty places, a swap (its
//     confirm says the undo is cleared), CLEAR (a confirm), SORT BY TAG cancelled;
//   - IMPORT: synthetic Digitone files (never real presets): two Digitone II read, one refused (not FM TONE), a
//     Digitone 1 version-4 sound (OS 1.42) read, sent;
//   - EDIT (Manager P2-A): a preset opened from LIBRARY's EDIT, every group drawn (the screenshots beside the mock-ups'
//     mockups/png/fizz-manager-p2-0N-*.png), every one of the 110 values on a group, a handle dragged, the wheel, ALGO
//     clicked, REVERT; the browser's own FIZZ plays it (a key of the keybed: sound out); SAVE AS NEW stores it on the
//     unit as edited; EXPORT: three homes, EXPERIMENTAL on the Digitone's, the carry rows, .dn2snd, the download.
// Chromium's cache is off (Network.setCacheDisabled) and the profile a fresh one each run: a static server sends no
// cache headers, and a module cached from another checkout once gave "X is not defined".
//   node site/tools/test_manager_page.mjs [--url http://127.0.0.1:8765/] [--chromium path]
// (no --url: the test serves site/ itself, site/tools/serve.mjs: python's http.server resets connections under load)
import fs from "node:fs";
import os from "node:os";
import path from "node:path";
import { spawn } from "node:child_process";
import { fileURLToPath } from "node:url";
import { dn2RecordFromParams, wrapDn2Record, dn2pstZip } from "../js/dn2pst.js";
import { dnsndV4 } from "./dnsnd_fixture.mjs";
import { serveSite } from "./serve.mjs";

const HERE = path.dirname(fileURLToPath(import.meta.url));
const OUT = path.resolve(HERE, "../.build/test");
const arg = (k, d) => { const i = process.argv.indexOf(k); return i > 0 ? process.argv[i + 1] : d; };
const served = process.argv.includes("--url") ? null : await serveSite(path.resolve(HERE, ".."));
const URL0 = new URL(`manager/?sim&v=${Date.now()}`, arg("--url", served && served.url)).href;
const CHROMIUM = arg("--chromium", "/Applications/Chromium.app/Contents/MacOS/Chromium");
const PORT = 9700 + Math.floor(Math.random() * 200);
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
fs.mkdirSync(OUT, { recursive: true });
let fails = 0;
const check = (ok, what) => { console.log(`${ok ? "ok  " : "FAIL"}  ${what}`); if (!ok) fails++; };

const profile = fs.mkdtempSync(path.join(os.tmpdir(), "fizz-mgr-"));
const chrome = spawn(CHROMIUM, ["--headless=new", "--use-mock-keychain", "--password-store=basic", "--no-first-run", "--no-default-browser-check",
  "--disable-gpu", `--user-data-dir=${profile}`, `--remote-debugging-port=${PORT}`, "--hide-scrollbars", "--window-size=1280,800",
  "--disable-audio-output", "about:blank"], { stdio: "ignore" });
process.on("exit", () => { try { chrome.kill("SIGKILL"); } catch (_) { /* gone */ } });

let ws = null, id = 0;
const pending = new Map(), errors = [];
async function connect() {
  for (let i = 0; i < 100 && !ws; i++) {
    try {
      const page = (await (await fetch(`http://127.0.0.1:${PORT}/json/list`)).json()).find((t) => t.type === "page");
      if (page) ws = new WebSocket(page.webSocketDebuggerUrl);
    } catch (_) { /* not up yet */ }
    if (!ws) await sleep(100);
  }
  if (!ws) throw new Error("Chromium did not start");
  await new Promise((r, j) => { ws.onopen = r; ws.onerror = j; });
  ws.onmessage = (ev) => {
    const m = JSON.parse(ev.data);
    if (m.id && pending.has(m.id)) { const p = pending.get(m.id); pending.delete(m.id); m.error ? p.j(new Error(m.error.message)) : p.r(m.result); return; }
    if (m.method === "Runtime.exceptionThrown") errors.push(m.params.exceptionDetails.exception ? m.params.exceptionDetails.exception.description : m.params.exceptionDetails.text);
    if (m.method === "Runtime.consoleAPICalled" && m.params.type === "error") errors.push(`console.error: ${m.params.args.map((a) => a.value ?? a.description).join(" ")}`);
    if (m.method === "Log.entryAdded" && m.params.entry.level === "error") errors.push(`log: ${m.params.entry.text} ${m.params.entry.url || ""}`);
  };
}
const send = (method, params = {}) => new Promise((r, j) => { const i = ++id; pending.set(i, { r, j }); ws.send(JSON.stringify({ id: i, method, params })); });
async function evaluate(expr) {
  const r = await send("Runtime.evaluate", { expression: expr, awaitPromise: true, returnByValue: true });
  if (r.exceptionDetails) throw new Error(`evaluate: ${r.exceptionDetails.exception ? r.exceptionDetails.exception.description : r.exceptionDetails.text}`);
  return r.result.value;
}
async function waitFor(expr, ms = 15000) {
  const t = Date.now();
  while (Date.now() - t < ms) { try { if (await evaluate(expr)) return true; } catch (_) { /* (navigating) */ } await sleep(100); }
  return false;
}
async function shot(file) {
  const r = await send("Page.captureScreenshot", { format: "png" });
  fs.writeFileSync(path.join(OUT, file), Buffer.from(r.data, "base64"));
}
const size = (w, h, mobile = false) => send("Emulation.setDeviceMetricsOverride", { width: w, height: h, deviceScaleFactor: 1, mobile });
const noScroll = () => evaluate("document.documentElement.scrollWidth <= window.innerWidth + 1");
const click = (sel) => evaluate(`(() => { const e = document.querySelector(${JSON.stringify(sel)}); if (!e) return false; e.click(); return true; })()`);
const idle = () => waitFor("window.__mgr && !__mgr.S.editing && !__mgr.S.busy", 20000);
// the mouse as a person's (a user gesture: the AudioContext may start), at an element's centre
const centre = (sel) => evaluate(`(() => { const e = document.querySelector(${JSON.stringify(sel)}); if (!e) return null; e.scrollIntoView({ block: "center" }); const r = e.getBoundingClientRect(); return { x: r.left + r.width / 2, y: r.top + r.height / 2 }; })()`);
const mouse = (type, x, y, extra = {}) => send("Input.dispatchMouseEvent", { type, x, y, button: "left", clickCount: 1, ...extra });
async function realClick(sel) { const c = await centre(sel); if (!c) return false; await mouse("mousePressed", c.x, c.y); await mouse("mouseReleased", c.x, c.y); return true; }
async function drag(sel, dx, dy) {
  const c = await centre(sel);
  if (!c) return false;
  await mouse("mousePressed", c.x, c.y);
  for (let k = 1; k <= 6; k++) { await mouse("mouseMoved", c.x + dx * k / 6, c.y + dy * k / 6, { buttons: 1 }); await sleep(20); }
  await mouse("mouseReleased", c.x + dx, c.y + dy);
  return true;
}

try {
  await connect();
  await send("Page.enable"); await send("Runtime.enable"); await send("Log.enable");
  await send("Network.enable"); await send("Network.setCacheDisabled", { cacheDisabled: true });
  await size(1280, 800);
  await send("Page.navigate", { url: URL0 });
  const up = await waitFor("window.__mgr && __mgr.S.unit && __mgr.S.unit.status && __mgr.S.unit.rows.length > 0", 30000);
  const st = await evaluate("__mgr.S.unit && { status: __mgr.S.unit.status, n: __mgr.S.unit.rows.filter((r) => r.source === 'mine').length }");
  check(up && st.status.tracks === 6, `?sim: connected, STATUS ${st && JSON.stringify({ tracks: st.status.tracks, projects: st.status.projects, mine: st.status.mine })}`);
  await sleep(400);
  const nums = await evaluate("[$('u-prj'), $('u-pre'), $('u-use')].map((e) => e.textContent)".replace(/\$\(/g, "document.getElementById("));
  check(nums[0] === "1" && +nums[1] > 0 && /%$/.test(nums[2]), `BACKUP: projects ${nums[0]}, my presets ${nums[1]}, store used ${nums[2]} (STATUS, no backup taken)`);
  // BACKUP: kept in this browser, no download; saved to a file by its button; DELETE asks
  await evaluate(`(() => { window.__dl = 0; const o = URL.createObjectURL; URL.createObjectURL = (b) => { window.__dl++; return o(b); };
    window.__saved = []; window.showSaveFilePicker = async (opt) => ({ name: opt.suggestedName, createWritable: async () => { const parts = [];
      return { write: async (x) => parts.push(x), close: async () => window.__saved.push({ name: opt.suggestedName, bytes: parts.reduce((a, p) => a + p.length, 0) }) }; } }); })()`);
  const nb0 = await evaluate("__mgr.S.baks.length");
  await click("#go-backup");
  await waitFor(`__mgr.S.baks.length === ${nb0 + 1} && !__mgr.S.busy`, 60000);
  const bk = await evaluate("({ dl: window.__dl, saved: __mgr.S.baks[0].saved, size: __mgr.S.baks[0].size, use: document.getElementById('b-use').textContent, unsaved: !!document.querySelector('#baks .bk .icon.unsaved') })");
  check(bk.dl === 0 && bk.saved === false && bk.size > 0 && bk.unsaved && /1 · /.test(bk.use), `BACK UP: kept in this browser, nothing downloaded (${bk.size} B; "${bk.use}")`);
  await sleep(300);
  await shot("manager-01-backup.png");
  await click("#baks .bk [data-act='save']");
  await waitFor("window.__saved.length === 1 && __mgr.S.baks[0].saved === true");
  const sv = await evaluate("({ s: window.__saved[0], icon: !!document.querySelector('#baks .bk .icon.saved') })");
  check(sv.s && /\.fizzbak$/.test(sv.s.name) && sv.s.bytes === bk.size && sv.icon, `BACKUP: its save button wrote ${sv.s && sv.s.name} (${sv.s && sv.s.bytes} B) to the folder picked; marked saved`);
  // a dropped file (here: the same bytes) is kept too, and a stored one opens the restore
  await evaluate("(async () => { const b = __mgr.S.baks[0]; await __mgr.openFile(b.bytes.slice(), 'DROPPED.fizzbak'); })()");
  await waitFor(`__mgr.S.baks.length === ${nb0 + 2}`);
  check(await evaluate("__mgr.S.baks.some((b) => b.name === 'DROPPED.fizzbak' && b.saved === true)"), "BACKUP: a dropped .fizzbak kept in the list (a file already)");
  await waitFor("!document.getElementById('rs').hidden && document.getElementById('rs-go').textContent === 'RESTORE'", 60000);
  await waitFor("document.getElementById('rs-safe').textContent.includes('saved first')", 60000);
  check(await evaluate("!document.getElementById('rs').hidden && !!document.querySelector('#rs-warn .undo.cleared')"), "RESTORE: opened from the dropped file, the FM-1 saved first, \"UNDO ON THE FM-1 IS CLEARED\" shown");
  await shot("manager-02-restore.png");
  await click("#rs-cancel");
  await idle();
  const nb1 = await evaluate("__mgr.S.baks.length");
  await click("#baks .bk [data-act='del']");
  await waitFor("!document.getElementById('cf').hidden");
  const cf = await evaluate("document.getElementById('cf-title').textContent");
  await click("#cf-no");
  check(cf === "DELETE BACKUP" && (await evaluate("__mgr.S.baks.length")) === nb1, "BACKUP: DELETE asks first; CANCEL keeps it");
  await click("#baks .bk [data-act='del']");
  await waitFor("!document.getElementById('cf').hidden");
  await click("#cf-go");
  await waitFor(`__mgr.S.baks.length === ${nb1 - 1}`);
  check((await evaluate("__mgr.S.baks.length")) === nb1 - 1, "BACKUP: DELETE confirmed: gone from the list");

  // REFRESH: a preset saved on the unit (straight into the simulated FIZZ) shows after the button, then after a focus
  const saveOnUnit = (name) => evaluate(`(async () => { const { storeSound } = await import('../js/fizz_sysex.js'); const b = new Uint8Array(96); b[0] = 0x51; b[1] = 5; b[2] = 6;
    window.__sim.engine.midiIn(storeSound({ name: ${JSON.stringify(name)}, blob: b, tags: 1 })); })()`);
  await saveOnUnit("ON THE UNIT");
  await sleep(600);
  check(await evaluate("!document.getElementById('refresh').hidden && !__mgr.S.unit.rows.some((r) => r.name === 'ON THE UNIT')"), "REFRESH: the button shown; a preset saved on the unit not listed yet");
  await click("#refresh");
  await waitFor("__mgr.S.unit.rows.some((r) => r.source === 'mine' && r.name === 'ON THE UNIT') && !__mgr.S.refreshing");
  check(await evaluate("__mgr.S.unit.rows.some((r) => r.source === 'mine' && r.name === 'ON THE UNIT')"), "REFRESH: pressed: the unit's new preset listed");
  await saveOnUnit("FOCUS TEST");
  await sleep(2500);                                   // (FIZZ's autosave: its STATUS changes; the focus refresh is 2 s apart)
  await evaluate("window.dispatchEvent(new Event('focus'))");
  await waitFor("__mgr.S.unit.rows.some((r) => r.source === 'mine' && r.name === 'FOCUS TEST')", 10000);
  check(await evaluate("__mgr.S.unit.rows.some((r) => r.source === 'mine' && r.name === 'FOCUS TEST')"), "REFRESH: the tab focused again: read again by itself (STATUS changed)");
  await idle();

  // LIBRARY: one list and its filters
  const MINE = (s) => `#l-list .cas[data-src='mine'][data-slot='${s}']`;
  await click("#t-library");
  await waitFor("__mgr.S.unit.dumps && document.querySelectorAll('#l-list .cas').length > 0");
  const counts = async () => evaluate("({ m: document.querySelectorAll('#l-list .cas[data-src=mine]').length, f: document.querySelectorAll('#l-list .cas[data-src=factory]').length, g: document.querySelectorAll('#l-list .grp').length })");
  const c0 = await counts();
  await click("#l-filter [data-src='factory']"); const c1 = await counts();
  await click("#l-filter [data-src='mine']"); const c2 = await counts();
  await click("#l-filter [data-src='all']");
  check(c0.m > 0 && c0.f > 0 && c0.g === 2 && !c1.m && c1.f === c0.f && c2.m === c0.m && !c2.f, `LIBRARY: one list, ALL ${c0.m} + ${c0.f}, FACTORY ${c1.f}, MY PRESETS ${c2.m}`);
  await click("#l-filter [data-tag='BASS']");
  const cb = await evaluate("[...document.querySelectorAll('#l-list .cas')].every((e) => e.querySelector('.tag.t-BASS'))");
  await click("#l-filter [data-tag='BASS']");
  check(cb, "LIBRARY: the BASS tag filter shows only BASS sounds");
  await click(MINE(2));
  await waitFor("document.querySelector('#l-users .uchip, #l-users .pchip')");
  await idle();
  const tag0 = await evaluate("__mgr.S.unit.rows.find((r) => r.source === 'mine' && r.slot === 2).tags");
  await click("#l-sel [data-dtag='6']");
  await idle();
  const tag1 = await evaluate("__mgr.S.unit.rows.find((r) => r.source === 'mine' && r.slot === 2).tags");
  check(tag1 === (tag0 ^ 64), `LIBRARY: FX toggled on MY PRESETS 2 (tags ${tag0} -> ${tag1})`);
  await evaluate("(() => { const i = document.getElementById('l-name'); i.value = 'BELL TOWER'; i.dispatchEvent(new Event('change')); })()");
  await waitFor("__mgr.S.unit.rows.some((r) => r.source === 'mine' && r.name === 'BELL TOWER')");
  await idle();
  check(await evaluate("__mgr.S.unit.rows.find((r) => r.source === 'mine' && r.slot === 2).name === 'BELL TOWER'"), "LIBRARY: renamed in place (RENAME): BELL TOWER");
  // a move: the drop handler's own call (HTML5 drag events are not synthesised headless)
  await evaluate(`(async () => { const ev = new KeyboardEvent('keydown', { key: 'ArrowDown', altKey: true, bubbles: true }); document.querySelector("${MINE(2)}").dispatchEvent(ev); })()`);
  await waitFor("__mgr.S.unit.rows.find((r) => r.source === 'mine' && r.name === 'BELL TOWER').slot === 3");
  await idle();
  check(await evaluate("__mgr.S.unit.rows.find((r) => r.source === 'mine' && r.name === 'BELL TOWER').slot === 3"), "LIBRARY: ALT + down moved it a place (SWAP)");
  const bellId = await evaluate("__mgr.S.unit.rows.find((x) => x.source === 'mine' && x.name === 'BELL TOWER').id");
  const inPool = () => evaluate(`__mgr.S.unit.rows.filter((x) => x.source === 'pool' && x.id === ${bellId}).map((x) => x.slot)`);
  const p0 = await inPool();
  if (p0.length) { await click("#l-pool"); await idle(); }          // (the demo's pool may hold it: out first)
  check(!(await inPool()).length && (await evaluate("document.getElementById('l-pool').textContent")) === "+ POOL", "LIBRARY: not in the pool: the big + POOL");
  await click("#l-pool");
  await idle();
  const p1 = await inPool();
  const mark = await evaluate(`(() => { const c = document.querySelector("${MINE(3)}"); const b = c && c.querySelector('.pbadge'); return { ring: !!(c && c.classList.contains('inpool')), badge: b ? b.textContent : '', tog: document.getElementById('l-pool').textContent, big: (document.querySelector('#l-sel .pbadge.big') || {}).textContent || '' }; })()`);
  check(p1.length === 1 && mark.ring && mark.badge === String(p1[0]) && mark.tog === "− POOL" && mark.big === String(p1[0]),
    `LIBRARY: + POOL: pool ${p1[0]}; the list's ring and badge "${mark.badge}", the panel's badge, the button now − POOL`);
  await shot("manager-03-library.png");
  check(await noScroll(), "LIBRARY at 1280: no horizontal scroll");
  await click("#l-pool");
  await idle();
  check(!(await inPool()).length && !(await evaluate(`!!document.querySelector("${MINE(3)} .pbadge")`)), "LIBRARY: − POOL: out of the pool, the badge gone");
  await click("#l-pool");                                // (in again: DELETE then says it is used)
  await idle();
  await click("#l-del");
  await waitFor("!document.getElementById('cf').hidden");
  const cfd = await evaluate("({ t: document.getElementById('cf-title').textContent, w: document.getElementById('cf-what').textContent, u: document.getElementById('cf-undo').textContent })");
  check(/^DELETE/.test(cfd.t) && cfd.w === "BELL TOWER" && /SHIFT \+ F1/.test(cfd.u), `LIBRARY: DELETE asks once: "${cfd.t}" ${cfd.w} · ${cfd.u}`);
  await shot("manager-06-confirm.png");
  await click("#cf-go");
  await waitFor("!__mgr.S.unit.rows.some((r) => r.source === 'mine' && r.name === 'BELL TOWER')");
  await idle();
  check(await evaluate("!__mgr.S.unit.rows.some((r) => r.source === 'mine' && r.name === 'BELL TOWER')"), "LIBRARY: DELETE confirmed: gone");

  // POOL
  await click("#t-pool");
  await waitFor("document.querySelectorAll('#p-grid .ps').length === 128");
  await idle();
  await sleep(800);
  const pc = await evaluate("({ m: document.querySelectorAll('#p-list .cas[data-src=mine]').length, f: document.querySelectorAll('#p-list .cas[data-src=factory]').length, seg: document.querySelectorAll('#p-filter [data-src]').length, tags: document.querySelectorAll('#p-filter [data-tag]').length })");
  check(pc.m > 0 && pc.f > 0 && pc.seg === 3 && pc.tags === 7, `POOL: the sounds beside with LIBRARY's filters (${pc.m} mine + ${pc.f} factory)`);
  const freeK = () => evaluate("[...document.querySelectorAll('#p-grid .ps:not(.full)')].map((e) => +e.dataset.k)[0]");
  const free = await freeK();
  await click("#p-list .cas[data-src='mine']");
  await click(`#p-grid .ps[data-k='${free}']`);
  await idle();
  check(await evaluate(`__mgr.S.unit.rows.some((r) => r.source === 'pool' && r.slot === ${free})`), `POOL: a MY preset picked, then put on place ${free} (POOL REF)`);
  await click("#p-filter [data-src='factory']");
  const free2 = await freeK();
  const facId = await evaluate("+document.querySelector('#p-list .cas[data-src=factory]').dataset.id");
  await click("#p-list .cas[data-src='factory']");
  const here = await evaluate("document.querySelectorAll('#p-grid .ps.here').length");
  await click(`#p-grid .ps[data-k='${free2}']`);
  await idle();
  check(await evaluate(`__mgr.S.unit.rows.some((r) => r.source === 'pool' && r.slot === ${free2} && r.id === ${facId})`), `POOL: a factory preset put on place ${free2} (POOL REF takes factory ids)`);
  await click("#p-filter [data-src='all']");
  await click(`#p-list .cas[data-id='${facId}']`);
  check((await evaluate("document.querySelectorAll('#p-grid .ps.here').length")) === here + 1 && (await evaluate(`!!document.querySelector("#p-list .cas[data-id='${facId}'].inpool .pbadge")`)),
    "POOL: a picked sound's places ringed in the grid; the list's badge as in LIBRARY");
  await click(`#p-list .cas[data-id='${facId}']`);
  await shot("manager-04-pool.png");
  check(await noScroll(), "POOL at 1280: no horizontal scroll");
  // a swap (ALT + right onto the next place): it asks, saying the undo is cleared
  await evaluate(`document.querySelector("#p-grid .ps[data-k='${free2}']").dispatchEvent(new KeyboardEvent('keydown', { key: 'ArrowRight', altKey: true, bubbles: true }))`);
  await waitFor("!document.getElementById('cf').hidden");
  const cfs = await evaluate("({ t: document.getElementById('cf-title').textContent, u: document.getElementById('cf-undo').textContent })");
  await click("#cf-go");
  await waitFor(`__mgr.S.unit.rows.some((r) => r.source === 'pool' && r.slot === ${free2 + 1} && r.id === ${facId})`, 20000);
  await idle();
  check(/^(SWAP|MOVE) POOL/.test(cfs.t) && cfs.u === "UNDO ON THE FM-1 IS CLEARED" && await evaluate(`__mgr.S.unit.rows.some((r) => r.source === 'pool' && r.slot === ${free2 + 1} && r.id === ${facId})`),
    `POOL: ALT + right: "${cfs.t}" · ${cfs.u}; confirmed: moved to ${free2 + 1}`);
  // SORT BY TAG asks; cancelled: nothing moves
  const before0 = await evaluate("JSON.stringify(__mgr.S.unit.rows.filter((r) => r.source === 'pool').map((r) => [r.slot, r.id]))");
  await click("#p-sort");
  const sortAsk = await waitFor("!document.getElementById('cf').hidden && document.getElementById('cf-title').textContent === 'SORT BY TAG'", 3000);
  if (sortAsk) await click("#cf-no");
  await sleep(300);
  check((!sortAsk && (await evaluate("document.getElementById('toast').textContent")) === "Sorted already") ||
    (sortAsk && before0 === await evaluate("JSON.stringify(__mgr.S.unit.rows.filter((r) => r.source === 'pool').map((r) => [r.slot, r.id]))")), `POOL: SORT BY TAG asks first; cancelled: nothing moved`);
  await click(`#p-grid .ps[data-k='${free}']`);
  await click("#p-clear");
  await waitFor("!document.getElementById('cf').hidden");
  check(await evaluate("document.getElementById('cf-title').textContent.startsWith('CLEAR POOL')"), "POOL: CLEAR asks first");
  await click("#cf-go");
  await idle();
  check(await evaluate(`!__mgr.S.unit.rows.some((r) => r.source === 'pool' && r.slot === ${free})`), `POOL: CLEAR confirmed: place ${free} empty again`);

  // IMPORT: synthetic Digitone files
  await click("#t-import");
  const a = dn2pstZip(wrapDn2Record(dn2RecordFromParams({ algo: 2 }, { name: "SYNTH BASS", tagBits: (1 << 10) | (1 << 24) })), { FormatVersion: 1, ProductType: ["24", "38"], Payload: "SYNTH BASS.dn2snd", FileType: "Sound", FirmwareVersion: "1.10D", MetaInfo: { Tags: ["BASS", "DARK"] } });
  const b = wrapDn2Record(dn2RecordFromParams({}, { name: "SYNTH PAD" }));
  const w = dn2RecordFromParams({}, { name: "WAVE TEST" }); w[244] = 1;
  const files = [["SYNTH BASS.dn2pst", a], ["SYNTH PAD.dn2snd", b], ["WAVE TEST.dn2snd", wrapDn2Record(w)], ["SYNTH KICK.dnsnd", dnsndV4()]].map(([n, x]) => ({ n, b64: Buffer.from(x).toString("base64") }));
  await evaluate(`__mgr.importer.add(${JSON.stringify(files)}.map((f) => new File([Uint8Array.from(atob(f.b64), (c) => c.charCodeAt(0))], f.n)))`);
  await waitFor("document.querySelectorAll('#i-rows .ir').length === 4");
  await shot("manager-05-import.png");
  check(await evaluate("document.querySelectorAll('#i-rows .ir.no').length === 1 && document.getElementById('i-send').textContent.includes('3')"), "IMPORT: four files, the WAVETONE one greyed, the version-4 Digitone 1 sound read: SEND 3");
  const before = await evaluate("__mgr.S.unit.rows.filter((r) => r.source === 'mine').length");
  await click("#i-send");
  await waitFor(`__mgr.S.unit.rows.filter((r) => r.source === 'mine').length === ${before + 3}`, 20000);
  await idle();
  check(await evaluate("['SYNTH BASS', 'SYNTH PAD', 'SYNTH KICK'].every((n) => __mgr.S.unit.rows.some((r) => r.source === 'mine' && r.name === n))"), "IMPORT: sent: all three in MY PRESETS");
  check(await evaluate("Object.values(__mgr.S.web.tags).some((t) => t.includes('DARK'))"), "IMPORT: the Digitone tag FIZZ has no tag for (DARK) kept as a web tag");
  check(await noScroll(), "IMPORT at 1280: no horizontal scroll");

  // EDIT (Manager P2-A): opened from LIBRARY, every group, every value; edits; the browser's FIZZ; SAVE AS NEW; EXPORT
  await click("#t-library");
  await click("#l-filter [data-src='factory']");
  await click("#l-list .cas[data-src='factory'][data-slot='4']");
  await waitFor("document.getElementById('l-edit') && !document.getElementById('l-edit').disabled");
  await click("#l-edit");
  await waitFor("!document.getElementById('tab-edit').hidden && document.querySelector('#ed-stage .algosvg')");
  const ename = await evaluate("__mgr.editor.sound().name");
  check(!!ename && (await evaluate("document.querySelector('#ed-head .h').textContent")).startsWith("FACTORY 4"), `EDIT: LIBRARY's EDIT opened FACTORY 4 ${ename}`);
  const GR = ["ops", "openv", "filter", "amp", "lfo", "mod", "play"], SHOT = { ops: "01-edit-operators", filter: "02-edit-filter-amp", lfo: "03-edit-lfo-mod" };
  const seen = new Set();
  const collect = () => evaluate(`[...document.querySelectorAll('#ed-stage [data-id], #ed-stage [data-x], #ed-stage [data-y], #ed-stage [data-pick], #ed-stage [data-set]')]
    .flatMap((e) => [e.dataset.id, e.dataset.x, e.dataset.y, e.dataset.pick, e.dataset.set && e.dataset.set.split(':')[0]]).filter((x) => x !== undefined).map(Number)`);
  for (const g of GR) {
    await click(`#ed-groups [data-g='${g}']`);
    await sleep(250);
    if (g === "mod") await evaluate("[0, 1, 2, 3, 4, 5, 6, 7].forEach((k) => __mgr.editor.set(86 + 3 * k, 1 + k % 8))");   // (every slot on: its cables)
    if (g === "filter") for (const ft of [0, 3, 4, 5, 6, 7]) { await evaluate(`__mgr.editor.set(36, ${ft})`); await sleep(120); (await collect()).forEach((i) => seen.add(i)); }
    if (g === "amp") for (const m of [1, 0]) { await evaluate(`__mgr.editor.set(74, ${m})`); await sleep(120); (await collect()).forEach((i) => seen.add(i)); }
    await sleep(200);
    (await collect()).forEach((i) => seen.add(i));
    check(await noScroll(), `EDIT ${g.toUpperCase()} at 1280: drawn, no horizontal scroll`);
    if (SHOT[g]) await shot(`manager-p2-${SHOT[g]}.png`);
  }
  const missing = Array.from({ length: 110 }, (_, i) => i).filter((i) => !seen.has(i));
  check(!missing.length, `EDIT: every one of FIZZ's 110 values is on a group${missing.length ? `: missing ${missing.join(", ")}` : ""}`);
  await evaluate("[0, 1, 2, 3, 4, 5, 6, 7].forEach((k) => __mgr.editor.set(86 + 3 * k, 0)); __mgr.editor.set(36, 3)");
  // a handle dragged (the filter's FREQ / RESO), the wheel over a value, ALGO clicked, REVERT
  await click("#ed-groups [data-g='filter']");
  await sleep(200);
  const f0 = await evaluate("[__mgr.editor.value(34), __mgr.editor.value(35)]");
  await drag("#ed-stage .curve .hd", -80, -40);
  await sleep(200);
  const f1 = await evaluate("[__mgr.editor.value(34), __mgr.editor.value(35)]");
  check(f1[0] < f0[0] && f1[1] > f0[1], `EDIT: the filter's handle dragged left and up: FREQ ${f0[0]} -> ${f1[0]}, RESO ${f0[1]} -> ${f1[1]}`);
  await click("#ed-groups [data-g='ops']");
  await sleep(200);
  const fb0 = await evaluate("__mgr.editor.value(6)"), wc = await centre("#ed-stage .dial[data-id='6']");
  await send("Input.dispatchMouseEvent", { type: "mouseWheel", x: wc.x, y: wc.y, deltaX: 0, deltaY: -100 });
  await sleep(150);
  const a0 = await evaluate("__mgr.editor.value(0)");
  await realClick("#ed-stage .algo");
  await sleep(150);
  check((await evaluate("__mgr.editor.value(6)")) === fb0 + 1 && (await evaluate("__mgr.editor.value(0)")) === a0 % 8 + 1, `EDIT: the wheel over FDBK (${fb0} -> ${fb0 + 1}); a click on ALGO: ${a0} -> ${a0 % 8 + 1}`);
  check(await evaluate("!document.getElementById('ed-revert').disabled && !!document.querySelector('#ed-head .edited')"), "EDIT: EDITED shown, REVERT on");
  await click("#ed-revert");
  await sleep(200);
  check(await evaluate("__mgr.editor.base().values.every((x, i) => x === __mgr.editor.sound().values[i]) && document.getElementById('ed-revert').disabled"), "EDIT: REVERT: the sound as it was opened");
  // the browser's FIZZ: started by the target's chip (a click), a key of the keybed sounds
  await realClick("#ed-target");
  const bf = await waitFor("__mgr.bfizz.ready", 20000);
  await evaluate(`(() => { const p = __mgr.bfizz.player, an = p.ctx.createAnalyser(); an.fftSize = 2048; p.out.connect(an); const b = new Float32Array(2048);
    window.__peak = () => { an.getFloatTimeDomainData(b); let m = 0; for (const x of b) m = Math.max(m, Math.abs(x)); return m; }; })()`);
  const kc = await centre("#ed-keys .wk[data-n='60']");
  await mouse("mousePressed", kc.x, kc.y + 20);
  let pk = 0; for (let i = 0; i < 12; i++) { await sleep(40); pk = Math.max(pk, await evaluate("window.__peak()")); }
  const lit = await evaluate("document.querySelector('#ed-keys .wk[data-n=\"60\"]').classList.contains('down')");
  await mouse("mouseReleased", kc.x, kc.y + 20);
  check(bf && pk > 0.005 && lit, `EDIT: the browser's FIZZ ${await evaluate("__mgr.bfizz.version")} plays it: C4 on the keybed (lit), output peak ${pk.toFixed(3)}`);
  // SAVE AS NEW: on the unit, as edited
  await evaluate("(() => { const i = document.getElementById('ed-name'); i.value = 'EDIT TEST'; i.dispatchEvent(new Event('input', { bubbles: true })); __mgr.editor.set(6, 77); __mgr.editor.set(83, 30); })()");
  await click("#ed-savenew");
  await waitFor("__mgr.S.unit.rows.some((r) => r.source === 'mine' && r.name === 'EDIT TEST')", 15000);
  await idle();
  const sv2 = await evaluate(`(async () => { const { soundFromFrame } = await import('./sound.js'); const r = __mgr.S.unit.rows.find((x) => x.source === 'mine' && x.name === 'EDIT TEST');
    const s = soundFromFrame(await __mgr.S.link.dump('mine', r.slot)); return { slot: r.slot, same: s.values.every((x, i) => x === __mgr.editor.sound().values[i]), fb: s.values[6], place: __mgr.editor.place().label }; })()`);
  check(sv2.same && sv2.fb === 77 && sv2.place === `MY PRESETS ${sv2.slot}`, `EDIT: SAVE AS NEW: MY PRESETS ${sv2.slot} EDIT TEST on the unit, its values the editor's (FDBK ${sv2.fb}); the editor now on it`);
  // EXPORT: the three homes, the carry rows, the download
  await evaluate("(() => { const e = __mgr.editor; e.set(66, 33); e.set(70, -3000); e.set(83, 40); })()");
  await click("#ed-export");
  await waitFor("!document.getElementById('ex').hidden");
  await click("#ex [data-k='dnsnd']");
  const ex = await evaluate(`({ n: document.querySelectorAll('#ex .fmt').length, exp: document.querySelectorAll('#ex .fmt .expmark').length, rows: [...document.querySelectorAll('#ex .lost')].map((e) => e.textContent.trim()),
    name: document.querySelector('#ex .exname').textContent })`);
  await shot("manager-p2-04-export.png");
  check(ex.n === 3 && ex.exp === 2 && ex.rows.some((r) => /LFO 3/.test(r) && /dropped/.test(r)) && ex.name === "EDIT TEST.dnsnd", `EXPORT: FIZZ / DIGITONE / DIGITONE II, EXPERIMENTAL twice; Digitone: ${ex.rows.join("; ")}`);
  await click("#ex [data-k='dn2']");
  await click("#ex [data-dn2='dn2snd']");
  const dl0 = await evaluate("window.__dl");
  check((await evaluate("document.querySelector('#ex .exname').textContent")) === "EDIT TEST.dn2snd", "EXPORT: Digitone II as .dn2snd");
  await click("#ex-go");
  await sleep(200);
  check((await evaluate("window.__dl")) === dl0 + 1 && (await evaluate("document.getElementById('ex').hidden")), "EXPORT: DOWNLOAD: one file, the dialog closed");
  await click("#t-library");
  await click("#l-export");
  check(await waitFor("!document.getElementById('ex').hidden"), "EXPORT from LIBRARY opens the same dialog");
  await click("#ex-no");

  // the phone
  await size(390, 844, true);
  for (const t of ["backup", "library", "edit", "pool", "import"]) {
    await click(`#t-${t}`);
    await sleep(500);
    check(await noScroll(), `${t.toUpperCase()} at 390: no horizontal scroll`);
    await shot(`manager-phone-${t}.png`);
  }
  check(!errors.length, `no console errors${errors.length ? `: ${errors.slice(0, 5).join(" | ")}` : ""}`);
} catch (e) {
  check(false, `the run: ${e.message}`);
}
console.log(fails ? `MANAGER PAGE TEST FAILED (${fails})` : `manager page test passed (screenshots in ${OUT})`);
process.exit(fails ? 1 : 0);
