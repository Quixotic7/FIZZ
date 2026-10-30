// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// LIBRARY's SELECT on the unit's presets (mock-ups 41, 42, 47) against the demo unit: the selection, what the bar
// counts, TAG on three (and a factory one), + POOL / − POOL (the used places asked once), DELETE with a project using
// one (the factory's stay). The emulator's run: FizzEngineTests/LibrarySelectEmulatorTests.swift.

import Foundation
import SwiftUI
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
    let dir = FileManager.default.temporaryDirectory.appendingPathComponent("fizz-sel-\(UUID().uuidString)", isDirectory: true)
    let src = DemoLinkSource(.fizz, bulkMs: 0...0.3)
    let m = UnitModel(source: src, backups: dir.appendingPathComponent("Backups"), projects: dir.appendingPathComponent("Projects"),
                      presets: dir.appendingPathComponent("Presets"))
    m.start()
    _ = await waitFor { m.dumpsLoaded && m.layout != nil }
    return (m, src.demo.units[0])
}

/// What a flow shows: its toast, its confirm, its DELETE sheet.
@MainActor
private final class Shown {
    var note: ScreenNote?
    var ask: ConfirmAsk?
    var deleting: BulkDeleteAsk?
    var exporting: ExportBox?

    func flow(_ m: UnitModel) -> BulkFlow {
        BulkFlow(model: m, note: Binding(get: { self.note }, set: { self.note = $0 }),
                 ask: Binding(get: { self.ask }, set: { self.ask = $0 }),
                 deleting: Binding(get: { self.deleting }, set: { self.deleting = $0 }),
                 exporting: Binding(get: { self.exporting }, set: { self.exporting = $0 }))
    }
}

@Suite("LIBRARY's SELECT (mock-ups 41, 42, 47)", .serialized) @MainActor
struct LibrarySelectTests {
    @Test func theSelection() async throws {
        let (m, _) = await connected()
        let p = m.picks
        #expect(!p.active)
        p.start()
        #expect(p.active && p.keys.isEmpty)
        let glass = m.mine.filter { $0.name.contains("GLASS") }
        let fac = try #require(m.factory.first)
        for r in glass + [fac] { p.toggle(r) }
        p.toggle(m.pool[0])
        #expect(p.rows(m.rows).map(\.slot) == glass.map(\.slot) + [fac.slot], "the list's order; a pool row is never ticked")
        // the bar's counts: DELETE the MY ones, + POOL the ones not in it (GLASS BELL is at place 3)
        let c = BulkCounts(p.rows(m.rows), places: poolPlaces(m.rows))
        #expect(c.n == glass.count + 1 && c.mine.count == glass.count && c.unit.count == c.n)
        #expect(c.inPool.contains { $0.source == "mine" && $0.slot == 2 } && c.inPool.count + c.outPool.count == c.n)
        #expect(c.outPool.allSatisfy { m.actions.places(of: $0.id).isEmpty })
        #expect(c.have(.pluck) == glass.filter { $0.tags & 4 != 0 }.count)
        // ALL: every row shown ticked; again: none of them
        p.toggleAll(Array(m.mine.prefix(5)))
        #expect(p.rows(m.rows).count == Set(glass.map(\.slot) + [1, 2, 3, 4, 5]).count + 1)
        p.toggleAll(Array(m.mine.prefix(5)))
        #expect(!p.rows(m.rows).contains { $0.source == "mine" && $0.slot <= 5 })
        p.clear()
        #expect(!p.active && p.keys.isEmpty && !m.libraryBarUp)
        m.stop()
    }

