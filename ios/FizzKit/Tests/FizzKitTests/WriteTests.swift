// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// FizzLink's writes against MockFizz: the bytes each op sends (the builders' own, checked against the JS in
// VectorTests), the wake before it, FIZZ's refusals as values, no write sent again blindly (PARAM only).

import Foundation
import Testing
@testable import FizzKit

@Suite("FizzLink writes", .serialized)
struct WriteTests {
    let clock = ScaledClock(speed: 20)

    private func link(_ configure: (MockFizz) -> Void = { _ in }) -> (FizzLink, MockFizz) {
        let m = MockFizz(clock: clock)
        m.writes = true
        configure(m)
        return (FizzLink(transport: m, clock: clock), m)
    }

    private func last(_ m: MockFizz) -> [UInt8] { m.sent.last?.bytes ?? [] }

    @Test func eachOpSendsItsFrameAfterAWake() async throws {
        let (l, m) = link()
        #expect(try await l.rename(slot: 12, name: "NOIR").isOK)
        #expect(m.sentCommands == [0x02, 0x13], "a quiet link woken by a STATUS first")
        #expect(last(m) == (try FizzSysEx.renameRequest(source: .mine, slot: 12, name: "NOIR")))
        // heard just now: no STATUS before the next ones
        #expect(try await l.tags(source: .factory, slot: 3, tags: ["PAD", "FX"]).isOK)
        #expect(last(m) == (try FizzSysEx.tagsRequest(source: .factory, slot: 3, tags: ["PAD", "FX"])))
        #expect(try await l.remove(source: .pool, slot: 5, confirm: true).isOK)
        #expect(last(m) == (try FizzSysEx.deleteRequest(source: .pool, slot: 5, confirm: true)))
        #expect(try await l.swap(source: .mine, a: 2, b: 9).isOK)
        #expect(last(m) == (try FizzSysEx.swapRequest(source: .mine, a: 2, b: 9)))
        let ref = try await l.poolRef(id: 0x1007)
        #expect(ref.value?.a == 7 && ref.value?.b == 0x1007, "ACK a = the place taken")
        #expect(last(m) == FizzSysEx.poolRefRequest(slot: 0, id: 0x1007))
        let p = try await l.project(.copy, 2, 5)
        #expect(p.value?.a == 2 && p.value?.b == 5)
        #expect(last(m) == FizzSysEx.projectRequest(op: .copy, p: 2, q: 5))
        #expect(try await l.renameProject(3, name: "SUNDAY").isOK)
        #expect(last(m) == (try FizzSysEx.renameRequest(source: .project, slot: 3, name: "SUNDAY")))
        let b = try await l.browse(track: .index(1), id: 300)
        #expect(b.value?.a == 2)
        #expect(last(m) == FizzSysEx.browseRequest(track: .index(1), id: 300))
        let f = try await l.flush()
        #expect(f.value?.tracks == 6)
        #expect(last(m) == FizzSysEx.flushRequest())
        let u = try await l.usedBy(source: .pool, slot: 4)
        #expect(u.value?.projects.count == 2)
        #expect(m.sentCommands == [0x02, 0x13, 0x12, 0x14, 0x15, 0x16, 0x24, 0x13, 0x31, 0x03, 0x17])
    }

    @Test func storeAndItsReply() async throws {
        let (l, m) = link()
        let blob = (0..<128).map { UInt8($0 & 0x7f) }
        let f = try FizzSysEx.storeSound(name: "NEW", blob: blob, slot: 0, own: true)
        let r = try await l.store(f)
        #expect(r.value?.slot == 9 && r.value?.pool == false)
        #expect(m.sentStores == [f])
        let into = try FizzSysEx.storeSound(name: "OVER", blob: blob, slot: 40)
        #expect(try await l.store(into).value?.slot == 40)
        m.set { $0.status[0x11] = 1 }
        #expect(try await l.store(f) == .refused(.full))
        m.set { $0.status[0x11] = 8 }
        #expect(try await l.store(into) == .refused(.busy))
        m.set { $0.errorFor[0x11] = 11 }
        #expect(try await l.store(f) == .refused(.badTrack), "FIZZ's ERROR for a STORE")
    }

