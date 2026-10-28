// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// The app's writes (ios/PLAN.md §4.3, "Write layer"): each action is the web Manager's (manager.js renderSel's
// handlers, swapMine, saveSound, addPresets, restoreImage / writeStore; pool.js; projtab.js; importer.js send;
// live.js), on FizzLink: the write, then what the page reads again (LIST until it shows the edit, STATUS, PROJECTS,
// a preset's DUMP), UnitModel updated, a LOG line, and a small outcome for the screen (done, refused, needs a confirm,
// no answer). One edit at a time from the screens' side (the link's queue serialises them anyway); nothing while a
// backup, a restore or a transfer runs. RESTORE and WRITE TO FM-1 keep a safety .fizzbak first, as the page does.

import Foundation
import FizzData
import FizzKit
import Observation

/// What a write came to.
public enum UnitOutcome: Sendable, Equatable {
    case ok
    /// done: the place or slot the unit gave (POOL REF, STORE, a copy's target), or a count (CLEAR UNUSED)
    case placed(Int)
    /// FIZZ said no (BUSY: playing or a job; FULL; NOT FOUND; updateFizz: BAD COMMAND ..)
    case refused(FizzRefusal)
    /// IN USE: steps use it; ask, then call again confirmed (the place it was about, for the pool)
    case needsConfirm(place: Int, projects: Int, steps: Int)
    /// the FM-1 did not answer (or went)
    case noAnswer
    /// not now: no unit, or a backup, restore or transfer runs
    case notNow

    public var isOK: Bool {
        switch self {
        case .ok, .placed: true
        default: false
        }
    }

    /// ui.js refusal's words (the LOG; the screens show a short state).
    public func text(_ what: String = "") -> String {
        switch self {
        case .ok, .placed: "OK"
        case .noAnswer: "The FM-1 did not answer."
        case .notNow: "Not now."
        case .needsConfirm(_, _, let steps): "\(steps) step\(steps == 1 ? "" : "s") use it: they go back to their track's sound."
        case .refused(let r):
            switch r {
            case .updateFizz: "This FIZZ cannot do that yet: update FIZZ."
            case .busy: "The FM-1 is playing (or still busy): stop it, then \(what.isEmpty ? "try again" : what)."
            case .notFound: "That is not on the FM-1 any more."
            case .full: "Full."
            case .badSlot: "Not there: read only."
            default: "The FM-1 refused it (\(r.name))."
            }
        }
    }
}

/// RESTORE / WRITE TO FM-1 on the progress screen.
public struct RestoreRun: Sendable, Equatable {
    public enum Phase: String, Sendable { case saving, checking, writing }
    /// saving: the safety backup (BackupModel.run draws it); checking: nothing changed yet; writing: DO NOT UNPLUG
    public var phase: Phase
    /// both passes, 0..1
    public var fraction: Double
    public var name: String
    public var started: Date
    public init(phase: Phase, fraction: Double, name: String, started: Date) {
        self.phase = phase; self.fraction = fraction; self.name = name; self.started = started
    }
    /// CANCEL offered (saving and checking only)
    public var canCancel: Bool { phase != .writing }
}

/// A RESTORE that did not end well: a short state, as manager.js restoreImage says it.
public enum RestoreFailure: Sendable, Equatable {
    case notSavedFirst          // the safety backup failed: nothing was restored
    case cancelled              // nothing changed
    case playing                // FZER 10: stop the FM-1
    case otherSize              // FZER 11: its presets can still be added
    case needsUSB               // FZER 15
    case notWhole               // FZER 12: nothing changed
    case notTakenUp             // no answer before writing: nothing changed, try again
    case cleared(String)        // 13, 14, unplugged or silent while writing: FIZZ cleared its store; restore the newest
    case other(String)

    public var text: String {
        switch self {
        case .notSavedFirst: "NOT SAVED FIRST"
        case .cancelled: "NOTHING CHANGED"
        case .playing: "STOP THE FM-1"
        case .otherSize: "ANOTHER STORE SIZE"
        case .needsUSB: "USB CABLE NEEDED"
        case .notWhole: "NOT WHOLE · NOTHING CHANGED"
        case .notTakenUp: "NOT TAKEN UP · NOTHING CHANGED"
        case .cleared: "STORE CLEARED · RESTORE AGAIN"
        case .other: "DID NOT FINISH"
        }
    }

    /// The FM-1's store is as it was.
    public var nothingChanged: Bool {
        switch self {
        case .cleared, .other: false
        default: true
        }
    }

    static func of(_ e: Error) -> RestoreFailure {
        guard let r = e as? FizzRestoreError else { return .other("\(e)") }
        switch r {
        case .cancelled: return .cancelled
        case .refused(10, _): return .playing
        case .refused(11, _): return .otherSize
        case .refused(15, _): return .needsUSB
        case .refused(12, _): return .notWhole
        case .timeout(false): return .notTakenUp
        case .refused(13, _), .refused(14, _), .lost, .timeout(true): return .cleared(r.text)
        default: return .other(r.text)
        }
    }
}

/// LOAD ON FM-1: the unit's own browser open on a preset (OCT- on the unit cancels, OCT+ loads).
public struct Browsing: Sendable, Equatable {
    public let id: Int, name: String
    /// the track it previews on, 0..5
    public let track: Int
    public let started: Date
}

