// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// BACK UP and PRESETS ONLY (manager.js backup / readUnit): the store read whole (FizzLink.backup, CRC-checked), MY
// PRESETS dumped (LIST, DUMP), the .fizzbak made by the Manager's own JS (FizzData makeFizzbak) and kept in
// BackupStore. Read-only on the unit. While it runs the screen stays awake; CANCEL keeps nothing.

import Foundation
import FizzData
import FizzKit
import Observation
#if canImport(UIKit)
import UIKit
#endif

/// What the progress screen shows.
public struct BackupRun: Sendable, Equatable {
    public var kind: FizzBackupKind
    /// 0..1: the store 0..0.9, MY PRESETS 0.9..1 (PRESETS ONLY: 0..1)
    public var fraction: Double
    /// Each sector of the region: all of it read
    public var lit: [Bool]
    /// The sector being read
    public var now: Int?
    /// MY PRESETS' sectors come first (cream), the projects' after (green)
    public var libSectors: Int
    public var started: Date

    public init(kind: FizzBackupKind, fraction: Double, lit: [Bool], now: Int?, libSectors: Int, started: Date) {
        self.kind = kind; self.fraction = fraction; self.lit = lit; self.now = now; self.libSectors = libSectors
        self.started = started
    }

    /// A run at `fraction` of the store read, as the screen draws it (the snapshots).
    public static func still(kind: FizzBackupKind = .full, fraction: Double, layout: FizzLayout = .device,
                             elapsed: TimeInterval) -> BackupRun {
        let n = layout.sectors, k = min(n, Int(Double(n) * min(1, fraction / 0.9)))
        return BackupRun(kind: kind, fraction: fraction, lit: (0..<n).map { $0 < k }, now: k < n ? k : nil,
                         libSectors: layout.libSectors, started: Date().addingTimeInterval(-elapsed))
    }
}

/// A backup that did not finish: a short state in a colour.
public enum BackupFailure: Sendable, Equatable {
    case updateFizz             // no bulk channel: an older FIZZ
    case changed                // the store changed while read, twice
    case stopped                // the FM-1 stopped answering
    case unplugged
    case refused(Int)           // FZER
    case presets(got: Int, of: Int)
    case notSaved
    case notABackup             // a file opened that does not read

    public var text: String {
        switch self {
        case .updateFizz: "UPDATE FIZZ"
        case .changed: "FM-1 CHANGED"
        case .stopped: "FM-1 STOPPED ANSWERING"
        case .unplugged: "UNPLUGGED"
        case .refused(let c): c == 10 ? "STOP THE FM-1" : "FZER \(c)"
        case .presets(let g, let o): "\(g) OF \(o) PRESETS"
        case .notSaved: "NOT SAVED"
        case .notABackup: "NOT A BACKUP"
        }
    }
    public var color: RGB {
        switch self {
        case .updateFizz, .changed, .refused(10): FZ.orange
        default: FZ.red
        }
    }
    /// Worth trying again at once
    public var again: Bool {
        switch self {
        case .changed, .stopped, .presets, .refused(10): true
        default: false
        }
    }

    static func of(_ e: Error) -> BackupFailure? {
        switch e {
        case let b as FizzBackupError:
            switch b {
            case .noBulk: return .updateFizz
            case .changed: return .changed
            case .cancelled: return nil
            case .fzer(let c): return .refused(c)
            case .timeout: return .stopped
            case .dump(let g, let o): return .presets(got: g, of: o)
            }
        case let l as FizzLinkError: return l == .timeout ? .stopped : .unplugged
        case is CancellationError: return nil
        default: return .notSaved
        }
    }
}

/// The read's progress, written from the link's actor, read by the screen every 60 ms.
final class BackupProgress: @unchecked Sendable {
    private let lock = NSLock()
    private var fraction = 0.0, last = -1, presetsDone = 0, presetsAll = 0, storeDone = false
    private var chunks: [Bool]
    let kind: FizzBackupKind, storeShare: Double, layout: FizzLayout

    init(kind: FizzBackupKind, size: Int, layout: FizzLayout, withPresets: Bool) {
        self.kind = kind
        self.layout = layout
        storeShare = kind == .presets ? 0 : withPresets ? 0.9 : 1
        chunks = [Bool](repeating: false, count: (size + FM1Bulk.chunk - 1) / FM1Bulk.chunk)
    }

    func store(_ f: Double, _ off: Int) {
        lock.withLock {
            fraction = max(fraction, f * storeShare)
            let k = off / FM1Bulk.chunk
            if chunks.indices.contains(k) { chunks[k] = true }
            last = off
        }
    }

