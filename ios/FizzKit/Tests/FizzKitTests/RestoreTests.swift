// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// FizzLink.restore against MockFizz's unit side (FZSR, two passes of 0x30 pulls, FZOK / FZER): the happy path, an
// FZSR not taken up, a request lost and an answer lost (asked again), FZER, cancelling in each pass, unplugged.

import Foundation
import Testing
@testable import FizzKit

private final class Phases: @unchecked Sendable {
    private let lock = NSLock()
    private var seen: [(FizzRestorePhase, Double)] = []
    func add(_ p: FizzRestorePhase, _ f: Double) { lock.withLock { seen.append((p, f)) } }
    var all: [(FizzRestorePhase, Double)] { lock.withLock { seen } }
    var last: (FizzRestorePhase, Double)? { all.last }
}

@Suite("FizzLink restore", .serialized)
struct RestoreTests {
    let clock = ScaledClock(speed: 20)
    static let size = 32 * 1024

    private func link(_ configure: (MockFizz) -> Void = { _ in }) -> (FizzLink, MockFizz, [UInt8]) {
        let m = MockFizz(clock: clock)
        m.writes = true
        m.store = MockFizz.randomStore(size: Self.size)
        configure(m)
        return (FizzLink(transport: m, clock: clock), m, MockFizz.randomStore(size: Self.size, seed: 0xabcdef))
    }

    private func fzsrs(_ m: MockFizz) -> Int {
        m.sent.filter { MockFizz.bulkRequest($0.bytes)?.0 == "FZSR" }.count
    }

    @Test func happyPath() async throws {
        let (l, m, image) = link()
        let seen = Phases()
        let r = try await l.restore(image) { seen.add($0, $1) }
        #expect(r.served == 2 * Self.size / 512)
        #expect(m.store == image, "the unit's store is the image")
        #expect(m.pulled[0] == image && m.pulled[1] == image)
        #expect(fzsrs(m) == 1)
        let ph = seen.all
        #expect(ph.first?.0 == .checking && ph.first?.1 == 0)
        #expect(ph.contains { $0.0 == .writing })
        #expect(zip(ph, ph.dropFirst()).allSatisfy { $0.1 <= $1.1 }, "the fraction never goes back")
        #expect(ph.last?.0 == .writing && ph.last?.1 == 1)
        #expect(ph.firstIndex { $0.0 == .writing }.map { ph[$0].1 } == 0.5)
        let wake = m.sentCommands.first
        #expect(wake == 0x02, "a quiet link woken first")
    }

    @Test func fzsrNotTakenUpIsSentAgain() async throws {
        let (l, m, image) = link { $0.fzsrIgnore = 1 }
        _ = try await l.restore(image)
        #expect(fzsrs(m) == 2)
        #expect(m.store == image)
    }

    @Test func aLostRequestAndALostAnswerAreAskedAgain() async throws {
        let (l, m, image) = link { $0.loseRequestAt = 4096; $0.loseAnswerAt = 8192 }
        let r = try await l.restore(image)
        #expect(m.store == image)
        #expect(m.asked.filter { $0 == 4096 }.count == 3, "twice in the check pass (one lost), once writing")
        #expect(m.asked.filter { $0 == 8192 }.count == 3)
        #expect(r.served == 2 * Self.size / 512 + 1, "the answer lost was sent twice")
    }

    @Test func fzer() async throws {
        let (l, m, image) = link { $0.fzerAtEnd = 13 }
        await #expect(throws: FizzRestoreError.refused(fzer: 13, written: true)) { try await l.restore(image) }
        #expect(m.store != image)
        m.set { $0.fzerAtEnd = nil; $0.playing = true }
        await #expect(throws: FizzRestoreError.refused(fzer: 10, written: false)) { try await l.restore(image) }
        m.set { $0.playing = false }
        await #expect(throws: FizzRestoreError.refused(fzer: 11, written: false)) { try await l.restore(Array(image.prefix(4096))) }
        #expect(FizzRestoreError.refused(fzer: 12, written: false).text == "the image did not arrive whole (nothing was changed)")
    }

    @Test func cancelInTheCheckPass() async throws {
        let (l, m, image) = link()
        let before = m.store
        let seen = Phases()
        let t = Task { try await l.restore(image) { seen.add($0, $1) } }
        while (seen.last?.1 ?? 0) < 0.1 { try await Task.sleep(for: .milliseconds(1)) }
        t.cancel()
        await #expect(throws: FizzRestoreError.cancelled) { try await t.value }
        #expect(m.store == before, "nothing erased")
        #expect(m.pulled[1].isEmpty)
        #expect(!seen.all.contains { $0.0 == .writing })
    }

    @Test func cancelWhileWritingGoesOn() async throws {
        let (l, m, image) = link()
        let seen = Phases()
        let t = Task { try await l.restore(image) { seen.add($0, $1) } }
        while seen.last?.0 != .writing { try await Task.sleep(for: .milliseconds(1)) }
        t.cancel()
        _ = try await t.value
        #expect(m.store == image)
    }

    @Test func unpluggedWhileWriting() async throws {
        let (l, m, image) = link()
        let seen = Phases()
        let t = Task { try await l.restore(image) { seen.add($0, $1) } }
        while (seen.last?.1 ?? 0) < 0.7 { try await Task.sleep(for: .milliseconds(1)) }
        m.unplug()
        await #expect(throws: FizzRestoreError.lost(written: true)) { try await t.value }
    }

    @Test func notOverBluetooth() async throws {
        let m = MockFizz(name: "FM-1 C1F0 Bluetooth", clock: clock)
        m.writes = true
        let l = FizzLink(transport: m, clock: clock)
        await #expect(throws: FizzRestoreError.refused(fzer: 15, written: false)) { try await l.restore([0, 1, 2]) }
        #expect(m.sent.isEmpty)
    }
}
