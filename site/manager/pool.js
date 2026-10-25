// SPDX-License-Identifier: GPL-3.0-only
// The manager's POOL tab (docs/WEB-MANAGER-PLAN.md §8 shot 4): the project's 128 pool places as tiles (16 x 8, the
// keybed's rows), each the preset it references in its tag's colour, dots for the steps that lock it. The sounds
// beside (MY PRESETS and the factory's, LIBRARY's filters; FIZZ's POOL REF takes a factory id as well as a MY
// preset's): one dropped (or picked, then a tile clicked) onto a place references it there (no copy); a picked one's
// places ringed. A tile dropped onto another swaps the two numbers in every pattern (SWAP: a job on the unit, stopped
// only); CLEAR takes a sound out (its steps go back to their track's sound, as on the panel). The project picker loads
// another project on the unit. SORT BY TAG and CLEAR UNUSED are runs of the same commands. Each destructive or slow
// one asks once (ui.js confirmBox), saying what it does to the unit's undo: each change is one undo level (SHIFT +
// F1), the pool's swaps and a project load excepted (they forget the levels).
import { FIZZ_TAGS } from "../js/fizz_sysex.js";
import { esc, tagsOf, colOf, DARK_TEXT_TAGS, refusal, confirmBox } from "./ui.js";
import { newFilter, filterHtml, filterClick, listHtml, poolPlaces } from "./presets.js";

