// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// A FIZZ unit in the app's own process (no MIDI): it answers the update handshake, IDENTITY, STATUS, LIST in parts,
// DUMP (one, or slot 0 streaming), PROJECTS and FZBI with frames made by FizzKit's builders, so FizzLink reads it as
// it reads an FM-1. Variants: a unit in update mode, one that is not FIZZ, and two units at once.

import Foundation
import FizzKit

// MARK: - The demo unit's data

/// One preset of the demo unit.
public struct DemoPreset: Sendable {
    public let slot: Int, id: Int, name: String, tags: Int, algorithm: Int
}

/// What a demo unit holds: its identity, presets, pool, projects, store.
public final class DemoUnit: Sendable {
    public let portName: String
    public let idText: String
    public let version: String?             // nil: not FIZZ (nothing but the handshake answered)
    public let mine: [DemoPreset]
    public let factory: [DemoPreset]
    public let pool: [(place: Int, preset: DemoPreset)]
    public let projects: [(name: String, pages: Int, demo: Bool)?]
    public let current: Int                  // 1..8
    public let storeSize: UInt32 = 299_008
    public let lib = [14, 18], prj = [30, 55]

    init(portName: String, idText: String, version: String?, mine: [DemoPreset] = [], factory: [DemoPreset] = [],
         pool: [(place: Int, preset: DemoPreset)] = [], projects: [(name: String, pages: Int, demo: Bool)?] = [],
         current: Int = 1) {
        self.portName = portName; self.idText = idText; self.version = version
        self.mine = mine; self.factory = factory; self.pool = pool; self.projects = projects; self.current = current
    }

    /// A FIZZ 0.12.0-beta.11 unit: 143 MY PRESETS, 23 factory, a pool of 19, 6 of 8 projects (one the demo).
    public static func fizz(port: String = "FIZZ FM-1") -> DemoUnit {
        let mine = DemoNames.mine.enumerated().map { k, p in
            DemoPreset(slot: k + 1, id: 0x1000 + (k + 1) * 7, name: p.0, tags: p.1, algorithm: p.2)
        }
        let factory = DemoNames.factory.enumerated().map { k, p in
            DemoPreset(slot: k + 1, id: 100 + k + 1, name: p.0, tags: p.1, algorithm: p.2)
        }
        let m = { (s: Int) in mine[s - 1] }, f = { (s: Int) in factory[s - 1] }
        let pool: [(place: Int, preset: DemoPreset)] = [
            (1, f(1)), (2, m(1)), (3, m(2)), (4, m(3)), (5, m(5)), (6, m(12)), (7, m(15)), (8, m(6)), (10, m(4)),
            (11, m(8)), (13, m(7)), (14, m(10)), (16, m(11)), (17, m(20)), (18, m(21)), (21, f(4)), (22, m(14)),
            (25, m(30)), (26, m(9)),
        ]
        return DemoUnit(portName: port, idText: "FM-1_46001100", version: "FIZZ 0.12.0-beta.11", mine: mine,
                        factory: factory, pool: pool,
                        projects: [("NOIR", 18, false), ("SUNDAY", 9, false), ("FIZZ TOUR", 16, true), ("SKETCH", 3, false),
                                   nil, ("ACID 3", 12, false), ("WINTER LP", 11, false), nil],
                        current: 1)
    }

    public static func updateMode() -> DemoUnit { DemoUnit(portName: "FM-1", idText: "ota-FM-1_0100", version: nil) }
    public static func notFizz() -> DemoUnit { DemoUnit(portName: "FM-1", idText: "FM-1_016", version: nil) }
    public static func choralRoot() -> DemoUnit {
        DemoUnit(portName: "ChoralRoot FM-1", idText: "FM-1_42002601", version: nil)
    }

    var inPool: Set<Int> { Set(pool.map(\.preset.id)) }

    /// A preset's 128-byte blob ('Q' 6): the algorithm in byte 2 (value 0 minus its min, 1), the rest believable.
    static func blob(_ p: DemoPreset) -> [UInt8] {
        var b = [UInt8](repeating: 0, count: FizzSysEx.blob6Len)
        b[0] = 0x51; b[1] = 6; b[2] = UInt8(max(0, p.algorithm - 1))
        var s = UInt32(truncatingIfNeeded: p.id &* 2_654_435_761)
        for i in 3..<124 {
            s = s &* 1_103_515_245 &+ 12345
            b[i] = UInt8((s >> 16) & 0x7f)
        }
        b[124] = 0; b[125] = 0; b[126] = 20; b[127] = 64
        return b
    }
}

