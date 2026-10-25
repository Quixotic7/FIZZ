// SPDX-License-Identifier: GPL-3.0-only
// The manager page (site/manager/) in headless Chromium over the DevTools protocol (as test_site.mjs: no Playwright,
// never a window), against its simulated FM-1 (?sim: the browser FIZZ behind a fake Web MIDI; headless Chromium's own
// Web MIDI mangles SysEx, so the unit is never reached from here):
//   - every tab at 1280 x 800 and 390 x 844: no console error, no horizontal scroll; the screenshots beside the
//     mock-ups' (mockups/png/fizz-web-manager-0N-*.png) for the eye;
//   - LIBRARY: a tag toggled, a preset renamed, moved (dropped on another place), + POOL, DELETE (once, then again
//     when it is used); the STATUS numbers on BACKUP without a backup;
//   - POOL: a MY preset picked and put on an empty place, CLEAR;
//   - IMPORT: two synthetic Digitone II files (never real presets) read, one refused (not FM TONE), sent.
//   node site/tools/test_manager_page.mjs [--url http://127.0.0.1:8765/] [--chromium path]
// (a static server on the site folder: python3 -m http.server 8765 --bind 127.0.0.1, from site/)
import fs from "node:fs";
import os from "node:os";
import path from "node:path";
import { spawn } from "node:child_process";
import { fileURLToPath } from "node:url";
import { dn2RecordFromParams, wrapDn2Record, dn2pstZip } from "../js/dn2pst.js";

const HERE = path.dirname(fileURLToPath(import.meta.url));
const OUT = path.resolve(HERE, "../.build/test");
const arg = (k, d) => { const i = process.argv.indexOf(k); return i > 0 ? process.argv[i + 1] : d; };
const URL0 = new URL("manager/?sim", arg("--url", "http://127.0.0.1:8765/")).href;
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

