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
//   &bt         (with ?sim) its port named "FM-1 C1F0 Bluetooth", as the Bluetooth package's
//   ?depth=N    the backup's requests outstanding (1, the default: one at a time); &gap=MS their spacing
import { findFizz, FZER, isBlePort } from "./fizzlink.js";
import { makeFizzbak, readBackup, backupFileName, soundKey } from "./fizzbak.js";
import { readRegion, layoutOf, LAYOUTS, PACKAGE_NAME, convertImage } from "./fizzstore.js";
import { modelOf, imageOf, fitModel, fitChoice, projects as projectsOf } from "./projects.js";
import { parseDump, FIZZ_TAGS, storeSound, paramRequest, CMD } from "../js/fizz_sysex.js";
import { soundFromStoreFrame, soundToFizzsnd, soundFileName, makeFizzlib, libraryFileName, sameSound } from "../js/library.js";
import { storeFrame } from "../js/digitone.js";
import { $, esc, tagsOf, colOf, emptyArt, refusal, confirmBox, MIDI_I, midiChip } from "./ui.js";
import { newFilter, filterHtml, filterClick, listHtml, poolPlaces, poolBadge } from "./presets.js";
import { mountPool } from "./pool.js";
import { mountImport } from "./importer.js";
import { mountProjects } from "./projtab.js";
import { mountEditor } from "./editor.js";
import { mountExport } from "./exporter.js";
import { BrowserFizz } from "./browserfizz.js";
import { BrowserUnit, BROWSER_ID } from "./browserunit.js";
import { mountUnit } from "./unitpicker.js";
import { mountLive } from "./live.js";
import { soundFromFrame, soundFromLib, storeFrameOf, libSound, tagNames, copySound, isMidiBlob, blob6, isMix } from "./sound.js";
import { browserRows, recordOf, keepIn, dropFrom, blankImage, asHost } from "./browserlib.js";
import { usedBy } from "./usedby.js";
import { mountKeys } from "./keystrip.js";
import { parseFizzsnd } from "../js/library.js";
import { readFiles } from "../js/digitone.js";

const Q = new URLSearchParams(location.search);
const SIM = Q.has("sim");
const DEPTH = Math.max(1, Math.min(4, +Q.get("depth") || 2)), GAP = Math.max(0, +Q.get("gap") || 0);   // (2 in flight: fm1-base holds two SysEx frames since 2026-10-27)
const kb = (n) => `${Math.round(n / 1024)} KB`;
const mmss = (ms) => { const s = Math.floor(ms / 1000); return `${Math.floor(s / 60)}:${String(s % 60).padStart(2, "0")}`; };

const PAGE_T0 = Date.now(), seenBaks = new Set();   // (BACKUP: a backup made since the page opened is NEW; a row pops in once)
const S = { mode: "fm1", access: null, link: null, unit: null, busy: false, baks: [], web: { tags: {} }, sel: null, filter: newFilter(), abort: null,
  editing: 0, writing: 0, users: new Map(), refreshing: false,
  brow: { img: null, rows: [] },                       // (LIBRARY's IN THIS BROWSER: the browser FIZZ's flash, its MY PRESETS)
  pick: { mode: false, keys: new Set(), anchor: "", to: "mine", tags: false },
  fresh: new Set() };                                  // (just sent to the FM-1: lit with NEW for a moment)   // (LIBRARY's SELECT: "source:slot" keys)

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
// the unit's package (docs/SYSEX.md "The package"): STATUS's package bytes, else its store's size (FZBI), else the USB
// one; its store's layout (fizzstore.js LAYOUTS: the backup's, the restore's) by FZBI's size. Over the Bluetooth
// package's own MIDI port a restore (FZSR) cannot run: the cable is needed
function unitPkg(u = S.unit) {
  const st = u && u.status, l = (u && u.info && layoutOf(u.info.size)) || null;
  const pk = st && st.package ? st.package : l ? l.package : "usb";
  return { package: pk, nproj: st && st.nproj ? st.nproj : l ? l.nproj : 8, nlib: st && st.nlib ? st.nlib : l ? l.nlib : 256, layout: l,
    name: PACKAGE_NAME[pk] || pk };
}
const viaBle = () => !!(S.link && S.link.viaBle);
const NEEDS_USB = "Connect the USB cable for this: a restore can't go over Bluetooth (backing up, the library and the projects' moves can).";
function renderUnit() {
  const u = S.unit, on = !!(u && S.link);
  const short = u && u.version ? u.version.replace(/-\d+-g[0-9a-f]+$/, "") : "";   // (a development build: git describe)
  // the version big, its beta a chip beside it (0.12.0-beta.12: 0.12.0 and BETA 12)
  const vm = short.match(/^(\d+\.\d+\.\d+)(?:-(beta|rc|alpha)\.?(\d+))?/i);
  $("u-ver").textContent = u ? (vm ? vm[1] : short || u.id) : "FM-1";
  $("u-ver").classList.toggle("dim", !on);
  $("u-ver").classList.toggle("long", !vm);
  $("u-beta").hidden = !(vm && vm[2]);
  if (vm && vm[2]) $("u-beta").textContent = `${vm[2].toUpperCase()} ${vm[3]}`;
  $("u-led").style.setProperty("--c", on ? "var(--green)" : "var(--dim)");
  $("u-pkg").textContent = on ? (S.mode === "browser" ? "BROWSER" : unitPkg().name.toUpperCase()) : "";
  const mine = u ? u.rows.filter((r) => r.source === "mine") : [];
  const sum = u && u.summary, st = u && u.status;
  $("u-pre").textContent = u && u.rows.length ? mine.length : "–";
  $("u-prj").textContent = st ? st.projects : sum ? sum.projects.length : "–";
  // the store's use: STATUS's pages (Phase 1: no backup needed), else the last backup's reading of it
  const use = st && st.lib[1] ? { lib: st.lib, prj: st.prj, all: (st.lib[0] + st.prj[0]) / (st.lib[1] + st.prj[1]) } : sum ? sum.used : null;
  $("u-use").innerHTML = use ? `${use.all > 0 ? Math.max(1, Math.round(use.all * 100)) : 0}<small>%</small>` : "–";
  const L = use && (st ? { lib: { nsec: st.lib[1] }, prj: { nsec: st.prj[1] } } : layoutOf(sum.size));
  const parts = L ? [use.lib[0] / use.lib[1] * L.lib.nsec, (1 - use.lib[0] / use.lib[1]) * L.lib.nsec,
    use.prj[0] / use.prj[1] * L.prj.nsec, (1 - use.prj[0] / use.prj[1]) * L.prj.nsec] : [0, 0, 0, 0];
  const tot = parts.reduce((a, b) => a + b, 0) || 1;
  // the store as 32 LED cells: MY PRESETS used (cream), their room (grey), PROJECTS used (green), their room (dark)
  const ends = parts.map((_, i) => parts.slice(0, i + 1).reduce((a, b) => a + b, 0) / tot * 32);
  const cells = Array.from({ length: 32 }, (_, i) => { const k = L ? ends.findIndex((e) => i + 0.5 < e) : -1; return ["lp", "lf", "lj", ""][k] ?? ""; });
  const sig = cells.join(",");
  if ($("u-meter").dataset.sig !== sig) {
    $("u-meter").dataset.sig = sig;
    $("u-meter").innerHTML = cells.map((c, i) => `<i class="${c}" style="--i:${i}"></i>`).join("");
  }
  $("u-meter").setAttribute("aria-label", use ? `The store ${Math.round(use.all * 100)} % used` : "The store");
  $("u-kb").textContent = u && u.info ? kb(u.info.size) : "";
  $("go-backup").disabled = !on || S.busy || !u.info;
  $("go-presets").disabled = !on || S.busy || !u.version;
  $("l-export-all").disabled = !on || S.busy || !u.version;
  $("refresh").hidden = !on;
  $("refresh").disabled = S.busy || S.refreshing;
}

