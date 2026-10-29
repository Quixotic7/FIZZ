// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// EDIT's borrow (mock-up 37) on the emulated FIZZ, the app's unit with no FM-1: a factory preset opened in EDIT is
// heard on the selected track at once (FIZZ's own AUDITION), a value turned is a PARAM there, leaving EDIT puts the
// track's own sound back, KEEP ON Tn leaves it. (In the Emulator suite: the core is the process's one.)

import Foundation
import Testing
import FizzData
import FizzKit
@testable import FizzEngine
@testable import FizzUI

extension EmulatorTests {
    @MainActor @Test func editBorrowsOnTheEmulatedFizz() async throws {
        let d = FileManager.default.temporaryDirectory.appendingPathComponent("fizz-emu-borrow-\(UUID().uuidString)")
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
        #expect(await until { m.onEmulator && m.dumpsLoaded && m.actions.trackCapable })
        let a = m.actions, ed = m.editor
        func onTrack(_ t: Int) async -> EditSound? {
            guard let f = (try? await m.link?.dumpTrack(slot: t + 1)) ?? nil else { return nil }
            return try? await FizzDataShared.engine().editSound(frame: f)
        }

        // EDIT on the track itself first: which one is selected, and its own sound
        await ed.show()
        let t = ed.track
        #expect(t >= 0 && ed.heard == .track(t))
        let own = try #require(await onTrack(t))
        ed.hide()
        #expect(await until { !ed.following })

        // a factory preset opened: heard on that track at once, no TRACK switch, no KEEP / CANCEL bar
        let r = try #require(m.factory.first { $0.name != own.name })
        m.editTarget = .preset(r)
        await ed.show()
        #expect(a.borrowed && a.pending?.track == t && !a.trackOn)
        #expect(ed.heard == .borrowed(t) && ed.place.label == "FACTORY \(r.slot)")
        let heard = await onTrack(t)
        #expect(heard?.name == r.name)

        // a value turned: FIZZ's PARAM onto the borrowed sound
        let v = ed.value(6) == 40 ? 41 : 40
        #expect(ed.set(6, v))
        await ed.settle()
        let turned = await onTrack(t)
        #expect(turned?.values[6] == v)

        // EDIT left: the track's own sound back, exactly
        ed.hide()
        #expect(await until { !a.borrowed && a.pending == nil })
        let back = try #require(await onTrack(t))
        #expect(back.name == own.name && back.values == own.values)

        // EDIT again: heard again with the edit; KEEP ON Tn: it stays
        await ed.show()
        let again = await onTrack(t)
        #expect(a.borrowed && again?.values[6] == v)
        await ed.keepOn()
        #expect(!a.borrowed && ed.heard == .track(t))
        ed.hide()
        #expect(await until { !ed.following })
        let kept = await onTrack(t)
        #expect(kept?.name == r.name)

        m.stop()
        await host.close()
    }
}
