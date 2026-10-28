// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// The whole store written (site/manager/fizzlink.js restore; firmware/app/app.c fz_restore): RESTORE, and PROJECTS'
// WRITE TO FM-1. FZSR len crc starts it (sent again, 3 times at most, when the unit pulls nothing 3 s after it); the
// unit then pulls the image with the update's 512-byte read requests (cmd 0x30), each answered from the image
// (FM1Bulk.response), twice: first all of it, to check its CRC-32 before anything is erased (the check pass), then
// again while it erases and writes each sector (the write pass: the addresses start again). It ends with FZOK, or
// FZER and a code (10 playing, 11 another size, 12 not whole: nothing changed, 13 a write failed: FIZZ cleared its
// store, 14 read back other, 15 over Bluetooth). Cancelling is honoured in the check pass only: the reads still to
// come are answered with zeros, so the unit's CRC check fails and nothing is erased (FZER 12). The unit asks a lost
// pull again every second; a request asked twice is answered twice.

import Foundation

public enum FizzRestorePhase: String, Sendable, Equatable {
    case checking       // the unit reads the image to check it: nothing changed yet
    case writing        // the unit erases and writes: DO NOT UNPLUG
}

public struct FizzRestoreDone: Sendable, Equatable {
    public let ms: Double
    /// read requests answered (both passes)
    public let served: Int
}

public enum FizzRestoreError: Error, Sendable, Equatable {
    case refused(fzer: Int, written: Bool)      // FZER (FM1Bulk.fzer's texts)
    case cancelled                              // in the check pass: nothing changed
    case badRequest(addr: Int, len: Int, written: Bool)   // the unit asked for bytes outside the image
    case timeout(written: Bool)                 // the unit went quiet (30 s)
    case lost(written: Bool)                    // unplugged, or the link closed

    /// The write pass had begun: FIZZ may have cleared its store (the safety backup is the way back).
    public var written: Bool {
        switch self {
        case .refused(_, let w), .badRequest(_, _, let w), .timeout(let w), .lost(let w): w
        case .cancelled: false
        }
    }

    public var fzer: Int? { if case .refused(let c, _) = self { c } else { nil } }

    public var text: String {
        switch self {
        case .refused(let c, _): FM1Bulk.fzer[UInt32(c)] ?? "FZER \(c)"
        case .cancelled: "cancelled"
        case .badRequest(let a, let n, _): "the FM-1 asked for \(String(a, radix: 16))+\(n), outside the image"
        case .timeout: "the FM-1 did not answer"
        case .lost: "the FM-1 was disconnected"
        }
    }
}

/// The restore: the link's current operation while it runs (its frames go to FizzLink.restoreFrame).
final class RestoreOp: PendingOp {
    let id: Int, timeout: Double, retry = 0
    var tries = 0, heard = false, gen = 0
    var timer: Task<Void, Never>?
    var kick: Task<Void, Never>?
    var chain: Task<Void, Never>?               // the replies sent in the order asked
    let image: [UInt8], crc: UInt32, t0: Double
    let progress: (@Sendable (FizzRestorePhase, Double) -> Void)?
    var pass = 1, last = -1, served = 0, sends = 0
    var bytes = [0, 0]
    var cancel = false, cancelled = false
    private(set) var finished = false
    private var k: CheckedContinuation<FizzRestoreDone, Error>?

    init(id: Int, image: [UInt8], idleMs: Double, t0: Double, progress: (@Sendable (FizzRestorePhase, Double) -> Void)?,
         k: CheckedContinuation<FizzRestoreDone, Error>) {
        self.id = id; self.timeout = idleMs; self.image = image; self.crc = FM1Bulk.crc32(image); self.t0 = t0
        self.progress = progress; self.k = k
    }

    var written: Bool { pass == 2 }
    func makeFrame() throws -> [UInt8] { [] }
    func frame(_ p: [UInt8]) -> FrameOutcome { .ignore }    // (FizzLink.restoreFrame reads them)
    func expire() { finish(.failure(FizzRestoreError.timeout(written: written))) }
    func fail(_ e: Error) { finish(.failure(FizzRestoreError.lost(written: written))) }

    func finish(_ r: Result<FizzRestoreDone, Error>) {
        guard !finished else { return }
        finished = true
        timer?.cancel(); kick?.cancel()
        let k = self.k
        self.k = nil
        k?.resume(with: r)
    }
}

extension FizzLink {
    public static let restoreIdleMs = 30000.0
    static let fzsrAgainMs = 3000.0, fzsrSends = 3, pullMax = 512

