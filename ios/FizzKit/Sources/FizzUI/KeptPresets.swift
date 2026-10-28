// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// KEPT, the phone's own presets (mock-ups 23-26, 28): Documents/Presets/, one .fizzsnd each, seen in the Files app,
// read and written by the web Manager's own JS (FizzData keptFile / keptRead: js/library.js). The files are the truth:
// an index (Caches) keeps each file's card by its size and date, so only new or changed files are read again; the
// folder is watched, so a file added or removed in Files (or AirDropped in) shows at once. File names are unique on
// disk ("NAME 2.fizzsnd"); FIZZ's tags and the web tags live in the file.

import Foundation
import FizzData
import FizzKit
import Observation

/// One kept preset's card.
public struct KeptPreset: Sendable, Equatable, Identifiable {
    /// its file's name
    public var id: String
    public var url: URL
    /// the file's sound (no source: the full file is read when it is sent, edited or changed)
    public var sound: FizzSound
    public var webTags: [String]
    public var size: Int
    public var modified: Date

    public var name: String { sound.name }
    /// FIZZ's tags as the unit's mask
    public var tagMask: Int { FizzTag.allCases.filter { sound.tags.contains($0.name) }.reduce(0) { $0 | 1 << $1.rawValue } }
    public var tint: RGB { FizzTag.tint(mask: tagMask) }
    public var algorithm: Int { sound.blob.count > 2 ? min(8, Int(sound.blob[2]) + 1) : 1 }
}

/// What → KEPT came to.
public enum KeepOutcome: Sendable, Equatable {
    case kept(KeptPreset)
    /// the same sound (name and patch) is kept already
    case already(KeptPreset)
    /// its DUMP could not be read, or the file not written
    case failed
}

@MainActor @Observable
public final class KeptPresets {
    /// By name.
    public private(set) var items: [KeptPreset] = []
    public private(set) var loaded = false
    public let directory: URL
    /// LIBRARY's SELECT on KEPT (the phone's tab bar gives way to the selection's bar)
    public var selecting = false
    /// A preset to show in LIBRARY's KEPT (a file opened in FIZZ, a new one saved)
    public var reveal: String?

    @ObservationIgnored private let fm = FileManager.default
    @ObservationIgnored private let indexURL: URL?
    @ObservationIgnored private var cache: [String: IndexEntry] = [:]
    @ObservationIgnored private var watcher: DispatchSourceFileSystemObject?
    @ObservationIgnored private var pending: Task<Void, Never>?
    @ObservationIgnored private var reloading = false
    @ObservationIgnored private var again = false

    /// index: where the cards are cached (nil: in memory only).
    public init(directory: URL, index: URL? = nil) {
        self.directory = directory
        self.indexURL = index
        if let index, let d = try? Data(contentsOf: index), let c = try? JSONDecoder().decode([String: IndexEntry].self, from: d) {
            cache = c
        }
    }

    /// Documents/Presets on iOS (the Files app shows it); a folder in the temporary directory on the Mac.
    public static var defaultDirectory: URL {
        #if os(iOS)
        return FileManager.default.urls(for: .documentDirectory, in: .userDomainMask)[0].appendingPathComponent("Presets", isDirectory: true)
        #else
        return FileManager.default.temporaryDirectory.appendingPathComponent("FIZZ-Presets", isDirectory: true)
        #endif
    }

    /// Caches/kept-presets.json on iOS (rebuilt from the files whenever it is gone).
    public static var defaultIndex: URL? {
        #if os(iOS)
        return FileManager.default.urls(for: .cachesDirectory, in: .userDomainMask)[0].appendingPathComponent("kept-presets.json")
        #else
        return nil
        #endif
    }

    public func item(_ id: String?) -> KeptPreset? { id.flatMap { i in items.first { $0.id == i } } }

    // MARK: reading

