// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// The store read whole (site/manager/fizzlink.js backup / readPatient / readRegion) and MY PRESETS read
// (site/manager/manager.js readUnit). Read-only: FZBI, FZBR, LIST and DUMP, nothing that writes.
//   - FZBI gives the region's size and CRC-32; the region is read in 16 KB pieces, each with FZBR (offset, length)
//     requests for 256-byte chunks, `depth` outstanding, answered by FZBD (offset, bytes) matched by offset;
//   - an overdue chunk is asked again (one per 10 ms tick, the oldest first); a piece that goes quiet is read again
//     after a pause; the copy's CRC-32 against FZBI's, read once more when they differ (an autosave).
// The whole backup is one operation of the link's queue: nothing else interleaves.

import Foundation

// MARK: - Results

/// Round trips of the chunks answered at the first ask (ms).
public struct FizzRTT: Sendable, Equatable {
    public var avg = 0.0, median = 0.0, p95 = 0.0, max = 0.0

    static func of(_ rtts: [Double]) -> FizzRTT {
        guard !rtts.isEmpty else { return FizzRTT() }
        let s = rtts.sorted()
        func q(_ f: Double) -> Double { s[min(s.count - 1, Int(f * Double(s.count)))] }
        return FizzRTT(avg: s.reduce(0, +) / Double(s.count), median: q(0.5), p95: q(0.95), max: s[s.count - 1])
    }
}

public struct FizzBackupStats: Sendable, Equatable {
    public var bytes: Int
    public var frames: Int          // FZBR sent
    public var resent: Int          // of them, sent again
    public var depth: Int
    public var gapMs: Double
    public var ms: Double
    public var kbps: Double
    public var perFrame: Double     // ms a chunk
    public var rtt: FizzRTT
    public var attempt: Int         // 2: read again after a CRC mismatch
}

public struct FizzStoreRead: Sendable {
    public let image: [UInt8]
    public let lo: Int
    public let size: Int
    public let crc: UInt32
    public let stats: FizzBackupStats
}

public enum FizzBackupError: Error, Equatable {
    case noBulk                     // this FIZZ has no backup over USB (update FIZZ)
    case changed                    // the store changed while it was read, twice
    case cancelled
    case fzer(Int)                  // the FM-1 refused a read
    case timeout(offset: Int)       // the FM-1 stopped answering at this offset
    case dump(got: Int, of: Int)    // MY PRESETS: not every preset came back
}

/// One of MY PRESETS: its LIST entry and its DUMP frame.
public struct FizzPreset: Sendable, Equatable {
    public let entry: FizzListEntry
    public let frame: [UInt8]
}

public struct FizzPresetsRead: Sendable, Equatable {
    public let rows: [FizzListEntry]    // the whole LIST (MY PRESETS, the factory's, the pool's)
    public let presets: [FizzPreset]    // MY PRESETS, in slot order
}

// MARK: - The read in flight

/// One piece's read: the link's current operation while it runs (the frames go to FizzLink.regionFrame).
final class RegionRead: PendingOp {
    struct Ask { var t = 0.0; var tries = 0; var again = false }
    struct Result { let piece: [UInt8]; let frames: Int; let resent: Int; let rtt: FizzRTT }

    let id: Int, timeout: Double, retry = 0
    var tries = 0, heard = false, gen = 0
    var timer: Task<Void, Never>?
    var ticker: Task<Void, Never>?
    var wait: Task<Void, Never>?

    let size: Int, from: Int, to: Int, k0: Int, end: Int, n: Int, depth: Int, gapMs: Double
    let progress: (Double, Int) -> Void
    var piece: [UInt8]
    var fly: [Int: Ask] = [:]
    var next: Int, done = 0, resent = 0, frames = 0, rto = FizzLink.rtoStart, sent = -Double.infinity
    var rtts: [Double] = []
    private(set) var finished = false
    private var k: CheckedContinuation<Result, Error>?

    init(id: Int, size: Int, from: Int, to: Int, depth: Int, gapMs: Double, idleMs: Double,
         progress: @escaping (Double, Int) -> Void, k: CheckedContinuation<Result, Error>) {
        let c = FM1Bulk.chunk
        self.id = id; self.timeout = idleMs; self.size = size; self.from = from; self.to = to
        self.k0 = from / c; self.end = (to + c - 1) / c; self.n = end - k0
        self.depth = depth; self.gapMs = gapMs; self.progress = progress; self.k = k
        self.next = k0
        self.piece = [UInt8](repeating: 0, count: to - from)
    }

