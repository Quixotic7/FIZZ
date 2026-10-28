// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// What a bare JSContext lacks and the bundled site modules use (ios/PLAN.md §3 B), in plain JS so that test.mjs runs
// the very same code in a Node vm context (which lacks them too): TextEncoder / TextDecoder (UTF-8, the WHATWG rules:
// U+FFFD for a lone surrogate or a bad sequence, the BOM dropped unless ignoreBOM), atob / btoa (forgiving base64).
// Each is installed only where the host has none (a browser's or Node's own stay). Not here: raw-deflate inflate, which
// the bundler routes to the host's native __fizzInflateRaw (FizzDataEngine: Apple's Compression; test.mjs: node:zlib),
// and console, which the host gives (FizzDataEngine: its logger). A plain script fragment, no import / export.
(function (g) {
  if (typeof g.TextEncoder === "undefined") {
    class TextEncoder {
      get encoding() { return "utf-8"; }
      encode(input = "") {
        const s = String(input), out = new Uint8Array(s.length * 3);
        let o = 0;
        for (let i = 0; i < s.length; i++) {
          let c = s.charCodeAt(i);
          if (c >= 0xD800 && c <= 0xDFFF) {
            const d = i + 1 < s.length ? s.charCodeAt(i + 1) : 0;
            if (c <= 0xDBFF && d >= 0xDC00 && d <= 0xDFFF) { c = 0x10000 + ((c - 0xD800) << 10) + (d - 0xDC00); i++; }
            else c = 0xFFFD;
          }
          if (c < 0x80) out[o++] = c;
          else if (c < 0x800) { out[o++] = 0xC0 | (c >> 6); out[o++] = 0x80 | (c & 63); }
          else if (c < 0x10000) { out[o++] = 0xE0 | (c >> 12); out[o++] = 0x80 | ((c >> 6) & 63); out[o++] = 0x80 | (c & 63); }
          else { out[o++] = 0xF0 | (c >> 18); out[o++] = 0x80 | ((c >> 12) & 63); out[o++] = 0x80 | ((c >> 6) & 63); out[o++] = 0x80 | (c & 63); }
        }
        return out.slice(0, o);
      }
    }
    g.TextEncoder = TextEncoder;
  }

  if (typeof g.TextDecoder === "undefined") {
    const LABELS = ["utf-8", "utf8", "unicode-1-1-utf-8"];
    class TextDecoder {
      constructor(label = "utf-8", options = {}) {
        if (!LABELS.includes(String(label).trim().toLowerCase())) throw new RangeError(`TextDecoder: only UTF-8 (${label})`);
        this.fatal = !!(options && options.fatal);
        this.ignoreBOM = !!(options && options.ignoreBOM);
      }
      get encoding() { return "utf-8"; }
      decode(input, options = {}) {
        if (options && options.stream) throw new TypeError("TextDecoder: streaming is not supported here");
        if (input === undefined) return "";
        const b = input instanceof ArrayBuffer ? new Uint8Array(input)
          : ArrayBuffer.isView(input) ? new Uint8Array(input.buffer, input.byteOffset, input.byteLength)
          : Object.prototype.toString.call(input) === "[object ArrayBuffer]" ? new Uint8Array(input) : null;
        if (!b) throw new TypeError("TextDecoder: an ArrayBuffer or a view");
        const units = [];
        let s = "", need = 0, seen = 0, cp = 0, lo = 0x80, hi = 0xBF;
        const bad = () => { if (this.fatal) throw new TypeError("TextDecoder: not UTF-8"); units.push(0xFFFD); };
        const flush = () => { if (units.length >= 0x4000) { s += String.fromCharCode.apply(null, units); units.length = 0; } };
        for (let i = 0; i < b.length; i++) {
          const x = b[i];
          if (!need) {
            if (x < 0x80) units.push(x);
            else if (x >= 0xC2 && x <= 0xDF) { need = 1; cp = x & 0x1F; }
            else if (x >= 0xE0 && x <= 0xEF) { if (x === 0xE0) lo = 0xA0; if (x === 0xED) hi = 0x9F; need = 2; cp = x & 0xF; }
            else if (x >= 0xF0 && x <= 0xF4) { if (x === 0xF0) lo = 0x90; if (x === 0xF4) hi = 0x8F; need = 3; cp = x & 7; }
            else bad();
          } else if (x < lo || x > hi) {                    // the sequence broken: U+FFFD, this byte read again
            need = seen = cp = 0; lo = 0x80; hi = 0xBF;
            bad(); i--;
          } else {
            lo = 0x80; hi = 0xBF;
            cp = (cp << 6) | (x & 63);
            if (++seen === need) {
              if (cp >= 0x10000) { cp -= 0x10000; units.push(0xD800 + (cp >> 10), 0xDC00 + (cp & 0x3FF)); }
              else units.push(cp);
              need = seen = cp = 0;
            }
          }
          flush();
        }
        if (need) bad();
        s += String.fromCharCode.apply(null, units);
        return !this.ignoreBOM && s.charCodeAt(0) === 0xFEFF ? s.slice(1) : s;
      }
    }
    g.TextDecoder = TextDecoder;
  }

  const B64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  const invalid = (what) => { const e = new Error(what); e.name = "InvalidCharacterError"; return e; };
  if (typeof g.btoa === "undefined") {
    g.btoa = function btoa(input) {
      const s = String(input);
      let out = "";
      for (let i = 0; i < s.length; i += 3) {
        const a = s.charCodeAt(i), b = s.charCodeAt(i + 1), c = s.charCodeAt(i + 2);
        if (a > 255 || b > 255 || c > 255) throw invalid("btoa: a character outside Latin-1");
        const n = (a << 16) | ((b || 0) << 8) | (c || 0);
        out += B64[n >> 18] + B64[(n >> 12) & 63] + (i + 1 < s.length ? B64[(n >> 6) & 63] : "=") + (i + 2 < s.length ? B64[n & 63] : "=");
      }
      return out;
    };
  }
  if (typeof g.atob === "undefined") {
    const IDX = new Int16Array(128).fill(-1);
    for (let i = 0; i < 64; i++) IDX[B64.charCodeAt(i)] = i;
    g.atob = function atob(input) {
      let s = String(input).replace(/[\t\n\f\r ]+/g, "");
      if (s.length % 4 === 0) s = s.replace(/==?$/, "");
      if (s.length % 4 === 1 || /[^A-Za-z0-9+/]/.test(s)) throw invalid("atob: not base64");
      let out = "", acc = 0, bits = 0;
      for (let i = 0; i < s.length; i++) {
        acc = (acc << 6) | IDX[s.charCodeAt(i)];
        bits += 6;
        if (bits >= 8) { bits -= 8; out += String.fromCharCode((acc >> bits) & 0xFF); }
      }
      return out;
    };
  }
})(globalThis);