// FM: the FM-1's side: Web MIDI (granted, or ?sim's fake), an FM-1 running FIZZ found on it while the browser's FIZZ is
// the unit (found), the browser FIZZ's version and STATUS when it was last the unit
const FM = { access: null, found: null, probing: false, bver: "", bstat: null, bmem: null, sig: "" };
// the FM-1 as the unit (CONNECT FM-1, the picker's FM-1, ⇄ FM-1): Web MIDI asked for here (a click: its prompt), else
// the browser's FIZZ stays the unit and the page says why
async function connect() {
  if (S.busy || attaching) { toast("Wait: a write is still going", "--orange"); return; }
  if (!FM.access && !(await openMidi())) return;
  if (S.mode === "browser") leaveBrowser();
  S.mode = "fm1";
  S.access = FM.access;
  const why = await attach();
  if (!S.link) {
    await useBrowser();
    if (why) say(why, "--orange");
  }
}
// Web MIDI (with SysEx): the FM-1's side, watched for ports coming and going. -> the access, or null (the page says why)
async function openMidi() {
  if (FM.access) return FM.access;
  if (SIM) FM.access = await simAccess();
  else {
    if (!navigator.requestMIDIAccess) { say("This browser has no Web MIDI with SysEx: the FM-1 needs <b>Chrome</b> or <b>Edge</b> on a computer. FIZZ in this browser works.", "--orange"); return null; }
    try { FM.access = await navigator.requestMIDIAccess({ sysex: true }); }
    catch (_) { say("MIDI (SysEx) was not allowed. Allow it for this site in the address bar, then CONNECT FM-1.", "--orange"); return null; }
  }
  FM.access.onstatechange = onState;
  return FM.access;
}
async function midiGranted() {
  if (SIM) return Q.get("sim") !== "nomidi";
  if (!navigator.requestMIDIAccess || !navigator.permissions) return false;
  try { return (await navigator.permissions.query({ name: "midi", sysex: true })).state === "granted"; } catch (_) { return false; }
}
// an FM-1 running FIZZ on Web MIDI, asked quietly: -> { id, port, version } or null
async function findUnit() {
  if (!FM.access) return null;
  try {
    const link = await findFizz(FM.access, { log: () => {} });
    try { const version = await link.identity(); return version ? { id: link.id, port: link.port, version } : null; } finally { link.close(); }
  } catch (_) { return null; }
}
let attaching = false;
// the link to S.access's unit (the FM-1's Web MIDI or the browser's FIZZ): -> "" or what went wrong (for the page's line)
async function attach() {
  if (attaching || S.link) return "";
  attaching = true;
  unitUi.render();
  if (S.mode === "fm1") say("");
  let why = "";
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
    const pk = unitPkg();
    log(`package: ${pk.name} (${pk.nproj} projects, ${pk.nlib} MY PRESETS)${link.viaBle ? `, over Bluetooth (${link.port})` : ""}`);
    if (S.mode === "browser") { FM.bver = version || ""; FM.bstat = status; }
    else {
      FM.found = null;
      if (!version && info) say("FIZZ does not answer (safe mode, or its data missing): back up and restore still work.", "--orange");
      else if (!info) say(`This FIZZ has no backup over USB yet: <a href="../install/">update FIZZ</a>.`, "--orange");
      else if (version && !status) say(`This FIZZ shows its library but cannot edit it yet: <a href="../install/">update FIZZ</a>.`, "--orange");
      else if (status && status.noStore) say("FIZZ runs without its store (safe mode): back up and restore still work.", "--orange");
      else if (link.viaBle) say("Connected over Bluetooth: everything works but a restore, which needs the USB cable.", "--cyan");
    }
    if ($("tab-backup").hidden) loadDumps();
    if (status) live.probe().then(() => { if (!$("tab-edit").hidden) live.show(); });
  } catch (e) {
    if (S.link) { try { S.link.close(); } catch (_) { /* gone */ } }
    S.link = null;
    why = {
      notfound: "No FM-1 answered. Check the USB cable, and close other apps and tabs that use MIDI (on Windows only one may).",
      notfizz: `This FM-1 runs ${esc(e.detail || "another firmware")}, not FIZZ. <a href="../install/">Install FIZZ</a>.`,
      update: `The FM-1 is in update mode. Finish the install on the <a href="../install/">install page</a>.`,
    }[e.code] || esc(e.message);
    log(`connect: ${e.code || ""} ${e.message}`);
  } finally {
    attaching = false;
    renderUnit(); renderLibrary(); renderBaks(); pool.render(); importer.render(); projtab.render();
    if (S.link && !$("tab-projects").hidden) projtab.show();
    unitUi.render();
  }
  return why;
}
// the FM-1 gone while it was the unit: back to the browser's FIZZ (once nothing writes)
function lost() {
  if (!S.link || S.mode !== "fm1") return;
  S.link = null;
  live.lost();
  log("the FM-1 was disconnected");
  FM.found = null;
  toast("FM-1 unplugged · back on THIS BROWSER", "--orange");
  renderUnit(); renderLibrary(); renderBaks(); pool.render(); importer.render(); projtab.render();
  (async () => { while (S.busy || attaching) await sleep(200); if (S.mode === "fm1" && !S.link) await useBrowser(); })();
}
let stateT = null;
function onState(ev) {
  const p = ev && ev.port;
  if (S.mode === "fm1") {
    if (p && p.state === "disconnected" && S.link) { try { S.link.close(); } catch (_) { /* gone */ } lost(); }
    return;
  }
  // the browser's FIZZ the unit: an FM-1 running FIZZ that comes is offered (⇄ FM-1), never taken by itself (a port
  // opened only, as a probe does: no change)
  const sig = portsSig();
  if (sig === FM.sig) return;
  FM.sig = sig;
  if (FM.found && !sig.split("|").includes(FM.found.port)) { FM.found = null; unitUi.render(); }
  clearTimeout(stateT);
  stateT = setTimeout(watchFm1, 800);
}
const portsSig = () => (FM.access ? [...FM.access.inputs.values()].filter((p) => p.state !== "disconnected").map((p) => p.name || "").sort().join("|") : "");
async function watchFm1() {
  if (S.mode !== "browser" || !FM.access || FM.probing) return;
  FM.probing = true;
  FM.sig = portsSig();
  const f = await findUnit();
  FM.probing = false;
  if (S.mode !== "browser") return;
  if (f && (!FM.found || FM.found.id !== f.id)) log(`an FM-1 running FIZZ ${f.version} on "${f.port}": ⇄ FM-1 to use it`);
  FM.found = f;
  unitUi.render();
}
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
    if (!safety) { toast("Backed up · kept in this browser", "--orange"); fizzUp(); }
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
// a backup done: bubbles rise from BACK UP for a few seconds
function fizzUp() {
  const a = $("b-acts");
  a.classList.remove("fizz");
  void a.offsetWidth;
  a.classList.add("fizz");
  clearTimeout(fizzUp.t);
  fizzUp.t = setTimeout(() => a.classList.remove("fizz"), 4200);
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
const SAVE_I = '<svg viewBox="0 0 20 20" aria-hidden="true"><path d="M10 3v10M5 9l5 5 5-5M3 17h14"/></svg>';
const DEL_I = '<svg viewBox="0 0 20 20" aria-hidden="true"><path d="M5 5l10 10M15 5L5 15"/></svg>';
const SHARE_I = '<svg viewBox="0 0 24 24" aria-hidden="true"><path d="M12 15V4M8 8l4-4 4 4M5 13v6h14v-6"/></svg>';
const MORE_I = '<svg viewBox="0 0 24 24" aria-hidden="true"><circle cx="5" cy="12" r="2"/><circle cx="12" cy="12" r="2"/><circle cx="19" cy="12" r="2"/></svg>';
const PLAY_I = '<svg viewBox="0 0 16 16" aria-hidden="true"><path d="M4 2.5l9.5 5.5L4 13.5z"/></svg>';
function whenText(t) {
  const d = new Date(t), now = new Date(), hm = `${String(d.getHours()).padStart(2, "0")}:${String(d.getMinutes()).padStart(2, "0")}`;
  const day = (x) => new Date(x.getFullYear(), x.getMonth(), x.getDate()).getTime();
  const dd = Math.round((day(now) - day(d)) / 86400000);
  return dd === 0 ? `Today ${hm}` : dd === 1 ? `Yesterday ${hm}` : `${d.getDate()} ${d.toLocaleString("en", { month: "short" })} ${hm}`;
}
function unitTracks() { return S.unit && S.unit.summary ? S.unit.summary.tracks : null; }
let bakMenu = null;                                     // (the row's ••• menu open: { el, btn })
function renderBaks() {
  closeBakMenu();
  const el = $("baks"), u = S.unit;
  el.innerHTML = S.baks.map((b, i) => {
    const ul = u && u.info && layoutOf(u.info.size), pkgs = ["device", "ble"];   // (one package's backup restores onto the other: fitModel)
    const old = b.kind === "full" && ((ul && b.layout && ul.name !== b.layout && !(pkgs.includes(ul.name) && pkgs.includes(b.layout))) ||
      (b.tracks && unitTracks() && b.tracks !== unitTracks()) || (b.tracks === 8));
    const what = b.kind === "presets" ? `presets only · ${b.presets}` : `${b.projects ? b.projects.length : "?"} project${b.projects && b.projects.length === 1 ? "" : "s"} · ${b.presets} preset${b.presets === 1 ? "" : "s"} · ${kb(b.size)}${b.tracks === 8 ? " · 8 tracks" : ""}${b.layout === "ble" ? " · Bluetooth" : ""}`;
    // (a tube each: the newest lit; NEW on one made since the page opened; an older FIZZ's orange)
    const ver = b.fizz ? b.fizz.replace(/-\d+-g[0-9a-f]+$/, "").replace(/-(beta|rc|alpha)\.?(\d+)$/i, (_, k, n) => ` ${k[0].toUpperCase()}${n}`) : "";
    const fresh = !seenBaks.has(b.id ?? b.name);
    return `<div class="bk tube ${fresh ? "pop" : ""} ${i === 0 ? "on" : ""} ${b.kind === "presets" ? "presets" : old ? "old" : b.safety ? "safety" : ""}" data-i="${i}" style="--i:${Math.min(i, 12)}">
      <span class="ic">${ICON}</span><div class="txt"><div class="when">${esc(whenText(b.date))}${b.date >= PAGE_T0 ? '<span class="new hud">NEW</span>' : ""}</div><div class="what">${esc(what)}</div></div>
      ${ver ? `<span class="ver hud ${old ? "old" : ""}">${esc(ver)}</span>` : ""}
      <div class="bka">${b.kind === "full" && typeof b.id === "number" ? `<button class="icon play" data-act="play" aria-label="Play ${esc(b.name)} in the browser's FIZZ" title="Play in the browser's FIZZ">${PLAY_I}</button>` : ""}
      <button class="ol rsb" data-act="restore" ${S.link && !S.busy ? "" : "disabled"}>RESTORE</button>
      <button class="icon share ${isSaved(b) ? "saved" : "unsaved"}" data-act="share" aria-label="Share ${esc(b.name)}${isSaved(b) ? "" : " (not saved to a file yet)"}" title="${isSaved(b) ? "Share" : "Share (not saved to a file yet)"}">${SHARE_I}</button>
      <button class="icon more" data-act="more" aria-haspopup="menu" aria-expanded="false" aria-label="More for ${esc(b.name)}">${MORE_I}</button></div></div>`;
  }).join("");
  for (const b of S.baks) seenBaks.add(b.id ?? b.name);   // (a row pops in once, when it first shows)
  renderUse();
}
const mb = (n) => (n >= 1073741824 ? `${+(n / 1073741824).toFixed(1)} GB` : n >= 1048576 ? `${(n / 1048576).toFixed(1)} MB` : kb(n));
let quota = null;
function renderUse() {
  const n = S.baks.length, sum = S.baks.reduce((a, b) => a + (b.size || 0), 0);
  $("b-use").innerHTML = n ? `<b>${String(n).padStart(2, "0")}</b><i> · </i><span>${mb(sum)}${quota ? `<small> ${esc(quota)}</small>` : ""}</span>` : "";
}
if (navigator.storage && navigator.storage.estimate) {
  navigator.storage.estimate().then((e) => { if (e && e.quota) { quota = `of ${mb(e.quota)}`; renderUse(); } }).catch(() => {});
}
// share (NEON): the file to the system's share sheet where the browser can share files (a phone), else saved to a
// file as before; shared counts as saved (a copy is outside this browser now); cancelled: nothing
async function shareBak(b) {
  let file = null, can = false;
  try { file = new File([b.bytes], b.name, { type: "application/octet-stream" }); can = !!(navigator.canShare && navigator.share && navigator.canShare({ files: [file] })); } catch (_) { can = false; }
  if (!can) return saveBak(b);
  try { await navigator.share({ files: [file] }); } catch (e) {
    if (e && e.name === "AbortError") return null;
    log(`share: ${e && e.message}: saved to a file instead`);
    return saveBak(b);
  }
  log(`shared ${b.name}`);
  if (!isSaved(b)) { b.saved = true; if (typeof b.id === "number") await DB.set(b); renderBaks(); }
  return null;
}
// the row's ••• menu: SAVE TO FILE, DELETE (a small popover by the button)
function closeBakMenu(refocus = false) {
  if (!bakMenu) return;
  const { el, btn } = bakMenu;
  bakMenu = null;
  el.remove();
  btn.setAttribute("aria-expanded", "false");
  if (refocus && btn.isConnected) btn.focus();
}
function openBakMenu(btn, b) {
  closeBakMenu();
  const m = document.createElement("div");
  m.className = "bkmenu";
  m.id = "bk-menu";
  m.setAttribute("role", "menu");
  m.innerHTML = `<button role="menuitem" data-act="save" class="${isSaved(b) ? "" : "unsaved"}">${SAVE_I}SAVE TO FILE</button><button role="menuitem" data-act="del" class="del">${DEL_I}DELETE</button>`;
  document.body.appendChild(m);
  const r = btn.getBoundingClientRect(), w = m.offsetWidth, h = m.offsetHeight;
  m.style.left = `${Math.max(12, Math.min(window.innerWidth - w - 12, r.right - w))}px`;
  m.style.top = `${r.bottom + 8 + h > window.innerHeight - 8 ? Math.max(8, r.top - h - 8) : r.bottom + 8}px`;
  btn.setAttribute("aria-expanded", "true");
  bakMenu = { el: m, btn };
  m.addEventListener("click", (ev) => { const x = ev.target.closest("[data-act]"); if (!x) return; closeBakMenu(); bakAct(x.dataset.act, b); });
  m.querySelector("button").focus();
}
document.addEventListener("pointerdown", (ev) => { if (bakMenu && !bakMenu.el.contains(ev.target) && ev.target !== bakMenu.btn && !bakMenu.btn.contains(ev.target)) closeBakMenu(); }, true);
document.addEventListener("keydown", (ev) => { if (ev.key === "Escape" && bakMenu) { ev.preventDefault(); closeBakMenu(true); } });
$("baks").addEventListener("scroll", () => closeBakMenu(), { passive: true });
window.addEventListener("resize", () => closeBakMenu());
$("baks").addEventListener("click", async (ev) => {
  const btn = ev.target.closest("button[data-act]");
  if (!btn) return;
  const b = S.baks[+btn.closest(".bk").dataset.i];
  if (!b) return;
  if (btn.dataset.act === "more") { if (bakMenu && bakMenu.btn === btn) closeBakMenu(); else openBakMenu(btn, b); return; }
  bakAct(btn.dataset.act, b);
});
async function bakAct(act, b) {
  if (act === "save") saveBak(b);
  else if (act === "share") shareBak(b);
  else if (act === "del") {
    const ok = await confirmBox({ title: "DELETE BACKUP", what: whenText(b.date), go: "DELETE",
      sub: isSaved(b) ? "From this browser. Its saved file stays." : "<b style='color:var(--yellow)'>Not saved to a file: this is its only copy.</b>" });
    if (!ok) return;
    if (typeof b.id === "number") await DB.del(b.id);
    S.baks.splice(S.baks.indexOf(b), 1);
    renderBaks();
  } else if (act === "restore") openFile(b.bytes, b.name, b);
  else if (act === "play") window.open(`../?backup=${b.id}`, "_blank");   // (js/backup-play.js reads it there)
}
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
const R = { parsed: null, rec: null, mode: "full", safe: false, fit: null, pick: null };
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
// the restore's target: the unit's own layout. A full backup of the other package is written again in this one's
// layout (projects.js fitModel: record by record; the Bluetooth package keeps 4 projects and 128 MY PRESETS, chosen
// here, the others left out and listed); a presets-only one adds what is not there yet (only as many as MY PRESETS has
// places for: chosen here too)
function openRestore(parsed, rec) {
  const u = S.unit, tgt = u.info && layoutOf(u.info.size), pk = unitPkg();
  R.parsed = parsed; R.rec = rec; R.fit = null; R.pick = null;
  const src = parsed.summary && parsed.summary.layout;
  R.mode = parsed.image && tgt && src ? "full" : "presets";
  if (R.mode === "full" && src !== tgt.name) {
    try {
      const m = modelOf(parsed.image);
      R.fit = { model: m, to: tgt, choice: fitChoice(m, tgt), from: LAYOUTS[src] };
    } catch (e) { log(`restore: ${e.message}`); R.mode = "presets"; }
  }
  R.safe = R.mode !== "full";
  const full = R.mode === "full", sum = parsed.summary;
  $("rs-modal").style.setProperty("--c", full ? "var(--yellow)" : "var(--cream)");
  $("rs-title").textContent = full ? "RESTORE" : "ADD PRESETS";
  $("rs-when").textContent = whenText(rec.date);
  const nowMine = u.rows.filter((r) => r.source === "mine").length;
  $("rs-now").textContent = `${u.summary ? u.summary.projects.length : "?"} · ${nowMine}`;
  const chips = full ? sum.projects.map((p) => `<span class="pchip">${esc(p.name || `PROJECT ${p.slot}`)}</span>`) : [];
  const d = parsed.presets.length - nowMine;
  if (full && d && !R.fit) chips.push(`<span class="tag delta" style="background:var(${d < 0 ? "--orange" : "--green"})">${d > 0 ? "+" : "−"}${Math.abs(d)} PRESETS</span>`);
  if (full && sum.tracks === 8) chips.push(`<span class="tag delta" style="background:var(--orange)">8 TRACKS · PROJECTS START FRESH</span>`);
  if (R.fit && R.fit.from.package !== R.fit.to.package) chips.push(`<span class="tag delta" style="background:var(--cyan)">${esc(PACKAGE_NAME[R.fit.from.package].toUpperCase())} BACKUP → ${esc(pk.name.toUpperCase())} PACKAGE</span>`);
  if (!full && parsed.image) chips.push(`<span class="tag delta" style="background:var(--orange)">ANOTHER STORE · PRESETS ONLY</span>`);
  $("rs-chips").innerHTML = chips.join("");
  $("rs-warn").hidden = !full;
  $("rs-type").value = "";
  $("rs-type").classList.remove("ok");
  $("rs-safe").innerHTML = "";
  lcd("ask");
  $("rs-key").textContent = "PLAY";
  $("rs").hidden = false;
  $("rs-fit").hidden = true; $("rs-fit").innerHTML = "";
  if (full) { $("rs-then").textContent = `${sum.projects.length} · ${parsed.presets.length}`; $("rs-go").textContent = "RESTORE"; rsFit(); }
  if (full && viaBle()) { $("rs-safe").innerHTML = `<b style="color:var(--orange)">${esc(NEEDS_USB)}</b>`; rsReady(); return; }
  if (full) { safetyCopy(); return; }
  rsPresets();
  rsReady();
  $("rs-go").focus();
  if (!u.dumps && S.link && u.version) loadDumps().then(() => { if (R.parsed === parsed && R.mode === "presets") { R.pick = null; rsPresets(); rsReady(); } });
}
// what fits: the choice of projects and MY presets when the backup holds more than this package has places for
const MAXLIST = 400;
function fitHtml(title, items, on, max, kind) {
  const n = items.filter((x) => on.has(x.key)).length;
  return `<div class="fitlist" data-kind="${kind}"><div class="kv">${title} <b class="${n > max ? "over" : ""}">${n} / ${max}</b></div><div class="fitrows">` +
    items.slice(0, MAXLIST).map((x) => `<label><input type="checkbox" data-k="${x.key}" ${on.has(x.key) ? "checked" : ""} ${!on.has(x.key) && n >= max ? "disabled" : ""}><span>${esc(x.label)}</span></label>`).join("") + "</div></div>";
}
function rsFit() {
  const box = $("rs-fit"), f = R.fit;
  if (!f) return;
  const all = projectsOf(f.model).filter((x) => x.there), sum = R.parsed.summary;
  const tooMany = all.length > f.to.nproj || sum.presets.length > f.to.nlib;
  let left = null;
  try { left = fitModel(f.model, f.to, f.choice).left; } catch (_) { left = null; }
  $("rs-then").textContent = `${f.choice.projects.length} · ${f.choice.presets.length}`;
  box.hidden = false;
  const head = `<div class="fithead">This FM-1 runs the <b>${esc(PACKAGE_NAME[f.to.package])} package</b>: ${f.to.nproj} projects, ${f.to.nlib} MY PRESETS. ` +
    (tooMany ? `This backup has ${all.length} projects and ${sum.presets.length} presets: <b>choose what to keep</b>.` : "Everything in this backup fits.") + "</div>";
  if (!tooMany) { box.innerHTML = head; return; }
  const on = new Set(f.choice.projects.map((p) => `p${p}`)), onl = new Set(f.choice.presets.map((i) => `l${i}`));
  const libs = sum.presets.map((x) => ({ key: `l${x.slot - 1}`, label: `${x.slot} ${x.name}` }));
  box.innerHTML = head + `<div class="fitcols">${fitHtml("PROJECTS", all.map((x) => ({ key: `p${x.p}`, label: `${x.p + 1} ${x.name || `PROJECT ${x.p + 1}`}` })), on, f.to.nproj, "p")}` +
    `${fitHtml("MY PRESETS", libs, onl, f.to.nlib, "l")}</div>` +
    `<div class="fitleft">${left ? leftText(left) : ""}</div>`;
}
function leftText(left) {
  const names = (xs, n = 8) => xs.slice(0, n).map((x) => esc(x.name || "?")).join(", ") + (xs.length > n ? ` and ${xs.length - n} more` : "");
  const parts = [];
  if (left.projects.length) parts.push(`${left.projects.length} project${left.projects.length === 1 ? "" : "s"} (${names(left.projects)})`);
  if (left.presets.length) parts.push(`${left.presets.length} MY preset${left.presets.length === 1 ? "" : "s"} (${names(left.presets)})`);
  return parts.length ? `<b>Left out:</b> ${parts.join("; ")}.` + (left.presets.length ? " A kept project that plays one of them keeps that sound as its own copy." : "") : "";
}
$("rs-fit").addEventListener("change", (ev) => {
  const i = ev.target.closest("input[data-k]");
  if (!i) return;
  const k = i.dataset.k, n = +k.slice(1);
  if (R.fit) {
    const list = k[0] === "p" ? R.fit.choice.projects : R.fit.choice.presets;
    const at = list.indexOf(n);
    if (i.checked && at < 0) list.push(n); else if (!i.checked && at >= 0) list.splice(at, 1);
    list.sort((a, b) => a - b);
    rsFit();
  } else if (R.pick) {
    if (i.checked) R.pick.on.add(n); else R.pick.on.delete(n);
    rsPresets();
  }
  rsReady();
});
// presets only: what MY PRESETS has places for (the Bluetooth package's 128 fill sooner); the ones already there skipped
function rsPresets() {
  const box = $("rs-fit"), u = S.unit, list = R.parsed.presets, room = unitPkg().nlib - u.rows.filter((r) => r.source === "mine").length;
  const have = u.dumps ? [...u.dumps.values()].map(soundFromStoreFrame) : [];
  const fresh = list.map((s, i) => ({ s, i })).filter((x) => !have.some((h) => sameSound(h, x.s)));
  if (!R.pick) R.pick = { room, fresh, on: new Set(fresh.slice(0, Math.max(0, room)).map((x) => x.i)) };
  const n = R.pick.on.size;
  $("rs-then").textContent = `${u.summary ? u.summary.projects.length : "?"} · ${Math.min(unitPkg().nlib, u.rows.filter((r) => r.source === "mine").length + n)}`;
  $("rs-go").textContent = `ADD ${n}`;
  if (fresh.length <= room) { box.hidden = true; box.innerHTML = ""; return; }
  const out = fresh.filter((x) => !R.pick.on.has(x.i)).map((x) => ({ name: x.s.name }));
  box.hidden = false;
  box.innerHTML = `<div class="fithead">MY PRESETS has <b>${Math.max(0, room)}</b> free place${room === 1 ? "" : "s"} (the ${esc(unitPkg().name)} package holds ${unitPkg().nlib}): ` +
    `this backup has ${fresh.length} presets not on the FM-1. <b>Choose which to add.</b></div>` +
    fitHtml("ADD", fresh.map((x) => ({ key: `l${x.i}`, label: x.s.name })), new Set([...R.pick.on].map((i) => `l${i}`)), Math.max(0, room), "l") +
    `<div class="fitleft">${out.length ? `<b>Left out:</b> ${out.length} (${out.slice(0, 8).map((x) => esc(x.name)).join(", ")}${out.length > 8 ? ` and ${out.length - 8} more` : ""})` : ""}</div>`;
}
function rsReady() {
  const typed = $("rs-type").value.trim().toUpperCase() === "RESTORE";
  $("rs-type").classList.toggle("ok", typed);
  const fitOk = !R.fit || (R.fit.choice.projects.length <= R.fit.to.nproj && R.fit.choice.presets.length <= R.fit.to.nlib);
  const pickOk = !R.pick || R.pick.on.size > 0;
  $("rs-go").disabled = S.busy || !S.link || !R.safe || (R.mode === "full" && (!typed || viaBle() || !fitOk)) || (R.mode !== "full" && !pickOk);
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
function closeRestore() { $("rs").hidden = true; R.parsed = null; R.fit = null; R.pick = null; }
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
  S.unit.dumps = null; S.unit.fdumps = null; S.unit.one = null; S.sel = null;
  if (!S.unit.version) { S.unit.rows = []; S.unit.status = null; S.unit.projects = null; }
  pool.forget();
  await refreshRows({ projects: true });               // (every tab drawn again)
}
async function restoreFull() {
  let image = R.parsed.image;
  const rec = R.rec, f = R.fit;
  if (f) {                                             // (another package's backup: its records in this one's layout)
    try {
      const r = fitModel(f.model, f.to, f.choice);
      image = imageOf(r.model);
      log(`restore ${rec.name}: ${PACKAGE_NAME[f.from.package]} -> ${PACKAGE_NAME[f.to.package]} layout, ${f.choice.projects.length} projects, ${f.choice.presets.length} MY presets, ${r.copies} sounds as the projects' own copies; left out ${r.left.projects.length} projects, ${r.left.presets.length} presets`);
    } catch (e) {
      say(e.code === "full" ? `What you chose does not fit the ${esc(PACKAGE_NAME[f.to.package])} package's store: keep fewer projects (or add the presets alone).`
        : `This backup could not be written for this FM-1: ${esc(e.message)}`, "--orange");
      return;
    }
  }
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
    await refresh().catch((e) => log(`after the restore: ${e.code || ""} ${e.message}`));   // (written: a lost read is not a failed write)
    return true;
  } catch (e) {
    progClose();
    log(`restore: ${e.code || ""} ${e.fzer || ""} ${e.message}`);
    if (e.code === "cancel") toast("Cancelled. Nothing changed.", "--muted");
    else if (e.fzer === 10) say("The FM-1 is playing. Stop it, then try again.", "--orange");
    else if (e.fzer === 11) say("This backup is from a store of another size. Its presets can still be added.", "--orange");
    else if (e.fzer === 15) say(esc(NEEDS_USB), "--orange");
    else if (e.fzer === 12) say("The store did not reach the FM-1 whole. Nothing was changed.", "--orange");
    else if (e.code === "timeout" && e.written === false) say("The FM-1 did not take the write up. Nothing was changed: try again.", "--orange");
    else if (e.fzer === 13 || e.fzer === 14 || e.code === "lost" || (e.code === "timeout" && e.written !== false))
      say(`The write was cut off. <b>FIZZ has cleared its store.</b> Plug the FM-1 in and restore the newest backup in BACKUP (${esc(FZER[e.fzer] || e.message)}).`, "--red");
    else say(`The write did not finish: ${esc(e.message)}`, "--red");
    if (S.link && e.code !== "lost") await refresh().catch(() => {});
    return false;
  } finally {
    S.busy = false; S.abort = null;
    renderUnit(); renderLibrary(); renderBaks(); pool.render(); importer.render(); projtab.render();
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
  const list = R.pick ? R.parsed.presets.filter((_, i) => R.pick.on.has(i)) : R.parsed.presets, rec = R.rec;
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
  } catch (e) {
    progClose();
    say(`Adding the presets stopped: ${esc(e.message)} (${added} added)`, "--red");
  } finally {
    S.busy = false;
    if (S.unit) { S.unit.dumps = null; S.unit.summary = null; }
    renderBaks();
  }
  await refreshRows();                                 // (every tab: what was added, also when it stopped)
}

