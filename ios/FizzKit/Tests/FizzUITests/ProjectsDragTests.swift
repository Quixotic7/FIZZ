// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// Mock-ups 43, 48: a pattern dragged in PROJECTS against the demo unit (projtab.js land / dropApply, projects.js
// pastePatterns): a copy within a project, a copy into another one (its pool sounds come along, the ones there reused),
// a move (its own place cleared), a full pool refused, a pattern there asked first; the TO… sheet; the drag itself
// over drop zones (a chip held springs its card open, a dot of it taken). Every drop one change waiting.

import CoreGraphics
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
private func opened() async throws -> (UnitModel, ProjectsModel) {
    let src = DemoLinkSource(.fizz, bulkMs: 0...0.2)
    let m = UnitModel(source: src, backups: tempDir(), projects: tempDir())
    m.start()
    _ = await waitFor { m.dumpsLoaded && m.layout != nil }
    let pm = m.projectsTab
    await pm.show()
    _ = try #require(pm.view)
    return (m, pm)
}

@MainActor
private func yes(_ pm: ProjectsModel) async {
    guard let a = pm.ask else { return }
    pm.ask = nil
    await a.run()
}

@MainActor
private func plan(_ pm: ProjectsModel, _ p: Int, _ k: Int) async throws -> FizzDropPlan {
    try await FizzDataShared.engine().projectsDropPlan(try #require(pm.session), p: p, k: k)
}

@Suite("Projects drag", .serialized) @MainActor
struct ProjectsDragTests {
    @Test func copyWithinAndMove() async throws {
        let (m, pm) = try await opened()
        let noir = try #require(pm.view?.project(0))
        let a07 = try #require(noir.pats[6]), free = try #require(pm.firstFree(0))
        #expect(noir.used == 18 && free == 18 && pm.liftable(0, 6))
        #expect(!pm.liftable(0, free) && !pm.liftable(2, 0), "an empty pattern, the demos' own: nothing to lift")

        // a copy onto an empty pattern: no question, one change, the same trigs, the pool as it was
        await pm.land(p: 0, k: 6, q: 0, j: 40, move: false)
        #expect(pm.ask == nil && pm.ops.count == 1 && pm.ops[0].kind == .drop && pm.ops[0].at == [40])
        #expect(pm.view?.project(0)?.pats[40] == a07 && pm.view?.project(0)?.pats[6] == a07)
        #expect(pm.ops[0].poolAdded == 0 && pm.view?.project(0)?.pool == noir.pool, "its own pool: every sound there")
        #expect(pm.pasted(0) == [40] && pm.pats == [40] && pm.note?.text == "A07 → C09")

        // a move: its own place cleared (dashed), one change; UNDO puts it back
        await pm.land(p: 0, k: 7, q: 0, j: 41, move: true)
        #expect(pm.ops.count == 2 && pm.ops[1].move && pm.ops[1].fromP == 0 && pm.ops[1].fromK == 7)
        #expect(pm.view?.project(0)?.pats[7] == nil && pm.view?.project(0)?.pats[41] == noir.pats[7] && pm.cleared(0) == [7])
        #expect(pm.view?.project(0)?.used == 19)
        await pm.undo()
        #expect(pm.ops.count == 1 && pm.view?.project(0)?.pats[7] == noir.pats[7])

        // onto a pattern there: asked first (COPY OVER … REPLACE), nothing until yes
        await pm.land(p: 0, k: 6, q: 0, j: 0, move: false)
        let a = try #require(pm.ask)
        #expect(a.title == "COPY OVER" && a.what == "A07 → NOIR A01" && a.go == "REPLACE" && pm.ops.count == 1)
        await yes(pm)
        #expect(pm.ops.count == 2 && pm.view?.project(0)?.pats[0] == a07)
        await pm.land(p: 0, k: 6, q: 0, j: 1, move: true)
        #expect(pm.ask?.title == "MOVE OVER")
        pm.ask = nil

        // a demo's A01..A03 never move; dropped on itself: nothing
        await pm.land(p: 2, k: 0, q: 0, j: 50, move: true)
        await pm.land(p: 0, k: 6, q: 0, j: 6, move: false)
        #expect(pm.ops.count == 2 && pm.slot(0).changed)
        m.stop()
    }

    @Test func copyAcrossCarriesThePool() async throws {
        let (m, pm) = try await opened()
        let sunday = try #require(pm.view?.project(1)), free = try #require(pm.firstFree(1))
        // a NOIR pattern whose pool sounds SUNDAY lacks (some of them)
        var pick: (k: Int, t: FizzDropPlan.Target, pl: FizzDropPlan)?
        for k in 0..<18 {
            let pl = try await plan(pm, 0, k)
            if let t = pl.target(1), t.added > 0 { pick = (k, t, pl); if t.there > 0 { break } }
        }
        let (k, t, pl) = try #require(pick)
        #expect(t.added + t.there == pl.sounds && pl.colours.count == pl.sounds && t.after == sunday.pool + t.added && !t.full)
        #expect(t.had.filter { !$0 }.count == t.added)

        // dropped on SUNDAY's chip: its first free pattern; the sounds in SUNDAY's pool; the bar's +n POOL
        await pm.land(p: 0, k: k, q: 1, j: nil, move: false)
        #expect(pm.ops.count == 1 && pm.ops[0].q == 1 && pm.ops[0].at == [free] && pm.ops[0].poolAdded == t.added)
        #expect(pm.view?.project(1)?.pool == sunday.pool + t.added && pm.view?.poolAdded == t.added)
        #expect(pm.view?.project(1)?.pats[free] == pm.view?.project(0)?.pats[k] && pm.slot(1).changed && !pm.slot(0).changed)
        #expect(pm.note?.text == "\(FizzClip.patName(k)) → SUNDAY \(FizzClip.patName(free))")
        // again: they are there now, reused
        let again = try await plan(pm, 0, k)
        #expect(again.target(1)?.added == 0 && again.target(1)?.there == pl.sounds)
        await pm.land(p: 0, k: k, q: 1, j: nil, move: false)
        #expect(pm.ops.count == 2 && pm.ops[1].poolAdded == 0 && pm.view?.project(1)?.pool == sunday.pool + t.added)

        // moved across: NOIR's place cleared, both projects changed
        await pm.land(p: 0, k: 2, q: 1, j: nil, move: true)
        #expect(pm.ops.count == 3 && pm.view?.project(0)?.pats[2] == nil && pm.slot(0).changed && pm.cleared(0) == [2])
        // an empty slot takes nothing
        await pm.land(p: 0, k: 3, q: 4, j: nil, move: false)
        #expect(pm.ops.count == 3 && pm.note?.text == "SLOT 5 EMPTY")
        m.stop()
    }

    @Test func aFullPoolRefuses() async throws {
        let (m, pm) = try await opened()
        let winter = try #require(pm.view?.project(6))
        #expect(winter.name == "WINTER LP" && winter.pool == 128)
        var k: Int?
        for x in 0..<18 where try await plan(pm, 0, x).target(6)?.full == true { k = x; break }
        let x = try #require(k, "a NOIR pattern with a sound WINTER LP's pool lacks")
        await pm.land(p: 0, k: x, q: 6, j: nil, move: false)
        #expect(pm.ops.isEmpty && pm.note?.text == "POOL FULL" && pm.view?.project(6)?.pool == 128)

        // the TO… sheet: WINTER LP not taken; FIRST FREE into SUNDAY
        await pm.openTo(p: 0, k: x)
        #expect(pm.to == ProjectsTo(p: 0, k: x, q: 0) && pm.plan?.target(6)?.full == true)
        pm.setToProject(6)
        #expect(pm.to?.q == 0 && pm.note?.text == "POOL FULL")
        pm.setToProject(1)
        pm.setToMove(true)
        let free = try #require(pm.firstFree(1))
        await pm.toPick(nil)
        #expect(pm.to == nil && pm.ops.count == 1 && pm.ops[0].move && pm.ops[0].at == [free] && pm.view?.project(0)?.pats[x] == nil)
        // a pattern there picked: asked first
        let y = x == 10 ? 11 : 10
        await pm.openTo(p: 0, k: y)
        pm.setToProject(1)
        await pm.toPick(0)
        #expect(pm.ask?.title == "COPY OVER" && pm.ask?.what == "NOIR \(FizzClip.patName(y)) → SUNDAY A01")
        pm.ask = nil
        // TO… from the selection: one pattern there
        pm.tap(4)
        #expect(pm.canTo)
        pm.selectRange(from: 4, to: 5)
        #expect(!pm.canTo)
        m.stop()
    }

    @Test func theDragOverZones() async throws {
        let (m, pm) = try await opened()
        // a grid of 64 tiles 40 wide at (0, 200), SUNDAY's chip at (100, 100), its card at (0, 300)
        let z = pm.zones
        z.spring = .card
        z.grids["main"] = .init(q: 0, rect: CGRect(x: 0, y: 200, width: 640, height: 160)) { p in Int(p.y / 40) * 16 + Int(p.x / 40) }
        z.chips[1] = CGRect(x: 100, y: 100, width: 40, height: 34)
        z.card = .init(q: 1, rect: CGRect(x: 0, y: 400, width: 160, height: 40)) { p in p.x < 10 ? nil : Int(p.y / 10) * 16 + Int(p.x / 10) }
        pm.lift(p: 0, k: 6, at: CGPoint(x: 260, y: 210), size: CGSize(width: 40, height: 40), grab: CGPoint(x: 20, y: 10), touch: true)
        #expect(pm.lift?.p == 0 && pm.lift?.k == 6)
        #expect(await waitFor { pm.plan != nil })
        pm.drag(to: CGPoint(x: 265, y: 215))
        #expect(pm.lift?.over == nil, "over its own place")
        pm.drag(to: CGPoint(x: 300, y: 215))
        #expect(pm.lift?.over == ProjectsDropAt(q: 0, j: 7))
        pm.drag(to: CGPoint(x: 120, y: 110))
        #expect(pm.lift?.over == ProjectsDropAt(q: 1, j: nil) && pm.lift?.overChip == true && pm.lift?.spring == nil)
        #expect(await waitFor(seconds: 3) { pm.lift?.spring == 1 }, "held a moment: SUNDAY's card")
        pm.drag(to: CGPoint(x: 155, y: 405))
        #expect(pm.lift?.over == ProjectsDropAt(q: 1, j: 15) && pm.lift?.spring == 1, "a dot of the card")
        pm.setDragMove(true)
        await pm.release()
        #expect(pm.lift == nil && pm.ask == nil, "SUNDAY's A16 is free")
        #expect(pm.ops.count == 1 && pm.ops[0].q == 1 && pm.ops[0].at == [15] && pm.ops[0].move)

        // let go over nothing after a long press: the TO… sheet
        pm.lift(p: 0, k: 8, at: CGPoint(x: 10, y: 10), size: CGSize(width: 40, height: 40), grab: .zero, touch: true)
        pm.drag(to: CGPoint(x: 900, y: 900))
        await pm.release()
        #expect(pm.lift == nil && pm.to == ProjectsTo(p: 0, k: 8, q: 0))
        pm.closeTo()
        // (a pointer's drag let go over nothing: nothing)
        pm.lift(p: 0, k: 8, at: CGPoint(x: 10, y: 10), size: CGSize(width: 40, height: 40), grab: .zero, touch: false)
        await pm.release()
        #expect(pm.to == nil && pm.ops.count == 1)
        m.stop()
    }

    @Test func iPadBeside() async throws {
        let (m, pm) = try await opened()
        #expect(pm.sel == 0 && pm.besideSlot == 1)
        await pm.tapPill(5)
        #expect(pm.sel == 5 && pm.besideSlot == 0)
        await pm.tapPill(0)
        #expect(pm.sel == 0 && pm.besideSlot == 5, "the one beside tapped: they swap")
        pm.swapBeside()
        #expect(pm.sel == 5 && pm.besideSlot == 0)
        // a pill held under a drag (landscape): it springs open beside
        pm.zones.spring = .beside
        pm.zones.chips[3] = CGRect(x: 0, y: 0, width: 80, height: 40)
        pm.lift(p: 5, k: 0, at: CGPoint(x: 500, y: 500), size: CGSize(width: 40, height: 40), grab: .zero, touch: false)
        pm.drag(to: CGPoint(x: 20, y: 20))
        #expect(await waitFor(seconds: 3) { pm.besideSlot == 3 })
        pm.endLift()
        m.stop()
    }
}
