// SPDX-License-Identifier: GPL-3.0-only
// The site in headless Chromium, over the DevTools protocol (no Playwright: Node's own WebSocket):
//
//   node site/tools/test_site.mjs [--url http://127.0.0.1:8765/] [--chromium path] [--dnsnd dir] [--real-audio]
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
import { spawn, execFileSync } from "node:child_process";
import { fileURLToPath } from "node:url";
import { feedbackUrl, browserName } from "../js/feedback.js";
import { dn2RecordFromParams, wrapDn2Record, dn2pstZip } from "../js/dn2pst.js";
import { dnReadZip } from "../js/fm1dnsnd.js";
import { readDn2 } from "../js/digitone.js";
import { soundFromStoreFrame } from "../js/library.js";
import { restoreFrame } from "../js/fizz_sysex.js";
import { fizzValues } from "../js/fizz2dn.js";
import { toSelectedTrack } from "../js/midimap.js";

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

// ---- the MIDI keyboard's channel (Node): to FIZZ's channel 16 (the selected track), system messages as they are
{
  const t = (b) => Array.from(toSelectedTrack(b)).join(" ");
  check(t([0x90, 60, 100]) === "159 60 100" && t([0x83, 60, 0]) === "143 60 0" && t([0xb0, 1, 64]) === "191 1 64" && t([0xe2, 0, 64]) === "239 0 64" &&
        t([0xf8]) === "248" && t([0xf0, 0x7d, 1, 0xf7]) === "240 125 1 247", "a MIDI keyboard's channel messages go to channel 16 (the selected track), system messages pass");
}

