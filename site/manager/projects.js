// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
// The PROJECTS tab's model (the owner's requests, 2026-10-26; docs/MANAGER-P2-PLAN.md §3, §4): a FIZZ store as its
// records, read and changed in the browser, with FIZZ's own record layouts (core/app/persist.c: ser_pat / de_pat,
// snd_rec, ser_hdr, ser_ftag; core/seq/pattern.c qd_step_wire). No firmware command is new: the FM-1's own PROJECT /
// RENAME do the projects' moves at once; a pattern pasted, a project kept here brought back, are written with the
// store's restore (FZSR) in one go. No DOM: the tests run it in Node.
//
//   modelOf(image) -> {layout, magic, lib: Map(key -> bytes), prj: Map}      imageOf(model) -> the store image
//   project(model, p) -> {p, there, name, demo, pats: [patSummary | null] x 64, used, pool: n}
//   walkPat(bytes) -> {v, used, rows: [6 x 16 trigs], trigs, soundAt, refAt, idAt} | null (a record it cannot read)
//   copyPatterns(model, p, ks) -> a clip {from, names, pats: [{k, bytes}], pool: Map(n -> {id} | {rec}), presets: Map(id -> LIB record)}
//   pastePatterns(model, q, j, clip) -> {at: [k], poolAdded, presetsAdded}   (throws {code: "poolfull" | "libfull" | "range"})
//   clearPatterns(model, q, ks)
//   keepProject(model, p) -> a kept project {name, demo, recs: [{kind, i, bytes}], presets: Map, pats, sounds}
//   loadProject(model, q, kept) -> {presetsAdded, copies}  (slot q's records replaced; throws {code: "range"})
//   limits(model) -> {nproj, nlib, package}           (the store's package: USB 8 and 256, Bluetooth 4 and 128)
//   fitModel(model, to, {projects, presets}) -> {model, copies, left: {projects, presets}}   (another package's layout)
//   fitChoice(model, to, {projects}) -> {projects, presets}   (fitModel's default: the first projects, their presets)
//   makeFizzprj(kept, {fizz, date}) / readFizzprj(bytes)    makeFizzpat(clip, ..) / readFizzpat(bytes)
// The changes are FIZZ's own rules, done here: a pattern's steps and track sounds name its project's pool numbers (1..128),
// re-mapped into the target's pool ('R' 1: a pool place's preset id; the same id where the target has it, else the first
// free place); its track sounds 'P' 5/6 name MY preset ids (the whole unit's): kept as they are when the target store has
// that preset with the same name and sound, else linked to one that has, else the preset added (a new id, as the unit's
// id_next) from the copy the clip carries; MY PRESETS full (the Bluetooth package's 128 fill sooner), the sound becomes the
// project's own copy instead (a track sound by reference written as a copy, a pool place 'S' with no id: persist.h
// P1.5w, the unit keeps it as it is until the library has room), so the project sounds the same everywhere. A project's
// MIDI FX groups stay its own (the owner: a project is "everything for
// one or two songs"): a pasted pattern plays the target's groups.
import { regionRecords, buildImage, preset as presetRec, layoutByName, PACKAGE_NAME } from "./fizzstore.js";
import { zipStored, soundToFizzsnd } from "../js/library.js";
import { dnReadZip } from "../js/fm1dnsnd.js";
import { FIZZ_TAGS } from "../js/fizz_sysex.js";

export const RK = { SET: 1, HDR: 2, PAT: 3, SND: 4, LIB: 5, FTAG: 6, MFX: 7 };       // persist.h QD_RK_*
export const NPROJ = 8, NPAT = 64, NPOOL = 128, NLIB = 256, NTRACK = 6, NFACTORY = 23, MINE0 = 0x100, DEMO_N = 3;   // (NPROJ, NLIB: the USB package's)
const NAME = 13, BLOB = 128, WIRE = 20, LIBLEN = 2 + NAME + BLOB + 8;
export const rkey = (kind, p, i) => ((kind << 24) | (p << 16) | i) >>> 0;
export const kindOf = (k) => k >>> 24, projOf = (k) => (k >>> 16) & 0xFF, idxOf = (k) => k & 0xFFFF;
export const patName = (k) => `${"ABCD"[k >> 4]}${String((k & 15) + 1).padStart(2, "0")}`;
const g16 = (b, o) => b[o] | (b[o + 1] << 8);
const p16 = (b, o, v) => { b[o] = v & 0xFF; b[o + 1] = (v >>> 8) & 0xFF; };
const cstr = (b, o, n) => { let s = ""; for (let i = 0; i < n && b[o + i]; i++) s += String.fromCharCode(b[o + i]); return s; };
const err = (code, msg, extra = {}) => Object.assign(new Error(msg), { code }, extra);

