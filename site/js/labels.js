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
//     reply. Until FIZZ's format lands, encode() is null and nothing is sent; stub (a function of the panel's held
//     controls, below) stands in where the page asks for it (?labels=stub, the tests).
//   stubLabels(held) -> labels   PLAYING.md's step view, nothing held, and its SHIFT layer: a fixed stand-in

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

// ---- FIZZ's LABELS SysEx (the adapter): filled when docs/SYSEX.md defines it
export const LABELS_READY = false;
export function encodeRequest(subscribe) { void subscribe; return null; }   // -> Uint8Array (F0 .. F7), or null
export function decodeLabels(bytes) { void bytes; return null; }          // a reply -> labels, or null (not ours)

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