    /// TAG: three MY presets and a factory one get PAD (it replaces their tags), one edit each; again: nothing to do.
    @Test func tagThree() async throws {
        let (m, demo) = await connected()
        let rows = Array(m.mine.filter { $0.name.contains("GLASS") }.prefix(3)) + [try #require(m.factory.first { $0.tags != 8 })]
        let d = await m.actions.bulkTag(rows, mask: 8)
        #expect(d == BulkDone(n: 4, stopped: nil))
        let now = rows.compactMap { r in m.rows.first { $0.source == r.source && $0.slot == r.slot } }
        #expect(now.count == 4 && now.allSatisfy { $0.tags == 8 })
        #expect(demo.minePresets.filter { p in rows.contains { $0.source == "mine" && $0.slot == p.slot } }.allSatisfy { $0.tags == 8 })
        #expect(await m.actions.bulkTag(now, mask: 8) == BulkDone(n: 0, stopped: nil), "they have it already")
        // the flow: the toast counts them all and names the tag; NONE clears
        let s = Shown()
        m.picks.start()
        for r in now { m.picks.toggle(r) }
        s.flow(m).tag(nil)
        #expect(await waitFor { s.note != nil })
        #expect(s.note?.text == "4 · NONE" && m.picks.run == nil)
        #expect(m.picks.rows(m.rows).allSatisfy { $0.tags == 0 })
        m.stop()
    }

    /// + POOL: the ones not in it, each into the first free place; − POOL: every place, the ones steps lock (the
    /// demo: every third place) asked once, then cleared with CONFIRM.
    @Test func poolThemAndBack() async throws {
        let (m, demo) = await connected()
        let rows = Array(m.mine.filter { $0.name.contains("GLASS") }.prefix(3))
        let before = m.pool.count
        let add = await m.actions.bulkPoolAdd(rows)
        #expect(add == BulkDone(n: 2, stopped: nil), "GLASS BELL was in already")
        #expect(rows.allSatisfy { !m.actions.places(of: $0.id).isEmpty })
        #expect(m.pool.count == before + 2 && demo.poolPlaces.count == before + 2)

        let s = Shown()
        m.picks.start()
        for r in rows { m.picks.toggle(r) }
        s.flow(m).poolRemove()
        #expect(await waitFor { s.ask != nil || s.note != nil })
        let ask = try #require(s.ask, "place 3 (GLASS BELL) is locked by steps: asked once")
        #expect(ask.title.hasPrefix("CLEAR ") && ask.go == "CLEAR")
        #expect(demo.poolPlaces[3] == rows[0].id, "nothing used cleared before the confirm")
        ask.action()
        #expect(await waitFor { s.note != nil })
        #expect(s.note?.text == "3 OUT OF THE POOL")
        #expect(rows.allSatisfy { m.actions.places(of: $0.id).isEmpty })
        #expect(!demo.poolPlaces.values.contains { v in rows.contains { $0.id == v } })
        m.stop()
    }

    /// DELETE: the MY presets only, one ask naming the used ones and their projects; the factory's stay.
    @Test func deleteWithAProjectUsingOne() async throws {
        let (m, demo) = await connected()
        let bell = try #require(m.mine.first { $0.slot == 2 })     // (in the pool at place 3: the open project uses it)
        let two = Array(m.mine.filter { $0.slot != 2 && m.actions.places(of: $0.id).isEmpty }.prefix(2))
        let fac = try #require(m.factory.first)
        let s = Shown()
        m.picks.start()
        for r in [bell] + two + [fac] { m.picks.toggle(r) }
        let f = s.flow(m)
        f.askDelete()
        let ask = try #require(s.deleting)
        #expect(ask.mine.map(\.slot) == ([bell] + two).map(\.slot).sorted() && ask.factory == 1)
        _ = await m.actions.bulkUsers(ask.mine)
        let u = BulkUsage(ask.mine, users: m.actions.users)
        #expect(u.used.map(\.slot) == [2])
        #expect(u.projects.contains { $0.project == demo.projectSlots.current } && u.steps >= DemoState.steps(3))
        f.delete(ask)
        #expect(await waitFor { s.note != nil })
        #expect(s.note?.text == "DELETED 3")
        #expect(!m.mine.contains { r in ask.mine.contains { $0.slot == r.slot && $0.id == r.id } })
        #expect(!demo.minePresets.contains { p in ask.mine.contains { $0.slot == p.slot } })
        #expect(m.factory.contains { $0.slot == fac.slot }, "the factory's stay")
        #expect(m.picks.rows(m.rows).map(\.source) == ["factory"], "the deleted untick")
        #expect(demo.poolPlaces[3] == nil, "its pool place went with it")
        m.stop()
    }

    /// A refusal stops the rest and says so after the count.
    @Test func busyStopsTheRest() async throws {
        let (m, demo) = await connected()
        demo.setPlaying(true)
        let d = await m.actions.bulkDelete(Array(m.mine.prefix(3)))
        #expect(d.n == 0 && d.stopped == .refused(.busy))
        #expect(BulkFlow.toast("DELETED 0", FZ.red, d)?.kind == .refused)
        #expect(BulkFlow.toast("2 → POOL", FZ.green, BulkDone(n: 2, stopped: .refused(.full)))?.text == "2 → POOL · FULL")
        #expect(demo.minePresets.count == m.mine.count)
        demo.setPlaying(false)
        m.stop()
    }
}
