// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2026 Charles Vestal
// Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// FIZZ's install page: fm1-base's web installer (web/installer.html's script: Felucca's, by way of X0X and FoMni-1),
// its install, resume, stock-firmware and crash-report logic kept as they are, with FIZZ's changes:
//   - the firmware is chosen from site/firmware/releases.json (the latest by default, ?v=VERSION to pick one), each
//     package checked against its SHA-256 and its identity before it is offered;
//   - after the install, FIZZ's data blob is written (fizz_data.js pushData: the package does not carry it);
//   - "Check my FM-1" says what the unit runs, FIZZ's own version included (fizz_data.js readUnit);
//   - no m-bootloader option, English only, FIZZ's styling.
import { productOf, logicalImage, ufwEntries, Updater, IS_OTA, IS_STOCK } from "./fm1lib.js";
import { pushData, readUnit, crc32 } from "./fizz_data.js";
import { md, esc } from "./notes.js";

const $ = (id) => document.getElementById(id);
const INDEX = "../firmware/releases.json";
const TEXT = {
  newtab: " (opens in a new tab)", pct: ", {n} percent",
  nomidi: "This browser can't talk to the FM-1 (no Web MIDI). Open this page in Chrome or Edge on a computer.",
  loadfail: "Could not load the firmware: ",
  noindex: "No FIZZ release is published yet.",
  start: "Starting", verify: "Checking the package", loader: "Switching to update mode", write: "Writing FIZZ",
  reboot: "Restarting", data: "Writing FIZZ's data (its type and drawings)",
  done: "Done. FIZZ is installed and running.",
  stopped: "Stopped. Replug the USB cable and press Install again.",
  error: "Error: ",
  denied: "MIDI (SysEx) access was not allowed. Allow MIDI in the site settings (the address bar) and press Install again.",
  notfound: "FM-1 not found. Check the USB cable (a data cable), close other apps and browser tabs that use MIDI (another firmware's installer or editor), and press Install again. Coming from another firmware, leave it running as it is: no update mode is needed.",
  model: "The connected device is not an FM-1.",
  open: "This FM-1 runs M-VAVE's Open firmware. Put the stock firmware back first (Going back, below), then install FIZZ.",
  lost: "The USB connection was lost. Replug it and press Install again.",
  noloader: "The FM-1 in update mode was not found. Replug the USB cable and press Install again (it stays in update mode until it is finished).",
  noreturn: "The FM-1 did not come back. Switch it off and on.",
  mismatch: "Written, but the FM-1 reports another version: ",
  badpkg: "The firmware file is damaged or not the one listed. Reload the page.",
  nofizz: "FIZZ is installed, but it did not answer for its data. Replug the USB cable and press \"Write FIZZ's data\".",
  dataerr: "FIZZ is installed, but its data could not be written: ",
  datastopped: "FIZZ is installed, but writing its data stopped. Replug the USB cable and press \"Write FIZZ's data\".",
  badreq: "The FM-1 asked for something outside the file. Nothing more was written. ",
  leave: "Writing. Do not close the page.",
  stockok: "That is M-VAVE's FM-1 firmware. Press the button to install it.",
  stockbad: "That is not M-VAVE's FM-1 firmware. Choose the file from the link above.",
  stockdone: "Done. The FM-1 has restarted with M-VAVE's firmware.",
  reportwait: "Reading…", reportnone: "No answer. Check that the FM-1 is on and connected, and that it runs FIZZ.",
  reportok: "Read. Copy it and paste it in Discord, with what you were doing.", reportcopied: "Copied.",
  checking: "Looking for the FM-1…",
};
const t = (k) => TEXT[k];

/* what happens goes to two regions: #status (polite) the steps and the progress, #alert a failure (fm1-base's) */
const FAILS = new Set(["nomidi", "loadfail", "noindex", "stopped", "error", "denied", "notfound", "model", "open", "lost",
  "noloader", "noreturn", "mismatch", "badpkg", "stockbad", "nofizz", "dataerr", "datastopped", "badreq"]);
