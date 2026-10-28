// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// UnitActions against the demo unit: each write, then the page's re-reads showing it in UnitModel; refusals (BUSY
// while playing, IN USE then confirmed); the projects; SAVE, SEND, ADD; the TRACK switch, AUDITION and CANCEL, LOAD ON
// FM-1; RESTORE with its safety backup.

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
    let src = DemoLinkSource(.fizz, bulkMs: 0...0.3)
    let m = UnitModel(source: src, backups: tempDir())
    m.start()
    _ = await waitFor { m.dumpsLoaded && m.layout != nil && m.actions.trackCapable }
    return (m, src.demo.units[0])
}

private func sound(_ name: String, _ seed: Int) -> FizzSound {
    var b = [UInt8](repeating: 0, count: 128)
    b[0] = 0x51; b[1] = 6; b[2] = UInt8(seed % 8)
    for i in 3..<124 { b[i] = UInt8((i * 7 + seed * 13) & 0x7f) }
    b[126] = 20; b[127] = 64
    return FizzSound(name: name, blob: b, drive: 3, pan: -4, sends: FizzSends(chorus: 5, delay: 6, reverb: 7), tags: ["LEAD"])
}

@Suite("UnitActions", .serialized) @MainActor
struct UnitActionsTests {
    @Test func libraryEdits() async throws {
        let (m, demo) = await connected()
        let a = m.actions
        let r = try #require(m.mine.first)
        #expect(a.frame(of: r) != nil, "the DUMPs from connect")

        #expect(await a.rename(r, to: "  new name ") == .ok)
        #expect(m.mine.first?.name == "NEW NAME")
        #expect(a.frame(of: m.mine[0]).flatMap(FizzSysEx.parseDump)?.name == "NEW NAME", "its DUMP read again")

        let r1 = m.mine[0]
        #expect(await a.toggleTag(r1, 3) == .ok)
        #expect(m.mine[0].tags == r1.tags ^ 8)
        let fac = try #require(m.factory.first)
        #expect(await a.setTags(fac, mask: 0x41) == .ok)
        #expect(m.factory.first?.tags == 0x41)

        #expect(await a.moveMine(1, to: 200) == .ok)
        #expect(m.mine.last?.slot == 200 && m.mine.last?.name == "NEW NAME")
        #expect(a.frame(of: m.mine.last!) != nil && a.frame(of: r) == nil)

        let gone = try #require(m.mine.first { $0.slot == 2 })
        #expect(await a.delete(gone) == .ok)
        #expect(m.mine.count == 142 && !m.mine.contains { $0.slot == 2 })
        #expect(demo.minePresets.count == 142)
        #expect(m.status?.mine == 142)
        m.stop()
    }

    @Test func thePool() async throws {
        let (m, demo) = await connected()
        let a = m.actions
        let free = try #require(m.mine.first { !$0.inPool })
        let k = await a.addToPool(free)
        guard case .placed(let at) = k else { Issue.record("\(k)"); return }
        #expect(at == 9, "the first free place")
        #expect(m.pool.contains { $0.slot == at && $0.id == free.id })
        #expect(demo.poolPlaces[at] == free.id)

        // − POOL on a used place: IN USE, then confirmed
        let used = try #require(m.pool.first { DemoState.steps($0.slot) > 0 })
        let row = try #require(m.rows.first { $0.source != "pool" && $0.id == used.id })
        let x = await a.removeFromPool(row)
        #expect(x == .needsConfirm(place: used.slot, projects: 1, steps: DemoState.steps(used.slot)))
        #expect(m.pool.contains { $0.slot == used.slot })
        #expect(await a.removeFromPool(row, confirmed: [used.slot]) == .ok)
        #expect(!m.pool.contains { $0.id == used.id })

        #expect(await a.place(100, id: free.id) == .placed(100))
        #expect(m.pool.contains { $0.slot == 100 && $0.id == free.id })
        let before = m.pool.count
        #expect(await a.swapPool(100, 101) == .ok)
        #expect(m.pool.contains { $0.slot == 101 } && !m.pool.contains { $0.slot == 100 } && m.pool.count == before)
        #expect(await a.clearPlace(101) == .ok)
        #expect(!m.pool.contains { $0.slot == 101 })
        let u = await a.poolUsers(6)
        #expect(u?.projects.first?.steps == DemoState.steps(6))

        // CLEAR UNUSED: the places with steps stay
        let n = m.pool.count, withSteps = m.pool.filter { DemoState.steps($0.slot) > 0 }.count
        guard case .placed(let cleared) = await a.clearUnused() else { Issue.record("clear unused"); return }
        #expect(cleared > 0 && m.pool.count == n - cleared)
        #expect(m.pool.count >= withSteps)

        // SORT BY TAG: the pool in the order it said
        let want = a.sortedPool().compactMap { k in m.pool.first { $0.slot == k }?.id }
        guard case .placed = await a.sortPoolByTag() else { Issue.record("sort"); return }
        #expect(m.pool.sorted { $0.slot < $1.slot }.map(\.id) == want)
        #expect(m.pool.map(\.slot).sorted() == Array(1...want.count))
        m.stop()
    }

    @Test func busyWhilePlaying() async throws {
        let (m, demo) = await connected()
        let a = m.actions
        demo.setPlaying(true)
        #expect(await a.swapPool(1, 2) == .refused(.busy))
        #expect(await a.loadProject(2) == .refused(.busy))
        #expect(await a.save(sound("OVER", 1), slot: 3) == .refused(.busy), "SAVE over a used place: a job")
        #expect(await a.save(sound("NEW", 1)) == .placed(144), "SAVE AS NEW: while playing too")
        #expect(await a.flush() == .refused(.busy))
        #expect(await a.moveMine(1, to: 2) == .ok, "MY PRESETS' move: while playing too")
        #expect(UnitOutcome.refused(.busy).text("SAVE") == "The FM-1 is playing (or still busy): stop it, then SAVE.")
        demo.setPlaying(false)
        #expect(await a.flush() == .ok)
        m.stop()
    }