    @Test func refusalsAsValues() async throws {
        let (l, m) = link()
        m.set { $0.inUseSteps = 16 }
        #expect(try await l.remove(source: .pool, slot: 3) == .refused(.inUse(projects: 1, steps: 16)))
        #expect(try await l.remove(source: .pool, slot: 3, confirm: true).isOK, "then with CONFIRM")
        m.set { $0.status[0x13] = 9 }
        #expect(try await l.rename(slot: 200, name: "X") == .refused(.notFound))
        m.set { $0.errorFor[0x12] = 7 }
        let old = try await l.tags(slot: 1, tags: 1)
        #expect(old == .refused(.updateFizz) && old.refusal?.isOld == true && old.refusal?.name == "BAD COMMAND")
        m.set { $0.playing = true }
        #expect(try await l.project(.load, 2) == .refused(.busy))
        #expect(try await l.swap(source: .pool, a: 1, b: 2) == .refused(.busy), "a pool swap: stopped only")
        #expect(try await l.swap(source: .mine, a: 1, b: 2).isOK, "MY PRESETS' swap: while playing too")
        #expect(try await l.flush() == .refused(.busy))
        #expect(FizzRefusal(status: 10, a: 2, b: 5).name == "IN USE" && FizzRefusal(status: 42) == .other(42))
        #expect(FizzRefusal.inUse(projects: 1, steps: 1).status == 10)
    }

    @Test func aWriteIsNotSentAgain() async throws {
        let (l, m) = link()
        _ = await l.status()
        m.set { $0.drop = 1 }
        await #expect(throws: FizzLinkError.timeout) { try await l.browse(id: 5) }
        #expect(m.sentCommands.filter { $0 == 0x31 }.count == 1)
        m.set { $0.dropStores = 1 }
        let f = try FizzSysEx.storeSound(name: "A", blob: [UInt8](repeating: 1, count: 96))
        await #expect(throws: FizzLinkError.timeout) { try await l.store(f) }
        #expect(m.sentStores.count == 1)
    }

    @Test func paramIsSentAgainOnce() async throws {
        let (l, m) = link()
        _ = await l.status()
        m.set { $0.drop = 1 }
        let r = try await l.param(track: .index(1), id: 5, value: -3)
        #expect(r.value?.a == 2 && r.value?.b == 5)
        let params = m.sent.map(\.bytes).filter { $0.count > 5 && $0[5] == 0x30 }
        #expect(params.count == 2 && params[0] == FizzSysEx.paramRequest(track: .index(1), id: 5, value: -3, ack: true))
        #expect(!m.sentCommands.suffix(2).contains(0x02), "a read-like op: no wake")
    }

    @Test func trackDumpAndNotes() async throws {
        let (l, m) = link()
        let f = try #require(try await l.dumpTrack())
        let d = try #require(FizzSysEx.parseDump(f))
        #expect(d.source == "track" && d.track == 2 && d.name == "OWN SOUND" && d.extras?.chan == 14)
        #expect(try await l.dumpTrack(slot: 5).flatMap(FizzSysEx.parseDump)?.track == 4)
        try await l.note(channel: 14, note: 60, velocity: 100)
        try await l.note(channel: 14, note: 60, velocity: 0)
        #expect(m.sent.suffix(2).map(\.bytes) == [[0x9e, 60, 100], [0x8e, 60, 0]])
        // an older FIZZ: no track DUMP (its DUMP of source 4 not answered as one)
        let (old, _) = link { $0.writes = false }
        #expect(try await old.dumpTrack() == nil)
    }

    @Test func bluetoothPort() async {
        #expect(FizzLink.isBlePort("FM-1 C1F0 Bluetooth") && FizzLink.isBlePort(" fm-1 bluetooth "))
        #expect(!FizzLink.isBlePort("FIZZ FM-1") && !FizzLink.isBlePort("Bluetooth"))
        let m = MockFizz(name: "FM-1 C1F0 Bluetooth", clock: clock)
        let l = FizzLink(transport: m, clock: clock)
        #expect(await l.viaBle)
    }
}
