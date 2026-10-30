// SPDX-License-Identifier: GPL-3.0-only
// PLAY (the parity build, 2026-10-29): a virtual FM-1 panel, the web twin of the iOS app's PLAY (ios/FizzKit/Sources/
// FizzUI/Play*.swift; its pure parts in playcore.js). Its own target, apart from the header's unit: THIS BROWSER
// (orange: the browser's FIZZ, browserunit.js, its screen and LEDs shown as they come; quiet until the first touch,
// then woken into the audio worklet) or the FM-1 (green, offered only while one is connected: its drawing on the
// screen, the buttons, knobs and MASTER through its remote on channel 16, the keys as notes on its selected track's
// channel; its own screen is not streamed). The input is the emulator's: panel.js's Controls hold every key and button
// by source and give the engine its panel (the browser), or a diff of it goes out as the remote (the FM-1);
// input.js's Keyboard plays it from the computer keys. The views: FULL, PANEL, KEYS, SCREEN (the chips, or a swipe
// sideways on the screen; a double click on it: SCREEN and back); six themes; a phone on its side shows the board
// under a strip (the Manager's bar gives way), upright it stacks. Everything held is let go (and the FM-1's CC 123)
// when PLAY is left, the target changes, or the page loses focus. The view, the target, the theme, the knob sets and
// MASTER are remembered in this browser.
//   mountPlay({ $, log, bu, buNow, fm1, fm1Track, octave, bver, back, blocked }) -> { show(), hide(), render(), state }
import { Controls, SRC, NKEY, labelToMatrix, roleToMatrix, ledIndexOf, ledAlpha } from "../js/panel.js";
import { Keyboard } from "../js/input.js";
import { THEMES, THEME_ORDER, SWATCH, BTN_LABEL, KNOB_LABEL, KNOB_INK, MASTER, VIEWS, swiped, keyLayout, velocity, Detents, masterFrom,
  RemoteOut, remoteNote, layoutOf, layoutName } from "./playcore.js";

const KEY = "fizz-play";
const load = (k, d) => { try { const v = localStorage.getItem(`${KEY}-${k}`); return v === null ? d : JSON.parse(v); } catch (_) { return d; } };
const keep = (k, v) => { try { localStorage.setItem(`${KEY}-${k}`, JSON.stringify(v)); } catch (_) { /* (no storage: forgotten) */ } };
const esc = (s) => String(s).replace(/[&<>"']/g, (c) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" }[c]));
const DEG = Math.PI / 180;
const LED = ledIndexOf(null);

