// SPDX-License-Identifier: GPL-3.0-only
// PLAY's pure parts (play.js draws and plays them; no DOM here, so Node tests them): the iOS app's PLAY mirrored
// (ios/FizzKit/Sources/FizzUI/PlayTheme.swift, PlayModel.swift, PlayParts.swift, PlayView.swift):
//   - the six themes' palettes (FIZZ first, black and neon; Tangerine, Midnight, Mint, Lilac, Cream);
//   - the views (FULL, PANEL, KEYS, SCREEN; a swipe on the screen moves between them, PANEL and KEYS one place) and
//     each layout's parts in board units (the parity mock-ups' boards: 1080 x 600 on a computer, 1240 x 540 a phone on
//     its side, 358 wide upright);
//   - the LED keybed (KeyLayout: 16 white keys in a row, the 11 black keys above them; a window of them scrolled),
//     the velocity from where a key is touched (soft at its far end, hard at its near end), the knobs' detents from a
//     drag (14 px each, up and right clockwise) and MASTER's absolute value (1023 over 220 px);
//   - the FM-1's remote (core/app/midi_map.h, THE REMOTE, channel 16): CC 20..33 the 14 buttons in QD_B_* order (127
//     pressed, 0 let go, on a change only), CC 40..46 the 7 encoders relative (64 + n), CC 7 MASTER (0..127, sent
//     only when it moved), CC 123 everything let go; the keys as notes on the selected track's own channel.
//   layoutOf(name, { knobs, offset }) -> { W, H, parts }; keyLayout({ w, h, visible, offset, gap }); velocity(depth);
//   new Detents().move(dx, dy); masterFrom(start, dx, dy); swiped(view, d); new RemoteOut(send)

// ---------------------------------------------------------------- the themes (PlayTheme.swift's palettes)
// body, label, bed, key, hi, lo, keyText, knob, mark (MASTER / PRESETS), bezel, led, ledOff; ledDim (a step's trig, a
// lit button: the web's FIZZ theme shows them cream, as the mock-ups; the others their LED colour)
const pal = (body, label, bed, key, hi, lo, keyText, knob, mark, bezel, led, ledOff, ledDim = led) =>
  ({ body, label, bed, key, hi, lo, keyText, knob, mark, bezel, led, ledOff, ledDim });
export const THEMES = {
  fizz: pal("#0E0E13", "#9A9AA6", "#050507", "#1C1C24", "#2C2C37", "#000000", "#ECECEF", "#050507", "#F2F2F5", "#000000", "#28E1FF", "#34343E", "#F1F0EA"),
  tangerine: pal("#EE7A3C", "#3A1606", "#874332", "#924E3A", "#A9614B", "#64301F", "#FBEDE3", "#1B1715", "#F4EDE6", "#141210", "#FFE6A8", "#6F6668"),
  midnight: pal("#1E2230", "#C9D2E6", "#12151D", "#2B3244", "#3B4359", "#0A0C12", "#E6ECF8", "#0A0B0F", "#6CF0C2", "#05060A", "#6CF0C2", "#454B5C"),
  mint: pal("#A8DCC8", "#143329", "#2F5B52", "#376A60", "#4A7F74", "#1E3F38", "#EAF7F1", "#1A2421", "#F2FFF9", "#121816", "#FFF3B0", "#6E8580"),
  lilac: pal("#C9AEE6", "#2A1642", "#573B77", "#634486", "#77589B", "#3B2655", "#F6EFFC", "#1C1624", "#F7F0FF", "#120E18", "#FFD6F0", "#8A7A9A"),
  cream: pal("#EFE6D2", "#2A231C", "#2A2520", "#3A332C", "#4B433A", "#141110", "#F3EBDC", "#E8622A", "#FFF4E8", "#15120F", "#FF7A3D", "#6A6158"),
};
export const THEME_ORDER = ["fizz", "tangerine", "midnight", "mint", "lilac", "cream"];
// the swatch: the theme's body with its accent dot (the mock-ups' picks)
export const SWATCH = { fizz: ["#0E0E13", "#28E1FF"], tangerine: ["#EE7A3C", "#3A1606"], midnight: ["#1E2230", "#6CF0C2"], mint: ["#A8DCC8", "#143329"],
  lilac: ["#C9AEE6", "#2A1642"], cream: ["#EFE6D2", "#E8622A"] };

