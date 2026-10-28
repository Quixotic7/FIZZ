// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// FizzLink against MockFizz: retries, LIST parts, DUMP streams, one operation at a time, pacing, find().

import Foundation
import Testing
@testable import FizzKit

private final class Counter: @unchecked Sendable {
    private let lock = NSLock()
    private var calls: [(Int, Int)] = []
    func add(_ a: Int, _ b: Int) { lock.withLock { calls.append((a, b)) } }
    var all: [(Int, Int)] { lock.withLock { calls } }
}

@Suite("FizzLink", .serialized)
struct LinkTests {
    let clock = ScaledClock(speed: 20)

    private func link(_ configure: (MockFizz) -> Void = { _ in }) -> (FizzLink, MockFizz) {
        let m = MockFizz(clock: clock)
        configure(m)
        return (FizzLink(transport: m, clock: clock), m)
    }

    @Test func identity() async throws {
        let (l, m) = link()
        #expect(try await l.identity() == "0.12.0-beta.9")
        #expect(m.sent.count == 1)
    }

    @Test func identityRetriedAfterADroppedFrame() async throws {
        let (l, m) = link { $0.drop = 1 }
        #expect(try await l.identity() == "0.12.0-beta.9")
        #expect(m.sentCommands == [0x01, 0x01])
        let gap = m.sent[1].t - m.sent[0].t
        #expect(gap >= 700, "sent again after the 700 ms timeout (\(gap))")
    }

    @Test func identitySilentGivesNil() async throws {
        let (l, m) = link { $0.semantic = false }
        #expect(try await l.identity() == nil)
        #expect(m.sent.count == 4)                    // the first and 3 retries
    }

    @Test func listPartsConcatenated() async throws {
        let (l, m) = link()
        let rows = try await l.list()
        #expect(rows.count == 35)
        #expect(rows.map(\.slot) == Array(1...35))
        #expect(rows.first?.name == "P1" && rows.last?.name == "P35")
        #expect(m.sent.count == 1)
    }

    @Test func listTimesOut() async throws {
        let (l, _) = link { $0.semantic = false }
        await #expect(throws: FizzLinkError.timeout) { try await l.list() }
    }

    @Test func dumpAllWithProgress() async throws {
        let (l, _) = link()
        let seen = Counter()
        let frames = try await l.dumpAll(source: .mine, total: 5) { seen.add($0, $1) }
        #expect(frames.count == 5)
        #expect(frames.compactMap { FizzSysEx.parseDump($0)?.slot } == [1, 2, 3, 4, 5])
        #expect(seen.all.map(\.0) == [1, 2, 3, 4, 5])
        #expect(seen.all.allSatisfy { $0.1 == 5 })
        #expect(try await l.dumpAll(source: .mine, total: 0).isEmpty)
    }

    @Test func dumpAllPartialOnTimeout() async throws {
        let (l, _) = link { $0.dumpStopAfter = 3 }
        let t0 = clock.now
        let frames = try await l.dumpAll(source: .mine, total: 5)
        #expect(frames.count == 3)
        #expect(clock.now - t0 >= 4000)
    }

    @Test func dumpOne() async throws {
        let (l, m) = link { $0.drop = 1 }
        let f = try await l.dump(source: .mine, slot: 3)
        #expect(f.flatMap(FizzSysEx.parseDump)?.slot == 3)
        #expect(m.sent.count == 2)                    // a read: sent again
        #expect(try await l.dump(source: .mine, slot: 99) == nil)   // FIZZ's ERROR for DUMP
    }

