// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// EDIT against the demo unit (phase I5): the editor on the unit's track (followed, read back, another track selected
// on the unit), PARAMs onto the track (REVERT puts it back), a preset opened (heard on the track: EditBorrowTests),
// SAVE / SAVE AS NEW, a MIDI sound's slots.

import Foundation
import Testing
import FizzData
import FizzKit
@testable import FizzUI

@MainActor
private func waitFor(seconds: Double = 20, _ ok: () -> Bool) async -> Bool {
    let end = Date().addingTimeInterval(seconds)
    while Date() < end {
        if ok() { return true }
        try? await Task.sleep(for: .milliseconds(20))
    }
    return ok()
}

@MainActor
private func connected() async -> (UnitModel, DemoUnit) {
    let dir = FileManager.default.temporaryDirectory.appendingPathComponent("fizz-edit-\(UUID().uuidString)", isDirectory: true)
    try? FileManager.default.createDirectory(at: dir, withIntermediateDirectories: true)
    let src = DemoLinkSource(.fizz, bulkMs: 0...0.3)
    let m = UnitModel(source: src, backups: dir)
    m.start()
    _ = await waitFor { m.dumpsLoaded && m.layout != nil && m.actions.trackCapable }
    return (m, src.demo.units[0])
}

/// The demo track's values as FIZZ would read them (its DUMP through sound.js).
private func trackValues(_ m: UnitModel, _ slot: Int) async -> [Int] {
    guard let f = (try? await m.link?.dumpTrack(slot: slot)) ?? nil,
          let s = try? await FizzDataShared.engine().editSound(frame: f) else { return [] }
    return s.values
}

@Suite("EDIT", .serialized) @MainActor
struct EditModelTests {
    @Test func followsTheTrack() async throws {
        let (m, demo) = await connected()
        let e = m.editor
        await e.show()
        #expect(e.following && e.track == 2 && e.unitSelected == 2)
        #expect(e.sound?.name == demo.trackSounds[2].name)
        #expect(e.place.label == "MY PRESETS 3" && e.place.source == "mine" && e.place.slot == 3 && e.place.fromTarget)
        #expect(e.table?.count == 115)

        // the track edited live, whatever TRACK is (EDIT has no TRACK switch)
        #expect(e.live && !m.actions.trackOn && e.heard == .track(2))

        // the unit's knob turned: the read-back takes it
        demo.state.lock.withLock { demo.state.tracks[2].name = "KNOB" }
        #expect(await waitFor(seconds: 5) { e.sound?.name == "KNOB" })

        // another track selected on the unit: followed
        demo.state.lock.withLock { demo.state.selected = 4 }
        #expect(await waitFor(seconds: 5) { e.track == 4 })
        #expect(e.sound?.name == demo.trackSounds[4].name && e.place.label == "MY PRESETS 5")
        e.hide()
    }

    @Test func paramsAndRevert() async throws {
        let (m, _) = await connected()
        let e = m.editor
        await e.show()
        #expect(e.live)
        let before = await trackValues(m, 3)
        let fdbk = e.value(6), algo = e.value(0)
        #expect(e.set(6, fdbk == 77 ? 78 : 77))
        #expect(e.set(0, algo % 8 + 1))
        #expect(e.dirty)
        await e.settle()
        var now = await trackValues(m, 3)
        #expect(now[6] == e.value(6) && now[0] == e.value(0), "PARAMs onto the track")

        // the read-back waits while edits go out, then sees the same values: nothing pulled back
        try? await Task.sleep(for: .milliseconds(1400))
        #expect(e.value(6) == (fdbk == 77 ? 78 : 77) && e.dirty)

        await e.revert()
        await e.settle()
        now = await trackValues(m, 3)
        #expect(now == before, "REVERT: the track as it was")
        #expect(!e.dirty)
        e.hide()
    }

    @Test func presetOpenedAndSaved() async throws {
        let (m, demo) = await connected()
        let e = m.editor, a = m.actions
        let own = demo.trackSounds[2].name
        let r = try #require(m.mine.first)
        m.editTarget = .preset(r)
        await e.show()
        #expect(m.editTarget == nil)
        #expect(a.borrowed && e.following && e.place.label == "MY PRESETS 1" && e.sound?.name == r.name)
        #expect(e.canSave && e.canSaveNew)
        let lev = e.value(46)
        #expect(e.set(46, lev == 100 ? 101 : 100))
        e.setName("my new one")
        #expect(e.sound?.name == "MY NEW ONE" && e.dirty)

        let n = m.mine.count
        let x = await e.save(asNew: true)
        guard case .placed(let slot) = x else { Issue.record("SAVE AS NEW: \(x)"); return }
        #expect(m.mine.count == n + 1 && m.mine.contains { $0.slot == slot && $0.name == "MY NEW ONE" })
        #expect(e.place.label == "MY PRESETS \(slot)" && !e.dirty && e.opened == e.place && a.borrowed)

        // SAVE over it (the screen asked first)
        e.toggleTag(4)
        #expect(await e.save(asNew: false) == .placed(slot))
        #expect(demo.state.lock.withLock { demo.state.mine[slot]?.tags ?? 0 } & 16 != 0)

        // another preset opened: heard in its place, the editor on the track; EDIT left: the track's own back
        let r2 = m.mine[1]
        await e.open(.preset(r2))
        #expect(a.pending?.name == r2.name && a.borrowed)
        #expect(e.following && e.sound?.name == r2.name)
        e.hide()
        #expect(await waitFor { a.pending == nil })
        #expect(demo.trackSounds[2].name == own)
    }

    @Test func midiSoundSlots() async throws {
        let (m, _) = await connected()
        let e = m.editor
        var b = [UInt8](repeating: 0, count: 128)
        b[0] = 0x51; b[1] = 6
        for k in 0..<28 { b[2 + 28 + k] = UInt8(16 + k) }
        b[2] = 100 | 128
        b[124] = 0x4d; b[125] = 0x49; b[126] = 0x44; b[127] = 0x49
        m.editTarget = .sound(FizzSound(name: "MIDI ONE", blob: b), label: "midi.fizzsnd")
        await e.show()
        #expect(e.sound?.isMidi == true && e.place.source == "file")
        #expect(e.sound?.midi?[0] == EditMidiSlot(cc: 16, value: 100, touched: true))
        #expect(e.setSlot(1, value: 64, touched: true))
        e.untouch(0)
        #expect(e.sound?.midi?[0].touched == false && e.sound?.midi?[1].value == 64)
        let p = try await FizzDataShared.engine().editPack(try #require(e.sound))
        #expect(p.blob[3] == 64 | 128 && p.blob[2] == 0)
        e.hide()
    }

    /// A turn's speed: whole near its axis, slower as the finger moves away sideways (a quarter at 180 pt and past);
    /// a drag of nothing turns nothing (counted from where the turn began: no jump).
    @Test func turnSpeedAndTravel() async throws {
        let (m, _) = await connected()
        let e = m.editor
        await e.show()
        #expect(TurnModifier.fine(0) == 1 && TurnModifier.fine(30) == 1)
        #expect(abs(TurnModifier.fine(80) - 0.5) < 0.001 && TurnModifier.fine(180) == 0.25 && TurnModifier.fine(400) == 0.25)
        #expect(TurnModifier.turned(e, QV.ptim, 40, 0, nil) == 40)
        // (TIME 0..127: its whole range a 600 pt drag at most, 240 at least)
        let up = try #require(TurnModifier.turned(e, QV.ptim, 40, 60, nil))
        #expect(up > 40 && up < 60)
        e.hide()
    }
}