    func want(_ k: Int) -> Int { min(FM1Bulk.chunk, size - k * FM1Bulk.chunk) }

    func makeFrame() throws -> [UInt8] { [] }
    func frame(_ p: [UInt8]) -> FrameOutcome { .ignore }    // (FizzLink.regionFrame reads them)

    /// Nothing heard for idleMs: the oldest chunk asked.
    func expire() {
        let k = fly.min { $0.value.t < $1.value.t }?.key
        finish(.failure(FizzBackupError.timeout(offset: (k ?? k0) * FM1Bulk.chunk)))
    }

    func fail(_ e: Error) { finish(.failure(e)) }

    func finish(_ r: Swift.Result<Result, Error>) {
        guard !finished else { return }
        finished = true
        timer?.cancel(); ticker?.cancel(); wait?.cancel()
        let k = self.k
        self.k = nil
        k?.resume(with: r)
    }
}

// MARK: - The link's backup

extension FizzLink {
    public static let backupPiece = 16384
    static let pieceTries = 5, pieceIdleMs = 4000.0, piecePauseMs = 1500.0
    static let tickMs = 10.0, rtoStart = 800.0, rtoMin = 150.0, rtoMax = 800.0, askLimit = 20

    /// The store read whole: `depth` FZBR outstanding (the Manager's 2; fm1-base holds two inbound frames), each next
    /// one at least `gapMs` after the one before. progress(fraction of the region, the offset just read). Task
    /// cancellation stops it (FizzBackupError.cancelled).
    public func backup(depth: Int = 2, gapMs: Double = 0, tries: Int = 2,
                       progress: (@Sendable (Double, Int) -> Void)? = nil) async throws -> FizzStoreRead {
        await acquire()
        defer { release() }
        let depth = max(1, depth)
        do {
            var attempt = 0
            while true {
                attempt += 1
                try Self.checkCancel()
                guard let info = try await storeInfo(held: true) else { throw FizzBackupError.noBulk }
                try Self.checkCancel()
                let (image, read) = try await readPatient(info, depth: depth, gapMs: gapMs, progress: progress)
                var stats = read
                let crc = FM1Bulk.crc32(image)
                stats.attempt = attempt
                logger?(Self.backupLine(info, stats, crc))
                if crc == info.crc {
                    return FizzStoreRead(image: image, lo: Int(info.lo), size: Int(info.size), crc: crc, stats: stats)
                }
                if attempt >= tries { throw FizzBackupError.changed }
                logger?("backup: the store changed while it was read (an autosave?): reading it again")
            }
        } catch is CancellationError {
            throw FizzBackupError.cancelled
        }
    }

    static func checkCancel() throws { if Task.isCancelled { throw FizzBackupError.cancelled } }

    static func backupLine(_ info: FizzStoreInfo, _ s: FizzBackupStats, _ crc: UInt32) -> String {
        func f(_ v: Double) -> String { String(format: "%.1f", v) }
        return "backup: \(info.size) B in \(String(format: "%.0f", s.ms)) ms, \(f(s.kbps)) KB/s, depth \(s.depth), " +
            "\(s.frames) frames, \(s.resent) sent again, round trip avg \(f(s.rtt.avg)) / median \(f(s.rtt.median)) / " +
            "p95 \(f(s.rtt.p95)) / max \(f(s.rtt.max)) ms; CRC-32 \(hex8(crc)) (the unit's \(hex8(info.crc)))"
    }

    static func hex8(_ v: UInt32) -> String { String(format: "%08x", v) }

