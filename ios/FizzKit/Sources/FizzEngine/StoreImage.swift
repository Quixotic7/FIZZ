// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// FIZZ's store image moved between layouts: the emulated FIZZ keeps the host's 1 MiB flash (core/app/persist.h,
// MY PRESETS 32 sectors, the projects 224 from 0x20000), while its bulk channel shows the USB package's region (73
// sectors), so a backup of it restores onto an FM-1 and an FM-1's onto it. The same records, moved record by record:
// a port of site/manager/fizzstore.js mount / get / records / writeStore / buildImage / convertImage (written from
// core/store/store.c's layout: 4 KiB sectors, page 0 the header "Z2FS", pages 1..15 records of 240 bytes).

import Foundation

public enum StoreImage {
    public struct Store: Sendable { public let base: Int, sectors: Int, reserve: Int }
    public struct Layout: Sendable {
        public let name: String, magic: [UInt8], size: Int, nproj: Int, nlib: Int
        public let lib: Store, prj: Store
    }
    public static let device = Layout(name: "device", magic: Array("Z2FS".utf8), size: 73 * sector, nproj: 8, nlib: 256,
                                      lib: Store(base: 0, sectors: 22, reserve: 2),
                                      prj: Store(base: 22 * sector, sectors: 51, reserve: 10))
    public static let host = Layout(name: "host", magic: Array("Z2FS".utf8), size: 0x100000, nproj: 8, nlib: 256,
                                    lib: Store(base: 0, sectors: 32, reserve: 6),
                                    prj: Store(base: 0x20000, sectors: 224, reserve: 6))
    /// the Bluetooth package's store (persist.h QD_PKG_BLE: 4 projects, 128 presets)
    public static let ble = Layout(name: "ble", magic: Array("Z2FB".utf8), size: 49 * sector, nproj: 4, nlib: 128,
                                   lib: Store(base: 0, sectors: 11, reserve: 2),
                                   prj: Store(base: 11 * sector, sectors: 38, reserve: 10))
    public static let layouts = [device, ble, host]
    /// the USB package's region on the FM-1 (FZBI's offset)
    public static let deviceRegion: UInt32 = 0x97000

    public enum Failure: Error, Equatable { case full, range, notStore }
    public struct Record: Sendable, Equatable { public let key: UInt32, bytes: [UInt8] }

    static let sector = 4096, page = 256, pps = 16, hdr = 16, payload = 240, deleted = 0xFFFE

    /// The same records in another layout (fizzstore.js convertImage).
    public static func convert(_ img: [UInt8], from: Layout, to: Layout) throws -> [UInt8] {
        let r = records(img, from)
        return try build(to, lib: r.lib, prj: r.prj)
    }

    /// The layout of an image (fizzstore.js layoutOf): by its size, checked against its sectors' magic (a blank
    /// image passes); nil for a size FIZZ never had.
    public static func layout(of img: [UInt8]) -> Layout? {
        guard let l = layouts.first(where: { $0.size == img.count }) else { return nil }
        var magics = Set<[UInt8]>()
        for o in stride(from: 0, to: img.count - sector + 1, by: sector) {
            let m = Array(img[o..<(o + 4)])
            if layouts.contains(where: { $0.magic == m }) { magics.insert(m) }
        }
        return magics.isEmpty || magics.contains(l.magic) ? l : nil
    }

    /// A backup's store image as the emulated FIZZ's whole flash (site/js/backup-play.js backupFlash:
    /// convertImage(image, "host")): throws .notStore for an image of no FIZZ layout.
    public static func hostFlash(from img: [UInt8]) throws -> [UInt8] {
        guard let l = layout(of: img) else { throw Failure.notStore }
        return try convert(img, from: l, to: host)
    }

    /// Every live record of both stores, by key (fizzstore.js regionRecords).
    public static func records(_ img: [UInt8], _ l: Layout) -> (lib: [Record], prj: [Record]) {
        (records(img, l.lib, l.magic), records(img, l.prj, l.magic))
    }

