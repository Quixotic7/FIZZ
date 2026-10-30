// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// PLAY's panel on its three boards (mock-ups 49-54): the approved numbers (the captions'), every view's parts on the
// board and clear of one another, the legends' fit rule.

import CoreGraphics
import Testing
@testable import FizzUI

@Suite struct PlayLayoutTests {
    /// Every part's frame (the knobs as their label and cap).
    private func parts(_ L: PlayLayout) -> [(String, CGRect)] {
        var r: [(String, CGRect)] = [("screen", L.screen)]
        for k in L.knobs {
            r.append((k.name, CGRect(x: k.cx - k.size / 2, y: k.labelTop, width: k.size, height: k.top + k.size - k.labelTop)))
        }
        if let t = L.tray { r.append(("tray", t.frame)) }
        if let w = L.well { r.append(("well", w.frame)) }
        if let w = L.pads { r.append(("pads", w.frame)) }
        if let k = L.keybed { r.append(("keybed", k.frame)) }
        if let l = L.led { r.append(("led", l)) }
        if let d = L.deck { r.append(("deck", d)) }
        if let c = L.chip { r.append(("chip", CGRect(x: c.x, y: c.y, width: 120, height: c.height))) }
        r.append(("grabber", L.grabber))
        r.append(("badge", CGRect(x: L.size.width - L.badge.right - (L.badge.round ? L.badge.height : 70), y: L.badge.top,
                                  width: L.badge.round ? L.badge.height : 70, height: L.badge.height)))
        return r
    }

