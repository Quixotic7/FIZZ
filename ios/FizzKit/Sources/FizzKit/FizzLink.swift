// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// The link to an FM-1 running FIZZ (site/manager/fizzlink.js): one operation outstanding at a time over both of
// FIZZ's SysEx channels (its own frames, and the bulk one in the update's framing); frames sent GAP_MS apart (the
// unit keeps one inbound SysEx frame and drops a second arriving within ~2 ms); reads sent again when nothing of
// them was heard.

import Foundation

// MARK: - Clock

/// Milliseconds. Injectable so tests run on their own time.
public protocol FizzClock: Sendable {
    var now: Double { get }
    func sleep(ms: Double) async throws
}

public struct SystemClock: FizzClock {
    private let origin = ContinuousClock.now
    public init() {}
    public var now: Double {
        let d = ContinuousClock.now - origin
        return Double(d.components.seconds) * 1000 + Double(d.components.attoseconds) / 1e15
    }
    public func sleep(ms: Double) async throws {
        try await Task.sleep(for: .microseconds(max(0, ms * 1000)))
    }
}

// MARK: - Errors and results

public enum FizzLinkError: Error, Equatable {
    case lost        // the FM-1 was disconnected
    case closed      // the link was closed
    case timeout     // the FM-1 did not answer
}

public enum FizzFindError: Error, Equatable {
    case notFound
    case updateMode(String)
    case notFizz(String)
}

/// A Phase 1 command's answer: the reply wanted (or its ACK), or FIZZ's ERROR for it (old: BAD COMMAND, an older FIZZ).
public enum FizzAnswer: Sendable, Equatable {
    case reply(FizzReply)
    case refused(status: Int?, statusName: String?, old: Bool)
}

/// A unit that answered the handshake.
public struct FizzProbe: Sendable {
    public let candidate: MIDIPortCandidate
    public let identity: FM1Identity
    public let link: FizzLink
}

// MARK: - The link

