// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// FizzData's typed values: what the bundled JS (site/manager/fizzbak.js, fizzstore.js, site/js/library.js) gives and
// takes, in Swift. Field names follow the JS; manager.js's own rules (the BACKUP list's "old") are noted where kept.

import Foundation
@preconcurrency import JavaScriptCore

/// A JavaScript exception (its message and `code`, as the page shows them), or a bridge failure.
public enum FizzDataError: Error, Sendable, Equatable, LocalizedError, CustomStringConvertible {
    case js(message: String, code: String?)
    case bridge(String)
    /// A promise that had not settled when the call returned (the engine has no timers, no event loop).
    case unsettled(String)

    public var message: String {
        switch self {
        case .js(let m, _): m
        case .bridge(let m): m
        case .unsettled(let f): "\(f): the JS promise did not settle"
        }
    }
    public var errorDescription: String? { message }
    public var description: String { message }

    static func from(_ v: JSValueRef, _ ctx: JSContextRef) -> FizzDataError {
        guard JSValueIsObject(ctx, v), let o = JSValueToObject(ctx, v, nil) else { return .js(message: JSBridge.toString(v, ctx), code: nil) }
        let m = JSBridge.property(o, "message", ctx).flatMap { JSValueIsUndefined(ctx, $0) ? nil : JSBridge.toString($0, ctx) }
        let c = JSBridge.property(o, "code", ctx).flatMap { JSValueIsString(ctx, $0) ? JSBridge.toString($0, ctx) : nil }
        return .js(message: m ?? JSBridge.toString(v, ctx), code: c)
    }
}

public enum FizzBackupKind: String, Sendable, Equatable { case full, presets }

public struct FizzSends: Sendable, Equatable, Hashable {
    public var chorus: Int, delay: Int, reverb: Int
    public init(chorus: Int = 0, delay: Int = 0, reverb: Int = 0) { self.chorus = chorus; self.delay = delay; self.reverb = reverb }
    init(_ v: FizzJSValue?) { self.init(chorus: v?["chorus"]?.int ?? 0, delay: v?["delay"]?.int ?? 0, reverb: v?["reverb"]?.int ?? 0) }
    var js: FizzJSValue { ["chorus": .int(chorus), "delay": .int(delay), "reverb": .int(reverb)] }
}

/// A preset in js/library.js's shape (a .fizzsnd, a backup's preset, a DUMP read back).
public struct FizzSound: Sendable, Equatable {
    public var name: String
    /// FM TONE's patch: 128 bytes ('Q' 6) or 96 ('Q' 5).
    public var blob: [UInt8]
    public var drive: Int
    public var pan: Int
    public var sends: FizzSends
    /// FIZZ's tags (FIZZ_TAGS names).
    public var tags: [String]
    public var favourite: Bool
    /// Where it came from ({fizz, from, exported, ..}, as the file had it).
    public var source: FizzJSValue
    /// The file it was read from (a .fizzsnd's label), if any.
    public var file: String?

    public init(name: String, blob: [UInt8], drive: Int = 0, pan: Int = 0, sends: FizzSends = .init(), tags: [String] = [],
                favourite: Bool = false, source: FizzJSValue = [:], file: String? = nil) {
        self.name = name; self.blob = blob; self.drive = drive; self.pan = pan; self.sends = sends; self.tags = tags
        self.favourite = favourite; self.source = source; self.file = file
    }
    public var blobVersion: Int { blob.count == 128 ? 6 : 5 }

    init(js v: FizzJSValue) {
        self.init(name: v["name"]?.string ?? "", blob: v["blob"]?.bytes ?? [], drive: v["drive"]?.int ?? 0, pan: v["pan"]?.int ?? 0,
                  sends: FizzSends(v["sends"]), tags: v["fizzTags"]?.array?.compactMap(\.string) ?? [],
                  favourite: v["favourite"]?.bool ?? false, source: v["source"]?.nonNull ?? [:], file: v["file"]?.string)
    }
    /// js/library.js's shape.
    var js: FizzJSValue {
        var m: [FizzJSMember] = [
            .init(key: "fizz", value: true), .init(key: "name", value: .string(name)), .init(key: "blob", value: .bytes(blob)),
            .init(key: "drive", value: .int(drive)), .init(key: "pan", value: .int(pan)), .init(key: "sends", value: sends.js),
            .init(key: "fizzTags", value: .array(tags.map { .string($0) })), .init(key: "favourite", value: .bool(favourite)),
            .init(key: "source", value: source), .init(key: "notes", value: []), .init(key: "origin", value: []),
            .init(key: "mark", value: ""), .init(key: "tags", value: []),
        ]
        if let file { m.insert(.init(key: "file", value: .string(file)), at: 0) }
        return .object(m)
    }
}