// ---- the store as records
export function modelOf(image) {
  const r = regionRecords(image);
  return { layout: r.layout, magic: r.magic, lib: new Map(r.lib.map((x) => [x.key, x.bytes])), prj: new Map(r.prj.map((x) => [x.key, x.bytes])) };
}
const sorted = (m) => [...m.entries()].sort((a, b) => a[0] - b[0]).map(([key, bytes]) => ({ key, bytes }));
export const imageOf = (m) => buildImage(m.layout, { lib: sorted(m.lib), prj: sorted(m.prj) });
export const limits = (m) => { const l = layoutByName(m.layout); return { nproj: l.nproj, nlib: l.nlib, package: l.package }; };
export const cloneModel = (m) => ({ layout: m.layout, magic: m.magic, lib: new Map(m.lib), prj: new Map(m.prj) });
const keysOf = (m, p) => [...m.prj.keys()].filter((k) => kindOf(k) !== RK.SET && projOf(k) === p);

// ---- a pattern record ('P' 2..6), walked as de_pat reads it: where its pool numbers and preset ids are, and its trigs
export function walkPat(b) {
  if (!b || b.length < 12 || b[0] !== 0x50 || b[1] < 2 || b[1] > 6 || b[2] !== WIRE) return null;
  const v = b[1], tsz = b[3], nlock = g16(b, 10), used = b[7];
  let o = 12;
  const refAt = [], idAt = [], soundAt = [], pages = [];
  if (v >= 5) { for (let t = 0; t < NTRACK; t++) refAt.push(o + 2 * t); o += 2 * NTRACK; }
  o += tsz * NTRACK + 4 * NTRACK;                      // the track settings, lock_start[], lock_n[]
  let trigs = 0;
  for (let t = 0; t < NTRACK; t++) {
    if (o >= b.length) return null;
    const mask = b[o++], pg = new Array(8).fill(0);
    for (let p = 0; p < 8; p++) {
      if (!((mask >> p) & 1)) continue;
      if (o + 2 > b.length) return null;
      const sm = g16(b, o);
      o += 2;
      for (let s = 0; s < 16; s++) {
        if (!((sm >> s) & 1)) continue;
        if (o + WIRE > b.length) return null;
        if (b[o + 16]) soundAt.push(o + 16);           // the step's sound lock (a pool number)
        if (b[o + 2] & 1) { pg[p] |= 1 << s; trigs++; }
        o += WIRE;
      }
    }
    pages.push(pg);
  }
  o += 4 * nlock;
  const sndAt = o;
  for (let t = 0; t < NTRACK; t++) {
    if (refAt.length && g16(b, refAt[t])) { soundAt.push(o); o += 2; continue; }   // by reference: its pool source, edited
    o += v >= 3 ? BLOB : 96;
    soundAt.push(o);
    o += 2 + NAME;
    if (v >= 2) { idAt.push(o); o += 2; }              // a copy's preset id (where it came from)
  }
  if (o !== b.length) return null;
  // the tile: the first page with trigs on any track, else the densest
  const pop = (x) => { let n = 0; for (; x; x &= x - 1) n++; return n; };
  let show = 0, best = -1;
  for (let p = 0; p < 8; p++) {
    const n = pages.reduce((a, pg) => a + pop(pg[p]), 0);
    if (p === 0 && n) { show = 0; break; }
    if (n > best) { best = n; show = p; }
  }
  const rows = pages.map((pg) => Array.from({ length: 16 }, (_, s) => !!((pg[show] >> s) & 1)));
  return { v, used: !!used, trigs, rows, page: show, soundAt: soundAt.filter((x) => b[x]), refAt, idAt, sndAt };
}