let lastKey = null, lastArg = "", lastPct = null;
const textOf = (k, arg, pct) => t(k) + arg + (pct == null ? "" : t("pct").replace("{n}", pct));
function show() {
  if (!lastKey) return;
  const txt = textOf(lastKey, lastArg, lastPct);
  if (FAILS.has(lastKey)) { $("status").textContent = ""; $("alert").textContent = txt; }
  else { $("alert").textContent = ""; $("status").textContent = txt; }
}
const sayK = (k, arg = "") => { lastKey = k; lastArg = arg; lastPct = null; show(); };
/* a step with its share done: said when the step changes, then every 10 percent or 2 seconds at most */
let saidAt = 0;
const sayPct = (k, pct) => {
  pct = Math.max(0, Math.min(100, Math.round(pct)));
  const now = performance.now();
  if (k === lastKey && lastPct != null && pct < lastPct + 10 && (now - saidAt < 2000 || pct === lastPct)) return;
  lastKey = k; lastArg = ""; lastPct = pct; saidAt = now; show();
};
const say = (txt) => { lastKey = null; $("alert").textContent = ""; $("status").textContent = txt; };
/* follows the newest line, unless the reader has scrolled up to look at an earlier one */
const log = (s) => {
  const el = $("log"), atEnd = el.scrollHeight - el.scrollTop - el.clientHeight < 24;
  el.append(s + "\n");
  if (atEnd) el.scrollTop = el.scrollHeight;
  $("logbox").hidden = false;
};
const hex = async (raw) => [...new Uint8Array(await crypto.subtle.digest("SHA-256", raw))].map((b) => b.toString(16).padStart(2, "0")).join("");

/* ---- the releases: site/firmware/releases.json, newest first */
let releases = [], chosen = null;          /* chosen: {rel, pkg, image, product, data} once checked */
const relOf = (v) => releases.find((r) => r.version === v);

function renderNotes() {
  const box = $("notes-list");
  if (!releases.length) { box.innerHTML = `<p class="dim">${esc(t("noindex"))}</p>`; return; }
  box.innerHTML = releases.map((r, i) => `
    <article class="rel" id="v${esc(r.version)}">
      <h3><span class="v">${esc(r.version)}</span>${r.channel === "beta" ? '<span class="tag beta">beta</span>' : ""}${i === 0 ? '<span class="tag latest">latest</span>' : ""}<span class="date">${esc(r.date || "")}</span></h3>
      ${md(r.notes)}
      <p class="files">${(r.packages || []).map((p) => `<a href="../firmware/${esc(p.fwsc.path)}" download>${esc(p.fwsc.path.split("/").pop())}</a> <span class="dim">(${Math.round(p.fwsc.bytes / 1024)} KB)</span> · <a href="../firmware/${esc(p.data.path)}" download>${esc(p.data.path.split("/").pop())}</a>`).join("<br>")}</p>
    </article>`).join("");
}

function fillChoice() {
  const sel = $("version");
  sel.innerHTML = releases.map((r, i) => `<option value="${esc(r.version)}">${esc(r.version)}${i === 0 ? " (latest)" : ""}${r.date ? " · " + esc(r.date) : ""}</option>`).join("");
  const want = new URLSearchParams(location.search).get("v");
  sel.value = want && relOf(want) ? want : releases[0].version;
  sel.disabled = releases.length < 2 || writing;
}

