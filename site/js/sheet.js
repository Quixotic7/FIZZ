// SPDX-License-Identifier: GPL-3.0-only
// The settings sheet: FM1VST plugin/Sheet.cpp's Graphite sheet, as DOM (RemoteFM1's web/site/js/sheet.js, cut to one
// firmware: no firmware browser, engine or recorder). It slides in from the right over a dimmed scrim; a click on the
// scrim or Esc closes it. Pages:
//   main      FIZZ, SAVES, USE MY FM-1, PLAYING, LOOK, VIEW, ABOUT
//   about     FIZZ's credits (its descriptor's), the site's and RemoteFM1's
// app (main.js) holds the state and the actions; the sheet re-renders its page on render().
import { PRESETS, accentOf, onAccent, css, hex } from "./theme.js";
import { AUTOSAVE } from "./saves.js";

// ---- icons: FM1VST Look::icon, 16 x 16 boxes
const ICON = {
  dots: '<circle cx="3.5" cy="8" r="1.5"/><circle cx="8" cy="8" r="1.5"/><circle cx="12.5" cy="8" r="1.5"/>',
  left: '<path d="M10 3.5 5.5 8 10 12.5"/>', right: '<path d="M6 3.5 10.5 8 6 12.5"/>', down: '<path d="M4 6l4 4 4-4"/>',
  close: '<path d="M4 4l8 8M12 4l-8 8"/>', search: '<circle cx="7" cy="7" r="4.5"/><path d="M10.2 10.2 13.5 13.5"/>',
  check: '<path d="M3.5 8.5l3 3 6-7"/>', info: '<circle cx="8" cy="8" r="6"/><path d="M8 7.2v4"/><circle cx="8" cy="4.9" r=".4"/>',
  plus: '<path d="M8 3.5v9M3.5 8h9"/>', file: '<path d="M4 2.5h5l3 3v8H4z"/><path d="M9 2.5v3h3"/>',
};
export const icon = (name, cls = "ic") =>
  `<svg class="${cls}" viewBox="0 0 16 16" aria-hidden="true" fill="${name === "dots" ? "currentColor" : "none"}" stroke="currentColor" stroke-width="1.6" stroke-linecap="round" stroke-linejoin="round">${ICON[name]}</svg>`;

// ---- a tiny DOM helper
export function h(tag, attrs = {}, ...kids) {
  const el = document.createElement(tag);
  for (const [k, v] of Object.entries(attrs || {})) {
    if (v == null || v === false) continue;
    if (k === "class") el.className = v;
    else if (k === "html") el.innerHTML = v;
    else if (k === "style" && typeof v === "object") Object.assign(el.style, v);
    else if (k.startsWith("on")) el.addEventListener(k.slice(2), v);
    else el.setAttribute(k, v === true ? "" : v);
  }
  for (const c of kids.flat()) if (c != null && c !== false) el.append(c.nodeType ? c : document.createTextNode(String(c)));
  return el;
}
const heading = (text, extra) => h("h3", { class: "heading" }, text, extra || null);
const row = (label, sub, control, opts = {}) =>
  h("div", { class: "row" + (opts.disabled ? " disabled" : "") }, h("div", { class: "row-label" }, h("div", {}, label), sub ? h("div", { class: "row-sub" }, sub) : null), control);
function toggle(on, onChange, disabled) {
  return h("button", { class: "switch" + (on ? " on" : ""), role: "switch", "aria-checked": String(!!on), disabled, onclick: () => onChange(!on) }, h("span"));
}
function segmented(labels, sel, onPick, disabledSet = new Set()) {
  return h("div", { class: "seg", role: "radiogroup" }, labels.map((l, i) =>
    h("button", { class: i === sel ? "on" : "", role: "radio", "aria-checked": String(i === sel), disabled: disabledSet.has(i), onclick: () => onPick(i) }, l)));
}
function stepper(value, onStep) {
  return h("div", { class: "stepper" },
    h("button", { "aria-label": "less", onclick: () => onStep(-1), html: '<svg viewBox="0 0 16 16"><path d="M3.5 8h9"/></svg>' }),
    h("span", {}, value),
    h("button", { "aria-label": "more", onclick: () => onStep(1), html: '<svg viewBox="0 0 16 16"><path d="M3.5 8h9M8 3.5v9"/></svg>' }));
}
const pill = (text, onclick, kind = "", extra = {}) => h("button", { class: "pill " + kind, onclick, ...extra }, text);
const link = (href, text) => h("a", { href, target: "_blank", rel: "noopener" }, text);


