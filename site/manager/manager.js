// SPDX-License-Identifier: GPL-3.0-only
// FIZZ Manager (docs/WEB-MANAGER-PLAN.md §9): connect to an FM-1 running FIZZ over Web MIDI (Chrome, Edge), back up
// its whole store to a .fizzbak and put one back (Phase 0); edit its library (tags, rename, delete, reorder), its
// project's pool (pool.js) and import Digitone sounds into it (importer.js) (Phase 1: FIZZ's STATUS .. PROJECT
// commands, docs/SYSEX.md); edit a preset's sound (editor.js: in the browser's own FIZZ, browserfizz.js) and export
// it for FIZZ, the Digitone or the Digitone II (exporter.js) (Phase 2-A, docs/MANAGER-P2-PLAN.md). The protocol is
// fizzlink.js's; the files fizzbak.js's; the offline store reader fizzstore.js's. P2-B (live.js): EDIT follows the
// FM-1's track; with TRACK on it edits it live and LIBRARY loads onto it (KEEP / CANCEL); off, the tracks are left
// alone. The owner's fixes (2026-10-25): LIBRARY's EXPORT and EXPORT ALL, LOAD ON FM-1 (the unit's own browser), the
// pool's changes shown at once (then read back until the unit agrees), IMPORT takes FIZZ's own files.
//   ?sim        a simulated FM-1 (simfizz.js: the browser FIZZ for its SysEx) instead of Web MIDI: headless checks
//   ?depth=N    the backup's requests outstanding (1, the default: one at a time); &gap=MS their spacing
import { findFizz, FZER } from "./fizzlink.js";
import { makeFizzbak, readBackup, backupFileName, soundKey } from "./fizzbak.js";
import { readRegion, layoutOf } from "./fizzstore.js";
import { parseDump, FIZZ_TAGS } from "../js/fizz_sysex.js";
import { soundFromStoreFrame, soundToFizzsnd, soundFileName, makeFizzlib, libraryFileName, sameSound } from "../js/library.js";
import { storeFrame } from "../js/digitone.js";
import { $, esc, tagsOf, colOf, algo, emptyArt, refusal, confirmBox, MIDI_I, midiChip } from "./ui.js";
import { newFilter, filterHtml, filterClick, listHtml, poolPlaces, poolBadge } from "./presets.js";
import { mountPool } from "./pool.js";
import { mountImport } from "./importer.js";
import { mountProjects } from "./projtab.js";
import { mountEditor } from "./editor.js";
import { mountExport } from "./exporter.js";
import { BrowserFizz } from "./browserfizz.js";
import { mountLive } from "./live.js";
import { soundFromFrame, soundFromLib, storeFrameOf, libSound, tagNames, copySound, isMidiBlob } from "./sound.js";
import { parseFizzsnd } from "../js/library.js";
import { readFiles } from "../js/digitone.js";

const Q = new URLSearchParams(location.search);
const SIM = Q.has("sim");
const DEPTH = Math.max(1, Math.min(4, +Q.get("depth") || 2)), GAP = Math.max(0, +Q.get("gap") || 0);   // (2 in flight: fm1-base holds two SysEx frames since 2026-10-27)
const kb = (n) => `${Math.round(n / 1024)} KB`;
const mmss = (ms) => { const s = Math.floor(ms / 1000); return `${Math.floor(s / 60)}:${String(s % 60).padStart(2, "0")}`; };

const S = { mode: "fm1", access: null, link: null, unit: null, busy: false, baks: [], web: { tags: {} }, sel: null, filter: newFilter(), abort: null,
  editing: 0, users: new Map(), refreshing: false };

// ---------------------------------------------------------------- the log, what the page says
const logLines = [];
function log(line) {
  const t = new Date().toISOString().slice(11, 19);
  logLines.push(`${t} ${line}`);
  console.log(`[manager] ${line}`);
  const el = $("log");
  el.textContent = logLines.slice(-400).join("\n");
  el.scrollTop = el.scrollHeight;
}
function say(html = "", c = "--cyan") { const el = $("say"); el.style.setProperty("--c", `var(${c})`); el.innerHTML = html; }
let toastT = null;
function toast(text, c = "--green") {
  const el = $("toast");
  el.style.setProperty("--c", `var(${c})`);
  el.textContent = text;
  el.hidden = false;
  clearTimeout(toastT);
  toastT = setTimeout(() => { el.hidden = true; }, 4200);
}
function download(bytes, name, type = "application/octet-stream") {
  const url = URL.createObjectURL(new Blob([bytes], { type }));
  const a = document.createElement("a");
  a.href = url; a.download = name; a.rel = "noopener";
  document.body.appendChild(a); a.click(); a.remove();
  setTimeout(() => URL.revokeObjectURL(url), 30000);
  log(`saved ${name} (${bytes.length} B)`);
}

// a file saved where the user picks (the File System Access API: Chrome, Edge), else an ordinary download.
// -> the name saved under, or null (cancelled)
async function saveFile(bytes, name, { what = "FIZZ backup", ext = ".fizzbak" } = {}) {
  if (typeof window.showSaveFilePicker === "function") {
    try {
      const h = await window.showSaveFilePicker({ id: "fizz-backups", suggestedName: name, types: [{ description: what, accept: { "application/octet-stream": [ext] } }] });
      const w = await h.createWritable();
      await w.write(bytes);
      await w.close();
      log(`saved ${h.name} (${bytes.length} B)`);
      return h.name;
    } catch (e) {
      if (e && e.name === "AbortError") return null;
      log(`save: ${e && e.message}: as a download instead`);
    }
  }
  download(bytes, name);
  return name;
}

// ---------------------------------------------------------------- the backups kept in this browser (IndexedDB)
const DB = (() => {
  let db = null;
  const open = () => db || (db = new Promise((res) => {
    try {
      const r = indexedDB.open("fizz-manager", 1);
      r.onupgradeneeded = () => { r.result.createObjectStore("backups", { keyPath: "id", autoIncrement: true }); r.result.createObjectStore("kv"); };
      r.onsuccess = () => res(r.result);
      r.onerror = () => res(null);
    } catch (_) { res(null); }
  }));
  const tx = async (store, mode, fn) => {
    const d = await open();
    if (!d) return null;
    return new Promise((res) => {
      try {
        const t = d.transaction(store, mode), r = fn(t.objectStore(store));
        t.oncomplete = () => res(r && "result" in r ? r.result : null);
        t.onerror = t.onabort = () => res(null);
      } catch (_) { res(null); }
    });
  };
  return {
    all: () => tx("backups", "readonly", (s) => s.getAll()),
    add: (rec) => tx("backups", "readwrite", (s) => s.add(rec)),
    set: (rec) => tx("backups", "readwrite", (s) => s.put(rec)),
    del: (id) => tx("backups", "readwrite", (s) => s.delete(id)),
    get: (k) => tx("kv", "readonly", (s) => s.get(k)),
    put: (k, v) => tx("kv", "readwrite", (s) => s.put(v, k)),
  };
})();
// (a backup is kept here only, until its save button writes it to a file: none is ever dropped by the page; the
// browser is asked to keep the site's storage, persist(), so it is not evicted under pressure either)
async function loadBaks() {
  S.baks = ((await DB.all()) || []).sort((a, b) => b.date - a.date);
  S.web = (await DB.get("web")) || { tags: {} };
  renderBaks();
}
let persisted = false;
async function keepBak(rec) {
  if (!persisted && navigator.storage && navigator.storage.persist) { persisted = true; navigator.storage.persist().then((ok) => log(`storage kept: ${ok}`)).catch(() => {}); }
  const id = await DB.add(rec);
  rec.id = id ?? `mem-${Date.now()}`;
  S.baks.unshift(rec);
  renderBaks();
}
// saved to a file: the records from before the page kept them here (it downloaded every backup then) count as saved
const isSaved = (b) => b.saved !== false;
async function saveBak(b) {
  const name = await saveFile(b.bytes, b.name);
  if (!name) return;
  b.saved = true;
  if (typeof b.id === "number") await DB.set(b);
  renderBaks();
  toast(`Saved ${name}`);
}
const saveWeb = () => DB.put("web", S.web);

// ---------------------------------------------------------------- the unit
function setUnit(state, label, sub = "") {
  const u = $("unit");
  u.dataset.state = state;
  $("unit-label").textContent = label;
  $("unit-sub").textContent = sub;
  u.disabled = state === "look";
}
function renderUnit() {
  const u = S.unit, on = !!(u && S.link);
  const short = u && u.version ? u.version.replace(/-\d+-g[0-9a-f]+$/, "") : "";   // (a development build: git describe)
  $("u-ver").textContent = u ? (short ? `FIZZ ${short}` : u.id) : "FM-1";
  $("u-ver").classList.toggle("dim", !on);
  const mine = u ? u.rows.filter((r) => r.source === "mine") : [];
  const sum = u && u.summary, st = u && u.status;
  $("u-pre").textContent = u && u.rows.length ? mine.length : "–";
  $("u-prj").textContent = st ? st.projects : sum ? sum.projects.length : "–";
  // the store's use: STATUS's pages (Phase 1: no backup needed), else the last backup's reading of it
  const use = st && st.lib[1] ? { lib: st.lib, prj: st.prj, all: (st.lib[0] + st.prj[0]) / (st.lib[1] + st.prj[1]) } : sum ? sum.used : null;
  $("u-use").textContent = use ? `${use.all > 0 ? Math.max(1, Math.round(use.all * 100)) : 0} %` : "–";
  const L = use && (st ? { lib: { nsec: st.lib[1] }, prj: { nsec: st.prj[1] } } : layoutOf(sum.size));
  const parts = L ? [use.lib[0] / use.lib[1] * L.lib.nsec, (1 - use.lib[0] / use.lib[1]) * L.lib.nsec,
    use.prj[0] / use.prj[1] * L.prj.nsec, (1 - use.prj[0] / use.prj[1]) * L.prj.nsec] : [0, 0, 0, 0];
  const tot = parts.reduce((a, b) => a + b, 0) || 1;
  [...$("u-meter").children].forEach((s, i) => { s.style.width = `${parts[i] / tot * 100}%`; });
  $("u-kb").textContent = u && u.info ? kb(u.info.size) : "";
  $("go-backup").disabled = !on || S.busy || !u.info;
  $("go-presets").disabled = !on || S.busy || !u.version;
  $("l-export-all").disabled = !on || S.busy || !u.version;
  $("refresh").hidden = !on;
  $("refresh").disabled = S.busy || S.refreshing;
}

