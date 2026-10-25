// SPDX-License-Identifier: GPL-3.0-only
// FIZZ Manager (docs/WEB-MANAGER-PLAN.md §9): connect to an FM-1 running FIZZ over Web MIDI (Chrome, Edge), back up
// its whole store to a .fizzbak and put one back (Phase 0); edit its library (tags, rename, delete, reorder), its
// project's pool (pool.js) and import Digitone sounds into it (importer.js) (Phase 1: FIZZ's STATUS .. PROJECT
// commands, docs/SYSEX.md). The protocol is fizzlink.js's; the files fizzbak.js's; the offline store reader
// fizzstore.js's.
//   ?sim        a simulated FM-1 (simfizz.js: the browser FIZZ for its SysEx) instead of Web MIDI: headless checks
//   ?depth=N    the backup's requests outstanding (1, the default: one at a time); &gap=MS their spacing
import { findFizz, FZER } from "./fizzlink.js";
import { makeFizzbak, readBackup, backupFileName, soundKey } from "./fizzbak.js";
import { readRegion, layoutOf } from "./fizzstore.js";
import { parseDump, FIZZ_TAGS } from "../js/fizz_sysex.js";
import { soundFromStoreFrame, soundToFizzsnd, soundFileName, makeFizzlib, libraryFileName, sameSound } from "../js/library.js";
import { storeFrame } from "../js/digitone.js";
import { $, esc, tagsOf, colOf, algo, emptyArt, refusal } from "./ui.js";
import { mountPool } from "./pool.js";
import { mountImport } from "./importer.js";

const Q = new URLSearchParams(location.search);
const SIM = Q.has("sim");
const DEPTH = Math.max(1, Math.min(4, +Q.get("depth") || 1)), GAP = Math.max(0, +Q.get("gap") || 0);
const kb = (n) => `${Math.round(n / 1024)} KB`;
const mmss = (ms) => { const s = Math.floor(ms / 1000); return `${Math.floor(s / 60)}:${String(s % 60).padStart(2, "0")}`; };

