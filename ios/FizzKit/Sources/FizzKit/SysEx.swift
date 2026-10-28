// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// FIZZ's semantic SysEx (docs/SYSEX.md): F0 7D 46 5A 01 <cmd> <body> <chk> F7. A port of tools/fizz_sysex.mjs:
// the same bytes out of every builder, the same fields out of every parser (tests: vectors.json from the JS).

import Foundation

// MARK: - Arguments

/// LIST / DUMP / RENAME .. sources (the protocol's numbers).
public enum FizzSource: Int, Sendable, CaseIterable, Codable {
    case mine = 0, factory = 1, pool = 2, project = 3, track = 4

    public var name: String {
        switch self {
        case .mine: "mine"
        case .factory: "factory"
        case .pool: "pool"
        case .project: "project"
        case .track: "track"
        }
    }

    public init?(name: String) {
        guard let s = FizzSource.allCases.first(where: { $0.name == name }) else { return nil }
        self = s
    }
}

/// A track: 0..5, or the selected one (127 on the wire).
public enum FizzTrack: Sendable, Equatable, ExpressibleByIntegerLiteral {
    case selected
    case index(Int)

    public init(integerLiteral value: Int) { self = .index(value) }
    var wire: Int {
        switch self {
        case .selected: 127
        case .index(let t): t
        }
    }
}

/// FIZZ's tags: a mask, or names of `FizzSysEx.fizzTags`.
public enum FizzTags: Sendable, Equatable, ExpressibleByIntegerLiteral, ExpressibleByArrayLiteral {
    case mask(Int)
    case names([String])

    public init(integerLiteral value: Int) { self = .mask(value) }
    public init(arrayLiteral elements: String...) { self = .names(elements) }

    public var mask: Int {
        switch self {
        case .mask(let m): return m & 0x7f
        case .names(let names):
            return names.reduce(0) { m, t in
                if let k = FizzSysEx.fizzTags.firstIndex(of: t.uppercased()) { return m | (1 << k) }
                return m
            }
        }
    }
}

/// A track DUMP's extras given back with an AUDITION.
public struct FizzTrackExtrasIn: Sendable, Equatable {
    public var id: Int, sound: Int, edited: Bool
    public init(id: Int, sound: Int, edited: Bool) { self.id = id; self.sound = sound; self.edited = edited }
}

public enum FizzProjectOp: Int, Sendable { case load = 1, new = 2, copy = 3, delete = 4 }

public enum FizzSysExError: Error, Equatable {
    case badBlob, badSlot, badSource
}

// MARK: - Replies

public struct FizzIdentityReply: Codable, Sendable, Equatable {
    public let `protocol`: Int?
    public let version: String
}

public struct FizzStoreReply: Codable, Sendable, Equatable {
    public let status: Int?
    public let statusName: String?
    public let pool: Bool
    public let slot: Int
}

public struct FizzErrorReply: Codable, Sendable, Equatable {
    public let status: Int?
    public let statusName: String?
    public let command: Int?
}

public struct FizzLabels: Codable, Sendable, Equatable {
    public let count: Int
    public let list: [String]
    public let knobs: [String: String]
    public let buttons: [String: String]
    public let white: [String]
    public let black: [String]
}

public struct FizzListEntry: Codable, Sendable, Equatable {
    public let source: String
    public let slot: Int
    public let id: Int
    public let name: String
    public let tags: Int
    public let imported: Bool
    public let inPool: Bool
}

public struct FizzListPart: Codable, Sendable, Equatable {
    public let part: Int
    public let last: Bool
    public let total: Int
    public let entries: [FizzListEntry]
}

public struct FizzDumpExtras: Codable, Sendable, Equatable {
    public let id: Int, sound: Int
    public let edited: Bool, selected: Bool, rec: Bool, playing: Bool, browse: Bool
    public let chan: Int, sel: Int
}

public struct FizzDump: Codable, Sendable, Equatable {
    public let source: String
    public let slot: Int
    public let name: String
    public let drive: Int, pan: Int, chorus: Int, delay: Int, reverb: Int, tags: Int
    public let blob: [UInt8]
    public let six: Bool
    public let frame: [UInt8]
    public let track: Int?
    public let extras: FizzDumpExtras?
}