// ---------------------------------------------------------------- the panel's names
// panel.js's orders: buttons by label (B: FX SEL ENV LFO EDIT GLO HOME SAVE ARP SEQ PLAY REC OCT- OCT+, the remote's
// QD_B_* order too), knobs by role (E: SELECT ALGO PRESETS KNOB1..4 MASTER; QD_K_* for the first seven)
export const BTN_LABEL = ["FX", "TRACKS", "MIX", "LATCH", "EDIT", "SHIFT", "HOME", "SOUNDS", "MIDI FX", "PATTERNS", "PLAY", "REC", "F1", "F2"];
export const KNOB_LABEL = ["SELECT", "ALGORITHM", "PRESETS", "KN1", "KN2", "KN3", "KN4", "MASTER"];
export const MASTER = 7;
const MAIN = [7, 0, 2, 1], KN = [3, 4, 5, 6];
// FIZZ's knob colours (PLAYING.md: SELECT yellow, KN1..4 blue, green, white, orange, ALGORITHM pink); MASTER and
// PRESETS the theme's mark
export const KNOB_INK = ["var(--yellow)", "var(--kalg)", "", "var(--k1)", "var(--k2)", "var(--k3)", "var(--k4)", ""];

// ---------------------------------------------------------------- the views
export const VIEWS = ["full", "panel", "keys", "screen"];
// a swipe on the screen: FULL, PANEL / KEYS (one place: the deck it had kept), SCREEN (-1 left, +1 right)
export function swiped(v, d) {
  const order = ["full", "keys", "screen"];
  const at = v === "panel" ? 1 : Math.max(0, order.indexOf(v));
  const n = order[Math.max(0, Math.min(order.length - 1, at + d))];
  return n === "keys" && v === "panel" ? "panel" : n;
}

// ---------------------------------------------------------------- the keybed (PlayModel.swift KeyLayout)
export const WHITES = [0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26];
export const BLACKS = [[0, 1], [1, 3], [2, 5], [4, 8], [5, 10], [7, 13], [8, 15], [9, 17], [11, 20], [12, 22], [14, 25]];
// w x h: the keys' stage; visible: the white keys across it (16: the whole row); offset: the first white key shown
export function keyLayout({ w, h, visible = 16, offset = 0, gap = 8 }) {
  const pitch = (w + gap) / visible, keyW = pitch - gap;
  const off = Math.max(0, Math.min(16 - visible, offset));
  const originX = -off * pitch;
  const blackH = Math.round(h * 0.44), whiteTop = blackH + gap, blackW = keyW * 0.82;
  const white = (i) => ({ x: originX + i * pitch, y: whiteTop, w: keyW, h: h - whiteTop });
  const black = (a) => { const cx = originX + (a + 1) * pitch - gap / 2; return { x: cx - blackW / 2, y: 0, w: blackW, h: blackH }; };
  const L = {
    w, h, visible, offset: off, gap, pitch, keyW, originX, blackH, whiteTop, blackW, white, black,
    frames: () => WHITES.map((k, i) => ({ key: k, black: false, i, ...white(i) })).concat(BLACKS.map(([a, k]) => ({ key: k, black: true, i: a, ...black(a) }))),
    // the key under (x, y) and where on it (0 its far end .. 1 its near end); black keys first; a white key's column
    // runs up under the black row, the gaps belong to the nearer key
    keyAt(x, y) {
      if (y < 0 || y > h || x < 0 || x > w) return null;
      for (const [a, k] of BLACKS) {
        const r = black(a);
        if (x >= r.x && x <= r.x + r.w && y >= r.y && y <= r.y + r.h) return { key: k, depth: (y - r.y) / Math.max(1, r.h) };
      }
      const i = Math.floor((x - originX + gap / 2) / pitch);
      if (i < 0 || i > 15) return null;
      const r = white(i);
      return { key: WHITES[i], depth: Math.max(0, y - r.y) / Math.max(1, r.h) };
    },
  };
  return L;
}
// the velocity from where the finger lands: 0 the key's far end (soft) .. 1 its near end (hard)
export function velocity(depth) {
  const c = Math.max(0, Math.min(1, +depth || 0));
  return Math.max(1, Math.min(127, Math.round(24 + c * 103)));
}

// ---------------------------------------------------------------- the knobs
// a drag's detents: up and right turn clockwise; what is left over is carried to the next move
export const DETENT_PX = 14;
export class Detents {
  constructor(step = DETENT_PX) { this.step = step; this.carried = 0; }
  move(dx, dy) {
    this.carried += dx - dy;
    const n = Math.trunc(this.carried / this.step);
    this.carried -= n * this.step;
    return n;
  }
}
// MASTER from a drag: the value at the start plus 1023 per `span` px (up and right raise it)
export const masterFrom = (start, dx, dy, span = 220) => Math.max(0, Math.min(1023, start + Math.round((dx - dy) / span * 1023)));

