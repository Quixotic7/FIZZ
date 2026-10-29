// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// IMPORT's EDIT on the emulated FIZZ: a .fizzsnd read by IMPORT opened in EDIT (heard on the selected track as EDIT
// borrows), a value turned, SAVE AS NEW: the sound stored in the unit's first free MY PRESETS slot, read back from the
// emulator with the value; EDIT then edits that preset. (In the Emulator suite: the core is the process's one.)

import Foundation
import Testing
import FizzData
import FizzKit
@testable import FizzEngine
@testable import FizzUI

extension EmulatorTests {
    @MainActor @Test func aFileSoundEditedIsSavedAsNew() async throws {
        let d = FileManager.default.temporaryDirectory.appendingPathComponent("fizz-emu-file-\(UUID().uuidString)")
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
        #expect(await until { m.onEmulator && m.dumpsLoaded && m.actions.trackCapable && m.actions.canWrite })
        let a = m.actions, ed = m.editor, eng = try FizzDataShared.engine()

        // a .fizzsnd (a factory sound renamed, as a file from elsewhere) read by IMPORT
        let r = try #require(m.factory.first)
        var s = try #require(await a.sound(of: r))
        s.name = "FROM A FILE"
        await m.importer.add(files: [(name: "from a file.fizzsnd", bytes: try await eng.keptFile(s))])
        let row = try #require(m.importer.rows.first { $0.item.sound?.name == "FROM A FILE" })
        let free = (1...(m.status?.nlib ?? 256)).first { k in !m.mine.contains { $0.slot == k } } ?? 0
        #expect(free > 0)

        // EDIT on the row: heard on the selected track (a borrow), a value turned
        m.importer.edit(row.id)
        await ed.show()
        #expect(ed.place.isFile && ed.sound?.name == "FROM A FILE")
        #expect(a.borrowed && ed.heard == .borrowed(ed.track))
        let v = ed.value(6) == 40 ? 41 : 40
        #expect(ed.set(6, v))
        await ed.settle()

        // SAVE AS NEW: the first free MY slot, the value in it as the emulated FIZZ stores it
        let x = await ed.save(asNew: true)
        #expect(x == .placed(free))
        #expect(ed.place.source == "mine" && ed.place.slot == free && ed.canSave)
        let f = try #require(try await m.link?.dump(source: .mine, slot: free))
        let back = try #require(try await eng.editSound(frame: f))
        #expect(back.name == "FROM A FILE" && back.values[6] == v)
        #expect(m.mine.contains { $0.slot == free && $0.name == "FROM A FILE" })

        // (EDIT let go of the unit before it stops: the core is the next test's too)
        ed.hide()
        await ed.enqueue {}.value
        #expect(await until { !a.borrowed && a.pending == nil && !ed.following })
        m.stop()
        e.stop()
    }
}