public struct FizzAck: Codable, Sendable, Equatable {
    public let command: Int
    public let status: Int
    public let statusName: String?
    public let a: Int
    public let b: Int
}

public struct FizzStatus: Codable, Sendable, Equatable {
    public let `protocol`: Int
    public let tracks: Int
    public let playing: Bool, noStore: Bool, dirty: Bool, job: Bool, demo: Bool, rec: Bool, browse: Bool
    public let project: Int, pattern: Int, projects: Int
    public let mine: Int, pool: Int, written: Int
    public let lib: [Int], prj: [Int]
    public let jobCommand: Int
    public let sel: Int?
    /// The package: "usb", or "ble" (the Bluetooth one appends its byte, its projects and its MY PRESETS' places)
    public let package: String
    public let nproj: Int, nlib: Int
}

public struct FizzUserProject: Codable, Sendable, Equatable {
    public let project: Int
    public let steps: Int
}

public struct FizzUsers: Codable, Sendable, Equatable {
    public let source: String
    public let slot: Int
    public let projects: [FizzUserProject]
    public let tracks: Int
}

public struct FizzProject: Codable, Sendable, Equatable {
    public let slot: Int
    public let there: Bool, current: Bool, demo: Bool
    public let name: String
    public let pages: Int
}

public struct FizzProjects: Codable, Sendable, Equatable {
    public let current: Int
    public let list: [FizzProject]
}

/// parseReply's result. A payload is nil where the JS spreads a null (a reply of that kind too short to read).
public enum FizzReply: Sendable, Equatable, Encodable {
    case identity(FizzIdentityReply)
    case store(FizzStoreReply)
    case error(FizzErrorReply)
    case labels(FizzLabels?)
    case list(FizzListPart?)
    case dump(FizzDump?)
    case ack(FizzAck?)
    case status(FizzStatus?)
    case users(FizzUsers?)
    case projects(FizzProjects?)

    public var kind: String {
        switch self {
        case .identity: "identity"
        case .store: "store"
        case .error: "error"
        case .labels: "labels"
        case .list: "list"
        case .dump: "dump"
        case .ack: "ack"
        case .status: "status"
        case .users: "users"
        case .projects: "projects"
        }
    }

    private enum Key: String, CodingKey { case kind }

    // {kind, ...payload}, as the JS spreads it
    public func encode(to encoder: Encoder) throws {
        var c = encoder.container(keyedBy: Key.self)
        try c.encode(kind, forKey: .kind)
        switch self {
        case .identity(let p): try p.encode(to: encoder)
        case .store(let p): try p.encode(to: encoder)
        case .error(let p): try p.encode(to: encoder)
        case .labels(let p): try p?.encode(to: encoder)
        case .list(let p): try p?.encode(to: encoder)
        case .dump(let p): try p?.encode(to: encoder)
        case .ack(let p): try p?.encode(to: encoder)
        case .status(let p): try p?.encode(to: encoder)
        case .users(let p): try p?.encode(to: encoder)
        case .projects(let p): try p?.encode(to: encoder)
        }
    }
}

// MARK: - The protocol

public enum FizzSysEx {
    public static let header: [UInt8] = [0x7d, 0x46, 0x5a]
    public static let protocolVersion: UInt8 = 1

    public enum CMD {
        public static let identity: UInt8 = 0x01, store: UInt8 = 0x10, store6: UInt8 = 0x11, labels: UInt8 = 0x20
        public static let list: UInt8 = 0x21, dump: UInt8 = 0x22, param: UInt8 = 0x30, browse: UInt8 = 0x31
        // Phase 1
        public static let status: UInt8 = 0x02, flush: UInt8 = 0x03, tags: UInt8 = 0x12, rename: UInt8 = 0x13
        public static let delete: UInt8 = 0x14, swap: UInt8 = 0x15, poolRef: UInt8 = 0x16, users: UInt8 = 0x17
        public static let projects: UInt8 = 0x23, project: UInt8 = 0x24
    }

