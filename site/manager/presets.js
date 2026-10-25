// SPDX-License-Identifier: GPL-3.0-only
// The preset lists of LIBRARY and POOL (one column each, the same look): MY PRESETS then the factory's, filtered by
// where they live (ALL, FACTORY, MY PRESETS) and by FIZZ's tags; a sound the project's pool references carries the
// pool's mark (a green ring and a badge with its place number, "+n" when it is in more places), as in the POOL grid.
import { FIZZ_TAGS, parseDump } from "../js/fizz_sysex.js";
import { esc, tagsOf, colOf, algo } from "./ui.js";

export const SRC_FILTERS = [["all", "ALL"], ["factory", "FACTORY"], ["mine", "MY PRESETS"]];
export const newFilter = () => ({ src: "all", tags: new Set() });

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

// the filter bar: the source segment, then the tag chips
export function filterHtml(f) {
  return `<div class="seg srcseg" role="group" aria-label="Show">${SRC_FILTERS.map(([k, t]) => `<button data-src="${k}" class="${f.src === k ? "on" : ""}" aria-pressed="${f.src === k}">${t}</button>`).join("")}</div>` +
    `<div class="chips">${FIZZ_TAGS.map((t) => `<button class="tag t-${t} ${f.tags.size && !f.tags.has(t) ? "off" : ""}" data-tag="${t}" aria-pressed="${f.tags.has(t)}">${t}</button>`).join("")}</div>`;
}
// a click in the filter bar: true when the filter changed
export function filterClick(f, ev) {
  const s = ev.target.closest("[data-src]"), t = ev.target.closest("[data-tag]");
  if (s) { f.src = s.dataset.src; return true; }
  if (t) { if (f.tags.has(t.dataset.tag)) f.tags.delete(t.dataset.tag); else f.tags.add(t.dataset.tag); return true; }
  return false;
}
const tagOk = (f, r) => !f.tags.size || tagsOf(r.tags).some((t) => f.tags.has(t));
// the rows shown: MY PRESETS first, then the factory's
export function shownRows(rows, f) {
  const mine = f.src === "factory" ? [] : rows.filter((r) => r.source === "mine" && tagOk(f, r));
  const fac = f.src === "mine" ? [] : rows.filter((r) => r.source === "factory" && tagOk(f, r));
  return { mine, fac };
}

// one cassette. o: { frameOf(r), places (poolPlaces), on(r) selected, drag(r) draggable }
export function casHtml(r, o) {
  const f = o.frameOf(r), d = f && parseDump(f), tg = tagsOf(r.tags), c = colOf(r.tags), pl = o.places.get(r.id);
  const drag = o.drag && o.drag(r) ? ' draggable="true"' : "";
  return `<button class="cas${pl ? " inpool" : ""}" style="--c:${c}" data-src="${r.source}" data-slot="${r.slot}" data-id="${r.id}" aria-pressed="${!!o.on(r)}"${drag}>` +
    `<span class="stripe"></span><span class="sl">${r.slot}</span>${algo(d ? d.blob[2] : r.slot, c)}<span class="nm">${esc(r.name)}</span>` +
    `<span class="mark">${r.imported ? '<span class="imp" title="imported">↓</span>' : ""}${tg[0] ? `<span class="tag t-${tg[0]}">${tg[0]}</span>` : ""}${poolBadge(pl)}</span></button>`;
}
// the whole list: the two groups, each under its name when both show
export function listHtml(rows, f, o) {
  const { mine, fac } = shownRows(rows, f);
  const grp = (name, n) => `<div class="grp" role="presentation">${name}<span>${n}</span></div>`;
  const both = f.src === "all";
  return (both && mine.length ? grp("MY PRESETS", mine.length) : "") + mine.map((r) => casHtml(r, o)).join("") +
    (both && fac.length ? grp("FACTORY", fac.length) : "") + fac.map((r) => casHtml(r, o)).join("");
}
