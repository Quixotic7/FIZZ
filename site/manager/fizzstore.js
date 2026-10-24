// SPDX-License-Identifier: GPL-3.0-only
// FIZZ's store image read in the browser, without a unit: the reader half of core/store/store.c (the page store) and
// the few record layouts of core/app/persist.c the manager shows. Written from FIZZ's own sources (store.h's layout,
// store.c's mount and get, persist.c's ser_hdr / ser_snd); no DOM, the tests run it in Node.
//
//   LAYOUTS                       the region's two stores (MY PRESETS' and the projects'), by the region's size
//   layoutOf(size)                -> a layout, or null (a size FIZZ never had)
//   mount(img, base, nsec)        -> one store's index {magic, sectors, keys: Map(key -> {rseq, page, len}), live, total}
//   get(store, key)               -> the record's bytes, or null (none, a tombstone)
//   readRegion(img)               -> {layout, magic, format, tracks, projects: [{slot, name}], presets: [{slot, id,
//                                     name, tags, flags, imported, blob, drive, pan, sends}], used: {lib, prj, all}}
//                                    | throws (not a FIZZ store image)
//
// THE LAYOUT (store.h): 4 KiB sectors; page 0 of a sector its header (magic "Z?FS", the sector's sequence number,
// 5A5A and its complement); pages 1..15 records. A page: key u32, the record's sequence u32, the page's index u16, the
// record's page count u16, its length u16 (FFFE a tombstone), CRC-16/CCITT-FALSE u16 over the header's 14 bytes and the
// 240-byte payload. A record's pages run on into the sector the log opened next; the highest sequence wins a key.
export const SECTOR = 4096, PAGE = 256, PPS = 16, HDR = 16, PAYLOAD = 240, DELETED = 0xFFFE;
export const LAYOUTS = {
  // persist.h (QD_DEVICE): the unit's data region 0x97000..0xDFFFF, MY PRESETS 22 sectors then the projects' 51
  device: { name: "device", size: 73 * SECTOR, lib: { base: 0, nsec: 22 }, prj: { base: 22 * SECTOR, nsec: 51 } },
  // persist.h (the host, the browser preview's wasm): 1 MiB, MY PRESETS 32 sectors, the projects' 224 from 0x20000
  host: { name: "host", size: 0x100000, lib: { base: 0, nsec: 32 }, prj: { base: 0x20000, nsec: 224 } },
};
export const layoutOf = (size) => Object.values(LAYOUTS).find((l) => l.size === size) || null;

const RK = { SET: 1, HDR: 2, PAT: 3, SND: 4, LIB: 5, FTAG: 6, MFX: 7 };   // persist.h QD_RK_*
const rkey = (kind, p, i) => ((kind << 24) | (p << 16) | i) >>> 0;
export const NPROJ = 8, NLIB = 256, PRESET_MINE0 = 0x100;
const NAME = 13, BLOB = 128;

const g16 = (b, o) => b[o] | (b[o + 1] << 8);
const g32 = (b, o) => (b[o] | (b[o + 1] << 8) | (b[o + 2] << 16) | (b[o + 3] << 24)) >>> 0;
function crc16(c, b, from, to) {
  for (let i = from; i < to; i++) {
    c ^= b[i] << 8;
    for (let k = 0; k < 8; k++) c = c & 0x8000 ? ((c << 1) ^ 0x1021) & 0xFFFF : (c << 1) & 0xFFFF;
  }
  return c;
}
const pagesOf = (len) => (len === DELETED || !len ? 1 : Math.ceil(len / PAYLOAD));
const isMagic = (b, o) => b[o] === 0x5A && b[o + 2] === 0x46 && b[o + 3] === 0x53;   // "Z?FS" (Z2FS today)

export function mount(img, base, nsec) {
  const sseq = new Array(nsec).fill(0);
  const order = [];
  let magic = "";
  for (let k = 0; k < nsec; k++) {
    const o = base + k * SECTOR;
    if (isMagic(img, o) && g32(img, o + 4) && g32(img, o + 4) !== 0xFFFFFFFF && g16(img, o + 8) === (~g16(img, o + 10) & 0xFFFF)) {
      sseq[k] = g32(img, o + 4);
      order.push(k);
      magic = magic || String.fromCharCode(img[o], img[o + 1], img[o + 2], img[o + 3]);
      continue;
    }
    let blank = true;
    for (let j = 0; j < SECTOR && blank; j++) blank = img[o + j] === 0xFF;
    sseq[k] = blank ? 0 : 0xFFFFFFFF;
  }
  order.sort((a, b) => sseq[a] - sseq[b]);
  const st = { img, base, nsec, sseq, magic, keys: new Map() };
  const hdr = (pg) => {
    const o = base + pg * PAGE;
    const h = { key: g32(img, o), rseq: g32(img, o + 4), idx: g16(img, o + 8), np: g16(img, o + 10), len: g16(img, o + 12), crc: g16(img, o + 14) };
    if (h.key === 0xFFFFFFFF && h.rseq === 0xFFFFFFFF) return { r: -1, h };
    return { r: crc16(crc16(0xFFFF, img, o, o + 14), img, o + HDR, o + PAGE) === h.crc ? 1 : 0, h };
  };
  let open = false, okey = 0, orseq = 0, onext = 0, ofirst = -1, onp = 0, olen = 0;
  for (let i = 0; i < order.length; i++) {
    const sec = order[i];
    for (let p = 1; p < PPS; p++) {
      const pg = sec * PPS + p, { r, h } = hdr(pg);
      if (r < 0) { open = false; break; }
      if (r === 0 || !h.np) { open = false; continue; }
      if (h.idx === 0) { open = true; okey = h.key; orseq = h.rseq; onext = 1; ofirst = pg; onp = h.np; olen = h.len; }
      else if (!open || h.key !== okey || h.rseq !== orseq || h.idx !== onext) { open = false; continue; }
      else onext++;
      if (open && onext === onp) {                        // the commit page: store.c index_put
        open = false;
        if (onp !== pagesOf(olen)) continue;
        const e = st.keys.get(okey);
        if (e && e.rseq > orseq) continue;
        st.keys.set(okey, { rseq: orseq, page: ofirst, len: olen });
      }
    }
  }
  st.live = [...st.keys.values()].reduce((n, e) => n + pagesOf(e.len), 0);
  st.total = nsec * (PPS - 1);
  st.sectors = order.length;
  return st;
}