    public enum REPLY {
        public static let identity: UInt8 = 0x41, store: UInt8 = 0x50, labels: UInt8 = 0x60, list: UInt8 = 0x61
        public static let error: UInt8 = 0x7f
        // Phase 1
        public static let status: UInt8 = 0x42, ack: UInt8 = 0x52, users: UInt8 = 0x57, projects: UInt8 = 0x63
    }

    public enum FLAGS {
        public static let pool = 0x01, batch = 0x02, load = 0x04, dump = 0x08, audition = 0x10, own = 0x20
    }

    public enum MIX {
        public static let pan = 0x01, chorus = 0x02, delay = 0x04, reverb = 0x08
    }

    public enum ST {
        public static let ok = 0, full = 1, badLength = 2, badChecksum = 3, badPatch = 4, badSlot = 5
        public static let badVersion = 6, badCommand = 7, busy = 8, notFound = 9, inUse = 10, badTrack = 11
    }

    public static let sources = ["mine", "factory", "pool"]
    public static let sourceProject = 3
    public static let sourceTrack = 4
    public static let xtraLen = 8
    public static let labelKnobs = ["SELECT", "ALGORITHM", "PRESETS", "K1", "K2", "K3", "K4", "MASTER"]
    public static let labelButtons = ["FX", "TRACKS", "MIX", "LATCH", "EDIT", "SHIFT", "HOME", "SOUNDS", "FILL", "PATTERNS",
                                      "PLAY", "REC", "F1", "F2"]
    public static let labelsN = labelKnobs.count + labelButtons.count + 16 + 11   // 49
    public static let statusNames = ["OK", "FULL", "BAD LENGTH", "BAD CHECKSUM", "BAD PATCH", "BAD SLOT", "BAD VERSION",
                                     "BAD COMMAND", "BUSY", "NOT FOUND", "IN USE", "BAD TRACK"]
    public static let projectOps = ["", "load", "new", "copy", "delete"]
    public static let nameLen = 12, blobLen = 96, blob6Len = 128
    public static let fizzTags = ["BASS", "KEYS", "PLUCK", "PAD", "LEAD", "DRUMS", "FX"]

    public static func statusName(_ s: Int?) -> String? {
        guard let s, s >= 0, s < statusNames.count else { return nil }
        return statusNames[s]
    }

    public static func fizzTagMask(_ tags: FizzTags?) -> Int { tags?.mask ?? 0 }

    // MARK: 7-bit packing: groups of 7 bytes -> 8, the first holding their top bits

    public static func pack7(_ bytes: [UInt8]) -> [UInt8] {
        var out: [UInt8] = []
        out.reserveCapacity(bytes.count + (bytes.count + 6) / 7)
        var i = 0
        while i < bytes.count {
            let g = bytes[i..<min(i + 7, bytes.count)]
            var hi: UInt8 = 0
            for (j, b) in g.enumerated() { hi |= ((b >> 7) & 1) << j }
            out.append(hi)
            for b in g { out.append(b & 0x7f) }
            i += 7
        }
        return out
    }

    public static func unpack7(_ bytes: some Collection<UInt8>) -> [UInt8] {
        let b = Array(bytes)
        var out: [UInt8] = []
        var i = 0
        while i < b.count {
            let hi = b[i]; i += 1
            var j = 0
            while j < 7 && i < b.count {
                out.append(b[i] | (((hi >> j) & 1) << 7))
                i += 1; j += 1
            }
        }
        return out
    }

    // MARK: frames

    static func checksum(_ v: [Int]) -> Int { (0x80 - (v.reduce(0, +) & 0x7f)) & 0x7f }

    static func frame(_ cmd: UInt8, _ body: [Int]) -> [UInt8] {
        let b = [Int(cmd)] + body
        var out: [UInt8] = [0xf0] + header + [protocolVersion]
        out += b.map { UInt8(truncatingIfNeeded: $0) }
        out.append(UInt8(checksum(b)))
        out.append(0xf7)
        return out
    }

    static func nameBytes(_ name: String) -> [Int] {
        let u = Array(name.utf16)
        return (0..<nameLen).map { i in
            let c = i < u.count ? Int(u[i]) : 0x20
            return c >= 0x20 && c <= 0x7e ? c : 0x3f
        }
    }

