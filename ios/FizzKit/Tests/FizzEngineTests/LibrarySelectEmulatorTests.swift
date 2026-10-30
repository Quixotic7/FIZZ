// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// LIBRARY's SELECT (mock-ups 41, 42, 47) on the emulated FIZZ, the app's unit with no FM-1: three presets saved, then
// TAG on the three, + POOL them, − POOL, and DELETE with the open project using one (its pool references it). FIZZ's
// own answers, not the demo's. (In the Emulator suite: the core is the process's one.)

import Foundation
import Testing
import FizzData
import FizzKit
@testable import FizzEngine
@testable import FizzUI

extension EmulatorTests {
    @MainActor @Test func selectTagPoolDeleteOnTheEmulatedFizz() async throws {
        let d = FileManager.default.temporaryDirectory.appendingPathComponent("fizz-emu-select-\(UUID().uuidString)")
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

        // three of the factory's saved into MY PRESETS (SAVE AS NEW)
        for s in await a.exportSounds(Array(m.factory.prefix(3))) {
            guard case .placed = await a.save(s) else { Issue.record("not saved: \(s.name)"); continue }
        }
        let three = m.mine
        #expect(three.count == 3)

        // TAG: LEAD (or PAD) on the three, one edit each (it replaces theirs); FIZZ's list says so
        let tag = three.contains { $0.tags == 16 } ? 8 : 16                     // (one none of them has yet)
        #expect(await a.bulkTag(three, mask: tag) == BulkDone(n: 3, stopped: nil))
        await a.refreshRows()
        #expect(m.mine.count == 3 && m.mine.allSatisfy { $0.tags == tag })

        // + POOL: each into the first free place; − POOL: out again (no steps lock them: nothing asked)
        let before = m.pool.count
        #expect(await a.bulkPoolAdd(m.mine) == BulkDone(n: 3, stopped: nil))
        #expect(m.pool.count == before + 3 && m.mine.allSatisfy { !a.places(of: $0.id).isEmpty })
        let (out, used) = await a.bulkPoolRemove(m.mine)
        #expect(out == BulkDone(n: 3, stopped: nil) && used.isEmpty)
        #expect(m.pool.count == before && m.mine.allSatisfy { a.places(of: $0.id).isEmpty })

        // DELETE with the open project using one (its pool references it): USED BY names it, then all three go
        let one = try #require(m.mine.first)
        #expect(await a.bulkPoolAdd([one]) == BulkDone(n: 1, stopped: nil))
        let mine = m.mine
        _ = await a.bulkUsers(mine)
        let u = BulkUsage(mine, users: a.users)
        #expect(u.used.map(\.slot) == [one.slot])
        #expect(u.projects.count == 1)
        let gone = await a.bulkDelete(mine + Array(m.factory.prefix(1)))
        #expect(gone == BulkDone(n: 3, stopped: nil))
        #expect(m.mine.isEmpty && m.factory.count == 23, "the factory's stay")
        #expect(a.places(of: one.id).isEmpty && m.pool.count == before, "its pool place went with it")

        m.stop()
        await host.close()
    }
}
