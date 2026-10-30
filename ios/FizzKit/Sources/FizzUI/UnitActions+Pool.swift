// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// POOL's SELECT (PoolSelect.swift): the writes on many places at once, one edit after another as pool.js does them
// one at a time: CLEAR (DELETE with CONFIRM, its steps back to their track's sound), MOVE as a block (pool.js's SWAP,
// a run of them as SORT BY TAG makes: every pattern's locks follow), the steps that use them (USERS) for the confirm.
// A progress count, the first refusal stops the rest, the lists read once at the end.

import Foundation
import FizzKit

extension UnitActions {
    /// The steps locking these places (USERS, read for the ones not read yet): the CLEAR confirm's count.
    public func poolStepsUsing(_ places: [Int]) async -> Int {
        var n = 0
        for k in places {
            if let u = await poolUsers(k) { n += u.projects.reduce(0) { $0 + $1.steps } }
        }
        return n
    }

    /// CLEAR n (pool.js p-clear, each place in turn): DELETE with CONFIRM (the steps using it go back to their
    /// track's sound; the unit's undo kept).
    public func bulkClearPlaces(_ places: [Int], progress: (Int, Int) -> Void = { _, _ in }) async -> BulkDone {
        let go = places.filter { k in unit?.pool.contains { $0.slot == k } == true }.sorted()
        var stop: UnitOutcome?
        let n = await bulkRun(go, progress: progress) { k in
            let x = await perform("CLEAR", place: k) { try await $0.remove(source: .pool, slot: k, confirm: true) }
            guard x.isOK else { stop = x.outcome; return nil }
            poolSet(k, nil)
            return true
        }
        if n > 0 { say("CLEAR: \(n) pool place\(n == 1 ? "" : "s") cleared") }
        let gone = Set(go.prefix(n))
        await refreshRows(until: n > 0 ? { rs in !rs.contains { $0.source == "pool" && gone.contains($0.slot) } } : nil)
        return BulkDone(n: n, stopped: stop)
    }

    /// MOVE as a block: where the places' sounds go (in place order, side by side from `start`, held inside 1...128)
    /// and the swaps that take them there (SORT BY TAG's bookkeeping: a sound already where it goes stays; the
    /// sounds in the way go to the places freed).
    public nonisolated static func blockMove(_ places: [Int], to start: Int, occupied: Set<Int>)
        -> (range: ClosedRange<Int>, swaps: [(a: Int, b: Int)]) {
        let go = Array(Set(places)).sorted()
        guard !go.isEmpty else { return (start...start, []) }
        let first = max(1, min(start, 129 - go.count))
        var at = Dictionary(uniqueKeysWithValues: go.map { ($0, $0) })       // sound (its place first) -> place now
        var who: [Int: Int] = [:]                                              // place -> sound (a pool sound's first place)
        for k in occupied.union(go) { who[k] = k }
        var swaps: [(a: Int, b: Int)] = []
        for (i, s) in go.enumerated() {
            let place = first + i
            guard let from = at[s], from != place else { continue }
            swaps.append((from, place))
            let other = who[place]
            at[s] = place; who[place] = s
            if let other { at[other] = from; who[from] = other } else { who[from] = nil }
        }
        return (first...(first + go.count - 1), swaps)
    }

    /// MOVE n → start: the swaps in turn (each a job on the unit, stopped only; the unit's undo is cleared), the
    /// places shown changed as each goes through. .n: the swaps made.
    public func bulkMovePool(_ places: [Int], to start: Int, progress: (Int, Int) -> Void = { _, _ in }) async -> BulkDone {
        let occupied = Set((unit?.pool ?? []).map(\.slot))
        let plan = Self.blockMove(places.filter(occupied.contains), to: start, occupied: occupied)
        var stop: UnitOutcome?
        let n = await bulkRun(plan.swaps, progress: progress) { s in
            let ea = unit?.pool.first { $0.slot == s.a }, eb = unit?.pool.first { $0.slot == s.b }
            let x = await perform("SWAP") { try await $0.swap(source: .pool, a: s.a, b: s.b) }
            guard x.isOK else { stop = x.outcome; return nil }
            poolSet(s.a, eb); poolSet(s.b, ea)
            return true
        }
        if n > 0 { say("MOVE: \(plan.range.count) pool sounds to \(plan.range.lowerBound)–\(plan.range.upperBound), \(n) swaps") }
        let want = Dictionary((unit?.pool ?? []).map { ($0.slot, $0.id) }, uniquingKeysWith: { a, _ in a })
        await refreshRows(until: n > 0 ? { rs in
            plan.range.allSatisfy { k in rs.first { $0.source == "pool" && $0.slot == k }?.id == want[k] }
        } : nil)
        return BulkDone(n: n, stopped: stop)
    }
}

extension UnitModel {
    /// → KEPT n: the places' presets onto the phone (the same sound in two places: kept once).
    public func keepPlaces(_ places: [Int], progress: (Int, Int) -> Void = { _, _ in }) async -> (kept: Int, already: Int) {
        let pm = Dictionary(pool.map { ($0.slot, $0.id) }, uniquingKeysWith: { a, _ in a })
        var ids: [Int] = []
        for k in places.sorted() { if let id = pm[k], id != 0, !ids.contains(id) { ids.append(id) } }
        let rows = ids.compactMap { id in self.rows.first { $0.source != "pool" && $0.id == id } }
        var kept = 0, already = 0
        for (i, r) in rows.enumerated() {
            progress(i, rows.count)
            switch await keep(r) {
            case .kept: kept += 1
            case .already: already += 1
            case .failed: break
            }
        }
        progress(rows.count, rows.count)
        return (kept, already)
    }
}
