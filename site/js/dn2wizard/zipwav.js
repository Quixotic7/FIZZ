// SPDX-License-Identifier: GPL-3.0-only
// A store-only ZIP writer (no compression: the records and the WAVs are what they are) and a 16-bit WAV encoder.
const CRC = Array.from({ length: 256 }, (_, n) => { let c = n; for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1; return c >>> 0; });
export function crc32(b) {
  let c = 0xffffffff;
  for (let i = 0; i < b.length; i++) c = CRC[(c ^ b[i]) & 255] ^ (c >>> 8);
  return (c ^ 0xffffffff) >>> 0;
}
const le16 = (v) => [v & 255, (v >> 8) & 255], le32 = (v) => [v & 255, (v >>> 8) & 255, (v >>> 16) & 255, (v >>> 24) & 255];

// files: [{name, data: Uint8Array | string}] -> Uint8Array (a ZIP, entries stored)
export function zipStore(files, when = new Date()) {
  const enc = new TextEncoder(), parts = [], cen = [];
  const dt = ((when.getHours() << 11) | (when.getMinutes() << 5) | (when.getSeconds() >> 1)) & 0xffff;
  const dd = (((when.getFullYear() - 1980) << 9) | ((when.getMonth() + 1) << 5) | when.getDate()) & 0xffff;
  let off = 0;
  for (const f of files) {
    const nb = enc.encode(f.name), data = typeof f.data === "string" ? enc.encode(f.data) : f.data, c = crc32(data);
    const fix = [...le16(20), ...le16(0x0800), ...le16(0), ...le16(dt), ...le16(dd), ...le32(c), ...le32(data.length), ...le32(data.length),
      ...le16(nb.length), ...le16(0)];
    const head = Uint8Array.from([...le32(0x04034b50), ...fix, ...nb]);
    parts.push(head, data);
    cen.push(Uint8Array.from([...le32(0x02014b50), ...le16(20), ...fix, ...le16(0), ...le16(0), ...le16(0), ...le32(0), ...le32(off), ...nb]));
    off += head.length + data.length;
  }
  const cl = cen.reduce((a, b) => a + b.length, 0);
  const end = Uint8Array.from([...le32(0x06054b50), 0, 0, 0, 0, ...le16(files.length), ...le16(files.length), ...le32(cl), ...le32(off), 0, 0]);
  const out = new Uint8Array(off + cl + end.length);
  let p = 0;
  for (const b of [...parts, ...cen, end]) { out.set(b, p); p += b.length; }
  return out;
}

// mono Float32 samples -> a 16-bit PCM WAV
export function wav16(samples, rate) {
  const n = samples.length, b = new Uint8Array(44 + 2 * n), v = new DataView(b.buffer);
  const s = (o, t) => { for (let i = 0; i < t.length; i++) b[o + i] = t.charCodeAt(i); };
  s(0, "RIFF"); v.setUint32(4, 36 + 2 * n, true); s(8, "WAVE"); s(12, "fmt ");
  v.setUint32(16, 16, true); v.setUint16(20, 1, true); v.setUint16(22, 1, true); v.setUint32(24, rate, true);
  v.setUint32(28, rate * 2, true); v.setUint16(32, 2, true); v.setUint16(34, 16, true); s(36, "data"); v.setUint32(40, 2 * n, true);
  for (let i = 0; i < n; i++) v.setInt16(44 + 2 * i, Math.max(-32768, Math.min(32767, Math.round(samples[i] * 32767))), true);
  return b;
}
