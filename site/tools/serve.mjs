// SPDX-License-Identifier: GPL-3.0-only
// A static server on the site folder for the headless tests (test_site.mjs, test_manager_page.mjs) when no --url is
// given: Node's http server (a listen backlog of 511; `python3 -m http.server` keeps 5, and a page loading ~40 modules
// with the cache off got its connections reset: "FIZZ is not defined"), no caching headers, 127.0.0.1 on a free port.
//   const { url, close } = await serveSite(dir)
import http from "node:http";
import fs from "node:fs";
import path from "node:path";

const TYPES = { ".html": "text/html; charset=utf-8", ".js": "text/javascript; charset=utf-8", ".mjs": "text/javascript; charset=utf-8",
  ".css": "text/css; charset=utf-8", ".json": "application/json", ".wasm": "application/wasm", ".woff": "font/woff", ".woff2": "font/woff2",
  ".png": "image/png", ".svg": "image/svg+xml", ".gif": "image/gif", ".ico": "image/x-icon", ".txt": "text/plain; charset=utf-8" };

export function serveSite(root) {
  const base = path.resolve(root);
  const server = http.createServer((req, res) => {
    let p = decodeURIComponent(new URL(req.url, "http://x").pathname);
    if (p.endsWith("/")) p += "index.html";
    const f = path.resolve(base, `.${p}`);
    if (!f.startsWith(base + path.sep)) { res.writeHead(403); res.end(); return; }
    fs.readFile(f, (e, data) => {
      if (e) { res.writeHead(404, { "Content-Type": "text/plain" }); res.end("not found"); return; }
      res.writeHead(200, { "Content-Type": TYPES[path.extname(f).toLowerCase()] || "application/octet-stream", "Cache-Control": "no-store" });
      res.end(data);
    });
  });
  return new Promise((resolve) => server.listen(0, "127.0.0.1", 511, () =>
    resolve({ url: `http://127.0.0.1:${server.address().port}/`, close: () => server.close() })));
}
