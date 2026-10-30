// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// LIBRARY's SELECT on the unit's presets (mock-ups 41, 42, 47): the writes on many at once, as manager.js bulkTag /
// bulkPool / bulkDelete do them: one edit after another (the protocol batches STORE only), a progress count, the
// first refusal stops the rest, the lists read once at the end.

import Foundation
import FizzKit

/// A bulk edit's outcome: how many went through, and what stopped the rest (nil: nothing did).
public struct BulkDone: Sendable, Equatable {
    public var n = 0
    public var stopped: UnitOutcome?
}

/// − POOL's place that projects' steps lock (FIZZ answered IN USE): asked once, then cleared with CONFIRM.
public struct PoolUse: Sendable, Equatable {
    public let row: FizzListEntry
    public let place: Int
    public let steps: Int
}

extension UnitActions {
    /// manager.js bulkRun: each in turn, `progress(i, of)` before it; `fn` answers true (done), false (skipped) or nil
    /// (stop: a refusal, no answer).
    func bulkRun<T: Sendable>(_ items: [T], progress: (Int, Int) -> Void, _ fn: (T) async -> Bool?) async -> Int {
        var n = 0
        for i in items.indices {
            progress(i, items.count)
            guard let ok = await fn(items[i]) else { break }
            if ok { n += 1 }
        }
        progress(items.count, items.count)
        return n
    }

    /// The unit's presets among rows (MY PRESETS, FACTORY: what TAG and the pool act on).
    nonisolated static func unitRows(_ rows: [FizzListEntry]) -> [FizzListEntry] {
        rows.filter { ($0.source == "mine" || $0.source == "factory") && $0.id != 0 }
    }

    /// TAG (manager.js bulkTag): each preset whose tags differ gets `mask` (one of FIZZ's seven, or 0: NONE), as
    /// setTags does one (its row and DUMP updated).
    public func bulkTag(_ rows: [FizzListEntry], mask: Int, progress: (Int, Int) -> Void = { _, _ in }) async -> BulkDone {
        let todo = Self.unitRows(rows).filter { $0.tags & 0x7f != mask }
        var stop: UnitOutcome?
        let n = await bulkRun(todo, progress: progress) { r in
            let o = await setTags(r, mask: mask)
            if o.isOK { return true }
            stop = o
            return nil
        }
        if n > 0 { say("tagged \(n) \(mask == 0 ? "NONE" : FizzSysEx.fizzTags[mask.trailingZeroBitCount])") }
        return BulkDone(n: n, stopped: stop)
    }

    /// + POOL (manager.js bulkPool): the presets not in the pool yet, each into the first free place (FULL stops).
    public func bulkPoolAdd(_ rows: [FizzListEntry], progress: (Int, Int) -> Void = { _, _ in }) async -> BulkDone {
        let todo = Self.unitRows(rows).filter { places(of: $0.id).isEmpty }
        var stop: UnitOutcome?
        let n = await bulkRun(todo, progress: progress) { r in
            let x = await perform("+ POOL") { try await $0.poolRef(slot: 0, id: r.id) }
            guard x.isOK, let k = x.value?.a else { stop = x.outcome; return nil }
            poolSet(k, r)
            return true
        }
        if n > 0 { say("+ POOL: \(n) into the pool") }
        await refreshRows()
        return BulkDone(n: n, stopped: stop)
    }

    /// − POOL (manager.js bulkPool): every place holding them cleared without CONFIRM; the ones projects' steps use
    /// answer IN USE and come back in `used`, to be asked about once (bulkPoolClear).
    public func bulkPoolRemove(_ rows: [FizzListEntry], progress: (Int, Int) -> Void = { _, _ in })
        async -> (done: BulkDone, used: [PoolUse]) {
        let ks = Self.unitRows(rows).flatMap { r in places(of: r.id).map { PoolUse(row: r, place: $0, steps: 0) } }
        var used: [PoolUse] = []
        var stop: UnitOutcome?
        let n = await bulkRun(ks, progress: progress) { p in
            let r = p.row, k = p.place
            let x = await perform("- POOL", place: k) { try await $0.remove(source: .pool, slot: k, confirm: false) }
            if case .needsConfirm(_, _, let steps) = x.outcome { used.append(PoolUse(row: r, place: k, steps: steps)); return false }
            guard x.isOK else { stop = x.outcome; return nil }
            poolSet(k, nil)
            return true
        }
        if n > 0 { say("- POOL: \(n) out of the pool") }
        await refreshRows()
        return (BulkDone(n: n, stopped: stop), used)
    }

    /// − POOL's used places, confirmed: cleared with CONFIRM (their steps go back to their track's sound).
    public func bulkPoolClear(_ used: [PoolUse], progress: (Int, Int) -> Void = { _, _ in }) async -> BulkDone {
        var stop: UnitOutcome?
        let n = await bulkRun(used, progress: progress) { u in
            let x = await perform("- POOL", place: u.place) { try await $0.remove(source: .pool, slot: u.place, confirm: true) }
            guard x.isOK else { stop = x.outcome; return nil }
            poolSet(u.place, nil)
            return true
        }
        if n > 0 { say("- POOL: \(n) used places cleared") }
        await refreshRows()
        return BulkDone(n: n, stopped: stop)
    }

    /// USED BY of each MY preset among rows (USERS, kept until the lists are read again): id -> its projects.
    public func bulkUsers(_ rows: [FizzListEntry]) async -> [Int: FizzUsers] {
        var out: [Int: FizzUsers] = [:]
        for r in rows where r.source == "mine" {
            if let u = await usedBy(r) { out[r.id] = u }
        }
        return out
    }

    /// DELETE (manager.js bulkDelete): the MY presets only (the factory's stay), the last place first, sent with
    /// CONFIRM (the screen asked once).
    public func bulkDelete(_ rows: [FizzListEntry], progress: (Int, Int) -> Void = { _, _ in }) async -> BulkDone {
        let mine = rows.filter { $0.source == "mine" }.sorted { $0.slot > $1.slot }
        var stop: UnitOutcome?
        let n = await bulkRun(mine, progress: progress) { r in
            let x = await perform("DELETE") { try await $0.remove(source: .mine, slot: r.slot, confirm: true) }
            guard x.isOK else { stop = x.outcome; return nil }
            frames["mine:\(r.slot)"] = nil
            return true
        }
        if n > 0 { say("deleted \(n) of MY PRESETS") }
        await refreshRows()
        return BulkDone(n: n, stopped: stop)
    }
}