    static func u14(_ v: Int) -> [Int] { [(v >> 7) & 0x7f, v & 0x7f] }
    static func u21(_ v: Int) -> [Int] { [(v >> 14) & 0x7f, (v >> 7) & 0x7f, v & 0x7f] }
    static func c7(_ v: Int?) -> Int { max(0, min(127, v ?? 0)) }

    static func srcOf(_ s: FizzSource) throws -> Int {
        if s == .track { throw FizzSysExError.badSource }
        return s.rawValue
    }

    // MARK: builders

    public static func identity() -> [UInt8] { frame(CMD.identity, []) }

    /// STORE (a 96-byte blob) or STORE6 (128 bytes). Slot 0: the first free.
    public static func storeSound(name: String = "", blob: [UInt8], drive: Int = 0, pool: Bool = false, slot: Int = 0,
                                  batch: Bool = false, track: Int? = nil, pan: Int? = nil, chorus: Int? = nil,
                                  delay: Int? = nil, reverb: Int? = nil, tags: FizzTags? = nil, audition: Bool = false,
                                  own: Bool = false, extras: FizzTrackExtrasIn? = nil) throws -> [UInt8] {
        guard blob.count == blobLen || blob.count == blob6Len else { throw FizzSysExError.badBlob }
        guard slot >= 0 && slot <= (pool ? 128 : 256) else { throw FizzSysExError.badSlot }
        let load = track.map { $0 >= 0 && $0 <= 7 } ?? false
        let mask = (pan != nil ? MIX.pan : 0) | (chorus != nil ? MIX.chorus : 0) | (delay != nil ? MIX.delay : 0) |
            (reverb != nil ? MIX.reverb : 0)
        let tm = fizzTagMask(tags)
        let flags = (pool ? FLAGS.pool : 0) | (batch ? FLAGS.batch : 0) | (load ? FLAGS.load : 0) |
            (audition && load ? FLAGS.audition : 0) | (own ? FLAGS.own : 0)
        let xt: [Int]? = audition && load ? extras.map(extrasBytes) : nil
        var body = [flags, (slot >> 7) & 0x7f, slot & 0x7f] + nameBytes(name)
        body += [c7(drive), load ? track! : 0, c7((pan ?? 0) + 64), c7(chorus), c7(delay), c7(reverb), mask]
        body += pack7(blob).map(Int.init)
        if tm != 0 || xt != nil { body.append(tm) }
        body += xt ?? []
        return frame(blob.count == blob6Len ? CMD.store6 : CMD.store, body)
    }

    static func extrasBytes(_ x: FizzTrackExtrasIn) -> [Int] {
        u21(x.id) + u14(x.sound) + [x.edited ? 1 : 0, 0, 0]
    }

    /// PARAM: one value of a track's sound (no reply but with ack).
    public static func paramRequest(track: FizzTrack = .selected, id: Int, value: Int, ack: Bool = false) -> [UInt8] {
        let v = Int(UInt32(truncatingIfNeeded: value + (1 << 20)))
        return frame(CMD.param, [track.wire & 0x7f, id & 0x7f] + u21(v) + [ack ? 1 : 0])
    }

    /// BROWSE: the unit's sound browser on a track at preset id.
    public static func browseRequest(track: FizzTrack = .selected, id: Int) -> [UInt8] {
        frame(CMD.browse, [track.wire & 0x7f] + u21(id))
    }

    public static func labelsRequest(subscribe: Bool = false) -> [UInt8] { frame(CMD.labels, [subscribe ? 1 : 0]) }

    public static func listRequest(factory: Bool = false, pool: Bool = false) -> [UInt8] {
        frame(CMD.list, [(factory ? 1 : 0) | (pool ? 2 : 0)])
    }

    /// DUMP: one preset, or slot 0: all of that source. Source mine, factory, pool or track.
    public static func dumpRequest(source: FizzSource = .mine, slot: Int = 0) throws -> [UInt8] {
        if source == .project { throw FizzSysExError.badSource }
        return frame(CMD.dump, [source.rawValue] + u14(slot))
    }

