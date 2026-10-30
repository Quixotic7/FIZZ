// SPDX-License-Identifier: GPL-3.0-only
// The manager page (site/manager/) in headless Chromium over the DevTools protocol (as test_site.mjs: no Playwright,
// never a window), against its simulated FM-1 (?sim: the browser FIZZ behind a fake Web MIDI; headless Chromium's own
// Web MIDI mangles SysEx, so the unit is never reached from here):
//   - BROWSER (2026-10-27): the browser's FIZZ as the unit (the unit picker); PROJECTS on it (KEEP, LOAD INTO, WRITE,
//     read back); the computer keys on LIBRARY, IMPORT's rows heard (down, SPACE); back to the FM-1 (⇄ FM-1);
//   - the offline mode (2026-10-29, last): no Web MIDI allowed (?sim=nomidi): THIS BROWSER the unit at start, quiet
//     until the first click (then the sound pill mutes); the picker (Escape, a click outside, a phone's bottom sheet),
//     CONNECT FM-1, back; RESET (asked, MY PRESETS kept in BACKUP first, a fresh store); Web MIDI allowed with no FM-1
//     (?sim=away): the FM-1 plugged in is offered (⇄ FM-1, refused while a write runs), one click switches, unplugged
//     back on THIS BROWSER;
//   - every tab at 1280 x 800 and 390 x 844: no console error, no horizontal scroll (at 1280 no page scroll at all: the
//     cards' lists scroll; LIBRARY's IN POOL / NOT IN POOL; BACKUP's play button); the screenshots beside the
//     mock-ups' (mockups/png/fizz-web-manager-0N-*.png) for the eye;
//   - BACKUP: the STATUS numbers without a backup; BACK UP keeps the backup in this browser (no download), its •••
//     menu's SAVE TO FILE writes it to a file (showSaveFilePicker, stubbed here), DELETE asks first; a stored backup and a dropped
//     file open the restore;
//   - the Bluetooth package (2026-10-27): a Bluetooth backup restored onto this (USB) FIZZ, written in its layout;
//     MY PRESETS with fewer places than a presets-only backup brings (as the Bluetooth package's 128): the ones to add
//     chosen (ADD n), the rest listed;
//   - REFRESH: a preset saved on the unit shows after the header's button, and after the tab comes back (focus);
//   - LIBRARY: one list, the filters ALL / FACTORY / MY PRESETS and the tags; a tag toggled, a preset renamed, moved,
//     + POOL then − POOL (the pool's badge with its place), DELETE (a confirm, then gone);
//   - POOL: the sounds beside with the same filters, a MY preset and a factory one put on empty places, a swap (its
//     moving card says the undo is cleared), CLEAR (a confirm), SORT BY TAG cancelled;
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
//   - FIZZ NEON's LIBRARY features: the search ("/", no notes while typing, Escape), the keys' strip, USED BY's step
//     strips, SELECT (shift range, cmd toggle; EXPORT, TAG, + POOL, − POOL, DELETE with its one confirm), IN THIS
//     BROWSER (KEEP IN BROWSER, kept over a reload, SEND TO FM-1), MINE ▾ at 390;
//   - FIZZ MIDI (e): a MIDI track's sound STOREd on the unit: its MIDI chip in LIBRARY; EDIT's MIDI view (seven pages
//     of four bars, the CC numbers, untouched dashed): a bar dragged, the wheel, a CC number typed, a slot untouched;
//     SAVE AS NEW DUMPs back the same; EXPORT's Digitone refuses it; with TRACK on an edit reaches the unit's track (an
//     AUDITION); at 1280 x 800 and 390 x 844 (no horizontal scroll, screenshots manager-midi-edit-*.png);
//   - bug fixes (2026-10-29): a project RENAME, + POOL and every other write with its reply lost, its request lost,
//     replies late, or the first LIST after it stale: shown on every tab without a reload, nothing said failed.
//   - NEON features (2026-10-29): EDIT's ALGO ‹ › (wrapping), the knob row (a drag, the same value elsewhere, the wiggle
//     only while dragged), the tag chip's popover (SAVE writes the tag), the phone's save bar (fixed, the panel's end
//     clear of it), one panel or pages (1440 x 900 one panel with every value; 1280 x 800 one panel or pages as
//     measured, keys shown or hidden, no scroll; 390 pages, each reachable, DRIVE and LEVEL on theirs), the keys'
//     toggle and the page remembered over a reload;
//     POOL's moving card (CANCEL, ESC, MOVE TO; at 390 fixed at the bottom), the tiles' tag icons; BACKUP's share
//     (navigator.share with a File, or a save where the browser cannot share files, cancelled: nothing), the ••• menu
//     (SAVE TO FILE, DELETE asks);
//   - parity with the iOS app (2026-10-29, last): the FM-1's port, USB or BLUETOOTH (?sim&bt), in the picker, on ⇄ FM-1
//     and the pill; NEW on an empty PROJECTS slot (the FM-1 and THIS BROWSER); LOAD ON FM-1 onto a track 1..6 (kept);
//   - EDIT pan and sends (2026-10-29): PAN, CHO, DEL, REV on AMP (panel, page, phone), dragged, SAVE AS NEW; live
//     PARAM 115..118, NOT FOUND's AUDITION (stubbed); the browser's FIZZ the same;
//   - nf-new (parity 2026-10-29): LIBRARY's + NEW (FM / MIDI) saved onto the FM-1 or, IN THIS BROWSER the source, into
//     the browser's store (IndexedDB read again); the IN THIS BROWSER card's rename, tags, DUPLICATE, EDIT -> SAVE,
//     SEND, LOAD, KEEP back with pan and sends kept; RESET then + NEW; LOAD ON FM-1's track under the name; 390 x 844;
//   - nf-units (2026-10-29): two FM-1s (?sim=two) in the picker, each by its port with USB or BLUETOOTH; RemoteFM1 and
//     the update loader greyed and named, not to pick; the second picked answers alone; PORTS from the picker and the
//     LOG (every port, what answered); the second unplugged: its row gone at once; a phone with no horizontal scroll;
//   - nf-speed (2026-10-29): THIS BROWSER's PROJECTS LOAD, WRITE and BACK UP read and written in the page, each under a
//     threshold, the moved pattern there after a re-read and in the running browser FIZZ; the FM-1 (?sim) still paced;
//   - nf-askload (2026-10-29): LOAD ON FM-1 of an IN THIS BROWSER preset the FM-1 lacks asks ADD · MY PRESETS n (the
//     first free place): CANCEL sends nothing, ADD stores it there and opens the unit's browser; one it has opens at
//     once; MY PRESETS full: FULL, nothing sent;
//   - nf-proj2 (the owner's fixes, 2026-10-29), on ?sim and THIS BROWSER: LIBRARY's first CTRL / CMD-click adds to the one
//     preset selected; PROJECTS: empty, no-trig and ghost-only patterns drag (an empty one clears the target); CLEAR at
//     once (dashed, waiting); DELETE / BACKSPACE clear; CTRL / CMD + X cut, + V paste; a click is not a drag; the
//     drag's ghost keeps the grabbed point under the pointer;
//   - nf-play2 (2026-10-29): PLAY flat (no glow around the board, no gradient on its buttons and keys), no THIS
//     BROWSER chip on it, key 11's longer LED; Shift-click latches (THIS BROWSER's panel; the FM-1's CC 127 held, 0 on
//     the second click; leaving the tab lets go); 1440 x 900 and 844 x 390 screenshots in .build/test/play2;
//   - nf-edit3 (2026-10-30): EDIT one panel on one 20 x 15 grid (every tile on its lines, MOD the same height empty or
//     full, ALGO centred, flat: no shadows on the panel, no glow in the head, the tabs, the keys; small handles; the
//     filter's 8-way TYPE); drag to modulate (LFO 2's plug, VEL, the card, ×, FULL, LFO 1's reach), tap to arm on the
//     page tabs (1280 x 700) and a phone (390 x 844), a drop with TRACK on reaches ?sim as PARAMs;
//   - nf-flat (2026-10-30, "Flatten everything."): no glow outside EDIT on every tab at 1440 x 900, the progress
//     screen, the picker, PORTS, a toast, + NEW, a confirm, LIBRARY at 390, THIS BROWSER's header (no coloured
//     box-shadow / text-shadow / drop-shadow but the lights kept, nothing breathing); screenshots in .build/test/flat;
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
const centre = (sel) => evaluate(`(() => { let e = document.querySelector(${JSON.stringify(sel)}); if (!e) return null;
  const pn = e.closest('#ed-stage.paged .pn');
  if (pn && !pn.classList.contains('cur')) { document.querySelector('#ed-pages [data-page="' + pn.dataset.sec + '"]').click(); e = document.querySelector(${JSON.stringify(sel)}); }
  e.scrollIntoView({ block: "center" }); const r = e.getBoundingClientRect(); return { x: r.left + r.width / 2, y: r.top + r.height / 2 }; })()`);
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
  await click("#baks .bk [data-act='more']");          // (NEON: SAVE TO FILE and DELETE behind the row's ••• menu)
  await click("#bk-menu [data-act='save']");
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
  await click("#baks .bk [data-act='more']");
  await click("#bk-menu [data-act='del']");
  await waitFor("!document.getElementById('cf').hidden");
  const cf = await evaluate("document.getElementById('cf-title').textContent");
  await click("#cf-no");
  check(cf === "DELETE BACKUP" && (await evaluate("__mgr.S.baks.length")) === nb1, "BACKUP: DELETE asks first; CANCEL keeps it");
  await click("#baks .bk [data-act='more']");
  await click("#bk-menu [data-act='del']");
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
  // a USB backup with 6 projects and 150 MY presets opened on a Bluetooth unit (this one made to look like one: its
  // store's size and STATUS's package): the projects and presets to keep chosen, the rest listed; not restored here
  await evaluate(`(async () => { const P = await import('./projects.js'), B = await import('./fizzbak.js'), F = await import('./fizzstore.js');
    const b = __mgr.S.baks.find((x) => x.kind === 'full'), parsed = await B.readBackup(b.bytes, b.name);
    const m = P.fitModel(P.modelOf(parsed.image), 'device').model, p0 = P.projects(m).find((x) => x.there).p;
    for (const q of [1, 2, 3, 4, 5]) if (q !== p0) P.loadProject(m, q, P.keepProject(m, p0));
    const rec = [...m.lib.entries()].find(([k]) => k >>> 24 === 5)[1];
    for (let i = 0; i < 150; i++) { const k = (5 << 24) | i; if (m.lib.has(k)) continue; const r = rec.slice(); r.fill(0, 2, 15); [...('MANY ' + i)].forEach((c, j) => { r[2 + j] = c.charCodeAt(0); }); r[20] ^= (i + 1) & 0x7f; r[143] = i & 0xff; r[144] = 0x10 + (i >> 8); m.lib.set(k, r); }
    const u = __mgr.S.unit;
    window.__keepUnit = { info: u.info, status: u.status };
    u.info = { ...u.info, size: F.LAYOUTS.ble.size }; u.status = { ...u.status, package: 'ble', nproj: 4, nlib: 128 };
    await __mgr.openFile(B.makeFizzbak({ kind: 'full', image: P.imageOf(m), presets: [], fizz: 'test' }), 'USB BIG.fizzbak'); })()`);
  await waitFor("!document.getElementById('rs').hidden && document.getElementById('rs-safe').textContent.includes('saved first')", 60000);
  const fx = await evaluate("({ chips: document.getElementById('rs-chips').textContent, fit: document.getElementById('rs-fit').textContent, then: document.getElementById('rs-then').textContent, lists: [...document.querySelectorAll('#rs-fit .fitlist .kv')].map((e) => e.textContent) })");
  check(/USB BACKUP → BLUETOOTH PACKAGE/.test(fx.chips) && /choose what to keep/.test(fx.fit) && fx.lists.join("|") === "PROJECTS 4 / 4|MY PRESETS 128 / 128" && /Left out: 2 projects/.test(fx.fit) && fx.then === "4 · 128",
    `RESTORE onto a Bluetooth unit: ${fx.lists.join(", ")}, "${(fx.fit.match(/Left out:[^.]*/) || [""])[0].slice(0, 90)}..."`);
  await shot("manager-02d-restore-fit.png");
  await click("#rs-fit .fitlist[data-kind=p] input:checked");
  const fx2 = await evaluate("({ lists: [...document.querySelectorAll('#rs-fit .fitlist .kv')].map((e) => e.textContent), then: document.getElementById('rs-then').textContent })");
  check(fx2.lists[0] === "PROJECTS 3 / 4" && fx2.then === "3 · 128", `RESTORE onto a Bluetooth unit: a project unticked: ${fx2.lists[0]}, then ${fx2.then}`);
  await click("#rs-cancel");
  await evaluate("(() => { Object.assign(__mgr.S.unit, window.__keepUnit); })()");
  await idle();
  // (that backup out of the list again: BROWSER boots the newest full one)
  await evaluate("(() => { const i = __mgr.S.baks.findIndex((b) => b.name === 'USB BIG.fizzbak'); document.querySelectorAll('#baks .bk')[i].querySelector('[data-act=more]').click(); document.querySelector('#bk-menu [data-act=del]').click(); })()");
  await waitFor("!document.getElementById('cf').hidden");
  await click("#cf-go");
  await waitFor("!__mgr.S.baks.some((b) => b.name === 'USB BIG.fizzbak')");

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
  await waitFor("document.querySelector('#l-users .uchip, #l-users .pchip, #l-users .ub')");
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
  // a swap (ALT + right onto the next place): it asks (NEON: the moving card), saying the undo is cleared
  await evaluate(`document.querySelector("#p-grid .ps[data-k='${free2}']").dispatchEvent(new KeyboardEvent('keydown', { key: 'ArrowRight', altKey: true, bubbles: true }))`);
  await waitFor("!document.getElementById('p-move').hidden");
  const cfs = await evaluate("({ t: document.querySelector('#p-move .mvline').textContent, u: document.querySelector('#p-move .mvundo').textContent })");
  await click("#p-mv-go");
  await waitFor(`__mgr.S.unit.rows.some((r) => r.source === 'pool' && r.slot === ${free2 + 1} && r.id === ${facId})`, 20000);
  await idle();
  check(new RegExp(`^(Moving|Swapping) ${free2} `).test(cfs.t) && cfs.u === "UNDO ON THE FM-1 CLEARS" && await evaluate(`__mgr.S.unit.rows.some((r) => r.source === 'pool' && r.slot === ${free2 + 1} && r.id === ${facId})`),
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
  const secs = await evaluate("({ s: [...document.querySelectorAll('#ed-stage .pn')].map((e) => e.dataset.sec).join(), mode: __mgr.editor.state.mode, tabs: [...document.querySelectorAll('#ed-pages [data-page]')].map((b) => b.dataset.page).join() })");
  check(secs.s === "ops,openv,play,filter,amp,drive,lfo,mod" && (secs.mode === "panel" ? secs.tabs === "" : secs.tabs === "ops,openv,play,filter,amp,drive,lfo,mod"),
    `EDIT at 1280: every section (${secs.s}) as ${secs.mode === "panel" ? "one panel" : `pages (${secs.tabs})`}`);
  (await collect()).forEach((i) => seen.add(i));
  await evaluate("[0, 1, 2, 3, 4, 5, 6, 7].forEach((k) => __mgr.editor.set(86 + 3 * k, 1 + k % 8))");   // (every slot on: its cables)
  await sleep(150);
  (await collect()).forEach((i) => seen.add(i));
  for (const ft of [0, 3, 4, 5, 6, 7]) { await evaluate(`__mgr.editor.set(36, ${ft})`); await sleep(120); (await collect()).forEach((i) => seen.add(i)); }
  for (const m of [1, 0]) { await evaluate(`__mgr.editor.set(74, ${m})`); await sleep(120); (await collect()).forEach((i) => seen.add(i)); }
  const missing = Array.from({ length: 110 }, (_, i) => i).filter((i) => !seen.has(i));
  check(!missing.length, `EDIT: every one of FIZZ's 110 values is on the one panel${missing.length ? `: missing ${missing.join(", ")}` : ""}`);
  // (nf-edit3: every value a tile of the grid, once; DRIVE and LEVEL no longer twice)
  const dupIds = await evaluate(`(() => { const n = new Map(); document.querySelectorAll('#ed-stage .t[data-id]').forEach((e) => n.set(e.dataset.id, (n.get(e.dataset.id) || 0) + 1)); return [...n].filter(([, c]) => c > 1).map(([k]) => k); })()`);
  check(!dupIds.length, `EDIT: each value's tile once on the panel${dupIds.length ? ` (twice: ${dupIds.join(", ")})` : ""}`);
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
  const fb0 = await evaluate("__mgr.editor.value(6)"), wc = await centre("#ed-stage .t[data-id='6']");
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

  // ---- bug fixes: rename + refresh after writes (the owner, 2026-10-29, on the FM-1: a project's RENAME did not take,
  // + POOL showed only after a reload). The unit loses a frame at times: here the sim loses a reply (__sim.hooks.reply
  // "drop"), loses a request, delays every reply, or the first LIST after a write is the one from before. Each write
  // still shows on every tab without a reload, and nothing says it failed when it did not
  {
    if (await evaluate("__mgr.live.on()")) { await click("#live"); await waitFor("!__mgr.live.on()"); }
    const dropReply = (cmd, kind = 0x52) => evaluate(`window.__lost = 0; __sim.hooks.reply = (d) => (d[5] === ${kind} && (${kind} !== 0x52 || d[6] === ${cmd}) && !window.__lost++ ? "drop" : 0)`);
    const dropAsk = (cmd) => evaluate(`(() => { const u = __sim; if (!u.__rx) u.__rx = u.rx.bind(u); window.__dropAsk = 0; u.rx = (d) => { if (d[5] === ${cmd} && !window.__dropAsk++) return; u.__rx(d); }; })()`);
    const clean = () => evaluate("(() => { __sim.hooks.reply = null; if (__sim.__rx) __sim.rx = __sim.__rx; })()");
    const settled = () => waitFor("!__mgr.S.editing && !__mgr.S.writing && !__mgr.S.busy && !__mgr.S.refreshing", 30000);
    const said = () => evaluate("document.getElementById('say').textContent");
    const lost = () => evaluate("window.__lost || window.__dropAsk || 0");
    const logHas = (re) => evaluate(`${re}.test(document.getElementById('log').textContent)`);
    const pj = () => evaluate("__mgr.S.unit.projects.list.map((p) => ({ there: p.there, current: p.current, name: p.name }))");
    // PROJECTS without the store read (the sim's 1 MiB read is slow): the slots are the unit's PROJECTS list
    const showProjects = () => evaluate("(() => { for (const t of ['backup', 'library', 'edit', 'pool', 'projects', 'import']) document.getElementById('tab-' + t).hidden = t !== 'projects'; __mgr.projtab.render(); })()");
    const renameProject = async (p, name) => {
      await evaluate(`__mgr.projtab.state.sel = ${p}; __mgr.projtab.render()`);
      await click('#pj-acts [data-act="rename"]');
      await waitFor("!!document.getElementById('pj-rename')", 3000);
      await evaluate(`(() => { const i = document.getElementById('pj-rename'); i.value = ${JSON.stringify(name)}; i.dispatchEvent(new KeyboardEvent('keydown', { key: 'Enter', bubbles: true })); })()`);
      await waitFor(`__mgr.S.unit.projects.list[${p}].name === ${JSON.stringify(name)} && !__mgr.S.editing && !__mgr.S.writing`, 20000);
      await settled();
      await sleep(300);
      return evaluate(`({ unit: __mgr.S.unit.projects.list[${p}].name, slot: (document.querySelector('#pj-slots .slot[data-p="${p}"] .sname') || {}).textContent, big: document.getElementById('pj-name').textContent })`);
    };
    await evaluate("__mgr.S.link.project('new', 2).then(() => __mgr.refreshRows({ projects: true }))");
    await showProjects();
    let pr = await renameProject(1, "CURRENT ONE");
    check(pr.unit === "CURRENT ONE" && pr.slot === pr.unit && pr.big === pr.unit, `PROJECTS: RENAME the open project: ${JSON.stringify(pr)}`);
    await dropReply(0x13);
    pr = await renameProject(0, "LOST REPLY");
    check((await lost()) === 1 && pr.unit === "LOST REPLY" && pr.slot === pr.unit && !(await said()) && (await logHas("/RENAME: its reply lost, the FM-1 has it/")),
      `PROJECTS: RENAME another project (a job), its reply lost: the projects read again show it (${JSON.stringify(pr)})`);
    await clean();
    await dropAsk(0x13);
    pr = await renameProject(0, "LOST ASK");
    check((await lost()) === 1 && pr.unit === "LOST ASK" && pr.slot === pr.unit && !(await said()) && (await logHas("/RENAME: no reply and not on the FM-1: sent again/")),
      `PROJECTS: RENAME, the request lost: not on the unit, sent once more (${JSON.stringify(pr)})`);
    await clean();
    // COPY TO, DELETE: their replies lost
    await evaluate("__mgr.projtab.state.sel = 0; __mgr.projtab.render()");
    await dropReply(0x24);
    await click('#pj-acts [data-act="copyto"]');
    await click('#pj-slots .slot[data-p="2"]');
    await waitFor("__mgr.S.unit.projects.list[2].there && !__mgr.S.editing && !__mgr.S.writing", 30000);
    await settled();
    let pl = await pj();
    check((await lost()) === 1 && pl[2].there && !!(await evaluate("document.querySelector('#pj-slots .slot[data-p=\"2\"] .sname')")) && !(await said()),
      `PROJECTS: COPY TO slot 3, its reply lost: slot 3 shown (${pl[2].name})`);
    await dropReply(0x24);
    await evaluate("__mgr.projtab.state.sel = 2; __mgr.projtab.render()");
    await click('#pj-acts [data-act="delete"]');
    await waitFor("!document.getElementById('cf').hidden");
    await click("#cf-go");
    await waitFor("!__mgr.S.unit.projects.list[2].there && !__mgr.S.editing && !__mgr.S.writing", 30000);
    await settled();
    check((await lost()) === 1 && (await evaluate("document.querySelector('#pj-slots .slot[data-p=\"2\"]').classList.contains('empty')")) && !(await said()),
      "PROJECTS: DELETE slot 3, its reply lost: shown empty");
    await clean();
    await evaluate("__mgr.S.link.project('load', 1).then(() => __mgr.S.link.project('delete', 2)).then(() => __mgr.S.link.rename('project', 1, 'PROJECT 1')).then(() => __mgr.refreshRows({ projects: true }))");
    await settled();
    pl = await pj();
    check(pl[0].current && pl[0].name === "PROJECT 1" && !pl[1].there, "PROJECTS: the sim's projects as before (PROJECT 1 open, slot 2 empty)");

    // LIBRARY + POOL: the POOL REF's reply lost (the owner's report); then with every reply late and the first LIST
    // after the write the one from before
    await click("#t-library");
    const freeMine = () => evaluate("(() => { const rs = __mgr.S.unit.rows, inp = new Set(rs.filter((x) => x.source === 'pool').map((x) => x.id)); const r = rs.find((x) => x.source === 'mine' && !inp.has(x.id) && x.name !== 'EDIT TEST'); return r && { slot: r.slot, id: r.id, name: r.name }; })()");
    const shownInPool = async (r) => {
      const at = await evaluate(`__mgr.S.unit.rows.filter((x) => x.source === 'pool' && x.id === ${r.id}).map((x) => x.slot)`);
      const badge = await evaluate(`(document.querySelector("${MINE(r.slot)} .pbadge") || {}).textContent || ""`);
      await click("#t-pool");
      await sleep(200);
      const tile = at.length ? await evaluate(`(() => { const t = document.querySelector("#p-grid .ps[data-k='${at[0]}']"); return !!t && t.classList.contains('full') && t.querySelector('.nm').textContent; })()`) : "";
      await click("#t-library");
      return { at, badge, tile };
    };
    let fm = await freeMine();
    await click(MINE(fm.slot));
    await settled();
    await dropReply(0x16);
    await click("#l-pool");
    await waitFor(`__mgr.S.unit.rows.some((x) => x.source === 'pool' && x.id === ${fm.id})`, 20000);
    await settled();
    let sp = await shownInPool(fm);
    check((await lost()) === 1 && sp.at.length === 1 && sp.badge === String(sp.at[0]) && sp.tile === fm.name && !(await said()),
      `+ POOL, its reply lost: LIBRARY's badge ${sp.badge}, POOL's place ${sp.at[0]} ${sp.tile}, no reload, nothing said`);
    // − POOL, its reply lost
    await click(MINE(fm.slot));
    await settled();
    await dropReply(0x14);
    await click("#l-pool");
    const out = await waitFor(`!__mgr.S.unit.rows.some((x) => x.source === 'pool' && x.id === ${fm.id}) && !document.querySelector("${MINE(fm.slot)} .pbadge") && !__mgr.S.editing`, 20000);
    await settled();
    await sleep(500);
    check((await lost()) === 1 && out && !(await evaluate(`!!document.querySelector("${MINE(fm.slot)} .pbadge")`)) && !(await said()), "− POOL, its reply lost: out of the pool, the badge gone");
    await clean();
    fm = await freeMine();
    await click(MINE(fm.slot));
    await settled();
    await evaluate(`(() => { __sim.hooks.reply = () => 120; const l = __mgr.S.link, o = l.list.bind(l), before = __mgr.S.unit.rows.map((x) => ({ ...x })); window.__stale = 0;
      l.list = async (a) => (window.__stale++ ? o(a) : before); window.__unstale = () => { l.list = o; }; })()`);
    await click("#l-pool");
    await waitFor(`__mgr.S.unit.rows.some((x) => x.source === 'pool' && x.id === ${fm.id}) && window.__stale > 1`, 20000);
    await settled();
    await evaluate("window.__unstale()");
    sp = await shownInPool(fm);
    check(sp.at.length === 1 && sp.badge === String(sp.at[0]) && sp.tile === fm.name && (await evaluate("window.__stale")) >= 2,
      `+ POOL, every reply 120 ms late and the first LIST after it stale: read again (${await evaluate("window.__stale")} LISTs), badge ${sp.badge}, POOL ${sp.at[0]}`);
    await clean();

    // TAGS, RENAME, MOVE, DELETE in LIBRARY; EDIT's header follows the preset it came from
    await click(MINE(fm.slot));
    await settled();
    const tg0 = await evaluate(`__mgr.S.unit.rows.find((r) => r.source === 'mine' && r.slot === ${fm.slot}).tags`);
    await dropReply(0x12);
    await click("#l-sel [data-dtag='5']");
    await waitFor(`__mgr.S.unit.rows.find((r) => r.source === 'mine' && r.slot === ${fm.slot}).tags === ${tg0 ^ 32} && !__mgr.S.editing && !__mgr.S.writing`, 20000);
    await settled();
    check((await lost()) === 1 && (await evaluate("document.querySelector(\"#l-sel [data-dtag='5']\").classList.contains('" + (tg0 & 32 ? "off" : "on") + "')")) && !(await said()),
      `TAGS, the reply lost: DRUMS toggled, shown (tags ${tg0} -> ${tg0 ^ 32})`);
    await clean();
    await click("#l-edit");
    await waitFor(`__mgr.editor.place().source === 'mine' && __mgr.editor.place().slot === ${fm.slot}`, 5000);
    await click("#t-library");
    await click(MINE(fm.slot));
    await settled();
    await dropAsk(0x13);
    await evaluate("(() => { const i = document.getElementById('l-name'); i.value = 'LOST ASK'; i.dispatchEvent(new Event('change')); })()");
    await waitFor(`__mgr.S.unit.rows.some((r) => r.source === 'mine' && r.slot === ${fm.slot} && r.name === 'LOST ASK') && !__mgr.S.editing && !__mgr.S.writing`, 20000);
    await settled();
    check((await lost()) === 1 && (await evaluate(`document.querySelector("${MINE(fm.slot)}").textContent.includes('LOST ASK')`)) && (await evaluate("__mgr.editor.sound().name")) === "LOST ASK" && !(await said()),
      "RENAME a MY preset, the request lost: sent again, shown in LIBRARY and EDIT's name");
    await clean();
    const to = fm.slot + 1;
    await dropReply(0x15);
    await evaluate(`document.querySelector("${MINE(fm.slot)}").dispatchEvent(new KeyboardEvent('keydown', { key: 'ArrowDown', altKey: true, bubbles: true }))`);
    await waitFor(`__mgr.S.unit.rows.some((r) => r.source === 'mine' && r.id === ${fm.id} && r.slot === ${to}) && !__mgr.S.editing && !__mgr.S.writing`, 20000);
    await settled();
    const ep = await evaluate("({ slot: __mgr.editor.place().slot, label: document.querySelector('#ed-head .h').textContent })");
    check((await lost()) === 1 && ep.slot === to && /MY PRESETS\s*\d+/.test(ep.label) && ep.label.includes(String(to)) && !(await said()),
      `MOVE (ALT + down), its reply lost: MY PRESETS ${to}; EDIT's header follows it (${ep.label.trim()})`);
    await click(MINE(to));
    await settled();
    await dropReply(0x14);
    await click("#l-del");
    await waitFor("!document.getElementById('cf').hidden");
    await click("#cf-go");
    await waitFor(`!__mgr.S.unit.rows.some((r) => r.source === 'mine' && r.id === ${fm.id}) && !__mgr.S.editing && !__mgr.S.writing`, 20000);
    await settled();
    check((await lost()) === 1 && !(await evaluate(`[...document.querySelectorAll('#l-list .cas')].some((c) => +c.dataset.id === ${fm.id})`)) &&
      (await evaluate("__mgr.editor.place().source === 'deleted' && document.getElementById('ed-save').disabled")) && !(await said()),
      "DELETE, its reply lost: gone from LIBRARY; EDIT's SAVE off (not over the place another preset may take)");
    await clean();
    // SAVE AS NEW from EDIT, its reply lost: LIBRARY shows it
    await click("#t-edit");
    await evaluate("(() => { const i = document.getElementById('ed-name'); i.value = 'LOST SAVE'; i.dispatchEvent(new Event('input', { bubbles: true })); })()");
    await dropReply(0, 0x50);
    await click("#ed-savenew");
    await click("#t-library");
    const saved = await waitFor("[...document.querySelectorAll('#l-list .cas')].some((c) => c.textContent.includes('LOST SAVE'))", 20000);
    const onIt = await waitFor("__mgr.editor.place().source === 'mine' && __mgr.S.unit.rows.some((r) => r.source === 'mine' && r.slot === __mgr.editor.place().slot && r.name === 'LOST SAVE')", 20000);
    await settled();
    check((await lost()) === 1 && saved && onIt && !(await said()),
      "SAVE AS NEW, its reply lost: the lists read again, LIBRARY shows it, the editor on it, nothing said");
    await clean();

    // POOL: a place, a swap, CLEAR, each reply lost
    await click("#t-pool");
    await waitFor("document.querySelectorAll('#p-grid .ps').length === 128");
    await settled();
    const freeK = () => evaluate("[...document.querySelectorAll('#p-grid .ps:not(.full)')].map((e) => +e.dataset.k).find((k) => k < 128 && !document.querySelector(`#p-grid .ps[data-k='${k + 1}']`).classList.contains('full'))");
    const k1 = await freeK();
    const pid = await evaluate("+document.querySelector('#p-list .cas[data-src=mine]').dataset.id");
    await click("#p-list .cas[data-src='mine']");
    await dropReply(0x16);
    await click(`#p-grid .ps[data-k='${k1}']`);
    await waitFor(`__mgr.S.unit.rows.some((r) => r.source === 'pool' && r.slot === ${k1} && r.id === ${pid}) && !__mgr.S.editing && !__mgr.S.writing`, 20000);
    await settled();
    check((await lost()) === 1 && (await evaluate(`document.querySelector("#p-grid .ps[data-k='${k1}']").classList.contains('full')`)) && !(await said()), `POOL: a place (${k1}), its reply lost: shown`);
    await dropReply(0x15);
    await evaluate(`document.querySelector("#p-grid .ps[data-k='${k1}']").dispatchEvent(new KeyboardEvent('keydown', { key: 'ArrowRight', altKey: true, bubbles: true }))`);
    await waitFor("!document.getElementById('p-move').hidden");   // (NEON: the moving card asks)
    await click("#p-mv-go");
    await waitFor(`__mgr.S.unit.rows.some((r) => r.source === 'pool' && r.slot === ${k1 + 1} && r.id === ${pid}) && !__mgr.S.editing && !__mgr.S.writing`, 20000);
    await settled();
    check((await lost()) === 1 && (await evaluate(`document.querySelector("#p-grid .ps[data-k='${k1 + 1}']").classList.contains('full') && !document.querySelector("#p-grid .ps[data-k='${k1}']").classList.contains('full')`)) && !(await said()),
      `POOL: a move ${k1} -> ${k1 + 1} (SWAP), its reply lost: shown`);
    await evaluate(`document.querySelector("#p-grid .ps[data-k='${k1 + 1}']").click()`);
    await dropReply(0x14);
    await click("#p-clear");
    await waitFor("!document.getElementById('cf').hidden");
    await click("#cf-go");
    await waitFor(`!__mgr.S.unit.rows.some((r) => r.source === 'pool' && r.slot === ${k1 + 1}) && !__mgr.S.editing && !__mgr.S.writing`, 20000);
    await settled();
    check((await lost()) === 1 && !(await evaluate(`document.querySelector("#p-grid .ps[data-k='${k1 + 1}']").classList.contains('full')`)) && !(await said()), `POOL: CLEAR ${k1 + 1}, its reply lost: shown empty`);
    await clean();

    // KEEP on TRACK: who uses the preset (USERS) read again
    await click("#t-library");
    await click("#live");
    await waitFor("__mgr.live.on()");
    const km = await evaluate("__mgr.S.unit.rows.find((r) => r.source === 'mine').slot");
    await click(MINE(km));
    await waitFor("!document.getElementById('loadbar').hidden", 8000);
    await settled();
    await sleep(300);
    const u0 = await evaluate("__sim.counts.cmds[0x17] || 0");
    await click("#lb-keep");
    check(await waitFor(`(__sim.counts.cmds[0x17] || 0) > ${u0}`, 5000), "KEEP on TRACK: LIBRARY's \"Used by\" read again (USERS)");
    await click("#live");
    await waitFor("!__mgr.live.on()");
    await settled();
  }
  // ---- (end of the bug fixes' block)

  // BROWSER (the owner, 2026-10-27): FIZZ in this browser as the unit, heard here; PROJECTS on it: KEEP, LOAD INTO a
  // slot, WRITE (the store before kept in BACKUP), read back; the computer keys on LIBRARY and IMPORT
  await realClick("#unit");
  await waitFor("!document.getElementById('upick').hidden", 3000);
  await realClick("#upick-pop .urow[data-u='browser']");
  const bOn = await waitFor("__mgr.S.mode === 'browser' && !!__mgr.S.link && __mgr.live.on()", 40000);
  check(bOn && (await evaluate("document.getElementById('unit').dataset.u === 'browser' && document.getElementById('fm1').dataset.state === 'found'")),
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
  // ---- bug fixes: rename + refresh after writes: the first LIST after the restore's FZOK lost (read again, not "did not finish")
  await evaluate("(() => { const u = __mgr.bu(); window.__lost = 0; window.__fzok = false; u.hooks.restored = () => { window.__fzok = true; }; u.hooks.reply = (d) => (window.__fzok && d[5] === 0x61 && !window.__lost ? (window.__lost = 1, 'drop') : 0); })()");
  await click('#pj-pending [data-act="write"]');
  const wrote = await waitFor("__mgr.projtab.state.ops.length === 0 && !__mgr.S.busy", 60000);
  const wsay = await evaluate("document.getElementById('say').textContent");
  await evaluate("(() => { const u = __mgr.bu(); u.hooks.reply = null; u.hooks.restored = null; })()");
  check(wrote && (await evaluate("window.__lost")) === 1 && !/did not finish|cut off/i.test(wsay) && (await evaluate("/LIST: .*read again|no answer in \\d+ ms: sent again/.test(document.getElementById('log').textContent)")),
    `PROJECTS: WRITE, the first LIST after FZOK lost: read again, the write not reported failed${wsay ? ` ("${wsay}")` : ""}`);
  await evaluate("__mgr.projtab.state.base = null; __mgr.projtab.show()");
  await waitFor("!!__mgr.projtab.state.base && !__mgr.S.busy", 60000);
  const p3 = await evaluate("__mgr.S.unit.projects.list[2]");
  check(wrote && p3 && p3.there && (await evaluate(`__mgr.S.baks.length === ${nbp0 + 1} && /before projects/.test(__mgr.S.baks[0].name)`)),
    `PROJECTS: WRITE: slot 3 is ${p3 && p3.name} on the browser's FIZZ, read back; the store before kept in BACKUP`);
  check(await evaluate("document.documentElement.scrollHeight <= window.innerHeight + 1"), "PROJECTS fits 1280 x 800");
  await shot("manager-projects.png");
  // ---- bug fixes: rename + refresh after writes: RENAME on the browser's FIZZ, its reply lost; the store read here
  // carries the new name too (KEEP, WRITE)
  {
    await evaluate("(() => { window.__lost = 0; __mgr.bu().hooks.reply = (d) => (d[5] === 0x52 && d[6] === 0x13 && !window.__lost++ ? 'drop' : 0); __mgr.projtab.state.sel = 2; __mgr.projtab.render(); })()");
    await click('#pj-acts [data-act="rename"]');
    await waitFor("!!document.getElementById('pj-rename')", 3000);
    await evaluate("(() => { const i = document.getElementById('pj-rename'); i.value = 'browser one'; i.dispatchEvent(new KeyboardEvent('keydown', { key: 'Enter', bubbles: true })); })()");
    await waitFor("__mgr.S.unit.projects.list[2].name === 'BROWSER ONE' && !__mgr.S.editing && !__mgr.S.writing", 30000);
    await waitFor("!__mgr.S.editing && !__mgr.S.writing && !__mgr.S.busy", 30000);
    await evaluate("__mgr.bu().hooks.reply = null");
    const br = await evaluate(`(async () => { const P = await import('./projects.js'); return { unit: __mgr.S.unit.projects.list[2].name, slot: document.querySelector('#pj-slots .slot[data-p="2"] .sname').textContent,
      model: P.project(__mgr.projtab.state.base.model, 2).name, said: document.getElementById('say').textContent }; })()`);
    check((await evaluate("window.__lost")) === 1 && br.unit === "BROWSER ONE" && br.slot === br.unit && br.model === br.unit && !br.said,
      `PROJECTS (BROWSER): RENAME slot 3, its reply lost: shown, and in the store read here (${JSON.stringify(br)})`);
  }

  // ---- PROJECTS drag copy/move (the owner, 2026-10-29): a pattern dragged onto a pattern copies it, ALT moves it (the
  // ghost's badge follows ALT), a used one asks first; dropped on a project: its first free pattern, the pool sounds
  // carried: after WRITE and the read back FIZZ counts the same steps a sound there as the record locks
  {
    const PJ = "__mgr.projtab.state";
    await evaluate(`(async () => { await __mgr.S.link.project('new', 2); ${PJ}.base = null; await __mgr.refreshRows({ projects: true }); await __mgr.projtab.show(); })()`);
    await waitFor(`!!${PJ}.base && !__mgr.S.busy`, 60000);
    // a pattern with sound locks: the simulated unit's demo A01 edited by its keys, PASTEd at A05 of PROJECT 1 (waits)
    const pasted = await evaluate(`(async () => { const P = await import('./projects.js'), s = __sim; s.press("HOME"); s.panel([], [2], 80); s.panel([], [], 60); s.panel([], [4], 80); s.panel([], [], 60); s.run(2500);
      const sm = P.modelOf(s.flash()), locks = (x) => { const w = P.walkPat(sm.prj.get(x)); return w ? w.soundAt.filter((o) => o < w.sndAt).length : -1; };
      const key = [...sm.prj.keys()].filter((x) => x >>> 24 === P.RK.PAT).sort((x, y) => locks(y) - locks(x))[0];
      if (key === undefined || locks(key) <= 0) return false;
      ${PJ}.clip = P.copyPatterns(sm, (key >>> 16) & 255, [key & 0xFFFF]); ${PJ}.sel = 0; ${PJ}.view = 'patterns'; ${PJ}.pats = new Set([4]); __mgr.projtab.render();
      document.querySelector('#pj-tools [data-act="paste"]').click(); return ${PJ}.ops.length === 1; })()`);
    const recOf = (q, k) => `(async () => { const P = await import('./projects.js'); const m = ${PJ}.cache || ${PJ}.base.model; const b = m.prj.get(P.rkey(P.RK.PAT, ${q}, ${k})); return b ? Array.from(b).join() : ""; })()`;
    const badge = () => evaluate("(() => { const g = document.querySelector('.pjghost'); return g && !g.hidden ? g.querySelector('.badge').textContent : ''; })()");
    const alt = (down) => send("Input.dispatchKeyEvent", { type: down ? "rawKeyDown" : "keyUp", key: "Alt", code: "AltLeft", windowsVirtualKeyCode: 18, modifiers: down ? 1 : 0 });
    // press on a pattern, move onto another (target: a selector), the modifiers held (1: ALT); during(): before letting go
    async function dragOnto(k, target, { modifiers = 0, during = null } = {}) {
      const a = await centre(`#pj-grid .pt[data-k="${k}"]`), t = await centre(target);
      if (!a || !t) return false;
      await mouse("mousePressed", a.x, a.y, { buttons: 1, modifiers });
      for (let i = 1; i <= 6; i++) { await mouse("mouseMoved", a.x + (t.x - a.x) * i / 6, a.y + (t.y - a.y) * i / 6, { buttons: 1, modifiers }); await sleep(20); }
      if (during) await during(t);
      await mouse("mouseReleased", t.x, t.y, { modifiers });
      await sleep(200);
      return true;
    }
    const src = await evaluate(recOf(0, 4));
    // copy onto an empty pattern, the badge flipped with ALT on the way (its key, then the mouse's altKey), let go as COPY
    const flips = [];
    await dragOnto(4, '#pj-grid .pt[data-k="9"]', { during: async (t) => {
      flips.push(await badge()); await alt(true); flips.push(await badge()); await alt(false); flips.push(await badge());
      await mouse("mouseMoved", t.x + 1, t.y, { buttons: 1, modifiers: 1 }); flips.push(await badge());
      await mouse("mouseMoved", t.x, t.y, { buttons: 1 }); flips.push(await badge());
      flips.push(await evaluate(`document.querySelector('#pj-grid .pt[data-k="9"]').classList.contains('drop')`));
    } });
    const c1 = await evaluate(`({ n: ${PJ}.ops.length, last: ${PJ}.ops[${PJ}.ops.length - 1] && ${PJ}.ops[${PJ}.ops.length - 1].kind })`);
    check(pasted && src && c1.n === 2 && c1.last === "drop" && (await evaluate(recOf(0, 9))) === src && (await evaluate(recOf(0, 4))) === src,
      `PROJECTS drag: A05 dropped on A10 copies it (${c1.n} changes waiting)`);
    check(flips.join() === "COPY,MOVE,COPY,MOVE,COPY,true", `PROJECTS drag: the ghost's badge follows ALT (${flips.join()}), the target lit`);
    // ALT held: moved (A10 -> A12, A10 cleared)
    await dragOnto(9, '#pj-grid .pt[data-k="11"]', { modifiers: 1 });
    check((await evaluate(`${PJ}.ops.length`)) === 3 && (await evaluate(recOf(0, 11))) === src && (await evaluate(recOf(0, 9))) === "" &&
      (await evaluate(`!document.querySelector('#pj-grid .pt[data-k="9"]').classList.contains('used')`)), "PROJECTS drag: ALT moves A10 to A12 (A10 empty)");
    // onto a used pattern: the confirm; CANCEL leaves it, REPLACE replaces it (a demo pattern, A02)
    await dragOnto(11, '#pj-grid .pt[data-k="1"]');
    const cf = await evaluate("({ open: !document.getElementById('cf').hidden, t: document.getElementById('cf-title').textContent, w: document.getElementById('cf-what').textContent })");
    await click("#cf-no");
    await sleep(150);
    const kept2 = (await evaluate(`${PJ}.ops.length`)) === 3 && (await evaluate(recOf(0, 1))) === "";
    await dragOnto(11, '#pj-grid .pt[data-k="1"]');
    await click("#cf-go");
    await sleep(200);
    check(cf.open && cf.t === "COPY OVER" && /A12 → PROJECT 1 A02/.test(cf.w) && kept2 && (await evaluate(`${PJ}.ops.length`)) === 4 && (await evaluate(recOf(0, 1))) === src,
      `PROJECTS drag: onto a used pattern asks (${cf.t}: ${cf.w}); CANCEL leaves it, REPLACE replaces it`);
    // onto PROJECT 2's row: its first free pattern, the pool sounds carried
    await dragOnto(4, '#pj-slots .slot[data-p="1"]');
    const d2 = await evaluate(`(() => { const o = ${PJ}.ops[${PJ}.ops.length - 1]; return { n: ${PJ}.ops.length, q: o.q, j: o.j, pool: o.result.poolAdded, need: [...o.clip.pool.values()].filter(Boolean).length }; })()`);
    check(d2.n === 5 && d2.q === 1 && d2.j === 0 && d2.pool > 0 && d2.pool === d2.need, `PROJECTS drag: onto PROJECT 2 lands at its A01, +${d2.pool} pool sounds`);
    await shot("manager-projects-drag.png");
    await click('#pj-pending [data-act="write"]');
    const wrote2 = await waitFor(`${PJ}.ops.length === 0 && !__mgr.S.busy`, 60000);
    await evaluate(`${PJ}.base = null; __mgr.projtab.show()`);
    await waitFor(`!!${PJ}.base && !__mgr.S.busy`, 60000);
    const steps = await evaluate(`(async () => { const P = await import('./projects.js'), L = __mgr.S.link, m = P.modelOf(${PJ}.base.image);
      const per = async (p, k) => { await L.project('load', p + 1); await new Promise((r) => setTimeout(r, 600));
        const pool = (await L.list({ pool: true })).filter((x) => x.source === 'pool'), name = new Map(pool.map((x) => [x.slot, x.name]));
        const b = m.prj.get(P.rkey(P.RK.PAT, p, k)), w = b && P.walkPat(b), locks = {}, users = {};
        if (!w) return null;
        for (const o of w.soundAt) if (o < w.sndAt) { const n = name.get(b[o]) || '?'; locks[n] = (locks[n] || 0) + 1; }
        if (p) for (const x of pool) { const u = await L.users('pool', x.slot); const s = u && u.projects ? u.projects.reduce((a, y) => a + y.steps, 0) : 0; if (s) users[x.name] = (users[x.name] || 0) + s; }
        return { locks, users }; };
      const a = await per(0, 4), b = await per(1, 0); await L.project('load', 1); return { a, b }; })()`);
    const eqm = (x, y) => !!x && !!y && Object.keys({ ...x, ...y }).every((k) => x[k] === y[k]);
    check(wrote2 && steps.a && steps.b && Object.keys(steps.a.locks).length > 0 && eqm(steps.a.locks, steps.b.locks) && eqm(steps.b.locks, steps.b.users),
      `PROJECTS drag: WRITE, read back: PROJECT 2 A01 locks the sounds A05 does, FIZZ counts the same steps a sound there (${JSON.stringify(steps.b && steps.b.users)})`);
    await idle();
    // a phone: a long press lifts it (the ghost), let go over nothing: the TO sheet; MOVE, PROJECT 2, its A03
    await size(390, 844, true);
    await send("Emulation.setTouchEmulationEnabled", { enabled: true, maxTouchPoints: 5 });
    await evaluate(`${PJ}.sel = 0; ${PJ}.view = 'patterns'; __mgr.projtab.render()`);
    await sleep(300);
    const p4 = await centre('#pj-grid .pt[data-k="4"]');
    await send("Input.dispatchTouchEvent", { type: "touchStart", touchPoints: [{ x: p4.x, y: p4.y }] });
    await sleep(550);
    const lifted = await badge();
    await send("Input.dispatchTouchEvent", { type: "touchEnd", touchPoints: [] });
    await sleep(300);
    const sheetOn = await evaluate("!document.getElementById('pj-sheet').hidden");
    await click('#pj-sheet [data-mode="move"]');
    await click('#pj-sheet .shp [data-q="1"]');
    await click('#pj-sheet .shg [data-j="2"]');
    await sleep(200);
    const sm = await evaluate(`(() => { const o = ${PJ}.ops[${PJ}.ops.length - 1]; return o ? { kind: o.kind, q: o.q, j: o.j, move: o.move } : null; })()`);
    check(lifted === "COPY" && sheetOn && sm && sm.kind === "drop" && sm.q === 1 && sm.j === 2 && sm.move && (await evaluate(recOf(0, 4))) === "" && (await noScroll()),
      `PROJECTS at 390: a long press lifts A05 (${lifted || "no ghost"}), let go: the TO sheet; MOVE to PROJECT 2 A03 waits (${JSON.stringify(sm)})`);
    await click('#pj-pending [data-act="undo"]');
    await send("Emulation.setTouchEmulationEnabled", { enabled: false });
    await size(1280, 800);
    await sleep(300);
  }
  const peakJs = `(async (key) => { const p = __mgr.bu().player; if (!window.__an) { window.__an = p.ctx.createAnalyser(); window.__an.fftSize = 2048; p.out.connect(window.__an); }
    const b = new Float32Array(2048), pk = () => { window.__an.getFloatTimeDomainData(b); let m = 0; for (const x of b) m = Math.max(m, Math.abs(x)); return m; };
    document.dispatchEvent(new KeyboardEvent("keydown", { key, bubbles: true })); let m = 0;
    for (let i = 0; i < 15; i++) { await new Promise((r) => setTimeout(r, 40)); m = Math.max(m, pk()); }
    document.dispatchEvent(new KeyboardEvent("keyup", { key, bubbles: true })); return m; })`;
  await click("#t-library");
  // ---- NEON features: LIBRARY (the browser's FIZZ as the unit, nothing picked: the THIS BROWSER card; MY PRESETS orange)
  await evaluate("__mgr.S.sel = null; __mgr.S.pick.keys.clear(); __mgr.S.pick.mode = false; document.querySelector('#l-filter [data-src=all]').click()");
  const tbc = await evaluate("({ card: !!document.querySelector('#l-sel .thisb'), n: (document.querySelector('#l-sel .tbstats .num') || {}).textContent, mine: __mgr.S.unit.rows.filter((r) => r.source === 'mine').length, bmode: document.getElementById('tab-library').classList.contains('bmode'), bsrc: !document.querySelector('#l-filter [data-src=browser]') })");
  await shot("manager-neon-this-browser.png");
  await click("#tb-import");
  const tbi = await evaluate("!document.getElementById('tab-import').hidden");
  await click("#t-library");
  check(tbc.card && +tbc.n === tbc.mine && tbc.bmode && tbc.bsrc && tbi, `LIBRARY in BROWSER: the THIS BROWSER card (${tbc.n} presets), no IN THIS BROWSER source, its IMPORT goes to IMPORT`);
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
  await click("#fm1");
  check(await waitFor("__mgr.S.mode === 'fm1' && !!__mgr.S.link && __mgr.S.unit && __mgr.S.unit.rows.length > 0", 30000), "FM-1 again (⇄ FM-1): the unit back from BROWSER");

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
  // ---- NEON features: EDIT POOL BACKUP
  // EDIT: ALGO's ‹ › (wrapping), the knob row (a drag, the same value elsewhere), the tag chip's popover (SAVE writes
  // it), the phone's save bar; POOL: the moving card (CANCEL, MOVE TO, ESC); BACKUP: share (or a save when the browser
  // cannot share a file), the ••• menu's DELETE
  {                                                    // (a block of its own: its names)
  await size(1280, 800);
  await sleep(300);
  await evaluate("__mgr.live.on() && __mgr.live.setOn(false)");
  await idle();
  await evaluate("__mgr.openInEditor(__mgr.S.unit.rows.find((q) => q.source === 'factory' && q.slot === 3))");
  await waitFor("!document.getElementById('tab-edit').hidden && !__mgr.editor.sound().midi && document.querySelector('#ed-stage .algonav')", 8000);
  await evaluate("__mgr.editor.set(0, 8)");
  await sleep(120);
  await realClick("#ed-stage .algonav [data-nav='next']");
  await sleep(120);
  const an1 = await evaluate("__mgr.editor.value(0)");
  await realClick("#ed-stage .algonav [data-nav='prev']");
  await sleep(120);
  const an2 = await evaluate("__mgr.editor.value(0)");
  await realClick("#ed-stage .algonav [data-nav='prev']");
  await sleep(120);
  const an3 = await evaluate("__mgr.editor.value(0)");
  const alab = await evaluate("[...document.querySelectorAll('#ed-stage .algonav button')].map((b) => b.getAttribute('aria-label')).join('|')");
  check(an1 === 1 && an2 === 8 && an3 === 7 && alab === "Previous algorithm|Next algorithm", `EDIT: ALGO's › from 8 wraps to ${an1}, ‹ back to ${an2}, ‹ again ${an3} (${alab})`);
  await size(1100, 800);                               // (pages: the operators' row, DRIVE on DRIVE · CRUSH, LEVEL on AMP)
  await sleep(400);
  const krow = await evaluate("[...document.querySelectorAll('#ed-stage .pn-ops .t[data-id]')].filter((e) => [4, 5, 6, 7, 73, 46].includes(+e.dataset.id)).map((e) => e.querySelector('small').textContent).join(' ')");
  check(krow === "HARM DTUN FDBK MIX", `EDIT: the operators' row: ${krow} (DRIVE and LEVEL left it)`);
  await evaluate("__mgr.editor.set(73, 20)");
  await sleep(120);
  await drag("#ed-stage .pn-drive .t[data-id='73']", 0, -60);
  await sleep(150);
  const kd = await evaluate(`({ v: __mgr.editor.value(73), t: document.querySelector('#ed-stage .pn-drive .t[data-id="73"] b').textContent })`);
  check(kd.v > 20 && kd.t === String(kd.v), `EDIT: DRIVE dragged up on DRIVE · CRUSH: 20 -> ${kd.v} (${kd.t})`);
  await evaluate("__mgr.editor.set(46, 90)");
  await sleep(120);
  await drag("#ed-stage .pn-amp .t[data-id='46']", 0, 40);
  await sleep(150);
  const kl = await evaluate(`({ v: __mgr.editor.value(46), t: document.querySelector('#ed-stage .pn-amp .t[data-id="46"] b').textContent })`);
  check(kl.v < 90 && kl.t === String(kl.v), `EDIT: LEVEL dragged down on AMP: 90 -> ${kl.v} (${kl.t})`);
  await size(1280, 800);
  await sleep(400);
  // one panel or pages (the owner: "keep one panel but then tabs once they are too small to fit"), every MOD slot on
  // (the tallest case): 1440 x 900 one panel, every value, no scroll; 1280 x 800 (the keys shown, then hidden) one
  // panel or pages as measured, no scroll on any page
  await evaluate("[0, 1, 2, 3, 4, 5, 6, 7].forEach((k) => __mgr.editor.set(86 + 3 * k, 1 + k % 8))");
  const fitAt = async (w, h) => {
    await size(w, h);
    await sleep(700);
    return evaluate(`(async () => { const st = document.getElementById('ed-stage'), mode = __mgr.editor.state.mode, wait = () => new Promise((r) => setTimeout(r, 120));
      const pageOk = () => document.documentElement.scrollHeight <= innerHeight + 1 && document.documentElement.scrollWidth <= innerWidth + 1, stageOk = () => st.scrollHeight <= st.clientHeight + 1;
      const sb = st.getBoundingClientRect(), inside = () => [...st.querySelectorAll('.t, .sg, .gd, .mslot')].filter((e) => e.offsetWidth > 0).every((e) => { const r = e.getBoundingClientRect(); return r.bottom <= sb.bottom + 1 && r.right <= sb.right + 1; });
      if (mode === "panel") return { mode, ok: pageOk() && stageOk() && inside(), fit: st.scrollHeight + '/' + st.clientHeight };
      const out = []; let ok = true;
      for (const b of [...document.querySelectorAll('#ed-pages [data-page]')]) { document.querySelector('#ed-pages [data-page="' + b.dataset.page + '"]').click(); await wait(); const good = pageOk() && stageOk(); ok = ok && good; out.push(b.dataset.page + (good ? '' : ' ' + st.scrollHeight + '/' + st.clientHeight)); }
      document.querySelector('#ed-pages [data-page="ops"]').click();
      return { mode, ok, fit: out.join(', ') }; })()`);
  };
  const f14 = await fitAt(1440, 900);
  check(f14.mode === "panel" && f14.ok, `EDIT at 1440 x 900, the keys shown: one panel, every value, no scroll (columns ${f14.fit})`);
  await shot("manager-neon-edit-fit-1440.png");
  const f12 = await fitAt(1280, 800);
  check(f12.ok, `EDIT at 1280 x 800, the keys shown: ${f12.mode === "panel" ? "one panel" : "pages"}, no scroll (${f12.fit})`);
  await shot("manager-neon-edit-fit-1280.png");
  await click("#ed-kbtg");
  const fk = await fitAt(1280, 800);
  check(fk.ok, `EDIT at 1280 x 800, the keys hidden: ${fk.mode === "panel" ? "one panel" : "pages"}, no scroll (${fk.fit})`);
  await click("#ed-kbtg");
  // DRIVE and LEVEL at 1280: where they show (the knob row, or their own pages too)
  await evaluate("__mgr.editor.set(73, 20); __mgr.editor.set(46, 90)");
  await sleep(150);
  const wheelOn = async (sel) => { const c = await centre(sel); if (!c) return false; await send("Input.dispatchMouseEvent", { type: "mouseWheel", x: c.x, y: c.y, deltaX: 0, deltaY: -100 }); await sleep(150); return true; };
  const dsel = (await evaluate("__mgr.editor.state.mode")) === "panel" ? ["#ed-stage .pn-drive .t[data-id='73']", "#ed-stage .pn-amp .t[data-id='46']"] : ["#ed-stage .pn-drive .t[data-id='73']", "#ed-stage .pn-amp .t[data-id='46']"];
  const dw = await wheelOn(dsel[0]), lw = await wheelOn(dsel[1]);
  check(dw && lw && (await evaluate("__mgr.editor.value(73) === 21 && __mgr.editor.value(46) === 91")), `EDIT at 1280: DRIVE and LEVEL set where they show (${dsel.join(", ")})`);
  await evaluate("[0, 1, 2, 3, 4, 5, 6, 7].forEach((k) => __mgr.editor.set(86 + 3 * k, 0))");
  await size(1280, 800);
  await sleep(400);
  // the tag: one chip beside the name, a popover of FIZZ's seven tags and NONE
  check(await evaluate("!document.querySelector('#ed-head .edtags') && document.querySelectorAll('#ed-head #ed-tag').length === 1"), "EDIT: the tag one chip beside the name (no row of tag buttons)");
  await click("#ed-tag");
  await waitFor("!!document.querySelector('.tagpop')", 3000);
  const tp = await evaluate("[...document.querySelectorAll('.tagpop [data-tag]')].map((b) => b.textContent).join(' ')");
  await click(".tagpop [data-tag='-1']");
  await sleep(100);
  const tg0 = await evaluate("({ t: __mgr.editor.sound().tags, open: !!document.querySelector('.tagpop') })");
  await click("#ed-tag");
  await waitFor("!!document.querySelector('.tagpop')", 3000);
  await shot("manager-neon-edit-tagpop.png");
  await click(".tagpop [data-tag='3']");
  await sleep(100);
  const tg1 = await evaluate("({ t: __mgr.editor.sound().tags, open: !!document.querySelector('.tagpop'), chip: document.getElementById('ed-tag').textContent })");
  check(tp === "BASS KEYS PLUCK PAD LEAD DRUMS FX NONE" && tg0.t === 0 && !tg0.open && tg1.t === 8 && !tg1.open && tg1.chip === "PAD",
    `EDIT: the tag chip's popover (${tp}): NONE -> ${tg0.t}, PAD -> ${tg1.t}, closed after a pick; the chip says ${tg1.chip}`);
  await evaluate("(() => { __mgr.editor.sound().tags = 3; __mgr.editor.renderHead(); })()");
  await click("#ed-tag");
  await waitFor("!!document.querySelector('.tagpop')", 3000);
  await click(".tagpop [data-tag='3']");
  await sleep(100);
  const tg2 = await evaluate("({ t: __mgr.editor.sound().tags, open: !!document.querySelector('.tagpop'), chip: document.getElementById('ed-tag').textContent })");
  await evaluate("document.dispatchEvent(new KeyboardEvent('keydown', { key: 'Escape', bubbles: true }))");
  await sleep(100);
  check(tg2.t === 11 && tg2.open && /^BASS\+2$/.test(tg2.chip) && !(await evaluate("!!document.querySelector('.tagpop')")),
    `EDIT: a sound with two tags (BASS KEYS) keeps both: PAD toggled on (${tg2.t}), the popover stays open; the chip "${tg2.chip}"; ESC closes it`);
  await evaluate("(() => { __mgr.editor.sound().tags = 0; __mgr.editor.renderHead(); })()");
  await click("#ed-tag");
  await waitFor("!!document.querySelector('.tagpop')", 3000);
  await click(".tagpop [data-tag='4']");
  await evaluate("(() => { const i = document.getElementById('ed-name'); i.value = 'TAG TEST'; i.dispatchEvent(new Event('input', { bubbles: true })); })()");
  await click("#ed-savenew");
  await waitFor("__mgr.S.unit.rows.some((r) => r.source === 'mine' && r.name === 'TAG TEST')", 15000);
  await idle();
  const tsv = await evaluate(`(async () => { const { soundFromFrame } = await import('./sound.js'); const r = __mgr.S.unit.rows.find((x) => x.source === 'mine' && x.name === 'TAG TEST');
    return { tags: soundFromFrame(await __mgr.S.link.dump('mine', r.slot)).tags, row: r.tags }; })()`);
  check(tsv.tags === 16, `EDIT: LEAD picked in the popover, SAVE AS NEW: the stored preset's tags ${tsv.tags} (LEAD)`);
  check(await evaluate("getComputedStyle(document.getElementById('ed-savebar')).position !== 'fixed'"), "EDIT at 1280: REVERT / SAVE AS NEW / SAVE stay in the head");
  await shot("manager-neon-edit-1280.png");
  // the phone: the save bar fixed at the bottom, the panel's end clear of it
  await size(390, 844, true);
  await sleep(500);
  const sb = await evaluate(`(() => { window.scrollTo(0, document.documentElement.scrollHeight); const b = document.getElementById('ed-savebar'), r = b.getBoundingClientRect(), k = document.getElementById('ed-keys').getBoundingClientRect();
    return { pos: getComputedStyle(b).position, gap: Math.round(innerHeight - r.bottom), top: Math.round(r.top), keys: Math.round(k.bottom), btns: [...b.querySelectorAll('button')].map((x) => x.id).join() }; })()`);
  check(sb.pos === "fixed" && sb.gap >= 0 && sb.gap <= 40 && sb.keys <= sb.top && sb.btns === "ed-revert,ed-savenew,ed-save" && await noScroll(),
    `EDIT at 390: the save bar fixed ${sb.gap} px from the bottom (${sb.btns}); scrolled to the end the keybed ends at ${sb.keys}, above it (${sb.top})`);
  await shot("manager-neon-edit-390.png");
  await evaluate("window.scrollTo(0, 0)");
  await size(1280, 800);
  await sleep(300);
  // POOL: the moving card
  await click("#t-pool");
  await waitFor("document.querySelectorAll('#p-grid .ps').length === 128");
  await idle();
  const icons = await evaluate(`(async () => { const U = await import('./ui.js'); const rows = new Map(__mgr.S.unit.rows.filter((r) => r.source === 'pool').map((r) => [r.slot, r]));
    return [...document.querySelectorAll('#p-grid .ps.full')].map((e) => { const t = U.tagsOf(rows.get(+e.dataset.k).tags)[0], i = e.querySelector('svg.ti');
      return t ? !!i && i.dataset.tag === t && getComputedStyle(i).stroke !== 'none' : !i && !!e.querySelector('.ring'); }); })()`);
  check(icons.length > 0 && icons.every(Boolean), `POOL: each of the ${icons.length} sounds' tiles shows its tag's icon (an untagged one its ring)`);
  const poolNow = () => evaluate("JSON.stringify(__mgr.S.unit.rows.filter((r) => r.source === 'pool').map((r) => [r.slot, r.id]))");
  const pab = await evaluate(`(() => { const t = [...document.querySelectorAll('#p-grid .ps')], a = t.find((e) => e.classList.contains('full'));
    const b = a && t.find((e) => !e.classList.contains('full') && +e.dataset.k > +a.dataset.k); return a && b ? [+a.dataset.k, +b.dataset.k] : null; })()`);
  const dnd = (a, b) => evaluate(`(() => { const dt = new DataTransfer(), g = (k) => document.querySelector('#p-grid .ps[data-k="' + k + '"]');
    g(${a}).dispatchEvent(new DragEvent('dragstart', { bubbles: true, dataTransfer: dt })); g(${b}).dispatchEvent(new DragEvent('dragover', { bubbles: true, cancelable: true, dataTransfer: dt }));
    g(${b}).dispatchEvent(new DragEvent('drop', { bubbles: true, cancelable: true, dataTransfer: dt })); g(${a}).dispatchEvent(new DragEvent('dragend', { bubbles: true, dataTransfer: dt })); })()`);
  if (pab) {
    const [pa, pb] = pab, p0 = await poolNow(), ida = await evaluate(`__mgr.S.unit.rows.find((r) => r.source === 'pool' && r.slot === ${pa}).id`);
    await dnd(pa, pb);
    await waitFor("!document.getElementById('p-move').hidden", 3000);
    const mc = await evaluate(`({ line: document.querySelector('#p-move .mvline').textContent, go: document.getElementById('p-mv-go').textContent, undo: document.querySelector('#p-move .mvundo').textContent,
      lift: document.querySelector('#p-grid .ps[data-k="${pa}"]').classList.contains('moving'), dest: document.querySelector('#p-grid .ps[data-k="${pb}"]').classList.contains('mvto'), bar: getComputedStyle(document.querySelector('#tab-pool .pbar')).display })`);
    check(mc.line === `Moving ${pa} → ${pb}. Its locks follow.` && mc.go === `MOVE TO ${pb}` && mc.undo === "UNDO ON THE FM-1 CLEARS" && mc.lift && mc.dest && mc.bar === "none",
      `POOL: a tile dropped on place ${pb}: the moving card ("${mc.line}", ${mc.go}, ${mc.undo}), the tile lifted, the place marked`);
    await shot("manager-neon-pool-moving-1280.png");
    await click("#p-mv-no");
    await sleep(200);
    check(await evaluate("document.getElementById('p-move').hidden && !document.querySelector('#p-grid .ps.moving, #p-grid .ps.mvto')") && p0 === await poolNow(), "POOL: CANCEL: the card gone, the pool as it was");
    await dnd(pa, pb);
    await waitFor("!document.getElementById('p-move').hidden", 3000);
    await evaluate("document.dispatchEvent(new KeyboardEvent('keydown', { key: 'Escape', bubbles: true }))");
    await sleep(200);
    check(await evaluate("document.getElementById('p-move').hidden") && p0 === await poolNow(), "POOL: ESC cancels the move too");
    await dnd(pa, pb);
    await waitFor("!document.getElementById('p-move').hidden", 3000);
    await click("#p-mv-go");
    await waitFor(`__mgr.S.unit.rows.some((r) => r.source === 'pool' && r.slot === ${pb} && r.id === ${ida})`, 20000);
    await idle();
    check(await evaluate(`__mgr.S.unit.rows.some((r) => r.source === 'pool' && r.slot === ${pb} && r.id === ${ida}) && !__mgr.S.unit.rows.some((r) => r.source === 'pool' && r.slot === ${pa}) && document.getElementById('p-move').hidden`),
      `POOL: MOVE TO ${pb}: the sound moved from ${pa} (SWAP on the unit)`);
    await size(390, 844, true);
    await sleep(500);
    await dnd(pb, pa);
    await waitFor("!document.getElementById('p-move').hidden", 3000);
    await sleep(600);                                  // (its slide up done)
    const mp = await evaluate("(() => { const r = document.getElementById('p-move').getBoundingClientRect(); return { pos: getComputedStyle(document.getElementById('p-move')).position, bottom: Math.round(innerHeight - r.bottom), w: Math.round(r.width) }; })()");
    check(mp.pos === "fixed" && mp.bottom >= 0 && mp.bottom < 40 && await noScroll(), `POOL at 390: the moving card fixed at the bottom (${mp.bottom} px up, ${mp.w} px wide)`);
    await shot("manager-neon-pool-moving-390.png");
    await click("#p-mv-no");
    await size(1280, 800);
    await sleep(300);
  } else check(false, "POOL: a sound and a free place after it to move it to");
  // BACKUP: share (stubbed), the save when the browser cannot share files, ••• -> DELETE
  await click("#t-backup");
  await evaluate(`(() => { window.__shared = []; window.__saved = [];
    Object.defineProperty(navigator, 'canShare', { configurable: true, writable: true, value: (d) => !!(d && d.files && d.files.length) });
    Object.defineProperty(navigator, 'share', { configurable: true, writable: true, value: async (d) => { window.__shared.push(d); } });
    window.showSaveFilePicker = async (opt) => ({ name: opt.suggestedName, createWritable: async () => { const parts = [];
      return { write: async (x) => parts.push(x), close: async () => window.__saved.push({ name: opt.suggestedName, bytes: parts.reduce((a, p) => a + p.length, 0) }) }; } }); })()`);
  const nbs = await evaluate("__mgr.S.baks.length");
  check(nbs > 0 && await evaluate("document.querySelectorAll('#baks .bk').length === document.querySelectorAll('#baks .bk [data-act=share]').length && !document.querySelector('#baks .bk [data-act=del], #baks .bk [data-act=save]')"),
    `BACKUP: each of the ${nbs} rows has its share button; save and delete only in the ••• menu`);
  await click("#baks .bk [data-act='share']");
  await waitFor("window.__shared.length === 1", 3000);
  const sh = await evaluate("(() => { const f = window.__shared[0] && window.__shared[0].files && window.__shared[0].files[0], b = __mgr.S.baks[0]; return f ? { file: f instanceof File, name: f.name, type: f.type, size: f.size, want: b.bytes.length, nm: b.name, saved: window.__saved.length } : null; })()");
  check(!!sh && sh.file && sh.name === sh.nm && sh.type === "application/octet-stream" && sh.size === sh.want && sh.saved === 0, `BACKUP: share: navigator.share got ${sh && sh.name} (${sh && sh.size} B) as a File`);
  await evaluate("navigator.canShare = () => false");
  await click("#baks .bk [data-act='share']");
  await waitFor("window.__saved.length === 1", 3000);
  check(await evaluate("window.__saved.length === 1 && window.__shared.length === 1 && /\\.fizzbak$/.test(window.__saved[0].name)"), "BACKUP: share where the browser cannot share a file: saved to a file instead");
  await evaluate("(() => { navigator.canShare = () => true; navigator.share = async () => { throw new DOMException('cancelled', 'AbortError'); }; window.__toast0 = document.getElementById('toast').hidden ? '' : document.getElementById('toast').textContent; })()");
  const e0 = errors.length;
  await click("#baks .bk [data-act='share']");
  await sleep(400);
  check(await evaluate("window.__saved.length === 1") && errors.length === e0, "BACKUP: share cancelled (AbortError): nothing else happens");
  await click("#baks .bk [data-act='more']");
  await waitFor("!!document.getElementById('bk-menu')", 3000);
  const menu = await evaluate("[...document.querySelectorAll('#bk-menu [data-act]')].map((b) => b.textContent.trim()).join('|')");
  await shot("manager-neon-backup-menu-1280.png");
  await click("#bk-menu [data-act='del']");
  await waitFor("!document.getElementById('cf').hidden");
  const dcf = await evaluate("document.getElementById('cf-title').textContent");
  await click("#cf-go");
  await waitFor(`__mgr.S.baks.length === ${nbs - 1}`, 5000);
  check(menu === "SAVE TO FILE|DELETE" && dcf === "DELETE BACKUP" && (await evaluate("__mgr.S.baks.length")) === nbs - 1 && !(await evaluate("!!document.getElementById('bk-menu')")),
    `BACKUP: ••• (${menu}) -> DELETE asks (${dcf}), then the row is gone`);
  await size(390, 844, true);
  await sleep(400);
  if (await evaluate("__mgr.S.baks.length > 0")) {
    await click("#baks .bk [data-act='more']");
    await waitFor("!!document.getElementById('bk-menu')", 3000);
    const mr = await evaluate("(() => { const r = document.getElementById('bk-menu').getBoundingClientRect(); return r.left >= 0 && r.right <= innerWidth && r.top >= 0 && r.bottom <= innerHeight; })()");
    check(mr && await noScroll(), "BACKUP at 390: the ••• menu inside the window");
    await shot("manager-neon-backup-menu-390.png");
    await evaluate("document.dispatchEvent(new KeyboardEvent('keydown', { key: 'Escape', bubbles: true }))");
  }
  await size(1280, 800);
  // the phone: pages, each one reachable by its tab; DRIVE and LEVEL on theirs
  await size(390, 844, true);
  await click("#t-edit");
  await waitFor("!document.getElementById('tab-edit').hidden");
  await sleep(500);
  const ph = await evaluate(`(async () => { const out = []; for (const b of [...document.querySelectorAll('#ed-pages [data-page]')]) { document.querySelector('#ed-pages [data-page="' + b.dataset.page + '"]').click(); await new Promise((r) => setTimeout(r, 120));
    const k = b.dataset.page, t = document.querySelector('#ed-pages [data-page="' + k + '"]'), cur = [...document.querySelectorAll('#ed-stage .pn.cur')];
    out.push(cur.length === 1 && cur[0].dataset.sec === k && cur[0].offsetHeight > 60 && t.getAttribute('aria-selected') === 'true' ? k : '!' + k + ':' + cur.length + ':' + (cur[0] && cur[0].offsetHeight) + ':' + t.getAttribute('aria-selected')); }
    return { mode: __mgr.editor.state.mode, pages: out.join(','), wide: document.documentElement.scrollWidth <= innerWidth + 1 }; })()`);
  check(ph.mode === "tabs" && ph.pages === "ops,openv,play,filter,amp,drive,lfo,mod" && ph.wide, `EDIT at 390: pages, each reachable by its tab (${ph.pages})`);
  await evaluate("__mgr.editor.set(73, 40); __mgr.editor.set(46, 60)");
  await sleep(150);
  const pd = await wheelOn("#ed-stage .pn-drive .t[data-id='73']"), pl = await wheelOn("#ed-stage .pn-amp .t[data-id='46']");
  check(pd && pl && (await evaluate("__mgr.editor.value(73) === 41 && __mgr.editor.value(46) === 61")), "EDIT at 390: DRIVE set on DRIVE · CRUSH's page, LEVEL on AMP's");
  await shot("manager-neon-edit-390-amp.png");
  // the keys hidden and a page shown: both remembered after a reload
  await click("#ed-kbtg");
  await click("#ed-pages [data-page='lfo']");
  const kh = await evaluate("({ hid: document.getElementById('ed-keys').hidden, p: document.getElementById('ed-kbtg').getAttribute('aria-pressed') })");
  await send("Page.navigate", { url: URL0 });
  await waitFor("window.__mgr && __mgr.S.unit && __mgr.S.unit.rows.length > 0", 30000);
  await idle();
  await click("#t-edit");
  await waitFor("!document.getElementById('tab-edit').hidden && !!document.getElementById('ed-kbtg')", 8000);
  await sleep(400);
  const kr = await evaluate("({ hid: document.getElementById('ed-keys').hidden, p: document.getElementById('ed-kbtg').getAttribute('aria-pressed'), page: __mgr.editor.state.page, cur: (document.querySelector('#ed-stage .pn.cur') || { dataset: {} }).dataset.sec, tab: (document.querySelector('#ed-pages .on') || { dataset: {} }).dataset.page })");
  check(kh.hid && kh.p === "false" && kr.hid && kr.p === "false" && kr.page === "lfo" && kr.cur === "lfo" && kr.tab === "lfo",
    `EDIT at 390: the keys hidden by their toggle and the LFO page, both still so after a reload (${JSON.stringify(kr)})`);
  await click("#ed-kbtg");
  check(await evaluate("!document.getElementById('ed-keys').hidden"), "EDIT: the toggle shows the keys again");
  await click("#ed-pages [data-page='ops']");
  await size(1280, 800);
  }

  // ---- NEON features: LIBRARY (search, SELECT and its bulk actions, IN THIS BROWSER, USED BY strips, MINE ▾, the keys' strip)
  {
  await size(1280, 800);
  await click("#t-library");
  await idle();
  await click("#l-filter [data-src='all']");
  const nShown = () => evaluate("document.querySelectorAll('#l-list .cas').length");
  const keyEv = async (key, type = "keyDown") => send("Input.dispatchKeyEvent", { type, key, text: type === "keyDown" && key.length === 1 ? key : undefined,
    code: key === "/" ? "Slash" : key.length === 1 ? `Key${key.toUpperCase()}` : key, windowsVirtualKeyCode: key === "Escape" ? 27 : key === "/" ? 191 : key.toUpperCase().charCodeAt(0) });
  const n0 = await nShown();
  await evaluate("document.activeElement && document.activeElement.blur && document.activeElement.blur()");
  await keyEv("/"); await keyEv("/", "keyUp");
  const qFocus = await evaluate("document.activeElement && document.activeElement.id === 'l-q'");
  await keyEv("s");
  const heldTyping = await evaluate("__mgr.editor.state.held.size");
  await keyEv("s", "keyUp");
  await keyEv("u"); await keyEv("u", "keyUp"); await keyEv("b"); await keyEv("b", "keyUp");
  await sleep(200);
  const qs = await evaluate(`({ v: document.getElementById('l-q').value, names: [...document.querySelectorAll('#l-list .cas .nm')].map((e) => e.textContent),
    all: +(document.querySelector('#l-filter [data-src=all] .fn') || {}).textContent })`);
  check(qFocus && heldTyping === 0 && qs.v === "sub" && qs.names.length > 0 && qs.names.length < n0 && qs.names.every((x) => /sub/i.test(x)) && qs.all === qs.names.length,
    `LIBRARY: "/" focuses the search; typing "${qs.v}" plays no note (held ${heldTyping}); ${qs.names.length} of ${n0} shown (${qs.names.join(", ")}), the count beside ALL ${qs.all}`);
  await shot("manager-neon-search.png");
  await keyEv("Escape"); await keyEv("Escape", "keyUp");
  await sleep(150);
  check((await evaluate("document.getElementById('l-q').value")) === "" && (await nShown()) === n0, "LIBRARY: Escape clears the search: all shown again");
  // the keys' strip: on LIBRARY, a held key's cap lit; hidden on BACKUP
  await evaluate("document.activeElement && document.activeElement.blur && document.activeElement.blur()");
  await click("#l-list .cas[data-src='factory'][data-slot='2']");
  await keyEv("g");
  const kOn = await evaluate("({ shown: !document.getElementById('keys').hidden, lit: !!document.querySelector('#keys .key[data-k=g].on'), text: document.getElementById('k-what').textContent, oct: document.getElementById('k-oct').textContent })");
  await keyEv("g", "keyUp");
  const kOff = await evaluate("!document.querySelector('#keys .key.on')");
  await click("#t-backup");
  const kHid = await evaluate("document.getElementById('keys').hidden");
  await click("#t-library");
  check(kOn.shown && kOn.lit && kOff && kHid && /play/i.test(kOn.text) && /OCTAVE · [A-G]#?-?\d/.test(kOn.oct), `the keys' strip: G lit while held ("${kOn.text}", ${kOn.oct}); hidden on BACKUP`);
  // USED BY: a MY preset on track 2 of the open pattern, read from the store PROJECTS would show (here: the sim's flash)
  await evaluate(`(async () => { const { storeSound } = await import('../js/fizz_sysex.js'); const b = new Uint8Array(96); b[0] = 0x51; b[1] = 5; b[2] = 3;
    window.__sim.engine.midiIn(storeSound({ name: 'USED ONE', blob: b, tags: 4, track: 1 })); })()`);
  await sleep(2600);
  await click("#refresh");
  await waitFor("__mgr.S.unit.rows.some((r) => r.source === 'mine' && r.name === 'USED ONE') && !__mgr.S.refreshing");
  await idle();
  await evaluate(`(async () => { const P = await import('./projects.js'); const img = window.__sim.engine.flash().bytes.slice();
    __mgr.projtab.state.base = { model: P.modelOf(img), image: img, written: null }; })()`);
  const usedSlot = await evaluate("__mgr.S.unit.rows.find((r) => r.source === 'mine' && r.name === 'USED ONE').slot");
  await click(MINE(usedSlot));
  await waitFor("document.querySelector('#l-users .ub')", 5000);
  const ub = await evaluate("({ rows: [...document.querySelectorAll('#l-users .ub')].map((e) => e.querySelector('.ubt').textContent), cells: document.querySelectorAll('#l-users .ub:first-child .steps i').length, lit: document.querySelectorAll('#l-users .ub:first-child .steps i.p').length })");
  check(ub.rows.length >= 1 && /track 2/.test(ub.rows[0]) && ub.cells === 16 && ub.lit > 0, `USED BY: ${ub.rows.join("; ")}: 16 steps, ${ub.lit} lit`);
  await shot("manager-neon-usedby.png");
  // SELECT: shift a range, cmd / ctrl one off; EXPORT the two, TAG, + POOL, − POOL
  const mclick = (sel, o) => evaluate(`document.querySelector(${JSON.stringify(sel)}).dispatchEvent(new MouseEvent('click', { bubbles: true, ...${JSON.stringify(o)} }))`);
  await click(MINE(1));
  await mclick(MINE(3), { shiftKey: true });
  const r3 = await evaluate("[...__mgr.S.pick.keys].sort().join()");
  await mclick(MINE(2), { metaKey: true });
  const r2 = await evaluate("({ k: [...__mgr.S.pick.keys].sort().join(), n: document.querySelector('#l-bulk .bkn').textContent, bulk: !document.getElementById('l-bulk').hidden, panel: getComputedStyle(document.getElementById('l-sel')).display, chk: document.querySelectorAll('#l-list .cas .chk.on').length })");
  check(r3 === "mine:1,mine:2,mine:3" && r2.k === "mine:1,mine:3" && r2.n === "2" && r2.bulk && r2.panel === "none" && r2.chk === 2,
    `SELECT: shift-click a range (${r3}), cmd-click one off (${r2.k}): the card shows ${r2.n} in the panel's place`);
  await shot("manager-neon-select.png");
  await evaluate("(() => { const ex = __mgr.exporter; window.__exOpen = ex.open; ex.open = (s, o) => { window.__ex = { n: s.length, names: s.map((x) => x.name), title: o.title }; }; })()");
  await click("#lb-export");
  await waitFor("!!window.__ex", 5000);
  const ex2 = await evaluate("window.__ex");
  await evaluate("__mgr.exporter.open = window.__exOpen");
  check(ex2 && ex2.n === 2, `SELECT: EXPORT the two: ${ex2 && ex2.names.join(", ")} ("${ex2 && ex2.title}")`);
  const ids2 = await evaluate("[1, 3].map((s) => __mgr.S.unit.rows.find((r) => r.source === 'mine' && r.slot === s).id)");
  await click("#lb-tag");
  await click("#l-bulk [data-btag='4']");
  await waitFor(`[1, 3].every((s) => __mgr.S.unit.rows.find((r) => r.source === 'mine' && r.slot === s).tags === 16) && !__mgr.S.editing`, 15000);
  await idle();
  check(await evaluate("[1, 3].every((s) => __mgr.S.unit.rows.find((r) => r.source === 'mine' && r.slot === s).tags === 16)"), "SELECT: TAG LEAD on both (the unit's lists read back)");
  const inPoolIds = (ids) => evaluate(`${JSON.stringify(ids)}.filter((id) => __mgr.S.unit.rows.some((r) => r.source === 'pool' && r.id === id)).length`);
  const before = await inPoolIds(ids2);
  if (before) { await click("#lb-prem"); await idle(); if (!(await evaluate("document.getElementById('cf').hidden"))) { await click("#cf-go"); } await waitFor(`${JSON.stringify(ids2)}.every((id) => !__mgr.S.unit.rows.some((r) => r.source === 'pool' && r.id === id))`, 15000); await idle(); }
  await click("#lb-padd");
  await waitFor(`${JSON.stringify(ids2)}.every((id) => __mgr.S.unit.rows.some((r) => r.source === 'pool' && r.id === id))`, 15000);
  await idle();
  const padd = await inPoolIds(ids2);
  const badges = await evaluate(`document.querySelectorAll("${MINE(1)} .pbadge, ${MINE(3)} .pbadge").length`);
  await click("#lb-prem");
  await waitFor(`${JSON.stringify(ids2)}.every((id) => !__mgr.S.unit.rows.some((r) => r.source === 'pool' && r.id === id))`, 15000);
  await idle();
  check(padd === 2 && (await inPoolIds(ids2)) === 0, `SELECT: + POOL put both on the pool (${padd}), − POOL took them out again`);
  void badges;
  // DELETE: asked once, the count named, the one a project uses named
  await click("#l-select");
  await idle();
  const delSlots = await evaluate("[__mgr.S.unit.rows.find((r) => r.source === 'mine' && r.name === 'USED ONE').slot, __mgr.S.unit.rows.filter((r) => r.source === 'mine' && r.name !== 'USED ONE').pop().slot]");
  await evaluate("__mgr.S.sel = null");               // (nf-proj2: a preset selected counts as picked by the first CTRL-click; none here)
  for (const sl of delSlots) await mclick(MINE(sl), { ctrlKey: true });
  const nMine0 = await evaluate("__mgr.S.unit.rows.filter((r) => r.source === 'mine').length");
  if (await evaluate("!document.getElementById('cf').hidden")) await click("#cf-no");
  const delState = await evaluate("({ keys: [...__mgr.S.pick.keys].join(), dis: (document.getElementById('lb-del') || {}).disabled, busy: __mgr.S.busy, say: document.getElementById('say').textContent })");
  await click("#lb-del");
  await waitFor("!document.getElementById('cf').hidden && /^DELETE/.test(document.getElementById('cf-title').textContent)", 30000);
  const cfb = await evaluate("({ t: document.getElementById('cf-title').textContent, s: document.getElementById('cf-sub').textContent })");
  await shot("manager-neon-delete.png");
  await click("#cf-go");
  await waitFor(`__mgr.S.unit.rows.filter((r) => r.source === 'mine').length === ${nMine0 - 2}`, 20000);
  await idle();
  check(cfb.t === "DELETE 2 PRESETS" && /1 used by projects: USED ONE/.test(cfb.s) && (await evaluate(`__mgr.S.unit.rows.filter((r) => r.source === 'mine').length === ${nMine0 - 2}`)),
    `SELECT: DELETE asks once ("${cfb.t}": "${cfb.s}"), then both gone${cfb.t === "DELETE 2 PRESETS" ? "" : ` (${JSON.stringify(delState)}, ${delSlots})`}`);
  // IN THIS BROWSER: KEEP IN BROWSER (the panel's), shown there, kept over a reload; SEND TO FM-1 from it
  await evaluate("if (!document.getElementById('l-bulk').hidden) document.getElementById('l-select').click()");
  await click("#l-list .cas[data-src='factory'][data-slot='3']");   // (a factory sound: not in the browser's MY PRESETS yet)
  const keepName = await evaluate("__mgr.S.unit.rows.find((r) => r.source === 'factory' && r.slot === 3).name");
  const nb0 = await evaluate("__mgr.S.brow.rows.length");
  await waitFor("!!document.getElementById('l-keep') && !document.getElementById('l-keep').disabled", 5000);
  await click("#l-keep");
  await waitFor(`__mgr.S.brow.rows.length === ${nb0 + 1}`, 10000);
  await click("#l-filter [data-src='browser']");
  const shownB = await evaluate(`[...document.querySelectorAll('#l-list .cas[data-src=browser] .nm')].map((e) => e.textContent)`);
  await shot("manager-neon-browser.png");
  check(shownB.includes(keepName) && shownB.length === nb0 + 1 && (await evaluate("!__mgr.bu()")), `IN THIS BROWSER: ${keepName} kept there (${shownB.length}); the idle browser FIZZ let go, so BROWSER boots the new flash`);
  await send("Page.navigate", { url: URL0.replace(/v=\d+/, `v=${Date.now()}`) + "#library" });
  await waitFor("window.__mgr && __mgr.S.unit && __mgr.S.unit.status && __mgr.S.unit.rows.length > 0", 30000);
  await waitFor(`__mgr.S.brow.rows.length === ${nb0 + 1}`, 10000);
  check(await evaluate(`__mgr.S.brow.rows.some((r) => r.name === ${JSON.stringify(keepName)})`), `IN THIS BROWSER: ${keepName} still there after a reload (${nb0 + 1})`);
  await click("#t-library");
  await idle();
  await click("#l-filter [data-src='browser']");
  const bslot = await evaluate(`__mgr.S.brow.rows.find((r) => r.name === ${JSON.stringify(keepName)}).slot`);
  await click(`#l-list .cas[data-src='browser'][data-slot='${bslot}']`);
  const nMine1 = await evaluate("__mgr.S.unit.rows.filter((r) => r.source === 'mine').length");
  await waitFor("!!document.getElementById('l-send') && !document.getElementById('l-send').disabled", 10000);
  await click("#l-send");
  await waitFor(`__mgr.S.unit.rows.filter((r) => r.source === 'mine').length === ${nMine1 + 1}`, 15000);
  await idle();
  const sent = await evaluate(`({ src: __mgr.S.filter.src, fresh: document.querySelectorAll('#l-list .cas.new').length, toast: document.getElementById('toast').textContent })`);
  check(sent.src === "mine" && sent.fresh === 1 && /1 SENT TO FM-1 · MY PRESETS \d+/.test(sent.toast), `IN THIS BROWSER: SEND TO FM-1: in MY PRESETS (${sent.toast}), the view there, the new one lit`);
  await shot("manager-neon-sent.png");
  // a phone: the source as MINE ▾ beside the search
  await size(390, 844, true);
  await sleep(400);
  await evaluate("window.scrollTo(0, 0)");
  const ph = await evaluate("({ btn: getComputedStyle(document.getElementById('l-src')).display !== 'none', seg: getComputedStyle(document.querySelector('#l-filter .srcseg')).display, label: document.getElementById('l-src').textContent })");
  await click("#l-src");
  const menu = await evaluate("({ open: !document.getElementById('l-srcpop').hidden, items: [...document.querySelectorAll('#l-srcpop [data-s]')].map((b) => b.textContent) })");
  await shot("manager-neon-phone-menu.png");
  await click("#l-srcpop [data-s='factory']");
  const after = await evaluate("({ src: __mgr.S.filter.src, label: document.getElementById('l-src').textContent, closed: document.getElementById('l-srcpop').hidden, mine: document.querySelectorAll('#l-list .cas[data-src=mine]').length })");
  check(ph.btn && ph.seg === "none" && ph.label === "MINE ▾" && menu.open && menu.items.join("|") === "MY PRESETS|FACTORY|IN THIS BROWSER|ALL" && after.src === "factory" && after.label === "FACTORY ▾" && after.closed && !after.mine && (await noScroll()),
    `MINE ▾ at 390: ${menu.items.join(", ")}; FACTORY picked: "${after.label}", no horizontal scroll`);
  await size(1280, 800);
  await click("#l-filter [data-src='all']");
  }

  // ---- offline mode: picker, start, FM-1 found
  {
  // no Web MIDI allowed yet (?sim=nomidi): THIS BROWSER is the unit at start, quiet (no sound before a click), the
  // FM-1 a dashed circle; Web MIDI is not asked for at start
  await size(1280, 800);
  await send("Page.navigate", { url: URL0.replace("?sim&", "?sim=nomidi&") + "#library" });
  const off = await waitFor("window.__mgr && __mgr.S.mode === 'browser' && !!__mgr.S.link && __mgr.S.unit && __mgr.S.unit.rows.length > 0", 40000);
  await sleep(600);
  const o1 = await evaluate(`({ quiet: __mgr.bu().quiet, u: document.getElementById('unit').dataset.u, txt: document.getElementById('unit').innerText.replace(/\\s+/g, ' '),
    spk: document.getElementById('spk').dataset.state, fm1: document.getElementById('fm1').dataset.state, midi: !!__mgr.FM.access || !!window.__sim,
    listed: __mgr.S.unit.rows.filter((r) => r.source === 'factory' || r.source === 'mine').slice(0, 3).every((r) => document.getElementById('tab-library').innerText.includes(r.name)) })`);
  check(off && o1.quiet && o1.u === "browser" && /THIS BROWSER/.test(o1.txt) && o1.spk === "off" && o1.fm1 === "absent" && o1.listed && !o1.midi,
    `offline: no Web MIDI: THIS BROWSER the unit at start (${o1.txt}), quiet, LIBRARY lists its presets, the FM-1 a dashed circle, MIDI not asked`);
  await shot("manager-offline-start.png");
  // the sound: the first click anywhere starts it (FIZZ moved into the audio worklet); the pill then mutes
  await realClick("#t-library");
  const woke = await waitFor("!__mgr.bu().quiet && __mgr.bu().running && document.getElementById('spk').dataset.state === 'on'", 20000);
  const answers = await evaluate("__mgr.S.link.status().then((s) => !!s && s.tracks === 6).catch(() => false)");
  check(woke && answers, "offline: the first click starts the sound (the pill orange), the browser's FIZZ still answers");
  await sleep(800);
  await realClick("#spk");
  const muted = await waitFor("document.getElementById('spk').dataset.state === 'mute' && __mgr.bu().player.volume === 0", 3000);
  await realClick("#spk");
  const unmuted = await waitFor("document.getElementById('spk').dataset.state === 'on' && __mgr.bu().player.volume === 1", 3000);
  check(muted && unmuted, "offline: the speaker pill mutes, then sounds again");
  // the picker: THIS BROWSER (checked), the FM-1 not found (– – –), CONNECT FM-1; Escape and a click outside close it
  await realClick("#unit");
  const pk = await waitFor("!document.getElementById('upick').hidden && document.querySelectorAll('#upick-pop .urow').length === 2", 3000);
  const p1 = await evaluate(`({ rows: [...document.querySelectorAll('#upick-pop .urow')].map((r) => r.innerText.replace(/\\s+/g, ' ').trim()),
    conn: !!document.getElementById('up-connect'), on: !!document.querySelector("#upick-pop .urow[data-u='browser'] .radio.y") })`);
  await shot("manager-offline-picker.png");
  await evaluate("document.dispatchEvent(new KeyboardEvent('keydown', { key: 'Escape', bubbles: true }))");
  const esc1 = await waitFor("document.getElementById('upick').hidden", 2000);
  await click("#unit");
  await click("#upick-scrim");
  const out1 = await waitFor("document.getElementById('upick').hidden", 2000);
  check(pk && p1.conn && p1.on && /THIS BROWSER/.test(p1.rows[0]) && /FM-1.*– – –/.test(p1.rows[1]) && esc1 && out1,
    `offline: the picker: ${p1.rows.join(" | ")}, CONNECT FM-1; Escape and a click outside close it`);
  // the phone: a bottom sheet
  await size(390, 844, true);
  await sleep(300);
  await click("#unit");
  await sleep(400);
  const sh = await evaluate("({ sheet: document.getElementById('upick').classList.contains('sheet'), b: Math.round(document.getElementById('upick-pop').getBoundingClientRect().bottom), h: innerHeight })");
  check(sh.sheet && Math.abs(sh.b - sh.h) <= 1 && (await noScroll()), "offline: on a phone the picker is a bottom sheet, no horizontal scroll");
  await shot("manager-offline-picker-phone.png");
  await click("#upick-scrim");
  await size(1280, 800);
  await sleep(300);
  // CONNECT FM-1 asks for Web MIDI (here the simulated FM-1) and connects; the picker's THIS BROWSER goes back
  await click("#unit");
  await click("#up-connect");
  const fmOn = await waitFor("__mgr.S.mode === 'fm1' && !!__mgr.S.link && __mgr.S.unit && __mgr.S.unit.id !== 'FM-1_46999999' && __mgr.S.unit.rows.length > 0", 40000);
  const f1 = await evaluate("({ u: document.getElementById('unit').dataset.u, spk: document.getElementById('spk').hidden, fm1: document.getElementById('fm1').hidden })");
  check(fmOn && f1.u === "fm1" && f1.spk && f1.fm1, "offline: CONNECT FM-1 connects the (simulated) FM-1: the pill green, no sound pill, no circle");
  await click("#unit");
  await click("#upick-pop .urow[data-u='browser']");
  const back = await waitFor("__mgr.S.mode === 'browser' && !!__mgr.S.link && __mgr.live.on() && document.getElementById('fm1').dataset.state === 'found'", 40000);
  check(back, "offline: the picker's THIS BROWSER: back on the browser's FIZZ, the FM-1 left on offer (⇄ FM-1)");
  // RESET (THIS BROWSER's •••, MEMORY): asked once; its MY PRESETS kept in BACKUP first; then a fresh store
  await evaluate(`(async () => { const { storeSound } = await import("../js/fizz_sysex.js"); const b = new Uint8Array(96); b[0] = 0x51; b[1] = 5; b[2] = 6;
    __mgr.bu().engine.midiIn(storeSound({ name: "KEEP ME", blob: b, tags: 1 })); await new Promise((r) => setTimeout(r, 800)); await __mgr.refreshRows(); })()`);
  const nb0 = await evaluate("__mgr.S.baks.length"), mine0 = await evaluate("__mgr.S.unit.rows.filter((r) => r.source === 'mine').length");
  await click("#unit");
  await click("#up-more");
  const drawer = await waitFor("!!document.getElementById('up-mem') && /\\d+%/.test(document.getElementById('up-mem').innerText) && document.querySelectorAll('#up-mem .cells i').length === 16", 2000);
  await shot("manager-offline-memory.png");
  await click("#up-reset");
  const asked = await waitFor("!document.getElementById('cf').hidden && /RESET/.test(document.getElementById('cf-title').textContent)", 3000);
  const sub = await evaluate("document.getElementById('cf-sub').textContent");
  await click("#cf-go");
  const fresh = await waitFor("__mgr.S.mode === 'browser' && !!__mgr.S.link && __mgr.S.unit && __mgr.S.unit.rows.length > 0 && __mgr.S.unit.rows.filter((r) => r.source === 'mine').length === 0", 40000);
  const kept = await evaluate(`__mgr.S.baks.length === ${nb0 + (mine0 ? 1 : 0)} && /before reset/.test(__mgr.S.baks[0].name) && __mgr.S.baks[0].presets === ${mine0}`);
  check(mine0 > 0 && drawer && asked && fresh && kept, `offline: RESET asks once ("${sub}"), ${mine0} MY PRESETS kept in BACKUP first, then a fresh store`);
  // Web MIDI allowed, no FM-1 yet (?sim=away): THIS BROWSER, watching; the FM-1 plugged in: the circle becomes ⇄ FM-1
  // (never taken by itself, refused while a write runs), one click switches; unplugged: back on THIS BROWSER
  await send("Page.navigate", { url: URL0.replace("?sim&", "?sim=away&") + "#library" });
  const aw = await waitFor("window.__mgr && __mgr.S.mode === 'browser' && !!__mgr.S.link && !!__mgr.FM.access && typeof window.__simPlug === 'function'", 40000);
  await sleep(500);
  const a1 = await evaluate("document.getElementById('fm1').dataset.state");
  await evaluate("__simPlug(true)");
  const offer = await waitFor("document.getElementById('fm1').dataset.state === 'found'", 15000);
  await sleep(1500);
  const a2 = await evaluate("({ mode: __mgr.S.mode, txt: document.getElementById('unit').innerText.replace(/\\s+/g, ' '), fm1: document.getElementById('fm1').innerText.replace(/\\s+/g, ' ') })");
  await shot("manager-offline-found.png");
  check(aw && a1 === "absent" && offer && a2.mode === "browser" && !/FIZZ/.test(a2.txt), `offline: the FM-1 plugged in: the circle becomes "${a2.fm1}", not taken by itself; the pill "${a2.txt}"`);
  await evaluate("__mgr.S.busy = true");
  await click("#fm1");
  const refused = await evaluate("__mgr.S.mode === 'browser' && /Wait/.test(document.getElementById('toast').textContent)");
  await evaluate("__mgr.S.busy = false");
  await realClick("#fm1");
  const sw = await waitFor("__mgr.S.mode === 'fm1' && !!__mgr.S.link && __mgr.S.unit && __mgr.S.unit.id !== 'FM-1_46999999' && __mgr.S.unit.rows.length > 0", 40000);
  check(refused && sw, "offline: ⇄ FM-1 refused while a write runs; one click then: the FM-1 is the unit");
  await evaluate("__simPlug(false)");
  const fb = await waitFor("__mgr.S.mode === 'browser' && !!__mgr.S.link && /unplugged/i.test(document.getElementById('toast').textContent)", 40000);
  check(fb && (await evaluate("document.getElementById('fm1').dataset.state === 'absent'")), "offline: the FM-1 unplugged: back on THIS BROWSER, a toast");
  }
  // ---- (end of the offline mode)
  // ---- parity: USB/BLUETOOTH, NEW project, LOAD to a track
  // (the iOS app's: the FM-1's port named in the picker and on ⇄ FM-1; NEW on an empty PROJECTS slot, the unit's
  // PROJECT NEW; LIBRARY's LOAD ON FM-1 onto a track 1..6, remembered for the tab's session. ?sim&bt names the simulated
  // port "FM-1 C1F0 Bluetooth", as the Bluetooth package's)
  {
  const via = () => evaluate(`({ fm1: (document.querySelector('#fm1 small') || {}).textContent || '', pill: (document.querySelector('#unit small') || {}).textContent || '',
    row: ((document.querySelector("#upick-pop .urow[data-u='fm1'] .usb") || {}).textContent || '').trim(), ble: !!document.querySelector("#upick-pop .urow[data-u='fm1'] .usb.ble") })`);
  for (const bt of [false, true]) {
    await send("Page.navigate", { url: URL0.replace("?sim&", `?sim=away${bt ? "&bt" : ""}&`) + "#library" });
    await waitFor("window.__mgr && __mgr.S.mode === 'browser' && !!__mgr.S.link && typeof window.__simPlug === 'function'", 40000);
    await evaluate("__simPlug(true)");
    const found = await waitFor("document.getElementById('fm1').dataset.state === 'found'", 15000);
    await click("#unit");
    await sleep(300);
    const v = await via(), want = bt ? "BLUETOOTH" : "USB";
    if (bt) await shot("manager-parity-picker-bluetooth.png");
    await click("#upick-scrim");
    check(found && v.fm1 === want && v.row === want && v.ble === bt, `parity: an FM-1 found on "${await evaluate("__mgr.FM.found && __mgr.FM.found.port")}": ⇄ FM-1 says ${v.fm1}, the picker's row ${v.row}`);
  }
  await realClick("#fm1");
  const bOn = await waitFor("__mgr.S.mode === 'fm1' && !!__mgr.S.link && __mgr.S.unit && __mgr.S.unit.rows.length > 0 && __mgr.S.unit.status", 40000);
  await click("#unit");
  await sleep(300);
  const v2 = await via();
  await click("#upick-scrim");
  check(bOn && / · BLUETOOTH$/.test(v2.pill) && v2.row === "BLUETOOTH" && v2.ble && (await evaluate("__mgr.S.link.viaBle")),
    `parity: the FM-1 the unit over Bluetooth: the pill "${v2.pill}", the picker's row ${v2.row}`);

  // NEW on the FM-1: an empty slot's NEW (in LOAD's place) makes an empty project there, open; the lists show it
  await idle();
  const p = await evaluate("__mgr.S.unit.projects.list.findIndex((x) => !x.there)");
  await evaluate(`(() => { for (const t of ['backup', 'library', 'edit', 'pool', 'projects', 'import']) document.getElementById('tab-' + t).hidden = t !== 'projects';
    __mgr.projtab.state.sel = ${p}; __mgr.projtab.render(); })()`);
  const n0 = await evaluate("({ nw: !!document.querySelector('#pj-acts [data-act=new]:not(:disabled)'), load: !!document.querySelector('#pj-acts [data-act=load]'), arc: !!document.querySelector('#pj-acts .arc[data-act=new]') })");
  await click('#pj-acts [data-act="new"]');
  const made = await waitFor(`__mgr.S.unit.projects.list[${p}].there && __mgr.S.unit.projects.list[${p}].current && !__mgr.S.editing && !__mgr.S.writing`, 20000);
  await sleep(200);
  const n1 = await evaluate(`({ empty: document.querySelector('#pj-slots .slot[data-p="${p}"]').classList.contains('empty'), load: !!document.querySelector('#pj-acts [data-act=load]'),
    nw: !!document.querySelector('#pj-acts [data-act=new]'), toast: document.getElementById('toast').textContent, st: __mgr.S.unit.status.project })`);
  check(p > 0 && n0.nw && n0.arc && !n0.load && made && !n1.empty && n1.load && !n1.nw && n1.st === p + 1 && /new project/i.test(n1.toast),
    `parity: NEW on the FM-1's empty slot ${p + 1}: a new project, open (STATUS project ${n1.st}); the slot drawn full, LOAD back ("${n1.toast}")`);

  // LOAD ON FM-1 onto a track: the choice starts on the unit's selected track, a pick is kept; OCT+ loads it there
  await evaluate("window.__sim.selectTrack(2)");
  await evaluate("__mgr.refreshRows()");
  await click("#t-library");
  await click("#l-filter [data-src='all']");
  await click("#l-list .cas[data-src='factory'][data-slot='6']");
  await waitFor("!!document.getElementById('l-unit') && !document.getElementById('l-unit').disabled", 8000);
  const sel0 = await evaluate("__mgr.S.unit.status.sel");
  const on = () => evaluate("[...document.querySelectorAll('#l-sel .ltrk button.on')].map((b) => +b.dataset.ltrk).join()");
  const d0 = await on();
  await click("#l-sel [data-ltrk='3']");
  const d1 = await on();
  await shot("manager-parity-load-track.png");
  const f6 = await evaluate("__mgr.S.unit.rows.find((r) => r.source === 'factory' && r.slot === 6)");
  await click("#l-unit");
  const opened = await waitFor("!!__mgr.live.browsing() && __mgr.live.browsing().t === 3 && document.getElementById('l-unit').classList.contains('on')", 5000);
  await evaluate("window.__sim.press('OCT+')");
  const loaded = await waitFor(`!__mgr.live.browsing() && document.getElementById('toast').textContent.includes(${JSON.stringify(f6.name)})`, 8000);
  const t4 = await evaluate(`(async () => { const { soundFromFrame } = await import('./sound.js'); const s = soundFromFrame(await __mgr.S.link.dumpTrack(4)); return { id: s.extras.id, name: s.name }; })()`);
  check(sel0 === 2 && d0 === "2" && d1 === "3" && opened && loaded && t4.id === f6.id,
    `parity: LOAD ON FM-1's track: ${+d0 + 1} at first (the unit's selected), 4 picked; ${f6.name} opened on track 4, OCT+: track 4 plays ${t4.name}`);
  await send("Page.navigate", { url: URL0 + "#library" });
  await waitFor("window.__mgr && __mgr.S.unit && __mgr.S.unit.status && __mgr.S.unit.rows.length > 0", 40000);
  await click("#l-filter [data-src='all']");
  await click("#l-list .cas[data-src='factory'][data-slot='2']");
  await waitFor("!!document.querySelector('#l-sel .ltrk')", 8000);
  check((await on()) === "3", "parity: the track picked is kept over a reload (this tab's session)");
  await evaluate("sessionStorage.removeItem('fizz-manager-load-track')");

  // NEW in THIS BROWSER: the empty slot's patterns area offers NEW PROJECT; after it the store is read again (its grid)
  await send("Page.navigate", { url: URL0.replace("?sim&", "?sim=nomidi&") + "#projects" });
  await waitFor("window.__mgr && __mgr.S.mode === 'browser' && !!__mgr.S.link && !!__mgr.projtab.state.base && !__mgr.S.busy", 60000);
  const q = await evaluate("__mgr.S.unit.projects.list.findIndex((x) => !x.there)");
  await click(`#pj-slots .slot[data-p="${q}"]`);
  const offered = await waitFor("!!document.getElementById('pj-new') && !!document.querySelector('#pj-acts [data-act=new]')", 5000);
  await click("#pj-new");
  const bmade = await waitFor(`__mgr.S.unit.projects.list[${q}].there && document.querySelectorAll('#pj-grid .pt').length === 64 && !__mgr.S.editing && !__mgr.S.writing && !__mgr.S.busy`, 30000);
  check(q > 0 && offered && bmade && !(await evaluate("!!document.getElementById('pj-new')")),
    `parity: NEW PROJECT in THIS BROWSER's empty slot ${q + 1}: made, its 64 patterns shown (the store read again)`);
  }
  // ---- (end of parity)
  // ---- EDIT pan and sends
  // (PAN, CHO, DEL, REV beside LEVEL on AMP: the one panel's 2 x 2, AMP's page; a drag, the wheel, a double click;
  // SAVE AS NEW stores them; with TRACK on an edit is a PARAM 115..118, NOT FOUND (an older FIZZ, stubbed): the
  // AUDITION, debounced; the browser's FIZZ the same, its fallback STORE + LOAD; a MIDI sound shows none)
  {
  await size(1440, 900);
  await send("Page.navigate", { url: URL0 + "#edit" });
  await waitFor("window.__mgr && __mgr.S.unit && __mgr.S.unit.status && __mgr.S.unit.rows.length > 0", 40000);
  await click("#t-edit");
  await waitFor("!!__mgr.editor.sound() && !!document.querySelector('#ed-stage .pn-amp .t[data-id=\"115\"]')", 15000);
  await sleep(700);
  const mixAt = () => evaluate(`(() => { const sec = document.querySelector('#ed-stage .pn-amp'), els = [115, 118, 116, 117].map((i) => sec && sec.querySelector('.t[data-id="' + i + '"]'));
    return { mode: __mgr.editor.state.mode, cur: !!(sec && (sec.classList.contains('cur') || __mgr.editor.state.mode === 'panel')), labels: els.map((e) => (e ? e.querySelector('small').textContent : '-')).join(' '),
      shown: els.every((e) => e && e.offsetWidth > 30 && e.offsetHeight > 20) }; })()`);
  const mx = (id) => evaluate(`document.querySelector('#ed-stage .pn-amp .t[data-id="${id}"] b').textContent`);
  const m14 = await mixAt();
  check(m14.mode === "panel" && m14.shown && m14.labels === "PAN CHO DEL REV", `pan and sends: at 1440 x 900 on the one panel's AMP (${m14.labels})`);
  // a sound of this browser's (not the track's): dragged, the wheel, a double click
  await evaluate(`(async () => { const { newSound } = await import('./sound.js'); __mgr.live.detach(); const s = newSound('fm'); s.name = 'MIX TEST';
    __mgr.editor.open(s, { label: 'MIX TEST', source: 'file' }); })()`);
  await sleep(300);
  const d0 = await evaluate("({ pan: __mgr.editor.sound().pan, rev: __mgr.editor.sound().sends.reverb })");
  await drag("#ed-stage .pn-amp .t[data-id='115']", 0, -60);
  await sleep(150);
  const r1 = await evaluate("__mgr.editor.sound().pan"), t1 = await mx(115);
  await drag("#ed-stage .pn-amp .t[data-id='115']", 0, 160);
  await sleep(150);
  const r2 = await evaluate("__mgr.editor.sound().pan"), t2 = await mx(115);
  check(d0.pan === 0 && d0.rev === 20 && r1 > 0 && t1 === `R${r1}` && r2 < 0 && t2 === `L${-r2}`, `pan and sends: PAN dragged up ${t1}, down ${t2} (from C; REVERB from 20)`);
  await evaluate(`document.querySelector('#ed-stage .pn-amp .t[data-id="115"]').dispatchEvent(new MouseEvent('dblclick', { bubbles: true }))`);
  await sleep(150);
  const wc = await centre("#ed-stage .pn-amp .t[data-id='116']");
  await send("Input.dispatchMouseEvent", { type: "mouseWheel", x: wc.x, y: wc.y, deltaX: 0, deltaY: -100 });
  await sleep(150);
  await drag("#ed-stage .pn-amp .t[data-id='118']", 0, -60);
  await sleep(150);
  const w1 = await evaluate("({ pan: __mgr.editor.sound().pan, s: __mgr.editor.sound().sends })"), tp = await mx(115), tc = await mx(118);
  check(w1.pan === 0 && tp === "C" && w1.s.delay === 1 && w1.s.chorus > 0 && tc === String(w1.s.chorus) &&
    (await evaluate("!!document.querySelector('#ed-head .edited') && !document.getElementById('ed-revert').disabled")),
    `pan and sends: a double click PAN back to C; the wheel over DEL ${w1.s.delay}; CHO dragged ${tc}; EDITED`);
  // SAVE AS NEW: the mix stored with it (its DUMP back)
  await evaluate("__mgr.editor.set(115, -12); __mgr.editor.set(116, 24); __mgr.editor.set(117, 40); __mgr.editor.set(118, 7)");
  await sleep(150);
  await shot("manager-mix-edit-1440.png");
  await click("#ed-savenew");
  await idle();
  await sleep(400);
  const sv = await evaluate(`(async () => { const { soundFromFrame } = await import('./sound.js'); const r = __mgr.S.unit.rows.find((q) => q.source === 'mine' && q.name.trim() === 'MIX TEST');
    if (!r) return null; const s = soundFromFrame(await __mgr.S.link.dump('mine', r.slot)); return { slot: r.slot, pan: s.pan, s: s.sends }; })()`);
  check(sv && sv.pan === -12 && sv.s.delay === 24 && sv.s.reverb === 40 && sv.s.chorus === 7, `pan and sends: SAVE AS NEW, DUMP back (MY PRESETS ${sv && sv.slot}): ${sv && JSON.stringify(sv)}`);
  // AMP's page: at 1280 x 800 (pages there) and on a phone
  await size(1280, 800);
  await sleep(700);
  if ((await evaluate("__mgr.editor.state.mode")) === "tabs") await click("#ed-pages [data-page='amp']");
  await sleep(200);
  const m12 = await mixAt();
  check(m12.shown && m12.cur && m12.labels === "PAN CHO DEL REV" && (await noScroll()), `pan and sends: at 1280 x 800 on ${m12.mode === "tabs" ? "AMP's page" : "the one panel's AMP"}`);
  await shot("manager-mix-edit-1280.png");
  await size(390, 844, true);
  await sleep(700);
  await click("#ed-pages [data-page='amp']");
  await sleep(200);
  const m39 = await mixAt();
  check(m39.mode === "tabs" && m39.shown && m39.cur && (await noScroll()), "pan and sends: at 390 on AMP's page, no horizontal scroll");
  await evaluate("document.querySelector('#ed-stage .pn-amp .t[data-id=\"115\"]').scrollIntoView({ block: 'center' })");
  await shot("manager-mix-edit-390.png");
  await size(1280, 800);
  await sleep(400);
  // a MIDI sound: no mix (a MIDI track takes no voice)
  await evaluate(`(async () => { const { newSound } = await import('./sound.js'); __mgr.editor.open(newSound('midi'), { label: 'MIDI', source: 'file' }); })()`);
  await sleep(300);
  check(await evaluate("!!document.querySelector('#ed-stage .mpg') && !document.querySelector('#ed-stage .t[data-id=\"115\"]')"), "pan and sends: a MIDI sound shows none");

  // TRACK on: an edit is a PARAM 115..118 (this wasm takes them); NOT FOUND (stubbed): the AUDITION, 120 ms after the last
  await click("#live");
  await waitFor("__mgr.live.on()");
  await evaluate("__mgr.live.attach()");
  await waitFor("__mgr.editor.place().fromTarget && !__mgr.editor.sound().midi", 8000);
  await evaluate(`(() => { const lk = __mgr.S.link; window.__pids = []; window.__stores = 0; window.__lp = lk.param; window.__ls = lk.store;
    lk.param = (t, id, v) => { window.__pids.push(id); return window.__stubNF && id >= 115 ? Promise.resolve({ kind: 'ack', command: 0x30, status: 9, statusName: 'NOT FOUND' }) : window.__lp.call(lk, t, id, v); };
    lk.store = (f) => { window.__stores++; return window.__ls.call(lk, f); }; })()`);
  const trkMix = () => evaluate(`(async () => { const { soundFromFrame } = await import('./sound.js'); const s = soundFromFrame(await __mgr.S.link.dumpTrack(__mgr.live.state.t + 1)); return { pan: s.pan, ...s.sends }; })()`);
  const drained = () => waitFor("!__mgr.live.state.pumping && !__mgr.live.state.q.size", 5000);
  await drag("#ed-stage .pn-amp .t[data-id='115']", 0, 50);
  await evaluate("__mgr.editor.set(116, 50); __mgr.editor.set(117, 60); __mgr.editor.set(118, 70)");
  await drained();
  await sleep(200);
  const lv = await evaluate("({ ids: [...new Set(window.__pids)].sort().join(), stores: window.__stores, pan: __mgr.editor.sound().pan })"), tm = await trkMix();
  check(lv.ids === "115,116,117,118" && lv.stores === 0 && lv.pan < 0 && tm.pan === lv.pan && tm.delay === 50 && tm.reverb === 60 && tm.chorus === 70,
    `pan and sends live: PARAMs ${lv.ids}, no AUDITION; the track has pan ${tm.pan}, delay ${tm.delay}, reverb ${tm.reverb}, chorus ${tm.chorus}`);
  await evaluate("window.__stubNF = true; window.__pids = []; window.__stores = 0; __mgr.editor.set(116, 77); __mgr.editor.set(117, 33); __mgr.editor.set(117, 34); __mgr.editor.set(117, 35)");
  await sleep(500);
  await drained();
  const nf1 = await evaluate("({ ids: window.__pids.join(), stores: window.__stores, no: !!__mgr.live.state.noMix })");
  await evaluate("window.__pids = []; window.__stores = 0; [11, 12, 13, 14].forEach((x) => __mgr.editor.set(118, x))");
  await sleep(400);
  await drained();
  await sleep(200);
  const nf2 = await evaluate("({ ids: window.__pids.join(), stores: window.__stores })"), tm2 = await trkMix();
  check(nf1.no && nf1.ids === "116,117" && nf1.stores === 1 && nf2.ids === "" && nf2.stores === 1 && tm2.delay === 77 && tm2.reverb === 35 && tm2.chorus === 14,
    `pan and sends live, NOT FOUND: PARAMs ${nf1.ids}, then remembered (none); one AUDITION each time (${nf1.stores}, ${nf2.stores}); the track has delay ${tm2.delay}, reverb ${tm2.reverb}, chorus ${tm2.chorus}`);
  await evaluate("(() => { const lk = __mgr.S.link; lk.param = window.__lp; lk.store = window.__ls; window.__stubNF = false; __mgr.live.state.noMix = null; })()");
  await click("#live");
  await waitFor("!__mgr.live.on()");

  // the browser's FIZZ (EDIT without TRACK): a PARAM onto its track 1; NOT FOUND (stubbed): STORE + LOAD as before
  await evaluate(`(async () => { const { newSound } = await import('./sound.js'); __mgr.live.detach(); const s = newSound('fm'); s.name = 'MIX BROWSER';
    __mgr.editor.open(s, { label: 'MIX BROWSER', source: 'file' }); })()`);
  await sleep(200);
  if (!(await evaluate("__mgr.bfizz.ready"))) await realClick("#ed-target");
  const bfr = await waitFor("__mgr.bfizz.ready", 20000);
  await sleep(500);
  await evaluate(`(() => { const bf = __mgr.bfizz; window.__bf = []; window.__ba = bf.ask;
    bf.ask = (f, want, ms) => { window.__bf.push(f[5]); return window.__bstub && f[5] === 0x30 ? Promise.resolve({ kind: 'ack', command: 0x30, status: 9, statusName: 'NOT FOUND' }) : window.__ba.call(bf, f, want, ms); }; })()`);
  const bMix = () => evaluate(`(async () => { const { dumpRequest } = await import('../js/fizz_sysex.js'); const r = await window.__ba.call(__mgr.bfizz, dumpRequest({ source: 'track', slot: 1 }), (x) => x.kind === 'dump', 2000);
    return r && { pan: r.pan, chorus: r.chorus, delay: r.delay, reverb: r.reverb }; })()`);
  await evaluate("__mgr.editor.set(115, -7); __mgr.editor.set(117, 66)");
  await sleep(600);
  const b1 = await evaluate("window.__bf.join()"), bm1 = await bMix();
  await evaluate("window.__bf = []; window.__bstub = true; __mgr.editor.set(116, 44)");
  await sleep(600);
  await evaluate("__mgr.editor.set(118, 9)");
  await sleep(600);
  const b2 = await evaluate("window.__bf.join()"), bm2 = await bMix();
  check(bfr && b1 === "48,48" && bm1 && bm1.pan === -7 && bm1.reverb === 66 && b2.startsWith("48,17") && !b2.slice(3).includes("48") && bm2 && bm2.delay === 44 && bm2.chorus === 9,
    `pan and sends, the browser's FIZZ: PARAMs (${b1}), its track 1 pan ${bm1 && bm1.pan}; NOT FOUND: STORE + LOAD from then (${b2}), delay ${bm2 && bm2.delay}, chorus ${bm2 && bm2.chorus}`);
  await evaluate("__mgr.bfizz.ask = window.__ba; window.__bstub = false");
  }
  // ---- (end of EDIT pan and sends)
  // ---- nf-play: PLAY (the virtual FM-1: its own target, THIS BROWSER or the FM-1's remote; the views; the phone)
  {
  const SHOTS = path.join(OUT, "play");
  fs.mkdirSync(SHOTS, { recursive: true });
  const pshot = (f) => shot(path.join("play", f));
  const pdown = async (sel) => { const c = await centre(sel); if (c) await mouse("mousePressed", c.x, c.y); return c; };
  const pup = async (c) => mouse("mouseReleased", c.x, c.y);
  await size(1280, 800);
  await send("Page.navigate", { url: URL0.replace(/v=\d+/, `v=${Date.now()}`) + "#library" });
  await waitFor("window.__mgr && __mgr.S.unit && __mgr.S.unit.rows.length > 0 && __mgr.S.mode === 'fm1'", 40000);
  const t7 = await evaluate(`(() => { const t = [...document.querySelectorAll('.tabs .tab')]; const p = document.getElementById('t-play');
    return { n: t.length, last: t[t.length - 1] === p, c: p.style.getPropertyValue('--c'), col: getComputedStyle(p.querySelector('i')).backgroundColor }; })()`);
  check(t7.n === 7 && t7.last && /--blue/.test(t7.c) && t7.col === "rgb(63, 125, 255)" && (await noScroll()), `PLAY: the seventh tab, blue (${t7.col}), the bar still fits at 1280`);
  await realClick("#t-play");
  await waitFor("__mgr.play.state.shown && !document.getElementById('tab-play').hidden");
  const tg = await evaluate("[...document.querySelectorAll('#pl-tgt .tg')].map((t) => t.dataset.tg).join()");
  await realClick("#pl-tgt [data-tg='fm1']");
  const onFm = await waitFor("__mgr.play.state.cur === 'fm1' && document.getElementById('pl-board').classList.contains('fm')", 3000);
  check(tg === "browser,fm1" && onFm, `PLAY: the target: THIS BROWSER and the FM-1 offered with an FM-1 connected (${tg}); FM-1 picked`);
  await realClick("#pl-views [data-v='full']");
  await waitFor("__mgr.play.state.lay === 'full'");
  await sleep(300);
  await evaluate("window.__pout = []; window.__sim.hooks.midi = (d) => window.__pout.push(Array.from(d).join(' '))");
  // a button (HOME, QD_B_* 6): CC 26 127 on channel 16 when pressed, 0 when let go
  const hb = await pdown("#pl-board .pb[data-b='6']");
  await sleep(150);
  const bDown = await evaluate("window.__pout.join('|')");
  await pup(hb);
  await sleep(150);
  const bUp = await evaluate("window.__pout.join('|')");
  check(bDown === "191 26 127" && bUp === "191 26 127|191 26 0", `PLAY → FM-1: HOME pressed sends CC 26 127, let go CC 26 0 on channel 16 (${bUp})`);
  // an encoder (KN1, QD_K_1 = 3) dragged up: CC 43 relative, 64 + n
  await evaluate("window.__pout = []");
  await drag("#pl-board .knob[data-role='3']", 0, -60);
  await sleep(150);
  const enc = await evaluate("window.__pout.slice()");
  const encN = enc.reduce((a, m) => a + (+m.split(" ")[2] - 64), 0);
  check(enc.length > 0 && enc.every((m) => /^191 43 (6[5-9]|[7-9]\d)$/.test(m)) && encN === 4, `PLAY → FM-1: KN1 dragged up 60 px: CC 43 at 64 + n, ${encN} detents (${enc.join("|")})`);
  // a key (white key 7, C4 at OCT 0): a note on the selected track's channel, velocity by depth, then its note off
  await evaluate("window.__pout = []");
  const ch = await evaluate("__mgr.play.state.track.chan"), oct = await evaluate("__mgr.editor.octave()");
  const kc = await pdown("#pl-board .k[data-key='7']");
  await sleep(150);
  await pup(kc);
  await sleep(150);
  const notes = await evaluate("window.__pout.slice()");
  const nOn = notes[0] && notes[0].split(" ").map(Number), nOff = notes[1] && notes[1].split(" ").map(Number);
  check(notes.length === 2 && nOn[0] === (0x90 | ch) && nOn[1] === 60 + 12 * oct && nOn[2] > 24 && nOn[2] < 127 && nOff[0] === (0x80 | ch) && nOff[1] === nOn[1],
    `PLAY → FM-1: a key is a note on the track's channel ${ch + 1} (${notes.join("|")}), not channel 16`);
  await pshot("play-1280-fm1-full.png");
  // leaving the tab lets everything go: CC 123 on channel 16
  await evaluate("window.__pout = []");
  await realClick("#t-library");
  await sleep(200);
  const left = await evaluate("window.__pout.join('|')");
  check(/191 123 0/.test(left) && !(await evaluate("__mgr.play.state.shown")), `PLAY → FM-1: leaving the tab sends CC 123 (${left})`);
  // the views: KEYS (the chips), a swipe on the screen, SCREEN; remembered
  await realClick("#t-play");
  await waitFor("__mgr.play.state.shown");
  await realClick("#pl-views [data-v='keys']");
  const vk = await waitFor("__mgr.play.state.lay === 'keys' && !!document.querySelector('#pl-board .bed') && !document.querySelector('#pl-board .pb')", 2000);
  await drag("#pl-board .bezel", -120, 0);
  const vs = await waitFor("__mgr.play.state.view === 'screen' && !document.querySelector('#pl-board .bed')", 2000);
  await realClick("#pl-views [data-v='panel']");
  const vp = await waitFor("__mgr.play.state.lay === 'panel' && document.querySelectorAll('#pl-board .pb').length === 14", 2000);
  const kept = await evaluate("localStorage.getItem('fizz-play-view') + ' ' + localStorage.getItem('fizz-play-target')");
  check(vk && vs && vp && kept === '"panel" "fm1"', `PLAY: the views: KEYS, a swipe on the screen to SCREEN, PANEL; the view and the target remembered (${kept})`);
  await pshot("play-1280-fm1-panel.png");
  await evaluate("window.__sim.hooks.midi = null");
  await realClick("#pl-tgt [data-tg='browser']");
  await realClick("#pl-views [data-v='full']");
  await realClick("#t-library");

  // THIS BROWSER (no Web MIDI: the browser's FIZZ, quiet until touched): no FM-1 offered; a key reaches the browser FIZZ
  // (on a fresh store, RESET's mark: the earlier sections leave MIDI sounds on its tracks, which make none)
  await evaluate(`new Promise((res) => { const r = indexedDB.open("fizz-manager", 1); r.onsuccess = () => { const t = r.result.transaction("kv", "readwrite");
    t.objectStore("kv").put(new Uint8Array(0), "browserFlash"); t.oncomplete = () => res(true); t.onerror = () => res(false); }; r.onerror = () => res(false); })`);
  await send("Page.navigate", { url: URL0.replace("?sim&", "?sim=nomidi&").replace(/v=\d+/, `v=${Date.now()}`) + "#play" });
  await waitFor("window.__mgr && __mgr.S.mode === 'browser' && !!__mgr.S.link && __mgr.bu() && __mgr.play.state.shown", 40000);
  await sleep(600);
  const nb = await evaluate("({ tiles: [...document.querySelectorAll('#pl-tgt .tg')].map((t) => t.dataset.tg).join(), cur: __mgr.play.state.cur, lcd: !!document.querySelector('#pl-board canvas.lcd') })");
  await realClick("#pl-board .bezel");
  const woke = await waitFor("!__mgr.bu().quiet && __mgr.bu().running", 20000);
  await sleep(1500);
  const hm = await pdown("#pl-board .pb[data-b='6']");   // (HOME: FIZZ out of its idle screen; the press that wakes it does nothing else)
  await sleep(120);
  await pup(hm);
  await sleep(800);
  await evaluate(`(() => { const p = __mgr.bu().player; window.__pan = p.ctx.createAnalyser(); window.__pan.fftSize = 2048; p.out.connect(window.__pan); window.__pb = new Float32Array(2048);
    window.__ppk = () => { window.__pan.getFloatTimeDomainData(window.__pb); let m = 0; for (const x of window.__pb) m = Math.max(m, Math.abs(x)); return m; };
    window.__psent = 0; const o = p.panel.bind(p); p.panel = (m) => { if (m.keys & (1 << 7)) window.__psent++; o(m); }; })()`);
  const kb = await pdown("#pl-board .k[data-key='7']");
  let pk = 0;
  for (let i = 0; i < 15; i++) { await sleep(40); pk = Math.max(pk, await evaluate("window.__ppk()")); }
  const lit = await evaluate("({ held: document.querySelector('#pl-board .k[data-key=\"7\"]').classList.contains('held'), px: !!__mgr.play.state.px, sent: window.__psent })");
  await pup(kb);
  check(nb.tiles === "browser" && nb.cur === "browser" && nb.lcd && woke && lit.sent > 0 && lit.px && pk > 0.001,
    `PLAY → THIS BROWSER: no FM-1 offered (${nb.tiles}); the first touch wakes it (${woke}); a key reaches its FIZZ (the panel's key bit sent ${lit.sent}, sound out ${pk.toFixed(4)}); its screen shown (${lit.px})`);
  await pshot("play-1280-browser-full.png");
  // the phone: upright (the tiles, the views, the board stacked), on its side (the bar gives way to the strip)
  await size(390, 844, true);
  await sleep(700);
  const pp = await evaluate("({ lay: __mgr.play.state.lay, sw: document.documentElement.scrollWidth, w: innerWidth, bw: Math.round(document.getElementById('pl-wrap').getBoundingClientRect().right) })");
  await pshot("play-390x844.png");
  await send("Emulation.setDeviceMetricsOverride", { width: 844, height: 390, deviceScaleFactor: 1, mobile: true, screenOrientation: { type: "landscapePrimary", angle: 90 } });
  await sleep(700);
  const pl = await evaluate(`({ lay: __mgr.play.state.lay, sw: document.documentElement.scrollWidth, w: innerWidth, top: getComputedStyle(document.querySelector('.top')).display,
    strip: getComputedStyle(document.querySelector('#tab-play .pstrip')).display, b: Math.round(document.getElementById('pl-wrap').getBoundingClientRect().bottom) })`);
  await pshot("play-844x390.png");
  check(pp.lay === "tall-full" && pp.sw <= pp.w + 1 && pp.bw <= pp.w && pl.lay === "wide" && pl.sw <= pl.w + 1 && pl.top === "none" && pl.strip === "flex" && pl.b <= 391,
    `PLAY on a phone: upright ${pp.lay} (no horizontal scroll), on its side ${pl.lay} under the strip, the bar gone, the board within the screen (${pl.b} px)`);
  await realClick("#pl-back");
  const back = await waitFor("!__mgr.play.state.shown && getComputedStyle(document.querySelector('.top')).display !== 'none'", 2000);
  check(back, "PLAY on its side: the strip's way back to the Manager");
  await size(1280, 800);
  await sleep(300);
  }
  // ---- (end of nf-play)
  // ---- nf-play2 (the owner's fixes, 2026-10-29): the board flat (no glow around it, no gradient on its buttons and
  // keys); no THIS BROWSER chip on it; key 11 (B11) with the longer LED; Shift-click latches a button (the FM-1: CC 127
  // once, held while other buttons are clicked; a click again CC 0), a plain click does not, leaving the tab lets go
  {
  const SHOTS2 = path.join(OUT, "play2");
  fs.mkdirSync(SHOTS2, { recursive: true });
  const shiftClick = async (sel) => { const c = await centre(sel); await mouse("mousePressed", c.x, c.y, { modifiers: 8 }); await mouse("mouseReleased", c.x, c.y, { modifiers: 8 }); };
  await size(1440, 900);
  await send("Page.navigate", { url: URL0.replace(/v=\d+/, `v=${Date.now()}`) + "#play" });
  await waitFor("window.__mgr && __mgr.S.unit && __mgr.S.unit.rows.length > 0 && __mgr.S.mode === 'fm1' && __mgr.play.state.shown", 40000);
  await realClick("#pl-tgt [data-tg='browser']");
  await realClick("#pl-views [data-v='full']");
  await waitFor("__mgr.play.state.cur === 'browser' && __mgr.play.state.lay === 'full'", 3000);
  await sleep(300);
  const look = await evaluate(`(() => { const b = document.getElementById('pl-board');
    const bg = (sel) => [...b.querySelectorAll(sel)].map((e) => getComputedStyle(e).backgroundImage);
    const led = (k) => parseFloat(getComputedStyle(b.querySelector('.k[data-key="' + k + '"]'), '::after').height);
    const pb = bg('.pb'), k = bg('.k');
    return { glow: getComputedStyle(b).boxShadow, pb: pb.length === 14 && pb.every((x) => x === 'none'), k: k.length === 27 && k.every((x) => x === 'none'),
      chip: /THIS BROWSER/.test(b.textContent) || !!b.querySelector('.bpill'), l11: led(25), near: [22, 24, 26].map(led) }; })()`);
  check(look.glow === "none" && look.pb && look.k, `PLAY flat: no glow around the board (${look.glow}), no gradient on its 14 buttons (${look.pb}) and 27 keys (${look.k})`);
  check(!look.chip, "PLAY → THIS BROWSER: no THIS BROWSER chip on the board");
  check(look.l11 > Math.max(...look.near) + 8, `PLAY: key 11 (B11, F#5) has the longer LED (${look.l11} px; B10, W15, W16: ${look.near.join(", ")})`);
  await shot(path.join("play2", "play-1440-browser-full.png"));
  // THIS BROWSER: a key Shift-clicked stays down in its FIZZ's panel until clicked again
  await realClick("#pl-board .bezel");
  await waitFor("!__mgr.bu().quiet && __mgr.bu().running", 20000);
  await sleep(1200);
  await realClick("#pl-board .pb[data-b='6']");
  await sleep(600);
  await evaluate("(() => { const p = __mgr.bu().player; window.__pk = []; const o = p.panel.bind(p); p.panel = (m) => { window.__pk.push(m.keys); o(m); }; })()");
  await shiftClick("#pl-board .k[data-key='7']");
  await sleep(500);
  const bl = await evaluate("({ held: __mgr.play.controls.keyLatched(7), last: window.__pk[window.__pk.length - 1], cls: document.querySelector('#pl-board .k[data-key=\"7\"]').classList.contains('latched') })");
  await shot(path.join("play2", "play-1440-browser-latched.png"));
  await realClick("#pl-board .k[data-key='7']");
  await sleep(200);
  const bu = await evaluate("({ held: __mgr.play.controls.keyHeld(7), last: window.__pk[window.__pk.length - 1] })");
  check(bl.held && bl.cls && (bl.last & (1 << 7)) && !bu.held && !(bu.last & (1 << 7)),
    `PLAY → THIS BROWSER: a Shift-clicked key stays down in its FIZZ's panel (${bl.last}) until clicked again (${bu.last})`);
  // the FM-1: SHIFT (QD_B 5) Shift-clicked: CC 25 127 once and held; HOME clicked meanwhile: momentary; SHIFT again: CC 25 0
  await realClick("#pl-tgt [data-tg='fm1']");
  await waitFor("__mgr.play.state.cur === 'fm1'", 3000);
  await sleep(300);
  check(await evaluate("getComputedStyle(document.getElementById('pl-board')).boxShadow === 'none'"), "PLAY → FM-1: no glow around the board either");
  await evaluate("window.__pout = []; window.__sim.hooks.midi = (d) => window.__pout.push(Array.from(d).join(' '))");
  await shiftClick("#pl-board .pb[data-b='5']");
  await sleep(250);
  const l1 = await evaluate("({ out: window.__pout.join('|'), held: __mgr.play.controls.btnHeld(5), cls: document.querySelector('#pl-board .pb[data-b=\"5\"]').classList.contains('latched') })");
  await evaluate("window.__pout = []");
  await realClick("#pl-board .pb[data-b='6']");
  await sleep(250);
  const l2 = await evaluate("({ out: window.__pout.join('|'), shift: __mgr.play.controls.btnHeld(5), home: __mgr.play.controls.btnHeld(6) || __mgr.play.controls.btnLatched(6) })");
  await shot(path.join("play2", "play-1440-fm1-latched.png"));
  await evaluate("window.__pout = []");
  await realClick("#pl-board .pb[data-b='5']");
  await sleep(250);
  const l3 = await evaluate("({ out: window.__pout.join('|'), held: __mgr.play.controls.btnHeld(5), cls: document.querySelector('#pl-board .pb[data-b=\"5\"]').classList.contains('latched') })");
  check(l1.out === "191 25 127" && l1.held && l1.cls, `PLAY → FM-1: Shift-click latches SHIFT: CC 25 127 sent once, held, ringed (${l1.out})`);
  check(l2.out === "191 26 127|191 26 0" && l2.shift && !l2.home, `PLAY → FM-1: a plain click on HOME meanwhile is momentary (${l2.out}), SHIFT still held`);
  check(l3.out === "191 25 0" && !l3.held && !l3.cls, `PLAY → FM-1: a second click lets SHIFT go: CC 25 0 (${l3.out})`);
  // latched, then the tab left: let go (CC 25 0, then CC 123)
  await shiftClick("#pl-board .pb[data-b='5']");
  await sleep(200);
  await evaluate("window.__pout = []");
  await realClick("#t-library");
  await sleep(250);
  const l4 = await evaluate("({ out: window.__pout.join('|'), held: __mgr.play.controls.btnHeld(5) })");
  check(/^191 25 0\|.*191 123 0/.test(l4.out) && !l4.held, `PLAY → FM-1: leaving the tab lets a latched SHIFT go (${l4.out})`);
  await realClick("#t-play");
  await waitFor("__mgr.play.state.shown", 3000);
  await sleep(300);
  await shot(path.join("play2", "play-1440-fm1-full.png"));
  await send("Emulation.setDeviceMetricsOverride", { width: 844, height: 390, deviceScaleFactor: 1, mobile: true, screenOrientation: { type: "landscapePrimary", angle: 90 } });
  await sleep(700);
  await shot(path.join("play2", "play-844x390-fm1.png"));
  await realClick("#pl-tgpill");
  await waitFor("__mgr.play.state.cur === 'browser'", 3000);
  await sleep(400);
  await shot(path.join("play2", "play-844x390-browser.png"));
  await evaluate("window.__sim.hooks.midi = null");
  await realClick("#pl-back");
  await size(1280, 800);
  await sleep(300);
  }
  // ---- (end of nf-play2)
  // ---- nf-new (parity 2026-10-29): LIBRARY's + NEW (FM / MIDI, into the FM-1 or IN THIS BROWSER), the IN THIS BROWSER
  // card (rename, tags, web tags, DUPLICATE, EDIT -> SAVE into the browser's store, SEND, LOAD), pan and sends kept
  // through each, RESET then + NEW, LOAD ON FM-1's track under the name, a phone
  {
  await size(1280, 800);
  await send("Page.navigate", { url: URL0.replace(/v=\d+/, `v=${Date.now()}`) + "#library" });
  await waitFor("window.__mgr && __mgr.S.mode === 'fm1' && __mgr.S.unit && __mgr.S.unit.status && __mgr.S.unit.rows.length > 0", 40000);
  await idle();
  await click("#t-library");
  const mineRows = () => evaluate("__mgr.S.unit.rows.filter((r) => r.source === 'mine').map((r) => ({ slot: r.slot, id: r.id, name: r.name }))");
  const mdump = (slot) => evaluate(`(async () => { const { parseDump } = await import('../js/fizz_sysex.js'); const { isMidiBlob } = await import('./sound.js');
    const d = parseDump(await __mgr.S.link.dump('mine', ${slot})); return d && { name: d.name, pan: d.pan, chorus: d.chorus, delay: d.delay, reverb: d.reverb, midi: isMidiBlob(d.blob) }; })()`);
  // (the browser's store read again from IndexedDB, not from the page's copy)
  const bdb = () => evaluate(`(async () => { const B = await import('./browserlib.js');
    const d = await new Promise((res) => { const r = indexedDB.open('fizz-manager'); r.onsuccess = () => res(r.result); r.onerror = () => res(null); });
    const img = await new Promise((res) => { const g = d.transaction('kv').objectStore('kv').get('browserFlash'); g.onsuccess = () => res(g.result); g.onerror = () => res(null); }); d.close();
    const u8 = img instanceof Uint8Array ? img : img ? new Uint8Array(img) : null;
    return { len: u8 ? u8.length : -1, rows: u8 ? B.browserRows(u8).map((r) => ({ slot: r.slot, id: r.id, name: r.name, tags: r.tags, pan: r.lib.pan, ...r.lib.sends })) : [] }; })()`);
  const stores = { on: () => evaluate("(() => { const lk = __mgr.S.link; window.__nst = 0; window.__ost = lk.store; lk.store = (...a) => { window.__nst++; return window.__ost.apply(lk, a); }; })()"),
    off: () => evaluate("(() => { __mgr.S.link.store = window.__ost; return window.__nst; })()") };

  // LOAD ON FM-1 under the name (mock 13): the label, the track 1..6 and LOAD, between the name and EDIT
  await click("#l-filter [data-src='mine']");
  await click("#l-list .cas[data-src='mine']");
  await waitFor("!!document.querySelector('#l-sel .ldon #l-unit')", 8000);
  const lay = await evaluate(`(() => { const q = (s) => document.querySelector(s).getBoundingClientRect(); const nm = q('#l-sel .selhead'), ld = q('#l-sel .ldon'), ed = q('#l-edit'), tr = q('#l-sel .ldon .ltrk');
    return { order: nm.bottom <= ld.top + 1 && ld.bottom <= ed.top + 1, row: Math.abs(tr.top + tr.height / 2 - (q('#l-unit').top + q('#l-unit').height / 2)) < 4, n: document.querySelectorAll('#l-sel .ldon .ltrk button').length,
      lbl: document.querySelector('#l-sel .ldon .lbl').textContent.trim(), first: document.querySelector('#l-sel .selhead').nextElementSibling.classList.contains('ldon') }; })()`);
  await shot("manager-nfnew-load-under-name.png");
  check(lay.order && lay.row && lay.n === 6 && lay.first && /^Load on FM-1$/i.test(lay.lbl), `nf-new: LOAD ON FM-1 under the name: "${lay.lbl}", tracks 1..${lay.n} and LOAD on one row, above EDIT`);

  // + NEW -> FM: EDIT with an INIT sound; SAVE adds it to the FM-1's MY PRESETS, the mix the track's defaults
  const m0 = await mineRows();
  await click("#l-new");
  await waitFor("!document.getElementById('l-newwrap').hidden", 3000);
  const pop = await evaluate("({ kinds: [...document.querySelectorAll('#l-newpop [data-new]')].map((b) => b.dataset.new).join(), dest: document.querySelector('#l-newpop .ndest').textContent, arc: document.getElementById('l-new').classList.contains('arc') })");
  await shot("manager-nfnew-new.png");
  await click("#l-newpop [data-new='fm']");
  const fmOpen = await waitFor("!document.getElementById('tab-edit').hidden && __mgr.editor.sound().name === 'INIT' && !__mgr.editor.sound().midi && __mgr.editor.place().source === 'new'", 5000);
  const fmSnd = await evaluate("(() => { const s = __mgr.editor.sound(); return { pan: s.pan, ...s.sends, save: !document.getElementById('ed-save').disabled }; })()");
  await click("#ed-save");
  const fmSaved = await waitFor(`__mgr.editor.place().source === 'mine' && __mgr.S.unit.rows.filter((r) => r.source === 'mine').length === ${m0.length + 1}`, 15000);
  await idle();
  const fmRow = (await mineRows()).find((r) => !m0.some((q) => q.id === r.id));
  const fmD = fmRow && (await mdump(fmRow.slot));
  check(pop.kinds === "fm,midi" && pop.arc && /FM-1/.test(pop.dest) && pop.dest.includes(`MY PRESETS`) && fmOpen && fmSnd.save && fmSaved && fmD && fmD.name === "INIT" && !fmD.midi && fmD.pan === 0 && fmD.reverb === 20 && fmD.chorus === 0 && fmD.delay === 0,
    `nf-new: + NEW (${pop.dest.trim()}) -> FM: EDIT on INIT, SAVE: MY PRESETS ${fmRow && fmRow.slot} on the FM-1 (pan ${fmD && fmD.pan}, reverb ${fmD && fmD.reverb})`);
  // + NEW -> MIDI
  await click("#t-library");
  await click("#l-new");
  await click("#l-newpop [data-new='midi']");
  const miOpen = await waitFor("!document.getElementById('tab-edit').hidden && !!__mgr.editor.sound().midi && __mgr.editor.place().source === 'new'", 5000);
  await click("#ed-save");
  const miSaved = await waitFor(`__mgr.editor.place().source === 'mine' && __mgr.S.unit.rows.filter((r) => r.source === 'mine').length === ${m0.length + 2}`, 15000);
  await idle();
  const miRow = (await mineRows()).find((r) => !m0.some((q) => q.id === r.id) && r.id !== fmRow.id);
  const miD = miRow && (await mdump(miRow.slot));
  check(miOpen && miSaved && miD && miD.midi && miD.reverb === 20, `nf-new: + NEW -> MIDI: EDIT on a MIDI sound, SAVE: MY PRESETS ${miRow && miRow.slot}, a MIDI sound on the FM-1 (reverb ${miD && miD.reverb})`);

  // IN THIS BROWSER the source: + NEW saves into the browser's store, nothing to the FM-1. Its pan and sends set in EDIT
  await click("#t-library");
  await click("#l-filter [data-src='browser']");
  const b0 = await bdb(), m1 = (await mineRows()).length;
  await stores.on();
  await click("#l-new");
  const bdest = await evaluate("document.querySelector('#l-newpop .ndest').textContent");
  await click("#l-newpop [data-new='fm']");
  await waitFor("!document.getElementById('tab-edit').hidden && __mgr.editor.place().source === 'new' && __mgr.editor.place().to === 'browser'", 5000);
  await evaluate("(() => { __mgr.editor.state.s.name = 'PANMIX'; __mgr.editor.set(115, -20); __mgr.editor.set(117, 90); __mgr.editor.renderHead(); })()");
  await click("#ed-save");
  const bSaved = await waitFor("__mgr.editor.place().source === 'browser'", 8000);
  const b1 = await bdb(), nst = await stores.off(), pm = b1.rows.find((r) => r.name === "PANMIX");
  check(/THIS BROWSER/.test(bdest) && bSaved && b1.rows.length === b0.rows.length + 1 && pm && pm.pan === -20 && pm.reverb === 90 && nst === 0 && (await mineRows()).length === m1,
    `nf-new: + NEW with IN THIS BROWSER the source (${bdest.trim()}): SAVE puts PANMIX in the browser's store (IndexedDB read again: ${b1.rows.length}, pan ${pm && pm.pan}, reverb ${pm && pm.reverb}); no STORE to the FM-1`);

  // the card: rename, a FIZZ tag, a web tag, DUPLICATE (the mix and the web tag with it)
  await click("#t-library");
  await click(`#l-list .cas[data-src='browser'][data-slot='${pm.slot}']`);
  await waitFor("!!document.getElementById('l-bname')", 5000);
  const card = await evaluate("[...document.querySelectorAll('#l-sel .selgo button')].map((b) => b.id).join() + '|' + [...document.querySelectorAll('#l-sel > *')].map((e) => e.className.split(' ')[0]).join()");
  await shot("manager-nfnew-browser-card.png");
  await evaluate("(() => { const i = document.getElementById('l-bname'); i.value = 'PANMIX R'; i.dispatchEvent(new Event('change')); })()");
  await waitFor("__mgr.S.brow.rows.some((r) => r.name === 'PANMIX R')", 5000);
  await click("#l-sel [data-btag='3']");
  await waitFor(`__mgr.S.brow.rows.some((r) => r.slot === ${pm.slot} && (r.tags & 8))`, 5000);
  await click("#l-addtag");
  await evaluate("(() => { const i = document.querySelector('#l-sel .tagin'); i.value = 'dark'; i.dispatchEvent(new KeyboardEvent('keydown', { key: 'Enter' })); })()");
  const b2 = await bdb(), pr = b2.rows.find((r) => r.slot === pm.slot);
  const web1 = await evaluate("[...document.querySelectorAll('#l-sel [data-web]')].map((b) => b.dataset.web).join()");
  check(card.startsWith("l-send,l-bedit,l-bdup,l-bexport,l-bdel|selhead,selsec,ldon,selgo") && pr && pr.name === "PANMIX R" && pr.id === pm.id && (pr.tags & 8) && pr.pan === -20 && pr.reverb === 90 && web1 === "DARK",
    `nf-new: the browser card (${card}): renamed ${pr && pr.name} (id ${pr && pr.id} kept), PAD on, web tag ${web1}; pan ${pr && pr.pan}, reverb ${pr && pr.reverb} kept`);
  await click("#l-bdup");
  await waitFor(`__mgr.S.brow.rows.length === ${b2.rows.length + 1}`, 5000);
  const b3 = await bdb(), dup = b3.rows.find((r) => r.name === "PANMIX R 2");
  const dsel = await evaluate("({ sel: __mgr.S.sel && __mgr.S.sel.slot, web: [...document.querySelectorAll('#l-sel [data-web]')].map((b) => b.dataset.web).join() })");
  check(dup && dup.pan === -20 && dup.reverb === 90 && (dup.tags & 8) && dup.id !== pr.id && dsel.sel === dup.slot && dsel.web === "DARK",
    `nf-new: DUPLICATE: ${dup && dup.name} at ${dup && dup.slot}, picked; pan ${dup && dup.pan}, reverb ${dup && dup.reverb}, its web tag ${dsel.web}`);
  // EDIT -> SAVE: back into the browser's store (asked), over the same place; nothing to the FM-1
  await click(`#l-list .cas[data-src='browser'][data-slot='${pm.slot}']`);
  await waitFor("!!document.getElementById('l-bedit')", 5000);
  await stores.on();
  await click("#l-bedit");
  const eOpen = await waitFor(`!document.getElementById('tab-edit').hidden && __mgr.editor.place().source === 'browser' && __mgr.editor.place().slot === ${pm.slot} && __mgr.editor.sound().pan === -20`, 5000);
  await evaluate("__mgr.editor.set(116, 33)");
  await click("#ed-save");
  await waitFor("!document.getElementById('cf').hidden", 3000);
  await click("#cf-go");
  await waitFor(`__mgr.S.brow.rows.some((r) => r.slot === ${pm.slot} && r.lib.sends.delay === 33)`, 8000);
  const b4 = await bdb(), ed = b4.rows.find((r) => r.slot === pm.slot), nst2 = await stores.off();
  check(eOpen && ed && ed.id === pm.id && ed.name === "PANMIX R" && ed.delay === 33 && ed.pan === -20 && ed.reverb === 90 && b4.rows.length === b3.rows.length && nst2 === 0 && (await mineRows()).length === m1,
    `nf-new: EDIT -> SAVE: back into IN THIS BROWSER ${pm.slot} (id kept, delay ${ed && ed.delay}, pan ${ed && ed.pan}, reverb ${ed && ed.reverb}); no STORE to the FM-1`);

  // SEND to the FM-1 and KEEP back: pan and sends with it each way; LOAD on the card opens the unit's browser on the copy sent
  await click("#t-library");
  await click(`#l-list .cas[data-src='browser'][data-slot='${dup.slot}']`);
  await waitFor("!!document.getElementById('l-send') && !document.getElementById('l-send').disabled", 8000);
  const m2 = await mineRows();
  await click("#l-send");
  await waitFor(`__mgr.S.unit.rows.filter((r) => r.source === 'mine').length === ${m2.length + 1}`, 15000);
  await idle();
  const sRow = (await mineRows()).find((r) => !m2.some((q) => q.id === r.id)), sD = sRow && (await mdump(sRow.slot));
  check(sD && sD.name === "PANMIX R 2" && sD.pan === -20 && sD.reverb === 90, `nf-new: SEND TO FM-1: MY PRESETS ${sRow && sRow.slot} has pan ${sD && sD.pan}, reverb ${sD && sD.reverb}`);
  await click("#l-filter [data-src='browser']");
  await click(`#l-list .cas[data-src='browser'][data-slot='${dup.slot}']`);
  await waitFor("!!document.getElementById('l-unit') && !document.getElementById('l-unit').disabled", 8000);
  await click("#l-unit");
  const lb = await waitFor(`!!__mgr.live.browsing() && __mgr.live.browsing().id === ${sRow ? sRow.id : -1}`, 8000);
  const m3 = (await mineRows()).length, lbw = await evaluate("JSON.stringify(__mgr.live.browsing()) + ' ' + document.getElementById('say').textContent");
  await evaluate("window.__sim.press('OCT-')");
  await waitFor("!__mgr.live.browsing()", 8000);
  check(lb && m3 === m2.length + 1, `nf-new: LOAD on the browser card: the FM-1's browser opened on MY PRESETS ${sRow && sRow.slot} (the copy sent), nothing sent again${lb && m3 === m2.length + 1 ? "" : ` (${lbw}; ${m3} MY PRESETS)`}`);
  await idle();
  await click("#l-filter [data-src='browser']");
  await click(`#l-list .cas[data-src='browser'][data-slot='${dup.slot}']`);
  await waitFor("!!document.getElementById('l-bdel')", 5000);
  await click("#l-bdel");
  await waitFor("!document.getElementById('cf').hidden", 3000);
  await click("#cf-go");
  await waitFor(`!__mgr.S.brow.rows.some((r) => r.name === 'PANMIX R 2')`, 5000);
  await click("#l-filter [data-src='mine']");
  await click(`#l-list .cas[data-src='mine'][data-slot='${sRow.slot}']`);
  await waitFor("!!document.getElementById('l-keep') && !document.getElementById('l-keep').disabled", 8000);
  await click("#l-keep");
  await waitFor("__mgr.S.brow.rows.some((r) => r.name === 'PANMIX R 2')", 8000);
  const kb = (await bdb()).rows.find((r) => r.name === "PANMIX R 2");
  check(kb && kb.pan === -20 && kb.reverb === 90, `nf-new: KEEP IN BROWSER from the FM-1: back in the browser's store, pan ${kb && kb.pan}, reverb ${kb && kb.reverb}`);

  // a phone: the card and the + NEW sheet, no horizontal scroll
  await click("#l-filter [data-src='browser']");
  await click(`#l-list .cas[data-src='browser'][data-slot='${pm.slot}']`);
  await size(390, 844, true);
  await sleep(500);
  const ph1 = await noScroll();
  await evaluate("document.getElementById('l-sel').scrollIntoView()");
  await sleep(300);
  await shot("manager-nfnew-phone-card.png");
  await evaluate("window.scrollTo(0, 0)");
  await click("#l-new");
  await sleep(400);
  const sh = await evaluate("(() => { const p = document.getElementById('l-newpop'), r = p.getBoundingClientRect(); return { sheet: p.classList.contains('sheet'), bottom: Math.round(r.bottom), h: innerHeight, l: Math.round(r.left), w: Math.round(r.width) }; })()");
  await shot("manager-nfnew-phone-new.png");
  const ph2 = await noScroll();
  await click("#l-newscrim");
  check(ph1 && ph2 && sh.sheet && Math.abs(sh.bottom - sh.h) < 2 && sh.l === 0 && sh.w === 390, `nf-new: 390 x 844: the browser card and + NEW's sheet (at the foot, ${sh.w} wide), no horizontal scroll`);
  await size(1280, 800);

  // RESET (THIS BROWSER's MEMORY) with the FM-1 the unit, then + NEW in the browser: a fresh store made for it
  await click("#unit");
  await click("#up-more");
  await waitFor("!!document.getElementById('up-reset')", 3000);
  await click("#up-reset");
  await waitFor("!document.getElementById('cf').hidden", 3000);
  await click("#cf-go");
  const rs = await waitFor("__mgr.S.brow.rows.length === 0 && __mgr.S.brow.img && __mgr.S.brow.img.length === 0", 10000);
  await click("#t-library");
  await click("#l-filter [data-src='browser']");
  await click("#l-new");
  await click("#l-newpop [data-new='fm']");
  await waitFor("!document.getElementById('tab-edit').hidden && __mgr.editor.place().to === 'browser'", 5000);
  await click("#ed-save");
  await waitFor("__mgr.editor.place().source === 'browser'", 8000);
  const b5 = await bdb();
  check(rs && b5.len > 0 && b5.rows.length === 1 && b5.rows[0].name === "INIT" && b5.rows[0].reverb === 20 && (await evaluate("__mgr.S.mode === 'fm1'")),
    `nf-new: RESET, then + NEW in the browser: a fresh store (${b5.len} B) with ${b5.rows.map((r) => r.name).join()} (reverb ${b5.rows[0] && b5.rows[0].reverb})`);
  await click("#t-library");
  }
  // ---- (end of nf-new)
  // ---- nf-askload: LOAD ON FM-1 of an IN THIS BROWSER preset the FM-1 lacks asks first (ADD · MY PRESETS n, the first
  // free place): CANCEL sends nothing, ADD stores it there and opens the unit's browser on it; one it has opens at once;
  // MY PRESETS full: FULL said, nothing asked or sent
  {
  await size(1440, 900);
  await send("Page.navigate", { url: URL0.replace(/v=\d+/, `v=${Date.now()}`) + "#library" });
  await waitFor("window.__mgr && __mgr.S.mode === 'fm1' && __mgr.S.unit && __mgr.S.unit.status && __mgr.S.unit.rows.length > 0", 40000);
  await idle();
  await click("#t-library");
  const mineRows = () => evaluate("__mgr.S.unit.rows.filter((r) => r.source === 'mine').map((r) => ({ slot: r.slot, id: r.id, name: r.name }))");
  const cmds = () => evaluate("(() => { const c = window.__sim.counts.cmds; return { store: (c[0x10] || 0) + (c[0x11] || 0), browse: c[0x31] || 0 }; })()");
  const firstFree = async () => { const u = new Set((await mineRows()).map((r) => r.slot)), n = await evaluate("__mgr.S.unit.status.nlib || 256"); for (let k = 1; k <= n; k++) if (!u.has(k)) return k; return 0; };
  const cfOpen = () => evaluate("!document.getElementById('cf').hidden");
  const cfText = () => evaluate("({ title: document.getElementById('cf-title').textContent, what: document.getElementById('cf-what').textContent, go: document.getElementById('cf-go').textContent })");
  const rename = async (slot, name) => {
    await evaluate(`(() => { const i = document.getElementById('l-bname'); i.value = ${JSON.stringify(name)}; i.dispatchEvent(new Event('change')); })()`);
    return waitFor(`__mgr.S.brow.rows.some((r) => r.slot === ${slot} && r.name === ${JSON.stringify(name)}) && !!document.getElementById('l-unit') && !document.getElementById('l-unit').disabled`, 8000);
  };
  // a browser preset: a MY preset kept in the browser, renamed (the FM-1 then has no copy of it)
  const m0 = await mineRows(), src = m0[0];
  await click("#l-filter [data-src='mine']");
  await click(`#l-list .cas[data-src='mine'][data-slot='${src.slot}']`);
  await waitFor("!!document.getElementById('l-keep') && !document.getElementById('l-keep').disabled", 8000);
  await click("#l-keep");
  await waitFor(`__mgr.S.brow.rows.some((r) => r.name === ${JSON.stringify(src.name)})`, 8000);
  await idle();
  const bslot = await evaluate(`__mgr.S.brow.rows.find((r) => r.name === ${JSON.stringify(src.name)}).slot`);
  await click("#l-filter [data-src='browser']");
  await click(`#l-list .cas[data-src='browser'][data-slot='${bslot}']`);
  await waitFor("!!document.getElementById('l-bname')", 5000);
  const rn1 = await rename(bslot, "ASKLOAD");
  // 1. LOAD: asked, the slot and the name; CANCEL: nothing sent, nothing opened
  const want = await firstFree(), c0 = await cmds();
  await click("#l-unit");
  const asked = await waitFor("!document.getElementById('cf').hidden", 8000);
  const t1 = await cfText();
  await shot("manager-nfaskload-confirm.png");
  await click("#cf-no");
  await sleep(800);
  await idle();
  const c1 = await cmds(), m1 = await mineRows(), br1 = await evaluate("!!__mgr.live.browsing()");
  check(rn1 && asked && t1.title === `ADD · MY PRESETS ${want}` && t1.what === "ASKLOAD" && /^ADD/.test(t1.go) && m1.length === m0.length && c1.store === c0.store && c1.browse === c0.browse && !br1,
    `nf-askload: LOAD of a browser preset the FM-1 lacks asks "${t1.title}" (${t1.what}, ${t1.go}); CANCEL: MY PRESETS ${m1.length} (was ${m0.length}), STORE +${c1.store - c0.store}, BROWSE +${c1.browse - c0.browse}, browser ${br1 ? "open" : "closed"}`);
  // 2. ADD: stored in that very slot, the unit's browser opened on it
  await click("#l-unit");
  await waitFor("!document.getElementById('cf').hidden", 8000);
  await click("#cf-go");
  const added = await waitFor(`__mgr.S.unit.rows.some((r) => r.source === 'mine' && r.slot === ${want} && r.name === 'ASKLOAD')`, 15000);
  const nr = await evaluate(`(__mgr.S.unit.rows.find((r) => r.source === 'mine' && r.slot === ${want} && r.name === 'ASKLOAD') || {}).id || -1`);
  const op2 = await waitFor(`!!__mgr.live.browsing() && __mgr.live.browsing().id === ${nr}`, 8000);
  const c2 = await cmds(), m2 = await mineRows();
  check(added && op2 && m2.length === m0.length + 1 && c2.store === c1.store + 1 && c2.browse === c1.browse + 1,
    `nf-askload: ADD: ASKLOAD in MY PRESETS ${want} (${m2.length} MY PRESETS, STORE +${c2.store - c1.store}), the FM-1's browser on it (BROWSE +${c2.browse - c1.browse})`);
  await evaluate("window.__sim.press('OCT-')");
  await waitFor("!__mgr.live.browsing()", 8000);
  await idle();
  // 3. one the FM-1 has: LOAD opens it at once, nothing asked, nothing sent
  await click("#l-filter [data-src='browser']");
  await click(`#l-list .cas[data-src='browser'][data-slot='${bslot}']`);
  await waitFor("!!document.getElementById('l-unit') && !document.getElementById('l-unit').disabled", 8000);
  await click("#l-unit");
  const op3 = await waitFor(`!!__mgr.live.browsing() && __mgr.live.browsing().id === ${nr}`, 8000);
  const cf3 = await cfOpen(), c3 = await cmds();
  check(op3 && !cf3 && c3.store === c2.store && c3.browse === c2.browse + 1 && (await mineRows()).length === m2.length,
    `nf-askload: LOAD of one the FM-1 has: its browser on MY PRESETS ${want} at once (${cf3 ? "asked" : "nothing asked"}, STORE +${c3.store - c2.store})`);
  await evaluate("window.__sim.press('OCT-')");
  await waitFor("!__mgr.live.browsing()", 8000);
  await idle();
  // 4. MY PRESETS full (its places stubbed to the ones used from 1): FULL said, nothing asked or sent
  await click("#l-filter [data-src='browser']");
  await click(`#l-list .cas[data-src='browser'][data-slot='${bslot}']`);
  await waitFor("!!document.getElementById('l-bname')", 5000);
  const rn4 = await rename(bslot, "ASKFULL");
  const ff = await firstFree(), nlib0 = await evaluate("__mgr.S.unit.status.nlib");
  await evaluate(`__mgr.S.unit.status.nlib = ${ff - 1}`);
  const c4 = await cmds(), m4 = await mineRows();
  await click("#l-unit");
  const full = await waitFor("!document.getElementById('toast').hidden && document.getElementById('toast').textContent === 'MY PRESETS is FULL'", 8000);
  await sleep(500);
  const cf4 = await cfOpen(), c5 = await cmds(), br4 = await evaluate("!!__mgr.live.browsing()");
  await evaluate(`__mgr.S.unit.status.nlib = ${JSON.stringify(nlib0)}`);
  check(rn4 && ff > 1 && full && !cf4 && !br4 && c5.store === c4.store && c5.browse === c4.browse && (await mineRows()).length === m4.length,
    `nf-askload: MY PRESETS full (${ff - 1} places): LOAD says FULL, ${cf4 ? "asked" : "nothing asked"}, STORE +${c5.store - c4.store}, BROWSE +${c5.browse - c4.browse}`);
  await size(1280, 800);
  }
  // ---- (end of nf-askload)
  // ---- nf-units: two FM-1s in the picker, PORTS
  // (?sim=two: "FIZZ FM-1" (USB) and "FM-1 C1F0 Bluetooth" running FIZZ, "FM-1" running RemoteFM1, "FM1 Update" in
  // its loader, "IAC Driver Bus 1" and "Roto-Control" never asked. Each FIZZ its own row named by its port, the one in
  // use checked; the others greyed with their firmware, not to pick; picking the second talks to it alone (its replies:
  // its own presets; the first sent nothing meanwhile); PORTS from the picker and from LOG, every port with what
  // answered; the second unplugged: its row gone at once, the first still the unit; a phone: no horizontal scroll)
  {
  const SHOTS = process.env.NF_UNITS_SHOTS || "";
  const keep = async (name) => { await shot(`manager-${name}.png`); if (SHOTS) fs.copyFileSync(path.join(OUT, `manager-${name}.png`), path.join(SHOTS, `${name}.png`)); };
  await size(1440, 900);
  await send("Page.navigate", { url: URL0.replace("?sim&", "?sim=two&") + "#library" });
  const two = await waitFor("window.__mgr && __mgr.S.mode === 'fm1' && !!__mgr.S.link && __mgr.S.unit && __mgr.S.unit.rows.length > 0 && !!window.__simTwo", 60000);
  await idle();
  const rowsOf = () => evaluate(`[...document.querySelectorAll('#upick-pop .urow[data-port]')].map((r) => ({ port: r.dataset.port, u: r.dataset.u || '',
    via: ((r.querySelector('.usb') || {}).textContent || '').trim(), fw: ((r.querySelector('.fwchip') || r.querySelector('.nm .hud')) || {}).textContent || '',
    on: r.classList.contains('on') && !!r.querySelector('.radio.y'), grey: r.classList.contains('nofizz') && r.getAttribute('aria-disabled') === 'true',
    shown: r.offsetWidth > 100 }))`);
  const byPort = (rs) => Object.fromEntries(rs.map((r) => [r.port, r]));
  await realClick("#unit");
  const up4 = await waitFor("document.querySelectorAll('#upick-pop .urow[data-port]').length === 4", 10000);
  await sleep(300);
  const r1 = await rowsOf(), b1 = byPort(r1), n1 = await evaluate("document.querySelector('#upick-pop .uphead .hud').textContent");
  check(two && up4 && r1.length === 4 && r1.every((r) => r.shown) && n1 === "5" &&
    b1["FIZZ FM-1"].u === "fm1" && b1["FIZZ FM-1"].via === "USB" && b1["FIZZ FM-1"].on && /^FIZZ /.test(b1["FIZZ FM-1"].fw) &&
    b1["FM-1 C1F0 Bluetooth"].u === "fm1" && b1["FM-1 C1F0 Bluetooth"].via === "BLUETOOTH" && !b1["FM-1 C1F0 Bluetooth"].on && b1["FM-1 C1F0 Bluetooth"].fw === "FIZZ 0.12.0",
    `nf-units: the picker lists each FM-1 by its port: ${r1.map((r) => `${r.port} ${r.via} ${r.fw}${r.on ? " (checked)" : ""}${r.grey ? " (greyed)" : ""}`).join(" | ")}; UNIT ${n1}`);
  await keep("units-desktop-two-fm1");
  // the greyed rows: named by their firmware, a click does nothing
  check(b1["FM-1"] && b1["FM-1"].grey && b1["FM-1"].fw === "REMOTEFM1" && b1["FM1 Update"] && b1["FM1 Update"].grey && b1["FM1 Update"].fw === "UPDATE MODE" && !b1["FM-1"].u,
    `nf-units: the ports not running FIZZ greyed with their firmware: FM-1 ${b1["FM-1"] && b1["FM-1"].fw}, FM1 Update ${b1["FM1 Update"] && b1["FM1 Update"].fw}`);
  await realClick("#upick-pop .urow[data-port='FM-1']");
  await realClick("#upick-pop .urow[data-port='FM1 Update']");
  await sleep(800);
  check(await evaluate("!document.getElementById('upick').hidden && __mgr.S.mode === 'fm1' && __mgr.S.link && __mgr.S.link.port === 'FIZZ FM-1' && !document.getElementById('logbox').open"),
    "nf-units: a greyed row can't be picked (the picker stays, the unit unchanged)");
  // PORTS from the picker: every port, what answered
  await realClick("#up-ports");
  const pv = await waitFor("document.getElementById('logbox').open && !document.getElementById('lp-view').hidden && document.querySelectorAll('#lp-view .prow:not(.hd)').length === 6 && document.getElementById('upick').hidden", 8000);
  await sleep(1200);
  const ptab = () => evaluate(`({ rows: [...document.querySelectorAll('#lp-view .prow:not(.hd)')].map((r) => ({ name: r.dataset.port, cls: r.className.replace('prow', '').trim(),
      io: r.querySelectorAll('.io:not(.no)').length, asked: r.querySelector('.ask').classList.contains('y'), via: ((r.querySelector('.usb') || {}).textContent || '').trim(),
      ans: (r.querySelector('.ans').innerText || '').replace(/\\s+/g, ' ').trim() })),
    stats: [...document.querySelectorAll('#lp-view .pstat')].map((s) => s.innerText.replace(/\\s+/g, ' ').trim()).join(), seg: document.querySelector('#lpanel .lpseg .on').textContent,
    when: document.getElementById('lp-when').textContent, fits: document.getElementById('lpanel').getBoundingClientRect().right <= innerWidth })`);
  const t1 = await ptab(), pb = Object.fromEntries(t1.rows.map((r) => [r.name, r]));
  check(pv && t1.seg === "PORTS" && t1.rows.length === 6 && t1.rows.every((r) => r.io === 2) && t1.stats === "6 IN,6 OUT,4 ASKED,2 FIZZ" && /^\d\d:\d\d:\d\d$/.test(t1.when) && t1.fits &&
    /FM-1_46\d+ FIZZ/.test(pb["FIZZ FM-1"].ans) && pb["FIZZ FM-1"].cls.includes("fizz") && pb["FIZZ FM-1"].via === "USB" &&
    /FM-1_46001200 FIZZ 0\.12\.0/.test(pb["FM-1 C1F0 Bluetooth"].ans) && pb["FM-1 C1F0 Bluetooth"].via === "BLUETOOTH" &&
    /FM-1_52010200 REMOTEFM1/.test(pb["FM-1"].ans) && pb["FM-1"].cls === "other" && /ota-FM1_0001 UPDATE MODE/.test(pb["FM1 Update"].ans) &&
    !pb["IAC Driver Bus 1"].asked && pb["IAC Driver Bus 1"].ans === "—" && pb["IAC Driver Bus 1"].cls === "quiet" && !pb["Roto-Control"].asked &&
    ["FIZZ FM-1", "FM-1 C1F0 Bluetooth", "FM-1", "FM1 Update"].every((n) => pb[n].asked),
    `nf-units: PORTS from the picker (${t1.stats}): ${t1.rows.map((r) => `${r.name}: ${r.asked ? r.ans : "not asked"}`).join(" | ")}`);
  await keep("ports-desktop");
  // PORTS from the LOG: LOG first, then its PORTS
  await click("#lp-log");
  await realClick("#logbox summary");
  await waitFor("!document.getElementById('logbox').open", 2000);
  await realClick("#logbox summary");
  const lg = await waitFor("document.getElementById('logbox').open && !document.getElementById('log').hidden && document.getElementById('lp-view').hidden && document.getElementById('log').textContent.length > 0", 3000);
  await realClick("#lp-ports");
  const lp = await waitFor("!document.getElementById('lp-view').hidden && document.getElementById('log').hidden && document.querySelectorAll('#lp-view .prow:not(.hd)').length === 6", 3000);
  check(lg && lp, "nf-units: the LOG opens on its lines; its PORTS shows the same table");
  await realClick("#logbox summary");
  // picking the second: that one answers (its own presets), the first is sent nothing meanwhile; the header follows
  const cmds = (k) => `Object.values(__simTwo.all[${k}].counts.cmds).reduce((x, y) => x + y, 0)`;
  await realClick("#unit");
  await waitFor("document.querySelectorAll('#upick-pop .urow[data-port]').length === 4", 5000);
  const a0 = await evaluate(cmds(0)), bb0 = await evaluate(cmds(1));
  await realClick("#upick-pop .urow[data-port='FM-1 C1F0 Bluetooth']");
  const sw = await waitFor("__mgr.S.mode === 'fm1' && __mgr.S.link && __mgr.S.link.port === 'FM-1 C1F0 Bluetooth' && __mgr.S.unit && __mgr.S.unit.status && __mgr.S.unit.rows.length > 0", 40000);
  await idle();
  await sleep(600);
  await evaluate("__mgr.refreshRows()");
  await idle();
  const s2 = await evaluate(`({ id: __mgr.S.unit.id, mine: __mgr.S.unit.rows.filter((r) => r.source === 'mine').map((r) => r.name.trim()).join(),
    pill: (document.querySelector('#unit small') || {}).textContent || '', a: ${cmds(0)}, b: ${cmds(1)}, lib: document.getElementById('tab-library').innerText })`);
  check(sw && s2.id === "FM-1_46001200" && s2.mine === "BEE BASS,MOTH PAD,WASP LEAD" && /BEE BASS/.test(s2.lib) && !/SUB DROP/.test(s2.lib) && s2.a === a0 && s2.b > bb0 && / · BLUETOOTH$/.test(s2.pill),
    `nf-units: the second picked: its replies (${s2.id}, MY PRESETS ${s2.mine}), the first sent nothing (${s2.a - a0} commands; the second ${s2.b - bb0}); the pill "${s2.pill}"`);
  await realClick("#unit");
  await sleep(400);
  const r2 = byPort(await rowsOf());
  check(r2["FM-1 C1F0 Bluetooth"].on && !r2["FIZZ FM-1"].on, "nf-units: the picker marks the second as the one in use");
  // back on the first; then the second unplugged: its row gone at once (the picker open), PORTS too, the first kept
  await realClick("#upick-pop .urow[data-port='FIZZ FM-1']");
  const back = await waitFor("__mgr.S.link && __mgr.S.link.port === 'FIZZ FM-1' && __mgr.S.unit && __mgr.S.unit.status && __mgr.S.unit.rows.length > 0", 40000);
  await idle();
  // a phone: the sheet and PORTS, no horizontal scroll
  await size(390, 844, true);
  await sleep(500);
  await click("#unit");
  await sleep(600);
  const ph1 = await evaluate("({ sheet: document.getElementById('upick').classList.contains('sheet'), n: document.querySelectorAll('#upick-pop .urow').length, w: document.getElementById('upick-pop').scrollWidth <= document.getElementById('upick-pop').clientWidth + 1 })");
  const ns1 = await noScroll();
  await keep("units-phone-two-fm1");
  await click("#up-ports");
  await sleep(900);
  const ph2 = await evaluate("(() => { const p = document.getElementById('lpanel').getBoundingClientRect(); return { l: p.left, r: p.right, w: innerWidth, rows: document.querySelectorAll('#lp-view .prow:not(.hd)').length, over: [...document.querySelectorAll('#lp-view .prow')].some((x) => x.scrollWidth > x.clientWidth + 1) }; })()");
  const ns2 = await noScroll();
  await keep("ports-phone");
  await click("#logbox summary");
  check(back && ph1.sheet && ph1.n === 5 && ph1.w && ns1 && ph2.l >= 0 && ph2.r <= ph2.w && ph2.rows === 6 && !ph2.over && ns2,
    `nf-units: a phone (390 x 844): the picker a sheet with ${ph1.n} rows, PORTS within the screen (${Math.round(ph2.l)}..${Math.round(ph2.r)}), no horizontal scroll`);
  await size(1440, 900);
  await sleep(400);
  await realClick("#unit");
  await waitFor("document.querySelectorAll('#upick-pop .urow[data-port]').length === 4", 5000);
  await evaluate("__simTwo.unplug(1)");
  const gone = await waitFor("document.querySelectorAll('#upick-pop .urow[data-port]').length === 3 && !document.querySelector(\"#upick-pop .urow[data-port='FM-1 C1F0 Bluetooth']\")", 3000);
  await sleep(1500);
  const kept = await evaluate("__mgr.S.mode === 'fm1' && __mgr.S.link && __mgr.S.link.port === 'FIZZ FM-1' && !!document.querySelector(\"#upick-pop .urow[data-port='FIZZ FM-1'].on\")");
  await click("#upick-scrim");
  await click("#lp-ports");
  if (!(await evaluate("document.getElementById('logbox').open"))) await realClick("#logbox summary");
  await realClick("#lp-ports");
  await sleep(500);
  const t3 = await ptab();
  check(gone && kept && t3.rows.length === 5 && !t3.rows.some((r) => r.name === "FM-1 C1F0 Bluetooth") && t3.stats === "5 IN,5 OUT,3 ASKED,1 FIZZ",
    `nf-units: the second unplugged: its row gone at once (picker and PORTS: ${t3.stats}), the first still the unit`);
  // plugged in again: asked again, its row back
  await evaluate("__simTwo.plug(1)");
  const again = await waitFor("document.querySelectorAll('#lp-view .prow.fizz').length === 2", 8000);
  check(again, "nf-units: plugged in again: asked again, back in PORTS as FIZZ");
  await realClick("#logbox summary");
  }
  // ---- (end of nf-units)
  // ---- nf-proj2: the owner's fixes (2026-10-29), on ?sim (the FM-1) and ?browser (THIS BROWSER's FIZZ): LIBRARY's
  // first CTRL / CMD-click adds to the one preset selected; PROJECTS: an empty pattern, one with no trigs and one with
  // only ghosts (LOCK steps) drag (an empty one dropped clears the target, asked first; onto an empty one nothing);
  // CLEAR at once, dashed, in the yellow bar; DELETE and BACKSPACE clear the selection (not while typing); CTRL+X and
  // CMD+X cut (copied, the source cleared as a change waiting), CMD / CTRL+V pastes it; a press let go without moving
  // (or under 4 px) is a click, not a drag; a drag's ghost keeps the grabbed point under the pointer (within 1 px)
  {
  const SHOTS = process.env.NF_PROJ2_SHOTS || "";
  const keep = async (name) => { await shot(`manager-${name}.png`); if (SHOTS) fs.copyFileSync(path.join(OUT, `manager-${name}.png`), path.join(SHOTS, `${name}.png`)); };
  const PJ = "__mgr.projtab.state";
  const MOD = { alt: 1, ctrl: 2, cmd: 4, shift: 8 };
  const key = async (k, code, vk, modifiers = 0) => {
    await send("Input.dispatchKeyEvent", { type: "rawKeyDown", key: k, code, windowsVirtualKeyCode: vk, modifiers });
    await send("Input.dispatchKeyEvent", { type: "keyUp", key: k, code, windowsVirtualKeyCode: vk, modifiers });
    await sleep(120);
  };
  async function modClick(sel, modifiers) {
    const c = await centre(sel);
    if (!c) return false;
    await mouse("mousePressed", c.x, c.y, { modifiers });
    await mouse("mouseReleased", c.x, c.y, { modifiers });
    await sleep(150);
    return true;
  }
  // a pattern record made here ('P' 4: no references; tracks' sounds copies of zeros): steps on track 1's first page
  // (s: [step, flags, type, note]), nlock locks, used
  const recJs = `(steps, nlock, used) => { const TSZ = 44, head = 12 + TSZ * 6 + 24, per = [], snd = 6 * (128 + 2 + 13 + 2);
    let sm = 0; for (const s of steps) sm |= 1 << s[0];
    const st = steps.length ? 1 + 2 + steps.length * 20 : 1, b = new Uint8Array(head + st + 5 + nlock * 4 + snd);
    b.set([0x50, 4, 20, TSZ, 0, 0, 0, used ? 1 : 0, 120, 0, nlock, 0]);
    let o = head;
    if (steps.length) { b[o++] = 1; b[o++] = sm & 255; b[o++] = sm >> 8;
      for (const s of [...steps].sort((x, y) => x[0] - y[0])) { b[o + 2] = s[1]; b[o + 3] = s[2]; b[o + 4] = s[3]; b[o + 10] = 100; o += 20; } }
    else b[o++] = 0;
    o += 5;
    for (let i = 0; i < nlock; i++) { b[o] = steps.length ? steps[i % steps.length][0] : 0; b[o + 1] = 3; b[o + 2] = 40; o += 4; }
    return b; }`;
  async function nfProj2(mode) {
    const tag = `nf-proj2 (${mode})`;
    await size(1440, 900);
    await send("Page.navigate", { url: URL0.replace(/v=\d+/, `v=${Date.now()}`) + "#library" });
    await waitFor("window.__mgr && __mgr.S.mode === 'fm1' && __mgr.S.unit && __mgr.S.unit.status && __mgr.S.unit.rows.length > 0", 40000);
    await idle();
    if (mode === "browser") {
      await realClick("#unit");
      await waitFor("!document.getElementById('upick').hidden", 3000);
      await realClick("#upick-pop .urow[data-u='browser']");
      await waitFor("__mgr.S.mode === 'browser' && !!__mgr.S.link && !!__mgr.S.unit && __mgr.S.unit.rows.length > 0", 40000);
      await idle();
    }
    // ---- LIBRARY: one preset selected, the first CTRL-click (then CMD-click) adds to it
    await click("#t-library");
    await sleep(300);
    const cards = await evaluate("[...document.querySelectorAll('#l-list .cas')].slice(0, 4).map((c) => `${c.dataset.src}:${c.dataset.slot}`)");
    const cas = (k) => `#l-list .cas[data-src="${k.split(":")[0]}"][data-slot="${k.split(":")[1]}"]`;
    const picked = () => evaluate("[...__mgr.S.pick.keys].sort().join()");
    const lib = [];
    for (const [m, name] of [[MOD.ctrl, "CTRL"], [MOD.cmd, "CMD"]]) {
      await evaluate("(() => { const P = __mgr.S.pick; P.mode = false; P.keys.clear(); P.anchor = ''; })()");
      await realClick(cas(cards[0]));
      await sleep(200);
      const sel = await evaluate("__mgr.S.sel && `${__mgr.S.sel.source}:${__mgr.S.sel.slot}`");
      await modClick(cas(cards[1]), m);
      const one = await picked();
      await modClick(cas(cards[2]), m);
      const two = await picked();
      lib.push({ name, sel, one, two });
    }
    const want = (n) => cards.slice(0, n).sort().join();
    check(cards.length >= 3 && lib.every((x) => x.sel === cards[0] && x.one === want(2) && x.two === want(3)),
      `${tag}: LIBRARY: one preset selected, the first CTRL-click and the first CMD-click add to it (${JSON.stringify(lib.map((x) => [x.name, x.one, x.two]))})`);
    await evaluate("(() => { const P = __mgr.S.pick; P.mode = false; P.keys.clear(); P.anchor = ''; })()");
    // ---- PROJECTS: a new project in slot 2 with A01 notes, A02 only ghosts, A03 no trigs (a record), A04 nothing
    await click("#t-projects");
    await waitFor(`!!${PJ}.base && !__mgr.S.busy`, 60000);
    await evaluate(`(async () => { await __mgr.S.link.project('new', 2); ${PJ}.base = null; await __mgr.refreshRows({ projects: true }); await __mgr.projtab.show(); })()`);
    await waitFor(`!!${PJ}.base && !__mgr.S.busy && !${PJ}.reading`, 60000);
    await idle();
    const made = await evaluate(`(async () => { const P = await import('./projects.js'), rec = ${recJs}, m = ${PJ}.base.model;
      for (const k of [...m.prj.keys()]) if (k >>> 24 === P.RK.PAT && ((k >>> 16) & 255) === 1) m.prj.delete(k);   // (slot 2 as made here only)
      m.prj.set(P.rkey(P.RK.PAT, 1, 0), rec([[0, 1, 1, 60], [4, 1, 1, 62], [8, 1, 1, 64]], 0, 1));
      m.prj.set(P.rkey(P.RK.PAT, 1, 1), rec([[2, 0x81, 2, 0], [6, 0x81, 2, 0]], 2, 1));
      m.prj.set(P.rkey(P.RK.PAT, 1, 2), rec([], 0, 0));
      ${PJ}.sel = 1; ${PJ}.view = 'patterns'; ${PJ}.pats = new Set(); ${PJ}.ops = []; ${PJ}.cache = null; __mgr.projtab.render();
      const pr = P.project(m, 1), w = (k) => P.walkPat(m.prj.get(P.rkey(P.RK.PAT, 1, k)));
      return { there: pr.there, a1: !!pr.pats[0], ghost: w(1) && w(1).trigs === 2 && w(1).locks.length === 6, quiet: pr.pats[2] === null && !!w(2), none: !m.prj.has(P.rkey(P.RK.PAT, 1, 3)) }; })()`);
    const recOf = (k) => `(async () => { const P = await import('./projects.js'); const m = ${PJ}.cache || ${PJ}.base.model; const b = m.prj.get(P.rkey(P.RK.PAT, 1, ${k})); return b ? Array.from(b).join() : ""; })()`;
    const nops = () => evaluate(`${PJ}.ops.length`);
    const ghostOn = () => evaluate("(() => { const g = document.querySelector('.pjghost'); return !!g && !g.hidden; })()");
    async function dragOnto(k, target, { modifiers = 0 } = {}) {
      const a = await centre(`#pj-grid .pt[data-k="${k}"]`), t = await centre(target);
      if (!a || !t) return false;
      await mouse("mousePressed", a.x, a.y, { buttons: 1, modifiers });
      for (let i = 1; i <= 6; i++) { await mouse("mouseMoved", a.x + (t.x - a.x) * i / 6, a.y + (t.y - a.y) * i / 6, { buttons: 1, modifiers }); await sleep(20); }
      const lifted = await ghostOn();
      await mouse("mouseReleased", t.x, t.y, { modifiers });
      await sleep(250);
      return lifted;
    }
    const tile = (k) => `#pj-grid .pt[data-k="${k}"]`;
    const r2 = await evaluate(recOf(2)), r1 = await evaluate(recOf(1)), r0 = await evaluate(recOf(0));
    // the one with no trigs (A03 -> A11) and the ghosts' (A02 -> A12): lifted, copied as they are
    const l2 = await dragOnto(2, tile(10)), l1 = await dragOnto(1, tile(11));
    check(made.there && made.a1 && made.ghost && made.quiet && made.none && l2 && l1 && (await nops()) === 2 && (await evaluate(recOf(10))) === r2 && (await evaluate(recOf(11))) === r1,
      `${tag}: PROJECTS: a pattern with no trigs (A03) and one with only ghosts (A02) drag: copied to A11, A12 (${JSON.stringify(made)})`);
    // an empty one (A04, no record): lifted; onto an empty one nothing; onto A01 (notes): asked, then A01 cleared, dashed
    const l3 = await dragOnto(3, tile(12));
    const nothing = (await nops()) === 2;
    await dragOnto(3, tile(0));
    const cf = await evaluate("({ open: !document.getElementById('cf').hidden, t: document.getElementById('cf-title').textContent })");
    await click("#cf-go");
    await sleep(250);
    const e0 = await evaluate(`({ n: ${PJ}.ops.length, cl: document.querySelector('${tile(0)}').classList.contains('cleared') })`);
    check(r0 && l3 && nothing && cf.open && cf.t === "COPY OVER" && e0.n === 3 && (await evaluate(recOf(0))) === "" && e0.cl,
      `${tag}: PROJECTS: an empty pattern (A04) drags: onto an empty one nothing changes; onto A01 asked (${cf.t}), A01 cleared, dashed (${e0.n} waiting)`);
    await evaluate(`${PJ}.ops = []; ${PJ}.cache = null; ${PJ}.pats = new Set(); __mgr.projtab.render()`);
    // CLEAR: at once (no question), the tile dashed yellow, the yellow bar
    await realClick(tile(1));
    await sleep(150);
    await click('#pj-tools [data-act="clear"]');
    await sleep(100);
    const c1 = await evaluate(`(() => { const t = document.querySelector('${tile(1)}'), cs = getComputedStyle(t); return { n: ${PJ}.ops.length, cf: !document.getElementById('cf').hidden, cl: t.classList.contains('cleared'),
      dash: cs.borderTopStyle, bar: !document.getElementById('pj-pending').hidden && /1 cleared in/.test(document.querySelector('#pj-pending .what').textContent) }; })()`);
    check(c1.n === 1 && !c1.cf && c1.cl && c1.dash === "dashed" && c1.bar && (await evaluate(recOf(1))) === "",
      `${tag}: PROJECTS: CLEAR shows at once: A02 dashed (${c1.dash}), in the yellow bar, no question (${JSON.stringify(c1)})`);
    if (mode === "fm1") { await keep("pj-cleared-1440"); await size(390, 844, true); await evaluate(`__mgr.projtab.render(); document.querySelector('${tile(1)}').scrollIntoView({ block: "center" })`); await sleep(2500); await keep("pj-cleared-390"); await size(1440, 900); await sleep(200); }
    await click('#pj-pending [data-act="undo"]');
    // DELETE and BACKSPACE clear the selection; not while typing in a field
    await realClick(tile(0));
    await sleep(150);
    await key("Delete", "Delete", 46);
    const d1 = await evaluate(`(() => { const o = ${PJ}.ops[${PJ}.ops.length - 1]; return { n: ${PJ}.ops.length, kind: o && o.kind, ks: o && o.ks.join() }; })()`);
    await click('#pj-pending [data-act="undo"]');
    await evaluate(`${PJ}.pats = new Set([0, 2]); __mgr.projtab.render()`);
    await key("Backspace", "Backspace", 8);
    const d2 = await evaluate(`(() => { const o = ${PJ}.ops[${PJ}.ops.length - 1]; return { n: ${PJ}.ops.length, kind: o && o.kind, ks: o && o.ks.join() }; })()`);
    await click('#pj-pending [data-act="undo"]');
    await evaluate("(() => { const i = document.createElement('input'); i.id = 'nfp2-typing'; document.getElementById('tab-projects').appendChild(i); i.focus(); })()");
    await key("Backspace", "Backspace", 8);
    await key("Delete", "Delete", 46);
    const typed = await nops();
    await evaluate("document.getElementById('nfp2-typing').remove()");
    check(d1.n === 1 && d1.kind === "clear" && d1.ks === "0" && d2.n === 1 && d2.ks === "0,2" && typed === 0,
      `${tag}: PROJECTS: DELETE clears A01 (${d1.ks}), BACKSPACE A01 + A03 (${d2.ks}); typing in a field clears nothing`);
    // CTRL+X and CMD+X: copied and the source cleared at once (dashed, waiting); CMD+V / CTRL+V pastes it
    const cut = [];
    for (const [m, name] of [[MOD.ctrl, "CTRL"], [MOD.cmd, "CMD"]]) {
      await evaluate(`${PJ}.ops = []; ${PJ}.cache = null; ${PJ}.clip = null; ${PJ}.pats = new Set([1]); __mgr.projtab.render()`);
      await key("x", "KeyX", 88, m);
      const x = await evaluate(`(() => { const o = ${PJ}.ops[0]; return { n: ${PJ}.ops.length, cut: !!(o && o.cut), clip: ${PJ}.clip ? ${PJ}.clip.pats.map((p) => p.k).join() : '',
        cl: document.querySelector('${tile(1)}').classList.contains('cleared'), bar: /1 cut from/.test(document.getElementById('pj-pending').textContent) }; })()`);
      x.gone = (await evaluate(recOf(1))) === "";
      await evaluate(`${PJ}.pats = new Set([20]); __mgr.projtab.render()`);
      await key("v", "KeyV", 86, m === MOD.ctrl ? MOD.cmd : MOD.ctrl);
      x.pasted = (await evaluate(recOf(20))) === r1 && (await nops()) === 2;
      cut.push({ name, ...x });
    }
    check(cut.every((x) => x.n === 1 && x.cut && x.clip === "1" && x.cl && x.bar && x.gone && x.pasted),
      `${tag}: PROJECTS: CTRL+X and CMD+X cut A02 (copied, cleared at once, dashed, waiting), then CMD+V / CTRL+V pastes it at A21 (${JSON.stringify(cut)})`);
    await evaluate(`${PJ}.ops = []; ${PJ}.cache = null; ${PJ}.clip = null; ${PJ}.pats = new Set(); __mgr.projtab.render()`);
    // a press let go without moving, and one moved 3 px: a click (selects), no ghost, nothing waiting
    const a = await centre(tile(0));
    await mouse("mousePressed", a.x, a.y, { buttons: 1 });
    await sleep(60);
    const g0 = await ghostOn();
    await mouse("mouseReleased", a.x, a.y);
    await sleep(150);
    const s0 = await evaluate(`[...${PJ}.pats].join()`);
    await mouse("mousePressed", a.x, a.y, { buttons: 1 });
    await mouse("mouseMoved", a.x + 3, a.y, { buttons: 1 });
    await sleep(60);
    const g3 = await ghostOn();
    await mouse("mouseReleased", a.x + 3, a.y);
    await sleep(150);
    const s3 = await evaluate(`[...${PJ}.pats].join()`);
    check(!g0 && !g3 && s0 === "0" && s3 === "" && (await nops()) === 0,
      `${tag}: PROJECTS: a press let go without moving selects A01 (${s0}), one moved 3 px is a click too (${s3 || "none"}): no ghost, no drag`);
    // CTRL-click and CMD-click add a pattern (a Mac's CTRL-click: the context menu, taken as one)
    await realClick(tile(0));
    await sleep(150);
    await modClick(tile(2), MOD.ctrl);
    await modClick(tile(5), MOD.cmd);
    const cc = await evaluate(`[...${PJ}.pats].sort((a, b) => a - b).join()`);
    check(cc === "0,2,5", `${tag}: PROJECTS: A01, then CTRL-click A03 and CMD-click A06: ${cc}`);
    await evaluate(`${PJ}.pats = new Set(); __mgr.projtab.render()`);
    // the ghost at the drag's start, and far away: the grabbed point under the pointer (1 px)
    const r = await evaluate(`(() => { const t = document.querySelector('${tile(1)}'); t.scrollIntoView({ block: "center" }); const r = t.getBoundingClientRect(); return { l: r.left, t: r.top, w: r.width, h: r.height }; })()`);
    const gx = r.l + 9, gy = r.t + 7;
    const under = (px, py) => evaluate(`(() => { const g = document.querySelector('.pjghost'); if (!g || g.hidden) return null; const r = g.getBoundingClientRect(), s = r.width / g.offsetWidth;
      return { x: (${px} - r.left) / s, y: (${py} - r.top) / s, s }; })()`);
    await mouse("mousePressed", gx, gy, { buttons: 1 });
    await mouse("mouseMoved", gx + 2, gy + 1, { buttons: 1 });
    const early = await ghostOn();
    await mouse("mouseMoved", gx + 5, gy + 2, { buttons: 1 });
    await sleep(300);                                  // (the lift's scale animation done)
    const u1 = await under(gx + 5, gy + 2);
    await mouse("mouseMoved", gx + 300, gy + 120, { buttons: 1 });
    await sleep(100);
    const u2 = await under(gx + 300, gy + 120);
    await key("Escape", "Escape", 27);
    await mouse("mouseReleased", gx + 300, gy + 120);
    await sleep(150);
    const near = (u) => !!u && Math.abs(u.x - 9) <= 1 && Math.abs(u.y - 7) <= 1;
    check(!early && near(u1) && near(u2) && (await nops()) === 0,
      `${tag}: PROJECTS: no ghost before 4 px; on lift and 300 px on the grabbed point (9, 7) stays under the pointer (${u1 && `${u1.x.toFixed(1)}, ${u1.y.toFixed(1)}`}; ${u2 && `${u2.x.toFixed(1)}, ${u2.y.toFixed(1)}`}; scale ${u1 && u1.s.toFixed(2)})`);
    if (mode === "fm1") {
      await mouse("mousePressed", gx, gy, { buttons: 1 });
      await mouse("mouseMoved", gx + 40, gy + 30, { buttons: 1 });
      await sleep(300);
      await keep("pj-drag-1440");
      await key("Escape", "Escape", 27);
      await mouse("mouseReleased", gx + 40, gy + 30);
      await sleep(150);
    }
    await evaluate(`${PJ}.ops = []; ${PJ}.cache = null; ${PJ}.base = null; __mgr.projtab.render()`);
  }
  await nfProj2("fm1");
  await nfProj2("browser");
  await size(1280, 800);
  }
  // ---- (end of nf-proj2)
  // ---- nf-speed: THIS BROWSER's store read and written in the page (the owner, 2026-10-29: "loading and writing to
  // browser take the same long time as writing to the FM-1"). The FM-1 (?sim) first: its PROJECTS read FZBR by FZBR,
  // its WRITE pulled twice by FZSR's 512-byte reads, as before. Then THIS BROWSER (the sound on: the worklet): a LOAD
  // and a WRITE of a pasted pattern dropped (moved) elsewhere each under SPEED_MS (measured ~16 ms and ~95 ms here;
  // before: ~9 s and ~6.5 s over the SysEx), no FZBR or FZSR read sent; the moved pattern there after a re-read, in the
  // running browser FIZZ's own flash and in the one kept in this browser
  {
    const SPEED_MS = { read: 1000, write: 1500 };   // (load-proof: well over the measured, far under the SysEx's)
    const PJ = "__mgr.projtab.state";
    const editDemo = (who) => `(() => { const s = ${who}, e = s.engine, t = e.info().panelTable;
      const set = (b, k) => { e.setPanel({ keys: k >>> 0, buttons: b >>> 0, enc: new Int32Array(7), keyVel: new Uint8Array(27) }); s.run(80); };
      set(1 << t.btnId[6], 0); set(0, 0); set(0, 1 << 2); set(0, 0); set(0, 1 << 4); set(0, 0); s.run(2500); return true; })()`;   // (HOME, two keys: the demo's A01 its own record)
    const timedRead = `(async () => { ${PJ}.base = null; const t = performance.now(); await __mgr.projtab.show(); return Math.round(performance.now() - t); })()`;
    const timedWrite = `(async () => { const t = performance.now(); document.querySelector('#pj-pending [data-act="write"]').click(); await new Promise((r) => setTimeout(r, 20));
      while (${PJ}.ops.length || __mgr.S.busy || __mgr.S.writing || __mgr.S.editing) await new Promise((r) => setTimeout(r, 5)); return Math.round(performance.now() - t); })()`;
    const logHas = (re) => evaluate(`${re}.test(document.getElementById('log').textContent)`);
    await size(1280, 800);
    await send("Page.navigate", { url: URL0 + "#projects" });
    await waitFor("window.__mgr && __mgr.S.mode === 'fm1' && !!__mgr.S.link && __mgr.S.unit && __mgr.S.unit.status", 60000);
    await waitFor(`!!${PJ}.base && !__mgr.S.busy`, 60000);
    await evaluate(editDemo("__sim"));
    await idle();
    // the FM-1: the paced SysEx
    const f0 = await evaluate("({ r: __sim.counts.fzbr, w: __sim.counts.fzsrReads, size: __mgr.S.unit.info.size })");
    const fmRead = await evaluate(timedRead);
    const f1 = await evaluate("({ r: __sim.counts.fzbr, w: __sim.counts.fzsrReads })");
    const fk = await evaluate(`(async () => { const P = await import('./projects.js'), m = ${PJ}.base.model;
      const k = [...m.prj.keys()].filter((x) => x >>> 24 === P.RK.PAT && ((x >>> 16) & 255) === 0).map((x) => x & 0xFFFF).find((x) => P.copyPatterns(m, 0, [x]).pats.length);
      if (k === undefined) return null;
      const j = P.project(m, 0).pats.findIndex((s, i) => !s && i > 8);
      ${PJ}.ops.push({ kind: 'drop', q: 0, j, clip: P.copyPatterns(m, 0, [k]), from: { p: 0, k }, move: false }); ${PJ}.cache = null; __mgr.projtab.render();
      return { k, j, rec: Array.from(m.prj.get(P.rkey(P.RK.PAT, 0, k))).join() }; })()`);
    const fmWrite = fk ? await evaluate(timedWrite) : -1;
    const f2 = await evaluate("({ r: __sim.counts.fzbr, w: __sim.counts.fzsrReads })");
    await evaluate(timedRead);
    const fmBack = fk && (await evaluate(`(async () => { const P = await import('./projects.js'), b = ${PJ}.base.model.prj.get(P.rkey(P.RK.PAT, 0, ${fk.j})); return !!b && Array.from(b).join() === ${JSON.stringify(fk.rec)}; })()`));
    check(fk && f1.r - f0.r >= Math.ceil(f0.size / 256) && f2.w - f1.w >= 2 * Math.ceil(f0.size / 512) && fmBack && (await logHas("/restore: FZOK/")) && !(await logHas("/THIS BROWSER's store/")),
      `nf-speed: the FM-1 (?sim) still paced: its read ${f1.r - f0.r} FZBR (${fmRead} ms), its WRITE ${f2.w - f1.w} FZSR reads (${fmWrite} ms), the drop there after a re-read`);
    // THIS BROWSER, the sound on: the clip of the FM-1's pattern pasted at A10, then dropped (moved) to A12, one WRITE
    const clip = fk ? await evaluate(`(async () => { const P = await import('./projects.js'); window.__nfClip = P.copyPatterns(${PJ}.base.model, 0, [${fk.k}]); return true; })()`) : false;
    await realClick("#unit");
    await waitFor("!document.getElementById('upick').hidden", 3000);
    await realClick("#upick-pop .urow[data-u='browser']");
    const bOn = await waitFor("__mgr.S.mode === 'browser' && !!__mgr.S.link && !!__mgr.bu() && !__mgr.bu().quiet && !__mgr.bu().waking", 40000);
    await waitFor(`!!${PJ}.base && !__mgr.S.busy`, 60000);
    await idle();
    const b0 = await evaluate("({ r: __mgr.bu().counts.fzbr, w: __mgr.bu().counts.fzsrReads })");
    const bRead = await evaluate(timedRead);
    const staged = clip && (await evaluate(`(async () => { const P = await import('./projects.js'), J = ${PJ}, m = P.cloneModel(J.base.model);
      P.pastePatterns(m, 0, 9, window.__nfClip);
      const drop = { kind: 'drop', q: 0, j: 11, clip: P.copyPatterns(m, 0, [9]), from: { p: 0, k: 9 }, move: true };
      P.pastePatterns(m, 0, 11, drop.clip); P.clearPatterns(m, 0, [9]);
      J.ops.push({ kind: 'paste', q: 0, j: 9, clip: window.__nfClip }, drop); J.cache = null; __mgr.projtab.render();
      window.__nfWant = Array.from(m.prj.get(P.rkey(P.RK.PAT, 0, 11))).join(); return J.ops.length === 2 && !document.getElementById('pj-pending').hidden; })()`));
    const bWrite = staged ? await evaluate(timedWrite) : -1;
    const wsay = await evaluate("document.getElementById('say').textContent");
    const bBack = await evaluate(`(async () => { const t = performance.now(); ${PJ}.base = null; await __mgr.projtab.show(); const P = await import('./projects.js'), { convertImage } = await import('./fizzstore.js');
      const at = (m) => { const b = m.prj.get(P.rkey(P.RK.PAT, 0, 11)); return { moved: !!b && Array.from(b).join() === window.__nfWant, left: !m.prj.has(P.rkey(P.RK.PAT, 0, 9)) }; };
      const fizz = await __mgr.bu().hostFlash();
      return { reread: at(${PJ}.base.model), running: at(P.modelOf(convertImage(fizz, 'device'))), kept: at(P.modelOf(convertImage(__mgr.S.brow.img, 'device'))),
        a12: !!(${PJ}.base.model && P.project(${PJ}.base.model, 0).pats[11]), ms: Math.round(performance.now() - t) }; })()`);
    const b1 = await evaluate("({ r: __mgr.bu().counts.fzbr, w: __mgr.bu().counts.fzsrReads })");
    const ok3 = (x) => x && x.moved && x.left;
    check(bOn && bRead >= 0 && bRead < SPEED_MS.read && b1.r === b0.r && (await logHas("/read: THIS BROWSER's store, \\d+ B in the page/")),
      `nf-speed: THIS BROWSER (the worklet): a PROJECTS LOAD in ${bRead} ms (under ${SPEED_MS.read}; the FM-1's ${fmRead}), no FZBR sent`);
    check(staged && bWrite >= 0 && bWrite < SPEED_MS.write && b1.w === b0.w && !/did not finish|cut off|could not/i.test(wsay),
      `nf-speed: THIS BROWSER: WRITE of a paste and a drop in ${bWrite} ms (under ${SPEED_MS.write}; the FM-1's ${fmWrite}), no FZSR read pulled${wsay ? ` ("${wsay}")` : ""}`);
    check(ok3(bBack.reread) && ok3(bBack.running) && ok3(bBack.kept) && bBack.a12,
      `nf-speed: THIS BROWSER: A10 moved to A12 after a re-read (${bBack.ms} ms), in the running FIZZ's flash and in the one kept here (${JSON.stringify(bBack)})`);
    // BACK UP of THIS BROWSER: the same read, kept in BACKUP
    const nb = await evaluate("__mgr.S.baks.length");
    const bk = await evaluate("(async () => { const t = performance.now(); const r = await __mgr.backup('full'); return { ms: Math.round(performance.now() - t), ok: !!r && !!r.image } })()");
    check(bk.ok && bk.ms < SPEED_MS.write && (await evaluate(`__mgr.S.baks.length === ${nb + 1}`)), `nf-speed: THIS BROWSER: BACK UP in ${bk.ms} ms, kept in BACKUP`);
    await idle();
  }
  // ---- (end of nf-speed)
  // ---- nf-edit3 (the owner, 2026-10-30): EDIT's one panel on one grid (design 12) and drag to modulate (design 13), flat.
  // At 1440 x 900: every value tile on the grid (its edges on the grid's lines within 1 px, the same size for the same
  // span), none overflowing; the MOD area as tall with 0 routes as with 8; ALGO's number centred; no shadow on the
  // tiles, frames, drawings, choices, sources; the head, the keys' toggle and the page tabs flat (no blur, no glow, no
  // drop-shadow); an envelope's handle <= 10 px; the filter's 8-way TYPE its stored values; LFO 2's plug onto MIX (its
  // DEST, its DEPTH kept); VEL onto FREQ (a free slot at +40), again (its card), × (gone); all 8 used: FULL, nothing
  // changed; LFO 1 not on its own values, VEL on LFO 1's SPEED; tap to arm on a 390 x 844 phone and on the page tabs at
  // 1280 x 700; a drop with TRACK on: PARAMs on ?sim's track
  {
  const SHOTS = process.env.NF_EDIT3_SHOTS || "";
  const keep = async (name) => { await shot(`manager-nfedit3-${name}.png`); if (SHOTS) fs.copyFileSync(path.join(OUT, `manager-nfedit3-${name}.png`), path.join(SHOTS, `${name}.png`)); };
  const V = (i) => evaluate(`__mgr.editor.value(${i})`);
  const at = (sel) => evaluate(`(() => { const e = document.querySelector(${JSON.stringify(sel)}); if (!e) return null; e.scrollIntoView({ block: 'center' }); const r = e.getBoundingClientRect(); return { x: r.left + r.width / 2, y: r.top + r.height / 2 }; })()`);
  // a source dragged with the mouse onto a value (let go there, or held: its cable shown)
  async function plug(from, to, hold = false) {
    const a = await at(from), b = await at(to);
    if (!a || !b) return false;
    await mouse("mousePressed", a.x, a.y);
    for (let k = 1; k <= 8; k++) { await mouse("mouseMoved", a.x + (b.x - a.x) * k / 8, a.y + (b.y - a.y) * k / 8, { buttons: 1 }); await sleep(25); }
    if (hold) return b;
    await mouse("mouseReleased", b.x, b.y);
    await sleep(250);
    return true;
  }
  const tap = async (sel) => { const a = await at(sel); if (!a) return false; await mouse("mousePressed", a.x, a.y); await mouse("mouseReleased", a.x, a.y); await sleep(250); return true; };
  const touch = async (sel) => { const a = await at(sel); if (!a) return false; await send("Input.dispatchTouchEvent", { type: "touchStart", touchPoints: [{ x: a.x, y: a.y }] }); await sleep(60);
    await send("Input.dispatchTouchEvent", { type: "touchEnd", touchPoints: [] }); await sleep(300); return true; };
  const routesOf = () => evaluate("__mgr.editor.sound().values.slice(86, 110).join()");
  const fresh = (js = "") => evaluate(`(async () => { const { newSound } = await import('./sound.js'); __mgr.live.detach(); const s = newSound('fm'); s.name = 'GRID TEST'; const v = s.values;
    v[86] = 2; v[87] = 15; v[88] = -36; v[47 + 3] = 26; v[47 + 7] = 2400; v[55 + 7] = 1600; ${js}
    __mgr.editor.open(s, { label: 'GRID TEST', source: 'file' }); })()`);
  await size(1440, 900);
  await send("Page.navigate", { url: URL0.replace(/v=\d+/, `v=${Date.now()}`) + "#edit" });
  await waitFor("window.__mgr && __mgr.S.unit && __mgr.S.unit.status && __mgr.S.unit.rows.length > 0", 40000);
  await idle();
  await click("#t-edit");
  await sleep(1500);                                   // (the track followed first: then this browser's own sound)
  await fresh();
  await waitFor("__mgr.editor.state.mode === 'panel' && !!document.querySelector('#ed-stage .fr-mod') && __mgr.editor.place().label === 'GRID TEST' && !(__mgr.editor.target() && __mgr.editor.target().locked && __mgr.editor.target().locked())", 8000);
  await sleep(600);
  // the grid: every tile's edges on the grid's lines, the same size for the same span, nothing overflowing
  const grid = () => evaluate(`(() => { const st = document.getElementById('ed-stage'), cs = getComputedStyle(st), sb = st.getBoundingClientRect();
    const cols = cs.gridTemplateColumns.split(' ').map(parseFloat), rows = cs.gridTemplateRows.split(' ').map(parseFloat), cg = parseFloat(cs.columnGap), rg = parseFloat(cs.rowGap);
    const lines = (sizes, gap, o) => { const a = [], b = []; let p = o; for (const z of sizes) { a.push(p); b.push(p + z); p += z + gap; } return [a, b]; };
    const [c0, c1] = lines(cols, cg, sb.left + parseFloat(cs.paddingLeft) + st.clientLeft), [r0, r1] = lines(rows, rg, sb.top + parseFloat(cs.paddingTop) + st.clientTop);
    const on = (x, L) => L.findIndex((y) => Math.abs(x - y) <= 1);
    const bad = [], sizes = new Map(); let n = 0, over = [];
    st.querySelectorAll('.t').forEach((t) => { const r = t.getBoundingClientRect(); if (!r.width) return; n++;
      const a = on(r.left, c0), b = on(r.right, c1), c = on(r.top, r0), d = on(r.bottom, r1);
      if (a < 0 || b < 0 || c < 0 || d < 0) { bad.push((t.dataset.id || t.className) + '@' + Math.round(r.left) + ',' + Math.round(r.top)); return; }
      const k = (b - a + 1) + 'x' + (d - c + 1), w = Math.round(r.width) + 'x' + Math.round(r.height); (sizes.get(k) || sizes.set(k, new Set()).get(k)).add(w);
      for (const e of t.querySelectorAll(':scope > small, :scope > b')) if (e.scrollWidth > e.clientWidth + 1) over.push((t.dataset.id || '') + ':' + e.textContent);
      if (r.right > sb.right + 1 || r.bottom > sb.bottom + 1) over.push('out ' + t.dataset.id); });
    const spans = [...sizes].map(([k, s]) => k + ':' + [...s].join('/'));
    const uneven = [...sizes].filter(([, s]) => [...s].some((x) => [...s].some((y) => Math.abs(+x.split('x')[0] - +y.split('x')[0]) > 1 || Math.abs(+x.split('x')[1] - +y.split('x')[1]) > 1)));
    return { n, bad, over, spans: spans.join(' '), uneven: uneven.length, cols: cols.length, rows: rows.length }; })()`);
  const g = await grid();
  check(g.cols === 20 && g.rows === 15 && g.n >= 80 && !g.bad.length && !g.uneven && !g.over.length,
    `nf-edit3: at 1440 x 900 one panel, 20 x 15: ${g.n} value tiles on the grid's lines, the same size a span (${g.spans}), none overflowing${g.bad.length ? `; off the grid: ${g.bad.slice(0, 6).join(" ")}` : ""}${g.over.length ? `; overflowing: ${g.over.slice(0, 6).join(" ")}` : ""}`);
  await keep("panel-1440");
  // the MOD area the same with 0 and 8 routes
  const modH = () => evaluate(`(() => { const f = document.querySelector('#ed-stage .fr-mod').getBoundingClientRect(), s = [...document.querySelectorAll('#ed-stage .mslot')].map((e) => e.getBoundingClientRect());
    return [Math.round(f.height), Math.round(Math.max(...s.map((r) => r.bottom)) - Math.min(...s.map((r) => r.top))), document.querySelectorAll('#ed-stage .mslot').length]; })()`);
  await evaluate("[0, 1, 2, 3, 4, 5, 6, 7].forEach((k) => __mgr.editor.set(86 + 3 * k, 0))");
  await sleep(250);
  const m0 = await modH();
  await evaluate("[0, 1, 2, 3, 4, 5, 6, 7].forEach((k) => { __mgr.editor.set(86 + 3 * k, 1 + k); __mgr.editor.set(87 + 3 * k, 30 + k); __mgr.editor.set(88 + 3 * k, 20); })");
  await sleep(250);
  const m8 = await modH(), g8 = await grid();
  check(m0.join() === m8.join() && m0[2] === 8 && !g8.bad.length && !g8.uneven, `nf-edit3: the MOD area ${m0[0]} px (slots ${m0[1]} px) with 0 routes, ${m8[0]} (${m8[1]}) with 8; the grid unchanged`);
  // ALGO centred; flat: no shadow on the panel's parts, none glowing in the head, the toggle, the tabs; handles small
  const flat = await evaluate(`(() => { const st = document.getElementById('ed-stage'), al = st.querySelector('.algoT'), b = al.querySelector('b').getBoundingClientRect(), a = al.getBoundingClientRect();
    const sh = [...st.querySelectorAll('.t, .fr, .gd, .sg, .src, .mslot, .nav')].filter((e) => getComputedStyle(e).boxShadow !== 'none').map((e) => e.className);
    const glow = (e, pseudo) => { const c = getComputedStyle(e, pseudo), bs = c.boxShadow; if (c.textShadow !== 'none' || (c.filter !== 'none' && /drop-shadow/.test(c.filter))) return true;
      if (bs === 'none') return false; return bs.split(/,(?![^(]*\\))/).some((l) => { const n = (l.replace(/rgba?\\([^)]*\\)/, '').match(/-?[\\d.]+px/g) || []).map(parseFloat); const col = (l.match(/rgba?\\([^)]*\\)/) || [''])[0];
        return !/inset/.test(l) && n[2] > 0 && !/^rgba?\\(0, 0, 0/.test(col); }); };
    const head = [...document.querySelectorAll('#ed-head, #ed-head *, #ed-pages *, #ed-srcs *, #ed-keys *')].filter((e) => glow(e) || glow(e, '::before') || glow(e, '::after')).map((e) => e.id || e.className);
    const hd = [...st.querySelectorAll('.env .hd')].map((e) => Math.max(e.getBoundingClientRect().width, e.getBoundingClientRect().height));
    const hit = [...st.querySelectorAll('.env .hit')].map((e) => e.getBoundingClientRect().width);
    return { dx: Math.abs((b.left + b.right) / 2 - (a.left + a.right) / 2), dy: Math.abs((b.top + b.bottom) / 2 - (a.top + a.bottom) / 2), sh, head, hd: Math.max(...hd), hdn: hd.length, hit: Math.min(...hit) }; })()`);
  check(flat.dx <= 2 && flat.dy <= 2, `nf-edit3: ALGO's number centred in its block (off by ${flat.dx.toFixed(1)}, ${flat.dy.toFixed(1)} px)`);
  check(!flat.sh.length, `nf-edit3: no box-shadow on the tiles, frames, drawings, choices, sources, slots${flat.sh.length ? `: ${flat.sh.slice(0, 5).join(" | ")}` : ""}`);
  check(!flat.head.length, `nf-edit3: flat: no glow (blur, text-shadow, drop-shadow) in EDIT's head, the keys' toggle, the page tabs, the keys${flat.head.length ? `: ${flat.head.slice(0, 6).join(" | ")}` : ""}`);
  check(flat.hdn >= 9 && flat.hd <= 10 && flat.hit >= 20, `nf-edit3: the envelopes' ${flat.hdn} handles ${flat.hd.toFixed(1)} px at most, a ${flat.hit.toFixed(0)} px hit ring`);
  // the filter's TYPE: one 8-way choice, its stored values; SHAPE's cell kept (dimmed) where unused
  const ft = [];
  for (let k = 0; k < 8; k++) { await click(`#ed-stage .sg.ftype [data-set="36:${k}"]`); await sleep(60); ft.push(await V(36)); }
  const ftl = await evaluate("[...document.querySelectorAll('#ed-stage .sg.ftype button')].map((b) => b.textContent).join(' ')");
  await click('#ed-stage .sg.ftype [data-set="36:1"]');
  await sleep(100);
  const dim = await evaluate("(() => { const d = document.querySelector('#ed-stage .pn-filter .t.dim0'); return d && d.textContent; })()");
  check(ft.join() === "0,1,2,3,4,5,6,7" && ftl === "OFF LP12 HP12 LP24 MULTI COMB+ COMB− EQ" && dim === "SHAPE—", `nf-edit3: the filter's TYPE (${ftl}) sets ${ft.join(" ")}; LP12: SHAPE dimmed "—" in its cell`);
  // design 13: LFO 2's plug onto MIX: its DEST, its DEPTH as it was
  await fresh("v[55 + 7] = 0;");
  await sleep(400);
  const held = await plug('#ed-stage .jack[data-lfo="1"]', '#ed-stage .t[data-id="7"]', true);
  const lit = await evaluate("({ n: document.querySelectorAll('#ed-stage .lit').length, hot: (document.querySelector('#ed-stage .hot') || { dataset: {} }).dataset.id, l1: document.querySelectorAll('#ed-stage .t.lit[data-id=\"47\"]').length, cable: !!document.querySelector('.dragov .cable') })");
  await keep("drag-lfo2-1440");
  await mouse("mouseReleased", held.x, held.y);
  await sleep(250);
  check(lit.hot === "7" && lit.l1 === 1 && lit.cable && (await V(58)) === 14 && (await V(62)) === 0 && (await evaluate("!!document.querySelector('#ed-stage .t[data-id=\"7\"] .mks i[data-route=\"lfo1\"]')")),
    `nf-edit3: LFO 2's plug dragged (${lit.n} values lit, LFO 1's SPEED among them) onto MIX: its DEST MIX, its DEPTH ${await V(62)} kept (no bump); MIX marked`);
  // VEL onto FREQ: the first free slot at +40; again: its card; × takes it off
  const b2 = await plug('#ed-stage .src[data-src="1"]', '#ed-stage .t[data-id="34"]', true);
  await keep("drag-vel-1440");
  await mouse("mouseReleased", b2.x, b2.y);
  await sleep(250);
  const r1 = await routesOf();
  check(r1.startsWith("2,15,-36,1,25,40,0"), `nf-edit3: VEL dragged onto FREQ fills slot 2 (the first free) at +40: ${r1.split(",").slice(0, 9).join(",")}`);
  await plug('#ed-stage .src[data-src="1"]', '#ed-stage .t[data-id="34"]');
  const card = await evaluate("(() => { const p = document.querySelector('.rtpop'); return p ? { who: p.querySelector('.who small').textContent, what: p.querySelector('.who b').textContent, dep: p.querySelector('.dep b').textContent } : null; })()");
  check(card && card.who === "MOD 2" && card.what === "VEL → FREQ" && card.dep === "+40" && (await routesOf()) === r1, `nf-edit3: VEL onto FREQ again opens its card (${card && `${card.who} · ${card.what} ${card.dep}`}), nothing added`);
  const dc = await at(".rtpop .dep");
  await mouse("mousePressed", dc.x, dc.y);
  for (let k = 1; k <= 5; k++) { await mouse("mouseMoved", dc.x, dc.y - 12 * k, { buttons: 1 }); await sleep(25); }
  await mouse("mouseReleased", dc.x, dc.y - 60);
  await sleep(150);
  const d2 = await V(91);
  await keep("route-card-1440");
  await click(".rtpop .rm");
  await sleep(200);
  check(d2 > 40 && (await V(89)) === 0 && !(await evaluate("!!document.querySelector('.rtpop')")) && !(await evaluate("!!document.querySelector('#ed-stage .t[data-id=\"34\"] .mks i')")),
    `nf-edit3: the card's DEPTH dragged up (+40 -> ${d2}); × takes the route off (slot 2 OFF), the card and FREQ's dot gone`);
  // a dot opens its card too
  await tap('#ed-stage .t[data-id="35"] .mks i');
  const card2 = await evaluate("(() => { const p = document.querySelector('.rtpop'); return p && p.querySelector('.who b').textContent; })()");
  await evaluate("document.dispatchEvent(new KeyboardEvent('keydown', { key: 'Escape', bubbles: true }))");
  check(card2 === "LFO 1 → RESO", `nf-edit3: RESO's dot opens its route's card (${card2})`);
  // all 8 used: FULL, nothing changed
  await evaluate("[0, 1, 2, 3, 4, 5, 6, 7].forEach((k) => { __mgr.editor.set(86 + 3 * k, 1 + k); __mgr.editor.set(87 + 3 * k, 30 + k); __mgr.editor.set(88 + 3 * k, 20); })");
  await sleep(250);
  const f0 = await evaluate("__mgr.editor.sound().values.join()");
  const b3 = await plug('#ed-stage .src[data-src="4"]', '#ed-stage .t[data-id="42"]', true);
  const fullOn = await evaluate("({ ghost: (document.querySelector('.dragov .ghost') || {}).textContent, full: document.getElementById('ed-stage').classList.contains('full'), atk: document.querySelector('#ed-stage .t[data-id=\"42\"]').classList.contains('lit') })");
  await keep("drag-full-1440");
  await mouse("mouseReleased", b3.x, b3.y);
  await sleep(250);
  check(/FULL/.test(fullOn.ghost || "") && fullOn.full && !fullOn.atk && (await evaluate("__mgr.editor.sound().values.join()")) === f0, `nf-edit3: all 8 slots used: AT dragged onto ATK shows "${fullOn.ghost}", the MOD frame red; dropped: nothing changed`);
  // LFO 1 not onto its own values; VEL onto LFO 1's SPEED
  await evaluate("[0, 1, 2, 3, 4, 5, 6, 7].forEach((k) => __mgr.editor.set(86 + 3 * k, 0))");
  await sleep(200);
  const l1d = await V(50);
  await plug('#ed-stage .jack[data-lfo="0"]', '#ed-stage .t[data-id="47"]');
  const l1after = await V(50);
  await plug('#ed-stage .src[data-src="1"]', '#ed-stage .t[data-id="47"]');
  check(l1after === l1d && (await routesOf()).startsWith("1,40,40"), `nf-edit3: LFO 1 dropped on its own SPEED: refused (DEST ${l1after}); VEL on LFO 1's SPEED: slot 1 VEL → L1SPD +40`);
  // a drop with TRACK on: the route reaches ?sim's track as PARAMs
  await click("#live");
  await waitFor("__mgr.live.on()");
  await evaluate("__mgr.live.attach()");
  await waitFor("__mgr.editor.place().fromTarget && !__mgr.editor.sound().midi", 8000);
  await sleep(500);
  const tk = await evaluate("__mgr.editor.place().track");
  const k0 = await evaluate("(() => { for (let k = 0; k < 8; k++) if (!__mgr.editor.value(86 + 3 * k)) return k; return -1; })()");
  await plug('#ed-stage .src[data-src="3"]', '#ed-stage .t[data-id="73"]');
  await waitFor("!__mgr.live.state.pumping && !__mgr.live.state.q.size", 5000);
  await sleep(200);
  const tv = await evaluate(`(async () => { const { soundFromFrame } = await import('./sound.js'); return soundFromFrame(await __mgr.S.link.dumpTrack(${tk})).values.slice(${86 + 3 * k0}, ${89 + 3 * k0}).join(); })()`);
  check(k0 >= 0 && tv === "3,53,40", `nf-edit3: TRACK on: MW dropped on DRIVE: track ${tk + 1}'s slot ${k0 + 1} is MW → DRIVE +40 on ?sim (${tv}, PARAMs)`);
  await click("#live");
  await waitFor("!__mgr.live.on()", 4000);
  // the other sizes: 1920 x 1080 and 1280 x 800 with the keys hidden one panel on the grid
  await fresh("v[86] = 1; v[87] = 25; v[88] = 48;");
  await size(1920, 1080);
  await sleep(700);
  const g19 = await grid();
  check(g19.cols === 20 && !g19.bad.length && !g19.uneven && !g19.over.length, `nf-edit3: 1920 x 1080 one panel, ${g19.n} tiles on the grid`);
  await keep("panel-1920");
  await size(1280, 800);
  await sleep(500);
  const kbOn = await evaluate("__mgr.editor.state.kbOn");
  if (kbOn) await click("#ed-kbtg");
  await sleep(600);
  const g12 = await grid(), m12 = await evaluate("__mgr.editor.state.mode");
  check(m12 === "panel" && !g12.bad.length && !g12.uneven && (await noScroll()), `nf-edit3: 1280 x 800, the keys hidden: one panel (${g12.n} tiles on the grid)`);
  await keep("panel-1280-keys-hidden");
  await click("#ed-kbtg");
  // the page tabs at 1280 x 700: tap VEL, tap FILTER's tab, tap RESO (lit): a route; × lets go
  await size(1280, 700);
  await sleep(700);
  const pm = await evaluate("__mgr.editor.state.mode");
  await click('#ed-pages [data-page="ops"]');
  await tap('#ed-srcs .src[data-src="2"]');
  const armed = await evaluate("({ a: !!document.querySelector('#ed-srcs .src.isarmed[data-src=\"2\"]'), x: !document.getElementById('ed-disarm').hidden, reach: [...document.querySelectorAll('#ed-pages .reach')].map((e) => e.parentNode.dataset.page).join() })");
  await tap('#ed-pages [data-page="filter"]');
  const lit2 = await evaluate("({ a: !!document.querySelector('#ed-srcs .src.isarmed'), n: document.querySelectorAll('#ed-stage .pn.cur .lit').length, page: __mgr.editor.state.page })");
  await keep("pages-armed-1280x700");
  await tap('#ed-stage .pn.cur .t[data-id="35"]');
  const rp = await routesOf();
  await tap('#ed-srcs .src[data-src="5"]');
  await tap("#ed-disarm");
  const off = await evaluate("!document.querySelector('#ed-srcs .src.isarmed') && !document.getElementById('ed-stage').classList.contains('modon')");
  check(pm === "tabs" && armed.a && armed.x && armed.reach.includes("filter") && !armed.reach.includes("mod") && lit2.a && lit2.page === "filter" && lit2.n >= 8 && rp.startsWith("1,25,48,2,26,40") && off,
    `nf-edit3: 1280 x 700 pages: KEY tapped (armed, the tabs it reaches marked: ${armed.reach}), FILTER's tab (${lit2.n} lit), RESO tapped: KEY → RESO +40; × lets go`);
  // a phone: the strip under the tabs, a touch arms, a touch on a tab, a touch on a lit value
  await size(390, 844, true);
  await send("Emulation.setTouchEmulationEnabled", { enabled: true, maxTouchPoints: 5 });
  await sleep(800);
  await touch('#ed-srcs .src[data-src="8"]');
  await touch('#ed-pages [data-page="amp"]');
  const ph = await evaluate("({ mode: __mgr.editor.state.mode, lay: __mgr.editor.state.lay, a: !!document.querySelector('#ed-srcs .src.isarmed[data-src=\"8\"]'), lit: !!document.querySelector('#ed-stage .pn-amp.cur .t.lit[data-id=\"115\"]') })");
  await evaluate("window.scrollTo(0, 0)");
  await keep("phone-armed-390");
  await touch('#ed-stage .pn-amp.cur .t[data-id="115"]');
  const rph = await routesOf();
  check(ph.mode === "tabs" && ph.lay === "phone" && ph.a && ph.lit && rph.startsWith("1,25,48,2,26,40,8,39,40") && (await noScroll()),
    `nf-edit3: 390 x 844: F ENV touched on the strip (armed), AMP's tab, PAN (lit) touched: F ENV → PAN +40; no horizontal scroll (${JSON.stringify(ph)} ${rph})`);
  await send("Emulation.setTouchEmulationEnabled", { enabled: false });
  await size(1280, 800);
  await sleep(300);
  }
  // ---- (end of nf-edit3)
  // ---- nf-flat (the owner, 2026-10-30: "Flatten everything."): no NEON glow left outside EDIT (rebuilt on its own):
  // every tab at 1440 x 900 (a preset picked in LIBRARY, a place in POOL), the progress screen, the unit picker, PORTS,
  // a toast, + NEW's popover, a confirm box, LIBRARY at 390 x 844, THIS BROWSER's header and PROJECTS: no element (nor
  // its ::before / ::after) with a coloured box-shadow (a zero-blur ring is a hairline border: linework; a black one lifts
  // a floating layer), a coloured text-shadow or drop-shadow filter, but the lights kept (PLAY's power LED, REC and the
  // keys' LEDs, the unit pill's dot, the restore's lit FM-1 key); no animation running a coloured shadow (none breathing)
  {
    const FLAT_SCAN = String.raw`(() => {
      const KEEP = ["#tab-play .led0", "#tab-play .pb .rec", ".unit .dot", ".fm1 .key.lit"], KEEP_AFTER = ["#tab-play .k"], SKIP = "#tab-edit";
      const split = (s) => { const o = []; let d = 0, c = ""; for (const ch of s) { if (ch === "(") d++; if (ch === ")") d--; if (ch === "," && !d) { o.push(c.trim()); c = ""; } else c += ch; } if (c.trim()) o.push(c.trim()); return o; };
      const col = (l) => { let m = l.match(/rgba?\(([^)]*)\)/); if (m) { const v = m[1].split(/[ ,/]+/).filter(Boolean).map(Number); return { c: [v[0] / 255, v[1] / 255, v[2] / 255], a: v.length > 3 ? v[3] : 1, rest: l.replace(m[0], "") }; }
        m = l.match(/color\(srgb ([^)]*)\)/); if (m) { const v = m[1].split(/[ /]+/).filter(Boolean).map(Number); return { c: v.slice(0, 3), a: v.length > 3 ? v[3] : 1, rest: l.replace(m[0], "") }; }
        if (/#000\b|#000000\b|\bblack\b/.test(l)) return { c: [0, 0, 0], a: 1, rest: l.replace(/#000000|#000|black/, "") };
        if (/transparent/.test(l)) return { c: [0, 0, 0], a: 0, rest: l.replace("transparent", "") };
        return { c: [1, 1, 1], a: 1, rest: l }; };
      const black = (k) => k.a === 0 || k.c.every((x) => x <= 0.01);
      const lens = (r) => (r.match(/-?[\d.]+px/g) || []).map(parseFloat);
      const shadowBad = (v) => !!v && v !== "none" && split(v).some((l) => { const k = col(l), n = lens(k.rest); return !black(k) && (n[0] || n[1] || n[2]); });
      const textBad = (v) => !!v && v !== "none" && split(v).some((l) => !black(col(l)));
      const filterBad = (v) => !!v && [...v.matchAll(/drop-shadow\(((?:[^()]|\([^)]*\))*)\)/g)].some((m) => !black(col(m[1])));
      const look = (el, pseudo) => { const cs = getComputedStyle(el, pseudo), why = [];
        if (shadowBad(cs.boxShadow)) why.push("box-shadow " + cs.boxShadow);
        if (textBad(cs.textShadow)) why.push("text-shadow " + cs.textShadow);
        if (filterBad(cs.filter)) why.push("filter " + cs.filter);
        return why; };
      const name = (el) => (el.id ? "#" + el.id : el.tagName.toLowerCase()) + (el.classList.length ? "." + [...el.classList].join(".") : "");
      const bad = [];
      for (const el of document.querySelectorAll("body *")) {
        if (el.closest(SKIP)) continue;
        if (!KEEP.some((s) => el.matches(s))) for (const w of look(el, null)) bad.push(name(el) + ": " + w);
        for (const ps of ["::before", "::after"]) { if (getComputedStyle(el, ps).content === "none" || (ps === "::after" && KEEP_AFTER.some((s) => el.matches(s)))) continue; for (const w of look(el, ps)) bad.push(name(el) + ps + ": " + w); }
      }
      const anim = [];
      for (const a of document.getAnimations()) {
        const t = a.effect && a.effect.target; if (!t || t.closest(SKIP) || a.playState !== "running" || [...KEEP, ...KEEP_AFTER].some((s) => t.matches(s))) continue;
        const glow = a.effect.getKeyframes().some((f) => shadowBad(f.boxShadow) || textBad(f.textShadow) || filterBad(f.filter));
        if (a.animationName === "breathe" || glow) anim.push(name(t) + ": " + (a.animationName || "?"));
      }
      return { n: bad.length, bad: [...new Set(bad)], anim: [...new Set(anim)] };
    })()`;
    const flat = async (what, file) => {
      if (file) await shot(path.join("flat", file));
      const r = await evaluate(FLAT_SCAN), all = [...r.bad, ...r.anim.map((a) => `animation ${a}`)];
      check(!all.length, `nf-flat: ${what}: no glow${all.length ? ` (${all.slice(0, 6).join(" | ")}${all.length > 6 ? ` … ${all.length} in all` : ""})` : ""}`);
    };
    const progDone = () => waitFor("document.getElementById('prog').hidden", 120000);
    fs.mkdirSync(path.join(OUT, "flat"), { recursive: true });
    await size(1440, 900);
    await send("Page.navigate", { url: URL0.replace(/v=\d+/, `v=${Date.now()}`) });
    await waitFor("window.__mgr && __mgr.S.unit && __mgr.S.unit.rows.length > 0", 40000);
    await idle();
    await progDone();
    for (const t of ["backup", "library", "edit", "pool", "projects", "import", "play"]) {
      await realClick(`#t-${t}`);
      await sleep(700);
      if (t === "projects" && !(await evaluate("document.getElementById('prog').hidden"))) { await flat("the progress screen (reading PROJECTS)", "progress-1440.png"); await progDone(); await sleep(500); }
      if (t === "library") { await click("#l-list .cas"); await sleep(400); }
      if (t === "pool") { await click("#tab-pool .ps.full"); await sleep(400); }
      await flat(`${t.toUpperCase()} at 1440 x 900`, `${t}-1440.png`);
    }
    await realClick("#t-library");
    await sleep(500);
    await realClick("#l-new");
    await waitFor("!document.getElementById('l-newwrap').hidden", 3000);
    await sleep(400);
    await flat("+ NEW's popover", "library-newpop-1440.png");
    await evaluate("document.dispatchEvent(new KeyboardEvent('keydown', { key: 'Escape', bubbles: true }))");
    await sleep(300);
    if (!(await evaluate("document.getElementById('l-newwrap').hidden"))) { await click("#l-newscrim"); await sleep(300); }
    await realClick("#unit");
    await waitFor("!document.getElementById('upick').hidden", 3000);
    await sleep(400);
    await flat("the unit picker", "picker-1440.png");
    await click("#upick-scrim");
    await sleep(300);
    await evaluate("document.getElementById('logbox').open = true");
    await click("#lp-ports");
    await sleep(900);
    await flat("the LOG's PORTS", "ports-1440.png");
    await evaluate("document.getElementById('logbox').open = false");
    await evaluate("(() => { const el = document.getElementById('toast'); el.style.setProperty('--c', 'var(--green)'); el.textContent = '+1 on the FM-1'; el.hidden = false; })()");
    await sleep(500);
    await flat("a toast", "toast-1440.png");
    await evaluate("document.getElementById('toast').hidden = true");
    await evaluate("import('./ui.js').then((m) => { m.confirmBox({ title: 'DELETE', what: 'SUB DROP', go: 'DELETE', undo: 'cleared' }); return true; })");
    await sleep(600);
    await flat("a confirm box", "confirm-1440.png");
    await click("#cf-no");
    await sleep(300);
    await send("Emulation.setDeviceMetricsOverride", { width: 390, height: 844, deviceScaleFactor: 1, mobile: true });
    await sleep(800);
    await flat("LIBRARY at 390 x 844", "library-390.png");
    await size(1440, 900);
    await sleep(400);
    await realClick("#unit");
    await waitFor("!document.getElementById('upick').hidden", 3000);
    await realClick("#upick-pop .urow[data-u='browser']");
    await waitFor("__mgr.S.mode === 'browser' && !!__mgr.S.link && !!__mgr.S.unit && __mgr.S.unit.rows.length > 0", 40000);
    await idle();
    await realClick("#t-projects");
    await progDone();
    await sleep(800);
    await flat("THIS BROWSER: the header (the sound pill, the unit pill) and PROJECTS", "projects-browser-1440.png");
    await size(1280, 800);
  }
  // ---- (end of nf-flat)
  check(!errors.length, `no console errors${errors.length ? `: ${errors.slice(0, 5).join(" | ")}` : ""}`);
} catch (e) {
  check(false, `the run: ${e.message}`);
}
console.log(fails ? `MANAGER PAGE TEST FAILED (${fails})` : `manager page test passed (screenshots in ${OUT})`);
process.exit(fails ? 1 : 0);
