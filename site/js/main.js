// SPDX-License-Identifier: GPL-3.0-only
// The FIZZ preview page: the FM-1 drawn on screen running FIZZ (fizz.wasm on RemoteFM1's native engine, in an
// AudioWorklet), its input, its flash kept in this browser, Connect FM-1 (RemoteFM1), the settings sheet. From
// RemoteFM1's web/site/js/main.js (GPL-3.0-only), cut to one firmware: no catalog, Hub, emulated engine, managers or
// recorder.
//   ?view=screen|device   the view
import { Controls, DeviceView, ScreenView, Lcd, VIEW, SRC, PANEL_LABELS, B } from "./panel.js";
import { LabelsLayer } from "./labels-ui.js";
import { createLabelsLink, stubLabels } from "./labels.js";
import { DEFAULT_THEME, presetNamed, css, derive } from "./theme.js";
import { Keyboard, attachPointer, MidiInput } from "./input.js";
import { Player } from "./audio.js";
import * as saves from "./saves.js";
import { Sheet, icon } from "./sheet.js";
import { makeSite } from "./remote-hook.js";
import { createFm1, mountUseFm1, mountHeader } from "./use-fm1.js";
import { loadPrefs, savePrefs as writePrefs, keyHintsOn } from "./prefs.js";
import { feedbackUrl } from "./feedback.js";
import { mountDigitone } from "./digitone-ui.js";
import { BUILD, ver } from "./build.js";

const $ = (id) => document.getElementById(id);
const params = new URLSearchParams(location.search);
const FW = "fizz", ENGINE = "native";
const INSTALLER = "https://quixotic7.github.io/RemoteFM1/installer/";

// ------------------------------------------------------------------ prefs ---
const store = (() => { try { return window.localStorage; } catch (_) { return null; } })();
const prefs = loadPrefs(store);
const savePrefs = () => { writePrefs(store, prefs); };

// ------------------------------------------------------------------ state ---
const state = {
  meta: null, info: null, running: false, engineState: "idle",
  theme: presetNamed(prefs.theme) || presetNamed(DEFAULT_THEME),
  view: params.get("view") === "screen" ? "screen" : params.get("view") === "device" ? "device" : prefs.view || "device",
  transpose: prefs.transpose | 0,
  notesPlayKeys: prefs.notesPlayKeys === true,            // FIZZ: a MIDI keyboard plays its tracks through its MIDI in
  reachFirmware: prefs.reachFirmware !== false,
  keyHints: keyHintsOn(prefs),
  labels: prefs.labels !== false,                          // FIZZ's live labels on the panel (on by default)
  saves: [], selectedSave: null, carryOffer: null,
  pixels: null, frameSeq: 0, levels: new Uint8Array(44), playGreen: 0,
  volume: typeof prefs.volume === "number" ? Math.max(0, Math.min(1, prefs.volume)) : 1,
  fm1: false,                                             // an FM-1 is connected (the feedback form's setup)
};
let player = null, sheet = null, site = null, siteEmit = () => {}, fm1 = null;
const lcd = new Lcd();
const controls = new Controls((p) => { if (player) player.panel(p); panelChanged(); });
const device = new DeviceView($("device"), controls, lcd);
const screen = new ScreenView($("screen"), lcd);
device.setHints(state.keyHints);
// FIZZ's live labels (js/labels.js, js/labels-ui.js): from FIZZ's LABELS replies; ?labels=stub: a fixed stand-in
// (PLAYING.md's step view and SHIFT layer) that follows the panel's SHIFT, for trying the page before FIZZ sends them
const labelsLayer = new LabelsLayer({ wrap: $("device-wrap"), canvas: $("device"), device });
labelsLayer.show(state.labels);
const LABELS_STUB = params.get("labels") === "stub";
let labelsLink = null;
function panelChanged() { if (LABELS_STUB && state.info) labelsLayer.set(stubLabels({ shift: controls.btnHeld(B.GLO) })); }
const version = () => (state.meta ? state.meta.version : "");
const wasmUrl = () => `fizz.wasm?v=${encodeURIComponent(state.meta ? (state.meta.commit || "").slice(0, 12) + "-" + state.meta.bytes : "0")}`;