    /// A whole region in the layout (fizzstore.js buildImage): throws .range for a record it has no place for, .full.
    public static func build(_ l: Layout, lib: [Record], prj: [Record]) throws -> [UInt8] {
        for r in lib where r.key >> 24 == 5 && Int(r.key & 0xFFFF) >= l.nlib { throw Failure.range }
        for r in prj where r.key >> 24 != 1 && Int((r.key >> 16) & 0xFF) >= l.nproj { throw Failure.range }
        var img = [UInt8](repeating: 0xFF, count: l.size)
        try write(&img, l.lib, lib, l.magic)
        try write(&img, l.prj, prj, l.magic)
        return img
    }

    // MARK: reading (store.c's mount and get)

    static func g16(_ b: UnsafeBufferPointer<UInt8>, _ o: Int) -> Int { Int(b[o]) | Int(b[o + 1]) << 8 }
    static func g32(_ b: UnsafeBufferPointer<UInt8>, _ o: Int) -> UInt32 {
        UInt32(b[o]) | UInt32(b[o + 1]) << 8 | UInt32(b[o + 2]) << 16 | UInt32(b[o + 3]) << 24
    }

    static let crcTable: [UInt16] = (0..<256).map { n in
        var c = UInt16(n) << 8
        for _ in 0..<8 { c = c & 0x8000 != 0 ? (c << 1) ^ 0x1021 : c << 1 }
        return c
    }
    /// CRC-16/CCITT-FALSE, continued from c
    static func crc16(_ c0: UInt16, _ b: UnsafeBufferPointer<UInt8>, _ from: Int, _ to: Int) -> UInt16 {
        var c = c0
        crcTable.withUnsafeBufferPointer { t in
            for i in from..<to { c = (c << 8) ^ t[Int((c >> 8) ^ UInt16(b[i]))] }
        }
        return c
    }

    static func pagesOf(_ len: Int) -> Int { len == deleted || len == 0 ? 1 : (len + payload - 1) / payload }

    struct Mounted {
        var sseq: [UInt32]
        var keys: [UInt32: (rseq: UInt32, page: Int, len: Int)] = [:]
    }

    static func mount(_ b: UnsafeBufferPointer<UInt8>, _ s: Store, _ magic: [UInt8]) -> Mounted {
        var m = Mounted(sseq: .init(repeating: 0, count: s.sectors))
        var order: [Int] = []
        for k in 0..<s.sectors {
            let o = s.base + k * sector
            let isMagic = b[o] == magic[0] && b[o + 1] == magic[1] && b[o + 2] == magic[2] && b[o + 3] == magic[3] &&
                g16(b, o + 8) == (~g16(b, o + 10) & 0xFFFF)
            let sq = g32(b, o + 4)
            if isMagic && sq != 0 && sq != 0xFFFF_FFFF {
                m.sseq[k] = sq
                order.append(k)
                continue
            }
            var blank = true
            for j in 0..<sector where b[o + j] != 0xFF { blank = false; break }
            m.sseq[k] = blank ? 0 : 0xFFFF_FFFF
        }
        order.sort { m.sseq[$0] < m.sseq[$1] }
        var open = false, okey: UInt32 = 0, orseq: UInt32 = 0, onext = 0, ofirst = -1, onp = 0, olen = 0
        for sec in order {
            for p in 1..<pps {
                let pg = sec * pps + p, o = s.base + pg * page
                let key = g32(b, o), rseq = g32(b, o + 4), idx = g16(b, o + 8), np = g16(b, o + 10), len = g16(b, o + 12)
                if key == 0xFFFF_FFFF && rseq == 0xFFFF_FFFF { open = false; break }
                let ok = crc16(crc16(0xFFFF, b, o, o + 14), b, o + hdr, o + page) == UInt16(g16(b, o + 14))
                if !ok || np == 0 { open = false; continue }
                if idx == 0 {
                    open = true; okey = key; orseq = rseq; onext = 1; ofirst = pg; onp = np; olen = len
                } else if !open || key != okey || rseq != orseq || idx != onext {
                    open = false; continue
                } else {
                    onext += 1
                }
                if open && onext == onp {
                    open = false
                    if onp != pagesOf(olen) { continue }
                    if let e = m.keys[okey], e.rseq > orseq { continue }
                    m.keys[okey] = (orseq, ofirst, olen)
                }
            }
        }
        return m
    }