// MARK: - The transport

/// One open port on a demo unit: requests in, replies out a little later.
public final class DemoFizz: MIDITransport, @unchecked Sendable {
    public let name: String
    public let messages: AsyncStream<[UInt8]>
    private let cont: AsyncStream<[UInt8]>.Continuation
    private let unit: DemoUnit
    private let lock = NSLock()
    private var closed = false, lostFlag = false
    private var queue: Task<Void, Never>?
    /// between the replies of a request (ms)
    public var replyMs: Double = 2

    public init(unit: DemoUnit) {
        self.unit = unit
        name = unit.portName
        (messages, cont) = AsyncStream.makeStream(of: [UInt8].self)
    }

    public var isLost: Bool { lock.withLock { lostFlag } }

    public func send(_ bytes: [UInt8]) throws {
        let (c, l) = lock.withLock { (closed, lostFlag) }
        if l { throw MIDITransportError.lost }
        if c { throw MIDITransportError.closed }
        let replies = DemoReplies(unit: unit).answer(bytes)
        guard !replies.isEmpty else { return }
        let prev = lock.withLock { queue }
        let ms = replyMs
        let t = Task { [weak self] in
            await prev?.value
            for r in replies {
                try? await Task.sleep(for: .microseconds(Int(ms * 1000)))
                guard let self, !self.lock.withLock({ self.closed || self.lostFlag }) else { return }
                self.cont.yield(r)
            }
        }
        lock.withLock { queue = t }
    }

    /// The cable pulled.
    public func unplug() {
        lock.withLock { lostFlag = true }
        cont.finish()
    }

    public func close() {
        lock.withLock { closed = true }
        cont.finish()
    }
}

/// The unit's answers, frame by frame.
struct DemoReplies {
    let unit: DemoUnit

    static func ascii(_ s: String) -> [Int] { s.unicodeScalars.map { Int($0.value) & 0x7f } }
    static func name12(_ s: String) -> [Int] { ascii(s.padding(toLength: 12, withPad: " ", startingAt: 0)) }
    static func r14(_ v: Int) -> [Int] { [(v >> 7) & 0x7f, v & 0x7f] }
    static func r21(_ v: Int) -> [Int] { [(v >> 14) & 0x7f, (v >> 7) & 0x7f, v & 0x7f] }

    /// F0 7D 46 5A 01 cmd body chk F7 (FizzKit's frame rule: the checksum over cmd and body).
    static func frame(_ cmd: UInt8, _ body: [Int]) -> [UInt8] {
        let b = [Int(cmd)] + body
        let chk = (0x80 - (b.reduce(0, +) & 0x7f)) & 0x7f
        return [0xf0] + FizzSysEx.header + [FizzSysEx.protocolVersion] + b.map { UInt8($0 & 0x7f) } + [UInt8(chk), 0xf7]
    }

    /// A DUMP reply: FizzKit's STORE6 frame with the DUMP flag set (and its checksum made again).
    static func dump(_ p: DemoPreset, pool: Bool = false) -> [UInt8] {
        var f = (try? FizzSysEx.storeSound(name: p.name, blob: DemoUnit.blob(p), pool: pool, slot: p.slot, pan: 0,
                                           chorus: 20, delay: 12, reverb: 30, tags: .mask(p.tags))) ?? []
        guard f.count > 8 else { return f }
        f[6] |= UInt8(FizzSysEx.FLAGS.dump)
        let sum = f[5..<(f.count - 2)].reduce(0) { $0 + Int($1) }
        f[f.count - 2] = UInt8((0x80 - (sum & 0x7f)) & 0x7f)
        return f
    }