// ------------------------------------------------------------------ drawing ---
let drawQueued = false;
const invalidate = () => { if (!drawQueued) { drawQueued = true; requestAnimationFrame(draw); } };
controls.onDraw = invalidate;
function draw() {
  drawQueued = false;
  if (!$("device-wrap").hidden) device.draw();
  if (!$("screen-wrap").hidden) screen.draw();
}

let stacked = false;
function layout() {
  const stage = $("stage"), W = stage.clientWidth, H = stage.clientHeight, dpr = window.devicePixelRatio || 1;
  stacked = W < 720 && H > W * 1.1;   // a phone upright: the screen, and the panel as a strip to scroll
  document.body.classList.toggle("stacked", stacked);
  const showDevice = stacked || state.view === "device", showScreen = stacked || state.view === "screen";
  $("device-wrap").hidden = !showDevice;
  $("screen-wrap").hidden = !showScreen;
  if (stacked) {
    const sh = Math.min(W, Math.max(160, H - 230));
    $("screen-wrap").style.height = sh + "px";
    const dh = H - sh, dw = Math.round(dh * VIEW.w / VIEW.h);
    $("device").style.width = dw + "px"; $("device").style.height = dh + "px";
    device.layout(dw, dh, dpr);
    screen.layout(W, sh, dpr);
  } else {
    $("screen-wrap").style.height = "";
    $("device").style.width = ""; $("device").style.height = "";
    if (showDevice) device.layout(W, H, dpr);
    if (showScreen) screen.layout(W, H, dpr);
  }
  labelsLayer.layout();
  placeCorner();
  invalidate();
}
// the corner button: on the body's empty top-right corner (units 860, 46), or the screen view's bezel corner
function placeCorner() {
  const b = $("corner"), stage = $("stage").getBoundingClientRect();
  let x, y, d;
  if (!$("screen-wrap").hidden && (state.view === "screen" || stacked)) {
    const r = $("screen").getBoundingClientRect(), w = Math.min(r.width, r.height);
    d = Math.max(22, Math.min(28, Math.round(w * 0.045)));
    const bz = screen.bezel();
    const c = Math.max(d / 2 + 3, bz.x - (r.width - w) / 2);
    x = r.left - stage.left + (r.width + w) / 2 - c; y = r.top - stage.top + (r.height - w) / 2 + c;
  } else {
    const r = $("device").getBoundingClientRect(), p = device.toCss(860, 46);
    d = Math.max(22, Math.min(28, Math.round(17.5 * device.unitPx())));
    x = r.left - stage.left + p.x; y = r.top - stage.top + p.y;
  }
  Object.assign(b.style, { left: x - d / 2 + "px", top: y - d / 2 + "px", width: d + "px", height: d + "px" });
  // the start overlay: over the panel's LCD in the device view
  const st = $("start").style;
  if (!stacked && state.view === "device") {
    const r = $("device").getBoundingClientRect(), p = device.toCss(347, 160);
    const half = 250, sx = r.left - stage.left + p.x;
    st.setProperty("--start-x", (stage.width > 2 * half + 32 ? Math.max(half + 16, Math.min(stage.width - half - 16, sx)) : stage.width / 2) + "px");
    st.setProperty("--start-y", r.top - stage.top + p.y + "px");
  } else { st.removeProperty("--start-x"); st.removeProperty("--start-y"); }
}
let chromeTimer = 0;
function activity() {
  if (sheet && sheet.isOpen) return;
  document.body.classList.add("chrome");
  clearTimeout(chromeTimer);
  chromeTimer = setTimeout(() => { if (!$("corner").matches(":hover")) document.body.classList.remove("chrome"); }, 1500);
}

// ------------------------------------------------------------------ toast ---
let toastTimer = 0;
function toast(text, kind = "", ms = 0) {
  const t = $("toast");
  t.textContent = text; t.className = "toast show " + kind;
  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => { t.className = "toast " + kind; }, ms || (kind === "bad" ? 7000 : 4000));
}
function setStatus(text) { $("status").textContent = text; }