    public static func statusRequest() -> [UInt8] { frame(CMD.status, []) }
    public static func flushRequest() -> [UInt8] { frame(CMD.flush, []) }
    public static func projectsRequest() -> [UInt8] { frame(CMD.projects, []) }

    public static func tagsRequest(source: FizzSource = .mine, slot: Int, tags: FizzTags? = nil) throws -> [UInt8] {
        frame(CMD.tags, [try srcOf(source)] + u14(slot) + [fizzTagMask(tags)])
    }

    public static func renameRequest(source: FizzSource = .mine, slot: Int, name: String = "") throws -> [UInt8] {
        frame(CMD.rename, [try srcOf(source)] + u14(slot) + nameBytes(name))
    }

    public static func deleteRequest(source: FizzSource = .mine, slot: Int, confirm: Bool = false) throws -> [UInt8] {
        frame(CMD.delete, [try srcOf(source)] + u14(slot) + [confirm ? 1 : 0])
    }

    public static func swapRequest(source: FizzSource = .mine, a: Int, b: Int) throws -> [UInt8] {
        frame(CMD.swap, [try srcOf(source)] + u14(a) + u14(b))
    }

    public static func poolRefRequest(slot: Int = 0, id: Int = 0) -> [UInt8] {
        frame(CMD.poolRef, u14(slot) + u21(id))
    }

    public static func usersRequest(source: FizzSource = .mine, slot: Int) throws -> [UInt8] {
        frame(CMD.users, [try srcOf(source)] + u14(slot))
    }

    public static func projectRequest(op: FizzProjectOp, p: Int, q: Int = 0) -> [UInt8] {
        frame(CMD.project, [op.rawValue, p, q])
    }

    /// A track's DUMP as an AUDITION back onto that track.
    public static func trackFrame(_ bytes: [UInt8]) -> [UInt8] {
        var b = bytes
        guard b.count >= 9 else { return b }
        b[6] = (b[6] & ~UInt8(FLAGS.dump) & 0x7f) | UInt8(FLAGS.audition) | UInt8(FLAGS.load)
        b[b.count - 2] = UInt8(checksum(b[5..<(b.count - 2)].map(Int.init)))
        return b
    }

    /// A dump as a STORE / STORE6 to send back (DUMP flag cleared; slot: elsewhere, 0 the first free).
    public static func restoreFrame(_ bytes: [UInt8], slot: Int? = nil) -> [UInt8] {
        var b = bytes
        guard b.count >= 9 else { return b }
        b[6] &= ~UInt8(FLAGS.dump) & 0x7f
        if let slot {
            b[7] = UInt8((slot >> 7) & 0x7f)
            b[8] = UInt8(slot & 0x7f)
        }
        b[b.count - 2] = UInt8(checksum(b[5..<(b.count - 2)].map(Int.init)))
        return b
    }

    /// js/digitone.js storeFrame: a sound imported or added (IMPORT's SEND, BACKUP's ADD): STORE / STORE6 into the
    /// first free place (pool: the pool too), its pan and sends always with it, LOAD onto `track` when it is 0..5.
    public static func storeFrame(name: String, blob: [UInt8], drive: Int = 0, pan: Int = 0, chorus: Int = 0,
                                  delay: Int = 0, reverb: Int = 0, fizzTags: [String] = [], pool: Bool = false,
                                  batch: Bool = false, track: Int = -1) throws -> [UInt8] {
        let load = track >= 0 && track <= 5
        return try storeSound(name: name, blob: blob, drive: drive, pool: pool, slot: 0, batch: batch,
                              track: load ? track : nil, pan: pan, chorus: chorus, delay: delay, reverb: reverb,
                              tags: .names(fizzTags))
    }

    /// js/library.js sameSound: the same name and the same patch (a version-5 blob's byte 87 not looked at).
    public static func sameSound(name a: String, blob ab: [UInt8], name b: String, blob bb: [UInt8]) -> Bool {
        guard a == b, ab.count == bb.count else { return false }
        for i in ab.indices where ab[i] != bb[i] && !(i == 87 && ab.count == blobLen) { return false }
        return true
    }