async function connect() {
  if (S.busy) return;
  if (!S.access) {
    if (SIM) S.access = await simAccess();
    else {
      if (!navigator.requestMIDIAccess) { say("This browser has no Web MIDI with SysEx. Open this page in <b>Chrome</b> or <b>Edge</b> on a computer.", "--red"); return; }
      try { S.access = await navigator.requestMIDIAccess({ sysex: true }); }
      catch (_) { say("MIDI (SysEx) was not allowed. Allow it for this site in the address bar, then press CONNECT.", "--red"); setUnit("bad", "NO MIDI"); return; }
    }
    S.access.onstatechange = onState;
  }
  await attach();
}
let attaching = false;
async function attach() {
  if (attaching || S.link) return;
  attaching = true;
  setUnit("look", "LOOKING…");
  say("");
  try {
    const link = await findFizz(S.access, { log });
    link.log = log;
    link.onLost = () => lost();
    S.link = link;
    const version = await link.identity();
    const info = await link.storeInfo();
    const rows = version ? await link.list({ factory: true, pool: true }) : [];
    const status = version ? await link.status() : null;       // (Phase 1; an older FIZZ: none, the library read only)
    const projects = status ? await link.projects() : null;
    const keep = S.unit && S.unit.id === link.id ? S.unit : null;
    S.unit = { id: link.id, port: link.port, version, info, rows, dumps: null, fdumps: null, summary: keep && keep.summary, status, projects };
    log(`connected: ${link.id} on "${link.port}", FIZZ ${version || "(no answer)"}, store ${info ? `${info.size} B at 0x${info.lo.toString(16)}, CRC-32 ${info.crc.toString(16)}` : "(no FZBI)"}, ` +
      `${rows.filter((r) => r.source === "mine").length} my presets`);
    const sub = version ? `FIZZ ${version.replace(/-\d+-g[0-9a-f]+$/, "")}${status ? ` · ${status.tracks} tracks` : ""}` : link.id;
    if (S.mode === "browser") { setUnit("idle", "FM-1"); setB("on", sub); } else { setUnit("on", "FM-1", sub); setB("off"); }
    if (!version && info) say("FIZZ does not answer (safe mode, or its data missing): back up and restore still work.", "--orange");
    else if (!info) say(`This FIZZ has no backup over USB yet: <a href="../install/">update FIZZ</a>.`, "--orange");
    else if (version && !status) say(`This FIZZ shows its library but cannot edit it yet: <a href="../install/">update FIZZ</a>.`, "--orange");
    else if (status && status.noStore) say("FIZZ runs without its store (safe mode): back up and restore still work.", "--orange");
    if ($("tab-backup").hidden) loadDumps();
    if (status) live.probe().then(() => { if (!$("tab-edit").hidden) live.show(); });
  } catch (e) {
    S.link = null;
    const m = {
      notfound: ["NO FM-1", "No FM-1 answered. Check the USB cable, and close other apps and tabs that use MIDI (on Windows only one may)."],
      notfizz: ["NOT FIZZ", `This FM-1 runs ${esc(e.detail || "another firmware")}, not FIZZ. <a href="../install/">Install FIZZ</a>.`],
      update: ["UPDATE MODE", `The FM-1 is in update mode. Finish the install on the <a href="../install/">install page</a>.`],
    }[e.code] || ["NO FM-1", esc(e.message)];
    setUnit("bad", m[0]);
    say(m[1], "--red");
    log(`connect: ${e.code || ""} ${e.message}`);
  } finally {
    attaching = false;
    renderUnit(); renderLibrary(); renderBaks(); pool.render(); importer.render(); projtab.render();
    if (S.link && !$("tab-projects").hidden) projtab.show();
  }
}
function lost() {
  if (!S.link) return;
  S.link = null;
  live.lost();
  setUnit("bad", "UNPLUGGED");
  log("the FM-1 was disconnected");
  renderUnit(); renderLibrary(); renderBaks(); pool.render(); importer.render(); projtab.render();
}
let stateT = null;
function onState(ev) {
  const p = ev && ev.port;
  if (p && p.state === "disconnected" && S.link) { try { S.link.close(); } catch (_) { /* gone */ } lost(); return; }
  clearTimeout(stateT);
  if (!S.link && !S.busy) stateT = setTimeout(attach, 800);
}
$("unit").addEventListener("click", () => { if (S.mode === "browser") { leaveBrowser(); connect(); } else if (!S.link) connect(); });
$("bunit").addEventListener("click", () => (S.mode === "browser" ? leaveBrowser() : useBrowser()));

// ---------------------------------------------------------------- the progress screen
const P = { t0: 0, tick: null, nsec: 0, lib: 0 };
function progOpen(label, size, write = false) {
  const L = layoutOf(size) || { lib: { nsec: 0 }, prj: { nsec: Math.ceil(size / 4096) } };
  P.nsec = L.lib.nsec + L.prj.nsec; P.lib = L.lib.nsec;
  const cells = [];
  for (let k = 0; k < P.nsec; k++) { if (k === P.lib && P.lib) cells.push('<i class="gap"></i>'); cells.push(`<i data-k="${k}"></i>`); }
  $("p-sec").innerHTML = cells.join("");
  $("p-sec").style.gap = P.nsec > 120 ? "1px" : "";
  $("p-sec").classList.toggle("many", P.nsec > 120);
  $("p-sec").classList.toggle("write", write);
  $("p-label").textContent = label;
  $("p-label").classList.toggle("warn", write);
  $("p-pct").classList.toggle("write", write);
  $("p-cancel").hidden = false;
  $("prog").hidden = false;
  P.t0 = performance.now();
  clearInterval(P.tick);
  P.tick = setInterval(() => { $("p-time").textContent = mmss(performance.now() - P.t0); }, 250);
  progSet(0, -1);
}
function progSet(frac, off, pass = 0) {
  $("p-pct").innerHTML = `${Math.floor(Math.max(0, Math.min(1, frac)) * 100)}<small>%</small>`;
  if (off < 0) return;
  const k = Math.floor(off / 4096), cells = $("p-sec").querySelectorAll("i[data-k]");
  cells.forEach((c, i) => { c.className = i < k ? (i < P.lib ? "lib" : "prj") : i === k ? "now" : ""; });
  if (pass) $("p-sec").classList.toggle("write", pass === 2);
}
function progClose() { clearInterval(P.tick); $("prog").hidden = true; }
$("p-cancel").addEventListener("click", () => { if (S.abort) S.abort.abort(); });