export class Sheet {
  constructor(app) {
    this.app = app;
    this.page = "main";
    this.inline = null;          // { kind: "saveas" | "rename" | "delete" | "erase", rec }
    this.scrim = h("div", { class: "scrim", onclick: () => this.close() });
    this.head = h("header", { class: "sheet-head" });
    this.body = h("div", { class: "sheet-body" });
    this.el = h("aside", { class: "sheet", "aria-label": "Settings", "aria-hidden": "true", tabindex: "-1" }, this.head, this.body);
    document.body.append(this.scrim, this.el);
    this.remoteSlot = h("div", { class: "remote-slot" });
  }
  get isOpen() { return this.el.classList.contains("open"); }
  open(page = "main") {
    this.show(page);
    this.el.classList.add("open"); this.scrim.classList.add("open");
    this.el.setAttribute("aria-hidden", "false");
    this.el.focus({ preventScroll: true });
    this.app.sheetChanged(true);
  }
  close() {
    this.el.classList.remove("open"); this.scrim.classList.remove("open");
    this.el.setAttribute("aria-hidden", "true");
    this.inline = null;
    this.app.sheetChanged(false);
  }
  show(page) {
    this.page = page;
    this.el.dataset.page = page;
    this.render();
    this.body.scrollTop = 0;
  }
  render() {
    const a = this.app, st = a.state;
    const accent = accentOf(st.theme);
    this.el.style.setProperty("--accent", css(accent));
    this.el.style.setProperty("--on-accent", css(onAccent(accent)));
    this.el.style.setProperty("--accent-soft", css(accent, 0.12));
    this.head.replaceChildren(
      this.page === "main"
        ? h("div", { class: "brand" }, h("span", { class: "fizz-mark", "aria-hidden": "true" }), h("b", {}, "FIZZ"), h("span", { class: "ver" }, st.meta ? st.meta.version : ""))
        : h("div", { class: "brand" }, h("button", { class: "icon-btn", "aria-label": "Back", onclick: () => this.show("main"), html: icon("left") }), h("b", {}, "About")),
      h("button", { class: "icon-btn", "aria-label": "Close", onclick: () => this.close(), html: icon("close") }));
    const keep = this.page === this.lastPage ? this.body.scrollTop : 0;
    this.body.replaceChildren(...(this.page === "about" ? this.aboutPage() : this.mainPage()));
    this.body.scrollTop = keep;
    this.lastPage = this.page;
  }

