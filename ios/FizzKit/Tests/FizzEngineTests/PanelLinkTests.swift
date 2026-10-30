// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// A RemoteFM1 unit as the emulated FIZZ's panel (PanelLink), against libremotefm1's fake unit (FakeRemoteFM1): nothing
// sent before claim(); claimed, the session comes up; the unit's buttons, keys, knobs and MASTER play FIZZ; FIZZ's LCD
// and LEDs reach the unit; a PANEL_R without the session flag, another host's session, an unplug let the unit go with
// nothing more sent; release() ends the session. HELLO tells a RemoteFM1 unit. (In the Emulator suite: the core is the
// process's one.)

import CRemoteFM1
import CoreGraphics
import FizzKit
import Foundation
import Testing
@testable import FizzEngine

extension EmulatorTests {
    static func until(_ s: Double = 5, _ f: () -> Bool) async -> Bool {
        let end = Date().addingTimeInterval(s)
        while !f() && Date() < end { try? await Task.sleep(for: .milliseconds(10)) }
        return f()
    }

    static func fakeStats(_ u: FakeRemoteFM1) -> rfm_fake_stats_t {
        u.with { f in var s = rfm_fake_stats_t(); rfm_fake_stats(f, &s); return s }
    }

    /// What the fake unit shows, as FIZZ's LCD slots (the same bytes).
    static func shown(_ u: FakeRemoteFM1) -> [UInt16] {
        u.with { f in
            let p = rfm_fake_panel(f)!
            return (0..<(240 * 240)).map { i in UInt16(p[2 * i]) | UInt16(p[2 * i + 1]) << 8 }
        }
    }

    static func ledBytes(_ l: rfm_leds_t) -> [UInt8] {
        var l = l
        return withUnsafeBytes(of: &l) { Array($0) }
    }

    static func lcd(_ e: FizzEmulator) -> [UInt16]? { e.lcdImage.flatMap(PanelLink.lcdSlots) }

    /// A link claimed on a fresh fake unit, up.
    static func claimed(_ e: FizzEmulator, _ unit: FakeRemoteFM1 = FakeRemoteFM1()) async -> PanelLink {
        let link = PanelLink(transport: unit, emulator: e)
        link.claim()
        _ = await until { link.state == .active }
        return link
    }

