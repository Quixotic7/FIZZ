// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// I6: the PROJECTS tab against the demo unit: the store read into the model, the slots' moves, copy / paste / clear /
// undo waiting, KEEP and LOAD INTO, a pattern copied out of a kept project, a .fizzpat out and in, and WRITE TO FM-1
// (refused while playing; the store as read kept first; the unit's store then has the paste; read again).

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

private func tempDir() -> URL {
    let u = FileManager.default.temporaryDirectory.appendingPathComponent("fizz-test-\(UUID().uuidString)", isDirectory: true)
    try? FileManager.default.createDirectory(at: u, withIntermediateDirectories: true)
    return u
}

@MainActor
private func connected() async -> (UnitModel, DemoUnit) {
    let src = DemoLinkSource(.fizz, bulkMs: 0...0.2)
    let m = UnitModel(source: src, backups: tempDir(), projects: tempDir())
    m.start()
    _ = await waitFor { m.dumpsLoaded && m.layout != nil }
    return (m, src.demo.units[0])
}

/// The question asked, answered yes.
@MainActor
private func yes(_ pm: ProjectsModel) async {
    guard let a = pm.ask else { return }
    pm.ask = nil
    await a.run()
}

@Suite("Projects", .serialized) @MainActor
struct ProjectsModelTests {
    @Test func readsTheStoreAndEditsWaiting() async throws {
        let (m, _) = await connected()
        let pm = m.projectsTab
        await pm.show()
        let v = try #require(pm.view)
        #expect(v.nproj == 8 && pm.sel == 0 && !pm.stale)
        #expect(pm.slot(0).name == "NOIR" && pm.slot(0).current && pm.slot(0).used == 18)
        #expect(pm.slot(2).demo && v.project(2)?.pats[0]?.demo == true)
        #expect(!pm.slot(4).there && pm.slot(4).used == 0)

        // SUNDAY A03..A05 copied, pasted at NOIR C01 (empty: no question), A01 cleared (asked), undone
        pm.select(1)
        pm.selectRange(from: 4, to: 2)
        #expect(pm.pats == [2, 3, 4])
        await pm.copy()
        #expect(pm.clip?.n == 3 && pm.clip?.range == "A03–A05")
        pm.select(0)
        pm.tap(32)
        pm.paste()
        #expect(await waitFor { pm.ops.count == 1 })
        #expect(pm.pats == [32, 33, 34] && pm.pasted(0) == [32, 33, 34] && pm.slot(0).changed && pm.slot(0).used == 21)
        #expect(pm.view?.poolAdded ?? 0 > 0, "SUNDAY's pool sounds added to NOIR's pool")
        pm.tap(0)
        pm.clear()
        #expect(pm.ask?.title == "CLEAR")
        await yes(pm)
        #expect(pm.ops.count == 2 && pm.cleared(0) == [0] && pm.slot(0).used == 20)
        pm.tap(16)
        pm.paste()
        #expect(pm.ask?.title == "PASTE OVER", "B01 has a pattern")
        pm.ask = nil
        await pm.undo()
        #expect(pm.ops.count == 1 && pm.slot(0).used == 21)
        pm.discard()
        await yes(pm)
        #expect(pm.ops.isEmpty && pm.slot(0).used == 18)
        // the demos' own refused, a short state said
        pm.select(2)
        pm.tap(0)
        pm.clear()
        await yes(pm)
        #expect(pm.ops.isEmpty && pm.note?.text == "THE DEMOS' OWN")
        m.stop()
    }

    @Test func slotMovesOnTheUnit() async throws {
        let (m, demo) = await connected()
        let pm = m.projectsTab
        await pm.show()
        pm.select(1)
        pm.startCopy()
        #expect(pm.arm?.kind == .copy)
        pm.setArmMove(true)
        await pm.tapSlot(4)                                   // (empty: no question)
        #expect(pm.arm == nil && pm.slot(4).name == "SUNDAY" && !pm.slot(1).there && pm.sel == 4)
        pm.select(4)
        pm.startCopy()
        await pm.tapSlot(0)                                   // (NOIR there: REPLACE asked)
        #expect(pm.ask?.go == "REPLACE")
        pm.ask = nil
        await pm.rename(to: "sunday two")
        #expect(pm.slot(4).name == "SUNDAY TWO")
        pm.select(1)
        await pm.newProject()
        #expect(pm.slot(1).there && pm.slot(1).name == "PROJECT 2")
        pm.delete()
        await yes(pm)
        #expect(!pm.slot(1).there)
        pm.select(5)
        await pm.load()
        #expect(pm.slot(5).current)
        demo.setPlaying(true)
        pm.select(0)
        await pm.load()
        #expect(pm.note?.text == "STOP THE FM-1")
        demo.setPlaying(false)
        m.stop()
    }

