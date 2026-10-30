// SPDX-License-Identifier: GPL-3.0-only
// The manager's EDIT tab: the sound editor (docs/MANAGER-P2-PLAN.md §1): every one of FIZZ's values on one panel, a
// synth's front panel (sound.js's table, the core's names and texts). Drag a value (SHIFT: fine; the two-decimal values
// 0.01 a pixel), the wheel or the arrow keys over it (the two-decimal values 0.01 a tick, quick ticks speed up to 1.00,
// SHIFT 1.00); a double click its default; a long list (a destination, a source) a picker. The keybed under it plays
// the sound (the FM-1's 27 keys from F, the computer's A..' row, Z / X the octave).
// Design 12 (the owner, 2026-10-30): the panel is one grid, 20 columns x 15 rows, every value a cell of it (a tile: its
// name over its value, a bar in its colour), the drawings a block of cells drawn in their own pixels; the sections
// (OPERATORS, OP ENV, FILTER, AMP, DRIVE · CRUSH, PLAY, LFO, MOD) a frame drawn on the grid. Clean linework: no glows,
// small handles (a ring with a larger hit area), ALGO's number centred. The operators' row HARM DTUN FDBK MIX (DRIVE
// on DRIVE · CRUSH, LEVEL on AMP); PHASE RESET and the KEY / VEL values with their operators; the filter's TYPE one
// 8-way choice (its stored values); SHAPE dimmed "—" where the type has none. One panel from about 1400 x 860 with the
// keys shown, 1280 x 760 with them hidden; smaller, a page a section under tabs (each its block on the same kind of
// grid, bigger cells; a phone four across).
// Design 13 (drag to modulate, modroute.js): an LFO's plug or a MOD source dragged onto a value routes it there (what
// it reaches lit in its colour); a route a dot on its value, a click on the dot its card (DEPTH, ×); pages: the sources
// in a strip under the tabs, a tap arms one, a tap on a tab, a tap on a lit value.
//
// Where the sound plays: the target (setTarget). By default the browser's own FIZZ (browserfizz.js: the same core in
// this page, the sound loaded onto its track 1). The live editor (P2-B, manager.js) gives a target that plays it on
// a track of the FM-1. A target: { name, ready(), start(), audition(sound), param(id, v), note(n, vel), locked()? }:
// locked (an FM-1 track shown with TRACK off) the values are not changed: ctx.onLocked() (the switch nudged).
// FIZZ MIDI (e): a MIDI track's sound (sound.js midi) shows its 28 slots instead (the device's MIDI editor,
// mockups/fizz-midi-track.html screens 4..6): seven pages of four bars in the knobs' colours, each its CC number and
// value; an untouched slot a dashed empty bar ("—"). A drag or the wheel sets a value (the slot touched), a click on
// the CC number types it, a double click (or DELETE, or ×) makes the slot untouched again. FIZZ's PARAM takes only a
// slot's value (not its CC number or touched mark): each change goes to the target as the whole sound, target.sound(s)
// (the FM-1's track: an AUDITION; the browser's FIZZ: its STORE6 with LOAD).
// The mix (2026-10-29): PAN, CHO, DEL, REV on AMP, the sound's pan and sends under PARAM's ids 115..118 (sound.js
// MIX_IDS). A MIDI sound shows none: a MIDI track takes no voice, so the mix does nothing there.
//   mountEditor(ctx) -> { open(sound, place), show(), sound(), setTarget(t), render() }
import { VALUES, ALGOS, DEST_ORDER, Q, lfo, mod, LF, info, text, clampV, tickStep, isHundredths, frac, copySound, sameValues, tagNames,
  isMix, mixValue, setMix, sameMix, newSound, MIX_IDS } from "./sound.js";
import { FIZZ_TAGS } from "../js/fizz_sysex.js";
import { esc, MIDI_I, TAGC } from "./ui.js";
import { SRC_NAMES, SRC_COLOURS, LFO_COLOURS, DEST_NAMES, TILE_DEST, PADS, routes, plan, removeSets, routeTitle, srcColour, srcName, sameSrc,
  isFull, FILTER_TYPES } from "./modroute.js";

const K = { c1: "--k1", c2: "--k2", c3: "--k3", c4: "--k4", alg: "--kalg" };
// an operator: its name, ratio, offset, colour and its KEY (C: the velocity's)
const OPS = [["C", Q.RC, Q.OFSC, K.c1, Q.VEL], ["A", Q.RA, Q.OFSA, K.c2, Q.AKEY], ["B1", Q.RB1, Q.OFSB1, K.c3, Q.B1KEY], ["B2", Q.RB2, Q.OFSB2, K.c4, Q.B2KEY]];
// (FIZZ NEON: each section's name after a ring in its colour, as the boards' EDIT pages)
export const SECTIONS = [
  ["ops", "OPERATORS", K.c1], ["openv", "OP ENV", K.c2], ["play", "PLAY", "--yellow"], ["filter", "FILTER", K.c4],
  ["amp", "AMP", K.c3], ["drive", "DRIVE · CRUSH", "--orange"], ["lfo", "LFO", "--cyan"], ["mod", "MOD", K.alg],
];
// each section's block on the panel's 20 x 15 grid [column, row, width, height]; a phone's page four across: its rows
const SEC_BOX = { ops: [1, 1, 10, 5], openv: [11, 1, 10, 5], filter: [1, 6, 10, 5], amp: [11, 6, 5, 5], drive: [16, 6, 5, 3], play: [16, 9, 5, 2],
  lfo: [1, 11, 12, 5], mod: [13, 11, 8, 5] };
const PHONE_ROWS = { ops: 9, openv: 8, play: 3, filter: 10, amp: 5, drive: 4, lfo: 12, mod: 10 };
const LFOC = LFO_COLOURS;
const isDest = (id) => (id >= 47 && id <= 70 && (id - 47) % 8 === LF.DEST) || (id >= 86 && id < 110 && (id - 86) % 3 === 1);
// a white key's and a black key's note from F (the FM-1's keybed: F3 .. G5 at OCT 0)
const WHITE = [0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26];
const BLACK = [[0, 1], [1, 3], [2, 5], [4, 8], [5, 10], [7, 13], [8, 15], [9, 17], [11, 20], [12, 22], [14, 25]];   // [after white, note]
const PC_WHITE = ["a", "s", "d", "f", "g", "h", "j", "k", "l", ";", "'"], PC_BLACK = { w: 1, e: 3, r: 5, y: 8, u: 10, o: 13, p: 15, "[": 17 };
const I_X = '<svg viewBox="0 0 20 20" aria-hidden="true"><path d="M5 5l10 10M15 5L5 15"/></svg>';

