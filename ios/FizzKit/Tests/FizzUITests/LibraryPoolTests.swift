// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// Phase I3 against the demo unit: LIBRARY's filters, the web tags (kept, moved on RENAME, written into a backup),
// LOAD ON FM-1 on a chosen track, EXPORT's sounds, − POOL's IN USE confirm, the outcome states.

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

/// A fresh folder with Backups in it (the web tags beside, so each test has its own).
private func backupsDir() -> URL {
    let u = FileManager.default.temporaryDirectory.appendingPathComponent("fizz-i3-\(UUID().uuidString)", isDirectory: true)
        .appendingPathComponent("Backups", isDirectory: true)
    try? FileManager.default.createDirectory(at: u, withIntermediateDirectories: true)
    return u
}

@MainActor
private func connected(_ dir: URL = backupsDir()) async -> (UnitModel, DemoUnit) {
    let src = DemoLinkSource(.fizz, bulkMs: 0...0.3)
    let m = UnitModel(source: src, backups: dir)
    m.start()
    _ = await waitFor { m.dumpsLoaded && m.layout != nil && m.actions.trackCapable }
    return (m, src.demo.units[0])
}

private func row(_ src: String, _ slot: Int, id: Int, tags: Int) -> FizzListEntry {
    FizzListEntry(source: src, slot: slot, id: id, name: "\(src) \(slot)", tags: tags, imported: false, inPool: false)
}

@Suite("LIBRARY and POOL (I3)", .serialized) @MainActor
struct LibraryPoolTests {
    @Test func filters() {
        let rows = [row("mine", 1, id: 11, tags: 1), row("mine", 2, id: 12, tags: 4), row("factory", 1, id: 101, tags: 1),
                    row("pool", 5, id: 11, tags: 1)]
        var f = LibraryFilter()
        #expect(f.shown(rows).mine.map(\.slot) == [1, 2] && f.shown(rows).factory.isEmpty)
        f.source = .all
        #expect(f.shown(rows).factory.count == 1)
        f.toggle(.bass)
        #expect(f.shown(rows).mine.map(\.slot) == [1] && f.shown(rows).factory.count == 1)
        f.toggle(.bass)
        f.turnPool()
        #expect(f.pool == .inPool && f.shown(rows).mine.map(\.id) == [11] && f.shown(rows).factory.isEmpty)
        f.turnPool()
        #expect(f.pool == .notInPool && f.shown(rows).mine.map(\.id) == [12] && f.shown(rows).factory.count == 1)
        f.turnPool()
        #expect(f.pool == .any)
        f.source = .factory
        #expect(f.shown(rows).mine.isEmpty)
        #expect(poolPlaces(rows) == [11: [5]])
    }

    @Test func webTagStore() {
        let url = backupsDir().deletingLastPathComponent().appendingPathComponent("webtags.json")
        let s = WebTagStore(url: url)
        #expect(s.add("  warm   pad ", to: "k1"))
        #expect(!s.add("WARM PAD", to: "k1"), "not twice")
        #expect(!s.add("bass", to: "k1"), "not one of FIZZ's")
        #expect(!s.add("   ", to: "k1"))
        #expect(s.add("a very long tag name indeed", to: "k1"))
        #expect(s.of("k1") == ["WARM PAD", "A VERY LONG TAG "])
        s.move(from: "k1", to: "k2")
        #expect(s.of("k1").isEmpty && s.of("k2").count == 2)
        s.remove("WARM PAD", from: "k2")
        #expect(WebTagStore(url: url).all == ["k2": ["A VERY LONG TAG "]], "kept in the file")
        s.remove("A VERY LONG TAG ", from: "k2")
        #expect(WebTagStore(url: url).all.isEmpty)
    }

