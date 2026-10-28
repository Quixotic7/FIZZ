// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
// FIZZ's store image read in the browser, without a unit: the reader half of core/store/store.c (the page store) and
// the few record layouts of core/app/persist.c the manager shows. Written from FIZZ's own sources (store.h's layout,
// store.c's mount and get, persist.c's ser_hdr / ser_snd); no DOM, the tests run it in Node.
//
//   LAYOUTS                       the region's two stores (MY PRESETS' and the projects'), per package
//   layoutOf(size, img)           -> a layout, or null (a size FIZZ never had; an image whose sectors are another
//                                    layout's: its magic)
//   magicsOf(img)                 -> the sector magics the image holds ("Z2FS", "Z2FB"), a Set
//   mount(img, base, nsec, magic) -> one store's index {magic, sectors, keys: Map(key -> {rseq, page, len}), live, total}
//   get(store, key)               -> the record's bytes, or null (none, a tombstone)
//   readRegion(img)               -> {layout, package, nproj, nlib, magic, format, tracks, projects: [{slot, name}],
//                                     presets: [{slot, id, name, tags, flags, imported, blob, drive, pan, sends}],
//                                     used: {lib, prj, all}}
//                                    | throws (not a FIZZ store image)
//
// THE LAYOUT (store.h): 4 KiB sectors; page 0 of a sector its header (magic "Z2FS", the Bluetooth package's "Z2FB";
// the sector's sequence number, 5A5A and its complement); pages 1..15 records. A page: key u32, the record's sequence
// u32, the page's index u16, the record's page count u16, its length u16 (FFFE a tombstone), CRC-16/CCITT-FALSE u16 over
// the header's 14 bytes and the 240-byte payload. A record's pages run on into the sector the log opened next; the
// highest sequence wins a key. A store reads only its own package's sectors: another magic is a sector to erase.
//
// THE TWO PACKAGES (core/app/persist.h, firmware/BUDGET.md "What the other tools need"): the USB package's region is
// 73 sectors from 0x97000 ("Z2FS": MY PRESETS 22 sectors, the projects 51; 8 projects, 256 MY PRESETS); the Bluetooth
// package's 49 sectors from 0xAF000 ("Z2FB": MY PRESETS 11, the projects 38 from 11 x 4096; 4 projects, 128). The
// records are the same in both: an image goes from one to the other record by record (projects.js fitModel), never
// sector by sector.
export const SECTOR = 4096, PAGE = 256, PPS = 16, HDR = 16, PAYLOAD = 240, DELETED = 0xFFFE;
export const RESERVE = 6;                                   // store.h QDS_RESERVE
// the blank sectors the unit keeps in each store (persist.c: the library's QD_FLASH_LIB_RESERVE, the projects' 10 for a
// full pattern's record); an image written here leaves as many (the host's: QDS_RESERVE)
export const LAYOUTS = {
  // persist.h (QD_DEVICE): the USB package's data region 0x97000..0xDFFFF, MY PRESETS 22 sectors then the projects' 51
  device: { name: "device", package: "usb", magic: "Z2FS", region: 0x97000, size: 73 * SECTOR, nproj: 8, nlib: 256,
    lib: { base: 0, nsec: 22, reserve: 2 }, prj: { base: 22 * SECTOR, nsec: 51, reserve: 10 } },
  // persist.h (QD_DEVICE, QD_PKG_BLE): the Bluetooth package's store 0xAF000..0xDFFFF (its second data blob before it)
  ble: { name: "ble", package: "ble", magic: "Z2FB", region: 0xAF000, size: 49 * SECTOR, nproj: 4, nlib: 128,
    lib: { base: 0, nsec: 11, reserve: 2 }, prj: { base: 11 * SECTOR, nsec: 38, reserve: 10 } },
  // persist.h (the host, the browser preview's wasm): 1 MiB, MY PRESETS 32 sectors, the projects' 224 from 0x20000
  host: { name: "host", package: "usb", magic: "Z2FS", size: 0x100000, nproj: 8, nlib: 256,
    lib: { base: 0, nsec: 32, reserve: RESERVE }, prj: { base: 0x20000, nsec: 224, reserve: RESERVE } },
};
export const layoutByName = (n) => (typeof n === "string" ? LAYOUTS[n] : n) || null;
// a layout by the image's size, checked against its sectors' magic when the image is given (a blank one passes)
export function layoutOf(size, img = null) {
  const l = Object.values(LAYOUTS).find((x) => x.size === size) || null;
  if (!l || !img) return l;
  const m = magicsOf(img);
  return !m.size || m.has(l.magic) ? l : null;
}
export const PACKAGE_NAME = { usb: "USB", ble: "Bluetooth" };

