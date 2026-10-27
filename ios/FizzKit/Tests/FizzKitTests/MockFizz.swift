// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// A simulated FM-1 behind a MIDITransport (no CoreMIDI): the update handshake, IDENTITY, STATUS, LIST in parts, DUMP,
// PROJECTS, USERS, FZBI. It can drop requests or go silent. ScaledClock: real time, run faster.

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

    private var log: [Sent] = []
    private var inFlight = false
    private(set) var overlapped = false
    private var lostFlag = false, closedFlag = false

    init(name: String = "FM-1", clock: ScaledClock) {
        self.name = name
        self.clock = clock
        (messages, cont) = AsyncStream.makeStream(of: [UInt8].self)
    }

    var sent: [Sent] { lock.withLock { log } }
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
                    return (0..<n).map { Self.dumpFrame(slot: $0 + 1, name: "D\($0 + 1)") }
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
        if let u = Optional(FM1Bulk.unpack7(b.dropFirst().dropLast())), u.count >= 10, u[2] == 0x46,
           String(decoding: u[6..<10], as: UTF8.self) == "FZBI" {
            let data: [UInt8] = Array("FZBI".utf8) + FM1Bulk.le32(0x97000) + FM1Bulk.le32(299_008) + FM1Bulk.le32(0x1a2b_3c4d)
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
