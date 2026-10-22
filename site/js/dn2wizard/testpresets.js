// SPDX-License-Identifier: GPL-3.0-only
// Three FIZZ sounds written as Digitone II presets (fizz2dn.js), for the wizard's owner to load by hand: does a
// FIZZ-made .dn2pst load, show its name and tags, and sound like this description?
//   1 FIZZ TEST 1: a plain tone (INIT, LEVEL 100)
//   2 FIZZ TEST 2: a bass (an LP24 filter that closes, FM on the attack)
//   3 FIZZ TEST 3: the newer fields: AHD with a hold, LFO 3 on HARM, portamento on, an LFO on the filter
import { fizzToDn2pst, defaults, Q } from "./fizz2dn.js";

const lfo = (k, f) => 47 + 8 * k + f;
function patch(edits) { const v = defaults(); for (const [i, x] of edits) v[i] = x; return v; }
export const TEST_PRESETS = [
  { name: "FIZZ TEST 1", tags: ["KEYS"], hint: "a plain, steady tone", values: patch([[Q.LEVEL, 100]]) },
  { name: "FIZZ TEST 2", tags: ["BASS"], hint: "a bass: bright attack, closing low-pass",
    values: patch([[Q.ALGO, 1], [Q.RC, 3], [Q.RA, 3], [Q.ALEV, 70], [Q.ADEC, 40], [Q.FDBK, 20], [Q.FTYPE, 3], [Q.FREQ, 6000],
      [Q.RESO, 3000], [Q.FATK, 0], [Q.FDEC, 50], [Q.FSUS, 0], [Q.FDEPTH, 2000], [Q.EREL, 20], [Q.LEVEL, 100]]) },
  { name: "FIZZ TEST 3", tags: ["PAD", "FX"], hint: "a held pad: holds while the key is down, LFO movement, glides between notes",
    values: patch([[Q.ALGO, 3], [Q.BLEV, 60], [Q.AMOD, 1], [Q.HOLD, 127], [Q.EDEC, 60], [Q.PORT, 1], [Q.PTIM, 50],
      [lfo(0, 3), 25], [lfo(0, 7), 2000], [lfo(2, 3), 11], [lfo(2, 7), 1500], [Q.LEVEL, 100]]) },
];
export const testPresetFiles = () => TEST_PRESETS.map((t) => ({ ...t, ...fizzToDn2pst({ values: t.values, name: t.name, tags: t.tags, pan: 0, sends: { reverb: 20 } }) }));
