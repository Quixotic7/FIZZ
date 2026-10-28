// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// Phase I4 against the demo unit: RESTORE from a backup's bytes (the plan, the numbers, the slide's restore with its
// safety backup), ADD PRESETS from a presets backup (the ones there skipped), IMPORT (the demo's Digitone and FIZZ
// files read, SEND, the duplicates after it, a row heard on the track).

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
    let dir = FileManager.default.temporaryDirectory.appendingPathComponent("fizz-i4-\(UUID().uuidString)", isDirectory: true)
    try? FileManager.default.createDirectory(at: dir, withIntermediateDirectories: true)
    let src = DemoLinkSource(.fizz, bulkMs: 0...0.3)
    let m = UnitModel(source: src, backups: dir)
    m.start()
    _ = await waitFor { m.dumpsLoaded && m.layout != nil && m.actions.trackCapable }
    return (m, src.demo.units[0])
}

@Suite("RESTORE, ADD, IMPORT", .serialized) @MainActor
struct TransferTests {
    @Test func restoreFromABackup() async throws {
        let (m, demo) = await connected()
        let e = try FizzDataShared.engine()
        let image = await demo.store.image(for: demo)
        let file = try await e.makeFizzbak(FizzbakInput(kind: .full, image: image, region: FizzStoreRegion(lo: 0x97000, size: image.count),
                                                       presets: demo.dumpFrames, fizz: "0.12.0-beta.11"))
        #expect(await m.actions.delete(m.mine[0]) == .ok)
        let r = m.restoring
        await r.present(bytes: file, name: "FIZZ backup test.fizzbak", date: Date())
        let o = try #require(r.offer)
        #expect(r.presented && o.full && o.plan.fit == nil && !r.viaBle && !r.mustChoose)
        #expect(r.now == (6, 142) && r.then == (6, 143))
        #expect(r.canRestore)

        await r.restore()
        #expect(!r.presented)
        #expect(m.actions.restoreFailure == nil && m.actions.restoreRun == nil)
        #expect(m.mine.count == 143 && m.mine.first?.name == "SUB DROP")
        #expect(r.safety?.presets == 142, "the safety backup kept first")
        #expect(r.last?.name == "FIZZ backup test.fizzbak")
        m.stop()
    }

    @Test func addPresetsSkipsTheOnesThere() async throws {
        let (m, demo) = await connected()
        let e = try FizzDataShared.engine()
        let file = try await e.makeFizzbak(FizzbakInput(kind: .presets, presets: Array(demo.dumpFrames.prefix(3)), fizz: "0.12.0-beta.11"))
        #expect(await m.actions.delete(m.mine[0]) == .ok)
        let r = m.restoring
        await r.present(bytes: file, name: "FIZZ presets test.fizzbak", date: Date())
        let o = try #require(r.offer)
        #expect(!o.full && r.fresh == [0] && r.picked == [0] && r.room == 256 - 142)
        #expect(r.then == (6, 143) && r.canAdd && !r.canRestore)
        r.togglePick(0)
        #expect(r.picked.isEmpty && !r.canAdd)
        r.togglePick(0)
        await r.add()
        #expect(r.added == AddResult(added: 1, same: 0, full: false, outcome: .ok))
        #expect(m.mine.count == 143 && m.mine.contains { $0.name == "SUB DROP" })
        r.close()
        #expect(!r.presented)
        m.stop()
    }

    @Test func importSendAndHear() async throws {
        let (m, demo) = await connected()
        let im = m.importer
        var files = try await DemoImports.files()
        files.append(("notes.txt", [1, 2, 3]))
        await im.add(files: files)
        #expect(im.rows.count == 12 && im.rows.allSatisfy { $0.item.ok && $0.on && $0.dup.isEmpty })
        #expect(im.refused == ["notes.txt"])
        #expect(im.formats == ["dnsnd", "dn2pst", "dn2snd", "fizzlib"])
        #expect(Set(im.rows.map(\.item.kind)) == [.dn1, .dn2, .fizz])
        #expect(im.rows.contains { $0.fidelity < 10 } && im.rows.filter { $0.item.kind == .fizz }.allSatisfy { $0.fidelity == 10 })

        // TRACK only with the switch on; a row heard, then the track's own sound back
        im.pick(.track)
        #expect(im.target == .mine)
        await m.actions.setTrackOn(true)
        im.pick(.track)
        im.pick(.track)
        #expect(im.target == .track && im.track == 1)
        let warm = try #require(im.rows.first { $0.item.sound?.name == "WARM SUB" })
        await im.play(warm.id)
        #expect(im.isHeard(warm.id) && demo.trackSounds[2].name == "WARM SUB")
        await im.play(warm.id)
        #expect(!im.isHeard(warm.id))

        // two left out, the rest sent (the first onto track 2)
        im.toggle(im.rows[0].id)
        im.toggle(im.rows[1].id)
        #expect(im.ticked.count == 10 && im.canSend)
        await im.send()
        #expect(m.mine.count == 153)
        #expect(im.rows.filter { if case .sent = $0.result { true } else { false } }.count == 10 && im.ticked.isEmpty)
        #expect(demo.trackSounds[1].name == im.rows[2].item.sound?.name)

        // the same files again: those now in MY PRESETS as they were sent (FIZZ's own at least; a version-5
        // Digitone patch comes back from the DUMP as version 6) and the two still in the list above not ticked
        await im.add(files: Array(files.dropLast()))
        let again = Array(im.rows.suffix(12))
        #expect(again.filter { $0.item.kind == .fizz }.allSatisfy { $0.dup == "mine" })
        #expect(again.filter { $0.dup == "list" }.count == 2)
        #expect(again.filter { !$0.dup.isEmpty }.allSatisfy { !$0.on })
        im.clear()
        #expect(im.rows.isEmpty)
        m.stop()
    }
}
