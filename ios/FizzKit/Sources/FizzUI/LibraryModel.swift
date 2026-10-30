// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// LIBRARY's and POOL's logic (phase I3): the filters (presets.js newFilter / shownRows), the web tags (manager.js
// S.web: free text a sound keeps on this phone and in its backups, keyed by fizzbak.js soundKey), the few actions the
// screens need beyond UnitActions (LOAD ON FM-1 on a chosen track, RENAME keeping the web tags, EXPORT's sounds), and
// a write's outcome as a short coloured screen state (PLAN §4.6).

import Foundation
import FizzData
import FizzKit
import Observation

// MARK: - Filters

/// presets.js newFilter: which presets (ALL / MY PRESETS / FACTORY), in the pool or not, the tags (any of them), the
/// search (q: a part of the name). ALL first: LIBRARY opens on it (the owner's ask, 2026-10-29).
struct LibraryFilter: Equatable {
    enum Source: Int, CaseIterable { case all, mine, factory }
    enum Pool: Int { case any, inPool, notInPool }
    var source: Source = .all
    var tags: Set<FizzTag> = []
    var pool: Pool = .any
    /// the search as typed (NameSearch reads it)
    var query = ""

    /// presets.js tagOk, poolOk and nameOk (places: poolPlaces of the rows).
    func ok(_ r: FizzListEntry, places: [Int: [Int]]) -> Bool {
        let tagOK = tags.isEmpty || FizzTag.of(mask: r.tags).contains { tags.contains($0) }
        let inPool = !(places[r.id] ?? []).isEmpty
        let poolOK = pool == .any || (pool == .inPool) == inPool
        return tagOK && poolOK && NameSearch.matches(r.name, query)
    }

    /// presets.js shownRows: MY PRESETS' and the factory's rows that pass.
    func shown(_ rows: [FizzListEntry]) -> (mine: [FizzListEntry], factory: [FizzListEntry]) {
        let pl = poolPlaces(rows)
        let mine = source == .factory ? [] : rows.filter { $0.source == "mine" && ok($0, places: pl) }
        let fac = source == .mine ? [] : rows.filter { $0.source == "factory" && ok($0, places: pl) }
        return (mine, fac)
    }

    /// The pool chip turned: off, IN POOL, NOT IN POOL, off.
    mutating func turnPool() { pool = Pool(rawValue: (pool.rawValue + 1) % 3) ?? .any }

    mutating func toggle(_ t: FizzTag) { if tags.contains(t) { tags.remove(t) } else { tags.insert(t) } }
}

/// LIBRARY's sources as the phone's switch shows them: ALL, MY PRESETS, FACTORY (the unit's), then the phone's KEPT
/// last. LIBRARY opens on ALL whichever the unit is (an FM-1 or THIS iPHONE); the side last shown is kept while the
/// app runs, not across launches (an earlier build remembered KEPT for good and forced it with no FM-1, so LIBRARY
/// always opened on KEPT and the unit's SELECT never showed).
enum LibrarySide: Hashable, CaseIterable {
    case all, mine, factory, kept

    var title: String {
        switch self {
        case .all: "ALL"
        case .mine: "MY PRESETS"
        case .factory: "FACTORY"
        case .kept: "KEPT"
        }
    }

    /// the unit's filter for this side (KEPT: nil)
    var source: LibraryFilter.Source? {
        switch self {
        case .all: .all
        case .mine: .mine
        case .factory: .factory
        case .kept: nil
        }
    }

    init(kept: Bool, source: LibraryFilter.Source) {
        if kept { self = .kept } else {
            switch source {
            case .all: self = .all
            case .mine: self = .mine
            case .factory: self = .factory
            }
        }
    }

    /// LIBRARY's first side
    static let opening: LibrarySide = .all

    /// The side shown while the app runs (`-kept`: KEPT, for the simulator's screenshots).
    @MainActor static var shown: LibrarySide = ProcessInfo.processInfo.arguments.contains("-kept") ? .kept : opening
}

