// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// LIBRARY's SELECT on the unit's presets (mock-ups 41, 42, 47; manager.js "SELECT (FIZZ NEON)": renderBulk, bulkTag,
// bulkPool, bulkDelete): SELECT ticks MY PRESETS and FACTORY rows; a selection holds one side (the unit's, or KEPT's),
// so KEPT greys while the unit's rows are ticked. The ticked: TAG (one of FIZZ's seven, or NONE: it replaces theirs),
// + POOL (the ones not in it, each into the first free place), − POOL (every place holding them; the places projects'
// steps use asked about once), DELETE (MY PRESETS only, asked once: the factory's stay), EXPORT, and SEND n TO the
// other unit (→ KEPT when there is none: the web's KEEP n IN BROWSER). This file: the selection and the flows the
// phone's bar and the iPad's panel share; LibrarySelectViews.swift draws them.

import SwiftUI
import FizzData
import FizzKit
import Observation

// MARK: - The selection

/// A bulk edit running: what, how far.
public struct BulkProgress: Equatable, Sendable {
    public var label: String
    public var done: Int
    public var of: Int
}

/// SELECT on the unit's side (manager.js S.pick): on, the ticked rows ("mine:2", "factory:14"), the iPad's TAG row
/// open, a bulk edit's progress. The phone's tab bar gives way to its bar while it is on (RootView).
@MainActor @Observable
public final class LibraryPicks {
    public var on = false
    public private(set) var keys: Set<String> = []
    /// the iPad's TAG row shown in the panel (manager.js P.tags)
    public var tagsOpen = false
    public internal(set) var run: BulkProgress?

    public init() {}

    /// SELECT is on, or rows are ticked (manager.js renderBulk's `on`)
    public var active: Bool { on || !keys.isEmpty }

    public static func key(_ r: FizzListEntry) -> String { "\(r.source):\(r.slot)" }

    public func picked(_ r: FizzListEntry) -> Bool { keys.contains(Self.key(r)) }

    public func toggle(_ r: FizzListEntry) {
        guard r.source == "mine" || r.source == "factory" else { return }
        let k = Self.key(r)
        if keys.contains(k) { keys.remove(k) } else { keys.insert(k) }
    }

    /// SELECT: on, nothing ticked yet.
    public func start() {
        keys = []
        tagsOpen = false
        on = true
    }

    /// DONE (or KEPT's side taken): off, nothing ticked.
    public func clear() {
        on = false
        keys = []
        tagsOpen = false
    }

    /// ALL: every row shown ticked (all of them already: none), as KEPT's ALL.
    public func toggleAll(_ shown: [FizzListEntry]) {
        let all = Set(shown.filter { $0.source == "mine" || $0.source == "factory" }.map(Self.key))
        keys = keys.isSuperset(of: all) ? keys.subtracting(all) : keys.union(all)
    }

    /// The ticked rows still on the unit, in the list's order (manager.js pickedRows).
    public func rows(_ all: [FizzListEntry]) -> [FizzListEntry] {
        all.filter { $0.source != "pool" && keys.contains(Self.key($0)) }
    }

    /// Rows gone from the unit (deleted) untick.
    func forget(_ gone: [FizzListEntry]) { keys.subtract(gone.map(Self.key)) }

    /// (tests, fizzshots) these rows ticked
    public func set(_ ks: Set<String>) { keys = ks }
}

/// What the bulk bar counts (manager.js renderBulk): the ticked, the MY ones (DELETE), the unit's (TAG), in the pool
/// (− POOL) and not (+ POOL).
struct BulkCounts {
    let rows: [FizzListEntry]
    let mine: [FizzListEntry]
    let unit: [FizzListEntry]
    let inPool: [FizzListEntry]
    let outPool: [FizzListEntry]

