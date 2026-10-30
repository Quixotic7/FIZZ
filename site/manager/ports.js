// SPDX-License-Identifier: GPL-3.0-only
// LOG | PORTS (2026-10-29, the iOS app's PortsSheet): the header's LOG opens a panel with two views, the log's lines
// and PORTS: every MIDI input and output the browser sees (portscan.js), one row each: its name, in, out, USB or
// BLUETOOTH, asked or not, what answered (FIZZ and its version, REMOTEFM1, M-VAVE, UPDATE MODE, nothing); the counts
// above (IN, OUT, ASKED, FIZZ), when they were asked, LOOK AGAIN (all asked again), the copy button (the view as text).
// The picker's PORTS opens it on PORTS. Live: render() on every port change and after each scan.
//   const p = mountPorts({ $, report(), again(), shown(), text(report), log(), busy() }); p.open("log" | "ports"); p.render()
import { esc } from "./ui.js";

const IC = {
  again: '<svg viewBox="0 0 20 20" aria-hidden="true"><path d="M16 10a6 6 0 1 1-1.8-4.3"/><path d="M16.5 3.5v3.6h-3.6"/></svg>',
  copy: '<svg viewBox="0 0 20 20" aria-hidden="true"><rect x="6.5" y="6.5" width="10" height="10" rx="2"/><path d="M13.5 6.5V4.5a1.5 1.5 0 0 0-1.5-1.5H5A1.5 1.5 0 0 0 3.5 4.5V12A1.5 1.5 0 0 0 5 13.5h1.5"/></svg>',
  yes: '<svg viewBox="0 0 16 16" aria-hidden="true"><path d="M3.5 8.5l3 3 6-7"/></svg>',
  usb: '<svg class="ic" viewBox="0 0 24 24" aria-hidden="true"><path d="M12 3v15M12 3l-2.5 3h5zM12 14l-5-3V8.5M12 16.5l5-3V11"/><circle cx="12" cy="19.5" r="1.8" fill="currentColor"/><rect x="15.5" y="9" width="3" height="2.5" fill="currentColor"/><circle cx="7" cy="7.8" r="1.4" fill="currentColor"/></svg>',
  bt: '<svg class="ic" viewBox="0 0 24 24" aria-hidden="true"><path d="M7 7l10 10-5 5V2l5 5L7 17"/></svg>',
};
const two = (n) => String(n).padStart(2, "0");
const hms = (d, s = true) => (d ? `${two(d.getHours())}:${two(d.getMinutes())}${s ? `:${two(d.getSeconds())}` : ""}` : "");

export function mountPorts(ctx) {
  const { $ } = ctx;
  const box = $("logbox"), panel = $("lpanel"), pre = $("log"), view = $("lp-view");
  let tab = "log";
  const put = (el, html) => { if (el.dataset.html !== html) { el.dataset.html = html; el.innerHTML = html; } };

  function row(p) {
    const cls = p.fizz ? "fizz" : p.id ? "other" : "quiet";
    const chip = p.id || p.ble ? `<span class="usb ${p.ble ? "ble" : ""}">${p.ble ? IC.bt : IC.usb}${p.ble ? "BLUETOOTH" : "USB"}</span>` : "";
    const fw = p.fizz ? `FIZZ${p.version ? ` ${p.version}` : ""}` : p.fw;
    const ans = p.id ? `<span class="hud">${esc(p.id)}</span><span class="fwchip ${p.fizz ? "fizz" : "red"}">${esc(fw)}</span>` : '<span class="none">—</span>';
    return `<div class="prow ${cls}${p.current ? " cur" : ""}" data-port="${esc(p.name)}" title="${esc(p.name)}">
      <span class="pn"><span class="nm">${esc(p.name)}</span>${chip}</span>
      <span class="io ${p.inn ? "" : "no"}" role="img" aria-label="${p.inn ? "in" : "no in"}"></span><span class="io ${p.out ? "" : "no"}" role="img" aria-label="${p.out ? "out" : "no out"}"></span>
      <span class="ask ${p.asked ? "y" : ""}" role="img" aria-label="${p.asked ? "asked" : "not asked"}">${p.asked ? IC.yes : ""}</span>
      <span class="ans">${ans}</span></div>`;
  }
  function render() {
    if (!box.open) return;
    for (const b of panel.querySelectorAll(".lpseg button")) { const on = b.dataset.lp === tab; b.classList.toggle("on", on); b.setAttribute("aria-selected", String(on)); }
    const r = ctx.report(), ph = window.matchMedia("(max-width: 700px)").matches;
    pre.hidden = tab !== "log"; view.hidden = tab !== "ports";
    $("lp-again").hidden = tab !== "ports";
    $("lp-again").classList.toggle("spin", tab === "ports" && ctx.busy());
    $("lp-when").textContent = tab === "ports" ? hms(r.at, !ph) : "";
    if (tab !== "ports") return;
    const stat = (n, l, c, on = false) => `<div class="pstat tube ${on ? "on" : ""}" style="--c:var(${c})"><span class="num hud">${n}</span><span class="lbl">${l}</span></div>`;
    put(view, `<div class="pstats">${stat(r.n.in, "IN", "--cyan")}${stat(r.n.out, "OUT", "--cyan")}${stat(r.n.asked, "ASKED", "--yellow")}${stat(r.n.fizz, "FIZZ", "--green", r.n.fizz > 0)}</div>
      <div class="ptab" role="table" aria-label="Ports"><div class="prow hd"><span class="lbl">Port</span><span class="lbl">In</span><span class="lbl">Out</span><span class="lbl">Asked</span><span class="lbl">Answer</span></div>
      ${r.ports.map(row).join("")}</div>`);
  }
  // under the header wherever it ends (a phone upright: its two rows)
  function place() {
    const top = document.querySelector(".top");
    if (top) panel.style.top = `${Math.round(top.getBoundingClientRect().bottom) + 8}px`;
  }
  function open(t = tab) {
    tab = t;
    if (!box.open) box.open = true; else { place(); render(); }
    if (tab === "ports") ctx.shown();
  }
  box.addEventListener("toggle", () => { if (box.open) { place(); render(); if (tab === "ports") ctx.shown(); if (tab === "log") pre.scrollTop = pre.scrollHeight; } });
  panel.addEventListener("click", async (ev) => {
    const t = ev.target;
    const seg = t.closest(".lpseg button");
    if (seg) { open(seg.dataset.lp); return; }
    if (t.closest("#lp-again")) { ctx.again(); render(); return; }
    if (t.closest("#lp-copy")) {
      const text = tab === "ports" ? ctx.text(ctx.report()) : ctx.log();
      try { await navigator.clipboard.writeText(text); $("lp-copy").classList.add("done"); setTimeout(() => $("lp-copy").classList.remove("done"), 900); } catch (_) { /* (not allowed) */ }
    }
  });
  window.addEventListener("resize", () => { if (box.open) { place(); render(); } });
  return { open, render, tab: () => tab };
}