extension UnitModel {
    /// SELECT can start on this side: the unit's sides once a unit answers (an FM-1 or THIS iPHONE alike), KEPT's
    /// always (the phone's own presets).
    func canSelect(on side: LibrarySide) -> Bool { side == .kept || unit != nil }
}

/// LIBRARY's search (manager.js l-q, presets.js nameOk: the query trimmed, a part of the name, any case), also blind
/// to accents here (a KEPT name typed on the phone may carry them; the unit's are ASCII).
enum NameSearch {
    /// The query as matched: trimmed; empty matches everything.
    static func clean(_ q: String) -> String { q.trimmingCharacters(in: .whitespacesAndNewlines) }

    static func matches(_ name: String, _ query: String) -> Bool {
        let q = clean(query)
        return q.isEmpty || name.range(of: q, options: [.caseInsensitive, .diacriticInsensitive, .widthInsensitive]) != nil
    }
}

/// presets.js poolPlaces: preset id -> the pool places referencing it (ascending).
func poolPlaces(_ rows: [FizzListEntry]) -> [Int: [Int]] {
    var m: [Int: [Int]] = [:]
    for r in rows where r.source == "pool" && r.id != 0 { m[r.id, default: []].append(r.slot) }
    return m.mapValues { $0.sorted() }
}

// MARK: - Web tags

/// The web tags (manager.js S.web.tags): sound key -> its free-text tags, kept in Documents/webtags.json and written
/// into backups (makeUnitFizzbak's webTags).
@MainActor @Observable
public final class WebTagStore {
    public private(set) var tags: [String: [String]] = [:]
    @ObservationIgnored let url: URL

    /// (read at once: a small file)
    public init(url: URL) {
        self.url = url
        if let d = try? Data(contentsOf: url), let t = try? JSONDecoder().decode([String: [String]].self, from: d) { tags = t }
    }

    /// Documents/webtags.json on the phone; beside the backups' folder elsewhere.
    public static func url(besides backups: URL) -> URL {
        backups.deletingLastPathComponent().appendingPathComponent("webtags.json")
    }

    /// Every sound's (a backup takes them)
    public var all: [String: [String]] { tags }

    public func of(_ key: String?) -> [String] { key.flatMap { tags[$0] } ?? [] }

    /// manager.js l-addtag: upper case, spaces as one, 16 at most; not one of FIZZ's tags, not twice.
    public static func clean(_ s: String) -> String {
        let t = s.trimmingCharacters(in: .whitespacesAndNewlines).uppercased()
            .replacingOccurrences(of: "\\s+", with: " ", options: .regularExpression)
        return String(t.prefix(16))
    }

    @discardableResult public func add(_ tag: String, to key: String) -> Bool {
        let t = Self.clean(tag), have = tags[key] ?? []
        guard !t.isEmpty, !FizzTag.allCases.map(\.name).contains(t), !have.contains(t) else { return false }
        tags[key] = have + [t]
        save()
        return true
    }

    public func remove(_ tag: String, from key: String) {
        let left = (tags[key] ?? []).filter { $0 != tag }
        tags[key] = left.isEmpty ? nil : left
        save()
    }

    /// A renamed sound's tags go with it (its key changes with its name).
    public func move(from old: String, to new: String) {
        guard old != new, let t = tags[old] else { return }
        tags[new] = t
        tags[old] = nil
        save()
    }

    private func save() {
        try? FileManager.default.createDirectory(at: url.deletingLastPathComponent(), withIntermediateDirectories: true)
        if let d = try? JSONEncoder().encode(tags) { try? d.write(to: url, options: .atomic) }
    }
}

// MARK: - A write's outcome on screen

/// A short coloured screen state (PLAN §4.6): what came of a write, in a few words.
struct ScreenNote: Equatable, Identifiable {
    enum Kind { case done, refused, note }
    let id = UUID()
    var text: String
    var color: RGB
    var kind: Kind = .done

