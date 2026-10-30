// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
// EDIT's drag to modulate (design 13, the owner 2026-10-30): a source (LFO 1..3's plug, or one of the MOD slots'
// sources VEL KEY MW AT PB BR RAND F ENV) dropped on a value routes it there. No DOM: the editor draws, Node tests.
//   - the destinations (the core's QD_* list, fizzvalues.js DEST) and the editor's value each one moves (DEST_TILE;
//     the ones without a value of their own, PITCH P AB2 R ALL and the A+B macros, are PADS);
//   - what a source reaches (reaches: the one rule): the core runs LFO 2, 3, then LFO 1, then the MOD slots
//     (eng_quad.c quad_render): LFO 1's own values (L1SPD .. L1DEP) move only with LFO 2 or 3 or a MOD slot (the MOD
//     slots' reach into LFO 1 is the firmware fix on its own branch); LFO 1 never reaches its own seven;
//   - a drop's plan: an LFO takes the value as its one DEST, its DEPTH kept as it is; a MOD source fills the first free
//     slot at +40; a value the source already moves opens that route's card; every slot used: FULL, nothing changed.
//   reaches(src, dest), routes(values), freeSlot(values), plan(values, src, dest), routeOf(values, src, dest)
//   src: { kind: "lfo", k: 0..2 } | { kind: "mod", src: 1..8 }
import { VALUES } from "./fizzvalues.js";

const lfo = (k, f) => 47 + 8 * k + f, mod = (k, f) => 86 + 3 * k + f;
const L_DEST = 3, L_DEPTH = 7;
export const SRC_NAMES = VALUES[86][5];                   // OFF VEL KEY MW AT PB BR RAND F ENV
export const DEST_NAMES = VALUES[50][5];                  // NONE PITCH .. SRR (0..63)
export const NDEST = DEST_NAMES.length;                   // 64
// each MOD source its colour (the owner's call 7), the LFOs theirs (core/ui ui_kit.c UI_LFO_COL)
export const SRC_COLOURS = [null, "--pink", "--green", "--blue", "--tangerine", "--red", "--champagne", "--magenta", "--orange"];
export const LFO_COLOURS = ["--cyan", "--yellow", "--violet"];
export const NEW_DEPTH = 40;                              // (a new MOD slot's route)
// a destination's value(s) on the panel (RAT B: B1's and B2's ratios)
export const DEST_TILE = { 3: [0], 4: [1], 5: [2], 6: [3, 71], 7: [8], 8: [9], 9: [10], 10: [11], 11: [4], 12: [5], 13: [6], 14: [7], 15: [15], 16: [19],
  17: [12], 18: [13], 19: [14], 20: [16], 21: [17], 22: [18], 23: [20], 24: [24], 25: [34], 26: [35], 27: [37], 28: [40], 29: [41], 30: [30], 31: [31],
  32: [32], 33: [33], 34: [42], 35: [43], 36: [44], 37: [45], 38: [46], 39: [115], 40: [47], 41: [48], 42: [49], 43: [51], 44: [52], 45: [53], 46: [54],
  53: [73], 54: [117], 55: [116], 56: [118], 57: [38], 58: [75], 59: [77], 60: [80], 61: [81], 62: [83], 63: [84] };
// the destinations with no value of their own: a pad shown while a source is dragged (the A+B macros: both envelopes)
export const PADS = { 1: "PITCH", 2: "P AB2", 47: "R ALL", 48: "LEV", 49: "ATK", 50: "DEC", 51: "END", 52: "DLY" };
export const TILE_DEST = {};
for (const [d, ids] of Object.entries(DEST_TILE)) for (const id of ids) TILE_DEST[id] = +d;
export const L1_OWN = [40, 41, 42, 43, 44, 45, 46];      // L1SPD L1MUL L1FAD L1WAV L1SPH L1MOD L1DEP

// what a source reaches: every destination but NONE; LFO 1 not its own values
export const reaches = (s, d) => Number.isInteger(d) && d > 0 && d < NDEST && !(s && s.kind === "lfo" && s.k === 0 && L1_OWN.includes(d));
export const srcColour = (s) => (s.kind === "lfo" ? LFO_COLOURS[s.k] : SRC_COLOURS[s.src]);
export const srcName = (s) => (s.kind === "lfo" ? `LFO ${s.k + 1}` : SRC_NAMES[s.src]);
export const sameSrc = (a, b) => !!(a && b) && a.kind === b.kind && (a.kind === "lfo" ? a.k === b.k : a.src === b.src);

// every route the sound has: { kind, k, src?, dest, depth, depthId, c, name }
export function routes(values) {
  const out = [];
  for (let k = 0; k < 3; k++) {
    const d = values[lfo(k, L_DEST)];
    if (d > 0) out.push({ kind: "lfo", k, dest: d, depth: values[lfo(k, L_DEPTH)], depthId: lfo(k, L_DEPTH), c: LFO_COLOURS[k], name: `LFO ${k + 1}` });
  }
  for (let k = 0; k < 8; k++) {
    const s = values[mod(k, 0)], d = values[mod(k, 1)];
    if (s > 0 && d > 0) out.push({ kind: "mod", k, src: s, dest: d, depth: values[mod(k, 2)], depthId: mod(k, 2), c: SRC_COLOURS[s], name: SRC_NAMES[s] });
  }
  return out;
}
// the route a source already has to a destination (null: none)
export const routeOf = (values, s, d) => routes(values).find((r) => r.dest === d && (s.kind === "lfo" ? r.kind === "lfo" && r.k === s.k : r.kind === "mod" && r.src === s.src)) || null;
// the first MOD slot not in use (its SOURCE OFF), -1 when all eight are
export const freeSlot = (values) => { for (let k = 0; k < 8; k++) if (!values[mod(k, 0)]) return k; return -1; };
export const isFull = (values) => freeSlot(values) < 0;
// a drop of source s on destination d -> { act: "no" } | { act: "card", route } | { act: "full" } | { act: "set", sets: [[id, value], ..] }
export function plan(values, s, d) {
  if (!reaches(s, d)) return { act: "no" };
  const r = routeOf(values, s, d);
  if (r) return { act: "card", route: r };
  if (s.kind === "lfo") return { act: "set", sets: [[lfo(s.k, L_DEST), d]] };   // (its DEPTH as it is)
  const k = freeSlot(values);
  if (k < 0) return { act: "full" };
  return { act: "set", sets: [[mod(k, 0), s.src], [mod(k, 1), d], [mod(k, 2), NEW_DEPTH]] };
}
// a route taken off: an LFO's DEST NONE; a MOD slot's SOURCE OFF (as its × in the MOD area)
export const removeSets = (r) => (r.kind === "lfo" ? [[lfo(r.k, L_DEST), 0]] : [[mod(r.k, 0), 0]]);
// the route's card's title: "MOD 1" / "LFO 2", and "VEL → FREQ"
export const routeTitle = (r) => ({ who: r.kind === "lfo" ? "LFO" : `MOD ${r.k + 1}`, what: `${r.name} → ${DEST_NAMES[r.dest]}` });

// the filter's TYPE as one 8-way choice: TYPE's own stored values (0..7), in the core's order
export const FILTER_TYPES = [[0, "OFF"], [1, "LP12"], [2, "HP12"], [3, "LP24"], [4, "MULTI"], [5, "COMB+"], [6, "COMB−"], [7, "EQ"]];