async function choose(version) {
  chosen = null;
  $("go").disabled = true;
  const rel = relOf(version);
  const pkg = rel && rel.packages && rel.packages[0];
  if (!pkg) { sayK("loadfail", version); return; }
  $("chosen").textContent = `FIZZ ${rel.version}`;
  $("chosenwhat").textContent = pkg.label || "";
  try {
    const get = async (f) => {
      const r = await fetch("../firmware/" + f.path);
      if (!r.ok) throw new Error(`${f.path}: HTTP ${r.status}`);
      const raw = new Uint8Array(await r.arrayBuffer());
      if (raw.length !== f.bytes || await hex(raw) !== f.sha256) throw Object.assign(new Error(f.path), { code: "badpkg" });
      return raw;
    };
    const [raw, data] = await Promise.all([get(pkg.fwsc), get(pkg.data)]);
    if (productOf(raw) !== rel.identity || crc32(data) !== pkg.data.crc32) { sayK("badpkg"); return; }
    if ($("version").value !== version) return;                 /* (another was chosen meanwhile) */
    chosen = { rel, pkg, image: logicalImage(raw), product: rel.identity, data, sha256: pkg.fwsc.sha256 };
    if (navigator.requestMIDIAccess) { say(`FIZZ ${rel.version} is ready to install.`); $("go").disabled = writing; }
    else sayK("nomidi");
  } catch (e) {
    if (e && e.code === "badpkg") sayK("badpkg"); else sayK("loadfail", e.message);
  }
}

$("version").addEventListener("change", (e) => {
  const u = new URL(location.href);
  u.searchParams.set("v", e.target.value);
  history.replaceState(null, "", u);
  choose(e.target.value);
});

let writing = false;
window.addEventListener("beforeunload", (e) => { if (writing) { e.preventDefault(); e.returnValue = t("leave"); } });

(async () => {
  try {
    const r = await fetch(INDEX, { cache: "no-cache" });
    if (!r.ok) throw new Error(`releases.json: HTTP ${r.status}`);
    releases = ((await r.json()).releases || []).filter((x) => x && x.version && x.packages && x.packages.length);
  } catch (e) { sayK("loadfail", e.message); }
  renderNotes();
  if (!releases.length) { $("chosen").textContent = "Nothing to install yet"; if (lastKey !== "loadfail") sayK("noindex"); return; }
  fillChoice();
  if (!navigator.requestMIDIAccess) $("nomidi").hidden = false;
  await choose($("version").value);
  if (location.hash) document.getElementById(location.hash.slice(1))?.scrollIntoView();
})();

/* the backup page: only where fm1-base's is built for FIZZ (firmware/tools/sync_installer.py, fm1.toml backup = true) */
fetch("backup/", { method: "HEAD" }).then((r) => {
  if (!r.ok) return;
  document.querySelectorAll(".baklink").forEach((a) => { a.hidden = false; });
  document.querySelectorAll(".nobak").forEach((a) => { a.hidden = true; });
}).catch(() => {});

/* ---- check my FM-1: what it runs, FIZZ's version from FIZZ itself */
$("checkgo").addEventListener("click", async () => {
  const out = $("unit");
  if (writing) return;
  if (!navigator.requestMIDIAccess) { out.textContent = t("nomidi"); return; }
  let midi;
  try { midi = await navigator.requestMIDIAccess({ sysex: true }); }
  catch (_) { out.textContent = t("denied"); return; }
  $("checkgo").disabled = true;
  out.textContent = t("checking");
  try {
    const u = await readUnit(midi);
    const latest = releases[0] && releases[0].version;
    if (!u) out.textContent = "No FM-1 answered. Check the USB cable and close other apps that use its MIDI.";
    else if (u.kind === "fizz") {
      const v = u.version || u.fromId;
      out.innerHTML = u.version ? `This FM-1 runs <b>FIZZ ${esc(v)}</b>` + (v === latest ? ", the latest." : latest ? `. The latest is ${esc(latest)}.` : ".")
        : `This FM-1 runs FIZZ ${v ? esc(v) + " " : "(a development build) "}but FIZZ did not answer: its data may be missing (install again) or it is in safe mode.`;
    } else if (u.kind === "stock") out.innerHTML = u.open ? `This FM-1 runs M-VAVE's Open firmware (${esc(u.id)}). Put the stock firmware back first (Going back, below).`
      : `This FM-1 runs M-VAVE's firmware (${esc(u.id)}).`;
    else if (u.kind === "update") out.textContent = "This FM-1 is in update mode: an install did not finish. Press Install to finish it.";
    else out.innerHTML = `This FM-1 runs another firmware (identity ${esc(u.id)}).`;
  } catch (e) { out.textContent = t("error") + (e && e.message); }
  $("checkgo").disabled = false;
});

