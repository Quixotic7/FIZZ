// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// FizzData.js: the web Manager's data modules (site/, unchanged) bundled for the iOS app's JavaScriptCore as ONE
// classic script that defines globalThis.FizzData (ios/PLAN.md §3 B). Plain Node, no dependencies (no esbuild here).
//
//   node ios/jsbundle/build.mjs            write ios/FizzKit/Sources/FizzData/Resources/FizzData.js
//   node ios/jsbundle/build.mjs --check    exit 1 if the committed bundle is not what the sources make (stale)
//   (--entry FILE, --out FILE: another entry / output)
//
// What it understands, and refuses the rest (a module that needs more fails the build, loudly):
//   import { a, b as c } from "./x.js";      import * as ns from "./x.js";      (relative paths, at a line's start)
//   export function|async function|class|const NAME ..   export { a, b as c };   export { a, b as c } from "./x.js";
// Each module runs in its own function scope (their private names collide: crc32, safe, NAME, ..), in dependency order,
// and returns its exports; an import becomes a destructuring of the module's exports. The export names are the ones
// Node itself sees (the module imported here), so the bundle exports exactly what the ESM does. Every section starts
// with its source's own text, its SPDX header included.
//
// One substitution: a raw-deflate inflate (fm1dnsnd.js dnInflateRaw, dn2pst.js inflateRaw) uses DecompressionStream
// (with Blob and Response) or a dynamic import("node:zlib"), neither of which a JSContext has; its body becomes a call
// to the host's native globalThis.__fizzInflateRaw(Uint8Array) -> Uint8Array (FizzDataEngine: Apple's Compression,
// COMPRESSION_ZLIB = raw DEFLATE; test.mjs: node:zlib). The build checks that the replaced body is still the inflate.
import { readFileSync, writeFileSync, mkdirSync } from "node:fs";
import { dirname, join, relative, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

const here = dirname(fileURLToPath(import.meta.url));
const root = resolve(here, "../..");
const arg = (k, d) => { const i = process.argv.indexOf(k); return i > 0 ? process.argv[i + 1] : d; };
const ENTRY = resolve(arg("--entry", join(here, "entry.js")));
const OUT = resolve(arg("--out", join(root, "ios/FizzKit/Sources/FizzData/Resources/FizzData.js")));
const POLYFILLS = join(here, "polyfills.js");
const CHECK = process.argv.includes("--check");

// the inflates routed to the host (see above): file (from the root) -> function name
const INFLATE = { "site/js/fm1dnsnd.js": "dnInflateRaw", "site/js/dn2pst.js": "inflateRaw" };

const fail = (msg) => { throw new Error(`build.mjs: ${msg}`); };
const rel = (f) => relative(root, f).split("\\").join("/");

const IMPORT_RE = /^import\s+([\s\S]*?)\s+from\s+(["'])([^"']+)\2\s*;?[ \t]*$/gm;
const REEXPORT_RE = /^export\s*\{([^}]*)\}\s*from\s+(["'])([^"']+)\2\s*;?[ \t]*$/gm;
const EXPORT_LIST_RE = /^export\s*\{([^}]*)\}\s*;?[ \t]*$/gm;
const EXPORT_DECL_RE = /^export\s+(?=(?:async\s+function|function|class|const)\b)/gm;

// "a, b as c" -> [[a, a], [b, c]] (imported / local, or local / exported)
function specList(s, file) {
  return s.split(",").map((x) => x.trim()).filter(Boolean).map((x) => {
    const m = /^([A-Za-z_$][\w$]*)(?:\s+as\s+([A-Za-z_$][\w$]*))?$/.exec(x);
    if (!m) fail(`${rel(file)}: cannot read "${x}"`);
    return [m[1], m[2] || m[1]];
  });
}
const resolveDep = (file, spec) => {
  if (!spec.startsWith("./") && !spec.startsWith("../")) fail(`${rel(file)}: "${spec}" is not a relative import`);
  return resolve(dirname(file), spec);
};

// the modules, dependencies first
const mods = new Map();                                   // file -> {file, src, deps}
const order = [];
function visit(file, stack = []) {
  if (stack.includes(file)) fail(`an import cycle: ${[...stack, file].map(rel).join(" -> ")}`);
  if (mods.has(file)) return;
  const src = readFileSync(file, "utf8");
  // the app carries no GPL (the owner, 2026-10-28): every bundled module must say MPL-2.0
  const spdx = (src.match(/SPDX-License-Identifier:\s*([^\s*]+)/) || [])[1];
  if (spdx !== "MPL-2.0") fail(`${rel(file)}: licence ${spdx || "(none)"}; the iOS app bundles MPL-2.0 modules only`);
  const deps = [];
  for (const re of [IMPORT_RE, REEXPORT_RE]) for (const m of src.matchAll(re)) deps.push(resolveDep(file, m[3]));
  mods.set(file, { file, src, deps });
  for (const d of deps) visit(d, [...stack, file]);
  order.push(file);
}
visit(ENTRY);
const idOf = new Map(order.map((f, i) => [f, `__fizz_m${i}`]));

// a function's body ({ .. } after its parameters), found by matching braces outside strings and comments
function bodyRange(src, at) {
  let i = src.indexOf("{", src.indexOf(")", at)), depth = 0;
  const start = i;
  for (; i < src.length; i++) {
    const c = src[i];
    if (c === '"' || c === "'" || c === "`") { const q = c; for (i++; i < src.length && src[i] !== q; i++) if (src[i] === "\\") i++; continue; }
    if (c === "/" && src[i + 1] === "/") { i = src.indexOf("\n", i); continue; }
    if (c === "/" && src[i + 1] === "*") { i = src.indexOf("*/", i) + 1; continue; }
    if (c === "{") depth++;
    else if (c === "}" && --depth === 0) return [start, i + 1];
  }
  fail("unbalanced braces");
}
function routeInflate(src, file) {
  const fn = INFLATE[rel(file)];
  if (!fn) return src;
  const m = new RegExp(`^(?:export\\s+)?async\\s+function\\s+${fn}\\s*\\(\\s*([A-Za-z_$][\\w$]*)\\s*\\)`, "m").exec(src);
  if (!m) fail(`${rel(file)}: no async function ${fn}(bytes) to route to __fizzInflateRaw`);
  const [a, b] = bodyRange(src, m.index), body = src.slice(a, b);
  if (!/deflate-raw/.test(body) || !/inflateRawSync/.test(body)) fail(`${rel(file)}: ${fn} is no longer the raw-deflate inflate it was: look at it again`);
  const p = m[1];
  return src.slice(0, a) + `{ /* FizzData: the host's native inflate (build.mjs) */ return __fizzInflateRaw(${p}); }` + src.slice(b);
}

async function section(file, isEntry) {
  const { src: original, deps } = mods.get(file);
  let src = routeInflate(original, file);
  const exportsLocal = new Map();                        // exported name -> local name
  let n = 0;
  src = src.replace(IMPORT_RE, (_, what, _q, spec) => {
    const dep = idOf.get(resolveDep(file, spec));
    what = what.trim();
    const ns = /^\*\s+as\s+([A-Za-z_$][\w$]*)$/.exec(what);
    if (ns) return `const ${ns[1]} = ${dep};`;
    const named = /^\{([\s\S]*)\}$/.exec(what);
    if (!named) fail(`${rel(file)}: only named and namespace imports ("${what}")`);
    const parts = specList(named[1], file).map(([a, b]) => (a === b ? a : `${a}: ${b}`));
    return `const { ${parts.join(", ")} } = ${dep};`;
  });
  src = src.replace(REEXPORT_RE, (_, list, _q, spec) => {
    const dep = idOf.get(resolveDep(file, spec));
    const parts = specList(list, file).map(([a, b]) => { const local = `__fizz_re${n++}`; exportsLocal.set(b, local); return `${a}: ${local}`; });
    return `const { ${parts.join(", ")} } = ${dep};`;
  });
  src = src.replace(EXPORT_LIST_RE, (_, list) => { for (const [a, b] of specList(list, file)) exportsLocal.set(b, a); return ""; });
  src = src.replace(EXPORT_DECL_RE, "");
  for (const re of [/^\s*export\b/m, /^\s*import\s/m, /\bimport\s*\(\s*["'`]/, /\bimport\.meta\b/]) {
    const m = re.exec(src);
    if (m) fail(`${rel(file)}: "${src.slice(m.index, m.index + 60).trim()}" is not something this bundler handles`);
  }
  if (/^export\s+(let|var)\b/m.test(original)) fail(`${rel(file)}: an exported let / var (a live binding) is not supported`);
  // the export names, as Node sees the module
  const names = Object.keys(await import(pathToFileURL(file).href));
  const ret = names.map((k) => { const l = exportsLocal.get(k) || k; return l === k ? k : `${k}: ${l}`; });
  const head = `// ======== ${rel(file)}${deps.length ? ` (imports ${[...new Set(deps)].map(rel).join(", ")})` : ""}\n`;
  return head + `const ${idOf.get(file)} = (function () {\n"use strict";\n${src.replace(/\s*$/, "")}\nreturn { ${ret.join(", ")} };\n})();\n` +
    (isEntry ? `globalThis.FizzData = Object.freeze(${idOf.get(file)});\n` : "");
}

async function bundle() {
  const parts = [
    "// GENERATED by ios/jsbundle/build.mjs from ios/jsbundle/entry.js and the site modules it imports: do not edit.\n" +
    "// Rebuild: node ios/jsbundle/build.mjs (check: --check). Each section below starts with its source's own text and\n" +
    "// licence header (SPDX): all MPL-2.0 (the site modules bundled here were relicensed MPL-2.0 by their author).\n" +
    `// Modules, in order: ${order.map(rel).join(", ")}\n` +
    "(function () {\n\"use strict\";\n" +
    "const __fizzInflateRaw = (b) => {\n" +
    "  if (typeof globalThis.__fizzInflateRaw !== \"function\") throw new Error(\"FizzData: this host gives no __fizzInflateRaw (a deflated ZIP member)\");\n" +
    "  return globalThis.__fizzInflateRaw(b);\n};\n",
    `// ======== ${rel(POLYFILLS)}\n${readFileSync(POLYFILLS, "utf8").replace(/\s*$/, "")}\n`,
  ];
  for (const f of order) parts.push(await section(f, f === ENTRY));
  parts.push("})();\n");
  return parts.join("\n");
}

const text = await bundle();
if (CHECK) {
  let old = null;
  try { old = readFileSync(OUT, "utf8"); } catch (_) { /* none */ }
  if (old !== text) { console.error(`${rel(OUT)} is stale: node ios/jsbundle/build.mjs`); process.exit(1); }
  console.log(`${rel(OUT)} is up to date (${order.length} modules, ${text.length} bytes)`);
} else {
  mkdirSync(dirname(OUT), { recursive: true });
  writeFileSync(OUT, text);
  console.log(`wrote ${rel(OUT)} (${order.length} modules, ${text.length} bytes)`);
}
