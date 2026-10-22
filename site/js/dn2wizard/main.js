// SPDX-License-Identifier: GPL-3.0-only
// The Digitone II harvest page (dn2-wizard.html): connect, check, confirm, run, download the zip.
// ?mock=1: a pretend Digitone II (mock.js); &fast=1: short waits and recordings; &auto=1: run without asking (tests).
import { Link } from "./link.js";
import { Recorder, audioInputs } from "./audio.js";
import { buildPlan, summarize, ITEMS } from "./plan.js";
import { Runner, WIZARD_VERSION } from "./runner.js";
import { IDENTITY_REQUEST, isIdentityReply, apiMessage, parseApi, readVersion, DUMP, trackRecord } from "./sysex.js";
import { createMockDevice } from "./mock.js";

const Q = new URLSearchParams(location.search);
const MOCK = Q.get("mock") === "1", FAST = Q.get("fast") === "1", AUTO = Q.get("auto") === "1";
const $ = (id) => document.getElementById(id);
const h = (tag, attrs = {}, ...kids) => {
  const e = document.createElement(tag);
  for (const [k, v] of Object.entries(attrs)) if (k === "class") e.className = v; else if (k.startsWith("on")) e.addEventListener(k.slice(2), v); else e.setAttribute(k, v);
  for (const c of kids) e.append(c);
  return e;
};
const fmtTime = (ms) => { const s = Math.round(ms / 1000); return s < 60 ? `${s} s` : `${Math.floor(s / 60)} min ${String(s % 60).padStart(2, "0")} s`; };
const patternName = (n) => `${"ABCDEFGH"[n >> 4]}${String((n & 15) + 1).padStart(2, "0")}`;

const W = { midi: null, link: null, recorder: new Recorder(), runner: null, mock: MOCK ? createMockDevice({ log: (s) => logLine(s) }) : null, zip: null };
window.DN2W = W;
let logLines = [];
function logLine(s) {
  logLines.push(s);
  if (logLines.length > 400) logLines = logLines.slice(-300);
  const el = $("log");
  if (el) { el.textContent = logLines.join("\n"); el.scrollTop = el.scrollHeight; }
}

// ---- 1. connect
function fillSelect(sel, items, pick) {
  sel.replaceChildren(...items.map(([v, label]) => h("option", { value: v }, label)));
  const i = items.findIndex(pick);
  if (i >= 0) sel.selectedIndex = i;
}
function ports() {
  const ins = [...W.midi.inputs.values()], outs = [...W.midi.outputs.values()], dn = (p) => /digitone/i.test(p[1]);
  fillSelect($("midi-in"), ins.map((p) => [p.id, p.name]), dn);
  fillSelect($("midi-out"), outs.map((p) => [p.id, p.name]), dn);
  applyPorts();
}
function applyPorts() {
  if (!W.midi || !W.link) return;
  W.link.setPorts(W.midi.inputs.get($("midi-in").value) || null, W.midi.outputs.get($("midi-out").value) || null);
  $("midi-state").textContent = W.link.connected ? "connected" : "not connected";
  $("midi-state").className = W.link.connected ? "ok" : "bad";
}
async function connectMidi() {
  try {
    W.midi = MOCK ? W.mock.access : await navigator.requestMIDIAccess({ sysex: true });
  } catch (e) { $("midi-state").textContent = `MIDI refused: ${e.message}. Allow MIDI and SysEx for this page.`; $("midi-state").className = "bad"; return; }
  W.link = W.link || new Link({ channel: +$("channel").value - 1, gapMs: FAST ? 0 : 3, log: logLine });
  W.midi.onstatechange = () => { ports(); if (W.runner && W.runner.state === "running" && !W.link.connected) W.runner.pause("The Digitone II went away."); update(); };
  ports();
  $("check").disabled = false;
}
async function connectAudio() {
  try {
    if (MOCK) await W.recorder.open({ makeStream: W.mock.makeStream });
    else await W.recorder.open({ deviceId: $("audio-in").value });
    $("audio-state").textContent = `recording from it (${W.recorder.rate} Hz)`;
    $("audio-state").className = "ok";
  } catch (e) { $("audio-state").textContent = `no audio: ${e.message}`; $("audio-state").className = "bad"; }
}
async function listAudio() {
  if (MOCK) { fillSelect($("audio-in"), [["mock", "Digitone II (mock audio)"]], () => true); return; }
  const l = await audioInputs();
  fillSelect($("audio-in"), l.map((d) => [d.deviceId, d.label || "an audio input"]), (x) => /digitone|elektron/i.test(x[1]));
}
function meter() {
  const db = W.recorder.peakDb(), w = Number.isFinite(db) ? Math.max(0, Math.min(1, (db + 60) / 60)) : 0;
  $("meter-bar").style.width = `${(w * 100).toFixed(1)}%`;
  $("meter-db").textContent = Number.isFinite(db) ? `${db.toFixed(0)} dB` : "silence";
  requestAnimationFrame(meter);
}