    /// The region in 16 KB pieces; a piece that goes quiet read again after a pause (5 tries).
    func readPatient(_ info: FizzStoreInfo, depth: Int, gapMs: Double,
                     progress: (@Sendable (Double, Int) -> Void)?) async throws -> ([UInt8], FizzBackupStats) {
        let size = Int(info.size), t0 = clock.now
        var image = [UInt8](repeating: 0, count: size)
        var frames = 0, resent = 0, rtts: [FizzRTT] = []
        let shown = Box(0.0)                // (a piece read again: the fraction does not go back)
        var from = 0
        while from < size {
            let to = min(size, from + Self.backupPiece), lo = from
            let onRead: (Double, Int) -> Void = { f, off in
                shown.v = Swift.max(shown.v, (Double(lo) + f * Double(to - lo)) / Double(size))
                progress?(shown.v, off)
            }
            var got: RegionRead.Result?
            var k = 1
            while got == nil {
                do {
                    got = try await readRegion(size: size, from: from, to: to, depth: depth, gapMs: gapMs,
                                               idleMs: Self.pieceIdleMs, progress: onRead)
                } catch FizzBackupError.timeout(let off) where k < Self.pieceTries && !Task.isCancelled {
                    logger?("backup: the FM-1 went quiet at \(String(from, radix: 16)) (\(String(off, radix: 16))): " +
                            "that piece read again (\(k))")
                    try await clock.sleep(ms: Self.piecePauseMs)
                    k += 1
                }
            }
            guard let r = got else { break }
            image.replaceSubrange(from..<to, with: r.piece)
            frames += r.frames; resent += r.resent; rtts.append(r.rtt)
            from = to
        }
        let ms = clock.now - t0, n = (size + FM1Bulk.chunk - 1) / FM1Bulk.chunk
        let rtt = rtts.isEmpty ? FizzRTT() : FizzRTT(avg: rtts.reduce(0) { $0 + $1.avg } / Double(rtts.count),
                                                    median: rtts.map(\.median).max()!, p95: rtts.map(\.p95).max()!,
                                                    max: rtts.map(\.max).max()!)
        let stats = FizzBackupStats(bytes: size, frames: frames, resent: resent, depth: depth, gapMs: gapMs, ms: ms,
                                    kbps: ms > 0 ? Double(size) / 1024 / (ms / 1000) : 0,
                                    perFrame: n > 0 ? ms / Double(n) : 0, rtt: rtt, attempt: 1)
        return (image, stats)
    }

    /// One piece [from, to): FZBR for each chunk, `depth` outstanding.
    func readRegion(size: Int, from: Int, to: Int, depth: Int, gapMs: Double, idleMs: Double,
                    progress: @escaping (Double, Int) -> Void) async throws -> RegionRead.Result {
        if isClosed { throw FizzLinkError.lost }
        try Self.checkCancel()
        opSeq += 1
        let id = opSeq
        return try await withTaskCancellationHandler {
            try await withCheckedThrowingContinuation { (k: CheckedContinuation<RegionRead.Result, Error>) in
                let r = RegionRead(id: id, size: size, from: from, to: to, depth: depth, gapMs: gapMs, idleMs: idleMs,
                                   progress: progress, k: k)
                cur = r
                arm(r)
                let clock = clock
                r.ticker = Task {
                    while true {
                        do { try await clock.sleep(ms: Self.tickMs) } catch { return }
                        if !self.regionTick(id) { return }
                    }
                }
                regionFill(r)
            }
        } onCancel: {
            Task { await self.regionCancel(id) }
        }
    }

    func region(_ id: Int) -> RegionRead? {
        guard let r = cur as? RegionRead, r.id == id, !r.finished else { return nil }
        return r
    }

    func regionAlive(_ id: Int) -> Bool { region(id) != nil }

    func endRegion(_ r: RegionRead, _ result: Swift.Result<RegionRead.Result, Error>) {
        if cur === r { cur = nil }
        r.finish(result)
    }

    func regionCancel(_ id: Int) {
        if let r = region(id) { endRegion(r, .failure(FizzBackupError.cancelled)) }
    }

    /// The next requests, `gapMs` apart at least.
    func regionFill(_ r: RegionRead) {
        r.wait?.cancel()
        r.wait = nil
        while r.fly.count < r.depth && r.next < r.end {
            let left = r.gapMs - (clock.now - r.sent)
            if !r.fly.isEmpty && left > 0 {
                let id = r.id, clock = clock
                r.wait = Task {
                    do { try await clock.sleep(ms: left) } catch { return }
                    if let r = self.region(id) { self.regionFill(r) }
                }
                return
            }
            regionAsk(r, r.next, again: false)
            r.next += 1
        }
    }