    /// The store written whole from `image` (its size the unit's store: FZER 11 otherwise). progress(phase, fraction
    /// of both passes) at the start, on the write pass's start and after each read answered. Cancelling the task
    /// stops it in the check pass (FizzRestoreError.cancelled, nothing changed); in the write pass it goes on.
    public func restore(_ image: [UInt8], idleMs: Double = FizzLink.restoreIdleMs,
                        progress: (@Sendable (FizzRestorePhase, Double) -> Void)? = nil) async throws -> FizzRestoreDone {
        if viaBle { throw FizzRestoreError.refused(fzer: 15, written: false) }   // (the unit pulls over USB only)
        await wake()
        await acquire()
        defer { release() }
        if isClosed { throw FizzRestoreError.lost(written: false) }
        opSeq += 1
        let id = opSeq
        return try await withTaskCancellationHandler {
            try await withCheckedThrowingContinuation { (k: CheckedContinuation<FizzRestoreDone, Error>) in
                let r = RestoreOp(id: id, image: image, idleMs: idleMs, t0: clock.now, progress: progress, k: k)
                cur = r
                arm(r)
                progress?(.checking, 0)
                if Task.isCancelled { r.cancel = true }
                restoreGo(r)
            }
        } onCancel: {
            Task { await self.restoreCancel(id) }
        }
    }

    func restoreOp(_ id: Int) -> RestoreOp? {
        guard let r = cur as? RestoreOp, r.id == id, !r.finished else { return nil }
        return r
    }

    func restoreCancel(_ id: Int) {
        guard let r = restoreOp(id) else { return }
        if r.pass == 1 { r.cancel = true } else { logger?("restore: writing: not cancelled") }
    }

    /// Sent after the ones before (a Task each would race).
    func restoreSend(_ r: RestoreOp, _ bytes: [UInt8]) {
        let prev = r.chain
        r.chain = Task {
            await prev?.value
            _ = try? await self.send(bytes)
        }
    }

    /// FZSR; again 3 s later while nothing was pulled (3 sends at most: the unit starts on one it heard).
    func restoreGo(_ r: RestoreOp) {
        r.sends += 1
        restoreSend(r, FM1Bulk.bulkFrame(tag: "FZSR", words: [UInt32(r.image.count), r.crc]))
        r.kick?.cancel()
        let id = r.id, clock = clock
        r.kick = Task {
            do { try await clock.sleep(ms: Self.fzsrAgainMs) } catch { return }
            self.restoreKick(id)
        }
    }

    func restoreKick(_ id: Int) {
        guard let r = restoreOp(id), r.served == 0, r.sends < Self.fzsrSends, !isClosed else { return }
        logger?("restore: FZSR not taken up, sent again (\(r.sends))")
        restoreGo(r)
    }

    func endRestore(_ r: RestoreOp, _ result: Result<FizzRestoreDone, Error>) {
        if cur === r { cur = nil }
        r.finish(result)
    }

    func restoreFrame(_ r: RestoreOp, _ p: [UInt8]) {
        let len = r.image.count
        if let q = FM1Bulk.parseRequest(p) {
            r.kick?.cancel()
            let addr = Int(q.addr)
            guard q.len <= Self.pullMax, addr + q.len <= len else {
                endRestore(r, .failure(FizzRestoreError.badRequest(addr: addr, len: q.len, written: r.written)))
                return
            }
            if r.pass == 1 && addr < r.last {
                r.pass = 2
                r.progress?(.writing, Double(r.bytes[0]) / Double(2 * max(1, len)))
            }
            r.last = addr
            let fake = r.pass == 1 && r.cancel
            if fake && !r.cancelled {
                r.cancelled = true
                logger?("restore: cancelled before anything was erased")
            }
            let data = fake ? [UInt8](repeating: 0, count: q.len) : Array(r.image[addr..<(addr + q.len)])
            restoreSend(r, FM1Bulk.response(addr: q.addr, data: data, fl: q.fl))
            r.served += 1
            r.bytes[r.pass - 1] = max(r.bytes[r.pass - 1], addr + q.len)
            r.progress?(r.pass == 1 ? .checking : .writing, Double(r.bytes[0] + r.bytes[1]) / Double(2 * max(1, len)))
            r.heard = true
            arm(r)
            return
        }
        guard let b = FM1Bulk.parseBulk(p), b.tag == "FZOK" || b.tag == "FZER" else { return }
        let code = b.data.count >= 4 ? Int(FM1Bulk.rd32(b.data, 0)) : 0
        let ms = clock.now - r.t0
        let kbps = ms > 0 ? Double(2 * len) / 1024 / (ms / 1000) : 0
        logger?("restore: \(b.tag) \(code) after \(r.served) reads, \(String(format: "%.0f", ms)) ms " +
                "(\(String(format: "%.1f", kbps)) KB/s both passes)")
        if b.tag == "FZOK" {
            endRestore(r, .success(FizzRestoreDone(ms: ms, served: r.served)))
        } else if r.cancelled && code == 12 {
            endRestore(r, .failure(FizzRestoreError.cancelled))
        } else {
            endRestore(r, .failure(FizzRestoreError.refused(fzer: code, written: r.written)))
        }
    }
}
