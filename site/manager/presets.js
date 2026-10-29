// SPDX-License-Identifier: GPL-3.0-only
// The preset lists of LIBRARY and POOL (one column each, the same look): MY PRESETS then the factory's, filtered by
// where they live (ALL, FACTORY, MY PRESETS), by the pool (IN POOL, NOT IN POOL: the owner, 2026-10-26) and by FIZZ's tags; a sound the project's pool references carries the
// pool's mark (a green ring and a badge with its place number, "+n" when it is in more places), as in the POOL grid.
import { FIZZ_TAGS, parseDump } from "../js/fizz_sysex.js";
import { esc, tagsOf, colOf, MIDI_I, midiChip } from "./ui.js";
import { isMidiBlob } from "./sound.js";

export const SRC_FILTERS = [["all", "ALL"], ["factory", "FACTORY"], ["mine", "MY PRESETS"]];
// (LIBRARY: the presets of the browser's own FIZZ, a source of their own; q: the search, a name's part, lower case)
export const BROWSER_SRC = ["browser", '<svg class="bglyph" viewBox="0 0 24 24" aria-hidden="true"><rect x="2.5" y="4" width="19" height="16" rx="3"/><path d="M2.5 8.5h19"/><circle cx="9.5" cy="15" r="2.6"/><circle cx="14.6" cy="12.6" r="1.6"/><circle cx="15.2" cy="17" r="1.1"/></svg>IN THIS BROWSER'];
export const newFilter = () => ({ src: "all", tags: new Set(), pool: "", q: "" });   // pool: "", "in" or "out"

// preset id -> the pool places referencing it (ascending)
export function poolPlaces(rows) {
  const m = new Map();
  for (const r of rows) if (r.source === "pool" && r.id) { if (!m.has(r.id)) m.set(r.id, []); m.get(r.id).push(r.slot); }
  for (const v of m.values()) v.sort((a, b) => a - b);
  return m;
}
// the pool's mark: a pool tile with the place's number
export function poolBadge(places, cls = "pbadge") {
  if (!places || !places.length) return "";
  return `<span class="${cls}" aria-label="in the pool, place ${places.join(", ")}"><svg viewBox="0 0 12 12" aria-hidden="true"><rect x="0" y="0" width="5" height="5" rx="1"/><rect x="7" y="0" width="5" height="5" rx="1"/><rect x="0" y="7" width="5" height="5" rx="1"/><rect x="7" y="7" width="5" height="5" rx="1"/></svg>${places[0]}${places.length > 1 ? `<small>+${places.length - 1}</small>` : ""}</span>`;
}

// the filter bar: where they live (a click on the lit one keeps it), the tags (lit when picked), the pool's segment (a
// click on the lit one turns it off). o.counts: { src: { all, factory, mine }, tags: { BASS: n, .. } } shown beside them
const POOL_I = '<svg viewBox="0 0 12 12" aria-hidden="true"><rect x="0" y="0" width="5" height="5" rx="1"/><rect x="7" y="0" width="5" height="5" rx="1"/><rect x="0" y="7" width="5" height="5" rx="1"/><rect x="7" y="7" width="5" height="5" rx="1"/></svg>';
// o.browser: IN THIS BROWSER among the sources; o.all: an ALL chip before the pool's (the tags and the pool off: a phone's row)
export function filterHtml(f, o = {}) {
  const c = o.counts || { src: {}, tags: {} }, n = (v) => (v === undefined ? "" : `<span class="fn hud">${v}</span>`);
  const srcs = o.browser ? [...SRC_FILTERS, BROWSER_SRC] : SRC_FILTERS;
  const all = o.all ? `<button class="tag allchip ${!f.tags.size && !f.pool ? "on" : ""}" data-clear="1" aria-pressed="${!f.tags.size && !f.pool}">ALL</button>` : "";
  return `<div class="fgrp"><span class="lbl">Source</span><div class="seg srcseg" role="group" aria-label="Show">${srcs.map(([k, t]) => `<button data-src="${k}" class="${f.src === k ? "on" : ""}" aria-pressed="${f.src === k}">${t}${n(c.src[k])}</button>`).join("")}</div></div>` +
    `<div class="fgrp"><span class="lbl">Tags</span><div class="chips">${all}${FIZZ_TAGS.map((t) => `<button class="tag t-${t} ${f.tags.has(t) ? "on" : f.tags.size ? "off" : ""}" data-tag="${t}" aria-pressed="${f.tags.has(t)}">${t}${n(c.tags[t])}</button>`).join("")}</div></div>` +
    `<div class="fgrp"><span class="lbl">Pool</span><div class="seg poolseg" role="group" aria-label="The pool">${[["in", "IN POOL"], ["out", "NOT IN POOL"]].map(([k, t]) => `<button data-pool="${k}" class="${k} ${f.pool === k ? "on" : ""}" aria-pressed="${f.pool === k}">${k === "in" ? POOL_I : ""}${t}</button>`).join("")}</div></div>`;
}
// a click in the filter bar: true when the filter changed
export function filterClick(f, ev) {
  const s = ev.target.closest("[data-src]"), t = ev.target.closest("[data-tag]"), p = ev.target.closest("[data-pool]");
  if (ev.target.closest("[data-clear]")) { f.tags.clear(); f.pool = ""; return true; }
  if (s) { f.src = s.dataset.src; return true; }
  if (p) { f.pool = f.pool === p.dataset.pool ? "" : p.dataset.pool; return true; }
  if (t) { if (f.tags.has(t.dataset.tag)) f.tags.delete(t.dataset.tag); else f.tags.add(t.dataset.tag); return true; }
  return false;
}
const tagOk = (f, r) => !f.tags.size || tagsOf(r.tags).some((t) => f.tags.has(t));
const inPool = (r, places) => r.source !== "browser" && !!(places.get(r.id) || []).length;   // (the unit's pool: not the browser's rows)
const poolOk = (f, r, places) => !f.pool || (f.pool === "in") === inPool(r, places);
const nameOk = (f, r) => !f.q || String(r.name).toLowerCase().includes(f.q);
// the rows shown: MY PRESETS first, then the factory's (places: poolPlaces of the rows, made here when not given); IN
// THIS BROWSER's rows (source "browser") only when that source is picked
export function shownRows(rows, f, places = poolPlaces(rows)) {
  const ok = (r) => tagOk(f, r) && poolOk(f, r, places) && nameOk(f, r);
  const mine = f.src === "factory" || f.src === "browser" ? [] : rows.filter((r) => r.source === "mine" && ok(r));
  const fac = f.src === "mine" || f.src === "browser" ? [] : rows.filter((r) => r.source === "factory" && ok(r));
  const brow = f.src === "browser" ? rows.filter((r) => r.source === "browser" && ok(r)) : [];
  return { mine, fac, brow };
}

