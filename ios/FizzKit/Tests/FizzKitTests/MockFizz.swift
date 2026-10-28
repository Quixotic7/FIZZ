// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// A simulated FM-1 behind a MIDITransport (no CoreMIDI): the update handshake, IDENTITY, STATUS, LIST in parts, DUMP,
// PROJECTS, USERS, FZBI, FZBR / FZBD over a store region. It can drop requests or go silent. ScaledClock: real time,
// run faster.

import Foundation
@testable import FizzKit

/// Real time run `speed` times faster: a 700 ms timeout takes 700 / speed ms.
struct ScaledClock: FizzClock {
    let speed: Double
    private let origin = ContinuousClock.now
    init(speed: Double = 20) { self.speed = speed }
    var now: Double {
        let d = ContinuousClock.now - origin
        return (Double(d.components.seconds) * 1000 + Double(d.components.attoseconds) / 1e15) * speed
    }
    func sleep(ms: Double) async throws {
        try await Task.sleep(for: .microseconds(max(0, ms * 1000 / speed)))
    }
}

final class MockFizz: MIDITransport, @unchecked Sendable {
    struct Sent { let t: Double; let bytes: [UInt8] }

    let name: String
    let messages: AsyncStream<[UInt8]>
    private let cont: AsyncStream<[UInt8]>.Continuation
    private let clock: ScaledClock
    private let lock = NSLock()

    // behaviour
    var idText = "FM-1_46001100"
    var version = "FIZZ 0.12.0-beta.9"
    var semantic = true                 // FIZZ answers its own frames
    var handshakes = true               // the update handshake answered
    var drop = 0                        // requests not answered
    var dumpTotal = 5                   // presets in MY PRESETS
    var dumpStopAfter: Int?             // DUMP slot 0 goes quiet after this many
    var replyMs = 1.0                   // between a request and each reply (the mock's clock)
    var dumpSkip: Set<Int> = []         // slots left out of DUMP slot 0's stream (lost on the way)

    // the store region (FZBI, FZBR)
    var bulk = true                     // FZBI and FZBR answered (false: a FIZZ before the bulk channel)
    var store = MockFizz.defaultStore
    var storeLo: UInt32 = 0x97000
    var bulkMs = 8.0                    // FZBR to its FZBD
    var hold = 2                        // FZBR waiting at most (fm1-base holds two inbound frames): more dropped
    var dropEvery = 0                   // every Nth FZBR not answered
    var stallAt: Int?                   // the first FZBR at or past this offset: silent for stallMs
    var stallMs = 0.0
    var changeAt: Int?                  // an FZBR for this offset: the store's last byte changes (an autosave)
    var changeAlways = false            // every time (else once)
    var fzerAt: Int?                    // an FZBR for this offset answered FZER fzerCode
    var fzerCode: UInt32 = 10

    private var log: [Sent] = []
    private var inFlight = false
    private(set) var overlapped = false
    private var waiting = 0, fzbrSeen = 0, silentUntil = -Double.infinity, stalled = false, changed = false
    private(set) var overflow = 0       // FZBR dropped: more than `hold` waiting
    private(set) var fzbrOffsets: [Int] = []
    private var lostFlag = false, closedFlag = false

    init(name: String = "FM-1", clock: ScaledClock) {
        self.name = name
        self.clock = clock
        (messages, cont) = AsyncStream.makeStream(of: [UInt8].self)
    }

    var sent: [Sent] { lock.withLock { log } }
    var reads: [Int] { lock.withLock { fzbrOffsets } }
    var dropped: Int { lock.withLock { overflow } }
    var storeCRC: UInt32 { lock.withLock { FM1Bulk.crc32(store) } }
    var sentCommands: [UInt8] { sent.map { Self.isFizz($0.bytes) ? $0.bytes[5] : 0 } }
    var isLost: Bool { lock.withLock { lostFlag } }
    var isClosed: Bool { lock.withLock { closedFlag } }

    func set(_ f: (MockFizz) -> Void) { lock.withLock { f(self) } }