  // ================================================================ main ===
  mainPage() {
    const a = this.app, st = a.state;
    const out = [];
    // FIZZ: the preview's version, the guide and the feedback form
    out.push(heading("FIZZ"));
    out.push(h("div", { class: "fizz-card" },
      h("p", {}, "An FM TONE step sequencer for the M-VAVE FM-1, in preview. It runs here exactly as it will on the unit."),
      h("div", { class: "pills two" }, h("a", { class: "pill", href: a.guideUrl }, "Guide"),
        h("a", { class: "pill primary", href: a.feedbackUrl(), target: "_blank", rel: "noopener" }, "Feedback")),
      h("div", { class: "pills two" }, pill("Import sounds", () => a.openImport && a.openImport()), pill("My presets", () => a.openPresets && a.openPresets()))));

    // SAVES
    out.push(heading("Saves"));
    out.push(...this.savesSection());

    // USE MY FM-1
    out.push(heading("Use my FM-1"));
    out.push(this.remoteSection());

    // PLAYING
    out.push(heading("Playing"));
    {
      const pct = Math.round(st.volume * 100), val = h("span", { class: "vol-val" }, `${pct}%`);
      const range = h("input", { type: "range", class: "vol", min: "0", max: "100", step: "1", value: String(pct), "aria-label": "Page volume",
        oninput: (ev) => { val.textContent = ev.target.value + "%"; a.setVolume(ev.target.value / 100, false); },
        onchange: (ev) => a.setVolume(ev.target.value / 100, true) });
      out.push(row("Volume", "this page's output", h("div", { class: "vol-wrap" }, range, val)));
    }
    const m = a.midi;
    let midiCtl;
    if (!m.supported) midiCtl = h("span", { class: "muted small" }, "no Web MIDI here");
    else if (!m.access) midiCtl = pill("Enable MIDI", () => a.enableMidi());
    else {
      const sel = h("select", { class: "select", "aria-label": "MIDI input", onchange: (ev) => a.selectMidi(ev.target.value) },
        h("option", { value: "" }, "None"), h("option", { value: "*" }, "All inputs"),
        m.inputs().map((i) => h("option", { value: i.id }, i.name + (i.state === "disconnected" ? " (gone)" : ""))));
      sel.value = m.selected;
      midiCtl = sel;
    }
    out.push(row("MIDI input", m.error || (m.access ? `${m.inputs().length} found` : "a MIDI keyboard plays FIZZ's tracks"), midiCtl));
    out.push(row("MIDI notes press the panel's keys", "off: they go to FIZZ's MIDI in, on the track's channel", toggle(st.notesPlayKeys, (v) => a.set("notesPlayKeys", v))));
    if (st.notesPlayKeys) {
      out.push(row("Transpose", "octaves, for those notes", stepper(String(st.transpose), (d) => a.set("transpose", Math.max(-3, Math.min(3, st.transpose + d))))));
      out.push(row("...and reach FIZZ's MIDI in too", null, toggle(st.reachFirmware, (v) => a.set("reachFirmware", v))));
    }
    out.push(row("Show computer-key hints", "the keyboard letters on the panel's keys and buttons", toggle(st.keyHints, (v) => a.set("keyHints", v))));
    out.push(h("p", { class: "note" }, "Steps W1-W16: 2 3 4 Tab, then A S D F G H J K L ; ' ]. Black keys B1-B11: F1-F4, E T Y U O P [. Buttons: C V B N M , (FX TRACKS MIX LATCH EDIT SHIFT) and 7 8 9 0 - = (HOME SOUNDS FILL PATTERNS PLAY REC). Z X are F1 F2, End both (F3). Page Up/Down pick a knob, arrows turn it; or drag a knob, or use the wheel. ` the screen view. ",
      h("a", { href: a.guideUrl + "#keys" }, "The key map")));

    // RECORDING (js/rec-ui.js: the header's record button, Shift+R)
    out.push(heading("Recording"));
    out.push(row("WAV format", "PCM, stereo, at the sound card's rate", segmented(["24-bit", "16-bit"], st.recBits === 16 ? 1 : 0, (i) => a.set("recBits", i ? 16 : 24))));
    out.push(row("Count-in", "3 seconds before a take starts", toggle(st.recCountIn, (v) => a.set("recCountIn", v))));
    out.push(h("p", { class: "note" }, "The record button (or Shift+R) records FIZZ's sound exactly as it renders, before this page's volume; the FM-1's MASTER knob counts. Takes stay in this page until you close it (up to 30 minutes each): save the ones to keep."));

    // LOOK
    out.push(heading("Look", h("span", { class: "heading-val" }, st.theme.name)));
    out.push(h("div", { class: "swatches" }, PRESETS.map((t) => {
      const b = h("button", { class: "swatch" + (t.name === st.theme.name ? " on" : ""), title: t.name, "aria-label": t.name, onclick: () => a.set("theme", t.name) });
      b.style.background = `linear-gradient(135deg, ${t.base} 0 50%, ${t.membrane} 50% 100%)`;
      return b;
    })));

    // VIEW
    out.push(heading("View"));
    out.push(row("Show", null, segmented(["Device", "Screen"], st.view === "screen" ? 1 : 0, (i) => a.set("view", i ? "screen" : "device"))));
    out.push(row("Live labels", "what each knob, button and key does right now, from FIZZ; hold SHIFT for its shortcuts", toggle(st.labels, (v) => a.set("labels", v))));

    // ABOUT
    out.push(heading("About"));
    out.push(h("button", { class: "fw-card slim", onclick: () => this.show("about") },
      h("span", { class: "ic-wrap", html: icon("info") }),
      h("span", { class: "fw-text" }, h("span", { class: "fw-top" }, h("b", {}, "FIZZ and credits")),
        h("span", { class: "fw-meta" }, "GPL-3.0-only · FM TONE from ChoralRoot FM-1")),
      h("span", { class: "chev", html: icon("right") })));
    out.push(h("div", { class: "links" }, link("https://github.com/Quixotic7/FIZZ", "FIZZ on GitHub"), " · ",
      link(a.installerUrl, "Install RemoteFM1 on your FM-1")));
    return out;
  }