    /// A MY preset's new name as manager.js takes it from the name field: upper case, printable ASCII, trimmed,
    /// twelve at most.
    public static func presetName(_ s: String) -> String {
        let v = String(String.UnicodeScalarView(s.uppercased().unicodeScalars.filter { $0.value >= 0x20 && $0.value <= 0x7e }))
        return String(jsTrim(v).prefix(nameLen))
    }

    /// A project's name as projects.js cleanName makes it (FIZZ's own letters, twelve at most, trimmed).
    public static func cleanName(_ s: String) -> String {
        let ok = Set("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-.,'&!?#+/()*:_~ ".unicodeScalars)
        let v = String(String.UnicodeScalarView(s.uppercased().unicodeScalars.filter { ok.contains($0) }))
        return jsTrim(String(v.prefix(nameLen)))
    }

    /// The frame as USB-MIDI event packets (cable 0, CIN 4 / 5 / 6 / 7).
    public static func usbPackets(_ f: [UInt8]) -> [UInt32] {
        var out: [UInt32] = []
        var i = 0
        while i < f.count {
            let k = min(3, f.count - i)
            let end = i + k >= f.count && f[f.count - 1] == 0xf7
            let cin = UInt32(end ? 4 + k : 4)
            let b1 = UInt32(k > 1 ? f[i + 1] : 0), b2 = UInt32(k > 2 ? f[i + 2] : 0)
            out.append(cin | (UInt32(f[i]) << 8) | (b1 << 16) | (b2 << 24))
            i += 3
        }
        return out
    }

    // MARK: parsers

    static func at(_ b: [UInt8], _ i: Int) -> Int? { i >= 0 && i < b.count ? Int(b[i]) : nil }
    static func slice(_ b: [UInt8], _ from: Int, _ to: Int) -> ArraySlice<UInt8> {
        let lo = min(max(from, 0), b.count), hi = min(max(to, lo), b.count)
        return b[lo..<hi]
    }
    static func r14(_ b: [UInt8], _ o: Int) -> Int { ((at(b, o) ?? 0) << 7) | (at(b, o + 1) ?? 0) }
    static func sumOk(_ b: [UInt8]) -> Bool { b[5..<(b.count - 1)].reduce(0) { $0 + Int($1) } & 0x7f == 0 }
    static func isFizz(_ b: [UInt8]) -> Bool {
        b.count >= 8 && b[0] == 0xf0 && b[b.count - 1] == 0xf7 && b[1] == 0x7d && b[2] == 0x46 && b[3] == 0x5a
    }
    static func body1(_ b: [UInt8], _ kind: UInt8) -> Bool { isFizz(b) && b[5] == kind && sumOk(b) }

    public static func parseReply(_ b: [UInt8]) -> FizzReply? {
        guard isFizz(b), sumOk(b) else { return nil }
        let kind = b[5]
        let rest = Array(b[6..<(b.count - 2)])
        let r = { (i: Int) -> Int? in at(rest, i) }
        switch kind {
        case REPLY.identity:
            return .identity(FizzIdentityReply(protocol: r(0), version: latin1(slice(rest, 1, rest.count))))
        case REPLY.store:
            return .store(FizzStoreReply(status: r(0), statusName: statusName(r(0)), pool: r(1) == 1,
                                         slot: ((r(2) ?? 0) << 7) | (r(3) ?? 0)))
        case REPLY.error:
            return .error(FizzErrorReply(status: r(0), statusName: statusName(r(0)), command: r(1)))
        case REPLY.labels: return .labels(parseLabels(b))
        case REPLY.list: return .list(parseList(b))
        case CMD.store, CMD.store6:
            return (r(0) ?? 0) & FLAGS.dump != 0 ? .dump(parseDump(b)) : nil
        case REPLY.ack: return .ack(parseAck(b))
        case REPLY.status: return .status(parseStatus(b))
        case REPLY.users: return .users(parseUsers(b))
        case REPLY.projects: return .projects(parseProjects(b))
        default: return nil
        }
    }