// one cassette: its ring (the algorithm, the tag's colour), its place, the pool's mark, the name, the tag; the one
// picked lit, with the audition's bars. o: { frameOf(r), places (poolPlaces), on(r) selected, drag(r) draggable, i (its
// place in the list: the pop's stagger) }
// o.check(r): SELECT's mark (a check circle) on it; o.isNew(r): just sent here (a NEW badge, a glow, for a moment)
export function casHtml(r, o) {
  const f = o.frameOf(r), d = (f && parseDump(f)) || (r.blob ? { blob: r.blob } : null), tg = tagsOf(r.tags), c = colOf(r.tags);
  const pl = r.source === "browser" ? null : o.places.get(r.id), mi = !!d && isMidiBlob(d.blob), on = !!o.on(r), ck = o.check ? o.check(r) : null;
  const drag = o.drag && o.drag(r) ? ' draggable="true"' : "";
  const ring = mi ? MIDI_I : `<span class="hud">${d ? Math.min(8, d.blob[2] + 1) : ""}</span>`;
  const nw = !!(o.isNew && o.isNew(r));
  return `<button class="cas${pl ? " inpool" : ""}${nw ? " new" : ""}${o.i !== undefined && o.i < 40 ? " pop" : ""}" style="--c:${c}${o.i !== undefined ? `;--i:${Math.min(o.i, 40)}` : ""}" data-src="${r.source}" data-slot="${r.slot}" data-id="${r.id}" aria-pressed="${on}"${drag}>` +
    `${ck === null ? "" : `<span class="chk${ck ? " on" : ""}" aria-hidden="true"><svg viewBox="0 0 16 16"><path d="M3.5 8.5l3 3 6-7"/></svg></span>`}<span class="ring" aria-hidden="true">${ring}</span><span class="sl hud">${String(r.slot).padStart(2, "0")}</span><span class="nm">${esc(r.name)}</span>` +
    `<span class="mark">${tg[0] ? `<span class="tag t-${tg[0]}">${tg[0]}</span>` : ""}${midiChip(mi)}${r.imported ? '<span class="imp" title="imported">↓</span>' : ""}${on ? '<span class="eq" aria-hidden="true"><i></i><i></i><i></i><i></i></span>' : ""}</span>` +
    `<span class="pmk">${nw ? '<span class="newb hud">NEW</span>' : poolBadge(pl)}</span></button>`;
}
// the whole list: the two groups, each under its name when both show
export function listHtml(rows, f, o) {
  const { mine, fac, brow } = shownRows(rows, f, o.places);
  const grp = (name, n) => `<div class="grp" role="presentation">${name}<span>${n}</span></div>`;
  const both = f.src === "all";
  const k = (r, i) => casHtml(r, o.pop ? { ...o, i } : o);
  return (both && mine.length ? grp("MY PRESETS", mine.length) : "") + mine.map(k).join("") +
    (both && fac.length ? grp("FACTORY", fac.length) : "") + fac.map((r, i) => k(r, mine.length + i)).join("") + brow.map(k).join("");
}
