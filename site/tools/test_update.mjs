// SPDX-License-Identifier: GPL-3.0-only
// A new deploy reaches a visitor who has the old one cached, in headless Chromium:
//
//   node site/tools/test_update.mjs
//
// A static server with GitHub Pages' caching (Cache-Control: max-age=600, ETags) serves an assembled site (build A,
// tools/assemble.mjs). The visitor plays it: a step added, FIZZ's autosave kept. Build B is deployed over it (another
// build of the same files, and fizz.json naming a new FIZZ version, as a release does). Then:
//   1. the open page notices it (the tab regains focus): "FIZZ <B's version> is out." with Reload;
//   2. a navigation back to the page within the 10 minutes gets the cached old HTML, which notices it at once;
//   3. Reload: every module, the worklet, the engine and Connect FM-1's modules come with B's build (none from A),
//      the page runs B, and the projects came along: the A version's save is copied to B's (A's kept as it was), and
//      FIZZ shows the step that was added.
import fs from "node:fs";
import os from "node:os";
import path from "node:path";
import http from "node:http";
import { spawn, execFileSync } from "node:child_process";
import { fileURLToPath } from "node:url";

const HERE = path.dirname(fileURLToPath(import.meta.url));
const OUT = path.resolve(HERE, "../.build/test");
const CHROMIUM = "/Applications/Chromium.app/Contents/MacOS/Chromium";
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
fs.mkdirSync(OUT, { recursive: true });
let fails = 0;
const check = (ok, what) => { console.log(`${ok ? "ok  " : "FAIL"}  ${what}`); if (!ok) fails++; };

// ---- the deploy folder and a server that caches as GitHub Pages does
const DIR = fs.mkdtempSync(path.join(os.tmpdir(), "fizz-deploy-"));
const assemble = (...a) => execFileSync("node", [path.join(HERE, "assemble.mjs"), DIR, ...a], { encoding: "utf8" }).trim();
const TYPES = { ".html": "text/html", ".js": "text/javascript", ".mjs": "text/javascript", ".css": "text/css", ".json": "application/json", ".wasm": "application/wasm", ".woff": "font/woff", ".txt": "text/plain" };
const served = [];
const server = http.createServer((req, res) => {
  const u = new URL(req.url, "http://x");
  let f = path.join(DIR, decodeURIComponent(u.pathname));
  if (f.endsWith("/")) f += "index.html";
  if (!f.startsWith(DIR) || !fs.existsSync(f)) { res.writeHead(404); return res.end(); }
  const st = fs.statSync(f), etag = `"${st.size}-${st.mtimeMs}"`;
  served.push(u.pathname + u.search);
  if (req.headers["if-none-match"] === etag) { res.writeHead(304, { ETag: etag, "Cache-Control": "max-age=600" }); return res.end(); }
  res.writeHead(200, { "Content-Type": TYPES[path.extname(f)] || "application/octet-stream", ETag: etag, "Cache-Control": "max-age=600" });
  res.end(fs.readFileSync(f));
});
await new Promise((r) => server.listen(0, "127.0.0.1", r));
const URL0 = `http://127.0.0.1:${server.address().port}/`;

// ---- Chromium over the DevTools protocol
const PORT = 9300 + Math.floor(Math.random() * 400);
const profile = fs.mkdtempSync(path.join(os.tmpdir(), "fizz-update-"));
const chrome = spawn(CHROMIUM, ["--headless=new", "--use-mock-keychain", "--password-store=basic", "--no-first-run", "--disable-gpu",
  `--user-data-dir=${profile}`, `--remote-debugging-port=${PORT}`, "--autoplay-policy=no-user-gesture-required", "--disable-audio-output" /* (a fake output: not the sound device) */, "--window-size=1280,800", "about:blank"], { stdio: "ignore" });
