// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// Mock-ups 45, 46 against the demo unit: PAN and the sends (the demo answers PARAM 115..118 NOT FOUND, as an older
// FIZZ: the whole sound goes instead, 120 ms after the last edit, and the link is remembered), the mix carried through
// a .fizzsnd and a STORE frame, + NEW (FM and MIDI) saved as new into the first free MY PRESETS place, leaving EDIT
// with it unsaved asks first. The emulator's own run (mix PARAMs taken live): EditMixEmulatorTests.

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
    let dir = FileManager.default.temporaryDirectory.appendingPathComponent("fizz-mix-\(UUID().uuidString)", isDirectory: true)
    try? FileManager.default.createDirectory(at: dir, withIntermediateDirectories: true)
    let src = DemoLinkSource(.fizz, bulkMs: 0...0.3)
    let m = UnitModel(source: src, backups: dir, projects: dir.appendingPathComponent("Projects"),
                      presets: dir.appendingPathComponent("Presets"))
    m.start()
    _ = await waitFor { m.dumpsLoaded && m.layout != nil && m.actions.trackCapable }
    return (m, src.demo.units[0])
}

private func trackDump(_ m: UnitModel, _ t: Int) async -> FizzDump? {
    guard let f = (try? await m.link?.dumpTrack(slot: t + 1)) ?? nil else { return nil }
    return FizzSysEx.parseDump(f)
}

/// A STORE frame with the DUMP flag, so parseDump reads it.
private func dumped(_ f: [UInt8]) -> [UInt8] {
    var b = f
    b[6] |= UInt8(FizzSysEx.FLAGS.dump)
    let sum = b[5..<(b.count - 2)].reduce(0) { $0 + Int($1) }
    b[b.count - 2] = UInt8((0x80 - (sum & 0x7f)) & 0x7f)
    return b
}