// ------------------------------------------------------------ the feedback form ---
function updateFeedback() {
  const ua = navigator.userAgent, platform = (navigator.userAgentData && navigator.userAgentData.platform) || "";
  const url = feedbackUrl({ meta: state.meta, setup: state.fm1 ? "fm1" : "screen", ua, platform });
  for (const a of document.querySelectorAll("a[data-feedback]")) a.href = url;
  return url;
}

// --------------------------------------------------------------- saves ---
async function refreshSaves() {
  state.saves = await saves.list(FW);
  if (sheet) sheet.render();
}

// (re)start FIZZ from a save, else its autosave, else (a new FIZZ version) the newest older version's autosave, carried
// over as a copy, else an erased flash. The saves of other versions are never changed or deleted.
let bootSeq = 0, carried = null;   // carried: { from, used, until } while a carried flash's first writes are watched
async function boot(fromSave = null, { blank = false } = {}) {
  const seq = ++bootSeq;
  if (!player || !state.meta) return;
  state.engineState = "loading"; state.info = null;
  setStatus("Loading FIZZ...");
  carried = null;
  try {
    let rec = blank ? null : fromSave || (await saves.get(FW, ENGINE, version(), saves.AUTOSAVE));
    if (!rec && !blank) {
      const o = await carry.check(FW, ENGINE, version()).catch(() => null);
      if (o) {
        rec = await carry.answer(o, true);
        carried = { from: o.from, used: usedBytes(rec.bytes), until: performance.now() + 12000 };
        await refreshSaves();
        toast(`Your FIZZ ${o.from} projects came along to ${o.to}. The ${o.from} save is kept as it was.`, "ok", 7000);
      }
    }
    await player.load(wasmUrl(), rec ? rec.bytes : null);
  } catch (err) {
    if (seq !== bootSeq) return;
    setStatus("");
    toast(err.message || String(err), "bad", 12000);
  }
}
const usedBytes = (b) => { let n = 0; for (let i = 0; i < b.length; i++) if (b[i] !== 0xff) n++; return n; };
// a carried flash a new FIZZ could not read: it formats its flash (most of the image erased) within seconds of booting
function checkCarried(bytes) {
  if (!carried) return;
  if (performance.now() > carried.until) { carried = null; return; }
  if (usedBytes(bytes) < carried.used / 2) {
    toast(`This FIZZ could not read your ${carried.from} projects and started blank. The ${carried.from} save is kept: export it from the settings (⋯, Saves) to keep it safe.`, "bad", 15000);
    carried = null;
  }
}

// a newer FIZZ version than the save's: its projects are carried over by itself (saves.js Carry; a copy)
function compareVersions(a, b) {
  const num = (v) => (String(v).match(/\d+/g) || []).map(Number);
  const x = num(a), y = num(b);
  for (let i = 0; i < Math.max(x.length, y.length); i++) { const d = (x[i] || 0) - (y[i] || 0); if (d) return d > 0 ? 1 : -1; }
  return 0;
}
const carry = new saves.Carry({
  compare: compareVersions,
  decisions: { get: (k) => (prefs.carry || {})[k], set: (k, v) => { prefs.carry = { ...(prefs.carry || {}), [k]: v }; savePrefs(); } },
});
const carryText = (o) => `Bring your FIZZ ${o.from} projects to ${o.to}?`;
async function answerCarry() { /* (carried by itself: boot) */ }

// ------------------------------------------------------------ a newer site ---
// fizz.json, never from a cache: a deploy newer than this page (its build, or FIZZ's commit) shows a note to reload
async function checkUpdate() {
  if (!state.meta) return;
  let j = null;
  try { j = await (await fetch("fizz.json", { cache: "no-store" })).json(); } catch (_) { return; }
  const newer = (j.site && BUILD !== "dev" && j.site !== BUILD) || j.commit !== state.meta.commit || j.bytes !== state.meta.bytes;
  const box = $("update");
  if (newer && box.hidden) {
    $("update-text").textContent = j.version !== state.meta.version ? `FIZZ ${j.version} is out.` : "A new FIZZ is out.";
    box.hidden = false;
  }
}