// ---------------------------------------------------------------- back up
async function readUnit(label, { presetsOnly = false } = {}) {
  const link = S.link, u = S.unit;
  S.abort = new AbortController();
  progOpen(label, u.info ? u.info.size : 4096);
  let image = null, stats = null, crc = null, summary = null;
  if (!presetsOnly) {
    const r = await link.backup({ depth: DEPTH, gapMs: GAP, signal: S.abort.signal, onProgress: (f, off) => progSet(f * 0.85, off) });
    image = r.image; stats = r.stats; crc = r.crc;
    summary = readRegion(image);
    u.summary = summary;
  }
  $("p-cancel").hidden = true;
  let frames = [];
  if (u.version) {
    u.rows = await link.list({ factory: true, pool: true });
    const mine = u.rows.filter((r) => r.source === "mine");
    const t0 = performance.now();
    frames = await link.dumpAll("mine", mine.length, (n, t) => progSet((presetsOnly ? 0 : 0.85) + (presetsOnly ? 1 : 0.15) * n / t, -1));
    log(`DUMP: ${frames.length} of ${mine.length} presets in ${(performance.now() - t0).toFixed(0)} ms`);
    if (frames.length < mine.length) {                  // (a reply lost on the way: the missing ones asked for one by one)
      const got = new Set(frames.map((f) => parseDump(f).slot));
      for (const r of mine) if (!got.has(r.slot)) { const f = await link.dump("mine", r.slot).catch(() => null); if (f) frames.push(f); }
      log(`DUMP: ${frames.length} of ${mine.length} after asking for the missing ones`);
    }
    if (frames.length < mine.length) throw Object.assign(new Error(`only ${frames.length} of ${mine.length} presets came back`), { code: "dump" });
    u.dumps = new Map(frames.map((f) => [parseDump(f).slot, f]));
  } else if (summary) {
    frames = null;
  }
  progSet(1, -1);
  const presets = frames ? frames.map((f) => ({ ...soundFromStoreFrame(f), source: { fizz: u.version || "", from: `MY PRESETS ${parseDump(f).slot}` } }))
    : summary.presets.map((p) => ({ fizz: true, name: p.name, blob: p.blob, drive: p.drive, pan: p.pan, sends: p.sends, fizzTags: tagsOf(p.tags), favourite: false, source: { from: `MY PRESETS ${p.slot}` }, notes: [], origin: [], mark: "", tags: [] }));
  return { image, crc, stats, summary, presets };
}
function webFor(presets) {
  const tags = {};
  for (const s of presets) { const k = soundKey(s); if (S.web.tags[k] && S.web.tags[k].length) tags[k] = S.web.tags[k]; }
  return { tags };
}
async function backup(kind, { safety = false } = {}) {
  if (S.busy || !S.link) return null;
  S.busy = true; renderUnit();
  const date = new Date();
  try {
    const r = await readUnit(safety ? "SAVING THE FM-1 FIRST" : kind === "full" ? "BACKING UP" : "PRESETS", { presetsOnly: kind === "presets" });
    const u = S.unit;
    const transfer = r.stats && { bytes: r.stats.bytes, ms: Math.round(r.stats.ms), kbps: +r.stats.kbps.toFixed(1), frames: r.stats.frames,
      resent: r.stats.resent, depth: r.stats.depth, gapMs: r.stats.gapMs, perFrameMs: +r.stats.perFrame.toFixed(2),
      rttMs: { median: +r.stats.rtt.median.toFixed(2), p95: +r.stats.rtt.p95.toFixed(2), max: +r.stats.rtt.max.toFixed(2) } };
    const file = makeFizzbak({ kind, image: r.image, crc: r.crc, region: u.info ? [u.info.lo, u.info.size] : null, summary: r.summary,
      presets: r.presets, fizz: u.version || "", unit: { identity: u.id, port: u.port }, web: webFor(r.presets), transfer, date });
    const name = safety ? backupFileName(date, kind).replace("FIZZ backup", "FIZZ before restore") : backupFileName(date, kind);
    await keepBak({ date: +date, name, kind, safety, saved: false, fizz: u.version || "", identity: u.id, bytes: file, size: file.length,
      tracks: r.summary ? r.summary.tracks : null, layout: r.summary ? r.summary.layout : null,
      projects: r.summary ? r.summary.projects.map((p) => p.name) : null, presets: r.presets.length });
    if (r.stats) showSpeed(r.stats);
    log(`kept ${name} in this browser (${file.length} B)`);
    if (!safety) toast("Backed up · kept in this browser");
    return { file, ...r };
  } catch (e) {
    log(`backup: ${e.code || ""} ${e.message}`);
    if (e.code === "cancel") toast("Cancelled", "--muted");
    else if (e.code === "lost") say("The FM-1 was disconnected during the backup. Nothing was saved.", "--red");
    else if (e.code === "changed") say("The FM-1 kept changing its store while it was read. Stop it, wait a moment, and back up again.", "--orange");
    else say(`The backup did not finish: ${esc(e.message)}`, "--red");
    return null;
  } finally {
    progClose();
    S.busy = false; S.abort = null;
    renderUnit(); renderLibrary(); renderBaks();
  }
}
function showSpeed(st) {
  const el = $("speed");
  el.hidden = false;
  el.innerHTML = `<span><b>${st.kbps.toFixed(1)} KB/s</b></span><span><b>${st.perFrame.toFixed(1)} ms</b> a read</span><span><b>${mmss(st.ms)}</b></span>` +
    (st.resent ? `<span><b>${st.resent}</b> sent again</span>` : "") + (st.depth > 1 ? `<span>depth <b>${st.depth}</b> gap <b>${st.gapMs}</b></span>` : "");
}
$("go-backup").addEventListener("click", () => backup("full"));
$("go-presets").addEventListener("click", () => backup("presets"));

// ---------------------------------------------------------------- the list of backups
const ICON = '<svg viewBox="0 0 22 22" aria-hidden="true"><rect x="2" y="5" width="18" height="12" rx="3"/><circle cx="8" cy="11" r="2"/><circle cx="14" cy="11" r="2"/></svg>';
const SAVE_I = '<svg viewBox="0 0 20 20"><path d="M10 3v10M5 9l5 5 5-5M3 17h14"/></svg>';
const DEL_I = '<svg viewBox="0 0 20 20"><path d="M5 5l10 10M15 5L5 15"/></svg>';
const PLAY_I = '<svg viewBox="0 0 16 16" aria-hidden="true"><path d="M4 2.5l9.5 5.5L4 13.5z"/></svg>';
function whenText(t) {
  const d = new Date(t), now = new Date(), hm = `${String(d.getHours()).padStart(2, "0")}:${String(d.getMinutes()).padStart(2, "0")}`;
  const day = (x) => new Date(x.getFullYear(), x.getMonth(), x.getDate()).getTime();
  const dd = Math.round((day(now) - day(d)) / 86400000);
  return dd === 0 ? `Today ${hm}` : dd === 1 ? `Yesterday ${hm}` : `${d.getDate()} ${d.toLocaleString("en", { month: "short" })} ${hm}`;
}
function unitTracks() { return S.unit && S.unit.summary ? S.unit.summary.tracks : null; }
function renderBaks() {
  const el = $("baks"), u = S.unit;
  el.innerHTML = S.baks.map((b, i) => {
    const old = b.kind === "full" && ((u && u.info && b.layout && layoutOf(u.info.size) && layoutOf(u.info.size).name !== b.layout) ||
      (b.tracks && unitTracks() && b.tracks !== unitTracks()) || (b.tracks === 8));
    const what = b.kind === "presets" ? `presets only · ${b.presets}` : `${b.projects ? b.projects.length : "?"} project${b.projects && b.projects.length === 1 ? "" : "s"} · ${b.presets} preset${b.presets === 1 ? "" : "s"} · ${kb(b.size)}${b.tracks === 8 ? " · 8 tracks" : ""}`;
    return `<div class="bk ${b.kind === "presets" ? "presets" : old ? "old" : b.safety ? "safety" : ""}" data-i="${i}">
      <span class="ic">${ICON}</span><div class="txt"><div class="when">${esc(whenText(b.date))}</div><div class="what">${esc(what)}</div></div>
      ${b.fizz ? `<span class="ver ${old ? "old" : ""}">${esc(b.fizz.replace(/-\d+-g[0-9a-f]+$/, ""))}</span>` : ""}
      <button class="icon ${isSaved(b) ? "saved" : "unsaved"}" data-act="save" aria-label="Save ${esc(b.name)} to a file${isSaved(b) ? " (saved before)" : " (not saved to a file yet)"}" title="${isSaved(b) ? "Save to a file again" : "Save to a file"}">${SAVE_I}</button>
      <button class="icon" data-act="del" aria-label="Delete from this browser" title="Delete from this browser">${DEL_I}</button>
      ${b.kind === "full" && typeof b.id === "number" ? `<button class="icon play" data-act="play" aria-label="Play ${esc(b.name)} in the browser's FIZZ" title="Play in the browser's FIZZ">${PLAY_I}</button>` : ""}
      <button class="btn small ghost" data-act="restore" ${S.link && !S.busy ? "" : "disabled"}>RESTORE</button></div>`;
  }).join("");
  renderUse();
}
const mb = (n) => (n >= 1073741824 ? `${+(n / 1073741824).toFixed(1)} GB` : n >= 1048576 ? `${(n / 1048576).toFixed(1)} MB` : kb(n));
let quota = null;
function renderUse() {
  const n = S.baks.length, sum = S.baks.reduce((a, b) => a + (b.size || 0), 0);
  $("b-use").innerHTML = n ? `${n} · ${mb(sum)}${quota ? `<small>${esc(quota)}</small>` : ""}` : "";
}
if (navigator.storage && navigator.storage.estimate) {
  navigator.storage.estimate().then((e) => { if (e && e.quota) { quota = `of ${mb(e.quota)}`; renderUse(); } }).catch(() => {});
}
$("baks").addEventListener("click", async (ev) => {
  const btn = ev.target.closest("button[data-act]");
  if (!btn) return;
  const b = S.baks[+btn.closest(".bk").dataset.i];
  if (!b) return;
  if (btn.dataset.act === "save") saveBak(b);
  else if (btn.dataset.act === "del") {
    const ok = await confirmBox({ title: "DELETE BACKUP", what: whenText(b.date), go: "DELETE",
      sub: isSaved(b) ? "From this browser. Its saved file stays." : "<b style='color:var(--yellow)'>Not saved to a file: this is its only copy.</b>" });
    if (!ok) return;
    if (typeof b.id === "number") await DB.del(b.id);
    S.baks.splice(S.baks.indexOf(b), 1);
    renderBaks();
  } else if (btn.dataset.act === "restore") openFile(b.bytes, b.name, b);
  else if (btn.dataset.act === "play") window.open(`../?backup=${b.id}`, "_blank");   // (js/backup-play.js reads it there)
});
// a file dropped or chosen: kept in the list, then the restore offered
async function openFile(bytes, name, rec = null) {
  let parsed;
  try { parsed = await readBackup(bytes, name); }
  catch (e) { say(esc(e.message), "--red"); log(`open ${name}: ${e.message}`); return; }
  log(`opened ${name}: ${parsed.source}, ${parsed.kind}, ${parsed.presets.length} presets${parsed.summary ? `, ${parsed.summary.projects.length} projects, ${parsed.summary.tracks || "?"} tracks, ${parsed.summary.layout}` : ""}`);
  if (!rec) {
    const m = parsed.manifest || {};
    rec = { date: m.date ? Date.parse(m.date) : Date.now(), name, kind: parsed.image ? "full" : "presets", fizz: m.fizz || "", identity: (m.unit && m.unit.identity) || "",
      bytes, size: bytes.length, tracks: parsed.summary ? parsed.summary.tracks : null, layout: parsed.summary ? parsed.summary.layout : null,
      projects: parsed.summary ? parsed.summary.projects.map((p) => p.name) : null, presets: parsed.presets.length, file: true, saved: true };
    await keepBak(rec);
  }
  if (S.link) openRestore(parsed, rec);
  else say("Connect the FM-1 to restore this backup.", "--cyan");
}
const drop = $("drop");
$("file").addEventListener("change", async (ev) => { for (const f of ev.target.files) await openFile(new Uint8Array(await f.arrayBuffer()), f.name); ev.target.value = ""; });
["dragenter", "dragover"].forEach((t) => drop.addEventListener(t, (ev) => { ev.preventDefault(); drop.classList.add("over"); }));
["dragleave", "drop"].forEach((t) => drop.addEventListener(t, () => drop.classList.remove("over")));
drop.addEventListener("drop", async (ev) => { ev.preventDefault(); for (const f of ev.dataTransfer.files) await openFile(new Uint8Array(await f.arrayBuffer()), f.name); });
document.addEventListener("dragover", (ev) => ev.preventDefault());
document.addEventListener("drop", (ev) => ev.preventDefault());

