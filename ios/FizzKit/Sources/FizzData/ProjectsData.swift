// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// PROJECTS' data (phase I6): projtab.js's model run by projects.js in the engine (entry.js "I6": pjOpen ..). The store's
// records stay in JavaScriptCore, held by a session number; Swift gets what the screens draw (each project's 64
// patterns as trig masks, the changes waiting) and the files' bytes (.fizzprj, .fizzpat, the store image to write).

import Foundation

/// One pattern's tile: each track's trigs on the page it shows (bit x = step x + 1), or a demo's / an unreadable one.
public struct FizzPatternTile: Sendable, Equatable {
    /// six 16-bit masks
    public var rows: [UInt16]
    /// one of the demos' own (compiled in), or a record projects.js cannot walk
    public var demo = false, bad = false

    public init(rows: [UInt16], demo: Bool = false, bad: Bool = false) { self.rows = rows; self.demo = demo; self.bad = bad }

    init?(js v: FizzJSValue) {
        guard v.nonNull != nil else { return nil }
        rows = (v["r"]?.array ?? []).map { UInt16(truncatingIfNeeded: $0.int ?? 0) }
        demo = v["demo"]?.bool ?? false
        bad = v["bad"]?.bool ?? false
    }

    public var trigs: Int { rows.reduce(0) { $0 + $1.nonzeroBitCount } }
}

/// One project of the store as read (and changed).
public struct FizzProjectModel: Sendable, Equatable, Identifiable {
    /// 0..7
    public var p: Int
    public var there: Bool, name: String, demo: Bool
    /// its patterns used, its pool's sounds
    public var used: Int, pool: Int
    /// a change waits for it
    public var changed: Bool
    /// A01..D16
    public var pats: [FizzPatternTile?]
    public var id: Int { p }

    init(js v: FizzJSValue) {
        p = v["p"]?.int ?? 0
        there = v["there"]?.bool ?? false
        name = v["name"]?.string ?? ""
        demo = v["demo"]?.bool ?? false
        used = v["used"]?.int ?? 0
        pool = v["pool"]?.int ?? 0
        changed = v["changed"]?.bool ?? false
        pats = (v["pats"]?.array ?? []).map { FizzPatternTile(js: $0) }
    }
}

/// A change waiting for WRITE TO FM-1.
public struct FizzProjectsOp: Sendable, Equatable {
    public enum Kind: String, Sendable { case paste, clear, load }
    public var kind: Kind
    /// the slot it changes (0..7)
    public var q: Int
    /// the patterns pasted (at) or cleared (ks)
    public var at: [Int], ks: [Int]
    /// the clip's project, the kept project's name
    public var name: String
    public var poolAdded: Int, presetsAdded: Int, copies: Int

    init(js v: FizzJSValue) {
        kind = Kind(rawValue: v["kind"]?.string ?? "") ?? .paste
        q = v["q"]?.int ?? 0
        at = (v["at"]?.array ?? []).compactMap(\.int)
        ks = (v["ks"]?.array ?? []).compactMap(\.int)
        name = v["name"]?.string ?? ""
        poolAdded = v["poolAdded"]?.int ?? 0
        presetsAdded = v["presetsAdded"]?.int ?? 0
        copies = v["copies"]?.int ?? 0
    }
}

/// The store as the PROJECTS tab draws it: its projects with the changes applied, the changes.
public struct FizzProjectsView: Sendable, Equatable {
    public var layout: String, package: String
    public var nproj: Int, nlib: Int
    public var projects: [FizzProjectModel]
    public var ops: [FizzProjectsOp]

    init(js v: FizzJSValue) {
        layout = v["layout"]?.string ?? ""
        package = v["package"]?.string ?? "usb"
        nproj = v["nproj"]?.int ?? 8
        nlib = v["nlib"]?.int ?? 256
        projects = (v["projects"]?.array ?? []).map(FizzProjectModel.init(js:))
        ops = (v["ops"]?.array ?? []).map(FizzProjectsOp.init(js:))
    }

    public func project(_ p: Int) -> FizzProjectModel? { projects.indices.contains(p) ? projects[p] : nil }
    /// the pool places the changes add
    public var poolAdded: Int { ops.reduce(0) { $0 + $1.poolAdded } }
}

/// Patterns copied (from a slot, a kept project, a .fizzpat), held in the engine by `c`.
public struct FizzClip: Sendable, Equatable {
    public var c: Int, n: Int
    public var from: String
    /// "A03" .., and their patterns (0..63)
    public var names: [String], ks: [Int]

    init(js v: FizzJSValue) {
        c = v["c"]?.int ?? 0
        n = v["n"]?.int ?? 0
        from = v["from"]?.string ?? ""
        names = (v["names"]?.array ?? []).compactMap(\.string)
        ks = (v["ks"]?.array ?? []).compactMap(\.int)
    }

    /// "A03–A05", "A03 A05", "A03 +4" (projtab.js range)
    public var range: String { FizzClip.range(ks) }