// ------------------------------------------------------------------ audio ---
let flashTimer = 0, pendingSave = null;
let lastSaved = "";
function quickHash(b) { let h = 2166136261; for (let i = 0; i < b.length; i += 7) h = Math.imul(h ^ b[i], 16777619); return (h >>> 0) + ":" + b.length; }
function makePlayer() {
  player = new Player({
    frame(px, seq) { state.pixels = px; state.frameSeq = seq; lcd.put(px); invalidate(); siteEmit("frame", px, seq); },
    leds(levels, green) { state.levels = levels; state.playGreen = green; device.setLeds(levels, green); invalidate(); siteEmit("leds", levels, green); },
    info(info) {
      state.info = info;
      state.engineState = "running";
      device.setButtonNames(info.buttonNames && info.buttonNames.length === 14 ? info.buttonNames : PANEL_LABELS);
      device.setKeymap(info.keymap || null);
      setStatus("");
      controls.emit();   // the panel as it stands, to the new engine
      invalidate();
      if (sheet) sheet.render();
      siteEmit("info", info); siteEmit("firmware", { entry: { id: FW, name: "FIZZ" }, engine: ENGINE });
      // the live labels: FIZZ is asked for them, and to send them again whenever they change
      if (LABELS_STUB) panelChanged();
      else if (labelsLink) labelsLink.request(true);
      // a carried flash: read back once FIZZ has settled (a FIZZ that cannot read it formats it while booting)
      if (carried) setTimeout(() => { if (carried && player) player.flashReq(); }, 2500);
    },
    // FIZZ writes its flash by itself (1.5 s after an edit): the image is fetched 0.4 s after it last moved and kept
    flash(m) {
      if (!m.bytes) { clearTimeout(flashTimer); flashTimer = setTimeout(() => player.flashReq(), 400); return; }
      checkCarried(m.bytes);
      const hsh = quickHash(m.bytes);
      if (lastSaved !== hsh) {
        lastSaved = hsh;
        saves.put(FW, ENGINE, version(), saves.AUTOSAVE, m.bytes).then(refreshSaves).catch(() => {});
      }
      if (pendingSave) { const f = pendingSave; pendingSave = null; f(m.bytes); }
    },
    halted(code) {
      toast(`FIZZ restarted (code ${code}).`);
      currentFlash().catch(() => null).then((bytes) => boot(bytes ? { name: saves.AUTOSAVE, bytes } : null));
    },
    midiOut(bytes) { siteEmit("midiOut", bytes); for (const f of midiOutSubs) f(bytes); },
    error(msg) {
      console.warn("worklet:", msg);
      if (/^load:/.test(msg)) { setStatus(""); toast(msg.replace(/^load:\s*/, "FIZZ could not start: ") + ".", "bad", 12000); }
      else if (/^render:/.test(msg)) toast(`FIZZ stopped: ${msg}. Reload the page (your projects are kept).`, "bad", 12000);
    },
  });
  player.setVolume(state.volume);
}
const midiOutSubs = new Set();   // FIZZ's MIDI out: the Digitone import's replies (nothing goes to a MIDI port)
setInterval(() => { if (state.running && !document.hidden && state.info) player.flashReq(); }, 30000);
const flushFlash = () => { if (state.running && player) player.flashReq(); };
document.addEventListener("visibilitychange", () => { if (document.hidden) flushFlash(); });
addEventListener("pagehide", flushFlash);
const currentFlash = () => new Promise((resolve, reject) => {
  if (!player || !state.info) return reject(new Error("FIZZ is not running."));
  pendingSave = resolve;
  player.flashReq();
  setTimeout(() => { if (pendingSave === resolve) { pendingSave = null; reject(new Error("FIZZ did not answer.")); } }, 3000);
});

