// SPDX-License-Identifier: GPL-3.0-only
// The site in headless Chromium, over the DevTools protocol (no Playwright: Node's own WebSocket):
//
//   node site/tools/test_site.mjs [--url http://127.0.0.1:8765/] [--chromium path] [--dnsnd dir]
//   (serve site/ first, e.g. python3 -m http.server 8765 --bind 127.0.0.1 -d site)
//
//   1. the player loads with no console error; FIZZ boots in the AudioWorklet (info, first frames); screenshot
//   2. audio through the panel interface: HOME, REC tapped, D4 held: the output (an AnalyserNode) is not silent
//   3. the flash: two steps added, FIZZ's autosave reaches IndexedDB; after a reload the steps are back
//   4. the Feedback link: a new issue on FIZZ from feedback.yml, with the version, setup and browser filled in
//   5. the guide renders with no console error; a phone-sized screenshot of both pages
//   6. Digitone sounds: .dnsnd files (--dnsnd, a few of them; never published) read and listed in the panel
// Screenshots: site/.build/test/*.png
import fs from "node:fs";
import path from "node:path";
import os from "node:os";
import { spawn } from "node:child_process";
import { fileURLToPath } from "node:url";
import { feedbackUrl, browserName } from "../js/feedback.js";
import { dn2RecordFromParams, wrapDn2Record, dn2pstZip } from "../js/dn2pst.js";

const HERE = path.dirname(fileURLToPath(import.meta.url));
const OUT = path.resolve(HERE, "../.build/test");
const arg = (k, d) => { const i = process.argv.indexOf(k); return i > 0 ? process.argv[i + 1] : d; };
const URL0 = arg("--url", "http://127.0.0.1:8765/");
const CHROMIUM = arg("--chromium", "/Applications/Chromium.app/Contents/MacOS/Chromium");
const DNSND = arg("--dnsnd", "/Volumes/Q7Media-2025/Projects/Github/ChoralRootFM1/DigitonePresets");
const PORT = 9300 + Math.floor(Math.random() * 400);
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
fs.mkdirSync(OUT, { recursive: true });

let fails = 0;
const check = (ok, what) => { console.log(`${ok ? "ok  " : "FAIL"}  ${what}`); if (!ok) fails++; };
const note = (what) => console.log(`      ${what}`);

// ---- feedback.js in Node first
{
  const u = new URL(feedbackUrl({ meta: { version: "0.6.2-p1.5p", commit: "33054d2e4a6a" }, setup: "fm1",
    ua: "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/141.0.0.0 Safari/537.36" }));
  const q = u.searchParams;
  check(u.origin + u.pathname === "https://github.com/Quixotic7/FIZZ/issues/new" && q.get("template") === "feedback.yml" &&
        q.get("version") === "0.6.2-p1.5p (33054d2)" && q.get("setup") === "FM-1 with RemoteFM1" && q.get("browser") === "Chrome 141 on macOS",
        `feedbackUrl (Node): ${u.href.slice(0, 120)}...`);
  check(browserName("Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:131.0) Gecko/20100101 Firefox/131.0") === "Firefox 131 on Windows", "browserName: Firefox on Windows");
}

// ---- Chromium
const profile = fs.mkdtempSync(path.join(os.tmpdir(), "fizz-site-"));
const chrome = spawn(CHROMIUM, ["--headless=new", "--use-mock-keychain", "--password-store=basic", "--no-first-run", "--no-default-browser-check",
  "--disable-gpu", `--user-data-dir=${profile}`, `--remote-debugging-port=${PORT}`, "--autoplay-policy=no-user-gesture-required",
  "--hide-scrollbars", "--window-size=1280,800", "about:blank"], { stdio: "ignore" });
const killChrome = () => { try { chrome.kill("SIGKILL"); } catch (_) {} };
process.on("exit", killChrome);