    @Test func webTagsFollowRenameIntoBackups() async throws {
        let (m, _) = await connected()
        let r = try #require(m.mine.first)
        let k = try #require(await m.actions.soundKey(of: r))
        m.webTags.add("dusty", to: k)
        #expect(await m.actions.rename(r, to: "renamed", webTags: m.webTags) == .ok)
        let n = try #require(m.mine.first)
        let k2 = try #require(await m.actions.soundKey(of: n))
        #expect(k2 != k && m.webTags.of(k2) == ["DUSTY"] && m.webTags.of(k).isEmpty)

        m.backups.start(.presets)
        #expect(await waitFor { !m.backups.busy })
        let e = try #require(m.backups.store.entries.first)
        let s = try await FizzDataShared.engine().readBackup([UInt8](try Data(contentsOf: m.backups.store.url(of: e))), fileName: e.name)
        #expect(s.webTags[k2] == ["DUSTY"], "the backup keeps the web tags")
        m.stop()
    }

    @Test func loadOnATrack() async throws {
        let (m, _) = await connected()
        let a = m.actions
        a.trackSlot = 0
        let r = try #require(m.mine.first { $0.slot == 2 })
        #expect(await a.loadOnUnit(r, track: 3) == .ok)
        #expect(a.browsing?.track == 2 && a.browsing?.id == r.id)
        #expect(a.trackSlot == 0, "the followed track left as it was")
        #expect(await waitFor(seconds: 10) { a.browsing == nil })
        #expect(a.browseLoaded == true)
        m.stop()
    }

    @Test func exportSounds() async throws {
        let (m, _) = await connected()
        let rows = Array(m.mine.prefix(3)) + Array(m.factory.prefix(1))
        let s = await m.actions.exportSounds(rows)
        #expect(s.map(\.name) == rows.map(\.name))
        #expect(s[0].source["from"]?.string == "MY PRESETS \(rows[0].slot)")
        #expect(s[3].source["from"]?.string == "FACTORY \(rows[3].slot)")
        #expect(s.allSatisfy { $0.blob.count == 128 })
        m.stop()
    }

    @Test func outOfThePoolAsksWhenUsed() async throws {
        let (m, demo) = await connected()
        // GLASS BELL (MY PRESETS 2) is at pool place 3, which steps lock (the demo: every third place)
        let r = try #require(m.mine.first { $0.slot == 2 })
        #expect(m.actions.places(of: r.id) == [3])
        let o = await m.actions.removeFromPool(r)
        #expect(o == .needsConfirm(place: 3, projects: 1, steps: DemoState.steps(3)))
        #expect(demo.poolPlaces[3] == r.id, "nothing cleared yet")
        #expect(await m.actions.removeFromPool(r, confirmed: [3]) == .ok)
        #expect(demo.poolPlaces[3] == nil && m.actions.places(of: r.id).isEmpty)
        m.stop()
    }

    @Test func busyIsAState() async throws {
        let (m, demo) = await connected()
        demo.setPlaying(true)
        let o = await m.actions.swapPool(2, 9)
        #expect(o == .refused(.busy))
        #expect(ScreenNote.of(o)?.text == "FM-1 BUSY · STOP IT")
        #expect(ScreenNote.of(.ok) == nil && ScreenNote.of(.needsConfirm(place: 1, projects: 1, steps: 1)) == nil)
        #expect(ScreenNote.of(.refused(.updateFizz))?.color == FZ.violet)
        demo.setPlaying(false)
        m.stop()
    }
}

/// A LIBRARY row let go after a sideways swipe (UX pass): pulled all the way right does + / − POOL at once; a short
/// pull or a flick shows the action; DELETE only for a MY preset; a swipe back shuts it.
@Test func librarySwipeSettles() {
    typealias R = LibraryRow
    #expect(R.settle(dx: 0, moved: 240, flick: 0, mine: true) == .pool)
    #expect(R.settle(dx: 0, moved: 80, flick: 0, mine: false) == .poolShown)
    #expect(R.settle(dx: 0, moved: 30, flick: 60, mine: true) == .poolShown)       // (a flick)
    #expect(R.settle(dx: 0, moved: 30, flick: 0, mine: true) == .shut)
    #expect(R.settle(dx: 0, moved: -60, flick: 0, mine: true) == .deleteShown)
    #expect(R.settle(dx: 0, moved: -60, flick: 0, mine: false) == .shut)           // (the factory's: no DELETE)
    #expect(R.settle(dx: R.reveal, moved: -100, flick: 0, mine: true) == .shut)    // (swiped back)
    #expect(R.settle(dx: -R.deleteWidth, moved: 80, flick: 0, mine: true) == .shut)
}