    public static func parseLabels(_ b: [UInt8]) -> FizzLabels? {
        guard b.count >= 10, b[0] == 0xf0, b[b.count - 1] == 0xf7, b[1] == 0x7d, b[2] == 0x46, b[3] == 0x5a,
              b[5] == REPLY.labels, sumOk(b) else { return nil }
        let count = (Int(b[6]) << 7) | Int(b[7]), n = Int(b[8])
        var list: [String] = []
        var o = 9, i = 0
        while i < n && o < b.count - 2 {
            let len = Int(b[o]); o += 1
            list.append(latin1(slice(b, o, o + len)))
            o += len; i += 1
        }
        guard list.count == n else { return nil }
        let atL = { (k: Int) -> String in k < list.count ? list[k] : "" }
        let nk = labelKnobs.count, nb = labelButtons.count
        var knobs: [String: String] = [:], buttons: [String: String] = [:]
        for (i, k) in labelKnobs.enumerated() { knobs[k] = atL(i) }
        for (i, k) in labelButtons.enumerated() { buttons[k] = atL(nk + i) }
        return FizzLabels(count: count, list: list, knobs: knobs, buttons: buttons,
                          white: (0..<16).map { atL(nk + nb + $0) }, black: (0..<11).map { atL(nk + nb + 16 + $0) })
    }

    public static func parseList(_ b: [UInt8]) -> FizzListPart? {
        guard isFizz(b), b[5] == REPLY.list, sumOk(b) else { return nil }
        var entries: [FizzListEntry] = []
        let n = at(b, 10) ?? -1
        var e = 0, o = 11
        while e < n && o + 20 <= b.count - 2 {
            let src = Int(b[o])
            entries.append(FizzListEntry(
                source: src < sources.count ? sources[src] : String(src),
                slot: (Int(b[o + 1]) << 7) | Int(b[o + 2]),
                id: (Int(b[o + 3]) << 14) | (Int(b[o + 4]) << 7) | Int(b[o + 5]),
                name: jsTrimEnd(latin1(b[(o + 6)..<(o + 18)])),
                tags: Int(b[o + 18]), imported: b[o + 19] & 1 != 0, inPool: b[o + 19] & 2 != 0))
            e += 1; o += 20
        }
        return FizzListPart(part: Int(b[6]), last: b[7] != 0, total: r14(b, 8), entries: entries)
    }

    /// A DUMP reply (a STORE / STORE6 frame with the DUMP flag). nil also when too short for the fixed fields (the JS
    /// would give undefined ones).
    public static func parseDump(_ b: [UInt8]) -> FizzDump? {
        guard isFizz(b), b[5] == CMD.store || b[5] == CMD.store6, sumOk(b), Int(b[6]) & FLAGS.dump != 0 else { return nil }
        let six = b[5] == CMD.store6, n7 = six ? 147 : 110
        let body = Array(b[6..<(b.count - 2)])
        guard body.count >= 21 else { return nil }
        let blob = unpack7(slice(body, 22, 22 + n7))
        let x: [UInt8]? = body.count >= 22 + n7 + 1 + xtraLen ? Array(body[(22 + n7 + 1)...]) : nil
        let load = Int(body[0]) & FLAGS.load != 0
        let extras = x.map { x in
            FizzDumpExtras(id: (Int(x[0]) << 14) | (Int(x[1]) << 7) | Int(x[2]), sound: (Int(x[3]) << 7) | Int(x[4]),
                           edited: x[5] & 1 != 0, selected: x[5] & 2 != 0, rec: x[5] & 4 != 0, playing: x[5] & 8 != 0,
                           browse: x[5] & 16 != 0, chan: Int(x[6]), sel: Int(x[7]))
        }
        return FizzDump(
            source: load ? "track" : Int(body[0]) & FLAGS.pool != 0 ? "pool" : "mine",
            slot: load ? Int(body[16]) + 1 : (Int(body[1]) << 7) | Int(body[2]),
            name: jsTrimEnd(latin1(body[3..<15])), drive: Int(body[15]), pan: Int(body[17]) - 64,
            chorus: Int(body[18]), delay: Int(body[19]), reverb: Int(body[20]),
            tags: body.count > 22 + n7 ? Int(body[22 + n7]) : 0, blob: blob, six: six, frame: b,
            track: load ? Int(body[16]) : nil, extras: extras)
    }

