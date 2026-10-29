// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// EDIT's borrow (mock-up 37) against the demo unit: a preset opened is heard on the selected track at once (no KEEP /
// CANCEL bar), its edits PARAMs there; leaving EDIT puts the track's own sound back, coming back hears it again with
// its edits; KEEP ON Tn leaves it; another track picked moves it; the track's own sound again; a MIDI sound silent;
// LIBRARY's audition keeps its bar; a unit change undoes it. (The emulator's own run: FizzEngineTests.)

import Foundation
import Testing
import FizzData
import FizzKit
@testable import FizzUI

@MainActor
private func waitFor(seconds: Double = 10, _ ok: () -> Bool) async -> Bool {
    let end = Date().addingTimeInterval(seconds)
    while Date() < end {
        if ok() { return true }
        try? await Task.sleep(for: .milliseconds(20))
    }
    return ok()
}

@MainActor
private func connected() async -> (UnitModel, DemoUnit) {
    let dir = FileManager.default.temporaryDirectory.appendingPathComponent("fizz-borrow-\(UUID().uuidString)", isDirectory: true)
    try? FileManager.default.createDirectory(at: dir, withIntermediateDirectories: true)
    let src = DemoLinkSource(.fizz, bulkMs: 0...0.3)
    let m = UnitModel(source: src, backups: dir, presets: dir.appendingPathComponent("Presets"))
    m.start()
    _ = await waitFor { m.dumpsLoaded && m.layout != nil && m.actions.trackCapable }
    return (m, src.demo.units[0])
}

/// The demo track's values as FIZZ would read them.
private func trackValues(_ m: UnitModel, _ slot: Int) async -> [Int] {
    guard let f = (try? await m.link?.dumpTrack(slot: slot)) ?? nil,
          let s = try? await FizzDataShared.engine().editSound(frame: f) else { return [] }
    return s.values
}

private func trackName(_ d: DemoUnit, _ t: Int) -> String { d.state.lock.withLock { d.state.tracks[t].name } }

@Suite("EDIT borrow", .serialized) @MainActor
struct EditBorrowTests {
    @Test func heardOnTheTrackThenItsOwnBack() async throws {
        let (m, demo) = await connected()
        let e = m.editor, a = m.actions
        #expect(!a.trackOn)
        let own = trackName(demo, 2)
        let r = try #require(m.mine.first { $0.slot == 3 })
        m.editTarget = .preset(r)
        await e.show()
        // heard at once on the selected track (3), no TRACK switch needed, no KEEP / CANCEL bar
        #expect(a.borrowed && a.pending?.track == 2)
        #expect(e.heard == .borrowed(2) && e.following && e.live && e.opened != nil)
        #expect(e.place.label == "MY PRESETS 3" && e.canSave)
        #expect(trackName(demo, 2) == r.name)

        // a value turned: a PARAM onto the borrowed sound
        let fdbk = e.value(6), v = fdbk == 77 ? 78 : 77
        #expect(e.set(6, v))
        await e.settle()
        let turned = await trackValues(m, 3)
        #expect(turned[6] == v)
        #expect(e.dirty)

        // EDIT left: the track's own sound back by itself
        e.hide()
        #expect(await waitFor { !a.borrowed && a.pending == nil })
        #expect(trackName(demo, 2) == own)
        #expect(!e.following && e.heard == .silent && e.sound?.name == r.name && e.value(6) == v)

        // EDIT again: heard again, as it was left (edited, the sound to go back to the preset's)
        await e.show()
        #expect(a.borrowed && e.heard == .borrowed(2))
        #expect(trackName(demo, 2) == r.name && e.value(6) == v && e.dirty)
        let again = await trackValues(m, 3)
        #expect(again[6] == v)
        await e.revert()
        await e.settle()
        let reverted = await trackValues(m, 3)
        #expect(!e.dirty && reverted[6] == fdbk, "REVERT: the preset as it was, on the track")

        // KEEP ON T3: it stays; the track's from now on
        await e.keepOn()
        #expect(!a.borrowed && a.pending == nil && e.opened == nil && e.heard == .track(2))
        e.hide()
        #expect(await waitFor { !e.following })
        #expect(trackName(demo, 2) == r.name)
        await e.show()
        #expect(e.heard == .track(2) && !a.borrowed && e.sound?.name == r.name)
        e.hide()
        m.stop()
    }