    /// The folder read again: each new or changed file read (a file that is not a FIZZ sound is left out).
    public func reload() async {
        if reloading { again = true; return }
        reloading = true
        defer { reloading = false }
        repeat {
            again = false
            await readFolder()
        } while again
        loaded = true
    }

    private func readFolder() async {
        try? fm.createDirectory(at: directory, withIntermediateDirectories: true)
        let keys: [URLResourceKey] = [.fileSizeKey, .contentModificationDateKey]
        let urls = (try? fm.contentsOfDirectory(at: directory, includingPropertiesForKeys: keys))?
            .filter { $0.pathExtension.lowercased() == "fizzsnd" } ?? []
        var out: [KeptPreset] = [], seen: [String: IndexEntry] = [:]
        for u in urls {
            let v = try? u.resourceValues(forKeys: Set(keys))
            let size = v?.fileSize ?? 0, date = v?.contentModificationDate ?? Date()
            let id = u.lastPathComponent
            var entry = cache[id]
            if entry == nil || entry!.size != size || abs(entry!.date.timeIntervalSince(date)) > 0.001 {
                guard let bytes = Self.read(u), let k = try? await FizzDataShared.engine().keptRead(bytes, fileName: id) else { continue }
                entry = IndexEntry(size: size, date: date, k)
            }
            guard let e = entry else { continue }
            seen[id] = e
            out.append(KeptPreset(id: id, url: u, sound: e.sound, webTags: e.webTags, size: size, modified: date))
        }
        let changed = seen != cache
        cache = seen
        items = Self.sorted(out)
        if changed { saveIndex() }
    }

    static func sorted(_ a: [KeptPreset]) -> [KeptPreset] {
        a.sorted { x, y in
            let c = x.name.localizedStandardCompare(y.name)
            return c == .orderedSame ? x.id.localizedStandardCompare(y.id) == .orderedAscending : c == .orderedAscending
        }
    }

    private static func read(_ u: URL) -> [UInt8]? {
        var data: Data?
        var err: NSError?
        NSFileCoordinator().coordinate(readingItemAt: u, options: .withoutChanges, error: &err) { x in data = try? Data(contentsOf: x) }
        return data.map { [UInt8]($0) }
    }

    /// A kept preset's whole file (its source too), read now.
    public func full(_ k: KeptPreset) async -> FizzKeptSound? {
        guard let b = Self.read(k.url) else { return nil }
        return try? await FizzDataShared.engine().keptRead(b, fileName: k.id)
    }

    /// The same sound (name and patch: js/library.js sameSound) kept already.
    public func same(as s: FizzSound) -> KeptPreset? { items.first { $0.name == s.name && $0.sound.blob == s.blob } }

    // MARK: watching

    /// The folder watched: a file added, removed or renamed outside the app (Files, AirDrop) read at once.
    public func watch() {
        guard watcher == nil else { return }
        try? fm.createDirectory(at: directory, withIntermediateDirectories: true)
        let fd = Darwin.open(directory.path, O_EVTONLY)
        guard fd >= 0 else { return }
        let s = DispatchSource.makeFileSystemObjectSource(fileDescriptor: fd, eventMask: [.write, .rename, .delete], queue: .main)
        s.setEventHandler { [weak self] in
            MainActor.assumeIsolated { self?.changed() }
        }
        s.setCancelHandler { Darwin.close(fd) }
        s.resume()
        watcher = s
    }

    public func unwatch() {
        watcher?.cancel()
        watcher = nil
    }

    private func changed() {
        pending?.cancel()
        pending = Task { [weak self] in
            try? await Task.sleep(for: .milliseconds(150))
            guard !Task.isCancelled else { return }
            await self?.reload()
        }
    }

    // MARK: writing

    /// A sound kept: its file written under a name not taken. The new card.
    @discardableResult public func add(_ s: FizzSound, webTags: [String] = [], source: FizzJSValue = [:]) async throws -> KeptPreset {
        try fm.createDirectory(at: directory, withIntermediateDirectories: true)
        let e = try FizzDataShared.engine()
        let bytes = try await e.keptFile(s, webTags: webTags, source: source)
        let u = try await free(for: s.name)
        try Data(bytes).write(to: u, options: .atomic)
        await reload()
        guard let k = item(u.lastPathComponent) else { throw CocoaError(.fileWriteUnknown) }
        return k
    }

