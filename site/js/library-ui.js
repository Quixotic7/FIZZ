// SPDX-License-Identifier: GPL-3.0-only
// "My presets": FIZZ's MY PRESETS listed (its LIST SysEx), any of them exported as files (DUMP): one as a .fizzsnd, several
// as a .fizzlib (js/library.js has the formats). Import goes through the Import sounds panel (js/digitone-ui.js).
import { h } from "./sheet.js";
import { soundFromStoreFrame, soundToFizzsnd, makeFizzlib, soundFileName, libraryFileName } from "./library.js";
import { digitoneExport, KINDS } from "./dnexport.js";

// the review before a Digitone download (EXPERIMENTAL): each preset with what its format cannot hold, then Download
export function digitoneReview(result, kind, { onDownload, onCancel }) {
  return h("div", { class: "dnx-review" },
    h("p", { class: "dnx-warn" }, h("b", {}, `${KINDS[kind].label} (.${kind})`), " · experimental: not yet confirmed to load on a ",
      KINDS[kind].label, ". Each preset, and what does not fit:"),
    h("ul", { class: "dnx-list" }, result.items.map((it) => h("li", {}, h("b", {}, it.file),
      it.notes.length ? h("details", { class: "dn-notes" }, h("summary", {}, `${it.notes.length} thing${it.notes.length === 1 ? "" : "s"} not carried over`),
        h("ul", {}, it.notes.map((n) => h("li", {}, n)))) : h("span", { class: "muted small" }, " fits whole")))),
    h("p", { class: "dnx-file" }, result.name),
    h("div", { class: "pills two" }, h("button", { class: "pill primary", onclick: onDownload }, "Download"),
      h("button", { class: "pill ghost", onclick: onCancel }, "Cancel")));
}

const X_ICON = '<svg class="ic" viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linecap="round"><path d="M4 4l8 8M12 4l-8 8"/></svg>';

