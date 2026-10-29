// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// Getting around PLAY: the FIZZ badge's way back to the manager (the tab last shown), the full-screen LCD put back
// when PLAY is left.

import Foundation
import Testing
@testable import FizzUI

@MainActor
private func freshPlay() -> PlayModel {
    PlayModel(host: nil, defaults: UserDefaults(suiteName: "play.nav.\(UUID().uuidString)")!)
}

@MainActor
@Suite struct PlayNavTests {
    @Test func badgeGoesToLibraryWithNoTabShownBefore() {
        let p = freshPlay()
        #expect(p.lastTab == nil)
        #expect(p.managerTab == .library)
        // (launched on PLAY, then IMPORT's sheet over it: still none)
        p.remember(.play)
        p.remember(.importer)
        #expect(p.managerTab == .library)
    }

    @Test func badgeGoesToTheManagerTabLastShown() {
        let p = freshPlay()
        // BACKUP -> POOL -> PLAY: back to POOL
        for t in [FizzTab.backup, .pool, .play] { p.remember(t) }
        #expect(p.managerTab == .pool)
        // PLAY's sheet -> IMPORT, then PLAY again: still POOL
        p.remember(.importer)
        p.remember(.play)
        #expect(p.managerTab == .pool)
        // EDIT, then PLAY
        p.remember(.edit)
        p.remember(.play)
        #expect(p.managerTab == .edit)
    }

    @Test func leavingPlayPutsTheLCDBackAndTheSheetUp() {
        let p = freshPlay()
        p.lcdFull = true
        p.sheet = true
        p.left()
        #expect(!p.lcdFull)
        #expect(!p.sheet)
        // (the view and the theme stay as they were)
        p.view = .screen
        p.lcdFull = true
        p.left()
        #expect(p.view == .screen)
        #expect(!p.lcdFull)
    }
}