    public static func range(_ ks: [Int]) -> String {
        let a = ks.sorted()
        guard let f = a.first, let l = a.last else { return "" }
        if a.count == 1 { return patName(f) }
        if l - f == a.count - 1 { return "\(patName(f))–\(patName(l))" }
        return a.count <= 3 ? a.map(patName).joined(separator: " ") : "\(patName(f)) +\(a.count - 1)"
    }

    /// projects.js patName: 0 -> "A01", 63 -> "D16"
    public static func patName(_ k: Int) -> String {
        let bank = ["A", "B", "C", "D"][max(0, min(3, k >> 4))]
        return bank + String(format: "%02d", (k & 15) + 1)
    }
}

/// A kept project's file made (KEEP), or renamed.
public struct FizzKeptFile: Sendable, Equatable {
    public var bytes: [UInt8]
    public var fileName: String, name: String
    public var pats: Int, sounds: Int
    public var colours: [String]
}

/// What a .fizzprj's card shows.
public struct FizzKeptInfo: Sendable, Equatable {
    public var name: String, demo: Bool
    public var pats: Int, sounds: Int
    /// its sounds' tag colours (projects.js colours: "--blue" ..)
    public var colours: [String]
    public var fizz: String
    public var date: Date?
    /// A01..D16 used
    public var used: [Bool]
}

/// What projects.js refused (code "poolfull", "full", "range", "demo", "libfull", "error"), its message.
public struct FizzProjectsRefusal: Error, Sendable, Equatable {
    public var code: String, message: String
}

extension FizzDataEngine {
    private func edit(_ name: String, _ args: [FizzJSValue]) async throws -> Result<FizzProjectsView, FizzProjectsRefusal> {
        let v = try await call(name, args)
        if v["ok"]?.bool == true, let view = v["view"] { return .success(FizzProjectsView(js: view)) }
        return .failure(FizzProjectsRefusal(code: v["code"]?.string ?? "error", message: v["message"]?.string ?? ""))
    }

    private static func opts(fizz: String, date: Date) -> FizzJSValue { ["fizz": .string(fizz), "date": .date(date)] }

    /// A session on a store image: its number and view.
    public func projectsOpen(_ image: [UInt8]) async throws -> (session: Int, view: FizzProjectsView) {
        let v = try await call("pjOpen", [.bytes(image)])
        guard let h = v["h"]?.int, let view = v["view"] else { throw FizzDataError.bridge("pjOpen") }
        return (h, FizzProjectsView(js: view))
    }

    public func projectsClose(_ h: Int) async { _ = try? await call("pjClose", [.int(h)]) }

    public func projectsView(_ h: Int) async throws -> FizzProjectsView { FizzProjectsView(js: try await call("pjView", [.int(h)])) }

    /// The store read again under the changes waiting (they are made again on it; one that no longer fits: all dropped,
    /// the refusal).
    public func projectsRebase(_ h: Int, image: [UInt8]) async throws -> (view: FizzProjectsView, refusal: FizzProjectsRefusal?) {
        let v = try await call("pjRebase", [.int(h), .bytes(image)])
        let view = FizzProjectsView(js: v["view"] ?? [:])
        return (view, v["ok"]?.bool == true ? nil : FizzProjectsRefusal(code: v["code"]?.string ?? "", message: v["message"]?.string ?? ""))
    }

    /// The clip's patterns into slot q from pattern j on.
    public func projectsPaste(_ h: Int, q: Int, j: Int, clip: Int) async throws -> Result<FizzProjectsView, FizzProjectsRefusal> {
        try await edit("pjPaste", [.int(h), .int(q), .int(j), .int(clip)])
    }

    public func projectsClear(_ h: Int, q: Int, ks: [Int]) async throws -> Result<FizzProjectsView, FizzProjectsRefusal> {
        try await edit("pjClear", [.int(h), .int(q), .array(ks.map { .int($0) })])
    }

    /// A kept project (.fizzprj) loaded into slot q (its records replace the slot's).
    public func projectsLoadKept(_ h: Int, q: Int, file: [UInt8], fileName: String) async throws -> Result<FizzProjectsView, FizzProjectsRefusal> {
        try await edit("pjLoadKept", [.int(h), .int(q), .bytes(file), .string(fileName)])
    }

    public func projectsUndo(_ h: Int) async throws -> FizzProjectsView { FizzProjectsView(js: try await call("pjUndo", [.int(h)])) }
    public func projectsDiscard(_ h: Int) async throws -> FizzProjectsView { FizzProjectsView(js: try await call("pjDiscard", [.int(h)])) }

    /// The store image with the changes (what WRITE TO FM-1 writes).
    public func projectsImage(_ h: Int) async throws -> Result<[UInt8], FizzProjectsRefusal> {
        let v = try await call("pjImage", [.int(h)])
        if let b = v["image"]?.bytes { return .success(b) }
        return .failure(FizzProjectsRefusal(code: v["code"]?.string ?? "error", message: v["message"]?.string ?? ""))
    }

