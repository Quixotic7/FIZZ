// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// Phase I4's calls into the Manager's JS (entry.js "I4"): IMPORT's files read and converted (importer.js readImport:
// .fizzlib .fizzsnd .fizzbak, Digitone .dnsnd, Digitone II .dn2pst / .dn2snd), the duplicates against MY PRESETS
// (markDuplicates), EXPORT's carry bars and files (exporter.js), and the restore's plan and fit for another package's
// backup (manager.js openRestore, projects.js fitModel).

import Foundation

/// Where an IMPORT row came from (its chip: FIZZ, DN1, DN II).
public enum FizzImportKind: String, Sendable, Equatable, CaseIterable {
    case fizz, dn1, dn2
}

/// One row of IMPORT (importer.js readImport's item).
public struct FizzImportItem: Sendable, Equatable {
    /// The file it came from ("lib.fizzlib: 001 NAME.fizzsnd" for a library's member).
    public var file: String
    public var ok: Bool
    /// Why it did not read (ok false).
    public var error: String
    public var kind: FizzImportKind
    public var sound: FizzSound?
    /// What did not carry over or was approximated (the fidelity bar counts them).
    public var notes: [String]
    /// The reader's quiet note about the file itself ("older Digitone file (OS 1.42)", "FIZZ 0.12.0").
    public var mark: String
    /// The Digitone's own tags (FIZZ's are the sound's tags).
    public var digitoneTags: [String]
    /// A .fizzbak's web tags for this sound.
    public var webTags: [String]
    /// A MIDI track's sound (sound.js isMidiBlob)
    public var midi: Bool

    init(js v: FizzJSValue) {
        file = v["file"]?.string ?? ""
        ok = v["ok"]?.bool ?? false
        error = v["error"]?.string ?? ""
        kind = FizzImportKind(rawValue: v["kind"]?.string ?? "") ?? .dn1
        let s = v["sound"]?.nonNull
        sound = s.map { FizzSound(js: $0) }
        notes = s?["notes"]?.array?.compactMap(\.string) ?? []
        mark = s?["mark"]?.string ?? ""
        digitoneTags = s?["tags"]?.array?.compactMap(\.string) ?? []
        webTags = v["web"]?.array?.compactMap(\.string) ?? []
        midi = v["midi"]?.bool ?? false
    }
}

/// EXPORT's formats (exporter.js KINDS; the Digitone II one as its two files).
public enum FizzExportKind: String, Sendable, Equatable, CaseIterable {
    case fizz, dnsnd, dn2pst, dn2snd
}

/// What a format keeps of the sounds (exporter.js rowsFor / carryOf).
public struct FizzCarry: Sendable, Equatable {
    public struct Row: Sendable, Equatable {
        /// Its subject ("LFO 3"), its verdict ("dropped", "as ADSR", "nearest step"), the page's colour token
        /// ("--red"), the writer's note, and in how many of the sounds
        public var what: String, how: String, color: String, note: String
        public var weight: Double
        public var count: Int
    }
    public var lost: Int, changed: Int
    /// The carry bar: 0..10 segments.
    public var bar: Int
    public var rows: [Row]
    /// Several sounds: how many lose something.
    public var touched: Int?
    /// Only MIDI sounds: nothing to write.
    public var none: Bool

    init(js v: FizzJSValue) {
        lost = v["lost"]?.int ?? 0
        changed = v["changed"]?.int ?? 0
        bar = v["bar"]?.int ?? 10
        rows = (v["rows"]?.array ?? []).map {
            Row(what: $0["what"]?.string ?? "", how: $0["how"]?.string ?? "", color: $0["c"]?.string ?? "",
                note: $0["note"]?.string ?? "", weight: $0["w"]?.number ?? 0, count: $0["n"]?.int ?? 1)
        }
        touched = v["touched"]?.int
        none = v["none"]?.bool ?? false
    }
}

/// A file EXPORT made (exporter.js exportFiles).
public struct FizzExportFile: Sendable, Equatable {
    public var name: String
    public var bytes: [UInt8]
    /// application/json, application/zip, application/octet-stream
    public var type: String
    /// The MIDI sounds a Digitone file left out.
    public var skipped: [String]
}

/// A layout's limits (fizzstore.js LAYOUTS).
public struct FizzPackageLimits: Sendable, Equatable {
    public var name: String, package: String, packageName: String
    public var nproj: Int, nlib: Int, size: Int

    init?(js v: FizzJSValue?) {
        guard let v, let name = v["name"]?.string else { return nil }
        self.name = name
        package = v["package"]?.string ?? ""
        packageName = v["packageName"]?.string ?? package
        nproj = v["nproj"]?.int ?? 8
        nlib = v["nlib"]?.int ?? 256
        size = v["size"]?.int ?? 0
    }
}

/// What to keep of a backup written in another layout: the projects (0-based) and MY PRESETS (0-based places).
public struct FizzFitChoice: Sendable, Equatable {
    public var projects: [Int], presets: [Int]
    public init(projects: [Int], presets: [Int]) { self.projects = projects; self.presets = presets }
    var js: FizzJSValue { ["projects": .array(projects.map { .int($0) }), "presets": .array(presets.map { .int($0) })] }
}