/// IMPORT's SEND: where the sounds go.
public enum SendDestination: Sendable, Equatable {
    case mine
    case pool                   // MY PRESETS and the pool
    case track(Int)             // MY PRESETS, the first one onto this track (0..5) too
}

/// ADD presets' count.
public struct AddResult: Sendable, Equatable {
    public var added = 0, same = 0
    public var full = false
    public var outcome: UnitOutcome = .ok
}

@MainActor @Observable
public final class UnitActions {
    /// Edits running (the page's S.editing): the read-back loops wait.
    public private(set) var editing = 0
    /// A restore, ADD, SEND or pool sort running (the page's S.busy): the other writes wait.
    public private(set) var busy = false
    /// A long write's count (SORT BY TAG, CLEAR UNUSED, ADD, SEND): done, of.
    public private(set) var job: (done: Int, of: Int)?
    /// The progress screen of a RESTORE / WRITE TO FM-1, and how the last one failed.
    public private(set) var restoreRun: RestoreRun?
    public private(set) var restoreFailure: RestoreFailure?
    /// Each preset's DUMP: "mine:12", "factory:3" (from connect, and read again after an edit).
    public private(set) var frames: [String: [UInt8]] = [:]
    /// USED BY: "mine:<id>", "pool:<place>" (cleared when the lists are read again).
    public private(set) var users: [String: FizzUsers] = [:]
    /// LOAD ON FM-1 while the unit's browser is open; then what came of it (true: the track loaded it).
    public private(set) var browsing: Browsing?
    public private(set) var browseLoaded: Bool?
    /// The TRACK switch: this FIZZ edits a track live (its track DUMP answered), and on.
    public private(set) var trackCapable = false
    public private(set) var trackOn = false
    /// The track followed: 0 the selected one, 1..6 (live.js L.track).
    public var trackSlot = 0
    /// The KEEP / CANCEL bar: the track's own sound kept while another is heard on it.
    public private(set) var pending: TrackAudition.Pending?

    @ObservationIgnored weak var unit: UnitModel?
    @ObservationIgnored private var oneOff: [String: [UInt8]] = [:]
    @ObservationIgnored private var liveLink: FizzLink?
    @ObservationIgnored private var _pump: ParamPump?
    @ObservationIgnored private var _audition: TrackAudition?
    @ObservationIgnored private var restoreTask: Task<FizzRestoreDone, Error>?
    @ObservationIgnored private var browseWatch: Task<Void, Never>?
    @ObservationIgnored private let awake = KeepAwake()

    init() {}

    // MARK: state

    var link: FizzLink? { unit?.link }
    /// No write now: no unit, or a backup, restore or transfer running.
    public var canWrite: Bool { link != nil && !busy && !(unit?.backups.busy ?? false) && unit?.status != nil }

    func say(_ s: String) { unit?.say(s) }

    /// A row's DUMP if read (from connect, an edit, or fetchFrame).
    public func frame(of r: FizzListEntry) -> [UInt8]? {
        frames["\(r.source):\(r.slot)"] ?? oneOff["\(r.source):\(r.slot):\(r.id)"]
    }

    /// A row's DUMP, read by itself when it is not here yet (manager.js fetchOne).
    public func fetchFrame(of r: FizzListEntry) async -> [UInt8]? {
        if let f = frame(of: r) { return f }
        guard let link, let src = FizzSource(name: r.source), unit?.unit?.version != nil else { return nil }
        guard let f = (try? await link.dump(source: src, slot: r.slot)) ?? nil else { return nil }
        oneOff["\(r.source):\(r.slot):\(r.id)"] = f
        return f
    }

    /// The pool places that reference a preset (presets.js poolPlaces).
    public func places(of id: Int) -> [Int] {
        (unit?.pool ?? []).filter { $0.id == id }.map(\.slot).sorted()
    }

    func dumpsRead(mine: [[UInt8]], factory: [[UInt8]]) {
        var f: [String: [UInt8]] = [:]
        for x in mine { if let d = FizzSysEx.parseDump(x) { f["mine:\(d.slot)"] = x } }
        for (k, x) in factory.enumerated() { if let d = FizzSysEx.parseDump(x) { f["factory:\(d.slot > 0 ? d.slot : k + 1)"] = x } }
        frames = f
        if unit?.status != nil { Task { await probeTrack() } }
    }

    func lost() {
        frames = [:]; oneOff = [:]; users = [:]
        browseWatch?.cancel(); browseWatch = nil; browsing = nil
        pending = nil; trackOn = false; trackCapable = false
        liveLink = nil; _pump = nil; _audition = nil
        job = nil
    }

    // MARK: the page's edit() and re-reads

    /// An edit's answer: FIZZ's (nil: none) and what the screen shows.
    struct Answer<V: Sendable & Equatable> {
        var result: FizzResult<V>?
        var outcome: UnitOutcome
        var value: V? { result?.value }
        var isOK: Bool { result?.isOK == true }
    }