// ---------------------------------------------------------------- restore
const R = { parsed: null, rec: null, mode: "full", safe: false };
function lcd(state) {
  const cv = $("rs-lcd"), g = cv.getContext("2d");
  g.fillStyle = "#000"; g.fillRect(0, 0, 240, 240);
  const c = state === "write" ? "#FF6A2C" : "#FFD23F";
  g.lineWidth = 5; g.strokeStyle = c; g.lineJoin = "round";
  g.beginPath(); g.roundRect(40, 58, 160, 100, 14); g.stroke();
  g.lineWidth = 4; [90, 150].forEach((x) => { g.beginPath(); g.arc(x, 104, 17, 0, 7); g.stroke(); });
  g.beginPath(); g.moveTo(70, 158); g.lineTo(84, 136); g.lineTo(156, 136); g.lineTo(170, 158); g.stroke();
  g.strokeStyle = "#28E1FF"; g.lineWidth = 4;
  g.beginPath(); g.moveTo(120, 18); g.lineTo(120, 44); g.stroke(); g.beginPath(); g.moveTo(108, 32); g.lineTo(120, 44); g.lineTo(132, 32); g.stroke();
  g.fillStyle = state === "write" ? "#FF6A2C" : "#ECECEF"; g.textAlign = "center";
  g.font = "800 28px 'Inter Tight', sans-serif";
  g.fillText(state === "write" ? "DO NOT UNPLUG" : "STOPPED?", 120, 210);
}
function openRestore(parsed, rec) {
  const u = S.unit;
  R.parsed = parsed; R.rec = rec;
  R.mode = parsed.image && u.info && parsed.image.length === u.info.size ? "full" : "presets";
  R.safe = R.mode !== "full";
  const full = R.mode === "full", sum = parsed.summary;
  $("rs-modal").style.setProperty("--c", full ? "var(--yellow)" : "var(--cream)");
  $("rs-title").textContent = full ? "RESTORE" : "ADD PRESETS";
  $("rs-when").textContent = whenText(rec.date);
  const nowMine = u.rows.filter((r) => r.source === "mine").length;
  $("rs-now").textContent = `${u.summary ? u.summary.projects.length : "?"} · ${nowMine}`;
  $("rs-then").textContent = full ? `${sum.projects.length} · ${parsed.presets.length}` : `${u.summary ? u.summary.projects.length : "?"} · ${parsed.presets.length}`;
  const chips = full ? sum.projects.map((p) => `<span class="pchip">${esc(p.name || `PROJECT ${p.slot}`)}</span>`) : [];
  const d = parsed.presets.length - nowMine;
  if (full && d) chips.push(`<span class="tag delta" style="background:var(${d < 0 ? "--orange" : "--green"})">${d > 0 ? "+" : "−"}${Math.abs(d)} PRESETS</span>`);
  if (full && sum.tracks === 8) chips.push(`<span class="tag delta" style="background:var(--orange)">8 TRACKS · PROJECTS START FRESH</span>`);
  if (!full && parsed.image) chips.push(`<span class="tag delta" style="background:var(--orange)">ANOTHER STORE SIZE · PRESETS ONLY</span>`);
  $("rs-chips").innerHTML = chips.join("");
  $("rs-warn").hidden = !full;
  $("rs-type").value = "";
  $("rs-type").classList.remove("ok");
  $("rs-safe").innerHTML = "";
  $("rs-go").textContent = full ? "RESTORE" : `ADD ${parsed.presets.length}`;
  lcd("ask");
  $("rs-key").textContent = "PLAY";
  $("rs").hidden = false;
  rsReady();
  if (full) safetyCopy();
  else $("rs-go").focus();
}
function rsReady() {
  const typed = $("rs-type").value.trim().toUpperCase() === "RESTORE";
  $("rs-type").classList.toggle("ok", typed);
  $("rs-go").disabled = S.busy || !S.link || !R.safe || (R.mode === "full" && !typed);
}
$("rs-type").addEventListener("input", rsReady);
$("rs-type").addEventListener("keydown", (ev) => { if (ev.key === "Enter" && !$("rs-go").disabled) $("rs-go").click(); });
async function safetyCopy() {
  $("rs-safe").innerHTML = "Saving the FM-1 first…";
  $("rs").hidden = true;
  const r = await backup("full", { safety: true });
  $("rs").hidden = false;
  if (!r) { $("rs-safe").innerHTML = "<b style='color:var(--red)'>The FM-1 could not be saved first: nothing will be restored.</b>"; return; }
  const u = S.unit;
  $("rs-now").textContent = `${u.summary.projects.length} · ${r.presets.length}`;
  R.safe = true;
  $("rs-safe").innerHTML = `<b>✓</b> FM-1 saved first · ${esc(S.baks[0] ? S.baks[0].name : "")}`;
  rsReady();
  $("rs-type").focus();
}
function closeRestore() { $("rs").hidden = true; R.parsed = null; }
$("rs-cancel").addEventListener("click", closeRestore);
$("rs-go").addEventListener("click", () => (R.mode === "full" ? restoreFull() : addPresets()));
document.addEventListener("keydown", (ev) => { if (ev.key === "Escape" && !$("rs").hidden && !S.busy) closeRestore(); });

async function refresh() {
  const link = S.link;
  for (let t = 0; t < 10; t++) {                     // (FIZZ starts again on the restored store)
    await new Promise((r) => setTimeout(r, 500));
    const v = await link.identity();
    if (v || t === 9) { S.unit.version = v; break; }
  }
  S.unit.info = await link.storeInfo();
  S.unit.rows = S.unit.version ? await link.list({ factory: true, pool: true }) : [];
  S.unit.status = S.unit.version ? await link.status() : null;
  S.unit.projects = S.unit.status ? await link.projects() : null;
  S.unit.dumps = null; S.unit.fdumps = null; S.unit.one = null; S.sel = null;
  pool.forget();
}
async function restoreFull() {
  const image = R.parsed.image, rec = R.rec;
  closeRestore();
  await restoreImage(image, rec.name);
}
// the store written (RESTORE, and PROJECTS' changes): true when the unit took it
async function restoreImage(image, name, label = "CHECKING") {
  S.busy = true; renderUnit(); renderBaks();
  S.abort = new AbortController();
  progOpen(label, image.length);
  try {
    await S.link.restore(image, {
      signal: S.abort.signal,
      onPhase: (ph) => {
        if (ph === "write") { $("p-label").textContent = "WRITING · DO NOT UNPLUG"; $("p-label").classList.add("warn"); $("p-pct").classList.add("write"); $("p-cancel").hidden = true; }
      },
      onProgress: (f, pass) => progSet(f, Math.floor(((pass === 2 ? f * 2 - 1 : f * 2) * image.length)), pass),
    });
    S.unit.summary = readRegion(image);
    progClose();
    toast(`Written: ${name}`);
    log(`restored ${name}`);
    await refresh();
    return true;
  } catch (e) {
    progClose();
    log(`restore: ${e.code || ""} ${e.fzer || ""} ${e.message}`);
    if (e.code === "cancel") toast("Cancelled. Nothing changed.", "--muted");
    else if (e.fzer === 10) say("The FM-1 is playing. Stop it, then try again.", "--orange");
    else if (e.fzer === 11) say("This backup is from a store of another size. Its presets can still be added.", "--orange");
    else if (e.fzer === 12) say("The store did not reach the FM-1 whole. Nothing was changed.", "--orange");
    else if (e.code === "timeout" && e.written === false) say("The FM-1 did not take the write up. Nothing was changed: try again.", "--orange");
    else if (e.fzer === 13 || e.fzer === 14 || e.code === "lost" || (e.code === "timeout" && e.written !== false))
      say(`The write was cut off. <b>FIZZ has cleared its store.</b> Plug the FM-1 in and restore the newest backup in BACKUP (${esc(FZER[e.fzer] || e.message)}).`, "--red");
    else say(`The write did not finish: ${esc(e.message)}`, "--red");
    if (S.link && e.code !== "lost") await refresh().catch(() => {});
    return false;
  } finally {
    S.busy = false; S.abort = null;
    renderUnit(); renderLibrary(); renderBaks(); projtab.render();
  }
}
// PROJECTS: the store read (no backup kept: the one before a write is), and written with the store as it was kept first
async function readStore(label) {
  if (S.busy || !S.link) return null;
  S.busy = true; renderUnit();
  S.abort = new AbortController();
  progOpen(label, S.unit.info ? S.unit.info.size : 4096);
  try {
    const r = await S.link.backup({ depth: DEPTH, gapMs: GAP, signal: S.abort.signal, onProgress: (f, off) => progSet(f, off) });
    S.unit.summary = readRegion(r.image);
    return r.image;
  } catch (e) {
    log(`read: ${e.code || ""} ${e.message}`);
    if (e.code === "cancel") toast("Cancelled", "--muted");
    else say(`The FM-1's store could not be read: ${esc(e.message)}. Wake the FM-1 (a key), then try again.`, "--orange");
    return null;
  } finally { progClose(); S.busy = false; S.abort = null; renderUnit(); }
}
async function writeStore(image, before) {
  const date = new Date(), sum = readRegion(before);
  const presets = sum.presets.map((p) => ({ fizz: true, name: p.name, blob: p.blob, drive: p.drive, pan: p.pan, sends: p.sends, fizzTags: tagsOf(p.tags),
    favourite: false, source: { from: `MY PRESETS ${p.slot}` }, notes: [], origin: [], mark: "", tags: [] }));
  const file = makeFizzbak({ kind: "full", image: before, summary: sum, presets, fizz: S.unit.version || "", unit: { identity: S.unit.id, port: S.unit.port }, web: webFor(presets), date });
  const name = backupFileName(date, "full").replace("FIZZ backup", "FIZZ before projects");
  await keepBak({ date: +date, name, kind: "full", safety: true, saved: false, fizz: S.unit.version || "", identity: S.unit.id, bytes: file, size: file.length,
    tracks: sum.tracks, layout: sum.layout, projects: sum.projects.map((p) => p.name), presets: presets.length });
  return restoreImage(image, "the projects", "CHECKING");
}

