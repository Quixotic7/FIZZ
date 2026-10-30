// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// Mock-ups 45, 46 on the emulated FIZZ (its core takes PARAM 115..119 live): PAN and the sends turned in EDIT reach
// the track as PARAMs (no fallback), read back from the track's DUMP; + NEW FM and MIDI sounds saved as new into the
// first free MY PRESETS place and read back. (In the Emulator suite: the core is the process's one.)

import Foundation
import Testing
import FizzData
import FizzKit
@testable import FizzEngine
@testable import FizzUI

extension EmulatorTests {
    @MainActor @Test func editMixOnTheEmulatedFizz() async throws {
        let d = FileManager.default.temporaryDirectory.appendingPathComponent("fizz-emu-mix-\(UUID().uuidString)")
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
        func onTrack(_ t: Int) async -> FizzDump? {
            guard let f = (try? await m.link?.dumpTrack(slot: t + 1)) ?? nil else { return nil }
            return FizzSysEx.parseDump(f)
        }

        // EDIT on the track: PAN and the sends turned, each a PARAM (115..118) the core takes
        await ed.show()
        let t = ed.track
        #expect(t >= 0 && ed.live)
        let pan = ed.value(EditMix.pan) == -12 ? 17 : -12
        #expect(ed.set(EditMix.pan, pan))
        #expect(ed.set(EditMix.delay, 33))
        #expect(ed.set(EditMix.reverb, 50))
        #expect(ed.set(EditMix.chorus, 7))
        await ed.settle()
        let got = try #require(await onTrack(t))
        #expect(got.pan == pan && got.delay == 33 && got.reverb == 50 && got.chorus == 7)
        #expect(!a.noMix, "the emulated FIZZ takes the mix live: no fallback")
        // the read-back keeps them
        try? await Task.sleep(for: .milliseconds(1400))
        #expect(ed.value(EditMix.pan) == pan && ed.value(EditMix.reverb) == 50)
        await ed.revert()
        await ed.settle()
        ed.hide()
        #expect(await until { !ed.following })

        // + NEW: an FM sound (INIT, reverb 20) and a MIDI sound, each SAVE AS NEW into the first free MY place
        for midi in [false, true] {
            let free = try #require(ed.firstFreeMine)
            m.editTarget = .new(midi: midi)
            await ed.show()
            #expect(ed.unsavedNew && ed.sound?.isMidi == midi)
            if !midi {
                #expect(ed.sound?.sends.reverb == 20)
                #expect(ed.set(EditMix.pan, 9))
            }
            #expect(await ed.save(asNew: true) == .placed(free))
            #expect(!ed.unsavedNew && ed.place.slot == free)
            let f = try #require((try? await m.link?.dump(source: .mine, slot: free)) ?? nil)
            let back = try #require(try await FizzDataShared.engine().editSound(frame: f))
            #expect(back.name == (midi ? "MIDI" : "INIT") && back.isMidi == midi)
            if midi {
                #expect(back.midi?.enumerated().allSatisfy { $0.element.cc == 16 + $0.offset && !$0.element.touched } == true)
            } else {
                #expect(back.pan == 9 && back.sends.reverb == 20)
            }
            ed.hide()
            #expect(await until { !a.borrowed && a.pending == nil })
        }

        m.stop()
        await host.close()
    }
}
