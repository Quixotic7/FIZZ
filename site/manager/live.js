// SPDX-License-Identifier: GPL-3.0-only
// The live editor (docs/MANAGER-P2-PLAN.md §1, P2-B, the owner's decisions of 2026-10-25): the Manager edits and loads
// sounds the way the unit's sound browser does. One clear switch in the header (TRACK): off, the Manager only manages
// presets and never touches the open project's tracks (safe while playing); on:
//   - EDIT syncs with the sound on the track (the selected one, or one chosen) and edits it live: each change a PARAM
//     (qd_player_live_edit, as a knob turned on the unit), one at a time with its ACK (the unit holds one SysEx frame);
//     the track read back twice a second (a track DUMP), so the unit's own knob turns show; REVERT puts the track back
//     exactly as it was when the editor took it (its DUMP sent back as an AUDITION);
//   - a preset picked in LIBRARY plays on that track at once (AUDITION: nothing stored, no undo level); the bar under
//     the header keeps the track's own sound: KEEP, or CANCEL puts it back (as OCT- on the unit's browser).
// An older FIZZ (no track DUMP) leaves the switch off: the editor plays in this browser only.
//   mountLive(ctx) -> { target, on(), probe(), lost(), pick(row), show(), tracksMenu(anchor), saveOpts() }
import { soundFromFrame, storeFrameOf } from "./sound.js";
import { parseDump, storeSound, trackFrame } from "../js/fizz_sysex.js";
import { esc, refusal } from "./ui.js";

