// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// The store's backup read and MY PRESETS read against MockFizz: drops, stalls, autosaves, FZER, cancellation,
// progress, pacing.

import Foundation
import Testing
@testable import FizzKit

private final class Seen<T>: @unchecked Sendable {
    private let lock = NSLock()
    private var items: [T] = []
    func add(_ x: T) { lock.withLock { items.append(x) } }
    var all: [T] { lock.withLock { items } }
}

@Suite("Backup", .serialized)
struct BackupTests {
    let clock = ScaledClock(speed: 20)
    static let small = MockFizz.randomStore(size: 40_000, seed: 7)     // 3 pieces, 157 chunks, a short last one

    private func link(_ clock: ScaledClock? = nil, _ configure: (MockFizz) -> Void = { _ in })
        -> (FizzLink, MockFizz, Seen<String>) {
        let c = clock ?? self.clock
        let m = MockFizz(clock: c)
        m.store = Self.small
        configure(m)
        let l = FizzLink(transport: m, clock: c)
        let log = Seen<String>()
        Task { await l.setLogger { log.add($0) } }
        return (l, m, log)
    }

    private func gapsHold(_ m: MockFizz) {
        let t = m.sent.map(\.t)
        for i in 1..<t.count where t[i] - t[i - 1] < FizzLink.gapMs - 0.001 {
            Issue.record("gap \(i): \(t[i] - t[i - 1]) ms")
        }
    }

    @Test func cleanRead() async throws {
        let (l, m, log) = link()
        let r = try await l.backup()
        #expect(r.image == Self.small)
        #expect(r.crc == FM1Bulk.crc32(Self.small) && r.lo == 0x97000 && r.size == 40_000)
        #expect(r.stats.bytes == 40_000 && r.stats.frames == 157 && r.stats.resent == 0)
        #expect(r.stats.depth == 2 && r.stats.attempt == 1 && r.stats.rtt.avg > 0 && r.stats.kbps > 0)
        #expect(r.stats.rtt.median <= r.stats.rtt.p95 && r.stats.rtt.p95 <= r.stats.rtt.max)
        #expect(m.dropped == 0, "depth 2: never more than two requests waiting at the unit")
        #expect(m.reads.sorted() == Array(stride(from: 0, to: 40_000, by: 256)))
        try await clock.sleep(ms: 20)
        #expect(log.all.contains { $0.hasPrefix("backup: 40000 B in ") && $0.contains("157 frames, 0 sent again") })
        gapsHold(m)
    }

    @Test func wholeRegion() async throws {
        let (l, m, _) = link { $0.store = MockFizz.defaultStore }
        let r = try await l.backup()
        #expect(r.size == 299_008 && r.image == MockFizz.defaultStore && r.crc == m.storeCRC)
        #expect(r.stats.frames == 1168 && r.stats.resent == 0 && m.dropped == 0)
        gapsHold(m)
    }

    @Test func dropsSentAgain() async throws {
        let (l, m, _) = link { $0.dropEvery = 7 }
        let r = try await l.backup()
        #expect(r.image == Self.small && r.stats.resent > 0)
        #expect(r.stats.frames == 157 + r.stats.resent)
        gapsHold(m)
    }

    @Test func depthThreeOverflowsTheUnitAndRecovers() async throws {
        // (replies 100 ms after: the third request always lands while two wait)
        let (l, m, _) = link { $0.store = MockFizz.randomStore(size: 8192, seed: 3); $0.bulkMs = 100 }
        let r = try await l.backup(depth: 3)
        #expect(r.image == m.store)
        #expect(m.dropped > 0, "a third request waiting at the unit dropped")
        #expect(r.stats.resent > 0)
    }

    @Test func stallReadsThePieceAgain() async throws {
        // silent for 5 s from the second piece: its read goes quiet (4 s), waits 1.5 s, and the second try goes through
        let (l, _, log) = link { $0.stallAt = 16384; $0.stallMs = 5000 }
        let r = try await l.backup()
        #expect(r.image == Self.small)
        #expect(log.all.contains { $0.hasPrefix("backup: the FM-1 went quiet at 4000") && $0.hasSuffix("(1)") })
    }

    @Test func autosaveOnceReadsAgain() async throws {
        let (l, m, log) = link { $0.changeAt = 0x100 }
        let r = try await l.backup()
        #expect(r.stats.attempt == 2)
        #expect(r.image == m.store && r.image != Self.small && r.crc == m.storeCRC)
        #expect(log.all.contains { $0.contains("reading it again") })
        #expect(m.sent.filter { MockFizz.bulkRequest($0.bytes)?.0 == "FZBI" }.count == 2)
    }

    @Test func alwaysChangingGivesChanged() async throws {
        let (l, m, _) = link { $0.changeAt = 0x100; $0.changeAlways = true }
        await #expect(throws: FizzBackupError.changed) { try await l.backup() }
        #expect(m.sent.filter { MockFizz.bulkRequest($0.bytes)?.0 == "FZBI" }.count == 2)
    }