// ---- a project
export function project(m, p) {
  const h = m.prj.get(rkey(RK.HDR, p, 0));
  const there = !!(h && h[0] === 0x48 && h[1] === 1);
  const demo = there && !!h[2 + NAME];
  const pats = [];
  let used = 0;
  for (let k = 0; k < NPAT; k++) {
    const b = m.prj.get(rkey(RK.PAT, p, k));
    let s = null;
    if (b) s = walkPat(b) || { bad: true, used: true, trigs: 0, rows: [] };
    else if (demo && k < DEMO_N) s = { demo: true, used: true, trigs: 0, rows: [] };   // (the demos' patterns, compiled in)
    if (s && !(s.used || s.trigs)) s = { ...s, empty: true };
    if (s && !s.empty) used++;
    pats.push(s && !s.empty ? s : null);
  }
  let pool = 0;
  const empty = poolStartsEmpty(m, p);
  for (let n = 1; n <= NPOOL; n++) if (there && poolEntry(m, p, n, empty)) pool++;
  return { p, there, name: there ? cstr(h, 2, NAME - 1) : "", demo, pats, used, pool };
}
export const projects = (m) => Array.from({ length: limits(m).nproj }, (_, p) => project(m, p));

// a pool place: {id} (a reference, 'R' 1), {rec} (a copy, 0.7's 'S'), or null (empty, 'E' 1, none). A project whose
// pool did not start empty (the demos', 0.6's: HDR's pool_empty byte, persist.c proj_read) holds factory preset n in
// places 1..23 that have no record (qd_sounds_init)
const HDR_POOL_EMPTY = 2 + NAME + 7 + 7 * NTRACK + 1;
function poolStartsEmpty(m, p) { const h = m.prj.get(rkey(RK.HDR, p, 0)); return !!(h && h.length > HDR_POOL_EMPTY && h[HDR_POOL_EMPTY]); }
function poolEntry(m, p, n, empty = poolStartsEmpty(m, p)) {
  const r = m.prj.get(rkey(RK.SND, p, n));
  if (!r) return !empty && n <= NFACTORY ? { id: n } : null;
  if (r[0] === 0x45) return null;
  if (r.length === 4 && r[0] === 0x52 && r[1] === 1) return { id: g16(r, 2) };
  return { rec: r };
}