// ---------------------------------------------------------------- the glyphs (unitpicker.js's, the mock-ups')
const G = {
  browser: '<svg class="g" viewBox="0 0 24 24" aria-hidden="true" fill="none" stroke="currentColor" stroke-width="2" stroke-linejoin="round"><rect x="2.5" y="4" width="19" height="16" rx="3"/><path d="M2.5 8.5h19"/><circle cx="9.5" cy="15" r="2.6" stroke-width="1.6"/><circle cx="14.6" cy="12.6" r="1.6" stroke-width="1.6"/><circle cx="15.2" cy="17" r="1.1" stroke-width="1.6"/></svg>',
  fm1: '<svg class="g" viewBox="0 0 26 18" aria-hidden="true" fill="none" stroke="currentColor" stroke-width="1.5"><rect x="1" y="1" width="24" height="16" rx="2.5"/><rect x="3.5" y="3.5" width="6" height="5" rx="1"/><circle cx="13.5" cy="5.5" r="1.4"/><circle cx="17.5" cy="5.5" r="1.4"/><circle cx="21.5" cy="5.5" r="1.4"/><path d="M4 12.5h18" stroke-dasharray="1.6 1.4" stroke-width="3"/></svg>',
  tick: '<span class="tick"><svg viewBox="0 0 24 24" aria-hidden="true"><path d="M5 12.5l4.5 4.5L19 7.5"/></svg></span>',
  chev: '<svg class="ic" viewBox="0 0 24 24" aria-hidden="true"><path d="M6 9l6 6 6-6"/></svg>',
  back: '<svg class="chv" viewBox="0 0 16 16" aria-hidden="true"><path d="M10 3L5 8l5 5"/></svg><svg class="bub" viewBox="0 0 32 32" aria-hidden="true"><circle cx="11" cy="20" r="7"/><circle cx="22" cy="11.5" r="5.5"/><circle cx="23.5" cy="25" r="3.5"/></svg>',
  rot: '<svg viewBox="0 0 24 24" aria-hidden="true"><rect x="7" y="2.5" width="10" height="17" rx="2.5"/><path d="M3 14a9 9 0 0 0 7.5 7.5M3 14l-1.2 2.6M3 14l2.6 1"/></svg>',
};
const VIEW_G = {
  full: '<svg viewBox="0 0 26 15" aria-hidden="true"><rect x="0" y="0" width="7" height="6" rx="1.2"/><rect x="9" y="0" width="8" height="6" rx="1.2" opacity=".55"/><rect x="19" y="0" width="7" height="6" rx="1.2"/><rect x="0" y="8.5" width="26" height="6.5" rx="1.5"/></svg>',
  panel: '<svg viewBox="0 0 26 15" aria-hidden="true"><rect x="0" y="0" width="13" height="15" rx="1.5" opacity=".55"/><rect x="15" y="0" width="11" height="6" rx="1.2"/><rect x="15" y="8" width="11" height="7" rx="1.2"/></svg>',
  keys: '<svg viewBox="0 0 26 15" aria-hidden="true"><rect x="0" y="0" width="9" height="7" rx="1.2" opacity=".55"/><rect x="11" y="0" width="15" height="7" rx="1.2"/><rect x="0" y="9" width="26" height="6" rx="1.5"/></svg>',
  screen: '<svg viewBox="0 0 26 15" aria-hidden="true"><rect x="6" y="0" width="14" height="15" rx="1.5" opacity=".55"/></svg>',
};
// the FM-1 on the screen (its own screen is not streamed): its drawing, the track the keys play, its channel
function fm1Screen(track, chan, via) {
  const rects = Array.from({ length: 8 }, (_, i) => `<rect x="${(7 + i * 8.2).toFixed(1)}" y="25" width="6" height="5" rx="1"/>`).join("")
    + Array.from({ length: 16 }, (_, i) => `<rect x="${(6 + i * 4.1).toFixed(1)}" y="35" width="3.2" height="9" rx=".8"/>`).join("");
  return `<svg class="lcd fm" viewBox="0 0 240 240" font-family="Silkscreen"><rect width="240" height="240" fill="#000"/><circle cx="120" cy="104" r="88" fill="#1FD47E" opacity=".06"/>`
    + `<svg x="50" y="58" width="140" height="92" viewBox="0 0 76 50" fill="none" stroke="#1FD47E" stroke-width="1.6" stroke-linejoin="round"><rect x="1.5" y="1.5" width="73" height="47" rx="4" stroke-width="2"/><rect x="7" y="6" width="17" height="13" rx="1.5"/><circle cx="13" cy="13.5" r="3" stroke-width="1.2"/><circle cx="18" cy="10.5" r="2" stroke-width="1.2"/><circle cx="34" cy="10" r="3.4"/><circle cx="44" cy="10" r="3.4"/><circle cx="54" cy="10" r="3.4"/><circle cx="64" cy="10" r="3.4"/><rect x="31" y="17" width="5" height="3" rx="1"/><rect x="41" y="17" width="5" height="3" rx="1"/><rect x="51" y="17" width="5" height="3" rx="1"/><rect x="61" y="17" width="5" height="3" rx="1"/>${rects}</svg>`
    + `<text x="120" y="186" font-size="18" fill="#ECECEF" text-anchor="middle">TRACK ${track >= 0 ? track + 1 : "–"}</text>`
    + `<text x="120" y="208" font-size="11" fill="#1FD47E" text-anchor="middle">CH ${chan + 1} · ${via}</text></svg>`;
}

