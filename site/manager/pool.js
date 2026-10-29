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
import { FIZZ_TAGS, parseDump } from "../js/fizz_sysex.js";
import { esc, tagsOf, colOf, refusal, confirmBox, midiChip, tagIcon } from "./ui.js";
import { isMidiBlob } from "./sound.js";
import { newFilter, filterHtml, filterClick, listHtml, poolPlaces } from "./presets.js";

const DN = '<svg class="dn" viewBox="0 0 24 24" aria-hidden="true"><path d="M12 5v14M5 12l7 7 7-7"/></svg>';   // (where a move goes)

export function mountPool(ctx) {
  const { S, $, say, toast, editSure, refreshRows, frameOf, projectName, poolSet } = ctx;
  const P = { sel: 0, pick: null, users: new Map(), gen: 0, filling: false, run: false, fizz: new Map(), dragFrom: null, filter: newFilter(), move: null };
  const grid = $("p-grid"), mine = $("p-list"), proj = $("p-proj");
  const rows = () => (S.unit ? S.unit.rows : []);
  const pool = () => new Map(rows().filter((r) => r.source === "pool").map((r) => [r.slot, r]));
  const can = () => !!(S.link && !S.busy && S.unit && S.unit.status && !P.run);
  // (FIZZ MIDI (e)) a pool sound that is a MIDI track's: its preset's DUMP, when read
  const midiOf = (e) => {
    const r = e && e.id && rows().find((q) => (q.source === "mine" || q.source === "factory") && q.id === e.id), f = r && frameOf(r), d = f && parseDump(f);
    return !!d && isMidiBlob(d.blob);
  };
  const shown = () => !$("tab-pool").hidden;

  // a place (FIZZ NEON, the board PhonePool): its number in HUD at the corner; a sound's in its tag's colour with a
  // ring, its name where the tile is wide enough, a glowing dot when steps use it
  function tile(k, e) {
    const u = P.users.get(k), cls = ["ps"];
    if (P.sel === k) cls.push("sel");
    if (P.fizz.has(k)) cls.push("fizz");
    if (e && P.pick && e.id === P.pick) cls.push("here");
    if (P.move && P.move.a === k) cls.push("moving");
    const dn = P.move && P.move.b === k ? DN : "";
    if (dn) cls.push("mvto");
    if (!e) return `<button class="${cls.join(" ")}" data-k="${k}" aria-label="Pool ${k}, empty"><span class="no hud">${k}</span>${dn}</button>`;
    cls.push("full");
    const dot = u && u.steps ? '<i class="use" aria-hidden="true"></i>' : "";
    return `<button class="${cls.join(" ")}" style="--c:${colOf(e.tags)};--i:${k % 16}" data-k="${k}" draggable="${can()}" aria-label="Pool ${k}, ${esc(e.name)}${u ? `, ${u.steps} steps` : ""}">` +
      `<span class="no hud">${k}</span>${tagsOf(e.tags)[0] ? tagIcon(tagsOf(e.tags)[0]) : '<span class="ring" aria-hidden="true"></span>'}<span class="nm">${esc(e.name)}</span>${midiChip(midiOf(e))}${dot}${dn}</button>`;
  }
  function render() {
    const u = S.unit, pm = pool(), st = u && u.status;
    $("p-count").innerHTML = u ? `${pm.size}<small>/128</small>` : "";
    // the eight projects as round numbers (the one on the FM-1 lit; an empty place dim), its name big
    const list = u && u.projects ? u.projects.list : [], cur = list.find((p) => p.current);
    proj.innerHTML = list.map((p) => `<button class="${p.current ? "on" : ""}${p.there ? "" : " none"}" data-p="${p.slot}" aria-pressed="${!!p.current}" aria-label="${esc(p.there ? p.name || `PROJECT ${p.slot}` : `Project ${p.slot}, empty`)}" ${p.there && (can() || p.current) ? "" : "disabled"}><span class="hud">${p.slot}</span></button>`).join("");
    $("p-pname").textContent = cur ? cur.name || `PROJECT ${cur.slot}` : "";
    $("p-pno").textContent = cur && cur.name && cur.name !== `PROJECT ${cur.slot}` ? `PROJECT ${cur.slot}` : "";
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
    // the place picked: its ring and number, the name, how many steps use it
    const sn = $("p-selname");
    sn.style.setProperty("--c", e ? colOf(e.tags) : "var(--green)");
    sn.classList.toggle("on", !!e);
    sn.innerHTML = e ? `<span class="pring"><span class="hud">${P.sel}</span></span><b class="pselnm">${esc(e.name)}</b>${midiOf(e) ? midiChip(true) : ""}` +
      `${u ? `<span class="psteps"><span class="num">${u.steps}</span><span class="lbl">steps</span></span>` : ""}` : "";
    $("p-clear").hidden = !e;
    $("p-clear").disabled = !can();
    $("p-clear").className = "ol redline";
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
  // (editSure: its reply lost, the lists read say whether it took; every tab drawn again)
  async function place(k, id) {
    P.pick = null;
    grid.classList.remove("picking");
    forget();
    await editSure("POOL", (l) => l.poolRef(k, id), { took: (rs) => { const e = at(rs, k); return !!e && e.id === id; },
      shown: (x) => { const p = presetOf(id); poolSet(x.a, p || { id, name: "?", tags: 0 }); bubble(x.a); } });
  }
  // the moving card: the sound's ring and name, where it goes, the steps that lock it (when known), what it does to the
  // unit's undo (cleared, as every pool swap); CANCEL / MOVE TO (SWAP onto a sound); ESC, ENTER. -> Promise<boolean>
  let mvDone = null;
  const marks = () => grid.querySelectorAll(".ps").forEach((x) => {
    const k = +x.dataset.k, d = !!P.move && P.move.b === k, dn = x.querySelector(".dn");
    x.classList.toggle("moving", !!P.move && P.move.a === k);
    x.classList.toggle("mvto", d);
    if (d && !dn) x.insertAdjacentHTML("beforeend", DN);
    else if (!d && dn) dn.remove();
  });
  function askMove(a, b) {
    if (mvDone) mvDone(false);
    const pm = pool(), ea = pm.get(a), eb = pm.get(b), ua = P.users.get(a), ub = P.users.get(b), card = $("p-move");
    const steps = eb ? (ua && ub ? ua.steps + ub.steps : null) : ua ? ua.steps : null;
    card.innerHTML = `<div class="mvtop"><span class="pring"><span class="hud">${a}</span></span>
      <div class="mvtxt"><b class="mvnm">${esc(ea ? ea.name : "—")}${eb ? ` ↔ ${esc(eb.name)}` : ""}</b>
        <span class="mvline">${eb ? `Swapping ${a} ↔ <b>${b}</b>. Their locks follow.` : `Moving ${a} → <b>${b}</b>. Its locks follow.`}</span>
        <span class="mvundo hud">UNDO ON THE FM-1 CLEARS</span></div>
      ${steps !== null ? `<span class="mvsteps"><span class="num">${steps}</span><span class="lbl">steps</span></span>` : ""}</div>
      <div class="mvacts"><button class="ol" id="p-mv-no">CANCEL</button><button class="arc breathe" id="p-mv-go" style="--c:var(--green)">${eb ? "SWAP" : `MOVE TO ${b}`}</button></div>`;
    P.move = { a, b };
    marks();
    card.hidden = false;
    $("p-mv-go").focus({ preventScroll: true });
    return new Promise((res) => {
      const end = (v) => { mvDone = null; P.move = null; card.hidden = true; marks(); document.removeEventListener("keydown", key, true); res(v); };
      const key = (ev) => {
        if (ev.key === "Escape") { ev.preventDefault(); ev.stopPropagation(); end(false); }
        else if (ev.key === "Enter" && document.activeElement !== $("p-mv-no")) { ev.preventDefault(); ev.stopPropagation(); end(true); }
      };
      mvDone = end;
      document.addEventListener("keydown", key, true);
      $("p-mv-go").onclick = () => end(true);
      $("p-mv-no").onclick = () => end(false);
    });
  }
  async function swap(a, b) {
    if (a === b) return null;
    const pm = pool(), ea = pm.get(a), eb = pm.get(b);
    const ok = await askMove(a, b);
    if (!ok) return null;
    say("");
    forget();
    return editSure("SWAP", (l) => l.swap("pool", a, b), { shown: () => { P.sel = b; poolSet(a, eb || null); poolSet(b, ea || null); bubble(b); },
      took: (rs) => { const na = at(rs, a), nb = at(rs, b); return (na ? na.id : 0) === (eb ? eb.id : 0) && (nb ? nb.id : 0) === (ea ? ea.id : 0); } });
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
    const e = P.sel && pool().get(P.sel);              // (the owner, 2026-10-27: heard on the computer keys)
    if (e && e.id && ctx.previewId) ctx.previewId(e.id);
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
    if (P.pick && ctx.previewId) ctx.previewId(id);
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
    P.sel = 0;
    forget();
    const x = await editSure("LOAD", (l) => l.project("load", p), { projects: true, took: (rs, pj) => !!pj && pj.current === p });
    if (x && x.status === 0) toast(`${projectName(p)} on the FM-1`);
  });
  $("p-clear").addEventListener("click", async () => {
    const k = P.sel, e = k && pool().get(k);
    if (!k || !e || !can()) return;
    const u = P.users.get(k);
    const ok = await confirmBox({ title: `CLEAR POOL ${k}`, what: e.name, go: "CLEAR", undo: "kept",
      sub: u && u.steps ? `${u.steps} step${u.steps === 1 ? "" : "s"} use it: they go back to their track's sound.` : "Its steps go back to their track's sound." });
    if (!ok) return;
    forget();
    const x = await editSure("CLEAR", (l) => l.remove("pool", k, { confirm: true }), { took: (rs) => !at(rs, k), shown: () => { P.sel = 0; poolSet(k, null); } });
    if (x && x.status === 0) { P.sel = 0; toast(`POOL ${k} cleared`); }
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
        const x = await S.link.swap("pool", from, place2).catch(() => null);   // (no answer: stopped, the lists read)
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
        const x = await S.link.remove("pool", k).catch(() => null);
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
    hide() { if (mvDone) mvDone(false); },
    fizz: (k) => bubble(k),
    forget,
  };
}
