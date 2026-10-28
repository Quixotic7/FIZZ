// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// The projects kept on the phone (projtab.js's "kept in this browser", IndexedDB there): Documents/Projects/, one
// .fizzprj each, seen in the Files app. The files are the truth: each card read from its file (projects.js readFizzprj
// through the engine), newest first.

import Foundation
import FizzData
import Observation
import UniformTypeIdentifiers

enum ProjectFileTypes {
    static let project = UTType.fizzProject
    static let patterns = UTType.fizzPatterns
}

/// One kept project's card.
public struct KeptProject: Sendable, Equatable, Identifiable {
    public var url: URL
    public var info: FizzKeptInfo
    public var size: Int
    /// the file's date when the file has none
    public var modified: Date
    public var id: String { url.lastPathComponent }
    public var name: String { info.name }
    public var date: Date { info.date ?? modified }
}

@MainActor @Observable
public final class KeptStore {
    /// Newest first.
    public private(set) var items: [KeptProject] = []
    public let directory: URL
    public var totalSize: Int { items.reduce(0) { $0 + $1.size } }

    @ObservationIgnored private let fm = FileManager.default
    @ObservationIgnored private var cache: [String: (size: Int, date: Date, info: FizzKeptInfo)] = [:]

    public init(directory: URL) { self.directory = directory }

    /// Documents/Projects on iOS; a folder in the temporary directory on the Mac (tests, snapshots).
    public static var defaultDirectory: URL {
        #if os(iOS)
        let docs = FileManager.default.urls(for: .documentDirectory, in: .userDomainMask)[0]
        return docs.appendingPathComponent("Projects", isDirectory: true)
        #else
        return FileManager.default.temporaryDirectory.appendingPathComponent("FIZZ-Projects", isDirectory: true)
        #endif
    }

    /// The folder read again (each new or changed file's card read).
    public func reload() async {
        try? fm.createDirectory(at: directory, withIntermediateDirectories: true)
        let urls = (try? fm.contentsOfDirectory(at: directory, includingPropertiesForKeys: [.fileSizeKey, .contentModificationDateKey]))?
            .filter { $0.pathExtension.lowercased() == "fizzprj" } ?? []
        var out: [KeptProject] = []
        for u in urls {
            let v = try? u.resourceValues(forKeys: [.fileSizeKey, .contentModificationDateKey])
            let size = v?.fileSize ?? 0, date = v?.contentModificationDate ?? Date()
            if let c = cache[u.lastPathComponent], c.size == size, c.date == date {
                out.append(KeptProject(url: u, info: c.info, size: size, modified: date))
                continue
            }
            guard let bytes = try? [UInt8](Data(contentsOf: u)),
                  let info = try? await FizzDataShared.engine().projectsKeptInfo(bytes, fileName: u.lastPathComponent) else { continue }
            cache[u.lastPathComponent] = (size, date, info)
            out.append(KeptProject(url: u, info: info, size: size, modified: date))
        }
        items = out.sorted { $0.date > $1.date }
    }

    /// A .fizzprj kept (its name made unique in the folder); its card.
    @discardableResult public func add(_ bytes: [UInt8], fileName: String) async throws -> KeptProject? {
        try fm.createDirectory(at: directory, withIntermediateDirectories: true)
        let u = unique(fileName)
        try Data(bytes).write(to: u, options: .atomic)
        await reload()
        return items.first { $0.url.lastPathComponent == u.lastPathComponent }
    }

    /// A file from outside (Files, AirDrop): read first (not a project file: nothing kept), then kept.
    @discardableResult public func open(_ url: URL) async throws -> KeptProject? {
        let scoped = url.startAccessingSecurityScopedResource()
        defer { if scoped { url.stopAccessingSecurityScopedResource() } }
        let bytes = try [UInt8](Data(contentsOf: url))
        _ = try await FizzDataShared.engine().projectsKeptInfo(bytes, fileName: url.lastPathComponent)
        return try await add(bytes, fileName: url.lastPathComponent)
    }

    public func bytes(_ k: KeptProject) -> [UInt8]? { try? [UInt8](Data(contentsOf: k.url)) }

    /// RENAME: the project's name inside the file, and the file's name.
    public func rename(_ k: KeptProject, to name: String, fizz: String) async throws {
        guard let b = bytes(k) else { return }
        let f = try await FizzDataShared.engine().projectsRenameKept(b, fileName: k.id, to: name, fizz: fizz, date: k.date)
        try fm.removeItem(at: k.url)
        cache[k.id] = nil
        try Data(f.bytes).write(to: unique(f.fileName), options: .atomic)
        await reload()
    }

    public func delete(_ k: KeptProject) {
        try? fm.removeItem(at: k.url)
        cache[k.id] = nil
        items.removeAll { $0.id == k.id }
    }

    private func unique(_ fileName: String) -> URL {
        let ext = (fileName as NSString).pathExtension.isEmpty ? "fizzprj" : (fileName as NSString).pathExtension
        let base = (fileName as NSString).deletingPathExtension
        var u = directory.appendingPathComponent("\(base).\(ext)")
        var n = 2
        while fm.fileExists(atPath: u.path) {
            u = directory.appendingPathComponent("\(base) \(n).\(ext)")
            n += 1
        }
        return u
    }
}