const RK = { SET: 1, HDR: 2, PAT: 3, SND: 4, LIB: 5, FTAG: 6, MFX: 7 };   // persist.h QD_RK_*
const rkey = (kind, p, i) => ((kind << 24) | (p << 16) | i) >>> 0;
export const NPROJ = 8, NLIB = 256, PRESET_MINE0 = 0x100;   // (the USB package's; a layout's own: nproj, nlib)
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
// a sector's magic: "Z2FS" (the USB package's, and the host's) or "Z2FB" (the Bluetooth package's); "" none
const MAGICS = ["Z2FS", "Z2FB"];
function sectorMagic(b, o) {
  if (o + 12 > b.length || b[o] !== 0x5A || b[o + 2] !== 0x46 || (b[o + 3] !== 0x53 && b[o + 3] !== 0x42)) return "";
  if (g16(b, o + 8) !== (~g16(b, o + 10) & 0xFFFF)) return "";
  return String.fromCharCode(b[o], b[o + 1], b[o + 2], b[o + 3]);
}
export function magicsOf(img) {
  const out = new Set();
  for (let o = 0; o + SECTOR <= img.length; o += SECTOR) { const m = sectorMagic(img, o); if (MAGICS.includes(m)) out.add(m); }
  return out;
}

export function mount(img, base, nsec, want = "") {
  const sseq = new Array(nsec).fill(0);
  const order = [];
  let magic = "";
  for (let k = 0; k < nsec; k++) {
    const o = base + k * SECTOR;
    const mg = sectorMagic(img, o);
    if (mg && (!want || mg === want) && g32(img, o + 4) && g32(img, o + 4) !== 0xFFFFFFFF) {
      sseq[k] = g32(img, o + 4);
      order.push(k);
      magic = magic || mg;
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
export function preset(b, slot) {
  if (!b || b.length < 2 || b[0] !== 0x53 || b[1] !== 5 || b.length !== 2 + NAME + BLOB + 8) return null;
  const o = 2 + NAME + BLOB, blob = b.slice(2 + NAME, o);
  const pan = (b[o + 4] << 24) >> 24;
  return { slot, id: g16(b, o), name: cstr(b, 2, NAME - 1) || "?", tags: b[o + 2] & 0x7F, flags: b[o + 3],
    imported: !!(b[o + 3] & 1), blob, drive: blob[87], pan: Math.max(-64, Math.min(63, pan)),
    sends: { chorus: b[o + 5] & 0x7F, delay: b[o + 6] & 0x7F, reverb: b[o + 7] & 0x7F } };
}

export function readRegion(img, layout = null) {
  if (!(img instanceof Uint8Array)) img = new Uint8Array(img);
  layout = layoutByName(layout) || layoutOf(img.length, img);
  if (!layout) {
    const other = layoutOf(img.length), m = [...magicsOf(img)];
    throw new Error(other ? `not a ${PACKAGE_NAME[other.package]} package's store: its sectors are ${m.join(", ")}`
      : `not a FIZZ store image (${img.length} bytes: the USB package's is ${LAYOUTS.device.size}, the Bluetooth package's ${LAYOUTS.ble.size})`);
  }
  const lib = mount(img, layout.lib.base, layout.lib.nsec, layout.magic), prj = mount(img, layout.prj.base, layout.prj.nsec, layout.magic);
  const blank = !lib.sectors && !prj.sectors && img.every((x) => x === 0xFF);
  if (!lib.sectors && !prj.sectors && !blank) throw new Error("not a FIZZ store image (no FIZZ sector in it)");
  const presets = [];
  for (let k = 0; k < layout.nlib; k++) { const p = preset(get(lib, rkey(RK.LIB, 0, k)), k + 1); if (p) presets.push(p); }
  const projects = [];
  for (let p = 0; p < layout.nproj; p++) {
    const h = get(prj, rkey(RK.HDR, p, 0));
    if (h && h[0] === 0x48 && h[1] === 1) projects.push({ slot: p + 1, name: cstr(h, 2, NAME - 1), demo: !!h[2 + NAME] });
  }
  const set = get(prj, rkey(RK.SET, 0, 0));
  // persist.h QD_PRJ_FORMAT: 'G' 2 six tracks (2026-10-24), 'G' 1 eight; a store without it: an empty or older one
  const format = set && set.length >= 2 && set[0] === 0x47 ? set[1] : null;
  const tracks = format === 2 ? 6 : format === 1 ? 8 : null;
  return { layout: layout.name, package: layout.package, nproj: layout.nproj, nlib: layout.nlib, size: img.length,
    magic: lib.magic || prj.magic || "", blank, format, tracks, projects, presets,
    used: { lib: [lib.live, lib.total], prj: [prj.live, prj.total], all: (lib.live + prj.live) / (lib.total + prj.total) } };
}

// ---- the writer (Manager PROJECTS, backups in the browser's FIZZ): a store written fresh, as store.c's own writes
// would leave it, but packed: the records one after another from the region's first sector, the sectors' sequence
// numbers 1, 2, .. in place order (a record runs on into the next sector, the log's next), the records' 1, 2, ..; every
// other sector erased. A tombstone is left out (no record is the same as a deleted one). The unit keeps blank sectors
// for its garbage collection (the layout's reserve): an image that would leave fewer is refused.
//
//   records(st)                      -> [{key, bytes}] every live record of a mounted store, by key
//   writeStore(img, base, nsec, recs, magic, reserve) -> sectors used | throws {code: "full"}
//   buildImage(layout, {lib, prj})   -> a whole region (layout.size bytes) in the layout's magic | throws {code:
//                                       "full" | "range"} (a project or a MY PRESETS place the layout does not have)
//   regionRecords(img)               -> {layout, magic, lib: [{key, bytes}], prj: [..]}
//   convertImage(img, to)            -> the same records in another layout ("device" <-> "host": a backup in the
//                                       browser's FIZZ, the 1 MiB flash the player keeps; "ble" -> "device" | "host";
//                                       into "ble" only what fits it: projects.js fitModel chooses)
const p16 = (b, o, v) => { b[o] = v & 0xFF; b[o + 1] = (v >>> 8) & 0xFF; };
const p32 = (b, o, v) => { b[o] = v & 0xFF; b[o + 1] = (v >>> 8) & 0xFF; b[o + 2] = (v >>> 16) & 0xFF; b[o + 3] = (v >>> 24) & 0xFF; };

export function records(st) {
  const out = [];
  for (const key of [...st.keys.keys()].sort((a, b) => a - b)) {
    const bytes = get(st, key);
    if (bytes) out.push({ key, bytes });
  }
  return out;
}

export function writeStore(img, base, nsec, recs, magic = "Z2FS", reserve = RESERVE) {
  img.fill(0xFF, base, base + nsec * SECTOR);
  let sec = -1, p = PPS, rseq = 0;
  const page = () => {
    if (p === PPS) {                                        // the next sector, its header (store.c open_sector)
      if (++sec >= nsec - reserve) throw Object.assign(new Error(`the records do not fit (${nsec - reserve} sectors)`), { code: "full" });
      const o = base + sec * SECTOR;
      for (let i = 0; i < 4; i++) img[o + i] = magic.charCodeAt(i);
      p32(img, o + 4, sec + 1);
      p16(img, o + 8, 0x5A5A); p16(img, o + 10, ~0x5A5A & 0xFFFF);
      p = 1;
    }
    return base + (sec * PPS + p++) * PAGE;
  };
  for (const { key, bytes } of recs) {
    const len = bytes.length, np = pagesOf(len);
    if (len >= DELETED) throw new Error(`record ${(key >>> 0).toString(16)}: ${len} bytes`);
    rseq++;
    for (let idx = 0; idx < np; idx++) {                    // (store.c write_page)
      const o = page(), take = Math.min(PAYLOAD, len - idx * PAYLOAD);
      img.fill(0, o + HDR, o + PAGE);
      if (take > 0) img.set(bytes.subarray(idx * PAYLOAD, idx * PAYLOAD + take), o + HDR);
      p32(img, o, key >>> 0); p32(img, o + 4, rseq); p16(img, o + 8, idx); p16(img, o + 10, np); p16(img, o + 12, len);
      p16(img, o + 14, crc16(crc16(0xFFFF, img, o, o + 14), img, o + HDR, o + PAGE));
    }
  }
  return sec + 1;
}

// a record the layout has no place for: a project past its nproj, a MY PRESETS place past its nlib
function outside(layout, key, inLib) {
  const kind = key >>> 24, p = (key >>> 16) & 0xFF, i = key & 0xFFFF;
  return inLib ? kind === RK.LIB && i >= layout.nlib : kind !== RK.SET && p >= layout.nproj;
}
export function buildImage(layout, { lib = [], prj = [] } = {}) {
  layout = layoutByName(layout);
  const bad = [...lib.filter((r) => outside(layout, r.key, true)), ...prj.filter((r) => outside(layout, r.key, false))];
  if (bad.length) throw Object.assign(new Error(`${bad.length} records have no place in the ${PACKAGE_NAME[layout.package]} package's store (${layout.nproj} projects, ${layout.nlib} MY PRESETS)`), { code: "range" });
  const img = new Uint8Array(layout.size).fill(0xFF);
  writeStore(img, layout.lib.base, layout.lib.nsec, lib, layout.magic, layout.lib.reserve);
  writeStore(img, layout.prj.base, layout.prj.nsec, prj, layout.magic, layout.prj.reserve);
  return img;
}

export function regionRecords(img, layout = null) {
  layout = layoutByName(layout) || layoutOf(img.length, img);
  if (!layout) throw new Error(`not a FIZZ store image (${img.length} bytes)`);
  const lib = mount(img, layout.lib.base, layout.lib.nsec, layout.magic), prj = mount(img, layout.prj.base, layout.prj.nsec, layout.magic);
  return { layout, magic: layout.magic, lib: records(lib), prj: records(prj) };
}

export function convertImage(img, to) {
  const r = regionRecords(img);
  return buildImage(to, { lib: r.lib, prj: r.prj });
}
