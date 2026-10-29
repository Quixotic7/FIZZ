// SPDX-License-Identifier: GPL-3.0-only
// The Manager's PROJECTS tab (the owner's request of 2026-10-26; mockups/fizz-projects.html, approved 2026-10-27):
// the FM-1's slots on the left (eight; four in the Bluetooth package) (LOAD, RENAME, COPY TO, MOVE TO, KEEP, DELETE), a project's 64 patterns on the
// right (click, SHIFT-click a range, CMD-click one more; COPY, PASTE, CLEAR, .fizzpat out and in), or the projects KEPT
// in this browser (.fizzprj in and out, LOAD INTO a slot).
//   - The slots' moves are the FM-1's own commands (PROJECT, RENAME): done at once.
//   - The patterns are the store read in the browser (projects.js): pasted, cleared, a kept project loaded, each
//     change waits in the yellow bar (UNDO, DISCARD) and goes to the FM-1 in one write of its store (the restore),
//     after the store as it was is kept in BACKUP's list. The store is read again first if the FM-1 changed it since.
//   mountProjects(ctx) -> { show(), render() }
//   ctx: S, $, log, say, toast, edit(what, fn), refreshRows(o), readStore(label) -> image | null, writeStore(image,
//        before) -> true | false, kv: {get, put}, download(bytes, name), fizz(), pkg() -> {nproj, nlib, name},
//        viaBle() (the Bluetooth MIDI port: WRITE, the store's restore, needs the USB cable), NEEDS_USB
// A project from the other package comes in as a .fizzprj (KEEP there, LOAD INTO here): its MY presets come along;
// those MY PRESETS has no room for stay in it as its own copies (projects.js derefPat), so it sounds the same.
import { modelOf, imageOf, cloneModel, projects, project, patName, copyPatterns, pastePatterns, clearPatterns, keepProject,
  loadProject, renameKept, renameProject, cleanName, makeFizzprj, readFizzprj, makeFizzpat, readFizzpat, patFileName, prjFileName, NPAT, limits } from "./projects.js";
import { esc, confirmBox } from "./ui.js";

const TC = ["--blue", "--green", "--cream", "--orange", "--pink", "--violet"];
const IC = {
  load: '<svg viewBox="0 0 20 20"><path d="M6 4l10 6-10 6z" fill="currentColor" stroke="none"/></svg>',
  down: '<svg viewBox="0 0 20 20"><path d="M10 3v10M5 9l5 5 5-5M3 17h14"/></svg>',
  up: '<svg viewBox="0 0 20 20"><path d="M10 14V4M5 8l5-5 5 5M3 17h14"/></svg>',
  copy: '<svg viewBox="0 0 20 20"><rect x="7" y="7" width="10" height="10" rx="2"/><path d="M13 7V5a2 2 0 0 0-2-2H5a2 2 0 0 0-2 2v6a2 2 0 0 0 2 2h2"/></svg>',
  paste: '<svg viewBox="0 0 20 20"><rect x="4" y="4" width="12" height="14" rx="2"/><path d="M8 2h4v4H8z"/></svg>',
  move: '<svg viewBox="0 0 20 20"><path d="M3 10h14M13 6l4 4-4 4"/></svg>',
  ren: '<svg viewBox="0 0 20 20"><path d="M4 16l2-6 8-8 4 4-8 8z"/></svg>',
  keep: '<svg viewBox="0 0 20 20"><rect x="3" y="4" width="14" height="12" rx="2"/><circle cx="7.5" cy="10" r="2"/><circle cx="12.5" cy="10" r="2"/></svg>',
  del: '<svg viewBox="0 0 20 20"><path d="M4 6h12M8 6V4h4v2M6 6l1 11h6l1-11"/></svg>',
  clear: '<svg viewBox="0 0 20 20"><path d="M5 5l10 10M15 5L5 15"/></svg>',
};
const range = (ks) => { const a = [...ks].sort((x, y) => x - y); return !a.length ? "" : a.length === 1 ? patName(a[0]) : a[a.length - 1] - a[0] === a.length - 1 ? `${patName(a[0])}–${patName(a[a.length - 1])}` : a.length <= 3 ? a.map(patName).join(" ") : `${patName(a[0])} +${a.length - 1}`; };
const ago = (t) => { const d = (Date.now() - t) / 86400000; return d < 1 ? "today" : d < 2 ? "yesterday" : d < 14 ? `${Math.floor(d)} days` : d < 60 ? `${Math.floor(d / 7)} weeks` : `${Math.floor(d / 30)} months`; };