// ---- MY PRESETS: their records by id; a preset's match is its name and packed sound (persist.c tsound_ref)
function libIndex(m) {
  const byId = new Map(), free = [];
  for (let i = 0; i < limits(m).nlib; i++) {
    const r = m.lib.get(rkey(RK.LIB, 0, i));
    if (r && r.length === LIBLEN && r[0] === 0x53 && r[1] === 5) byId.set(g16(r, 2 + NAME + BLOB), { i, r });
    else free.push(i);
  }
  return { byId, free };
}
const sameSound = (a, b) => { for (let i = 2; i < 2 + NAME + BLOB; i++) if (a[i] !== b[i]) return false; return true; };
function nextId(m, ix) {
  let n = MINE0;
  for (const id of ix.byId.keys()) n = Math.max(n, id + 1);
  const f = m.lib.get(rkey(RK.FTAG, 0, 0));
  if (f && f[0] === 0x46 && f[1] === 2 && f.length >= 2 + NFACTORY + 2) n = Math.max(n, g16(f, 2 + NFACTORY));
  return n;
}
// the ids a clip or a kept project names, made the target store's: kept, linked to the same sound, or added; with
// MY PRESETS full, lost (-> lost: id -> its record: the callers write the sound as the project's own copy). The ids
// the patterns and the pool play (need) get the free places first, those only named as a copy's origin after
function relink(m, presets, need = null) {
  const map = new Map(), lost = new Map(), ix = libIndex(m);
  let added = 0;
  const order = [...presets].sort((a, b) => (need ? (need.has(b[0]) ? 1 : 0) - (need.has(a[0]) ? 1 : 0) : 0));
  for (const [id, rec] of order) {
    if (id <= NFACTORY) { map.set(id, id); continue; }
    const here = ix.byId.get(id);
    if (here && sameSound(here.r, rec)) { map.set(id, id); continue; }
    const same = [...ix.byId.entries()].find(([, x]) => sameSound(x.r, rec));
    if (same) { map.set(id, same[0]); continue; }
    if (!ix.free.length) { lost.set(id, rec); continue; }
    const i = ix.free.shift(), nid = nextId(m, ix), r = rec.slice();
    p16(r, 2 + NAME + BLOB, nid);
    m.lib.set(rkey(RK.LIB, 0, i), r);
    ix.byId.set(nid, { i, r });
    map.set(id, nid);
    added++;
    const f = m.lib.get(rkey(RK.FTAG, 0, 0));
    if (f && f[0] === 0x46 && f[1] === 2 && f.length >= 2 + NFACTORY + 2 && g16(f, 2 + NFACTORY) <= nid) {
      const g = f.slice(); p16(g, 2 + NFACTORY, nid + 1); m.lib.set(rkey(RK.FTAG, 0, 0), g);
    }
  }
  return { map, added, lost };
}
// the ids a project's patterns play by reference and its pool references (relink's need)
function neededIds(pats, poolIds) {
  const need = new Set(poolIds);
  for (const b of pats) { const w = walkPat(b); if (w) w.refAt.forEach((o) => { const id = g16(b, o); if (id) need.add(id); }); }
  return need;
}
// a MY PRESETS record as a pool place's own copy ('S' 5 with no id: "a copy the library had no room for", persist.c
// snd_rec; the unit makes it a reference again when its project loads with room in the library)
export const copyRecord = (rec) => { const r = rec.slice(); p16(r, 2 + NAME + BLOB, 0); return r; };
// a pattern record's track sounds that name a lost preset (lost: id -> its MY PRESETS record) written as the track's own
// copy (the preset's packed sound and name, as de_pat reads a reference: the same sound, no id); a copy's origin id that
// is lost cleared -> {bytes, n: the track sounds made copies}
export function derefPat(b, lost) {
  const w = walkPat(b);
  if (!w || !lost.size) return { bytes: b, n: 0 };
  const ids = w.refAt.map((o) => g16(b, o));
  if (!ids.some((id) => lost.has(id)) && !w.idAt.some((o) => lost.has(g16(b, o)))) return { bytes: b, n: 0 };
  const head = b.slice(0, w.sndAt), parts = [head];
  let o = w.sndAt, n = 0;
  const cbl = w.v >= 3 ? BLOB : 96;
  for (let t = 0; t < NTRACK; t++) {
    const id = ids.length ? ids[t] : 0;
    if (id) {                                          // (by reference: its pool source and edited bytes)
      const two = b.slice(o, o + 2);
      o += 2;
      const rec = lost.get(id);
      if (!rec) { parts.push(two); continue; }
      const c = new Uint8Array(BLOB + 2 + NAME + 2);
      c.set(rec.subarray(2 + NAME, 2 + NAME + BLOB), 0);
      c.set(two, BLOB);
      c.set(rec.subarray(2, 2 + NAME), BLOB + 2);
      p16(head, w.refAt[t], 0);
      parts.push(c);
      n++;
      continue;
    }
    const len = cbl + 2 + NAME + (w.v >= 2 ? 2 : 0), c = b.slice(o, o + len);
    if (w.v >= 2 && lost.has(g16(c, len - 2))) p16(c, len - 2, 0);
    parts.push(c);
    o += len;
  }
  const out = new Uint8Array(parts.reduce((a, x) => a + x.length, 0));
  let k = 0;
  for (const x of parts) { out.set(x, k); k += x.length; }
  return { bytes: out, n };
}
// the MY presets a pattern record names (track references and copies' ids) and a pool's references
function presetsOf(m, bytesList, poolIds) {
  const ix = libIndex(m), out = new Map();
  const want = (id) => { if (id > NFACTORY && ix.byId.has(id)) out.set(id, ix.byId.get(id).r); };
  for (const b of bytesList) { const w = walkPat(b); if (w) [...w.refAt, ...w.idAt].forEach((o) => want(g16(b, o))); }
  for (const id of poolIds) want(id);
  return out;
}
// a pattern record with its pool numbers and preset ids changed (pmap: number -> number; imap: id -> id)
function remapPat(b, pmap, imap) {
  const w = walkPat(b);
  if (!w) return b;
  const r = b.slice();
  for (const o of w.soundAt) if (pmap.has(r[o])) r[o] = pmap.get(r[o]);
  for (const o of w.refAt) { const id = g16(r, o); if (id && imap.has(id)) p16(r, o, imap.get(id)); }
  for (const o of w.idAt) { const id = g16(r, o); if (id && imap.has(id)) p16(r, o, imap.get(id)); }
  return r;
}

