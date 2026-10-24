// SPDX-License-Identifier: GPL-3.0-only
// The published site: site/'s pages, scripts, styles, engine, font and wasm copied into OUT, every asset URL versioned
// with the site's build so a deploy reaches the next load instead of a cached old file (GitHub Pages caches for
// about 10 minutes, and module scripts, the worklet and the wasm can stay longer):
//
//   node site/tools/assemble.mjs OUT            (the Pages workflow: OUT = _site)
//
// - the build: the first 10 hex digits of a SHA-256 over every published file (path and bytes), so any change to any
//   of them is a new build;
// - the pages: <meta name="fizz-build" content="..."> set to it, and their js/ and css/ links ?v=<build> (the
//   install page's own too: "install.js", "../css/install.css");
// - the modules (js/ and install/): every relative import ("./x.js") ?v=<build> (one URL per module, the same from
//   every importer);
// - install/ (the install page, MPL-2.0) and firmware/ (its packages and releases.json, which the page fetches
//   uncached) are published as they are;
//   js/build.js gives the rest of the page the build for the URLs it makes itself (the worklet, the engine, Connect
//   FM-1's modules; fizz.wasm is versioned by FIZZ's commit and size);
// - fizz.json: "site": <build> added. The page fetches it uncached and offers a reload when it differs from its own.
import fs from "node:fs";
import path from "node:path";
import crypto from "node:crypto";
import { fileURLToPath } from "node:url";

const SITE = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");
const ROOT = path.resolve(SITE, "..");
const OUT = process.argv[2];
if (!OUT) { console.error("usage: node site/tools/assemble.mjs OUT [--salt S] [--fizz-version V]"); process.exit(2); }
// (tests: --salt makes another build of the same files; --fizz-version stands in a new FIZZ version in fizz.json)
const opt = (k) => { const i = process.argv.indexOf(k); return i > 0 ? process.argv[i + 1] : null; };
const SALT = opt("--salt"), FIZZ_VERSION = opt("--fizz-version");

const ENTRIES = ["index.html", "guide.html", "dn2-wizard.html", "fizz.wasm", "fizz.json", "css", "js", "worklet", "fonts", "remote", "engine/engine.js",
  "install", "firmware"];
const files = [];
const walk = (rel) => {
  const p = path.join(SITE, rel);
  if (fs.statSync(p).isDirectory()) for (const n of fs.readdirSync(p).sort()) walk(path.join(rel, n));
  else files.push(rel);
};
for (const e of ENTRIES) walk(e);

const h = crypto.createHash("sha256");
if (SALT) h.update("salt\0" + SALT);
for (const f of files) { h.update(f + "\0"); h.update(fs.readFileSync(path.join(SITE, f))); }
const BUILD = h.digest("hex").slice(0, 10);

const v = (u) => `${u}?v=${BUILD}`;
let rewrites = 0;
const count = (re, s, fn) => s.replace(re, (...a) => { rewrites++; return fn(...a); });
for (const f of files) {
  const src = path.join(SITE, f), dst = path.join(OUT, f);
  fs.mkdirSync(path.dirname(dst), { recursive: true });
  if (f.endsWith(".html")) {
    let s = fs.readFileSync(src, "utf8");
    s = count(/<meta name="fizz-build" content="[^"]*">/, s, () => `<meta name="fizz-build" content="${BUILD}">`);
    s = count(/((?:src|href)=")((?:js|css)\/[^"?#]+\.(?:js|css))"/g, s, (_, a, u) => `${a}${v(u)}"`);
    if (f.startsWith("install/")) s = count(/((?:src|href)=")((?:\.\.\/css\/)?[\w.-]+\.(?:js|css))"/g, s, (_, a, u) => `${a}${v(u)}"`);
    s = count(/(from\s+")(\.\/js\/[^"?#]+\.js)"/g, s, (_, a, u) => `${a}${v(u)}"`);
    fs.writeFileSync(dst, s);
  } else if (/^(js|install)\/.*\.js$/.test(f)) {
    let s = fs.readFileSync(src, "utf8");
    s = count(/((?:^|\n)\s*(?:import|export)\b[^;"]*?\bfrom\s+"|(?:^|\n)\s*import\s+")(\.\.?\/[^"?#]+\.m?js)"/g, s, (_, a, u) => `${a}${v(u)}"`);
    fs.writeFileSync(dst, s);
  } else if (f === "fizz.json") {
    const j = JSON.parse(fs.readFileSync(src, "utf8"));
    j.site = BUILD;
    if (FIZZ_VERSION) j.version = FIZZ_VERSION;
    fs.writeFileSync(dst, JSON.stringify(j, null, 2) + "\n");
  } else fs.copyFileSync(src, dst);
}
fs.copyFileSync(path.join(ROOT, "LICENSE"), path.join(OUT, "LICENSE.txt"));
fs.writeFileSync(path.join(OUT, ".nojekyll"), "");

// every relative import of every published module now carries the build (none left bare)
const bare = [];
for (const f of files.filter((x) => /^(js|install)\/.*\.js$/.test(x))) {
  const s = fs.readFileSync(path.join(OUT, f), "utf8");
  for (const m of s.matchAll(/\bfrom\s+"(\.\.?\/[^"]+)"/g)) if (!m[1].includes(`?v=${BUILD}`)) bare.push(`${f}: ${m[1]}`);
}
if (bare.length) { console.error("assemble: imports left unversioned:\n  " + bare.join("\n  ")); process.exit(1); }
console.log(`assemble: ${files.length + 2} files into ${OUT}, build ${BUILD}, ${rewrites} URLs versioned`);