let ws = null, id = 0;
const pending = new Map(), listeners = [];
async function connect() {
  for (let i = 0; i < 100; i++) {
    try {
      const list = await (await fetch(`http://127.0.0.1:${PORT}/json/list`)).json();
      const page = list.find((t) => t.type === "page");
      if (page) { ws = new WebSocket(page.webSocketDebuggerUrl); break; }
    } catch (_) { /* not up yet */ }
    await sleep(100);
  }
  if (!ws) throw new Error("Chromium did not start");
  await new Promise((r, j) => { ws.onopen = r; ws.onerror = j; });
  ws.onmessage = (ev) => {
    const m = JSON.parse(ev.data);
    if (m.id && pending.has(m.id)) { const p = pending.get(m.id); pending.delete(m.id); m.error ? p.j(new Error(m.error.message)) : p.r(m.result); }
    else if (m.method) for (const l of listeners) l(m);
  };
}
const send = (method, params = {}) => new Promise((r, j) => { const i = ++id; pending.set(i, { r, j }); ws.send(JSON.stringify({ id: i, method, params })); });
async function evaluate(expr) {
  const r = await send("Runtime.evaluate", { expression: expr, awaitPromise: true, returnByValue: true });
  if (r.exceptionDetails) throw new Error(`evaluate: ${r.exceptionDetails.exception ? r.exceptionDetails.exception.description : r.exceptionDetails.text}`);
  return r.result.value;
}
async function waitFor(expr, ms = 10000) {
  const t = Date.now();
  while (Date.now() - t < ms) { try { if (await evaluate(expr)) return true; } catch (_) { /* (navigating) */ } await sleep(100); }
  return false;
}
async function shot(file, clip) {
  const r = await send("Page.captureScreenshot", { format: "png", ...(clip ? { clip } : {}) });
  fs.writeFileSync(path.join(OUT, file), Buffer.from(r.data, "base64"));
}
const errors = [];
listeners.push((m) => {
  if (m.method === "Runtime.exceptionThrown") errors.push(m.params.exceptionDetails.exception ? m.params.exceptionDetails.exception.description : m.params.exceptionDetails.text);
  if (m.method === "Runtime.consoleAPICalled" && (m.params.type === "error" || m.params.type === "warning"))
    errors.push(`console.${m.params.type}: ${m.params.args.map((a) => a.value ?? a.description).join(" ")}`);
  if (m.method === "Log.entryAdded" && m.params.entry.level === "error") errors.push(`log: ${m.params.entry.text} ${m.params.entry.url || ""}`);
});
async function load(url) {
  errors.length = 0;
  try { await evaluate("window.__leaving = 1"); } catch (_) { /* (no page yet) */ }
  await send("Page.navigate", { url });
  await waitFor("!window.__leaving && document.readyState === 'complete'", 15000);   // (the new document, not the old one)
}