// ---- patterns: copy, paste, clear
export function copyPatterns(m, p, ks) {
  const info = project(m, p), pats = [], pool = new Map();
  for (const k of ks) {
    const b = m.prj.get(rkey(RK.PAT, p, k));
    if (!b) continue;                                  // (an empty pattern, or a demo's compiled in: nothing to copy)
    pats.push({ k, bytes: b });
    const w = walkPat(b);
    if (w) for (const o of w.soundAt) { const n = b[o]; if (!pool.has(n)) pool.set(n, poolEntry(m, p, n)); }
  }
  const presets = presetsOf(m, pats.map((x) => x.bytes), [...pool.values()].filter((e) => e && e.id).map((e) => e.id));
  return { from: info.name || `PROJECT ${p + 1}`, project: p, names: pats.map((x) => patName(x.k)), pats, pool, presets };
}
// the clip's patterns from pattern j on, in their order (a range keeps its gaps: A03, A05 -> j, j + 2)
export function pastePatterns(m, q, j, clip) {
  if (!clip.pats.length) return { at: [], poolAdded: 0, presetsAdded: 0 };
  const k0 = clip.pats[0].k, at = clip.pats.map((x) => j + x.k - k0);
  if (at.some((k) => k < 0 || k >= NPAT)) throw err("range", `the patterns run past D16 from ${patName(j)}`);
  if (q < 0 || q >= limits(m).nproj) throw err("range", `slot ${q + 1}: this store has ${limits(m).nproj} projects`);
  const need = neededIds(clip.pats.map((x) => x.bytes), [...clip.pool.values()].filter((e) => e && e.id).map((e) => e.id));
  const { map: imap, added, lost } = relink(m, clip.presets, need);
  let copies = 0;
  const pmap = new Map();
  let poolAdded = 0;
  const free = [];
  for (let n = 1; n <= NPOOL; n++) if (!poolEntry(m, q, n)) free.push(n);
  for (const [n, e0] of clip.pool) {
    if (!e0) { pmap.set(n, 0); continue; }             // (a lock on an empty place: the track's sound, as FIZZ plays it)
    const e = e0.id && lost.has(e0.id) ? { rec: copyRecord(lost.get(e0.id)) } : e0;   // (MY PRESETS full: its own copy)
    if (e !== e0) copies++;
    let hit = 0;
    for (let x = 1; x <= NPOOL && !hit; x++) {
      const t = poolEntry(m, q, x);
      if (t && e.id && t.id === imap.get(e.id)) hit = x;
      else if (t && e.rec && t.rec && t.rec.length === e.rec.length && t.rec.every((v, i) => v === e.rec[i])) hit = x;
    }
    if (!hit) {
      if (!free.length) throw err("poolfull", "the pool is full (128)", { need: [...clip.pool.values()].filter(Boolean).length });
      hit = free.shift();
      if (e.id) { const r = new Uint8Array(4); r[0] = 0x52; r[1] = 1; p16(r, 2, imap.get(e.id) ?? e.id); m.prj.set(rkey(RK.SND, q, hit), r); }
      else m.prj.set(rkey(RK.SND, q, hit), e.rec);
      poolAdded++;
    }
    pmap.set(n, hit);
  }
  clip.pats.forEach((x, i) => {
    const d = derefPat(remapPat(x.bytes, pmap, imap), lost);
    copies += d.n;
    m.prj.set(rkey(RK.PAT, q, at[i]), d.bytes);
  });
  return { at, poolAdded, presetsAdded: added, copies };
}
// cleared: no record, as a pattern never written. A demo project's A01..A03 without a record play the demos (compiled
// in): those are refused ({code: "demo"}), nothing changed
export function clearPatterns(m, q, ks) {
  if (project(m, q).demo && ks.some((k) => k < DEMO_N)) throw err("demo", "a demo project's A01..A03 cannot be cleared here");
  for (const k of ks) m.prj.delete(rkey(RK.PAT, q, k));
}