@Suite("EDIT · mix and + NEW", .serialized) @MainActor
struct EditMixTests {
    @Test func theMixTable() async throws {
        let t = try await FizzDataShared.engine().editTable()
        #expect(t.count == 115)
        #expect(t.text(EditMix.pan, -12) == "L12" && t.text(EditMix.pan, 0) == "C" && t.text(EditMix.pan, 63) == "R63")
        #expect(t.text(EditMix.pan, -64) == "L64")
        #expect(t.info(EditMix.pan)?.min == -64 && t.info(EditMix.pan)?.max == 63 && t.info(EditMix.pan)?.def == 0)
        for id in [EditMix.delay, EditMix.reverb, EditMix.chorus] { #expect(t.info(id)?.max == 127 && t.text(id, 40) == "40") }
        #expect(t.info(EditMix.reverb)?.def == 20 && t.info(EditMix.chorus)?.def == 0 && t.info(EditMix.delay)?.def == 0)
        #expect(t.mixDefaults.pan == 0 && t.mixDefaults.sends == FizzSends(chorus: 0, delay: 0, reverb: 20))
        #expect(t.clamp(EditMix.pan, -90) == -64 && t.clamp(EditMix.reverb, 300) == 127)
    }

    /// DUMP -> EDIT -> .fizzsnd -> EDIT, and the STORE frame: pan and the sends with the mix mask.
    @Test func theMixCarried() async throws {
        let e = try FizzDataShared.engine()
        var s = try await e.newSound(midi: false)
        #expect(s.name == "INIT" && s.pan == 0 && s.sends == FizzSends(chorus: 0, delay: 0, reverb: 20) && !s.isMidi)
        s.pan = -23; s.sends = FizzSends(chorus: 11, delay: 22, reverb: 99)
        let lib = try await e.libSound(s)
        #expect(lib.pan == -23 && lib.sends == s.sends)
        let back = try await e.editSound(lib: lib)
        #expect(back.pan == -23 && back.sends == s.sends && back.values == s.values)
        let f = try await e.storeFrame(s, slot: 0, own: true)
        let d = try #require(FizzSysEx.parseDump(dumped(f)))
        #expect(d.pan == -23 && d.chorus == 11 && d.delay == 22 && d.reverb == 99)
        #expect(f[6 + 21] == 0x0f, "the mix mask: pan, chorus, delay, reverb")
        let fromDump = try #require(try await e.editSound(frame: dumped(f)))
        #expect(fromDump.pan == -23 && fromDump.sends == s.sends)
    }

    /// An older FIZZ (the demo): PAN answered NOT FOUND, remembered; the whole sound goes 120 ms after the last edit.
    @Test func olderFizzGetsTheWholeSound() async throws {
        let (m, _) = await connected()
        let e = m.editor, a = m.actions
        await e.show()
        #expect(e.live && e.track == 2 && !a.noMix)
        let t = e.track
        #expect(e.set(EditMix.pan, -12))
        #expect(e.text(EditMix.pan) == "L12" && e.dirty)
        #expect(await waitFor(seconds: 5) { a.noMix }, "NOT FOUND remembered for this link")
        await e.settle()
        #expect(await trackDump(m, t)?.pan == -12, "the whole sound sent instead")
        // (further mix edits: no PARAM, the sound once they pause)
        #expect(e.set(EditMix.reverb, 77))
        #expect(e.set(EditMix.chorus, 9))
        #expect(e.set(EditMix.delay, 31))
        await e.settle()
        let d = try #require(await trackDump(m, t))
        #expect(d.reverb == 77 && d.chorus == 9 && d.delay == 31 && d.pan == -12)
        // the read-back keeps them (nothing pulled back)
        try? await Task.sleep(for: .milliseconds(1400))
        #expect(e.value(EditMix.reverb) == 77 && e.value(EditMix.pan) == -12)
        // a double tap: PAN to C
        e.reset(EditMix.pan)
        #expect(e.value(EditMix.pan) == 0)
        await e.settle()
        #expect(await trackDump(m, t)?.pan == 0)
        // REVERT: the track's own mix back
        await e.revert()
        await e.settle()
        #expect(!e.dirty && e.value(EditMix.reverb) == e.base?.sends.reverb)
        e.hide()
    }

    @Test func newFMSoundSavedAsNew() async throws {
        let (m, demo) = await connected()
        let e = m.editor
        let free = try #require(e.firstFreeMine)
        #expect(!m.mine.contains { $0.slot == free })
        m.editTarget = .new(midi: false)
        await e.show()
        #expect(e.sound?.name == "INIT" && e.place.unsaved && e.unsavedNew && e.place.label == "NEW")
        #expect(e.sound?.sends.reverb == 20 && e.sound?.pan == 0)
        #expect(!e.canSave && e.canSaveNew && !e.dirty)
        #expect(e.set(EditMix.pan, 20))
        let x = await e.save(asNew: true)
        #expect(x == .placed(free))
        #expect(!e.unsavedNew && e.place.label == "MY PRESETS \(free)" && e.place.source == "mine")
        let p = try #require(demo.state.lock.withLock { demo.state.mine[free] })
        #expect(p.name == "INIT" && p.pan == 20 && p.reverb == 20)
        e.hide()
    }

    @Test func newMidiSoundSavedAsNew() async throws {
        let (m, _) = await connected()
        let e = m.editor
        let free = try #require(e.firstFreeMine)
        m.editTarget = .new(midi: true)
        await e.show()
        let s = try #require(e.sound)
        #expect(s.isMidi && s.name == "MIDI" && e.place.unsaved && e.silent == "MIDI")
        #expect(s.midi?.count == 28 && s.midi?.enumerated().allSatisfy { $0.element.cc == 16 + $0.offset && !$0.element.touched } == true)
        #expect(!e.set(EditMix.pan, 10), "a MIDI sound has no mix")
        #expect(await e.save(asNew: true) == .placed(free))
        let f = try #require((try? await m.link?.dump(source: .mine, slot: free)) ?? nil)
        let back = try #require(try await FizzDataShared.engine().editSound(frame: f))
        #expect(back.isMidi && back.name == "MIDI" && back.midi?[3].cc == 19)
        e.hide()
    }

    /// Leaving EDIT with + NEW's sound unsaved: EDIT again, asked; DISCARD drops it and goes on.
    @Test func leavingAsksFirst() async throws {
        let (m, _) = await connected()
        let e = m.editor
        m.editTarget = .new(midi: false)
        await e.show()
        #expect(e.unsavedNew)
        e.tabChanged(from: .edit, to: .library)
        #expect(e.leaving == .library && m.goTo == .edit && e.shown)
        m.goTo = nil
        await e.answerLeaving(.stay)
        #expect(e.leaving == nil && e.unsavedNew && m.goTo == nil)
        e.tabChanged(from: .edit, to: .pool)
        #expect(e.leaving == .pool)
        m.goTo = nil
        await e.answerLeaving(.discard)
        #expect(e.leaving == nil && !e.unsavedNew && m.goTo == .pool)
        e.tabChanged(from: .edit, to: .pool)
        #expect(e.leaving == nil && !e.shown)
        // IMPORT's sheet over EDIT is not leaving it
        m.editTarget = .new(midi: false)
        await e.show()
        e.tabChanged(from: .edit, to: .importer)
        #expect(e.leaving == nil)
        e.hide()
    }
}