    @Test func theProjects() async throws {
        let (m, _) = await connected()
        let a = m.actions
        #expect(await a.loadProject(2) == .ok)
        #expect(m.projects?.current == 2 && m.status?.project == 2)
        #expect(await a.renameProject(2, to: "sunday 2!") == .ok)
        #expect(m.projects?.list[1].name == "SUNDAY 2!")
        #expect(await a.copyProject(1, to: 5) == .placed(5))
        #expect(m.projects?.list[4].there == true && m.projects?.list[4].name == "NOIR")
        #expect(await a.moveProject(5, to: 8) == .placed(8))
        #expect(m.projects?.list[4].there == false && m.projects?.list[7].name == "NOIR")
        #expect(await a.deleteProject(8) == .ok)
        #expect(m.projects?.list[7].there == false)
        #expect(await a.loadProject(5) == .refused(.notFound))
        m.stop()
    }

    @Test func saveSendAdd() async throws {
        let (m, demo) = await connected()
        let a = m.actions
        #expect(await a.save(sound("MY SAVE", 2)) == .placed(144))
        #expect(m.mine.count == 144 && m.mine.last?.name == "MY SAVE" && m.mine.last?.imported == false)
        #expect(await a.save(sound("OVER", 3), slot: 144) == .placed(144))
        #expect(m.mine.last?.name == "OVER")

        let out = await a.send([sound("IMP A", 4), sound("IMP B", 5)], to: .pool)
        #expect(out.count == 2 && out.allSatisfy(\.isOK))
        #expect(m.mine.count == 146 && m.mine.last?.imported == true)
        #expect(m.pool.contains { $0.name == "IMP A" } && m.pool.contains { $0.name == "IMP B" })
        let t = await a.send([sound("ON TRACK", 6)], to: .track(4))
        #expect(t == [.placed(147)])
        #expect(demo.trackSounds[4].name == "ON TRACK")

        // ADD: the ones there already skipped
        let r = await a.addPresets([sound("IMP A", 4), sound("ADDED", 7), sound("OVER", 3)])
        #expect(r.added == 1 && r.same == 2 && !r.full && r.outcome == .ok)
        #expect(m.mine.count == 148)
        #expect(!a.busy && a.job == nil)
        m.stop()
    }

    @Test func trackLive() async throws {
        let (m, demo) = await connected()
        let a = m.actions
        #expect(a.trackCapable)
        let r = m.mine[10]
        #expect(await a.pick(r) == .notNow, "TRACK off: the tracks left alone")
        await a.setTrackOn(true)
        #expect(a.trackOn)
        #expect(await a.pick(r) == .ok)
        #expect(a.pending?.track == 2 && a.pending?.name == r.name)
        #expect(demo.trackSounds[2].name == r.name && demo.trackSounds[2].id == r.id)
        #expect(await a.cancelAudition())
        #expect(a.pending == nil && demo.trackSounds[2].name == m.mine[2].name, "its own sound back")

        // the pump: a PARAM onto the track, the read-back sees it
        let pump = try #require(a.pump)
        await pump.setTrack(2)
        await pump.param(id: 6, value: 99)                 // FDBK
        await pump.idle()
        let t = try #require(await a.readTrack())
        #expect(t.dump.blob[DemoBlob.offset(6)] == 99 && t.dump.extras?.edited == true)

        // LOAD ON FM-1: the unit's browser, then the track has it
        let other = m.mine[20]
        #expect(await a.loadOnUnit(other) == .ok)
        #expect(a.browsing?.id == other.id)
        #expect(await waitFor(seconds: 10) { a.browseLoaded != nil })
        #expect(a.browseLoaded == true && a.browsing == nil)
        #expect(demo.trackSounds[2].id == other.id)

        await a.setTrackOn(false)
        let on = await pump.isOn
        #expect(!a.trackOn && !on)
        m.stop()
    }

    @Test func restoreWithItsSafetyBackup() async throws {
        let (m, demo) = await connected()
        let a = m.actions
        let image = await demo.store.image(for: demo)
        #expect(await a.delete(m.mine[0]) == .ok)
        #expect(m.mine.count == 142)

        var sawPhases = Set<String>()
        let t = Task { await a.restore(image: image, name: "the test's") }
        for _ in 0..<12000 {
            if let p = a.restoreRun?.phase { sawPhases.insert(p.rawValue) }
            if sawPhases.contains("writing") && a.restoreRun == nil { break }
            try await Task.sleep(for: .milliseconds(5))
        }
        #expect(await t.value == .ok)
        #expect(sawPhases.isSuperset(of: ["saving", "checking", "writing"]), "\(sawPhases)")
        #expect(a.restoreFailure == nil && !a.busy)
        #expect(m.mine.count == 143 && m.mine.first?.name == "SUB DROP", "the store as the image had it")
        #expect(m.unit?.version == "0.12.0-beta.11")
        let kept = m.backups.store.entries
        #expect(kept.count == 1 && kept[0].name.hasPrefix("FIZZ before restore ") && kept[0].presets == 142)

        // playing: the safety backup still made, the unit says FZER 10
        demo.setPlaying(true)
        #expect(await a.restore(image: image, name: "again") == .refused(.busy))
        #expect(a.restoreFailure == .playing && a.restoreFailure?.nothingChanged == true)
        #expect(m.backups.store.entries.count == 2)
        demo.setPlaying(false)
        m.stop()
    }
}
