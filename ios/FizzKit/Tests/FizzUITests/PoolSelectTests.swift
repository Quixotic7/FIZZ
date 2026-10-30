// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// POOL's SELECT against the demo unit: a long-press starts it (an empty place does nothing), taps tick more; three
// ticked, one of them locked by steps: the CLEAR confirm's count, → KEPT, CLEAR (with CONFIRM: no IN USE), MOVE as a
// block (the swaps it plans, the sounds side by side after). The emulator's run: FizzEngineTests/PoolSelectEmulatorTests.

import Foundation
import Testing
import FizzData
import FizzKit
@testable import FizzUI

@MainActor
private func waitFor(seconds: Double = 30, _ ok: () -> Bool) async -> Bool {
    let end = Date().addingTimeInterval(seconds)
    while Date() < end {
        if ok() { return true }
        try? await Task.sleep(for: .milliseconds(20))
    }
    return ok()
}

@MainActor
private func connected() async -> (UnitModel, DemoUnit) {
    let dir = FileManager.default.temporaryDirectory.appendingPathComponent("fizz-poolsel-\(UUID().uuidString)", isDirectory: true)
    let src = DemoLinkSource(.fizz, bulkMs: 0...0.3)
    let m = UnitModel(source: src, backups: dir.appendingPathComponent("Backups"), projects: dir.appendingPathComponent("Projects"),
                      presets: dir.appendingPathComponent("Presets"))
    m.start()
    _ = await waitFor { m.dumpsLoaded && m.layout != nil }
    return (m, src.demo.units[0])
}

/// The swaps run on a pool (place -> sound id), as the unit does them.
private func swapped(_ pool: [Int: Int], _ swaps: [(a: Int, b: Int)]) -> [Int: Int] {
    var p = pool
    for s in swaps { let a = p[s.a], b = p[s.b]; p[s.a] = b; p[s.b] = a }
    return p
}

@Suite("POOL's SELECT", .serialized) @MainActor
struct PoolSelectTests {
    @Test func theSelection() {
        var p = PoolPicks()
        p.hold(7, full: false)
        #expect(!p.on && p.places.isEmpty, "a long-press on an empty place: nothing")
        p.tap(3, full: true)
        #expect(!p.on && p.places.isEmpty, "a tap before the long-press: not SELECT's")
        p.hold(3, full: true)
        #expect(p.on && p.sorted == [3])
        p.tap(9, full: true); p.tap(6, full: true); p.tap(12, full: false)
        #expect(p.sorted == [3, 6, 9], "an empty place takes no tick")
        p.tap(9, full: true)
        #expect(p.sorted == [3, 6])
        p.hold(9, full: true)
        #expect(p.sorted == [3, 6, 9], "on already: a long-press toggles as a tap")
        p.moving = true
        p.keep([3, 9, 40])
        #expect(p.sorted == [3, 9] && p.on)
        p.moved(to: 20...21)
        #expect(p.sorted == [20, 21] && !p.moving)
        p.clear()
        #expect(p == PoolPicks())
    }

    @Test func theBlockMovePlan() {
        let pool = Dictionary(uniqueKeysWithValues: (1...10).map { ($0, 100 + $0) })
        // three spread out onto 4: the ones in the way go to the places freed
        let a = UnitActions.blockMove([9, 2, 5], to: 4, occupied: Set(pool.keys))
        #expect(a.range == 4...6)
        let after = swapped(pool, a.swaps)
        #expect([after[4], after[5], after[6]] == [102, 105, 109] && Set(after.values) == Set(pool.values))
        #expect(a.swaps.count == 2, "105 is at 5 already")
        // already side by side there: no swap; past 128: held inside
        #expect(UnitActions.blockMove([4, 5, 6], to: 4, occupied: Set(pool.keys)).swaps.isEmpty)
        let end = UnitActions.blockMove([1, 2, 3], to: 127, occupied: Set(pool.keys))
        #expect(end.range == 126...128 && swapped(pool, end.swaps)[126] == 101 && swapped(pool, end.swaps)[128] == 103)
        // onto empty places: moves
        let e = UnitActions.blockMove([1, 3], to: 60, occupied: Set(pool.keys))
        let ea = swapped(pool, e.swaps)
        #expect(ea[60] == 101 && ea[61] == 103 && ea[1] == nil && ea[3] == nil)
    }

    @Test func selectThreeClearKeepMove() async throws {
        let (m, demo) = await connected()
        let a = m.actions
        // three ticked, one locked by steps (DemoState: every third place)
        let used = try #require(m.pool.map(\.slot).sorted().first { DemoState.steps($0) > 0 })
        let free = m.pool.map(\.slot).sorted().filter { DemoState.steps($0) == 0 && $0 != used }
        try #require(free.count >= 4)
        var p = PoolPicks()
        p.hold(used, full: true)
        p.tap(free[0], full: true); p.tap(free[1], full: true)
        let go = p.sorted
        #expect(go.count == 3)

        // CLEAR's confirm: the steps that use them
        #expect(await a.poolStepsUsing(go) == DemoState.steps(used))

        // → KEPT n: their presets on the phone
        let ids = Set(go.compactMap { k in m.pool.first { $0.slot == k }?.id })
        let k = await m.keepPlaces(go)
        #expect(k.kept == ids.count && k.already == 0)
        #expect(m.kept.items.count == ids.count)
        #expect(await m.keepPlaces(go).already == ids.count, "again: kept already")

        // CLEAR n: all three out (the used one with CONFIRM: no IN USE asked), the rest of the pool as it was
        let n = m.pool.count
        #expect(await a.bulkClearPlaces(go) == BulkDone(n: 3, stopped: nil))
        #expect(m.pool.count == n - 3 && !m.pool.contains { go.contains($0.slot) })
        #expect(go.allSatisfy { demo.poolPlaces[$0] == nil })
        p.keep(Set(m.pool.map(\.slot)))
        #expect(p.places.isEmpty && p.on)

        // MOVE as a block: three onto 100..102, in place order
        let three = Array(m.pool.map(\.slot).sorted().prefix(3)), before = m.pool.count
        let want = three.compactMap { k in m.pool.first { $0.slot == k }?.id }
        let d = await a.bulkMovePool(three, to: 100)
        #expect(d.stopped == nil && d.n == 3)
        #expect((100...102).map { k in m.pool.first { $0.slot == k }?.id } == want)
        #expect(m.pool.count == before && (100...102).map { demo.poolPlaces[$0] } == want)
        m.stop()
    }

    @Test func busyStopsTheMove() async throws {
        let (m, demo) = await connected()
        demo.setPlaying(true)
        let three = Array(m.pool.map(\.slot).sorted().prefix(3))
        let d = await m.actions.bulkMovePool(three, to: 100)
        #expect(d == BulkDone(n: 0, stopped: .refused(.busy)), "SWAP is a job: stopped only")
        m.stop()
    }
}