    init(_ rows: [FizzListEntry], places: [Int: [Int]]) {
        self.rows = rows
        mine = rows.filter { $0.source == "mine" }
        unit = UnitActions.unitRows(rows)
        inPool = unit.filter { !(places[$0.id] ?? []).isEmpty }
        outPool = unit.filter { (places[$0.id] ?? []).isEmpty }
    }

    var n: Int { rows.count }
    /// the ticked rows' colours (the bar's rings: four at most)
    var tints: [RGB] { rows.prefix(4).map { FizzTag.tint(mask: $0.tags) } }
    /// how many of the ticked carry each tag (the TAG sheet's dots)
    func have(_ t: FizzTag) -> Int { unit.filter { ($0.tags >> t.rawValue) & 1 != 0 }.count }
    /// all of them carry only that tag already (its tile shown lit)
    func all(_ mask: Int) -> Bool { !unit.isEmpty && unit.allSatisfy { $0.tags & 0x7f == mask } }
}

/// The DELETE sheet's question (mock-up 42): the MY presets ticked, and how many FACTORY rows stay.
struct BulkDeleteAsk: Identifiable {
    let id = UUID()
    let mine: [FizzListEntry]
    let factory: Int
}

/// The TAG sheet open (the phone).
struct BulkTagAsk: Identifiable {
    let id = UUID()
}

/// The USED BY of the MY presets in a DELETE (USERS read): the used ones, each project with its steps, all the steps.
struct BulkUsage: Equatable {
    var used: [FizzListEntry] = []
    var projects: [(project: Int, steps: Int)] = []
    var steps = 0

    init(_ rows: [FizzListEntry], users: [String: FizzUsers]) {
        var per: [Int: Int] = [:]
        for r in rows {
            guard let u = users["mine:\(r.id)"], !u.projects.isEmpty else { continue }
            used.append(r)
            for p in u.projects { per[p.project, default: 0] += p.steps }
        }
        projects = per.keys.sorted().map { ($0, per[$0]!) }
        steps = per.values.reduce(0, +)
    }

    static func == (a: BulkUsage, b: BulkUsage) -> Bool {
        a.used == b.used && a.steps == b.steps && a.projects.map(\.project) == b.projects.map(\.project)
    }
}

// MARK: - The flows (the phone's bar and the iPad's panel)

/// What the bar's buttons do: the writes (UnitActions+Bulk.swift) with their progress, then a toast; DELETE and
/// − POOL's used places ask first.
@MainActor
struct BulkFlow {
    let model: UnitModel
    @Binding var note: ScreenNote?
    @Binding var ask: ConfirmAsk?
    @Binding var deleting: BulkDeleteAsk?
    @Binding var exporting: ExportBox?

    private var picks: LibraryPicks { model.picks }
    private var actions: UnitActions { model.actions }
    var rows: [FizzListEntry] { picks.rows(model.rows) }
    var counts: BulkCounts { BulkCounts(rows, places: poolPlaces(model.rows)) }
    /// a write may start (manager.js `can`)
    var can: Bool { actions.canWrite && picks.run == nil }

    private func progress(_ label: String) -> (Int, Int) -> Void {
        { [picks] d, of in picks.run = BulkProgress(label: label, done: d, of: of) }
    }

    /// The toast: what was done, the refusal that stopped the rest after it.
    static func toast(_ text: String, _ color: RGB, _ d: BulkDone) -> ScreenNote? {
        if let s = d.stopped, let bad = ScreenNote.of(s) {
            return d.n == 0 ? bad : ScreenNote(text: "\(text) · \(bad.text)", color: bad.color, kind: .refused)
        }
        return .done(text, color)
    }

    /// TAG: all the ticked get `tag` (nil: NONE).
    func tag(_ tag: FizzTag?) {
        let go = rows, mask = tag.map { 1 << $0.rawValue } ?? 0, n = UnitActions.unitRows(go).count
        picks.tagsOpen = false
        Task {
            let d = await actions.bulkTag(go, mask: mask, progress: progress("TAG"))
            picks.run = nil
            note = Self.toast("\(d.stopped == nil ? n : d.n) · \(tag?.name ?? "NONE")", tag?.tint ?? FZ.muted, d)
        }
    }