async function start() {
  if (state.running) return true;
  try {
    if (!player) makePlayer();
    const ok = await player.start();
    if (!ok) return false;
  } catch (e) { toast(e.message || String(e), "bad"); return false; }
  state.running = true;
  document.body.classList.add("running");
  $("start").hidden = true;
  await boot();
  return true;
}

// ------------------------------------------------------------- my FM-1 ---
let fm1Tone = null;
function onFm1(v) {
  const b = $("start-fm1"), note = $("start-fm1-note");
  b.hidden = v.tone === "on";
  b.disabled = v.tone === "busy";
  b.querySelector(".fm1-dot").className = "fm1-dot " + v.tone + (v.live && v.tone !== "off" ? " live" : "");
  b.querySelector("span:last-child").textContent = v.tone === "busy" ? "Connecting your FM-1..." : "Play on my FM-1";
  b.title = v.title;
  note.hidden = v.tone !== "on" && v.tone !== "bad";
  note.className = "start-note " + v.tone;
  note.textContent = v.tone === "on" ? `Your FM-1 is connected${v.via ? " over " + v.via : ""}: press Play and it plays FIZZ.` : v.detail;
  if (fm1Tone !== null && v.tone !== fm1Tone) {
    if (v.tone === "on") toast(`FM-1 connected${v.via ? " over " + v.via : ""}: its keys, knobs and screen are FIZZ's panel.`, "ok");
    else if (v.tone === "bad") toast(v.detail, "bad");
  }
  fm1Tone = v.tone;
  state.fm1 = v.tone === "on";
  updateFeedback();
}
// one click: the sound and the unit (audio start, Web MIDI, the WebUSB chooser: the same user gesture)
function playOnFm1() {
  if (fm1 && !fm1.connected) fm1.connect();
  start();
}

// ------------------------------------------------------------------- MIDI ---
const midi = new MidiInput(onMidi, () => { if (sheet) sheet.render(); });
function onMidi(d) {
  if (!d || !d.length) return;
  const s = d[0] & 0xF0;
  if (state.notesPlayKeys && (s === 0x90 || s === 0x80) && d.length >= 3) {
    const k = d[1] - 53 - 12 * state.transpose, on = s === 0x90 && d[2] > 0;
    if (k >= 0 && k < 27) controls.holdKey(k, SRC.MIDI, on, on ? d[2] : 0);
    if (!state.reachFirmware) return;
  }
  if (player) player.midi(d);
}