    /// A file from outside (Open in FIZZ, AirDrop, IMPORT): read first (not a FIZZ sound: an error, nothing kept).
    @discardableResult public func open(_ url: URL) async throws -> KeptPreset {
        let scoped = url.startAccessingSecurityScopedResource()
        defer { if scoped { url.stopAccessingSecurityScopedResource() } }
        guard let b = Self.read(url) else { throw CocoaError(.fileReadUnknown) }
        let k = try await FizzDataShared.engine().keptRead(b, fileName: url.lastPathComponent)
        if url.deletingLastPathComponent().standardizedFileURL == directory.standardizedFileURL, let have = item(url.lastPathComponent) {
            return have
        }
        if let have = same(as: k.sound) { return have }
        return try await add(k.sound, webTags: k.webTags)
    }

    /// Written again (SAVE in EDIT, a rename, a tag): the file's source kept; a new name moves the file too.
    @discardableResult public func update(_ k: KeptPreset, _ change: (inout FizzKeptSound) -> Void) async throws -> KeptPreset {
        guard var f = await full(k) else { throw CocoaError(.fileReadNoSuchFile) }
        let old = f.sound.name
        change(&f)
        let e = try FizzDataShared.engine()
        let bytes = try await e.keptFile(f.sound, webTags: f.webTags)
        var u = k.url
        if f.sound.name != old {
            let want = try await e.keptFileName(f.sound.name)
            if want.lowercased() != k.id.lowercased() { u = try await free(for: f.sound.name) }
        }
        try Data(bytes).write(to: u, options: .atomic)
        if u != k.url { try? fm.removeItem(at: k.url) }
        cache[k.id] = nil
        await reload()
        return item(u.lastPathComponent) ?? k
    }

    public func rename(_ k: KeptPreset, to name: String) async throws -> KeptPreset {
        let n = Self.clean(name)
        guard !n.isEmpty, n != k.name else { return k }
        return try await update(k) { $0.sound.name = n }
    }

    /// One of FIZZ's seven tags on or off.
    public func toggle(_ k: KeptPreset, tag t: FizzTag) async throws -> KeptPreset {
        try await update(k) { f in
            var on = Set(f.sound.tags)
            if on.contains(t.name) { on.remove(t.name) } else { on.insert(t.name) }
            f.sound.tags = FizzTag.allCases.map(\.name).filter { on.contains($0) }
        }
    }

    /// A web tag added (WebTagStore.clean's rules) or taken off.
    public func addWebTag(_ k: KeptPreset, _ tag: String) async throws -> KeptPreset {
        let t = WebTagStore.clean(tag)
        guard !t.isEmpty, !FizzTag.allCases.map(\.name).contains(t), !k.webTags.contains(t) else { return k }
        return try await update(k) { $0.webTags.append(t) }
    }

    public func removeWebTag(_ k: KeptPreset, _ tag: String) async throws -> KeptPreset {
        try await update(k) { $0.webTags.removeAll { $0 == tag } }
    }

    /// DUPLICATE: a copy under a name not kept yet ("SUB DROP 3").
    @discardableResult public func duplicate(_ k: KeptPreset) async throws -> KeptPreset {
        guard var f = await full(k) else { throw CocoaError(.fileReadNoSuchFile) }
        f.sound.name = uniqueName(f.sound.name)
        return try await add(f.sound, webTags: f.webTags)
    }

    public func delete(_ ids: [String]) {
        for id in ids {
            guard let k = item(id) else { continue }
            try? fm.removeItem(at: k.url)
            cache[id] = nil
        }
        items.removeAll { ids.contains($0.id) }
        saveIndex()
    }

