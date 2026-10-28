// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// ParamPump (one PARAM in flight, the latest value of each id winning, the read-back's quiet time, an older FIZZ) and
// TrackAudition (a preset heard on the track, KEEP, CANCEL putting the track's own sound back, a lost reply checked).

import Foundation
import Testing
@testable import FizzKit

private final class Flag: @unchecked Sendable {
    private let lock = NSLock()
    private var n = 0
    func hit() { lock.withLock { n += 1 } }
    var count: Int { lock.withLock { n } }
}

@Suite("Live: PARAM pump and AUDITION", .serialized)
struct LiveTests {
    let clock = ScaledClock(speed: 20)

    private func link(_ configure: (MockFizz) -> Void = { _ in }) -> (FizzLink, MockFizz) {
        let m = MockFizz(clock: clock)
        m.writes = true
        configure(m)
        return (FizzLink(transport: m, clock: clock), m)
    }

    private func params(_ m: MockFizz) -> [(id: Int, value: Int, track: Int)] {
        m.sent.map(\.bytes).filter { $0.count == 14 && $0[5] == 0x30 }.map { (b: [UInt8]) -> (id: Int, value: Int, track: Int) in
            let v: Int = (Int(b[8]) << 14) | (Int(b[9]) << 7) | Int(b[10])
            return (Int(b[7]), v - (1 << 20), Int(b[6]))
        }
    }

    @Test func oneInFlightLatestWins() async throws {
        let (l, m) = link { $0.replyMs = 400 }        // (20 ms here: the values below all come while one waits)
        _ = await l.status()
        let pump = ParamPump(link: l, clock: clock)
        await pump.setTrack(3)
        await pump.param(id: 5, value: 1)              // (not on yet: dropped)
        #expect(await pump.pending == 0)
        await pump.setOn(true)
        await pump.param(id: 5, value: 10)
        while params(m).isEmpty { try await Task.sleep(for: .milliseconds(1)) }     // (in flight)
        for v in 11...20 { await pump.param(id: 5, value: v) }
        await pump.param(id: 7, value: -64)
        #expect(await pump.followBlocked())
        await pump.idle()
        let p = params(m)
        #expect(p.map(\.id) == [5, 5, 7] && p.map(\.value) == [10, 20, -64], "\(p)")
        #expect(p.allSatisfy { $0.track == 3 })
        #expect(!m.overlapped, "a PARAM sent while another waited for its ACK")
        #expect(await pump.followBlocked(), "within 0.7 s of the last PARAM")
        try await clock.sleep(ms: ParamPump.followQuietMs + 50)
        #expect(await !pump.followBlocked())
    }

    @Test func soundAndClear() async throws {
        let (l, m) = link { $0.replyMs = 20 }
        _ = await l.status()
        let pump = ParamPump(link: l, clock: clock)
        await pump.setTrack(0)
        await pump.setOn(true)
        let f = try FizzSysEx.storeSound(name: "MIDI", blob: [UInt8](repeating: 2, count: 128), track: 0, audition: true)
        await pump.param(id: 1, value: 1)
        while params(m).isEmpty { try await Task.sleep(for: .milliseconds(1)) }     // (in flight)
        await pump.sound(f)
        await pump.sound(f)
        await pump.param(id: 2, value: 2)
        await pump.setOn(false)                        // (TRACK off: the queue dropped)
        await pump.idle()
        #expect(params(m).map(\.id) == [1])
        #expect(m.sentStores.isEmpty)
        await pump.setOn(true)
        await pump.sound(f)
        await pump.idle()
        #expect(m.sentStores == [f])
    }

    @Test func anOlderFizzTurnsItOff() async throws {
        let (l, m) = link { $0.errorFor[0x30] = 7 }
        let old = Flag(), refused = Flag()
        let pump = ParamPump(link: l, clock: clock)
        await pump.setOnOld { old.hit() }
        await pump.setOnRefused { _, _ in refused.hit() }
        await pump.setTrack(1)
        await pump.setOn(true)
        await pump.param(id: 3, value: 3)
        await pump.param(id: 4, value: 4)
        await pump.idle()
        #expect(old.count == 1 && refused.count == 0)
        #expect(await !pump.isOn)
        #expect(await pump.pending == 0)
        #expect(params(m).count == 1)
        m.set { $0.errorFor[0x30] = nil; $0.status[0x30] = 11 }
        await pump.setOn(true)
        await pump.param(id: 3, value: 3)
        await pump.idle()
        #expect(refused.count == 1)
        #expect(await pump.isOn, "BAD TRACK: logged, the pump stays on")
    }

    @Test func pickKeepCancel() async throws {
        let (l, m) = link()
        let a = TrackAudition(link: l)
        let preset = MockFizz.dumpFrame(slot: 4, name: "GLASS BELL")
        let x = await a.pick(preset: preset, id: 0x1004, name: "GLASS BELL")
        #expect(x?.isOK == true)
        let p = try #require(await a.pending)
        #expect(p.track == 2 && p.name == "GLASS BELL" && FizzSysEx.parseDump(p.own)?.name == "OWN SOUND")
        let s = try #require(m.sentStores.last.flatMap { FizzSysEx.parseDump(TrackAudition.dumped($0)) })
        #expect(s.name == "GLASS BELL" && s.track == 2 && s.extras?.id == 0x1004 && s.extras?.edited == false)
        #expect(Int(m.sentStores.last![6]) & (FizzSysEx.FLAGS.audition | FizzSysEx.FLAGS.load) == 0x14)
        // a second pick keeps the first own sound
        _ = await a.pick(preset: MockFizz.dumpFrame(slot: 5, name: "D5"), id: 0x1005, name: "D5")
        let p2 = await a.pending
        #expect(p2?.own == p.own && p2?.name == "D5")
        #expect(await a.cancel())
        #expect(await a.pending == nil)
        #expect(m.sentStores.last == FizzSysEx.trackFrame(p.own), "CANCEL: the track's DUMP sent back")
        // KEEP
        _ = await a.pick(preset: preset, id: 0x1004, name: "GLASS BELL")
        await a.keep()
        #expect(await a.pending == nil)
        #expect(await a.revert(to: p.own)?.isOK == true)
    }

    @Test func aLostReplyCheckedOnTheTrack() async throws {
        let (l, m) = link { $0.silentStores = 1 }
        let a = TrackAudition(link: l)
        let x = await a.pick(preset: MockFizz.dumpFrame(slot: 4, name: "BELL"), id: 9, name: "BELL")
        #expect(x?.isOK == true)
        #expect(m.sentStores.count == 1, "the track had it: not sent again")
        // lost on the way: the track does not have it, sent again
        let (l2, m2) = link { $0.dropStores = 1 }
        let b = TrackAudition(link: l2)
        #expect(await b.pick(preset: MockFizz.dumpFrame(slot: 4, name: "BELL"), id: 9, name: "BELL")?.isOK == true)
        #expect(m2.sentStores.count == 2)
    }

    @Test func noTrackNoPick() async {
        let (l, _) = link { $0.writes = false }
        let a = TrackAudition(link: l)
        #expect(await a.pick(preset: MockFizz.dumpFrame(slot: 1, name: "X"), id: 1, name: "X") == nil)
        #expect(await a.pending == nil)
    }
}