// ---- 2. check: read-only (identity, version, the track's sound)
async function check() {
  W.link.ch = +$("channel").value - 1;
  applyPorts();
  if (!W.link.connected) { $("check-out").textContent = "Choose the Digitone II's MIDI ports first."; return; }
  $("check-out").textContent = "Asking the Digitone II ...";
  const out = [];
  const id = await W.link.request(IDENTITY_REQUEST, isIdentityReply, 800);
  const vr = await W.link.request(apiMessage(0x2001, 0x02), (m) => { const a = parseApi(m); return !!a && a.respId === 0x2001; }, 1500);
  const v = vr ? readVersion(parseApi(vr).body) : null;
  out.push(v ? `Digitone II, OS ${v.version} (build ${v.build})` : id ? "a device answered the identity request (no OS version)" : "no answer to the identity or version request");
  const t = +$("track").value - 1, pat = +$("pattern").value;
  const d = await W.link.dump(DUMP.KIT, pat, 2500) || await W.link.dump(DUMP.PATTERN_KIT, pat, 6000);
  const r = d && trackRecord(d, t);
  if (!r) out.push(`no sound came back for pattern ${patternName(pat)}: is the pattern right, and SysEx on?`);
  else {
    let name = ""; for (let i = 12; i < 28 && r[i]; i++) name += String.fromCharCode(r[i]);
    out.push(`track ${t + 1}: "${name}", ${r[244] === 0 ? "FM TONE" : `machine ${r[244]}: NOT FM TONE, set an INIT FM TONE sound first`}`);
  }
  $("check-out").textContent = out.join(" · ");
  $("plan-box").hidden = false;
  showPlan();
  return !!r && r[244] === 0;
}

// ---- 3. the plan and the confirmation
function plan() { return buildPlan({ person: $("person").checked }); }
function showPlan() {
  const S = plan(), sum = summarize(S, +$("channel").value - 1);
  const counts = {};
  for (const s of S) counts[s.item] = (counts[s.item] || 0) + 1;
  $("items").replaceChildren(...ITEMS.filter((it) => counts[it.key]).map((it) => h("li", { class: it.person ? "person" : "" }, it.title, it.person ? h("span", { class: "tag" }, "needs you") : "")));
  const est = S.reduce((a, s) => a + (s.k === "probe" || s.k === "ref" || s.k === "dump" || s.k === "restore" ? 120 : s.k === "audio" ? s.ms + 150 : s.k === "manual" ? 20000 : 50), 0);
  $("sends").replaceChildren(
    h("li", {}, `On MIDI channel ${sum.channel + 1} only: CC ${sum.cc.join(", ")}`),
    h("li", {}, `NRPN (to set exact values and to put each value back): ${sum.nrpn.length} parameters`),
    h("li", {}, `Notes ${sum.notes.join(", ")} at velocity 100, at most 2.2 s each, in ${sum.audios} short recordings (${fmtTime(sum.recMs)} in all); all notes off at the end and on any stop`),
    h("li", {}, `Read-only requests: the sound of track ${$("track").value} (Elektron dump requests 0x62 / 0x60), the OS version`),
    h("li", {}, "Never: save, a project, kit or sound write, a firmware, or anything to the +Drive"));
  $("plan-sum").textContent = `${S.length} steps, ${sum.probes} read-backs, about ${fmtTime(est)}${sum.manual ? `, with ${sum.manual} things for you to do on the Digitone` : ""}.`;
}

