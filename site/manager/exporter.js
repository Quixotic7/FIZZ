// SPDX-License-Identifier: GPL-3.0-only
// The manager's EXPORT (docs/MANAGER-P2-PLAN.md §2, shot 4): one preset, or MY PRESETS all, as FIZZ's own .fizzsnd
// (several: a .fizzlib), a Digitone .dnsnd, or a Digitone II .dn2pst / .dn2snd (several: a ZIP of them). Each home a
// card with the carry bar (10 segments: 10 less what that format drops or changes for these sounds) and, under it, the
// list of what does not carry for the format picked, from the writers' own notes (js/fizz2dn.js). The Digitone files
// are marked EXPERIMENTAL until a Digitone and a Digitone II owner confirm they load (DN2-FORMAT.md "The next proof").
//   carryOf(notes) -> { lost, changed, bar, rows }   (no DOM: the tests)
//   exportFiles(sounds, kind, date) -> { name, bytes, type, notes: [per sound] }   kind: fizz, dnsnd, dn2pst, dn2snd
//   mountExport({ download, log }) -> { open(sounds, { title, tags }) }
// FIZZ MIDI (e): a MIDI track's sound (sound.js isMidiBlob) is no FM TONE sound: the Digitone files leave it out (its
// row "MIDI SOUND · not written"; only MIDI sounds: nothing to write, DOWNLOAD off); .fizzsnd / .fizzlib keep it.
//   exportFiles(..) -> { .., skipped: [the MIDI sounds' names] } (Digitone kinds; none left: throws MIDI_ONLY)
import { fizzToDnsnd, fizzToDn2pst, dnName } from "../js/fizz2dn.js";
import { soundToFizzsnd, soundFileName, makeFizzlib, libraryFileName, zipStored } from "../js/library.js";
import { esc } from "./ui.js";
import { isMidiBlob } from "./sound.js";

export const MIDI_ONLY = "a MIDI sound cannot become a Digitone sound";
const MIDI_ROW = { what: "MIDI SOUND", how: "not written", c: "--midi", w: 1, note: MIDI_ONLY };

export const KINDS = [
  { k: "fizz", ext: ".fizzsnd", title: "FIZZ", c: "--cream" },
  { k: "dnsnd", ext: ".dnsnd", title: "DIGITONE", c: "--cyan", exp: true },
  { k: "dn2", ext: ".dn2pst · .dn2snd", title: "DIGITONE II", c: "--magenta", exp: true },
];

// a writer's note -> a row: what (its subject), how (a short verdict), its weight (dropped 1, changed .5), the note
export function noteRow(n) {
  const parts = n.split(": "), last = parts[parts.length - 1];
  let what = parts[0].replace(/\s*\([^)]*\)/g, "").replace(/ (has|have) no .*$/, "").trim().toUpperCase();
  if (what.length > 24) what = `${what.slice(0, 23)}…`;
  const how = /dropped/i.test(last) || last.trim() === "off" ? ["dropped", "--red", 1]
    : /not written/i.test(last) ? ["not written", "--yellow", 1]
    : /no such step|nearest/i.test(n) ? ["nearest step", "--yellow", 0.5]
    : /approximated/i.test(last) ? ["approximated", "--orange", 0.5]
    : /as ADSR/i.test(last) ? ["as ADSR", "--orange", 0.5]
    : /written as/i.test(last) ? ["written as " + last.replace(/^.*written as\s*/i, "").split(/[ ,]/)[0], "--orange", 0.5]
    : /percent|%/.test(last) ? ["in percent", "--yellow", 0.5]
    : /before|after/i.test(last) ? ["moved", "--orange", 0.5] : ["changed", "--orange", 0.5];
  return { what, how: how[0], c: how[1], w: how[2], note: n };
}
export function carryOf(notes) {
  const rows = notes.map(noteRow);
  const lost = rows.filter((r) => r.w >= 1).length, changed = rows.length - lost;
  const bar = rows.length ? Math.max(1, Math.min(9, Math.round(10 - rows.reduce((a, r) => a + r.w, 0)))) : 10;
  return { lost, changed, bar, rows };
}
const carryText = (c) => (!c.rows.length ? "everything" : [c.lost ? `${c.lost} value${c.lost === 1 ? "" : "s"} lost` : "",
  c.changed ? `${c.changed} changed` : ""].filter(Boolean).join(" · "));
const barC = (n) => (n >= 10 ? "var(--green)" : n >= 8 ? "var(--yellow)" : "var(--orange)");