    func send(_ bytes: [UInt8]) throws {
        let replies: [[UInt8]] = try lock.withLock {
            if lostFlag { throw MIDITransportError.lost }
            if closedFlag { throw MIDITransportError.closed }
            log.append(Sent(t: clock.now, bytes: bytes))
            if drop > 0 { drop -= 1; return [] }
            if let r = fzbr(bytes) { return r }
            let r = respond(bytes)
            if !r.isEmpty {
                if inFlight { overlapped = true }
                inFlight = true
            }
            return r
        }
        guard !replies.isEmpty else { return }
        let clock = clock, ms = replyMs
        Task {
            for r in replies {
                if ms > 0 { try? await clock.sleep(ms: ms) }
                self.deliver(r)
            }
            self.lock.withLock { self.inFlight = false }
        }
    }

    // FZBR off n: FZBD after bulkMs, unless dropped (a knob, or more than `hold` waiting)
    private func fzbr(_ b: [UInt8]) -> [[UInt8]]? {
        guard let (tag, words) = Self.bulkRequest(b), tag == "FZBR", words.count >= 2 else { return nil }
        let off = Int(words[0]), n = Int(words[1])
        fzbrOffsets.append(off)
        fzbrSeen += 1
        guard bulk else { return [] }
        let now = clock.now
        if dropEvery > 0 && fzbrSeen % dropEvery == 0 { return [] }
        if let s = stallAt, off >= s, !stalled { stalled = true; silentUntil = now + stallMs }
        if now < silentUntil { return [] }
        if waiting >= hold { overflow += 1; return [] }
        let reply: [UInt8]
        if off == fzerAt {
            reply = FM1Bulk.updateFrame(cmd: 0x46, payload: Array("FZER".utf8) + FM1Bulk.le32(fzerCode))
        } else {
            if off == changeAt && (changeAlways || !changed) { changed = true; store[store.count - 1] &+= 1 }
            guard off >= 0, n > 0, n <= FM1Bulk.chunk, off + n <= store.count else { return [] }
            reply = FM1Bulk.updateFrame(cmd: 0x46, payload: Array("FZBD".utf8) + FM1Bulk.le32(UInt32(off)) +
                                        Array(store[off..<(off + n)]))
        }
        waiting += 1
        let clock = clock, ms = bulkMs
        Task {
            if ms > 0 { try? await clock.sleep(ms: ms) }
            self.lock.withLock { self.waiting -= 1 }
            self.deliver(reply)
        }
        return []
    }

    /// A bulk request's tag and words.
    static func bulkRequest(_ b: [UInt8]) -> (String, [UInt32])? {
        guard b.count >= 3, b[0] == 0xf0, b[b.count - 1] == 0xf7 else { return nil }
        let u = FM1Bulk.unpack7(b.dropFirst().dropLast())
        guard u.count >= 11, u[0] == 0, u[1] == 0x59, u[2] == 0x46 else { return nil }
        let n = Int(u[3]) | (Int(u[4]) << 8)
        guard n >= 4, u.count >= 6 + n else { return nil }
        let words = stride(from: 10, to: 6 + n - 3, by: 4).map { FM1Bulk.rd32(u, $0) }
        return (String(decoding: u[6..<10], as: UTF8.self), words)
    }

    /// Seeded bytes (xorshift), the same every run.
    static func randomStore(size: Int, seed: UInt64 = 0x46_495a_5a) -> [UInt8] {
        var x = seed | 1
        return (0..<size).map { _ in
            x ^= x << 13; x ^= x >> 7; x ^= x << 17
            return UInt8(truncatingIfNeeded: x >> 24)
        }
    }

    static let defaultStore = randomStore(size: 299_008)

    private func deliver(_ r: [UInt8]) {
        let ok = lock.withLock { !lostFlag && !closedFlag }
        if ok { cont.yield(r) }
    }

    func unplug() {
        lock.withLock { lostFlag = true }
        cont.finish()
    }

    func close() {
        lock.withLock { closedFlag = true }
        cont.finish()
    }

    // MARK: the unit

    static func isFizz(_ b: [UInt8]) -> Bool { b.count >= 7 && b[1] == 0x7d && b[2] == 0x46 && b[3] == 0x5a }
    static func ascii(_ s: String) -> [Int] { s.unicodeScalars.map { Int($0.value) } }
    static func name12(_ s: String) -> [Int] { ascii(s.padding(toLength: 12, withPad: " ", startingAt: 0)) }
    static func r14(_ v: Int) -> [Int] { [(v >> 7) & 0x7f, v & 0x7f] }

