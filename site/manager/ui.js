// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
// The manager's small drawing helpers, shared by its tabs (manager.js, pool.js, importer.js): the tag colours, the
// algorithm glyph (a stand-in for FIZZ's own), a cassette's colour, the empty-place art.
import { FIZZ_TAGS } from "../js/fizz_sysex.js";

export const $ = (id) => document.getElementById(id);
export const TAGC = { BASS: "--blue", KEYS: "--cream", PLUCK: "--yellow", PAD: "--violet", LEAD: "--green", DRUMS: "--orange", FX: "--pink" };
export const DARK_TEXT_TAGS = new Set(["BASS", "PAD", "FX"]);   // (white letters on these)
export const esc = (s) => String(s).replace(/[&<>"']/g, (c) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" }[c]));
export const tagsOf = (m) => FIZZ_TAGS.filter((_, k) => (m >> k) & 1);
export const colOf = (tags) => `var(${TAGC[tagsOf(tags)[0]] || "--cream"})`;

const ALGO_P = [[[6, 6], [24, 6], [6, 24], [24, 24]], [[15, 5], [6, 24], [15, 15], [24, 24]], [[6, 6], [15, 15], [24, 6], [15, 26]], [[6, 15], [15, 6], [24, 15], [15, 25]]];
// four operators, the carriers below
export function algo(k, c, cls = "algo") {
  const P2 = ALGO_P[k % 4], L = [[0, 2], [1, 3], [0, 1]];
  return `<svg class="${cls}" viewBox="0 0 30 30" aria-hidden="true">${L.map(([a, b]) => `<line x1="${P2[a][0]}" y1="${P2[a][1]}" x2="${P2[b][0]}" y2="${P2[b][1]}" stroke="${c}" stroke-width="2" opacity=".55"/>`).join("")}` +
    `${P2.map(([x, y], i) => `<circle cx="${x}" cy="${y}" r="${i >= 2 ? 4.2 : 3.2}" fill="${i >= 2 ? c : "none"}" stroke="${c}" stroke-width="2"/>`).join("")}</svg>`;
}
// FIZZ MIDI (e): the MIDI mark (a DIN socket, the device's MIDI header) and a MIDI sound's chip in the lists
export const MIDI_I = '<svg class="din" viewBox="0 0 24 24" aria-hidden="true"><circle cx="12" cy="12" r="10"/><path d="M10 3.4h4"/><circle cx="6.5" cy="12" r="1.3"/><circle cx="17.5" cy="12" r="1.3"/><circle cx="8.2" cy="7.9" r="1.3"/><circle cx="15.8" cy="7.9" r="1.3"/><circle cx="12" cy="6.4" r="1.3"/></svg>';
export const midiChip = (on) => (on ? '<span class="mchip" title="a MIDI track\'s sound">MIDI</span>' : "");
export function emptyArt() {
  return '<svg viewBox="0 0 160 120"><rect x="20" y="24" width="120" height="76" rx="12" stroke="#2a2a33"/><circle cx="58" cy="60" r="13" stroke="#2a2a33"/><circle cx="102" cy="60" r="13" stroke="#2a2a33"/><path d="M42 100l10-16h56l10 16" stroke="#2a2a33"/></svg>';
}
// what a refused edit means, for the page's line (FIZZ's statuses: docs/SYSEX.md)
export function refusal(r, what = "") {
  if (!r) return "The FM-1 did not answer.";
  if (r.old) return 'This FIZZ cannot do that yet: <a href="../install/">update FIZZ</a>.';
  if (r.status === 8) return `The FM-1 is playing (or still busy): stop it, then ${what || "try again"}.`;
  if (r.status === 9) return "That is not on the FM-1 any more.";
  if (r.status === 1) return "Full.";
  if (r.status === 5) return "Not there: read only.";
  return `The FM-1 refused it (${esc(r.statusName || r.status)}).`;
}

// the one confirm step of a destructive or slow change (the page's small modal, #cf): what it does, to what, and what
// it does to the FM-1's undo. -> Promise<boolean>. undo: "cleared" (the unit forgets its undo levels: a pool swap, a
// project load, a restore), "kept" (SHIFT + F1 on the FM-1 undoes it), or nothing said
let cfDone = null;
export function confirmBox({ title, what = "", sub = "", go = "OK", color = "--red", undo = "" }) {
  const box = $("cf");
  if (cfDone) cfDone(false);
  $("cf-modal").style.setProperty("--c", `var(${color})`);
  $("cf-title").textContent = title;
  $("cf-what").textContent = what;
  $("cf-what").hidden = !what;
  $("cf-sub").innerHTML = sub;
  $("cf-sub").hidden = !sub;
  const u = $("cf-undo");
  u.className = `undo ${undo}`;
  u.textContent = undo === "cleared" ? "UNDO ON THE FM-1 IS CLEARED" : undo === "kept" ? "SHIFT + F1 ON THE FM-1 UNDOES IT" : "";
  u.hidden = !undo;
  $("cf-go").textContent = go;
  box.hidden = false;
  $("cf-go").focus();
  return new Promise((res) => {
    const end = (v) => { cfDone = null; box.hidden = true; document.removeEventListener("keydown", key, true); res(v); };
    const key = (ev) => {
      if (ev.key === "Escape") { ev.preventDefault(); ev.stopPropagation(); end(false); }
      else if (ev.key === "Enter" && document.activeElement !== $("cf-no")) { ev.preventDefault(); ev.stopPropagation(); end(true); }
    };
    cfDone = end;
    document.addEventListener("keydown", key, true);
    $("cf-go").onclick = () => end(true);
    $("cf-no").onclick = () => end(false);
    box.onclick = (ev) => { if (ev.target === box) end(false); };
  });
}