    public static func parseAck(_ b: [UInt8]) -> FizzAck? {
        guard body1(b, REPLY.ack), b.count >= 14 else { return nil }
        return FizzAck(command: Int(b[6]), status: Int(b[7]), statusName: statusName(Int(b[7])), a: r14(b, 8),
                       b: r14(b, 10))
    }

    public static func parseStatus(_ b: [UInt8]) -> FizzStatus? {
        guard body1(b, REPLY.status), b.count >= 30 else { return nil }
        let f = b[8], ble = b.count >= 35 && b[29] & 1 != 0
        return FizzStatus(
            protocol: Int(b[6]), tracks: Int(b[7]), playing: f & 1 != 0, noStore: f & 2 != 0, dirty: f & 4 != 0,
            job: f & 8 != 0, demo: f & 16 != 0, rec: f & 32 != 0, browse: f & 64 != 0, project: Int(b[9]) + 1,
            pattern: Int(b[10]), projects: Int(b[11]), mine: r14(b, 12), pool: r14(b, 14),
            written: (Int(b[16]) << 14) | r14(b, 17), lib: [r14(b, 19), r14(b, 21)], prj: [r14(b, 23), r14(b, 25)],
            jobCommand: Int(b[27]), sel: b.count >= 31 ? Int(b[28]) : nil,
            package: ble ? "ble" : "usb", nproj: ble ? Int(b[30]) : 8, nlib: ble ? r14(b, 31) : 256)
    }

    public static func parseUsers(_ b: [UInt8]) -> FizzUsers? {
        guard body1(b, REPLY.users), b.count >= 12 else { return nil }
        let n = Int(b[9])
        var projects: [FizzUserProject] = []
        var i = 0, o = 10
        while i < n && o + 3 <= b.count - 2 {
            projects.append(FizzUserProject(project: Int(b[o]) + 1, steps: r14(b, o + 1)))
            i += 1; o += 3
        }
        let src = Int(b[6])
        return FizzUsers(source: src < sources.count ? sources[src] : String(src), slot: r14(b, 7), projects: projects,
                         tracks: at(b, 10 + 3 * n) ?? 0)
    }

    public static func parseProjects(_ b: [UInt8]) -> FizzProjects? {
        guard body1(b, REPLY.projects), b.count >= 10 else { return nil }
        var list: [FizzProject] = []
        var p = 0, o = 8
        while p < Int(b[7]) && o + 15 <= b.count - 2 {
            list.append(FizzProject(slot: p + 1, there: b[o] & 1 != 0, current: b[o] & 2 != 0, demo: b[o] & 4 != 0,
                                    name: jsTrimEnd(latin1(b[(o + 1)..<(o + 13)])), pages: r14(b, o + 13)))
            p += 1; o += 15
        }
        return FizzProjects(current: Int(b[6]) + 1, list: list)
    }
}

// MARK: - JS string helpers

/// String.fromCharCode over bytes: each byte one code point (Latin-1).
func latin1(_ bytes: some Sequence<UInt8>) -> String {
    var s = String.UnicodeScalarView()
    for b in bytes { s.append(Unicode.Scalar(b)) }
    return String(s)
}

/// JS String.prototype.trimEnd: its whitespace and line terminators.
func jsTrimEnd(_ s: String) -> String {
    var u = Array(s.unicodeScalars)
    while let last = u.last, isJSSpace(last.value) { u.removeLast() }
    var v = String.UnicodeScalarView()
    v.append(contentsOf: u)
    return String(v)
}

/// JS String.prototype.trim.
func jsTrim(_ s: String) -> String {
    var u = Array(s.unicodeScalars)
    while let last = u.last, isJSSpace(last.value) { u.removeLast() }
    while let first = u.first, isJSSpace(first.value) { u.removeFirst() }
    var v = String.UnicodeScalarView()
    v.append(contentsOf: u)
    return String(v)
}

func isJSSpace(_ c: UInt32) -> Bool {
    switch c {
    case 0x09...0x0d, 0x20, 0xa0, 0x1680, 0x2000...0x200a, 0x2028, 0x2029, 0x202f, 0x205f, 0x3000, 0xfeff: true
    default: false
    }
}