/// manager.js openRestore: a full restore, or the presets added; for another layout's backup, what to choose from.
public struct FizzRestorePlan: Sendable, Equatable {
    public enum Mode: String, Sendable { case full, presets }
    public struct Fit: Sendable, Equatable {
        public var choice: FizzFitChoice
        /// The backup's projects (0-based) and MY PRESETS (0-based place, slot, name, tag mask).
        public var projects: [(p: Int, name: String)]
        public var presets: [(i: Int, slot: Int, name: String, tags: Int)]

        public static func == (a: Fit, b: Fit) -> Bool {
            a.choice == b.choice && a.projects.map(\.p) == b.projects.map(\.p) && a.projects.map(\.name) == b.projects.map(\.name)
                && a.presets.map(\.i) == b.presets.map(\.i) && a.presets.map(\.name) == b.presets.map(\.name)
        }
    }
    public var mode: Mode
    /// The unit's layout, and the backup's (nil: no store image in it).
    public var to: FizzPackageLimits?, from: FizzPackageLimits?
    public var fit: Fit?
    /// Why a full backup is added as presets only (its image did not read as a model).
    public var error: String

    init(js v: FizzJSValue) {
        mode = Mode(rawValue: v["mode"]?.string ?? "") ?? .presets
        to = FizzPackageLimits(js: v["to"]?.nonNull)
        from = FizzPackageLimits(js: v["from"]?.nonNull)
        error = v["error"]?.string ?? ""
        fit = v["fit"]?.nonNull.map { f in
            Fit(choice: FizzFitChoice(projects: f["choice"]?["projects"]?.array?.compactMap(\.int) ?? [],
                                      presets: f["choice"]?["presets"]?.array?.compactMap(\.int) ?? []),
                projects: (f["projects"]?.array ?? []).map { ($0["p"]?.int ?? 0, $0["name"]?.string ?? "") },
                presets: (f["presets"]?.array ?? []).map { ($0["i"]?.int ?? 0, $0["slot"]?.int ?? 0, $0["name"]?.string ?? "", $0["tags"]?.int ?? 0) })
        }
    }

    /// The package changes (a USB backup onto the Bluetooth package, or back).
    public var crossesPackages: Bool { fit != nil && from?.package != to?.package }
}

/// projects.js fitModel's result: the image for this layout, how many sounds became the projects' own copies, and
/// what was left out.
public struct FizzRestoreFit: Sendable, Equatable {
    public var image: [UInt8]?
    public var copies: Int
    public var leftProjects: [String]
    public var leftPresets: [String]
}

extension FizzDataEngine {
    /// importer.js readImport: each file's sounds (a .fizzlib's every member, a .fizzbak's presets), or why not.
    public func importRead(_ files: [(name: String, bytes: [UInt8])]) async throws -> [FizzImportItem] {
        let v = try await call("importRead", [.array(files.map { ["name": .string($0.name), "bytes": .bytes($0.bytes)] })])
        return (v.array ?? []).map(FizzImportItem.init(js:))
    }

    /// importer.js markDuplicates: "mine" (already in MY PRESETS: mine are their DUMP replies), "list" (earlier in
    /// these), or "".
    public func importDuplicates(_ sounds: [FizzSound], mine: [[UInt8]]) async throws -> [String] {
        let v = try await call("importDuplicates", [.array(sounds.map(\.js)), .array(mine.map { .bytes($0) })])
        return (v.array ?? []).map { $0.string ?? "" }
    }

    /// exporter.js rowsFor: what `kind` keeps of these sounds.
    public func exportCarry(_ sounds: [FizzSound], kind: FizzExportKind) async throws -> FizzCarry {
        FizzCarry(js: try await call("exportCarry", [.array(sounds.map(\.js)), .string(kind.rawValue)]))
    }

    /// exporter.js exportFiles: one sound its file, several a .fizzlib or a ZIP. Throws for MIDI sounds only (a
    /// Digitone kind).
    public func exportFile(_ sounds: [FizzSound], kind: FizzExportKind, date: Date = Date()) async throws -> FizzExportFile {
        let v = try await call("exportFile", [.array(sounds.map(\.js)), .string(kind.rawValue), .date(date)])
        return FizzExportFile(name: v["name"]?.string ?? "", bytes: v["bytes"]?.bytes ?? [], type: v["type"]?.string ?? "",
                              skipped: v["skipped"]?.array?.compactMap(\.string) ?? [])
    }

    /// manager.js openRestore: how a backup goes onto a unit whose store is `unitSize` bytes.
    public func restorePlan(image: [UInt8]?, unitSize: Int) async throws -> FizzRestorePlan {
        FizzRestorePlan(js: try await call("restorePlan", [image.map { .bytes($0) } ?? .null, .int(unitSize)]))
    }

    /// projects.js fitModel: the backup's records in the unit's layout as chosen (throws, code "range", when the
    /// choice holds more than the package does).
    public func restoreFit(image: [UInt8], unitSize: Int, choice: FizzFitChoice, withImage: Bool) async throws -> FizzRestoreFit {
        let v = try await call("restoreFit", [.bytes(image), .int(unitSize), choice.js, .bool(withImage)])
        let names = { (k: String) in (v["left"]?[k]?.array ?? []).map { $0["name"]?.string ?? "?" } }
        return FizzRestoreFit(image: v["image"]?.bytes, copies: v["copies"]?.int ?? 0, leftProjects: names("projects"),
                              leftPresets: names("presets"))
    }
}
