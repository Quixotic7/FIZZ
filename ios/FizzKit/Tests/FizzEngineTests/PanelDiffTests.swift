// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// What the panel sends while FIZZ is idle, on the clock by hand against the fake unit (the first real-unit run sent
// ~100 frames and ~1.2 MB each 5 s with FIZZ "idle"). FIZZ's power-on screen (QD_MODE_IDLE, core/app/ctl.c: "any key or
// button: the step view") is an animation: ~165 of the 225 tiles, ~8,500 pixels, change every 50 ms. The step view
// after a first key or button is still, and then the panel sends nothing: no frame, no tile, through several PANEL_Rs
// and LEDS keepalives. So the diff is right; the traffic was the idle animation.

import CRemoteFM1
import FizzKit
import Foundation
import Testing
@testable import FizzEngine

extension EmulatorTests {
    /// The 16x16 tiles that differ between two LCDs.
    static func tilesChanged(_ a: [UInt16], _ b: [UInt16]) -> Set<Int> {
        var out = Set<Int>()
        for y in 0..<240 {
            for x in 0..<240 where a[y * 240 + x] != b[y * 240 + x] { out.insert((y / 16) * 15 + x / 16) }
        }
        return out
    }

    @Test func aStillScreenSendsNothing() async throws {
        let e = FizzEmulator(storage: nil)
        e.boot(flash: nil)
        e.run(ms: 1500)
        // the power-on screen animates (idle mode)
        #expect(e.uiInfo().hasPrefix("idle:"))
        var prev = try #require(Self.lcd(e))
        var most = 0
        for _ in 0..<10 {
            e.run(ms: 50)
            let now = try #require(Self.lcd(e))
            most = max(most, Self.tilesChanged(prev, now).count)
            prev = now
        }
        #expect(most > 100, "the idle animation: \(most) tiles in 50 ms")

        // a button: the step view, still
        e.tap(.play); e.run(ms: 100); e.tap(.play); e.run(ms: 1000)
        #expect(e.uiInfo().hasPrefix("steps:") && !e.isPlaying)
        let still = try #require(Self.lcd(e))
        e.run(ms: 500)
        #expect(Self.lcd(e) == still)

        let unit = FakeRemoteFM1()
        let link = await Self.claimed(e, unit)
        #expect(link.state == .active)
        #expect(await Self.until { Self.shown(unit) == still && link.stats.frames > 0 })
        try await Task.sleep(for: .milliseconds(200))           // (the last FRAME_ACKs in)
        let before = link.stats, acks = Self.fakeStats(unit).acks
        // 2.5 s with the clock running by hand: PANEL_Rs and keepalives come and go, the screen stays
        for _ in 0..<50 {
            e.run(ms: 50)
            try await Task.sleep(for: .milliseconds(50))
        }
        let after = link.stats
        #expect(after.panelR >= before.panelR + 4, "PANEL_Rs heard: \(before.panelR) -> \(after.panelR)")
        #expect(after.frames == before.frames && after.tiles == before.tiles && after.bodyBytes == before.bodyBytes,
                "nothing sent for a still screen: \(before) -> \(after)")
        #expect(after.sessions == 1 && Self.fakeStats(unit).acks == acks && link.state == .active)
        #expect(Self.shown(unit) == still)

        // one change: only its tiles go
        let n0 = link.stats.tiles
        unit.with { rfm_fake_enc($0, UInt32(e.panelTable!.encID[FizzKnob.k1.rawValue]), 1) }
        #expect(await Self.until { link.stats.events > after.events })
        // (every screen on the way counts, a tile changed twice sent twice: the turn's screens in 20 ms steps, as
        // run(ms:) publishes them)
        var seen = still, changed = 0
        for _ in 0..<5 {
            e.run(ms: 20)
            let now = try #require(Self.lcd(e))
            changed += Self.tilesChanged(seen, now).count
            seen = now
        }
        #expect(await Self.until { Self.shown(unit) == Self.lcd(e) })
        #expect(changed > 0 && link.stats.tiles - n0 <= changed, "\(link.stats.tiles - n0) tiles sent, \(changed) changed")
        link.release()
        _ = await Self.until { link.state == .released(.byUser) }
    }
}