export function mountLibrary({ button, presets, ensureRunning, fizzVersion, openImport, toast, save }) {
  let rows = [], sel = new Set(), busy = false, status = "", lastExport = null, fmt = "fizz", review = null;
  const fmtSeg = h("div", { class: "seg", role: "radiogroup", "aria-label": "Export as" });
  const reviewSlot = h("div", {});
  const list = h("ul", { class: "dn-list lib-list" });
  const all = h("input", { type: "checkbox", "aria-label": "Select every preset", onchange: (ev) => { sel = ev.target.checked ? new Set(rows.map((r) => r.slot)) : new Set(); render(); } });
  const author = h("input", { class: "text", type: "text", maxlength: "40", placeholder: "Author (optional)", "aria-label": "Author (optional)" });
  const exp = h("button", { class: "pill primary dn-send", onclick: () => doExport() });
  const line = h("p", { class: "dn-who small" });
  const panel = h("div", { class: "dn-panel lib-panel", role: "dialog", "aria-label": "My presets", hidden: true },
    h("div", { class: "dn-head" }, h("b", {}, "My presets"),
      h("span", { class: "lib-acts" }, h("button", { class: "pill", onclick: () => refresh() }, "Refresh"),
        h("button", { class: "icon-btn", "aria-label": "Close", onclick: () => close(), html: X_ICON }))),
    h("p", { class: "dn-lead" }, "FIZZ's MY PRESETS. Pick some or all and export them as files to keep, share or load back later (Import sounds)."),
    h("label", { class: "lib-all" }, all, h("span", {}, "All")),
    list,
    h("div", { class: "dn-row" }, h("span", { class: "row-label" }, "As"), fmtSeg),
    h("div", { class: "dn-row" }, author),
    exp, reviewSlot, line,
    h("div", { class: "pills two" }, h("button", { class: "pill", onclick: () => { close(); openImport(); } }, "Import presets...")));
  document.body.append(panel);

  async function refresh() {
    if (!presets.ready) { rows = []; status = "Listing MY PRESETS needs a FIZZ that answers LIST (coming with its next build)."; render(); return; }
    status = "Asking FIZZ..."; render();
    try { await ensureRunning(); } catch (_) { /* (no audio yet) */ }
    const got = await presets.list();
    rows = got || [];
    status = got ? `${rows.length} preset${rows.length === 1 ? "" : "s"} in MY PRESETS.` : "FIZZ did not answer.";
    sel = new Set([...sel].filter((s) => rows.some((r) => r.slot === s)));
    render();
  }
  function open() { panel.hidden = false; button.setAttribute("aria-expanded", "true"); refresh(); }
  function close() { panel.hidden = true; button.setAttribute("aria-expanded", "false"); }
  button.addEventListener("click", () => (panel.hidden ? open() : close()));
  addEventListener("keydown", (e) => { if (e.key === "Escape" && !panel.hidden) { e.stopPropagation(); close(); } }, true);

  function render() {
    list.replaceChildren(...rows.map((r) => {
      const cb = h("input", { type: "checkbox", "aria-label": `Select ${r.name}`, onchange: (ev) => { if (ev.target.checked) sel.add(r.slot); else sel.delete(r.slot); render(); } });
      cb.checked = sel.has(r.slot);
      return h("li", { class: "dn-item lib-item" + (sel.has(r.slot) ? " on" : "") },
        h("label", { class: "dn-top" }, cb, h("span", { class: "lib-slot" }, String(r.slot)), h("b", {}, r.name),
          ...(r.tags || []).map((t) => h("span", { class: "dn-tag", "data-tag": t }, t)),
          r.favourite ? h("span", { class: "lib-mark", title: "a favourite" }, "★") : null,
          r.imported ? h("span", { class: "lib-mark", title: "imported" }, "↓") : null));
    }));
    if (!rows.length) list.append(h("li", { class: "dn-empty muted small" }, presets.ready ? "No presets yet." : "—"));
    all.checked = rows.length > 0 && sel.size === rows.length;
    const n = sel.size;
    fmtSeg.replaceChildren(...[["fizz", "FIZZ"], ["dnsnd", "Digitone"], ["dn2pst", "Digitone II"]].map(([k, label]) =>
      h("button", { class: fmt === k ? "on" : "", role: "radio", "aria-checked": String(fmt === k), title: k === "fizz" ? ".fizzsnd / .fizzlib" : `.${k}, experimental`,
        onclick: () => { fmt = k; review = null; render(); } }, label, k === "fizz" ? null : h("sup", { class: "dnx-exp" }, "exp"))));
    author.parentNode.hidden = fmt !== "fizz";
    const what = fmt === "fizz" ? (n === 1 ? ".fizzsnd" : ".fizzlib") : n === 1 ? `.${fmt}` : `.${fmt} files, a zip`;
    exp.textContent = busy ? "Exporting..." : n ? `Export ${n} preset${n === 1 ? "" : "s"} (${what})` : "Export";
    reviewSlot.replaceChildren(review || "");
    exp.disabled = busy || !n;
    line.textContent = status;
  }

  async function doExport() {
    const slots = rows.filter((r) => sel.has(r.slot));
    if (!slots.length || busy) return;
    busy = true; render();
    try {
      const sounds = [];
      for (const r of slots) {
        const f = await presets.dump(r.slot);
        if (!f) throw new Error(`FIZZ did not send MY PRESETS ${r.slot}`);
        const s = soundFromStoreFrame(f);
        s.favourite = !!r.favourite;
        s.source = { fizz: fizzVersion(), exported: new Date().toISOString(), from: `MY PRESETS ${r.slot}` };
        sounds.push(s);
      }
      if (fmt !== "fizz") {                   // (Digitone: the notes first, then Download)
        const result = digitoneExport(sounds, fmt);
        lastExport = { name: result.name, bytes: result.bytes, items: result.items, pending: true };
        review = digitoneReview(result, fmt, {
          onDownload: () => { save(result.name, result.bytes, result.type); lastExport.pending = false; review = null; render(); toast(`${result.name}: ${sounds.length} preset${sounds.length === 1 ? "" : "s"} (experimental).`, "ok"); },
          onCancel: () => { review = null; render(); } });
        return;
      }
      const one = sounds.length === 1;
      const name = one ? soundFileName(sounds[0].name) : libraryFileName();
      const bytes = one ? new TextEncoder().encode(soundToFizzsnd(sounds[0])) : makeFizzlib(sounds, { fizz: fizzVersion(), author: author.value.trim() || undefined });
      lastExport = { name, bytes };
      save(name, bytes, one ? "application/json" : "application/zip");
      toast(`${name}: ${sounds.length} preset${one ? "" : "s"}.`, "ok");
    } catch (e) { toast(e.message || String(e), "bad"); }
    finally { busy = false; render(); }
  }
  render();
  return { open, close, refresh, exportSelected: doExport, select: (slots) => { sel = new Set(slots); render(); },
    setFormat: (k) => { fmt = k; review = null; render(); }, download: () => { const b = reviewSlot.querySelector(".dnx-review .pill.primary"); if (b) b.click(); },
    get rows() { return rows; }, get lastExport() { return lastExport; } };
}
