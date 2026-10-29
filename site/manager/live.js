// SPDX-License-Identifier: GPL-3.0-only
// The live editor (docs/MANAGER-P2-PLAN.md §1, P2-B, the owner's decisions of 2026-10-25 and their fixes): the Manager
// edits and loads sounds the way the unit's sound browser does. One clear switch in the header (TRACK, its number the
// track it follows): off, the Manager never changes the open project's tracks (safe while playing); on, it does.
//   - EDIT follows the FM-1's track: the selected one (it follows the unit's SELECT: the track read back twice a
//     second, a track DUMP), or one picked on the editor's track strip (SELECTED, 1..6). The tracked track is big in
//     the editor's header. TRACK on: each change a PARAM (qd_player_live_edit, as a knob turned on the unit), one at a
//     time with its ACK (the unit holds one SysEx frame); the read-back waits 0.7 s after the last PARAM, so a drag is
//     not pulled back; REVERT puts the track back exactly (its DUMP sent back as an AUDITION). TRACK off: the track's
//     sound is shown and follows the unit, but not changed (a drag nudges the switch);
//   - a preset picked in LIBRARY with TRACK on plays on that track at once (AUDITION: nothing stored, no undo level);
//     the bar under the header keeps the track's own sound: KEEP, or CANCEL puts it back (as OCT- on the unit);
//   - LOAD ON FM-1 (LIBRARY, either way: the owner decides on the unit): BROWSE opens the unit's own sound browser on
//     the preset, as PRESETS turned to it (OCT- cancels, OCT+ loads); STATUS read until the browser closes, then the
//     track's DUMP says which.
// An older FIZZ (no track DUMP) leaves the switch off: the editor plays in this browser only.
// FIZZ MIDI (e): a MIDI track's sound (PARAM takes a slot's value only, not its CC number or touched mark): each change
// the whole sound as an AUDITION (target.sound), the latest one, in the same queue as the PARAMs.
//   mountLive(ctx) -> { target, on(), probe(), lost(), pick(row), show(), hide(), strip(E), stripClick(b), nudge(),
//                       loadOnUnit(row), browsing(), saveOpts(), afterSave(), detach() }
import { soundFromFrame, storeFrameOf, copySound } from "./sound.js";
import { parseDump, storeSound, trackFrame } from "../js/fizz_sysex.js";
import { esc, refusal } from "./ui.js";

const HINT_KEY = "fizz-manager-track-seen";
const seenHint = () => { try { return localStorage.getItem(HINT_KEY) === "1"; } catch (_) { return true; } };
const markHint = () => { try { localStorage.setItem(HINT_KEY, "1"); } catch (_) { /* (no storage: the hint again) */ } };

