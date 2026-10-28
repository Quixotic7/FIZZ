// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// BACK UP and PRESETS ONLY against the demo unit (its store a real device-layout image), cancelling, the backups kept
// (BackupStore in a temporary folder: save, order, delete, the index made again, files opened), the "old" rule.

import Foundation
import Testing
import FizzData
import FizzKit
@testable import FizzUI

@MainActor
private func waitFor(seconds: Double = 30, _ ok: () -> Bool) async -> Bool {
    let end = Date().addingTimeInterval(seconds)
    while Date() < end {
        if ok() { return true }
        try? await Task.sleep(for: .milliseconds(20))
    }
    return ok()
}

private func tempDir() -> URL {
    let u = FileManager.default.temporaryDirectory.appendingPathComponent("fizz-test-\(UUID().uuidString)", isDirectory: true)
    try? FileManager.default.createDirectory(at: u, withIntermediateDirectories: true)
    return u
}

private func files(_ dir: URL, _ ext: String = "fizzbak") -> [String] {
    ((try? FileManager.default.contentsOfDirectory(atPath: dir.path)) ?? []).filter { $0.hasSuffix(".\(ext)") }.sorted()
}

@MainActor
private func connected(_ dir: URL) async -> UnitModel {
    let m = UnitModel(source: DemoLinkSource(.fizz, bulkMs: 0...0.3), backups: dir)
    m.start()
    _ = await waitFor { m.dumpsLoaded && m.layout != nil }
    return m
}

@Suite("Backup flow", .serialized) @MainActor
struct BackupFlowTests {
    @Test func fullBackupOfTheDemoUnit() async throws {
        let dir = tempDir()
        let m = await connected(dir)
        #expect(m.layout == .device)
        let info = try #require(m.storeInfo)
        var maxLit = 0, sawNow = false
        m.backups.start(.full)
        #expect(m.backups.busy && m.backups.run != nil)
        #expect(await waitFor {
            if let r = m.backups.run { maxLit = max(maxLit, r.lit.filter { $0 }.count); sawNow = sawNow || r.now != nil }
            return !m.backups.busy
        })
        #expect(m.backups.failure == nil)
        #expect(m.backups.run == nil)
        #expect(sawNow && maxLit > 0)
        let e = try #require(m.backups.store.entries.first)
        #expect(m.backups.store.entries.count == 1)
        #expect(e.kind == "full" && e.presets == 143 && e.layout == "device" && e.tracks == 6 && !e.saved && !e.file)
        #expect(e.projects == ["NOIR", "SUNDAY", "FIZZ TOUR", "SKETCH", "ACID 3", "WINTER LP"])
        #expect(e.fizz == "0.12.0-beta.11")
        #expect(e.name.hasPrefix("FIZZ backup ") && e.name.hasSuffix(".fizzbak"))
        #expect(m.backups.added == e.name)
        #expect(m.backups.speed?.bytes == 299_008)
        #expect(m.storeTracks == 6)

        // the file as the web Manager reads it
        let bytes = [UInt8](try Data(contentsOf: m.backups.store.url(of: e)))
        #expect(bytes.count == e.size)
        let s = try await FizzDataShared.engine().readBackup(bytes, fileName: e.name)
        #expect(s.kind == .full && s.presetCount == 143 && s.layout == "device")
        #expect(s.projects?.map(\.name) == e.projects)
        #expect(s.storeCRC == String(format: "%08x", info.crc))
        #expect(s.image.map { FM1Bulk.crc32($0) } == info.crc)
        #expect(s.region == FizzStoreRegion(lo: 0x97000, size: 299_008))
        #expect(s.unitIdentity == "FM-1_46001100" && s.unitPort == "FIZZ FM-1")
        #expect(s.presets.first?.name == "SUB DROP")
        #expect(s.transfer?["bytes"]?.int == 299_008)
        #expect(e.isOld(unitLayout: m.layout?.name, unitTracks: m.storeTracks) == false)
        m.stop()
    }

    @Test func presetsOnly() async throws {
        let dir = tempDir()
        let m = await connected(dir)
        m.backups.start(.presets)
        #expect(await waitFor { !m.backups.busy })
        #expect(m.backups.failure == nil)
        let e = try #require(m.backups.store.entries.first)
        #expect(e.kind == "presets" && e.presets == 143 && e.projects == nil && e.layout == nil)
        #expect(e.name.hasPrefix("FIZZ presets "))
        let s = try await FizzDataShared.engine().readBackup([UInt8](try Data(contentsOf: m.backups.store.url(of: e))), fileName: e.name)
        #expect(s.kind == .presets && s.image == nil && s.presetCount == 143)
        #expect(m.backups.speed == nil)
        m.stop()
    }

    @Test func cancellingKeepsNothing() async throws {
        let dir = tempDir()
        let m = await connected(dir)
        m.backups.start(.full)
        #expect(await waitFor { (m.backups.run?.fraction ?? 0) > 0.1 })
        m.backups.cancel()
        #expect(m.backups.run == nil)
        #expect(await waitFor { !m.backups.busy })
        #expect(m.backups.failure == nil)
        #expect(m.backups.store.entries.isEmpty)
        #expect(files(dir).isEmpty)
        // and the next one runs
        m.backups.start(.presets)
        #expect(await waitFor { !m.backups.busy })
        #expect(m.backups.store.entries.count == 1)
        m.stop()
    }

    @Test func noBackupWithoutAUnit() {
        let m = UnitModel(source: DemoLinkSource(.fizz), backups: tempDir())
        #expect(!m.backups.canBackUp(.full) && !m.backups.canBackUp(.presets))
        m.backups.start(.full)
        #expect(!m.backups.busy && m.backups.run == nil)
    }

    @Test func failures() {
        #expect(BackupFailure.of(FizzBackupError.noBulk) == .updateFizz)
        #expect(BackupFailure.of(FizzBackupError.changed) == .changed)
        #expect(BackupFailure.of(FizzBackupError.timeout(offset: 512)) == .stopped)
        #expect(BackupFailure.of(FizzLinkError.lost) == .unplugged)
        #expect(BackupFailure.of(FizzBackupError.fzer(11)) == .refused(11))
        #expect(BackupFailure.of(FizzBackupError.cancelled) == nil)
        #expect(BackupFailure.of(CancellationError()) == nil)
        #expect(BackupFailure.changed.text == "FM-1 CHANGED" && BackupFailure.changed.again)
        #expect(BackupFailure.updateFizz.text == "UPDATE FIZZ" && !BackupFailure.updateFizz.again)
    }

    @Test func runStill() {
        let r = BackupRun.still(fraction: 0.6, layout: .device, elapsed: 7)
        #expect(r.lit.count == 73 && r.libSectors == 22)
        #expect(r.lit.filter { $0 }.count == 48 && r.now == 48)
    }
}