/* stock firmware: M-VAVE's FM-1 firmware, any version, chosen by the user (M-VAVE's server allows no cross-site
   download) and named as M-VAVE's (IS_STOCK, fm1lib.js); the FM-1 checks the package itself before writing */
let stock = null;
$("stockfile").addEventListener("change", async (e) => {
  stock = null;
  $("stockgo").disabled = true;
  const f = e.target.files && e.target.files[0];
  if (!f) return;
  try {
    const raw = new Uint8Array(await f.arrayBuffer());
    const product = productOf(raw);
    if (!IS_STOCK(product)) { sayK("stockbad"); return; }
    stock = { image: logicalImage(raw), product, sha256: await hex(raw) };
    $("stockgo").disabled = writing;
    sayK("stockok");
  } catch (err) { sayK("error", err && err.message); }
});

/* before anything is written: the screen, the cable, the time, a backup (fm1-base's dialog) */
function confirmed(isStock) {
  const d = $("confirm");
  $("confirmfizz").hidden = isStock;
  $("confirmstock").hidden = !isStock;
  $("bakok").checked = false;                    /* asked every time */
  $("confirmgo").disabled = true;
  d.returnValue = "";
  d.showModal();
  return new Promise((r) => d.addEventListener("close", () => r(d.returnValue === "go"), { once: true }));
}
$("bakok").addEventListener("change", () => { $("confirmgo").disabled = !$("bakok").checked; });

function busy(on) {
  writing = on;
  $("go").disabled = on || !chosen;
  $("stockgo").disabled = on || !stock;
  $("datago").disabled = on;
  $("checkgo").disabled = on;
  $("version").disabled = on || releases.length < 2;
}

/* FIZZ's data after the install: its own share of the bar, from `from` percent to 100 */
async function writeData(midi, data, from) {
  sayK("data");
  const r = await pushData(midi, data, { log, onProgress: (f) => { $("bar").value = Math.min(99, from + f * (100 - from)); sayPct("data", $("bar").value); } });
  let v = null;
  try { const u = await readUnit(midi); v = u && u.kind === "fizz" ? u.version : null; } catch (_) { /* said below */ }
  $("bar").value = 100;
  $("datago").hidden = true;
  say(v ? `Done. The FM-1 runs FIZZ ${v}.` : t("done"));
  log(`data ${r.written ? "written" : "already there"}; FIZZ ${v || "did not say its version"}`);
}

async function run(img, product, isStock, sha256) {
  if (!(await confirmed(isStock))) return;
  let midi;
  try { midi = await navigator.requestMIDIAccess({ sysex: true }); }
  catch (e) { sayK("denied"); return; }
  const data = isStock ? null : chosen.data;
  busy(true);
  $("datago").hidden = true;
  try {
    const up = new Updater(midi, log);
    /* progress: the bytes the device reads, against what it reads for this package (the size of the package,
       not a fixed request count). Step 1 (the running firmware): the UFW header, the loader's head twice, its
       body. Step 2 (fm1-base's loader): the header, the chip-key check, the whole flash.bin (checked before
       anything is erased), then the app area again as it writes it. Then (FIZZ) its data, read once. Held at 99
       until the device comes back. */
    const ent = ufwEntries(img), fl = ent[0], ldr = ent[100];
    const CHECK = 0x400 + 0x400 + (ldr ? ldr.len - 0x20 : 0);
    const WRITE = fl && fl.len > 0x4000 && fl.len <= 0x100000 ? 0x400 + 0x20 + fl.len + (fl.len - 0x4000) : img.length;
    const DATA = data ? data.length : 0;
    const total = CHECK + WRITE + DATA;
    const step = (k, a, b) => {
      log(`${k} ${a ?? ""}`);
      if (k === "start") sayK("start");
      if (k === "verify") { $("bar").value = (Math.min(b, CHECK) / total) * 100; sayPct("verify", $("bar").value); }
      if (k === "loader") sayK("loader");
      if (k === "write") { $("bar").value = Math.min(99, ((CHECK + b) / total) * 100); sayPct("write", $("bar").value); }
      if (k === "reboot") sayK("reboot");
      if (k === "done" && isStock) { $("bar").value = 100; sayK("stockdone"); }
    };
    const ota = await up.find((id) => /^ota-/i.test(id.model));
    let done;
    if (ota) {
      ota.link.close();
      done = await up.resume(img, step);
      if (!done) sayK("stopped");
      else if (isStock) { $("bar").value = 100; sayK("stockdone"); }
    } else {
      await up.install(img, product, step, sha256);
      done = true;
    }
    if (done && data) {
      try { await writeData(midi, data, ((CHECK + WRITE) / total) * 100); }
      catch (e) { $("datago").hidden = false; throw e; }
    }
  } catch (e) {
    if (isStock && e && e.code === "mismatch") { $("bar").value = 100; sayK("stockdone"); }   /* stock names itself its own way */
    else if (e && e.code === "dataerr") sayK("dataerr", e.detail || e.message);
    else if (e && e.code && TEXT[e.code]) sayK(e.code, e.code === "mismatch" ? (e.detail || "") : e.code === "badreq" ? e.message : "");
    else sayK("error", e && e.message);
    log(String(e && e.stack || e));
  }
  busy(false);
}