    @Test func fzerStopsTheRead() async throws {
        let (l, _, _) = link { $0.fzerAt = 0x2000; $0.fzerCode = 10 }
        await #expect(throws: FizzBackupError.fzer(10)) { try await l.backup() }
        #expect(try await l.identity() == "0.12.0-beta.9", "the link still works")
    }

    @Test func olderFizzHasNoBulk() async throws {
        let (l, m, _) = link { $0.bulk = false }
        await #expect(throws: FizzBackupError.noBulk) { try await l.backup() }
        #expect(m.reads.isEmpty)
    }

    @Test func cancelStopsSending() async throws {
        let real = ScaledClock(speed: 1)                 // real time: "within 50 ms" means 50 ms
        let (l, m, _) = link(real) { $0.store = MockFizz.defaultStore }
        let t = Task { try await l.backup() }
        while m.reads.count < 30 { try await Task.sleep(for: .milliseconds(2)) }
        let at = real.now
        t.cancel()
        await #expect(throws: FizzBackupError.cancelled) { try await t.value }
        #expect(real.now - at < 50, "the error came \(real.now - at) ms after the cancel")
        let n = m.sent.count, last = m.sent.last!.t
        #expect(last - at < 50, "a frame sent \(last - at) ms after the cancel")
        try await Task.sleep(for: .milliseconds(60))
        #expect(m.sent.count == n, "nothing sent after the cancel")
        #expect(try await l.identity() == "0.12.0-beta.9", "the queue was let go")
    }

    @Test func cancelDuringThePauseBeforeAPieceReadAgain() async throws {
        let (l, m, log) = link { $0.stallAt = 0; $0.stallMs = 60_000 }
        let t = Task { try await l.backup() }
        while !log.all.contains(where: { $0.contains("went quiet") }) { try await Task.sleep(for: .milliseconds(2)) }
        t.cancel()
        await #expect(throws: FizzBackupError.cancelled) { try await t.value }
        let n = m.sent.count
        try await clock.sleep(ms: 2000)
        #expect(m.sent.count == n)
    }

    @Test func progressMonotonicToOne() async throws {
        let (l, _, _) = link()
        let seen = Seen<(Double, Int)>()
        _ = try await l.backup { seen.add(($0, $1)) }
        let p = seen.all
        #expect(p.count == 157)
        #expect(zip(p, p.dropFirst()).allSatisfy { $0.0 <= $1.0 })
        #expect(p.last?.0 == 1.0)
        #expect(p.map(\.1).sorted() == Array(stride(from: 0, to: 40_000, by: 256)))
    }

    @Test func oneOperationWhileItReads() async throws {
        let (l, m, _) = link()
        let b = Task { try await l.backup() }
        while m.reads.isEmpty { try await Task.sleep(for: .milliseconds(1)) }
        let s = await l.status()
        #expect(s?.tracks == 6)
        _ = try await b.value
        let kinds = m.sent.map { MockFizz.isFizz($0.bytes) ? "fizz" : MockFizz.bulkRequest($0.bytes)?.0 ?? "?" }
        #expect(kinds.first == "FZBI" && kinds.last == "fizz" && kinds.filter { $0 == "fizz" }.count == 1,
                "the STATUS waited for the backup")
    }

    // MARK: MY PRESETS

    private static func dumpSlots(_ m: MockFizz) -> [Int] {
        m.sent.filter { MockFizz.isFizz($0.bytes) && $0.bytes[5] == FizzSysEx.CMD.dump }
            .map { (Int($0.bytes[7]) << 7) | Int($0.bytes[8]) }
    }

    @Test func presetsFillTheMissingOnes() async throws {
        let (l, m, _) = link { $0.dumpTotal = 35; $0.dumpSkip = [7, 20] }
        let seen = Seen<(Int, Int)>()
        let r = try await l.presets { seen.add(($0, $1)) }
        #expect(r.rows.count == 35 && r.presets.count == 35)
        #expect(r.presets.map(\.entry.slot) == Array(1...35))
        #expect(r.presets.allSatisfy { FizzSysEx.parseDump($0.frame)?.slot == $0.entry.slot })
        #expect(Self.dumpSlots(m) == [0, 7, 20])
        #expect(seen.all.count == 33 && seen.all.allSatisfy { $0.1 == 35 })
    }

    @Test func presetsAllInOneStream() async throws {
        let (l, m, _) = link { $0.dumpTotal = 35 }
        let rows = try await l.list(factory: true, pool: true)
        let r = try await l.presets(rows: rows)
        #expect(r.presets.count == 35 && r.rows == rows)
        #expect(Self.dumpSlots(m) == [0])
    }

    @Test func presetsMissingGivesDump() async throws {
        let (l, _, _) = link { $0.dumpTotal = 33 }      // LIST counts 35; 34 and 35 answer FIZZ's error
        await #expect(throws: FizzBackupError.dump(got: 33, of: 35)) { try await l.presets() }
    }
}
