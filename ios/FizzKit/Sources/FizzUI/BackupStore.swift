// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// The backups kept (manager.js keepBak / renderBaks, IndexedDB there): Documents/Backups/, one file each (.fizzbak,
// or a .fizzlib / .bin opened from outside), and index.json with what the list shows. The files are the truth: the
// index is made again from them (fizzbak.js readBackup) when it is missing or a file came or went in the Files app.

import Foundation
import FizzData
import Observation
import UniformTypeIdentifiers

extension UTType {
    /// .fizzbak (a stored ZIP; declared as data, so Files does not offer to unzip it)
    public static let fizzBackup = UTType(exportedAs: "com.quixotic7.fizz.backup", conformingTo: .data)
    /// .fizzlib
    public static let fizzLibrary = UTType(exportedAs: "com.quixotic7.fizz.library", conformingTo: .data)
}

/// One backup in the list (manager.js's record without its bytes).
public struct BackupEntry: Codable, Sendable, Equatable, Identifiable {
    /// The file's name in Backups/.
    public var name: String
    public var date: Date
    /// "full" (a store image in it) or "presets"
    public var kind: String
    public var size: Int
    public var fizz: String
    public var identity: String
    /// The image's projects (nil: presets only)
    public var projects: [String]?
    public var presets: Int
    public var tracks: Int?
    public var layout: String?
    /// Shared at least once (the web's "saved to a file")
    public var saved: Bool
    /// Came from outside (opened, or put in the folder from Files)
    public var file: Bool

    public var id: String { name }
    public var isFull: Bool { kind == "full" }

    /// manager.js renderBaks's "old" against the connected unit.
    public func isOld(unitLayout: String?, unitTracks: Int?) -> Bool {
        FizzBackupSummary.isOld(full: isFull, layout: layout, tracks: tracks, unitLayout: unitLayout, unitTracks: unitTracks)
    }

    init(name: String, size: Int, summary s: FizzBackupSummary, date fallback: Date, saved: Bool, file: Bool) {
        self.name = name
        self.date = s.date ?? fallback
        kind = s.image != nil ? "full" : "presets"
        self.size = size
        fizz = s.fizz
        identity = s.unitIdentity ?? ""
        projects = s.summary.map { $0.projects.map(\.name) }
        presets = s.presetCount
        tracks = s.summary?.tracks
        layout = s.summary?.layout
        self.saved = saved
        self.file = file
    }
}

public enum BackupStoreError: Error, Equatable {
    case notABackup(String)
}

@MainActor @Observable
public final class BackupStore {
    /// Newest first.
    public private(set) var entries: [BackupEntry] = []
    public let directory: URL
    public var totalSize: Int { entries.reduce(0) { $0 + $1.size } }

    static let extensions: Set<String> = ["fizzbak", "fizzlib", "bin"]
    static let indexName = "index.json"

    @ObservationIgnored private var unreadable: [String: Int] = [:]
    @ObservationIgnored private var loaded = false
    @ObservationIgnored private let fm = FileManager.default

    public init(directory: URL) { self.directory = directory }

    /// Documents/Backups on iOS (seen in the Files app); a folder in the temporary directory on the Mac (the tests,
    /// the snapshots: the Mac's Documents stays untouched).
    public static var defaultDirectory: URL {
        #if os(iOS)
        let docs = FileManager.default.urls(for: .documentDirectory, in: .userDomainMask)[0]
        return docs.appendingPathComponent("Backups", isDirectory: true)
        #else
        return FileManager.default.temporaryDirectory.appendingPathComponent("FIZZ-Backups", isDirectory: true)
        #endif
    }

    public func url(of e: BackupEntry) -> URL { directory.appendingPathComponent(e.name) }

    // MARK: reading the folder

    /// The list from index.json, checked against the files (each new or changed one read with readBackup).
    public func reload() async {
        try? fm.createDirectory(at: directory, withIntermediateDirectories: true)
        let index = readIndex()
        var known: [String: BackupEntry] = [:]
        for e in index ?? [] { known[e.name] = e }
        var out: [BackupEntry] = []
        var changed = index == nil
        for (name, size, modified) in files() {
            if let e = known[name], e.size == size { out.append(e); continue }
            changed = true
            if unreadable[name] == size { continue }
            guard let bytes = try? [UInt8](Data(contentsOf: directory.appendingPathComponent(name))),
                  let s = try? await FizzDataShared.engine().readBackup(bytes, fileName: name) else {
                unreadable[name] = size
                continue
            }
            // (a file the index lost: not known to be shared; one put in the folder from outside: a copy exists)
            let old = known[name]
            out.append(BackupEntry(name: name, size: size, summary: s, date: modified,
                                   saved: old?.saved ?? (index != nil), file: old?.file ?? (index != nil)))
        }
        if out.count != (index ?? []).count { changed = true }
        // (one added while the files were read)
        let seen = Set(out.map(\.name))
        out += entries.filter { !seen.contains($0.name) && fm.fileExists(atPath: url(of: $0).path) }
        entries = Self.sorted(out)
        loaded = true
        if changed { writeIndex() }
    }