export function mountEditor(ctx) {
  const { $, log = () => {} } = ctx;
  const E = { s: null, base: null, place: { label: "NEW SOUND" }, op: 1, oct: 0, held: new Map(), target: null,
    drag: null, raf: 0, picker: null, rec: false, onChange: null, live: false, dims: new Map(), view: {},
    rt: [], md: null, arm: null, card: null, cardEl: null, lay: "" };
  const PAGE_KEY = "fizz-manager-edit-page";        // (the page last shown, remembered)
  E.mode = "";                                       // "panel" (every section) or "tabs" (a page each)
  E.page = (() => { try { return localStorage.getItem(PAGE_KEY) || "ops"; } catch (_) { return "ops"; } })();
  const stage = $("ed-stage"), head = $("ed-head"), kb = $("ed-keys");
  const srcsEl = $("ed-srcs");
  // the KEEP / CANCEL card (live.js) when it sits fixed at the bottom (a phone): its room as --lbh, for the save bar
  // and POOL's moving card to sit above it
  const lbar = $("loadbar");
  const lbh = () => {
    const fixed = lbar && !lbar.hidden && getComputedStyle(lbar).position === "fixed";
    document.documentElement.style.setProperty("--lbh", fixed ? `${Math.max(0, Math.round(window.innerHeight - lbar.getBoundingClientRect().top))}px` : "0px");
  };
  if (lbar && typeof ResizeObserver === "function") new ResizeObserver(lbh).observe(lbar);
  window.addEventListener("resize", lbh);
  // the on-screen keys shown or hidden (remembered in this browser); hidden, the panel takes their room
  const KB_KEY = "fizz-manager-keybed";
  E.kbOn = (() => { try { return localStorage.getItem(KB_KEY) !== "0"; } catch (_) { return true; } })();
  kb.hidden = !E.kbOn;
  function showKeys(on) {
    E.kbOn = !!on;
    try { localStorage.setItem(KB_KEY, on ? "1" : "0"); } catch (_) { /* (no storage: this page only) */ }
    kb.hidden = !on;
    renderHead();
    decide();                                       // (the keys' room: the panel may fit now, or no longer)
  }

  // ---------------------------------------------------------------- values
  const v = (id) => (isMix(id) ? mixValue(E.s, id) : E.s.values[id]);
  function set(id, x, { quiet = false } = {}) {
    if (!E.s || !((id >= 0 && id < VALUES.length) || isMix(id))) return false;
    x = clampV(id, x);
    if (v(id) === x) return false;
    if (!quiet && E.target && E.target.locked && E.target.locked()) { if (ctx.onLocked) ctx.onLocked(); return false; }
    if (isMix(id)) setMix(E.s, id, x); else E.s.values[id] = x;
    if (!quiet && E.target) E.target.param(id, x, E.s);
    if (E.onChange) E.onChange(id, x);
    later();
    return true;
  }
  const later = () => { if (!E.raf) E.raf = requestAnimationFrame(() => { E.raf = 0; renderStage(); renderHead(); }); };
  // FIZZ MIDI (e): slot k of a MIDI sound changed (cc, value, touched); the whole sound to the target
  function setSlot(k, patch, { quiet = false } = {}) {
    if (!E.s || !E.s.midi || !E.s.midi.slots[k]) return false;
    const o = E.s.midi.slots[k], c7 = (x) => Math.max(0, Math.min(127, Math.round(+x || 0)));
    const n = { cc: c7(patch.cc ?? o.cc), value: c7(patch.value ?? o.value), touched: !!(patch.touched ?? o.touched) };
    if (n.cc === o.cc && n.value === o.value && n.touched === o.touched) return false;
    if (!quiet && E.target && E.target.locked && E.target.locked()) { if (ctx.onLocked) ctx.onLocked(); return false; }
    E.s.midi.slots[k] = n;
    if (!quiet && E.target) (E.target.sound ? E.target.sound(E.s) : E.target.audition(E.s));
    if (E.onChange) E.onChange(-1, k);
    later();
    return true;
  }
  // a slot's value by a step (the wheel, the arrows): an untouched slot starts from 0, as its first turn on the unit
  const stepSlot = (k, d) => { const o = E.s.midi.slots[k]; return setSlot(k, { value: (o.touched ? o.value : 0) + d, touched: true }); };
  const untouch = (k) => setSlot(k, { value: 0, touched: false });

  // ---------------------------------------------------------------- the cells (design 12: every value a cell of one grid)
  // cell: [column, row, width, height] on the panel's grid (or a page's)
  const area = ([c, r, w = 1, h = 1]) => `grid-area:${r} / ${c} / span ${h} / span ${w}`;
  // (--l, --w: the value's part of its range, the tile's bar; from the middle for a signed value)
  const span = (id) => { const r = info(id), f = frac(id, v(id)), z = r.min < 0 ? frac(id, 0) : 0; return `--l:${Math.min(z, f).toFixed(3)};--w:${Math.abs(f - z).toFixed(3)}`; };
  // design 13: a value a route moves: its destination (data-dest), a dot each route in its source's colour
  const destAttr = (id) => (TILE_DEST[id] !== undefined ? ` data-dest="${TILE_DEST[id]}"` : "");
  const routesOn = (id) => { const d = TILE_DEST[id]; return d === undefined ? [] : E.rt.filter((x) => x.dest === d); };
  function marks(id) {
    const rs = routesOn(id);
    if (!rs.length) return "";
    return `<span class="mks">${rs.slice(0, 3).map((x) => `<i style="--m:var(${x.c})" data-route="${x.kind}${x.k}" role="button" aria-label="${esc(x.name)} → ${esc(DEST_NAMES[x.dest])}"></i>`).join("")}` +
      `${rs.length > 3 ? `<em>+${rs.length - 3}</em>` : ""}</span>`;
  }
  const modded = (id) => { const rs = routesOn(id); return rs.length ? { cls: " modded", st: `;--m:var(${rs[0].c})` } : { cls: "", st: "" }; };
  // a value: its name over its value, a bar; dragged up / down, the wheel, the arrows
  function T(id, label, c, cell, cls = "") {
    const m = modded(id), t = text(id, v(id));
    return `<div class="t${cls ? ` ${cls}` : ""}${m.cls}" data-id="${id}" data-y="${id}"${destAttr(id)} tabindex="0" role="slider" aria-label="${esc(label)}" aria-valuetext="${esc(t)}" ` +
      `style="${area(cell)};--c:var(${c});${span(id)}${m.st}"><small>${esc(label)}</small><b>${esc(t)}</b>${marks(id)}</div>`;
  }
  // a cell kept where a value is not used now (SHAPE on a type without it, AHD's fourth): dimmed, "—"
  const DIM = (label, cell) => `<div class="t dim0" style="${area(cell)};--c:var(--muted)" aria-hidden="true"><small>${esc(label)}</small><b>—</b></div>`;
  // a long list: a click opens the picker
  function P(id, label, c, cell, cls = "") {
    const m = modded(id), t = text(id, v(id));
    return `<div class="t pk${cls ? ` ${cls}` : ""}${m.cls}" data-id="${id}" data-pick="${id}"${destAttr(id)} tabindex="0" role="button" aria-label="${esc(label)}: ${esc(t)}" ` +
      `style="${area(cell)};--c:var(${c})${m.st}"><small>${esc(label)}</small><b>${esc(t)}</b>${marks(id)}</div>`;
  }
  // a few choices in one cell: a click the next (its pips show which)
  function TG(id, label, c, cell) {
    const r = info(id), x = v(id), nx = x >= r.max ? r.min : x + 1;
    return `<div class="t tg" data-id="${id}" data-set="${id}:${nx}" tabindex="0" role="button" aria-label="${esc(label)}: ${esc(text(id, x))}" style="${area(cell)};--c:var(${c})">` +
      `<small>${esc(label)}</small><b>${esc(text(id, x))}</b><span class="pips">${Array.from({ length: r.max - r.min + 1 }, (_, k) => `<i class="${x === r.min + k ? "on" : ""}"></i>`).join("")}</span></div>`;
  }
  // a choice of a few, side by side: [[value, label], ..]
  function SG(id, label, c, cell, pairs = null, cls = "") {
    const r = info(id), opts = pairs || Array.from({ length: r.max - r.min + 1 }, (_, k) => [r.min + k, text(id, r.min + k)]);
    return `<div class="sg${cls ? ` ${cls}` : ""}" style="${area(cell)};--c:var(${c})" role="group" aria-label="${esc(label || r.name)}">${label ? `<small>${esc(label)}</small>` : ""}` +
      `<span class="opts">${opts.map(([x, l]) => `<button data-set="${id}:${x}" class="${v(id) === x ? "on" : ""}" aria-pressed="${v(id) === x}">${esc(l)}</button>`).join("")}</span></div>`;
  }
  // an operator: its name in a ring, its ratio big (dragged); a click picks it
  function OPT(o, cell) {
    const [nm, rid, , c] = OPS[o], t = text(rid, v(rid)), [w, f] = t.includes(".") ? t.split(".") : [t, ""], m = modded(rid);
    return `<div class="t op${E.op === o ? " on" : ""}${m.cls}" data-id="${rid}" data-y="${rid}" data-op="${o}"${destAttr(rid)} tabindex="0" role="slider" aria-label="RATIO ${nm}" aria-valuetext="${esc(t)}" ` +
      `style="${area(cell)};--c:var(${c})${m.st}"><span class="ring">${nm}</span><b>${esc(w)}${f ? `<small>.${esc(f)}</small>` : ""}</b>${marks(rid)}</div>`;
  }
  // a drawing: a block of cells, drawn in its own pixels (measured: E.dims)
  const D = (key, c, cell, draw, inner = "", cls = "") => {
    const [w, h] = E.dims.get(key) || [240, 120];
    return `<div class="gd${cls ? ` ${cls}` : ""}" data-dk="${key}" style="${area(cell)};--c:var(${c})">${draw(w, h)}${inner}</div>`;
  };
  // a destination without a value of its own (PITCH, P AB2, R ALL, the A+B macros): a pad while a source is dragged
  const PAD = (d, cell, pre = "") => `<div class="pad" data-dest="${d}" style="${area(cell)}">${pre ? `<small>${pre}</small>` : ""}${PADS[d]}</div>`;
  // a section's frame on the grid: a line in the gaps around its cells, its name on it
  const FR = (k, nm, c, cell) => `<div class="fr fr-${k}" style="${area(cell)};--c:var(${c})" aria-hidden="true"><span class="lg"><i></i>${esc(nm)}</span></div>`;
  // an on / off value as a pill in a drawing's corner
  const pill = (id, label) => `<button class="pl${v(id) ? " on" : ""}" data-set="${id}:${v(id) ? 0 : 1}" aria-pressed="${!!v(id)}">${esc(label)}</button>`;

  // a handle in a drawing: a small ring (r 4, ~9.5 px), an invisible r 12 ring takes the pointer; x, y its place (the
  // drawing's pixels), the ids it moves sideways / up and down, the pixels for each value's whole range
  const HR = 4, HIT = 12;
  const handle = (x, y, { xid = -1, yid = -1, xs = 300, ys = 120, c = "--yellow", label = "" } = {}) =>
    `<g class="hg" style="--c:var(${c})" tabindex="0" role="slider" aria-label="${esc(label || [xid, yid].filter((i) => i >= 0).map((i) => info(i).name).join(" · "))}"` +
    `${xid >= 0 ? ` data-x="${xid}" data-xs="${xs}"` : ""}${yid >= 0 ? ` data-y="${yid}" data-ys="${ys}"` : ""} data-id="${yid >= 0 ? yid : xid}">` +
    `<circle class="hit" cx="${x.toFixed(1)}" cy="${y.toFixed(1)}" r="${HIT}"/><circle class="hd" cx="${x.toFixed(1)}" cy="${y.toFixed(1)}" r="${HR}"/></g>`;

  // each drawing's box measured: a drawing whose box changed is drawn again (-> true)
  function measure() {
    let again = false;
    stage.querySelectorAll(".gd[data-dk]").forEach((b) => {
      const k = b.dataset.dk, old = E.dims.get(k), w = Math.round(b.clientWidth), h = Math.round(b.clientHeight);
      if (w < 40 || h < 24) return;
      if (!old || Math.abs(old[0] - w) > 2 || Math.abs(old[1] - h) > 2) { E.dims.set(k, [w, h]); again = true; }
    });
    return again;
  }

  // ---------------------------------------------------------------- the drawings
  // ADSR (or AHD): attack to the top, decay to the sustain level, held, released; handles on the corners
  function adsr(ids, c, { w, h, ahd = false, top = 14 }) {
    const [a, d, s, r] = ids, X = (w - 30) * 0.27 / 127, bot = h - 10, Y = (lv) => bot - (bot - top) * lv / 127;
    const p0 = [10, bot], p1 = [10 + v(a) * X, top];
    let pts, hds;
    if (ahd) {                                       // AHD: ATK, HOLD (NOTE: as long as the key), DEC to nothing
      const hold = v(Q.HOLD), p2 = [p1[0] + (hold >= 127 ? 0.22 * (w - 30) : hold * X), top], p3 = [p2[0] + v(Q.EDEC) * X, bot];
      pts = [p0, p1, p2, p3];
      hds = handle(p1[0], p1[1], { xid: a, xs: 127 * X, c }) + handle(p2[0], p2[1], { xid: Q.HOLD, xs: 127 * X, c: "--yellow" }) +
        handle(p3[0], p3[1], { xid: Q.EDEC, xs: 127 * X, c });
    } else {
      const p2 = [p1[0] + v(d) * X, Y(v(s))], p3 = [p2[0] + 0.16 * (w - 30), Y(v(s))], p4 = [p3[0] + v(r) * X, bot];
      pts = [p0, p1, p2, p3, p4];
      hds = handle(p1[0], p1[1], { xid: a, xs: 127 * X, c }) + handle(p2[0], p2[1], { xid: d, xs: 127 * X, yid: s, ys: bot - top, c: "--yellow" }) +
        handle(p4[0], p4[1], { xid: r, xs: 127 * X, c });
    }
    const path = pts.map(([x, y], i) => `${i ? "L" : "M"}${x.toFixed(1)} ${y.toFixed(1)}`).join(" ");
    return `<svg class="env" viewBox="0 0 ${w} ${h}" style="--c:var(${c})"><path d="${path} Z" class="area"/><path d="${path}" class="line"/>${hds}</svg>`;
  }
  // OPERATORS: the algorithm drawn (modulators above what they modulate, carriers on the bus, the feedback a loop, the
  // modulation's dots running); the roundels dragged (the ratio), a click picks the operator
  function algoSvg() {
    const [mods, fb, ox, oy] = ALGOS[v(Q.ALGO) - 1], out = ox | oy, dep = [0, 0, 0, 0];
    for (let it = 0; it < 4; it++) for (let t = 0; t < 4; t++) for (let o = 0; o < 4; o++)
      if (o !== t && ((mods[t] >> o) & 1) && dep[o] < dep[t] + 1) dep[o] = dep[t] + 1;
    const n = [0, 0, 0, 0], row = [0, 0, 0, 0];
    let md = 0;
    for (let o = 0; o < 4; o++) { row[o] = n[Math.min(3, dep[o])]++; md = Math.max(md, dep[o]); }
    const sp = md >= 3 ? 82 : md === 2 ? 104 : 120, R = 34, BUS = 372;
    const x = [], y = [];
    for (let o = 0; o < 4; o++) {
      const d = Math.min(3, dep[o]), cnt = n[d];
      x[o] = 330 + (row[o] - (cnt - 1) / 2) * 120 + (md >= 3 && cnt === 1 ? (d % 2 ? 34 : -34) : 0);
      y[o] = BUS - 62 - d * sp;
    }
    const mx = v(Q.MIX), sx = (63 - mx) / 127, sy = 1 - sx;
    const parts = [`<line x1="150" y1="${BUS}" x2="520" y2="${BUS}" class="bus"/>`, `<path d="M528 ${BUS - 12}h12v24h-12zM540 ${BUS - 12}l16-14v52l-16-14" class="spk"/>`];
    for (let o = 0; o < 4; o++) if ((out >> o) & 1) {          // a carrier into the bus, as thick as its MIX share
      const sh = (ox >> o) & 1 && (oy >> o) & 1 ? 1 : (ox >> o) & 1 ? sx : sy, lw = 1.5 + 3 * sh, cx = dep[o] ? x[o] + R + 8 : x[o];
      if (dep[o]) parts.push(`<path d="M${x[o] + R} ${y[o]} H${cx} V${BUS}" class="carrier" style="stroke-width:${lw.toFixed(1)};opacity:${(0.35 + 0.65 * sh).toFixed(2)}"/>`);
      else parts.push(`<line x1="${cx}" y1="${y[o] + R}" x2="${cx}" y2="${BUS}" class="carrier" style="stroke-width:${lw.toFixed(1)};opacity:${(0.35 + 0.65 * sh).toFixed(2)}"/>`);
      parts.push(`<circle cx="${cx}" cy="${BUS}" r="${(3 + 2 * sh).toFixed(1)}" class="joint"/>`);
    }
    for (let t = 0; t < 4; t++) for (let o = 0; o < 4; o++) if (o !== t && ((mods[t] >> o) & 1)) {   // o modulates t
      const dx = x[t] - x[o], dy = y[t] - y[o], l = Math.hypot(dx, dy), ux = dx / l, uy = dy / l;
      const sx0 = x[o] + ux * (R + 2), sy0 = y[o] + uy * (R + 2), ex = x[t] - ux * (R + 6), ey = y[t] - uy * (R + 6);
      parts.push(`<line x1="${sx0.toFixed(1)}" y1="${sy0.toFixed(1)}" x2="${ex.toFixed(1)}" y2="${ey.toFixed(1)}" class="modl"/>` +
        `<path d="M${ex.toFixed(1)} ${ey.toFixed(1)} l${(-ux * 14 - uy * 8).toFixed(1)} ${(-uy * 14 + ux * 8).toFixed(1)} l${(uy * 16).toFixed(1)} ${(-ux * 16).toFixed(1)} z" class="arrow"/>` +
        `<circle r="4" class="spark"><animateMotion dur="${(1.1 + 0.15 * (o + t)).toFixed(2)}s" repeatCount="indefinite" path="M${sx0.toFixed(1)} ${sy0.toFixed(1)} L${ex.toFixed(1)} ${ey.toFixed(1)}"/></circle>`);
    }
    if (fb < 4) parts.push(`<path d="M${x[fb] + R * 0.7} ${y[fb] - R * 0.7} a26 26 0 1 1 ${R * 0.5} ${R * 0.95}" class="fb" style="stroke:var(${OPS[fb][3]})"/>`);
    OPS.forEach(([nm, rid, , c], o) => {
      parts.push(`<g class="op ${E.op === o ? "on" : ""}" data-y="${rid}" data-id="${rid}" data-op="${o}" tabindex="0" role="slider" aria-label="RATIO ${nm}" aria-valuetext="${esc(text(rid, v(rid)))}" style="--c:var(${c})">` +
        `<circle cx="${x[o]}" cy="${y[o]}" r="${R}" class="rim"/><circle cx="${x[o]}" cy="${y[o]}" r="${R - 8}" class="inner"/><text x="${x[o]}" y="${y[o] + 9}" text-anchor="middle">${nm}</text></g>`);
    });
    return `<svg class="algosvg" viewBox="120 0 470 420" preserveAspectRatio="xMidYMid meet">${parts.join("")}</svg>`;
  }
  // OP ENV: the two operator envelopes (A: operator A; B: B1 and B2): DELAY a handle at the start, the attack, the
  // decay to END
  const OPENV = [[Q.AATK, Q.ADEC, Q.AEND, Q.ALEV, Q.ADLY, Q.ATRIG, Q.ARST], [Q.BATK, Q.BDEC, Q.BEND, Q.BLEV, Q.BDLY, Q.BTRIG, Q.BRST]];
  function envSvg(k, w, h) {
    const X = (w - 30) * 0.3 / 127, top = 30, bot = h - 12, Y = (lv) => bot - (bot - top) * lv / 127;
    const [atk, dec, end, , dly] = OPENV[k], c = k ? K.c3 : K.c2;
    const p0 = [12 + v(dly) * X, bot], p1 = [p0[0] + v(atk) * X, top], p2 = [p1[0] + v(dec) * X, Y(v(end))], p3 = [w - 12, Y(v(end))];
    const d = `M12 ${bot} L${p0[0].toFixed(1)} ${bot} L${p1[0].toFixed(1)} ${top} L${p2[0].toFixed(1)} ${p2[1].toFixed(1)} L${p3[0]} ${p3[1].toFixed(1)}`;
    return `<svg class="env" viewBox="0 0 ${w} ${h}" style="--c:var(${c})"><path d="${d} L${p3[0]} ${bot} Z" class="area"/><path d="${d}" class="line"/>` +
      `${handle(p0[0], bot, { xid: dly, xs: 127 * X, c: "--muted" })}${handle(p1[0], top, { xid: atk, xs: 127 * X, c })}${handle(p2[0], p2[1], { xid: dec, xs: 127 * X, yid: end, ys: bot - top })}</svg>`;
  }
  // FILTER: its curve, FREQ and RESO (GAIN on the EQ) the handle
  function curve(w, h) {
    const ft = v(Q.FTYPE), f = v(Q.FREQ) / 12700 * 0.82 + 0.06, r = v(Q.RESO) / 12700, sh = v(Q.FSHAPE) / 127, N = 90, db = [];
    for (let i = 0; i <= N; i++) {
      const lr = (i / N - f) * 9;                  // octaves from the cutoff
      const w2 = Math.pow(2, lr), lp = (n) => -10 * Math.log10(1 + Math.pow(w2, 2 * n)), hp = (n) => -10 * Math.log10(1 + Math.pow(1 / w2, 2 * n));
      const peak = 18 * r * Math.exp(-lr * lr * 3);
      let d = 0;
      if (ft === 1) d = lp(2) + peak; else if (ft === 2) d = hp(2) + peak; else if (ft === 3) d = lp(4) + peak;
      else if (ft === 4) d = (sh < 0.5 ? lp(2) * (1 - 2 * sh) + Math.min(lp(1), hp(1)) * 2 * sh : Math.min(lp(1), hp(1)) * (2 - 2 * sh) + hp(2) * (2 * sh - 1)) + peak;
      else if (ft === 5 || ft === 6) { const ph = Math.pow(2, (i / N) * 9) / Math.pow(2, f * 9) * 2 * Math.PI; d = 9 * r * (ft === 5 ? Math.cos(ph) : -Math.cos(ph)) - 3 * sh * (i / N); }
      else if (ft === 7) d = (v(Q.FGAIN) / 64) * 15 * Math.exp(-lr * lr * (0.4 + 3 * sh));
      db.push(Math.max(-36, Math.min(20, d)));
    }
    const t0 = Math.min(26, h * 0.14), sp = h - t0 - Math.min(16, h * 0.1), Y = (d) => t0 + sp * (20 - d) / 56, xs = (i) => 10 + (w - 20) * i / N;
    const path = db.map((d, i) => `${i ? "L" : "M"}${xs(i).toFixed(1)} ${Y(d).toFixed(1)}`).join(" ");
    const hx = xs(f * N), hy = Y(db[Math.round(f * N)]) - 2, yid = ft === 7 ? Q.FGAIN : ft === 0 ? -1 : Q.RESO;
    return `<svg class="curve" viewBox="0 0 ${w} ${h}" style="--c:var(${K.c4})">${[1, 2, 3, 4, 5].map((k) => `<line x1="${(w * k / 6).toFixed(0)}" y1="14" x2="${(w * k / 6).toFixed(0)}" y2="${h - 12}" class="grid"/>`).join("")}` +
      `<path d="${path} L${w - 10} ${h - 12} L10 ${h - 12} Z" class="area"/><path d="${path}" class="line"/>` +
      `${handle(hx, Math.max(14, hy), { xid: Q.FREQ, xs: (w - 20) * 0.82, yid, ys: sp * 0.8, c: K.c4, label: "FREQ" })}</svg>`;
  }
  // DRIVE · CRUSH: a sine through the crusher (its bits, its rate)
  function crushSvg(w, h) {
    const bits = v(Q.BITS), srr = v(Q.SRR), q = bits ? Math.max(2, Math.round(2 + 14 * (1 - bits / 127))) : 0, hold = srr ? 1 + Math.floor(srr / 9) : 1;
    let d = "", last = 0;
    for (let i = 0; i <= 160; i++) {
      if (i % hold === 0) { let s = Math.sin(i / 160 * Math.PI * 6); if (q) s = Math.round(s * q) / q; last = s; }
      d += `${i ? "L" : "M"}${(10 + (w - 20) * i / 160).toFixed(1)} ${(h / 2 - last * (h / 2 - 7)).toFixed(1)}`;
    }
    return `<svg class="crush" viewBox="0 0 ${w} ${h}" style="--c:var(--pink)"><path d="${d}" class="line"/></svg>`;
  }
  // LFO: its wave (moving), the plug at its right
  function wavePath(k, w, h) {
    const wv = v(lfo(k, LF.WAVE)), sp = Math.abs(v(lfo(k, LF.SPEED))) / 6400, dp = v(lfo(k, LF.DEPTH)) / 6400, ph = v(lfo(k, LF.PHASE)) / 128;
    const fade = v(lfo(k, LF.FADE)), cyc = 1.2 + 3.3 * sp + (v(lfo(k, LF.MULT)) % 12) / 8, amp = (0.12 + 0.88 * Math.abs(dp)) * (dp < 0 ? -1 : 1);
    const N = 160, pts = [], mid = h / 2 + 6, A = Math.max(4, h / 2 - 16);
    let seed = 7 + k;
    const rnd = () => (seed = (seed * 1103515245 + 12345) & 0x7fffffff) / 0x7fffffff * 2 - 1;
    let held = rnd(), lastStep = -1;
    for (let i = 0; i <= N; i++) {
      const t = i / N, p = (t * cyc + ph) % 1;
      let y;
      if (wv === 0) y = 1 - 4 * Math.abs(p - 0.5); else if (wv === 1) y = Math.sin(p * 2 * Math.PI); else if (wv === 2) y = p < 0.5 ? 1 : -1;
      else if (wv === 3) y = 1 - 2 * p; else if (wv === 4) y = 2 * p - 1; else if (wv === 5) y = 2 * Math.exp(-4 * p) - 1;
      else { const st = Math.floor(t * cyc * 2 + ph * 2); if (st !== lastStep) { held = rnd(); lastStep = st; } y = held; }
      const fm = fade < 0 ? Math.min(1, t * 64 / Math.max(1, -fade)) : fade > 0 ? Math.max(0, 1 - t * 64 / Math.max(1, 64 - fade + 1) * 0.5 * (fade / 63)) : 1;
      pts.push([10 + (w - 64) * t, mid - y * amp * fm * A]);
    }
    return pts.map(([x, y], i) => `${i ? "L" : "M"}${x.toFixed(1)} ${y.toFixed(1)}`).join(" ");
  }
  const waveSvg = (k, w, h) => {
    const d = wavePath(k, w, h);
    return `<svg class="wave" viewBox="0 0 ${w} ${h}" style="--c:var(${LFOC[k]})"><line x1="10" y1="${h / 2 + 6}" x2="${w - 54}" y2="${h / 2 + 6}" class="axis"/><path d="${d}" class="line"/>` +
      `<circle r="3" class="spark"><animateMotion dur="${(3.2 - 2.4 * Math.abs(v(lfo(k, LF.SPEED))) / 6400).toFixed(2)}s" repeatCount="indefinite" path="${d}"/></circle></svg>`;
  };

  // ---------------------------------------------------------------- the sections' blocks
  // o: the block's first cell { c, r } and phone (a phone's page: four across)
  function opsBlock({ c, r, phone }) {
    const a = v(Q.ALGO), m = modded(Q.ALGO);
    const algoT = (cell) => `<div class="t algo algoT${m.cls}" data-id="${Q.ALGO}" data-y="${Q.ALGO}" data-ys="240"${destAttr(Q.ALGO)} tabindex="0" role="slider" aria-label="ALGO" aria-valuetext="${a}" ` +
      `style="${area(cell)};--c:var(--kalg)${m.st}"><small>ALGO</small><b>${a}</b>${marks(Q.ALGO)}</div>`;
    const arrow = (nav, to, d, l, cell) => `<button class="nav" data-set="${Q.ALGO}:${to}" data-nav="${nav}" aria-label="${l}" style="${area(cell)}"><svg viewBox="0 0 24 24" aria-hidden="true"><path d="${d}"/></svg></button>`;
    const nav = (c0, c1) => `<span class="algonav">${arrow("prev", (a + 6) % 8 + 1, "M15 5l-7 7 7 7", "Previous algorithm", c0)}${arrow("next", a % 8 + 1, "M9 5l7 7-7 7", "Next algorithm", c1)}</span>`;
    const algoD = (cell) => `<div class="gd algod" style="${area(cell)};--c:var(${K.c1})">${algoSvg()}</div>`;
    let h = "";
    if (phone) {
      h += algoT([c, r, 2, 2]) + algoD([c + 2, r, 2, 3]) + nav([c, r + 2], [c + 1, r + 2]) + PAD(1, [c + 2, r]) + PAD(2, [c + 3, r]) + PAD(47, [c + 2, r + 1]);
      OPS.forEach((op, i) => { h += OPT(i, [c, r + 3 + i, 2]) + T(op[2], "OFS", op[3], [c + 2, r + 3 + i], "ofs") + T(op[4], i ? `${op[0]} KEY` : "VEL", op[3], [c + 3, r + 3 + i]); });
      return h + T(Q.HARM, "HARM", K.c1, [c, r + 7]) + T(Q.DTUN, "DTUN", K.c2, [c + 1, r + 7]) + T(Q.FDBK, "FDBK", K.c4, [c + 2, r + 7]) + T(Q.MIX, "MIX", K.c3, [c + 3, r + 7]) +
        SG(Q.PHRT, "PHASE RESET", K.c1, [c, r + 8, 4]);
    }
    h += algoT([c, r, 2, 3]) + nav([c, r + 3], [c + 1, r + 3]) + algoD([c + 2, r, 4, 4]) + PAD(1, [c + 2, r]) + PAD(2, [c + 3, r]) + PAD(47, [c + 4, r]);
    OPS.forEach((op, i) => { h += OPT(i, [c + 6, r + i, 2]) + T(op[2], "OFS", op[3], [c + 8, r + i], "ofs") + T(op[4], i ? `${op[0]} KEY` : "VEL", op[3], [c + 9, r + i]); });
    return h + T(Q.HARM, "HARM", K.c1, [c, r + 4]) + T(Q.DTUN, "DTUN", K.c2, [c + 1, r + 4]) + T(Q.FDBK, "FDBK", K.c4, [c + 2, r + 4]) + T(Q.MIX, "MIX", K.c3, [c + 3, r + 4]) +
      SG(Q.PHRT, "PHASE RESET", K.c1, [c + 4, r + 4, 6]);
  }
  function openvBlock({ c, r, phone }) {
    let h = "";
    [0, 1].forEach((k) => {
      const [atk, dec, end, lev, dly, trg, rst] = OPENV[k], col = k ? K.c3 : K.c2;
      const cc = phone ? c : c + 5 * k, rr = phone ? r + 4 * k : r;
      const cap = `<span class="cap">ENV ${k ? "B" : "A"}<span class="hud">${k ? "B1 · B2" : "A"}</span></span><span class="pills">${pill(trg, "TRIG")}${pill(rst, "RESET")}</span>`;
      h += D(`env${k}`, col, [cc, rr, phone ? 4 : 5, phone ? 2 : 4], (w, hh) => envSvg(k, w, hh), cap);
      if (phone) {
        [49, 50, 51, 52].forEach((d, i) => { h += PAD(d, [cc + i, rr + 1], "A+B"); });
        h += PAD(48, [cc + 1, rr + 3], "A+B");
        h += T(atk, "ATK", col, [cc, rr + 2]) + T(dec, "DEC", col, [cc + 1, rr + 2]) + T(end, "END", col, [cc + 2, rr + 2]) + T(dly, "DLY", col, [cc + 3, rr + 2]) + T(lev, "LEVEL", col, [cc, rr + 3]);
      } else {
        [49, 50, 51, 52, 48].forEach((d, i) => { h += PAD(d, [cc + i, rr + 3], "A+B"); });
        h += T(atk, "ATK", col, [cc, rr + 4]) + T(dec, "DEC", col, [cc + 1, rr + 4]) + T(end, "END", col, [cc + 2, rr + 4]) + T(dly, "DLY", col, [cc + 3, rr + 4]) + T(lev, "LEVEL", col, [cc + 4, rr + 4]);
      }
    });
    return h;
  }
  function filterBlock({ c, r, phone }) {
    const ft = v(Q.FTYPE), shp = ft === 4 ? "TYPE" : ft === 7 ? "Q" : ft >= 5 ? "LPF" : "SHAPE";
    const R = (cell) => (ft === 7 ? T(Q.FGAIN, "GAIN", K.c2, cell) : T(Q.RESO, ft >= 5 ? "FDBK" : "RESO", K.c2, cell));
    const SH = (cell) => (ft >= 4 ? T(Q.FSHAPE, shp, K.c3, cell) : DIM("SHAPE", cell));
    const fenv = (cell) => D("fenv", K.c4, cell, (w, h) => adsr([Q.FATK, Q.FDEC, Q.FSUS, Q.FREL], K.c4, { w, h, top: 30 }), '<span class="cap">ENV</span>');
    if (phone) {
      return D("curve", K.c4, [c, r, 4, 3], curve) + T(Q.FREQ, "FREQ", K.c4, [c, r + 3]) + R([c + 1, r + 3]) + SH([c + 2, r + 3]) + T(Q.FKTRK, "KEY TRK", K.c3, [c + 3, r + 3]) +
        T(Q.BASE, "BASE", K.c1, [c, r + 4]) + T(Q.WIDTH, "WIDTH", K.c2, [c + 1, r + 4]) + SG(Q.BWPOST, "BASE · WIDTH", K.c3, [c + 2, r + 4, 2], [[0, "BEFORE"], [1, "AFTER"]]) +
        fenv([c, r + 5, 4, 2]) + T(Q.FATK, "ATK", K.c4, [c, r + 7]) + T(Q.FDEC, "DEC", K.c4, [c + 1, r + 7]) + T(Q.FSUS, "SUS", K.c4, [c + 2, r + 7]) + T(Q.FREL, "REL", K.c4, [c + 3, r + 7]) +
        T(Q.FDEPTH, "DEPTH", K.c4, [c, r + 8]) + T(Q.FDLY, "DLY", K.c4, [c + 1, r + 8]) + SG(Q.FTYPE, "TYPE", K.c4, [c + 2, r + 8, 2], FILTER_TYPES.slice(0, 3), "ftype") +
        SG(Q.FTYPE, "", K.c4, [c, r + 9, 4], FILTER_TYPES.slice(3), "ftype");
    }
    return D("curve", K.c4, [c, r, 6, 3], curve) + fenv([c + 6, r, 4, 3]) +
      T(Q.FREQ, "FREQ", K.c4, [c, r + 3]) + R([c + 1, r + 3]) + SH([c + 2, r + 3]) + T(Q.FKTRK, "KEY TRK", K.c3, [c + 3, r + 3]) + T(Q.BASE, "BASE", K.c1, [c + 4, r + 3]) + T(Q.WIDTH, "WIDTH", K.c2, [c + 5, r + 3]) +
      T(Q.FATK, "ATK", K.c4, [c + 6, r + 3]) + T(Q.FDEC, "DEC", K.c4, [c + 7, r + 3]) + T(Q.FSUS, "SUS", K.c4, [c + 8, r + 3]) + T(Q.FREL, "REL", K.c4, [c + 9, r + 3]) +
      SG(Q.FTYPE, "TYPE", K.c4, [c, r + 4, 6], FILTER_TYPES, "ftype") + T(Q.FDEPTH, "DEPTH", K.c4, [c + 6, r + 4]) + T(Q.FDLY, "DLY", K.c4, [c + 7, r + 4]) +
      SG(Q.BWPOST, "BASE · WIDTH", K.c3, [c + 8, r + 4, 2], [[0, "BEFORE"], [1, "AFTER"]]);
  }
  // AMP: the amp envelope (ADSR or AHD: ENV), LEVEL, the mix (PAN, the chorus, delay and reverb sends)
  function ampBlock({ c, r, phone }) {
    const ahd = v(Q.AMOD) === 1, e = phone ? r + 2 : r + 3;
    const env = ahd ? T(Q.EATK, "ATK", K.c3, [c, e]) + T(Q.HOLD, "HOLD", "--yellow", [c + 1, e]) + T(Q.EDEC, "DEC", K.c3, [c + 2, e]) + DIM("", [c + 3, e])
      : T(Q.EATK, "ATK", K.c3, [c, e]) + T(Q.EDEC, "DEC", K.c3, [c + 1, e]) + T(Q.ESUS, "SUS", K.c3, [c + 2, e]) + T(Q.EREL, "REL", K.c3, [c + 3, e]);
    const mix = (m) => T(MIX_IDS.pan, "PAN", "--text", [c, m], "pan") + T(MIX_IDS.chorus, "CHO", "--cyan", [c + 1, m]) + T(MIX_IDS.delay, "DEL", "--yellow", [c + 2, m]) + T(MIX_IDS.reverb, "REV", "--violet", [c + 3, m]);
    const draw = (cell) => D("amp", K.c3, cell, (w, h) => adsr([Q.EATK, Q.EDEC, Q.ESUS, Q.EREL], K.c3, { w, h, ahd }));
    if (phone) return draw([c, r, 4, 2]) + env + T(Q.LEVEL, "LEVEL", K.c3, [c, r + 3]) + TG(Q.AMOD, "ENV", K.c3, [c + 1, r + 3]) + mix(r + 4);
    return draw([c, r, 5, 3]) + env + T(Q.LEVEL, "LEVEL", K.c3, [c + 4, r + 3]) + mix(r + 4) + TG(Q.AMOD, "ENV", K.c3, [c + 4, r + 4]);
  }
  // DRIVE and its route; CRUSH (the bits, the rate, before / after)
  function driveBlock({ c, r, phone }) {
    return D("crush", "--pink", [c, r, phone ? 4 : 5, 2], crushSvg) + T(Q.DRIVE, "DRIVE", "--orange", [c, r + 2]) + TG(Q.DRPRE, "ROUTE", "--orange", [c + 1, r + 2]) +
      T(Q.BITS, "BITS", "--pink", [c + 2, r + 2]) + T(Q.SRR, "SRR", "--pink", [c + 3, r + 2]) + TG(Q.CRPOST, "ROUTE", "--pink", phone ? [c, r + 3] : [c + 4, r + 2]);
  }
  // PLAY: the portamento (PORTA, TIME, SLOPE) and the voice (VOICE, UNISON, DETUNE, SPREAD, PRIO)
  const VO = 110;                                  // (the VOICE page's first value)
  function playBlock({ c, r, phone }) {
    const porta = SG(Q.PORT, "PORTA", "--yellow", [c, r, 3]) + T(Q.PTIM, "TIME", "--yellow", [c + 3, r]);
    if (phone) {
      return porta + TG(Q.PSLOPE, "SLOPE", "--yellow", [c, r + 1]) + TG(VO, "VOICE", K.c1, [c + 1, r + 1]) + T(VO + 1, "UNISON", K.c2, [c + 2, r + 1]) + T(VO + 2, "DETUNE", K.c3, [c + 3, r + 1]) +
        T(VO + 3, "SPREAD", K.c4, [c, r + 2]) + P(VO + 4, "PRIO", K.alg, [c + 1, r + 2]);
    }
    return porta + TG(Q.PSLOPE, "SLOPE", "--yellow", [c + 4, r]) + TG(VO, "VOICE", K.c1, [c, r + 1]) + T(VO + 1, "UNISON", K.c2, [c + 1, r + 1]) + T(VO + 2, "DETUNE", K.c3, [c + 2, r + 1]) +
      T(VO + 3, "SPREAD", K.c4, [c + 3, r + 1]) + P(VO + 4, "PRIO", K.alg, [c + 4, r + 1]);
  }
  // LFO: each its wave and plug, DEST WAVE TRIG SPEED, DEPTH MULT FADE PHASE
  function lfoBlock({ c, r, phone }) {
    let h = "";
    for (let k = 0; k < 3; k++) {
      const col = LFOC[k], cc = phone ? c : c + 4 * k, rr = phone ? r + 4 * k : r, dh = phone ? 2 : 3;
      const plug = `<span class="cap">LFO ${k + 1}</span><span class="jack${sameSrc(E.arm, { kind: "lfo", k }) ? " isarmed" : ""}" data-lfo="${k}" tabindex="0" role="button" aria-label="LFO ${k + 1}" style="--c:var(${col})"></span>`;
      h += D(`lfo${k}`, col, [cc, rr, 4, dh], (w, hh) => waveSvg(k, w, hh), plug, "lfosrc");
      h += P(lfo(k, LF.DEST), "DEST", col, [cc, rr + dh]) + P(lfo(k, LF.WAVE), "WAVE", col, [cc + 1, rr + dh]) + P(lfo(k, LF.TRIG), "TRIG", col, [cc + 2, rr + dh]) +
        T(lfo(k, LF.SPEED), "SPEED", col, [cc + 3, rr + dh]) + T(lfo(k, LF.DEPTH), "DEPTH", col, [cc, rr + dh + 1]) + P(lfo(k, LF.MULT), "MULT", col, [cc + 1, rr + dh + 1]) +
        T(lfo(k, LF.FADE), "FADE", col, [cc + 2, rr + dh + 1]) + T(lfo(k, LF.PHASE), "PHASE", col, [cc + 3, rr + dh + 1]);
    }
    return h;
  }
  // MOD: the eight sources (each its colour: dragged onto a value, a route), the eight slots (four cells each, the
  // same size full or empty): SRC, the cable (its depth), DEST, DEPTH, ×
  function modBlock({ c, r, phone }) {
    let h = "";
    for (let i = 1; i <= 8; i++) {
      const cell = phone ? [c + (i - 1) % 4, r + Math.floor((i - 1) / 4)] : [c + i - 1, r];
      h += `<div class="src${sameSrc(E.arm, { kind: "mod", src: i }) ? " isarmed" : ""}" data-src="${i}" tabindex="0" role="button" aria-label="${SRC_NAMES[i]}" style="${area(cell)};--c:var(${SRC_COLOURS[i]})"><span class="grip"></span>${SRC_NAMES[i]}</div>`;
    }
    for (let k = 0; k < 8; k++) {
      const cell = phone ? [c, r + 2 + k, 4] : [k < 4 ? c : c + 4, r + 1 + (k % 4), 4], src = v(mod(k, 0));
      if (!src) { h += `<div class="mslot empty" data-slot="${k}" style="${area(cell)}"><span class="sn">${k + 1}</span><button class="madd" data-set="${mod(k, 0)}:1" aria-label="Modulation slot ${k + 1}: add">+</button></div>`; continue; }
      const dp = v(mod(k, 2)), wv = Math.max(1, Math.abs(dp) / 32), col = SRC_COLOURS[src], dc = dp < 0 ? "--orange" : col;
      h += `<div class="mslot" data-slot="${k}" style="${area(cell)};--c:var(${col})">` +
        `<div class="t pk" data-id="${mod(k, 0)}" data-pick="${mod(k, 0)}" tabindex="0" role="button" aria-label="SRC ${k + 1}: ${esc(SRC_NAMES[src])}" style="--c:var(${col})"><small>SRC</small><b>${esc(SRC_NAMES[src])}</b><span class="sn">${k + 1}</span></div>` +
        `<div class="cab" data-x="${mod(k, 2)}" data-id="${mod(k, 2)}" style="--c:var(${dc})" aria-hidden="true"><svg viewBox="0 0 100 30" preserveAspectRatio="none"><path d="M6 15 C38 ${dp >= 0 ? 2 : 28} 62 ${dp >= 0 ? 28 : 2} 94 15" style="stroke-width:${wv.toFixed(1)}" vector-effect="non-scaling-stroke"/></svg></div>` +
        `<div class="t pk" data-id="${mod(k, 1)}" data-pick="${mod(k, 1)}" tabindex="0" role="button" aria-label="DEST ${k + 1}: ${esc(text(mod(k, 1), v(mod(k, 1))))}" style="--c:var(--text)"><small>DEST</small><b>${esc(text(mod(k, 1), v(mod(k, 1))))}</b></div>` +
        `<div class="t" data-id="${mod(k, 2)}" data-y="${mod(k, 2)}" tabindex="0" role="slider" aria-label="DEPTH ${k + 1}" aria-valuetext="${esc(text(mod(k, 2), dp))}" style="--c:var(${dc});${span(mod(k, 2))}"><small>DEPTH</small><b>${esc(text(mod(k, 2), dp))}</b></div>` +
        `<button class="mx" data-set="${mod(k, 0)}:0" aria-label="Modulation slot ${k + 1}: off">${I_X}</button></div>`;
    }
    return h;
  }
  const BLOCKS = { ops: opsBlock, openv: openvBlock, filter: filterBlock, amp: ampBlock, drive: driveBlock, lfo: lfoBlock, mod: modBlock, play: playBlock };
  // a section's cells for the mode: the panel's (its place on the 20 x 15 grid, its frame), a page's (from the first
  // cell), a phone's (four across)
  function blockHtml(k) {
    if (E.mode === "panel") {
      const [c, r, w, h] = SEC_BOX[k], [, nm, col] = SECTIONS.find((x) => x[0] === k);
      return FR(k, nm, col, [c, r, w, h]) + BLOCKS[k]({ c, r, phone: false });
    }
    return BLOCKS[k]({ c: 1, r: 1, phone: E.lay === "phone" });
  }

  // ---------------------------------------------------------------- the frame: header, the panel, keys
  // the header: the track the editor follows big (pink: LIVE; dim: only shown, TRACK off), the place, the name, the
  // tags; the track strip (live.js: SELECTED, 1..6, LIVE) when an FM-1 that edits tracks is there; the actions
  function renderHead() {
    if (!E.s) return;
    const t = E.target, dirty = !sameValues(E.s, E.base) || !sameMix(E.s, E.base) || E.s.name !== E.base.name || E.s.tags !== E.base.tags;
    const live = !!(t && t.live), locked = !!(t && t.locked && t.locked()), tc = live ? "--pink" : "--violet";
    const tl = t ? t.label() : "";
    const can = ctx.canSave ? ctx.canSave(E) : { save: false, saveNew: false };
    const strip = ctx.strip ? ctx.strip(E) : "";
    const nm = head.querySelector("#ed-name");
    if (nm && document.activeElement === nm) {     // (typing: only the parts around the name)
      head.querySelector("#ed-acts").innerHTML = acts();
      return;
    }
    // (FIZZ NEON: where it plays, then REVERT, SAVE AS NEW, EXPORT outlined, SAVE the one fill, breathing when there is
    // something to save)
    function acts() {
      const where = `${strip}${strip && live ? "" : `<button class="aud ${live ? "live" : t && t.ready() ? "on" : ""}" id="ed-target" style="--c:var(${tc})" aria-label="Where the sound plays: ${esc(tl)}"><span class="dot"></span>${esc(tl)}</button>`}`;
      // (NEON: REVERT, SAVE AS NEW, SAVE a bar of their own: fixed at the bottom on a phone upright)
      const kbt = `<button class="icon kbtg${E.kbOn ? " on" : ""}" id="ed-kbtg" aria-pressed="${E.kbOn}" aria-label="On-screen keys" title="${E.kbOn ? "Hide the keys" : "Show the keys"}">` +
        '<svg viewBox="0 0 24 24" aria-hidden="true"><rect x="2" y="6" width="20" height="12" rx="2.5"/><path d="M6 10h.01M10 10h.01M14 10h.01M18 10h.01M7 14h10"/></svg></button>';
      return `<div class="edwhere">${where}</div>${kbt}
        <button class="ol" id="ed-export" style="--c:var(--violet)">EXPORT</button>
        <div class="edsave" id="ed-savebar" role="group" aria-label="Save">
        <button class="ol" id="ed-revert" ${dirty && !locked ? "" : "disabled"}><svg viewBox="0 0 24 24" aria-hidden="true"><path d="M9 7H4V2"/><path d="M4.5 7A8 8 0 1 1 4 12"/></svg>REVERT</button>
        <button class="ol" id="ed-savenew" style="--c:var(--pink)" ${can.saveNew ? "" : "disabled"}>SAVE AS NEW</button>
        <button class="arc${can.save && dirty ? " breathe" : ""}" id="ed-save" style="--c:var(--pink)" ${can.save ? "" : "disabled"}>SAVE</button></div>`;
    }
    const tk = E.place.track >= 0 && E.place.fromTarget ? E.place.track : -1;
    const pl = /^(.*\S)\s+(\d+)$/.exec(E.place.label || ""), plh = pl ? `${esc(pl[1])} <span class="hud">${pl[2]}</span>` : esc(E.place.label || "");
    head.innerHTML = `${tk >= 0 ? `<div class="trkbig ${live && !locked ? "live" : "view"}" aria-label="Track ${tk + 1}${locked ? ", shown only" : ", live"}"><small>TRACK</small><b>${tk + 1}</b></div>` : ""}
      <div class="edname"><div class="h">${E.s.midi ? '<span class="mchip">MIDI</span> ' : ""}${plh}${dirty ? ' <span class="edited">EDITED</span>' : ""}</div>
        <div class="ednrow"><input class="bigname" id="ed-name" maxlength="12" spellcheck="false" autocomplete="off" value="${esc(E.s.name)}" aria-label="Name">${tagChip()}</div></div>
      <div class="row edacts" id="ed-acts">${acts()}</div>`;
    stage.classList.toggle("locked", locked);
  }
  // ---------------------------------------------------------------- the tag (NEON: one chip beside the name)
  // a click opens the seven tags and NONE; a sound with one tag (or none): a pick sets it and closes; with two or more
  // (the mask allows it): a click toggles one, the popover stays
  const nTags = (m) => tagNames(m).length;
  function tagChip() {
    const tg = tagNames(E.s.tags), t0 = tg[0];
    return `<button class="chip edtag${t0 ? "" : " none"}" id="ed-tag" style="--c:var(${t0 ? TAGC[t0] : "--muted"})" aria-haspopup="menu" aria-expanded="${!!E.tagPop}" aria-label="Tag: ${esc(tg.join(", ") || "none")}">` +
      `${t0 || "TAG"}${tg.length > 1 ? `<small>+${tg.length - 1}</small>` : ""}</button>`;
  }
  const tagPopHtml = () => FIZZ_TAGS.map((tg, k) => {
    const on = !!((E.s.tags >> k) & 1);
    return `<button class="chip${on ? " on" : ""}" data-tag="${k}" role="menuitemcheckbox" aria-checked="${on}" style="--c:var(${TAGC[tg]})">${tg}</button>`;
  }).join("") + `<button class="chip none${E.s.tags ? "" : " on"}" data-tag="-1" role="menuitemcheckbox" aria-checked="${!E.s.tags}" style="--c:var(--muted)">NONE</button>`;
  function openTags() {
    closeTags();
    const el = head.querySelector("#ed-tag");
    if (!el || !E.s) return;
    const p = document.createElement("div");
    p.className = "tagpop";
    p.setAttribute("role", "menu");
    p.setAttribute("aria-label", "Tag");
    p.innerHTML = tagPopHtml();
    document.body.appendChild(p);
    const b = el.getBoundingClientRect(), w = p.offsetWidth;
    p.style.left = `${Math.max(12, Math.min(window.innerWidth - w - 12, b.left))}px`;
    p.style.top = `${b.bottom + 8 + window.scrollY}px`;
    E.tagPop = p;
    p.addEventListener("click", (ev) => { const o = ev.target.closest("[data-tag]"); if (o) pickTag(+o.dataset.tag); });
    (p.querySelector(".on") || p.firstElementChild).focus();
    renderHead();
  }
  function closeTags(refocus = false) {
    if (!E.tagPop) return;
    E.tagPop.remove();
    E.tagPop = null;
    renderHead();
    if (refocus) { const el = head.querySelector("#ed-tag"); if (el) el.focus(); }
  }
  function pickTag(k) {
    if (!E.s) return;
    const many = nTags(E.s.tags) >= 2;
    if (k < 0) E.s.tags = 0;
    else E.s.tags = many ? E.s.tags ^ (1 << k) : 1 << k;
    if (many && k >= 0) { E.tagPop.innerHTML = tagPopHtml(); const o = E.tagPop.querySelector(`[data-tag="${k}"]`); if (o) o.focus(); renderHead(); }
    else closeTags(true);
  }
  document.addEventListener("pointerdown", (ev) => { if (E.tagPop && !E.tagPop.contains(ev.target) && !ev.target.closest("#ed-tag")) closeTags(); }, true);
  document.addEventListener("keydown", (ev) => { if (ev.key === "Escape" && E.tagPop) { ev.preventDefault(); closeTags(true); } });

  // FIZZ MIDI (e): a MIDI sound's page n (0..6): its four slots, KNOB 1..4's colours
  const MK = [K.c1, K.c2, K.c3, K.c4];
  // (drawn once; then each slot changed in place: a bar dragged or clicked twice stays the same element)
  const ccHtml = (k, x) => (E.ccEdit === k ? `<input class="mccin" id="ed-cc" data-ccin="${k}" inputmode="numeric" maxlength="3" value="${x.cc}" aria-label="CC number">`
    : `<button class="mcc" data-cc="${k}" aria-label="CC number ${x.cc}">CC ${x.cc}</button>`);
  const midiPage = (n) => `<div class="mslots">${[0, 1, 2, 3].map((j) => {
    const k = n * 4 + j;
    return `<div class="msl" data-msl="${k}" style="--c:var(${MK[j]})"><span class="mbar" data-slot="${k}" tabindex="0" role="slider" aria-valuemin="0" aria-valuemax="127"><i></i></span>` +
      `<span class="mccw"></span><span class="mval"><b></b><button class="mx" data-untouch="${k}">×</button></span></div>`;
  }).join("")}</div>`;
  function slotDraw(k) {
    const x = E.s.midi.slots[k], el = stage.querySelector(`[data-msl="${k}"]`);
    if (!el) return;
    const key = `${x.cc}|${x.value}|${x.touched}|${E.ccEdit === k}`;
    if (el.dataset.k === key) return;
    el.dataset.k = key;
    const val = x.touched ? String(x.value) : "—", bar = el.querySelector(".mbar"), w = el.querySelector(".mccw"), ux = el.querySelector(".mx");
    el.classList.toggle("un", !x.touched);
    bar.setAttribute("aria-label", `CC ${x.cc}`);
    bar.setAttribute("aria-valuenow", String(x.value));
    bar.setAttribute("aria-valuetext", x.touched ? val : "untouched");
    bar.firstElementChild.style.height = `${x.touched ? (x.value / 127 * 100).toFixed(1) : 0}%`;
    el.querySelector(".mval b").textContent = val;
    ux.hidden = !x.touched;
    ux.setAttribute("aria-label", `CC ${x.cc}: untouched`);
    const typing = E.ccEdit === k;
    if (w.dataset.typing !== String(typing) || !typing) { w.dataset.typing = String(typing); w.innerHTML = ccHtml(k, x); }
  }
  function renderMidi() {
    if (!stage.querySelector(".mpg")) {
      stage.innerHTML = Array.from({ length: 7 }, (_, n) => `<section class="pn mpg" data-mpg="${n}" style="--c:var(--midi)" aria-label="MIDI page ${n + 1}">` +
        `<div class="pnh">${MIDI_I}<span class="mdots" aria-hidden="true">${Array.from({ length: 7 }, (_, i) => `<i class="${i === n ? "on" : ""}"></i>`).join("")}</span>${n + 1}</div><div class="pnb">${midiPage(n)}</div></section>`).join("");
      drawn.clear();
    }
    for (let k = 0; k < E.s.midi.slots.length; k++) slotDraw(k);
    const ci = stage.querySelector("#ed-cc");
    if (ci && document.activeElement !== ci) { ci.focus(); ci.select(); }
  }
  // the panel: every section at once; a section drawn again only when its cells changed
  const drawn = new Map();
  // one panel or pages: the panel where the window holds it (about 1400 x 860 with the keys, 1280 x 760 without), its
  // cells big enough (measured); else a page a section under tabs (a phone: four across)
  const pagesEl = $("ed-pages");
  const shown = () => !$("tab-edit").hidden;
  const wantPanel = () => (E.kbOn ? window.innerWidth >= 1400 && window.innerHeight >= 860 : window.innerWidth >= 1280 && window.innerHeight >= 760);
  const layOf = (m) => (m === "panel" ? "panel" : window.innerWidth < 700 ? "phone" : "page");
  const bound = () => window.innerWidth > 900 || window.innerWidth > window.innerHeight;   // (the card the window's height)
  let decided = false;                               // (a decide() asked for the next frame)
  function setMode(m) {
    const lay = layOf(m);
    if (E.mode !== m || E.lay !== lay) { E.mode = m; E.lay = lay; drawn.clear(); E.dims.clear(); }
    stage.classList.toggle("panel", m === "panel");
    stage.classList.toggle("paged", m === "tabs");
    stage.classList.toggle("phone", lay === "phone");
    pagesEl.hidden = m !== "tabs";
    if (srcsEl) srcsEl.hidden = m !== "tabs";
    SECTIONS.forEach(([k]) => {
      const s = stage.querySelector(`[data-sec="${k}"]`);
      if (s) { s.style.setProperty("--pc", lay === "phone" ? 4 : SEC_BOX[k][2]); s.style.setProperty("--pr", lay === "phone" ? PHONE_ROWS[k] : SEC_BOX[k][3]); }
    });
    renderPages();
  }
  // the panel's cells too small (or the grid over its room): pages
  const tooSmall = () => {
    const t = [...stage.querySelectorAll(".t:not(.algoT):not(.op)")].find((e) => e.offsetWidth > 0);
    return !t || t.offsetHeight < 28 || t.offsetWidth < 44;
  };
  // the mode tried afresh
  function decide() {
    if (!E.s || E.s.midi || !shown()) return;
    setMode(wantPanel() ? "panel" : "tabs");
    renderStage();
    if (E.mode === "panel" && tooSmall()) { setMode("tabs"); renderStage(); }
  }
  // the cells' type sized to the cells (and a page's rows to the room)
  function fit() {
    if (E.mode === "tabs") {
      const k = E.page, rows = E.lay === "phone" ? PHONE_ROWS[k] : SEC_BOX[k][3];
      let prow = 54;
      if (E.lay !== "phone" && bound() && stage.clientHeight > 100) {
        const cs = getComputedStyle(stage), gap = parseFloat(cs.getPropertyValue("--gap")) || 10, room = stage.clientHeight - parseFloat(cs.paddingTop) - parseFloat(cs.paddingBottom) - 6;
        prow = Math.max(46, Math.min(104, Math.floor((room - gap * (rows - 1)) / rows)));
      }
      stage.style.setProperty("--prow", `${prow}px`);
    }
    const t = [...stage.querySelectorAll(".t:not(.algoT):not(.op):not(.dim0)")].find((e) => e.offsetWidth > 0);
    if (t) {
      const r = t.getBoundingClientRect(), max = E.mode === "panel" ? 21 : 26;
      stage.style.setProperty("--ch", `${Math.round(r.height)}px`);
      stage.style.setProperty("--vf", `${Math.max(12, Math.min(max, Math.round(r.height * 0.36), Math.round(r.width * 0.235)))}px`);
      stage.classList.toggle("narrow", r.width < 57);
    }
    const al = [...stage.querySelectorAll(".algoT")].find((e) => e.offsetWidth > 0);
    if (al) stage.style.setProperty("--algof", `${Math.round(Math.min(al.clientHeight * 0.66, al.clientWidth * 0.62))}px`);
  }
  function renderPages() {
    if (pagesEl.hidden) { if (pagesEl.innerHTML) { pagesEl.innerHTML = ""; pagesEl.dataset.h = ""; } renderSrcs(); return; }
    if (!SECTIONS.some(([k]) => k === E.page)) E.page = "ops";
    const s = curSrc(), reach = s ? SECTIONS.filter(([k]) => [...stage.querySelectorAll(`[data-sec="${k}"] [data-dest]`)].some((e) => litOK(s, +e.dataset.dest))).map(([k]) => k) : [];
    const html = SECTIONS.map(([k, nm, c]) => `<button class="tube${k === E.page ? " on" : ""}" role="tab" data-page="${k}" aria-selected="${k === E.page}" ` +
      `tabindex="${k === E.page ? 0 : -1}" style="--c:var(${c})${s ? `;--s:var(${srcColour(s)})` : ""}"><i class="pdot" aria-hidden="true"></i>${nm}${reach.includes(k) ? '<i class="reach" aria-hidden="true"></i>' : ""}</button>`).join("");
    if (pagesEl.dataset.h !== html) {
      pagesEl.innerHTML = html;
      pagesEl.dataset.h = html;
      const on = pagesEl.querySelector(".on");
      if (on && on.scrollIntoView && !E.md) on.scrollIntoView({ block: "nearest", inline: "nearest" });
    }
    stage.querySelectorAll(".pn[data-sec]").forEach((e) => e.classList.toggle("cur", e.dataset.sec === E.page));
    renderSrcs();
  }
  // design 13 on pages: the sources' strip under the tabs (LFO 1..3, then the MOD sources); × disarms
  function renderSrcs() {
    if (!srcsEl) return;
    if (srcsEl.hidden) { if (srcsEl.firstChild) srcsEl.innerHTML = ""; return; }
    if (!srcsEl.firstChild) {
      const chip = (s) => `<button class="src${s.kind === "lfo" ? " lfo" : ""}" ${s.kind === "lfo" ? `data-lfo="${s.k}"` : `data-src="${s.src}"`} style="--c:var(${srcColour(s)})" aria-pressed="false"><span class="grip"></span>${srcName(s)}</button>`;
      srcsEl.innerHTML = `<button class="disarm" id="ed-disarm" aria-label="Done" hidden>${I_X}</button>${[0, 1, 2].map((k) => chip({ kind: "lfo", k })).join("")}<span class="sep"></span>` +
        `${[1, 2, 3, 4, 5, 6, 7, 8].map((i) => chip({ kind: "mod", src: i })).join("")}`;
    }
    const x = srcsEl.querySelector("#ed-disarm");
    x.hidden = !E.arm;
    if (E.arm) x.style.setProperty("--c", `var(${srcColour(E.arm)})`);
    const full = E.s && isFull(E.s.values);
    srcsEl.querySelectorAll(".src").forEach((e) => {
      const s = srcOf(e), a = sameSrc(E.arm, s);
      e.classList.toggle("isarmed", a);
      e.classList.toggle("from", !!(E.md && E.md.moved && sameSrc(E.md.src, s)));
      e.classList.toggle("full", a && s.kind === "mod" && full);
      e.setAttribute("aria-pressed", String(a));
    });
  }
  function page(k, focus = false) {
    E.page = k;
    try { localStorage.setItem(PAGE_KEY, k); } catch (_) { /* (this page only) */ }
    renderPages();
    fit();
    if (measure()) renderStage();                    // (its drawings, drawn in their boxes now they show)
    if (focus) { const b = pagesEl.querySelector(".on"); if (b) b.focus(); }
  }
  pagesEl.addEventListener("click", (ev) => { const b = ev.target.closest("[data-page]"); if (b) page(b.dataset.page); });
  pagesEl.addEventListener("keydown", (ev) => {
    const d = { ArrowLeft: -1, ArrowRight: 1 }[ev.key];
    if (!d) return;
    ev.preventDefault();
    const i = SECTIONS.findIndex(([k]) => k === E.page), n = SECTIONS.length;
    page(SECTIONS[(i + d + n) % n][0], true);
  });
  const refit = () => { if (!E.s || E.s.midi) return; fit(); if (measure()) renderStage(); };
  if (typeof ResizeObserver === "function") new ResizeObserver(refit).observe(stage);
  let rz = 0;
  window.addEventListener("resize", () => { if (!rz) rz = requestAnimationFrame(() => { rz = 0; decide(); }); });
  function renderStage() {
    if (!E.s) return;
    const mode = E.s.midi ? "midi" : "fm";
    if (stage.dataset.mode !== mode) { stage.dataset.mode = mode; stage.classList.toggle("midi", mode === "midi"); stage.innerHTML = ""; drawn.clear(); }
    if (E.s.midi) { stage.classList.remove("panel", "paged", "phone", "modon", "full"); pagesEl.hidden = true; if (srcsEl) srcsEl.hidden = true; renderSrcs(); disarm(false); closeCard(); renderMidi(); return; }
    if (!stage.querySelector(".pn")) {
      stage.innerHTML = SECTIONS.map(([k, nm, c]) => `<section class="pn pn-${k}" data-sec="${k}" style="--c:var(${c})" aria-label="${nm}"></section>`).join("");
      drawn.clear();
      setMode(E.mode || (wantPanel() ? "panel" : "tabs"));
      if (!decided) { decided = true; requestAnimationFrame(() => { decided = false; decide(); }); }
    }
    E.rt = routes(E.s.values);
    const keep = document.activeElement && stage.contains(document.activeElement) ? focusKey(document.activeElement) : null;
    let any = false;
    for (const [k] of SECTIONS) {
      const html = blockHtml(k);
      if (drawn.get(k) === html) continue;
      drawn.set(k, html);
      stage.querySelector(`[data-sec="${k}"]`).innerHTML = html;
      any = true;
    }
    renderCard();
    if (!any) return;
    fit();
    if (measure()) { renderStage(); return; }       // (a drawing's box changed: drawn again in it)
    if (E.drag) { const el = findKey(E.drag.key); if (el) dragMark(el); }
    if (keep) { const el = findKey(keep); if (el && el.focus && el !== document.activeElement) el.focus({ preventScroll: true }); }
    if (E.picker) openPicker(E.picker.id, true);
    if (E.arm || E.md) markLit(); else if (!pagesEl.hidden) renderPages();
  }
  // an element's identity across renders: its ids, its kind
  const focusKey = (el) => [el.tagName, el.dataset.x || "", el.dataset.y || "", el.dataset.id || "", el.dataset.op || "", el.dataset.nav || "", el.dataset.src || "", el.dataset.lfo || ""].join("|");
  const findKey = (key) => [...stage.querySelectorAll("[data-id],[data-nav],[data-src],[data-lfo]")].find((e) => focusKey(e) === key);
  const dragMark = (el) => { el.classList.add("dragging"); };
  function renderKeys() {
    const lo = 53 + 12 * E.oct, ph = (n) => E.held.has(n);
    const wk = WHITE.map((o, i) => `<button class="wk ${ph(lo + o) ? "down" : ""}" data-n="${lo + o}" style="left:${(i * 100 / 16).toFixed(3)}%;width:${(100 / 16).toFixed(3)}%" aria-label="${noteName(lo + o)}"></button>`).join("");
    const bk = BLACK.map(([a, o]) => `<button class="bk2 ${ph(lo + o) ? "down" : ""}" data-n="${lo + o}" style="left:${((a + 1) * 100 / 16 - 100 / 16 * 0.31).toFixed(3)}%;width:${(100 / 16 * 0.62).toFixed(3)}%" aria-label="${noteName(lo + o)}"></button>`).join("");
    // (FIZZ NEON: OCT − / + beside the keys, the keys' range between them)
    kb.innerHTML = `<div class="octs"><button class="oct ol" data-oct="-1" aria-label="Octave down">OCT −</button><span class="octv hud">${noteName(lo)}–${noteName(lo + 26)}</span>` +
      `<button class="oct ol" data-oct="1" aria-label="Octave up">OCT +</button></div><div class="bed ${E.rec ? "rec" : ""}">${wk}${bk}</div>`;
  }
  const NOTE = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"];
  const noteName = (n) => `${NOTE[n % 12]}${Math.floor(n / 12) - 1}`;
  function render() { renderHead(); renderStage(); renderKeys(); decide(); }

  // ---------------------------------------------------------------- design 13: drag to modulate
  // a source from its element: an LFO's plug (or its chip on the strip), a MOD source
  const srcOf = (el) => (el.dataset.lfo !== undefined ? { kind: "lfo", k: +el.dataset.lfo } : { kind: "mod", src: +el.dataset.src });
  const curSrc = () => (E.md && E.md.moved ? E.md.src : E.arm);
  // a value lit for a source: a drop there routes it (or opens the route it has); every slot used: none
  const litOK = (s, d) => { const p = plan(E.s.values, s, d); return p.act === "set" || p.act === "card"; };
  function markLit() {
    const s = curSrc();
    stage.classList.toggle("modon", !!s);
    if (s) stage.style.setProperty("--s", `var(${srcColour(s)})`); else stage.style.removeProperty("--s");
    stage.classList.toggle("full", !!(s && s.kind === "mod" && isFull(E.s.values)));
    stage.querySelectorAll("[data-dest]").forEach((e) => e.classList.toggle("lit", !!s && litOK(s, +e.dataset.dest)));
    stage.querySelectorAll(".src[data-src], .jack[data-lfo]").forEach((e) => {
      const me = srcOf(e);
      e.classList.toggle("from", !!s && sameSrc(me, s));
      e.classList.toggle("isarmed", sameSrc(E.arm, me));
    });
    if (!s) stage.querySelectorAll(".hot").forEach((e) => e.classList.remove("hot"));
    renderPages();
  }
  function arm(s) { E.arm = s; closeCard(); markLit(); }
  function disarm(redraw = true) { if (!E.arm) return; E.arm = null; if (redraw) markLit(); }
  const toggleArm = (s) => (sameSrc(E.arm, s) ? disarm() : arm(s));
  // a drop (or a tap on a lit value): the route made (live: PARAMs), its card, or FULL
  function apply(s, d) {
    const p = plan(E.s.values, s, d);
    if (p.act === "set") for (const [id, x] of p.sets) set(id, x);
    else if (p.act === "card") { renderStage(); openCard(p.route); }
    else if (p.act === "full") flashFull();
    return p;
  }
  function flashFull() {
    stage.classList.add("full", "fullflash");
    clearTimeout(E.fullT);
    E.fullT = setTimeout(() => { stage.classList.remove("fullflash"); if (!curSrc()) stage.classList.remove("full"); }, 900);
  }
  // the drag's cable from the source to the pointer, its plug, its label
  let ov = null;
  function drawCable(x1, y1) {
    const d = E.md;
    if (!ov) { ov = document.createElement("div"); ov.className = "dragov"; ov.setAttribute("aria-hidden", "true"); document.body.appendChild(ov); }
    const [x0, y0] = d.c0, full = d.src.kind === "mod" && isFull(E.s.values) && !d.hot, mx = (x0 + x1) / 2, sag = Math.min(160, Math.abs(x1 - x0) * 0.25 + 60);
    ov.style.setProperty("--s", `var(${srcColour(d.src)})`);
    const lab = `${esc(srcName(d.src))}${d.hot ? ` <svg viewBox="0 0 12 12"><path d="M2 6h8M7 3l3 3-3 3"/></svg><b>${esc(DEST_NAMES[+d.hot.dataset.dest])}</b>` : full ? ' <span class="fullb">FULL</span>' : ""}`;
    ov.innerHTML = `<svg><path class="cable${full ? " no" : ""}" d="M${x0} ${y0} C${x0} ${y0 + sag * 0.6} ${mx} ${Math.max(y0, y1) + sag} ${x1} ${y1}"/><circle class="plug${full ? " no" : ""}" cx="${x1}" cy="${y1}" r="6"/></svg>` +
      `<span class="ghost${full ? " no" : ""}" style="left:${x1 + 16}px;top:${y1 + 12}px">${lab}</span>`;
  }
  const endCable = () => { if (ov) { ov.remove(); ov = null; } };
  // the value under the pointer, when it is lit
  const hotAt = (x, y) => { const e = document.elementFromPoint(x, y), t = e && e.closest("[data-dest]"); return t && stage.contains(t) && t.classList.contains("lit") ? t : null; };
  function srcDown(ev, el) {
    if (ev.pointerType === "mouse" && ev.button !== 0) return;
    ev.preventDefault();
    const r = el.getBoundingClientRect();
    E.md = { src: srcOf(el), pid: ev.pointerId, x0: ev.clientX, y0: ev.clientY, c0: [r.left + r.width / 2, r.top + r.height / 2], moved: false, hot: null };
    closeCard();
  }
  window.addEventListener("pointermove", (ev) => {
    const d = E.md;
    if (!d || ev.pointerId !== d.pid) return;
    if (!d.moved) { if (Math.hypot(ev.clientX - d.x0, ev.clientY - d.y0) < 5) return; d.moved = true; E.arm = null; markLit(); }
    const h = hotAt(ev.clientX, ev.clientY);
    if (h !== d.hot) { if (d.hot) d.hot.classList.remove("hot"); d.hot = h; if (h) h.classList.add("hot"); }
    drawCable(ev.clientX, ev.clientY);
  });
  window.addEventListener("pointerup", (ev) => {
    const d = E.md;
    if (!d || ev.pointerId !== d.pid) return;
    E.md = null;
    endCable();
    if (!d.moved) { toggleArm(d.src); return; }
    const h = d.hot;
    if (h) h.classList.remove("hot");
    markLit();
    if (h) apply(d.src, +h.dataset.dest);
    else if (d.src.kind === "mod" && isFull(E.s.values)) flashFull();
  });
  window.addEventListener("pointercancel", (ev) => { if (E.md && ev.pointerId === E.md.pid) { E.md = null; endCable(); markLit(); } });
  if (srcsEl) {
    srcsEl.addEventListener("pointerdown", (ev) => { const el = ev.target.closest(".src[data-src], .src[data-lfo]"); if (el && E.s) srcDown(ev, el); });
    srcsEl.addEventListener("click", (ev) => { if (ev.target.closest("#ed-disarm")) disarm(); });
    srcsEl.addEventListener("keydown", (ev) => { const el = ev.target.closest(".src"); if (el && (ev.key === "Enter" || ev.key === " ")) { ev.preventDefault(); toggleArm(srcOf(el)); } });
  }
  // armed: a tap anywhere but the stage, the strip and the tabs lets go
  document.addEventListener("pointerdown", (ev) => {
    if (!E.arm || stage.contains(ev.target) || (srcsEl && srcsEl.contains(ev.target)) || pagesEl.contains(ev.target)) return;
    disarm();
  }, true);

  // a route's card: "MOD n · SRC → DEST", its DEPTH dragged (the wheel, the arrows), × takes it off
  function openCard(r) {
    E.card = { kind: r.kind, k: r.k };
    if (E.cardEl) { E.cardEl.remove(); E.cardEl = null; }
    renderCard();
  }
  function closeCard() { E.card = null; if (E.cardEl) { E.cardEl.remove(); E.cardEl = null; } }
  function renderCard() {
    if (!E.card || !E.s || E.s.midi) { if (E.cardEl) closeCard(); return; }
    const r = routes(E.s.values).find((x) => x.kind === E.card.kind && x.k === E.card.k);
    if (!r) { closeCard(); return; }
    const t = routeTitle(r), f = Math.abs(frac(r.depthId, r.depth) - frac(r.depthId, 0)), key = `${r.kind}${r.k}|${r.dest}|${r.c}`;
    if (!E.cardEl || E.cardEl.dataset.key !== key) {
      if (E.cardEl) E.cardEl.remove();
      const p = document.createElement("div");
      p.className = "rtpop";
      p.dataset.key = key;
      p.setAttribute("role", "dialog");
      p.setAttribute("aria-label", `${t.who} ${t.what}`);
      p.innerHTML = `<div class="who"><small>${esc(t.who)}</small><b>${esc(r.name)} <em>→ ${esc(DEST_NAMES[r.dest])}</em></b></div>` +
        `<div class="dep" tabindex="0" role="slider" aria-label="DEPTH"><small>DEPTH</small><b></b></div><button class="rm" aria-label="Take the route off">${I_X}</button>`;
      document.body.appendChild(p);
      E.cardEl = p;
      cardEvents(p);
      const a = [...stage.querySelectorAll(`.mks i[data-route="${r.kind}${r.k}"]`), ...stage.querySelectorAll(`[data-dest="${r.dest}"]`)].find((e) => e.offsetWidth > 0);
      const b = a ? a.getBoundingClientRect() : { left: window.innerWidth / 2, right: window.innerWidth / 2, bottom: window.innerHeight / 2, top: window.innerHeight / 2 };
      const w = p.offsetWidth, ph = p.offsetHeight, left = Math.max(12, Math.min(window.innerWidth - w - 12, b.left - 26));
      const below = b.bottom + 12 + ph < window.innerHeight - 8;
      p.style.left = `${left}px`;
      p.style.top = `${below ? b.bottom + 12 : Math.max(8, b.top - 12 - ph)}px`;
      p.classList.toggle("up", !below);
      p.style.setProperty("--ax", `${Math.max(10, Math.min(w - 22, (b.left + b.right) / 2 - left - 6))}px`);
    }
    E.cardEl.style.setProperty("--m", `var(${r.c})`);
    E.cardEl.style.setProperty("--w", f.toFixed(3));
    const dep = E.cardEl.querySelector(".dep"), tx = text(r.depthId, r.depth);
    dep.dataset.cid = String(r.depthId);
    dep.querySelector("b").textContent = tx;
    dep.setAttribute("aria-valuetext", tx);
  }
  function cardEvents(p) {
    let cd = null;
    p.addEventListener("pointerdown", (ev) => {
      const dep = ev.target.closest(".dep");
      if (!dep) return;
      ev.preventDefault();
      const id = +dep.dataset.cid, r = info(id);
      cd = { id, v0: v(id), x0: ev.clientX, y0: ev.clientY, pid: ev.pointerId, span: r.max - r.min };
      try { dep.setPointerCapture(ev.pointerId); } catch (_) { /* (gone) */ }
      dep.classList.add("dragging");
      dep.focus({ preventScroll: true });
    });
    p.addEventListener("pointermove", (ev) => {
      if (!cd || ev.pointerId !== cd.pid) return;
      const px = Math.max(240, Math.min(cd.span * 12, 600)), delta = (ev.clientX - cd.x0) - (ev.clientY - cd.y0);
      set(cd.id, cd.v0 + (ev.shiftKey && isHundredths(cd.id) ? Math.round(delta) : Math.round(delta / px * cd.span * (ev.shiftKey ? 0.1 : 1))));
    });
    const up = () => { cd = null; const d = p.querySelector(".dep"); if (d) d.classList.remove("dragging"); };
    p.addEventListener("pointerup", up);
    p.addEventListener("pointercancel", up);
    p.addEventListener("wheel", (ev) => {
      const dep = ev.target.closest(".dep");
      if (!dep) return;
      ev.preventDefault();
      const id = +dep.dataset.cid, st = tickStep(id, ev.shiftKey, accel(id));
      set(id, v(id) + (ev.deltaY < 0 ? st : -st));
    }, { passive: false });
    p.addEventListener("keydown", (ev) => {
      const dep = ev.target.closest(".dep");
      if (!dep) return;
      const d = { ArrowUp: 1, ArrowRight: 1, ArrowDown: -1, ArrowLeft: -1 }[ev.key];
      if (!d) return;
      ev.preventDefault();
      const id = +dep.dataset.cid;
      set(id, v(id) + d * tickStep(id, ev.shiftKey, accel(id)));
    });
    p.addEventListener("click", (ev) => {
      if (!ev.target.closest(".rm") || !E.card) return;
      const r = routes(E.s.values).find((x) => x.kind === E.card.kind && x.k === E.card.k);
      closeCard();
      if (r) for (const [id, x] of removeSets(r)) set(id, x);
    });
  }
  document.addEventListener("pointerdown", (ev) => { if (E.cardEl && !E.cardEl.contains(ev.target) && !ev.target.closest(".mks i")) closeCard(); }, true);
  document.addEventListener("keydown", (ev) => {
    if (ev.key !== "Escape") return;
    if (E.md) { E.md = null; endCable(); markLit(); ev.preventDefault(); }
    else if (E.cardEl) { closeCard(); ev.preventDefault(); }
    else if (E.arm) { disarm(); ev.preventDefault(); }
  });

  // ---------------------------------------------------------------- dragging, the wheel, the keys
  const primary = (el) => +(el.dataset.id ?? el.dataset.y ?? el.dataset.x);
  function scaleOf(el) {                               // the drawing's pixels a viewBox unit
    const svg = el.closest("svg");
    if (!svg || !svg.viewBox || !svg.viewBox.baseVal || !svg.viewBox.baseVal.width) return 1;
    return svg.getBoundingClientRect().width / svg.viewBox.baseVal.width;
  }
  // FIZZ MIDI (e): a bar's value where the pointer is (the bar's height its range)
  const barValue = (el, y) => { const r = el.getBoundingClientRect(); return Math.round(Math.max(0, Math.min(1, (r.bottom - y) / Math.max(1, r.height))) * 127); };
  stage.addEventListener("pointerdown", (ev) => {
    const mb = ev.target.closest(".mbar[data-slot]");
    if (mb && E.s && E.s.midi && ev.button === 0) {
      ev.preventDefault();
      const k = +mb.dataset.slot;
      E.drag = { midi: true, slot: k, el: mb, pid: ev.pointerId, moved: true };
      E.lastBar = { slot: k, t: performance.now() };   // (the stage holds the pointer: a double click lands on it)
      try { stage.setPointerCapture(ev.pointerId); } catch (_) { /* (gone) */ }
      mb.classList.add("dragging");
      mb.focus({ preventScroll: true });
      setSlot(k, { value: barValue(mb, ev.clientY), touched: true });
      return;
    }
    if (!E.s || E.s.midi) return;
    // design 13: a route's dot opens its card; a source (a MOD source, an LFO's plug) is dragged or tapped (armed)
    const dot = ev.target.closest(".mks i[data-route]");
    if (dot) {
      ev.preventDefault();
      const m = /^(lfo|mod)(\d)$/.exec(dot.dataset.route), r = m && routes(E.s.values).find((x) => x.kind === m[1] && x.k === +m[2]);
      if (r) { disarm(); openCard(r); }
      return;
    }
    const sr = ev.target.closest(".src[data-src], .jack[data-lfo]");
    if (sr) { srcDown(ev, sr); return; }
    if (E.arm) {                                       // (armed: a lit value routes the source there; elsewhere lets go)
      const t = ev.target.closest("[data-dest]");
      ev.preventDefault();
      if (t && t.classList.contains("lit")) { const s = E.arm; disarm(false); apply(s, +t.dataset.dest); markLit(); }
      else disarm();
      E.swallow = performance.now();
      return;
    }
    const el = ev.target.closest("[data-x],[data-y]");
    if (!el || !E.s || ev.button !== 0) return;
    ev.preventDefault();
    const sc = scaleOf(el);
    const ax = (k) => { const id = el.dataset[k]; if (id === undefined) return null; const r = info(+id); return { id: +id, v0: v(+id), span: r.max - r.min, px: (+(el.dataset[`${k}s`] || 0) || 0) * sc }; };
    E.drag = { el, key: focusKey(el), x0: ev.clientX, y0: ev.clientY, x: ax("x"), y: ax("y"), moved: false, pid: ev.pointerId, t0: performance.now() };
    try { stage.setPointerCapture(ev.pointerId); } catch (_) { /* (gone) */ }
    dragMark(el);
    if (el.dataset.op !== undefined) { E.op = +el.dataset.op; }
    if (el.focus) el.focus({ preventScroll: true });
  });
  stage.addEventListener("pointermove", (ev) => {
    const d = E.drag;
    if (!d || ev.pointerId !== d.pid) return;
    if (d.midi) { setSlot(d.slot, { value: barValue(stage.querySelector(`.mbar[data-slot="${d.slot}"]`) || d.el, ev.clientY), touched: true }); return; }
    const dx = ev.clientX - d.x0, dy = ev.clientY - d.y0;
    if (!d.moved && Math.hypot(dx, dy) < 3) return;
    d.moved = true;
    const fine = ev.shiftKey ? 0.1 : 1;
    const move = (a, delta) => {
      if (!a) return;
      // a drawing's handle follows the pointer (its pixels for the range); a chip: a range a 240 px drag, at least 12 px a step
      const px = a.px || Math.max(240, Math.min(a.span * 12, 600));
      set(a.id, a.v0 + (ev.shiftKey && isHundredths(a.id) ? Math.round(delta) : Math.round(delta / px * a.span * fine)));
    };
    move(d.x, dx);
    move(d.y, -dy);
  });
  const endDrag = (ev) => {
    const d = E.drag;
    if (!d || (ev && ev.pointerId !== d.pid)) return;
    E.drag = null;
    stage.querySelectorAll(".dragging").forEach((e) => e.classList.remove("dragging"));
    if (d.midi) { renderStage(); return; }
    if (!d.moved) click(d.el, ev);
    renderStage();
  };
  stage.addEventListener("pointerup", endDrag);
  stage.addEventListener("pointercancel", endDrag);
  function click(el, ev) {
    if (el.dataset.pick !== undefined) { openPicker(+el.dataset.pick); return; }
    if (el.dataset.op !== undefined) { E.op = +el.dataset.op; renderStage(); }
    if (el.classList.contains("algo")) set(Q.ALGO, v(Q.ALGO) % 8 + 1);   // (a click on ALGO: the next one)
    void ev;
  }
  stage.addEventListener("click", (ev) => {
    if (E.s && E.s.midi) {                             // (FIZZ MIDI (e): the CC number typed, a slot untouched)
      const cc = ev.target.closest("[data-cc]"), ux = ev.target.closest("[data-untouch]");
      if (cc) { E.ccEdit = +cc.dataset.cc; renderStage(); }
      else if (ux) untouch(+ux.dataset.untouch);
      return;
    }
    if (E.swallow && performance.now() - E.swallow < 700) { E.swallow = 0; return; }   // (the tap that routed or let go)
    const b = ev.target.closest("[data-set]");
    if (!b) return;
    const [id, x] = b.dataset.set.split(":").map(Number);
    set(id, x);
    renderStage();
  });
  stage.addEventListener("dblclick", (ev) => {
    const mb = ev.target.closest(".mbar[data-slot]"), lb = E.lastBar && performance.now() - E.lastBar.t < 800 ? E.lastBar.slot : -1;
    if (E.s && E.s.midi) { if (mb || lb >= 0) untouch(mb ? +mb.dataset.slot : lb); return; }
    const el = ev.target.closest("[data-x],[data-y]");
    if (!el) return;
    for (const k of ["x", "y"]) if (el.dataset[k] !== undefined) set(+el.dataset[k], info(+el.dataset[k]).def);
  });
  // acceleration: ticks / key repeats on one value within 120 ms of each other count up (tickStep grows the step)
  const acc = { id: -1, t: 0, n: 0 };
  const accel = (id) => { const t = performance.now(); acc.n = id === acc.id && t - acc.t < 120 ? acc.n + 1 : 0; acc.id = id; acc.t = t; return acc.n; };
  stage.addEventListener("wheel", (ev) => {
    if (E.s && E.s.midi) {                             // (FIZZ MIDI (e): a bar's value, a CC number)
      const mb = ev.target.closest(".mbar[data-slot]"), cc = ev.target.closest("[data-cc]");
      if (!mb && !cc) return;
      ev.preventDefault();
      const k = +(mb || cc).dataset[mb ? "slot" : "cc"], d = (ev.deltaY < 0 ? 1 : -1) * (ev.shiftKey ? 10 : Math.min(8, 1 + accel(`m${k}${mb ? "" : "c"}`)));
      if (mb) stepSlot(k, d); else setSlot(k, { cc: E.s.midi.slots[k].cc + Math.sign(d) });
      return;
    }
    const el = ev.target.closest("[data-id]");
    if (!el || !E.s) return;
    ev.preventDefault();
    const id = primary(el), st = tickStep(id, ev.shiftKey, accel(id)) * (isDest(id) ? 0 : 1);
    if (isDest(id)) set(id, destStep(v(id), ev.deltaY < 0 ? 1 : -1));
    else set(id, v(id) + (ev.deltaY < 0 ? st : -st));
  }, { passive: false });
  stage.addEventListener("keydown", (ev) => {
    if (E.s && E.s.midi) {
      const ci = ev.target.closest("[data-ccin]");
      if (ci && (ev.key === "Enter" || ev.key === "Escape")) { ev.preventDefault(); if (ev.key === "Escape") ci.dataset.cancel = "1"; ci.blur(); return; }
      const mb = ev.target.closest(".mbar[data-slot]");
      if (!mb) return;
      const k = +mb.dataset.slot, up = ev.key === "ArrowUp" || ev.key === "ArrowRight", dn = ev.key === "ArrowDown" || ev.key === "ArrowLeft";
      if (ev.key === "Delete" || ev.key === "Backspace") { ev.preventDefault(); untouch(k); return; }
      if (!up && !dn) return;
      ev.preventDefault();
      stepSlot(k, (up ? 1 : -1) * (ev.shiftKey ? 10 : 1));
      return;
    }
    if (!E.s) return;
    const enter = ev.key === "Enter" || ev.key === " ";
    const sr = ev.target.closest(".src[data-src], .jack[data-lfo]");
    if (sr && enter) { ev.preventDefault(); toggleArm(srcOf(sr)); return; }
    const lt = ev.target.closest("[data-dest].lit");
    if (lt && E.arm && enter) { ev.preventDefault(); const s = E.arm; disarm(false); apply(s, +lt.dataset.dest); markLit(); return; }
    const sb = ev.target.closest("div[data-set]");
    if (sb && enter) { ev.preventDefault(); const [i, x] = sb.dataset.set.split(":").map(Number); set(i, x); renderStage(); return; }
    const el = ev.target.closest("[data-id]");
    if (!el) return;
    const up = ev.key === "ArrowUp" || ev.key === "ArrowRight", dn = ev.key === "ArrowDown" || ev.key === "ArrowLeft";
    if (!up && !dn) { if (ev.key === "Enter" && el.dataset.pick !== undefined) openPicker(+el.dataset.pick); return; }
    ev.preventDefault();
    const id = primary(el);
    if (isDest(id)) set(id, destStep(v(id), up ? 1 : -1));
    else set(id, v(id) + (up ? 1 : -1) * tickStep(id, ev.shiftKey, accel(id)));
  });
  // (FIZZ MIDI (e)) the CC number typed: digits only, taken when it loses the focus (ENTER), ESC leaves it
  stage.addEventListener("input", (ev) => { const ci = ev.target.closest("[data-ccin]"); if (ci) ci.value = ci.value.replace(/\D/g, "").slice(0, 3); });
  stage.addEventListener("focusout", (ev) => {
    const ci = ev.target.closest("[data-ccin]");
    if (!ci || E.ccEdit === undefined) return;
    const k = +ci.dataset.ccin, n = parseInt(ci.value, 10);
    E.ccEdit = undefined;
    if (!ci.dataset.cancel && Number.isFinite(n)) setSlot(k, { cc: n });
    setTimeout(() => { renderStage(); const b = stage.querySelector(`[data-cc="${k}"]`); if (b) b.focus({ preventScroll: true }); }, 0);
  });
  // a destination steps in the editor's order (the device's)
  const destStep = (x, d) => { const o = DEST_ORDER, i = o.indexOf(x); return o[Math.max(0, Math.min(o.length - 1, (i < 0 ? 0 : i) + d))]; };

  // the picker: a long list's every entry as a chip
  function openPicker(id, again = false) {
    closePicker(again);
    const r = info(id), el = stage.querySelector(`[data-pick="${id}"]`);
    if (!el) return;
    const order = isDest(id) ? DEST_ORDER : Array.from({ length: r.max - r.min + 1 }, (_, k) => r.min + k);
    const p = document.createElement("div");
    p.className = "picker";
    p.setAttribute("role", "listbox");
    p.setAttribute("aria-label", r.name);
    p.innerHTML = order.map((x) => `<button role="option" data-v="${x}" aria-selected="${v(id) === x}" class="${v(id) === x ? "on" : ""}">${esc(text(id, x))}</button>`).join("");
    document.body.appendChild(p);
    const b = el.getBoundingClientRect(), pw = Math.min(520, window.innerWidth - 24);
    p.style.width = `${pw}px`;
    p.style.left = `${Math.max(12, Math.min(window.innerWidth - pw - 12, b.left))}px`;
    p.style.top = `${Math.min(window.innerHeight - 40, b.bottom + 8) + window.scrollY}px`;
    E.picker = { id, el: p };
    p.addEventListener("click", (ev) => {
      const o = ev.target.closest("[data-v]");
      if (!o) return;
      set(id, +o.dataset.v);
      closePicker();
      renderStage();
    });
    if (!again) { const on = p.querySelector(".on") || p.firstElementChild; if (on) on.focus(); }
  }
  function closePicker(keep = false) {
    if (E.picker) { E.picker.el.remove(); if (!keep) E.picker = null; }
  }
  document.addEventListener("pointerdown", (ev) => { if (E.picker && !E.picker.el.contains(ev.target) && !ev.target.closest("[data-pick]")) closePicker(); }, true);
  document.addEventListener("keydown", (ev) => { if (ev.key === "Escape" && E.picker) { closePicker(); ev.preventDefault(); } });

  // ---------------------------------------------------------------- the header's actions
  head.addEventListener("click", async (ev) => {
    const b = ev.target.closest("button");
    if (!b || !E.s) return;
    if (b.dataset.trk !== undefined) { if (ctx.stripClick) await ctx.stripClick(b, E); renderHead(); return; }
    if (b.id === "ed-kbtg") { showKeys(!E.kbOn); return; }
    if (b.id === "ed-tag") { if (E.tagPop) closeTags(true); else openTags(); return; }
    if (b.id === "ed-revert") { revert(); renderHead(); return; }
    if (b.id === "ed-target") { if (E.target && E.target.click) await E.target.click(E, b); else if (E.target && !E.target.ready()) await startTarget(); renderHead(); return; }
    if (b.id === "ed-export" && ctx.exportSound) { ctx.exportSound(E.s); return; }
    if ((b.id === "ed-save" || b.id === "ed-savenew") && ctx.save) {
      const ok = await ctx.save(E, b.id === "ed-savenew");
      if (ok) { E.base = copySound(E.s); if (ok.place) E.place = ok.place; }
      renderHead();
    }
  });
  head.addEventListener("input", (ev) => {
    if (ev.target.id !== "ed-name") return;
    const nm = ev.target, p = nm.selectionStart;
    nm.value = nm.value.toUpperCase().replace(/[^\x20-\x7E]/g, "");
    nm.setSelectionRange(p, p);
    E.s.name = nm.value.trim().slice(0, 12) || "SOUND";
    renderHead();
  });
  head.addEventListener("keydown", (ev) => { if (ev.target.id === "ed-name" && (ev.key === "Enter" || ev.key === "Escape")) ev.target.blur(); });
  head.addEventListener("focusout", (ev) => { if (ev.target.id === "ed-name") setTimeout(renderHead, 0); });

  function revert() {
    if (!E.base) return;
    E.s = copySound(E.base);
    if (E.target) E.target.revert ? E.target.revert(E.s, E) : E.target.audition(E.s);
    render();
  }

  // ---------------------------------------------------------------- the keybed (the FM-1's: 27 keys from F)
  // (the owner, 2026-10-27) the computer keys play on LIBRARY, POOL and IMPORT too: there ctx.keys says what plays (the
  // FM-1's track with TRACK on, else the browser's FIZZ with the sound picked); on EDIT the editor's own target
  const elsewhere = () => $("tab-edit").hidden && !!(ctx.keys && ctx.keys.active());
  const tgt = () => (elsewhere() ? ctx.keys.target() : E.target);
  function noteOn(n) {
    if (E.held.has(n)) return;
    const T = tgt();
    E.held.set(n, T);
    if (T && T.ready()) T.note(n, 100);
    else if (T) (elsewhere() ? ctx.keys.start() : startTarget()).then(() => { if (E.held.get(n) === T && T.ready()) T.note(n, 100); });
    renderKeys();
    if (ctx.keys && ctx.keys.held) ctx.keys.held(E.held.size);
  }
  function noteOff(n) {
    if (!E.held.has(n)) return;
    const T = E.held.get(n);
    E.held.delete(n);
    if (T && T.ready()) T.note(n, 0);
    renderKeys();
    if (ctx.keys && ctx.keys.held) ctx.keys.held(E.held.size);
  }
  const kbDown = new Map();
  kb.addEventListener("pointerdown", (ev) => {
    const o = ev.target.closest("[data-oct]");
    if (o) { E.oct = Math.max(-2, Math.min(2, E.oct + +o.dataset.oct)); renderKeys(); return; }
    const k = ev.target.closest("[data-n]");
    if (!k) return;
    ev.preventDefault();
    try { kb.setPointerCapture(ev.pointerId); } catch (_) { /* (gone) */ }
    kbDown.set(ev.pointerId, +k.dataset.n);
    noteOn(+k.dataset.n);
  });
  const kbUp = (ev) => { const n = kbDown.get(ev.pointerId); if (n === undefined) return; kbDown.delete(ev.pointerId); noteOff(n); };
  kb.addEventListener("pointerup", kbUp);
  kb.addEventListener("pointercancel", kbUp);
  const pcNote = (key) => {
    const w = PC_WHITE.indexOf(key);
    if (w >= 0) return 53 + 12 * E.oct + WHITE[w];
    if (PC_BLACK[key] !== undefined) return 53 + 12 * E.oct + PC_BLACK[key];
    return -1;
  };
  const typing = (ev) => ev.target && (ev.target.tagName === "INPUT" || ev.target.tagName === "TEXTAREA" || ev.target.tagName === "SELECT" || ev.target.isContentEditable);
  const pcDown = new Map();
  document.addEventListener("keydown", (ev) => {
    if (typing(ev) || ev.metaKey || ev.ctrlKey || ev.altKey || ev.repeat) return;
    if (!(elsewhere() || (!$("tab-edit").hidden && E.s))) return;
    const k = ev.key.toLowerCase();
    if (k === "z" || k === "x") { E.oct = Math.max(-2, Math.min(2, E.oct + (k === "z" ? -1 : 1))); renderKeys(); if (ctx.keys && ctx.keys.oct) ctx.keys.oct(E.oct); return; }
    const n = pcNote(k);
    if (n < 0) return;
    ev.preventDefault();
    pcDown.set(k, n);
    noteOn(n);
  });
  document.addEventListener("keyup", (ev) => {
    const k = ev.key.toLowerCase(), n = pcDown.get(k);
    if (n === undefined) return;
    pcDown.delete(k);
    noteOff(n);
  });
  window.addEventListener("blur", () => { for (const n of [...E.held.keys()]) noteOff(n); pcDown.clear(); });

  // ---------------------------------------------------------------- the target
  async function startTarget() {
    if (!E.target) return false;
    const ok = await E.target.start();
    if (ok && E.s) await E.target.audition(E.s);
    renderHead();
    return ok;
  }
  // (the browser's FIZZ takes the editor's sound at once; a track keeps its own until the editor takes it)
  function setTarget(t, push = !(t && t.live)) {
    for (const n of [...E.held.keys()]) noteOff(n);
    E.target = t;
    if (push && t && t.ready() && E.s) t.audition(E.s);
    renderHead();
  }

  // ---------------------------------------------------------------- open
  function open(sound, place = {}) {
    E.s = copySound(sound);
    E.base = copySound(sound);
    E.ccEdit = undefined;
    E.place = { label: place.label || "", ...place };
    closePicker();
    if (E.tagPop) { E.tagPop.remove(); E.tagPop = null; }
    if (E.target && !E.target.live && E.target.ready()) E.target.audition(E.s);   // (a track: live.js loads it, then the
                                                    // editor takes the track)
    log(`edit: ${E.s.name} (${E.place.label})`);
    render();
  }
  // the sound read back from the target (the unit's knobs): the values taken, not sent back
  // rebase: the read is the sound to go back to too (an FM-1 track only shown: no edits of the page's own)
  function follow(sound, { rebase = false } = {}) {
    if (!E.s || E.drag) return false;
    let n = 0;
    if (!sound.midi !== !E.s.midi) {                   // (FIZZ MIDI (e): ENGINE changed on the unit: the other kind)
      E.s = copySound({ ...sound, tags: E.s.tags });
      if (rebase || !E.base || !E.base.midi !== !sound.midi) E.base = copySound(E.s);
      E.ccEdit = undefined;
      later();
      return true;
    }
    if (sound.midi && E.ccEdit === undefined) sound.midi.slots.forEach((x, k) => {
      const o = E.s.midi.slots[k];
      if (o.cc !== x.cc || o.value !== x.value || o.touched !== x.touched) { E.s.midi.slots[k] = { ...x }; n++; }
    });
    sound.values.forEach((x, i) => { if (E.s.values[i] !== x) { E.s.values[i] = x; n++; } });
    if (sound.sends) Object.values(MIX_IDS).forEach((id) => { const x = mixValue(sound, id); if (x !== undefined && x !== v(id)) { setMix(E.s, id, x); n++; } });
    if (sound.name !== E.s.name && document.activeElement !== head.querySelector("#ed-name")) { E.s.name = sound.name; n++; }
    if (n && rebase) E.base = copySound({ ...E.s, tags: E.base ? E.base.tags : E.s.tags });
    if (n) later();
    return n > 0;
  }
  const blank = () => newSound("fm");
  return {
    open, render, renderHead, setTarget, follow, blank, startTarget,
    setPlace(p) { E.place = { label: p.label || "", ...p }; renderHead(); },
    target: () => E.target,
    show() { if (!E.s) open(blank(), { label: "NEW SOUND" }); else render(); },
    sound: () => E.s, base: () => E.base, place: () => E.place, state: E,
    setRec(on) { if (E.rec !== !!on) { E.rec = !!on; renderKeys(); } },
    set, value: v, tagNames, octave: () => E.oct,
    setSlot, slot: (k) => (E.s && E.s.midi ? { ...E.s.midi.slots[k] } : null),
  };
}