public actor FizzLink {
    public static let gapMs = 3.0
    static let wakeMs = 1000.0

    let transport: any MIDITransport
    let clock: any FizzClock
    /// The emulated FIZZ (MIDITransport.isEmulated): its frames sent with no gap.
    public nonisolated let isEmulated: Bool
    /// GAP_MS between frames; none to the emulated FIZZ (it takes every frame at once).
    let gap: Double
    public private(set) var idText: String
    public private(set) var portName: String
    public private(set) var isClosed = false
    private var lastRx = -Double.infinity
    private var nextAt = -Double.infinity
    private var lastSent = -Double.infinity
    var cur: (any PendingOp)?
    var opSeq = 0
    private var busy = false
    private var waiters: [CheckedContinuation<Void, Never>] = []
    private var onLost: (@Sendable () -> Void)?
    var logger: (@Sendable (String) -> Void)?

    public init(transport: any MIDITransport, clock: any FizzClock = SystemClock(), idText: String = "",
                portName: String = "") {
        self.transport = transport
        self.clock = clock
        self.isEmulated = transport.isEmulated
        self.gap = transport.isEmulated ? 0 : Self.gapMs
        self.idText = idText
        self.portName = portName.isEmpty ? transport.name : portName
        Task { await self.read() }
    }

    public func setOnLost(_ f: (@Sendable () -> Void)?) { onLost = f }
    public func setLogger(_ f: (@Sendable (String) -> Void)?) { logger = f }

    public func close() {
        guard !isClosed else { return }
        isClosed = true
        if let c = cur { cur = nil; c.fail(FizzLinkError.closed) }
        transport.close()
    }

    // the reader: every frame to the operation waiting; the stream's end is the port lost
    private func read() async {
        for await p in transport.messages {
            lastRx = clock.now
            dispatch(p)
        }
        lost()
    }

    func lost() {
        guard !isClosed else { return }
        isClosed = true
        if let c = cur { cur = nil; c.fail(FizzLinkError.lost) }
        onLost?()
    }

    private func dispatch(_ p: [UInt8]) {
        guard let c = cur else { return }
        if let r = c as? RegionRead { regionFrame(r, p); return }
        if let r = c as? RestoreOp { restoreFrame(r, p); return }
        switch c.frame(p) {
        case .ignore: break
        case .more: c.heard = true; arm(c)
        case .finished: c.timer?.cancel(); cur = nil
        }
    }

    /// Each frame at least GAP_MS after the one before. `region` (a backup's read): once the wait is over, not sent
    /// (false) when that read has ended (cancelled, failed or done).
    @discardableResult
    func send(_ bytes: [UInt8], region: Int? = nil) async throws -> Bool {
        if isClosed { throw FizzLinkError.lost }
        let t = clock.now, at = max(t, nextAt)
        nextAt = at + gap
        if at > t { try? await clock.sleep(ms: at - t) }
        // (a sleep that ran late, or another frame sent while this one waited: the gap from the last real send)
        while clock.now < lastSent + gap { try? await clock.sleep(ms: lastSent + gap - clock.now) }
        if isClosed { throw FizzLinkError.lost }
        if let region, !regionAlive(region) { return false }
        do { try transport.send(bytes) } catch {
            lost()
            throw FizzLinkError.lost
        }
        lastSent = clock.now
        nextAt = max(nextAt, lastSent + gap)
        return true
    }

    // MARK: operations

    enum Step<R> {
        case notMine        // not this operation's frame
        case more           // its, keep waiting (the wait starts again)
        case done(R)
    }

    /// One operation: start() gives the frame to send, onFrame reads the replies. On a quiet `timeout` (ms): sent again
    /// up to `retry` times while nothing of it was heard, then partial() or FizzLinkError.timeout.
    func op<R: Sendable>(timeout: Double, retry: Int = 0, partial: (() -> R)? = nil,
                         start: @escaping () throws -> [UInt8],
                         onFrame: @escaping ([UInt8]) throws -> Step<R>) async throws -> R {
        await acquire()
        defer { release() }
        return try await opHeld(timeout: timeout, retry: retry, partial: partial, start: start, onFrame: onFrame)
    }

    /// op() for a caller that already holds the queue (the backup: one operation of several reads).
    func opHeld<R: Sendable>(timeout: Double, retry: Int = 0, partial: (() -> R)? = nil,
                             start: @escaping () throws -> [UInt8],
                             onFrame: @escaping ([UInt8]) throws -> Step<R>) async throws -> R {
        if isClosed { throw FizzLinkError.lost }
        opSeq += 1
        let id = opSeq
        return try await withCheckedThrowingContinuation { (k: CheckedContinuation<R, Error>) in
            let c = Op(id: id, timeout: timeout, retry: retry, k: k, start: start, onFrame: onFrame, partial: partial)
            cur = c
            arm(c)
            Task { await self.kick(id) }
        }
    }

    private func kick(_ id: Int) async {
        guard let c = cur, c.id == id else { return }
        do { try await send(try c.makeFrame()) } catch {
            if let c = cur, c.id == id { cur = nil; c.timer?.cancel(); c.fail(error) }
        }
    }

    func arm(_ c: any PendingOp) {
        c.timer?.cancel()
        c.gen += 1
        let id = c.id, gen = c.gen, ms = c.timeout, clock = clock
        c.timer = Task {
            do { try await clock.sleep(ms: ms) } catch { return }
            self.expired(id, gen)
        }
    }

    private func expired(_ id: Int, _ gen: Int) {
        guard let c = cur, c.id == id, c.gen == gen else { return }
        if c.tries < c.retry && !c.heard {
            c.tries += 1
            logger?("no answer in \(Int(c.timeout)) ms: sent again (\(c.tries))")
            arm(c)
            Task { await self.kick(id) }
            return
        }
        cur = nil
        c.expire()
    }

    func acquire() async {
        if !busy { busy = true; return }
        await withCheckedContinuation { waiters.append($0) }
    }

    func release() {
        if waiters.isEmpty { busy = false } else { waiters.removeFirst().resume() }
    }

    // MARK: the semantic channel

    /// FIZZ's version ("0.12.0-beta.9"), nil when FIZZ does not answer (no data blob, or safe mode).
    public func identity() async throws -> String? {
        try await op(timeout: 700, retry: 3, partial: { nil }, start: { FizzSysEx.identity() }) { p in
            if case .identity(let r)? = FizzSysEx.parseReply(p) { return .done(Self.stripFizz(r.version)) }
            return .notMine
        }
    }

    static func stripFizz(_ v: String) -> String {
        guard v.hasPrefix("FIZZ") else { return v }
        return String(String.UnicodeScalarView(v.unicodeScalars.dropFirst(4).drop { isJSSpace($0.value) }))
    }

    /// MY PRESETS (and the factory's, the pool's), every LIST part.
    public func list(factory: Bool = false, pool: Bool = false) async throws -> [FizzListEntry] {
        let rows = Box<[FizzListEntry]>([])
        return try await op(timeout: 3000, start: {
            rows.v.removeAll()
            return FizzSysEx.listRequest(factory: factory, pool: pool)
        }) { p in
            guard let l = FizzSysEx.parseList(p) else { return .notMine }
            rows.v += l.entries
            return l.last ? .done(rows.v) : .more
        }
    }

    /// One preset's DUMP frame; nil when FIZZ answers an error for it.
    public func dump(source: FizzSource, slot: Int) async throws -> [UInt8]? {
        try await op(timeout: 1000, retry: 3, start: { try FizzSysEx.dumpRequest(source: source, slot: slot) }) { p in
            if let d = FizzSysEx.parseDump(p), d.slot == slot || (source == .factory && d.slot == 0) {
                return .done(d.frame)
            }
            if case .error(let e)? = FizzSysEx.parseReply(p), e.command == Int(FizzSysEx.CMD.dump) { return .done(nil) }
            return .notMine
        }
    }

    /// Every preset of a source (DUMP slot 0), as many as LIST counted; what came when the unit went quiet.
    public func dumpAll(source: FizzSource, total: Int,
                        progress: (@Sendable (Int, Int) -> Void)? = nil) async throws -> [[UInt8]] {
        guard total > 0 else { return [] }
        let out = Box<[[UInt8]]>([])
        return try await op(timeout: 4000, partial: { out.v }, start: {
            try FizzSysEx.dumpRequest(source: source, slot: 0)
        }) { p in
            guard let d = FizzSysEx.parseDump(p), source == .factory || d.source == source.name else { return .notMine }
            out.v.append(d.frame)
            progress?(out.v.count, total)
            return out.v.count >= total ? .done(out.v) : .more
        }
    }

    /// A quiet link (nothing heard for 1 s) woken by a STATUS before a command that changes something.
    public func wake() async {
        if clock.now - lastRx > Self.wakeMs { _ = await status() }
    }

    /// A Phase 1 command: the reply `want` picks (by default its ACK), or FIZZ's ERROR for that command.
    func ask1(_ f: [UInt8], cmd: UInt8, want: (@Sendable (FizzReply) -> Bool)? = nil, timeout: Double = 15000,
              retry: Int = 0) async throws -> FizzAnswer {
        if retry == 0 { await wake() }
        let c = Int(cmd)
        return try await op(timeout: timeout, retry: retry, start: { f }) { p in
            guard let r = FizzSysEx.parseReply(p) else { return .notMine }
            if let want, want(r) { return .done(.reply(r)) }
            if case .ack(let a?) = r, a.command == c { return .done(.reply(r)) }
            if case .error(let e) = r, e.command == c {
                return .done(.refused(status: e.status, statusName: e.statusName, old: e.status == 7))
            }
            return .notMine
        }
    }

    /// STATUS, nil when FIZZ does not answer it (or an older FIZZ).
    public func status() async -> FizzStatus? {
        let r = try? await ask1(FizzSysEx.statusRequest(), cmd: FizzSysEx.CMD.status, want: {
            if case .status(_?) = $0 { true } else { false }
        }, timeout: 800, retry: 3)
        if case .reply(.status(let s?))? = r { return s }
        return nil
    }

    /// Where a preset is used: USERS, or FIZZ's refusal.
    public func users(source: FizzSource, slot: Int) async throws -> FizzAnswer {
        try await ask1(try FizzSysEx.usersRequest(source: source, slot: slot), cmd: FizzSysEx.CMD.users, want: {
            if case .users(_?) = $0 { true } else { false }
        }, timeout: 5000, retry: 1)
    }

    /// The projects, nil when FIZZ does not answer.
    public func projects() async -> FizzProjects? {
        let r = try? await ask1(FizzSysEx.projectsRequest(), cmd: FizzSysEx.CMD.projects, want: {
            if case .projects(_?) = $0 { true } else { false }
        }, timeout: 800, retry: 3)
        if case .reply(.projects(let p?))? = r { return p }
        return nil
    }

    // MARK: the bulk channel

    func bulk(_ tag: String, _ words: [UInt32], want: String, timeout: Double, retry: Int = 0, held: Bool = false)
        async throws -> FizzBulkReply? {
        let start = { FM1Bulk.bulkFrame(tag: tag, words: words) }
        let onFrame: ([UInt8]) throws -> Step<FizzBulkReply?> = { p in
            guard let r = FM1Bulk.parseBulk(p), r.tag == want || r.tag == "FZER" else { return .notMine }
            return .done(r)
        }
        if held { return try await opHeld(timeout: timeout, retry: retry, partial: { nil }, start: start, onFrame: onFrame) }
        return try await op(timeout: timeout, retry: retry, partial: { nil }, start: start, onFrame: onFrame)
    }

    /// FZBI: the store's region and CRC-32; nil for a FIZZ without the bulk channel.
    public func storeInfo() async throws -> FizzStoreInfo? { try await storeInfo(held: false) }

    func storeInfo(held: Bool) async throws -> FizzStoreInfo? {
        guard let r = try await bulk("FZBI", [], want: "FZBI", timeout: 2500, retry: 3, held: held), r.tag == "FZBI",
              r.data.count >= 12 else { return nil }
        return FizzStoreInfo(lo: FM1Bulk.rd32(r.data, 0), size: FM1Bulk.rd32(r.data, 4), crc: FM1Bulk.rd32(r.data, 8))
    }

    // The writes: Writes.swift; the backup's read: Backup.swift; the restore: Restore.swift.

    // MARK: finding the unit

    /// The update handshake: the unit's identity, nil when nothing answered (2 tries, 1 s each).
    public func handshake() async throws -> FM1Identity? {
        try await op(timeout: 1000, retry: 1, partial: { nil }, start: { FM1Bulk.handshakeQuery }) { p in
            if let id = FM1Bulk.parseIdentity(p) { return .done(id) }
            return .notMine
        }
    }

    func setIdentity(_ text: String) { idText = text }

    /// Every unit that answered the handshake, one per device; their links left open.
    public static func probe(scanner: some MIDIPortScanning, clock: any FizzClock = SystemClock()) async -> [FizzProbe] {
        var out: [FizzProbe] = []
        var devices = Set<UInt32>()
        for c in scanner.candidates() {
            if c.device != 0 && devices.contains(c.device) { continue }
            guard let t = try? scanner.open(c) else { continue }
            let link = FizzLink(transport: t, clock: clock, portName: c.name)
            var found = (try? await link.handshake()) ?? nil
            if found == nil, let v = (try? await link.identity()) ?? nil {
                // over Bluetooth the update handshake can go unanswered (fm1-base's updater listens on USB), while
                // FIZZ's own IDENTITY is answered: a FIZZ all the same
                found = FM1Identity(text: "\(FM1Bulk.fizzPrefix) FIZZ \(v)", model: "FM-1", version: 0, open: false)
            }
            if let id = found {
                await link.setIdentity(id.text)
                out.append(FizzProbe(candidate: c, identity: id, link: link))
                if c.device != 0 { devices.insert(c.device) }
            } else {
                await link.close()
            }
        }
        return out
    }

    /// The first unit running FIZZ; the others closed.
    public static func find(scanner: some MIDIPortScanning, clock: any FizzClock = SystemClock()) async throws
        -> (link: FizzLink, idText: String, portName: String) {
        let units = await probe(scanner: scanner, clock: clock)
        let fizz = units.first { $0.identity.isFizz }
        for u in units where u.link !== fizz?.link { await u.link.close() }
        if let f = fizz { return (f.link, f.identity.text, f.candidate.name) }
        guard let u = units.first else { throw FizzFindError.notFound }
        if u.identity.isOTA { throw FizzFindError.updateMode(u.identity.text) }
        throw FizzFindError.notFizz(u.identity.text)
    }
}