export function mountProjects(ctx) {
  const { S, $, log, say, toast } = ctx;
  const J = { sel: 0, pats: new Set(), anchor: -1, view: "patterns", clip: null, base: null, ops: [], cache: null, kept: null,
    ksel: -1, arm: null, renaming: false, reading: false };
  const link = () => S.link;
  const nproj = () => (J.base ? limits(model()).nproj : ctx.pkg ? ctx.pkg().nproj : 8);
  const viaBle = () => !!(ctx.viaBle && ctx.viaBle());
  const on = () => !!(S.link && S.unit && S.unit.status && !S.busy);
  const list = () => (S.unit && S.unit.projects ? S.unit.projects.list : []);
  const model = () => {
    if (!J.base) return null;
    if (!J.cache) {
      const m = cloneModel(J.base.model);
      for (const op of J.ops) apply(m, op);
      J.cache = m;
    }
    return J.cache;
  };
  function apply(m, op) {
    if (op.kind === "paste") return pastePatterns(m, op.q, op.j, op.clip);
    if (op.kind === "clear") return clearPatterns(m, op.q, op.ks);
    if (op.kind === "load") return loadProject(m, op.q, op.kept);
    return null;
  }
  function push(op) {
    const m = cloneModel(model());
    try { op.result = apply(m, op); } catch (e) { say(esc(refuse(e)), "--orange"); return false; }
    J.ops.push(op);
    J.cache = m;
    render();
    return true;
  }
  const refuse = (e) => (e.code === "poolfull" ? "The pool there is full (128): clear some of its sounds first."
    : e.code === "full" ? "That does not fit the FM-1's store: clear some patterns or projects first."
      : e.code === "range" ? (/slot/.test(e.message) ? `This FM-1 has ${nproj()} project slots.` : "The patterns run past D16 from there: pick an earlier pattern.")
        : e.code === "demo" ? "A demo project's first three patterns stay the demo's here." : e.message);

  // ---------------------------------------------------------------- the store read (and read again when it changed)
  const stale = () => !J.base || !S.unit || !S.unit.status || J.base.written !== S.unit.status.written;
  async function read({ force = false } = {}) {
    if (J.reading || !on() || (!force && !stale())) return !!J.base;
    J.reading = true;
    render();
    try {
      const st = S.unit.status;
      if (st && st.dirty && !st.playing) await link().flush().catch(() => null);   // (the open project's latest written first)
      const image = await ctx.readStore("READING THE PROJECTS");
      if (!image) return false;
      const st2 = await link().status().catch(() => null);
      if (st2) S.unit.status = st2;
      J.base = { model: modelOf(image), image, written: st2 ? st2.written : null };
      J.cache = null;
      return true;
    } catch (e) { say(`The projects could not be read: ${esc(e.message)}`, "--red"); return false; }
    finally { J.reading = false; render(); }
  }

  // ---------------------------------------------------------------- the slots
  function slotsHtml() {
    const m = model(), L = list();
    if (!L.length && !m) return "";                   // (no FM-1: nothing to show)
    return Array.from({ length: nproj() }, (_, p) => {
      const e = L[p], there = e ? e.there : m ? project(m, p).there : false;
      const info = m ? project(m, p) : null, changed = J.ops.some((o) => o.q === p);
      const target = J.arm && J.arm.from !== p;
      if (!there) return `<button class="slot empty ${target ? "target" : ""} ${J.sel === p && !J.arm ? "sel" : ""}" data-p="${p}" aria-pressed="${J.sel === p}"><span class="no hud">${p + 1}</span><span class="meta">${J.arm && J.arm.kind === "load" && target ? esc(J.arm.name) : "empty"}</span></button>`;
      const name = (e && e.name) || (info && info.name) || `PROJECT ${p + 1}`;
      const n = info ? info.used : null;
      const dots = info ? `<span class="dots">${info.pats.slice(0, 32).map((x) => `<i class="${x ? "u" : ""}"></i>`).join("")}</span>` : "";
      const nameEl = J.renaming && J.sel === p ? `<input class="slotname" id="pj-rename" maxlength="12" value="${esc(name)}" spellcheck="false" autocomplete="off">` : `<div class="sname">${esc(name)}</div>`;
      return `<button class="slot ${e && e.current ? "cur" : ""} ${J.sel === p && !J.arm ? "sel" : ""} ${target ? "target" : ""}" data-p="${p}" aria-pressed="${J.sel === p}"><span class="no hud">${p + 1}</span>` +
        `<div class="pt0">${nameEl}<div class="meta">${e && e.current ? '<span class="led" aria-hidden="true"></span>' : ""}${n !== null ? `${n} pattern${n === 1 ? "" : "s"}` : ""}${e && e.current ? `${n !== null ? " · " : ""}open` : ""}</div></div>${changed ? '<span class="chg"></span>' : ""}${dots}</button>`;
    }).join("");
  }
  function actsHtml() {
    const L = list(), e = L[J.sel], there = e && e.there, idle = on() && !J.ops.length && !J.arm;
    const b = (act, icon, label, ok, cls = "") => `<button class="${cls === "go" ? "arc" : "ol"} ${cls}" data-act="${act}" ${ok ? "" : "disabled"}>${icon}${label}</button>`;
    if (J.arm) return `<div class="armed">${J.arm.kind === "load" ? IC.load : J.arm.kind === "move" ? IC.move : IC.copy}<b>${esc(J.arm.name)}</b></div>${b("disarm", IC.clear, "CANCEL", true)}`;
    // (LOAD the screen's fill while nothing waits to be written; then WRITE in the yellow bar is)
    return b("load", IC.load, "LOAD", idle && there && !e.current, J.ops.length ? "cyan" : "go") + b("rename", IC.ren, "RENAME", idle && there) + b("copyto", IC.copy, "COPY TO", idle && there) +
      b("moveto", IC.move, "MOVE TO", idle && there) + b("keep", IC.keep, "KEEP", on() && there && !!J.base, "champ") + b("delete", IC.del, "DELETE", idle && there, "red");
  }

  // ---------------------------------------------------------------- the patterns
  function tile(s, k) {
    const cls = ["pt", s ? "used" : "", J.pats.has(k) ? "sel" : "", J.ops.some((o) => o.q === J.sel && o.result && o.result.at && o.result.at.includes(k)) ? "new" : ""];
    const rows = TC.map((c, t) => `<div class="tr">${Array.from({ length: 16 }, (_, x) => `<i${s && s.rows[t] && s.rows[t][x] ? ` style="background:var(${c})"` : ""}></i>`).join("")}</div>`).join("");
    const demo = TC.map(() => `<div class="tr demo">${"<i></i>".repeat(16)}</div>`).join("");   // (the demos' own, compiled in)
    return `<button class="${cls.join(" ")}" data-k="${k}" aria-pressed="${J.pats.has(k)}"><span class="no hud">${patName(k)}</span>${s && s.demo ? demo : rows}</button>`;
  }
  function gridHtml() {
    const m = model();
    if (!m) return `<div class="pjwait">${J.reading ? "" : on() ? `<button class="ol cyan" id="pj-read">${IC.down}READ</button>` : ""}</div>`;
    const pr = project(m, J.sel);
    if (!pr.there) return '<div class="pjwait"></div>';
    return [0, 1, 2, 3].map((b) => `<div class="bankrow"><span class="bank hud">${"ABCD"[b]}</span><div class="pgrid">${Array.from({ length: 16 }, (_, x) => tile(pr.pats[b * 16 + x], b * 16 + x)).join("")}</div></div>`).join("");
  }
  function toolsHtml() {
    const m = model(), ok = !!m && project(m, J.sel).there && !S.busy, n = J.pats.size;
    const b = (act, icon, label, en, cls = "") => `<button class="ol ${cls}" data-act="${act}" ${en ? "" : "disabled"}>${icon}${label}</button>`;
    return `${n ? `<span class="selname hud">${range(J.pats)}</span>` : ""}` + b("copy", IC.copy, "COPY", ok && n, "cyan") +
      b("paste", IC.paste, "PASTE", ok && n && !!J.clip, J.clip ? "yellow" : "") + b("clear", IC.clear, "CLEAR", ok && n) +
      b("fizzpat", IC.down, ".fizzpat", ok && n, "fl") + b("import", IC.up, "IMPORT", ok);
  }
  function pendingHtml() {
    if (!J.ops.length) return "";
    const last = J.ops[J.ops.length - 1], L = list();
    const pname = (q) => (L[q] && L[q].name) || (model() ? project(model(), q).name : "") || `PROJECT ${q + 1}`;
    const what = last.kind === "paste" ? `${last.result.at.length} pattern${last.result.at.length === 1 ? "" : "s"} into ${esc(pname(last.q))}`
      : last.kind === "clear" ? `${last.ks.length} cleared in ${esc(pname(last.q))}` : `${esc(last.kept.name)} into slot ${last.q + 1}`;
    const r = last.result;
    const extra = r && (r.poolAdded || r.presetsAdded || r.copies)
      ? [r.poolAdded ? `+${r.poolAdded} pool` : "", r.presetsAdded ? `+${r.presetsAdded} MY PRESETS` : "",
        r.copies ? `${r.copies} sound${r.copies === 1 ? "" : "s"} kept as the project's own (MY PRESETS full)` : ""].filter(Boolean).join(" · ") : "";
    return `<span class="cnt num">${J.ops.length}</span><div class="what">${what}${extra ? `<small>${extra}</small>` : ""}</div>` +
      `<span class="pacts"><button class="ol yellow" data-act="undo">UNDO</button><button class="ol" data-act="discard">DISCARD</button>` +
      (viaBle() ? `<button class="arc breathe" data-act="usb" style="--c:var(--orange)">${IC.down}WRITE NEEDS USB</button>`
        : `<button class="arc breathe" data-act="write" style="--c:var(--yellow)" ${on() ? "" : "disabled"}>${IC.down}WRITE TO ${S.mode === "browser" ? "BROWSER" : "FM-1"}</button>`) + "</span>";
  }

  // ---------------------------------------------------------------- kept in this browser
  async function keptList() { if (!J.kept) J.kept = (await ctx.kv.get("keptProjects")) || []; return J.kept; }
  const saveKept = () => ctx.kv.put("keptProjects", J.kept);
  function keptHtml() {
    const K = J.kept || [];
    return `<div class="kgrid">${K.map((k, i) => `<button class="bp ${J.ksel === i ? "sel" : ""}" data-i="${i}" aria-pressed="${J.ksel === i}"><span class="bpt"><span class="sname">${esc(k.name)}</span><span class="ago">${ago(k.date)}</span></span>` +
      `<span class="meta">${k.pats} pattern${k.pats === 1 ? "" : "s"} · ${k.sounds} sound${k.sounds === 1 ? "" : "s"}</span><span class="snd">${(k.colours || []).map((c) => `<i style="background:var(${c})"></i>`).join("")}</span></button>`).join("")}</div>` +
      `<label class="drop" for="pj-file">${IC.up}.fizzprj</label>`;
  }
  function keptTools() {
    const k = J.kept && J.kept[J.ksel];
    const b = (act, icon, label, en, cls = "") => `<button class="ol ${cls}" data-act="${act}" ${en ? "" : "disabled"}>${icon}${label}</button>`;
    return b("kload", IC.load, "LOAD INTO", !!k && on() && !!J.base, "champ") + b("kfile", IC.down, ".fizzprj", !!k) + b("kren", IC.ren, "RENAME", !!k) +
      b("kdel", IC.del, "DELETE", !!k, "red fl");
  }

  // ---------------------------------------------------------------- render
  function render() {
    if ($("tab-projects").hidden) return;
    const L = list(), m = model(), K = J.kept || [];
    $("pj-n").textContent = L.length ? `${L.filter((x) => x.there).length} / ${nproj()}` : "";
    $("pj-on").textContent = S.mode === "browser" ? "IN THE BROWSER'S FIZZ" : "ON THE FM-1";
    const st = S.unit && S.unit.status;
    const pu = st && st.prj[1] ? st.prj[0] / st.prj[1] : -1;   // (the projects' store: 12 LED cells and its %)
    $("pj-use").innerHTML = pu >= 0 ? `<span class="cells">${Array.from({ length: 12 }, (_, i) => `<i class="${(i + 0.5) / 12 < pu ? "u" : ""}"></i>`).join("")}</span>${Math.round(pu * 100)}<small>%</small>` : "";
    $("pj-slots").innerHTML = slotsHtml();
    $("pj-acts").innerHTML = actsHtml();
    const kept = J.view === "kept";
    $("pj-view").querySelectorAll("button").forEach((b) => b.classList.toggle("on", b.dataset.v === J.view));
    $("pj-view").style.setProperty("--c", kept ? "var(--champagne)" : "var(--cyan)");
    $("pj-kn").textContent = K.length ? String(K.length) : "";
    const pr = m ? project(m, J.sel) : null, e = L[J.sel];
    $("pj-name").textContent = kept ? "IN THIS BROWSER" : (e && e.there && e.name) || (pr && pr.name) || (e && !e.there ? `SLOT ${J.sel + 1}` : "");
    $("pj-count").innerHTML = !kept && pr && pr.there ? `<b>${pr.used}</b>/${NPAT}` : "";
    const c = $("pj-clip");
    c.hidden = !J.clip || kept;
    if (J.clip) c.innerHTML = `${IC.copy}${J.clip.pats.length} <small>${esc(J.clip.from)} ${esc(range(J.clip.pats.map((x) => x.k)))}</small>`;
    $("pj-grid").hidden = kept;
    $("pj-kept").hidden = !kept;
    if (kept) $("pj-kept").innerHTML = keptHtml(); else $("pj-grid").innerHTML = gridHtml();
    $("pj-tools").innerHTML = kept ? keptTools() : toolsHtml();
    const pd = $("pj-pending");
    pd.hidden = !J.ops.length;
    pd.innerHTML = pendingHtml();
    const ri = $("pj-rename");
    if (ri) { ri.focus(); ri.select(); }
  }

  // ---------------------------------------------------------------- the slots' actions (the FM-1's own commands)
  // (ctx.editSure: the projects read again after each, until they show it; a reply lost, they say whether it took)
  const sure = (what, fn, took) => ctx.editSure(what, fn, { projects: true, took: (rs, pj) => !!pj && took(pj.list) });
  async function after(what, { reread = false } = {}) {
    J.cache = null;
    render();
    if (what) toast(what, "--cyan");
    if (reread && J.base && !J.ops.length) await read({ force: true });   // (the patterns shown are the store's: read again)
  }
  async function slotAct(act) {
    const p = J.sel, L = list(), e = L[p];
    if (act === "load") {
      const x = await sure("LOAD", (l) => l.project("load", p + 1), (pl) => !!pl[p] && pl[p].current);
      if (x && x.status === 0) await after(`${e.name} open on the ${S.mode === "browser" ? "browser's FIZZ" : "FM-1"}`); else render();
    } else if (act === "rename") { J.renaming = true; render(); }
    else if (act === "copyto" || act === "moveto") { J.arm = { kind: act === "copyto" ? "copy" : "move", from: p, name: e.name }; render(); }
    else if (act === "disarm") { J.arm = null; render(); }
    else if (act === "keep") {
      const kept = keepProject(model(), p);
      const file = makeFizzprj(kept, { fizz: ctx.fizz() });
      (await keptList()).unshift({ id: Date.now(), name: kept.name, date: Date.now(), pats: kept.pats, sounds: kept.sounds, colours: kept.colours, bytes: file });
      await saveKept();
      toast(`${kept.name} kept in this browser`, "--champagne");
      render();
    } else if (act === "delete") {
      const ok = await confirmBox({ title: "DELETE PROJECT", what: e.name, go: "DELETE", sub: `Slot ${p + 1} on the ${S.mode === "browser" ? "browser's FIZZ" : "FM-1"}. KEEP it first to have a copy here.` });
      if (!ok) return;
      const x = await sure("DELETE", (l) => l.project("delete", p + 1), (pl) => !!pl[p] && !pl[p].there);
      if (x && x.status === 0) await after(`${e.name} deleted`, { reread: true }); else render();
    }
  }
  async function armedTo(q) {
    const a = J.arm, L = list();
    J.arm = null;
    if (q === a.from) { render(); return; }
    if (L[q] && L[q].there) {
      const ok = await confirmBox({ title: a.kind === "load" ? "LOAD INTO" : a.kind === "move" ? "MOVE ONTO" : "COPY ONTO", what: L[q].name, go: "REPLACE", sub: `Slot ${q + 1}'s project goes.` });
      if (!ok) { render(); return; }
    }
    if (a.kind === "load") {
      const kept = await readFizzprj(J.kept[a.i].bytes, J.kept[a.i].name).catch((e) => { say(esc(e.message), "--red"); return null; });
      if (kept && push({ kind: "load", q, kept })) { J.sel = q; render(); }
      return;
    }
    const was = !!(L[q] && L[q].there);
    const x = await sure(a.kind === "move" ? "MOVE" : "COPY", (l) => l.project("copy", a.from + 1, q + 1), (pl) => !!pl[q] && pl[q].there && (!was || pl[q].name === a.name));
    if (!x || x.status !== 0) { render(); return; }
    if (a.kind === "move") {
      const y = await sure("MOVE", (l) => l.project("delete", a.from + 1), (pl) => !!pl[a.from] && !pl[a.from].there);
      if (!y || y.status !== 0) say(`${esc(a.name)} was copied to slot ${q + 1}; slot ${a.from + 1} kept it too.`, "--orange");
    }
    J.sel = q;
    await after(`${a.name} ${a.kind === "move" ? "moved" : "copied"} to slot ${q + 1}`, { reread: true });
  }
  async function renameDone(commit) {
    const i = $("pj-rename"), p = J.sel, e = list()[p];
    J.renaming = false;
    const name = i ? cleanName(i.value) : "";
    if (!commit || !name || !e || name === e.name) { render(); return; }
    const old = e.name;
    e.name = name;                                     // (shown at once; the projects read again say what the unit has)
    render();
    // (a name is safe sent twice: its reply lost and the unit without it, sent once more)
    const x = await ctx.editSure("RENAME", (l) => l.rename("project", p + 1, name), { again: true, projects: true,
      took: (rs, pj) => !!pj && !!pj.list[p] && pj.list[p].name === name });
    if (x && x.status === 0) {
      if (J.base) renameProject(J.base.model, p, name);
      await after(`Slot ${p + 1}: ${name}`);
    } else {
      if (list()[p] === e) e.name = old;               // (the projects not read again: the old name back)
      render();
    }
  }

  // ---------------------------------------------------------------- the patterns' actions (wait for WRITE)
  async function patAct(act) {
    const m = model(), ks = [...J.pats].sort((a, b) => a - b), pr = m && project(m, J.sel);
    if (!pr) return;
    if (act === "copy") {
      const clip = copyPatterns(m, J.sel, ks);
      if (!clip.pats.length) { say("Nothing to copy there: empty patterns (or the demos' own).", "--orange"); return; }
      J.clip = clip;
      toast(`${clip.pats.length} copied`, "--cyan");
      render();
    } else if (act === "paste") {
      const j = ks[0], k0 = J.clip.pats[0].k, at = J.clip.pats.map((x) => j + x.k - k0);
      const used = at.filter((k) => k < NPAT && pr.pats[k]);
      if (used.length) {
        const ok = await confirmBox({ title: "PASTE OVER", what: range(used), go: "PASTE", color: "--yellow", sub: `${used.length} pattern${used.length === 1 ? "" : "s"} there now.` });
        if (!ok) return;
      }
      if (push({ kind: "paste", q: J.sel, j, clip: J.clip })) { J.pats = new Set(at.filter((k) => k < NPAT)); render(); }
    } else if (act === "clear") {
      const used = ks.filter((k) => pr.pats[k]);
      if (!used.length) return;
      const ok = await confirmBox({ title: "CLEAR", what: range(used), go: "CLEAR" });
      if (ok) push({ kind: "clear", q: J.sel, ks: used });
    } else if (act === "fizzpat") {
      const clip = copyPatterns(m, J.sel, ks);
      if (!clip.pats.length) { say("Nothing to save there: empty patterns (or the demos' own).", "--orange"); return; }
      ctx.download(makeFizzpat(clip, { fizz: ctx.fizz() }), patFileName(clip));
    } else if (act === "import") { $("pj-file").click(); }
  }
  async function pendAct(act) {
    if (act === "undo") { J.ops.pop(); J.cache = null; render(); }
    else if (act === "discard") {
      const ok = await confirmBox({ title: "DISCARD", what: `${J.ops.length} change${J.ops.length === 1 ? "" : "s"}`, go: "DISCARD", sub: "Nothing was written yet." });
      if (ok) { J.ops = []; J.cache = null; render(); }
    } else if (act === "write") await write();
    else if (act === "usb") say(ctx.NEEDS_USB || "Connect the USB cable for this.", "--orange");
  }
  async function write() {
    if (!on() || !J.ops.length) return;
    if (viaBle()) { say(ctx.NEEDS_USB || "Connect the USB cable for this.", "--orange"); return; }
    const st = await link().status().catch(() => null);
    if (st && st.playing) { say("Stop the FM-1, then WRITE again.", "--orange"); return; }
    if (st) S.unit.status = st;
    if (stale()) {                                      // (changed on the unit since it was read: read again, the changes again)
      if (!(await read({ force: true }))) return;
      try { J.cache = null; model(); } catch (e) { say(`The changes no longer fit what is on the FM-1: ${esc(refuse(e))}`, "--orange"); J.ops = []; J.cache = null; render(); return; }
    }
    const image = imageOf(model());
    const ok = await ctx.writeStore(image, J.base.image);
    if (!ok) return;
    const n = J.ops.length;
    J.ops = []; J.base = { model: modelOf(image), image, written: null }; J.cache = null;
    await after(`${n} change${n === 1 ? "" : "s"} written`);
  }

  // ---------------------------------------------------------------- kept: the actions
  async function keptAct(act) {
    const k = J.kept[J.ksel];
    if (!k) return;
    if (act === "kload") { J.arm = { kind: "load", i: J.ksel, from: -1, name: k.name }; render(); }
    else if (act === "kfile") ctx.download(k.bytes, prjFileName(k));
    else if (act === "kren") {
      const el = $("pj-kept").querySelector(`.bp[data-i="${J.ksel}"] .sname`);
      if (!el) return;
      el.innerHTML = `<input class="slotname" id="pj-kname" maxlength="12" value="${esc(k.name)}" spellcheck="false">`;
      const i = $("pj-kname");
      i.focus(); i.select();
      const done = async (commit) => {
        i.onblur = null;
        const name = cleanName(i.value);
        if (commit && name && name !== k.name) {
          const kp = renameKept(await readFizzprj(k.bytes, k.name), name);
          Object.assign(k, { name: kp.name, bytes: makeFizzprj(kp, { fizz: ctx.fizz() }) });
          await saveKept();
        }
        render();
      };
      i.onkeydown = (ev) => { ev.stopPropagation(); if (ev.key === "Enter") done(true); else if (ev.key === "Escape") done(false); };
      i.onblur = () => done(true);
    } else if (act === "kdel") {
      const ok = await confirmBox({ title: "DELETE", what: k.name, go: "DELETE", sub: "From this browser." });
      if (ok) { J.kept.splice(J.ksel, 1); J.ksel = -1; await saveKept(); render(); }
    }
  }
  async function files(list) {
    for (const f of list) {
      const bytes = new Uint8Array(await f.arrayBuffer());
      try {
        if (/\.fizzprj$/i.test(f.name)) {
          const kp = await readFizzprj(bytes, f.name);
          (await keptList()).unshift({ id: Date.now(), name: kp.name, date: Date.now(), pats: kp.pats, sounds: kp.sounds, colours: kp.colours, bytes });
          await saveKept();
          J.view = "kept"; J.ksel = 0;
          toast(`${kp.name} kept in this browser`, "--champagne");
        } else {
          J.clip = await readFizzpat(bytes, f.name);
          if (J.pats.size && model()) await patAct("paste"); else toast(`${J.clip.pats.length} pattern${J.clip.pats.length === 1 ? "" : "s"} ready to PASTE`, "--cyan");
        }
      } catch (e) { say(esc(e.message), "--red"); }
    }
    render();
  }

  // ---------------------------------------------------------------- events
  $("pj-slots").addEventListener("click", async (ev) => {
    const b = ev.target.closest(".slot[data-p]");
    if (!b || ev.target.id === "pj-rename") return;
    const p = +b.dataset.p;
    if (J.arm) { await armedTo(p); return; }
    if (J.renaming) return;
    if (p !== J.sel) { J.sel = p; J.pats.clear(); J.anchor = -1; }
    if (J.view === "kept") J.view = "patterns";
    render();
  });
  $("pj-slots").addEventListener("keydown", (ev) => {
    if (ev.target.id !== "pj-rename") return;
    ev.stopPropagation();
    if (ev.key === "Enter") renameDone(true); else if (ev.key === "Escape") renameDone(false);
  });
  $("pj-slots").addEventListener("focusout", (ev) => { if (ev.target.id === "pj-rename" && J.renaming) renameDone(true); });
  $("pj-acts").addEventListener("click", (ev) => { const b = ev.target.closest("button[data-act]"); if (b && !b.disabled) slotAct(b.dataset.act); });
  $("pj-grid").addEventListener("click", (ev) => {
    if (ev.target.closest("#pj-read")) { read({ force: true }); return; }
    const t = ev.target.closest(".pt[data-k]");
    if (!t) return;
    const k = +t.dataset.k;
    if (ev.shiftKey && J.anchor >= 0) { J.pats = new Set(); for (let x = Math.min(J.anchor, k); x <= Math.max(J.anchor, k); x++) J.pats.add(x); }
    else if (ev.metaKey || ev.ctrlKey) { if (J.pats.has(k)) J.pats.delete(k); else J.pats.add(k); J.anchor = k; }
    else { J.pats = new Set(J.pats.size === 1 && J.pats.has(k) ? [] : [k]); J.anchor = k; }
    render();
  });
  $("pj-tools").addEventListener("click", (ev) => {
    const b = ev.target.closest("button[data-act]");
    if (!b || b.disabled) return;
    if (J.view === "kept") keptAct(b.dataset.act); else patAct(b.dataset.act);
  });
  $("pj-pending").addEventListener("click", (ev) => { const b = ev.target.closest("button[data-act]"); if (b && !b.disabled) pendAct(b.dataset.act); });
  $("pj-view").addEventListener("click", async (ev) => { const b = ev.target.closest("button[data-v]"); if (!b) return; J.view = b.dataset.v; if (J.view === "kept") await keptList(); render(); });
  $("pj-kept").addEventListener("click", (ev) => { const b = ev.target.closest(".bp[data-i]"); if (b && !ev.target.closest("input")) { J.ksel = J.ksel === +b.dataset.i ? -1 : +b.dataset.i; render(); } });
  $("pj-file").addEventListener("change", async (ev) => { const f = [...ev.target.files]; ev.target.value = ""; await files(f); });
  const card = $("tab-projects");
  card.addEventListener("dragover", (ev) => ev.preventDefault());
  card.addEventListener("drop", (ev) => { ev.preventDefault(); files([...ev.dataTransfer.files].filter((f) => /\.(fizzprj|fizzpat)$/i.test(f.name))); });
  document.addEventListener("keydown", (ev) => {
    if (card.hidden || (ev.target && /^(INPUT|TEXTAREA|SELECT)$/.test(ev.target.tagName))) return;
    if (ev.key === "Escape" && J.arm) { J.arm = null; render(); return; }
    if (!(ev.metaKey || ev.ctrlKey) || J.view !== "patterns") return;
    const k = ev.key.toLowerCase();
    if (k === "c" && J.pats.size) { ev.preventDefault(); patAct("copy"); }
    else if (k === "v" && J.pats.size && J.clip) { ev.preventDefault(); patAct("paste"); }
    else if (k === "z" && J.ops.length) { ev.preventDefault(); pendAct("undo"); }
  });

  return {
    async show() {
      await keptList();
      if (S.unit && S.unit.projects) { const cur = S.unit.projects.list.findIndex((x) => x.current); if (J.sel < 0 || !list()[J.sel] || (!J.base && cur >= 0)) J.sel = Math.max(0, cur); }
      render();
      if (on() && stale() && !J.ops.length) await read();
    },
    render,
    state: J,
  };
}