    /// One edit (manager.js edit): not now without a unit or while a transfer runs; a refusal but IN USE said in the
    /// LOG; the FM-1 silent or gone: noAnswer.
    private func perform<V: Sendable & Equatable>(_ what: String, place: Int = 0,
                                                  _ f: (FizzLink) async throws -> FizzResult<V>) async -> Answer<V> {
        guard canWrite, let link else { return Answer(result: nil, outcome: .notNow) }
        editing += 1
        defer { editing -= 1 }
        do {
            let r = try await f(link)
            let o = Self.outcome(r, place: place)
            if case .refused = o { say("\(what): \(o.text(what))") }
            return Answer(result: r, outcome: o)
        } catch {
            say("\(what): \(error)")
            return Answer(result: nil, outcome: .noAnswer)
        }
    }

    static func outcome<V>(_ r: FizzResult<V>, place: Int = 0) -> UnitOutcome {
        switch r {
        case .ok: .ok
        case .refused(.inUse(let p, let s)): .needsConfirm(place: place, projects: p, steps: s)
        case .refused(let x): .refused(x)
        }
    }

    /// RESTORE and the long writes: a unit, nothing else running (STATUS not needed: a FIZZ in safe mode restores).
    var canTransfer: Bool { link != nil && !busy && editing == 0 && !(unit?.backups.busy ?? false) }

    /// The unit's lists again (manager.js refreshRows): LIST (until it shows what the edit made, 4 tries 0.3 s apart;
    /// one that failed keeps the rows), STATUS, PROJECTS; USED BY forgotten.
    public func refreshRows(projects: Bool = false, until: (([FizzListEntry]) -> Bool)? = nil) async {
        guard let u = unit, let link, u.unit?.version != nil else { return }
        for k in 0..<4 {
            let rows = try? await link.list(factory: true, pool: true)
            if let rows, until == nil || until!(rows) || k == 3 {
                if let until, !until(rows) { say("refresh: the FM-1's lists still differ from the edit: the FM-1's kept") }
                u.took(rows: rows)
                break
            }
            try? await Task.sleep(for: .milliseconds(300))
        }
        u.took(status: await link.status())
        if projects || u.projects == nil { u.took(projects: await link.projects()) }
        users = [:]
        oneOff = [:]
    }

    /// A MY preset's DUMP again (its name, tags or place changed: manager.js redump), its glyph too.
    func redump(_ slot: Int) async {
        guard unit?.dumpsLoaded == true, let link else { return }
        let f = (try? await link.dump(source: .mine, slot: slot)) ?? nil
        frames["mine:\(slot)"] = f
        let a = f.flatMap(FizzSysEx.parseDump).flatMap { $0.blob.count > 2 ? Int($0.blob[2]) + 1 : nil }
        unit?.took(algorithm: a, for: "mine:\(slot)")
    }

    /// A pool place shown changed at once (manager.js poolSet): the preset now there, or nil.
    func poolSet(_ slot: Int, _ r: FizzListEntry?) {
        guard let u = unit else { return }
        var rows = u.rows.filter { !($0.source == "pool" && $0.slot == slot) }
        if let r { rows.append(FizzListEntry(source: "pool", slot: slot, id: r.id, name: r.name, tags: r.tags, imported: false, inPool: true)) }
        u.took(rows: rows)
        users = users.filter { !$0.key.hasPrefix("pool:") }
    }

    private func preset(id: Int) -> FizzListEntry? { unit?.rows.first { $0.source != "pool" && $0.id == id } }

    // MARK: LIBRARY

    /// RENAME a MY preset (the name cleaned as the page's field does); its DUMP and the lists read again.
    public func rename(_ r: FizzListEntry, to name: String) async -> UnitOutcome {
        let v = FizzSysEx.presetName(name)
        guard r.source == "mine", !v.isEmpty, v != r.name else { return .ok }
        let x = await perform("RENAME") { try await $0.rename(source: .mine, slot: r.slot, name: v) }
        if x.isOK {
            await redump(r.slot)
            say("renamed MY PRESETS \(r.slot): \(v)")
            await refreshRows()
        }
        return x.outcome
    }

    /// TAGS of a MY or factory preset (FIZZ's seven: a mask); the row and its DUMP updated.
    public func setTags(_ r: FizzListEntry, mask: Int) async -> UnitOutcome {
        guard let src = FizzSource(name: r.source), src == .mine || src == .factory else { return .notNow }
        let x = await perform("TAGS") { try await $0.tags(source: src, slot: r.slot, tags: .mask(mask)) }
        if x.isOK, let u = unit {
            u.took(rows: u.rows.map { $0.source == r.source && $0.slot == r.slot
                ? FizzListEntry(source: $0.source, slot: $0.slot, id: $0.id, name: $0.name, tags: mask & 0x7f,
                                imported: $0.imported, inPool: $0.inPool) : $0 })
            if src == .mine { await redump(r.slot) }
            else if let f = (try? await link?.dump(source: .factory, slot: r.slot)) ?? nil { frames["factory:\(r.slot)"] = f }
        }
        return x.outcome
    }

    /// One tag (0..6, FizzSysEx.fizzTags) turned on or off.
    public func toggleTag(_ r: FizzListEntry, _ k: Int) async -> UnitOutcome {
        await setTags(r, mask: r.tags ^ (1 << k))
    }