    @Test func statusProjectsUsersStoreInfo() async throws {
        let (l, m) = link()
        let s = try #require(await l.status())
        #expect(s.tracks == 6 && s.playing && s.dirty && !s.noStore && s.project == 3 && s.sel == 3)
        #expect(s.lib == [300, 512] && s.prj == [20, 64] && s.written == 5000)
        let p = try #require(await l.projects())
        #expect(p.current == 2 && p.list.count == 8 && p.list[1].current && p.list[7].name == "PRJ 8")
        let u = try await l.users(source: .mine, slot: 130)
        guard case .reply(.users(let users?)) = u else { Issue.record("users: \(u)"); return }
        #expect(users.slot == 130 && users.projects == [FizzUserProject(project: 1, steps: 16),
                                                         FizzUserProject(project: 4, steps: 4)] && users.tracks == 1)
        let info = try #require(try await l.storeInfo())
        #expect(info == FizzStoreInfo(lo: 0x97000, size: 299_008, crc: m.storeCRC))
    }

    @Test func olderFizzAnswersBadCommand() async throws {
        let (l, m) = link()
        m.set { $0.semantic = true }
        // FLUSH is not in the mock: its ERROR (BAD COMMAND) comes back as refused, old
        let r = try await l.ask1(FizzSysEx.flushRequest(), cmd: FizzSysEx.CMD.flush, timeout: 1000, retry: 1)
        #expect(r == .refused(status: 7, statusName: "BAD COMMAND", old: true))
    }

    @Test func oneOperationAtATime() async throws {
        let (l, m) = link { $0.replyMs = 5 }
        let a = Task { await l.status() }
        try await Task.sleep(for: .milliseconds(2))
        let b = Task { try await l.identity() }
        try await Task.sleep(for: .milliseconds(2))
        let c = Task { try await l.list() }
        try await Task.sleep(for: .milliseconds(2))
        let d = Task { try await l.dump(source: .mine, slot: 2) }
        let (sa, sb, sc, sd) = (await a.value, try await b.value, try await c.value, try await d.value)
        #expect(sa?.tracks == 6)
        #expect(sb == "0.12.0-beta.9")
        #expect(sc.count == 35)
        #expect(sd.flatMap(FizzSysEx.parseDump)?.slot == 2)
        #expect(m.sentCommands == [0x02, 0x01, 0x21, 0x22], "in the callers' order")
        #expect(!m.overlapped, "a request sent while another's replies were coming")
    }

    @Test func pacing() async throws {
        let (l, m) = link { $0.replyMs = 0 }
        for _ in 0..<8 { _ = await l.status() }
        _ = try await l.identity()
        _ = try await l.dumpAll(source: .mine, total: 5)
        let t = m.sent.map(\.t)
        #expect(t.count == 10)
        for i in 1..<t.count { #expect(t[i] - t[i - 1] >= FizzLink.gapMs - 0.001, "gap \(i): \(t[i] - t[i - 1])") }
    }

    @Test func wakeSendsStatusOnlyWhenQuiet() async throws {
        let (l, m) = link()
        await l.wake()
        #expect(m.sentCommands == [0x02])
        await l.wake()                                 // heard just now
        #expect(m.sentCommands == [0x02])
        try await clock.sleep(ms: 1100)
        await l.wake()
        #expect(m.sentCommands == [0x02, 0x02])
    }

    @Test func lostDuringAnOperation() async throws {
        let (l, m) = link { $0.semantic = false }
        let lost = Counter()
        await l.setOnLost { lost.add(1, 1) }
        let t = Task { try await l.list() }
        try await clock.sleep(ms: 200)
        m.unplug()
        await #expect(throws: FizzLinkError.lost) { try await t.value }
        #expect(lost.all.count == 1)
        #expect(await l.isClosed)
        await #expect(throws: FizzLinkError.lost) { try await l.identity() }
    }

    @Test func closeFailsTheOperation() async throws {
        let (l, m) = link { $0.semantic = false }
        let t = Task { try await l.list() }
        try await clock.sleep(ms: 100)
        await l.close()
        await #expect(throws: FizzLinkError.closed) { try await t.value }
        #expect(m.isClosed)
    }

    // MARK: find

    private func unit(_ ref: UInt32, _ name: String, device: UInt32, _ configure: (MockFizz) -> Void = { _ in })
        -> (MIDIPortCandidate, MockFizz) {
        let m = MockFizz(name: name, clock: clock)
        configure(m)
        return (MockScanner.candidate(ref, name, device: device), m)
    }

    @Test func findsFizz() async throws {
        let stock = unit(1, "FM-1", device: 10) { $0.idText = "FM-1_016" }
        let fizz = unit(3, "FIZZ FM-1", device: 20)
        let r = try await FizzLink.find(scanner: MockScanner([stock, fizz]), clock: clock)
        #expect(r.idText == "FM-1_46001100" && r.portName == "FIZZ FM-1")
        #expect(await r.link.idText == "FM-1_46001100")
        #expect(stock.1.isClosed && !fizz.1.isClosed)
        #expect(try await r.link.identity() == "0.12.0-beta.9")
    }

    @Test func oneUnitTwoPortPairs() async throws {
        let a = unit(1, "FM-1", device: 10), b = unit(3, "FM-1 Port 2", device: 10)
        let units = await FizzLink.probe(scanner: MockScanner([a, b]), clock: clock)
        #expect(units.count == 1 && units[0].candidate.name == "FM-1")
        #expect(b.1.sent.isEmpty, "the same device not probed twice")
        // the first pair silent: the second one answers for the device
        let c = unit(5, "FM-1", device: 30) { $0.handshakes = false }, d = unit(7, "FM-1 Port 2", device: 30)
        let units2 = await FizzLink.probe(scanner: MockScanner([c, d]), clock: clock)
        #expect(units2.map(\.candidate.name) == ["FM-1 Port 2"])
        #expect(c.1.sent.count == 2 && c.1.isClosed)  // two handshakes, then closed
    }

    @Test func findErrors() async throws {
        await #expect(throws: FizzFindError.notFound) {
            try await FizzLink.find(scanner: MockScanner([]), clock: clock)
        }
        await #expect(throws: FizzFindError.notFound) {
            try await FizzLink.find(scanner: MockScanner([unit(1, "FM-1", device: 1) { $0.handshakes = false }]), clock: clock)
        }
        await #expect(throws: FizzFindError.updateMode("ota-FM1_015")) {
            try await FizzLink.find(scanner: MockScanner([unit(1, "FM-1", device: 1) { $0.idText = "ota-FM1_015" }]),
                                    clock: clock)
        }
        await #expect(throws: FizzFindError.notFizz("FM-1_016")) {
            try await FizzLink.find(scanner: MockScanner([unit(1, "FM-1", device: 1) { $0.idText = "FM-1_016" }]),
                                    clock: clock)
        }
    }
}
