// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// The phone's EDIT pages fit their height (FitColumn): the drawings give way first, then the gaps; with room, the
// drawings grow to their most and the rest stays below.

import CoreGraphics
import Testing
@testable import FizzUI

@MainActor
struct EditFitTests {
    /// three parts: 40 tall, a card (60 besides its drawing; the drawing 50..200), 30 tall
    let col = FitColumn(spacing: 10, minSpacing: 4)
    let flex: [(i: Int, f: FitFlex, base: CGFloat)] = [(1, FitFlex(min: 50, max: 200, weight: 1), 60)]
    let parts: [CGFloat] = [40, 0, 30]

    @Test func roomGoesToTheDrawing() {
        let p = col.share(parts, flex, 300)
        #expect(p.gap == 10)
        // (the drawing: its least 50 and the 100 left)
        #expect(p.heights == [40, 60 + 50 + 100, 30])
        #expect(p.total == 300)
    }

    @Test func theDrawingStopsAtItsMost() {
        let p = col.share(parts, flex, 500)
        #expect(p.heights[1] == 260)
        // (the gaps open a little: at most 60 % more)
        #expect(p.gap == 16)
        #expect(p.total <= 500)
    }

    @Test func shortTheGapsCloseUp() {
        // (the least: 40 + 110 + 30 = 180, with full gaps 200)
        let p = col.share(parts, flex, 188)
        #expect(p.heights == [40, 110, 30])
        #expect(p.gap == 4)
        #expect(p.total == 188)
        // (shorter still: the gaps no smaller than their least, the page runs over)
        #expect(col.share(parts, flex, 150).gap == 4)
    }

    @Test func twoDrawingsShareByWeight() {
        let two: [(i: Int, f: FitFlex, base: CGFloat)] = [(0, FitFlex(min: 0, max: 400, weight: 1), 0),
                                                          (1, FitFlex(min: 0, max: 400, weight: 3), 0)]
        let p = col.share([0, 0], two, 210)
        #expect(p.heights == [50, 150])
    }

    @Test func noHeightAPlainColumn() {
        let p = col.share([40, 80, 30], flex, nil)
        #expect(p.heights == [40, 80, 30])
        #expect(p.gap == 10)
        #expect(p.total == 170)
    }
}