async function addPresets() {
  const list = R.parsed.presets, rec = R.rec;
  closeRestore();
  S.busy = true; renderUnit(); renderBaks();
  progOpen("PRESETS", 4096);
  $("p-cancel").hidden = true;
  let added = 0, same = 0, full = false;
  try {
    const u = S.unit, link = S.link;
    const mine = u.rows.filter((r) => r.source === "mine");
    const have = (await link.dumpAll("mine", mine.length)).map(soundFromStoreFrame);
    for (let i = 0; i < list.length; i++) {
      progSet(i / list.length, -1);
      if (have.some((h) => sameSound(h, list[i]))) { same++; continue; }
      const r = await link.store(storeFrame(list[i], { batch: added > 0 }));
      if (r.kind === "store" && r.status === 0) added++;
      else if (r.status === 1) { full = true; break; }
      else log(`add ${list[i].name}: ${r.statusName || r.status}`);
    }
    progClose();
    log(`added ${added} presets from ${rec.name} (${same} already there${full ? ", MY PRESETS full" : ""})`);
    toast(`+${added} presets${same ? ` · ${same} already there` : ""}${full ? " · MY PRESETS FULL" : ""}`, full ? "--orange" : "--green");
    u.rows = await link.list({ factory: true, pool: true });
    u.status = await link.status();
    u.dumps = null; u.summary = null;
  } catch (e) {
    progClose();
    say(`Adding the presets stopped: ${esc(e.message)} (${added} added)`, "--red");
  } finally {
    S.busy = false;
    renderUnit(); renderLibrary(); renderBaks();
  }
}

// ---------------------------------------------------------------- edits on the unit (Phase 1)
// one edit at a time (the link's queue serialises them anyway): the page's state refreshed after it; a refusal said
async function edit(what, fn) {
  if (!S.link || S.busy) return null;
  S.editing++;
  renderLibrary();
  try {
    const r = await fn(S.link);
    if (r && typeof r.status === "number" && r.status !== 0 && r.status !== 10) say(refusal(r, what), r.status === 8 ? "--orange" : "--red");
    return r;
  } catch (e) {
    log(`${what}: ${e.code || ""} ${e.message}`);
    say(`${esc(what)}: ${esc(e.message)}`, "--red");
    return null;
  } finally {
    S.editing--;
  }
}
// the unit's lists again: LIST (MY PRESETS, the factory's, the pool), STATUS, PROJECTS. until(rows): what an edit just
// made shows in them (the page showed it already): read again (up to 4 times, 0.3 s apart) until it does; a LIST that
// failed keeps the rows the page has
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
async function refreshRows({ projects = false, until = null } = {}) {
  const u = S.unit, link = S.link;
  if (!u || !link || !u.version) return;
  try {
    for (let k = 0; k < 4; k++) {
      const rows = await link.list({ factory: true, pool: true }).catch((e) => { log(`refresh: LIST ${e.code || ""} ${e.message}`); return null; });
      if (rows && (!until || until(rows) || k === 3)) {
        if (until && !until(rows)) log("refresh: the FM-1's lists still differ from the edit: the FM-1's kept");
        u.rows = rows;
        break;
      }
      await sleep(300);
    }
    u.status = await link.status();
    if (projects || !u.projects) u.projects = await link.projects();
  } catch (e) { log(`refresh: ${e.code || ""} ${e.message}`); }
  S.users.clear();
  u.one = null;                                        // (a preset read by itself: again, as it is now)
  renderUnit(); renderLibrary(); pool.render(); projtab.render();
}
// a pool place changed by an edit the unit acknowledged: shown at once (row: the preset now there, null: empty)
function poolSet(slot, row) {
  const u = S.unit;
  if (!u) return;
  u.rows = u.rows.filter((x) => !(x.source === "pool" && x.slot === slot));
  if (row) u.rows.push({ source: "pool", slot, id: row.id, name: row.name, tags: row.tags, imported: false, inPool: true });
  pool.forget();
  renderLibrary(); pool.render();
}
// REFRESH (the header's button, and the tab coming back into view): everything read again, so presets saved and pool
// changes made on the unit show here. quick (the focus): only when STATUS says the unit changed (its records committed,
// MY PRESETS' and the pool's counts, the project): a STATUS when nothing did
const unitSig = (st) => (st ? [st.written, st.mine, st.pool, st.project, st.projects, st.dirty, st.noStore].join(".") : "");
async function reread({ quick = false } = {}) {
  const u = S.unit, link = S.link;
  if (!u || !link || S.busy || S.refreshing || attaching) return false;
  if (quick && (S.editing || !u.version)) return false;
  S.refreshing = true; renderUnit();
  $("refresh").classList.add("spin");
  try {
    if (quick) {
      const st = await link.status().catch(() => null);
      if (!st || unitSig(st) === unitSig(u.status)) { if (st) u.status = st; return false; }
      log("the FM-1 changed: read again");
    }
    if (!u.version) u.version = await link.identity();
    u.info = await link.storeInfo();
    u.dumps = null;                                    // (MY PRESETS' DUMPs again: names, tags, sounds saved on the unit)
    pool.forget();
    await refreshRows({ projects: true });
    if (S.sel && !u.rows.some((r) => r.source === S.sel.source && r.slot === S.sel.slot)) S.sel = null;
    if ($("tab-backup").hidden) await loadDumps();
    await live.probe();
    return true;
  } catch (e) { log(`refresh: ${e.code || ""} ${e.message}`); return false; }
  finally {
    S.refreshing = false;
    $("refresh").classList.remove("spin");
    renderUnit(); renderLibrary(); pool.render(); importer.render();
  }
}
$("refresh").addEventListener("click", async () => { if (await reread()) toast("Read the FM-1 again", "--green"); });
let focusT = 0;
function onFocus() {
  if (document.visibilityState !== "visible" || Date.now() - focusT < 2000) return;
  focusT = Date.now();
  reread({ quick: true });
}
document.addEventListener("visibilitychange", onFocus);
window.addEventListener("focus", onFocus);
// a MY preset's DUMP again (its name, tags or place changed)
async function redump(slot) {
  const u = S.unit;
  if (!u.dumps) return;
  const f = await S.link.dump("mine", slot).catch(() => null);
  if (f) u.dumps.set(slot, f); else u.dumps.delete(slot);
}
const projectName = (p) => {
  const e = S.unit && S.unit.projects && S.unit.projects.list[p - 1];
  return e && e.name ? e.name : `PROJECT ${p}`;
};

// ---------------------------------------------------------------- the library
const frameOf = (r) => (r.source === "mine" ? S.unit.dumps && S.unit.dumps.get(r.slot) : S.unit.fdumps && S.unit.fdumps[r.slot - 1]) ||
  (S.unit.one && S.unit.one.get(`${r.source}:${r.slot}:${r.id}`)) || null;