    /// DELETE a MY preset (the screen confirmed first: sent with CONFIRM, as the page does).
    public func delete(_ r: FizzListEntry) async -> UnitOutcome {
        guard r.source == "mine" else { return .notNow }
        let x = await perform("DELETE") { try await $0.remove(source: .mine, slot: r.slot, confirm: true) }
        if x.isOK {
            frames["mine:\(r.slot)"] = nil
            say("deleted MY PRESETS \(r.slot) \(r.name)")
            await refreshRows()
        }
        return x.outcome
    }

    /// Reorder: MY PRESETS a and b swap places (into an empty one: a move); both DUMPs and the lists read again.
    public func moveMine(_ a: Int, to b: Int) async -> UnitOutcome {
        let n = unit?.status?.nlib ?? 256
        guard a != b, b >= 1, b <= n else { return .ok }
        let x = await perform("MOVE") { try await $0.swap(source: .mine, a: a, b: b) }
        if x.isOK {
            await redump(a)
            await redump(b)
            await refreshRows()
        }
        return x.outcome
    }

    /// + POOL: the first free place references the preset (.placed(the place)).
    public func addToPool(_ r: FizzListEntry) async -> UnitOutcome {
        let x = await perform("+ POOL") { try await $0.poolRef(slot: 0, id: r.id) }
        guard let k = x.value?.a else { return x.outcome }
        poolSet(k, r)
        say("\(r.name) -> POOL \(k)")
        await refreshRows(until: { $0.contains { $0.source == "pool" && $0.slot == k && $0.id == r.id } })
        return .placed(k)
    }

    /// − POOL: every place that references the preset cleared, without CONFIRM first; a used one answers
    /// .needsConfirm(place:): ask, then call again with that place in `confirmed`.
    public func removeFromPool(_ r: FizzListEntry, confirmed: Set<Int> = []) async -> UnitOutcome {
        let pl = places(of: r.id)
        var n = 0
        var result = UnitOutcome.ok
        for k in pl {
            let x = await perform("- POOL", place: k) { try await $0.remove(source: .pool, slot: k, confirm: confirmed.contains(k)) }
            if x.isOK { n += 1; poolSet(k, nil); continue }
            result = x.outcome
            break
        }
        if n > 0 { say("\(r.name) out of the pool") }
        await refreshRows(until: { !$0.contains { $0.source == "pool" && $0.id == r.id && pl.contains($0.slot) } })
        return result
    }

    /// USED BY a MY preset (USERS: a job on the unit, a moment), kept until the lists are read again.
    public func usedBy(_ r: FizzListEntry) async -> FizzUsers? {
        let key = "mine:\(r.id)"
        if let u = users[key] { return u }
        guard canWrite, let link, let u = (try? await link.usedBy(source: .mine, slot: r.slot))?.value else { return nil }
        users[key] = u
        return u
    }

    /// LOAD ON FM-1: the unit's own sound browser opened on the preset (a pending audition cancelled first); watched
    /// until it closes (`browsing`, then `browseLoaded`).
    public func loadOnUnit(_ r: FizzListEntry) async -> UnitOutcome {
        guard canWrite, let link, r.id != 0 else { return .notNow }
        if pending != nil { _ = await cancelAudition() }
        let tr: FizzTrack = trackSlot == 0 ? .selected : .index(trackSlot - 1)
        let x = await perform("LOAD ON FM-1") { try await $0.browse(track: tr, id: r.id) }
        guard let a = x.value else {
            if x.result?.refusal == .busy { say("A name is being typed on the FM-1: finish it there, then LOAD ON FM-1 again.") }
            return x.outcome
        }
        let b = Browsing(id: r.id, name: r.name, track: a.a - 1, started: Date())
        browsing = b
        browseLoaded = nil
        say("LOAD ON FM-1: \(r.name) in the FM-1's browser on TRACK \(a.a) (OCT- cancels, OCT+ loads)")
        browseWatch?.cancel()
        browseWatch = Task { [weak self] in await self?.watchBrowse(b, link) }
        return .ok
    }

    /// STATUS every 0.6 s until the unit's browser closes (3 minutes at most), then the track says which.
    private func watchBrowse(_ b: Browsing, _ link: FizzLink) async {
        while !Task.isCancelled {
            try? await Task.sleep(for: .milliseconds(600))
            guard browsing == b, !Task.isCancelled else { return }
            if Date().timeIntervalSince(b.started) > 180 { browsing = nil; return }
            if busy || editing > 0 { continue }
            guard let st = await link.status(), !st.browse else { continue }
            let f = (try? await link.dumpTrack(slot: b.track + 1)) ?? nil
            guard browsing == b else { return }
            let loaded = f.flatMap(FizzSysEx.parseDump)?.extras?.id == b.id
            browsing = nil
            browseLoaded = loaded
            say("TRACK \(b.track + 1): \(loaded ? b.name : "its own sound")")
            return
        }
    }

    // MARK: POOL

    /// A sound into pool place k (POOL REF).
    public func place(_ k: Int, id: Int) async -> UnitOutcome {
        let x = await perform("POOL") { try await $0.poolRef(slot: k, id: id) }
        guard let at = x.value?.a else { await refreshRows(); return x.outcome }
        poolSet(at, preset(id: id) ?? FizzListEntry(source: "mine", slot: 0, id: id, name: "?", tags: 0, imported: false, inPool: true))
        await refreshRows(until: { $0.contains { $0.source == "pool" && $0.slot == at && $0.id == id } })
        return .placed(at)
    }