    func presets(_ n: Int, _ t: Int) {
        lock.withLock {
            storeDone = true
            presetsDone = n; presetsAll = t
            fraction = max(fraction, storeShare + (1 - storeShare) * Double(n) / Double(max(1, t)))
        }
    }

    func run(started: Date) -> BackupRun {
        lock.withLock {
            let n = layout.sectors, per = FizzLayout.sector / FM1Bulk.chunk
            var lit = [Bool](repeating: false, count: n)
            var now: Int?
            if kind == .presets {
                let k = presetsAll > 0 ? layout.libSectors * presetsDone / presetsAll : 0
                for s in 0..<min(k, n) { lit[s] = true }
                now = k < layout.libSectors ? k : nil
            } else if storeDone || fraction >= storeShare {
                lit = [Bool](repeating: true, count: n)
            } else {
                for s in 0..<n {
                    let a = s * per, b = min(chunks.count, a + per)
                    lit[s] = a < b && chunks[a..<b].allSatisfy { $0 }
                }
                if last >= 0 {
                    var s = last / FizzLayout.sector
                    if s < n, lit[s] { s += 1 }
                    now = s < n && !lit[s] ? s : nil
                }
            }
            return BackupRun(kind: kind, fraction: fraction, lit: lit, now: now, libSectors: layout.libSectors, started: started)
        }
    }
}

/// The screen kept on and a little background time while a backup runs (iOS).
@MainActor final class KeepAwake {
    #if canImport(UIKit)
    private var task: UIBackgroundTaskIdentifier = .invalid
    /// (a test bundle has no UIApplication)
    private var inApp: Bool { Bundle.main.bundleURL.pathExtension == "app" }
    #endif

    func begin(onExpire: @escaping @MainActor () -> Void) {
        #if canImport(UIKit)
        guard inApp else { return }
        UIApplication.shared.isIdleTimerDisabled = true
        task = UIApplication.shared.beginBackgroundTask(withName: "FIZZ backup") {
            MainActor.assumeIsolated { onExpire() }
        }
        #endif
    }

    func end() {
        #if canImport(UIKit)
        guard inApp else { return }
        UIApplication.shared.isIdleTimerDisabled = false
        if task != .invalid { UIApplication.shared.endBackgroundTask(task); task = .invalid }
        #endif
    }
}

@MainActor @Observable
public final class BackupModel {
    public let store: BackupStore
    /// The progress screen (nil: none)
    public private(set) var run: BackupRun?
    public private(set) var busy = false
    public private(set) var failure: BackupFailure?
    /// The last store read's speed (KB/s, ms a read, resent, depth), shown until the next backup
    public private(set) var speed: FizzBackupStats?
    /// The row just added (it flashes)
    public private(set) var added: String?
    /// The last one started (AGAIN)
    public private(set) var lastKind: FizzBackupKind = .full

    @ObservationIgnored weak var unit: UnitModel?
    @ObservationIgnored private var task: Task<BackupEntry?, Never>?
    @ObservationIgnored private var poll: Task<Void, Never>?
    @ObservationIgnored private var cancelled = false
    @ObservationIgnored private let awake = KeepAwake()
    @ObservationIgnored private var unflash: Task<Void, Never>?

    public init(store: BackupStore) { self.store = store }

    public func canBackUp(_ kind: FizzBackupKind) -> Bool {
        guard !busy, let u = unit, u.link != nil, let info = u.unit else { return false }
        return kind == .full ? u.storeInfo != nil : info.version != nil
    }

    /// BACK UP (.full) or PRESETS ONLY (.presets).
    public func start(_ kind: FizzBackupKind) { _ = begin(kind) }

    /// manager.js safetyCopy: a full backup before a RESTORE (the progress screen as BACK UP's), kept as "FIZZ
    /// before restore ..". nil: it could not be made (or was cancelled): then nothing is to be restored.
    public func safetyBackup() async -> BackupEntry? {
        guard let t = begin(.full, safety: true) else { return nil }
        return await t.value
    }

