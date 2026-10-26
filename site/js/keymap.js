// SPDX-License-Identifier: GPL-3.0-only
// The guide's one-screen key map: the FM-1's controls drawn as line work in their places, each with FIZZ's name for
// it, the black keys in their modes' colours (FIZZ core/ui ui_kit.c UI_MODE_COL), and the computer key that presses
// it (js/panel.js KEYMAP: FM1VST's map, plus [ for B11).
const WHITE_PC = ["2", "3", "4", "Tab", "A", "S", "D", "F", "G", "H", "J", "K", "L", ";", "'", "]"];
const BLACK_PC = ["F1", "F2", "F3", "F4", "E", "T", "Y", "U", "O", "P", "["];
const BLACK_AFTER = [0, 1, 2, 4, 5, 7, 8, 9, 11, 12, 14];      // a black key sits after these white keys (F3 .. G5)
const MODES = [["NOTE", "#3F7DFF"], ["VEL", "#1FD47E"], ["LEN", "#36D6E7"], ["MICRO", "#A879FF"], ["RATCH", "#FF6A2C"],
  ["CHNC", "#FFD23F"], ["COND", "#FF52C0"], ["TRIG", "#F1F0EA"], ["FUNC", "#9FE6FF"], ["SOUND", "#B5E853"], ["P-LOCK", "#D8C39A"]];
// [printed, FIZZ's job, computer key, colour]
const BUTTONS = [
  ["FX", "FX", "C"], ["SEL", "TRACKS", "V"], ["ENV", "MIX", "B"], ["LFO", "LATCH", "N"], ["EDIT", "EDIT", "M"], ["GLO", "SHIFT", ",", "#FFD23F"],
  ["HOME", "HOME", "7"], ["SAVE", "SOUNDS", "8"], ["ARP", "MIDI FX", "9"], ["SEQ", "PATTERNS", "0"], ["PLAY", "PLAY", "-", "#1FD47E"], ["REC", "REC", "=", "#FF3448"],
];
// [cx, cy, name, job, colour]
const KNOBS = [[84, 82, "MASTER", "volume"], [204, 82, "SELECT", "page", "#FFD23F"], [84, 196, "PRESETS", "sound", "#28E1FF"],
  [204, 196, "ALGORITHM", "step cursor", "#FF8FC8"],
  [560, 70, "KNOB 1", "", "#3F7DFF"], [660, 70, "KNOB 2", "", "#1FD47E"], [760, 70, "KNOB 3", "", "#F1F0EA"], [860, 70, "KNOB 4", "", "#FF6A2C"]];

const esc = (s) => String(s).replace(/&/g, "&amp;").replace(/</g, "&lt;");
const t = (x, y, s, cls, style = "") => `<text x="${x}" y="${y}" class="${cls}" text-anchor="middle"${style ? ` style="${style}"` : ""}>${esc(s)}</text>`;
const cap = (cx, y, s) => { const w = Math.max(20, 9 * s.length + 10); return `<rect x="${cx - w / 2}" y="${y}" width="${w}" height="17" rx="4" class="km-cap"/>${t(cx, y + 12.5, s, "km-cap-t")}`; };

export function keymapSvg() {
  const o = [`<svg class="keymap" viewBox="0 0 1000 640" role="img" aria-label="The FM-1's panel with FIZZ's names for every control and the computer keys">`];
  o.push(`<rect x="8" y="8" width="984" height="580" rx="28" class="km-body"/>`);
  // the knobs
  for (const [cx, cy, name, job, col] of KNOBS) {
    const st = col ? `stroke:${col}` : "";
    o.push(`<circle cx="${cx}" cy="${cy}" r="24" class="km-knob" style="${st}"/><line x1="${cx}" y1="${cy - 4}" x2="${cx}" y2="${cy - 19}" class="km-knob-mark" style="${st}"/>`);
    o.push(t(cx, cy - 34, name, "km-name"));
    if (job) o.push(t(cx, cy + 44, job, "km-job"));
  }
  o.push(t(710, 124, "KNOB 1-4: the held step's values, or the track's defaults", "km-note"));
  // OCT- / OCT+: F1 / F2
  for (const [x, f, printed, pc] of [[48, "F1", "OCT−", "Z"], [168, "F2", "OCT+", "X"]]) {
    o.push(t(x + 36, 262, printed, "km-print"));
    o.push(`<rect x="${x}" y="268" width="72" height="34" rx="8" class="km-btn"/>`, t(x + 36, 291, f, "km-btn-t"));
    o.push(cap(x + 36, 310, pc));
  }
  o.push(t(144, 350, "both together: F3 (or hold PLAY) · End", "km-job"));
  // the screen
  o.push(`<rect x="300" y="34" width="180" height="180" rx="12" class="km-lcd"/>`);
  o.push(t(390, 132, "FIZZ", "km-lcd-t"));
  o.push(t(390, 250, "a knob: PgUp / PgDn pick,", "km-job"), t(390, 268, "↑ ↓ turn · or drag it, or the wheel", "km-job"));
  // the buttons: 2 rows of 6
  BUTTONS.forEach(([printed, job, pc, col], i) => {
    const x = 516 + (i % 6) * 80, y = 166 + ((i / 6) | 0) * 92;
    o.push(t(x + 34, y - 7, printed, "km-print"));
    o.push(`<rect x="${x}" y="${y}" width="68" height="44" rx="9" class="km-btn"${col ? ` style="stroke:${col}"` : ""}/>`);
    o.push(t(x + 34, y + 27, job, "km-btn-job" + (job.length > 6 ? " small" : ""), col ? `fill:${col}` : ""));
    o.push(cap(x + 34, y + 51, pc));
  });
  // the keybed
  const W0 = 36, WW = 58, KW = 52;
  BLACK_AFTER.forEach((after, i) => {
    const x = W0 + (after + 1) * WW - KW / 2 - 3, [mode, col] = MODES[i];
    o.push(`<rect x="${x}" y="384" width="${KW}" height="86" rx="10" class="km-key black" style="stroke:${col}"/>`);
    o.push(t(x + KW / 2, 408, `B${i + 1}`, "km-key-n", `fill:${col}`));
    o.push(t(x + KW / 2, 428, mode, "km-mode" + (mode.length > 5 ? " small" : ""), `fill:${col}`));
    o.push(cap(x + KW / 2, 443, BLACK_PC[i]));
  });
  for (let i = 0; i < 16; i++) {
    const x = W0 + i * WW;
    o.push(`<rect x="${x}" y="482" width="${KW}" height="86" rx="10" class="km-key white${i % 4 === 0 ? " beat" : ""}"/>`);
    o.push(t(x + KW / 2, 514, `W${i + 1}`, "km-key-n white"));
    o.push(cap(x + KW / 2, 538, WHITE_PC[i]));
  }
  o.push(t(500, 610, "W1-W16: the page's 16 steps (tap: on / off, hold: edit) · B1-B11, nothing held: what you edit; a step held: its values", "km-note"));
  o.push(t(870, 352, "FILL: SHIFT + FX held ( , + C )", "km-job"));
  o.push(`<rect x="420" y="620" width="22" height="15" rx="4" class="km-cap"/>`, `<text x="450" y="632" class="km-job">= the computer key</text>`);
  o.push("</svg>");
  return o.join("");
}