    /// Two pool places swapped (into an empty one: a move): every pattern's locks follow, a job, stopped only (the
    /// screen confirmed first; the unit's undo is cleared).
    public func swapPool(_ a: Int, _ b: Int) async -> UnitOutcome {
        guard a != b else { return .ok }
        let ea = unit?.pool.first { $0.slot == a }, eb = unit?.pool.first { $0.slot == b }
        let x = await perform("SWAP") { try await $0.swap(source: .pool, a: a, b: b) }
        if x.isOK { poolSet(a, eb); poolSet(b, ea) }
        await refreshRows(until: x.isOK ? { rs in
            (rs.first { $0.source == "pool" && $0.slot == a }?.id ?? 0) == (eb?.id ?? 0) &&
                (rs.first { $0.source == "pool" && $0.slot == b }?.id ?? 0) == (ea?.id ?? 0)
        } : nil)
        return x.outcome
    }

    /// CLEAR a pool place (confirm: as the page's CLEAR, after its confirm; without it a used one: .needsConfirm).
    public func clearPlace(_ k: Int, confirm: Bool = true) async -> UnitOutcome {
        let x = await perform("CLEAR", place: k) { try await $0.remove(source: .pool, slot: k, confirm: confirm) }
        if x.isOK { poolSet(k, nil) }
        await refreshRows(until: x.isOK ? { !$0.contains { $0.source == "pool" && $0.slot == k } } : nil)
        return x.outcome
    }

    /// SORT BY TAG's order: the places' sounds by their first tag (FIZZ's order), then name, then place.
    public func sortedPool() -> [Int] {
        let pool = unit?.pool ?? []
        func tag(_ m: Int) -> Int { (0..<7).first { (m >> $0) & 1 == 1 } ?? 9 }
        return pool.sorted {
            let ta = tag($0.tags), tb = tag($1.tags)
            if ta != tb { return ta < tb }
            if $0.name != $1.name { return $0.name.localizedCompare($1.name) == .orderedAscending }
            return $0.slot < $1.slot
        }.map(\.slot)
    }

    /// SORT BY TAG: a swap for each sound out of place (a job each; stopped only). .placed(the moves made).
    public func sortPoolByTag() async -> UnitOutcome {
        guard canWrite, let link else { return .notNow }
        let want = sortedPool()
        var at = Dictionary(uniqueKeysWithValues: want.map { ($0, $0) })     // sound (its first place) -> place now
        var who = Dictionary(uniqueKeysWithValues: want.map { ($0, $0) })    // place -> sound
        busy = true
        defer { busy = false; job = nil }
        var n = 0, result = UnitOutcome.ok
        for (i, s) in want.enumerated() {
            let place = i + 1
            guard let from = at[s], from != place else { continue }
            job = (i + 1, want.count)
            let x = try? await link.swap(source: .pool, a: from, b: place)
            guard let x, x.isOK else {
                result = x.map { Self.outcome($0) } ?? .noAnswer
                say("SORT BY TAG: \(result.text("SORT BY TAG"))")
                break
            }
            n += 1
            let other = who[place]
            at[s] = place; who[place] = s
            if let other { at[other] = from; who[from] = other } else { who[from] = nil }
        }
        users = [:]
        await refreshRows()
        return result.isOK ? .placed(n) : result
    }

    /// CLEAR UNUSED: every place no step locks and no track plays (DELETE without CONFIRM; IN USE ones stay).
    /// .placed(how many cleared).
    public func clearUnused() async -> UnitOutcome {
        guard canWrite, let link else { return .notNow }
        let places = (unit?.pool ?? []).map(\.slot).sorted()
        busy = true
        defer { busy = false; job = nil }
        var n = 0, result = UnitOutcome.ok
        for (i, k) in places.enumerated() {
            job = (i + 1, places.count)
            let x = try? await link.remove(source: .pool, slot: k)
            if x?.isOK == true { n += 1; continue }
            if case .refused(.inUse)? = x { continue }
            result = x.map { Self.outcome($0) } ?? .noAnswer
            say("CLEAR UNUSED: \(result.text("CLEAR UNUSED"))")
            break
        }
        users = [:]
        await refreshRows()
        return result.isOK ? .placed(n) : result
    }

    /// A pool place's users (its steps, the tracks playing it), kept until the lists are read again.
    public func poolUsers(_ k: Int) async -> FizzUsers? {
        let key = "pool:\(k)"
        if let u = users[key] { return u }
        guard canWrite, editing == 0, let link, let u = (try? await link.usedBy(source: .pool, slot: k))?.value else { return nil }
        users[key] = u
        return u
    }

    // MARK: PROJECTS

    /// LOAD a project on the unit (its undo is cleared).
    public func loadProject(_ p: Int) async -> UnitOutcome {
        let x = await perform("LOAD") { try await $0.project(.load, p) }
        users = [:]
        await refreshRows(projects: true)
        return x.outcome
    }

    /// RENAME a project (projects.js cleanName).
    public func renameProject(_ p: Int, to name: String) async -> UnitOutcome {
        let v = FizzSysEx.cleanName(name)
        guard !v.isEmpty else { return .ok }
        let x = await perform("RENAME") { try await $0.renameProject(p, name: v) }
        if x.isOK { await refreshRows(projects: true) }
        return x.outcome
    }