    private func begin(_ kind: FizzBackupKind, safety: Bool = false) -> Task<BackupEntry?, Never>? {
        guard canBackUp(kind), let u = unit, let link = u.link, let info = u.unit else { return nil }
        let size = Int(u.storeInfo?.size ?? 0)
        let layout = u.layout ?? FizzLayout(name: "", size: size, libSectors: 0,
                                            prjSectors: max(1, (size + FizzLayout.sector - 1) / FizzLayout.sector))
        let progress = BackupProgress(kind: kind, size: size, layout: layout, withPresets: info.version != nil)
        let started = Date()
        failure = nil
        lastKind = kind
        speed = kind == .full ? nil : speed
        busy = true
        cancelled = false
        run = progress.run(started: started)
        awake.begin { [weak self] in self?.cancel() }
        poll = Task { [weak self] in
            while !Task.isCancelled {
                try? await Task.sleep(for: .milliseconds(60))
                guard let self, self.run != nil, !Task.isCancelled else { return }
                self.run = progress.run(started: started)
            }
        }
        let t = Task { [weak self] () -> BackupEntry? in
            let r: Result<Done, Error>
            do { r = .success(try await Self.perform(kind, link, info, progress, self?.store, safety: safety)) }
            catch { r = .failure(error) }
            self?.finish(r)
            return try? r.get().entry
        }
        task = t
        return t
    }

    /// CANCEL: the screen goes at once; the read stops and nothing is kept.
    public func cancel() {
        guard busy else { return }
        cancelled = true
        task?.cancel()
        poll?.cancel()
        awake.end()
        run = nil
    }

    public func dismissFailure() { failure = nil }

    struct Done { let entry: BackupEntry; let stats: FizzBackupStats?; let rows: [FizzListEntry]? }

    private static func perform(_ kind: FizzBackupKind, _ link: FizzLink, _ info: UnitInfo, _ progress: BackupProgress,
                                _ store: BackupStore?, safety: Bool = false) async throws -> Done {
        func check() throws { if Task.isCancelled { throw FizzBackupError.cancelled } }
        let date = Date()
        var read: FizzStoreRead?
        if kind == .full {
            read = try await link.backup(depth: 2) { f, off in progress.store(f, off) }
        }
        try check()
        var frames: [[UInt8]]?, rows: [FizzListEntry]?
        if info.version != nil {
            let all = try await link.list(factory: true, pool: true)
            try check()
            progress.presets(0, 1)
            let p = try await link.presets(rows: all) { n, t in progress.presets(n, t) }
            frames = p.presets.map(\.frame)
            rows = p.rows
        }
        try check()
        let engine = try FizzDataShared.engine()
        let input = FizzbakInput(kind: kind, image: read?.image, region: read.map { FizzStoreRegion(lo: $0.lo, size: $0.size) },
                                 crc: read?.crc, presets: frames, fizz: info.version ?? "", unitIdentity: info.idText,
                                 unitPort: info.portName, webTags: [:], stats: read.map { FizzTransferStats($0.stats) },
                                 date: date)
        let bytes = try await engine.makeFizzbak(input)
        var name = try await engine.backupFileName(date: date, kind: kind)
        if safety { name = name.replacingOccurrences(of: "FIZZ backup", with: "FIZZ before restore") }
        try check()
        guard let store else { throw FizzBackupError.cancelled }
        let entry = try await store.add(bytes, name: name)
        return Done(entry: entry, stats: read?.stats, rows: rows)
    }

    private func finish(_ r: Result<Done, Error>) {
        poll?.cancel()
        awake.end()
        task = nil
        busy = false
        run = nil
        switch r {
        case .success(let d):
            if let s = d.stats { speed = s }
            unit?.backedUp(rows: d.rows, tracks: d.entry.isFull ? d.entry.tracks : nil)
            unit?.say("kept \(d.entry.name) (\(d.entry.size) B)")
            flash(d.entry.name)
        case .failure(let e):
            unit?.say("backup: \(e)")
            if !cancelled { failure = BackupFailure.of(e) }
        }
        cancelled = false
    }

    private func flash(_ name: String) {
        added = name
        unflash?.cancel()
        unflash = Task { [weak self] in
            try? await Task.sleep(for: .seconds(2.4))
            if !Task.isCancelled { self?.added = nil }
        }
    }

    // MARK: the list

    /// A file from outside (the picker, Open in FIZZ, AirDrop): copied in, listed, flashed.
    @discardableResult public func open(_ url: URL) async -> Bool {
        do {
            let e = try await store.importFile(at: url)
            unit?.say("opened \(url.lastPathComponent) (\(e.kind), \(e.presets) presets)")
            flash(e.name)
            return true
        } catch {
            unit?.say("open \(url.lastPathComponent): \(error)")
            failure = .notABackup
            return false
        }
    }

    public func delete(_ e: BackupEntry) { store.delete(e) }
    public func shared(_ e: BackupEntry) { store.markSaved(e) }
}
