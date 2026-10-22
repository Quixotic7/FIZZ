// SPDX-License-Identifier: GPL-3.0-only
// The Feedback button: a new issue on FIZZ from its feedback form (.github/ISSUE_TEMPLATE/feedback.yml), the
// fields this page knows filled in (GitHub fills an issue form's fields from query parameters named by their ids).
// No DOM: site/tools/test_site.mjs checks it in Node.
export const REPO = "https://github.com/Quixotic7/FIZZ";

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

// meta: site/fizz.json; setup: "fm1" (an FM-1 running RemoteFM1 connected) or "screen"
export function feedbackUrl({ meta = null, setup = "screen", ua = "", platform = "" } = {}) {
  const v = meta ? `${meta.version}${meta.commit ? ` (${meta.commit.slice(0, 7)})` : ""}` : "";
  const q = new URLSearchParams({
    template: "feedback.yml",
    title: `[Feedback] ${meta ? "FIZZ " + meta.version + ": " : ""}`,
    version: v,
    setup: setup === "fm1" ? "FM-1 with RemoteFM1" : "On-screen panel (mouse, touch or computer keyboard)",
    browser: browserName(ua, platform),
  });
  return `${REPO}/issues/new?${q}`;
}