    /// COPY TO: p onto q (the screen asked first when q holds one).
    public func copyProject(_ p: Int, to q: Int) async -> UnitOutcome {
        guard p != q else { return .ok }
        let x = await perform("COPY") { try await $0.project(.copy, p, q) }
        if x.isOK { await refreshRows(projects: true); return .placed(q) }
        return x.outcome
    }

    /// MOVE TO: copied, then p deleted. A delete refused after the copy: the refusal (slot q has it, p kept it too).
    public func moveProject(_ p: Int, to q: Int) async -> UnitOutcome {
        guard p != q else { return .ok }
        let x = await perform("MOVE") { try await $0.project(.copy, p, q) }
        guard x.isOK else { return x.outcome }
        let y = await perform("MOVE") { try await $0.project(.delete, p) }
        if !y.isOK { say("project \(p) was copied to slot \(q); slot \(p) kept it too") }
        await refreshRows(projects: true)
        return y.isOK ? .placed(q) : y.outcome
    }

    /// DELETE a project (the screen confirmed first).
    public func deleteProject(_ p: Int) async -> UnitOutcome {
        let x = await perform("DELETE") { try await $0.project(.delete, p) }
        if x.isOK { await refreshRows(projects: true) }
        return x.outcome
    }

    /// FLUSH: the open project's latest written (stopped only), then STATUS (projtab.js before reading the store).
    public func flush() async -> UnitOutcome {
        let x = await perform("FLUSH") { try await $0.flush() }
        if let s = x.value { unit?.took(status: s) }
        return x.outcome
    }

    // MARK: EDIT

    /// SAVE (over MY PRESETS `slot`, the screen confirmed first) or SAVE AS NEW (slot 0): STORE6 OWN; onto `track`
    /// (0..5) too when the editor is live on it. .placed(the slot it went to).
    public func save(_ s: FizzSound, slot: Int = 0, track: Int? = nil) async -> UnitOutcome {
        guard let f = try? FizzSysEx.storeSound(name: s.name, blob: s.blob, drive: s.drive, slot: slot, track: track, pan: s.pan,
                                                chorus: s.sends.chorus, delay: s.sends.delay, reverb: s.sends.reverb,
                                                tags: .names(s.tags), own: true) else { return .refused(.badPatch) }
        let x = await perform("SAVE") { try await $0.store(f) }
        guard let r = x.value else { return x.outcome }
        say("\(s.name) -> MY PRESETS \(r.slot)")
        await redump(r.slot)
        await refreshRows()
        return .placed(r.slot)
    }

    /// The TRACK switch's live objects for this link: the PARAM pump and the auditions (nil: no unit).
    public var pump: ParamPump? { live()?.0 }
    public var audition: TrackAudition? { live()?.1 }

    private func live() -> (ParamPump, TrackAudition)? {
        guard let link else { return nil }
        if liveLink !== link || _pump == nil || _audition == nil {
            let p = ParamPump(link: link, clock: unit?.source.clock ?? SystemClock()), a = TrackAudition(link: link)
            Task { [weak self] in
                await p.setOnOld { [weak self] in Task { @MainActor in self?.cannotEditLive() } }
            }
            liveLink = link; _pump = p; _audition = a
            pending = nil
        }
        return (_pump!, _audition!)
    }

    private func cannotEditLive() {
        trackCapable = false
        trackOn = false
        say("This FIZZ cannot edit a track live yet: update FIZZ")
    }

    /// A FIZZ that edits a track live: its track DUMP answers (connect, refresh).
    @discardableResult public func probeTrack() async -> Bool {
        guard let link, unit?.status != nil else { trackCapable = false; return false }
        let f = (try? await link.dumpTrack(slot: 0)) ?? nil
        trackCapable = f != nil
        if !trackCapable && trackOn { await setTrackOn(false) }
        return trackCapable
    }

    /// TRACK on: EDIT edits the unit's track, LIBRARY and IMPORT audition on it; off: the tracks are left alone.
    public func setTrackOn(_ on: Bool) async {
        guard let (p, _) = live() else { trackOn = false; return }
        let v = on && trackCapable
        trackOn = v
        await p.setOn(v)
        if !v { pending = nil; await audition?.keep() }
        say(v ? "TRACK on: EDIT edits the track, LIBRARY loads onto it" : "TRACK off: the Manager leaves the tracks alone")
    }

    /// The followed track's DUMP (trackSlot) and its sound.
    public func readTrack() async -> (frame: [UInt8], dump: FizzDump)? {
        await audition?.readTrack(slot: trackSlot)
    }

    /// A preset picked with TRACK on: heard on the track, nothing stored (KEEP / CANCEL).
    public func pick(_ r: FizzListEntry) async -> UnitOutcome {
        guard trackOn, canWrite, let a = audition, let f = await fetchFrame(of: r) else { return .notNow }
        let x = await a.pick(preset: f, id: r.id, name: r.name, trackSlot: trackSlot)
        pending = await a.pending
        guard let x else { return .noAnswer }
        if !x.isOK { say("pick: \(Self.outcome(x).text("pick again"))") }
        return Self.outcome(x)
    }