    @Test func everyViewFitsItsBoardWithNothingOverlapping() {
        for b in PlayBoard.allCases {
            for v in PlayViewKind.allCases {
                for kn in [false, true] {
                    let L = PlayLayout.of(v, on: b, knobsKN: kn)
                    let bounds = CGRect(origin: .zero, size: b.size)
                    let ps = parts(L)
                    for (n, f) in ps { #expect(bounds.contains(f), "\(b) \(v): \(n) \(f) off the board") }
                    for i in ps.indices {
                        for j in ps.indices where j > i {
                            let x = ps[i].1.intersection(ps[j].1)
                            #expect(x.isNull || x.width * x.height < 1, "\(b) \(v): \(ps[i].0) overlaps \(ps[j].0)")
                        }
                    }
                    if let d = L.dots { #expect(d.maxY == b.size.height - 18) }
                }
            }
        }
    }

    @Test func theIPhoneUprightAsDrawn() {
        // 49: the chip (16, 52) 28 high, the knobs 46 at (16, 96), the tray (16, 246) 148 x 52, the screen 202 at
        // (172, 96), KN1-4 at y 316, the well (21, 400) of 48 x 42 keys (legends 11), the keybed (16, 530) 358 x 272
        let L = PlayLayout.of(.full, on: .phone)
        #expect(L.chip == PlayLayout.Chip(x: 16, y: 52, height: 28, font: 13.5))
        #expect(L.knobs[0].cap == CGRect(x: 30, y: 111, width: 46, height: 46) && L.knobs[0].labelTop == 96)
        #expect(L.tray?.frame == CGRect(x: 16, y: 246, width: 148, height: 52))
        #expect(L.screen == CGRect(x: 172, y: 96, width: 202, height: 202))
        #expect(L.knobs.filter { $0.name.hasPrefix("KN") }.allSatisfy { $0.labelTop == 316 })
        #expect(L.well?.frame == CGRect(x: 21, y: 400, width: 348, height: 112) && L.well?.legend == 11)
        #expect(L.keybed?.frame == CGRect(x: 16, y: 530, width: 358, height: 272) && L.keybed?.keyW == 40)
        // 52: the steps 8 x 2 at (16, 316), 358 wide; the keybed from 484
        let S = PlayLayout.of(.steps, on: .phone)
        #expect(S.pads?.frame == CGRect(x: 16, y: 316, width: 358, height: 156))
        #expect(S.keybed?.frame.minY == 484 && S.knobs.map(\.name) == ["KN1", "KN2", "KN3", "KN4"])
        // 54: the screen 340 at (25, 92), the knobs 50 in a row, the deck (95, 565) 200 x 36, the well (15, 611) of
        // 50 x 48 keys (legends 12.5), the tray (95, 747) 200 x 48
        let C = PlayLayout.of(.panel, on: .phone)
        #expect(C.screen == CGRect(x: 25, y: 92, width: 340, height: 340))
        #expect(C.knobs.map(\.name) == ["MASTER", "SELECT", "PRESETS", "ALGORITHM"] && C.knobs.allSatisfy { $0.size == 50 })
        #expect(C.deck == CGRect(x: 95, y: 565, width: 200, height: 36))
        #expect(C.well?.frame.origin == CGPoint(x: 15, y: 611) && C.well?.legend == 12.5)
        #expect(C.tray?.frame == CGRect(x: 95, y: 747, width: 200, height: 48))
        #expect(PlayLayout.of(.keys, on: .phone, knobsKN: true).knobs.map(\.name) == ["KN1", "KN2", "KN3", "KN4"])
    }

    @Test func theIPhoneOnItsSideAndTheIPadAsDrawn() {
        // 50: no chip, the grabber (402, 20), the tray (54, 144) 132 x 48, the screen 172 at (198, 20), the well
        // (386, 88) of 58 x 38 keys (legends 10), the keybed's row (50, 206) 744 x 170, the round badge at (799, 16)
        let W = PlayLayout.of(.full, on: .phoneWide)
        #expect(W.chip == nil && W.dots == nil && W.badge.round)
        #expect(W.tray?.frame == CGRect(x: 54, y: 144, width: 132, height: 48))
        #expect(W.screen == CGRect(x: 198, y: 20, width: 172, height: 172))
        #expect(W.well?.frame == CGRect(x: 386, y: 88, width: 408, height: 104) && W.well?.legend == 10)
        #expect(W.keybed?.frame == CGRect(x: 50, y: 206, width: 744, height: 170) && W.keybed?.keyW == nil)
        #expect(844 - W.badge.right - W.badge.height == 799)
        // 51: the chip (47.5, 26.5) 30 high, the knobs 63, the screen 295 at (269.5, 86.5), the well (599, 230) of
        // 77 x 59 keys (legends 15.5), the keybed (47.5, 417) 1085 x 356, the dots
        let P = PlayLayout.of(.full, on: .pad)
        #expect(P.chip?.height == 30 && P.knobs.allSatisfy { $0.size == 63 })
        #expect(P.screen == CGRect(x: 269.5, y: 86.5, width: 295, height: 295))
        #expect(P.well?.legend == 15.5 && P.well?.frame.height == 148)
        #expect(P.keybed?.frame == CGRect(x: 47.5, y: 417, width: 1085, height: 356) && P.dots != nil)
    }

    @Test func legendsFitAsTheMockUpsSay() {
        // 49: 48 wide keys at 11: TRACKS 10.75, SOUNDS 10.25, PATTERNS 8.25 (no tracking); the short ones keep it
        #expect(PlayLegend.fit("TRACKS", keyWidth: 48, base: 11) == (10.75, 0))
        #expect(PlayLegend.fit("SOUNDS", keyWidth: 48, base: 11) == (10.25, 0))
        #expect(PlayLegend.fit("PATTERNS", keyWidth: 48, base: 11) == (8.25, 0))
        #expect(PlayLegend.fit("FX", keyWidth: 48, base: 11) == (11, 0.06))
        #expect(PlayLegend.fit("MIDI FX", keyWidth: 48, base: 11) == (11, 0.06))
        // 54: TRACKS 11.25, SOUNDS 10.75, PATTERNS 8.5
        #expect(PlayLegend.fit("TRACKS", keyWidth: 50, base: 12.5).size == 11.25)
        #expect(PlayLegend.fit("SOUNDS", keyWidth: 50, base: 12.5).size == 10.75)
        #expect(PlayLegend.fit("PATTERNS", keyWidth: 50, base: 12.5).size == 8.5)
        // 50: every name fits at 10 sideways, PATTERNS without tracking; 51: PATTERNS 13.75
        #expect(PlayLegend.fit("SOUNDS", keyWidth: 58, base: 10) == (10, 0.06))
        #expect(PlayLegend.fit("PATTERNS", keyWidth: 58, base: 10) == (10, 0))
        #expect(PlayLegend.fit("PATTERNS", keyWidth: 76.9, base: 15.5).size == 13.75)
    }

    @Test func aWellsKeyUnderAFinger() {
        let w = PlayLayout.of(.full, on: .phone).well!
        #expect(PlayInput.cell(at: CGPoint(x: w.key(0).midX, y: w.key(0).midY), in: w) == 0)
        #expect(PlayInput.cell(at: CGPoint(x: w.key(11).midX, y: w.key(11).midY), in: w) == 11)
        // (the gap between two belongs to the nearer; off the edge: the edge's key)
        #expect(PlayInput.cell(at: CGPoint(x: w.key(1).minX - 1, y: w.key(1).midY), in: w) == 1)
        #expect(PlayInput.cell(at: CGPoint(x: -20, y: 500), in: w) == 6)
    }
}
