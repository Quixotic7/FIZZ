// SPDX-License-Identifier: GPL-3.0-only
// The site's build: tools/assemble.mjs writes it into the pages' <meta name="fizz-build"> and every module's import
// URLs (?v=<build>), and into the published fizz.json ("site"), so a new deploy is never served from a stale cache.
// From the source folder (site/ served as it is) it is "dev" and nothing is versioned.
const m = typeof document !== "undefined" && document.querySelector('meta[name="fizz-build"]');
export const BUILD = (m && m.content) || "dev";
// a URL relative to the page, with the build appended (the worklet, the engine, Connect FM-1's modules)
export const ver = (u) => (BUILD === "dev" ? u : u + (u.includes("?") ? "&" : "?") + "v=" + BUILD);