@Suite("Backups kept", .serialized) @MainActor
struct BackupStoreTests {
    /// A real .fizzbak of the demo unit's store (or presets only) at `date`.
    static func fizzbak(_ kind: FizzBackupKind = .full, presets n: Int = 143, date: Date) async throws -> [UInt8] {
        let e = try FizzDataShared.engine(), u = DemoUnit.fizz()
        let img = await u.store.image(for: u)
        return try await e.makeFizzbak(FizzbakInput(kind: kind, image: kind == .full ? img : nil,
                                                    region: FizzStoreRegion(lo: 0x97000, size: img.count),
                                                    presets: Array(u.dumpFrames.prefix(n)), fizz: "0.12.0-beta.11",
                                                    unitIdentity: u.idText, unitPort: u.portName, date: date))
    }

    @Test func saveListDeleteAndTheIndex() async throws {
        let dir = tempDir()
        let s = BackupStore(directory: dir)
        await s.reload()
        #expect(s.entries.isEmpty)
        let t0 = Date(timeIntervalSince1970: 1_800_000_000)
        let a = try await s.add(try await Self.fizzbak(date: t0.addingTimeInterval(60)), name: "a.fizzbak")
        let b = try await s.add(try await Self.fizzbak(.presets, presets: 20, date: t0.addingTimeInterval(180)), name: "b.fizzbak")
        let c = try await s.add(try await Self.fizzbak(date: t0), name: "a.fizzbak")         // (the same name: made unique)
        #expect(c.name == "a 2.fizzbak")
        #expect(s.entries.map(\.name) == ["b.fizzbak", "a.fizzbak", "a 2.fizzbak"])           // newest first
        #expect(b.kind == "presets" && b.presets == 20 && a.kind == "full" && a.layout == "device")
        #expect(s.totalSize == a.size + b.size + c.size)
        #expect(FileManager.default.fileExists(atPath: dir.appendingPathComponent("index.json").path))

        // shared, then read again by another store from the index alone
        s.markSaved(a)
        let s2 = BackupStore(directory: dir)
        await s2.reload()
        #expect(s2.entries == s.entries)
        #expect(s2.entries.first { $0.name == "a.fizzbak" }?.saved == true)

        // a file removed and one added behind its back (the Files app)
        try FileManager.default.removeItem(at: dir.appendingPathComponent("b.fizzbak"))
        try Data(try await Self.fizzbak(presets: 5, date: t0.addingTimeInterval(600))).write(to: dir.appendingPathComponent("from files.fizzbak"))
        try Data("not a backup".utf8).write(to: dir.appendingPathComponent("junk.fizzbak"))
        await s2.reload()
        #expect(s2.entries.map(\.name) == ["from files.fizzbak", "a.fizzbak", "a 2.fizzbak"])
        let f = try #require(s2.entries.first)
        #expect(f.file && f.saved && f.presets == 5)

        // the index lost: made again from the files
        try FileManager.default.removeItem(at: dir.appendingPathComponent("index.json"))
        let s3 = BackupStore(directory: dir)
        await s3.reload()
        #expect(s3.entries.map(\.name) == ["from files.fizzbak", "a.fizzbak", "a 2.fizzbak"])
        #expect(s3.entries[1].date == a.date && s3.entries[1].presets == 143 && s3.entries[1].tracks == 6)
        #expect(FileManager.default.fileExists(atPath: dir.appendingPathComponent("index.json").path))

        // delete
        s3.delete(s3.entries[0])
        #expect(s3.entries.map(\.name) == ["a.fizzbak", "a 2.fizzbak"])
        #expect(!FileManager.default.fileExists(atPath: dir.appendingPathComponent("from files.fizzbak").path))
        let s4 = BackupStore(directory: dir)
        await s4.reload()
        #expect(s4.entries.map(\.name) == ["a.fizzbak", "a 2.fizzbak"])
    }