const fileBase = (name) => (dnName(name) || "FIZZ").replace(/[\\/:*?"<>|]+/g, "_");
// the files: one sound its file, several a .fizzlib / a ZIP
export function exportFiles(sounds, kind, date = new Date()) {
  const write = (s) => {
    const x = { blob: s.blob, name: s.name, tags: s.fizzTags || [], pan: s.pan | 0, sends: s.sends || {}, drive: s.drive | 0 };
    if (kind === "dnsnd") return fizzToDnsnd(x, { date });
    const o = fizzToDn2pst(x, { date });
    return kind === "dn2snd" ? { ...o, bytes: o.payload } : o;
  };
  if (kind === "fizz") {
    if (sounds.length === 1) return { name: soundFileName(sounds[0].name), bytes: new TextEncoder().encode(soundToFizzsnd(sounds[0])), type: "application/json", notes: [[]] };
    return { name: libraryFileName(date), bytes: makeFizzlib(sounds, { date }), type: "application/zip", notes: sounds.map(() => []) };
  }
  const skipped = sounds.filter((s) => isMidiBlob(s.blob)).map((s) => s.name);
  sounds = sounds.filter((s) => !isMidiBlob(s.blob));
  if (!sounds.length) throw new Error(MIDI_ONLY);
  const used = new Map(), files = [], notes = [];
  for (const s of sounds) {
    const out = write(s);
    let base = fileBase(s.name);
    const n = (used.get(base) || 0) + 1; used.set(base, n);
    if (n > 1) base += ` ${n}`;
    files.push({ name: `${base}.${kind}`, bytes: out.bytes });
    notes.push(out.notes);
  }
  if (files.length === 1) return { name: files[0].name, bytes: files[0].bytes, type: "application/octet-stream", notes, skipped };
  const label = kind === "dnsnd" ? "Digitone" : "Digitone II";
  return { name: `FIZZ presets ${date.toISOString().slice(0, 10)} (${label} ${kind}).zip`, bytes: zipStored(files, date), type: "application/zip", notes, skipped };
}

export function mountExport({ download, log = () => {} }) {
  const X = { sounds: [], pick: "dnsnd", dn2: "dn2pst", el: null, title: "", tags: [] };
  const box = document.createElement("div");
  box.className = "scrim";
  box.id = "ex";
  box.hidden = true;
  box.innerHTML = '<div class="modal ex" id="ex-modal" role="dialog" aria-labelledby="ex-title"></div>';
  document.body.appendChild(box);
  const modal = box.firstElementChild;
  const kindOf = (k) => (k === "dn2" ? X.dn2 : k);
  // the notes of every sound for a format, merged (several: "in n" after each)
  function rowsFor(k) {
    if (k === "fizz") return carryOf([]);
    const midi = X.sounds.filter((s) => isMidiBlob(s.blob)).length;   // (FIZZ MIDI (e): left out, its row)
    if (midi === X.sounds.length) return { lost: midi, changed: 0, bar: 0, rows: [{ ...MIDI_ROW, n: midi }], touched: midi, none: true };
    const all = X.sounds.filter((s) => !isMidiBlob(s.blob)).map((s) => {
      const x = { blob: s.blob, name: s.name, tags: s.fizzTags || [], pan: s.pan | 0, sends: s.sends || {}, drive: s.drive | 0 };
      return (k === "dnsnd" ? fizzToDnsnd(x) : fizzToDn2pst(x)).notes;
    });
    if (midi) {
      const one = carryOf(all.flat()), by = new Map([["MIDI", { ...MIDI_ROW, n: midi }]]);
      for (const r of one.rows) { const key = `${r.what}|${r.how}`; const e = by.get(key) || { ...r, n: 0 }; e.n++; by.set(key, e); }
      const touched = midi + all.filter((n) => n.length).length;
      return { ...one, lost: one.lost + midi, rows: [...by.values()], touched, bar: Math.max(1, Math.min(9, Math.round(10 - 9 * touched / X.sounds.length))) };
    }
    if (all.length === 1) return carryOf(all[0]);
    const one = carryOf(all.flat()), by = new Map();
    for (const r of one.rows) { const key = `${r.what}|${r.how}`; const e = by.get(key) || { ...r, n: 0 }; e.n++; by.set(key, e); }
    const touched = all.filter((n) => n.length).length;
    return { ...one, rows: [...by.values()], touched, bar: touched ? Math.max(1, Math.min(9, Math.round(10 - 9 * touched / all.length))) : 10 };
  }
  function render() {
    const many = X.sounds.length > 1, cur = rowsFor(X.pick === "dn2" ? "dn2pst" : X.pick);
    const name = (() => { try { return exportName(); } catch (_) { return ""; } })();
    modal.innerHTML = `<div class="row exhead"><div style="min-width:0"><div class="h" id="ex-title" style="margin:0">EXPORT</div><div class="big">${esc(X.title)}</div></div>
        ${X.tags.map((t) => `<span class="tag t-${t}">${t}</span>`).join("")}</div>
      <div class="fmts" role="radiogroup" aria-label="Format">${KINDS.map((f) => {
        const c = rowsFor(f.k === "dn2" ? "dn2pst" : f.k);
        const txt = c.none ? (many ? "MIDI sounds: none written" : "a MIDI sound: not written") : many && f.k !== "fizz" ? (c.touched ? `${c.touched} of ${X.sounds.length} lose something` : "everything") : carryText(c);
        return `<button class="fmt ${X.pick === f.k ? "on" : ""}" role="radio" aria-checked="${X.pick === f.k}" data-k="${f.k}" style="--c:var(${f.c})">
          <span class="ext">${f.ext}${f.exp ? '<span class="expmark">EXPERIMENTAL</span>' : ""}</span><span class="t">${f.title}</span>
          <span class="fid">${Array.from({ length: 10 }, (_, k) => `<i style="background:${k < c.bar ? barC(c.bar) : "#24242b"}"></i>`).join("")}</span>
          <span class="kv carry ${c.rows.length ? "" : "all"}">${esc(txt)}</span></button>`;
      }).join("")}</div>
      ${X.pick === "dn2" ? `<div class="seg dn2seg" role="group" aria-label="Digitone II file" style="--c:var(--magenta)">${["dn2pst", "dn2snd"].map((k) => `<button data-dn2="${k}" class="${X.dn2 === k ? "on" : ""}" aria-pressed="${X.dn2 === k}">.${k}</button>`).join("")}</div>` : ""}
      <div class="lostlist">${cur.rows.map((r) => `<div class="lost" style="--c:var(${r.c})" title="${esc(r.note)}"><span class="what">${esc(r.what)}</span><span class="how">${esc(r.how)}</span>${r.n > 1 ? `<span class="val">×${r.n}</span>` : ""}</div>`).join("")}</div>
      <div class="row exfoot"><span class="kv exname">${esc(name)}</span><button class="btn" id="ex-go" style="--c:var(${KINDS.find((f) => f.k === X.pick).c})" ${cur.none ? "disabled" : ""}>↓ DOWNLOAD</button><button class="btn ghost" id="ex-no">CANCEL</button></div>`;
    modal.querySelectorAll("[data-k]").forEach((b) => b.addEventListener("click", () => { X.pick = b.dataset.k; render(); }));
    modal.querySelectorAll("[data-dn2]").forEach((b) => b.addEventListener("click", () => { X.dn2 = b.dataset.dn2; render(); }));
    modal.querySelector("#ex-go").addEventListener("click", go);
    modal.querySelector("#ex-no").addEventListener("click", close);
  }
  const exportName = () => exportFiles(X.sounds, kindOf(X.pick)).name;
  function go() {
    let f;
    try { f = exportFiles(X.sounds, kindOf(X.pick)); } catch (e) { log(`export: ${e.message}`); return; }
    download(f.bytes, f.name, f.type);
    log(`export: ${X.sounds.length - (f.skipped || []).length} as ${kindOf(X.pick)} (${f.name})${f.skipped && f.skipped.length ? `, ${f.skipped.length} MIDI left out` : ""}`);
    close();
  }
  const key = (ev) => { if (ev.key === "Escape") { ev.preventDefault(); close(); } };
  function close() { box.hidden = true; document.removeEventListener("keydown", key, true); }
  box.addEventListener("click", (ev) => { if (ev.target === box) close(); });
  return {
    open(sounds, { title = "", tags = [], pick } = {}) {
      X.sounds = sounds; X.title = title || (sounds.length === 1 ? sounds[0].name : `${sounds.length} PRESETS`);
      X.tags = tags; if (pick) X.pick = pick;
      render();
      box.hidden = false;
      document.addEventListener("keydown", key, true);
      const g = modal.querySelector("#ex-go"); if (g) g.focus();
    },
    close, get open_() { return !box.hidden; },
  };
}