    static func dumpFrame(slot: Int, name: String) -> [UInt8] {
        let blob = (0..<96).map { UInt8(($0 * 7 + slot) & 0xff) }
        let body = [0x08] + r14(slot) + name12(name) + [5, 0, 64, 1, 2, 3, 0] + FizzSysEx.pack7(blob).map(Int.init) + [1]
        return FizzSysEx.frame(0x10, body)
    }

    static func entry(_ slot: Int) -> [Int] {
        [0] + r14(slot) + [0, (slot >> 7) & 0x7f, slot & 0x7f] + name12("P\(slot)") + [1, 1]
    }

    private func respond(_ b: [UInt8]) -> [[UInt8]] {
        if b == FM1Bulk.handshakeQuery {
            guard handshakes else { return [] }
            let t = Array(idText.utf8.prefix(27)) + [UInt8](repeating: 0, count: max(0, 27 - idText.utf8.count))
            return [FM1Bulk.updateFrame(cmd: 0x11, payload: t)]
        }
        if Self.isFizz(b) {
            guard semantic else { return [] }
            switch b[5] {
            case FizzSysEx.CMD.identity:
                return [FizzSysEx.frame(0x41, [1] + Self.ascii(version))]
            case FizzSysEx.CMD.status:
                let body = [1, 6, 0x05, 2, 17, 8] + Self.r14(dumpTotal) + Self.r14(40) + [0] + Self.r14(5000) +
                    Self.r14(300) + Self.r14(512) + Self.r14(20) + Self.r14(64) + [0, 3]
                return [FizzSysEx.frame(0x42, body)]
            case FizzSysEx.CMD.list:
                let slots = Array(1...35)
                let parts = [Array(slots[0..<16]), Array(slots[16..<32]), Array(slots[32...])]
                return parts.enumerated().map { k, p in
                    FizzSysEx.frame(0x61, [k, k == 2 ? 1 : 0] + Self.r14(35) + [p.count] + p.flatMap(Self.entry))
                }
            case FizzSysEx.CMD.dump:
                let slot = (Int(b[7]) << 7) | Int(b[8])
                if slot == 0 {
                    let n = min(dumpTotal, dumpStopAfter ?? dumpTotal)
                    return (0..<n).map { $0 + 1 }.filter { !dumpSkip.contains($0) }
                        .map { Self.dumpFrame(slot: $0, name: "D\($0)") }
                }
                if slot <= dumpTotal { return [Self.dumpFrame(slot: slot, name: "D\(slot)")] }
                return [FizzSysEx.frame(0x7f, [5, 0x22])]
            case FizzSysEx.CMD.projects:
                let list = (0..<8).flatMap { p in [p == 1 ? 3 : 1] + Self.name12("PRJ \(p + 1)") + Self.r14(p * 3) }
                return [FizzSysEx.frame(0x63, [1, 8] + list)]
            case FizzSysEx.CMD.users:
                return [FizzSysEx.frame(0x57, [Int(b[6]), Int(b[7]), Int(b[8]), 2, 0] + Self.r14(16) + [3] + Self.r14(4) + [1])]
            default:
                return [FizzSysEx.frame(0x7f, [7, Int(b[5])])]
            }
        }
        if let (tag, _) = Self.bulkRequest(b), tag == "FZBI" {
            guard bulk else { return [] }
            let data: [UInt8] = Array("FZBI".utf8) + FM1Bulk.le32(storeLo) + FM1Bulk.le32(UInt32(store.count)) +
                FM1Bulk.le32(FM1Bulk.crc32(store))
            return [FM1Bulk.updateFrame(cmd: 0x46, payload: data)]
        }
        return []
    }
}

/// Port pairs, each opened on a transport the test made.
final class MockScanner: MIDIPortScanning, @unchecked Sendable {
    let units: [(MIDIPortCandidate, MockFizz)]
    init(_ units: [(MIDIPortCandidate, MockFizz)]) { self.units = units }
    func candidates() -> [MIDIPortCandidate] { units.map(\.0) }
    func open(_ c: MIDIPortCandidate) throws -> any MIDITransport {
        guard let u = units.first(where: { $0.0 == c }) else { throw MIDITransportError.lost }
        return u.1
    }

    static func candidate(_ ref: UInt32, _ name: String, device: UInt32) -> MIDIPortCandidate {
        MIDIPortCandidate(source: MIDIEndpointInfo(ref: ref, name: name, entity: ref, device: device),
                          destination: MIDIEndpointInfo(ref: ref + 1000, name: name, entity: ref, device: device))
    }
}
