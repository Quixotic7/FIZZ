// SPDX-License-Identifier: GPL-3.0-only
// The manager's EDIT tab: the sound editor (docs/MANAGER-P2-PLAN.md §1). The owner's fix (2026-10-25): every value on
// one page, a synth's whole front panel: OPERATORS, OP ENV, FILTER, AMP, DRIVE · CRUSH, LFO, MOD, PLAY side by side
// (a column on a phone), each value a part of a drawing that is dragged, in its knob's colour. Every one of FIZZ's
// 110 values is on the panel once (sound.js's table, the core's names and texts); a section is drawn again only when
// its drawing changed. Drag (SHIFT: fine; the two-decimal values 0.01 a pixel), the wheel or the arrow keys over a
// value (the two-decimal values 0.01 a tick, quick ticks speed up to 1.00, SHIFT 1.00); a double click its default; a long list (a destination, a source) a
// picker. The keybed under it plays the sound (the FM-1's 27 keys from F, the computer's A..' row, Z / X the octave).
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
//   mountEditor(ctx) -> { open(sound, place), show(), sound(), setTarget(t), render() }
import { VALUES, ALGOS, DEST_ORDER, Q, lfo, mod, LF, info, text, clampV, stepOf, tickStep, isHundredths, isHi, frac, defaults, copySound, sameValues, tagNames } from "./sound.js";
import { FIZZ_TAGS } from "../js/fizz_sysex.js";
import { esc, MIDI_I } from "./ui.js";

const K = { c1: "--k1", c2: "--k2", c3: "--k3", c4: "--k4", alg: "--kalg" };
const OPS = [["C", Q.RC, Q.OFSC, K.c1], ["A", Q.RA, Q.OFSA, K.c2], ["B1", Q.RB1, Q.OFSB1, K.c3], ["B2", Q.RB2, Q.OFSB2, K.c4]];
// (FIZZ NEON: each section's name after a ring in its colour, as the boards' EDIT pages)
export const SECTIONS = [
  ["ops", "OPERATORS", K.c1], ["openv", "OP ENV", K.c2], ["play", "PLAY", "--yellow"], ["filter", "FILTER", K.c4],
  ["amp", "AMP", K.c3], ["drive", "DRIVE · CRUSH", "--orange"], ["lfo", "LFO", "--cyan"], ["mod", "MOD", K.alg],
];
const LFOC = ["--cyan", "--yellow", "--violet"];      // (core/ui ui_kit.c UI_LFO_COL)
const MACH = [["LEGACY", [0, 1, 2]], ["LP4", [3]], ["MULTI", [4]], ["COMB+", [5]], ["COMB−", [6]], ["EQ", [7]]];
const isDest = (id) => (id >= 47 && id <= 70 && (id - 47) % 8 === LF.DEST) || (id >= 86 && (id - 86) % 3 === 1);
// a white key's and a black key's note from F (the FM-1's keybed: F3 .. G5 at OCT 0)
const WHITE = [0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26];
const BLACK = [[0, 1], [1, 3], [2, 5], [4, 8], [5, 10], [7, 13], [8, 15], [9, 17], [11, 20], [12, 22], [14, 25]];   // [after white, note]
const PC_WHITE = ["a", "s", "d", "f", "g", "h", "j", "k", "l", ";", "'"], PC_BLACK = { w: 1, e: 3, r: 5, y: 8, u: 10, o: 13, p: 15, "[": 17 };

