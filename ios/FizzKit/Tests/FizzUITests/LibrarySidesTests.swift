// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// LIBRARY's sides (the owner's asks, 2026-10-29): it opens on ALL, the switch reads ALL, MY PRESETS, FACTORY and the
// phone's KEPT last; SELECT starts on every side with the demo FM-1 (and on KEPT with no unit). THIS iPHONE's run:
// FizzEngineTests/LibrarySelectEmulatorTests.swift.

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

/// SELECT on one side as the phone's list does it: on, the side's first row ticked, what the bar counts.
@MainActor
func selectOne(_ m: UnitModel, on side: LibrarySide) -> Int {
    let p = m.picks
    p.start()
    let shown = LibraryFilter(source: side.source ?? .all).shown(m.rows)
    if let r = (shown.mine + shown.factory).first { p.toggle(r) }
    let n = BulkCounts(p.rows(m.rows), places: poolPlaces(m.rows)).n
    #expect(p.active)
    p.clear()
    return n
}

@Suite("LIBRARY's sides: ALL first, SELECT everywhere", .serialized) @MainActor
struct LibrarySidesTests {
    @Test func opensOnAllWithKeptLast() {
        #expect(LibraryFilter().source == .all)
        #expect(LibraryFilter.Source.allCases == [.all, .mine, .factory], "the iPad's switch")
        #expect(LibrarySide.allCases == [.all, .mine, .factory, .kept], "the phone's switch: the phone last")
        #expect(LibrarySide.opening == .all && LibrarySide.shown == .all, "LIBRARY opens on ALL, not KEPT")
        #expect(LibrarySide.allCases.map(\.title) == ["ALL", "MY PRESETS", "FACTORY", "KEPT"])
        for s in LibrarySide.allCases { #expect(LibrarySide(kept: s == .kept, source: s.source ?? .mine) == s) }
    }

    @Test func selectOnEverySideWithTheFM1() async throws {
        let dir = FileManager.default.temporaryDirectory.appendingPathComponent("fizz-sides-\(UUID().uuidString)", isDirectory: true)
        let m = UnitModel(source: DemoLinkSource(.fizz, bulkMs: 0...0.3), backups: dir.appendingPathComponent("Backups"),
                          projects: dir.appendingPathComponent("Projects"), presets: dir.appendingPathComponent("Presets"))
        // no unit yet: KEPT's SELECT only
        #expect(LibrarySide.allCases.filter { m.canSelect(on: $0) } == [.kept])
        m.start()
        #expect(await waitFor { m.dumpsLoaded })
        #expect(!m.noFM1, "the demo FM-1")
        for s in LibrarySide.allCases { #expect(m.canSelect(on: s), "SELECT on \(s.title)") }
        for s in [LibrarySide.all, .mine, .factory] { #expect(selectOne(m, on: s) == 1, "a row ticked on \(s.title)") }
        m.stop()
    }
}