// -------------------------------------------------------------------- app ---
const app = {
  get state() { return state; },
  midi,
  installerUrl: INSTALLER,
  guideUrl: "guide.html",
  remoteMounted: false,
  remoteStatus: "",
  version, carryText, answerCarry,
  feedbackUrl: () => updateFeedback(),
  sheetChanged(open) { document.body.classList.toggle("sheet-open", open); if (open) document.body.classList.remove("chrome"); else $("stage").focus({ preventScroll: true }); },
  set(k, v) {
    if (k === "theme") { state.theme = presetNamed(v) || state.theme; prefs.theme = state.theme.name; applyTheme(); }
    else if (k === "view") { state.view = v; prefs.view = v; layout(); }
    else if (k === "labels") { state.labels = !!v; prefs.labels = !!v; labelsLayer.show(state.labels); invalidate(); }
    else if (k === "keyHints") { state.keyHints = !!v; prefs.keyHints = !!v; device.setHints(state.keyHints); invalidate(); }
    else { state[k] = v; prefs[k] = v; }
    savePrefs();
    if (sheet) sheet.render();
  },
  setVolume(v, keep) {
    state.volume = Math.max(0, Math.min(1, +v || 0));
    if (player) player.setVolume(state.volume);
    if (keep) { prefs.volume = state.volume; savePrefs(); }
  },
  async enableMidi() { if (await midi.enable()) { if (!midi.selected) midi.select(prefs.midi || "*"); } },
  selectMidi(id) { midi.select(id); prefs.midi = id; savePrefs(); },
  async loadSave(rec) { state.selectedSave = rec.name; if (sheet) sheet.render(); if (state.running) await boot(rec); toast(`Started from "${rec.name}".`); },
  async saveAs(name) {
    try { const b = await currentFlash(); await saves.put(FW, ENGINE, version(), name, b); state.selectedSave = name; await refreshSaves(); toast(`Saved "${name}".`, "ok"); }
    catch (err) { toast(err.message, "bad"); if (sheet) sheet.render(); }
  },
  async saveOver(rec) { if (rec) return app.saveAs(rec.name); },
  async renameSave(rec, to) { await saves.rename(rec.fw, rec.engine, rec.version, rec.name, to); state.selectedSave = to; await refreshSaves(); },
  async deleteSave(rec) { await saves.remove(rec.fw, rec.engine, rec.version, rec.name); state.selectedSave = null; await refreshSaves(); toast(`Deleted "${rec.name}".`); },
  async eraseFlash() {
    await saves.remove(FW, ENGINE, version(), saves.AUTOSAVE).catch(() => {});
    lastSaved = ""; state.selectedSave = null;
    await boot(null, { blank: true });
    await refreshSaves();
    toast("FIZZ starts blank.");
  },
  exportSaves() {
    const json = saves.backupJson(state.saves);
    const a = Object.assign(document.createElement("a"), { href: URL.createObjectURL(new Blob([json], { type: "application/json" })),
      download: `fizz-saves-${new Date().toISOString().slice(0, 10)}.json` });
    document.body.append(a); a.click(); a.remove();
    setTimeout(() => URL.revokeObjectURL(a.href), 5000);
  },
  async importSaves(file) {
    try {
      const buf = new Uint8Array(await file.arrayBuffer());
      if (buf[0] === 0x7B) { const n = await saves.importBackup(new TextDecoder().decode(buf)); toast(`Imported ${n} save${n === 1 ? "" : "s"}.`, "ok"); }
      else {   // a raw flash image (FM1VST's 1 MiB core flash): one named save
        const name = file.name.replace(/\.[^.]+$/, "").slice(0, 40) || "Imported";
        await saves.put(FW, ENGINE, version(), name, buf);
        state.selectedSave = name;
        toast(`Imported "${name}".`, "ok");
      }
      await refreshSaves();
    } catch (err) { toast(err.message || String(err), "bad"); }
  },
};

function applyTheme() {
  device.setTheme(state.theme); screen.setTheme(state.theme);
  labelsLayer.setTheme(derive(state.theme));
  const p = derive(state.theme);
  document.documentElement.style.setProperty("--plate", css(p.plate));
  document.documentElement.style.setProperty("--backdrop", css(p.backdrop));
  invalidate();
}

