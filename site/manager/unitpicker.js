// SPDX-License-Identifier: GPL-3.0-only
// The header's unit (the offline mode, 2026-10-29): the unit pill (THIS BROWSER orange, or the FM-1 green), the sound
// pill while the browser's FIZZ is the unit, the FM-1 beside it (a dashed circle when absent; found: a green ⇄ FM-1),
// and the unit picker (a popover from the pill; a bottom sheet on a phone): THIS BROWSER (its ••• a MEMORY drawer with
// RESET), the FM-1 (its port: USB or BLUETOOTH), CONNECT FM-1.
//   const u = mountUnit({ $, view, pick(unit), connect(), sound(), reset() }); u.render(); u.open(); u.close()
//   view() -> { mode: "fm1" | "browser", look, bver, fver, fsub, found, via: "usb" | "ble", sound: null | "off" | "look" | "on" | "mute",
//               mem: { lib: [used, all], prj: [used, all], all } | null }
import { esc } from "./ui.js";

const IC = {
  chev: '<svg class="ic chev" viewBox="0 0 24 24" aria-hidden="true"><path d="M6 9l6 6 6-6"/></svg>',
  spkOn: '<svg class="ic" viewBox="0 0 24 24" aria-hidden="true"><path d="M4 9.5v5h3.5L12 18.5v-13L7.5 9.5z" fill="currentColor" fill-opacity=".25"/><path d="M15.5 9a4 4 0 0 1 0 6M18.5 6a8.2 8.2 0 0 1 0 12"/></svg>',
  spkOff: '<svg class="ic" viewBox="0 0 24 24" aria-hidden="true"><path d="M4 9.5v5h3.5L12 18.5v-13L7.5 9.5z"/><path d="M16 9.5l5 5M21 9.5l-5 5"/></svg>',
  check: '<svg class="ic" viewBox="0 0 24 24" aria-hidden="true"><path d="M5 12.5l4.5 4.5L19 7.5"/></svg>',
  swap: '<svg class="ic" viewBox="0 0 24 24" aria-hidden="true"><path d="M4 8h14M14 4l4 4-4 4M20 16H6M10 12l-4 4 4 4"/></svg>',
  usb: '<svg class="ic" viewBox="0 0 24 24" aria-hidden="true"><path d="M12 3v15M12 3l-2.5 3h5zM12 14l-5-3V8.5M12 16.5l5-3V11"/><circle cx="12" cy="19.5" r="1.8" fill="currentColor"/><rect x="15.5" y="9" width="3" height="2.5" fill="currentColor"/><circle cx="7" cy="7.8" r="1.4" fill="currentColor"/></svg>',
  bt: '<svg class="ic" viewBox="0 0 24 24" aria-hidden="true"><path d="M7 7l10 10-5 5V2l5 5L7 17"/></svg>',
  plug: '<svg class="ic" viewBox="0 0 24 24" aria-hidden="true"><path d="M9 3v5M15 3v5M6.5 8h11v3.5a5.5 5.5 0 0 1-11 0zM12 17v4"/></svg>',
};
// the browser: a window with FIZZ's bubbles inside; the FM-1: its screen, four knobs, the keys
const G = {
  browser: '<svg class="g" viewBox="0 0 24 24" aria-hidden="true" fill="none" stroke="currentColor" stroke-width="2" stroke-linejoin="round"><rect x="2.5" y="4" width="19" height="16" rx="3"/><path d="M2.5 8.5h19"/><circle cx="9.5" cy="15" r="2.6" stroke-width="1.6"/><circle cx="14.6" cy="12.6" r="1.6" stroke-width="1.6"/><circle cx="15.2" cy="17" r="1.1" stroke-width="1.6"/></svg>',
  browserBig: '<svg class="big" viewBox="0 0 76 50" aria-hidden="true" fill="none" stroke="currentColor" stroke-width="2" stroke-linejoin="round" stroke-linecap="round"><rect x="2" y="2" width="72" height="46" rx="7"/><path d="M2 12h72"/><circle cx="9" cy="7" r="1.6" fill="currentColor"/><circle cx="15" cy="7" r="1.6" fill="currentColor"/><circle cx="21" cy="7" r="1.6" fill="currentColor"/><rect x="30" y="5" width="38" height="4" rx="2" stroke-width="1.4"/><circle cx="31" cy="32" r="8" stroke="var(--cyan)" stroke-width="2.4"/><circle cx="43" cy="23" r="5.5" stroke="var(--pink)" stroke-width="2.4"/><circle cx="45" cy="38" r="3.6" stroke="var(--yellow)" stroke-width="2.4"/><path d="M56 22h11M56 28h8M56 34h11M56 40h6" opacity=".7"/></svg>',
  fm1Big: '<svg class="big" viewBox="0 0 76 50" aria-hidden="true" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linejoin="round"><rect x="1.5" y="1.5" width="73" height="47" rx="4" stroke-width="2"/><rect x="7" y="6" width="17" height="13" rx="1.5"/><circle cx="13" cy="13.5" r="3" stroke-width="1.2"/><circle cx="18" cy="10.5" r="2" stroke-width="1.2"/><circle cx="34" cy="10" r="3.4"/><circle cx="44" cy="10" r="3.4"/><circle cx="54" cy="10" r="3.4"/><circle cx="64" cy="10" r="3.4"/><rect x="31" y="17" width="5" height="3" rx="1"/><rect x="41" y="17" width="5" height="3" rx="1"/><rect x="51" y="17" width="5" height="3" rx="1"/><rect x="61" y="17" width="5" height="3" rx="1"/>' +
    Array.from({ length: 8 }, (_, i) => `<rect x="${7 + i * 8.2}" y="25" width="6" height="5" rx="1"/>`).join("") +
    Array.from({ length: 16 }, (_, i) => `<rect x="${6 + i * 4.1}" y="35" width="3.2" height="9" rx=".8"/>`).join("") + "</svg>",
  fm1Small: '<svg class="g" viewBox="0 0 26 18" aria-hidden="true" fill="none" stroke="currentColor" stroke-width="1.5"><rect x="1" y="1" width="24" height="16" rx="2.5"/><rect x="3.5" y="3.5" width="6" height="5" rx="1"/><circle cx="13.5" cy="5.5" r="1.4"/><circle cx="17.5" cy="5.5" r="1.4"/><circle cx="21.5" cy="5.5" r="1.4"/><path d="M4 12.5h18" stroke-dasharray="1.6 1.4" stroke-width="3"/></svg>',
};
const NONE = "– – –";
const VIA = { usb: "USB", ble: "BLUETOOTH" };
const viaChip = (v) => `<span class="usb ${v === "ble" ? "ble" : ""}">${v === "ble" ? IC.bt : IC.usb}${VIA[v] || "USB"}</span>`;
const phone = () => window.matchMedia("(max-width: 900px)").matches;