// ---- 4. the run
function update() {
  const R = W.runner;
  if (!R) return;
  const p = R.progress();
  $("bar").style.width = `${(100 * p.i / p.n).toFixed(1)}%`;
  $("count").textContent = `${p.i} / ${p.n}`;
  const it = p.step ? ITEMS.find((x) => x.key === p.step.item) : null;
  $("now").textContent = p.state === "done" ? "Done." : p.state === "stopped" ? "Stopped." : it ? it.title : "";
  $("eta").textContent = p.state === "running" ? `about ${fmtTime(p.eta)} left` : p.msg || "";
  $("pause").disabled = p.state !== "running";
  $("resume").disabled = p.state !== "paused";
  $("stop").disabled = !["running", "paused", "pausing"].includes(p.state);
  if ((p.state === "done" || p.state === "stopped") && !W.zip) results();
}
function ask(text) {
  return new Promise((resolve) => {
    $("ask-text").textContent = text;
    $("ask").hidden = false;
    const done = (v) => { $("ask").hidden = true; $("ask-go").onclick = $("ask-skip").onclick = null; resolve(v); };
    $("ask-go").onclick = () => done("continue");
    $("ask-skip").onclick = () => done("skip");
  });
}
async function start() {
  if (!$("agree").checked && !AUTO) return;
  W.link.ch = +$("channel").value - 1;
  applyPorts();
  W.zip = null;
  $("run-box").hidden = false;
  $("results").hidden = true;
  W.runner = new Runner({ link: W.link, recorder: W.recorder, plan: plan(), mock: W.mock,
    cfg: { track: +$("track").value - 1, pattern: +$("pattern").value, fast: FAST, auto: AUTO },
    ui: { update, log: logLine, ask } });
  $("start").disabled = true;
  await W.runner.start();
}
function results() {
  const z = W.runner.zip(), d = new Date(), stamp = d.toISOString().slice(0, 16).replace(/[-:]/g, "").replace("T", "-");
  W.zip = z;
  const url = URL.createObjectURL(new Blob([z], { type: "application/zip" }));
  const a = $("download");
  a.href = url;
  a.download = `dn2-harvest-${stamp}${MOCK ? "-mock" : ""}.zip`;
  a.textContent = `Download the results (${(z.length / 1048576).toFixed(1)} MB)`;
  $("results").hidden = false;
  $("start").disabled = false;
}
W.zipBase64 = () => { let s = ""; const z = W.zip; for (let i = 0; i < z.length; i += 0x8000) s += String.fromCharCode(...z.subarray(i, i + 0x8000)); return btoa(s); };
W.state = () => W.runner ? W.runner.progress().state : "idle";

// ---- the page
function init() {
  $("version").textContent = `wizard ${WIZARD_VERSION}${MOCK ? " · MOCK DEVICE" : ""}`;
  fillSelect($("channel"), Array.from({ length: 16 }, (_, i) => [i + 1, `${i + 1}`]), (x) => x[0] === 1);
  fillSelect($("track"), Array.from({ length: 16 }, (_, i) => [i + 1, `${i + 1}`]), (x) => x[0] === 1);
  fillSelect($("pattern"), Array.from({ length: 128 }, (_, i) => [i, patternName(i)]), (x) => x[0] === 0);
  $("track").addEventListener("change", () => { $("channel").value = $("track").value; showPlan(); });
  for (const id of ["channel", "person"]) $(id).addEventListener("change", showPlan);
  for (const id of ["midi-in", "midi-out"]) $(id).addEventListener("change", applyPorts);
  $("connect").addEventListener("click", connectMidi);
  $("audio-list").addEventListener("click", listAudio);
  $("audio-open").addEventListener("click", connectAudio);
  $("check").addEventListener("click", check);
  $("agree").addEventListener("change", () => { $("start").disabled = !$("agree").checked; });
  $("start").addEventListener("click", start);
  $("pause").addEventListener("click", () => W.runner && W.runner.pause());
  $("resume").addEventListener("click", () => { applyPorts(); W.runner && W.runner.resume(); });
  $("stop").addEventListener("click", () => W.runner && W.runner.stop());
  window.addEventListener("error", () => W.link && W.link.allNotesOff());
  window.addEventListener("beforeunload", () => W.link && W.link.allNotesOff());
  requestAnimationFrame(meter);
  if (!navigator.requestMIDIAccess && !MOCK) $("midi-state").textContent = "This browser has no Web MIDI: use Chrome or Edge.";
  if (AUTO) (async () => {
    await connectMidi(); await listAudio(); await connectAudio();
    if (await check()) await start();
  })();
}
init();