  savesSection() {
    const a = this.app, st = a.state, saves = st.saves || [];
    const out = [];
    // a save of an older FIZZ, offered once (main.js offerCarry)
    if (st.carryOffer) out.push(h("div", { class: "carry" }, a.carryText(st.carryOffer),
      h("div", { class: "pills two" }, pill("Bring it", () => a.answerCarry(true), "primary"), pill("Start fresh", () => a.answerCarry(false), "ghost"))));
    const mine = saves.filter((r) => r.version === a.version());
    const named = mine.filter((r) => r.name !== AUTOSAVE);
    const sel = named.find((r) => r.name === st.selectedSave) || null;
    const auto = mine.find((r) => r.name === AUTOSAVE);
    const names = named.map((r) => r.name);
    const i = sel ? names.indexOf(sel.name) : -1;
    const step = (d) => { if (!names.length) return; const j = ((i < 0 ? (d > 0 ? -1 : 0) : i) + d + names.length) % names.length; a.loadSave(named[j]); };
    const picker = h("select", { class: "nav-select", "aria-label": "Saves", onchange: (ev) => { const r = named.find((x) => x.name === ev.target.value); if (r) a.loadSave(r); } },
      h("option", { value: "" }, auto ? "Autosave (running)" : "No save yet"), names.map((n) => h("option", { value: n }, n)));
    picker.value = sel ? sel.name : "";
    out.push(h("div", { class: "nav" },
      h("button", { class: "nav-arrow", "aria-label": "Previous save", onclick: () => step(-1), html: icon("left") }),
      h("div", { class: "nav-mid" }, picker, h("span", { class: "nav-chev", html: icon("down") })),
      h("button", { class: "nav-arrow", "aria-label": "Next save", onclick: () => step(1), html: icon("right") })));
    if (this.inline && (this.inline.kind === "saveas" || this.inline.kind === "rename")) {
      const input = h("input", { class: "text", type: "text", maxlength: "40", placeholder: "Name", value: this.inline.kind === "rename" && sel ? sel.name : "" });
      const ok = async () => {
        const n = input.value.trim();
        if (!n || n === AUTOSAVE) return input.focus();
        const kind = this.inline.kind;
        this.inline = null;
        if (kind === "saveas") await a.saveAs(n); else if (sel) await a.renameSave(sel, n);
      };
      input.addEventListener("keydown", (ev) => { if (ev.key === "Enter") ok(); });
      out.push(h("div", { class: "inline" }, input, pill(this.inline.kind === "saveas" ? "Save" : "Rename", ok, "primary"), pill("Cancel", () => { this.inline = null; this.render(); }, "ghost")));
      setTimeout(() => input.focus(), 0);
    } else if (this.inline && this.inline.kind === "delete" && sel) {
      out.push(h("div", { class: "inline" }, h("span", { class: "grow" }, `Delete "${sel.name}"?`),
        pill("Delete", async () => { this.inline = null; await a.deleteSave(sel); }, "danger-fill"), pill("Cancel", () => { this.inline = null; this.render(); }, "ghost")));
    } else if (this.inline && this.inline.kind === "erase") {
      out.push(h("div", { class: "inline" }, h("span", { class: "grow" }, "Erase FIZZ's flash and start blank? (named saves stay)"),
        pill("Erase", async () => { this.inline = null; await a.eraseFlash(); }, "danger-fill"), pill("Cancel", () => { this.inline = null; this.render(); }, "ghost")));
    }
    const fileIn = h("input", { type: "file", accept: ".json,.bin,.fm1save,application/json,application/octet-stream", hidden: true,
      onchange: (ev) => { const f = ev.target.files[0]; ev.target.value = ""; if (f) a.importSaves(f); } });
    out.push(h("div", { class: "pills" },
      pill("Save", () => a.saveOver(sel), "", { disabled: !sel || !st.running, title: sel ? `Write the running flash over "${sel.name}"` : "Pick a save first" }),
      pill("Save as", () => { this.inline = { kind: "saveas" }; this.render(); }, "", { disabled: !st.running }),
      pill("Rename", () => { this.inline = { kind: "rename" }; this.render(); }, "", { disabled: !sel }),
      pill("Delete", () => { this.inline = { kind: "delete" }; this.render(); }, "danger", { disabled: !sel }),
      pill("Export...", () => a.exportSaves(), "", { disabled: !saves.length, title: "A backup file of every save" }),
      pill("Import...", () => fileIn.click()), fileIn,
      pill("Start blank", () => { this.inline = { kind: "erase" }; this.render(); }, "danger", { disabled: !st.running, title: "Erase the running flash: FIZZ starts as new" })));
    out.push(h("p", { class: "note" }, auto
      ? `FIZZ saves its projects into its flash by itself; this browser keeps that flash (last kept ${new Date(auto.saved).toLocaleString()}). A named save is a copy you keep; picking one restarts FIZZ from it.`
      : "FIZZ saves its projects into its flash by itself; this browser keeps that flash. A named save is a copy you keep; picking one restarts FIZZ from it."));
    return out;
  }

