// SPDX-License-Identifier: GPL-3.0-only
// The Digitone II harvest wizard (dn2-wizard.html) against its pretend Digitone II, in headless Chromium:
//
//   node site/tools/test_dn2wizard.mjs [--chromium path] [--full]
//
// Serves site/ itself, opens dn2-wizard.html?mock=1&fast=1&auto=1 (connect, check, run every step, the person's steps
// done to the mock), waits for the run to end, takes the zip, and reads it with tools/dn2_harvest_analyze.mjs:
//   - no console error; the mock received nothing the wizard may not send (sysex.js allowed);
//   - every parameter swept moved exactly its own bytes, 28 + 2 x id, and its law is the mock's (MOCK_LAWS);
//   - the person's steps moved the bytes the mock changed; the recordings are there, with sound in them.
// Without --full the plan skips nothing either: fast mode only shortens the waits and the recordings.
import fs from "node:fs";
import path from "node:path";
import os from "node:os";
import http from "node:http";
import { spawn } from "node:child_process";
import { fileURLToPath } from "node:url";
import { analyze, report } from "../../tools/dn2_harvest_analyze.mjs";
import { PARAMS } from "../js/dn2wizard/params.js";

const HERE = path.dirname(fileURLToPath(import.meta.url)), SITE = path.resolve(HERE, ".."), OUT = path.resolve(SITE, ".build/test");
const arg = (k, d) => { const i = process.argv.indexOf(k); return i > 0 ? process.argv[i + 1] : d; };
const CHROMIUM = arg("--chromium", "/Applications/Chromium.app/Contents/MacOS/Chromium");
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
fs.mkdirSync(OUT, { recursive: true });
let fails = 0;
const check = (ok, what) => { console.log(`${ok ? "ok  " : "FAIL"}  ${what}`); if (!ok) fails++; };

// ---- a static server for site/
const TYPES = { ".html": "text/html", ".js": "text/javascript", ".css": "text/css", ".json": "application/json", ".wasm": "application/wasm" };
const server = http.createServer((q, s) => {
  const p = path.join(SITE, decodeURIComponent(new URL(q.url, "http://x").pathname));
  if (!p.startsWith(SITE) || !fs.existsSync(p) || fs.statSync(p).isDirectory()) { s.writeHead(404); return s.end(); }
  s.writeHead(200, { "Content-Type": TYPES[path.extname(p)] || "application/octet-stream" });
  fs.createReadStream(p).pipe(s);
});
await new Promise((r) => server.listen(0, "127.0.0.1", r));
const URL0 = `http://127.0.0.1:${server.address().port}/dn2-wizard.html?mock=1&fast=1&auto=1`;

// ---- Chromium over the DevTools protocol (as test_site.mjs)
const PORT = 9700 + Math.floor(Math.random() * 300), profile = fs.mkdtempSync(path.join(os.tmpdir(), "fizz-dn2w-"));
const chrome = spawn(CHROMIUM, ["--headless=new", "--use-mock-keychain", "--password-store=basic", "--no-first-run", "--no-default-browser-check",
  "--disable-gpu", `--user-data-dir=${profile}`, `--remote-debugging-port=${PORT}`, "--autoplay-policy=no-user-gesture-required", "--disable-audio-output" /* (a fake output: not the sound device) */,
  "--use-fake-ui-for-media-stream", "--window-size=1100,900", "about:blank"], { stdio: "ignore" });
