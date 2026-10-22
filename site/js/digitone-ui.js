// SPDX-License-Identifier: GPL-3.0-only
// "Digitone sounds": the header's button and a drop anywhere on the page open a panel that reads .dnsnd files (in this
// browser: nothing is uploaded), lists each sound with what was lost or approximated, and sends them to FIZZ's
// library (default) or the current project's pool (js/digitone.js: the reader and the adapter).
import { h } from "./sheet.js";
import { readFiles, createSender } from "./digitone.js";

export function mountDigitone({ button, sendMidi, onMidiOut, ensureRunning, toast }) {
  const sender = createSender({ sendMidi, onMidiOut });
  let items = [], target = "library", busy = false;

  const fileIn = h("input", { type: "file", accept: ".dnsnd", multiple: true, hidden: true,
    onchange: (ev) => { const f = [...ev.target.files]; ev.target.value = ""; if (f.length) add(f); } });
  const list = h("ul", { class: "dn-list" });
  const seg = h("div", { class: "seg", role: "radiogroup", "aria-label": "Where the sounds go" });
  const send = h("button", { class: "pill primary dn-send", onclick: () => go() });
  const foot = h("p", { class: "note dn-foot" });
  const panel = h("div", { class: "dn-panel", role: "dialog", "aria-label": "Digitone sounds", hidden: true },
    h("div", { class: "dn-head" }, h("b", {}, "Digitone sounds"),
      h("button", { class: "icon-btn", "aria-label": "Close", onclick: () => close(), html: '<svg class="ic" viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linecap="round"><path d="M4 4l8 8M12 4l-8 8"/></svg>' })),
    h("p", { class: "dn-lead" }, "Elektron Digitone .dnsnd files become FIZZ sounds. They are read and converted here, in this browser: nothing is uploaded."),
    h("div", { class: "dn-pick" }, h("button", { class: "pill", onclick: () => fileIn.click() }, "Choose files..."), h("span", { class: "muted small" }, "or drop them on the page"), fileIn),
    list,
    h("div", { class: "dn-row" }, h("span", { class: "row-label" }, "Into"), seg),
    send, foot);
  document.body.append(panel);
  const drop = h("div", { class: "dn-drop", hidden: true }, h("div", {}, h("b", {}, "Drop Digitone sounds"), h("span", {}, ".dnsnd files, read in this browser")));
  document.body.append(drop);

  function open() { panel.hidden = false; button.setAttribute("aria-expanded", "true"); render(); }
  function close() { panel.hidden = true; button.setAttribute("aria-expanded", "false"); }
  button.addEventListener("click", () => (panel.hidden ? open() : close()));
  addEventListener("keydown", (e) => { if (e.key === "Escape" && !panel.hidden) { e.stopPropagation(); close(); } }, true);

  async function add(files) {
    const read = await Promise.all(files.map(async (f) => ({ name: f.name, bytes: new Uint8Array(await f.arrayBuffer()) })));
    const got = await readFiles(read);
    items = items.concat(got.map((g) => ({ ...g, result: null })));
    open();
  }

  function render() {
    list.replaceChildren(...items.map((it, i) => {
      const x = h("button", { class: "icon-btn dn-x", "aria-label": `Remove ${it.file}`, onclick: () => { items.splice(i, 1); render(); },
        html: '<svg class="ic" viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linecap="round"><path d="M4 4l8 8M12 4l-8 8"/></svg>' });
      if (!it.ok) return h("li", { class: "dn-item bad" }, h("div", { class: "dn-top" }, h("b", {}, it.file), x), h("div", { class: "dn-err" }, it.error));
      const s = it.sound, r = it.result;
      return h("li", { class: "dn-item" + (r ? (r.ok ? " sent" : " failed") : "") },
        h("div", { class: "dn-top" }, h("b", {}, s.name), h("span", { class: "muted small dn-file" }, it.file),
          r ? h("span", { class: "dn-res" }, r.ok ? `in ${target === "pool" ? "the pool" : "the library"}${r.slot ? ", slot " + r.slot : ""}` : r.error) : null, x),
        s.notes.length ? h("details", { class: "dn-notes" }, h("summary", {}, `${s.notes.length} thing${s.notes.length === 1 ? "" : "s"} lost or approximated`),
          h("ul", {}, s.notes.map((n) => h("li", {}, n)))) : h("div", { class: "muted small" }, "converted whole"));
    }));
    if (!items.length) list.append(h("li", { class: "dn-empty muted small" }, "No sounds yet."));
    seg.replaceChildren(...[["library", "Library"], ["pool", "This project's pool"]].map(([k, label]) =>
      h("button", { class: target === k ? "on" : "", role: "radio", "aria-checked": String(target === k), onclick: () => { target = k; render(); } }, label)));
    const n = items.filter((it) => it.ok && !(it.result && it.result.ok)).length;
    send.textContent = busy ? "Sending..." : `Send ${n || ""} sound${n === 1 ? "" : "s"} to FIZZ`.replace("  ", " ");
    send.disabled = busy || !n || !sender.ready;
    foot.textContent = !sender.ready ? "Sending to FIZZ arrives with its next build (its sound SysEx); reading and converting work now."
      : target === "pool" ? "Into the project you are on: pick them with PRESETS." : "Into the library, shared by every project: SAVE opens it, F2 puts a sound in the pool.";
  }

  async function go() {
    const todo = items.filter((it) => it.ok && !(it.result && it.result.ok));
    if (!todo.length || busy) return;
    busy = true; render();
    try {
      await ensureRunning();
      await sender.send(todo.map((it) => it.sound), target, (i, r) => { todo[i].result = r; render(); });
      const ok = todo.filter((it) => it.result && it.result.ok).length;
      toast(ok ? `${ok} Digitone sound${ok === 1 ? "" : "s"} in FIZZ's ${target === "pool" ? "pool" : "library"}.` : "No sound went in.", ok ? "ok" : "bad");
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
    const f = [...e.dataTransfer.files];
    const dn = f.filter((x) => /\.dnsnd$/i.test(x.name));
    if (!dn.length) { toast("Only Digitone .dnsnd files can be dropped here.", "bad"); return; }
    add(dn);
  });
  render();
  return { open, close, add, get items() { return items; } };
}