    static func done(_ s: String, _ c: RGB = FZ.green) -> ScreenNote { ScreenNote(text: s, color: c, kind: .done) }

    /// A refusal's state (ui.js refusal, short); nil when the outcome is OK or asks for a confirm.
    static func of(_ o: UnitOutcome) -> ScreenNote? {
        func no(_ s: String, _ c: RGB) -> ScreenNote { ScreenNote(text: s, color: c, kind: .refused) }
        switch o {
        case .ok, .placed, .needsConfirm: return nil
        case .noAnswer: return no("NO ANSWER", FZ.red)
        case .notNow: return no("NOT NOW", FZ.muted)
        case .refused(let r):
            switch r {
            case .busy: return no("FM-1 BUSY · STOP IT", FZ.orange)
            case .full: return no("FULL", FZ.red)
            case .notFound: return no("NOT ON THE FM-1", FZ.muted)
            case .updateFizz: return no("UPDATE FIZZ", FZ.violet)
            case .badSlot: return no("READ ONLY", FZ.muted)
            default: return no("REFUSED · \(r.name)", FZ.red)
            }
        }
    }

    static func == (a: ScreenNote, b: ScreenNote) -> Bool { a.id == b.id }
}

/// A confirm (ui.js confirmBox): what, the button, its colour, and what it does to the unit's undo.
struct ConfirmAsk: Identifiable {
    enum Undo { case kept, cleared }
    let id = UUID()
    var title: String
    var what: String
    var detail: String?
    var go: String
    var color: RGB = FZ.red
    var undo: Undo = .kept
    var action: @MainActor () -> Void
}

// MARK: - Actions the screens need

extension UnitActions {
    /// LOAD ON FM-1 onto track `track` (1..6), the followed track (trackSlot) left as it is.
    public func loadOnUnit(_ r: FizzListEntry, track: Int) async -> UnitOutcome {
        let keep = trackSlot
        trackSlot = max(0, min(6, track))
        defer { trackSlot = keep }
        return await loadOnUnit(r)
    }

    /// RENAME, the web tags moved to the sound's new key (manager.js l-name).
    public func rename(_ r: FizzListEntry, to name: String, webTags: WebTagStore) async -> UnitOutcome {
        let old = await soundKey(of: r)
        let o = await rename(r, to: name)
        guard o.isOK, let old, let n = unit?.mine.first(where: { $0.slot == r.slot }), let new = await soundKey(of: n) else { return o }
        webTags.move(from: old, to: new)
        return o
    }

    /// A preset's sound (its DUMP as js/library.js's sound), nil when its DUMP is not here.
    public func sound(of r: FizzListEntry) async -> FizzSound? {
        guard let f = await fetchFrame(of: r), let e = try? FizzDataShared.engine() else { return nil }
        return try? await e.soundFromStoreFrame(f)
    }

    /// The web tags' key of a preset (fizzbak.js soundKey of its sound).
    public func soundKey(of r: FizzListEntry) async -> String? {
        guard let s = await sound(of: r), let e = try? FizzDataShared.engine() else { return nil }
        return try? await e.soundKey(s)
    }

    /// EXPORT's sounds (manager.js l-export, l-export-all): each preset's, with where it came from.
    public func exportSounds(_ rows: [FizzListEntry]) async -> [FizzSound] {
        var out: [FizzSound] = []
        let fizz = unit?.unit?.version ?? ""
        for r in rows {
            guard var s = await sound(of: r) else { continue }
            s.source = ["fizz": .string(fizz), "from": .string("\(r.source == "mine" ? "MY PRESETS" : "FACTORY") \(r.slot)")]
            out.append(s)
        }
        return out
    }

    /// The steps that lock a pool place (USERS, read before), nil when not read yet.
    func poolSteps(_ k: Int) -> Int? {
        users["pool:\(k)"].map { $0.projects.reduce(0) { $0 + $1.steps } }
    }
}
