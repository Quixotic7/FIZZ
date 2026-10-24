// SPDX-License-Identifier: GPL-3.0-only
// The Feedback button: FIZZ's Discord, where the owner takes feedback (2026-10-23: Discord only; the GitHub issue
// form, .github/ISSUE_TEMPLATE/feedback.yml, is no longer linked). No DOM: site/tools/test_site.mjs checks it in Node.
export const REPO = "https://github.com/Quixotic7/FIZZ";
export const DISCORD = "https://discord.gg/emf6DmZ9V";

// "Chrome 141 on macOS" from a user agent string (and the client hints' platform, where there are some)
export function browserName(ua = "", platform = "") {
  const m = (re) => { const r = ua.match(re); return r ? r[1].split(".")[0] : null; };
  let b = "a browser";
  if (/Edg\//.test(ua)) b = `Edge ${m(/Edg\/([\d.]+)/)}`;
  else if (/OPR\//.test(ua)) b = `Opera ${m(/OPR\/([\d.]+)/)}`;
  else if (/Firefox\//.test(ua)) b = `Firefox ${m(/Firefox\/([\d.]+)/)}`;
  else if (/Chrome\//.test(ua)) b = `${/Brave/.test(ua) ? "Brave/Chrome" : "Chrome"} ${m(/Chrome\/([\d.]+)/)}`;
  else if (/Safari\//.test(ua)) b = `Safari ${m(/Version\/([\d.]+)/) || ""}`.trim();
  const os = /iPhone|iPad/.test(ua) ? "iOS" : /Android/.test(ua) ? "Android" : /Mac OS X|Macintosh/.test(ua) ? "macOS"
    : /Windows/.test(ua) ? "Windows" : /CrOS/.test(ua) ? "ChromeOS" : /Linux/.test(ua) ? "Linux" : platform || "";
  return os ? `${b} on ${os}` : b;
}

// the Feedback link (its callers pass what the page knows; Discord takes no prefilled fields)
export function feedbackUrl() {
  return DISCORD;
}