    /// Several as one .fizzlib in the temporary folder (SHARE).
    public func library(_ ks: [KeptPreset], fizz: String = "", date: Date = Date()) async throws -> URL {
        var entries: [FizzKeptSound] = []
        for k in ks { if let f = await full(k) { entries.append(f) } }
        let e = try FizzDataShared.engine()
        let bytes = try await e.keptLib(entries, fizz: fizz, date: date)
        let name = ks.count == 1 ? (ks[0].id as NSString).deletingPathExtension : "FIZZ kept \(Self.day(date))"
        let dir = fm.temporaryDirectory.appendingPathComponent("share-\(UUID().uuidString.prefix(8))", isDirectory: true)
        try fm.createDirectory(at: dir, withIntermediateDirectories: true)
        let u = dir.appendingPathComponent("\(name).fizzlib")
        try Data(bytes).write(to: u, options: .atomic)
        return u
    }

    // MARK: names

    /// A preset's name as FIZZ keeps it: upper case, printable ASCII, 12 at most.
    public static func clean(_ s: String) -> String {
        let up = String(s.uppercased().unicodeScalars.filter { $0.value >= 0x20 && $0.value <= 0x7e }.map(Character.init))
        return String(up.trimmingCharacters(in: .whitespaces).prefix(12)).trimmingCharacters(in: .whitespaces)
    }

    /// A name no kept preset has: NAME, else NAME 2, NAME 3 .. (a trailing number counted on; 12 characters at most).
    public func uniqueName(_ name: String) -> String {
        let taken = Set(items.map(\.name))
        guard taken.contains(name) else { return name }
        var base = name, n = 2
        if let m = name.range(of: " \\d+$", options: .regularExpression), let k = Int(name[m].dropFirst()) {
            base = String(name[..<m.lowerBound])
            n = k + 1
        }
        while true {
            let tail = " \(n)"
            let c = String(base.prefix(12 - tail.count)).trimmingCharacters(in: .whitespaces) + tail
            if !taken.contains(c) { return c }
            n += 1
        }
    }

    /// A file for `name` not taken on disk ("NAME.fizzsnd", "NAME 2.fizzsnd" ..).
    private func free(for name: String) async throws -> URL {
        let file = try await FizzDataShared.engine().keptFileName(name)
        let base = (file as NSString).deletingPathExtension
        var u = directory.appendingPathComponent(file)
        var n = 2
        while fm.fileExists(atPath: u.path) {
            u = directory.appendingPathComponent("\(base) \(n).fizzsnd")
            n += 1
        }
        return u
    }

    private static func day(_ d: Date) -> String {
        let f = DateFormatter()
        f.locale = Locale(identifier: "en_US_POSIX")
        f.dateFormat = "yyyy-MM-dd"
        return f.string(from: d)
    }

    private func saveIndex() {
        guard let indexURL else { return }
        try? fm.createDirectory(at: indexURL.deletingLastPathComponent(), withIntermediateDirectories: true)
        if let d = try? JSONEncoder().encode(cache) { try? d.write(to: indexURL, options: .atomic) }
    }

    /// A file's card as cached.
    struct IndexEntry: Codable, Equatable {
        var size: Int
        var date: Date
        var name: String
        var blob: [UInt8]
        var drive, pan, chorus, delay, reverb: Int
        var tags: [String]
        var favourite: Bool
        var webTags: [String]

        init(size: Int, date: Date, _ k: FizzKeptSound) {
            self.size = size; self.date = date
            let s = k.sound
            name = s.name; blob = s.blob; drive = s.drive; pan = s.pan
            chorus = s.sends.chorus; delay = s.sends.delay; reverb = s.sends.reverb
            tags = s.tags; favourite = s.favourite; webTags = k.webTags
        }

        var sound: FizzSound {
            FizzSound(name: name, blob: blob, drive: drive, pan: pan, sends: FizzSends(chorus: chorus, delay: delay, reverb: reverb),
                      tags: tags, favourite: favourite)
        }
    }
}