try {
  await connect();
  await send("Page.enable"); await send("Runtime.enable"); await send("Log.enable");
  await size(1280, 800);
  await send("Page.navigate", { url: URL0 });
  const up = await waitFor("window.__mgr && __mgr.S.unit && __mgr.S.unit.status && __mgr.S.unit.rows.length > 0", 30000);
  const st = await evaluate("__mgr.S.unit && { status: __mgr.S.unit.status, n: __mgr.S.unit.rows.filter((r) => r.source === 'mine').length }");
  check(up && st.status.tracks === 6, `?sim: connected, STATUS ${st && JSON.stringify({ tracks: st.status.tracks, projects: st.status.projects, mine: st.status.mine })}`);
  await sleep(400);
  const nums = await evaluate("[$('u-prj'), $('u-pre'), $('u-use')].map((e) => e.textContent)".replace(/\$\(/g, "document.getElementById("));
  check(nums[0] === "1" && +nums[1] > 0 && /%$/.test(nums[2]), `BACKUP: projects ${nums[0]}, my presets ${nums[1]}, store used ${nums[2]} (STATUS, no backup taken)`);
  await shot("manager-01-backup.png");

  // LIBRARY
  await click("#t-library");
  await waitFor("__mgr.S.unit.dumps && document.querySelectorAll('#l-mine .cas').length > 0");
  await click("#l-mine .cas[data-slot='2']");
  await waitFor("document.querySelector('#l-users .uchip, #l-users .pchip')");
  await idle();
  await shot("manager-03-library.png");
  check(await noScroll(), "LIBRARY at 1280: no horizontal scroll");
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
  await evaluate("(async () => { const ev = new KeyboardEvent('keydown', { key: 'ArrowDown', altKey: true, bubbles: true }); document.querySelector(\"#l-mine .cas[data-slot='2']\").dispatchEvent(ev); })()");
  await waitFor("__mgr.S.unit.rows.find((r) => r.source === 'mine' && r.name === 'BELL TOWER').slot === 3");
  await idle();
  check(await evaluate("__mgr.S.unit.rows.find((r) => r.source === 'mine' && r.name === 'BELL TOWER').slot === 3"), "LIBRARY: ALT + down moved it a place (SWAP)");
  await click("#l-pool");
  await idle();
  const inPool = await evaluate("(() => { const r = __mgr.S.unit.rows.find((x) => x.source === 'mine' && x.name === 'BELL TOWER'); return __mgr.S.unit.rows.some((x) => x.source === 'pool' && x.id === r.id); })()");
  check(inPool, "LIBRARY: + POOL: the pool references it");
  await click("#l-del");
  await idle();
  const armed = await evaluate("!!__mgr.S.armed && document.getElementById('l-del').textContent");
  check(armed && /DELETE\?/.test(armed), `LIBRARY: DELETE of a used preset asks again: "${armed}"`);
  await click("#l-del");
  await idle();
  check(await evaluate("!__mgr.S.unit.rows.some((r) => r.source === 'mine' && r.name === 'BELL TOWER')"), "LIBRARY: DELETE pressed again: gone");

  // POOL
  await click("#t-pool");
  await waitFor("document.querySelectorAll('#p-grid .ps').length === 128");
  await idle();
  await sleep(800);
  await shot("manager-04-pool.png");
  check(await noScroll(), "POOL at 1280: no horizontal scroll");
  const free = await evaluate("[...document.querySelectorAll('#p-grid .ps:not(.full)')].map((e) => +e.dataset.k)[0]");
  await click("#p-mine .cas");
  await click(`#p-grid .ps[data-k='${free}']`);
  await idle();
  check(await evaluate(`__mgr.S.unit.rows.some((r) => r.source === 'pool' && r.slot === ${free})`), `POOL: a MY preset picked, then put on place ${free} (POOL REF)`);
  await click(`#p-grid .ps[data-k='${free}']`);
  await click("#p-clear");
  await idle();
  check(await evaluate(`!__mgr.S.unit.rows.some((r) => r.source === 'pool' && r.slot === ${free})`), `POOL: CLEAR took it out again (${free})`);

  // IMPORT: synthetic Digitone II files
  await click("#t-import");
  const a = dn2pstZip(wrapDn2Record(dn2RecordFromParams({ algo: 2 }, { name: "SYNTH BASS", tagBits: (1 << 10) | (1 << 24) })), { FormatVersion: 1, ProductType: ["24", "38"], Payload: "SYNTH BASS.dn2snd", FileType: "Sound", FirmwareVersion: "1.10D", MetaInfo: { Tags: ["BASS", "DARK"] } });
  const b = wrapDn2Record(dn2RecordFromParams({}, { name: "SYNTH PAD" }));
  const w = dn2RecordFromParams({}, { name: "WAVE TEST" }); w[244] = 1;
  const files = [["SYNTH BASS.dn2pst", a], ["SYNTH PAD.dn2snd", b], ["WAVE TEST.dn2snd", wrapDn2Record(w)]].map(([n, x]) => ({ n, b64: Buffer.from(x).toString("base64") }));
  await evaluate(`__mgr.importer.add(${JSON.stringify(files)}.map((f) => new File([Uint8Array.from(atob(f.b64), (c) => c.charCodeAt(0))], f.n)))`);
  await waitFor("document.querySelectorAll('#i-rows .ir').length === 3");
  await shot("manager-05-import.png");
  check(await evaluate("document.querySelectorAll('#i-rows .ir.no').length === 1 && document.getElementById('i-send').textContent.includes('2')"), "IMPORT: three files, the WAVETONE one greyed, SEND 2");
  const before = await evaluate("__mgr.S.unit.rows.filter((r) => r.source === 'mine').length");
  await click("#i-send");
  await waitFor(`__mgr.S.unit.rows.filter((r) => r.source === 'mine').length === ${before + 2}`, 20000);
  await idle();
  check(await evaluate("['SYNTH BASS', 'SYNTH PAD'].every((n) => __mgr.S.unit.rows.some((r) => r.source === 'mine' && r.name === n))"), "IMPORT: sent: both in MY PRESETS");
  check(await evaluate("Object.values(__mgr.S.web.tags).some((t) => t.includes('DARK'))"), "IMPORT: the Digitone tag FIZZ has no tag for (DARK) kept as a web tag");
  check(await noScroll(), "IMPORT at 1280: no horizontal scroll");

  // the phone
  await size(390, 844, true);
  for (const t of ["backup", "library", "pool", "import"]) {
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
