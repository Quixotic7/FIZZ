// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// The iPhone on its side (LandscapeRoot.swift, LandscapeTurning.swift): which layout a size gets (decided on the
// height: a Pro Max on its side is wider than the iPad's 720 pt), and the ways the screen turns, the same on PLAY and
// on the manager so going between them keeps the iPhone sideways.

import CoreGraphics
import Testing
@testable import FizzUI

@Suite("Landscape")
struct LandscapeTests {
    @Test("the iPhone upright: the phone's layout")
    func upright() {
        for s in [CGSize(width: 428, height: 926), CGSize(width: 390, height: 844), CGSize(width: 375, height: 667),
                  CGSize(width: 440, height: 956)] {
            #expect(RootLayout.pick(s) == .phone, "\(s)")
        }
    }

    @Test("the iPhone on its side, a Pro Max's 926 pt included: the slim rail's layout")
    func sideways() {
        for s in [CGSize(width: 926, height: 428), CGSize(width: 844, height: 390), CGSize(width: 667, height: 375),
                  CGSize(width: 956, height: 440), CGSize(width: 568, height: 320)] {
            #expect(RootLayout.pick(s) == .phoneWide, "\(s)")
        }
    }

    @Test("the iPad either way: the big rail; a narrow iPad window: the phone's")
    func pad() {
        for s in [CGSize(width: 1180, height: 820), CGSize(width: 820, height: 1180), CGSize(width: 1366, height: 1024),
                  CGSize(width: 1024, height: 768), CGSize(width: 768, height: 1024)] {
            #expect(RootLayout.pick(s) == .pad, "\(s)")
        }
        #expect(RootLayout.pick(CGSize(width: 375, height: 1024)) == .phone)
        #expect(RootLayout.pick(CGSize(width: 507, height: 768)) == .phone)
    }

    @Test("PLAY and the manager allow the same ways: leaving PLAY keeps the iPhone sideways, entering it too")
    func turning() {
        let play = Turning.allowed(pad: false, playShown: true)
        let manager = Turning.allowed(pad: false, playShown: false)
        #expect(play == manager)
        #expect(manager.contains(.landscapeLeft) && manager.contains(.landscapeRight) && manager.contains(.portrait))
        #expect(!manager.contains(.upsideDown))
        #expect(Turning.allowed(pad: true, playShown: false) == Set(Turn.allCases))
        #expect(Turning.allowed(pad: true, playShown: true) == Set(Turn.allCases))
    }

    @Test("PROJECTS on its side: the grid (a bank a row) ends above the one-row bars, changes waiting or not")
    func patternsFit() {
        // (the content under the 42-pt top bar: an iPhone 13 Pro Max's 428 and an iPhone 14's 390 pt high)
        for screenH in [428.0 - LandscapeTopBar.height, 390 - LandscapeTopBar.height] as [CGFloat] {
            for pending in [false, true] {
                let below = ProjectsPatternsView.below(pending: pending, clearance: LandscapeRail.bottom, wide: true)
                let h = ProjectsPatternsView.tileH(screenH, pending: pending, clearance: LandscapeRail.bottom, wide: true)
                #expect(4 + 61 + 53 + 4 * h + 3 * 12 <= screenH - below, "\(screenH) pending \(pending): \(h)")
                // (the changes waiting cost the grid nothing sideways: they sit beside the selection's bar)
                #expect(below == ProjectsPatternsView.below(pending: false, clearance: LandscapeRail.bottom, wide: true))
            }
        }
    }
}