$("go").addEventListener("click", () => { if (chosen) run(chosen.image, chosen.product, false, chosen.sha256); });
$("stockgo").addEventListener("click", () => { if (stock) run(stock.image, stock.product, true, stock.sha256); });
/* FIZZ's data again, without installing again (after a data step that failed) */
$("datago").addEventListener("click", async () => {
  if (writing || !chosen) return;
  let midi;
  try { midi = await navigator.requestMIDIAccess({ sysex: true }); }
  catch (e) { sayK("denied"); return; }
  busy(true);
  $("bar").value = 0;
  try { await writeData(midi, chosen.data, 0); }
  catch (e) {
    if (e && e.code === "dataerr") sayK("dataerr", e.detail || e.message);
    else if (e && e.code && TEXT[e.code]) sayK(e.code);
    else sayK("error", e && e.message);
    log(String(e && e.stack || e));
  }
  busy(false);
});

/* the crash report: the firmware's log readout (fm1-base device/main_fm1.c log_frame: F0 7D 4C 47 F7 asks, a frame a
   line, "end" last), the same as fm1-base's tools/fm1_remote.py log */
const LG = [0x7D, 0x4C, 0x47];
$("reportgo").addEventListener("click", async () => {
  const st = (k) => { $("reportstatus").textContent = t(k); };
  let midi;
  try { midi = await navigator.requestMIDIAccess({ sysex: true }); }
  catch (e) { $("reportstatus").textContent = t(navigator.requestMIDIAccess ? "denied" : "nomidi"); return; }
  if (writing) return;
  $("reportgo").disabled = true;
  st("reportwait");
  const lines = [];
  try {
    const dev = await new Updater(midi).find((id) => !IS_OTA(id));
    if (dev) {
      lines.push(`${dev.id.text} (${dev.name}), ${new Date().toISOString()}`);
      dev.link.drain();
      dev.link.send([0xF0, ...LG, 0xF7]);
      const end = Date.now() + 4000;
      while (Date.now() < end) {
        const p = await dev.link.read(end - Date.now());
        if (!p) break;
        if (p.length < 5 || p[1] !== LG[0] || p[2] !== LG[1] || p[3] !== LG[2]) continue;
        const txt = String.fromCharCode(...p.slice(4, -1));
        if (txt === "end") break;
        lines.push(txt);
      }
      dev.link.close();
    }
  } catch (e) { lines.push(String(e && e.message || e)); }
  $("reportgo").disabled = false;
  if (lines.length < 2) { st("reportnone"); return; }
  $("report").value = lines.join("\n");
  $("report").hidden = $("reportcopy").hidden = false;
  st("reportok");
});
$("reportcopy").addEventListener("click", async () => {
  try { await navigator.clipboard.writeText($("report").value); $("reportstatus").textContent = t("reportcopied"); }
  catch (_) { $("report").select(); }
});