  remoteSection() {
    const a = this.app;
    if (!a.remoteMounted) {
      this.remoteSlot.replaceChildren(
        h("p", { class: "row-sub", style: { margin: "0 0 10px" } }, "Play FIZZ from your own FM-1: its keys, knobs and screen, the sound here. The FM-1 needs RemoteFM1."),
        h("div", { class: "pills two" },
          pill("Connect my FM-1", null, "", { disabled: true, title: "Loading..." }),
          h("a", { class: "pill", href: a.installerUrl, target: "_blank", rel: "noopener" }, "Install RemoteFM1")),
        h("p", { class: "remote-status muted small" }, a.remoteStatus || "Loading..."));
    }
    return this.remoteSlot;
  }

  // =============================================================== about ===
  aboutPage() {
    const a = this.app, st = a.state, info = st.info || {}, meta = st.meta || {};
    const out = [];
    const kv = (k, v) => h("div", { class: "kv" }, h("span", { class: "k" }, k), h("span", { class: "v" }, v));
    out.push(heading("FIZZ"));
    out.push(h("p", { class: "about-what" }, "A step sequencer for FM TONE, ChoralRoot's FM voice, made for the M-VAVE FM-1: eight tracks sharing 16 voices, p-locks, conditions, chance, ratchets and chords. This is a preview of the prototype, compiled to WebAssembly from its C source."));
    out.push(kv("Version", meta.version || info.version || ""));
    if (meta.commit) out.push(kv("Build", h("span", {}, link(`https://github.com/Quixotic7/FIZZ/tree/${meta.commit}`, meta.commit.slice(0, 7)), ` · ${meta.built ? meta.built.slice(0, 10) : ""} · ${meta.bytes ? Math.round(meta.bytes / 1024) + " KiB" : ""}`)));
    out.push(kv("Licence", "GPL-3.0-only"));
    out.push(kv("Source", link("https://github.com/Quixotic7/FIZZ", "github.com/Quixotic7/FIZZ")));
    if (meta.fm1vst) out.push(kv("Glue", h("span", {}, "FM1VST's core ABI: ", link(`https://github.com/Quixotic7/FM1VST`, "FM1VST"), ` (${meta.fm1vst.slice(0, 7)})`)));
    if (info.credits) out.push(h("pre", { class: "credits" }, String(info.credits)));
    out.push(heading("Credits and notices"));
    out.push(h("ul", { class: "credit-list" },
      h("li", {}, "FM TONE, the effects and the output chain: ", link("https://github.com/Quixotic7/ChoralRootFM1", "ChoralRoot FM-1"), " (GPL-3.0-only), a fork of Felucca (Leo Kuroshita, Hügelton Instruments)."),
      h("li", {}, "The player, the panel and Connect FM-1: ", link("https://github.com/Quixotic7/RemoteFM1", "RemoteFM1"), "'s site (GPL-3.0-only): its native engine, AudioWorklet host, saves, the recorder, Shift-click latching, and libremotefm1 compiled with Emscripten (MIT). The panel and the sheet follow ", link("https://github.com/Quixotic7/FM1VST", "FM1VST"), "'s design (GPL-3.0)."),
      h("li", {}, "Drawn with ", link("https://github.com/sammycage/plutovg", "plutovg"), " (MIT). Portions copyright (c) 2006 The FreeType Project."),
      h("li", {}, "Inter Tight by the Inter Project Authors, SIL Open Font License 1.1 (", h("a", { href: "fonts/OFL.txt" }, "OFL.txt"), ")."),
      h("li", {}, "The sequencer's controls owe a lot to the OMX-27 Form sequencer and to Elektron's machines."),
      h("li", {}, "\"M-VAVE\" and \"FM-1\" belong to their owners. Not made or endorsed by M-VAVE.")));
    return out;
  }
}
export { hex };