    @Test func thePanelDrivesTheEmulatedFizz() async throws {
        // the clock by hand (a still screen between steps; the same power-on gives the same screens)
        let e = FizzEmulator(storage: nil)
        e.boot(flash: nil)
        e.run(ms: 600)
        let table = try #require(e.panelTable)
        let k1 = table.encID[FizzKnob.k1.rawValue], dir = Int(table.encDir[FizzKnob.k1.rawValue])
        // K1 turned 3 clockwise on FIZZ itself, and not at all: the screens to expect
        e.turn(.k1, by: 3)
        e.run(ms: 300)
        let turned = Self.lcd(e)
        e.boot(flash: nil)
        e.run(ms: 900)
        let plain = Self.lcd(e)
        #expect(turned != nil && turned != plain)

        e.boot(flash: nil)
        e.run(ms: 600)
        let unit = FakeRemoteFM1()
        let link = PanelLink(transport: unit, emulator: e)
        // no claim: nothing sent to the unit
        try await Task.sleep(for: .milliseconds(300))
        #expect(unit.hostMessages.isEmpty && link.state == .idle)
        link.claim()
        #expect(await Self.until { link.state == .active })
        #expect(unit.with { rfm_fake_session($0) } == 1)
        #expect(link.hello?.isPanel == true && link.hello?.name == "RemoteFM1")
        // FIZZ's LCD and LEDs on the unit
        #expect(await Self.until { Self.lcd(e) == Self.shown(unit) })
        #expect(await Self.until {
            Self.ledBytes(PanelLink.leds(e.leds)) == Self.ledBytes(unit.with { rfm_fake_leds($0).pointee })
        })

        // the unit's matrix encoder for K1, 3 detents its way: FIZZ's K1 turned 3 clockwise, the screen shown
        let ev = link.stats.events
        unit.with { rfm_fake_enc($0, UInt32(k1), Int32(3 * dir)) }
        #expect(await Self.until { link.stats.events > ev })
        e.run(ms: 300)
        #expect(Self.lcd(e) == turned)
        #expect(await Self.until { Self.shown(unit) == turned })

        // PLAY on the unit: FIZZ plays, and stops; its LEDs follow on the unit
        let play = UInt32(table.btnID[FizzButton.play.rawValue])
        func button(_ id: UInt32, _ down: Int32) async {
            let n = link.stats.events
            unit.with { rfm_fake_button($0, id, down) }
            _ = await Self.until { link.stats.events > n }
            e.run(ms: 40)
        }
        await button(play, 1); await button(play, 0)
        e.run(ms: 200)
        #expect(e.isPlaying)
        #expect(await Self.until {
            Self.ledBytes(PanelLink.leds(e.leds)) == Self.ledBytes(unit.with { rfm_fake_leds($0).pointee })
        })
        await button(play, 1); await button(play, 0)
        e.run(ms: 100)
        #expect(!e.isPlaying)

        // a held key and a held button, as FIZZ sees them ("keys 0001000 buttons 0000": hex bit masks)
        var n = link.stats.events
        unit.with { rfm_fake_key($0, 12, 1) }
        #expect(await Self.until { link.stats.events > n })
        e.run(ms: 20)
        #expect(e.uiInfo().contains("keys 0001000"))
        n = link.stats.events
        unit.with { rfm_fake_key($0, 12, 0) }
        #expect(await Self.until { link.stats.events > n })
        e.run(ms: 20)
        #expect(e.uiInfo().contains("keys 0000000"))
        let rec = UInt32(table.btnID[FizzButton.rec.rawValue])
        await button(rec, 1)
        #expect(!e.uiInfo().contains("buttons 0000,"))
        await button(rec, 0)
        #expect(e.uiInfo().contains("buttons 0000,"))

        // MASTER
        unit.with { rfm_fake_master($0, 300) }
        #expect(await Self.until { e.master == 300 })
        #expect(link.stats.frames > 0 && link.stats.ledsSent > 0)

        // let go by hand: SESSION 0, the unit back to its waiting screen
        link.release()
        #expect(await Self.until { link.state == .released(.byUser) })
        #expect(unit.with { rfm_fake_session($0) } == 0)
        #expect(Self.fakeStats(unit).sessions_ended == 1)
    }

    @Test func aPanelRWithoutTheSessionLetsTheUnitGo() async throws {
        let e = FizzEmulator(storage: nil)
        let unit = FakeRemoteFM1()
        let link = await Self.claimed(e, unit)
        #expect(link.state == .active)
        // another host on the port ends the unit's session and asks for its panel: PANEL_R, no session flag
        unit.inject(RFM_SESSION, [0, 0, 0])
        unit.inject(RFM_PANEL_REQ)
        #expect(await Self.until(1.0) { link.state == .released(.sessionEnded) }, "before the heartbeat's 1.6 s")
        let sent = unit.hostMessages.count
        try await Task.sleep(for: .milliseconds(2500))
        #expect(unit.hostMessages.count == sent, "nothing more to the unit: no SESSION start, no HELLO")
        #expect(Self.fakeStats(unit).sessions_started == 1)
        #expect(unit.with { rfm_fake_session($0) } == 0)
    }

    @Test func anotherHostsSessionIsLeftAlone() async throws {
        let e = FizzEmulator(storage: nil)
        e.boot(flash: nil)
        #expect(e.start(sound: false) == .timer)
        defer { e.stop() }
        let unit = FakeRemoteFM1()
        let link = await Self.claimed(e, unit)
        // (FIZZ's own first frame acknowledged: a start before it would look like the session's own shadow reset)
        #expect(await Self.until { link.stats.frames > 0 })
        try await Task.sleep(for: .milliseconds(300))
        // another host starts its own session on the unit: FIZZ sees its shadow reset (or the heartbeat go) and lets go
        unit.inject(RFM_SESSION, [1, 0xB8, 0x0B])
        e.tap(.edit)                                    // (a new frame for FIZZ to send)
        #expect(await Self.until(3) { if case .released = link.state { true } else { false } })
        #expect([.released(.unitReset), .released(.heartbeat), .released(.sessionEnded)].contains(link.state))
        try await Task.sleep(for: .milliseconds(1500))
        #expect(Self.fakeStats(unit).sessions_started == 2, "the other host's start, none of FIZZ's after it")
        #expect(unit.with { rfm_fake_session($0) } == 1)
    }

    @Test func anUnpluggedUnitIsLetGo() async throws {
        let e = FizzEmulator(storage: nil)
        let unit = FakeRemoteFM1()
        let link = await Self.claimed(e, unit)
        #expect(link.state == .active)
        unit.unplug()
        #expect(await Self.until(3) { link.state == .released(.lost) })
    }

    @Test func aClaimFindsNoUnit() async throws {
        let e = FizzEmulator(storage: nil)
        let silent = FakeRemoteFM1 { $0.silent = 1 }
        let a = PanelLink(transport: silent, emulator: e)
        a.claim()
        #expect(await Self.until(3) { a.state == .released(.notFound) })
        #expect(silent.hostMessages.count == 1, "one HELLO, nothing more")
        let other = FakeRemoteFM1 { $0.name = UnsafePointer(strdup("RemoteFM1-M0")); $0.proto = 1 }
        let b = PanelLink(transport: other, emulator: e)
        b.claim()
        #expect(await Self.until(3) { b.state == .released(.notFound) })
        #expect(Self.fakeStats(other).sessions_started == 0)
    }

    @Test func helloTellsARemoteFM1Unit() async throws {
        let h = await RemoteFM1Unit.hello(on: FakeRemoteFM1())
        #expect(h?.isPanel == true && h?.name == "RemoteFM1" && h?.proto == 2 && h?.inSession == false)
        #expect(h?.bluetooth == false && h?.screenWidth == 240)
        let ble = await RemoteFM1Unit.hello(on: FakeRemoteFM1 { $0.ble = 1 })
        #expect(ble?.isPanel == true && ble?.bluetooth == true)
        #expect(await RemoteFM1Unit.hello(on: FakeRemoteFM1 { $0.silent = 1 }, timeoutMs: 200) == nil)
        // a unit in another host's session says so (and HELLO changes nothing)
        let busy = FakeRemoteFM1()
        busy.inject(RFM_SESSION, [1, 0xB8, 0x0B])
        #expect(await RemoteFM1Unit.hello(on: busy)?.inSession == true)
        #expect(Self.fakeStats(busy).sessions_started == 1 && busy.with { rfm_fake_session($0) } == 1)
        // the update identity: 0.2 and later
        #expect(RemoteFM1Unit.isRemoteFM1(identity: "FM-1_52000203"))
        #expect(!RemoteFM1Unit.isRemoteFM1(identity: "FM-1_46001200") && !RemoteFM1Unit.isRemoteFM1(identity: "FM-1_015"))
    }

    @Test func fizzsLEDsAsTheUnitsPlanes() {
        var raw = [UInt8](repeating: 0, count: 44)
        raw[0 * 4 + 0] = 255        // column 0, line 0: lit, bit 1
        raw[3 * 4 + 2] = 96         // column 3, line 2: mid, bit 3
        raw[10 * 4 + 3] = 40        // column 10, line 3: dim, bit 4
        raw[5 * 4 + 1] = 12         // column 5, line 1: faint, bit 2
        let l = PanelLink.leds(FizzLEDs(buttons: [], keys: [], playGreen: 255, raw: raw))
        var b = Self.ledBytes(l)
        #expect(b[0] == 0x02 && b[11 + 3] == 0x08 && b[22 + 10] == 0x10 && b[33 + 5] == 0x04)
        #expect(b[44] == UInt8(RFM_LEDS_F_PLAY_GREEN))
        b[0] = 0; b[14] = 0; b[32] = 0; b[38] = 0; b[44] = 0
        #expect(b.allSatisfy { $0 == 0 })
    }
}