const done = () => { try { chrome.kill("SIGKILL"); } catch (_) {} server.close(); };
process.on("exit", done);
let ws = null, id = 0;
const pending = new Map(), errors = [];
for (let i = 0; i < 100 && !ws; i++) {
  try { const page = (await (await fetch(`http://127.0.0.1:${PORT}/json/list`)).json()).find((t) => t.type === "page"); if (page) ws = new WebSocket(page.webSocketDebuggerUrl); } catch (_) { /* not up */ }
  if (!ws) await sleep(100);
}
if (!ws) { console.error("Chromium did not start"); process.exit(1); }
await new Promise((r, j) => { ws.onopen = r; ws.onerror = j; });
ws.onmessage = (ev) => {
  const m = JSON.parse(ev.data);
  if (m.id && pending.has(m.id)) { const p = pending.get(m.id); pending.delete(m.id); m.error ? p.j(new Error(m.error.message)) : p.r(m.result); }
  else if (m.method === "Runtime.exceptionThrown") errors.push(m.params.exceptionDetails.exception ? m.params.exceptionDetails.exception.description : m.params.exceptionDetails.text);
  else if (m.method === "Runtime.consoleAPICalled" && m.params.type === "error") errors.push(m.params.args.map((a) => a.value ?? a.description).join(" "));
  else if (m.method === "Log.entryAdded" && m.params.entry.level === "error") errors.push(`${m.params.entry.text} ${m.params.entry.url || ""}`);
};
const send = (method, params = {}) => new Promise((r, j) => { const i = ++id; pending.set(i, { r, j }); ws.send(JSON.stringify({ id: i, method, params })); });
const evaluate = async (expr) => {
  const r = await send("Runtime.evaluate", { expression: expr, awaitPromise: true, returnByValue: true });
  if (r.exceptionDetails) throw new Error(r.exceptionDetails.exception ? r.exceptionDetails.exception.description : r.exceptionDetails.text);
  return r.result.value;
};