export function mountPool(ctx) {
  const { S, $, say, toast, edit, refreshRows, frameOf, projectName, poolSet } = ctx;
  const P = { sel: 0, pick: null, users: new Map(), gen: 0, filling: false, run: false, fizz: new Map(), dragFrom: null, filter: newFilter() };
  const grid = $("p-grid"), mine = $("p-list"), proj = $("p-proj");
  const rows = () => (S.unit ? S.unit.rows : []);
  const pool = () => new Map(rows().filter((r) => r.source === "pool").map((r) => [r.slot, r]));
  const can = () => !!(S.link && !S.busy && S.unit && S.unit.status && !P.run);
  const shown = () => !$("tab-pool").hidden;

  function tile(k, e) {
    const u = P.users.get(k), cls = ["ps"];
    if (P.sel === k) cls.push("sel");
    if (P.fizz.has(k)) cls.push("fizz");
    if (e && P.pick && e.id === P.pick) cls.push("here");
    if (!e) return `<button class="${cls.join(" ")}" data-k="${k}" aria-label="Pool ${k}, empty"><span class="no">${k}</span></button>`;
    const t = tagsOf(e.tags)[0];
    cls.push("full");
    const dots = u && u.steps ? `<span class="use">${"<i></i>".repeat(Math.min(4, u.steps))}</span>` : "";
    return `<button class="${cls.join(" ")}" style="--c:${colOf(e.tags)};--t:${t && DARK_TEXT_TAGS.has(t) ? "#fff" : "#000"}" data-k="${k}" draggable="${can()}" aria-label="Pool ${k}, ${esc(e.name)}${u ? `, ${u.steps} steps` : ""}">` +
      `<span class="no">${k}</span><span class="pn">${esc(e.name)}</span>${dots}</button>`;
  }
  function render() {
    const u = S.unit, pm = pool(), st = u && u.status;
    $("p-count").textContent = u ? `${pm.size} / 128` : "";
    const list = u && u.projects ? u.projects.list.filter((p) => p.there) : [];
    proj.innerHTML = list.map((p) => `<button class="${p.current ? "on" : ""}" data-p="${p.slot}" ${can() || p.current ? "" : "disabled"}>${esc(p.name || `PROJECT ${p.slot}`)}</button>`).join("");
    let html = "";
    for (let k = 1; k <= 128; k++) html += tile(k, pm.get(k));
    grid.innerHTML = html;
    grid.classList.toggle("off", !st);
    $("p-filter").innerHTML = filterHtml(P.filter);
    const rs = rows();
    mine.innerHTML = u ? listHtml(rs, P.filter, { frameOf, places: poolPlaces(rs), on: (r) => P.pick === r.id, drag: () => can() }) : "";
    renderBar();
    if (shown()) fill();
  }
  function renderBar() {
    const e = P.sel && pool().get(P.sel), u = e && P.users.get(P.sel);
    $("p-selname").innerHTML = e ? `<b>${P.sel}</b> ${esc(e.name)}${u ? ` <span class="dots">${"●".repeat(Math.min(6, u.steps))}</span>` : ""}` : "";
    $("p-clear").hidden = !e;
    $("p-clear").disabled = !can();
    $("p-clear").className = "btn small ghost redline";
    $("p-clear").textContent = "CLEAR";
    $("p-sort").disabled = $("p-unused").disabled = !can() || !pool().size;
  }
  // the steps using each sound: USERS, one after another in the background (a job each on the unit, a moment)
  async function fill() {
    if (P.filling || !can()) return;
    P.filling = true;
    const g = P.gen;
    try {
      for (const k of pool().keys()) {
        if (g !== P.gen || !shown() || !can() || S.editing) break;
        if (P.users.has(k)) continue;
        const r = await S.link.users("pool", k);
        if (g !== P.gen) break;
        if (r && r.projects) {
          P.users.set(k, { steps: r.projects.reduce((a, p) => a + p.steps, 0), tracks: r.tracks });
          const el = grid.querySelector(`[data-k="${k}"]`);
          if (el) el.outerHTML = tile(k, pool().get(k));
          if (k === P.sel) renderBar();
        }
      }
    } catch (_) { /* (the next render tries again) */ } finally { P.filling = false; }
  }
  function forget() { P.gen++; P.users.clear(); }
  // after an edit: the lists read again (until: what the edit made shows in them; the page shows it at once)
  async function after(x, msg, until = null) {
    forget();
    if (x && x.status === 0 && msg) toast(msg);
    await refreshRows({ until: x && x.status === 0 ? until : null });
  }
  const at = (rows, k) => rows.find((r) => r.source === "pool" && r.slot === k);
  const presetOf = (id) => rows().find((r) => r.source !== "pool" && r.id === id);
  function bubble(k) {
    P.fizz.set(k, setTimeout(() => { P.fizz.delete(k); const el = grid.querySelector(`[data-k="${k}"]`); if (el) el.classList.remove("fizz"); }, 1100));
    const el = grid.querySelector(`[data-k="${k}"]`);
    if (el) el.classList.add("fizz");
  }
  async function place(k, id) {
    const x = await edit("POOL", (l) => l.poolRef(k, id));
    P.pick = null;
    grid.classList.remove("picking");
    if (x && x.status === 0) { const p = presetOf(id); poolSet(x.a, p || { id, name: "?", tags: 0 }); bubble(x.a); }
    await after(x, "", (rs) => { const e = at(rs, x.a); return !!e && e.id === id; });
  }
  async function swap(a, b) {
    if (a === b) return null;
    const pm = pool(), ea = pm.get(a), eb = pm.get(b);
    const ok = await confirmBox({ title: eb ? `SWAP POOL ${a} ↔ ${b}` : `MOVE POOL ${a} → ${b}`, what: eb ? `${ea ? ea.name : "—"} ↔ ${eb.name}` : ea ? ea.name : "",
      sub: "Every pattern's steps follow (the FM-1 stopped).", go: eb ? "SWAP" : "MOVE", color: "--yellow", undo: "cleared" });
    if (!ok) return null;
    say("");
    const x = await edit("SWAP", (l) => l.swap("pool", a, b));
    if (x && x.status === 0) { P.sel = b; poolSet(a, eb || null); poolSet(b, ea || null); bubble(b); }
    await after(x, "", (rs) => { const na = at(rs, a), nb = at(rs, b); return (na ? na.id : 0) === (eb ? eb.id : 0) && (nb ? nb.id : 0) === (ea ? ea.id : 0); });
    return x;
  }

  grid.addEventListener("click", (ev) => {
    const b = ev.target.closest(".ps");
    if (!b || !can()) return;
    const k = +b.dataset.k;
    if (P.pick) { place(k, P.pick); return; }
    P.sel = P.sel === k ? 0 : k;
    grid.querySelectorAll(".ps.sel").forEach((x) => x.classList.remove("sel"));
    if (P.sel) b.classList.add("sel");
    renderBar();
  });
  grid.addEventListener("keydown", (ev) => {
    const b = ev.target.closest(".ps");
    if (!b) return;
    const k = +b.dataset.k, d = { ArrowLeft: -1, ArrowRight: 1, ArrowUp: -16, ArrowDown: 16 }[ev.key];
    if (!d || k + d < 1 || k + d > 128) return;
    ev.preventDefault();
    if (ev.altKey && pool().get(k) && can()) {          // (ALT + an arrow: the sound moved a place: a swap)
      swap(k, k + d).then(() => { const n = grid.querySelector(`[data-k="${k + d}"]`); if (n) n.focus(); });
      return;
    }
    const n = grid.querySelector(`[data-k="${k + d}"]`);
    if (n) n.focus();
  });
  // drag: a MY preset onto a tile (a reference), a tile onto a tile (a swap)
  const drop = (ev) => ev.target.closest(".ps");
  $("p-filter").addEventListener("click", (ev) => { if (filterClick(P.filter, ev)) render(); });
  grid.addEventListener("dragstart", (ev) => {
    const b = drop(ev);
    if (!b) return;
    P.dragFrom = { tile: +b.dataset.k };
    ev.dataTransfer.effectAllowed = "move";
    ev.dataTransfer.setData("text/plain", `pool ${b.dataset.k}`);
    b.classList.add("lift");
  });
  grid.addEventListener("dragend", (ev) => { const b = drop(ev); if (b) b.classList.remove("lift"); P.dragFrom = null; });
  grid.addEventListener("dragover", (ev) => {
    const b = drop(ev);
    if (!b || !P.dragFrom) return;
    ev.preventDefault();
    grid.querySelectorAll(".ps.target").forEach((x) => x !== b && x.classList.remove("target"));
    b.classList.add("target");
  });
  grid.addEventListener("dragleave", (ev) => { const b = drop(ev); if (b) b.classList.remove("target"); });
  grid.addEventListener("drop", (ev) => {
    const b = drop(ev);
    if (!b || !P.dragFrom) return;
    ev.preventDefault();
    b.classList.remove("target");
    const to = +b.dataset.k, from = P.dragFrom;
    P.dragFrom = null;
    if (from.id) place(to, from.id);
    else if (from.tile) swap(from.tile, to);
  });
  mine.addEventListener("dragstart", (ev) => {
    const b = ev.target.closest(".cas");
    if (!b) return;
    P.dragFrom = { id: +b.dataset.id };
    ev.dataTransfer.effectAllowed = "copy";
    ev.dataTransfer.setData("text/plain", `preset ${b.dataset.id}`);
    b.classList.add("lift");
  });
  mine.addEventListener("dragend", (ev) => { const b = ev.target.closest(".cas"); if (b) b.classList.remove("lift"); P.dragFrom = null; });
  mine.addEventListener("click", (ev) => {              // (a single click: picked; the next tile clicked takes it)
    const b = ev.target.closest(".cas");
    if (!b || !can()) return;
    const id = +b.dataset.id;
    P.pick = P.pick === id ? null : id;
    mine.querySelectorAll(".cas").forEach((x) => x.setAttribute("aria-pressed", String(+x.dataset.id === P.pick)));
    grid.classList.toggle("picking", !!P.pick);
    const pm = pool();
    grid.querySelectorAll(".ps").forEach((x) => { const e = pm.get(+x.dataset.k); x.classList.toggle("here", !!(P.pick && e && e.id === P.pick)); });
  });
  proj.addEventListener("click", async (ev) => {
    const b = ev.target.closest("[data-p]");
    if (!b || !can()) return;
    const p = +b.dataset.p, cur = S.unit.projects && S.unit.projects.current;
    if (p === cur) return;
    const ok = await confirmBox({ title: "LOAD PROJECT", what: projectName(p), go: "LOAD", color: "--green", undo: "cleared",
      sub: `The FM-1 switches from ${esc(projectName(cur))}.` });
    if (!ok) return;
    const x = await edit("LOAD", (l) => l.project("load", p));
    P.sel = 0;
    forget();
    await refreshRows({ projects: true });
    if (x && x.status === 0) toast(`${projectName(p)} on the FM-1`);
  });
  $("p-clear").addEventListener("click", async () => {
    const k = P.sel, e = k && pool().get(k);
    if (!k || !e || !can()) return;
    const u = P.users.get(k);
    const ok = await confirmBox({ title: `CLEAR POOL ${k}`, what: e.name, go: "CLEAR", undo: "kept",
      sub: u && u.steps ? `${u.steps} step${u.steps === 1 ? "" : "s"} use it: they go back to their track's sound.` : "Its steps go back to their track's sound." });
    if (!ok) return;
    const x = await edit("CLEAR", (l) => l.remove("pool", k, { confirm: true }));
    if (x && x.status === 0) { P.sel = 0; poolSet(k, null); }
    await after(x, `POOL ${k} cleared`, (rs) => !at(rs, k));
  });
  // SORT BY TAG: the sounds to the front, in FIZZ's tag order then by name: a swap each that is out of place
  $("p-sort").addEventListener("click", async () => {
    if (!can()) return;
    const pm = pool(), want = [...pm.values()].sort((a, b) => {
      const ta = tagsOf(a.tags)[0], tb = tagsOf(b.tags)[0];
      const ia = ta ? FIZZ_TAGS.indexOf(ta) : 9, ib = tb ? FIZZ_TAGS.indexOf(tb) : 9;
      return ia - ib || a.name.localeCompare(b.name) || a.slot - b.slot;
    }).map((e) => e.slot);
    const moves = want.filter((s, i) => s !== i + 1).length;
    if (!moves) { toast("Sorted already"); return; }
    const go = await confirmBox({ title: "SORT BY TAG", what: `${want.length} sounds`, go: "SORT", color: "--yellow", undo: "cleared",
      sub: `Up to ${moves} swaps, a moment each; every pattern's steps follow (the FM-1 stopped).` });
    if (!go || !can()) return;
    const at = new Map(want.map((s) => [s, s])), who = new Map([...pm.keys()].map((s) => [s, s]));   // (sound -> place, place -> sound)
    P.run = true; render();
    let n = 0, ok = true;
    try {
      for (let i = 0; i < want.length && ok; i++) {
        const place2 = i + 1, from = at.get(want[i]);
        if (from === place2) continue;
        say(`SORTING · ${i + 1} / ${want.length}`, "--green");
        const x = await S.link.swap("pool", from, place2);
        ok = x && x.status === 0;
        if (!ok) { say(refusal(x, "SORT BY TAG"), "--orange"); break; }
        n++;
        const other = who.get(place2);
        at.set(want[i], place2); who.set(place2, want[i]);
        if (other !== undefined) { at.set(other, from); who.set(from, other); } else who.delete(from);
      }
    } finally { P.run = false; }
    if (ok) say("");
    P.sel = 0;
    await after({ status: ok ? 0 : 1 }, ok ? `Sorted · ${n} moves` : "");
  });
  // CLEAR UNUSED: every sound no step locks and no track plays (DELETE without confirm: the unit says IN USE otherwise)
  $("p-unused").addEventListener("click", async () => {
    if (!can()) return;
    const ok = await confirmBox({ title: "CLEAR UNUSED", what: `${pool().size} sounds looked at`, go: "CLEAR UNUSED", undo: "kept",
      sub: "Every pool sound no step locks and no track plays comes out of the pool." });
    if (!ok || !can()) return;
    P.run = true; render();
    let n = 0;
    try {
      for (const k of [...pool().keys()]) {
        const x = await S.link.remove("pool", k);
        if (x && x.status === 0) n++;
        else if (!x || x.status !== 10) { say(refusal(x, "CLEAR UNUSED"), "--orange"); break; }
      }
    } finally { P.run = false; }
    P.sel = 0;
    await after({ status: 0 }, `${n} cleared`);
  });

  return {
    render,
    show() { forget(); render(); },
    fizz: (k) => bubble(k),
    forget,
  };
}