process.on("exit", () => { try { chrome.kill("SIGKILL"); } catch (_) {} });
let ws = null, id = 0;
const pending = new Map(), errors = [];
for (let i = 0; i < 100 && !ws; i++) {
  try { const page = (await (await fetch(`http://127.0.0.1:${PORT}/json/list`)).json()).find((t) => t.type === "page"); if (page) ws = new WebSocket(page.webSocketDebuggerUrl); } catch (_) {}
  if (!ws) await sleep(100);
}
await new Promise((r) => { ws.onopen = r; });
ws.onmessage = (ev) => {
  const m = JSON.parse(ev.data);
  if (m.id && pending.has(m.id)) { const p = pending.get(m.id); pending.delete(m.id); m.error ? p.j(new Error(m.error.message)) : p.r(m.result); }
  else if (m.method === "Runtime.exceptionThrown") errors.push(m.params.exceptionDetails.exception ? m.params.exceptionDetails.exception.description : m.params.exceptionDetails.text);
  else if (m.method === "Runtime.consoleAPICalled" && m.params.type === "error") errors.push(m.params.args.map((a) => a.value ?? a.description).join(" "));
};
const send = (method, params = {}) => new Promise((r, j) => { const i = ++id; pending.set(i, { r, j }); ws.send(JSON.stringify({ id: i, method, params })); });
const evaluate = async (expr) => { const r = await send("Runtime.evaluate", { expression: expr, awaitPromise: true, returnByValue: true }); if (r.exceptionDetails) throw new Error(r.exceptionDetails.text + " " + (r.exceptionDetails.exception || {}).description); return r.result.value; };
const waitFor = async (expr, ms = 15000) => { const t = Date.now(); while (Date.now() - t < ms) { try { if (await evaluate(expr)) return true; } catch (_) {} await sleep(100); } return false; };
// (the new document's FIZZ, not the old one's: a mark set before each navigation is gone once it has been replaced)
const booted = () => waitFor("!window.__leaving && window.FIZZ && FIZZ.state.info && FIZZ.state.frameSeq > 0");
const leave = async () => { try { await evaluate("window.__leaving = 1"); } catch (_) { /* (no page yet) */ } };
const tap = async (b) => { await evaluate(`FIZZ.controls.holdBtn(${b}, 4, true)`); await sleep(120); await evaluate(`FIZZ.controls.holdBtn(${b}, 4, false)`); await sleep(150); };
const tapKey = async (k) => { await evaluate(`FIZZ.controls.holdKey(${k}, 4, true)`); await sleep(120); await evaluate(`FIZZ.controls.holdKey(${k}, 4, false)`); await sleep(150); };
// the LCD's step rows (the bottom 60 lines: the page's 16 steps; FIZZ's pictures above them animate, and the steps last
// touched are highlighted, so screens are compared by which they are closer to)
const HASH = "Array.from(FIZZ.state.pixels.subarray(180 * 240))";
const diff = (x, y) => { let n = 0; for (let i = 0; i < x.length; i++) if (x[i] !== y[i]) n++; return n; };
const shot = async (f) => fs.writeFileSync(path.join(OUT, f), Buffer.from((await send("Page.captureScreenshot", { format: "png" })).data, "base64"));