const S = { access: null, link: null, unit: null, busy: false, baks: [], web: { tags: {} }, sel: null, filter: new Set(), abort: null,
  editing: 0, armed: null, users: new Map() };

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
    del: (id) => tx("backups", "readwrite", (s) => s.delete(id)),
    get: (k) => tx("kv", "readonly", (s) => s.get(k)),
    put: (k, v) => tx("kv", "readwrite", (s) => s.put(v, k)),
  };
})();
const KEEP = 16;
async function loadBaks() {
  S.baks = ((await DB.all()) || []).sort((a, b) => b.date - a.date);
  S.web = (await DB.get("web")) || { tags: {} };
  renderBaks();
}
async function keepBak(rec) {
  const id = await DB.add(rec);
  rec.id = id ?? `mem-${Date.now()}`;
  S.baks.unshift(rec);
  for (const old of S.baks.splice(KEEP)) if (typeof old.id === "number") await DB.del(old.id);
  renderBaks();
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
  $("l-export-all").disabled = !on || S.busy || !u.dumps;
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
    setUnit("on", "FM-1", version ? `FIZZ ${version.replace(/-\d+-g[0-9a-f]+$/, "")}${status ? ` · ${status.tracks} tracks` : ""}` : link.id);
    if (!version && info) say("FIZZ does not answer (safe mode, or its data missing): back up and restore still work.", "--orange");
    else if (!info) say(`This FIZZ has no backup over USB yet: <a href="../install/">update FIZZ</a>.`, "--orange");
    else if (version && !status) say(`This FIZZ shows its library but cannot edit it yet: <a href="../install/">update FIZZ</a>.`, "--orange");
    else if (status && status.noStore) say("FIZZ runs without its store (safe mode): back up and restore still work.", "--orange");
    if ($("tab-backup").hidden) loadDumps();
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
    renderUnit(); renderLibrary(); renderBaks(); pool.render(); importer.render();
  }
}
function lost() {
  if (!S.link) return;
  S.link = null;
  setUnit("bad", "UNPLUGGED");
  log("the FM-1 was disconnected");
  renderUnit(); renderLibrary(); renderBaks(); pool.render(); importer.render();
}
let stateT = null;
function onState(ev) {
  const p = ev && ev.port;
  if (p && p.state === "disconnected" && S.link) { try { S.link.close(); } catch (_) { /* gone */ } lost(); return; }
  clearTimeout(stateT);
  if (!S.link && !S.busy) stateT = setTimeout(attach, 800);
}
$("unit").addEventListener("click", () => { if (!S.link) connect(); });

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
    download(file, name);
    await keepBak({ date: +date, name, kind, safety, fizz: u.version || "", identity: u.id, bytes: file, size: file.length,
      tracks: r.summary ? r.summary.tracks : null, layout: r.summary ? r.summary.layout : null,
      projects: r.summary ? r.summary.projects.map((p) => p.name) : null, presets: r.presets.length });
    if (r.stats) showSpeed(r.stats);
    if (!safety) toast(`Saved ${name}`);
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
      <button class="icon" data-act="save" aria-label="Save ${esc(b.name)} again">${SAVE_I}</button>
      <button class="icon" data-act="del" aria-label="Remove from this list">${DEL_I}</button>
      <button class="btn small ghost" data-act="restore" ${S.link && !S.busy ? "" : "disabled"}>RESTORE</button></div>`;
  }).join("");
}
$("baks").addEventListener("click", async (ev) => {
  const btn = ev.target.closest("button[data-act]");
  if (!btn) return;
  const b = S.baks[+btn.closest(".bk").dataset.i];
  if (!b) return;
  if (btn.dataset.act === "save") download(b.bytes, b.name);
  else if (btn.dataset.act === "del") {
    if (typeof b.id === "number") await DB.del(b.id);
    S.baks.splice(S.baks.indexOf(b), 1);
    renderBaks();
  } else if (btn.dataset.act === "restore") openFile(b.bytes, b.name, b);
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
      projects: parsed.summary ? parsed.summary.projects.map((p) => p.name) : null, presets: parsed.presets.length, file: true };
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
  S.unit.dumps = null; S.unit.fdumps = null; S.sel = null;
  pool.forget();
}
async function restoreFull() {
  const image = R.parsed.image, rec = R.rec;
  closeRestore();
  S.busy = true; renderUnit(); renderBaks();
  S.abort = new AbortController();
  progOpen("CHECKING", image.length);
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
    toast(`Restored ${rec.name}`);
    log(`restored ${rec.name}`);
    await refresh();
  } catch (e) {
    progClose();
    log(`restore: ${e.code || ""} ${e.fzer || ""} ${e.message}`);
    if (e.code === "cancel") toast("Cancelled. Nothing changed.", "--muted");
    else if (e.fzer === 10) say("The FM-1 is playing. Stop it, then RESTORE again.", "--orange");
    else if (e.fzer === 11) say("This backup is from a store of another size. Its presets can still be added.", "--orange");
    else if (e.fzer === 12) say("The backup did not reach the FM-1 whole. Nothing was changed.", "--orange");
    else if (e.fzer === 13 || e.fzer === 14 || e.code === "lost" || (e.code === "timeout" && e.written !== false))
      say(`The restore was cut off while writing. <b>FIZZ has cleared its store.</b> Plug the FM-1 in and restore this backup again (${esc(FZER[e.fzer] || e.message)}).`, "--red");
    else say(`The restore did not finish: ${esc(e.message)}`, "--red");
    if (S.link && e.code !== "lost") await refresh().catch(() => {});
  } finally {
    S.busy = false; S.abort = null;
    renderUnit(); renderLibrary(); renderBaks();
  }
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
// the unit's lists again: LIST (MY PRESETS, the factory's, the pool), STATUS, PROJECTS
async function refreshRows({ projects = false } = {}) {
  const u = S.unit, link = S.link;
  if (!u || !link || !u.version) return;
  try {
    u.rows = await link.list({ factory: true, pool: true });
    u.status = await link.status();
    if (projects || !u.projects) u.projects = await link.projects();
  } catch (e) { log(`refresh: ${e.code || ""} ${e.message}`); }
  S.users.clear();
  renderUnit(); renderLibrary(); pool.render();
}
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
const frameOf = (r) => (r.source === "mine" ? S.unit.dumps && S.unit.dumps.get(r.slot) : S.unit.fdumps && S.unit.fdumps[r.slot - 1]);
function cas(r) {
  const f = frameOf(r), d = f && parseDump(f), tg = tagsOf(r.tags), c = colOf(r.tags);
  const on = S.sel && S.sel.source === r.source && S.sel.slot === r.slot;
  const drag = r.source === "mine" && S.unit.status ? ' draggable="true"' : "";
  return `<button class="cas" style="--c:${c}" data-src="${r.source}" data-slot="${r.slot}" aria-pressed="${on}"${drag}><span class="stripe"></span><span class="sl">${r.slot}</span>${algo(d ? d.blob[2] : r.slot, c)}` +
    `<span class="nm">${esc(r.name)}</span><span class="mark">${r.imported ? '<span class="imp" title="imported">↓</span>' : ""}${r.inPool && r.source === "mine" ? '<span class="pl">POOL</span>' : ""}${tg[0] ? `<span class="tag t-${tg[0]}">${tg[0]}</span>` : ""}</span></button>`;
}
function renderLibrary() {
  const u = S.unit;
  const rows = u ? u.rows : [];
  const mine = rows.filter((r) => r.source === "mine"), fac = rows.filter((r) => r.source === "factory");
  $("l-count").textContent = u ? `${mine.length} / 256` : "";
  $("l-fcount").textContent = u ? fac.length : "";
  $("l-filter").innerHTML = FIZZ_TAGS.map((t) => `<button class="tag t-${t} ${S.filter.size && !S.filter.has(t) ? "off" : ""}" data-tag="${t}" aria-pressed="${S.filter.has(t)}">${t}</button>`).join("");
  const shown = mine.filter((r) => !S.filter.size || tagsOf(r.tags).some((t) => S.filter.has(t)));
  $("l-mine").innerHTML = shown.map(cas).join("") || (u ? "" : `<div class="empty">${emptyArt()}</div>`);
  $("l-fac").innerHTML = fac.map(cas).join("");
  renderSel();
}
const DOTS = (n) => `<span class="dots">${"●".repeat(Math.min(6, n))}${n > 6 ? "+" : ""}</span>`;
function renderSel() {
  const el = $("l-sel"), u = S.unit;
  const r = S.sel && u && u.rows.find((x) => x.source === S.sel.source && x.slot === S.sel.slot);
  if (!r) {
    S.armed = null;
    el.innerHTML = u ? `<div class="empty">${emptyArt()}</div>`
      : `<div class="empty"><div style="display:grid;gap:22px;justify-items:center">${emptyArt()}<button class="btn huge" id="l-connect">CONNECT</button></div></div>`;
    const b = $("l-connect");
    if (b) b.addEventListener("click", connect);
    return;
  }
  const f = frameOf(r), d = f && parseDump(f), c = colOf(r.tags);
  const snd = f && soundFromStoreFrame(f), key = snd && soundKey(snd), web = (key && S.web.tags[key]) || [];
  const can = !!(S.link && !S.busy && u.status), mineSel = r.source === "mine";
  const users = mineSel ? S.users.get(`mine:${r.id}`) : null;
  const armed = S.armed && S.armed.slot === r.slot && S.armed.source === r.source ? S.armed : null;
  el.innerHTML = `<div class="selhead">${algo(d ? d.blob[2] : r.slot, c)}<div style="min-width:0;flex:1"><div class="h" style="margin:0">${mineSel ? "MY PRESETS" : "FACTORY"} ${r.slot}</div>
      ${mineSel && can ? `<input class="bigname" id="l-name" maxlength="12" spellcheck="false" autocomplete="off" value="${esc(r.name)}" aria-label="Name">` : `<div class="big">${esc(r.name)}</div>`}</div></div>
    <div><div class="h">TAGS</div><div class="row tags" style="flex-wrap:wrap;gap:8px">
      ${FIZZ_TAGS.map((t, k) => `<button class="tag t-${t} ${(r.tags >> k) & 1 ? "" : "off"}" data-dtag="${k}" aria-pressed="${!!((r.tags >> k) & 1)}" ${can ? "" : "disabled"}>${t}</button>`).join("")}
      ${web.map((t) => `<button class="tag web" data-web="${esc(t)}" aria-label="${esc(t)}, remove">${esc(t)}</button>`).join("")}
      ${key ? '<button class="tag web" id="l-addtag" aria-label="Add a tag (kept on this computer and in backups)">+</button>' : ""}</div></div>
    ${mineSel ? `<div><div class="h">USED BY</div><div class="row" style="gap:10px;flex-wrap:wrap" id="l-users">${users === undefined ? '<span class="pchip ghost">…</span>'
      : users && users.projects ? (users.projects.length ? users.projects.map((p) => `<span class="uchip">${esc(projectName(p.project))}${p.steps ? DOTS(p.steps) : ""}</span>`).join("") : '<span class="pchip ghost">—</span>') : ""}</div></div>` : ""}
    ${d ? `<div class="row" style="gap:22px;flex-wrap:wrap"><div class="kv"><b>${d.pan > 0 ? "R" + d.pan : d.pan < 0 ? "L" + -d.pan : "C"}</b>pan</div><div class="kv"><b>${d.chorus}</b>chorus</div><div class="kv"><b>${d.delay}</b>delay</div><div class="kv"><b>${d.reverb}</b>reverb</div><div class="kv"><b>${d.drive}</b>drive</div></div>` : ""}
    <div class="row selacts">${r.imported ? '<span class="tag web" style="height:34px;padding:0 12px">↓ IMPORTED</span>' : ""}
      <button class="btn" id="l-pool" style="--c:var(--green)" ${can && r.id ? "" : "disabled"}>+ POOL</button>
      <button class="btn ghost" id="l-export" ${f ? "" : "disabled"}>EXPORT</button>
      ${mineSel ? `<button class="btn ${armed ? "danger" : "ghost redline"}" id="l-del" style="margin-left:auto" ${can ? "" : "disabled"}>${armed ? `DELETE? ${armed.projects ? `${armed.projects} ▣` : ""}${armed.steps ? ` ${armed.steps} ●` : ""}` : "DELETE"}</button>` : ""}</div>`;
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
    const s = { ...snd, source: { fizz: u.version || "", from: `${r.source === "mine" ? "MY PRESETS" : "FACTORY"} ${r.slot}` } };
    const j = JSON.parse(soundToFizzsnd(s));
    if (web.length) j.web = { tags: web };
    download(new TextEncoder().encode(JSON.stringify(j, null, 2) + "\n"), soundFileName(r.name), "application/json");
  });
  const pl = $("l-pool");
  if (pl) pl.addEventListener("click", async () => {
    const x = await edit("+ POOL", (l) => l.poolRef(0, r.id));
    if (x && x.status === 0) { toast(`${r.name} → POOL ${x.a}`); pool.fizz(x.a); await refreshRows(); }
  });
  const del = $("l-del");
  if (del) del.addEventListener("click", async () => {
    const confirm = !!armed;
    const x = await edit("DELETE", (l) => l.remove("mine", r.slot, { confirm }));
    if (x && x.status === 10) { S.armed = { source: r.source, slot: r.slot, projects: x.a, steps: x.b }; renderSel(); return; }
    S.armed = null;
    if (x && x.status === 0) {
      toast(`Deleted ${r.name}`, "--red");
      if (u.dumps) u.dumps.delete(r.slot);
      S.sel = null;
      await refreshRows();
    } else renderSel();
  });
}
$("l-filter").addEventListener("click", (ev) => {
  const b = ev.target.closest("[data-tag]");
  if (!b) return;
  if (S.filter.has(b.dataset.tag)) S.filter.delete(b.dataset.tag); else S.filter.add(b.dataset.tag);
  renderLibrary();
});
function pick(ev) {
  const b = ev.target.closest(".cas");
  if (!b) return;
  if (!S.sel || S.sel.source !== b.dataset.src || S.sel.slot !== +b.dataset.slot) S.armed = null;
  S.sel = { source: b.dataset.src, slot: +b.dataset.slot };
  renderLibrary();
}
$("l-mine").addEventListener("click", pick);
$("l-fac").addEventListener("click", pick);
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
  for (const s of [a, b]) { const el = $("l-mine").querySelector(`.cas[data-slot="${s}"]`); if (el) el.classList.add("moved"); }   // (slid in)
}
let dragSlot = 0;
$("l-mine").addEventListener("dragstart", (ev) => {
  const b = ev.target.closest(".cas");
  if (!b) return;
  dragSlot = +b.dataset.slot;
  ev.dataTransfer.effectAllowed = "move";
  ev.dataTransfer.setData("application/x-fizz-preset", JSON.stringify({ slot: dragSlot, id: (S.unit.rows.find((r) => r.source === "mine" && r.slot === dragSlot) || {}).id }));
  b.classList.add("lift");
});
$("l-mine").addEventListener("dragend", (ev) => { const b = ev.target.closest(".cas"); if (b) b.classList.remove("lift"); dragSlot = 0; });
$("l-mine").addEventListener("dragover", (ev) => {
  const b = ev.target.closest(".cas");
  if (!dragSlot || !b) return;
  ev.preventDefault();
  $("l-mine").querySelectorAll(".cas.target").forEach((x) => x !== b && x.classList.remove("target"));
  b.classList.toggle("target", +b.dataset.slot !== dragSlot);
});
$("l-mine").addEventListener("dragleave", (ev) => { const b = ev.target.closest(".cas"); if (b) b.classList.remove("target"); });
$("l-mine").addEventListener("drop", (ev) => {
  const b = ev.target.closest(".cas");
  if (!dragSlot || !b) return;
  ev.preventDefault();
  b.classList.remove("target");
  swapMine(dragSlot, +b.dataset.slot);
});
$("l-mine").addEventListener("keydown", (ev) => {
  const b = ev.target.closest(".cas");
  if (!b || !ev.altKey || (ev.key !== "ArrowUp" && ev.key !== "ArrowDown") || !S.unit || !S.unit.status) return;
  ev.preventDefault();
  const a = +b.dataset.slot;
  S.sel = { source: "mine", slot: a };
  swapMine(a, a + (ev.key === "ArrowUp" ? -1 : 1)).then(() => { const n = $("l-mine").querySelector(`.cas[data-slot="${S.sel.slot}"]`); if (n) n.focus(); });
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
$("l-export-all").addEventListener("click", () => {
  const u = S.unit;
  if (!u || !u.dumps) return;
  const sounds = [...u.dumps.entries()].sort((a, b) => a[0] - b[0]).map(([slot, f]) => ({ ...soundFromStoreFrame(f), source: { fizz: u.version || "", from: `MY PRESETS ${slot}` } }));
  download(makeFizzlib(sounds, { fizz: u.version || "" }), libraryFileName(), "application/zip");
});

// ---------------------------------------------------------------- the pool, the import (their own files)
const ctx = { S, $, log, say, toast, edit, refreshRows, loadDumps, frameOf, projectName, saveWeb, render: () => { renderUnit(); renderLibrary(); } };
const pool = mountPool(ctx);
const importer = mountImport(ctx);

// ---------------------------------------------------------------- tabs
const TABS = ["backup", "library", "pool", "import"];
function tab(name) {
  for (const t of TABS) {
    $(`t-${t}`).setAttribute("aria-selected", String(t === name));
    $(`tab-${t}`).hidden = t !== name;
  }
  if (name !== "backup") loadDumps();
  if (name === "pool") pool.show();
  if (name === "import") importer.render();
  if (location.hash !== `#${name}`) history.replaceState(null, "", `${location.search}#${name}`);
}
for (const t of TABS) $(`t-${t}`).addEventListener("click", () => tab(t));
window.addEventListener("beforeunload", (ev) => { if (S.busy) { ev.preventDefault(); ev.returnValue = ""; } });

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
window.__mgr = { S, connect, backup, openFile, tab, pool, importer, refreshRows };