try {
  await connect();
  await send("Page.enable"); await send("Runtime.enable"); await send("Log.enable");

  // 1. the player
  await load(URL0);
  const booted = await waitFor("window.FIZZ && FIZZ.state.info && FIZZ.state.frameSeq > 0", 15000);
  const info = await evaluate("FIZZ.state.info && { name: FIZZ.state.info.name, version: FIZZ.state.info.version, sr: FIZZ.player.sampleRate, running: FIZZ.state.running }");
  check(booted, `FIZZ boots in the page's AudioWorklet: ${JSON.stringify(info)}`);
  await sleep(1500);
  await shot("page-boot.png");
  const meta = await evaluate("FIZZ.state.meta");
  const ver = await evaluate("document.querySelector('[data-version]').textContent");
  check(ver === meta.version, `the header shows the version (${ver})`);

  // 2. audio through the panel interface (panel.js labels: HOME 6, REC 11; SRC.POINTER 4)
  const tap = async (b) => { await evaluate(`FIZZ.controls.holdBtn(${b}, 4, true)`); await sleep(120); await evaluate(`FIZZ.controls.holdBtn(${b}, 4, false)`); await sleep(150); };
  const tapKey = async (k) => { await evaluate(`FIZZ.controls.holdKey(${k}, 4, true)`); await sleep(120); await evaluate(`FIZZ.controls.holdKey(${k}, 4, false)`); await sleep(150); };
  await tap(6); await tap(11);
  await evaluate("FIZZ.peak()");
  let before = 0; for (let i = 0; i < 5; i++) { before = Math.max(before, await evaluate("FIZZ.peak()")); await sleep(40); }
  await evaluate("FIZZ.controls.holdKey(9, 4, true, 100)");
  let peak = 0; for (let i = 0; i < 12; i++) { await sleep(40); peak = Math.max(peak, await evaluate("FIZZ.peak()")); }
  await evaluate("FIZZ.controls.holdKey(9, 4, false)");
  check(peak > 0.002, `REC + D4 through the panel: output peak ${peak.toFixed(4)} (before the key ${before.toFixed(4)})`);
  await tap(11);                                           // (REC again: back to the steps)
  await shot("page-keyboard.png");
  // the render load the worklet reports (host.js "load" messages are not wired here: measured as audio time per wall time)
  const ctxTime = await evaluate("(async () => { const c = FIZZ.player.ctx, a = c.currentTime, t = performance.now(); await new Promise(r => setTimeout(r, 2000)); return (c.currentTime - a) / ((performance.now() - t) / 1000); })()");
  note(`audio clock against the wall clock over 2 s: ${ctxTime.toFixed(3)} (1.000: real time, no stall)`);

  // 3. the flash: steps W2, W3 (keys 2, 4) on a fresh unit, autosave, reload
  await tap(6);
  const t0 = await evaluate("Date.now()");
  await tapKey(2); await tapKey(4);
  await sleep(500);
  // the LCD's pixels (FIZZ's screens animate a little: compared by how many pixels differ, not exactly)
  const hash = "Array.from(FIZZ.state.pixels)";
  // (the bottom rows: the page's 16 steps; the picture above them may animate)
  const diff = (x, y) => { let n = 0; for (let i = 180 * 240; i < x.length; i++) if (x[i] !== y[i]) n++; return n; };
  const saved = await waitFor(`(() => { const s = FIZZ.state.saves.find(r => r.name === 'Autosave'); return !!s && Date.parse(s.saved) > ${t0}; })()`, 15000);
  const lag = await evaluate(`Date.parse(FIZZ.state.saves.find(r => r.name === 'Autosave').saved) - ${t0}`);
  check(saved, `FIZZ's autosave of the edit reached this browser's IndexedDB ${lag} ms after the first key`);
  const edited = await evaluate(hash);              // (settled: a tapped step's own flash is over)
  await shot("page-edited.png");
  await load(URL0);
  await waitFor("window.FIZZ && FIZZ.state.info && FIZZ.state.frameSeq > 0", 15000);
  await sleep(800);
  await tap(6);
  await sleep(400);
  const back = await evaluate(hash);
  await shot("page-reloaded.png");
  const dBack = diff(back, edited);
  // a fresh profile's step view for contrast: Start blank, HOME
  await evaluate("(async () => { await FIZZ.boot(null, { blank: true }); })()");
  await waitFor("FIZZ.state.info", 8000);
  await sleep(800); await tap(6); await sleep(400);
  const blankHash = await evaluate(hash);
  const dBlank = diff(blankHash, edited);
  // (closer to the edited screen than to a blank unit's: a highlight of the steps last touched may differ)
  const dBackBlank = diff(back, blankHash);
  check(dBack < dBackBlank && dBlank > 0, `after a reload FIZZ comes back with the steps (the step rows: ${dBack} px differ from the edited screen, ${dBackBlank} from a blank unit's)`);

  // 4. the Feedback link
  const href = await evaluate("document.querySelector('a[data-feedback]').href");
  const u = new URL(href), q = u.searchParams;
  check(u.pathname === "/Quixotic7/FIZZ/issues/new" && q.get("template") === "feedback.yml" && q.get("version").startsWith(meta.version) &&
        q.get("setup").startsWith("On-screen") && /Chrom/.test(q.get("browser")), `Feedback: ${href}`);

  // 6. Digitone sounds (local test files only): read, sent to FIZZ's library, kept over a reload
  const files = fs.existsSync(DNSND) ? fs.readdirSync(DNSND).filter((f) => f.endsWith(".dnsnd")).sort().slice(0, 4) : [];
  const fileJs = (list) => JSON.stringify(list.map((f) => ({ name: f, b64: fs.readFileSync(path.join(DNSND, f)).toString("base64") })));
  const addJs = (json) => `(async () => { const list = ${json}.map(x => new File([Uint8Array.from(atob(x.b64), c => c.charCodeAt(0))], x.name)); await FIZZDigitone.add(list); })()`;
  if (files.length === 4) {
    await evaluate(addJs(fileJs(files.slice(0, 3)).replace(/]$/, ',{"name":"broken.dnsnd","b64":"UEsDBAAA"}]')));
    await waitFor("!!FIZZDigitone.fizz", 8000);
    const items = await evaluate("FIZZDigitone.items.map(i => i.ok ? { name: i.sound.name, notes: i.sound.notes.length, blob: i.sound.blob.length } : { error: i.error })");
    check(items.length === 4 && items.slice(0, 3).every((i) => i.blob === 96) && items[3].error,
      `Digitone: 3 .dnsnd read and converted, a broken file refused: ${JSON.stringify(items)}`);
    const who = await evaluate("FIZZDigitone.fizz");
    check(who === `FIZZ ${meta.version}`, `the panel shows who answers the identity query: "${who}"`);
    const tSend = await evaluate("Date.now()");
    await evaluate("FIZZDigitone.send()");
    const res = await evaluate("FIZZDigitone.items.filter(i => i.ok).map(i => i.result)");
    check(res.length === 3 && res.every((r, k) => r && r.ok && !r.pool && r.slot === k + 1), `sent through the page into the running wasm: ${JSON.stringify(res)}`);
    // P1.5x: the sends are kept (no "not imported" note), the Digitone's tags shown as FIZZ's
    const tg = await evaluate(`[...document.querySelectorAll(".dn-item")].slice(0, 3).map((li) => ({ tags: [...li.querySelectorAll(".dn-tag")].map((c) => c.textContent),
      sendLost: /send .* not imported/.test(li.textContent) }))`);
    check(tg.every((t) => !t.sendLost) && tg[1].tags.join() === "PAD", `tags as chips and no lost sends: ${JSON.stringify(tg)}`);
    await evaluate("document.querySelector('.dn-notes') && (document.querySelector('.dn-notes').open = true)");
    await shot("page-digitone.png");
    const kept = await waitFor(`(() => { const s = FIZZ.state.saves.find(r => r.name === 'Autosave'); return !!s && Date.parse(s.saved) > ${tSend}; })()`, 15000);
    check(kept, "the imported sounds reached the autosave");
    await load(URL0);
    await waitFor("window.FIZZ && FIZZ.state.info && FIZZ.state.frameSeq > 0", 15000);
    await evaluate(addJs(fileJs(files.slice(3))));
    await waitFor("!!FIZZDigitone.fizz", 8000);
    await evaluate("FIZZDigitone.send()");
    const r4 = await evaluate("FIZZDigitone.items[0].result");
    check(r4 && r4.ok && r4.slot === 4, `after a reload the library still holds them: the next sound goes to LIBRARY ${r4 && r4.slot}`);
    await sleep(700);
    // an older Digitone file (the owner's, read in place): a grey mark, nothing counted as lost, and it imports
    const OLD = "/Volumes/Q7Media-2025/Projects/Github/ChoralRootFM1/Digitone2Presets/H003 BRITE2.dnsnd";
    if (fs.existsSync(OLD)) {
      await evaluate(addJs(JSON.stringify([{ name: path.basename(OLD), b64: fs.readFileSync(OLD).toString("base64") }])));
      await evaluate("FIZZDigitone.send()");
      const li = await evaluate(`(() => { const li = [...document.querySelectorAll(".dn-item")].pop(); const r = FIZZDigitone.items[FIZZDigitone.items.length - 1].result;
        return { mark: (li.querySelector(".dn-mark") || {}).textContent, lost: !!li.querySelector(".dn-notes"), whole: li.textContent.includes("converted whole"), ok: r && r.ok, slot: r && r.slot }; })()`);
      check(li.mark === "older Digitone file (OS 1.42)" && !li.lost && li.whole && li.ok,
        `${path.basename(OLD)} in the panel: mark "${li.mark}", ${li.lost ? "a lost count shown" : "no lost count"}, ${li.ok ? `in LIBRARY ${li.slot}` : "not stored"}`);
    }
    // Digitone II: the owner's BASSSINE.dn2pst (read in place) with its DN2 chip, stored; a WAVETONE one refused
    const DN2 = "/Volumes/Q7Media-2025/Projects/Github/ChoralRootFM1/Digitone2Presets/BASSSINE.dn2pst";
    if (fs.existsSync(DN2)) {
      const wave = dn2RecordFromParams({}, { name: "WAVE TEST" }); wave[244] = 1;
      const waveZip = dn2pstZip(wrapDn2Record(wave), { FormatVersion: 1, ProductType: ["24", "38"], Payload: "WAVE TEST.dn2snd", FileType: "Sound", FirmwareVersion: "1.10D", MetaInfo: { Tags: [] } });
      const before = await evaluate("FIZZDigitone.items.length");
      await evaluate(addJs(JSON.stringify([{ name: path.basename(DN2), b64: fs.readFileSync(DN2).toString("base64") }, { name: "WAVE TEST.dn2pst", b64: Buffer.from(waveZip).toString("base64") }])));
      await evaluate("FIZZDigitone.send()");
      const d2 = await evaluate(`(() => { const lis = [...document.querySelectorAll(".dn-item")].slice(${before}); const it = FIZZDigitone.items.slice(${before});
        return { chip: !!lis[0].querySelector(".dn-chip"), name: it[0].ok && it[0].sound.name, res: it[0].result, refused: lis[1].querySelector(".dn-err") && lis[1].querySelector(".dn-err").textContent }; })()`);
      check(d2.chip && d2.res && d2.res.ok && /^Not importable: .*WAVETONE/.test(d2.refused || ""),
        `Digitone II in the panel: "${d2.name}" with its DN2 chip, ${d2.res && d2.res.ok ? `in LIBRARY ${d2.res.slot}` : "not stored"}; ${d2.refused}`);
    } else note(`no ${DN2}: the Digitone II check skipped`);
    await shot("page-digitone-reloaded.png");
    await evaluate("FIZZDigitone.close()");
  } else note("Digitone: no .dnsnd files to try (--dnsnd)");
  // 7. the live labels (the stand-in: ?labels=stub, until FIZZ's LABELS SysEx lands): drawn, flipped by SHIFT, toggled
  await load(new URL("?labels=stub", URL0).href);
  await waitFor("window.FIZZ && FIZZ.state.info && FIZZ.labels.visible", 15000);
  const lab = () => evaluate(`(() => { const L = FIZZ.labels; const t = (d) => d.firstChild.textContent;
    return { shown: !L.el.hidden, fx: t(L.btns[0]), octdn: t(L.btns[12]), b1: t(L.keys[1]), w1: t(L.keys[0]), sel: t(L.knobs[0]),
      glow: document.querySelectorAll(".lbl.glow").length, drawn: [...document.querySelectorAll(".lbl")].filter((d) => d.offsetParent && !d.classList.contains("none")).length }; })()`);
  const base = await lab();
  check(base.shown && base.fx === "EFFECTS" && base.b1 === "NOTE" && base.w1 === "STEP 1" && base.sel === "PAGE" && base.drawn >= 40,
    `labels drawn on the panel: ${base.drawn} (FX "${base.fx}", B1 "${base.b1}", W1 "${base.w1}", SELECT "${base.sel}")`);
  await shot("labels-base.png");
  await evaluate("FIZZ.controls.holdBtn(5, 4, true)");             // SHIFT (GLO) held
  await sleep(120);
  const sh = await lab();
  await shot("labels-shift.png");
  check(sh.octdn === "UNDO" && sh.b1 === "SNAPSHOT" && sh.glow > 5, `SHIFT held flips them: OCT- "${sh.octdn}", B1 "${sh.b1}", ${sh.glow} glowing`);
  await evaluate("FIZZ.controls.holdBtn(5, 4, false)");
  await sleep(120);
  check((await lab()).b1 === "NOTE", "SHIFT let go: the step view's labels again");
  const hov = await evaluate(`(() => { const c = document.getElementById("device"), r = c.getBoundingClientRect(), p = FIZZ.labels.device.toCss(518, 195);
    c.dispatchEvent(new PointerEvent("pointermove", { clientX: r.left + p.x, clientY: r.top + p.y, pointerType: "mouse", bubbles: true }));
    const d = FIZZ.labels.btns[0]; return { hover: d.classList.contains("hover"), key: getComputedStyle(d.querySelector("kbd")).display !== "none" && d.querySelector("kbd").textContent,
      others: [...document.querySelectorAll(".lbl kbd")].filter((k) => getComputedStyle(k).display !== "none").length }; })()`);
  check(hov.hover && /\bC\b/.test(hov.key) && hov.others === 1, `the computer key only on hover: FX shows "${hov.key}", ${hov.others} key shown`);
  await evaluate("FIZZ.setLabels(false)");
  const off = await evaluate("({ hidden: FIZZ.labels.el.hidden, labelled: FIZZ.labels.device.labelled, kept: JSON.parse(localStorage.getItem('fizz-preview')).labels })");
  await shot("labels-off.png");
  check(off.hidden && !off.labelled && off.kept === false, `the toggle hides them (the printed names back) and is remembered: ${JSON.stringify(off)}`);
  await load(new URL("?labels=stub", URL0).href);
  await waitFor("window.FIZZ && FIZZ.state.info", 15000);
  check(await evaluate("FIZZ.labels.el.hidden"), "after a reload they stay off");
  await evaluate("FIZZ.setLabels(true)");
  check(errors.length === 0, `the player: no console errors${errors.length ? ": " + errors.join(" | ") : ""}`);

  // 5. the guide, and phone-sized pages
  await load(new URL("guide.html", URL0).href);
  await sleep(500);
  const gh = await evaluate("({ h1: document.querySelector('h1') && document.querySelector('h1').textContent, sections: document.querySelectorAll('section').length, keys: !!document.getElementById('keys'), fb: document.querySelector('a[data-feedback]') && document.querySelector('a[data-feedback]').href, w: document.documentElement.scrollWidth, cw: document.documentElement.clientWidth })");
  check(gh.h1 && gh.sections >= 5 && gh.keys && /issues\/new/.test(gh.fb || ""), `the guide: "${gh.h1}", ${gh.sections} sections, the key map, Feedback ${(gh.fb || "").slice(0, 70)}...`);
  const full = await evaluate("document.documentElement.scrollHeight");
  await send("Emulation.setDeviceMetricsOverride", { width: 1280, height: Math.min(full, 6000), deviceScaleFactor: 1, mobile: false });
  await sleep(300);
  await shot("guide.png");
  check(errors.length === 0, `the guide: no console errors${errors.length ? ": " + errors.join(" | ") : ""}`);
  await send("Emulation.setDeviceMetricsOverride", { width: 390, height: 844, deviceScaleFactor: 2, mobile: true });
  await sleep(300);
  const gw = await evaluate("document.documentElement.scrollWidth <= document.documentElement.clientWidth");
  check(gw, "the guide at 390 px: no horizontal scroll");
  await shot("guide-phone.png");
  await load(new URL("?labels=stub", URL0).href);
  await waitFor("window.FIZZ && FIZZ.state.frameSeq > 0 && FIZZ.labels.visible", 15000);
  await sleep(800);
  const pw = await evaluate("document.documentElement.scrollWidth <= document.documentElement.clientWidth");
  note(`the phone's labels: ${await evaluate("['knobs', 'btns', 'keys'].filter((g) => !FIZZ.labels.el.classList.contains('no-' + g)).join(', ') || 'none (too small)'")}`);
  check(pw, "the player at 390 px: no horizontal scroll");
  await shot("page-phone.png");
  await send("Emulation.clearDeviceMetricsOverride");
} catch (e) {
  check(false, (e.stack || String(e)) + (errors.length ? "\n      page errors: " + errors.join(" | ") : ""));
} finally {
  try { ws && ws.close(); } catch (_) {}
  killChrome();
}
console.log(`\nscreenshots in ${path.relative(process.cwd(), OUT)}`);
console.log(fails ? `${fails} check(s) failed` : "all passed");
process.exit(fails ? 1 : 0);