export function mountLive(ctx) {
  const { S, $, log, say, toast, editor, browserTarget } = ctx;
  const L = { on: false, cap: false, track: "selected", t: -1, base: null, ext: null, pending: null, q: new Map(), pumping: false,
    lastParam: 0, timer: null, attaching: false, menu: null };
  const sw = $("live"), bar = $("loadbar");
  const slot = () => (L.track === "selected" ? 0 : L.track + 1);
  const link = () => S.link;
  const ok = () => L.on && L.cap && !!S.link && !S.busy;
  const tn = (t) => `TRACK ${t + 1}`;

  // ---------------------------------------------------------------- the switch, the bar
  function render() {
    sw.hidden = !S.link || !S.unit || !S.unit.status;
    sw.setAttribute("aria-pressed", String(L.on));
    sw.classList.toggle("on", L.on);
    sw.disabled = !L.cap;
    sw.title = L.cap ? (L.on ? "Edit and load onto the FM-1's track: on" : "Edit and load onto the FM-1's track: off (the tracks are left alone)")
      : "This FIZZ cannot edit a track live yet: update FIZZ";
    $("live-label").textContent = L.on && L.t >= 0 ? tn(L.t) : "TRACK";
    bar.hidden = !(L.on && L.pending);
    if (L.on && L.pending) {
      bar.innerHTML = `<button class="lbt" id="lb-track" aria-label="Which track">${tn(L.pending.t)}</button><span class="arr">←</span><b>${esc(L.pending.name)}</b>` +
        `<button class="btn small" id="lb-keep" style="--c:var(--pink);color:#fff">KEEP</button><button class="btn small ghost" id="lb-cancel">CANCEL</button>`;
      $("lb-keep").onclick = keep;
      $("lb-cancel").onclick = cancel;
      $("lb-track").onclick = (ev) => tracksMenu(ev.currentTarget);
    }
  }
  sw.addEventListener("click", () => setOn(!L.on));
  async function setOn(on) {
    if (on && !L.cap) { say('This FIZZ cannot edit a track live yet: <a href="../install/">update FIZZ</a>.', "--orange"); return; }
    L.on = on;
    if (!on) { L.pending = null; L.q.clear(); stopPoll(); editor.setTarget(browserTarget); log("TRACK off: the Manager leaves the tracks alone"); }
    else { log("TRACK on: EDIT edits the track, LIBRARY loads onto it"); editor.setTarget(target); if (!$("tab-edit").hidden) await attach(); else await readTrack(); }
    render();
    if (ctx.changed) ctx.changed();
  }

  // ---------------------------------------------------------------- the track: read, taken into the editor
  async function readTrack() {
    if (!S.link) return null;
    const f = await link().dumpTrack(slot()).catch(() => null);
    if (!f) return null;
    const s = soundFromFrame(f);
    L.t = s.track; L.ext = s.extras;
    editor.setRec(!!(s.extras && s.extras.rec));
    return { f, s };
  }
  const presetRow = (id) => (S.unit && id >= 0x100 ? S.unit.rows.find((r) => r.source === "mine" && r.id === id) : null);
  function placeOf(s) {
    const id = s.extras ? s.extras.id : 0, row = presetRow(id), fac = id >= 1 && id <= 23;
    return { label: `${tn(s.track)} · ${row ? `MY PRESETS ${row.slot}` : fac ? `FACTORY ${id}` : "ITS OWN SOUND"}`, source: row ? "mine" : "track",
      slot: row ? row.slot : 0, id, track: s.track, fromTarget: true };
  }
  async function attach() {
    if (!ok() || L.attaching) return false;
    L.attaching = true;
    try {
      const r = await readTrack();
      if (!r) return false;
      L.base = r.f;
      editor.open(r.s, placeOf(r.s));
      startPoll();
      render();
      return true;
    } finally { L.attaching = false; }
  }
  // twice a second while EDIT shows: the track read back (the unit's knobs), another track selected: taken
  function startPoll() {
    stopPoll();
    L.timer = setInterval(async () => {
      if (!ok() || $("tab-edit").hidden || L.attaching || L.q.size || L.pumping || performance.now() - L.lastParam < 700 || S.editing) return;
      const t0 = L.t, r = await readTrack();
      if (!r || !L.on) return;
      if (r.s.track !== t0) { L.base = r.f; editor.open(r.s, placeOf(r.s)); render(); return; }
      if (L.q.size || performance.now() - L.lastParam < 700) return;   // (an edit went out meanwhile: not the read's)
      editor.follow(r.s);
    }, 500);
  }
  function stopPoll() { clearInterval(L.timer); L.timer = null; }

  // ---------------------------------------------------------------- PARAMs: the latest value of each, one at a time
  async function pump() {
    if (L.pumping) return;
    L.pumping = true;
    try {
      while (L.q.size && ok()) {
        const [id, v] = L.q.entries().next().value;
        L.q.delete(id);
        const r = await link().param(L.t, id, v).catch(() => null);
        L.lastParam = performance.now();
        if (r && r.old) { L.cap = false; setOn(false); say('This FIZZ cannot edit a track live yet: <a href="../install/">update FIZZ</a>.', "--orange"); break; }
        if (r && r.status) log(`PARAM ${id}: ${r.statusName || r.status}`);
      }
    } finally { L.pumping = false; }
  }
  // a sound onto the track, nothing stored (its preset id with it: the track plays that preset, not edited)
  function audition(s, t = L.t) {
    if (!ok() || t < 0) return Promise.resolve(null);
    const x = s.extras || {};
    return link().store(storeFrameOf(s, { track: t, audition: true, extras: { id: x.id || 0, sound: x.sound || 0, edited: !!x.edited } }));
  }

  // the editor's target: the FM-1's track
  const target = {
    live: true,
    label: () => (L.t >= 0 ? `${tn(L.t)} · LIVE` : "TRACK · LIVE"),
    ready: () => ok() && L.t >= 0,
    start: () => attach(),
    audition: (s) => audition(s),
    param: (id, v) => { if (!ok()) return; L.q.set(id, v); L.lastParam = performance.now(); pump(); },
    note: (n, vel) => { if (!S.link || L.t < 0) return; const ch = L.track === "selected" || (L.ext && L.ext.selected) ? 15 : (L.ext ? L.ext.chan : 15); S.link.note(ch, n, vel); },
    // REVERT: the sound the editor took from the track, put back exactly (its DUMP); a preset opened: that preset again
    revert: async (s, E) => {
      L.q.clear();
      if (E.place && E.place.fromTarget && L.base) await link().store(trackFrame(L.base)).catch(() => null);
      else await audition(s);
      L.lastParam = performance.now();
    },
    click: (E, el) => tracksMenu(el),
  };

  // the track chooser: the selected one, or one of the six
  function tracksMenu(anchor) {
    closeMenu();
    const m = document.createElement("div");
    m.className = "picker tracks";
    m.setAttribute("role", "listbox");
    m.innerHTML = [["selected", "SELECTED"], ...[0, 1, 2, 3, 4, 5].map((t) => [t, String(t + 1)])].map(([k, l]) =>
      `<button role="option" data-t="${k}" class="${L.track === k ? "on" : ""}" aria-selected="${L.track === k}">${l}</button>`).join("");
    document.body.appendChild(m);
    const b = anchor.getBoundingClientRect();
    m.style.left = `${Math.max(12, Math.min(window.innerWidth - 340, b.left))}px`;
    m.style.top = `${b.bottom + 8 + window.scrollY}px`;
    L.menu = m;
    m.addEventListener("click", async (ev) => {
      const o = ev.target.closest("[data-t]");
      if (!o) return;
      L.track = o.dataset.t === "selected" ? "selected" : +o.dataset.t;
      closeMenu();
      if (L.pending) { await cancel(); }
      if (!$("tab-edit").hidden) await attach(); else await readTrack();
      render();
    });
    setTimeout(() => document.addEventListener("pointerdown", outside, true), 0);
  }
  const outside = (ev) => { if (L.menu && !L.menu.contains(ev.target)) closeMenu(); };
  function closeMenu() { if (L.menu) { L.menu.remove(); L.menu = null; } document.removeEventListener("pointerdown", outside, true); }

  // ---------------------------------------------------------------- LIBRARY: a preset picked plays on the track
  async function pick(row, frame) {
    if (!ok() || !frame) return false;
    if (!L.pending) {                                   // (the track's own sound, kept for CANCEL)
      const r = await readTrack();
      if (!r) return false;
      L.pending = { frame: r.f, t: r.s.track, name: "" };
    }
    const d = parseDump(frame);
    const x = await link().store(storeSound({ name: d.name, blob: d.blob, drive: d.drive, track: L.pending.t, pan: d.pan, chorus: d.chorus,
      delay: d.delay, reverb: d.reverb, tags: d.tags, audition: true, extras: { id: row.id, sound: 0, edited: false } })).catch(() => null);
    if (!x || x.kind !== "store" || x.status !== 0) { say(refusal(x, "pick again"), "--orange"); return false; }
    L.pending.name = row.name;
    log(`${tn(L.pending.t)} plays ${row.name} (AUDITION)`);
    render();
    return true;
  }
  function keep() { if (L.pending) toast(`${tn(L.pending.t)}: ${L.pending.name}`, "--pink"); L.pending = null; render(); }
  async function cancel() {
    const p = L.pending;
    L.pending = null;
    render();
    if (!p || !S.link) return;
    const x = await link().store(trackFrame(p.frame)).catch(() => null);
    if (x && x.status === 0) toast(`${tn(p.t)}: its own sound again`, "--muted");
    if (!$("tab-edit").hidden && L.on) await attach();
  }

  return {
    target, render, attach, pick, keep, cancel, tracksMenu,
    on: () => L.on && L.cap,
    state: L,
    // a FIZZ that can: its track DUMP answers (connect, REFRESH)
    async probe() {
      L.cap = !!(S.link && S.unit && S.unit.status && (await S.link.dumpTrack(0).catch(() => null)));
      if (!L.cap && L.on) setOn(false);
      render();
      return L.cap;
    },
    lost() { stopPoll(); L.pending = null; L.q.clear(); if (L.on) { L.on = false; editor.setTarget(browserTarget); } L.cap = false; render(); if (ctx.changed) ctx.changed(); },
    // EDIT shown: take the track (on), else nothing
    show() { if (ok()) attach(); },
    hide() { stopPoll(); },
    // SAVE's frame options: on, the track plays the saved preset (LOAD, not edited)
    saveOpts: () => (ok() && L.t >= 0 ? { track: L.t } : {}),
    afterSave: async () => { if (ok() && !$("tab-edit").hidden) await attach(); },
  };
}