// ---- a project kept in this browser, and brought back
export function keepProject(m, p) {
  const info = project(m, p);
  if (!info.there) throw err("empty", `slot ${p + 1} is empty`);
  const recs = keysOf(m, p).map((k) => ({ kind: kindOf(k), i: idxOf(k), bytes: m.prj.get(k) }));
  const pools = [];
  for (let n = 1; n <= NPOOL; n++) { const e = poolEntry(m, p, n); if (e && e.id) pools.push(e.id); }
  const presets = presetsOf(m, recs.filter((r) => r.kind === RK.PAT).map((r) => r.bytes), pools);
  return { name: info.name, demo: info.demo, recs, presets, pats: info.used, sounds: info.pool, colours: colours(presets) };
}
export function loadProject(m, q, kept) {
  const { nproj } = limits(m);
  if (q < 0 || q >= nproj) throw err("range", `slot ${q + 1}: this store has ${nproj} projects`);
  for (const k of keysOf(m, q)) m.prj.delete(k);
  const isRef = (b) => b.length === 4 && b[0] === 0x52 && b[1] === 1;
  const need = neededIds(kept.recs.filter((r) => r.kind === RK.PAT).map((r) => r.bytes),
    kept.recs.filter((r) => r.kind === RK.SND && isRef(r.bytes)).map((r) => g16(r.bytes, 2)));
  const { map: imap, added, lost } = relink(m, kept.presets, need);
  const same = new Map();                               // (the pool numbers stay: the whole pool comes along)
  let copies = 0;
  for (const r of kept.recs) {
    let b = r.bytes;
    if (r.kind === RK.PAT) { const d = derefPat(remapPat(b, same, imap), lost); b = d.bytes; copies += d.n; }
    else if (r.kind === RK.SND && isRef(b) && lost.has(g16(b, 2))) { b = copyRecord(lost.get(g16(b, 2))); copies++; }
    else if (r.kind === RK.SND && isRef(b) && imap.has(g16(b, 2))) { b = b.slice(); p16(b, 2, imap.get(g16(b, 2))); }
    m.prj.set(rkey(r.kind, q, r.i), b);
  }
  return { presetsAdded: added, copies };
}