export function mountPlay(ctx) {
  const { $ } = ctx;
  const sec = $("tab-play");
  const P = {
    shown: false, want: load("target", "browser"), cur: "browser", view: VIEWS.includes(load("view", "")) ? load("view", "") : "full",
    theme: THEMES[load("theme", "")] ? load("theme", "") : "fizz", knobs: { panel: "main", keys: "kn", ...load("knobs", {}) },
    offset: 3, lay: "", L: null, scale: 1, u: null, px: null, levels: null, green: 0, track: { t: -1, chan: 14 }, prevView: "full",
    fingers: new Map(), raf: 0, poll: 0, tick: 0, fm1Seen: false, pop: false,
  };
  const R = new RemoteOut((b) => { const f = ctx.fm1(); if (f && f.link) try { f.link.send(b); } catch (_) { /* (gone: the target goes back) */ } });
  const C = new Controls((snap) => sink(snap));
  // the knobs' marks as the iOS app starts them; MASTER as kept here
  [[0, -20], [1, 30], [2, -120], [3, -60], [4, 135], [5, 0], [6, 40]].forEach(([r, a]) => { C.angle[r] = a * DEG; });
  { const m = +load("master", NaN); if (m >= 0 && m <= 1023) C.setMaster(m); }
  C.onDraw = () => draw();

  // ---------------------------------------------------------------- the target
  const offered = () => !!ctx.fm1();
  const eff = () => (P.want === "fm1" && offered() ? "fm1" : "browser");
  function sink(snap) {
    if (P.cur === "browser") {
      const u = P.u;
      if (u && u.engine && u.panel) u.panel(snap);
      return;
    }
    let mask = 0;
    for (let b = 0; b < 14; b++) if (C.btnHeld(b)) mask |= 1 << b;
    R.buttons(mask);
    for (let r = 0; r < 7; r++) { const n = snap.enc[roleToMatrix(r)]; if (n) R.turn(r, n); }
    R.master(snap.master);
    const oct = ctx.octave ? ctx.octave() : 0;
    for (let k = 0; k < NKEY; k++) {
      const h = C.keyHeld(k);
      if (h && !R.held(k)) R.keyDown(k, remoteNote(k, oct), P.track.chan, C.keyVel[k] || 100);
      else if (!h && R.held(k)) R.keyUp(k);
    }
  }
  // everything held let go on the target driven now (the FM-1: each note off, each button up, CC 123)
  function letGo() {
    P.fingers.clear();
    C.releaseSrc(0xff);
    if (P.cur === "fm1") R.letGo();
    if (C.encPending) C.encPending.fill(0);
  }
  // the target driven: changed when picked, or when the FM-1 comes or goes
  function drive() {
    const now = eff();
    if (now === P.cur) return false;
    letGo();
    P.cur = now;
    if (now === "fm1") { R.sync(C.master); readTrack(); }
    if (P.shown) { if (now === "browser") follow(); else unfollow(); }
    return true;
  }
  function pick(t) {
    if (t === "fm1" && !offered()) return;
    P.want = t;
    keep("target", t);
    drive();
    render();
  }

  // ---------------------------------------------------------------- the browser's FIZZ: its screen and LEDs
  const view = {
    frame(px) { P.px = px; draw(); },
    leds(levels, green) { P.levels = levels; P.green = green; draw(); },
  };
  function attach(u) {
    if (P.u && P.u !== u) P.u.view = null;
    P.u = u;
    if (!u) return;
    u.view = view;
    const s = u.screenNow ? u.screenNow() : null;
    if (s) { if (s.pixels) P.px = s.pixels; if (s.leds) { P.levels = s.leds.levels; P.green = s.leds.playGreen ?? s.leds.green ?? 0; } }
    draw();
  }
  async function follow() {
    const u = await ctx.bu();
    if (!P.shown || P.cur !== "browser") return;
    attach(u);
    const c = u && u.player && u.player.ctx;
    if (c && c.state === "suspended" && !u.quiet) c.resume().then(() => C.emit()).catch(() => {});
  }
  function unfollow(suspend = true) {
    if (P.u) P.u.view = null;
    if (suspend) ctx.idle && ctx.idle(P.u);
  }
  // a touch (or a key) on the board: the browser's FIZZ woken (its sound on; what is held then sent again)
  let waking = null;
  function wake() {
    if (P.cur !== "browser" || waking) return;
    waking = (async () => {
      const u = await ctx.bu();
      if (!u) return;
      attach(u);
      const c = u.player && u.player.ctx;
      if (c && c.state === "suspended" && !u.quiet) await c.resume().catch(() => {});
      if (u.quiet) await u.wake().catch(() => false);
      if (P.cur === "browser") C.emit();
    })().finally(() => { waking = null; });
  }

  // ---------------------------------------------------------------- the FM-1's selected track (its channel)
  async function readTrack() {
    if (P.cur !== "fm1" || !ctx.fm1Track) return;
    const t = await ctx.fm1Track().catch(() => null);
    if (!t || P.cur !== "fm1") return;
    if (t.t !== P.track.t || t.chan !== P.track.chan) { P.track = t; render(); }
  }

  // ---------------------------------------------------------------- the strip, the toolbar
  sec.innerHTML = `<div class="pstrip"><button class="back" id="pl-back" aria-label="Manager">${G.back}</button><button class="tgpill" id="pl-tgpill"></button>`
    + `<span class="sp"></span><div class="views ico" id="pl-views-s" role="group" aria-label="View"></div><button class="ol more" id="pl-more" aria-expanded="false">•••</button>`
    + `<div class="thpop" id="pl-thpop" hidden><div class="themes" id="pl-themes-s" role="group" aria-label="Theme"></div></div></div>`
    + `<div class="ptool"><span class="lbl plays">Plays</span><div class="tgt" id="pl-tgt" role="group" aria-label="Plays"></div><span class="sp"></span>`
    + `<div class="views" id="pl-views" role="group" aria-label="View"></div><span class="sp sp2"></span><div class="themes" id="pl-themes" role="group" aria-label="Theme"></div>`
    + `<button class="ol rot" id="pl-rot" aria-label="On its side">${G.rot}</button></div>`
    + `<div class="pstage" id="pl-stage"><div class="bwrap" id="pl-wrap"><div class="board" id="pl-board"></div></div></div>`;
  const board = $("pl-board"), wrap = $("pl-wrap"), stage = $("pl-stage");

  function tiles() {
    const f = ctx.fm1(), br = P.cur === "browser";
    const ver = ctx.bver ? ctx.bver() : "";
    const via = f && f.viaBle ? "BLUETOOTH" : "USB", tn = P.track.t >= 0 ? P.track.t + 1 : "–";
    $("pl-tgt").innerHTML = `<button class="tg tube${br ? " on" : ""}" data-tg="browser" aria-pressed="${br}">${G.browser}<span class="t"><b>THIS BROWSER</b><span class="hud">${esc(ver ? `FIZZ ${ver}` : "FIZZ")}</span></span>${br ? G.tick : ""}</button>`
      + (f ? `<button class="tg fm tube${br ? "" : " on"}" data-tg="fm1" aria-pressed="${!br}">${G.fm1}<span class="t"><b>FM-1</b><span class="hud">${via} · TRACK ${tn}</span></span>${br ? "" : G.tick}</button>` : "");
    const tp = $("pl-tgpill");
    tp.className = `tgpill${br ? " br" : ""}`;
    tp.disabled = !f;
    tp.innerHTML = br ? `${G.browser}THIS BROWSER${f ? G.chev : ""}` : `<span class="dot"></span>FM-1<span class="hud">TRACK ${tn}</span>${G.chev}`;
  }
  function chips() {
    const v = (ico) => VIEWS.map((k) => `<button class="vw${k === P.view ? " on" : ""}" data-v="${k}" aria-pressed="${k === P.view}" aria-label="${k.toUpperCase()}">${VIEW_G[k]}${ico ? "" : `<span>${k.toUpperCase()}</span>`}</button>`).join("");
    $("pl-views").innerHTML = v(false);
    $("pl-views-s").innerHTML = v(true);
    const th = THEME_ORDER.map((k) => `<button class="th${k === P.theme ? " on" : ""}" data-th="${k}" aria-pressed="${k === P.theme}" aria-label="${k.toUpperCase()}" style="background:${SWATCH[k][0]}"><i style="background:${SWATCH[k][1]}"></i></button>`).join("");
    $("pl-themes").innerHTML = th;
    $("pl-themes-s").innerHTML = th;
  }
  sec.addEventListener("click", (ev) => {
    const t = ev.target.closest("button");
    if (!t || !sec.contains(t)) return;
    if (t.dataset.tg) pick(t.dataset.tg);
    else if (t.dataset.v) setView(t.dataset.v);
    else if (t.dataset.th) { P.theme = t.dataset.th; keep("theme", P.theme); chips(); paint(); }
    else if (t.id === "pl-tgpill") pick(P.cur === "fm1" ? "browser" : "fm1");
    else if (t.id === "pl-back") { if (ctx.back) ctx.back(); }
    else if (t.id === "pl-more") { P.pop = !P.pop; $("pl-thpop").hidden = !P.pop; t.setAttribute("aria-expanded", String(P.pop)); }
    else if (t.id === "pl-rot") turnSideways();
    else if (t.dataset.k) { P.knobs[kset()] = t.dataset.k; keep("knobs", P.knobs); build(); }
    else if (t.dataset.deck) setView(t.dataset.deck);
  });
  // the phone turned on its side (fullscreen, then the orientation locked where the browser lets a page)
  async function turnSideways() {
    try { if (!document.fullscreenElement && document.documentElement.requestFullscreen) await document.documentElement.requestFullscreen(); } catch (_) { /* (not allowed) */ }
    try { if (screen.orientation && screen.orientation.lock) await screen.orientation.lock("landscape"); } catch (_) { /* (not here) */ }
  }
  function setView(v) {
    if (!VIEWS.includes(v) || v === P.view) return;
    if (v === "screen") P.prevView = P.view;
    P.view = v;
    keep("view", v);
    chips();
    build();
  }
  const kset = () => (P.view === "panel" ? "panel" : "keys");

  // ---------------------------------------------------------------- the board
  const phone = () => window.matchMedia("(max-width: 900px)").matches;
  const landscape = () => window.innerWidth > window.innerHeight;
  function paint() {
    const t = THEMES[P.theme];
    for (const [k, v] of Object.entries(t)) board.style.setProperty(`--${k}`, v);
    board.dataset.theme = P.theme;
  }
  function knobHtml(p) {
    const ink = KNOB_INK[p.role] || "var(--mark)", lab = KNOB_LABEL[p.role];
    const lp = p.lab === "left" ? `left:${p.cx - p.r - 14}px;top:${p.cy - 9}px;transform:translateX(-100%)` : `left:${p.cx}px;top:${p.cy - p.r - 26}px`;
    return `<div class="klbl petch" style="${lp}">${lab}</div><div class="knob" data-p="knob" data-role="${p.role}" role="slider" aria-label="${lab}" style="left:${p.cx}px;top:${p.cy}px;width:${2 * p.r}px;height:${2 * p.r}px;--m:${ink}"><i class="kmark"></i></div>`;
  }
  function btnHtml(p) {
    const l = BTN_LABEL[p.b];
    const inner = p.b === 10 ? `<span class="tri"></span>${l}` : p.b === 11 ? `<span class="rec"></span>${l}` : l.replace(" ", "<br>");
    return `<div class="pb petch" data-p="btn" data-b="${p.b}" role="button" aria-label="${l}" style="left:${p.x.toFixed(1)}px;top:${p.y.toFixed(1)}px;width:${p.w.toFixed(1)}px;height:${p.h.toFixed(1)}px">${inner}</div>`;
  }
  function segHtml(p) {
    const pos = p.right !== undefined ? `right:${p.right}px` : `left:${p.x}px`;
    if (p.kind === "knobs") {
      const k = P.knobs[kset()];
      return `<div class="bseg petch" style="${pos};top:${p.y}px"><button data-k="main" class="${k === "main" ? "on" : ""}">MAIN</button><button data-k="kn" class="${k === "kn" ? "on" : ""}">KN 1–4</button></div>`;
    }
    return `<div class="bseg petch" style="${pos};top:${p.y}px"><button data-deck="panel" class="${P.view === "panel" ? "on" : ""}">BUTTONS</button><button data-deck="keys" class="${P.view === "keys" ? "on" : ""}">KEYS</button></div>`;
  }
  function pillHtml(p) {
    const f = ctx.fm1();
    const inner = P.cur === "fm1" ? `<span class="dot"></span>FM-1<span class="hud">TRACK ${P.track.t >= 0 ? P.track.t + 1 : "–"} · ${f && f.viaBle ? "BLUETOOTH" : "USB"}</span>` : `${G.browser}THIS BROWSER`;
    return `<div class="bpill" style="left:${p.x}px;top:${p.y}px">${inner}</div>`;
  }
  function screenHtml() {
    if (P.cur === "fm1") { const f = ctx.fm1(); return fm1Screen(P.track.t, P.track.chan, f && f.viaBle ? "BLUETOOTH" : "USB"); }
    return `<canvas class="lcd" width="240" height="240"></canvas>`;
  }
  let bed = null, ovw = null, lcdCtx = null, img = null;
  function build() {
    const lay = layoutName(P.view, { phone: phone(), landscape: landscape() });
    const L = layoutOf(lay, { knobs: P.knobs[kset()], offset: P.offset });
    P.lay = lay; P.L = L;
    bed = null; ovw = null;
    const html = [];
    for (const p of L.parts) {
      switch (p.t) {
        case "pill": html.push(pillHtml(p)); break;
        case "knob": html.push(knobHtml(p)); break;
        case "well": html.push(`<div class="well" style="left:${p.x}px;top:${p.y}px;width:${p.w}px;height:${p.h}px"></div>`); break;
        case "btn": html.push(btnHtml(p)); break;
        case "led": html.push(`<div class="led0" style="left:${p.x}px;top:${p.y}px"></div>`); break;
        case "seg": html.push(segHtml(p)); break;
        case "bezel": {
          const pad = Math.max(8, p.s * 0.05);
          html.push(`<div class="bezel" data-p="screen" style="left:${p.x}px;top:${p.y}px;width:${p.s}px;height:${p.s}px;--pad:${pad}px">${screenHtml()}</div>`);
          break;
        }
        case "bed": {
          bed = { p, L: keyLayout({ w: p.w - 2 * p.pad, h: p.h - 2 * p.pad, visible: p.visible, offset: P.offset, gap: p.gap }) };
          const keys = bed.L.frames().map((f) => `<div class="k${f.black ? " blk" : ""}" data-key="${f.key}"></div>`).join("");
          html.push(`<div class="bed" data-p="bed" style="left:${p.x}px;top:${p.y}px;width:${p.w}px;height:${p.h}px;--pad:${p.pad}px"><div class="kstage">${keys}</div></div>`);
          break;
        }
        case "ovw":
          ovw = { p };
          html.push(`<div class="ovw" data-p="strip" style="left:${p.x}px;top:${p.y}px;width:${p.w}px;height:${p.h}px">${Array.from({ length: 16 }, (_, i) => `<i data-i="${i}"></i>`).join("")}<span class="win"></span></div>`);
          break;
      }
    }
    board.className = `board${P.cur === "fm1" ? " fm" : ""} l-${lay.replace("tall-", "tall t-")}`;
    board.style.width = `${L.W}px`;
    board.style.height = `${L.H}px`;
    board.innerHTML = html.join("");
    paint();
    const cv = board.querySelector("canvas.lcd");
    lcdCtx = cv ? cv.getContext("2d") : null;
    img = lcdCtx ? lcdCtx.createImageData(240, 240) : null;
    if (lcdCtx) { lcdCtx.fillStyle = "#000"; lcdCtx.fillRect(0, 0, 240, 240); }
    if (bed) bed.el = board.querySelector(".bed .kstage");
    placeKeys();
    fit();
    P.lastPx = null;
    draw();
  }
  function placeKeys() {
    if (!bed) return;
    const p = bed.p;
    bed.L = keyLayout({ w: p.w - 2 * p.pad, h: p.h - 2 * p.pad, visible: p.visible, offset: P.offset, gap: p.gap });
    const fr = new Map(bed.L.frames().map((f) => [f.key, f]));
    for (const el of bed.el.children) {
      const f = fr.get(+el.dataset.key);
      el.style.cssText = `left:${f.x.toFixed(1)}px;top:${f.y}px;width:${f.w.toFixed(1)}px;height:${f.h}px`;
    }
    if (ovw) {
      const win = board.querySelector(".ovw .win"), w = ovw.p.w;
      win.style.left = `${(w * bed.L.offset / 16).toFixed(1)}px`;
      win.style.width = `${(w * bed.L.visible / 16).toFixed(1)}px`;
    }
  }
  function fit() {
    if (!P.L || !P.shown) return;
    const r = stage.getBoundingClientRect();
    if (!r.width) return;
    const up = phone() && !landscape();
    const m = up ? 0 : 12;
    let s = up ? r.width / P.L.W : Math.min((r.width - 2 * m) / P.L.W, (r.height - 2 * m) / P.L.H);
    s = Math.max(0.2, Math.min(up ? 1.25 : 2, s));
    P.scale = s;
    wrap.style.width = `${Math.floor(P.L.W * s)}px`;
    wrap.style.height = `${Math.floor(P.L.H * s)}px`;
    board.style.transform = `scale(${s})`;
  }
  // the state drawn (once a frame at most): the knobs' marks, what is held, the LEDs, the screen
  function draw() {
    if (P.raf || !P.shown) return;
    P.raf = requestAnimationFrame(() => { P.raf = 0; paintState(); });
  }
  function paintState() {
    const lv = P.cur === "browser" ? P.levels : null;
    for (const el of board.querySelectorAll(".knob")) {
      const r = +el.dataset.role, a = r === MASTER ? (C.master / 1023 - 0.5) * 270 : C.angle[r] / DEG;
      el.firstChild.style.transform = `rotate(${a.toFixed(1)}deg)`;
      el.classList.toggle("hot", [...P.fingers.values()].some((f) => f.kind === "knob" && f.role === r));
    }
    for (const el of board.querySelectorAll(".pb")) {
      const b = +el.dataset.b, a = lv ? ledAlpha(lv[LED[labelToMatrix(b)]] || 0) : 0;
      el.classList.toggle("down", C.btnHeld(b));
      el.classList.toggle("lit", a > 0.3 && b !== 10);
      el.classList.toggle("on", b === 10 && lv ? P.green > 12 : false);
    }
    if (bed) {
      for (const el of bed.el.children) {
        const k = +el.dataset.key, held = C.keyHeld(k), a = lv ? ledAlpha(lv[LED[14 + k]] || 0) : 0;
        el.classList.toggle("held", held);
        el.classList.toggle("lit", a > 0.04 && a <= 0.75);
        el.classList.toggle("hot", a > 0.75);
        el.style.setProperty("--a", a.toFixed(2));
      }
      if (ovw) {
        const off = bed.L.offset, vis = bed.L.visible;
        board.querySelectorAll(".ovw i").forEach((el, i) => {
          const k = [0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26][i], a = lv ? ledAlpha(lv[LED[14 + k]] || 0) : 0;
          el.classList.toggle("in", i >= Math.floor(off) && i < off + vis);
          el.classList.toggle("t", a > 0.3 || C.keyHeld(k));
        });
      }
    }
    if (lcdCtx && P.px && P.px !== P.lastPx) {
      const d = img.data, px = P.px;
      for (let i = 0, o = 0; i < 57600; i++, o += 4) {
        const v = px[i], r = (v >> 11) & 31, g = (v >> 5) & 63, b = v & 31;
        d[o] = (r << 3) | (r >> 2); d[o + 1] = (g << 2) | (g >> 4); d[o + 2] = (b << 3) | (b >> 2); d[o + 3] = 255;
      }
      lcdCtx.putImageData(img, 0, 0);
      P.lastPx = px;
    }
  }

  // ---------------------------------------------------------------- touch (every finger its own; held until it lifts)
  const at = (e) => { const r = board.getBoundingClientRect(); return { x: (e.clientX - r.left) / P.scale, y: (e.clientY - r.top) / P.scale }; };
  function keyUnder(e) {
    if (!bed) return null;
    const q = at(e);
    return bed.L.keyAt(q.x - bed.p.x - bed.p.pad, q.y - bed.p.y - bed.p.pad);
  }
  const stillHeld = (kind, idx, but) => [...P.fingers.entries()].some(([id, f]) => id !== but && f.kind === kind && f.idx === idx);
  board.addEventListener("pointerdown", (e) => {
    const el = e.target.closest("[data-p]");
    if (!el || e.button > 0) return;
    if (P.cur === "browser") wake();
    const kind = el.dataset.p;
    if (kind === "screen") { P.fingers.set(e.pointerId, { kind, x0: e.clientX, y0: e.clientY }); return; }
    e.preventDefault();
    try { el.setPointerCapture(e.pointerId); } catch (_) { /* (a synthetic event) */ }
    if (kind === "knob") {
      const role = +el.dataset.role;
      P.fingers.set(e.pointerId, { kind, role, x0: e.clientX, y0: e.clientY, lx: e.clientX, ly: e.clientY, det: new Detents(), m0: C.master });
      draw();
    } else if (kind === "btn") {
      const b = +el.dataset.b;
      P.fingers.set(e.pointerId, { kind, idx: b });
      C.holdBtn(b, SRC.POINTER, true);
    } else if (kind === "bed") {
      const h = keyUnder(e);
      if (!h) return;
      P.fingers.set(e.pointerId, { kind: "key", idx: h.key, depth: h.depth });
      C.holdKey(h.key, SRC.POINTER, true, velocity(h.depth));
    } else if (kind === "strip") {
      const q = at(e), w = ovw.p.w, x = (q.x - ovw.p.x) / w * 16, vis = bed.L.visible, off = bed.L.offset;
      const start = x >= off && x <= off + vis ? off : x - vis / 2;   // (a touch outside the window: centred there first)
      P.fingers.set(e.pointerId, { kind, x0: e.clientX, off0: start });
      scrollKeys(start);
    }
  });
  board.addEventListener("pointermove", (e) => {
    const f = P.fingers.get(e.pointerId);
    if (!f) return;
    if (f.kind === "knob") {
      if (f.role === MASTER) { const v = masterFrom(f.m0, e.clientX - f.x0, e.clientY - f.y0); if (v !== C.master) { C.setMaster(v); keep("master", C.master); } }
      else { const n = f.det.move(e.clientX - f.lx, e.clientY - f.ly); if (n) C.turn(f.role, n); }
      f.lx = e.clientX; f.ly = e.clientY;
    } else if (f.kind === "key") {
      const h = keyUnder(e);   // (a finger sliding onto another key plays it)
      if (!h || h.key === f.idx) return;
      const old = f.idx;
      f.idx = h.key;
      if (!stillHeld("key", old, e.pointerId)) C.holdKey(old, SRC.POINTER, false);
      C.holdKey(h.key, SRC.POINTER, true, velocity(f.depth));
    } else if (f.kind === "strip") {
      scrollKeys(f.off0 + (e.clientX - f.x0) / P.scale / ovw.p.w * 16);
    }
  });
  const end = (e) => {
    const f = P.fingers.get(e.pointerId);
    if (!f) return;
    P.fingers.delete(e.pointerId);
    if (f.kind === "btn" && !stillHeld("btn", f.idx)) C.holdBtn(f.idx, SRC.POINTER, false);
    else if (f.kind === "key" && !stillHeld("key", f.idx)) C.holdKey(f.idx, SRC.POINTER, false);
    else if (f.kind === "knob") draw();
    else if (f.kind === "screen" && e.type === "pointerup") {
      const dx = e.clientX - f.x0, dy = e.clientY - f.y0;   // (a swipe sideways: the next view)
      if (Math.abs(dx) > 50 && Math.abs(dx) > Math.abs(dy)) setView(swiped(P.view, dx < 0 ? 1 : -1));
    }
    if (f.kind === "btn" && f.idx === 1 && P.cur === "fm1") setTimeout(readTrack, 300);   // (TRACKS let go: maybe another track)
  };
  board.addEventListener("pointerup", end);
  board.addEventListener("pointercancel", end);
  board.addEventListener("contextmenu", (e) => e.preventDefault());
  board.addEventListener("dblclick", (e) => { if (e.target.closest("[data-p='screen']")) setView(P.view === "screen" ? (P.prevView || "full") : "screen"); });
  let wheelAcc = 0;
  board.addEventListener("wheel", (e) => {
    const el = e.target.closest("[data-p='knob']");
    if (!el) return;
    e.preventDefault();
    const px = e.deltaMode === 1 ? e.deltaY * 16 : e.deltaMode === 2 ? e.deltaY * 400 : e.deltaY;
    wheelAcc += -px / 40;
    const s = Math.trunc(wheelAcc);
    if (s) { wheelAcc -= s; C.turn(+el.dataset.role, s); if (+el.dataset.role === MASTER) keep("master", C.master); }
  }, { passive: false });
  function scrollKeys(off) {
    if (!bed) return;
    P.offset = Math.max(0, Math.min(16 - bed.L.visible, off));
    placeKeys();
    draw();
  }

  // the computer keys (the emulator's map, input.js) while PLAY shows and nothing else has them
  const free = (e) => P.shown && !(ctx.blocked && ctx.blocked(e));
  new Keyboard({ controls: C, enabled: (e) => { if (free(e) && P.cur === "browser") wake(); return free(e); }, onLcdView: () => setView(P.view === "screen" ? (P.prevView || "full") : "screen") });
  // the page loses focus or is hidden: everything let go
  addEventListener("blur", () => { if (P.shown) letGo(); });
  document.addEventListener("visibilitychange", () => { if (document.hidden && P.shown) letGo(); });
  addEventListener("resize", () => {
    if (!P.shown) return;
    if (layoutName(P.view, { phone: phone(), landscape: landscape() }) !== P.lay) build(); else fit();
  });
  if (typeof ResizeObserver !== "undefined") new ResizeObserver(() => fit()).observe(stage);

  function render() {
    if (!P.shown) return;
    drive();
    tiles();
    chips();
    build();
  }
  return {
    state: P, controls: C, remote: R,
    show() {
      if (P.shown) return;
      P.shown = true;
      document.documentElement.classList.add("play-on");
      const tb = document.getElementById("t-play");   // (a phone's scrolling row of tabs: PLAY in view)
      if (tb && tb.scrollIntoView && phone()) tb.scrollIntoView({ block: "nearest", inline: "nearest" });
      P.cur = eff();
      if (P.cur === "fm1") { R.sync(C.master); readTrack(); } else follow();
      render();
      // (the FM-1 coming or going, the browser's FIZZ replaced, the FM-1's selected track)
      clearInterval(P.poll);
      P.poll = setInterval(() => {
        if (!P.shown) return;
        const seen = offered();
        if (seen !== P.fm1Seen || drive()) { P.fm1Seen = seen; tiles(); build(); }
        if (P.cur === "browser") { const u = ctx.buNow && ctx.buNow(); if (u !== P.u && u && u.engine) attach(u); }
        if (P.cur === "fm1" && ++P.tick % 3 === 0) readTrack();
      }, 600);
      P.fm1Seen = offered();
    },
    hide() {
      if (!P.shown) return;
      letGo();
      P.shown = false;
      document.documentElement.classList.remove("play-on");
      clearInterval(P.poll);
      unfollow();
      P.pop = false;
      $("pl-thpop").hidden = true;
    },
    render,
    letGo,
  };
}