    /// Slot p's patterns ks copied (nil: nothing there to copy: empty, or the demos' own).
    public func projectsCopy(_ h: Int, p: Int, ks: [Int]) async throws -> FizzClip? {
        let c = FizzClip(js: try await call("pjCopy", [.int(h), .int(p), .array(ks.map { .int($0) })]))
        return c.n > 0 ? c : nil
    }

    /// Patterns copied out of a kept project (as slot 1 of a store of its own; h: the session whose MY PRESETS it uses).
    public func projectsCopyKept(_ h: Int?, file: [UInt8], fileName: String, ks: [Int]) async throws -> Result<FizzClip?, FizzProjectsRefusal> {
        let v = try await call("pjCopyKept", [.int(h ?? 0), .bytes(file), .string(fileName), .array(ks.map { .int($0) })])
        guard v["ok"]?.bool == true else { return .failure(FizzProjectsRefusal(code: v["code"]?.string ?? "", message: v["message"]?.string ?? "")) }
        let c = FizzClip(js: v)
        return .success(c.n > 0 ? c : nil)
    }

    /// A .fizzpat read into a clip.
    public func projectsReadFizzpat(_ file: [UInt8], fileName: String) async throws -> Result<FizzClip?, FizzProjectsRefusal> {
        let v = try await call("pjReadFizzpat", [.bytes(file), .string(fileName)])
        guard v["ok"]?.bool == true else { return .failure(FizzProjectsRefusal(code: v["code"]?.string ?? "", message: v["message"]?.string ?? "")) }
        let c = FizzClip(js: v)
        return .success(c.n > 0 ? c : nil)
    }

    public func projectsFreeClip(_ c: Int) async { _ = try? await call("pjFree", [.int(c)]) }

    /// A clip as a .fizzpat: its bytes and file name.
    public func projectsFizzpat(_ c: Int, fizz: String, date: Date = Date()) async throws -> (bytes: [UInt8], fileName: String) {
        let v = try await call("pjFizzpat", [.int(c), Self.opts(fizz: fizz, date: date)])
        guard let b = v["bytes"]?.bytes else { throw FizzDataError.bridge("pjFizzpat") }
        return (b, v["fileName"]?.string ?? "patterns.fizzpat")
    }

    /// KEEP: slot p (as changed) as a .fizzprj.
    public func projectsKeep(_ h: Int, p: Int, fizz: String, date: Date = Date()) async throws -> FizzKeptFile {
        let v = try await call("pjKeep", [.int(h), .int(p), Self.opts(fizz: fizz, date: date)])
        return try Self.kept(v, "pjKeep")
    }

    /// A .fizzprj's card.
    public func projectsKeptInfo(_ file: [UInt8], fileName: String) async throws -> FizzKeptInfo {
        let v = try await call("pjKeptInfo", [.bytes(file), .string(fileName)])
        guard v["ok"]?.bool == true else { throw FizzDataError.bridge(v["message"]?.string ?? "not a FIZZ project file") }
        return FizzKeptInfo(name: v["name"]?.string ?? "", demo: v["demo"]?.bool ?? false, pats: v["pats"]?.int ?? 0,
                            sounds: v["sounds"]?.int ?? 0, colours: (v["colours"]?.array ?? []).compactMap(\.string),
                            fizz: v["fizz"]?.string ?? "", date: v["date"]?.string.flatMap(FizzDataEngine.parseDate),
                            used: (v["used"]?.array ?? []).map { $0.bool ?? false })
    }

    /// A kept project renamed (a new .fizzprj).
    public func projectsRenameKept(_ file: [UInt8], fileName: String, to name: String, fizz: String, date: Date = Date()) async throws -> FizzKeptFile {
        let v = try await call("pjRenameKept", [.bytes(file), .string(fileName), .string(name), Self.opts(fizz: fizz, date: date)])
        return try Self.kept(v, "pjRenameKept")
    }

    private static func kept(_ v: FizzJSValue, _ what: String) throws -> FizzKeptFile {
        guard let b = v["bytes"]?.bytes else { throw FizzDataError.bridge(what) }
        return FizzKeptFile(bytes: b, fileName: v["fileName"]?.string ?? "PROJECT.fizzprj", name: v["name"]?.string ?? "",
                            pats: v["pats"]?.int ?? 0, sounds: v["sounds"]?.int ?? 0,
                            colours: (v["colours"]?.array ?? []).compactMap(\.string))
    }

    /// The demo unit's store with its projects' patterns (synthetic; the ones of `prev` kept by name, else by slot).
    public func demoPatterns(_ image: [UInt8], prev: [UInt8]?, projects: [(slot: Int, name: String, pats: Int, demo: Bool)]) async throws -> [UInt8] {
        let ps: [FizzJSValue] = projects.map { ["slot": .int($0.slot), "name": .string($0.name), "pats": .int($0.pats), "demo": .bool($0.demo)] }
        let v = try await call("demoPatterns", [.bytes(image), ["prev": prev.map { .bytes($0) } ?? .null, "projects": .array(ps)]])
        guard let b = v.bytes else { throw FizzDataError.bridge("demoPatterns") }
        return b
    }
}