// ---- a store in another package's layout (a full backup restored across packages: USB 8 projects and 256 MY PRESETS,
// Bluetooth 4 and 128), record by record. The projects chosen go to slots 1.. in their order, the MY presets chosen to
// places 1.. in theirs (their ids kept: what names them by id still finds them); a track sound or a pool place that
// names a preset left out becomes the project's own copy (derefPat, copyRecord: it sounds the same). The settings
// record's project follows its project (the first slot when it was left out).
export function fitChoice(m, to, { projects: ps = null } = {}) {
  to = layoutByName(to);
  const there = projects(m).filter((x) => x.there).map((x) => x.p);
  const chosen = (ps || there).slice(0, to.nproj);
  const ix = libIndex(m), placeOf = new Map([...ix.byId].map(([id, x]) => [id, x.i]));
  const used = [];
  for (const p of chosen) {
    const recs = keysOf(m, p).map((k) => ({ k, b: m.prj.get(k) }));
    const ids = neededIds(recs.filter((r) => kindOf(r.k) === RK.PAT).map((r) => r.b),
      recs.filter((r) => kindOf(r.k) === RK.SND && r.b.length === 4 && r.b[0] === 0x52).map((r) => g16(r.b, 2)));
    for (const id of ids) if (placeOf.has(id) && !used.includes(placeOf.get(id))) used.push(placeOf.get(id));
  }
  const all = [...ix.byId.values()].map((x) => x.i).sort((a, b) => a - b);
  const presets = [...used.sort((a, b) => a - b), ...all.filter((i) => !used.includes(i))].slice(0, to.nlib);
  return { projects: chosen, presets };
}
export function fitModel(m, to, choice = null) {
  to = layoutByName(to);
  const c = choice || fitChoice(m, to);
  if (c.projects.length > to.nproj || c.presets.length > to.nlib)
    throw err("range", `the ${PACKAGE_NAME[to.package]} package holds ${to.nproj} projects and ${to.nlib} MY PRESETS`);
  const out = { layout: to, magic: to.magic, lib: new Map(), prj: new Map() };
  const ix = libIndex(m), lost = new Map();
  for (const [k, b] of m.lib) if (kindOf(k) !== RK.LIB) out.lib.set(k, b);   // (the factory tags, the next id)
  c.presets.forEach((i, n) => out.lib.set(rkey(RK.LIB, 0, n), m.lib.get(rkey(RK.LIB, 0, i))));
  for (const [id, x] of ix.byId) if (!c.presets.includes(x.i)) lost.set(id, x.r);
  let copies = 0;
  c.projects.forEach((p, q) => {
    for (const k of keysOf(m, p)) {
      let b = m.prj.get(k);
      if (kindOf(k) === RK.PAT) { const d = derefPat(b, lost); b = d.bytes; copies += d.n; }
      else if (kindOf(k) === RK.SND && b.length === 4 && b[0] === 0x52 && b[1] === 1 && lost.has(g16(b, 2))) { b = copyRecord(lost.get(g16(b, 2))); copies++; }
      out.prj.set(rkey(kindOf(k), q, idxOf(k)), b);
    }
  });
  const set = m.prj.get(rkey(RK.SET, 0, 0));
  if (set) {
    const g = set.slice(), q = set.length > 3 ? c.projects.indexOf(set[2]) : 0;
    if (set.length > 3) { g[2] = Math.max(0, q); if (q < 0) g[3] = 0; }
    out.prj.set(rkey(RK.SET, 0, 0), g);
  }
  const all = projects(m);
  return { model: out, copies, left: { projects: all.filter((x) => x.there && !c.projects.includes(x.p)).map((x) => ({ slot: x.p + 1, name: x.name })),
    presets: [...ix.byId.entries()].filter(([, x]) => !c.presets.includes(x.i)).map(([id, x]) => ({ slot: x.i + 1, id, name: (presetRec(x.r, x.i + 1) || {}).name || "?" })) } };
}
// a kept project's name (HDR 'H' 1: name[13] after the two bytes)
export function renameKept(kept, name) {
  const h = kept.recs.find((r) => r.kind === RK.HDR);
  if (!h) return kept;
  const b = h.bytes.slice(), s = cleanName(name);
  for (let i = 0; i < NAME; i++) b[2 + i] = i < s.length ? s.charCodeAt(i) : 0;
  h.bytes = b;
  kept.name = s;
  return kept;
}
// (FIZZ's names: core/app/fizz_sx.c name_ok's letters, twelve at most)
export const cleanName = (s) => String(s || "").toUpperCase().replace(/[^A-Z0-9\-.,'&!?#+/()*:_~ ]/g, "").slice(0, NAME - 1).trim();
// the tag colours of a project's sounds (the kept cards' dots)
const TAGC = { BASS: "--blue", KEYS: "--cream", PLUCK: "--yellow", PAD: "--violet", LEAD: "--green", DRUMS: "--orange", FX: "--pink" };
function colours(presets) {
  const out = [];
  for (const r of presets.values()) { const t = r[2 + NAME + BLOB + 2] & 0x7F; const k = FIZZ_TAGS.findIndex((_, i) => (t >> i) & 1); out.push(k >= 0 ? TAGC[FIZZ_TAGS[k]] || "--muted" : "--muted"); }
  return out.slice(0, 12);
}

// ---- files: .fizzprj (a kept project), .fizzpat (patterns): ZIPs as .fizzbak, the records as the unit holds them
export const PRJ_FORMAT = "fizz-project", PAT_FORMAT = "fizz-patterns", FILE_VERSION = 1;
const enc = new TextEncoder(), dec = new TextDecoder();
const hex = (n, w = 8) => (n >>> 0).toString(16).padStart(w, "0");
const safe = (s) => String(s || "").replace(/[\\/:*?"<>|\x00-\x1f]+/g, "_").trim() || "PRESET";
function presetFiles(presets, fizz) {
  const files = [], list = [];
  for (const [id, r] of presets) {
    const s = presetRec(r, 0);
    const base = `presets/${id} ${safe(s ? s.name : "")}`;
    files.push({ name: `${base}.rec`, bytes: r });
    if (s) files.push({ name: `${base}.fizzsnd`, bytes: enc.encode(soundToFizzsnd({ fizz: true, name: s.name, blob: s.blob, drive: s.drive, pan: s.pan,
      sends: s.sends, fizzTags: FIZZ_TAGS.filter((_, k) => (s.tags >> k) & 1), favourite: false, notes: [], origin: [], mark: "", tags: [] }, { fizz }) + "\n") });
    list.push({ id, name: s ? s.name : "", file: `${base}.rec` });
  }
  return { files, list };
}
export function makeFizzprj(kept, { fizz = "", date = new Date() } = {}) {
  const recs = kept.recs.map((r) => ({ kind: r.kind, i: r.i, file: `records/${hex(rkey(r.kind, 0, r.i))}.rec`, bytes: r.bytes }));
  const pf = presetFiles(kept.presets, fizz);
  const manifest = { format: PRJ_FORMAT, version: FILE_VERSION, name: kept.name, demo: kept.demo, date: date.toISOString(), fizz,
    patterns: kept.pats, sounds: kept.sounds, records: recs.map(({ kind, i, file }) => ({ kind, i, file })), presets: pf.list };
  return zipStored([{ name: "manifest.json", bytes: enc.encode(JSON.stringify(manifest, null, 2) + "\n") },
    ...recs.map((r) => ({ name: r.file, bytes: r.bytes })), ...pf.files], date);
}
async function readZipJson(bytes, format, file) {
  const zip = await dnReadZip(bytes).catch(() => { throw new Error(`${file || "this file"}: not a ZIP`); });
  const mt = await zip.get("manifest.json");
  let m = null;
  try { m = JSON.parse(dec.decode(mt)); } catch (_) { /* below */ }
  if (!m || m.format !== format) throw new Error(`${file || "this file"}: not a FIZZ ${format === PRJ_FORMAT ? "project" : "patterns"} file`);
  if (m.version > FILE_VERSION) throw new Error(`${file || "this file"}: a newer file (version ${m.version}): update this page`);
  const need = async (n) => { const d = await zip.get(n); if (!d) throw new Error(`${file || "this file"}: ${n} is missing`); return new Uint8Array(d); };
  const presets = new Map();
  for (const x of m.presets || []) presets.set(x.id, await need(x.file));
  return { m, need, presets };
}
export async function readFizzprj(bytes, file = "") {
  const { m, need, presets } = await readZipJson(bytes, PRJ_FORMAT, file);
  const recs = [];
  for (const r of m.records || []) recs.push({ kind: r.kind, i: r.i, bytes: await need(r.file) });
  if (!recs.some((r) => r.kind === RK.HDR)) throw new Error(`${file || "this file"}: no project header`);
  return { name: m.name, demo: !!m.demo, recs, presets, pats: m.patterns | 0, sounds: m.sounds | 0, colours: colours(presets), fizz: m.fizz || "", date: m.date };
}
export function makeFizzpat(clip, { fizz = "", date = new Date() } = {}) {
  const pats = clip.pats.map((x) => ({ k: x.k, name: patName(x.k), file: `patterns/${patName(x.k)}.rec`, bytes: x.bytes }));
  const pool = [...clip.pool].map(([n, e], i) => (e && e.rec ? { n, file: `pool/${n}.rec`, bytes: e.rec } : { n, id: e ? e.id : 0 }));
  const pf = presetFiles(clip.presets, fizz);
  const manifest = { format: PAT_FORMAT, version: FILE_VERSION, from: clip.from, date: date.toISOString(), fizz,
    patterns: pats.map(({ k, name, file }) => ({ k, name, file })), pool: pool.map(({ n, id, file }) => (file ? { n, file } : { n, id })), presets: pf.list };
  return zipStored([{ name: "manifest.json", bytes: enc.encode(JSON.stringify(manifest, null, 2) + "\n") },
    ...pats.map((x) => ({ name: x.file, bytes: x.bytes })), ...pool.filter((x) => x.file).map((x) => ({ name: x.file, bytes: x.bytes })), ...pf.files], date);
}
export async function readFizzpat(bytes, file = "") {
  const { m, need, presets } = await readZipJson(bytes, PAT_FORMAT, file);
  const pats = [];
  for (const x of m.patterns || []) pats.push({ k: x.k, bytes: await need(x.file) });
  pats.sort((a, b) => a.k - b.k);
  const pool = new Map();
  for (const x of m.pool || []) pool.set(x.n, x.file ? { rec: await need(x.file) } : x.id ? { id: x.id } : null);
  return { from: m.from || "", project: -1, names: pats.map((x) => patName(x.k)), pats, pool, presets };
}
export const patFileName = (clip) => `${safe(clip.from)} ${clip.names.length > 1 ? `${clip.names[0]}-${clip.names[clip.names.length - 1]}` : clip.names[0] || ""}.fizzpat`;
export const prjFileName = (kept) => `${safe(kept.name)}.fizzprj`;
