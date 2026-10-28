// SPDX-License-Identifier: GPL-3.0-only
// The manager page (site/manager/) in headless Chromium over the DevTools protocol (as test_site.mjs: no Playwright,
// never a window), against its simulated FM-1 (?sim: the browser FIZZ behind a fake Web MIDI; headless Chromium's own
// Web MIDI mangles SysEx, so the unit is never reached from here):
//   - BROWSER (2026-10-27): the browser's FIZZ as the unit; PROJECTS on it (KEEP, LOAD INTO, WRITE, read back); the
//     computer keys on LIBRARY, IMPORT's rows heard (down, SPACE); back to the FM-1;
//   - every tab at 1280 x 800 and 390 x 844: no console error, no horizontal scroll (at 1280 no page scroll at all: the
//     cards' lists scroll; LIBRARY's IN POOL / NOT IN POOL; BACKUP's play button); the screenshots beside the
//     mock-ups' (mockups/png/fizz-web-manager-0N-*.png) for the eye;
//   - BACKUP: the STATUS numbers without a backup; BACK UP keeps the backup in this browser (no download), its save
//     button writes it to a file (showSaveFilePicker, stubbed here), DELETE asks first; a stored backup and a dropped
//     file open the restore;
//   - the Bluetooth package (2026-10-27): a Bluetooth backup restored onto this (USB) FIZZ, written in its layout;
//     MY PRESETS with fewer places than a presets-only backup brings (as the Bluetooth package's 128): the ones to add
//     chosen (ADD n), the rest listed;
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
//     unit as edited; EXPORT: three homes, EXPERIMENTAL on the Digitone's, the carry rows, .dn2snd, the download;
//   - TRACK (Manager P2-B, the live editor): the switch off leaves the tracks alone; on, a preset picked in LIBRARY
//     plays on the track (the bar: KEEP / CANCEL puts the track's own sound back, its DUMP the same); EDIT takes the
//     track, a drag is a PARAM on the unit, a value turned on the unit (a PARAM from elsewhere) shows in the editor,
//     REVERT puts the track back, the keybed plays the unit, SAVE over the preset (BUSY while playing; stopped, the
//     track then plays it, not edited); the header, the bar and EDIT at 1280 and 390;
//   - the owner's fixes (2026-10-25): the TRACK switch's first-use ring; EXPORT ALL (a .fizzlib); + POOL shown before
//     the unit's lists come back (LIST held 1.5 s); EDIT one panel with all 110 values (no groups), following the unit's
//     selected track (its panel's TRACKS + a key) with TRACK off: shown, a drag refused and the switch nudged; LOAD ON
//     FM-1 (the unit's browser: OCT+ on its panel loads, the page says so); IMPORT of a .fizzlib (all already there,
//     flagged, none ticked) and a new .fizzsnd;
//   - FIZZ MIDI (e): a MIDI track's sound STOREd on the unit: its MIDI chip in LIBRARY; EDIT's MIDI view (seven pages
//     of four bars, the CC numbers, untouched dashed): a bar dragged, the wheel, a CC number typed, a slot untouched;
//     SAVE AS NEW DUMPs back the same; EXPORT's Digitone refuses it; with TRACK on an edit reaches the unit's track (an
//     AUDITION); at 1280 x 800 and 390 x 844 (no horizontal scroll, screenshots manager-midi-edit-*.png).
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
async function fullShot(file) {
  const m = await evaluate("({ w: document.documentElement.scrollWidth, h: document.documentElement.scrollHeight })");
  const r = await send("Page.captureScreenshot", { format: "png", captureBeyondViewport: true, clip: { x: 0, y: 0, width: m.w, height: m.h, scale: 1 } });
  fs.writeFileSync(path.join(OUT, file), Buffer.from(r.data, "base64"));
}
const size = (w, h, mobile = false) => send("Emulation.setDeviceMetricsOverride", { width: w, height: h, deviceScaleFactor: 1, mobile });
const noScroll = () => evaluate("document.documentElement.scrollWidth <= window.innerWidth + 1");
const click = (sel) => evaluate(`(() => { const e = document.querySelector(${JSON.stringify(sel)}); if (!e) return false; e.click(); return true; })()`);
const idle = () => waitFor("window.__mgr && !__mgr.S.editing && !__mgr.S.busy && !__mgr.S.refreshing", 20000);   // (a REFRESH reading the lists: done)
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
  check(await waitFor("document.getElementById('live').classList.contains('hint') && !document.getElementById('live').hidden", 8000),
    "TRACK: the switch shown with its first-use ring (never used in this browser)");
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

  // the Bluetooth package: the unit's own backup fitted into the Bluetooth layout, restored onto this USB FIZZ
  await idle();
  const bleNames = await evaluate(`(async () => { const P = await import('./projects.js'), B = await import('./fizzbak.js');
    const b = __mgr.S.baks.find((x) => x.kind === 'full'), parsed = await B.readBackup(b.bytes, b.name);
    const img = P.imageOf(P.fitModel(P.modelOf(parsed.image), 'ble').model);
    await __mgr.openFile(B.makeFizzbak({ kind: 'full', image: img, presets: parsed.presets, fizz: 'test' }), 'BLUETOOTH.fizzbak');
    return parsed.summary.projects.map((x) => x.name); })()`);
  await waitFor("!document.getElementById('rs').hidden && document.getElementById('rs-safe').textContent.includes('saved first')", 60000);
  const bx = await evaluate("({ chips: document.getElementById('rs-chips').textContent, fit: document.getElementById('rs-fit').textContent, shown: !document.getElementById('rs-fit').hidden })");
  check(/BLUETOOTH BACKUP → USB PACKAGE/.test(bx.chips) && bx.shown && /Everything in this backup fits/.test(bx.fit), `RESTORE: a Bluetooth backup on this USB FIZZ: "${bx.fit}"`);
  await shot("manager-02b-restore-bluetooth.png");
  await evaluate("(() => { const i = document.getElementById('rs-type'); i.value = 'RESTORE'; i.dispatchEvent(new Event('input')); })()");
  await click("#rs-go");
  await waitFor("document.getElementById('rs').hidden && !__mgr.S.busy && __mgr.S.unit.projects", 90000);
  await idle();
  const blAfter = await evaluate("__mgr.S.unit.projects ? __mgr.S.unit.projects.list.filter((x) => x.there).map((x) => x.name) : []");
  check(blAfter.length > 0 && JSON.stringify(blAfter) === JSON.stringify(bleNames), `RESTORE: the Bluetooth backup written in this FIZZ's layout: ${blAfter.join(", ")}`);
  // fewer MY PRESETS places than a presets-only backup brings (the unit made a 2-places-left package here)
  await evaluate(`(async () => { const B = await import('./fizzbak.js');
    const b = __mgr.S.baks.find((x) => x.kind === 'full'), parsed = await B.readBackup(b.bytes, b.name), s0 = parsed.presets[0];
    const u = __mgr.S.unit, mine = u.rows.filter((r) => r.source === 'mine').length;
    window.__keepStatus = u.status; u.status = { ...u.status, package: 'ble', nproj: 4, nlib: mine + 2 };
    await __mgr.openFile(B.makeFizzbak({ kind: 'presets', presets: [1, 2, 3, 4, 5].map((k) => ({ ...s0, name: 'NEW ' + k })), fizz: 'test' }), 'MORE.fizzbak'); })()`);
  await waitFor("!document.getElementById('rs').hidden && document.getElementById('rs-go').textContent === 'ADD 2' && !document.getElementById('rs-fit').hidden", 30000);
  const pfit = await evaluate("({ fit: document.getElementById('rs-fit').textContent, go: document.getElementById('rs-go').textContent, on: document.querySelectorAll('#rs-fit input:checked').length, off: document.querySelectorAll('#rs-fit input:disabled').length })");
  check(pfit.go === "ADD 2" && pfit.on === 2 && pfit.off === 3 && /2 free places/.test(pfit.fit) && /Left out: 3/.test(pfit.fit), `ADD PRESETS: 2 places free of 5 new: ${pfit.go}, ${pfit.on} ticked, the rest listed ("${pfit.fit.slice(-60)}")`);
  await shot("manager-02c-presets-fit.png");
  await click("#rs-fit input:checked");
  const pk2 = await evaluate("({ go: document.getElementById('rs-go').textContent, off: document.querySelectorAll('#rs-fit input:disabled').length })");
  check(pk2.go === "ADD 1" && pk2.off === 0, `ADD PRESETS: one unticked: ${pk2.go}, the others free to pick`);
  await click("#rs-cancel");
  await evaluate("(() => { __mgr.S.unit.status = window.__keepStatus; })()");
  await idle();

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
  const outOfPool = () => waitFor(`!__mgr.S.unit.rows.some((x) => x.source === 'pool' && x.id === ${bellId})`);
  if (p0.length) { await click("#l-pool"); await outOfPool(); await idle(); }          // (the demo's pool may hold it: out first)
  check(!(await inPool()).length && (await evaluate("document.getElementById('l-pool').textContent")) === "+ POOL", "LIBRARY: not in the pool: the big + POOL");
  await evaluate("(() => { const l = __mgr.S.link, o = l.list.bind(l); window.__slowList = 0; l.list = async (a) => { window.__slowList++; await new Promise((r) => setTimeout(r, 1500)); return o(a); }; window.__unslow = () => { l.list = o; }; })()");
  const tPool = Date.now();
  await click("#l-pool");
  const fast = await waitFor(`!!document.querySelector("${MINE(3)} .pbadge") && window.__slowList > 0`, 1400);
  check(fast && Date.now() - tPool < 1450, `+ POOL: the badge shows at once (${Date.now() - tPool} ms), before the unit's lists come back (LIST held 1.5 s)`);
  await waitFor(`__mgr.S.unit.rows.some((x) => x.source === 'pool' && x.id === ${bellId})`);   // (+ POOL's edit, then its lists read again)
  await idle();
  await evaluate("window.__unslow()");
  const p1 = await inPool();
  const mark = await evaluate(`(() => { const c = document.querySelector("${MINE(3)}"); const b = c && c.querySelector('.pbadge'); return { ring: !!(c && c.classList.contains('inpool')), badge: b ? b.textContent : '', tog: document.getElementById('l-pool').textContent, big: (document.querySelector('#l-sel .pbadge.big') || {}).textContent || '' }; })()`);
  check(p1.length === 1 && mark.ring && mark.badge === String(p1[0]) && mark.tog === "− POOL" && mark.big === String(p1[0]),
    `LIBRARY: + POOL: pool ${p1[0]}; the list's ring and badge "${mark.badge}", the panel's badge, the button now − POOL`);
  await shot("manager-03-library.png");
  check(await noScroll(), "LIBRARY at 1280: no horizontal scroll");
  await click("#l-pool");
  await outOfPool();
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
  const seen = new Set();
  const collect = () => evaluate(`[...document.querySelectorAll('#ed-stage [data-id], #ed-stage [data-x], #ed-stage [data-y], #ed-stage [data-pick], #ed-stage [data-set]')]
    .flatMap((e) => [e.dataset.id, e.dataset.x, e.dataset.y, e.dataset.pick, e.dataset.set && e.dataset.set.split(':')[0]]).filter((x) => x !== undefined).map(Number)`);
  const secs = await evaluate("[...document.querySelectorAll('#ed-stage .pn')].map((e) => e.dataset.sec).join()");
  check(secs === "ops,openv,play,filter,amp,drive,lfo,mod", `EDIT: one panel, every section at once (${secs}), no groups to switch`);
  (await collect()).forEach((i) => seen.add(i));
  await evaluate("[0, 1, 2, 3, 4, 5, 6, 7].forEach((k) => __mgr.editor.set(86 + 3 * k, 1 + k % 8))");   // (every slot on: its cables)
  await sleep(150);
  (await collect()).forEach((i) => seen.add(i));
  for (const ft of [0, 3, 4, 5, 6, 7]) { await evaluate(`__mgr.editor.set(36, ${ft})`); await sleep(120); (await collect()).forEach((i) => seen.add(i)); }
  for (const m of [1, 0]) { await evaluate(`__mgr.editor.set(74, ${m})`); await sleep(120); (await collect()).forEach((i) => seen.add(i)); }
  const missing = Array.from({ length: 110 }, (_, i) => i).filter((i) => !seen.has(i));
  check(!missing.length, `EDIT: every one of FIZZ's 110 values is on the one panel${missing.length ? `: missing ${missing.join(", ")}` : ""}`);
  const dupIds = await evaluate(`(() => { const n = new Map(); document.querySelectorAll('#ed-stage .vc[data-id], #ed-stage .dial[data-id], #ed-stage .vbar[data-id]').forEach((e) => n.set(e.dataset.id, (n.get(e.dataset.id) || 0) + 1)); return [...n].filter(([, c]) => c > 1).map(([k]) => k); })()`);
  check(!dupIds.length, `EDIT: each value's chip, dial or bar once on the panel${dupIds.length ? ` (twice: ${dupIds.join(", ")})` : ""}`);
  check(await noScroll(), "EDIT at 1280: the panel drawn, no horizontal scroll");
  await fullShot("manager-p2-01-edit-panel.png");
  await evaluate("[0, 1, 2, 3, 4, 5, 6, 7].forEach((k) => __mgr.editor.set(86 + 3 * k, 0)); __mgr.editor.set(36, 3)");
  await sleep(250);                                    // (drawn again: the curve's handle RESO's, not the EQ's GAIN)
  // a handle dragged (the filter's FREQ / RESO), the wheel over a value, ALGO clicked, REVERT
  const f0 = await evaluate("[__mgr.editor.value(34), __mgr.editor.value(35)]");
  await drag("#ed-stage .curve .hd", -80, -40);
  await sleep(200);
  const f1 = await evaluate("[__mgr.editor.value(34), __mgr.editor.value(35)]");
  check(f1[0] < f0[0] && f1[1] > f0[1], `EDIT: the filter's handle dragged left and up: FREQ ${f0[0]} -> ${f1[0]}, RESO ${f0[1]} -> ${f1[1]}`);
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
  check(await waitFor("!document.getElementById('ex').hidden"), "EXPORT from LIBRARY (the selected preset's) opens the same dialog");
  await click("#ex-no");
  await click("#l-export-all");
  await waitFor("!document.getElementById('ex').hidden");
  const exa = await evaluate("({ t: document.querySelector('#ex .big').textContent, n: document.querySelector('#ex .exname').textContent, on: document.querySelector('#ex .fmt.on').dataset.k })");
  const dlA = await evaluate("window.__dl");
  await click("#ex-go");
  await sleep(200);
  check(exa.t === "MY PRESETS" && exa.on === "fizz" && /\.fizzlib$/.test(exa.n) && (await evaluate("window.__dl")) === dlA + 1, `EXPORT ALL: MY PRESETS as ${exa.n} (downloaded)`);

  // TRACK (P2-B): the live editor
  await click("#t-library");
  await click("#l-filter [data-src='all']");
  const trk = () => evaluate(`(async () => { const { soundFromFrame } = await import('./sound.js'); const f = await __mgr.S.link.dumpTrack(0); const s = soundFromFrame(f);
    return { name: s.name, t: s.track, id: s.extras.id, edited: s.extras.edited, v: s.values, frame: Array.from(f) }; })()`);
  await idle();
  const t0 = await trk();
  check(await evaluate("!document.getElementById('live').hidden && document.getElementById('live').getAttribute('aria-pressed') === 'false' && __mgr.live.state.cap"),
    "TRACK: the switch shown (this FIZZ edits tracks live), off");
  await click("#l-list .cas[data-src='factory'][data-slot='9']");
  await sleep(500);
  const t1 = await trk();
  check(t1.name === t0.name && JSON.stringify(t1.frame) === JSON.stringify(t0.frame) && (await evaluate("document.getElementById('loadbar').hidden")),
    `TRACK off: a preset picked in LIBRARY leaves track ${t0.t + 1} alone (${t0.name})`);
  await click("#live");
  await waitFor("__mgr.live.on()");
  const fac9 = await evaluate("__mgr.S.unit.rows.find((r) => r.source === 'factory' && r.slot === 7)");
  await click("#l-list .cas[data-src='factory'][data-slot='7']");
  await waitFor("!document.getElementById('loadbar').hidden");
  await idle();
  const t2 = await trk();
  check(t2.name === fac9.name && t2.id === fac9.id && !t2.edited && (await evaluate("document.getElementById('loadbar').textContent")).includes(fac9.name),
    `TRACK on: LIBRARY's ${fac9.name} plays on track ${t2.t + 1} (its preset ${t2.id}), the bar shows it`);
  await shot("manager-p2-05-track-load.png");
  await click("#lb-cancel");
  await waitFor("document.getElementById('loadbar').hidden");
  await sleep(300);
  const t3 = await trk();
  check(JSON.stringify(t3.frame) === JSON.stringify(t0.frame), `TRACK: CANCEL: track ${t0.t + 1} its own sound again (${t3.name}, its DUMP the same)`);
  // EDIT takes the track; a drag is a PARAM; the unit's own change shows; REVERT
  await click("#t-edit");
  await waitFor(`__mgr.editor.place().track === ${t0.t} && __mgr.editor.sound().name === ${JSON.stringify(t0.name)}`, 5000);
  check((await evaluate("!!document.querySelector('#ed-head .trkbig.live') && document.querySelector('#ed-head .trkbig b').textContent")) === String(t0.t + 1) &&
    (await evaluate(`!!document.querySelector('#ed-head .trk.live.on') && !!document.querySelector("#ed-head .trk[data-trk='${t0.t}'].on")`)) &&
    (await evaluate("__mgr.editor.place().label")).startsWith(`TRACK ${t0.t + 1}`),
    `EDIT takes track ${t0.t + 1} (big and pink in its header, LIVE on its strip): ${await evaluate("__mgr.editor.place().label")}`);
  await evaluate("__mgr.editor.set(36, 3)");
  await sleep(300);
  await drag("#ed-stage .curve .hd", -60, -30);
  await waitFor("!__mgr.live.state.pumping && !__mgr.live.state.q.size", 5000);
  const t4 = await trk(), ev = await evaluate("[__mgr.editor.value(34), __mgr.editor.value(35)]");
  check(t4.v[34] === ev[0] && t4.v[35] === ev[1] && t4.edited && ev[0] !== t0.v[34], `EDIT live: the handle dragged: the unit's track has FREQ ${t4.v[34]}, RESO ${t4.v[35]} (PARAMs), marked edited`);
  await evaluate(`(async () => { const { paramRequest } = await import('../js/fizz_sysex.js'); window.__sim.engine.midiIn(paramRequest({ track: ${t0.t}, id: 6, value: 111 })); window.__sim.run(20); })()`);
  check(await waitFor("__mgr.editor.value(6) === 111", 4000), "EDIT live: FDBK turned on the unit (111) shows in the editor (the track read back)");
  await shot("manager-p2-06-track-edit.png");
  await click("#ed-revert");
  await sleep(800);
  const t5 = await trk();
  check(JSON.stringify(t5.frame) === JSON.stringify(t0.frame) && (await evaluate("__mgr.editor.value(6)")) === t0.v[6], "EDIT live: REVERT: the track exactly as the editor took it");
  // the keybed plays the unit
  const n0 = await evaluate("__mgr.S.unit && window.__sim.counts.notes || 0");
  const kc2 = await centre("#ed-keys .wk[data-n='60']");
  await mouse("mousePressed", kc2.x, kc2.y + 20);
  await sleep(100);
  const lit2 = await evaluate("document.querySelector('#ed-keys .wk[data-n=\"60\"]').classList.contains('down')");
  await mouse("mouseReleased", kc2.x, kc2.y + 20);
  await sleep(100);
  const n1 = await evaluate("window.__sim.counts.notes");
  check(lit2 && n1 === n0 + 2, `EDIT live: the keybed plays the FM-1 (C4 lit; its note on and off sent to the unit: ${n1 - n0})`);
  // SAVE over the preset the track came from: BUSY while playing, then saved; the track plays it, not edited
  await click("#t-library");
  await click("#l-list .cas[data-src='mine'][data-slot='8']");
  await waitFor("!document.getElementById('loadbar').hidden");
  await click("#lb-keep");
  await click("#t-edit");
  await waitFor("__mgr.editor.place().source === 'mine' && __mgr.editor.place().slot === 8", 5000);
  await evaluate("__mgr.editor.set(6, 42)");
  await sleep(600);
  await evaluate("window.__sim.tapPlay()");
  await click("#ed-save");
  await waitFor("!document.getElementById('cf').hidden");
  await click("#cf-go");
  await idle();
  const busy = await evaluate("document.getElementById('say').textContent");
  await evaluate("window.__sim.tapPlay()");
  await click("#ed-save");
  await waitFor("!document.getElementById('cf').hidden");
  await click("#cf-go");
  await idle();
  await sleep(600);
  const t6 = await trk(), m8 = await evaluate(`(async () => { const { soundFromFrame } = await import('./sound.js'); return soundFromFrame(await __mgr.S.link.dump('mine', 8)).values[6]; })()`);
  check(/playing/i.test(busy) && m8 === 42 && !t6.edited && t6.v[6] === 42, `SAVE over MY PRESETS 8 with TRACK on: while playing "${busy.slice(0, 40)}"; stopped: saved (FDBK ${m8}), the track plays it, not edited`);
  check(await noScroll(), "TRACK on, EDIT at 1280: no horizontal scroll");
  await click("#t-import");
  check(await evaluate("!document.querySelector('#i-dest [data-d=track]').disabled"), "TRACK on: IMPORT may load onto a track");
  await click("#live");
  check(await waitFor("!__mgr.live.on() && document.getElementById('loadbar').hidden"), "TRACK off again");
  check(await evaluate("document.querySelector('#i-dest [data-d=track]').disabled && __mgr.importer.state.dest !== 'track'"), "TRACK off: IMPORT's TRACK destination off too (the tracks left alone)");

  // EDIT follows the unit's selected track, TRACK off: shown, not changed
  await click("#t-edit");
  await sleep(300);
  await click("#ed-head [data-trk='selected']");
  await waitFor("__mgr.live.following() && __mgr.editor.place().fromTarget", 5000);
  await evaluate("window.__sim.selectTrack(2)");
  const fol = await waitFor("__mgr.editor.place().track === 2 && document.querySelector('#ed-head .trkbig b').textContent === '3'", 6000);
  const t3s = await trk();
  check(fol && t3s.t === 2 && (await evaluate("__mgr.editor.sound().name")) === t3s.name && (await evaluate("document.getElementById('live-n').textContent")) === "3",
    `EDIT follows the FM-1: track 3 selected on the unit's panel: the editor on it (${t3s.name}), TRACK 3 big in its header and on the switch`);
  await fullShot("manager-fix-edit-follows-1280.png");
  const fdk = await evaluate("__mgr.editor.value(6)");
  await evaluate("__mgr.editor.set(6, (__mgr.editor.value(6) + 20) % 127)");
  await sleep(100);
  const t3b = await trk();
  check((await evaluate("__mgr.editor.value(6)")) === fdk && t3b.v[6] === t3s.v[6] && (await evaluate("document.getElementById('live').classList.contains('nudge')")),
    "TRACK off: a change in EDIT refused (FDBK kept, the unit's track untouched), the TRACK switch nudged");
  await evaluate(`(async () => { const { paramRequest } = await import('../js/fizz_sysex.js'); window.__sim.engine.midiIn(paramRequest({ track: 2, id: 6, value: 99 })); window.__sim.run(20); })()`);
  check(await waitFor("__mgr.editor.value(6) === 99 && document.getElementById('ed-revert').disabled", 4000), "TRACK off: FDBK turned on the unit (99) shows in the editor (nothing to revert)");
  await evaluate("window.__sim.selectTrack(0)");
  await waitFor("__mgr.editor.place().track === 0", 6000);

  // LOAD ON FM-1: the unit's own browser on the preset; OCT+ on its panel loads it
  await click("#t-library");
  await click("#l-filter [data-src='all']");
  await click("#l-list .cas[data-src='factory'][data-slot='6']");
  await waitFor("!document.getElementById('l-unit').disabled");
  const f6 = await evaluate("__mgr.S.unit.rows.find((r) => r.source === 'factory' && r.slot === 6)");
  await click("#l-unit");
  const opened = await waitFor(`!!__mgr.live.browsing() && document.getElementById('l-unit').classList.contains('on')`, 5000);
  const tb = await trk();
  check(opened && tb.id === f6.id && tb.name === f6.name, `LOAD ON FM-1: ${f6.name} in the unit's browser on track ${tb.t + 1} (previewing); the button lit with OCT- / OCT+`);
  await shot("manager-fix-load-on-fm1.png");
  await evaluate("window.__sim.press('OCT+')");
  const done = await waitFor(`!__mgr.live.browsing() && document.getElementById('toast').textContent.includes(${JSON.stringify(f6.name)})`, 6000);
  const tl = await trk();
  check(done && tl.id === f6.id && !tl.edited, `LOAD ON FM-1: OCT+ on the unit: loaded (track ${tl.t + 1} plays ${tl.name}); the page says so`);

  // IMPORT: a .fizzlib of MY PRESETS (all there already: flagged, none ticked) and a new .fizzsnd
  await click("#t-import");
  await click("#i-clear");
  await evaluate(`(async () => { const { exportFiles } = await import('./exporter.js'); const { soundFromStoreFrame } = await import('../js/library.js');
    const s = [...__mgr.S.unit.dumps.values()].map(soundFromStoreFrame); const lib = exportFiles(s, 'fizz');
    const n = { ...s[0], name: 'FRESH ONE', blob: s[0].blob.slice() }; n.blob[3] = (n.blob[3] + 5) & 15; const one = exportFiles([n], 'fizz');
    window.__nlib = s.length;
    await __mgr.importer.add([new File([lib.bytes], lib.name), new File([one.bytes], one.name)]); })()`);
  await waitFor("document.querySelectorAll('#i-rows .ir').length === window.__nlib + 1");
  const imp = await evaluate("({ rows: document.querySelectorAll('#i-rows .ir').length, dup: [...document.querySelectorAll('#i-rows .note.dup')].length, send: document.getElementById('i-send').textContent, fz: document.querySelectorAll('#i-rows .src.fz').length })");
  check(imp.dup === imp.rows - 1 && imp.fz === imp.rows && /SEND 1 /.test(imp.send), `IMPORT: a .fizzlib (${imp.rows - 1}, every one "already in MY PRESETS", not ticked) and a new .fizzsnd: ${imp.send}`);
  await shot("manager-fix-import-fizzlib.png");
  const nb = await evaluate("__mgr.S.unit.rows.filter((r) => r.source === 'mine').length");
  await click("#i-send");
  await waitFor(`__mgr.S.unit.rows.filter((r) => r.source === 'mine').length === ${nb + 1}`, 15000);
  await idle();
  check(await evaluate("__mgr.S.unit.rows.filter((r) => r.source === 'mine' && r.name === 'FRESH ONE').length === 1"), "IMPORT: only the new one sent (FRESH ONE in MY PRESETS)");

  // every page fits the window (the owner, 2026-10-26): at 1280 x 800 the page never scrolls, each card's list does;
  // LIBRARY's last preset picked keeps its panel in view; IN POOL / NOT IN POOL; BACKUP's play button
  const fit = [];
  for (const t of ["backup", "library", "edit", "pool", "import"]) {   // (PROJECTS: in BROWSER, below: the sim's 1 MiB read is slow)
    await click(`#t-${t}`);
    await sleep(400);
    if (!(await evaluate("document.documentElement.scrollHeight <= window.innerHeight + 1 && window.scrollY === 0"))) fit.push(t);
  }
  check(!fit.length, `every tab fits 1280 x 800, no page scroll${fit.length ? `: not ${fit.join(", ")}` : ""}`);
  await click("#t-library");
  const lib = await evaluate(`(async () => { const l = document.getElementById('l-list'); l.scrollTop = l.scrollHeight; const c = [...l.querySelectorAll('.cas')].pop(); c.click();
    await new Promise((r) => setTimeout(r, 300)); const r = document.getElementById('l-sel').getBoundingClientRect();
    return { scrolls: l.scrollHeight > l.clientHeight, top: r.top, page: window.scrollY }; })()`);
  check(lib.scrolls && lib.page === 0 && lib.top > 0 && lib.top < 200, `LIBRARY: the list scrolls by itself; the last preset picked, its panel stays at ${Math.round(lib.top)} px`);
  const pf = await evaluate(`(async () => { const n = () => document.querySelectorAll('#l-list .cas').length, t = (k) => document.querySelector('#l-filter [data-pool="' + k + '"]'), w = () => new Promise((r) => setTimeout(r, 150));
    const all = n(); t("in").click(); await w(); const a = n(); t("out").click(); await w(); const b = n(); t("out").click(); await w(); return { all, a, b, back: n() }; })()`);
  check(pf.a > 0 && pf.b > 0 && pf.a + pf.b === pf.all && pf.back === pf.all, `LIBRARY: IN POOL ${pf.a} + NOT IN POOL ${pf.b} = ${pf.all}; off again ${pf.back}`);
  await click("#t-backup");
  check(await evaluate("[...document.querySelectorAll('#baks .bk')].some((b) => b.querySelector('.icon.play'))"), "BACKUP: a full backup has its play button (the browser's FIZZ)");

  // BROWSER (the owner, 2026-10-27): FIZZ in this browser as the unit, heard here; PROJECTS on it: KEEP, LOAD INTO a
  // slot, WRITE (the store before kept in BACKUP), read back; the computer keys on LIBRARY and IMPORT
  await realClick("#bunit");
  const bOn = await waitFor("__mgr.S.mode === 'browser' && !!__mgr.S.link && __mgr.live.on()", 40000);
  check(bOn && (await evaluate("document.getElementById('bunit').dataset.state === 'on' && document.getElementById('unit').dataset.state === 'idle'")),
    `BROWSER: the browser's FIZZ as the FM-1 (${await evaluate("__mgr.S.unit && __mgr.S.unit.version")}), TRACK on`);
  await click("#t-projects");
  const read1 = await waitFor("!!__mgr.projtab.state.base && !__mgr.S.busy", 60000);
  const nbp0 = await evaluate("__mgr.S.baks.length");
  await evaluate("__mgr.projtab.state.sel = 0; __mgr.projtab.render()");
  await click('#pj-acts [data-act="keep"]');
  const kept = await waitFor("(__mgr.projtab.state.kept || []).length === 1", 5000);
  await click('#pj-view [data-v="kept"]');
  await click('#pj-kept .bp[data-i="0"]');
  await click('#pj-tools [data-act="kload"]');
  await click('#pj-slots .slot[data-p="2"]');
  const staged = await evaluate("__mgr.projtab.state.ops.length === 1 && !document.getElementById('pj-pending').hidden");
  await shot("manager-projects-staged.png");
  check(read1 && kept && staged, "PROJECTS: the store read, PROJECT 1 KEPT in this browser, LOAD INTO slot 3 waits in the yellow bar");
  await click('#pj-pending [data-act="write"]');
  const wrote = await waitFor("__mgr.projtab.state.ops.length === 0 && !__mgr.S.busy", 60000);
  await evaluate("__mgr.projtab.state.base = null; __mgr.projtab.show()");
  await waitFor("!!__mgr.projtab.state.base && !__mgr.S.busy", 60000);
  const p3 = await evaluate("__mgr.S.unit.projects.list[2]");
  check(wrote && p3 && p3.there && (await evaluate(`__mgr.S.baks.length === ${nbp0 + 1} && /before projects/.test(__mgr.S.baks[0].name)`)),
    `PROJECTS: WRITE: slot 3 is ${p3 && p3.name} on the browser's FIZZ, read back; the store before kept in BACKUP`);
  check(await evaluate("document.documentElement.scrollHeight <= window.innerHeight + 1"), "PROJECTS fits 1280 x 800");
  await shot("manager-projects.png");
  const peakJs = `(async (key) => { const p = __mgr.bu().player; if (!window.__an) { window.__an = p.ctx.createAnalyser(); window.__an.fftSize = 2048; p.out.connect(window.__an); }
    const b = new Float32Array(2048), pk = () => { window.__an.getFloatTimeDomainData(b); let m = 0; for (const x of b) m = Math.max(m, Math.abs(x)); return m; };
    document.dispatchEvent(new KeyboardEvent("keydown", { key, bubbles: true })); let m = 0;
    for (let i = 0; i < 15; i++) { await new Promise((r) => setTimeout(r, 40)); m = Math.max(m, pk()); }
    document.dispatchEvent(new KeyboardEvent("keyup", { key, bubbles: true })); return m; })`;
  await click("#t-library");
  await click("#l-list .cas[data-src='factory'][data-slot='1']");
  await waitFor("!document.getElementById('loadbar').hidden", 5000);
  const lp = await evaluate(`${peakJs}("a")`);
  check(lp > 0.002, `LIBRARY: a preset picked, the A key plays it on the browser's FIZZ (peak ${lp.toFixed(3)})`);
  await click("#lb-cancel");
  await click("#t-import");
  await click("#i-clear");
  await evaluate(`(async () => { const L = await import("../js/library.js"); const blob = new Uint8Array(128); blob[0] = 0x51; blob[1] = 6; blob[2] = 3; blob[3] = 8; blob[50] = 60;
    const mk = (name) => new File([L.soundToFizzsnd({ fizz: true, name, blob, drive: 0, pan: 0, sends: { chorus: 0, delay: 0, reverb: 0 }, fizzTags: ["LEAD"], favourite: false, notes: [], origin: [], mark: "", tags: [] }, {})], name + ".fizzsnd");
    await __mgr.importer.add([mk("HEAR ONE"), mk("HEAR TWO")]); })()`);
  await click('#i-rows .ir[data-k="0"] .nm');
  const heard = await waitFor("/HEAR ONE/.test(document.getElementById('loadbar').innerText)", 5000);
  await evaluate(`document.dispatchEvent(new KeyboardEvent("keydown", { key: "ArrowDown", bubbles: true }))`);
  const heard2 = await waitFor("/HEAR TWO/.test(document.getElementById('loadbar').innerText) && __mgr.importer.state.cur === 1", 5000);
  await evaluate(`document.dispatchEvent(new KeyboardEvent("keydown", { key: " ", bubbles: true }))`);
  const iq = await evaluate("({ bar: document.getElementById('loadbar').innerText, cur: __mgr.importer.state.cur, on: __mgr.importer.state.items.map((x) => x.on), n: __mgr.importer.state.items.length, ok: __mgr.importer.state.items.map((x) => x.ok) })");
  check(heard && heard2 && (await evaluate("__mgr.importer.state.items[1].on === false")), `IMPORT: a row clicked plays on the track, down the next one, SPACE unticks it${heard && heard2 ? "" : ` (${JSON.stringify(iq)})`}`);
  await click("#lb-cancel");
  await click("#i-clear");
  // back to the (simulated) FM-1
  await click("#unit");
  check(await waitFor("__mgr.S.mode === 'fm1' && !!__mgr.S.link && __mgr.S.unit && __mgr.S.unit.rows.length > 0", 30000), "FM-1 again: the unit back from BROWSER");

  // the phone
  await size(390, 844, true);
  for (const t of ["backup", "library", "edit", "pool", "import"]) {
    await click(`#t-${t}`);
    await sleep(500);
    check(await noScroll(), `${t.toUpperCase()} at 390: no horizontal scroll`);
    await (t === "edit" ? fullShot : shot)(`manager-phone-${t}.png`);
  }
  // the phone with TRACK on: a preset picked plays on the track (the bar), EDIT on the track
  await click("#t-library");
  await click("#live");
  await waitFor("__mgr.live.on()");
  await click("#l-list .cas[data-src='factory'][data-slot='5']");
  await waitFor("!document.getElementById('loadbar').hidden");
  await idle();
  await evaluate("window.scrollTo(0, 0)");
  check(await noScroll(), "TRACK on, LIBRARY at 390: the switch and the bar fit");
  await shot("manager-phone-track-load.png");
  await click("#t-edit");
  await waitFor("__mgr.editor.place().fromTarget === true", 5000);
  await sleep(400);
  check(await noScroll(), "TRACK on, EDIT at 390: no horizontal scroll");
  await shot("manager-phone-track-edit.png");
  await click("#lb-cancel");
  await click("#live");
  // FIZZ MIDI (e): a MIDI track's sound in LIBRARY and EDIT (last: the BROWSER part's AUDITIONs long done)
  await size(1280, 800);
  await sleep(1500);                                   // (the phone's CANCEL done)
  await click("#t-library");
  await evaluate("__mgr.live.on() && __mgr.live.setOn(false)");
  await idle();
  const mst = await evaluate(`(async () => { const X = await import('../js/fizz_sysex.js'), Sd = await import('./sound.js'); const m = Sd.newMidi();
    m.slots[0] = { cc: 16, value: 64, touched: true }; m.slots[2] = { cc: 18, value: 20, touched: true }; m.slots[3] = { cc: 19, value: 127, touched: true }; m.slots[9] = { cc: 74, value: 90, touched: true };
    const r = await __mgr.S.link.store(X.storeSound({ name: 'MIDI DRUMS', blob: Sd.midiBlob(m) })); return r && r.status === 0 ? r.slot : 0; })()`);
  await evaluate("(async () => { __mgr.S.unit.dumps = null; await __mgr.refreshRows(); __mgr.tab('library'); })()");
  await idle();
  await waitFor(`__mgr.S.unit.dumps && __mgr.S.unit.dumps.has(${mst})`, 20000);
  await click("#l-filter [data-src='mine']");
  await sleep(200);
  check(mst > 0 && (await evaluate(`!!document.querySelector("#l-list .cas[data-src='mine'][data-slot='${mst}'] .mchip") && document.querySelectorAll('#l-list .mchip').length === 1`)),
    `MIDI: MY PRESETS ${mst} MIDI DRUMS (a MIDI sound) carries the MIDI chip in LIBRARY, the only one`);
  await click(`#l-list .cas[data-src='mine'][data-slot='${mst}']`);
  await waitFor("document.getElementById('l-edit') && !document.getElementById('l-edit').disabled");
  check(await evaluate("!!document.querySelector('#l-sel .mchip') && !!document.querySelector('#l-sel .selhead .din')"), "MIDI: LIBRARY's selected preset: the MIDI mark and chip");
  await click("#l-edit");
  await waitFor("!document.getElementById('tab-edit').hidden && document.querySelectorAll('#ed-stage .mbar').length === 28");
  const mv = await evaluate(`({ pages: document.querySelectorAll('#ed-stage .mpg').length, bars: document.querySelectorAll('#ed-stage .mbar').length, un: document.querySelectorAll('#ed-stage .msl.un').length,
    cc: [...document.querySelectorAll('#ed-stage .mcc')].slice(0, 5).map((e) => e.textContent), fm: document.querySelectorAll('#ed-stage .algosvg, #ed-stage .pn-ops').length,
    chip: !!document.querySelector('#ed-head .mchip'), vals: [...document.querySelectorAll('#ed-stage .mval b')].slice(0, 4).map((e) => e.textContent),
    col: getComputedStyle(document.querySelector('#ed-stage .mbar[data-slot="0"] i')).backgroundColor })`);
  check(mv.pages === 7 && mv.bars === 28 && mv.un === 24 && mv.cc.join() === "CC 16,CC 17,CC 18,CC 19,CC 20" && !mv.fm && mv.chip && mv.vals.join() === "64,—,20,127" && mv.col === "rgb(63, 125, 255)",
    `MIDI: EDIT shows the MIDI view: 7 pages of 4 bars (${mv.un} untouched), ${mv.cc.join(" ")} .., values ${mv.vals.join(" ")}, KNOB 1 ${mv.col}; FM groups ${mv.fm}`);
  // a bar dragged (slot 2, untouched: now touched), the wheel (slot 5 from 0), a CC number typed (slot 1), a slot untouched (10)
  const bc = await centre("#ed-stage .mbar[data-slot='1']");
  await mouse("mousePressed", bc.x, bc.y + 30);
  for (let k = 1; k <= 5; k++) { await mouse("mouseMoved", bc.x, bc.y + 30 - 12 * k, { buttons: 1 }); await sleep(20); }
  await mouse("mouseReleased", bc.x, bc.y - 30);
  await sleep(150);
  const s1 = await evaluate("__mgr.editor.slot(1)");
  const wb = await centre("#ed-stage .mbar[data-slot='4']");
  for (let k = 0; k < 3; k++) { await send("Input.dispatchMouseEvent", { type: "mouseWheel", x: wb.x, y: wb.y, deltaX: 0, deltaY: -100 }); await sleep(200); }
  const s4 = await evaluate("__mgr.editor.slot(4)");
  await realClick("#ed-stage .mcc[data-cc='0']");
  await waitFor("document.activeElement && document.activeElement.id === 'ed-cc'", 3000);
  await evaluate("(() => { const i = document.getElementById('ed-cc'); i.value = '7'; i.dispatchEvent(new Event('input', { bubbles: true })); })()");
  await send("Input.insertText", { text: "4" });
  await send("Input.dispatchKeyEvent", { type: "keyDown", key: "Enter", code: "Enter", windowsVirtualKeyCode: 13 });
  await send("Input.dispatchKeyEvent", { type: "keyUp", key: "Enter", code: "Enter", windowsVirtualKeyCode: 13 });
  await sleep(200);
  const s0 = await evaluate("__mgr.editor.slot(0)");
  const dc = await centre("#ed-stage .mbar[data-slot='9']");
  await mouse("mousePressed", dc.x, dc.y, { clickCount: 1 }); await mouse("mouseReleased", dc.x, dc.y, { clickCount: 1 });
  await mouse("mousePressed", dc.x, dc.y, { clickCount: 2 }); await mouse("mouseReleased", dc.x, dc.y, { clickCount: 2 });
  await sleep(200);
  const s9 = await evaluate("__mgr.editor.slot(9)");
  const un9 = await evaluate(`document.querySelector('#ed-stage .mbar[data-slot="9"]').closest('.msl').classList.contains('un') && !!document.querySelector('#ed-head .edited')`);
  check(s1.touched && s1.value > 0 && s4.touched && s4.value >= 3 && s0.cc === 74 && s0.value === 64 && !s9.touched && un9,
    `MIDI: a bar dragged (CC 17 = ${s1.value}, touched), the wheel (CC 20 = ${s4.value}), a CC number typed (CC ${s0.cc}), a double click: CC 74's slot untouched (${!s9.touched}); EDITED`);
  await evaluate("window.scrollTo(0, 0)");
  check(await noScroll(), "MIDI: EDIT's MIDI view at 1280: no horizontal scroll");
  await fullShot("manager-midi-edit-1280.png");
  // SAVE AS NEW: DUMPed back the same, still MIDI
  await evaluate("(() => { const i = document.getElementById('ed-name'); i.value = 'MIDI EDITED'; i.dispatchEvent(new Event('input', { bubbles: true })); })()");
  await click("#ed-savenew");
  await waitFor("__mgr.S.unit.rows.some((r) => r.source === 'mine' && r.name === 'MIDI EDITED')", 15000);
  await idle();
  const msv = await evaluate(`(async () => { const Sd = await import('./sound.js'); const r = __mgr.S.unit.rows.find((x) => x.source === 'mine' && x.name === 'MIDI EDITED');
    const s = Sd.soundFromFrame(await __mgr.S.link.dump('mine', r.slot)); return { slot: r.slot, midi: !!s.midi, same: Sd.sameValues(s, __mgr.editor.sound()) }; })()`);
  check(msv.midi && msv.same, `MIDI: SAVE AS NEW: MY PRESETS ${msv.slot} DUMPs back as the editor's MIDI sound`);
  // EXPORT: the Digitone's refuse it, FIZZ keeps it
  await click("#ed-export");
  await waitFor("!document.getElementById('ex').hidden");
  await click("#ex [data-k='dnsnd']");
  const mex = await evaluate("({ off: document.getElementById('ex-go').disabled, rows: [...document.querySelectorAll('#ex .lost')].map((e) => e.textContent.trim()) })");
  await shot("manager-midi-export.png");
  await click("#ex [data-k='fizz']");
  const mfz = await evaluate("({ off: document.getElementById('ex-go').disabled, name: document.querySelector('#ex .exname').textContent })");
  await click("#ex-no");
  check(mex.off && mex.rows.length === 1 && /MIDI SOUND/.test(mex.rows[0]) && !mfz.off && mfz.name === "MIDI EDITED.fizzsnd",
    `MIDI: EXPORT: Digitone refused (${mex.rows.join("; ")}), DOWNLOAD off; FIZZ: ${mfz.name}`);
  // TRACK on: the MIDI preset onto the track (KEEP), EDIT takes the track, an edit is an AUDITION on the unit
  await click("#t-library");
  await click("#live");
  await waitFor("__mgr.live.on()");
  await click(`#l-list .cas[data-src='mine'][data-slot='${msv.slot}']`);
  await waitFor("!document.getElementById('loadbar').hidden");
  await idle();
  await click("#lb-keep");
  await click("#t-edit");
  await waitFor("__mgr.editor.place().fromTarget === true && !!__mgr.editor.sound().midi && document.querySelectorAll('#ed-stage .mbar').length === 28", 8000);
  await evaluate("__mgr.editor.setSlot(6, { value: 55, touched: true, cc: 33 })");
  await waitFor("!__mgr.live.state.pumping && !__mgr.live.state.q.size", 5000);
  await sleep(300);
  const mt = await evaluate(`(async () => { const Sd = await import('./sound.js'); const s = Sd.soundFromFrame(await __mgr.S.link.dumpTrack(0)); return { midi: !!s.midi, x: s.midi && s.midi.slots[6], edited: s.extras.edited }; })()`);
  check(mt.midi && mt.x.value === 55 && mt.x.cc === 33 && mt.x.touched && mt.edited, `MIDI, TRACK on: a slot set in EDIT reaches the unit's track (CC ${mt.x && mt.x.cc} = ${mt.x && mt.x.value}), marked edited`);
  await fullShot("manager-midi-edit-track-1280.png");
  // .. on the phone
  await size(390, 844, true);
  await sleep(400);
  await evaluate("window.scrollTo(0, 0)");
  check(await noScroll(), "MIDI: EDIT's MIDI view at 390: no horizontal scroll");
  await fullShot("manager-midi-edit-390.png");
  await size(1280, 800);
  await click("#live");
  await waitFor("!__mgr.live.on()");
  check(!errors.length, `no console errors${errors.length ? `: ${errors.slice(0, 5).join(" | ")}` : ""}`);
} catch (e) {
  check(false, `the run: ${e.message}`);
}
console.log(fails ? `MANAGER PAGE TEST FAILED (${fails})` : `manager page test passed (screenshots in ${OUT})`);
process.exit(fails ? 1 : 0);