    func answer(_ b: [UInt8]) -> [[UInt8]] {
        if b == FM1Bulk.handshakeQuery {
            let t = Array(unit.idText.utf8.prefix(27))
            return [FM1Bulk.updateFrame(cmd: 0x11, payload: t + [UInt8](repeating: 0, count: 27 - t.count))]
        }
        if let tag = bulkTag(b) {
            guard unit.version != nil, tag == "FZBI" else { return [] }
            let data = Array("FZBI".utf8) + FM1Bulk.le32(0x97000) + FM1Bulk.le32(unit.storeSize) + FM1Bulk.le32(0x5eed_f122)
            return [FM1Bulk.updateFrame(cmd: FM1Bulk.bulkCmd, payload: data)]
        }
        guard b.count >= 8, b[1] == 0x7d, b[2] == 0x46, b[3] == 0x5a, let version = unit.version else { return [] }
        switch b[5] {
        case FizzSysEx.CMD.identity:
            return [Self.frame(FizzSysEx.REPLY.identity, [1] + Self.ascii(version))]
        case FizzSysEx.CMD.status:
            return [status()]
        case FizzSysEx.CMD.list:
            return list(flags: Int(b[6]))
        case FizzSysEx.CMD.dump:
            let src = Int(b[6]), slot = (Int(b[7]) << 7) | Int(b[8])
            let set = src == 0 ? unit.mine : src == 1 ? unit.factory : []
            if src == 2 {
                let ps = unit.pool.filter { slot == 0 || $0.place == slot }
                return ps.map { Self.dump(DemoPreset(slot: $0.place, id: $0.preset.id, name: $0.preset.name,
                                                    tags: $0.preset.tags, algorithm: $0.preset.algorithm), pool: true) }
            }
            if slot == 0, !set.isEmpty { return set.map { Self.dump($0) } }
            if let p = set.first(where: { $0.slot == slot }) { return [Self.dump(p)] }
            return [Self.frame(FizzSysEx.REPLY.error, [FizzSysEx.ST.notFound, Int(FizzSysEx.CMD.dump)])]
        case FizzSysEx.CMD.projects:
            var body = [unit.current - 1, unit.projects.count]
            for (k, p) in unit.projects.enumerated() {
                let flags = (p != nil ? 1 : 0) | (k + 1 == unit.current ? 2 : 0) | (p?.demo == true ? 4 : 0)
                body += [flags] + Self.name12(p?.name ?? "") + Self.r14(p?.pages ?? 0)
            }
            return [Self.frame(FizzSysEx.REPLY.projects, body)]
        default:
            return [Self.frame(FizzSysEx.REPLY.error, [FizzSysEx.ST.badCommand, Int(b[5])])]
        }
    }

    func status() -> [UInt8] {
        let written = 4711
        let body = [1, 6, 0, unit.current - 1, 4, unit.projects.compactMap { $0 }.count] + Self.r14(unit.mine.count) +
            Self.r14(unit.pool.count) + Self.r21(written) + Self.r14(unit.lib[0]) + Self.r14(unit.lib[1]) +
            Self.r14(unit.prj[0]) + Self.r14(unit.prj[1]) + [0, 2]
        return Self.frame(FizzSysEx.REPLY.status, body)
    }

    func list(flags: Int) -> [[UInt8]] {
        let pooled = unit.inPool
        func entry(_ src: Int, _ p: DemoPreset, slot: Int? = nil, inPool: Bool) -> [Int] {
            [src] + Self.r14(slot ?? p.slot) + Self.r21(p.id) + Self.name12(p.name) + [p.tags, inPool ? 2 : 0]
        }
        var all = unit.mine.map { entry(0, $0, inPool: pooled.contains($0.id)) }
        if flags & 1 != 0 { all += unit.factory.map { entry(1, $0, inPool: pooled.contains($0.id)) } }
        if flags & 2 != 0 { all += unit.pool.map { entry(2, $0.preset, slot: $0.place, inPool: true) } }
        let per = 12
        let parts = stride(from: 0, to: max(all.count, 1), by: per).map { Array(all[$0..<min($0 + per, all.count)]) }
        return parts.enumerated().map { k, p in
            Self.frame(FizzSysEx.REPLY.list, [k, k == parts.count - 1 ? 1 : 0] + Self.r14(all.count) + [p.count] +
                p.flatMap { $0 })
        }
    }

    /// The bulk channel's tag of a request, nil for anything else.
    func bulkTag(_ b: [UInt8]) -> String? {
        guard b.count > 4, b.first == 0xf0, b.last == 0xf7, b[1] != 0x7d else { return nil }
        let u = FM1Bulk.unpack7(b.dropFirst().dropLast())
        guard u.count >= 10, u[0] == 0, u[1] == 0x59, u[2] == FM1Bulk.bulkCmd else { return nil }
        return String(decoding: u[6..<10], as: UTF8.self)
    }
}

// MARK: - The scanner

/// The demo's ports: one per unit, each open a new transport on it.
public final class DemoScanner: MIDIPortScanning, @unchecked Sendable {
    public let units: [DemoUnit]
    private let lock = NSLock()
    private var opened: [DemoFizz] = []

    public init(units: [DemoUnit]) { self.units = units }

    public func candidates() -> [MIDIPortCandidate] {
        units.enumerated().map { k, u in
            let ref = UInt32(100 + 10 * k)
            return MIDIPortCandidate(source: MIDIEndpointInfo(ref: ref, name: u.portName, entity: ref, device: ref),
                                     destination: MIDIEndpointInfo(ref: ref + 1, name: u.portName, entity: ref, device: ref))
        }
    }