// ---------------------------------------------------------------- the FM-1's remote (midi_map.h THE REMOTE)
export const REMOTE_CH = 15;
const CC = 0xB0 | REMOTE_CH;
// key k (0 = F3) at the keybed's octave (-2..2): the MIDI note
export const remoteNote = (k, octave = 0) => Math.max(0, Math.min(127, 53 + 12 * octave + k));
export const master7 = (v) => Math.max(0, Math.min(127, Math.round(v * 127 / 1023)));
export class RemoteOut {
  constructor(send) { this.send = send; this.btn = 0; this.notes = new Map(); this.m7 = null; }
  // MASTER as it is now, noted (not sent: the FM-1's own volume stays until MASTER moves here)
  sync(master) { this.m7 = master7(master); }
  // the 14 buttons held (a mask, bit b = button b): each change one CC
  buttons(mask) {
    for (let b = 0; b < 14; b++) {
      const on = (mask >> b) & 1;
      if (on !== ((this.btn >> b) & 1)) this.send([CC, 20 + b, on ? 127 : 0]);
    }
    this.btn = mask;
  }
  // encoder role 0..6 turned n detents (64 + n; long turns in pieces of 63)
  turn(role, n) {
    if (role < 0 || role > 6) return;
    while (n) {
      const d = Math.max(-63, Math.min(63, n));
      this.send([CC, 40 + role, 64 + d]);
      n -= d;
    }
  }
  master(v) {
    const m = master7(v);
    if (m === this.m7) return;
    this.m7 = m;
    this.send([CC, 7, m]);
  }
  keyDown(k, note, ch, vel) {
    if (this.notes.has(k)) return;
    this.notes.set(k, { note, ch });
    this.send([0x90 | (ch & 15), note & 127, Math.max(1, Math.min(127, vel | 0))]);
  }
  keyUp(k) {
    const n = this.notes.get(k);
    if (!n) return;
    this.notes.delete(k);
    this.send([0x80 | (n.ch & 15), n.note & 127, 0]);
  }
  held(k) { return this.notes.has(k); }
  // everything let go: each note off, each button up, then CC 123 (the remote lets go whatever it still holds)
  letGo() {
    for (const k of [...this.notes.keys()]) this.keyUp(k);
    this.buttons(0);
    this.send([CC, 123, 0]);
  }
}

// ---------------------------------------------------------------- the layouts
// parts (board units): pill {x, y}; knob {role, cx, cy, r, lab: "above" | "left", hot}; well {x, y, w, h};
// btn {b, x, y, w, h}; bezel {x, y, s}; led {x, y}; bed {x, y, w, h, visible, pad, gap}; seg {kind: knobs | deck,
// x, y, right}; ovw {x, y, w, h} (the strip of all 16 steps over a scrolled keybed)
const knob = (role, cx, cy, r, lab = "above") => ({ t: "knob", role, cx, cy, r, lab });
const well = (x, y, w, h) => ({ t: "well", x, y, w, h });
function grid(x, y, w, h, { cols = 6, gap = 8, pad = 12, list = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11] } = {}) {
  const rows = Math.ceil(list.length / cols), bw = (w - 2 * pad - (cols - 1) * gap) / cols, bh = (h - 2 * pad - (rows - 1) * gap) / rows;
  return [well(x, y, w, h), ...list.map((b, i) => ({ t: "btn", b, x: x + pad + (i % cols) * (bw + gap), y: y + pad + Math.floor(i / cols) * (bh + gap), w: bw, h: bh }))];
}
const fpair = (x, y, w, h, pad = 8, gap = 6) => {
  const bw = (w - 2 * pad - gap) / 2;
  return [well(x, y, w, h), { t: "btn", b: 12, x: x + pad, y: y + pad, w: bw, h: h - 2 * pad }, { t: "btn", b: 13, x: x + pad + bw + gap, y: y + pad, w: bw, h: h - 2 * pad }];
};
const row = (set) => (set === "main" ? MAIN : KN);