/// The store's region on the unit (FZBI: its address and size).
public struct FizzStoreRegion: Sendable, Equatable {
    public var lo: Int, size: Int
    public init(lo: Int, size: Int) { self.lo = lo; self.size = size }
}

/// A backup read's stats (fizzlink.js backup().stats; FizzKit's FizzBackupStats field for field): the manifest's
/// "transfer" is made from them in JS as manager.js does (entry.js transferOf: ms rounded, kbps to 0.1, ..).
public struct FizzTransferStats: Sendable, Equatable {
    public var bytes: Int, ms: Double, kbps: Double, frames: Int, resent: Int, depth: Int, gapMs: Double, perFrame: Double
    public var rttAvg: Double, rttMedian: Double, rttP95: Double, rttMax: Double
    public init(bytes: Int, ms: Double, kbps: Double, frames: Int, resent: Int, depth: Int, gapMs: Double, perFrame: Double,
                rttAvg: Double = 0, rttMedian: Double, rttP95: Double, rttMax: Double) {
        self.bytes = bytes; self.ms = ms; self.kbps = kbps; self.frames = frames; self.resent = resent; self.depth = depth
        self.gapMs = gapMs; self.perFrame = perFrame; self.rttAvg = rttAvg; self.rttMedian = rttMedian; self.rttP95 = rttP95; self.rttMax = rttMax
    }
    var js: FizzJSValue {
        ["bytes": .int(bytes), "ms": .number(ms), "kbps": .number(kbps), "frames": .int(frames), "resent": .int(resent),
         "depth": .int(depth), "gapMs": .number(gapMs), "perFrame": .number(perFrame),
         "rtt": ["avg": .number(rttAvg), "median": .number(rttMedian), "p95": .number(rttP95), "max": .number(rttMax)]]
    }
}

/// makeFizzbak's arguments as manager.js backup() passes them.
public struct FizzbakInput: Sendable {
    public var kind: FizzBackupKind
    /// The store image (FZBR), for a full backup.
    public var image: [UInt8]?
    public var region: FizzStoreRegion?
    /// The unit's CRC-32 of the image (FZBI); nil: the image's own.
    public var crc: UInt32?
    /// MY PRESETS' DUMP replies (STORE / STORE6 frames, DUMP flag), made sounds in JS as manager.js does; nil: the
    /// image's presets (a FIZZ that gives no version, no DUMP).
    public var presets: [[UInt8]]?
    public var fizz: String
    public var unitIdentity: String
    public var unitPort: String
    /// The page's web tags (every sound key's); the file keeps those of its presets.
    public var webTags: [String: [String]]
    public var stats: FizzTransferStats?
    public var date: Date

    public init(kind: FizzBackupKind, image: [UInt8]? = nil, region: FizzStoreRegion? = nil, crc: UInt32? = nil, presets: [[UInt8]]? = [],
                fizz: String = "", unitIdentity: String = "", unitPort: String = "", webTags: [String: [String]] = [:],
                stats: FizzTransferStats? = nil, date: Date = Date()) {
        self.kind = kind; self.image = image; self.region = region; self.crc = crc; self.presets = presets; self.fizz = fizz
        self.unitIdentity = unitIdentity; self.unitPort = unitPort; self.webTags = webTags; self.stats = stats; self.date = date
    }
}

public struct FizzProjectSlot: Sendable, Equatable, Hashable {
    public var slot: Int, name: String, demo: Bool
    public init(slot: Int, name: String, demo: Bool = false) { self.slot = slot; self.name = name; self.demo = demo }
}

/// Pages used / pages in all, of MY PRESETS' store and the projects', and the whole region's fraction.
public struct FizzStoreUse: Sendable, Equatable {
    public var libUsed: Int, libTotal: Int, prjUsed: Int, prjTotal: Int, all: Double
    init(_ v: FizzJSValue?) {
        libUsed = v?["lib"]?[0]?.int ?? 0; libTotal = v?["lib"]?[1]?.int ?? 0
        prjUsed = v?["prj"]?[0]?.int ?? 0; prjTotal = v?["prj"]?[1]?.int ?? 0
        all = v?["all"]?.number ?? 0
    }
}