export function mountLive(ctx) {
  const { S, $, log, say, toast, editor, browserTarget } = ctx;
  const L = { on: false, cap: false, follow: false, track: "selected", t: -1, usel: -1, base: null, ext: null, pending: null,
    q: new Map(), pumping: false, lastParam: 0, timer: null, attaching: false, nudgeT: 0, browse: null, browseT: null, hint: !seenHint() };
  const sw = $("live"), bar = $("loadbar");
  const slot = () => (L.track === "selected" ? 0 : L.track + 1);
  const link = () => S.link;
  const canRead = () => L.cap && !!S.link && !S.busy;
  const ok = () => L.on && canRead();
  const editShown = () => !$("tab-edit").hidden;
  const tn = (t) => `TRACK ${t + 1}`;

  // ---------------------------------------------------------------- the switch, the bar
  function render() {
    sw.hidden = !S.link || !S.unit || !S.unit.status;
    sw.setAttribute("aria-pressed", String(L.on));
    sw.classList.toggle("on", L.on);
    sw.classList.toggle("hint", L.hint && L.cap && !L.on);
    sw.disabled = !L.cap;
    const n = L.t >= 0 ? L.t : L.usel;
    sw.title = L.cap ? (L.on ? "Edit and load onto the FM-1's track: on" : "Edit and load onto the FM-1's track: off (the tracks are left alone)")
      : "This FIZZ cannot edit a track live yet: update FIZZ";
    sw.setAttribute("aria-label", `TRACK ${n >= 0 ? n + 1 : ""}: ${L.on ? "on" : "off"}`);
    $("live-n").textContent = n >= 0 ? String(n + 1) : "–";
    bar.hidden = !(L.on && L.pending);
    if (L.on && L.pending) {
      bar.innerHTML = `<div class="lbh"><span class="lbt"><span class="eq" aria-hidden="true"><i></i><i></i><i></i></span>HEARD ON ${tn(L.pending.t)}</span><b>${esc(L.pending.name)}</b></div>` +
        `<div class="lbb"><button class="ol" id="lb-cancel" style="--c:var(--pink)">CANCEL</button><button class="arc breathe" id="lb-keep" style="--c:var(--pink)">KEEP</button></div>`;
      $("lb-keep").onclick = keep;
      $("lb-cancel").onclick = cancel;
    }
  }
  sw.addEventListener("click", () => setOn(!L.on));
  async function setOn(on) {
    if (on && !L.cap) { say('This FIZZ cannot edit a track live yet: <a href="../install/">update FIZZ</a>.', "--orange"); return; }
    if (L.hint) { L.hint = false; markHint(); }
    L.on = on;
    if (!on) { L.pending = null; L.q.clear(); log("TRACK off: the Manager leaves the tracks alone"); }
    else {
      log("TRACK on: EDIT edits the track, LIBRARY loads onto it");
      if (editShown()) await attach(); else await readTrack();
    }
    render();
    editor.render();
    if (ctx.changed) ctx.changed();
  }
  // a change refused while TRACK is off: the switch (and the strip's LIVE) pulse
  function nudge() {
    const now = performance.now();
    if (now - L.nudgeT < 900) return;
    L.nudgeT = now;
    for (const el of [sw, document.getElementById("trk-live")]) {
      if (!el) continue;
      el.classList.remove("nudge");
      void el.offsetWidth;
      el.classList.add("nudge");
      setTimeout(() => el.classList.remove("nudge"), 900);
    }
  }

  // ---------------------------------------------------------------- the track: read, followed by the editor
  async function readTrack() {
    if (!S.link) return null;
    const f = await link().dumpTrack(slot()).catch(() => null);
    if (!f) return null;
    const s = soundFromFrame(f);
    L.t = s.track; L.ext = s.extras;
    if (s.extras) L.usel = s.extras.sel;
    editor.setRec(!!(s.extras && s.extras.rec));
    return { f, s };
  }
  const presetRow = (id) => (S.unit && id >= 0x100 ? S.unit.rows.find((r) => r.source === "mine" && r.id === id) : null);
  function placeOf(s) {
    const id = s.extras ? s.extras.id : 0, row = presetRow(id), fac = id >= 1 && id <= 23;
    return { label: `${tn(s.track)} · ${row ? `MY PRESETS ${row.slot}` : fac ? `FACTORY ${id}` : "ITS OWN SOUND"}${s.extras && s.extras.edited ? " · EDITED ON THE FM-1" : ""}`,
      source: row ? "mine" : "track", slot: row ? row.slot : 0, id, track: s.track, fromTarget: true };
  }
  // the editor on the track: read, opened, followed (TRACK on or off: off, only shown)
  async function attach() {
    if (!canRead() || L.attaching) return false;
    L.attaching = true;
    try {
      const r = await readTrack();
      if (!r) return false;
      L.base = r.f;
      L.follow = true;
      editor.setTarget(target, false);
      editor.open(r.s, placeOf(r.s));
      startPoll();
      render();
      if (ctx.changed) ctx.changed();
      return true;
    } finally { L.attaching = false; }
  }
  // the editor off the track (a preset opened in this browser): the poll stops
  function detach() {
    if (!L.follow) return;
    L.follow = false;
    stopPoll();
    L.q.clear();
    editor.setTarget(browserTarget);
  }
  // twice a second while EDIT shows: the track read back (the unit's knobs), another track selected on the unit: taken
  function startPoll() {
    stopPoll();
    L.timer = setInterval(async () => {
      if (!canRead() || !L.follow || !editShown() || L.attaching || L.q.size || L.pumping || performance.now() - L.lastParam < 700 || S.editing) return;
      const t0 = L.t, u0 = L.usel, r = await readTrack();
      if (!r || !L.follow) return;
      if (r.s.track !== t0) { L.base = r.f; editor.open(r.s, placeOf(r.s)); render(); log(`EDIT follows ${tn(r.s.track)} (selected on the FM-1)`); return; }
      if (L.usel !== u0) render();
      if (L.q.size || performance.now() - L.lastParam < 700) return;   // (an edit went out meanwhile: not the read's)
      if (editor.follow(r.s, { rebase: !L.on }) && !L.on) L.base = r.f;   // (only shown: the unit's own sound is the one to go back to)
      const p = editor.place();
      if (p.fromTarget && r.s.extras && p.id !== r.s.extras.id) { editor.setPlace(placeOf(r.s)); L.base = r.f; }   // (loaded on the unit)
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
        if (id === "sound") {                           // (FIZZ MIDI (e): the whole sound, edited)
          const x = await audition(v).catch(() => null);
          L.lastParam = performance.now();
          if (x && x.status) log(`AUDITION: ${x.statusName || x.status}`);
          continue;
        }
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

  // the editor's target: the FM-1's track (locked while TRACK is off: shown, not changed)
  const target = {
    live: true,
    locked: () => !L.on,
    label: () => (L.t >= 0 ? `${tn(L.t)} · ${L.on ? "LIVE" : "SHOWN"}` : "TRACK"),
    ready: () => canRead() && L.t >= 0,
    start: () => attach(),
    audition: (s) => audition(s),
    param: (id, v) => { if (!ok()) return; L.q.set(id, v); L.lastParam = performance.now(); pump(); },
    sound: (s) => { if (!ok()) return; L.q.set("sound", copySound({ ...s, extras: { ...(s.extras || {}), edited: true } })); L.lastParam = performance.now(); pump(); },
    // (the track's own MIDI channel: it plays that track on any FIZZ; the selected track's channel moved from 16 to 15
    // with FIZZ MIDI (a), 16 now the remote's)
    note: (n, vel) => { if (!S.link || L.t < 0) return; S.link.note(L.ext ? L.ext.chan : 14, n, vel); },
    // REVERT: the sound the editor took from the track, put back exactly (its DUMP); a preset opened: that preset again
    revert: async (s, E) => {
      L.q.clear();
      if (!L.on) return;
      if (E.place && E.place.fromTarget && L.base) await link().store(trackFrame(L.base)).catch(() => null);
      else await audition(s);
      L.lastParam = performance.now();
    },
  };

  // ---------------------------------------------------------------- the editor's track strip
  // SELECTED (follow the unit's selection), 1..6 (the unit's selected one dotted, the followed one lit), LIVE (the
  // header's switch, the same state)
  function strip() {
    if (!L.cap || !S.link) return "";
    const cur = L.follow ? L.t : -1;
    const btn = (k, label, cls, aria) => `<button class="trk ${cls}" data-trk="${k}" aria-pressed="${cls.includes(" on")}" aria-label="${aria}">${label}</button>`;
    return `<div class="trks" role="group" aria-label="The FM-1's track">` +
      btn("selected", '<svg viewBox="0 0 20 20" aria-hidden="true"><circle cx="10" cy="10" r="3.2"/><path d="M10 1.5v3.5M10 15v3.5M1.5 10h3.5M15 10h3.5"/></svg>SELECTED',
        `sel${L.follow && L.track === "selected" ? " on" : ""}`, "Follow the FM-1's selected track") +
      [0, 1, 2, 3, 4, 5].map((t) => btn(t, String(t + 1), `n${t === cur ? " on" : ""}${t === L.usel ? " usel" : ""}`, `Track ${t + 1}`)).join("") +
      `<button class="trk live ${L.on ? "on" : ""}" id="trk-live" data-trk="live" aria-pressed="${L.on}" aria-label="Live: edit the FM-1's track"><span class="sw" aria-hidden="true"><i></i></span>LIVE</button></div>`;
  }
  async function stripClick(b) {
    const k = b.dataset.trk;
    if (k === "live") { await setOn(!L.on); return; }
    L.track = k === "selected" ? "selected" : +k;
    if (L.pending) await cancel();
    await attach();
  }

  // ---------------------------------------------------------------- LIBRARY: a preset picked plays on the track
  async function pick(row, frame) {
    if (!ok() || !frame) return false;
    if (!L.pending) {                                   // (the track's own sound, kept for CANCEL)
      const r = await readTrack();
      if (!r) return false;
      L.pending = { frame: r.f, t: r.s.track, name: "" };
    }
    const d = parseDump(frame);
    L.pending.name = row.name;                          // (the bar up at once: CANCEL puts the track's own sound back whatever came of it)
    render();
    const x = await sure(storeSound({ name: d.name, blob: d.blob, drive: d.drive, track: L.pending.t, pan: d.pan, chorus: d.chorus,
      delay: d.delay, reverb: d.reverb, tags: d.tags, audition: true, extras: { id: row.id, sound: 0, edited: false } }), d.name);
    if (!x || x.status !== 0) { say(refusal(x, "pick again"), "--orange"); return false; }
    log(`${tn(L.pending.t)} plays ${row.name} (AUDITION)`);
    render();
    return true;
  }
  // (the owner, 2026-10-27) a sound that is not on the unit (an IMPORT row) heard on the track the same way: AUDITION,
  // nothing stored; KEEP / CANCEL as for a preset picked in LIBRARY
  async function pickSound(s, name) {
    if (!ok() || !s) return false;
    if (!L.pending) {
      const r = await readTrack();
      if (!r) return false;
      L.pending = { frame: r.f, t: r.s.track, name: "" };
    }
    L.pending.name = name || s.name;
    render();
    const x = await sure(storeSound({ name: s.name, blob: s.blob, drive: s.drive || 0, track: L.pending.t, pan: s.pan,
      chorus: s.sends && s.sends.chorus, delay: s.sends && s.sends.delay, reverb: s.sends && s.sends.reverb, audition: true,
      extras: { id: 0, sound: 0, edited: true } }), s.name);
    return !!(x && x.status === 0);
  }
  // an AUDITION (it sets the track's sound: sending it again is harmless): its reply lost, the track read to see
  // whether it took, else sent again (up to three times)
  async function sure(frame, name, t = L.pending ? L.pending.t : -1) {
    for (let i = 0; i < 3; i++) {
      const x = await link().store(frame).catch(() => null);
      if (x && x.kind === "store") return x;
      const f = await link().dumpTrack(t + 1).catch(() => null);
      const d = f && parseDump(f);
      if (d && d.name.trim() === String(name).trim()) { log("AUDITION: its reply lost, the track has it"); return { kind: "store", status: 0 }; }
      log(`AUDITION: no answer, sent again (${i + 1})`);
    }
    return null;
  }
  function keep() { if (L.pending) toast(`${tn(L.pending.t)}: ${L.pending.name}`, "--pink"); L.pending = null; render(); if (ctx.kept) ctx.kept(); }
  async function cancel() {
    const p = L.pending;
    L.pending = null;
    render();
    if (!p || !S.link) return;
    const x = await sure(trackFrame(p.frame), parseDump(p.frame).name, p.t);
    if (x && x.status === 0) { toast(`${tn(p.t)}: its own sound again`, "--muted"); if (ctx.kept) ctx.kept(); }
    else { L.pending = p; render(); say(`${tn(p.t)}'s own sound did not go back yet: CANCEL again.`, "--orange"); }
    if (editShown() && L.follow) await attach();
  }

  // ---------------------------------------------------------------- LOAD ON FM-1: the unit's own sound browser
  async function loadOnUnit(row) {
    if (!S.link || S.busy || !row || !row.id) return null;
    if (L.pending) await cancel();                      // (the track's own sound back first: the browser starts from it)
    const tr = L.track === "selected" ? "selected" : L.track;
    const x = await S.link.browse(tr, row.id).catch(() => null);
    if (!x || x.status) {
      say(x && x.old ? 'This FIZZ cannot open its browser from here yet: <a href="../install/">update FIZZ</a>.'
        : x && x.status === 8 ? "A name is being typed on the FM-1: finish it there, then LOAD ON FM-1 again." : refusal(x, "LOAD ON FM-1 again"), "--orange");
      return x;
    }
    L.browse = { id: row.id, name: row.name, t: x.a - 1, t0: Date.now() };
    L.usel = x.a - 1;
    log(`LOAD ON FM-1: ${row.name} in the FM-1's browser on ${tn(x.a - 1)} (OCT- cancels, OCT+ loads)`);
    render();
    if (ctx.changed) ctx.changed();
    watchBrowse();
    return x;
  }
  // the unit's browser open: STATUS until it closes (OCT+ / OCT- on the unit, or anything else), then the track says
  function watchBrowse() {
    clearTimeout(L.browseT);
    L.browseT = setTimeout(async () => {
      const b = L.browse;
      if (!b) return;
      if (!S.link || Date.now() - b.t0 > 180000) { L.browse = null; if (ctx.changed) ctx.changed(); return; }
      if (S.busy || S.editing) { watchBrowse(); return; }
      const st = await S.link.status().catch(() => null);
      if (L.browse !== b) return;
      if (!st || st.browse) { watchBrowse(); return; }
      const f = await S.link.dumpTrack(b.t + 1).catch(() => null), s = f && soundFromFrame(f);
      L.browse = null;
      if (s && s.extras && s.extras.id === b.id) toast(`${tn(b.t)} · ${b.name}`, "--pink");
      else toast(`${tn(b.t)}: its own sound`, "--muted");
      if (ctx.changed) ctx.changed();
    }, 600);
  }

  return {
    target, render, attach, detach, pick, pickSound, keep, cancel, strip, stripClick, nudge, loadOnUnit, setOn,
    on: () => L.on && L.cap,
    following: () => L.follow,
    browsing: () => L.browse,
    state: L,
    // a FIZZ that can: its track DUMP answers (connect, REFRESH)
    async probe() {
      const f = S.link && S.unit && S.unit.status ? await S.link.dumpTrack(0).catch(() => null) : null;
      L.cap = !!f;
      if (f) { const s = soundFromFrame(f); if (s && s.extras) L.usel = s.extras.sel; if (L.t < 0 && s) L.t = s.track; }
      if (!L.cap && L.on) setOn(false);
      render();
      return L.cap;
    },
    lost() {
      stopPoll(); L.pending = null; L.q.clear(); clearTimeout(L.browseT); L.browse = null;
      if (L.follow) { L.follow = false; editor.setTarget(browserTarget); }
      L.on = false; L.cap = false; render(); if (ctx.changed) ctx.changed();
    },
    // EDIT shown: the editor follows the FM-1's track, unless it holds a preset or a file opened in this browser
    show() {
      const p = editor.place();
      if (canRead() && (L.follow || L.on || !p.source || p.source === "new")) attach();
    },
    hide() { stopPoll(); },
    // SAVE's frame options: on, the track plays the saved preset (LOAD, not edited)
    saveOpts: () => (ok() && L.t >= 0 && L.follow ? { track: L.t } : {}),
    afterSave: async () => { if (ok() && editShown() && L.follow) await attach(); },
  };
}