// ------------------------------------------------------------------- boot ---
async function main() {
  $("corner").innerHTML = icon("dots");
  $("menu-btn").innerHTML = icon("dots");
  applyTheme();
  updateFeedback();
  try {
    const r = await fetch("fizz.json", { cache: "no-store" });
    state.meta = await r.json();
  } catch (e) {
    toast("FIZZ's build information could not be loaded. Reload the page.", "bad");
    return;
  }
  for (const el of document.querySelectorAll("[data-version]")) el.textContent = state.meta.version;
  document.title = `FIZZ ${state.meta.version} · preview`;
  if (state.meta.site && BUILD !== "dev" && state.meta.site !== BUILD) checkUpdate();   // (this page is older than the deploy)
  updateFeedback();
  await refreshSaves();

  sheet = new Sheet(app);
  const made = makeSite({ controls, getState: () => state, installerUrl: INSTALLER,
    setStatus: (t) => { app.remoteStatus = t; const el = sheet.remoteSlot.querySelector(".remote-status"); if (el) el.textContent = t; },
    midiIn: (b) => player && player.midi(b) });
  site = made.site; siteEmit = made.emit;
  window.FM1Site = site;
  // Connect FM-1: RemoteFM1's web back end (remote/remote.js, remote/remotefm1.mjs: site/build_remote.sh)
  import(new URL(ver("remote/remote.js"), document.baseURI).href).then((m) => {
    app.remoteMounted = true;
    if (typeof m.isCandidatePortName === "function") midi.isUnitPort = m.isCandidatePortName;
    fm1 = createFm1(site, m, { moduleUrl: new URL(ver("remote/remotefm1.mjs"), document.baseURI).href,
                               onUnitPorts: (ports) => { midi.unitPorts = ports; midi.bind(); } });
    mountUseFm1(sheet.remoteSlot, fm1, site);
    mountHeader($("fm1-btn"), fm1, { openSheet: () => sheet.open() });
    fm1.on(onFm1);
    onFm1(fm1.view());
    sheet.render();
    fm1.auto();                                          // connects now if MIDI was allowed before (no prompt)
  }).catch((e) => { console.warn("remote:", e); app.remoteStatus = "Connect FM-1 could not load."; if (sheet) sheet.render(); });

  // Digitone sounds (js/digitone-ui.js): read here, sent into FIZZ's MIDI in as its sound SysEx
  window.FIZZDigitone = mountDigitone({ button: $("dn-btn"), sendMidi: (b) => player && player.midi(b),
    onMidiOut: (cb) => midiOutSubs.add(cb), ensureRunning: async () => { if (!state.running) await start(); }, toast });

  labelsLink = createLabelsLink({ sendMidi: (b) => player && player.midi(b), onMidiOut: (cb) => midiOutSubs.add(cb), onLabels: (l) => labelsLayer.set(l) });

  // input
  new Keyboard({
    controls,
    enabled: (e) => { if (sheet.isOpen && sheet.el.contains(e.target)) return false; if (!state.running) start(); return true; },
    onLcdView: () => app.set("view", state.view === "screen" ? "device" : "screen"),
    onActivity: activity,
  });
  attachPointer($("device"), device, controls, { onActivity: () => { activity(); if (!state.running) start(); }, onScreenTap: () => {} });
  $("screen").addEventListener("pointerdown", () => { activity(); if (!state.running) start(); });
  $("stage").addEventListener("pointermove", activity);
  $("corner").addEventListener("click", () => sheet.open());
  $("corner").addEventListener("mouseleave", activity);
  $("menu-btn").addEventListener("click", () => sheet.open());
  $("start-btn").addEventListener("click", () => start());
  $("start-fm1").addEventListener("click", playOnFm1);
  $("update-btn").addEventListener("click", () => location.reload());
  $("update-no").addEventListener("click", () => { $("update").hidden = true; });
  addEventListener("focus", checkUpdate);
  document.addEventListener("visibilitychange", () => { if (!document.hidden) checkUpdate(); });
  setInterval(() => { if (!document.hidden) checkUpdate(); }, 10 * 60 * 1000);
  for (const a of document.querySelectorAll("a[data-feedback]")) a.addEventListener("click", () => updateFeedback());
  addEventListener("keydown", (e) => {
    if (e.key !== "Escape" || !sheet.isOpen) return;
    e.stopImmediatePropagation(); e.preventDefault();
    if (sheet.inline) { sheet.inline = null; sheet.render(); } else if (sheet.page !== "main") sheet.show("main"); else sheet.close();
  }, true);
  addEventListener("resize", layout);
  new ResizeObserver(layout).observe($("stage"));
  layout();

  // audio starts at once where the browser allows it, else on the first click or key
  if (!player) makePlayer();
  if (await player.tryAutostart()) return start();
  $("start").hidden = false;
}

// (for tests and the curious: the page's state and its player, read only in spirit)
window.FIZZ = {
  get state() { return state; }, get player() { return player; }, controls, start, boot, checkUpdate, BUILD, labels: labelsLayer, setLabels: (v) => app.set("labels", v),
  // the output's peak since the last call (an AnalyserNode after the worklet)
  peak() {
    if (!player || !player.ctx) return 0;
    if (!this._an) { this._an = player.ctx.createAnalyser(); this._an.fftSize = 2048; player.node.connect(this._an); this._buf = new Float32Array(2048); }
    this._an.getFloatTimeDomainData(this._buf);
    let p = 0; for (const v of this._buf) p = Math.max(p, Math.abs(v));
    return p;
  },
  currentFlash,
};
main();