// a selected preset's DUMP read by itself when the whole library's is not here yet (EXPORT, EDIT need it)
function fetchOne(r) {
  const u = S.unit, k = `${r.source}:${r.slot}:${r.id}`;
  if (!S.link || !u.version || S.busy || (u.one && u.one.has(k)) || (S.fetching && S.fetching.has(k))) return;
  (S.fetching || (S.fetching = new Set())).add(k);
  S.link.dump(r.source, r.slot).then((f) => {
    (u.one || (u.one = new Map())).set(k, f || null);    // (none: not asked again until the lists are read again)
    if (f && PV.want === r) { PV.want = null; previewSound(soundFromFrame(f)); }
    if (S.sel && S.sel.source === r.source && S.sel.slot === r.slot) renderSel();
  }).catch(() => {}).finally(() => S.fetching.delete(k));
}
const places = () => poolPlaces(S.unit ? S.unit.rows : []);
function renderLibrary() {
  const u = S.unit;
  const rows = u ? u.rows : [];
  const mine = rows.filter((r) => r.source === "mine");
  $("l-count").textContent = u ? `${mine.length} / 256` : "";
  $("l-filter").innerHTML = filterHtml(S.filter);
  const html = u ? listHtml(rows, S.filter, { frameOf, places: places(), on: (r) => S.sel && S.sel.source === r.source && S.sel.slot === r.slot,
    drag: (r) => r.source === "mine" && !!u.status }) : "";
  $("l-list").innerHTML = html || (u ? "" : `<div class="empty">${emptyArt()}</div>`);
  renderSel();
}
const DOTS = (n) => `<span class="dots">${"●".repeat(Math.min(6, n))}${n > 6 ? "+" : ""}</span>`;
function usedText(x) {
  if (!x || !x.projects || !x.projects.length) return "";
  const steps = x.projects.reduce((a, p) => a + (p.steps || 0), 0);
  return `Used in ${x.projects.length} project${x.projects.length === 1 ? "" : "s"}${steps ? ` · ${steps} step${steps === 1 ? "" : "s"}` : ""}: those steps go back to their track's sound.`;
}
function renderSel() {
  const el = $("l-sel"), u = S.unit;
  const r = S.sel && u && u.rows.find((x) => x.source === S.sel.source && x.slot === S.sel.slot);
  if (!r) {
    el.innerHTML = u ? `<div class="empty">${emptyArt()}</div>`
      : `<div class="empty"><div style="display:grid;gap:22px;justify-items:center">${emptyArt()}<button class="btn huge" id="l-connect">CONNECT</button></div></div>`;
    const b = $("l-connect");
    if (b) b.addEventListener("click", connect);
    return;
  }
  const f = frameOf(r), d = f && parseDump(f), c = colOf(r.tags);
  if (!f) fetchOne(r);
  const br = live && live.browsing(), onUnit = br && br.id === r.id;
  const snd = f && soundFromStoreFrame(f), key = snd && soundKey(snd), web = (key && S.web.tags[key]) || [];
  const can = !!(S.link && !S.busy && u.status), mineSel = r.source === "mine";
  const users = mineSel ? S.users.get(`mine:${r.id}`) : null;
  const pl = places().get(r.id) || [];
  const mi = !!d && isMidiBlob(d.blob);
  el.innerHTML = `<div class="selhead">${mi ? MIDI_I.replace('class="din"', 'class="din algo"') : algo(d ? d.blob[2] : r.slot, c)}<div style="min-width:0;flex:1"><div class="h" style="margin:0">${midiChip(mi)}${mi ? " " : ""}${mineSel ? "MY PRESETS" : "FACTORY"} ${r.slot}</div>
      ${mineSel && can ? `<input class="bigname" id="l-name" maxlength="12" spellcheck="false" autocomplete="off" value="${esc(r.name)}" aria-label="Name">` : `<div class="big">${esc(r.name)}</div>`}</div>
      ${poolBadge(pl, "pbadge big")}</div>
    <div class="row selgo">
      <button class="btn" id="l-edit" style="--c:var(--pink);color:#fff" ${f ? "" : "disabled"}><svg viewBox="0 0 20 20" aria-hidden="true"><path d="M3 15l9-9 3 3-9 9H3zM13 5l2-2 3 3-2 2"/></svg>EDIT</button>
      <button class="btn onunit ${onUnit ? "on" : ""}" id="l-unit" ${S.link && !S.busy && u.status && r.id ? "" : "disabled"} aria-pressed="${!!onUnit}" aria-label="${onUnit ? `On the FM-1, track ${br.t + 1}: OCT minus cancels, OCT plus loads` : "Load on the FM-1"}">${onUnit
        ? `<span class="okeys" aria-hidden="true"><i class="no">OCT−</i><i class="yes">OCT+</i></span>TRACK ${br.t + 1}`
        : '<svg viewBox="0 0 22 20" aria-hidden="true"><rect x="1.5" y="3" width="19" height="13" rx="3"/><path d="M6 19h10M9 7.5l4 2.5-4 2.5z"/></svg>LOAD ON FM-1'}</button>
      <button class="btn" id="l-export" style="--c:var(--cream)" ${f ? "" : "disabled"}><svg viewBox="0 0 20 20" aria-hidden="true"><path d="M10 3v10M5 9l5 5 5-5M3 17h14"/></svg>EXPORT</button></div>
    <div class="row selpool"><button class="btn pooltog ${pl.length ? "out" : ""}" id="l-pool" ${can && r.id ? "" : "disabled"} aria-pressed="${!!pl.length}">${pl.length ? "− POOL" : "+ POOL"}</button></div>
    <div><div class="h">TAGS</div><div class="row tags" style="flex-wrap:wrap;gap:8px">
      ${FIZZ_TAGS.map((t, k) => `<button class="tag t-${t} ${(r.tags >> k) & 1 ? "" : "off"}" data-dtag="${k}" aria-pressed="${!!((r.tags >> k) & 1)}" ${can ? "" : "disabled"}>${t}</button>`).join("")}
      ${web.map((t) => `<button class="tag web" data-web="${esc(t)}" aria-label="${esc(t)}, remove">${esc(t)}</button>`).join("")}
      ${key ? '<button class="tag web" id="l-addtag" aria-label="Add a tag (kept on this computer and in backups)">+</button>' : ""}</div></div>
    ${mineSel ? `<div><div class="h">USED BY</div><div class="row" style="gap:10px;flex-wrap:wrap" id="l-users">${users === undefined ? '<span class="pchip ghost">…</span>'
      : users && users.projects ? (users.projects.length ? users.projects.map((p) => `<span class="uchip">${esc(projectName(p.project))}${p.steps ? DOTS(p.steps) : ""}</span>`).join("") : '<span class="pchip ghost">—</span>') : ""}</div></div>` : ""}
    ${d ? `<div class="row" style="gap:22px;flex-wrap:wrap"><div class="kv"><b>${d.pan > 0 ? "R" + d.pan : d.pan < 0 ? "L" + -d.pan : "C"}</b>pan</div><div class="kv"><b>${d.chorus}</b>chorus</div><div class="kv"><b>${d.delay}</b>delay</div><div class="kv"><b>${d.reverb}</b>reverb</div><div class="kv"><b>${d.drive}</b>drive</div></div>` : ""}
    <div class="row selacts">${r.imported ? '<span class="tag web" style="height:34px;padding:0 12px">↓ IMPORTED</span>' : ""}
      ${mineSel ? `<button class="btn ghost redline" id="l-del" style="margin-left:auto" ${can ? "" : "disabled"}>DELETE</button>` : ""}</div>`;
  if (mineSel && users === undefined && can) {          // (who uses it: a job on the unit, a moment)
    S.users.set(`mine:${r.id}`, null);
    S.link.users("mine", r.slot).then((x) => { S.users.set(`mine:${r.id}`, x && x.projects ? x : null); if (S.sel && S.sel.slot === r.slot) renderSel(); }).catch(() => {});
  }
  el.querySelectorAll("[data-web]").forEach((b) => b.addEventListener("click", () => {
    S.web.tags[key] = web.filter((t) => t !== b.dataset.web);
    if (!S.web.tags[key].length) delete S.web.tags[key];
    saveWeb(); renderSel();
  }));
  el.querySelectorAll("[data-dtag]").forEach((b) => b.addEventListener("click", async () => {
    const mask = r.tags ^ (1 << +b.dataset.dtag);
    const x = await edit("TAGS", (l) => l.tags(r.source, r.slot, mask));
    if (x && x.status === 0) {
      r.tags = mask;
      if (r.source === "mine") await redump(r.slot);
      else if (u.fdumps) u.fdumps[r.slot - 1] = await S.link.dump("factory", r.slot).catch(() => null) || u.fdumps[r.slot - 1];
    }
    renderLibrary(); pool.render();
  }));
  const nm = $("l-name");
  if (nm) {
    const done = async (keep) => {
      const v = nm.value.toUpperCase().replace(/[^\x20-\x7E]/g, "").trim().slice(0, 12);
      if (!keep || !v || v === r.name) { nm.value = r.name; return; }
      const x = await edit("RENAME", (l) => l.rename("mine", r.slot, v));
      if (x && x.status === 0) {
        const wk = key && S.web.tags[key];
        r.name = v;
        await redump(r.slot);
        if (wk) { const f2 = frameOf(r); if (f2) { S.web.tags[soundKey(soundFromStoreFrame(f2))] = wk; delete S.web.tags[key]; saveWeb(); } }
        toast(`Renamed ${v}`);
        await refreshRows();
      } else renderSel();
    };
    nm.addEventListener("input", () => { const p = nm.selectionStart; nm.value = nm.value.toUpperCase(); nm.setSelectionRange(p, p); });
    nm.addEventListener("keydown", (ev) => { if (ev.key === "Enter") nm.blur(); else if (ev.key === "Escape") { nm.value = r.name; nm.blur(); } });
    nm.addEventListener("change", () => done(true));
  }
  const add = $("l-addtag");
  if (add) add.addEventListener("click", () => {
    const inp = document.createElement("input");
    inp.className = "tagin"; inp.maxLength = 16; inp.setAttribute("aria-label", "New tag");
    add.replaceWith(inp); inp.focus();
    const done = (keep) => {
      const t = inp.value.trim().toUpperCase().replace(/\s+/g, " ");
      if (keep && t && !FIZZ_TAGS.includes(t) && !web.includes(t)) { S.web.tags[key] = [...web, t]; saveWeb(); }
      renderSel();
    };
    inp.addEventListener("keydown", (ev) => { if (ev.key === "Enter") done(true); else if (ev.key === "Escape") done(false); });
    inp.addEventListener("blur", () => done(true));
  });
  const ex = $("l-export");
  if (ex && f) ex.addEventListener("click", () => {
    const s = { ...snd, source: { fizz: u.version || "", from: `${r.source === "mine" ? "MY PRESETS" : "FACTORY"} ${r.slot}` }, ...(web.length ? { web: { tags: web } } : {}) };
    exporter.open([s], { title: r.name, tags: tagsOf(r.tags) });
  });
  const ed = $("l-edit");
  if (ed && f) ed.addEventListener("click", () => openInEditor(r));
  const lu = $("l-unit");                                // (the unit's own browser on it: OCT- cancels, OCT+ loads)
  if (lu) lu.addEventListener("click", async () => { if (live) { await live.loadOnUnit(r); renderSel(); } });
  // + POOL: the first free place references it; − POOL: every place that does is cleared (a used one asks first)
  const tog = $("l-pool");
  if (tog) tog.addEventListener("click", async () => {
    if (!pl.length) {
      const x = await edit("+ POOL", (l) => l.poolRef(0, r.id));
      if (x && x.status === 0) {
        poolSet(x.a, r);
        toast(`${r.name} → POOL ${x.a}`); pool.fizz(x.a);
        await refreshRows({ until: (rows) => rows.some((q) => q.source === "pool" && q.slot === x.a && q.id === r.id) });
      }
      return;
    }
    let n = 0;
    for (const k of pl) {
      let x = await edit("− POOL", (l) => l.remove("pool", k));
      if (x && x.status === 10) {
        const ok = await confirmBox({ title: `CLEAR POOL ${k}`, what: r.name, go: "CLEAR", color: "--red", undo: "kept",
          sub: `${x.b || 0} step${x.b === 1 ? "" : "s"} use it: they go back to their track's sound.` });
        if (!ok) break;
        x = await edit("− POOL", (l) => l.remove("pool", k, { confirm: true }));
      }
      if (x && x.status === 0) { n++; poolSet(k, null); } else break;
    }
    pool.forget();
    if (n) toast(`${r.name} out of the pool`, "--muted");
    await refreshRows({ until: (rows) => !rows.some((q) => q.source === "pool" && q.id === r.id && pl.includes(q.slot)) });
  });
  const del = $("l-del");
  if (del) del.addEventListener("click", async () => {
    const ok = await confirmBox({ title: `DELETE · MY PRESETS ${r.slot}`, what: r.name, go: "DELETE", undo: "kept",
      sub: usedText(users) || (pl.length ? `In the pool at ${pl.join(", ")}: it comes out of it.` : "") });
    if (!ok) return;
    const x = await edit("DELETE", (l) => l.remove("mine", r.slot, { confirm: true }));
    if (x && x.status === 0) {
      toast(`Deleted ${r.name}`, "--red");
      if (u.dumps) u.dumps.delete(r.slot);
      S.sel = null;
      pool.forget();
      await refreshRows();
    } else renderSel();
  });
}
$("l-filter").addEventListener("click", (ev) => { if (filterClick(S.filter, ev)) renderLibrary(); });
const list = $("l-list");
list.addEventListener("click", (ev) => {
  const b = ev.target.closest(".cas");
  if (!b) return;
  S.sel = { source: b.dataset.src, slot: +b.dataset.slot };
  renderLibrary();
  previewRow(S.unit.rows.find((x) => x.source === S.sel.source && x.slot === S.sel.slot));   // (TRACK on: on the track, KEEP / CANCEL)
  if (matchMedia("(max-width: 900px)").matches) $("l-sel").scrollIntoView({ behavior: "smooth", block: "start" });   // (a phone: the panel is below the list)
});
// reorder MY PRESETS: a cassette dropped on another swaps their places (the ids go with them: nothing else changes);
// ALT + up / down moves the selected one a place (into an empty place too)
async function swapMine(a, b) {
  if (a === b || b < 1 || b > 256) return;
  const x = await edit("MOVE", (l) => l.swap("mine", a, b));
  if (!x || x.status !== 0) return;
  await redump(a);
  await redump(b);
  if (S.sel && S.sel.source === "mine") S.sel.slot = S.sel.slot === a ? b : S.sel.slot === b ? a : S.sel.slot;
  await refreshRows();
  for (const s of [a, b]) { const el = list.querySelector(`.cas[data-src="mine"][data-slot="${s}"]`); if (el) el.classList.add("moved"); }   // (slid in)
}
const mineCas = (ev) => { const b = ev.target.closest(".cas"); return b && b.dataset.src === "mine" ? b : null; };
let dragSlot = 0;
list.addEventListener("dragstart", (ev) => {
  const b = mineCas(ev);
  if (!b) return;
  dragSlot = +b.dataset.slot;
  ev.dataTransfer.effectAllowed = "move";
  ev.dataTransfer.setData("application/x-fizz-preset", JSON.stringify({ slot: dragSlot, id: +b.dataset.id }));
  b.classList.add("lift");
});
list.addEventListener("dragend", (ev) => { const b = ev.target.closest(".cas"); if (b) b.classList.remove("lift"); dragSlot = 0; });
list.addEventListener("dragover", (ev) => {
  const b = mineCas(ev);
  if (!dragSlot || !b) return;
  ev.preventDefault();
  list.querySelectorAll(".cas.target").forEach((x) => x !== b && x.classList.remove("target"));
  b.classList.toggle("target", +b.dataset.slot !== dragSlot);
});
list.addEventListener("dragleave", (ev) => { const b = ev.target.closest(".cas"); if (b) b.classList.remove("target"); });
list.addEventListener("drop", (ev) => {
  const b = mineCas(ev);
  if (!dragSlot || !b) return;
  ev.preventDefault();
  b.classList.remove("target");
  swapMine(dragSlot, +b.dataset.slot);
});
list.addEventListener("keydown", (ev) => {
  const b = mineCas(ev);
  if (!b || !ev.altKey || (ev.key !== "ArrowUp" && ev.key !== "ArrowDown") || !S.unit || !S.unit.status) return;
  ev.preventDefault();
  const a = +b.dataset.slot;
  S.sel = { source: "mine", slot: a };
  swapMine(a, a + (ev.key === "ArrowUp" ? -1 : 1)).then(() => { const n = list.querySelector(`.cas[data-src="mine"][data-slot="${S.sel.slot}"]`); if (n) n.focus(); });
});
let dumping = false;
async function loadDumps() {
  const u = S.unit;
  if (!u || !S.link || !u.version || dumping || S.busy) return;
  dumping = true;
  try {
    if (!u.dumps) {
      const mine = u.rows.filter((r) => r.source === "mine");
      const t0 = performance.now();
      const frames = await S.link.dumpAll("mine", mine.length);
      log(`DUMP: ${frames.length} of ${mine.length} presets in ${(performance.now() - t0).toFixed(0)} ms`);
      u.dumps = new Map(frames.map((f) => [parseDump(f).slot, f]));
      renderLibrary(); renderUnit(); pool.render();
    }
    if (!u.fdumps) {
      const fac = u.rows.filter((r) => r.source === "factory");
      u.fdumps = await S.link.dumpAll("factory", fac.length);
      renderLibrary();
    }
  } catch (e) { log(`library: ${e.message}`); } finally { dumping = false; }
}
$("l-export-all").addEventListener("click", async () => {
  const u = S.unit;
  if (!u || !S.link) return;
  if (!u.dumps) { $("l-export-all").classList.add("spin"); await loadDumps(); $("l-export-all").classList.remove("spin"); }
  if (!u.dumps) { say("The FM-1's presets could not be read: try again.", "--orange"); return; }
  const sounds = [...u.dumps.entries()].sort((a, b) => a[0] - b[0]).map(([slot, f]) => ({ ...soundFromStoreFrame(f), source: { fizz: u.version || "", from: `MY PRESETS ${slot}` } }));
  exporter.open(sounds, { title: "MY PRESETS", pick: "fizz" });
});