    public func open(_ c: MIDIPortCandidate) throws -> any MIDITransport {
        guard let k = candidates().firstIndex(of: c) else { throw MIDITransportError.lost }
        let t = DemoFizz(unit: units[k])
        lock.withLock { opened.append(t) }
        return t
    }

    /// Every port pulled out (the open ones go lost).
    public func unplugAll() {
        for t in lock.withLock({ opened }) { t.unplug() }
    }
}

// MARK: - Names

enum DemoNames {
    static let t = (bass: 1, keys: 2, pluck: 4, pad: 8, lead: 16, drums: 32, fx: 64)

    /// the factory's 23
    static let factory: [(String, Int, Int)] = [
        ("INIT", t.keys, 1), ("FM PIANO", t.keys, 2), ("BASS 1", t.bass, 3), ("CHIME", t.pluck, 4),
        ("DRUM KIT", t.drums, 5), ("STRING", t.pad, 6), ("BRASS", t.lead, 7), ("SPARKLE", t.fx, 8),
        ("E PIANO 2", t.keys, 6), ("ORGAN", t.keys, 8), ("MARIMBA", t.pluck, 2), ("SYNC LEAD", t.lead, 4),
        ("PAD 1", t.pad, 7), ("HARP", t.pluck, 1), ("BELL TREE", t.fx, 3), ("CLAV", t.keys, 5),
        ("SNARE", t.drums, 4), ("HAT", t.drums, 8), ("TOM", t.drums, 5), ("WOBBLE", t.bass, 7),
        ("SAW STACK", t.lead, 3), ("CHOIR", t.pad, 2), ("NOISE FX", t.fx, 1),
    ]

    /// MY PRESETS: the mock-ups' first fourteen, then made up from words in the tags' moods
    static let mine: [(String, Int, Int)] = {
        var out: [(String, Int, Int)] = [
            ("SUB DROP", t.bass, 1), ("GLASS BELL", t.pluck, 2), ("NIGHT PAD", t.pad, 3), ("WIRE LEAD", t.lead, 4),
            ("TIN KICK", t.drums, 5), ("DUSTY EP", t.keys, 6), ("BUBBLE FX", t.fx, 7), ("ROUND BASS", t.bass, 5),
            ("SODA PLUCK", t.pluck, 1), ("SLOW CLOUD", t.pad, 2), ("ACID LINE", t.bass, 3), ("ZAP SNARE", t.drums, 4),
            ("CHROME KEYS", t.keys, 6), ("VAPOR LEAD", t.lead, 7),
        ]
        let words: [(Int, [String], [String])] = [
            (t.bass, ["DEEP", "FAT", "DUB", "RUBBER", "LOW", "GRIT"], ["SUB", "BASS", "FLOOR", "KNOT", "THUMP"]),
            (t.keys, ["SOFT", "TINE", "WARM", "OLD", "VELVET"], ["EP", "KEYS", "ORGAN", "CLAV", "PIANO"]),
            (t.pluck, ["TINY", "GLASS", "DRY", "SNAP", "KOTO"], ["PLUCK", "HARP", "BELL", "PING", "MALLET"]),
            (t.pad, ["SLOW", "HAZE", "DAWN", "VAST", "FOG"], ["PAD", "CLOUD", "VEIL", "DRIFT", "SWELL"]),
            (t.lead, ["HOT", "NEON", "SYNC", "BRIGHT", "SOLO"], ["LEAD", "WIRE", "SAW", "HORN", "LINE"]),
            (t.drums, ["TIN", "DRY", "808", "CLAP", "ROOM"], ["KICK", "SNARE", "HAT", "TOM", "RIM"]),
            (t.fx, ["ZAP", "WARP", "ALIEN", "STATIC", "LASER"], ["FX", "RISE", "DROP", "BLIP", "SWEEP"]),
        ]
        var seen = Set(out.map(\.0))
        var k = 0
        while out.count < 143 {
            let (tag, a, b) = words[k % words.count]
            let name = k < 4000 ? "\(a[(k / 7) % a.count]) \(b[(k / 3 + k) % b.count])" : "SOUND \(out.count + 1)"
            let extra = (k % 11 == 5) ? (tag == t.keys ? t.pluck : 0) : 0
            if name.count <= 12, !seen.contains(name) {
                seen.insert(name)
                out.append((name, tag | extra, (k * 5 + 3) % 8 + 1))
            }
            k += 1
        }
        return out
    }()
}