    /// A sound not on the unit (an IMPORT row) heard on the track the same way.
    public func pickSound(_ s: FizzSound) async -> UnitOutcome {
        guard trackOn, canWrite, let a = audition else { return .notNow }
        let x = await a.play(name: s.name, trackSlot: trackSlot) { t in
            try FizzSysEx.storeSound(name: s.name, blob: s.blob, drive: s.drive, track: t, pan: s.pan, chorus: s.sends.chorus,
                                     delay: s.sends.delay, reverb: s.sends.reverb, audition: true,
                                     extras: FizzTrackExtrasIn(id: 0, sound: 0, edited: true))
        }
        pending = await a.pending
        return x.map { Self.outcome($0) } ?? .noAnswer
    }

    /// KEEP: the track keeps what it plays.
    public func keepAudition() async {
        await audition?.keep()
        pending = nil
    }

    /// CANCEL: the track's own sound back. false: not yet (the bar stays: CANCEL again).
    public func cancelAudition() async -> Bool {
        guard let a = audition else { pending = nil; return true }
        let ok = await a.cancel()
        pending = await a.pending
        if !ok { say("the track's own sound did not go back yet: CANCEL again") }
        return ok
    }

    /// REVERT: the track put back exactly as `base` (its DUMP read when the editor took it).
    public func revert(to base: [UInt8]) async -> UnitOutcome {
        guard trackOn, let a = audition else { return .notNow }
        await pump?.clear()
        let x = await a.revert(to: base)
        await pump?.touch()
        return x.map { Self.outcome($0) } ?? .noAnswer
    }

    /// A note on the track's channel (extras.chan; 14 when unknown).
    public func note(channel: Int = 14, note n: Int, velocity: Int) async {
        try? await link?.note(channel: channel, note: n, velocity: velocity)
    }

    // MARK: IMPORT and ADD

    /// IMPORT's SEND: each sound STORE / STORE6 into the first free place (BATCH after the first: one undo), the pool
    /// too, or the first onto a track; stops at FULL. One outcome per sound sent (.placed(its slot or place)).
    public func send(_ sounds: [FizzSound], to dest: SendDestination) async -> [UnitOutcome] {
        guard canTransfer, let link else { return [] }
        busy = true
        defer { busy = false; job = nil }
        var out: [UnitOutcome] = []
        for (i, s) in sounds.enumerated() {
            job = (i + 1, sounds.count)
            var track = -1
            if case .track(let t) = dest, i == 0 { track = t }
            guard let f = try? FizzSysEx.storeFrame(name: s.name, blob: s.blob, drive: s.drive, pan: s.pan, chorus: s.sends.chorus,
                                                   delay: s.sends.delay, reverb: s.sends.reverb, fizzTags: s.tags,
                                                   pool: dest == .pool, batch: i > 0, track: track)
            else { out.append(.refused(.badPatch)); continue }
            do {
                let r = try await link.store(f)
                if let v = r.value { out.append(.placed(v.slot)); continue }
                out.append(Self.outcome(r))
                if r.refusal == .full { break }
            } catch {
                say("SEND: \(error)")
                out.append(.noAnswer)
                break
            }
        }
        let n = out.filter(\.isOK).count
        say("sent \(n) of \(sounds.count) to the FM-1")
        await refreshRows()
        await unit?.reloadDumps()
        return out
    }

    /// ADD presets (a .fizzlib, a presets backup, another store's): MY PRESETS read to skip the ones already there
    /// (sameSound), each new one stored (BATCH after the first) until FULL; then LIST and STATUS.
    public func addPresets(_ sounds: [FizzSound]) async -> AddResult {
        var res = AddResult()
        guard canTransfer, let link, let u = unit else { res.outcome = .notNow; return res }
        busy = true
        defer { busy = false; job = nil }
        do {
            let have = try await link.dumpAll(source: .mine, total: u.mine.count).compactMap(FizzSysEx.parseDump)
            for (i, s) in sounds.enumerated() {
                job = (i + 1, sounds.count)
                if have.contains(where: { FizzSysEx.sameSound(name: $0.name, blob: $0.blob, name: s.name, blob: s.blob) }) {
                    res.same += 1
                    continue
                }
                let f = try FizzSysEx.storeFrame(name: s.name, blob: s.blob, drive: s.drive, pan: s.pan, chorus: s.sends.chorus,
                                                 delay: s.sends.delay, reverb: s.sends.reverb, fizzTags: s.tags, batch: res.added > 0)
                let r = try await link.store(f)
                if r.isOK { res.added += 1 }
                else if r.refusal == .full { res.full = true; break }
                else { say("add \(s.name): \(r.refusal?.name ?? "")") }
            }
            say("added \(res.added) presets (\(res.same) already there\(res.full ? ", MY PRESETS full" : ""))")
        } catch {
            say("Adding the presets stopped: \(error) (\(res.added) added)")
            res.outcome = .noAnswer
        }
        if let rows = try? await link.list(factory: true, pool: true) { u.took(rows: rows) }
        u.took(status: await link.status())
        await u.reloadDumps()
        return res
    }

    // MARK: RESTORE and WRITE TO FM-1