try {
  await send("Page.enable"); await send("Runtime.enable"); await send("Log.enable");
  const t0 = Date.now();
  await send("Page.navigate", { url: URL0 });
  let state = "idle", last = 0, unplugged = 0;
  while (Date.now() - t0 < 30 * 60 * 1000) {
    await sleep(500);
    try { state = await evaluate("window.DN2W && DN2W.state()"); } catch (_) { continue; }
    const pr = await evaluate("DN2W.runner ? DN2W.runner.progress().i : 0");
    if (pr - last >= 500) { console.log(`      ${pr} steps, ${Math.round((Date.now() - t0) / 1000)} s`); last = pr; }
    if (!unplugged && pr > 1500) {                         // the cable out: the run pauses; back in, Resume goes on
      unplugged = pr;
      await evaluate("DN2W.mock.input.state = DN2W.mock.output.state = 'disconnected'");
      let st = "";
      for (let k = 0; k < 40 && st !== "paused"; k++) { await sleep(100); st = await evaluate("DN2W.state()"); }
      const at = await evaluate("DN2W.runner.progress()");
      await evaluate("DN2W.mock.input.state = DN2W.mock.output.state = 'connected'; DN2W.runner.resume()");
      check(st === "paused" && /not connected/.test(at.msg), `unplugged at step ${at.i}: the run pauses (${at.msg.slice(0, 60)}...), then resumes there`);
      continue;
    }
    if (state === "done" || state === "stopped" || state === "paused") break;
  }
  const secs = Math.round((Date.now() - t0) / 1000);
  const prog = await evaluate("DN2W.runner && DN2W.runner.progress()");
  check(state === "done", `the run ends (${state} at step ${prog && prog.i} of ${prog && prog.n}, ${secs} s, fast mode)${prog && prog.msg ? ": " + prog.msg : ""}`);
  await sleep(300);
  const H = await evaluate("document.documentElement.scrollHeight");
  await send("Emulation.setDeviceMetricsOverride", { width: 1100, height: Math.min(H, 4000), deviceScaleFactor: 1, mobile: false });
  const shot = await send("Page.captureScreenshot", { format: "png" });
  fs.writeFileSync(path.join(OUT, "dn2-wizard.png"), Buffer.from(shot.data, "base64"));
  const v = await evaluate("DN2W.mock.violations.length"), rcv = await evaluate("DN2W.mock.received");
  check(v === 0, `the device received only what the wizard may send (${rcv} messages, ${v} refused)`);
  const sent = await evaluate("DN2W.link.count");
  console.log(`      sent: ${JSON.stringify(sent)}`);
  const b64 = await evaluate("DN2W.zip ? DN2W.zipBase64() : ''");
  check(b64.length > 0, "the zip is made");
  // the results page: the FIZZ Discord as a link beside the download (a new tab), no address placeholder
  const dc = await evaluate(`(() => { const r = document.getElementById("results"), d = document.getElementById("discord");
    return { shown: !r.hidden, href: d && d.href, target: d && d.target, links: [...r.querySelectorAll('a[href^="https://discord.gg/"]')].length, text: r.textContent }; })()`);
  check(dc.shown && dc.href === "https://discord.gg/emf6DmZ9V" && dc.target === "_blank" && dc.links === 2 && !/address here|\[the owner/.test(dc.text),
    `the results page sends to the FIZZ Discord: ${dc.href} (a new tab, ${dc.links} links), no address placeholder`);
  const zip = new Uint8Array(Buffer.from(b64, "base64"));
  fs.writeFileSync(path.join(OUT, "dn2-harvest-mock.zip"), zip);
  const a = analyze(zip);
  fs.writeFileSync(path.join(OUT, "dn2-harvest-mock.md"), report(a));
  check(a.device && a.device.version === "1.11" && a.settings.dump === "kit 0x62", `the device read: OS ${a.device && a.device.version}, ${a.settings.dump}`);
  // every parameter, by CC and by NRPN (the person's filter probes too): its own bytes, the mock's law
  const want = new Set(PARAMS.map((p) => `${p.id}|cc`).filter((k) => { const p = PARAMS.find((x) => `${x.id}|cc` === k); return p.cc !== null && p.cc !== undefined; }));
  for (const p of PARAMS) want.add(`${p.id}|nrpn`);
  const got = new Set(a.params.filter((x) => !x.tag).map((x) => `${x.id}|${x.via}`));
  const missing = [...want].filter((k) => !got.has(k));
  check(!missing.length, `every parameter swept by CC and NRPN (${got.size} of ${want.size}${missing.length ? "; missing " + missing.slice(0, 8).join(", ") : ""})`);
  const wrong = a.params.filter((x) => !x.matchesRule);
  check(!wrong.length, `each moved exactly its 28 + 2 x id (${a.params.length} sweeps${wrong.length ? "; not: " + wrong.slice(0, 6).map((x) => `${x.name} ${x.via} -> ${x.offsets.join(",")}`).join("; ") : ""})`);
  const lawOk = (x) => {
    const p = PARAMS.find((q) => q.id === x.id);
    if (x.via === "nrpn") return p.kind === "enum" || p.kind === "rpair" ? !!x.law : /^u16 = NRPN << 1\b/.test(x.law || "");
    if (p.kind === "enum") return x.law === `stored = floor(CC x ${x.values} / 128)` || (p.n >= 128 && x.law === "stored = CC");
    if (p.kind === "rpair") return /RATIO B/.test(x.law || "");
    return x.law === "stored = CC";
  };
  const badLaw = a.params.filter((x) => !x.tag && !lawOk(x));
  check(!badLaw.length, `the laws found are the mock's (${badLaw.length ? badLaw.slice(0, 6).map((x) => `${x.name} ${x.via}: ${x.law}`).join("; ") : "all"})`);
  const man = (lbl) => a.manual.find((m) => m.label === lbl);
  check(man("machine COMB+") && man("machine COMB+").changed.some((c) => c.startsWith("+245: 0 -> 5")), "the person's step: COMB+ moved byte 245 to 5");
  check(man("WAVETONE") && man("WAVETONE").changed.some((c) => c.startsWith("+244: 0 -> 1")), "the person's step: WAVETONE moved byte 244 to 1");
  check(man("octave -1") && man("octave -1").changed.some((c) => c.startsWith("+251:")), "the person's step: the octave moved byte 251");
  const heard = a.audio.filter((r) => !r.missing && r.peakDb !== null && r.peakDb > -30);
  check(a.audio.length >= 60 && heard.length === a.audio.length, `the recordings: ${a.audio.length}, ${heard.length} with sound`);
  // the pitch read from a recording (fast mode's notes are short: the last note's, C4 = 261.6 Hz)
  const porta = a.audio.find((r) => r.label === "PTIM cc64"), mm = a.audio.find((r) => r.label === "TYPE cc0");
  const near = (f, hz) => f && Math.abs(f / hz - 1) < 0.02;
  check(porta && near(porta.f0End, 261.6) && mm && near(mm.f0Start, 261.6), `the pitch in the recordings: PTIM's last note ${porta && porta.f0End} Hz, TYPE's C4 ${mm && mm.f0Start} Hz`);
  check(!errors.length, `no console error${errors.length ? ": " + errors.slice(0, 3).join(" | ") : ""}`);
  console.log(`      report: ${path.join(OUT, "dn2-harvest-mock.md")}, zip ${(zip.length / 1048576).toFixed(1)} MB`);
} catch (e) { check(false, `the test ran: ${e.message}`); }
done();
console.log(fails ? `test_dn2wizard: ${fails} failed` : "test_dn2wizard: all passed");
process.exit(fails ? 1 : 0);
