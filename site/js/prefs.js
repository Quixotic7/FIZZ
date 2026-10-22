// SPDX-License-Identifier: GPL-3.0-only
// From RemoteFM1 web/site/js/prefs.js (GPL-3.0-only, github.com/Quixotic7/RemoteFM1), copied for FIZZ's preview site.
// The visitor's preferences, in this browser's localStorage (one JSON record). Every access is in try/catch: a private
// window, blocked site data or a full quota gives the defaults and saves nothing. No DOM: web/site/test.mjs runs it.
//   loadPrefs(storage)          the stored record ({} when there is none or it cannot be read)
//   savePrefs(storage, prefs)   writes it back; false when it could not
//   keyHintsOn(prefs)           the computer-key hints on the panel's keys and buttons: off unless the visitor turned them on
export const PREF_KEY = "fizz-preview";

export function loadPrefs(storage) {
  try {
    const v = JSON.parse(storage.getItem(PREF_KEY));
    return v && typeof v === "object" && !Array.isArray(v) ? v : {};
  } catch (_) { return {}; }
}
export function savePrefs(storage, prefs) {
  try { storage.setItem(PREF_KEY, JSON.stringify(prefs)); return true; } catch (_) { return false; }
}
export const keyHintsOn = (prefs) => !!prefs && prefs.keyHints === true;
// "Shift-click to hold" (input.js latches) not shown yet: it shows once, ever (RemoteFM1 ceb0025)
export const shiftHintDue = (prefs) => !prefs || prefs.shiftHintShown !== true;