export const LAYOUTS = ["full", "panel", "keys", "screen", "wide", "tall-full", "tall-panel", "tall-keys", "tall-screen"];
// the layout for a view: a computer, a phone on its side (FULL as one band), a phone upright
export function layoutName(view, { phone = false, landscape = false } = {}) {
  if (!phone) return view;
  if (landscape) return view === "full" ? "wide" : view;
  return `tall-${view}`;
}
export function layoutOf(name, { knobs = "kn", offset = 3 } = {}) {
  const p = [];
  let W = 1080, H = 600;
  switch (name) {
    case "full":
      p.push({ t: "pill", x: 26, y: 20 });
      p.push(knob(7, 90, 120, 30), knob(0, 200, 120, 30), knob(2, 90, 232, 30), knob(1, 200, 232, 30));
      p.push(...fpair(36, 286, 218, 58));
      p.push({ t: "bezel", x: 282, y: 34, s: 290 }, { t: "led", x: 600, y: 62 });
      KN.forEach((r, i) => p.push(knob(r, 680 + i * 112, 104, 32)));
      p.push(...grid(608, 172, 448, 156));
      p.push({ t: "bed", x: 26, y: 362, w: 1028, h: 218, visible: 16, pad: 12, gap: 8 });
      break;
    case "panel":
      p.push({ t: "pill", x: 26, y: 20 });
      p.push({ t: "bezel", x: 36, y: 70, s: 500 });
      p.push({ t: "seg", kind: "knobs", x: 600, y: 24 }, { t: "seg", kind: "deck", x: 832, y: 24 });
      row(knobs).forEach((r, i) => p.push(knob(r, 640 + i * 122, 158, 38)));
      p.push(...grid(576, 236, 480, 220));
      p.push(...fpair(576, 470, 236, 66, 10, 8), { t: "led", x: 1028, y: 496 });
      break;
    case "keys":
      p.push({ t: "pill", x: 26, y: 20 });
      p.push({ t: "bezel", x: 36, y: 64, s: 238 }, { t: "led", x: 290, y: 72 });
      p.push({ t: "seg", kind: "knobs", x: 376, y: 24 }, { t: "seg", kind: "deck", x: 832, y: 24 });
      row(knobs).forEach((r, i) => p.push(knob(r, 420 + i * 170, 176, 44)));
      p.push({ t: "bed", x: 26, y: 322, w: 1028, h: 258, visible: 16, pad: 12, gap: 8 });
      break;
    case "screen":
      p.push({ t: "pill", x: 26, y: 20 });
      p.push({ t: "bezel", x: 260, y: 30, s: 540 });
      break;
    case "wide":   // a phone on its side (mock-up 03): one band, the strip above it carries the target
      W = 1240; H = 540;
      p.push(knob(7, 74, 74, 28), knob(0, 192, 74, 28), knob(2, 74, 178, 28), knob(1, 192, 178, 28));
      p.push(...fpair(28, 220, 212, 52, 7, 6));
      p.push({ t: "bezel", x: 262, y: 18, s: 256 }, { t: "led", x: 532, y: 252 });
      KN.forEach((r, i) => p.push(knob(r, 630 + i * 170, 60, 30, "left")));
      p.push(...grid(560, 104, 664, 170));
      p.push({ t: "bed", x: 16, y: 288, w: 1208, h: 238, visible: 16, pad: 12, gap: 8 });
      break;
    case "tall-full":   // upright (the iOS app's full390)
      W = 358; H = 600;
      p.push(knob(7, 36, 50, 24), knob(0, 114, 50, 24), knob(2, 36, 130, 24), knob(1, 114, 130, 24));
      p.push(...fpair(0, 168, 150, 50, 7, 6));
      p.push({ t: "bezel", x: 158, y: 0, s: 200 }, { t: "led", x: 4, y: 244 });
      KN.forEach((r, i) => p.push(knob(r, 110 + i * 74, 266, 24)));
      p.push(...grid(0, 304, 358, 112, { gap: 6, pad: 8 }));
      p.push({ t: "ovw", x: 0, y: 428, w: 358, h: 16 });
      p.push({ t: "bed", x: 0, y: 452, w: 358, h: 148, visible: 6, offset, pad: 8, gap: 6 });
      break;
    case "tall-panel":
    case "tall-keys":   // upright, the linked views (mock-up 04): the screen big, one knob set, BUTTONS or KEYS
      W = 358;
      p.push({ t: "bezel", x: 44, y: 0, s: 270 }, { t: "led", x: 330, y: 8 });
      row(knobs).forEach((r, i) => p.push(knob(r, 48 + i * 87.3, 318, 24)));
      p.push({ t: "seg", kind: "knobs", x: 0, y: 356 }, { t: "seg", kind: "deck", right: 0, y: 356 });
      if (name === "tall-keys") {
        H = 560;
        p.push({ t: "ovw", x: 0, y: 404, w: 358, h: 16 });
        p.push({ t: "bed", x: 0, y: 430, w: 358, h: 130, visible: 6, offset, pad: 8, gap: 6 });
      } else {
        H = 568;
        p.push(...grid(0, 404, 358, 104, { gap: 6, pad: 8 }));
        p.push(...fpair(79, 516, 200, 52, 7, 6));
      }
      break;
    case "tall-screen":
      W = 358; H = 358;
      p.push({ t: "bezel", x: 0, y: 0, s: 358 });
      break;
    default:
      return layoutOf("full", { knobs, offset });
  }
  return { name, W, H, parts: p };
}
