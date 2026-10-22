// SPDX-License-Identifier: GPL-3.0-only
// "Import sounds": the header's button and a drop anywhere on the page open a panel that reads Digitone .dnsnd, Digitone
// II .dn2pst and FIZZ's own .fizzsnd / .fizzlib files (in this browser: nothing is uploaded), lists each sound with what
// was lost or approximated (and, with FIZZ's preset list, which are already in MY PRESETS), and sends them to FIZZ's
// library (default) or the current project's pool, the first one optionally loaded onto a track with its pan and
// sends (js/digitone.js: the reader and FIZZ's SysEx).
import { h } from "./sheet.js";
import { readFiles, createSender, notesWithLoad, IMPORTABLE } from "./digitone.js";
import { soundFromStoreFrame, sameSound } from "./library.js";

const X_ICON = '<svg class="ic" viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linecap="round"><path d="M4 4l8 8M12 4l-8 8"/></svg>';

// presets: js/library.js's link (LIST / DUMP), for the duplicate check (by name and patch: skip or keep both)
export function mountDigitone({ button, sendMidi, onMidiOut, ensureRunning, toast, presets = null }) {
  const sender = createSender({ sendMidi, onMidiOut });
  let items = [], pool = false, track = -1, busy = false, fizz = null, asked = false, keepDups = false;

  const fileIn = h("input", { type: "file", accept: ".dnsnd,.dn2pst,.dn2snd,.fizzsnd,.fizzlib", multiple: true, hidden: true,
    onchange: (ev) => { const f = [...ev.target.files]; ev.target.value = ""; if (f.length) add(f); } });
  const list = h("ul", { class: "dn-list" });
  const seg = h("div", { class: "seg", role: "radiogroup", "aria-label": "Where the sounds go" });
  const trackSel = h("select", { class: "select", "aria-label": "Also load the first sound onto a track",
    onchange: (ev) => { track = +ev.target.value; render(); } },
    h("option", { value: "-1" }, "No"), [1, 2, 3, 4, 5, 6, 7, 8].map((n) => h("option", { value: String(n - 1) }, `Track ${n}`)));
  const dupSeg = h("div", { class: "seg", role: "radiogroup", "aria-label": "Sounds already in MY PRESETS" });
  const dupRow = h("div", { class: "dn-row", hidden: true }, h("span", { class: "row-label" }, "Already in MY PRESETS"), dupSeg);
  const send = h("button", { class: "pill primary dn-send", onclick: () => go() });
  const who = h("p", { class: "dn-who small" });
  const foot = h("p", { class: "note dn-foot" });
  const panel = h("div", { class: "dn-panel", role: "dialog", "aria-label": "Import sounds", hidden: true },
    h("div", { class: "dn-head" }, h("b", {}, "Import sounds"), h("button", { class: "icon-btn", "aria-label": "Close", onclick: () => close(), html: X_ICON })),
    h("p", { class: "dn-lead" }, "FIZZ presets (.fizzsnd, .fizzlib) and Elektron Digitone (.dnsnd) and Digitone II (.dn2pst) FM TONE sounds. They are read and converted here, in this browser: nothing is uploaded."),
    h("div", { class: "dn-pick" }, h("button", { class: "pill", onclick: () => fileIn.click() }, "Choose files..."), h("span", { class: "muted small" }, "or drop them on the page"), fileIn),
    list,
    h("div", { class: "dn-row" }, h("span", { class: "row-label" }, "Into"), seg),
    h("div", { class: "dn-row" }, h("span", { class: "row-label" }, "Also load the first onto"), trackSel),
    dupRow, send, who, foot);
  document.body.append(panel);
  const drop = h("div", { class: "dn-drop", hidden: true }, h("div", {}, h("b", {}, "Drop sounds"), h("span", {}, ".fizzsnd, .fizzlib, .dnsnd and .dn2pst files, read in this browser")));
  document.body.append(drop);

  // who is listening: FIZZ's identity reply (asked once FIZZ runs and the panel opens)
  async function identify() {
    if (asked) return;
    asked = true;
    try { await ensureRunning(); } catch (_) { asked = false; return; }
    for (let i = 0; i < 20 && !fizz; i++) {             // (FIZZ answers once it has booted: a few tries)
      fizz = await sender.identify();
      if (!fizz) await new Promise((r) => setTimeout(r, 250));
    }
    render();
  }
  function open() { panel.hidden = false; button.setAttribute("aria-expanded", "true"); render(); identify(); }
  function close() { panel.hidden = true; button.setAttribute("aria-expanded", "false"); }
  button.addEventListener("click", () => (panel.hidden ? open() : close()));
  addEventListener("keydown", (e) => { if (e.key === "Escape" && !panel.hidden) { e.stopPropagation(); close(); } }, true);

  async function add(files) {
    const read = await Promise.all(files.map(async (f) => ({ name: f.name, bytes: new Uint8Array(await f.arrayBuffer()) })));
    const got = await readFiles(read);
    items = items.concat(got.map((g) => ({ ...g, result: null })));
    open();
    await checkDups();
  }
  // the duplicate check: FIZZ's MY PRESETS (LIST), and for each same name its patch (DUMP)
  async function checkDups() {
    if (!presets || !presets.ready) return;
    try { await ensureRunning(); } catch (_) { return; }
    const listed = (await presets.list()) || [];
    const cache = new Map();
    for (const it of items) {
      if (!it.ok || it.result) continue;
      it.dup = 0;
      for (const p of listed.filter((x) => x.name === it.sound.name)) {
        if (!cache.has(p.slot)) { const f = await presets.dump(p.slot); cache.set(p.slot, f ? soundFromStoreFrame(f) : null); }
        if (sameSound(cache.get(p.slot), it.sound)) { it.dup = p.slot; break; }
      }
    }
    render();
  }

  const pending = () => items.filter((it) => it.ok && !(it.result && (it.result.ok || it.result.skipped)));
  function render() {
    const first = pending()[0];
    list.replaceChildren(...items.map((it, i) => {
      const x = h("button", { class: "icon-btn dn-x", "aria-label": `Remove ${it.file}`, onclick: () => { items.splice(i, 1); render(); }, html: X_ICON });
      if (!it.ok) return h("li", { class: "dn-item bad" }, h("div", { class: "dn-top" }, h("b", {}, it.file), x),
        h("div", { class: "dn-err" }, /FM TONE sounds only/.test(it.error) ? `Not importable: ${it.error}` : it.error));
      const s = it.sound, r = it.result;
      const loads = r ? r.ok && r.loaded >= 0 : it === first && track >= 0;
      const notes = loads ? notesWithLoad(s) : s.notes;
      const where = r && r.ok ? `${r.pool ? "POOL" : "MY PRESETS"} ${r.slot}${r.loaded >= 0 ? `, on track ${r.loaded + 1}` : ""}` : r ? r.error
        : it.dup ? `already MY PRESETS ${it.dup}` : loads ? `+ track ${track + 1}` : null;
      return h("li", { class: "dn-item" + (r ? (r.ok ? " sent" : r.skipped ? " skipped" : " failed") : it.dup ? " dup" : "") },
        h("div", { class: "dn-top" }, h("b", {}, s.name), s.dn2 ? h("span", { class: "dn-chip", title: "a Digitone II sound" }, "DN2") : null,
          ...(s.fizzTags || []).map((t) => h("span", { class: "dn-tag", "data-tag": t, title: `FIZZ's ${t} tag, from the Digitone's ${s.tags.join(", ")}` }, t)),
          h("span", { class: "muted small dn-file" }, it.file),
          where ? h("span", { class: "dn-res" }, where) : null, x),
        s.mark ? h("div", { class: "dn-mark", title: s.origin.join("\n") }, s.mark) : null,
        notes.length ? h("details", { class: "dn-notes" }, h("summary", {}, `${notes.length} thing${notes.length === 1 ? "" : "s"} lost or approximated`),
          h("ul", {}, notes.map((n) => h("li", {}, n)))) : h("div", { class: "muted small" }, "converted whole"));
    }));
    if (!items.length) list.append(h("li", { class: "dn-empty muted small" }, "No sounds yet."));
    seg.replaceChildren(...[[false, "My presets"], [true, "This project's pool"]].map(([k, label]) =>
      h("button", { class: pool === k ? "on" : "", role: "radio", "aria-checked": String(pool === k), onclick: () => { pool = k; render(); } }, label)));
    trackSel.value = String(track);
    const dups = pending().filter((it) => it.dup).length;
    dupRow.hidden = !dups;
    dupSeg.replaceChildren(...[[false, `Skip ${dups}`], [true, "Keep both"]].map(([k, label]) =>
      h("button", { class: keepDups === k ? "on" : "", role: "radio", "aria-checked": String(keepDups === k), onclick: () => { keepDups = k; render(); } }, label)));
    const n = pending().length;
    send.textContent = busy ? "Sending..." : n ? `Send ${n} sound${n === 1 ? "" : "s"} to FIZZ` : "Send to FIZZ";
    send.disabled = busy || !n;
    who.textContent = fizz ? `${fizz} is listening.` : asked ? "Waiting for FIZZ..." : "";
    foot.textContent = (pool ? "Into the project you are on: PRESETS picks them. " : "Into MY PRESETS, shared by every project: PRESETS finds them. ")
      + "Each keeps its pan, its reverb, delay and chorus sends and its tags. "
      + (track >= 0 ? `The first also goes onto track ${track + 1}, with its mix. ` : "")
      + "SHIFT + F1 on FIZZ undoes the whole import.";
  }

  async function go() {
    for (const it of pending()) if (it.dup && !keepDups) it.result = { ok: false, skipped: true, error: `skipped: already MY PRESETS ${it.dup}` };
    const todo = pending();
    if (!todo.length || busy) { render(); return; }
    busy = true; render();
    try {
      await ensureRunning();
      if (!fizz) { asked = false; await identify(); }
      const res = await sender.send(todo.map((it) => it.sound), { pool, track }, (i, r) => { todo[i].result = r; render(); });
      const ok = res.filter((r) => r.ok).length, full = res.find((r) => r.full);
      if (full) for (const it of todo.slice(res.length)) it.result = { ok: false, error: "not sent: " + full.error };
      const where = pool ? "pool" : "MY PRESETS";
      toast(full ? `${ok} sound${ok === 1 ? "" : "s"} in FIZZ's ${where}, then ${full.error}: the rest were not sent.`
        : ok === todo.length ? `${ok} sound${ok === 1 ? "" : "s"} in FIZZ's ${where}. SHIFT + F1 undoes the import.`
        : `${ok} of ${todo.length} sounds went in; see the list.`, ok && !full && ok === todo.length ? "ok" : "bad", 7000);
      if (ok) track = -1;                               // (the load was the first sound's: not again for the next batch)
    } finally { busy = false; render(); }
  }

  // drag and drop anywhere on the page (files only)
  let depth = 0;
  const hasFiles = (e) => e.dataTransfer && [...e.dataTransfer.types].includes("Files");
  addEventListener("dragenter", (e) => { if (!hasFiles(e)) return; depth++; drop.hidden = false; });
  addEventListener("dragleave", (e) => { if (!hasFiles(e)) return; if (--depth <= 0) { depth = 0; drop.hidden = true; } });
  addEventListener("dragover", (e) => { if (hasFiles(e)) e.preventDefault(); });
  addEventListener("drop", (e) => {
    if (!hasFiles(e)) return;
    e.preventDefault(); depth = 0; drop.hidden = true;
    const dn = [...e.dataTransfer.files].filter((x) => IMPORTABLE.test(x.name));
    if (!dn.length) { toast("Only FIZZ (.fizzsnd, .fizzlib), Digitone (.dnsnd) and Digitone II (.dn2pst) files can be dropped here.", "bad"); return; }
    add(dn);
  });
  render();
  return { open, close, add, send: go, pick: () => fileIn.click(), setKeepDups: (v) => { keepDups = !!v; render(); },
    get items() { return items; }, get fizz() { return fizz; } };
}