// ---------------------------------------------------------------- the pool, the import (their own files)
let live = null;                                        // (live.js, mounted with the editor below)
const ctx = { S, $, log, say, toast, edit, refreshRows, loadDumps, frameOf, projectName, saveWeb, poolSet, render: () => { renderUnit(); renderLibrary(); },
  trackOn: () => !!(live && live.on()), previewRow: (r) => previewRow(r), previewId: (id) => previewId(id),
  previewLib: (snd, name) => previewLib(snd, name) };
const pool = mountPool(ctx);
const importer = mountImport(ctx);
const projtab = mountProjects({ ...ctx, readStore: (l) => readStore(l), writeStore: (i, b) => writeStore(i, b), download,
  kv: { get: (k) => DB.get(k), put: (k, v) => DB.put(k, v) }, fizz: () => (S.unit && S.unit.version) || "" });

// ---------------------------------------------------------------- export, the sound editor (Phase 2-A)
const exporter = mountExport({ download, log });
const bfizz = new BrowserFizz({ log });
// the browser's FIZZ as the editor's target: the sound loaded onto its track 1 (STORE6 with LOAD into its MY PRESETS
// 256, a scratch place of this page's own FIZZ), again after each change (the latest one, as fast as it answers)
const browserTarget = (() => {
  let want = null, going = false;
  async function flush() {
    if (going) return;
    going = true;
    while (want) {
      const s = want;
      want = null;
      await bfizz.ask(storeFrameOf(s, { slot: 256, track: 0 }), (x) => x.kind === "store", 2000);
    }
    going = false;
  }
  const push = (s) => { if (!bfizz.ready) return; want = copySound(s); flush(); };
  return {
    live: false,
    label: () => (bfizz.ready ? "THIS BROWSER · TRACK 1" : "▶ PLAY IN THIS BROWSER"),
    ready: () => bfizz.ready,
    start: () => bfizz.start(),
    audition: (s) => push(s),
    param: (id, v, s) => push(s),
    sound: (s) => push(s),                              // (FIZZ MIDI (e): a MIDI sound's slot changed)
    note: (n, vel) => bfizz.note(n, vel),
  };
})();
const editor = mountEditor({
  $, log,
  canSave: () => { const on = !!(S.link && !S.busy && S.unit && S.unit.status); const p = editor ? editor.place() : {}; return { save: on && p.source === "mine" && p.slot > 0, saveNew: on }; },
  save: (E, asNew) => saveSound(E, asNew),
  exportSound: (s) => exporter.open([libSound(s, { fizz: (S.unit && S.unit.version) || "", from: editor.place().label || "" })], { title: s.name, tags: tagNames(s.tags) }),
  strip: () => (live ? live.strip() : ""),
  stripClick: (b) => (live ? live.stripClick(b) : null),
  onLocked: () => { if (live) live.nudge(); },
  keys: { active: () => KEY_TABS.includes(curTab()), target: () => keysTarget(), start: () => keysStart() },
});
editor.setTarget(browserTarget);
live = mountLive({ S, $, log, say, toast, editor, browserTarget, changed: () => { importer.render(); if (!$("tab-library").hidden) renderSel(); } });
async function openInEditor(r) {
  const f = frameOf(r), snd = f && soundFromFrame(f);
  if (!snd) return;
  if (live.on()) {                                      // (P2-B: onto the track (KEEP / CANCEL), the editor takes the track)
    if (await live.pick(r, f)) { tab("edit"); return; }
  }
  live.detach();
  snd.extras = { id: r.id, sound: 0, edited: false };
  editor.open(snd, { label: `${r.source === "mine" ? "MY PRESETS" : "FACTORY"} ${r.slot}`, source: r.source, slot: r.slot, id: r.id });
  tab("edit");
}
// SAVE: over the MY preset it came from (asked first), SAVE AS NEW: the first free place
async function saveSound(E, asNew) {
  const p = E.place, slot = asNew ? 0 : p.slot;
  if (!asNew) {
    const ok = await confirmBox({ title: `SAVE · MY PRESETS ${slot}`, what: E.s.name, go: "SAVE", color: "--pink", undo: "kept",
      sub: "The tracks that loaded it keep their own copies." });
    if (!ok) return null;
  }
  // (P2-B: the owner's preset, OWN; TRACK on: the track then plays it, LOAD. Over a used place FIZZ runs a job: the
  // other projects' tracks that loaded it keep the sound they had; refused while the FM-1 plays)
  const x = await edit("SAVE", (l) => l.store(storeFrameOf(E.s, { slot, own: true, ...live.saveOpts() })));
  if (!x || x.kind !== "store" || x.status !== 0) return null;
  toast(`${E.s.name} → MY PRESETS ${x.slot}`, "--pink");
  if (S.unit.dumps) await redump(x.slot);
  await refreshRows();
  const row = S.unit.rows.find((q) => q.source === "mine" && q.slot === x.slot);
  if (live.on()) { await live.afterSave(); return { place: editor.place() }; }
  return { place: { label: `MY PRESETS ${x.slot}`, source: "mine", slot: x.slot, id: row ? row.id : 0 } };
}
// a sound file dropped on the editor: opened in it (.fizzsnd, or a Digitone sound converted as IMPORT does)
const edTab = $("tab-edit");
edTab.addEventListener("dragover", (ev) => ev.preventDefault());
edTab.addEventListener("drop", async (ev) => {
  ev.preventDefault();
  const f = ev.dataTransfer.files && ev.dataTransfer.files[0];
  if (!f) return;
  const bytes = new Uint8Array(await f.arrayBuffer());
  try {
    let snd;
    if (/\.fizzsnd$/i.test(f.name)) snd = parseFizzsnd(bytes, f.name);
    else { const got = await readFiles([{ name: f.name, bytes }]); if (!got[0] || !got[0].ok) throw new Error((got[0] && got[0].error) || "not a sound"); snd = got[0].sound; }
    live.detach();
    editor.open(soundFromLib(snd), { label: f.name.replace(/\.[^.]+$/, "").toUpperCase().slice(0, 24), source: "file" });
  } catch (e) { say(`${esc(f.name)}: ${esc(e.message)}`, "--orange"); }
});

