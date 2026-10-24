// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
// The install page's release notes (site/firmware/releases.json "notes", CHANGELOG.md's sections) as HTML.

export const esc = (s) => String(s).replace(/[&<>"]/g, (c) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;" })[c]);

/* release notes: a small Markdown (paragraphs, "- " lists, ### headings, **bold**, `code`, [text](https://...)) */
export function md(src) {
  const inline = (s) => esc(s)
    .replace(/`([^`]+)`/g, "<code>$1</code>")
    .replace(/\*\*([^*]+)\*\*/g, "<b>$1</b>")
    .replace(/\[([^\]]+)\]\((https?:\/\/[^)\s]+)\)/g, '<a href="$2" target="_blank" rel="noopener">$1</a>');
  const out = [];
  let list = null, para = [];
  const flush = () => {
    if (para.length) { out.push(`<p>${inline(para.join(" "))}</p>`); para = []; }
    if (list) { out.push(`<ul>${list.map((l) => `<li>${inline(l)}</li>`).join("")}</ul>`); list = null; }
  };
  for (const raw of String(src || "").split("\n")) {
    const line = raw.trimEnd();
    if (!line.trim()) { flush(); continue; }
    const h = /^#{1,4}\s+(.*)$/.exec(line);
    if (h) { flush(); out.push(`<h4>${inline(h[1])}</h4>`); continue; }
    const li = /^\s*[-*]\s+(.*)$/.exec(line);
    if (li) { if (para.length) flush(); (list = list || []).push(li[1]); continue; }
    if (list && /^\s{2,}\S/.test(raw)) { list[list.length - 1] += " " + line.trim(); continue; }
    if (list) flush();
    para.push(line.trim());
  }
  flush();
  return out.join("\n");
}