function nextPage(st, pg) {
  const sec = Math.floor(pg / PPS), p = pg % PPS;
  if (p < PPS - 1) return pg + 1;
  let best = -1;
  for (let k = 0; k < st.nsec; k++) {
    const live = st.sseq[k] && st.sseq[k] !== 0xFFFFFFFF;
    if (live && st.sseq[k] > st.sseq[sec] && (best < 0 || st.sseq[k] < st.sseq[best])) best = k;
  }
  return best < 0 ? -1 : best * PPS + 1;
}

export function get(st, key) {
  const e = st.keys.get(key >>> 0);
  if (!e || e.len === DELETED) return null;
  const out = new Uint8Array(e.len);
  let pg = e.page, got = 0;
  for (let k = 0; k < pagesOf(e.len) && pg >= 0; k++) {
    const take = Math.min(PAYLOAD, e.len - got), o = st.base + pg * PAGE + HDR;
    out.set(st.img.subarray(o, o + take), got);
    got += take;
    pg = nextPage(st, pg);
  }
  return got === e.len ? out : null;
}

const cstr = (b, o, n) => { let s = ""; for (let i = 0; i < n && b[o + i]; i++) s += String.fromCharCode(b[o + i]); return s; };

// a MY PRESETS record ('S' 5: name[13], the version-6 blob, id u16, tags, flags, pan, chorus, delay, reverb)
function preset(b, slot) {
  if (!b || b.length < 2 || b[0] !== 0x53 || b[1] !== 5 || b.length !== 2 + NAME + BLOB + 8) return null;
  const o = 2 + NAME + BLOB, blob = b.slice(2 + NAME, o);
  const pan = (b[o + 4] << 24) >> 24;
  return { slot, id: g16(b, o), name: cstr(b, 2, NAME - 1) || "?", tags: b[o + 2] & 0x7F, flags: b[o + 3],
    imported: !!(b[o + 3] & 1), blob, drive: blob[87], pan: Math.max(-64, Math.min(63, pan)),
    sends: { chorus: b[o + 5] & 0x7F, delay: b[o + 6] & 0x7F, reverb: b[o + 7] & 0x7F } };
}

export function readRegion(img, layout = layoutOf(img.length)) {
  if (!(img instanceof Uint8Array)) img = new Uint8Array(img);
  if (!layout) throw new Error(`not a FIZZ store image (${img.length} bytes: the unit's is ${LAYOUTS.device.size})`);
  const lib = mount(img, layout.lib.base, layout.lib.nsec), prj = mount(img, layout.prj.base, layout.prj.nsec);
  const blank = !lib.sectors && !prj.sectors && img.every((x) => x === 0xFF);
  if (!lib.sectors && !prj.sectors && !blank) throw new Error("not a FIZZ store image (no FIZZ sector in it)");
  const presets = [];
  for (let k = 0; k < NLIB; k++) { const p = preset(get(lib, rkey(RK.LIB, 0, k)), k + 1); if (p) presets.push(p); }
  const projects = [];
  for (let p = 0; p < NPROJ; p++) {
    const h = get(prj, rkey(RK.HDR, p, 0));
    if (h && h[0] === 0x48 && h[1] === 1) projects.push({ slot: p + 1, name: cstr(h, 2, NAME - 1), demo: !!h[2 + NAME] });
  }
  const set = get(prj, rkey(RK.SET, 0, 0));
  // persist.h QD_PRJ_FORMAT: 'G' 2 six tracks (2026-10-24), 'G' 1 eight; a store without it: an empty or older one
  const format = set && set.length >= 2 && set[0] === 0x47 ? set[1] : null;
  const tracks = format === 2 ? 6 : format === 1 ? 8 : null;
  return { layout: layout.name, size: img.length, magic: lib.magic || prj.magic || "", blank, format, tracks, projects, presets,
    used: { lib: [lib.live, lib.total], prj: [prj.live, prj.total], all: (lib.live + prj.live) / (lib.total + prj.total) } };
}