/// A MY PRESETS record of the image (fizzstore.js preset()).
public struct FizzStorePreset: Sendable, Equatable {
    public var slot: Int, id: Int, name: String, tagMask: Int, tags: [String], flags: Int, imported: Bool
    public var blob: [UInt8], drive: Int, pan: Int, sends: FizzSends
}

/// fizzstore.js readRegion: what the BACKUP card shows of a store image.
public struct FizzRegionSummary: Sendable, Equatable {
    public var layout: String, size: Int, magic: String, blank: Bool
    /// persist.h QD_PRJ_FORMAT ('G' 2: six tracks, 1: eight), nil: an empty or older store.
    public var format: Int?, tracks: Int?
    public var projects: [FizzProjectSlot]
    public var presets: [FizzStorePreset]
    public var used: FizzStoreUse

    init(js v: FizzJSValue, tagNames: [String]) {
        layout = v["layout"]?.string ?? ""; size = v["size"]?.int ?? 0; magic = v["magic"]?.string ?? ""; blank = v["blank"]?.bool ?? false
        format = v["format"]?.int; tracks = v["tracks"]?.int
        projects = (v["projects"]?.array ?? []).map { FizzProjectSlot(slot: $0["slot"]?.int ?? 0, name: $0["name"]?.string ?? "", demo: $0["demo"]?.bool ?? false) }
        presets = (v["presets"]?.array ?? []).map { p in
            let mask = p["tags"]?.int ?? 0
            return FizzStorePreset(slot: p["slot"]?.int ?? 0, id: p["id"]?.int ?? 0, name: p["name"]?.string ?? "", tagMask: mask,
                                   tags: tagNames.enumerated().filter { (mask >> $0.offset) & 1 == 1 }.map(\.element),
                                   flags: p["flags"]?.int ?? 0, imported: p["imported"]?.bool ?? false, blob: p["blob"]?.bytes ?? [],
                                   drive: p["drive"]?.int ?? 0, pan: p["pan"]?.int ?? 0, sends: FizzSends(p["sends"]))
        }
        used = FizzStoreUse(v["used"])
    }
}

public struct FizzSkippedFile: Sendable, Equatable {
    public var file: String, error: String
}

/// fizzbak.js readBackup, as the BACKUP screen shows a backup (manager.js openFile's record).
public struct FizzBackupSummary: Sendable, Equatable {
    public enum Source: String, Sendable { case fizzbak, fizzlib, bin }
    /// readBackup's kind ("full" with a store image).
    public var kind: FizzBackupKind
    public var source: Source
    /// The manifest's date (ISO), and as a Date.
    public var dateText: String?
    public var date: Date?
    public var fizz: String
    public var unitIdentity: String?
    public var unitPort: String?
    /// Of the image (readRegion), else the manifest's store.
    public var layout: String?, tracks: Int?, format: Int?, magic: String?
    /// The image's projects; nil for a backup without one (presets only), as manager.js keeps it.
    public var projects: [FizzProjectSlot]?
    public var presetCount: Int
    public var storeSize: Int?
    /// The image's CRC-32 (hex, as the manifest writes it).
    public var storeCRC: String?
    public var region: FizzStoreRegion?
    public var used: FizzStoreUse?
    public var presets: [FizzSound]
    public var webTags: [String: [String]]
    /// A .fizzlib's files that did not read.
    public var skipped: [FizzSkippedFile]
    public var image: [UInt8]?
    /// The image read (readRegion), with its presets' records.
    public var summary: FizzRegionSummary?
    public var transfer: FizzJSValue?
    public var manifest: FizzJSValue

    /// manager.js renderBaks: a full backup (one with an image) of another layout or another track count than the
    /// unit's, or of eight tracks, is shown "old". unitLayout: the layout of the unit's store size (layoutOf).
    public func isOld(unitLayout: String?, unitTracks: Int?) -> Bool {
        Self.isOld(full: image != nil, layout: layout, tracks: tracks, unitLayout: unitLayout, unitTracks: unitTracks)
    }

    /// The same rule for a backup kept in a list (its kind, layout and tracks only).
    public static func isOld(full: Bool, layout: String?, tracks: Int?, unitLayout: String?, unitTracks: Int?) -> Bool {
        guard full else { return false }
        if let u = unitLayout, let l = layout, u != l { return true }
        if let t = tracks, let u = unitTracks, t != u { return true }
        return tracks == 8
    }
}

/// A project for demoStore: its slot (1..8) and name.
public struct FizzProjectName: Sendable, Equatable {
    public var slot: Int, name: String
    public init(slot: Int, name: String) { self.slot = slot; self.name = name }
}