// ---------------------------------------------------------------- edits on the unit (Phase 1)
// one edit at a time (the link's queue serialises them anyway): the page's state refreshed after it; a refusal said
async function edit(what, fn, { quiet = false } = {}) {
  if (!S.link || S.busy) return null;
  S.editing++;
  renderLibrary();
  try {
    const r = await fn(S.link);
    if (r && typeof r.status === "number" && r.status !== 0 && r.status !== 10) say(refusal(r, what), r.status === 8 ? "--orange" : "--red");
    return r;
  } catch (e) {
    log(`${what}: ${e.code || ""} ${e.message}`);
    if (!(quiet && e.code === "timeout")) say(`${esc(what)}: ${esc(e.message)}`, "--red");   // (quiet: editSure says it)
    return null;
  } finally {
    S.editing--;
  }
}
// a write, then the unit's lists read again until they show it. took(rows, projects): true when they do (the unit's
// lists can lag a write a moment). Its reply lost or late (the owner's FM-1 loses a frame at times): the lists say
// whether it took; a write that is safe twice (again: a name, tags) is then sent once more. shown(x): the page shows
// it at once, before the lists come back. -> the reply ({status: 0, unsure: true}: only the lists showed it) | null
async function editSure(what, fn, { took = null, again = false, projects = false, shown = null } = {}) {
  if (!S.link || S.busy) return null;
  S.writing++;                                         // (the write and its lists: the focus's quick REFRESH waits)
  try {
    let x = null;
    for (let k = 0; k < (again ? 2 : 1) && !x && S.link; k++) {
      x = await edit(what, fn, { quiet: true });
      if (x) break;
      if (!S.link) return null;
      await refreshRows({ projects, until: took });
      if (took && took(S.unit.rows, S.unit.projects)) { log(`${what}: its reply lost, the FM-1 has it`); return { status: 0, unsure: true }; }
      if (again && !k) log(`${what}: no reply and not on the FM-1: sent again`);
    }
    if (!x) {
      if (S.link) say(`${esc(what)}: the FM-1 did not answer and does not show it. Try again.`, "--red");
      return null;
    }
    if (x.status === 0 && shown) shown(x);
    await refreshRows({ projects, until: x.status === 0 ? took : null });
    return x;
  } finally { S.writing--; }
}
// the unit's lists again: LIST (MY PRESETS, the factory's, the pool), STATUS, PROJECTS. until(rows, projects): what an
// edit just made shows in them (the page showed it already): read again (up to 4 times, 0.3 s apart) until it does,
// else the last read is kept; a read that failed keeps what the page has. Every tab drawn again
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
async function refreshRows({ projects = false, until = null } = {}) {
  const u = S.unit, link = S.link;
  if (!u || !link || !u.version) return;
  try {
    for (let k = 0; k < 4; k++) {
      const rows = await link.list({ factory: true, pool: true }).catch((e) => { log(`refresh: LIST ${e.code || ""} ${e.message}`); return null; });
      const pj = projects ? await link.projects() : null;
      if (rows) u.rows = rows;
      if (pj) u.projects = pj;
      if (rows && (!until || until(u.rows, u.projects))) break;
      if (k === 3) { if (until) log("refresh: the FM-1's lists still differ from the edit: the FM-1's kept"); break; }
      await sleep(300);
    }
    const st = await link.status();
    if (st) u.status = st;
    if (!u.projects) u.projects = await link.projects();
  } catch (e) { log(`refresh: ${e.code || ""} ${e.message}`); }
  S.users.clear();
  u.one = null;                                        // (a preset read by itself: again, as it is now)
  followPlace();
  renderUnit(); renderLibrary(); pool.render(); projtab.render(); importer.render();
}
// EDIT's header: the MY preset it came from followed by its id (moved, renamed, deleted on the unit), so SAVE goes to
// the right place
function followPlace() {
  const p = editor.place(), E = editor.state;
  if (!S.unit || !E.s || p.source !== "mine" || !p.id || p.fromTarget) return;
  const row = S.unit.rows.find((q) => q.source === "mine" && q.id === p.id);
  if (!row) { editor.setPlace({ label: "NEW SOUND", source: "deleted" }); return; }
  if (E.base && E.s.name === E.base.name) E.s.name = E.base.name = row.name;   // (not renamed here: the unit's name)
  if (E.base && E.s.tags === E.base.tags) E.s.tags = E.base.tags = row.tags;
  editor.setPlace({ ...p, label: `MY PRESETS ${row.slot}`, slot: row.slot });
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
  if (quick && (S.editing || S.writing || !u.version)) return false;
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
let libSig = "", keys = null;                          // (keys: the computer keys' strip, keystrip.js)
const pkey = (r) => `${r.source}:${r.slot}`;
const browserShown = () => S.mode !== "browser";      // (BROWSER mode: MY PRESETS are the browser's already)
const SRC_SHORT = { all: "ALL", factory: "FACTORY", mine: "MINE", browser: "BROWSER" };
function renderLibrary() {
  const u = S.unit;
  const rows = u ? u.rows : [];
  if (!browserShown() && S.filter.src === "browser") S.filter.src = "all";
  const brows = u && browserShown() ? S.brow.rows : [];
  const mine = rows.filter((r) => r.source === "mine");
  const br = S.filter.src === "browser";
  $("l-count").innerHTML = u ? (br ? `${brows.length}<small>/256</small>` : `${mine.length}<small>/${unitPkg().nlib}</small>`) : "";
  // (the counts beside the filters: the sources', and the tags' among the source shown; the search narrows them)
  const qok = (r) => !S.filter.q || r.name.toLowerCase().includes(S.filter.q);
  const lr = rows.filter((r) => (r.source === "mine" || r.source === "factory") && qok(r)), bq = brows.filter(qok);
  const sr = br ? bq : lr.filter((r) => S.filter.src === "all" || r.source === S.filter.src);
  const nm = lr.filter((r) => r.source === "mine").length;
  const counts = u ? { src: { all: lr.length, factory: lr.length - nm, mine: nm, browser: bq.length }, tags: Object.fromEntries(FIZZ_TAGS.map((t, k) => [t, sr.filter((r) => (r.tags >> k) & 1).length])) } : null;
  $("l-filter").innerHTML = filterHtml(S.filter, { counts, browser: !!u && browserShown(), all: true });
  $("l-src").textContent = `${SRC_SHORT[S.filter.src] || "ALL"} ▾`;
  $("l-src").classList.toggle("br", S.filter.src === "browser");
  // (the list pops in when what it shows changes, not on every pick)
  const sig = u ? JSON.stringify([S.filter.src, [...S.filter.tags], S.filter.pool, S.filter.q, lr.map((r) => r.id), bq.map((r) => r.id)]) : "";
  const pop = sig !== libSig;
  libSig = sig;
  const P = S.pick, picking = P.mode || P.keys.size > 0;
  const html = u ? listHtml([...rows, ...brows], S.filter, { frameOf, places: places(), pop,
    on: (r) => (picking ? P.keys.has(pkey(r)) : !!S.sel && S.sel.source === r.source && S.sel.slot === r.slot),
    check: picking ? (r) => P.keys.has(pkey(r)) : null, isNew: (r) => S.fresh.has(pkey(r)),
    drag: (r) => r.source === "mine" && !!u.status && !picking }) : "";
  $("l-list").innerHTML = html || (u ? "" : `<div class="empty">${emptyArt()}</div>`);
  $("l-list").classList.toggle("picking", picking);
  $("tab-library").classList.toggle("bmode", S.mode === "browser");   // (the browser's FIZZ is the unit: MY PRESETS orange)
  renderSel();
  renderBulk();
}
const DOTS = (n) => `<span class="dots">${"●".repeat(Math.min(6, n))}${n > 6 ? "+" : ""}</span>`;
function usedText(x) {
  if (!x || !x.projects || !x.projects.length) return "";
  const steps = x.projects.reduce((a, p) => a + (p.steps || 0), 0);
  return `Used in ${x.projects.length} project${x.projects.length === 1 ? "" : "s"}${steps ? ` · ${steps} step${steps === 1 ? "" : "s"}` : ""}: those steps go back to their track's sound.`;
}
// USED BY with each pattern's steps (FIZZ NEON): from a store read here (BROWSER: the browser's flash; the PROJECTS tab's
// last read; else this unit's newest backup kept here, its time shown) -> {model, at} | null (none) | undefined (reading)
const ubModels = new WeakMap();
function storeModel(src) {
  const m = ubModels.get(src);
  if (m !== undefined) return m === "…" ? undefined : m;
  ubModels.set(src, "…");
  (async () => {
    try { const img = src instanceof Uint8Array ? src : (await readBackup(src.bytes, src.name)).image; ubModels.set(src, img ? modelOf(img) : null); }
    catch (_) { ubModels.set(src, null); }
    if (!$("tab-library").hidden) renderSel();
  })();
  return undefined;
}
function usedModel(browserRow = false) {
  let src = null, at = null;
  if (browserRow) src = S.brow.img;
  else if (S.mode === "browser") src = (BU && BU.host) || S.brow.img;
  else if (projtab.state.base) return { model: projtab.state.base.model, at: null };
  else { src = S.unit && S.baks.find((x) => x.kind === "full" && x.identity === S.unit.id); at = src ? src.date : null; }
  if (!src) return null;
  const m = storeModel(src);
  return m ? { model: m, at } : m;
}
const hasId = (m, id) => [...m.lib.values()].some((b) => b.length === 151 && b[0] === 0x53 && (b[143] | (b[144] << 8)) === id);
const ageText = (t) => { const d = new Date(t), now = new Date(); return d.toDateString() === now.toDateString()
  ? `${String(d.getHours()).padStart(2, "0")}:${String(d.getMinutes()).padStart(2, "0")}` : `${d.getDate()} ${d.toLocaleString("en", { month: "short" }).toUpperCase()}`; };
// -> the section's html, or "" (no store has it: the unit's USERS chips instead)
function usedStrips(id, browserRow = false) {
  const um = usedModel(browserRow);
  if (!um || !id || !hasId(um.model, id)) return "";
  const x = usedBy(um.model, id, { max: 8 });
  const age = um.at ? `<span class="hud uage" title="From the backup of ${esc(whenText(um.at))}">${esc(ageText(um.at))}</span>` : "";
  const strip = (row) => `<div class="ub"><span class="ubt"><b>${esc(row.project)}</b> · ${row.pat} · track ${row.track}</span>` +
    `<span class="steps" role="img" aria-label="${row.plays} step${row.plays === 1 ? "" : "s"}">${row.steps.map((st, i) => `<i class="${st.plays ? "p" : st.trig ? "t" : ""}" style="--i:${i}"></i>`).join("")}</span></div>`;
  return `<div class="selsec"><div class="lbl ubh">Used by${age}</div><div class="ubs" id="l-users">${x.rows.length ? x.rows.map(strip).join("") +
    (x.more ? `<span class="hud ubmore">+${x.more}</span>` : "") : '<span class="pchip ghost">—</span>'}</div></div>`;
}
// LOAD ON FM-1's track (0..5): the one picked in this tab's session, else the unit's selected track
const LT_KEY = "fizz-manager-load-track";
let loadT = (() => { try { const v = sessionStorage.getItem(LT_KEY); return v !== null && +v >= 0 && +v < 6 ? +v : null; } catch (_) { return null; } })();
function unitSel() {   // (STATUS's, read after each write and REFRESH; the live editor's while it follows the unit)
  const st = S.unit && S.unit.status, ls = live && live.state;
  const u = ls && ls.usel >= 0 && (live.following() || !st || st.sel === undefined) ? ls.usel : st && st.sel !== undefined ? st.sel : -1;
  return u >= 0 && u < 6 ? u : -1;
}
const loadTrack = () => (loadT !== null ? loadT : Math.max(0, unitSel()));
function renderSel() {
  if (keys) keys.render();
  if (S.sel && S.sel.source === "browser") { renderBrowserSel(); return; }
  const el = $("l-sel"), u = S.unit;
  const r = S.sel && u && u.rows.find((x) => x.source === S.sel.source && x.slot === S.sel.slot);
  if (!r && u && S.mode === "browser") { thisBrowserCard(el); return; }
  if (!r) {
    el.style.removeProperty("--c");
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
  const tg = tagsOf(r.tags), ring = mi ? MIDI_I.replace('class="din"', 'class="din algo"') : `<span class="hud">${d ? Math.min(8, d.blob[2] + 1) : ""}</span>`;
  el.style.setProperty("--c", c);
  el.innerHTML = `<div class="selhead"><div class="selring" aria-hidden="true"><span class="halo"></span><span class="core">${ring}</span></div>
      <div class="selid"><div class="selsrc hud">${midiChip(mi)}${mineSel ? "MY PRESETS" : "FACTORY"} · ${String(r.slot).padStart(2, "0")}</div>
      ${mineSel && can ? `<input class="bigname" id="l-name" maxlength="12" spellcheck="false" autocomplete="off" value="${esc(r.name)}" aria-label="Name">` : `<div class="big">${esc(r.name)}</div>`}
      <div class="selchips">${tg[0] ? `<span class="tag t-${tg[0]}">${tg[0]}</span>` : ""}${poolBadge(pl, "pbadge big")}</div></div></div>
    <div class="selgo">
      <div class="loadon wide"><button class="ol onunit ${onUnit ? "on" : ""}" id="l-unit" style="--c:var(--cream)" ${S.link && !S.busy && u.status && r.id ? "" : "disabled"} aria-pressed="${!!onUnit}" aria-label="${onUnit ? `On the FM-1, track ${br.t + 1}: OCT minus cancels, OCT plus loads` : "Load on the FM-1"}">${onUnit
        ? `<span class="okeys" aria-hidden="true"><i class="no">OCT−</i><i class="yes">OCT+</i></span>TRACK ${br.t + 1}`
        : '<svg viewBox="0 0 22 20" aria-hidden="true"><rect x="1.5" y="3" width="19" height="13" rx="3"/><path d="M6 19h10M9 7.5l4 2.5-4 2.5z"/></svg>LOAD ON FM-1'}</button>
      <span class="ltrk" role="radiogroup" aria-label="The track to load on">${[0, 1, 2, 3, 4, 5].map((t) => { const on = onUnit ? br.t === t : loadTrack() === t;
        return `<button class="${on ? "on" : ""}${unitSel() === t ? " usel" : ""}" data-ltrk="${t}" role="radio" aria-checked="${on}" aria-label="Track ${t + 1}" ${S.link && !S.busy && u.status && r.id && !onUnit ? "" : "disabled"}>${t + 1}</button>`; }).join("")}</span></div>
      <button class="ol" id="l-edit" style="--c:var(--pink)" ${f ? "" : "disabled"}><svg viewBox="0 0 20 20" aria-hidden="true"><path d="M3 15l9-9 3 3-9 9H3zM13 5l2-2 3 3-2 2"/></svg>EDIT</button>
      <button class="ol pooltog ${pl.length ? "out" : ""}" id="l-pool" style="--c:var(--green)" ${can && r.id ? "" : "disabled"} aria-pressed="${!!pl.length}">${pl.length ? "− POOL" : "+ POOL"}</button>
      <button class="ol ${browserShown() ? "" : "wide"}" id="l-export" style="--c:var(--violet)" ${f ? "" : "disabled"}><svg viewBox="0 0 20 20" aria-hidden="true"><path d="M10 13V3M6 7l4-4 4 4M4 12v5h12v-5"/></svg>EXPORT</button>
      ${browserShown() ? `<button class="ol" id="l-keep" style="--c:var(--cream)" ${f ? "" : "disabled"}>${BROWSER_I}KEEP IN BROWSER</button>` : ""}</div>
    <div class="selsec"><div class="lbl">Tags</div><div class="row tags">
      ${FIZZ_TAGS.map((t, k) => `<button class="tag t-${t} ${(r.tags >> k) & 1 ? "on" : "off"}" data-dtag="${k}" aria-pressed="${!!((r.tags >> k) & 1)}" ${can ? "" : "disabled"}>${t}</button>`).join("")}
      ${web.map((t) => `<button class="tag web" data-web="${esc(t)}" aria-label="${esc(t)}, remove">${esc(t)}</button>`).join("")}
      ${key ? '<button class="tag web" id="l-addtag" aria-label="Add a tag (kept on this computer and in backups)">+</button>' : ""}</div></div>
    ${(mineSel && usedStrips(r.id)) || (mineSel ? `<div class="selsec"><div class="lbl">Used by</div><div class="row users" id="l-users">${users === undefined ? '<span class="pchip ghost">…</span>'
      : users && users.projects ? (users.projects.length ? users.projects.map((p) => `<span class="uchip">${esc(projectName(p.project))}${p.steps ? DOTS(p.steps) : ""}</span>`).join("") : '<span class="pchip ghost">—</span>') : ""}</div></div>` : "")}
    ${d ? `<div class="selvals"><div class="kv"><b>${d.pan > 0 ? "R" + d.pan : d.pan < 0 ? "L" + -d.pan : "C"}</b>pan</div><div class="kv"><b>${d.chorus}</b>chorus</div><div class="kv"><b>${d.delay}</b>delay</div><div class="kv"><b>${d.reverb}</b>reverb</div><div class="kv"><b>${d.drive}</b>drive</div></div>` : ""}
    <div class="selacts">${r.imported ? '<span class="tag web">↓ IMPORTED</span>' : ""}
      ${mineSel ? `<button class="ol redline" id="l-del" style="--c:var(--red)" ${can ? "" : "disabled"}>DELETE</button>` : ""}</div>`;
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
    const x = await editSure("TAGS", (l) => l.tags(r.source, r.slot, mask), { again: true, shown: () => { r.tags = mask; renderLibrary(); },
      took: (rs) => rs.some((q) => q.source === r.source && q.slot === r.slot && q.tags === mask) });
    if (x && x.status === 0) {
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
      const x = await editSure("RENAME", (l) => l.rename("mine", r.slot, v), { again: true, shown: () => { r.name = v; renderLibrary(); },
        took: (rs) => rs.some((q) => q.source === "mine" && q.slot === r.slot && q.name === v) });
      if (x && x.status === 0) {
        const wk = key && S.web.tags[key];
        await redump(r.slot);
        if (wk) { const f2 = frameOf(r); if (f2) { S.web.tags[soundKey(soundFromStoreFrame(f2))] = wk; delete S.web.tags[key]; saveWeb(); } }
        toast(`Renamed ${v}`);
        renderLibrary();
      }
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
  const kb = $("l-keep");
  if (kb && f) kb.addEventListener("click", () => keepInBrowser([r]));
  const lu = $("l-unit");                                // (the unit's own browser on it: OCT- cancels, OCT+ loads)
  if (lu) lu.addEventListener("click", async () => { if (live) { await live.loadOnUnit(r, loadTrack()); renderSel(); } });
  el.querySelectorAll("[data-ltrk]").forEach((b) => b.addEventListener("click", () => {
    loadT = +b.dataset.ltrk;
    try { sessionStorage.setItem(LT_KEY, String(loadT)); } catch (_) { /* (no storage: this page only) */ }
    renderSel();
  }));
  // + POOL: the first free place references it; − POOL: every place that does is cleared (a used one asks first)
  const tog = $("l-pool");
  if (tog) tog.addEventListener("click", async () => {
    if (!pl.length) {
      const x = await editSure("+ POOL", (l) => l.poolRef(0, r.id), { took: (rs) => rs.some((q) => q.source === "pool" && q.id === r.id),
        shown: (y) => { poolSet(y.a, r); toast(`${r.name} → POOL ${y.a}`); pool.fizz(y.a); } });
      if (x && x.unsure) { const at = (places().get(r.id) || [])[0]; if (at) toast(`${r.name} → POOL ${at}`); }
      return;
    }
    let n = 0, quiet = false;
    for (const k of pl) {
      let x = await edit("− POOL", (l) => l.remove("pool", k), { quiet: true });
      if (x && x.status === 10) {
        const ok = await confirmBox({ title: `CLEAR POOL ${k}`, what: r.name, go: "CLEAR", color: "--red", undo: "kept",
          sub: `${x.b || 0} step${x.b === 1 ? "" : "s"} use it: they go back to their track's sound.` });
        if (!ok) break;
        x = await edit("− POOL", (l) => l.remove("pool", k, { confirm: true }), { quiet: true });
      }
      if (x && x.status === 0) { n++; poolSet(k, null); } else { quiet = !x; break; }
    }
    pool.forget();
    if (n) toast(`${r.name} out of the pool`, "--muted");
    await refreshRows({ until: (rows) => !rows.some((q) => q.source === "pool" && q.id === r.id && pl.includes(q.slot)) });
    if (quiet && S.link) {                             // (a reply lost: the lists say whether it took)
      if (places().get(r.id)) say("− POOL: the FM-1 did not answer and still has it in the pool. Try again.", "--red");
      else if (!n) toast(`${r.name} out of the pool`, "--muted");
    }
  });
  const del = $("l-del");
  if (del) del.addEventListener("click", async () => {
    const ok = await confirmBox({ title: `DELETE · MY PRESETS ${r.slot}`, what: r.name, go: "DELETE", undo: "kept",
      sub: usedText(users) || (pl.length ? `In the pool at ${pl.join(", ")}: it comes out of it.` : "") });
    if (!ok) return;
    const gone = () => { if (u.dumps) u.dumps.delete(r.slot); S.sel = null; pool.forget(); };
    const x = await editSure("DELETE", (l) => l.remove("mine", r.slot, { confirm: true }), { shown: gone,
      took: (rs) => !rs.some((q) => q.source === "mine" && q.id === r.id) });
    if (x && x.unsure) { gone(); renderLibrary(); }
    if (x && x.status === 0) toast(`Deleted ${r.name}`, "--red");
  });
}
// ---------------------------------------------------------------- IN THIS BROWSER (FIZZ NEON): the browser FIZZ's MY PRESETS
// read from its flash (IndexedDB "browserFlash") without booting it; written only while it is not the unit (a FIZZ
// booted before, now idle, is let go: BROWSER boots the new flash)
const BROWSER_I = '<svg viewBox="0 0 20 20" aria-hidden="true"><rect x="2" y="3.5" width="16" height="11" rx="2"/><path d="M7 17.5h6M10 6.5v5M7.5 9l2.5 2.5L12.5 9"/></svg>';
const SEND_I = '<svg viewBox="0 0 20 20" aria-hidden="true"><path d="M10 3v11M5.5 9.5L10 14l4.5-4.5M4 17h12"/></svg>';
async function loadBrowserLib() {
  const img = await DB.get("browserFlash");
  S.brow.img = img instanceof Uint8Array ? img : img ? new Uint8Array(img) : null;
  S.brow.rows = S.brow.img ? browserRows(S.brow.img) : [];
  if (S.sel && S.sel.source === "browser" && !S.brow.rows.some((r) => r.slot === S.sel.slot)) S.sel = null;
  renderLibrary();
}
async function writeBrowser(fn) {
  if (S.mode === "browser") { say("The browser's FIZZ is the unit now: back to the FM-1 first.", "--orange"); return null; }
  const img = S.brow.img;                                // (an empty one: RESET's mark, a fresh store)
  const base = img ? (img.length ? img : blankImage()) : ((await newestImage().then((i) => i && asHost(i)).catch(() => null))) || blankImage();
  const r = fn(base);
  if (!r || r.nochange) return r;
  if (BU) { clearTimeout(BU.saveT); BU.dispose(); BU = null; log("the browser's FIZZ let go: BROWSER boots its new flash"); }
  await DB.put("browserFlash", r.image);
  S.brow.img = r.image; S.brow.rows = browserRows(r.image);
  ubModels.delete(r.image);
  return r;
}
// the frames of unit rows (MY PRESETS' and the factory's DUMPs; one asked for by itself when missing)
async function framesFor(rows) {
  const u = S.unit;
  if (rows.some((r) => !frameOf(r))) await loadDumps();
  for (const r of rows) {
    if (frameOf(r) || !S.link) continue;
    const f = await S.link.dump(r.source, r.slot).catch(() => null);
    if (f) (u.one || (u.one = new Map())).set(`${r.source}:${r.slot}:${r.id}`, f);
  }
  return rows.filter((r) => frameOf(r));
}
function recOfRow(r) {
  const f = frameOf(r), d = f && parseDump(f);
  if (!d) return null;
  return recordOf({ name: d.name, blob: d.blob.length === 128 ? d.blob : blob6(soundFromFrame(f)), pan: d.pan, chorus: d.chorus, delay: d.delay, reverb: d.reverb, tags: r.tags });
}
async function keepInBrowser(rows) {
  const recs = (await framesFor(rows)).map(recOfRow).filter(Boolean);
  if (!recs.length) return null;
  const r = await writeBrowser((img) => { const k = keepIn(img, recs); return k.added ? k : { ...k, nochange: true }; });   // (nothing new: nothing written)
  if (r) {
    const at = r.at.length > 1 ? ` · ${Math.min(...r.at)}–${Math.max(...r.at)}` : r.at.length ? ` · ${r.at[0]}` : "";
    toast(`${r.added ? `${r.added} KEPT IN THIS BROWSER${at}` : "IN THIS BROWSER ALREADY"}${r.added && r.same ? ` · ${r.same} already there` : ""}${r.full ? ` · ${r.full} DID NOT FIT` : ""}`, "--orange");
    if (S.pick.keys.size) clearPick();
  }
  renderLibrary();
  return r;
}
// the browser's presets onto the unit: MY PRESETS (the first free places), the pool (its first empty places: a MY
// preset referenced there) as one batch (one undo level); TRACK: one heard on the track (KEEP / CANCEL)
async function sendToUnit(rows, to = "mine") {
  if (!S.link || S.busy || !S.unit || !S.unit.status || !rows.length) return 0;
  if (to === "track") { previewLib(rows[0].lib, rows[0].name); return 1; }
  let n = 0, full = false;
  const got = [];
  for (const [i, r] of rows.entries()) {
    say(`SEND ${i + 1} / ${rows.length}`, "--violet");
    const s = r.lib;
    const x = await edit("SEND", (l) => l.store(storeSound({ name: s.name, blob: s.blob, drive: s.drive, pan: s.pan, chorus: s.sends.chorus, delay: s.sends.delay,
      reverb: s.sends.reverb, tags: r.tags, pool: to === "pool", slot: 0, batch: i > 0 })));
    if (x && x.kind === "store" && x.status === 0) { n++; got.push(x.slot); }
    else { full = !!(x && x.status === 1); break; }
  }
  say("");
  const span = got.length ? ` · ${to === "pool" ? "POOL" : "MY PRESETS"} ${got.length > 1 ? `${Math.min(...got)}–${Math.max(...got)}` : got[0]}` : "";
  toast(`${n} SENT TO FM-1${span}${full ? " · FULL" : ""}`, full ? "--orange" : "--violet");
  S.unit.dumps = null;
  pool.forget();
  clearPick();
  if (to === "mine" && got.length) {                    // (the view jumps to them: lit with NEW for a moment)
    S.filter.src = "mine"; S.filter.q = ""; $("l-q").value = "";
    S.fresh = new Set(got.map((k) => `mine:${k}`));
    clearTimeout(sendToUnit.t);
    sendToUnit.t = setTimeout(() => { S.fresh.clear(); renderLibrary(); }, 6000);
  }
  await refreshRows();
  await loadDumps();
  const first = list.querySelector(".cas.new");
  if (first) first.scrollIntoView({ block: "nearest", behavior: "smooth" });
  return n;
}
// (fizz-web-offline 01) the browser's FIZZ as the unit, nothing picked: THIS BROWSER, its numbers, its store, BACK UP, IMPORT
function thisBrowserCard(el) {
  const u = S.unit, st = u.status, mine = u.rows.filter((r) => r.source === "mine").length;
  const tot = st && st.lib[1] + st.prj[1], use = tot ? (st.lib[0] + st.prj[0]) / tot : 0;
  const p = tot ? Math.ceil(st.lib[0] / tot * 32) : 0, j = tot ? Math.ceil(st.prj[0] / tot * 32) : 0;
  const ver = u.version ? u.version.replace(/-\d+-g[0-9a-f]+$/, "").replace(/-(beta|rc|alpha)\.?(\d+)$/i, "") : "";
  el.style.setProperty("--c", "var(--browser)");
  el.innerHTML = `<div class="thisb">${BROWSER_BIG.replace('class="dglyph"', 'class="dglyph tbg"')}<div class="tbn num">THIS BROWSER</div><div class="hud tbv">FIZZ ${esc(ver)}</div></div>
    <div class="tbstats"><div><span class="num">${mine}</span><span class="lbl">presets</span></div><div><span class="num">${st ? st.projects : "–"}</span><span class="lbl">projects</span></div>
      <div><span class="num">${tot ? Math.max(use > 0 ? 1 : 0, Math.round(use * 100)) : "–"}<small>%</small></span><span class="lbl">store</span></div></div>
    <span class="cells tbcells">${Array.from({ length: 32 }, (_, i) => `<i class="${i < p ? "p" : i >= 32 - j ? "j" : ""}"></i>`).join("")}</span>
    <div class="legend"><span><i class="lp"></i>MY PRESETS</span><span><i class="lj"></i>PROJECTS</span></div>
    <div class="selgo"><button class="ol" id="tb-backup" style="--c:var(--yellow)">BACK UP</button><button class="ol" id="tb-import" style="--c:var(--violet)">IMPORT</button></div>`;
  $("tb-backup").onclick = () => tab("backup");
  $("tb-import").onclick = () => tab("import");
}
// the panel of a browser preset: SEND TO FM-1, EXPORT, DELETE (from the browser's store), USED BY there
function renderBrowserSel() {
  const el = $("l-sel"), r = S.brow.rows.find((x) => x.slot === S.sel.slot);
  if (!r || !browserShown()) { S.sel = null; renderSel(); return; }
  const c = colOf(r.tags), tg = tagsOf(r.tags), mi = isMidiBlob(r.blob), s = r.lib;
  const can = !!(S.link && !S.busy && S.unit && S.unit.status);
  const ring = mi ? MIDI_I.replace('class="din"', 'class="din algo"') : `<span class="hud">${Math.min(8, r.blob[2] + 1)}</span>`;
  el.style.setProperty("--c", c);
  el.innerHTML = `<div class="selhead"><div class="selring" aria-hidden="true"><span class="halo"></span><span class="core">${ring}</span></div>
      <div class="selid"><div class="selsrc hud">${midiChip(mi)}IN THIS BROWSER · ${String(r.slot).padStart(2, "0")}</div><div class="big">${esc(r.name)}</div>
      <div class="selchips">${tg[0] ? `<span class="tag t-${tg[0]}">${tg[0]}</span>` : ""}</div></div></div>
    <div class="selgo">
      <button class="ol wide" id="l-send" style="--c:var(--cream)" ${can ? "" : "disabled"}>${SEND_I}SEND TO FM-1</button>
      <button class="ol" id="l-bexport" style="--c:var(--violet)"><svg viewBox="0 0 20 20" aria-hidden="true"><path d="M10 13V3M6 7l4-4 4 4M4 12v5h12v-5"/></svg>EXPORT</button>
      <button class="ol redline" id="l-bdel" style="--c:var(--red)">DELETE</button></div>
    ${usedStrips(r.id, true)}
    <div class="selvals"><div class="kv"><b>${s.pan > 0 ? "R" + s.pan : s.pan < 0 ? "L" + -s.pan : "C"}</b>pan</div><div class="kv"><b>${s.sends.chorus}</b>chorus</div><div class="kv"><b>${s.sends.delay}</b>delay</div><div class="kv"><b>${s.sends.reverb}</b>reverb</div><div class="kv"><b>${s.drive}</b>drive</div></div>`;
  $("l-send").onclick = () => sendToUnit([r], "mine");
  $("l-bexport").onclick = () => exporter.open([{ ...s, source: { fizz: "", from: `IN THIS BROWSER ${r.slot}` } }], { title: r.name, tags: tg });
  $("l-bdel").onclick = () => dropBrowser([r]);
}
async function dropBrowser(rows) {
  const ok = await confirmBox({ title: rows.length === 1 ? `DELETE · IN THIS BROWSER ${rows[0].slot}` : `DELETE ${rows.length} PRESETS`,
    what: rows.length === 1 ? rows[0].name : `${rows.length} IN THIS BROWSER`, go: "DELETE", sub: "From the browser's FIZZ." });
  if (!ok) return false;
  const r = await writeBrowser((img) => ({ image: dropFrom(img, rows.map((x) => x.slot)) }));
  if (!r) return false;
  toast(`Deleted ${rows.length === 1 ? rows[0].name : rows.length} · IN THIS BROWSER`, "--red");
  if (S.sel && S.sel.source === "browser") S.sel = null;
  for (const x of rows) S.pick.keys.delete(pkey(x));
  renderLibrary();
  return true;
}

// ---------------------------------------------------------------- SELECT (FIZZ NEON): many presets at once
// SELECT (or shift / cmd / ctrl + a click) picks several: shift a range in the order shown, cmd / ctrl one more. The
// card (the panel's place on a computer, a card at the foot on a phone) acts on them all: TAG, + POOL, − POOL, DELETE,
// EXPORT; KEEP n IN BROWSER (the FM-1's) or SEND n TO FM-1 (the browser's, to MY PRESETS, the pool or the track). A
// selection holds one side's rows: the unit's, or the browser's. The edits one by one (the protocol has a batch for
// STORE only), one progress line, the lists read once at the end.
const sideOf = (k) => (k.startsWith("browser:") ? "b" : "u");
function rowOfKey(k) {
  const [src, slot] = k.split(":");
  return src === "browser" ? S.brow.rows.find((r) => r.slot === +slot) : S.unit && S.unit.rows.find((r) => r.source === src && r.slot === +slot);
}
const pickedRows = () => [...S.pick.keys].map(rowOfKey).filter(Boolean);
function clearPick() { const P = S.pick; P.mode = false; P.keys.clear(); P.anchor = ""; P.tags = false; }
function pickClick(b, ev) {
  const P = S.pick, k = `${b.dataset.src}:${b.dataset.slot}`;
  if (P.keys.size && sideOf([...P.keys][0]) !== sideOf(k)) P.keys.clear();
  if (ev.shiftKey) {
    const cards = [...list.querySelectorAll(".cas")].map((c) => `${c.dataset.src}:${c.dataset.slot}`);
    const from = P.anchor && cards.includes(P.anchor) ? P.anchor : S.sel ? `${S.sel.source}:${S.sel.slot}` : k;
    let a = cards.indexOf(from);
    const z = cards.indexOf(k);
    if (a < 0) a = z;
    for (let i = Math.min(a, z); i <= Math.max(a, z); i++) if (sideOf(cards[i]) === sideOf(k)) P.keys.add(cards[i]);
  } else {
    if (P.keys.has(k)) P.keys.delete(k); else P.keys.add(k);
    P.anchor = k;
  }
  renderLibrary();
}
const TRASH_I = '<svg viewBox="0 0 20 20" aria-hidden="true"><path d="M4 6h12M8 6V4h4v2M6 6l1 11h6l1-11"/></svg>';
const EXPORT_I = '<svg viewBox="0 0 20 20" aria-hidden="true"><path d="M10 13V3M6 7l4-4 4 4M4 12v5h12v-5"/></svg>';
// (the boards' browser glyph: a window with FIZZ's bubbles)
const BROWSER_G = '<svg class="bglyph" viewBox="0 0 24 24" aria-hidden="true"><rect x="2.5" y="4" width="19" height="16" rx="3"/><path d="M2.5 8.5h19"/><circle cx="9.5" cy="15" r="2.6"/><circle cx="14.6" cy="12.6" r="1.6"/><circle cx="15.2" cy="17" r="1.1"/></svg>';
const FM1_G = '<svg class="dglyph" viewBox="0 0 76 50" aria-hidden="true"><rect x="1.5" y="1.5" width="73" height="47" rx="4"/><rect x="7" y="6" width="17" height="13" rx="1.5"/><circle cx="34" cy="10" r="3.4"/><circle cx="44" cy="10" r="3.4"/><circle cx="54" cy="10" r="3.4"/><circle cx="64" cy="10" r="3.4"/>' +
  Array.from({ length: 16 }, (_, i) => `<rect x="${6 + i * 4.1}" y="33" width="3.2" height="10" rx=".8"/>`).join("") + "</svg>";
const BROWSER_BIG = '<svg class="dglyph" viewBox="0 0 76 50" aria-hidden="true"><rect x="2" y="2" width="72" height="46" rx="7"/><path d="M2 12h72"/><circle cx="31" cy="32" r="8" style="stroke:var(--cyan)"/><circle cx="43" cy="23" r="5.5" style="stroke:var(--pink)"/><circle cx="45" cy="38" r="3.6" style="stroke:var(--yellow)"/><path d="M56 22h11M56 28h8M56 34h11M56 40h6"/></svg>';
// the first free places of a list of used ones (1..max)
const freeOf = (used, max, n) => { const u = new Set(used), out = []; for (let k = 1; k <= max && out.length < n; k++) if (!u.has(k)) out.push(k); return out; };
// a store as 32 cells: what is used, what comes now (lit), the rest
const cells = (used, add, max) => { const a = Math.round(used / max * 32), b = Math.max(add ? 1 : 0, Math.round((used + add) / max * 32) - a);
  return `<span class="cells">${Array.from({ length: 32 }, (_, i) => `<i class="${i < a ? "p" : i < a + b ? "n" : ""}"></i>`).join("")}</span>`; };
function renderBulk() {
  const el = $("l-bulk"), P = S.pick, u = S.unit, on = !!u && (P.mode || P.keys.size > 0);
  el.hidden = !on;
  $("l-select").innerHTML = on ? '<svg viewBox="0 0 16 16" aria-hidden="true"><path d="M3.5 8.5l3 3 6-7"/></svg>DONE' : "SELECT";
  $("l-select").setAttribute("aria-pressed", String(on));
  $("l-select").disabled = !u;
  if (!on) return;
  const rows = pickedRows(), n = rows.length, brow = n > 0 && rows[0].source === "browser";
  const can = !!(S.link && !S.busy && u.status), pl = places();
  const mine = rows.filter((r) => r.source === "mine"), unit = rows.filter((r) => r.source !== "browser" && r.id);
  const inP = unit.filter((r) => (pl.get(r.id) || []).length), outP = unit.filter((r) => !(pl.get(r.id) || []).length);
  const ndel = brow ? n : mine.length;
  // where each goes: SEND to MY PRESETS' or the pool's first free places, the track; KEEP to the browser's
  const pk = unitPkg(), mineUsed = u.rows.filter((r) => r.source === "mine").map((r) => r.slot), poolUsed = [...new Set(u.rows.filter((r) => r.source === "pool").map((r) => r.slot))];
  const tn = live.state.t >= 0 ? live.state.t + 1 : "";
  const okTo = { mine: true, pool: true, track: n === 1 && live.on() };
  if (brow && !okTo[P.to]) P.to = "mine";
  const to = !brow ? freeOf(S.brow.rows.map((r) => r.slot), 256, n) : P.to === "pool" ? freeOf(poolUsed, 128, n) : P.to === "track" ? [`T${tn}`] : freeOf(mineUsed, pk.nlib, n);
  const c = brow ? "var(--send)" : "var(--browser)";
  el.style.setProperty("--c", c);
  el.classList.toggle("keep", !brow);
  const MAXL = 5;
  const mini = `<div class="bklist">${rows.slice(0, MAXL).map((r, i) => {
    const d = r.source === "browser" ? r.blob : (parseDump(frameOf(r) || []) || {}).blob;
    return `<div class="bkrow" style="--c:${colOf(r.tags)}"><span class="ring"><span class="hud">${d ? Math.min(8, d[2] + 1) : ""}</span></span><b>${esc(r.name)}</b><span class="hud to">${to[i] !== undefined ? (typeof to[i] === "number" ? String(to[i]).padStart(2, "0") : to[i]) : "–"}</span></div>`;
  }).join("")}${n > MAXL ? `<span class="hud bkmore">+${n - MAXL}</span>` : ""}</div>`;
  const rings = `<span class="bkrings">${rows.slice(0, 4).map((r) => `<i style="--c:${colOf(r.tags)}"></i>`).join("")}</span>`;
  const head = `<div class="bkhead"><span class="num bkn">${n}</span>${rings}<span class="bkic">
    <button class="ol" id="lb-all" aria-label="Select all shown">ALL</button>
    <button class="ol ibtn" id="lb-export" style="--c:var(--violet)" aria-label="Export ${n}" title="Export" ${n ? "" : "disabled"}>${EXPORT_I}</button>
    <button class="ol ibtn red" id="lb-del" style="--c:var(--red)" aria-label="Delete ${ndel}" title="Delete" ${ndel && (brow || can) ? "" : "disabled"}>${TRASH_I}</button></span></div>`;
  let mid = "", dest, go;
  if (brow) {
    mid = `<div class="seg bkto" role="group" aria-label="Send to">${[["mine", "MY PRESETS"], ["pool", "POOL"], ["track", `TRACK${tn ? ` <b class="hud">${tn}</b>` : ""}`]].map(([k, t]) =>
      `<button data-to="${k}" class="${P.to === k ? "on" : ""}" aria-pressed="${P.to === k}" ${okTo[k] ? "" : "disabled"}>${t}</button>`).join("")}</div>`;
    const add = P.to === "track" ? 0 : n;
    dest = P.to === "pool" ? `POOL · ${poolUsed.length + add}/128` : `FM-1 · ${mineUsed.length + (P.to === "mine" ? n : 0)}/${pk.nlib}`;
    dest = `<div class="bkdest">${FM1_G}<span class="bkdl"><span class="hud">${dest}</span>${P.to === "pool" ? cells(poolUsed.length, add, 128) : cells(mineUsed.length, P.to === "mine" ? n : 0, pk.nlib)}</span></div>`;
    go = `<button class="arc breathe bkgo" id="lb-go" ${n && can ? "" : "disabled"}>${SEND_I}SEND ${n} TO FM-1</button>`;
  } else {
    mid = `<div class="bktools">
      <button class="ol" id="lb-tag" aria-expanded="${P.tags}" ${unit.length && can ? "" : "disabled"}>TAG ▾</button>
      <button class="ol" id="lb-padd" style="--c:var(--green)" ${outP.length && can ? "" : "disabled"}>+ POOL${outP.length ? ` <b class="hud">${outP.length}</b>` : ""}</button>
      <button class="ol" id="lb-prem" style="--c:var(--green)" ${inP.length && can ? "" : "disabled"}>− POOL${inP.length ? ` <b class="hud">${inP.length}</b>` : ""}</button></div>
      ${P.tags ? `<div class="bktags">${FIZZ_TAGS.map((t, k) => `<button class="tag t-${t}" data-btag="${k}">${t}</button>`).join("")}<button class="tag" data-btag="-1">NONE</button></div>` : ""}`;
    const nb = S.brow.rows.length;
    dest = browserShown() ? `<div class="bkdest">${BROWSER_BIG}<span class="bkdl"><span class="hud">THIS BROWSER · ${nb + n}/256</span>${cells(nb, n, 256)}</span></div>` : "";
    go = browserShown() ? `<button class="arc breathe bkgo" id="lb-go" ${n ? "" : "disabled"}>${BROWSER_G}KEEP ${n} IN BROWSER</button>` : "";
  }
  el.innerHTML = head + (n ? mini : "") + mid + dest + go;
}
$("l-select").addEventListener("click", () => {
  const P = S.pick;
  if (P.mode || P.keys.size) clearPick(); else P.mode = true;
  renderLibrary();
});
$("l-bulk").addEventListener("click", async (ev) => {
  const b = ev.target.closest("button");
  if (!b || b.disabled) return;
  const P = S.pick, rows = pickedRows(), brow = rows.length > 0 && rows[0].source === "browser";
  if (b.id === "lb-all") {
    const side = P.keys.size ? sideOf([...P.keys][0]) : S.filter.src === "browser" ? "b" : "u";
    list.querySelectorAll(".cas").forEach((c) => { const k = `${c.dataset.src}:${c.dataset.slot}`; if (sideOf(k) === side) P.keys.add(k); });
    renderLibrary();
    return;
  }
  if (b.dataset.to) { P.to = b.dataset.to; renderBulk(); return; }
  if (b.id === "lb-tag") { P.tags = !P.tags; renderBulk(); return; }
  if (b.id === "lb-export") { bulkExport(rows); return; }
  if (b.id === "lb-del") { if (brow) dropBrowser(rows); else bulkDelete(rows); return; }
  if (b.id === "lb-go") { if (brow) sendToUnit(rows, P.to); else keepInBrowser(rows.filter((r) => r.source !== "browser")); return; }
  if (b.dataset.btag !== undefined) { P.tags = false; bulkTag(rows, +b.dataset.btag < 0 ? 0 : 1 << +b.dataset.btag); return; }
  if (b.id === "lb-padd") bulkPool(rows, true);
  else if (b.id === "lb-prem") bulkPool(rows, false);
});
// one by one with a progress line; null from fn stops
async function bulkRun(label, rows, fn) {
  let n = 0;
  for (const [i, r] of rows.entries()) {
    say(`${label} ${i + 1} / ${rows.length}`, "--cream");
    const ok = await fn(r, i);
    if (ok === null) break;
    if (ok) n++;
  }
  say("");
  return n;
}
async function bulkExport(rows) {
  const unit = await framesFor(rows.filter((r) => r.source !== "browser"));
  const sounds = [...unit.map((r) => ({ ...soundFromStoreFrame(frameOf(r)), source: { fizz: S.unit.version || "", from: `${r.source === "mine" ? "MY PRESETS" : "FACTORY"} ${r.slot}` } })),
    ...rows.filter((r) => r.source === "browser").map((r) => ({ ...r.lib, source: { fizz: "", from: `IN THIS BROWSER ${r.slot}` } }))];
  if (sounds.length) exporter.open(sounds, { title: `${sounds.length} SOUND${sounds.length === 1 ? "" : "S"}`, pick: "fizz" });
}
async function bulkTag(rows, mask) {
  const unit = rows.filter((r) => r.source !== "browser" && r.tags !== mask);
  const n = await bulkRun("TAG", unit, async (r) => { const x = await edit("TAGS", (l) => l.tags(r.source, r.slot, mask)); if (!x || x.status !== 0) return null; r.tags = mask; return true; });
  toast(`${n} tagged ${mask ? FIZZ_TAGS[Math.log2(mask)] : "NONE"}`, "--cream");
  S.unit.dumps = null; S.unit.fdumps = null; S.unit.one = null;
  await refreshRows();
  loadDumps();
}
// + POOL: the first empty places of the project; − POOL: every place holding them (the used ones asked once)
async function bulkPool(rows, add) {
  const pl = places(), unit = rows.filter((r) => r.source !== "browser" && r.id);
  if (add) {
    const n = await bulkRun("+ POOL", unit.filter((r) => !(pl.get(r.id) || []).length), async (r) => {
      const x = await edit("+ POOL", (l) => l.poolRef(0, r.id));
      return x && x.status === 0 ? true : null;
    });
    toast(`${n} → POOL`, "--green");
  } else {
    const used = [];
    const ks = unit.flatMap((r) => (pl.get(r.id) || []).map((k) => ({ r, k })));
    let n = await bulkRun("− POOL", ks, async ({ r, k }) => {
      const x = await edit("− POOL", (l) => l.remove("pool", k));
      if (x && x.status === 10) { used.push({ r, k, steps: x.b || 0 }); return false; }
      return x && x.status === 0 ? true : null;
    });
    if (used.length) {
      const steps = used.reduce((a, x) => a + x.steps, 0);
      const ok = await confirmBox({ title: `CLEAR ${used.length} POOL PLACE${used.length === 1 ? "" : "S"}`, what: used.length <= 3 ? used.map((x) => x.r.name).join(", ") : `${used.length} PLACES`,
        go: "CLEAR", color: "--red", undo: "kept", sub: `${steps} step${steps === 1 ? "" : "s"} use them: they go back to their track's sound.` });
      if (ok) n += await bulkRun("− POOL", used, async ({ k }) => { const x = await edit("− POOL", (l) => l.remove("pool", k, { confirm: true })); return x && x.status === 0 ? true : null; });
    }
    if (n) toast(`${n} out of the pool`, "--muted");
  }
  pool.forget();
  await refreshRows();
}
// DELETE: MY PRESETS only (the factory's stay); asked once, naming the count and the ones a project uses
async function bulkDelete(rows) {
  const mine = rows.filter((r) => r.source === "mine");
  if (!mine.length || !S.link) return;
  const used = [], um = usedModel();
  for (const r of mine) {
    if (um && hasId(um.model, r.id) && usedBy(um.model, r.id, { max: 0 }).total) { used.push(r); continue; }   // (USED BY's store: a track's own sound too)
    let x = S.users.get(`mine:${r.id}`);
    if (x === undefined || x === null) { x = await S.link.users("mine", r.slot).catch(() => null); S.users.set(`mine:${r.id}`, x && x.projects ? x : null); }
    if (x && x.projects && x.projects.length) used.push(r);
  }
  const ok = await confirmBox({ title: `DELETE ${mine.length} PRESET${mine.length === 1 ? "" : "S"}`, what: `${mine.length} MY PRESETS`, go: "DELETE",
    sub: (used.length ? `<b style="color:var(--yellow)">${used.length} used by projects${used.length <= 3 ? `: ${used.map((r) => esc(r.name)).join(", ")}` : ""}</b>: those steps go back to their track's sound.` : "") +
      (rows.length > mine.length ? `${used.length ? " " : ""}The factory's stay.` : "") });
  if (!ok) return;
  const u = S.unit;
  const n = await bulkRun("DELETE", [...mine].sort((a, b) => b.slot - a.slot), async (r) => {
    const x = await edit("DELETE", (l) => l.remove("mine", r.slot, { confirm: true }));
    if (!x || x.status !== 0) return null;
    if (u.dumps) u.dumps.delete(r.slot);
    S.pick.keys.delete(pkey(r));
    if (S.sel && S.sel.source === "mine" && S.sel.slot === r.slot) S.sel = null;
    return true;
  });
  toast(`Deleted ${n}`, "--red");
  pool.forget();
  await refreshRows();
}

// ---------------------------------------------------------------- find a sound; the source as a menu (a phone)
const qIn = $("l-q");
qIn.addEventListener("input", () => { S.filter.q = qIn.value.trim().toLowerCase(); renderLibrary(); });
qIn.addEventListener("keydown", (ev) => { if (ev.key === "Escape") { ev.preventDefault(); ev.stopPropagation(); qIn.value = ""; S.filter.q = ""; renderLibrary(); qIn.blur(); } });
const typingIn = (t) => t && (t.tagName === "INPUT" || t.tagName === "TEXTAREA" || t.tagName === "SELECT" || t.isContentEditable);
document.addEventListener("keydown", (ev) => {
  if ($("tab-library").hidden || !$("cf").hidden || !$("rs").hidden || typingIn(ev.target) || ev.metaKey || ev.ctrlKey || ev.altKey) return;
  if (ev.key === "/") { ev.preventDefault(); qIn.focus(); qIn.select(); }
  else if (ev.key === "Escape" && !$("l-srcpop").hidden) srcMenu(false);
  else if (ev.key === "Escape" && (S.pick.mode || S.pick.keys.size)) { clearPick(); renderLibrary(); }
});
function srcMenu(open) {
  const pop = $("l-srcpop");
  pop.hidden = !open;
  $("l-src").setAttribute("aria-expanded", String(open));
  if (!open) return;
  const srcs = [["mine", "MY PRESETS"], ["factory", "FACTORY"], ...(browserShown() ? [["browser", "IN THIS BROWSER"]] : []), ["all", "ALL"]];
  pop.innerHTML = srcs.map(([k, t]) => `<button role="menuitemradio" data-s="${k}" aria-checked="${S.filter.src === k}" class="${S.filter.src === k ? "on" : ""}">${t}</button>`).join("");
}
$("l-src").addEventListener("click", (ev) => { ev.stopPropagation(); srcMenu($("l-srcpop").hidden); });
$("l-srcpop").addEventListener("click", (ev) => {
  const b = ev.target.closest("[data-s]");
  if (!b) return;
  S.filter.src = b.dataset.s;
  srcMenu(false);
  renderLibrary();
});
document.addEventListener("click", (ev) => { if (!$("l-srcpop").hidden && !ev.target.closest(".srcmenu")) srcMenu(false); });

$("l-filter").addEventListener("click", (ev) => { if (filterClick(S.filter, ev)) renderLibrary(); });
const list = $("l-list");
list.addEventListener("click", (ev) => {
  const b = ev.target.closest(".cas");
  if (!b) return;
  if (ev.shiftKey || ev.metaKey || ev.ctrlKey || S.pick.mode) { pickClick(b, ev); return; }
  if (S.pick.keys.size) clearPick();
  S.sel = { source: b.dataset.src, slot: +b.dataset.slot };
  renderLibrary();
  if (S.sel.source === "browser") { const r = S.brow.rows.find((x) => x.slot === S.sel.slot); if (r) previewLib(r.lib, r.name); }
  else previewRow(S.unit.rows.find((x) => x.source === S.sel.source && x.slot === S.sel.slot));   // (TRACK on: on the track, KEEP / CANCEL)
  if (matchMedia("(max-width: 900px) and (orientation: portrait)").matches) $("l-sel").scrollIntoView({ behavior: "smooth", block: "start" });   // (a phone: the panel is below the list)
});
// reorder MY PRESETS: a cassette dropped on another swaps their places (the ids go with them: nothing else changes);
// ALT + up / down moves the selected one a place (into an empty place too)
async function swapMine(a, b) {
  if (a === b || b < 1 || b > unitPkg().nlib) return;
  const id = (s) => { const q = S.unit.rows.find((x) => x.source === "mine" && x.slot === s); return q ? q.id : 0; };
  const ia = id(a), ib = id(b);
  const follow = () => { if (S.sel && S.sel.source === "mine") S.sel.slot = S.sel.slot === a ? b : S.sel.slot === b ? a : S.sel.slot; };
  const x = await editSure("MOVE", (l) => l.swap("mine", a, b), { shown: follow, took: () => id(a) === ib && id(b) === ia });
  if (!x || x.status !== 0) return;
  if (x.unsure) follow();
  await redump(a);
  await redump(b);
  renderLibrary();
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
const ctx = { S, $, log, say, toast, edit, editSure, refreshRows, loadDumps, frameOf, projectName, saveWeb, poolSet, render: () => { renderUnit(); renderLibrary(); },
  pkg: () => unitPkg(), viaBle, NEEDS_USB,
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
// (the mix: PARAM 115..118 onto its track 1; a FIZZ that refuses them: the whole sound, as every other value)
const browserTarget = (() => {
  let want = null, going = false, mixOk = true, last = null;
  const mixQ = new Map();
  async function flush() {
    if (going) return;
    going = true;
    while (want || mixQ.size) {
      if (mixQ.size) {
        const [id, v] = mixQ.entries().next().value;
        mixQ.delete(id);
        const r = await bfizz.ask(paramRequest({ track: 0, id, value: v, ack: true }), (x) => (x.kind === "ack" || x.kind === "error") && x.command === CMD.PARAM, 1000);
        if (!r || r.status) { mixOk = false; mixQ.clear(); if (last) want = copySound(last); log("browser FIZZ: no mix PARAM, the sound is sent instead"); }
        continue;
      }
      const s = want;
      want = null;
      await bfizz.ask(storeFrameOf(s, { slot: 256, track: 0 }), (x) => x.kind === "store", 2000);
    }
    going = false;
  }
  const push = (s) => { if (!bfizz.ready) return; want = copySound(s); flush(); };
  const param = (id, v, s) => {
    if (!isMix(id) || !mixOk || !bfizz.ready) { push(s); return; }
    last = s;
    mixQ.set(id, v);
    flush();
  };
  return {
    live: false,
    label: () => (bfizz.ready ? "THIS BROWSER · TRACK 1" : "▶ PLAY IN THIS BROWSER"),
    ready: () => bfizz.ready,
    start: () => bfizz.start(),
    audition: (s) => push(s),
    param,
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
live = mountLive({ S, $, log, say, toast, editor, browserTarget, changed: () => { importer.render(); if (!$("tab-library").hidden) renderSel(); keys.render(); },
  kept: () => { S.users.clear(); pool.forget(); renderLibrary(); pool.render(); } });   // (KEEP, CANCEL: who uses a preset changed)
// (FIZZ NEON) the keys' strip: what they play and where (keysTarget's rules: the track with TRACK on, else this browser)
keys = mountKeys({ $, editor, info: () => {
  if (live.on()) { const L = live.state, t = L.pending ? L.pending.t : L.t; return { name: L.pending ? L.pending.name : "", where: t >= 0 ? `on track ${t + 1}` : "on the track" }; }
  return { name: PV.sound ? PV.sound.name : "", where: "in this browser" };
} });
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
  const had = new Set(S.unit.rows.filter((q) => q.source === "mine").map((q) => q.id));
  let x = await edit("SAVE", (l) => l.store(storeFrameOf(E.s, { slot, own: true, ...live.saveOpts() })), { quiet: true });
  if (!x && S.link) {                                  // (its reply lost: the lists say whether it was stored)
    await refreshRows();
    const got = asNew && S.unit.rows.find((q) => q.source === "mine" && !had.has(q.id) && q.name === E.s.name.trim());
    if (!got) { say(asNew ? "SAVE AS NEW: the FM-1 did not answer and does not show it. Try again." : "SAVE: the FM-1 did not answer: SAVE again to be sure.", "--red"); return null; }
    log("SAVE AS NEW: its reply lost, the FM-1 has it");
    x = { kind: "store", status: 0, slot: got.slot };
  }
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
  if (name === "pool") pool.show(); else pool.hide();   // (a move asked: cancelled when POOL is left)
  if (name === "import") importer.render();
  if (name === "edit") { editor.show(); live.show(); if (bfizz.ready && editor.sound() && !live.on()) browserTarget.audition(editor.sound()); } else live.hide();
  if (name === "projects") projtab.show();
  keys.show(KEY_TABS.includes(name));
  if (name !== "library") srcMenu(false);
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
  keys.render();
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

// ---------------------------------------------------------------- FIZZ in this browser as the unit (the owner, 2026-10-27, 2026-10-29)
// THIS BROWSER: the browser's own FIZZ (browserunit.js) takes the FM-1's place, heard on the computer: every tab works on
// it. It starts from the store it had last time in this browser, else the newest full backup kept here, else blank;
// TRACK is on (nothing to protect). With no FM-1 the page opens on it; before the page's first click it runs quietly
// (the autoplay rule), and that click starts its sound (the speaker pill then mutes it).
let BU = null, booting = false, muted = false, wokeAt = 0;
const gestured = () => !!(navigator.userActivation && navigator.userActivation.hasBeenActive);
const short = (v) => String(v || "").replace(/^FIZZ\s*/i, "").replace(/-\d+-g[0-9a-f]+$/, "");
async function newestImage() {
  const b = S.baks.find((x) => x.kind === "full" && x.layout !== "host");
  if (!b) return null;
  try { return (await readBackup(b.bytes, b.name)).image; } catch (_) { return null; }
}
function dropLink() {
  if (S.link) { try { S.link.close(); } catch (_) { /* gone */ } S.link = null; live.lost(); }
  if (S.mode === "browser" && BU) { BU.unplug(); if (BU.player.ctx) BU.player.ctx.suspend().catch(() => {}); }
  S.access = null;
}
async function useBrowser() {
  if (S.busy || attaching || booting) return;
  const was = S.mode === "fm1" && S.link && S.unit && S.unit.version ? { id: S.unit.id, port: S.unit.port, version: S.unit.version } : null;
  dropLink();
  S.mode = "browser";
  FM.found = was;                                         // (the FM-1 just left stays on offer)
  booting = true;
  unitUi.render();
  try {
    if (!BU) { BU = new BrowserUnit({ log, onFlash: (b) => { DB.put("browserFlash", b); S.brow.img = b; S.brow.rows = browserRows(b); } }); BU.setVolume(muted ? 0 : 1); }
    if (gestured()) await BU.start().catch(() => false);
    if (!BU.engine) {
      const kept = await DB.get("browserFlash");      // (an empty one: RESET, a fresh store)
      await BU.boot(kept ? (kept.length ? kept : null) : await newestImage());
    } else await BU.replug();
    if (gestured() && BU.quiet) await BU.wake();       // (a click while it booted)
  } catch (e) { say(esc(e.message || String(e)), "--orange"); log(`browser FIZZ: ${e.message || e}`); }
  booting = false;
  if (BU && BU.engine) {
    S.access = BU.access;
    await attach();
    if (S.link && (await live.probe()) && !live.on()) await live.setOn(true);
  }
  unitUi.render();
  if (FM.access && !FM.found) watchFm1();
}
function leaveBrowser() {
  if (S.busy) return;
  dropLink();
  S.mode = "fm1";
  S.unit = null;
  renderUnit(); renderLibrary(); renderBaks(); pool.render(); importer.render(); projtab.render();
  loadBrowserLib();
  unitUi.render();
}
// the picker's rows, ⇄ FM-1: never while a write runs
function pick(u) {
  if (u === S.mode && (S.link || attaching || booting)) return;
  if (S.busy || attaching || booting) { toast("Wait: a write is still going", "--orange"); return; }
  if (u === "fm1") connect(); else useBrowser();
}
// the sound: the page's first click starts it (FIZZ moved into the audio worklet); the pill then mutes it
function wake(ev) {
  if (S.mode !== "browser" || !BU || !BU.engine || BU.running || BU.waking || booting) return;
  if (ev && ev.target && ev.target.closest && ev.target.closest("#fm1, #up-connect, .urow[data-u='fm1']")) return;   // (on the way to the FM-1)
  wokeAt = performance.now();
  BU.wake().then(() => unitUi.render(), () => unitUi.render());
  unitUi.render();
}
document.addEventListener("pointerdown", wake, true);
document.addEventListener("keydown", (ev) => { if (ev.key !== "Escape") wake(ev); }, true);
function soundClick() {
  if (S.mode !== "browser" || !BU) return;
  if (!BU.running || performance.now() - wokeAt < 700) { wake(); return; }   // (the click that started it)
  muted = !muted;
  BU.setVolume(muted ? 0 : 1);
  unitUi.render();
}
// the browser FIZZ's store: its STATUS when it is the unit, else its flash as kept here
function memView() {
  const st = S.mode === "browser" && S.unit && S.link ? S.unit.status : FM.bstat;
  if (st && st.lib && st.lib[1]) return { lib: st.lib, prj: st.prj, all: (st.lib[0] + st.prj[0]) / (st.lib[1] + st.prj[1]) };
  return FM.bmem || null;
}
async function readMem() {
  try { const f = await DB.get("browserFlash"); FM.bmem = f && f.length ? readRegion(f).used : null; } catch (_) { FM.bmem = null; }
  unitUi.render();
}
// RESET (the picker's MEMORY): asked once; MY PRESETS there are kept in BACKUP first (the page's way: "before"); then the
// browser's FIZZ starts again on a fresh store
async function resetBrowser() {
  if (S.busy || attaching || booting) { toast("Wait: a write is still going", "--orange"); return; }
  const host = (S.mode === "browser" && S.link && BU ? await BU.hostFlash() : null) || (await DB.get("browserFlash")) || (BU && BU.host);
  let sum = null;
  try { sum = host && host.length ? readRegion(host) : null; } catch (_) { sum = null; }
  const n = sum ? sum.presets.length : 0, np = sum ? sum.projects.length : 0;
  const ok = await confirmBox({ title: "RESET THIS BROWSER", what: "A fresh FIZZ store", go: "RESET", color: "--red",
    sub: n ? `Its MY PRESETS (${n}) and its projects (${np}) are kept in BACKUP first.` : "The browser's FIZZ starts again on an empty store." });
  if (!ok) return;
  if (n) await keepBrowserBak(host);
  const unit = S.mode === "browser";
  if (unit) dropLink();
  if (BU) { BU.dispose(); BU = null; }
  FM.bstat = null; FM.bmem = null;
  await DB.put("browserFlash", new Uint8Array(0));
  if (unit) { S.unit = null; await useBrowser(); }
  unitUi.render();
  toast("THIS BROWSER: a fresh store", "--orange");
}
async function keepBrowserBak(host) {
  const date = new Date(), image = convertImage(host, "device"), sum = readRegion(image), fizz = FM.bver || "";
  const presets = sum.presets.map((p) => ({ fizz: true, name: p.name, blob: p.blob, drive: p.drive, pan: p.pan, sends: p.sends,
    fizzTags: FIZZ_TAGS.filter((_, k) => (p.tags >> k) & 1), favourite: false, source: { fizz, from: `MY PRESETS ${p.slot}` }, notes: [], origin: [], mark: "", tags: [] }));
  const file = makeFizzbak({ kind: "full", image, summary: sum, presets, fizz, unit: { identity: BROWSER_ID, port: "this browser" }, web: webFor(presets), date });
  const name = backupFileName(date, "full").replace("FIZZ backup", "FIZZ browser before reset");
  await keepBak({ date: +date, name, kind: "full", safety: true, saved: false, fizz, identity: BROWSER_ID, bytes: file, size: file.length,
    tracks: sum.tracks, layout: sum.layout, projects: sum.projects.map((p) => p.name), presets: presets.length });
  log(`kept ${name} in this browser (${file.length} B)`);
}
const unitUi = mountUnit({
  $, pick, connect, sound: soundClick, reset: resetBrowser,
  view: () => {
    const br = S.mode === "browser", u = S.unit && S.link ? S.unit : null;
    const fv = short(u && u.version).replace(/-.*$/, "");
    return {
      mode: S.mode,
      look: br ? booting || attaching : attaching || !S.link,
      bver: short(br && u ? u.version : FM.bver),
      fver: short(br ? FM.found && FM.found.version : u && u.version),
      fsub: !br && u ? `${fv ? `FIZZ ${fv} · ` : ""}${S.link.viaBle ? "BLUETOOTH" : "USB"}` : "",
      found: br && !!FM.found,
      // the FM-1's port: its name ends in " Bluetooth" (fizzlink.js isBlePort) or it is USB; none yet: USB
      via: (br ? FM.found && isBlePort(FM.found.port) : !!(S.link && S.link.viaBle)) ? "ble" : "usb",
      sound: br ? (!BU || !BU.engine ? "off" : BU.waking ? "look" : BU.running ? (muted ? "mute" : "on") : "off") : null,
      mem: memView(),
    };
  },
});

// ---------------------------------------------------------------- the simulated FM-1 (?sim)
async function simAccess() {
  const [{ SimFizz }, { createEngine }, { storeSound }] = await Promise.all([import("./simfizz.js"), import("../engine/engine.js"), import("../js/fizz_sysex.js")]);
  const module = await WebAssembly.compileStreaming(fetch("../fizz.wasm"));
  const sim = new SimFizz({ createEngine, module, frameMs: Math.max(0, +(Q.get("frame") ?? 1)), playing: Q.has("playing"),
    ...(Q.has("bt") ? { name: "FM-1 C1F0 Bluetooth" } : {}) });   // (?sim&bt: the port named as the Bluetooth package's)
  await sim.boot();
  const demo = [["SUB DROP", 0], ["GLASS BELL", 2], ["NIGHT PAD", 3], ["WIRE LEAD", 4], ["TIN KICK", 5], ["DUSTY EP", 1], ["BUBBLE FX", 6], ["ROUND BASS", 0],
    ["SODA PLUCK", 2], ["SLOW CLOUD", 3], ["ZAP SNARE", 5], ["TOY ORGAN", 1], ["HOLLOW LD", 4], ["FIZZ HAT", 5]];
  demo.forEach(([name, t], k) => {
    const blob = new Uint8Array(96); blob[0] = 0x51; blob[1] = 5; blob[2] = k % 8; blob[3] = (2 * k) % 16; blob[50] = (10 * k) % 100;
    sim.engine.midiIn(storeSound({ name, blob, tags: 1 << t, pan: (k % 5) - 2, reverb: 10 * (k % 4) }));
    sim.run(40);
  });
  sim.run(2500);
  if (Q.get("sim") === "away") {                       // (Web MIDI allowed, the FM-1 not plugged in: __simPlug(true) plugs it)
    const acc = { inputs: new Map(), outputs: new Map(), onstatechange: null, sysexEnabled: true };
    sim.access = acc;
    sim.gone = true; sim.input.state = sim.output.state = "disconnected";
    window.__simPlug = async (on) => {
      if (on) { acc.inputs.set("in", sim.input); acc.outputs.set("out", sim.output); await sim.replug(); }
      else { sim.unplug(); acc.inputs.clear(); acc.outputs.clear(); }
    };
  }
  window.__sim = sim;
  log("a simulated FM-1 (?sim): the browser FIZZ behind a fake Web MIDI");
  return sim.access;
}

// ---------------------------------------------------------------- start
// ?sim: the simulated FM-1 at start, as before; ?sim=away: Web MIDI allowed, the FM-1 not plugged in (__simPlug(on));
// ?sim=nomidi: Web MIDI not allowed yet (CONNECT FM-1 "asks"). Otherwise: Web MIDI asked only if the page already may
// (no prompt at start): an FM-1 running FIZZ there is the unit; else this browser's FIZZ
tab(TABS.includes(location.hash.slice(1)) ? location.hash.slice(1) : "backup");
const baksUp = loadBaks();
loadBrowserLib();
S.mode = SIM && !Q.get("sim") ? "fm1" : "browser";
renderUnit(); renderLibrary(); pool.render(); importer.render(); unitUi.render();
(async function startUp() {
  if (SIM && !Q.get("sim")) { await connect(); readMem(); return; }
  if ((await midiGranted()) && (await openMidi()) && (await findUnit())) { await connect(); if (S.link && S.mode === "fm1") { readMem(); return; } }
  await baksUp;
  await useBrowser();
})();
window.__mgr = { S, connect, backup, openFile, tab, pool, importer, refreshRows, editor, exporter, bfizz, openInEditor, live, projtab, useBrowser, leaveBrowser, bu: () => BU,
  FM, pick, unit: unitUi, loadBrowserLib, keys: () => keys };