// ---------------------------------------------------------------- tabs
const TABS = ["backup", "library", "edit", "pool", "projects", "import"];
function tab(name) {
  for (const t of TABS) {
    $(`t-${t}`).setAttribute("aria-selected", String(t === name));
    $(`tab-${t}`).hidden = t !== name;
  }
  if (name !== "backup") loadDumps();
  if (name === "pool") pool.show();
  if (name === "import") importer.render();
  if (name === "edit") { editor.show(); live.show(); if (bfizz.ready && editor.sound() && !live.on()) browserTarget.audition(editor.sound()); } else live.hide();
  if (name === "projects") projtab.show();
  if (location.hash !== `#${name}`) history.replaceState(null, "", `${location.search}#${name}`);
}
for (const t of TABS) $(`t-${t}`).addEventListener("click", () => tab(t));
window.addEventListener("beforeunload", (ev) => { if (S.busy) { ev.preventDefault(); ev.returnValue = ""; } });

// ---------------------------------------------------------------- the computer keys, the sound picked (the owner, 2026-10-27)
// On LIBRARY, POOL and IMPORT the keys (A S D F .., W E R ..; Z / X the octave) play the sound picked: on the unit's
// track with TRACK on (a LIBRARY or POOL preset loaded there, KEEP / CANCEL), else in this browser's FIZZ (its track 1)
const KEY_TABS = ["library", "pool", "import"];
const PV = { sound: null, want: null };
const curTab = () => TABS.find((t) => !$(`tab-${t}`).hidden);
const keysTarget = () => (live.on() ? live.target : browserTarget);
async function keysStart() {
  if (live.on()) return live.target.start();
  const ok = await bfizz.start();
  if (ok && PV.sound) browserTarget.audition(PV.sound);
  return ok;
}
function previewSound(snd) {
  PV.sound = snd;
  if (snd && !live.on() && bfizz.ready) browserTarget.audition(snd);
}
function previewRow(r) {
  if (!r || !S.unit) return;
  if (live.on()) { const f = frameOf(r); if (f) live.pick(r, f); return; }
  const f = frameOf(r);
  if (f) previewSound(soundFromFrame(f));
  else { PV.want = r; fetchOne(r); }
}
function previewId(id) {
  const r = S.unit && S.unit.rows.find((x) => (x.source === "mine" || x.source === "factory") && x.id === id);
  previewRow(r);
}
function previewLib(snd, name) {
  if (!snd) return;
  if (live.on()) { live.pickSound(snd, name); return; }
  previewSound(soundFromLib(snd));
}

// ---------------------------------------------------------------- FIZZ in this browser as the FM-1 (the owner, 2026-10-27)
// BROWSER in the header: the browser's own FIZZ (browserunit.js) takes the FM-1's place, heard on the computer: every tab
// works on it. It starts from the store it had last time in this browser, else the newest full backup kept here, else
// blank; TRACK is on (nothing to protect). FM-1 in the header goes back to the unit.
let BU = null;
function setB(state, sub = "") {
  const b = $("bunit");
  b.dataset.state = state;
  $("bunit-sub").textContent = sub;
}
async function newestImage() {
  const b = S.baks.find((x) => x.kind === "full" && x.layout !== "host");
  if (!b) return null;
  try { return (await readBackup(b.bytes, b.name)).image; } catch (_) { return null; }
}
function dropLink() {
  if (S.access) S.access.onstatechange = null;
  if (S.link) { try { S.link.close(); } catch (_) { /* gone */ } S.link = null; live.lost(); }
  if (S.mode === "browser" && BU) { BU.unplug(); if (BU.player.ctx) BU.player.ctx.suspend().catch(() => {}); }
  S.access = null;
}
async function useBrowser() {
  if (S.busy || attaching) return;
  dropLink();
  S.mode = "browser";
  setUnit("idle", "FM-1"); setB("look", "");
  try {
    if (!BU) { const { BrowserUnit } = await import("./browserunit.js"); BU = new BrowserUnit({ log, onFlash: (b) => DB.put("browserFlash", b) }); }
    if (!(await BU.start())) throw new Error("the browser held the sound back: click BROWSER again");
    if (BU.player.ctx) await BU.player.ctx.resume();
    if (!BU.engine) await BU.boot((await DB.get("browserFlash")) || (await newestImage()));
    else await BU.replug();
  } catch (e) { S.mode = "fm1"; setB("off"); setUnit("off", "CONNECT"); say(esc(e.message || String(e)), "--orange"); return; }
  S.access = BU.access;
  await attach();
  if (S.link && (await live.probe()) && !live.on()) await live.setOn(true);
}
function leaveBrowser() {
  if (S.busy) return;
  dropLink();
  S.mode = "fm1";
  setB("off");
  setUnit("off", "CONNECT");
  S.unit = null;
  renderUnit(); renderLibrary(); renderBaks(); pool.render(); importer.render(); projtab.render();
}

// ---------------------------------------------------------------- the simulated FM-1 (?sim)
async function simAccess() {
  const [{ SimFizz }, { createEngine }, { storeSound }] = await Promise.all([import("./simfizz.js"), import("../engine/engine.js"), import("../js/fizz_sysex.js")]);
  const module = await WebAssembly.compileStreaming(fetch("../fizz.wasm"));
  const sim = new SimFizz({ createEngine, module, frameMs: Math.max(0, +(Q.get("frame") ?? 1)), playing: Q.has("playing") });
  await sim.boot();
  const demo = [["SUB DROP", 0], ["GLASS BELL", 2], ["NIGHT PAD", 3], ["WIRE LEAD", 4], ["TIN KICK", 5], ["DUSTY EP", 1], ["BUBBLE FX", 6], ["ROUND BASS", 0],
    ["SODA PLUCK", 2], ["SLOW CLOUD", 3], ["ZAP SNARE", 5], ["TOY ORGAN", 1], ["HOLLOW LD", 4], ["FIZZ HAT", 5]];
  demo.forEach(([name, t], k) => {
    const blob = new Uint8Array(96); blob[0] = 0x51; blob[1] = 5; blob[2] = k % 8; blob[3] = (2 * k) % 16; blob[50] = (10 * k) % 100;
    sim.engine.midiIn(storeSound({ name, blob, tags: 1 << t, pan: (k % 5) - 2, reverb: 10 * (k % 4) }));
    sim.run(40);
  });
  sim.run(2500);
  window.__sim = sim;
  log("a simulated FM-1 (?sim): the browser FIZZ behind a fake Web MIDI");
  return sim.access;
}

// ---------------------------------------------------------------- start
tab(TABS.includes(location.hash.slice(1)) ? location.hash.slice(1) : "backup");
loadBaks();
renderUnit(); renderLibrary(); pool.render(); importer.render();
if (SIM) connect();
else if (navigator.permissions && navigator.requestMIDIAccess) {
  navigator.permissions.query({ name: "midi", sysex: true }).then((p) => { if (p.state === "granted") connect(); }).catch(() => {});
}
window.__mgr = { S, connect, backup, openFile, tab, pool, importer, refreshRows, editor, exporter, bfizz, openInEditor, live, projtab, useBrowser, leaveBrowser, bu: () => BU };
