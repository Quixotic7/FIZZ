// SPDX-License-Identifier: GPL-3.0-only
// The computer keys' strip at the foot of LIBRARY, POOL and IMPORT (FIZZ NEON, WebDesktop's footer): what they play
// and where, the keys as caps (the one held lit), Z / X and the octave. The keys themselves are editor.js's (its
// PC_WHITE / PC_BLACK from F: the same map here, in the keys' note order); this only shows them.
//   mountKeys({ $, editor, info: () => ({ name, where }) }) -> { render(), show(on) }
const KEYS = ["a", "w", "s", "e", "d", "r", "f", "g", "y", "h", "u", "j", "k", "o", "l", "p", ";", "[", "'"];
const BLACK = new Set(["w", "e", "r", "y", "u", "o", "p", "["]);
const NOTE = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"];
const noteName = (n) => `${NOTE[n % 12]}${Math.floor(n / 12) - 1}`;
const esc = (s) => String(s).replace(/[&<>"']/g, (c) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" }[c]));

export function mountKeys({ $, editor, info }) {
  const el = $("keys");
  el.innerHTML = `<span class="lbl kwhat" id="k-what"></span><div class="kcaps" id="k-caps">${KEYS.map((k) =>
    `<span class="key${BLACK.has(k) ? " blk" : ""}" data-k="${esc(k)}">${esc(k.toUpperCase())}</span>`).join("")}</div><span class="hud koct" id="k-oct"></span>`;
  const caps = new Map([...el.querySelectorAll(".key")].map((c) => [c.dataset.k, c]));
  function render() {
    if (el.hidden) return;
    const x = info() || {};
    $("k-what").innerHTML = x.name ? `Your keys play <b>${esc(x.name)}</b> ${esc(x.where || "")}` : `Your keys play ${esc(x.where || "")}`;
    $("k-oct").textContent = `Z X · OCTAVE · ${noteName(53 + 12 * editor.octave())}`;
  }
  const typing = (ev) => ev.target && (ev.target.tagName === "INPUT" || ev.target.tagName === "TEXTAREA" || ev.target.tagName === "SELECT" || ev.target.isContentEditable);
  // (after editor.js's own listener: its octave is the new one by now)
  document.addEventListener("keydown", (ev) => {
    if (el.hidden || typing(ev) || ev.metaKey || ev.ctrlKey || ev.altKey) return;
    const k = ev.key.toLowerCase();
    if (k === "z" || k === "x") { render(); return; }
    const c = caps.get(k);
    if (c) c.classList.add("on");
  });
  document.addEventListener("keyup", (ev) => { const c = caps.get(ev.key.toLowerCase()); if (c) c.classList.remove("on"); });
  window.addEventListener("blur", () => caps.forEach((c) => c.classList.remove("on")));
  return {
    render,
    show(on) { el.hidden = !on; if (!on) caps.forEach((c) => c.classList.remove("on")); render(); },
  };
}