export function mountEditor(ctx) {
  const { $, log = () => {} } = ctx;
  const E = { s: null, base: null, place: { label: "NEW SOUND" }, op: 1, oct: 0, held: new Map(), target: null,
    drag: null, raf: 0, picker: null, rec: false, onChange: null, live: false };
  const stage = $("ed-stage"), head = $("ed-head"), kb = $("ed-keys");

  // ---------------------------------------------------------------- values
  const v = (id) => E.s.values[id];
  function set(id, x, { quiet = false } = {}) {
    if (!E.s || id < 0 || id >= VALUES.length) return false;
    x = clampV(id, x);
    if (E.s.values[id] === x) return false;
    if (!quiet && E.target && E.target.locked && E.target.locked()) { if (ctx.onLocked) ctx.onLocked(); return false; }
    E.s.values[id] = x;
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

  // ---------------------------------------------------------------- the small parts
  // an arc of a knob's ring (FIZZ NEON: 270°, from the lower left), t0..t1 of its range
  const arcD = (cx, cy, R, t0, t1) => {
    const a0 = Math.PI * 0.75, a1 = Math.PI * 2.25, at = (t) => a0 + (a1 - a0) * t, pt = (a) => [cx + R * Math.cos(a), cy + R * Math.sin(a)];
    const [x0, y0] = pt(at(t0)), [x1, y1] = pt(at(t1));
    return `M${x0.toFixed(1)} ${y0.toFixed(1)} A${R} ${R} 0 ${Math.abs(t1 - t0) * 270 > 180 ? 1 : 0} ${t1 > t0 ? 1 : 0} ${x1.toFixed(1)} ${y1.toFixed(1)}`;
  };
  // a value's ring: the dark ring, the value an arc in its colour (from the middle for a signed value)
  const ringSvg = (id, cx, cy, R, vb) => {
    const r = info(id), f = frac(id, v(id)), z = r.min < 0 ? frac(id, 0) : 0;
    return `<svg viewBox="${vb}" aria-hidden="true"><path d="${arcD(cx, cy, R, 0, 1)}" class="track"/>${Math.abs(f - z) > 0.002 ? `<path d="${arcD(cx, cy, R, Math.min(z, f), Math.max(z, f))}" class="fill"/>` : ""}</svg>`;
  };
  // a value chip: its ring, its name, its text; dragged up / down, the wheel, the arrows
  const chip = (id, label = info(id).name, c = "--text", cls = "") =>
    `<span class="vc ${cls}" data-id="${id}" data-y="${id}" tabindex="0" style="--c:var(${c})" role="slider" aria-label="${esc(label)}" aria-valuetext="${esc(text(id, v(id)))}">${ringSvg(id, 12, 12, 8.5, "0 0 24 24")}<small>${esc(label)}</small><b>${esc(text(id, v(id)))}</b></span>`;
  // a picker chip (a long list: the destinations, the sources): a click opens the list
  const pick = (id, label, c = "--text", cls = "") =>
    `<span class="vc pick ${cls}" data-id="${id}" data-y="${id}" data-pick="${id}" tabindex="0" style="--c:var(${c})" role="button" aria-label="${esc(label)}: ${esc(text(id, v(id)))}"><small>${esc(label)}</small><b>${esc(text(id, v(id)))}</b></span>`;
  // a segmented choice (a short list)
  const seg = (id, c = "--text", labels = null) => {
    const r = info(id);
    return `<span class="vseg" style="--c:var(${c})" role="group" aria-label="${esc(r.name)}">${Array.from({ length: r.max - r.min + 1 }, (_, k) => {
      const x = r.min + k, on = v(id) === x;
      return `<button data-set="${id}:${x}" class="${on ? "on" : ""}" aria-pressed="${on}">${esc(labels ? labels[k] : text(id, x))}</button>`;
    }).join("")}</span>`;
  };
  // an on / off value as one pill, lit when on
  const tog = (id, label, c = "--text") => `<button class="vtog ${v(id) ? "on" : ""}" data-set="${id}:${v(id) ? 0 : 1}" aria-pressed="${!!v(id)}" style="--c:var(${c})">${esc(label)}</button>`;
  // a caption over a part
  const cap = (label, html) => `<span class="capt"><small>${esc(label)}</small>${html}</span>`;
  // the same with only some of the values: [[value, label], ..]
  const segOf = (id, c, pairs) => `<span class="vseg" style="--c:var(${c})" role="group" aria-label="${esc(info(id).name)}">${pairs.map(([x, l]) =>
    `<button data-set="${id}:${x}" class="${v(id) === x ? "on" : ""}" aria-pressed="${v(id) === x}">${esc(l)}</button>`).join("")}</span>`;
  // an arc (a dial): from the left end, or from the middle for a signed value
  function dial(id, label, c) {
    return `<span class="dial" data-id="${id}" data-y="${id}" tabindex="0" style="--c:var(${c})" role="slider" aria-label="${esc(label)}" aria-valuetext="${esc(text(id, v(id)))}">
      ${ringSvg(id, 35, 34, 27, "0 0 70 62")}<b>${esc(text(id, v(id)))}</b><small>${esc(label)}</small></span>`;
  }
  // a horizontal bar, dragged sideways
  const bar = (id, label, c) => {
    const r = info(id), f = frac(id, v(id)), z = r.min < 0 ? frac(id, 0) : 0;
    return `<span class="vbar" data-id="${id}" data-x="${id}" tabindex="0" style="--c:var(${c})" role="slider" aria-label="${esc(label)}" aria-valuetext="${esc(text(id, v(id)))}">
      <small>${esc(label)}</small><b>${esc(text(id, v(id)))}</b><i><span style="left:${(Math.min(z, f) * 100).toFixed(1)}%;width:${(Math.abs(f - z) * 100).toFixed(1)}%"></span></i></span>`;
  };
  // a handle in a drawing: x, y its place (the viewBox's), the ids it moves sideways / up and down, the drawing's
  // pixels for each value's whole range
  const handle = (x, y, { xid = -1, yid = -1, xs = 300, ys = 120, c = "--yellow", r = 9, label = "" } = {}) =>
    `<circle class="hd" cx="${x.toFixed(1)}" cy="${y.toFixed(1)}" r="${r}" style="--c:var(${c})" tabindex="0" role="slider" aria-label="${esc(label || [xid, yid].filter((i) => i >= 0).map((i) => info(i).name).join(" · "))}"` +
    `${xid >= 0 ? ` data-x="${xid}" data-xs="${xs}"` : ""}${yid >= 0 ? ` data-y="${yid}" data-ys="${ys}"` : ""} data-id="${yid >= 0 ? yid : xid}"/>`;

  // ---------------------------------------------------------------- envelopes
  // ADSR: attack to the top, decay to the sustain level, held, released; handles on the corners
  function adsr(ids, c, { w = 380, h = 150, ahd = false, label = "" } = {}) {
    const [a, d, s, r] = ids, X = (w - 30) * 0.27 / 127, top = label ? 30 : 14, bot = h - 10, Y = (lv) => bot - (bot - top) * lv / 127;
    const p0 = [10, bot], p1 = [10 + v(a) * X, top];
    let pts, hds;
    if (ahd) {                                       // AHD: ATK, HOLD (NOTE: as long as the key), DEC to nothing
      const hold = v(Q.HOLD), p2 = [p1[0] + (hold >= 127 ? 0.22 * (w - 30) : hold * X), top], p3 = [p2[0] + v(Q.EDEC) * X, bot];
      pts = [p0, p1, p2, p3];
      hds = handle(p1[0], p1[1], { xid: a, xs: 127 * X, c }) + handle(p2[0], p2[1], { xid: Q.HOLD, xs: 127 * X, c: "--yellow" }) +
        handle(p3[0], p3[1], { xid: Q.EDEC, xs: 127 * X, c });
      if (hold >= 127) hds += `<text x="${p2[0] - 6}" y="${top + 22}" class="lab" text-anchor="end">NOTE</text>`;
    } else {
      const p2 = [p1[0] + v(d) * X, Y(v(s))], p3 = [p2[0] + 0.16 * (w - 30), Y(v(s))], p4 = [p3[0] + v(r) * X, bot];
      pts = [p0, p1, p2, p3, p4];
      hds = handle(p1[0], p1[1], { xid: a, xs: 127 * X, c }) + handle(p2[0], p2[1], { xid: d, xs: 127 * X, yid: s, ys: bot - top, c: "--yellow" }) +
        handle(p4[0], p4[1], { xid: r, xs: 127 * X, c });
    }
    const path = pts.map(([x, y], i) => `${i ? "L" : "M"}${x.toFixed(1)} ${y.toFixed(1)}`).join(" ");
    return `<svg class="env" viewBox="0 0 ${w} ${h}" style="--c:var(${c})">${label ? `<text x="10" y="16" class="lab">${esc(label)}</text>` : ""}
      <path d="${path} Z" class="area"/><path d="${path}" class="line"/>${hds}</svg>`;
  }

  // ---------------------------------------------------------------- the panel's sections
  // OPERATORS: the algorithm drawn (modulators above what they modulate, carriers on the bus, the feedback a loop, the
  // modulation's dots running), ALGO a big pink number, a card an operator (its ratio big, its offset a bar)
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
    const parts = [`<line x1="150" y1="${BUS}" x2="520" y2="${BUS}" class="bus"/>`,
      `<path d="M528 ${BUS - 12}h12v24h-12zM540 ${BUS - 12}l16-14v52l-16-14" class="spk"/><path d="M566 ${BUS - 16}a22 22 0 0 1 0 32" class="spk wave"/>`];
    for (let o = 0; o < 4; o++) if ((out >> o) & 1) {          // a carrier into the bus, as thick as its MIX share
      const sh = (ox >> o) & 1 && (oy >> o) & 1 ? 1 : (ox >> o) & 1 ? sx : sy, lw = 2 + 6 * sh, cx = dep[o] ? x[o] + R + 8 : x[o];
      if (dep[o]) parts.push(`<path d="M${x[o] + R} ${y[o]} H${cx} V${BUS}" class="carrier" style="stroke-width:${lw.toFixed(1)};opacity:${(0.35 + 0.65 * sh).toFixed(2)}"/>`);
      else parts.push(`<line x1="${cx}" y1="${y[o] + R}" x2="${cx}" y2="${BUS}" class="carrier" style="stroke-width:${lw.toFixed(1)};opacity:${(0.35 + 0.65 * sh).toFixed(2)}"/>`);
      parts.push(`<circle cx="${cx}" cy="${BUS}" r="${(3 + 2 * sh).toFixed(1)}" class="joint"/>`);
    }
    for (let t = 0; t < 4; t++) for (let o = 0; o < 4; o++) if (o !== t && ((mods[t] >> o) & 1)) {   // o modulates t
      const dx = x[t] - x[o], dy = y[t] - y[o], l = Math.hypot(dx, dy), ux = dx / l, uy = dy / l;
      const sx0 = x[o] + ux * (R + 2), sy0 = y[o] + uy * (R + 2), ex = x[t] - ux * (R + 6), ey = y[t] - uy * (R + 6);
      parts.push(`<line x1="${sx0.toFixed(1)}" y1="${sy0.toFixed(1)}" x2="${ex.toFixed(1)}" y2="${ey.toFixed(1)}" class="modl"/>` +
        `<path d="M${ex.toFixed(1)} ${ey.toFixed(1)} l${(-ux * 14 - uy * 8).toFixed(1)} ${(-uy * 14 + ux * 8).toFixed(1)} l${(uy * 16).toFixed(1)} ${(-ux * 16).toFixed(1)} z" class="arrow"/>` +
        `<circle r="5" class="spark"><animateMotion dur="${(1.1 + 0.15 * (o + t)).toFixed(2)}s" repeatCount="indefinite" path="M${sx0.toFixed(1)} ${sy0.toFixed(1)} L${ex.toFixed(1)} ${ey.toFixed(1)}"/></circle>`);
    }
    if (fb < 4) parts.push(`<path d="M${x[fb] + R * 0.7} ${y[fb] - R * 0.7} a26 26 0 1 1 ${R * 0.5} ${R * 0.95}" class="fb" style="stroke:var(${OPS[fb][3]})"/>`);
    OPS.forEach(([nm, rid, , c], o) => {
      parts.push(`<g class="op ${E.op === o ? "on" : ""}" data-y="${rid}" data-id="${rid}" data-op="${o}" tabindex="0" role="slider" aria-label="RATIO ${nm}" aria-valuetext="${esc(text(rid, v(rid)))}" style="--c:var(${c})">` +
        `<circle cx="${x[o]}" cy="${y[o]}" r="${R}" class="rim"/><circle cx="${x[o]}" cy="${y[o]}" r="${R - 8}" class="inner"/><text x="${x[o]}" y="${y[o] + 9}" text-anchor="middle">${nm}</text></g>`);
    });
    parts.push(`<g class="algo" data-y="${Q.ALGO}" data-id="${Q.ALGO}" data-ys="240" data-step="1" tabindex="0" role="slider" aria-label="ALGO" aria-valuetext="${v(Q.ALGO)}">` +
      `<text x="22" y="40" class="algolab">ALGO</text><text x="12" y="186" class="algonum">${v(Q.ALGO)}</text></g>`);
    return `<svg class="algosvg" viewBox="0 0 600 400">${parts.join("")}</svg>`;
  }
  function opsPanel() {
    const card = ([nm, rid, oid, c], o) => {
      const t = text(rid, v(rid)), [w, f] = t.includes(".") ? t.split(".") : [t, ""];
      return `<div class="opc ${E.op === o ? "on" : ""}" data-op="${o}" style="--c:var(${c})"><span class="opn">${nm}</span>
        <span class="opv" data-y="${rid}" data-id="${rid}" tabindex="0" role="slider" aria-label="RATIO ${nm}" aria-valuetext="${esc(t)}">${esc(w)}${f ? `<small>.${esc(f)}</small>` : ""}</span>
        ${bar(oid, "OFS", c)}</div>`;
    };
    return `<div class="ops"><div class="algowrap">${algoSvg()}</div><div class="opcards">${OPS.map(card).join("")}</div></div>
      <div class="dials">${dial(Q.HARM, "HARM", K.c1)}${dial(Q.DTUN, "DTUN", K.c2)}${dial(Q.FDBK, "FDBK", K.c4)}${dial(Q.MIX, "MIX X·Y", K.c3)}</div>`;
  }

  // OP ENV: the two operator envelopes (A: operator A; B: B1 and B2) as shapes, DELAY a handle at the start, LEVEL a
  // bar, the operators' TRIG and RESET
  function openvPanel() {
    const env = (k) => {
      const [atk, dec, end, lev, dly, trg, rst] = k ? [Q.BATK, Q.BDEC, Q.BEND, Q.BLEV, Q.BDLY, Q.BTRIG, Q.BRST] : [Q.AATK, Q.ADEC, Q.AEND, Q.ALEV, Q.ADLY, Q.ATRIG, Q.ARST];
      const c = k ? K.c3 : K.c2, w = 420, h = 150, X = (w - 30) * 0.3 / 127, top = 18, bot = h - 12, Y = (lv) => bot - (bot - top) * lv / 127;
      const p0 = [12 + v(dly) * X, bot], p1 = [p0[0] + v(atk) * X, top], p2 = [p1[0] + v(dec) * X, Y(v(end))], p3 = [w - 12, Y(v(end))];
      const path = `M12 ${bot} L${p0[0].toFixed(1)} ${bot} L${p1[0].toFixed(1)} ${top} L${p2[0].toFixed(1)} ${p2[1].toFixed(1)} L${p3[0]} ${p3[1].toFixed(1)}`;
      return `<div class="sub envc" style="--c:var(${c})"><div class="row"><span class="cn">ENV ${k ? "B" : "A"}</span><span class="kv">${k ? "B1 · B2" : "A"}</span>
          <span class="tgs">${tog(trg, "TRIG", c)}${tog(rst, "RESET", c)}</span></div>
        <svg class="env" viewBox="0 0 ${w} ${h}"><path d="${path} L${p3[0]} ${bot} Z" class="area"/><path d="${path}" class="line"/>
          ${handle(p0[0], bot, { xid: dly, xs: 127 * X, c: "--muted", r: 8 })}${handle(p1[0], top, { xid: atk, xs: 127 * X, c, r: 10 })}${handle(p2[0], p2[1], { xid: dec, xs: 127 * X, yid: end, ys: bot - top, c: "--yellow", r: 10 })}</svg>
        <div class="row wrap">${chip(atk, "ATK", c)}${chip(dec, "DEC", c)}${chip(end, "END", c)}${chip(dly, "DLY", c)}</div>
        ${bar(lev, "LEVEL", c)}</div>`;
    };
    return `<div class="duo">${env(0)}${env(1)}</div>`;
  }

  // FILTER: the machine as chips, its curve with FREQ and RESO (GAIN on the EQ) the handle; the filter envelope; the
  // band's BASE / WIDTH and before / after
  function curve(w, h) {
    const ft = v(Q.FTYPE), f = v(Q.FREQ) / 12700, r = v(Q.RESO) / 12700, sh = v(Q.FSHAPE) / 127, N = 90;
    const db = [];
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
    const Y = (d) => 30 + (h - 50) * (20 - d) / 56, xs = (i) => 10 + (w - 20) * i / N;
    const path = db.map((d, i) => `${i ? "L" : "M"}${xs(i).toFixed(1)} ${Y(d).toFixed(1)}`).join(" ");
    const hx = xs(f * N), hy = Y(db[Math.round(f * N)]) - 2;
    const yid = ft === 7 ? Q.FGAIN : ft === 0 ? -1 : Q.RESO;
    const tx = Math.min(w - 150, hx + 20), ty = hy > 80 ? hy - 38 : hy + 44;   // (the values beside the handle, clear of the curve)
    return `<svg class="curve" viewBox="0 0 ${w} ${h}" style="--c:var(${K.c4})">
      ${[1, 2, 3, 4, 5].map((k) => `<line x1="${(w * k / 6).toFixed(0)}" y1="20" x2="${(w * k / 6).toFixed(0)}" y2="${h - 18}" class="grid"/>`).join("")}
      <path d="${path} L${w - 10} ${h - 18} L10 ${h - 18} Z" class="area"/><path d="${path}" class="line"/>
      ${handle(hx, Math.max(16, hy), { xid: Q.FREQ, xs: w - 20, yid, ys: (h - 50) * 0.8, r: 14, label: "FREQ" })}
      <text x="${tx.toFixed(0)}" y="${ty.toFixed(0)}" class="val">${esc(text(Q.FREQ, v(Q.FREQ)))}</text>
      ${yid >= 0 ? `<text x="${tx.toFixed(0)}" y="${(ty + 26).toFixed(0)}" class="sub">${ft === 7 ? "GAIN" : ft >= 5 ? "FDBK" : "RESO"} ${esc(text(yid, v(yid)))}</text>` : ""}</svg>`;
  }
  function filterPanel() {
    const ft = v(Q.FTYPE), m = MACH.findIndex(([, l]) => l.includes(ft));
    const machines = `<span class="vseg mach" style="--c:var(${K.c4})" role="group" aria-label="MACHINE">${MACH.map(([nm, l], k) =>
      `<button data-set="${Q.FTYPE}:${k === 0 ? (ft <= 2 ? ft : 1) : l[0]}" class="${k === m ? "on" : ""}" aria-pressed="${k === m}">${nm}</button>`).join("")}</span>`;
    const legacy = m === 0 ? segOf(Q.FTYPE, K.c4, [[0, "OFF"], [1, "LP12"], [2, "HP12"]]) : "";
    const shape = m >= 2 ? chip(Q.FSHAPE, m === 2 ? "TYPE" : m === 5 ? "Q" : "LPF", K.c3) : "";
    return `<div class="row wrap">${machines}${legacy}</div>
      <div class="duo filt"><div class="sub">${curve(600, 300)}
          <div class="row wrap">${m === 5 ? chip(Q.FGAIN, "GAIN", K.c2) : chip(Q.RESO, m >= 3 && m <= 4 ? "FDBK" : "RESO", K.c2)}${shape}${chip(Q.FKTRK, "KEY TRK", K.c3)}</div></div>
        <div class="sub">${adsr([Q.FATK, Q.FDEC, Q.FSUS, Q.FREL], K.c4, { w: 420, h: 150, label: "ENV" })}
          <div class="row wrap">${chip(Q.FDEPTH, "DEPTH", K.c4, "big")}${chip(Q.FDLY, "DLY", K.c4)}</div>
          <div class="row wrap">${chip(Q.FATK, "ATK", K.c4)}${chip(Q.FDEC, "DEC", K.c4)}${chip(Q.FSUS, "SUS", K.c4)}${chip(Q.FREL, "REL", K.c4)}</div></div></div>
      <div class="row wrap">${chip(Q.BASE, "BASE", K.c1)}${chip(Q.WIDTH, "WIDTH", K.c2)}${seg(Q.BWPOST, K.c3, ["BASE · WIDTH BEFORE", "AFTER"])}</div>`;
  }

  // AMP: the amp envelope (ADSR or AHD), LEVEL
  function ampPanel() {
    const ahd = v(Q.AMOD) === 1;
    return `<div class="row">${seg(Q.AMOD, K.c3)}</div>
      ${adsr([Q.EATK, Q.EDEC, Q.ESUS, Q.EREL], K.c3, { w: 520, h: 170, ahd })}
      <div class="row wrap">${ahd ? chip(Q.EATK, "ATK", K.c3) + chip(Q.HOLD, "HOLD", "--yellow") + chip(Q.EDEC, "DEC", K.c3)
        : chip(Q.EATK, "ATK", K.c3) + chip(Q.EDEC, "DEC", K.c3) + chip(Q.ESUS, "SUS", K.c3) + chip(Q.EREL, "REL", K.c3)}</div>
      ${bar(Q.LEVEL, "LEVEL", K.c3)}`;
  }

  // DRIVE and its route; CRUSH (the bits, the rate, before / after)
  function crushSvg(w, h) {
    const bits = v(Q.BITS), srr = v(Q.SRR), q = bits ? Math.max(2, Math.round(2 + 14 * (1 - bits / 127))) : 0, hold = srr ? 1 + Math.floor(srr / 9) : 1;
    let d = "", last = 0;
    for (let i = 0; i <= 120; i++) {
      if (i % hold === 0) { let s = Math.sin(i / 120 * Math.PI * 4); if (q) s = Math.round(s * q) / q; last = s; }
      d += `${i ? "L" : "M"}${(10 + (w - 20) * i / 120).toFixed(1)} ${(h / 2 - last * (h / 2 - 10)).toFixed(1)}`;
    }
    return `<svg class="crush" viewBox="0 0 ${w} ${h}" style="--c:var(--pink)"><path d="${d}" class="line"/></svg>`;
  }
  function drivePanel() {
    return `<div class="row wrap drv">${dial(Q.DRIVE, "DRIVE", "--orange")}${seg(Q.DRPRE, "--orange", ["AFTER FILTER", "BEFORE"])}</div>
      <div class="sub" style="--c:var(--pink)"><span class="cn">CRUSH</span>${crushSvg(380, 90)}
        <div class="row wrap">${chip(Q.BITS, "BITS", "--pink")}${chip(Q.SRR, "SRR", "--pink")}${seg(Q.CRPOST, "--pink", ["BEFORE FILTER", "AFTER"])}</div></div>`;
  }

  // a wave's glyph (TRI SINE SQR SAW RAMP EXP RAND), for the wave's choice
  const WAVE_D = ["M2 14 L8 4 L14 14 L20 4 L26 14", "M2 9 Q5 0 8 9 T14 9 T20 9 T26 9", "M2 14 V4 H9 V14 H16 V4 H23 V14", "M2 4 L12 14 V4 L22 14 V4",
    "M2 14 L12 4 V14 L22 4 V14", "M2 4 Q4 13 12 14 M12 4 Q14 13 22 14", "M2 10 H7 V4 H12 V13 H17 V7 H22 V11 H26"];
  const waveSeg = (id, c) => `<span class="vseg waves" style="--c:var(${c})" role="group" aria-label="WAVE">${WAVE_D.map((d, k) =>
    `<button data-set="${id}:${k}" class="${v(id) === k ? "on" : ""}" aria-pressed="${v(id) === k}" aria-label="${esc(text(id, k))}" title="${esc(text(id, k))}"><svg viewBox="0 0 28 18" aria-hidden="true"><path d="${d}"/></svg></button>`).join("")}</span>`;
  // LFO: each a card: its wave (moving), its destination a chip, SPEED MULT DEPTH FADE PHASE, TRIG and the wave chosen
  function wavePath(k, w, h) {
    const wv = v(lfo(k, LF.WAVE)), sp = Math.abs(v(lfo(k, LF.SPEED))) / 6400, dp = v(lfo(k, LF.DEPTH)) / 6400, ph = v(lfo(k, LF.PHASE)) / 128;
    const fade = v(lfo(k, LF.FADE)), cyc = 1.2 + 3.3 * sp + (v(lfo(k, LF.MULT)) % 12) / 8, amp = (0.12 + 0.88 * Math.abs(dp)) * (dp < 0 ? -1 : 1);
    const N = 160, pts = [];
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
      pts.push([10 + (w - 20) * t, h / 2 - y * amp * fm * (h / 2 - 8)]);
    }
    return pts.map(([x, y], i) => `${i ? "L" : "M"}${x.toFixed(1)} ${y.toFixed(1)}`).join(" ");
  }
  function lfoPanel() {
    const card = (k) => {
      const c = LFOC[k], d = wavePath(k, 420, 80);
      return `<div class="sub lfoc" style="--c:var(${c})"><div class="row"><span class="cn">LFO ${k + 1}</span>${pick(lfo(k, LF.DEST), "DEST", c, "dest")}</div>
        <svg class="wave" viewBox="0 0 420 80"><line x1="10" y1="40" x2="410" y2="40" class="axis"/><path d="${d}" class="line"/><circle r="6" class="spark"><animateMotion dur="${(3.2 - 2.4 * Math.abs(v(lfo(k, LF.SPEED))) / 6400).toFixed(2)}s" repeatCount="indefinite" path="${d}"/></circle></svg>
        <div class="row wrap">${chip(lfo(k, LF.SPEED), "SPEED", c)}${chip(lfo(k, LF.DEPTH), "DEPTH", c)}${chip(lfo(k, LF.MULT), "MULT", c)}${chip(lfo(k, LF.FADE), "FADE", c)}${chip(lfo(k, LF.PHASE), "PHASE", c)}</div>
        <div class="row wrap">${waveSeg(lfo(k, LF.WAVE), c)}${seg(lfo(k, LF.TRIG), c)}</div></div>`;
    };
    return `<div class="lfos">${[0, 1, 2].map(card).join("")}</div>`;
  }

  // MOD: the eight modulation slots as cables, each from its source to its destination, as thick as its depth
  function modPanel() {
    return Array.from({ length: 8 }, (_, k) => {
      const src = v(mod(k, 0)), dp = v(mod(k, 2)), c = dp < 0 ? "--orange" : "--cyan";
      if (!src) return `<div class="mrow empty"><span class="mk">${k + 1}</span><button class="madd" data-set="${mod(k, 0)}:1" aria-label="Modulation slot ${k + 1}: add">+</button></div>`;
      const wv = Math.max(1.5, Math.abs(dp) / 14);
      return `<div class="mrow" style="--c:var(${c})"><span class="mk">${k + 1}</span>${pick(mod(k, 0), "SRC", K.alg, "src")}
        <svg class="cable" viewBox="0 0 160 34" data-x="${mod(k, 2)}" data-id="${mod(k, 2)}" data-xs="160"><path d="M4 17 C50 ${dp >= 0 ? 0 : 34} 110 ${dp >= 0 ? 34 : 0} 156 17" style="stroke-width:${wv.toFixed(1)}"/></svg>
        ${pick(mod(k, 1), "DEST", "--text", "dest")}${chip(mod(k, 2), "DEPTH", c, "depth")}
        <button class="icon mx" data-set="${mod(k, 0)}:0" aria-label="Modulation slot ${k + 1}: off"><svg viewBox="0 0 20 20"><path d="M5 5l10 10M15 5L5 15"/></svg></button></div>`;
    }).join("");
  }

  const VO = 110;                                  // (the VOICE page's first value)
  // PLAY: the operators' phase reset, the key tracks and the velocity, the portamento (its glide drawn)
  function playPanel() {
    const pt = v(Q.PTIM), on = v(Q.PORT), w = 380, h = 90, gx = 60 + 260 * (pt / 127);
    const glide = `<svg class="glide" viewBox="0 0 ${w} ${h}" style="--c:var(--yellow)"><path d="M10 70 H60 ${on ? `C${(60 + gx) / 2} 70 ${(60 + gx) / 2} 20 ${gx.toFixed(1)} 20` : "V20"} H${w - 10}" class="line ${on ? "" : "dim"}"/>
      ${on ? handle(gx, 20, { xid: Q.PTIM, xs: 260, c: "--yellow", r: 10 }) : ""}</svg>`;
    return `<div class="row wrap">${cap("PHASE RESET", seg(Q.PHRT, K.c1))}</div>
      <div class="bars">${bar(Q.AKEY, "A KEY", K.c2)}${bar(Q.B1KEY, "B1 KEY", K.c3)}${bar(Q.B2KEY, "B2 KEY", K.c4)}${bar(Q.VEL, "VEL", K.c1)}</div>
      <div class="sub" style="--c:var(--yellow)"><span class="cn">PORTAMENTO</span>${glide}
        <div class="row wrap">${seg(Q.PORT, "--yellow")}${chip(Q.PTIM, "TIME", "--yellow")}${cap("SLOPE", seg(Q.PSLOPE, "--yellow"))}</div></div>
      <div class="sub" style="--c:var(--violet)"><span class="cn">VOICE</span>
        <div class="row wrap">${seg(VO, K.c1)}${chip(VO + 1, "UNISON", K.c2)}${chip(VO + 2, "DETUNE", K.c3)}${chip(VO + 3, "SPREAD", K.c4)}${cap("PRIORITY", seg(VO + 4, K.alg))}</div></div>`;
  }
  // 2026-10-26: the VOICE page's values (110..114: VOICE, UNISON, DETUNE, SPREAD, PRIORITY), with PLAY's portamento
  const PANELS = { ops: opsPanel, openv: openvPanel, filter: filterPanel, amp: ampPanel, drive: drivePanel, lfo: lfoPanel, mod: modPanel, play: playPanel };

  // ---------------------------------------------------------------- the frame: header, the panel, keys
  // the header: the track the editor follows big (pink: LIVE; dim: only shown, TRACK off), the place, the name, the
  // tags; the track strip (live.js: SELECTED, 1..6, LIVE) when an FM-1 that edits tracks is there; the actions
  function renderHead() {
    if (!E.s) return;
    const t = E.target, dirty = !sameValues(E.s, E.base) || E.s.name !== E.base.name || E.s.tags !== E.base.tags;
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
      return `<div class="edwhere">${where}</div>
        <button class="ol" id="ed-revert" ${dirty && !locked ? "" : "disabled"}>↺ REVERT</button>
        <button class="ol" id="ed-savenew" style="--c:var(--pink)" ${can.saveNew ? "" : "disabled"}>SAVE AS NEW</button>
        <button class="ol" id="ed-export" style="--c:var(--violet)">EXPORT</button>
        <button class="arc${can.save && dirty ? " breathe" : ""}" id="ed-save" style="--c:var(--pink)" ${can.save ? "" : "disabled"}>SAVE</button>`;
    }
    const tk = E.place.track >= 0 && E.place.fromTarget ? E.place.track : -1;
    const pl = /^(.*\S)\s+(\d+)$/.exec(E.place.label || ""), plh = pl ? `${esc(pl[1])} <span class="hud">${pl[2]}</span>` : esc(E.place.label || "");
    head.innerHTML = `${tk >= 0 ? `<div class="trkbig ${live && !locked ? "live" : "view"}" aria-label="Track ${tk + 1}${locked ? ", shown only" : ", live"}"><small>TRACK</small><b>${tk + 1}</b></div>` : ""}
      <div class="edname"><div class="h">${E.s.midi ? '<span class="mchip">MIDI</span> ' : ""}${plh}${dirty ? ' <span class="edited">EDITED</span>' : ""}</div>
        <input class="bigname" id="ed-name" maxlength="12" spellcheck="false" autocomplete="off" value="${esc(E.s.name)}" aria-label="Name">
        <div class="edtags">${FIZZ_TAGS.map((tg, k) => `<button class="tag t-${tg} ${(E.s.tags >> k) & 1 ? "" : "off"}" data-tag="${k}" aria-pressed="${!!((E.s.tags >> k) & 1)}">${tg}</button>`).join("")}</div></div>
      <div class="row edacts" id="ed-acts">${acts()}</div>`;
    stage.classList.toggle("locked", locked);
  }
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
  // the panel: every section at once (a synth's front panel); a section drawn again only when its drawing changed
  const drawn = new Map();
  function renderStage() {
    if (!E.s) return;
    const mode = E.s.midi ? "midi" : "fm";
    if (stage.dataset.mode !== mode) { stage.dataset.mode = mode; stage.classList.toggle("midi", mode === "midi"); stage.innerHTML = ""; drawn.clear(); }
    if (E.s.midi) { renderMidi(); return; }
    if (!stage.querySelector(".pn")) {
      stage.innerHTML = SECTIONS.map(([k, nm, c]) => `<section class="pn pn-${k}" data-sec="${k}" style="--c:var(${c})" aria-label="${nm}">` +
        `<div class="pnh"><i class="pdot" aria-hidden="true"></i>${nm}</div><div class="pnb"></div></section>`).join("");
      drawn.clear();
    }
    const keep = document.activeElement && stage.contains(document.activeElement) ? focusKey(document.activeElement) : null;
    let any = false;
    for (const [k] of SECTIONS) {
      const html = PANELS[k]();
      if (drawn.get(k) === html) continue;
      drawn.set(k, html);
      stage.querySelector(`[data-sec="${k}"] .pnb`).innerHTML = html;
      any = true;
    }
    if (!any) return;
    if (E.drag) { const el = findKey(E.drag.key); if (el) el.classList.add("dragging"); }
    if (keep) { const el = findKey(keep); if (el && el.focus && el !== document.activeElement) el.focus({ preventScroll: true }); }
    if (E.picker) openPicker(E.picker.id, true);
  }
  // an element's identity across renders: its ids, its kind
  const focusKey = (el) => [el.tagName, el.dataset.x || "", el.dataset.y || "", el.dataset.id || "", el.dataset.op || ""].join("|");
  const findKey = (key) => [...stage.querySelectorAll("[data-id]")].find((e) => focusKey(e) === key);
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
  function render() { renderHead(); renderStage(); renderKeys(); }

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
    const el = ev.target.closest("[data-x],[data-y]");
    if (!el || !E.s || ev.button !== 0) return;
    ev.preventDefault();
    const sc = scaleOf(el);
    const ax = (k) => { const id = el.dataset[k]; if (id === undefined) return null; const r = info(+id); return { id: +id, v0: v(+id), span: r.max - r.min, px: (+(el.dataset[`${k}s`] || 0) || 0) * sc }; };
    E.drag = { el, key: focusKey(el), x0: ev.clientX, y0: ev.clientY, x: ax("x"), y: ax("y"), moved: false, pid: ev.pointerId };
    try { stage.setPointerCapture(ev.pointerId); } catch (_) { /* (gone) */ }
    el.classList.add("dragging");
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
    const b = ev.target.closest("[data-set]");
    const oc = ev.target.closest(".opc[data-op]");
    if (!b && oc) { E.op = +oc.dataset.op; renderStage(); return; }
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
    const el = ev.target.closest("[data-id]");
    if (!el || !E.s) return;
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
    if (b.dataset.tag !== undefined) { E.s.tags ^= 1 << +b.dataset.tag; renderHead(); return; }
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
    if (sound.name !== E.s.name && document.activeElement !== head.querySelector("#ed-name")) { E.s.name = sound.name; n++; }
    if (n && rebase) E.base = copySound({ ...E.s, tags: E.base ? E.base.tags : E.s.tags });
    if (n) later();
    return n > 0;
  }
  const blank = () => ({ name: "INIT", values: defaults(), pan: 0, sends: { chorus: 0, delay: 0, reverb: 0 }, tags: 0, source: "new", slot: 0, track: -1, extras: null });
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