    /// RESTORE a whole store image: a safety backup first (kept as "FIZZ before restore .."; none made: nothing
    /// restored), then the write (restoreRun shows it; CANCEL in the check pass only), then everything read again.
    public func restore(image: [UInt8], name: String) async -> UnitOutcome {
        guard canTransfer, let u = unit, let link else { return .notNow }
        restoreFailure = nil
        if await link.viaBle { restoreFailure = .needsUSB; return .refused(.other(15)) }
        restoreRun = RestoreRun(phase: .saving, fraction: 0, name: name, started: Date())
        let safe = await u.backups.safetyBackup()
        if restoreRun == nil { restoreFailure = .cancelled; return .ok }          // (cancelled while saving)
        guard safe != nil else {
            restoreRun = nil
            restoreFailure = .notSavedFirst
            say("the FM-1 could not be saved first: nothing restored")
            return .noAnswer
        }
        return await restoreImage(image, name)
    }

    /// PROJECTS' WRITE TO FM-1: refused while playing; `before` (the store as read) kept as "FIZZ before projects ..",
    /// then `image` written as a restore.
    public func writeProjects(image: [UInt8], before: [UInt8]) async -> UnitOutcome {
        guard canTransfer, let u = unit, let link, let info = u.unit else { return .notNow }
        restoreFailure = nil
        if await link.viaBle { restoreFailure = .needsUSB; return .refused(.other(15)) }
        let st = await link.status()
        if st?.playing == true { restoreFailure = .playing; return .refused(.busy) }
        if let st { u.took(status: st) }
        do {
            let e = try FizzDataShared.engine(), date = Date()
            let bytes = try await e.makeFizzbak(FizzbakInput(
                kind: .full, image: before, region: u.storeInfo.map { FizzStoreRegion(lo: Int($0.lo), size: before.count) },
                presets: nil, fizz: info.version ?? "", unitIdentity: info.idText, unitPort: info.portName, date: date))
            let name = try await e.backupFileName(date: date, kind: .full).replacingOccurrences(of: "FIZZ backup", with: "FIZZ before projects")
            try await u.backups.store.add(bytes, name: name)
        } catch {
            restoreFailure = .notSavedFirst
            say("the FM-1's store could not be kept first: \(error)")
            return .noAnswer
        }
        return await restoreImage(image, "the projects")
    }

    /// CANCEL on the progress screen: the safety backup stopped, or the check pass (nothing changed); not while writing.
    public func cancelRestore() {
        guard let r = restoreRun, r.canCancel else { return }
        if r.phase == .saving { unit?.backups.cancel(); restoreRun = nil; return }
        restoreTask?.cancel()
    }

    public func dismissRestoreFailure() { restoreFailure = nil }

    private final class Progress: @unchecked Sendable {
        let lock = NSLock()
        var phase = FizzRestorePhase.checking, fraction = 0.0
    }

    private func restoreImage(_ image: [UInt8], _ name: String) async -> UnitOutcome {
        guard let link, let u = unit else { return .notNow }
        busy = true
        let started = Date()
        restoreRun = RestoreRun(phase: .checking, fraction: 0, name: name, started: started)
        awake.begin { [weak self] in self?.cancelRestore() }
        let box = Progress()
        let poll = Task { [weak self] in
            while !Task.isCancelled {
                try? await Task.sleep(for: .milliseconds(60))
                let (p, f) = box.lock.withLock { (box.phase, box.fraction) }
                guard let self, self.restoreRun != nil else { return }
                self.restoreRun = RestoreRun(phase: p == .writing ? .writing : .checking, fraction: f, name: name, started: started)
            }
        }
        let t = Task { try await link.restore(image) { p, f in box.lock.withLock { box.phase = p; box.fraction = f } } }
        restoreTask = t
        let r = await t.result
        poll.cancel()
        awake.end()
        restoreTask = nil
        restoreRun = nil
        defer { busy = false }
        switch r {
        case .success:
            say("restored \(name)")
            await refreshAfterRestore(link, u)
            return .ok
        case .failure(let e):
            let f = RestoreFailure.of(e)
            restoreFailure = f
            say("restore: \(e)")
            let lost: Bool = { if case .lost? = e as? FizzRestoreError { true } else { false } }()
            if !lost && self.link === link { await refreshAfterRestore(link, u) }
            return f == .cancelled ? .ok : f == .playing ? .refused(.busy) : .noAnswer
        }
    }

    /// manager.js refresh: IDENTITY until FIZZ answers again (it starts on the restored store: 10 x 0.5 s), FZBI,
    /// LIST, STATUS, PROJECTS, the DUMPs.
    private func refreshAfterRestore(_ link: FizzLink, _ u: UnitModel) async {
        var v: String?
        for t in 0..<10 {
            try? await Task.sleep(for: .milliseconds(500))
            v = (try? await link.identity()) ?? nil
            if v != nil || t == 9 { break }
        }
        u.took(version: v)
        await u.took(storeInfo: (try? await link.storeInfo()) ?? nil)
        u.took(rows: v != nil ? ((try? await link.list(factory: true, pool: true)) ?? []) : [])
        u.took(status: v != nil ? await link.status() : nil)
        u.took(projects: u.status != nil ? await link.projects() : nil)
        frames = [:]; oneOff = [:]; users = [:]
        await u.reloadDumps()
    }
}