    @Test func keptProjectsAndFiles() async throws {
        let (m, _) = await connected()
        let pm = m.projectsTab
        await pm.show()
        pm.select(6)
        await pm.keep()
        let k = try #require(pm.kept.items.first)
        #expect(k.name == "WINTER LP" && k.info.pats == 11 && pm.keptSel == k.id)
        #expect(FileManager.default.fileExists(atPath: k.url.path) && k.url.pathExtension == "fizzprj")

        // LOAD INTO the empty slot 5: waits as a change
        pm.startLoad(k)
        await pm.tapSlot(4)
        #expect(pm.ops.count == 1 && pm.ops[0].kind == .load && pm.slot(4).there && pm.slot(4).name == "WINTER LP" && pm.sel == 4)
        await pm.undo()
        // a pattern copied straight out of the kept project
        await pm.copy(from: k, ks: [2])
        #expect(pm.clip?.n == 1 && pm.clip?.from == "WINTER LP" && pm.clip?.ks == [2])
        // rename, then delete
        await pm.renameKept(k, to: "winter two")
        #expect(pm.kept.items.map(\.name) == ["WINTER TWO"])
        // .fizzpat out, and in again (pasted over the selection)
        pm.select(3)
        pm.selectRange(from: 0, to: 1)
        let url = try #require(await pm.fizzpatFile())
        #expect(url.lastPathComponent == "SKETCH A01-A02.fizzpat")
        pm.tap(40)
        #expect(await pm.open(url))
        #expect(await waitFor { pm.ops.count == 1 } && pm.pasted(3) == [40, 41])
        // a .fizzprj from outside kept
        let other = try #require(pm.kept.items.first)
        let copy = FileManager.default.temporaryDirectory.appendingPathComponent("IN \(UUID().uuidString.prefix(4)).fizzprj")
        try FileManager.default.copyItem(at: other.url, to: copy)
        #expect(await pm.open(copy) && pm.kept.items.count == 2 && pm.mode == .kept)
        #expect(await pm.open(URL(fileURLWithPath: "/tmp/x.fizzbak")) == false)
        pm.deleteKept(other)
        await yes(pm)
        #expect(pm.kept.items.count == 1)
        m.stop()
    }

    @Test func writeToTheUnit() async throws {
        let (m, demo) = await connected()
        let pm = m.projectsTab
        await pm.show()
        pm.select(0)
        pm.selectRange(from: 0, to: 1)
        await pm.copy()
        pm.select(3)
        pm.tap(20)
        pm.paste()
        #expect(await waitFor { pm.ops.count == 1 })
        let want = pm.view?.project(3)?.pats[20]

        demo.setPlaying(true)
        await pm.write()
        #expect(pm.note?.text == "STOP THE FM-1" && pm.ops.count == 1 && m.backups.store.entries.isEmpty)
        demo.setPlaying(false)

        var sawWriting = false
        let t = Task { await pm.write() }
        for _ in 0..<12000 {
            if pm.writing != nil, m.actions.restoreRun?.phase == .writing { sawWriting = true }
            if sawWriting && pm.writing == nil { break }
            try await Task.sleep(for: .milliseconds(5))
        }
        await t.value
        #expect(sawWriting)
        #expect(pm.ops.isEmpty && pm.note?.text == "1 CHANGE WRITTEN", "\(String(describing: pm.note))")
        #expect(m.backups.store.entries.first?.name.hasPrefix("FIZZ before projects ") == true)
        // read again from the unit: SKETCH has NOIR's A01 at B05 now
        #expect(!pm.stale && pm.view?.project(3)?.pats[20] == want && pm.slot(3).used == 5)
        #expect(m.actions.restoreFailure == nil)
        m.stop()
    }

    /// UX pass: the write slide from anywhere (the knob at the end as the finger reaches the slide's end), and the
    /// pattern grid's tiles made shorter, never under the yellow bar.
    @Test func slideAndGridFit() {
        // from the knob: one to one; from the middle: the rest of the slide is the whole travel
        #expect(WriteSlide.knob(100, from: 27, width: 342, travel: 288) == 100)
        #expect(WriteSlide.knob(167, from: 167, width: 342, travel: 288) == 288)
        #expect(WriteSlide.knob(-20, from: 100, width: 342, travel: 288) == 0)
        #expect(WriteSlide.knob(400, from: 27, width: 342, travel: 288) == 288)
        // a phone's height: full tiles without changes; the grid ends above the selection's bar and the yellow bar
        #expect(ProjectsPatternsView.tileH(744, pending: false) == 40)
        let h = ProjectsPatternsView.tileH(760, pending: true)
        #expect(h < 40 && h >= 28)
        #expect(4 + 61 + 53 + 8 * h + 4 * 5 + 3 * 12 <= 760 - ProjectsPatternsView.below(pending: true))
        #expect(ProjectsPatternsView.tileH(400, pending: true) == 28)
    }
}