// MARK: - The operation waiting

enum FrameOutcome { case ignore, more, finished }

protocol PendingOp: AnyObject {
    var id: Int { get }
    var timeout: Double { get }
    var retry: Int { get }
    var tries: Int { get set }
    var heard: Bool { get set }
    var gen: Int { get set }
    var timer: Task<Void, Never>? { get set }
    func makeFrame() throws -> [UInt8]
    func frame(_ p: [UInt8]) -> FrameOutcome
    func expire()
    func fail(_ e: Error)
}

final class Op<R: Sendable>: PendingOp {
    let id: Int, timeout: Double, retry: Int
    var tries = 0, heard = false, gen = 0
    var timer: Task<Void, Never>?
    private var k: CheckedContinuation<R, Error>?
    private let start: () throws -> [UInt8]
    private let onFrame: ([UInt8]) throws -> FizzLink.Step<R>
    private let partial: (() -> R)?

    init(id: Int, timeout: Double, retry: Int, k: CheckedContinuation<R, Error>, start: @escaping () throws -> [UInt8],
         onFrame: @escaping ([UInt8]) throws -> FizzLink.Step<R>, partial: (() -> R)?) {
        self.id = id; self.timeout = timeout; self.retry = retry; self.k = k
        self.start = start; self.onFrame = onFrame; self.partial = partial
    }

    func makeFrame() throws -> [UInt8] { try start() }

    func frame(_ p: [UInt8]) -> FrameOutcome {
        do {
            switch try onFrame(p) {
            case .notMine: return .ignore
            case .more: return .more
            case .done(let r): resume(.success(r)); return .finished
            }
        } catch {
            resume(.failure(error))
            return .finished
        }
    }

    func expire() {
        timer?.cancel()
        if let partial { resume(.success(partial())) } else { resume(.failure(FizzLinkError.timeout)) }
    }

    func fail(_ e: Error) { timer?.cancel(); resume(.failure(e)) }

    private func resume(_ r: Result<R, Error>) {
        guard let k else { return }
        self.k = nil
        k.resume(with: r)
    }
}

final class Box<T> {
    var v: T
    init(_ v: T) { self.v = v }
}