    @Test func anotherTrackAnotherSoundItsOwnSound() async throws {
        let (m, demo) = await connected()
        let e = m.editor, a = m.actions
        let own2 = trackName(demo, 2), own4 = trackName(demo, 4)
        let r = m.mine[0], r2 = m.mine[1]
        m.editTarget = .preset(r)
        await e.show()
        #expect(a.borrowed && trackName(demo, 2) == r.name)

        // another sound opened: it takes the borrow's place, the track's own sound still kept
        m.editTarget = .preset(r2)
        await e.show()
        #expect(a.borrowed && a.pending?.track == 2 && trackName(demo, 2) == r2.name && e.sound?.name == r2.name)

        // another track picked (the chip's menu): T3 its own again, the sound heard on T5
        await e.follow(slot: 5)
        #expect(a.borrowed && a.pending?.track == 4 && e.heard == .borrowed(4))
        #expect(trackName(demo, 2) == own2 && trackName(demo, 4) == r2.name)
        #expect(e.trackShown == 4)

        // the track's own sound again (the chip's menu): editing T5 itself
        await e.ownSound()
        #expect(!a.borrowed && a.pending == nil && e.opened == nil)
        #expect(trackName(demo, 4) == own4 && e.heard == .track(4) && e.sound?.name == own4)
        e.hide()
        #expect(await waitFor { !e.following })
        m.stop()
    }

    @Test func aMidiSoundIsSilentAndLibraryKeepsItsBar() async throws {
        let (m, demo) = await connected()
        let e = m.editor, a = m.actions
        var b = [UInt8](repeating: 0, count: 128)
        b[0] = 0x51; b[1] = 6
        for k in 0..<28 { b[2 + 28 + k] = UInt8(16 + k) }
        b[124] = 0x4d; b[125] = 0x49; b[126] = 0x44; b[127] = 0x49
        let own = trackName(demo, 2)
        m.editTarget = .sound(FizzSound(name: "MIDI ONE", blob: b), label: "midi.fizzsnd")
        await e.show()
        #expect(e.sound?.isMidi == true && e.heard == .silent && e.silent == "MIDI")
        #expect(!a.borrowed && a.pending == nil && trackName(demo, 2) == own)
        #expect(e.setSlot(1, value: 64, touched: true), "edited silently")
        e.hide()

        // LIBRARY's TRACK on: its audition still has KEEP / CANCEL (not a borrow)
        await a.setTrackOn(true)
        #expect(await a.pick(m.mine[2]) == .ok)
        #expect(a.pending != nil && !a.borrowed)
        #expect(await a.cancelAudition())
        await a.setTrackOn(false)
        m.stop()
    }

    @Test func editShownBeforeTheUnitIsReady() async throws {
        let dir = FileManager.default.temporaryDirectory.appendingPathComponent("fizz-borrow-\(UUID().uuidString)", isDirectory: true)
        let src = DemoLinkSource(.fizz, bulkMs: 0...0.3)
        let m = UnitModel(source: src, backups: dir, presets: dir.appendingPathComponent("Presets"))
        let e = m.editor
        await e.show()
        #expect(!e.following && e.heard == .silent)
        m.start()
        #expect(await waitFor { m.actions.trackCapable })
        await e.unitReady()                                 // (EditView calls it when trackCapable turns on)
        #expect(e.following && e.heard == .track(2) && e.sound?.name == trackName(src.demo.units[0], 2))
        e.hide()
        m.stop()
    }

    @Test func aKeptSoundAndTheUnitGoing() async throws {
        let (m, demo) = await connected()
        let e = m.editor, a = m.actions
        let own = trackName(demo, 2)
        // + NEW on KEPT: INIT heard on the track, SAVE writes KEPT
        m.editTarget = .newKept
        await e.show()
        #expect(a.borrowed && e.place.isKept && trackName(demo, 2) == "INIT")
        #expect(e.set(46, 90))
        #expect(await e.save(asNew: false) == .ok)
        #expect(e.place.kept != nil && e.opened == e.place && a.borrowed)

        // the unit about to change (another unit, a restore): undone first
        await e.unitGoes()
        #expect(!a.borrowed && a.pending == nil && trackName(demo, 2) == own)
        e.hide()
        m.stop()
    }
}