    private func files() -> [(String, Int, Date)] {
        let keys: [URLResourceKey] = [.fileSizeKey, .contentModificationDateKey, .isRegularFileKey]
        let urls = (try? fm.contentsOfDirectory(at: directory, includingPropertiesForKeys: keys, options: [.skipsHiddenFiles])) ?? []
        return urls.compactMap { u in
            guard Self.extensions.contains(u.pathExtension.lowercased()),
                  let v = try? u.resourceValues(forKeys: Set(keys)), v.isRegularFile == true else { return nil }
            return (u.lastPathComponent, v.fileSize ?? 0, v.contentModificationDate ?? Date())
        }
    }

    static func sorted(_ a: [BackupEntry]) -> [BackupEntry] {
        a.sorted { $0.date != $1.date ? $0.date > $1.date : $0.name > $1.name }
    }

    private struct Index: Codable { var version = 1; var backups: [BackupEntry] }

    private static let encoder: JSONEncoder = {
        let e = JSONEncoder()
        e.outputFormatting = [.prettyPrinted, .sortedKeys]
        e.dateEncodingStrategy = .iso8601
        return e
    }()
    private static let decoder: JSONDecoder = {
        let d = JSONDecoder()
        d.dateDecodingStrategy = .iso8601
        return d
    }()

    private func readIndex() -> [BackupEntry]? {
        guard let d = try? Data(contentsOf: directory.appendingPathComponent(Self.indexName)) else { return nil }
        return (try? Self.decoder.decode(Index.self, from: d))?.backups
    }

    private func writeIndex() {
        guard let d = try? Self.encoder.encode(Index(backups: entries)) else { return }
        try? d.write(to: directory.appendingPathComponent(Self.indexName), options: .atomic)
    }

    // MARK: changes

    /// A backup's bytes kept under `name` (made unique): checked with readBackup first, nothing written if it
    /// does not read.
    @discardableResult
    public func add(_ bytes: [UInt8], name: String, file: Bool = false, saved: Bool = false) async throws -> BackupEntry {
        if !loaded { await reload() }
        let s: FizzBackupSummary
        do { s = try await FizzDataShared.engine().readBackup(bytes, fileName: name) } catch {
            throw BackupStoreError.notABackup(name)
        }
        try fm.createDirectory(at: directory, withIntermediateDirectories: true)
        let unique = uniqueName(name)
        try Data(bytes).write(to: directory.appendingPathComponent(unique), options: .atomic)
        let e = BackupEntry(name: unique, size: bytes.count, summary: s, date: Date(), saved: saved, file: file)
        entries = Self.sorted(entries + [e])
        writeIndex()
        return e
    }

    /// A file from outside (the file picker, Open in FIZZ): read under its security scope, copied in. One already
    /// in the folder (opened from Files) is only listed.
    @discardableResult
    public func importFile(at url: URL) async throws -> BackupEntry {
        let scoped = url.startAccessingSecurityScopedResource()
        defer { if scoped { url.stopAccessingSecurityScopedResource() } }
        if url.deletingLastPathComponent().standardizedFileURL.path == directory.standardizedFileURL.path {
            await reload()
            if let e = entries.first(where: { $0.name == url.lastPathComponent }) { return e }
            throw BackupStoreError.notABackup(url.lastPathComponent)
        }
        var data: Data?
        var err: NSError?
        NSFileCoordinator().coordinate(readingItemAt: url, options: .withoutChanges, error: &err) { u in
            data = try? Data(contentsOf: u)
        }
        guard let data else { throw err ?? BackupStoreError.notABackup(url.lastPathComponent) }
        return try await add([UInt8](data), name: url.lastPathComponent, file: true, saved: true)
    }

    /// The file deleted and the list updated.
    public func delete(_ e: BackupEntry) {
        try? fm.removeItem(at: url(of: e))
        entries.removeAll { $0.name == e.name }
        writeIndex()
    }

    /// Shared (Files, AirDrop, Mail): its delete no longer warns.
    public func markSaved(_ e: BackupEntry) {
        guard let k = entries.firstIndex(where: { $0.name == e.name }), !entries[k].saved else { return }
        entries[k].saved = true
        writeIndex()
    }

    func uniqueName(_ name: String) -> String {
        let clean = name.replacingOccurrences(of: "/", with: "_").replacingOccurrences(of: ":", with: "_")
        guard fm.fileExists(atPath: directory.appendingPathComponent(clean).path) else { return clean }
        let ext = (clean as NSString).pathExtension, stem = (clean as NSString).deletingPathExtension
        var k = 2
        while true {
            let n = ext.isEmpty ? "\(stem) \(k)" : "\(stem) \(k).\(ext)"
            if !fm.fileExists(atPath: directory.appendingPathComponent(n).path) { return n }
            k += 1
        }
    }
}