// ---- Chromium (its fake audio output: the AudioContext runs on its own clock, not this machine's sound device, which
// may be busy, asleep or missing; --real-audio uses the device)
const profile = fs.mkdtempSync(path.join(os.tmpdir(), "fizz-site-"));
const chrome = spawn(CHROMIUM, ["--headless=new", "--use-mock-keychain", "--password-store=basic", "--no-first-run", "--no-default-browser-check",
  "--disable-gpu", `--user-data-dir=${profile}`, `--remote-debugging-port=${PORT}`, "--autoplay-policy=no-user-gesture-required",
  "--hide-scrollbars", "--window-size=1280,800", ...(process.argv.includes("--real-audio") ? [] : ["--disable-audio-output"]), "about:blank"], { stdio: "ignore" });
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
// a dialog (a page asking before it closes) would stall the run: accepted, and counted as an error
listeners.push((m) => { if (m.method === "Page.javascriptDialogOpening") { errors.push(`dialog: ${m.params.type} ${m.params.message}`); send("Page.handleJavaScriptDialog", { accept: true }); } });
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
    check(items.length === 4 && items.slice(0, 3).every((i) => i.blob === 96 || i.blob === 128) && items[3].error,
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
  // 7a. FIZZ's own live labels (P1.6l, subscribed): the step view's, then SHIFT held: SELECT TRACK, B8 FOLLOW
  await load(URL0);
  await waitFor("window.FIZZ && FIZZ.state.info && FIZZ.labels.visible", 15000);
  await evaluate("FIZZ.controls.holdBtn(6, 4, true)"); await sleep(120); await evaluate("FIZZ.controls.holdBtn(6, 4, false)"); await sleep(400);
  const real = () => evaluate("(() => { const L = FIZZ.labels, t = (d) => d.firstChild.textContent; return { sel: t(L.knobs[0]), w1: t(L.keys[0]), b1: t(L.keys[1]), b8: t(L.keys[17]), f1: t(L.btns[12]) }; })()");
  const r0 = await real();
  await evaluate("FIZZ.controls.holdBtn(5, 4, true)");
  await waitFor("FIZZ.labels.knobs[0].firstChild.textContent === 'TRACK'", 3000);
  const r1 = await real();
  await shot("labels-fizz-shift.png");
  await evaluate("FIZZ.controls.holdBtn(5, 4, false)");
  await waitFor(`FIZZ.labels.knobs[0].firstChild.textContent === ${JSON.stringify(r0.sel)}`, 3000);
  check(r0.w1 === "STEP 1" && r0.b1 === "NOTE" && r1.sel === "TRACK" && r1.b8 === "FOLLOW" && r1.f1 === "UNDO",
    `FIZZ's labels: the step view SELECT "${r0.sel}", W1 "${r0.w1}", B1 "${r0.b1}"; SHIFT held: SELECT "${r1.sel}", B8 "${r1.b8}", F1 "${r1.f1}"`);
  await shot("labels-fizz.png");
  // P1.6d: ARP tapped, a group (ALGORITHM a click): the MIDI FX screen's labels on the panel
  await evaluate("FIZZ.controls.holdBtn(8, 4, true)"); await sleep(120); await evaluate("FIZZ.controls.holdBtn(8, 4, false)"); await sleep(300);
  await evaluate("FIZZ.controls.turn(1, 1)");
  await waitFor("FIZZ.labels.knobs[2].firstChild.textContent === 'EFFECT'", 3000);
  const mfx = await evaluate("(() => { const L = FIZZ.labels, t = (d) => d.firstChild.textContent; return { algo: t(L.knobs[1]), presets: t(L.knobs[2]), b: [1, 3, 5, 8, 10, 13, 15, 17].map((k) => t(L.keys[k])) }; })()");
  await shot("labels-midifx.png");
  check(mfx.algo === "GROUP" && mfx.presets === "EFFECT" && mfx.b[0] === "EMPTY" && mfx.b[7] === "SLOT 8",
    `the MIDI FX screen on the panel: ALGORITHM "${mfx.algo}", PRESETS "${mfx.presets}", B1..B8 ${mfx.b.join(" / ")}`);
  // P1.6f: the MIDI FX screen in STEPS: the panel's W keys read STEP n and REC KEYS (what a tap switches to); REC
  // tapped: the keys play notes, REC reads STEPS; again: back
  const lbl = "(() => { const L = FIZZ.labels, t = (d) => d.firstChild.textContent; return { w1: t(L.keys[0]), w16: t(L.keys[26]), rec: t(L.btns[11]) }; })()";
  const steps = await evaluate(lbl);
  await shot("labels-midifx-steps.png");
  await evaluate("FIZZ.controls.holdBtn(11, 4, true)"); await sleep(120); await evaluate("FIZZ.controls.holdBtn(11, 4, false)");
  await waitFor("FIZZ.labels.btns[11].firstChild.textContent === 'STEPS'", 3000);
  const keysV = await evaluate(lbl);
  check(steps.w1 === "STEP 1" && steps.w16 === "STEP 16" && steps.rec === "KEYS" && keysV.rec === "STEPS" && !/^STEP/.test(keysV.w1),
    `the MIDI FX screen on the panel: the white keys "${steps.w1}" .. "${steps.w16}", REC "${steps.rec}"; REC tapped: the keys "${keysV.w1}" .., REC "${keysV.rec}"`);
  await evaluate("FIZZ.controls.holdBtn(11, 4, true)"); await sleep(120); await evaluate("FIZZ.controls.holdBtn(11, 4, false)"); await sleep(300);
  await evaluate("FIZZ.controls.holdBtn(6, 4, true)"); await sleep(120); await evaluate("FIZZ.controls.holdBtn(6, 4, false)"); await sleep(300);

  // 7b. Shift-click latching (RemoteFM1 ceb0025), with real mouse events: Shift-click GLO (FIZZ's SHIFT) and the black
  // key B6 (SHIFT + B6: TAP): both stay held, FIZZ gets both (the panel message, its SHIFT labels, its TAP screen);
  // a plain click on GLO lets it go; Shift-click on an empty spot lets the rest go
  {
    const at = async (ux, uy) => evaluate(`(() => { const c = document.getElementById("device"), r = c.getBoundingClientRect(), p = FIZZ.labels.device.toCss(${ux}, ${uy}); return { x: r.left + p.x, y: r.top + p.y }; })()`);
    const click = async (pt, shift) => {
      for (const type of ["mouseMoved", "mousePressed", "mouseReleased"])
        await send("Input.dispatchMouseEvent", { type, x: pt.x, y: pt.y, button: type === "mouseMoved" ? "none" : "left", buttons: type === "mousePressed" ? 1 : 0, clickCount: 1, modifiers: shift ? 8 : 0 });
      await sleep(150);
    };
    await evaluate("FIZZ.controls.holdBtn(6, 4, true)"); await sleep(120); await evaluate("FIZZ.controls.holdBtn(6, 4, false)"); await sleep(400);
    await evaluate("window.__panel = null; { const p = FIZZ.player, f = p.panel.bind(p); p.panel = (m) => { window.__panel = m; f(m); }; }");
    const glo = await at(786, 195), b6 = await at(453, 364), empty = await at(470, 300);
    const lcd0 = await evaluate("Array.from(FIZZ.state.pixels.subarray(0, 240 * 180))");
    await click(glo, true);
    await waitFor("FIZZ.labels.knobs[0].firstChild.textContent === 'TRACK'", 3000);
    await click(b6, true);
    await sleep(400);
    const st = await evaluate("({ glo: FIZZ.controls.btnLatched(5), b6: FIZZ.controls.keyLatched(13), msg: window.__panel && { keys: window.__panel.keys, buttons: window.__panel.buttons }, sel: FIZZ.labels.knobs[0].firstChild.textContent })");
    const lcd1 = await evaluate("Array.from(FIZZ.state.pixels.subarray(0, 240 * 180))");
    await shot("latch.png");
    const changed = lcd1.filter((v, i) => v !== lcd0[i]).length;
    check(st.glo && st.b6 && st.msg && (st.msg.buttons & (1 << 7)) && (st.msg.keys & (1 << 13)) && st.sel === "TRACK" && changed > 500,
      `Shift-click GLO and B6: both latched, FIZZ gets both (buttons ${st.msg && st.msg.buttons.toString(2)}, keys bit 13 ${st.msg && !!(st.msg.keys & (1 << 13))}), SHIFT's labels (SELECT "${st.sel}"), its screen changed (${changed} px: TAP)`);
    await click(glo, false);
    await waitFor("FIZZ.labels.knobs[0].firstChild.textContent !== 'TRACK'", 3000);
    const st2 = await evaluate("({ glo: FIZZ.controls.btnLatched(5), b6: FIZZ.controls.keyLatched(13), buttons: window.__panel.buttons })");
    await click(empty, true);
    const st3 = await evaluate("({ any: FIZZ.controls.anyLatched(), keys: window.__panel.keys })");
    check(!st2.glo && st2.b6 && !(st2.buttons & (1 << 7)) && !st3.any && !(st3.keys & (1 << 13)),
      `a plain click lets GLO go (B6 still held); Shift-click on an empty spot lets the rest go (FIZZ's keys ${st3.keys})`);
    // P1.6d: FILL is SHIFT + ARP held: Shift-click GLO, then ARP: FILL while both are latched (ARP's LED blinks)
    const arp = await at(626, 249);
    await click(glo, true);
    await waitFor("FIZZ.labels.btns[8].firstChild.textContent === 'FILL'", 3000);
    await click(arp, true);
    const arpLed = await evaluate("(() => { const km = FIZZ.state.info.keymap; for (let r = 1; r <= 4; r++) for (let c = 0; c < 11; c++) if (km[r][c] === 10) return c * 4 + (r - 1); return -1; })()");
    const lvs = new Set();
    for (let i = 0; i < 30; i++) { await sleep(40); lvs.add(await evaluate(`FIZZ.state.levels[${arpLed}]`)); }
    const both = await evaluate("({ glo: FIZZ.controls.btnLatched(5), arp: FIZZ.controls.btnLatched(8), buttons: window.__panel.buttons })");
    await shot("latch-fill.png");
    await click(empty, true);
    check(both.glo && both.arp && (both.buttons & (1 << 7)) && (both.buttons & (1 << 10)) && lvs.size > 1,
      `Shift-click GLO then ARP: FILL (SHIFT + ARP held; FIZZ gets both, ARP's LED blinks: ${[...lvs].join(" / ")})`);
  }

  // 7d. P1.6d's automation recording: REC held from the computer key (=) while a knob turns (the arrows): the panel
  // message carries REC held and the knob's detents together
  {
    await evaluate("window.__msgs = []; { const p = FIZZ.player, f = p.panel.bind(p); p.panel = (m) => { window.__msgs.push({ b: m.buttons, e: Array.from(m.enc || []) }); f(m); }; }");
    await send("Input.dispatchKeyEvent", { type: "keyDown", key: "=", code: "Equal", windowsVirtualKeyCode: 187 });
    await sleep(150);
    for (let i = 0; i < 3; i++) { await send("Input.dispatchKeyEvent", { type: "keyDown", key: "ArrowUp", code: "ArrowUp", windowsVirtualKeyCode: 38 }); await send("Input.dispatchKeyEvent", { type: "keyUp", key: "ArrowUp", code: "ArrowUp", windowsVirtualKeyCode: 38 }); await sleep(60); }
    await send("Input.dispatchKeyEvent", { type: "keyUp", key: "=", code: "Equal", windowsVirtualKeyCode: 187 });
    await sleep(150);
    const am = await evaluate("window.__msgs.filter((m) => (m.b & (1 << 13)) && m.e.some((x) => x)).length");
    check(am >= 3, `REC held from the computer key (=) with a knob turned (the arrows): ${am} panel messages carry both`);
  }

  // 7e. the TAP ape (0.9.2): SHIFT + B6 tapped four times draws the ape (offscreen layers) in the page's AudioWorklet;
  // the worklet's render-load reports (host.js "load", every half second) while it dances
  {
    await evaluate("window.__load = []; FIZZ.player.ev.load = (m) => window.__load.push(m);");
    await evaluate("FIZZ.controls.holdBtn(6, 4, true)"); await sleep(120); await evaluate("FIZZ.controls.holdBtn(6, 4, false)"); await sleep(300);
    const lcd0 = await evaluate("Array.from(FIZZ.state.pixels)");
    // (a baseline: the step view while the sequencer plays, its playhead redrawn)
    await evaluate("FIZZ.controls.holdBtn(10, 4, true)"); await sleep(120); await evaluate("FIZZ.controls.holdBtn(10, 4, false)");
    await evaluate("window.__load = []"); await sleep(2000);
    const base = await evaluate("window.__load");
    await evaluate("FIZZ.controls.holdBtn(10, 4, true)"); await sleep(120); await evaluate("FIZZ.controls.holdBtn(10, 4, false)"); await sleep(300);
    await evaluate("FIZZ.controls.holdBtn(5, 4, true)"); await sleep(100);
    for (let i = 0; i < 4; i++) { await evaluate("FIZZ.controls.holdKey(13, 4, true)"); await sleep(60); await evaluate("FIZZ.controls.holdKey(13, 4, false)"); await sleep(400); }
    await evaluate("FIZZ.controls.holdBtn(5, 4, false)");
    await evaluate("window.__load = []");
    await sleep(2000);
    const lcd1 = await evaluate("Array.from(FIZZ.state.pixels)");
    await shot("ape.png");
    const ld = await evaluate("window.__load");
    const ch = lcd1.filter((v, i) => v !== lcd0[i]).length;
    const maxMs = Math.max(...ld.map((m) => m.maxMs)), late = Math.max(...ld.map((m) => m.lateMs)), load = Math.max(...ld.map((m) => m.load));
    check(ch > 5000 && ld.length >= 2 && late < 50, `the TAP ape in the page (${ch} px from the step view); the worklet over 2 s: the slowest quantum ${maxMs.toFixed(2)} ms, load ${(load * 100).toFixed(1)} %, late ${late.toFixed(0)} ms (${ld.length} reports); the step view playing: ${Math.max(...base.map((m) => m.maxMs)).toFixed(2)} ms, ${(Math.max(...base.map((m) => m.load)) * 100).toFixed(1)} %`);
    await evaluate("FIZZ.controls.holdBtn(6, 4, true)"); await sleep(120); await evaluate("FIZZ.controls.holdBtn(6, 4, false)"); await sleep(300);
  }

  // 7f. P1.6g: recording in every view, from the computer keys and from Shift-click. Stopped, REC held about a second
  // arms it (REC reads REC OFF), then PLAY starts it; a REC tap stops it. A MIDI keyboard on channel 1 plays the selected
  // track through channel 16 (FIZZ's output has sound)
  {
    const recLabel = "FIZZ.labels.btns[11].firstChild.textContent";
    const key = async (type, k, code, vk) => send("Input.dispatchKeyEvent", { type, key: k, code, windowsVirtualKeyCode: vk });
    await evaluate("FIZZ.controls.holdBtn(6, 4, true)"); await sleep(120); await evaluate("FIZZ.controls.holdBtn(6, 4, false)"); await sleep(300);
    // the computer keys: = held 1.3 s, then - (PLAY)
    await key("keyDown", "=", "Equal", 187); await sleep(1300);
    const armed = await evaluate(recLabel);
    await key("keyUp", "=", "Equal", 187); await sleep(200);
    await key("keyDown", "-", "Minus", 189); await sleep(100); await key("keyUp", "-", "Minus", 189); await sleep(600);
    const recording = await evaluate(recLabel);
    await key("keyDown", "=", "Equal", 187); await sleep(100); await key("keyUp", "=", "Equal", 187); await sleep(400);
    const stopped = await evaluate(recLabel);
    await key("keyDown", "-", "Minus", 189); await sleep(100); await key("keyUp", "-", "Minus", 189); await sleep(400);
    check(armed === "REC OFF" && recording === "REC OFF" && stopped !== "REC OFF",
      `the computer keys: = held 1.3 s arms (REC "${armed}"), - starts it (REC "${recording}"), = tapped stops it (REC "${stopped}")`);
    // Shift-click: REC latched 1.3 s (arms), clicked again (let go), PLAY clicked, REC clicked (stops)
    const at = async (ux, uy) => evaluate(`(() => { const c = document.getElementById("device"), r = c.getBoundingClientRect(), p = FIZZ.labels.device.toCss(${ux}, ${uy}); return { x: r.left + p.x, y: r.top + p.y }; })()`);
    const click = async (pt, shift) => { for (const type of ["mouseMoved", "mousePressed", "mouseReleased"]) await send("Input.dispatchMouseEvent", { type, x: pt.x, y: pt.y, button: type === "mouseMoved" ? "none" : "left", buttons: type === "mousePressed" ? 1 : 0, clickCount: 1, modifiers: shift ? 8 : 0 }); await sleep(150); };
    const rec = await at(786, 249), play = await at(733, 249);
    await click(rec, true); await sleep(1300);
    const armed2 = await evaluate(recLabel);
    await click(rec, false); await sleep(200);
    await click(play, false); await sleep(600);
    const rec2 = await evaluate(recLabel);
    await click(rec, false); await sleep(400);
    const stop2 = await evaluate(recLabel);
    await click(play, false); await sleep(300);
    check(armed2 === "REC OFF" && rec2 === "REC OFF" && stop2 !== "REC OFF",
      `Shift-click: REC latched 1.3 s arms (REC "${armed2}"), let go and PLAY clicked records (REC "${rec2}"), REC clicked stops it (REC "${stop2}")`);
    // a MIDI keyboard (on channel 1) plays the selected track
    await evaluate("FIZZ.peak()"); await sleep(100);
    await evaluate("FIZZ.midiIn(Uint8Array.from([0x90, 62, 110]))");
    let pk = 0; for (let i = 0; i < 12; i++) { await sleep(40); pk = Math.max(pk, await evaluate("FIZZ.peak()")); }
    await evaluate("FIZZ.midiIn(Uint8Array.from([0x80, 62, 0]))");
    check(pk > 0.002, `a MIDI keyboard on channel 1 plays the selected track (sent on channel 16): output peak ${pk.toFixed(4)}`);
  }

  // 7c. the recorder (RemoteFM1's ad408fa): Shift+R records, FIZZ playing, Shift+R stops; a take with sound in it
  {
    const shiftR = async () => {
      await send("Input.dispatchKeyEvent", { type: "keyDown", key: "R", code: "KeyR", modifiers: 8, windowsVirtualKeyCode: 82 });
      await send("Input.dispatchKeyEvent", { type: "keyUp", key: "R", code: "KeyR", modifiers: 8, windowsVirtualKeyCode: 82 });
    };
    await evaluate("FIZZ.controls.holdBtn(10, 4, true)"); await sleep(120); await evaluate("FIZZ.controls.holdBtn(10, 4, false)");   // PLAY
    await shiftR();
    await waitFor("document.body.classList.contains('recording')", 5000);
    await sleep(1500);
    await shiftR();
    await waitFor("FIZZRec.takes.length === 1", 5000);
    const take = await evaluate("(async () => { const t = FIZZRec.takes[0]; const b = new Uint8Array(await t.blob.arrayBuffer()); let peak = 0; for (let i = 44; i + 2 < b.length; i += 3) { const v = (b[i] | b[i + 1] << 8 | b[i + 2] << 16) << 8 >> 8; peak = Math.max(peak, Math.abs(v)); } return { name: t.name, frames: t.frames, sr: t.sampleRate, bits: t.bits, peak, riff: String.fromCharCode(...b.subarray(0, 4)) }; })()");
    await shot("recorder.png");
    await evaluate("FIZZ.controls.holdBtn(10, 4, true)"); await sleep(120); await evaluate("FIZZ.controls.holdBtn(10, 4, false)");
    await evaluate("FIZZRec.takes.forEach((t) => { t.saved = true; })");   // (else the page asks before a reload: unsaved takes)
    check(take.riff === "RIFF" && take.frames > take.sr && take.bits === 24 && take.peak > 1000, `Shift+R records: "${take.name}", ${(take.frames / take.sr).toFixed(1)} s, ${take.bits}-bit, peak ${take.peak}`);
  }

  // 7. the live labels (the stand-in: ?labels=stub, for the layer itself): drawn, flipped by SHIFT, toggled
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

  // 8. My presets (FIZZ's LIST / DUMP, P1.6m): 3 imported, exported, re-imported into a blank flash: the same DUMP
  // frames, read back the same by FIZZ's own test_sysex --feed; duplicates; a long export's progress; corrupt files
  if (files.length === 4) {
    await load(URL0);
    await waitFor("window.FIZZ && FIZZ.state.info && FIZZ.state.frameSeq > 0", 15000);
    const blank = async () => { await evaluate("(async () => { await FIZZ.boot(null, { blank: true }); })()"); await waitFor("FIZZ.state.info", 8000); await sleep(600); };
    const dumpJs = "(async () => { const out = []; for (const s of [1, 2, 3]) { const f = await FIZZ.presetLink.dump(s); out.push(Array.from(f || [])); } return out; })()";
    await blank();
    await evaluate(addJs(fileJs(files.slice(0, 3))));
    await waitFor("!!FIZZDigitone.fizz", 8000);
    await evaluate("FIZZDigitone.send()");
    await evaluate("FIZZPresets.open()");
    await waitFor("FIZZPresets.rows.length === 3", 5000);
    const rows = await evaluate("FIZZPresets.rows.map(r => r.slot + ' ' + r.name + ' ' + r.tags.join('+'))");
    check(rows.length === 3, `My presets lists them: ${rows.join(", ")}`);
    const orig = await evaluate(dumpJs);
    await evaluate("FIZZPresets.select([1, 2, 3])");
    await evaluate("FIZZPresets.exportSelected()");
    const lib = await evaluate("(() => { const e = FIZZPresets.lastExport; let s = ''; for (const b of e.bytes) s += String.fromCharCode(b); return { name: e.name, b64: btoa(s) }; })()");
    await shot("presets.png");
    check(/^FIZZ presets \d{4}-\d{2}-\d{2}\.fizzlib$/.test(lib.name), `3 presets export as "${lib.name}" (${Buffer.from(lib.b64, "base64").length} bytes)`);
    await evaluate("FIZZPresets.select([1])");
    await evaluate("FIZZPresets.exportSelected()");
    const one = await evaluate("(() => { const e = FIZZPresets.lastExport; return { name: e.name, text: new TextDecoder().decode(e.bytes) }; })()");
    const oneJ = JSON.parse(one.text);
    check(one.name === `${oneJ.name}.fizzsnd` && oneJ.format === "fizz-sound" && oneJ.patch.blobVersion === 5, `1 preset exports as "${one.name}" (fizz-sound, version ${oneJ.version})`);
    // Digitone II export from My presets (experimental): the notes shown first, then the zip
    await evaluate("FIZZPresets.select([1, 2, 3])");
    await evaluate("FIZZPresets.setFormat('dn2pst')");
    await evaluate("FIZZPresets.exportSelected()");
    const rv = await evaluate("({ shown: !!document.querySelector('.lib-panel .dnx-review'), items: FIZZPresets.lastExport.items.map(i => i.file + ':' + i.notes.length), pending: FIZZPresets.lastExport.pending })");
    await shot("presets-digitone.png");
    await evaluate("FIZZPresets.download()");
    const dz = await evaluate("(() => { const e = FIZZPresets.lastExport; let s = ''; for (const b of e.bytes) s += String.fromCharCode(b); return { name: e.name, pending: e.pending, b64: btoa(s) }; })()");
    const dzBytes = new Uint8Array(Buffer.from(dz.b64, "base64"));
    const dzZip = await dnReadZip(dzBytes);
    const reread = [];
    for (const n of dzZip.names) { const s = (await readDn2(await dzZip.get(n), n)); reread.push(s); }
    const origS = orig.map((f) => soundFromStoreFrame(Uint8Array.from(f)));
    const sameVals = reread.length === 3 && reread.every((s, i) => fizzValues(s).join() === fizzValues(origS[i]).join());
    check(rv.shown && rv.pending && !dz.pending && /\(Digitone II\)\.zip$/.test(dz.name) && sameVals,
      `My presets -> Digitone II (experimental): notes shown first (${rv.items.join(", ")}), then "${dz.name}"; read back: ${sameVals ? "the same values" : "DIFFERENT"}`);
    await evaluate("FIZZPresets.setFormat('fizz')");
    await evaluate("FIZZPresets.close()");
    // into a blank flash
    await blank();
    const before = await evaluate("FIZZDigitone.items.length");
    await evaluate(addJs(JSON.stringify([{ name: lib.name, b64: lib.b64 }])));
    await evaluate("FIZZDigitone.send()");
    const re = await evaluate(`FIZZDigitone.items.slice(${before}).map(i => i.result && i.result.ok && i.result.slot)`);
    const back2 = await evaluate(dumpJs);
    check(re.join() === "1,2,3" && back2.every((f, i) => f.length && f.join() === orig[i].join()), `the .fizzlib back into a blank flash: MY PRESETS ${re.join(", ")}, the same frames as exported`);
    const FEED = process.env.FIZZ_TEST_SYSEX || path.resolve(HERE, "../../tools/build/test_sysex");
    if (fs.existsSync(FEED)) {
      const tmp = fs.mkdtempSync(path.join(OUT, "feed-"));
      const read = (f) => { fs.writeFileSync(path.join(tmp, "one.syx"), restoreFrame(Uint8Array.from(f))); return execFileSync(FEED, ["--feed", path.join(tmp, "one.syx")], { encoding: "utf8" }).split("\n").filter((l) => /^(name|values|mix|tags) /.test(l)).join("\n"); };
      const same = orig.every((f, i) => read(f) === read(back2[i]) && read(f).includes("values"));
      check(same, "FIZZ's test_sysex --feed reads the re-imported presets exactly as the originals (name, values, mix, tags)");
    } else note("no tools/build/test_sysex: the FIZZ read-back skipped");
    // the .fizzlib's presets straight to Digitone files from Import sounds (no FIZZ needed)
    await evaluate("FIZZDigitone.exportDigitone('dnsnd')");
    await evaluate("FIZZDigitone.download()");
    const ix = await evaluate("(() => { const e = FIZZDigitone.lastExport; let s = ''; for (const b of e.bytes) s += String.fromCharCode(b); return { name: e.name, pending: e.pending, b64: btoa(s) }; })()");
    const ixZip = await dnReadZip(new Uint8Array(Buffer.from(ix.b64, "base64")));
    check(!ix.pending && ixZip.names.length >= 3 && ixZip.names.every((n) => n.endsWith(".dnsnd")), `Import sounds -> Digitone (experimental): "${ix.name}" (${ixZip.names.length} .dnsnd)`);
    // a duplicate: skipped, then kept
    await evaluate(addJs(JSON.stringify([{ name: one.name, b64: Buffer.from(one.text).toString("base64") }])));
    await waitFor("FIZZDigitone.items[FIZZDigitone.items.length - 1].dup === 1", 5000);
    await evaluate("FIZZDigitone.send()");
    const sk = await evaluate("FIZZDigitone.items[FIZZDigitone.items.length - 1].result");
    await evaluate(addJs(JSON.stringify([{ name: one.name, b64: Buffer.from(one.text).toString("base64") }])));
    await waitFor("FIZZDigitone.items[FIZZDigitone.items.length - 1].dup === 1", 5000);
    await evaluate("FIZZDigitone.setKeepDups(true)");
    await evaluate("FIZZDigitone.send()");
    const kept = await evaluate("FIZZDigitone.items[FIZZDigitone.items.length - 1].result");
    check(sk && sk.skipped && kept && kept.ok && kept.slot === 4, `a duplicate (same name and patch): ${sk && sk.error}; kept both: MY PRESETS ${kept && kept.slot}`);
    // a long export: everything (DUMP slot 0, paced), with its progress
    await evaluate(addJs(fileJs(files.slice(3))));
    await evaluate("FIZZDigitone.send()");
    await evaluate("FIZZPresets.open()");
    await waitFor("FIZZPresets.rows.length === 5", 5000);
    await evaluate("window.__progress = []; new MutationObserver(() => { const t = document.querySelector('.lib-panel .dn-who').textContent; if (/Reading/.test(t)) window.__progress.push(t); }).observe(document.querySelector('.lib-panel .dn-who'), { childList: true, characterData: true, subtree: true })");
    await evaluate("FIZZPresets.select(FIZZPresets.rows.map(r => r.slot))");
    await evaluate("FIZZPresets.exportSelected()");
    const all5 = await evaluate("({ name: FIZZPresets.lastExport.name, n: FIZZPresets.lastExport.bytes.length, progress: window.__progress })");
    const all5lib = await evaluate("(async () => { const { parseFizzlib } = await import('./js/library.js'); return (await parseFizzlib(FIZZPresets.lastExport.bytes)).items.filter(i => i.ok).length; })()");
    check(all5lib === 5 && all5.progress.some((t) => /5 \/ 5/.test(t)), `all 5 at once (DUMP everything): "${all5.name}" with ${all5lib} presets; progress shown: ${all5.progress.slice(-1)[0]}`);
    await evaluate("FIZZPresets.close()");
    // corrupt files
    await evaluate(addJs(JSON.stringify([{ name: "broken.fizzsnd", b64: Buffer.from("{oops").toString("base64") }, { name: "broken.fizzlib", b64: "AQID" },
      { name: "wrong.fizzsnd", b64: Buffer.from(JSON.stringify({ ...oneJ, patch: { blob: "AAAA", blobVersion: 5 } })).toString("base64") }])));
    const errs = await evaluate("FIZZDigitone.items.slice(-3).map(i => i.ok ? 'ACCEPTED' : i.error)");
    check(errs.every((e) => e !== "ACCEPTED" && /FIZZ/.test(e)), `corrupt files refused: ${errs.join(" | ")}`);
    await evaluate("FIZZDigitone.close()");
  }

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