    func regionAsk(_ r: RegionRead, _ k: Int, again: Bool) {
        var e = r.fly[k] ?? RegionRead.Ask()
        e.t = clock.now
        r.sent = e.t
        e.tries += 1
        e.again = again || e.tries > 1
        r.fly[k] = e
        r.frames += 1
        if again { r.resent += 1 }
        let bytes = FM1Bulk.bulkFrame(tag: "FZBR", words: [UInt32(k * FM1Bulk.chunk), UInt32(r.want(k))])
        let id = r.id
        Task { _ = try? await self.send(bytes, region: id) }
    }

    /// Every 10 ms: the oldest overdue chunk asked again (more at once would collide); 20 asks and it fails.
    func regionTick(_ id: Int) -> Bool {
        guard let r = region(id) else { return false }
        let t = clock.now
        guard let (k, e) = r.fly.filter({ t - $0.value.t >= r.rto }).min(by: { $0.value.t < $1.value.t })
        else { return true }
        if e.tries >= Self.askLimit {
            endRegion(r, .failure(FizzBackupError.timeout(offset: k * FM1Bulk.chunk)))
            return false
        }
        regionAsk(r, k, again: true)
        return true
    }

    func regionFrame(_ r: RegionRead, _ p: [UInt8]) {
        guard let b = FM1Bulk.parseBulk(p) else { return }
        if b.tag == "FZER" {
            endRegion(r, .failure(FizzBackupError.fzer(b.data.count >= 4 ? Int(FM1Bulk.rd32(b.data, 0)) : 0)))
            return
        }
        guard b.tag == "FZBD", b.data.count >= 4 else { return }
        r.heard = true
        defer { if !r.finished { arm(r) } }
        let off = Int(FM1Bulk.rd32(b.data, 0)), c = FM1Bulk.chunk
        guard off % c == 0, let e = r.fly[off / c], b.data.count == 4 + r.want(off / c) else { return }  // (late)
        r.piece.replaceSubrange((off - r.from)..<(off - r.from + b.data.count - 4), with: b.data[4...])
        if !e.again { r.rtts.append(clock.now - e.t) }
        // (capped at 0.8 s: one slow reply once made it 3 s, and two lost chunks then outlasted the wait)
        if r.rtts.count >= 8 {
            r.rto = max(Self.rtoMin, min(Self.rtoMax, 4 * (r.rtts.suffix(64).max() ?? 0)))
        }
        r.fly[off / c] = nil
        r.done += 1
        r.progress(Double(r.done) / Double(r.n), off)
        if r.done == r.n {
            endRegion(r, .success(RegionRead.Result(piece: r.piece, frames: r.frames, resent: r.resent,
                                                    rtt: .of(r.rtts))))
            return
        }
        regionFill(r)
    }

    // MARK: MY PRESETS

    /// MY PRESETS read as the Manager does (readUnit): LIST (unless given), DUMP slot 0 for all of them, then a DUMP
    /// for each LIST slot whose frame did not come. progress(presets come, total) during the DUMP stream.
    public func presets(rows: [FizzListEntry]? = nil,
                        progress: (@Sendable (Int, Int) -> Void)? = nil) async throws -> FizzPresetsRead {
        let all: [FizzListEntry]
        if let rows { all = rows } else { all = try await list(factory: true, pool: true) }
        let mine = all.filter { $0.source == FizzSource.mine.name }.sorted { $0.slot < $1.slot }
        var bySlot: [Int: [UInt8]] = [:]
        let t0 = clock.now
        for f in try await dumpAll(source: .mine, total: mine.count, progress: progress) {
            if let d = FizzSysEx.parseDump(f), bySlot[d.slot] == nil { bySlot[d.slot] = f }
        }
        logger?("DUMP: \(bySlot.count) of \(mine.count) presets in \(String(format: "%.0f", clock.now - t0)) ms")
        let missing = mine.filter { bySlot[$0.slot] == nil }
        if !missing.isEmpty {                       // (a reply lost on the way: the missing ones one by one)
            for e in missing {
                do {
                    if let f = try await dump(source: .mine, slot: e.slot) { bySlot[e.slot] = f }
                } catch FizzLinkError.timeout {}
            }
            logger?("DUMP: \(mine.filter { bySlot[$0.slot] != nil }.count) of \(mine.count) after asking for the missing ones")
        }
        let got = mine.compactMap { e in bySlot[e.slot].map { FizzPreset(entry: e, frame: $0) } }
        if got.count < mine.count { throw FizzBackupError.dump(got: got.count, of: mine.count) }
        return FizzPresetsRead(rows: all, presets: got)
    }
}