try {
  await send("Page.enable"); await send("Runtime.enable");
  // build A, played
  const a = assemble();
  const A = JSON.parse(fs.readFileSync(path.join(DIR, "fizz.json"), "utf8"));
  console.log(`      ${a}`);
  await send("Page.navigate", { url: URL0 });
  check(await booted(), `build ${A.site} (FIZZ ${A.version}) boots`);
  check(await evaluate("FIZZ.BUILD") === A.site, `the page runs build ${A.site}`);
  await tap(6);
  await sleep(600);
  const plain = await evaluate(HASH);
  const t0 = await evaluate("Date.now()");
  await tapKey(2); await tapKey(4);
  check(await waitFor(`(() => { const s = FIZZ.state.saves.find(r => r.name === 'Autosave'); return !!s && Date.parse(s.saved) > ${t0}; })()`),
    "the edit is kept in IndexedDB (A's version)");
  const edited = await evaluate(HASH);

  // build B deployed over it: a new build of the site, and a new FIZZ version in fizz.json
  const NEWV = A.version.replace(/^(\d+)\.(\d+)\.(\d+).*$/, (_, x, y, z) => `${x}.${y}.${+z + 1}-update-test`);
  const b = assemble("--salt", "b", "--fizz-version", NEWV);
  const B = JSON.parse(fs.readFileSync(path.join(DIR, "fizz.json"), "utf8"));
  console.log(`      ${b}`);
  check(B.site !== A.site, `a new build: ${A.site} -> ${B.site}`);

  // 1. the open page notices when the tab regains focus
  await evaluate("window.dispatchEvent(new Event('focus'))");
  check(await waitFor("!document.getElementById('update').hidden", 5000), `the open page notices: "${await evaluate("document.getElementById('update-text').textContent")}"`);
  await shot("update-note.png");

  // 2. a navigation back within the cache's 10 minutes: the old HTML, which notices at once
  served.length = 0;
  await send("Page.navigate", { url: "about:blank" }); await sleep(300);
  await leave();
  await send("Page.navigate", { url: URL0 });
  await booted();
  const stale = await evaluate("FIZZ.BUILD");
  const noted = await waitFor("!document.getElementById('update').hidden", 5000);
  check(stale === B.site || noted, stale === B.site ? "a navigation got B's page already" : `a navigation got the cached page (build ${stale}) and it says so at once`);

  // 3. Reload
  served.length = 0;
  await leave(); await send("Page.reload", {});
  check(await booted(), "after Reload FIZZ boots");
  const now = await evaluate("({ build: FIZZ.BUILD, version: FIZZ.state.meta.version, res: performance.getEntriesByType('resource').map(e => e.name) })");
  // (what the server was asked for since the Reload: the worklet and the engine load outside the page's resource timing)
  const mine = served.filter((u) => /^\/(js|worklet|engine|remote|css)\//.test(u));
  const old = mine.filter((u) => u.includes(`v=${A.site}`)), bare = mine.filter((u) => !u.includes("?v="));
  check(now.build === B.site && now.version === NEWV, `the page runs build ${now.build}, FIZZ ${now.version}`);
  check(old.length === 0 && bare.length === 0 && mine.includes(`/worklet/host.js?v=${B.site}`) && mine.includes(`/engine/engine.js?v=${B.site}`) && mine.includes(`/js/main.js?v=${B.site}`),
    `every asset fetched anew with B's build: ${mine.length} URLs (${old.length} of A's, ${bare.length} unversioned)${old.length || bare.length ? ": " + old.concat(bare).join(" ") : ""}`);
  // (remotefm1.mjs loads only when an FM-1 connects: its URL is made the same way, js/build.js ver())
  check(mine.includes(`/remote/remote.js?v=${B.site}`), "Connect FM-1's module with B's build");
  const saves = await evaluate("FIZZ.state.saves.map(r => r.version + '/' + r.name)");
  check(saves.includes(`${A.version}/Autosave`) && saves.includes(`${NEWV}/Autosave`), `the projects came along, A's save kept: ${saves.join(", ")}`);
  await sleep(800); await tap(6); await sleep(400);
  const back = await evaluate(HASH);
  check(diff(back, edited) < diff(back, plain), `FIZZ ${NEWV} shows the steps changed under ${A.version} (the step rows: ${diff(back, edited)} px from the edited screen, ${diff(back, plain)} from the one before the edit)`)
  await shot("update-reloaded.png");
  // 4. a flash a new FIZZ cannot read (junk under an old version, the only save): said so, nothing deleted
  await evaluate(`(async () => {
    const db = await new Promise((r, j) => { const q = indexedDB.open("fizz-preview", 1); q.onsuccess = () => r(q.result); q.onerror = j; });
    const t = db.transaction("saves", "readwrite"), s = t.objectStore("saves"), k = (v) => ["fizz", "native", v, "Autosave"].join("\\u0001");
    s.delete(k(${JSON.stringify(A.version)})); s.delete(k(${JSON.stringify(NEWV)}));
    const junk = new Uint8Array(1 << 20); let x = 12345; for (let i = 0; i < junk.length; i++) { x ^= x << 13; x ^= x >>> 17; x ^= x << 5; junk[i] = x & 255; }
    s.put({ key: k("0.0.1"), fw: "fizz", engine: "native", version: "0.0.1", name: "Autosave", bytes: junk, saved: new Date().toISOString() });
    await new Promise((r) => { t.oncomplete = r; });
  })()`);
  const NEWV2 = NEWV.replace("-update-test", "-update-test2").replace(/^(\d+)\.(\d+)\.(\d+)/, (_, x, y, z) => `${x}.${y}.${+z + 1}`);
  assemble("--salt", "c", "--fizz-version", NEWV2);
  await leave(); await send("Page.reload", {});
  await booted();
  const warned = await waitFor("/could not read your 0\\.0\\.1 projects/.test(document.getElementById('toast').textContent)", 15000);
  check(warned, `an unreadable old flash is reported: "${await evaluate("document.getElementById('toast').textContent")}"`);
  const kept = await evaluate("FIZZ.state.saves.map(r => r.version + '/' + r.name)");
  check(kept.includes("0.0.1/Autosave"), `the unreadable save is kept: ${kept.join(", ")}`);
  check(errors.length === 0, `no console errors${errors.length ? ": " + errors.join(" | ") : ""}`);
} catch (e) {
  check(false, e.stack || String(e));
} finally {
  try { ws.close(); } catch (_) {}
  server.close();
}
console.log(fails ? `${fails} check(s) failed` : "all passed");
process.exit(fails ? 1 : 0);
