// SPDX-License-Identifier: GPL-3.0-only
// FIZZ's live labels: what every control does right now (FIZZ sends them: its LABELS SysEx, docs/SYSEX.md, being
// defined), so the drawn panel can name each knob, button and key, and flip to the shortcut layer while SHIFT is held.
// No DOM: site/tools/test_site.mjs runs it in Node too.
//
// The labels (one object, the shape FIZZ's reply will have):
//   { enc: [7] matrix order (SELECT, ALGORITHM, KNOB 1..4, PRESETS), master: "", buttons: [14] matrix order (OCT-, OCT+,
//     FX .. REC), keys: [27] (F3 .. G5) }   "" = the control does nothing now
//
//   createLabelsLink({ sendMidi, onMidiOut, onLabels, stub }) -> { request(subscribe), ready }
//     the adapter: request() asks FIZZ (subscribe: FIZZ sends again whenever they change); onLabels(labels) on each
//     reply (subscribed: FIZZ sends them again whenever they change, at most 20 a second). stubLabels (below) stands in
//     where the page asks for it (?labels=stub: the tests of the layer itself).
//   stubLabels(held) -> labels   PLAYING.md's step view, nothing held, and its SHIFT layer: a fixed stand-in

import { labelsRequest, parseLabels } from "./fizz_sysex.js";

export const NENC = 7, NBTN = 14, NKEY = 27;
export const empty = () => ({ enc: Array(NENC).fill(""), master: "", buttons: Array(NBTN).fill(""), keys: Array(NKEY).fill("") });

// ---- the stand-in (PLAYING.md "Getting around", "SHIFT's keys"): held = { shift: bool }
const BLACK = [1, 3, 5, 8, 10, 13, 15, 17, 20, 22, 25];        // B1..B11 as keys (F#3 .. F#5)
const WHITE = [0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26];
const MODES = ["NOTE", "VEL", "LEN", "MICRO", "RATCH", "CHNC", "COND", "TRIG", "FUNC", "SOUND", "P-LOCK"];
// buttons in matrix order: 0 OCT- 1 OCT+ 2 FX 3 SEL 4 ENV 5 LFO 6 EDIT 7 GLO 8 HOME 9 SAVE 10 ARP 11 SEQ 12 PLAY 13 REC
const BASE_BTN = ["COPY", "MOVE", "EFFECTS", "TRACKS", "MIX", "LATCH", "EDITOR", "SHIFT", "HOME", "SOUNDS", "FILL", "PATTERNS", "PLAY", "KEYS"];
const SHIFT_BTN = ["UNDO", "REDO", "", "GLOBAL", "", "", "", "SHIFT", "VIEWS", "", "", "", "", "REC SET"];
const SHIFT_BLACK = ["SNAPSHOT", "", "RECALL", "COPY PAT", "PASTE PAT", "TAP TEMPO", "CLICK", "FOLLOW", "DOUBLE", "", "CLEAR TRK"];   // (P1.5z: B8 page follow)
export function stubLabels(held = {}) {
  const l = empty();
  if (held.shift) {
    l.enc = ["TRACK", "SCALE", "FINE", "FINE", "FINE", "FINE", "ROOT"];   // (P1.5z: SHIFT + SELECT the track)
    l.master = "VOLUME";
    l.buttons = SHIFT_BTN.slice();
    BLACK.forEach((k, i) => { l.keys[k] = SHIFT_BLACK[i]; });
    return l;
  }
  l.enc = ["PAGE", "CURSOR", "NOTE 2", "NOTE 3", "NOTE 4", "NOTE 5", "SOUND"];
  l.master = "VOLUME";
  l.buttons = BASE_BTN.slice();
  BLACK.forEach((k, i) => { l.keys[k] = MODES[i]; });
  WHITE.forEach((k, i) => { l.keys[k] = `STEP ${i + 1}`; });
  return l;
}

// ---- FIZZ's LABELS SysEx (docs/SYSEX.md section 20, P1.6l): fizz_sysex.js's labelsRequest / parseLabels. Its 49
// labels come in panel order (the knobs SELECT ALGORITHM PRESETS K1..K4 MASTER, the buttons as printed FX .. REC F1
// F2, W1..W16, B1..B11): here they become the shape above (matrix orders, the keys by note)
export const LABELS_READY = true;
const ROLE_MATRIX = [0, 1, 6, 2, 3, 4, 5];                       // (panel.js roleToMatrix: knob role -> matrix encoder)
const LABEL_MATRIX = (b) => (b < 12 ? b + 2 : b - 12);           // (panel.js labelToMatrix: printed button -> matrix)
export function encodeRequest(subscribe) { return labelsRequest({ subscribe }); }
export function decodeLabels(bytes) {
  const p = parseLabels(bytes);
  if (!p || p.list.length !== 49) return null;
  const l = empty(), at = (i) => p.list[i] || "";
  for (let r = 0; r < 7; r++) l.enc[ROLE_MATRIX[r]] = at(r);
  l.master = at(7);
  for (let b = 0; b < 14; b++) l.buttons[LABEL_MATRIX(b)] = at(8 + b);
  WHITE.forEach((k, i) => { l.keys[k] = at(22 + i); });
  BLACK.forEach((k, i) => { l.keys[k] = at(38 + i); });
  l.count = p.count;
  return l;
}

export function createLabelsLink({ sendMidi, onMidiOut, onLabels }) {
  let sx = null;
  onMidiOut((bytes) => {
    for (const b of bytes) {
      if (b === 0xf0) sx = [b];
      else if (sx && b === 0xf7) { sx.push(b); const l = decodeLabels(Uint8Array.from(sx)); sx = null; if (l) onLabels(l); }
      else if (sx && b < 0x80) sx.push(b);
      else if (sx && b < 0xf8) sx = null;
    }
  });
  return {
    ready: LABELS_READY,
    request(subscribe = true) { const m = encodeRequest(subscribe); if (m) sendMidi(m); return !!m; },
  };
}
