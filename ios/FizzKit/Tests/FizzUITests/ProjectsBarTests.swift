// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// PROJECTS' fixed selection bar (ProjectsBar.swift) against the demo unit: everything off with nothing selected, PASTE
// only with something copied, TO… only with one pattern there, nothing in a slot with no project; the phone shows it
// on a read project's patterns only, and the grid leaves room for it.

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

@Suite("Projects bar", .serialized) @MainActor
struct ProjectsBarTests {
    @Test func enabledStates() async throws {
        let src = DemoLinkSource(.fizz, bulkMs: 0...0.2)
        let m = UnitModel(source: src, backups: tempDir(), projects: tempDir())
        m.start()
        _ = await waitFor { m.dumpsLoaded && m.layout != nil }
        let pm = m.projectsTab
        await pm.show()
        let noir = try #require(pm.view?.project(0))
        let free = try #require(pm.firstFree(0))
        #expect(noir.there && noir.pats[6] != nil && noir.pats[free] == nil)

        // nothing selected: all off
        #expect(pm.pats.isEmpty && ProjectsBarState(pm) == ProjectsBarState())
        #expect(!ProjectsBarState(pm).any)

        // one pattern there: all but PASTE (nothing copied yet)
        pm.tap(6)
        #expect(ProjectsBarState(pm) == ProjectsBarState(copy: true, paste: false, clear: true, share: true, to: true))
        // copied: PASTE on
        await pm.copy()
        #expect(pm.clip != nil)
        #expect(ProjectsBarState(pm) == ProjectsBarState(copy: true, paste: true, clear: true, share: true, to: true))
        // a range: TO… off (one pattern only)
        pm.selectRange(from: 2, to: 4)
        #expect(ProjectsBarState(pm) == ProjectsBarState(copy: true, paste: true, clear: true, share: true, to: false))
        // one empty pattern: nothing to send TO…
        pm.tap(free)
        #expect(ProjectsBarState(pm) == ProjectsBarState(copy: true, paste: true, clear: true, share: true, to: false))
        // the tile tapped again: deselected, all off even with something copied
        pm.tap(free)
        #expect(pm.pats.isEmpty && ProjectsBarState(pm) == ProjectsBarState())

        // the phone shows the bar on a read project's patterns only
        #expect(!ProjectsPhoneBar.showsSelection(pm), "the slots")
        await pm.enter(.patterns)
        #expect(ProjectsPhoneBar.showsSelection(pm))
        // a slot with no project: no bar, and nothing on even with a tile tapped
        let empty = try #require((0..<8).first { pm.view?.project($0)?.there != true })
        pm.select(empty)
        pm.tap(0)
        #expect(!ProjectsPhoneBar.showsSelection(pm) && ProjectsBarState(pm) == ProjectsBarState())
        m.stop()
    }

    /// The tiles leave room for the bar: an iPhone 13 Pro Max's screen under the top bar (~820) keeps full tiles with
    /// both bars; a smaller one shrinks them so the grid still ends above the bars.
    @Test func gridLeavesRoom() {
        #expect(ProjectsPatternsView.tileH(820, pending: true) == 40)
        #expect(ProjectsPatternsView.below(pending: false) == 100 + ProjectsSelectionBar.height + 10)
        for screenH in [740.0, 780, 820] as [CGFloat] {
            for pending in [false, true] {
                let h = ProjectsPatternsView.tileH(screenH, pending: pending)
                #expect(4 + 61 + 53 + 8 * h + 4 * 5 + 3 * 12 <= screenH - ProjectsPatternsView.below(pending: pending),
                        "\(screenH) pending \(pending): \(h)")
            }
        }
        // the phone on its side (a content column ~714 x 360 beside the rail, clearance 8): a bank in one row, the grid
        // above the selection's bar; upright: two rows a bank
        #expect(ProjectsPatternsView.wide(714) && ProjectsPatternsView.wide(632) && !ProjectsPatternsView.wide(358))
        let w = ProjectsPatternsView.tileH(360, pending: false, clearance: 8, wide: true)
        #expect(w >= 28 && w <= 48)
        #expect(4 + 61 + 53 + 4 * w + 3 * 12 <= 360 - ProjectsPatternsView.below(pending: false, clearance: 8))
    }
}