    static func nextPage(_ m: Mounted, _ pg: Int) -> Int {
        let sec = pg / pps, p = pg % pps
        if p < pps - 1 { return pg + 1 }
        var best = -1
        for k in m.sseq.indices {
            let live = m.sseq[k] != 0 && m.sseq[k] != 0xFFFF_FFFF
            if live && m.sseq[k] > m.sseq[sec] && (best < 0 || m.sseq[k] < m.sseq[best]) { best = k }
        }
        return best < 0 ? -1 : best * pps + 1
    }

    static func records(_ img: [UInt8], _ s: Store, _ magic: [UInt8]) -> [Record] {
        img.withUnsafeBufferPointer { b in
            let m = mount(b, s, magic)
            var out: [Record] = []
            for key in m.keys.keys.sorted() {
                let e = m.keys[key]!
                guard e.len != deleted else { continue }
                var bytes: [UInt8] = []
                bytes.reserveCapacity(e.len)
                var pg = e.page
                for _ in 0..<pagesOf(e.len) where pg >= 0 {
                    let take = min(payload, e.len - bytes.count), o = s.base + pg * page + hdr
                    bytes += b[o..<(o + take)]
                    pg = nextPage(m, pg)
                }
                if bytes.count == e.len { out.append(Record(key: key, bytes: bytes)) }
            }
            return out
        }
    }

    // MARK: writing (fizzstore.js writeStore: packed, sectors and records numbered 1, 2, ..)

    static func write(_ img: inout [UInt8], _ s: Store, _ recs: [Record], _ magic: [UInt8]) throws {
        func p16(_ o: Int, _ v: Int) { img[o] = UInt8(v & 0xFF); img[o + 1] = UInt8((v >> 8) & 0xFF) }
        func p32(_ o: Int, _ v: UInt32) { for i in 0..<4 { img[o + i] = UInt8((v >> (8 * UInt32(i))) & 0xFF) } }
        var sec = -1, p = pps, rseq: UInt32 = 0
        func nextPage() throws -> Int {
            if p == pps {
                sec += 1
                if sec >= s.sectors - s.reserve { throw Failure.full }
                let o = s.base + sec * sector
                for i in 0..<4 { img[o + i] = magic[i] }
                p32(o + 4, UInt32(sec + 1))
                p16(o + 8, 0x5A5A); p16(o + 10, ~0x5A5A & 0xFFFF)
                p = 1
            }
            defer { p += 1 }
            return s.base + (sec * pps + p) * page
        }
        for r in recs {
            let len = r.bytes.count, np = pagesOf(len)
            guard len < deleted else { throw Failure.range }
            rseq += 1
            for idx in 0..<np {
                let o = try nextPage(), from = idx * payload, take = min(payload, len - from)
                for i in (o + hdr)..<(o + page) { img[i] = 0 }
                if take > 0 { img.replaceSubrange((o + hdr)..<(o + hdr + take), with: r.bytes[from..<(from + take)]) }
                p32(o, r.key); p32(o + 4, rseq); p16(o + 8, idx); p16(o + 10, np); p16(o + 12, len)
                let c = img.withUnsafeBufferPointer { crc16(crc16(0xFFFF, $0, o, o + 14), $0, o + hdr, o + page) }
                p16(o + 14, Int(c))
            }
        }
    }
}