    func poolAdd() {
        let go = rows
        Task {
            let d = await actions.bulkPoolAdd(go, progress: progress("+ POOL"))
            picks.run = nil
            note = Self.toast("\(d.n) → POOL", FZ.green, d)
        }
    }

    /// − POOL: the places cleared; those projects' steps use asked about once (manager.js bulkPool).
    func poolRemove() {
        let go = rows
        Task {
            let (d, used) = await actions.bulkPoolRemove(go, progress: progress("− POOL"))
            picks.run = nil
            if !used.isEmpty {
                let steps = used.reduce(0) { $0 + $1.steps }
                var names: [String] = []
                for u in used where !names.contains(u.row.name) { names.append(u.row.name) }
                ask = ConfirmAsk(title: "CLEAR \(used.count) POOL PLACE\(used.count == 1 ? "" : "S")",
                                 what: used.count <= 3 ? names.joined(separator: ", ") : "\(used.count) PLACES",
                                 detail: stepsText(steps), go: "CLEAR", color: FZ.red) {
                    Task {
                        let c = await actions.bulkPoolClear(used, progress: progress("− POOL"))
                        picks.run = nil
                        note = Self.toast("\(d.n + c.n) OUT OF THE POOL", FZ.muted, BulkDone(n: d.n + c.n, stopped: c.stopped))
                    }
                }
            } else if d.n > 0 || d.stopped != nil {
                note = Self.toast("\(d.n) OUT OF THE POOL", FZ.muted, d)
            }
        }
    }

    /// DELETE: the sheet (mock-up 42) asks once; nothing when no MY preset is ticked.
    func askDelete() {
        let go = rows, mine = go.filter { $0.source == "mine" }
        guard !mine.isEmpty else { return }
        deleting = BulkDeleteAsk(mine: mine, factory: go.count - mine.count)
    }

    func delete(_ a: BulkDeleteAsk) {
        Task {
            let d = await actions.bulkDelete(a.mine, progress: progress("DELETE"))
            picks.run = nil
            picks.forget(a.mine.filter { r in !model.rows.contains { $0.source == "mine" && $0.slot == r.slot && $0.id == r.id } })
            note = Self.toast("DELETED \(d.n)", FZ.red, d)
        }
    }

    func export() {
        let go = rows
        Task {
            let s = await actions.exportSounds(go)
            if s.isEmpty { note = ScreenNote(text: "NOT READ YET", color: FZ.muted, kind: .refused) } else { exporting = ExportBox(sounds: s) }
        }
    }

    /// SEND n TO the other unit (into its MY PRESETS), or → KEPT with no other unit (the web's KEEP n IN BROWSER).
    func send() {
        let go = rows
        if let o = model.other {
            Task {
                picks.run = BulkProgress(label: "SEND", done: 0, of: go.count)
                let out = await model.sendToOther(rows: go)
                picks.run = nil
                note = keptSendNote(out, to: .mine, unit: o.name)
            }
        } else {
            Task {
                var kept = 0, already = 0
                for (i, r) in go.enumerated() {
                    picks.run = BulkProgress(label: "KEPT", done: i, of: go.count)
                    switch await model.keep(r) {
                    case .kept: kept += 1
                    case .already: already += 1
                    case .failed: break
                    }
                }
                picks.run = nil
                note = kept + already == 0 ? ScreenNote(text: "NOT READ YET", color: FZ.muted, kind: .refused)
                    : .done("→ KEPT · \(kept)\(already > 0 ? " · \(already) ALREADY" : "")", FZ.champagne)
            }
        }
    }
}

extension UnitModel {
    /// The phone's tab bar gives way to a SELECT bar (KEPT's or the unit's) on LIBRARY.
    var libraryBarUp: Bool { kept.selecting || picks.active }
}