export function mountUnit(ctx) {
  const { $ } = ctx;
  const U = { open: false, more: false };
  const pill = $("unit"), spk = $("spk"), fm1 = $("fm1"), box = $("upick"), pop = $("upick-pop");

  // the store's use as 16 LED cells: MY PRESETS used (orange), their room, PROJECTS used (cyan), their room
  function cells(m) {
    if (!m) return Array.from({ length: 16 }, () => "<i></i>").join("");
    const tot = (m.lib[1] + m.prj[1]) || 1, nl = Math.max(1, Math.min(15, Math.round(m.lib[1] / tot * 16)));   // (the library's cells, the projects' after)
    const used = (u, all, n) => (u > 0 ? Math.max(1, Math.round(u / (all || 1) * n)) : 0);   // (anything used lights a cell)
    const ul = used(m.lib[0], m.lib[1], nl), up = used(m.prj[0], m.prj[1], 16 - nl);
    return Array.from({ length: 16 }, (_, i) => `<i class="${i < nl ? (i < ul ? "p" : "") : i - nl < up ? "j" : ""}"></i>`).join("");
  }
  const put = (el, html) => { if (el.dataset.html !== html) { el.dataset.html = html; el.innerHTML = html; } };   // (a click's target kept)
  const pct = (m) => (m ? `${m.all > 0 ? Math.max(1, Math.round(m.all * 100)) : 0}%` : "–");

  function renderPop(v) {
    const br = v.mode === "browser", fmOn = v.mode === "fm1", fmFound = fmOn || v.found;
    const brRow = `<div class="urow tube ${br ? "on" : ""}" data-u="browser" role="button" tabindex="0" aria-pressed="${br}">${G.browserBig}
        <div class="nm"><b>THIS BROWSER</b><span class="hud">${esc(v.bver ? `FIZZ ${v.bver.replace(/-.*$/, "")}` : NONE)}</span></div>
        <button class="more ${U.more ? "tube on" : "ol"}" id="up-more" aria-expanded="${U.more}" aria-label="Memory" title="Memory">•••</button>
        <span class="radio ${br ? "y" : ""}">${br ? IC.check : ""}</span></div>`;
    const drawer = U.more ? `<div class="drawer tube" id="up-mem"><span class="lbl">MEMORY</span><span class="cells" role="img" aria-label="The store ${pct(v.mem)} used">${cells(v.mem)}</span>
        <span class="hud">${pct(v.mem)}</span><button class="ol reset" id="up-reset">RESET</button></div>` : "";
    const fmRow = fmFound
      ? `<div class="urow fm tube ${fmOn ? "on" : ""}" data-u="fm1" role="button" tabindex="0" aria-pressed="${fmOn}">${G.fm1Big}
          <div class="nm"><b>FM-1 ${viaChip(v.via)}</b><span class="hud">${esc(v.fver ? `FIZZ ${v.fver.replace(/-.*$/, "")}` : "FIZZ")}</span></div>
          <span class="radio ${fmOn ? "y" : ""}">${fmOn ? IC.check : ""}</span></div>`
      : `<div class="urow fm ants off" data-u="fm1" role="button" tabindex="0" aria-disabled="true">${G.fm1Big}
          <div class="nm"><b>FM-1 ${viaChip("usb")}</b><span class="hud">${NONE}</span></div><span class="radio"></span></div>`;
    const go = fmFound ? "" : `<button class="arc breathe connect" id="up-connect">${IC.plug}CONNECT FM-1</button>`;
    put(pop, `${phone() ? '<div class="grab" aria-hidden="true"></div>' : ""}<div class="uphead"><span class="lbl">UNIT</span><span class="hud">2</span></div>${brRow}${drawer}${fmRow}${go}`);
  }
  function place() {
    if (phone()) { pop.style.left = pop.style.right = pop.style.top = ""; pop.style.removeProperty("--nub"); return; }
    const r = pill.getBoundingClientRect(), W = Math.min(430, window.innerWidth - 32);
    const mid = r.left + r.width / 2, left = Math.max(16, Math.min(window.innerWidth - 16 - W, mid - W + 160));
    pop.style.left = `${left}px`; pop.style.top = `${r.bottom + 10}px`; pop.style.right = "auto";
    pop.style.setProperty("--nub", `${Math.max(20, Math.min(W - 36, mid - left - 8))}px`);
  }

  function render() {
    const v = ctx.view(), br = v.mode === "browser";
    // the unit pill
    pill.dataset.u = v.mode;
    pill.dataset.state = v.look ? "look" : "on";
    pill.dataset.found = br && v.found ? "1" : "";
    const sub = br ? (v.found ? "" : v.bver ? `FIZZ ${v.bver.replace(/-.*$/, "")}` : "") : v.fsub || "";
    put(pill, br
      ? `${G.browser}<span class="ul"><span class="this">THIS </span>BROWSER</span>${sub ? `<small>${esc(sub)}</small>` : ""}${IC.chev}`
      : `<span class="dot"></span><span class="ul">${v.look ? "FM-1…" : "FM-1"}</span>${sub ? `<small>${esc(sub)}</small>` : ""}${IC.chev}`);
    pill.setAttribute("aria-label", `Unit: ${br ? "this browser" : "the FM-1"}. Choose`);
    pill.setAttribute("aria-expanded", String(U.open));
    // the sound, while the browser's FIZZ is the unit
    spk.hidden = !br || !v.sound;
    if (br && v.sound) {
      spk.dataset.state = v.sound;
      put(spk, (v.sound === "off" || v.sound === "look" ? '<span class="ring"></span><span class="ring"></span>' : "") + (v.sound === "on" ? IC.spkOn : IC.spkOff));
      spk.setAttribute("aria-label", v.sound === "on" ? "Sound on: mute" : v.sound === "mute" ? "Muted: sound on" : "Start the sound");
      spk.title = spk.getAttribute("aria-label");
    }
    // the FM-1 beside it
    fm1.hidden = !br;
    fm1.dataset.state = v.found ? "found" : "absent";
    put(fm1, v.found
      ? `<span class="bubs" aria-hidden="true"><i></i><i></i><i></i></span>${IC.swap}<span>FM-1</span><small>${VIA[v.via] || "USB"}</small>`
      : G.fm1Small);
    fm1.setAttribute("aria-label", v.found ? `Use the FM-1 (FIZZ ${v.fver || ""})` : "FM-1: not connected");
    fm1.title = v.found ? "Use the FM-1" : "FM-1: connect";
    fit();
    if (U.open) { renderPop(v); place(); }
  }
  // a bar that would squeeze the tabs (a computer): the crumb goes, then the pill's version, the dashed FM-1 (⇄ FM-1's
  // USB), THIS, then the pill's word
  function fit() {
    const top = pill.closest(".top"), t = top && top.querySelector(".tabs"), steps = ["fit1", "fit2", "fit3", "fit4", "fit5"];
    if (!t) return;
    top.classList.remove(...steps);
    if (phone()) return;
    for (const c of steps) { if (t.scrollWidth <= t.clientWidth + 1) break; top.classList.add(c); }
  }

  function open() {
    if (U.open) return;
    U.open = true;
    box.hidden = false;
    box.classList.toggle("sheet", phone());
    render();
    const first = pop.querySelector(".urow");
    if (first) first.focus({ preventScroll: true });
  }
  function close(back = true) {
    if (!U.open) return;
    U.open = false; U.more = false;
    box.hidden = true;
    render();
    if (back) pill.focus({ preventScroll: true });
  }

  pill.addEventListener("click", () => (U.open ? close() : open()));
  fm1.addEventListener("click", () => { if (fm1.dataset.state === "found") ctx.pick("fm1"); else open(); });
  spk.addEventListener("click", () => ctx.sound());
  $("upick-scrim").addEventListener("click", () => close(false));
  pop.addEventListener("click", async (ev) => {
    const t = ev.target;
    if (t.closest("#up-more")) { ev.stopPropagation(); U.more = !U.more; render(); return; }
    if (t.closest("#up-reset")) { close(false); await ctx.reset(); return; }
    if (t.closest("#up-connect")) { close(false); await ctx.connect(); return; }
    const row = t.closest(".urow");
    if (!row || row.getAttribute("aria-disabled") === "true") { if (row) { close(false); ctx.connect(); } return; }
    close(false);
    ctx.pick(row.dataset.u);
  });
  pop.addEventListener("keydown", (ev) => {
    if ((ev.key === "Enter" || ev.key === " ") && ev.target.classList.contains("urow")) { ev.preventDefault(); ev.target.click(); }
  });
  document.addEventListener("keydown", (ev) => { if (ev.key === "Escape" && U.open) { ev.preventDefault(); close(); } });
  window.addEventListener("resize", () => { if (U.open) box.classList.toggle("sheet", phone()); render(); });
  return { render, open, close, isOpen: () => U.open };
}
