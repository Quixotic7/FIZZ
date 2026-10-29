// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
// LIBRARY's USED BY (FIZZ NEON): where a MY preset plays in a store's projects, pattern by pattern and track by track,
// with that track's 16 steps (the first page where it plays). A track plays the preset when its sound names it (by
// reference, or a copy made from it); a step's sound lock plays the pool place's sound instead (a lock on an empty place
// plays the track's sound, as FIZZ does). No DOM: the tests run it in Node.
//   usedBy(model, id, {max}) -> {rows: [{p, project, pat, k, track, page, steps: [{trig, plays}] x 16, plays}], more, total}
import { project, walkPat, poolEntry, rkey, RK, patName, limits, NPAT } from "./projects.js";

const NAME = 13, BLOB = 128;
const g16 = (b, o) => b[o] | (b[o + 1] << 8);
// a pool place's sound is the preset: a reference to it, or a copy whose id is it
const entryIs = (e, id) => !!e && (e.id === id || (!!e.rec && e.rec.length === 2 + NAME + BLOB + 8 && e.rec[0] === 0x53 && g16(e.rec, 2 + NAME + BLOB) === id));

export function usedBy(m, id, { max = 8 } = {}) {
  const rows = [];
  if (!m || !id) return { rows, more: 0, total: 0 };
  for (let p = 0; p < limits(m).nproj; p++) {
    const info = project(m, p);
    if (!info.there) continue;
    const pool = new Map(), poolIs = (n) => { if (!pool.has(n)) pool.set(n, poolEntry(m, p, n)); return pool.get(n); };
    for (let k = 0; k < NPAT; k++) {
      const b = m.prj.get(rkey(RK.PAT, p, k)), w = b && walkPat(b);
      if (!w) continue;
      for (let t = 0; t < w.trk.length; t++) {
        const own = w.trk[t].ref === id || (!w.trk[t].ref && w.trk[t].id === id);
        const plays = (pg, s) => {
          if (!((w.pages[t][pg] >> s) & 1)) return false;
          const n = w.locks[t][pg][s];
          if (!n) return own;
          const e = poolIs(n);
          return e ? entryIs(e, id) : own;
        };
        let page = -1, total = 0;
        for (let pg = 0; pg < 8; pg++) for (let s = 0; s < 16; s++) if (plays(pg, s)) { total++; if (page < 0) page = pg; }
        if (!total) continue;
        rows.push({ p, project: info.name || `PROJECT ${p + 1}`, pat: patName(k), k, track: t + 1, page, plays: total,
          steps: Array.from({ length: 16 }, (_, s) => ({ trig: !!((w.pages[t][page] >> s) & 1), plays: plays(page, s) })) });
      }
    }
  }
  return { rows: rows.slice(0, max), more: Math.max(0, rows.length - max), total: rows.length };
}
