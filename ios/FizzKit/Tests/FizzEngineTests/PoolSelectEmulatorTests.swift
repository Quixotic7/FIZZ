// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// POOL's SELECT on the emulated FIZZ (no FM-1): three places ticked, one of them locked by steps; the CLEAR
// confirm's count (FIZZ's USERS), → KEPT, CLEAR n (FIZZ's DELETE with CONFIRM), MOVE as a block (FIZZ's SWAPs, the
// locks following). FIZZ's own answers, not the demo's. (In the Emulator suite: the core is the process's one.)

import Foundation
import Testing
import FizzData
import FizzKit
@testable import FizzEngine
@testable import FizzUI

extension EmulatorTests {
    @MainActor @Test func poolSelectOnTheEmulatedFizz() async throws {
        let d = FileManager.default.temporaryDirectory.appendingPathComponent("fizz-emu-poolsel-\(UUID().uuidString)")
        let e = FizzEmulator(storage: nil)
        let host = EmulatorHost(emulator: e)
        let m = UnitModel(source: NoPorts(), emulator: host, backups: d.appendingPathComponent("Backups"),
                          projects: d.appendingPathComponent("Projects"), presets: d.appendingPathComponent("Presets"))
        host.launch()
        m.start()
        func until(_ ok: () -> Bool) async -> Bool {
            let end = Date().addingTimeInterval(15)
            while !ok() && Date() < end { try? await Task.sleep(for: .milliseconds(20)) }
            return ok()
        }
        #expect(await until { m.onEmulator && m.dumpsLoaded })
        let a = m.actions
        func steps(_ k: Int) async -> Int? { await a.poolUsers(k).map { $0.projects.reduce(0) { $0 + $1.steps } } }

        // the fresh FIZZ's pool: the drums locked by its demo pattern's steps, the rest not
        var locked: [Int: Int] = [:], free: [Int] = []
        for k in m.pool.map(\.slot).sorted() {
            guard let n = await steps(k) else { continue }
            if n > 0 { locked[k] = n } else { free.append(k) }
        }
        try #require(locked.count >= 2 && free.count >= 3, "pool \(m.pool.count), locked \(locked)")
        let used = locked.keys.min()!

        // three ticked (a long-press, then taps), one of them locked
        var p = PoolPicks()
        p.hold(free[0], full: true)
        p.tap(used, full: true); p.tap(free[1], full: true)
        let go = p.sorted
        #expect(go.count == 3)
        #expect(await a.poolStepsUsing(go) == locked[used], "CLEAR's confirm: FIZZ's USERS")

        // → KEPT: their presets on the phone
        let k = await m.keepPlaces(go)
        #expect(k.kept == 3 && k.already == 0)
        #expect(m.kept.items.count == 3)

        // CLEAR 3: FIZZ's DELETE with CONFIRM (the locked one too: its steps back to their track's sound)
        let n = m.pool.count
        #expect(await a.bulkClearPlaces(go) == BulkDone(n: 3, stopped: nil))
        await a.refreshRows()
        #expect(m.pool.count == n - 3 && !m.pool.contains { go.contains($0.slot) }, "FIZZ's list says so")

        // MOVE as a block: three (one locked) side by side from 40; its steps follow it (FIZZ's USERS there)
        let left = m.pool.map(\.slot).sorted()
        let lock = try #require(locked.keys.sorted().first { left.contains($0) })
        let three = Array(Set([free[2], lock, left.last!])).sorted()
        let want = three.compactMap { k in m.pool.first { $0.slot == k }?.id }
        let mv = await a.bulkMovePool(three, to: 40)
        #expect(mv.stopped == nil && mv.n == three.count)
        await a.refreshRows()
        #expect((40..<(40 + three.count)).map { k in m.pool.first { $0.slot == k }?.id } == want)
        #expect(m.pool.count == n - 3)
        #expect(await steps(40 + three.firstIndex(of: lock)!) == locked[lock], "its steps follow it")
        let there = await steps(lock) ?? 0
        #expect(there != locked[lock], "not where it was")

        m.stop()
        await host.close()
    }
}