    @Test func openingFiles() async throws {
        let dir = tempDir(), outside = tempDir()
        let s = BackupStore(directory: dir)
        await s.reload()
        // a .fizzbak from outside
        let bak = outside.appendingPathComponent("FIZZ backup 2026-10-20 0930.fizzbak")
        try Data(try await Self.fizzbak(date: Date(timeIntervalSince1970: 1_790_000_000))).write(to: bak)
        let e = try await s.importFile(at: bak)
        #expect(e.file && e.saved && e.kind == "full" && e.presets == 143)
        #expect(files(dir) == ["FIZZ backup 2026-10-20 0930.fizzbak"])
        // a .fizzlib
        let lib = outside.appendingPathComponent("my sounds.fizzlib")
        try FileManager.default.copyItem(at: Bundle.module.url(forResource: "lib", withExtension: "fizzlib")!, to: lib)
        let l = try await s.importFile(at: lib)
        #expect(l.kind == "presets" && l.presets > 0 && l.file)
        #expect(files(dir, "fizzlib") == ["my sounds.fizzlib"])
        // a store dump (.bin)
        let u = DemoUnit.fizz()
        let bin = outside.appendingPathComponent("store.bin")
        try Data(await u.store.image(for: u)).write(to: bin)
        let r = try await s.importFile(at: bin)
        #expect(r.kind == "full" && r.layout == "device" && r.presets == 143)
        // not a backup: nothing kept
        let junk = outside.appendingPathComponent("junk.fizzbak")
        try Data("nope".utf8).write(to: junk)
        await #expect(throws: BackupStoreError.self) { try await s.importFile(at: junk) }
        #expect(s.entries.count == 3)
        #expect(!FileManager.default.fileExists(atPath: dir.appendingPathComponent("junk.fizzbak").path))
        // one already in the folder: listed, not copied
        let again = try await s.importFile(at: dir.appendingPathComponent("my sounds.fizzlib"))
        #expect(again.name == "my sounds.fizzlib" && s.entries.count == 3)
    }

    @Test func theOldRule() {
        func e(_ kind: String, layout: String?, tracks: Int?) -> Bool {
            FizzBackupSummary.isOld(full: kind == "full", layout: layout, tracks: tracks, unitLayout: "device", unitTracks: 6)
        }
        #expect(!e("full", layout: "device", tracks: 6))
        #expect(e("full", layout: "host", tracks: 6))                  // another layout
        #expect(e("full", layout: "device", tracks: 8))                // eight tracks
        #expect(!e("presets", layout: nil, tracks: nil))               // presets only: never old
        #expect(FizzBackupSummary.isOld(full: true, layout: "device", tracks: 8, unitLayout: nil, unitTracks: nil))
        #expect(!FizzBackupSummary.isOld(full: true, layout: "device", tracks: 6, unitLayout: nil, unitTracks: nil))
        #expect(FizzBackupSummary.isOld(full: true, layout: "device", tracks: 6, unitLayout: "device", unitTracks: 8))
        #expect(whenText(Date(), now: Date()).hasPrefix("Today "))
        #expect(sizeText(299_008) == "292 KB" && sizeText(1_153_434) == "1.1 MB")
    }

    /// A backup row let go: a flick decides, else past half the buttons' width opens it.
    @Test func rowSwipeLetGo() {
        let r: CGFloat = 184
        #expect(BackupRow.opens(at: -100, speed: 0, reveal: r))         // past half way
        #expect(!BackupRow.opens(at: -80, speed: 0, reveal: r))         // short of it: shuts
        #expect(BackupRow.opens(at: -30, speed: -900, reveal: r))       // a flick left opens
        #expect(!BackupRow.opens(at: -150, speed: 900, reveal: r))      // a flick right shuts an open one
    }
}
