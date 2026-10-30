// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// ▶ play a backup (mock-up 44), end to end on the emulated FIZZ: a backup of THIS iPHONE with a preset in it, a preset
// added after it; ▶ boots the backup in a throwaway FIZZ (the preset there, the later one not), THIS iPHONE's flash
// file untouched by what is done in it; × gives THIS iPHONE the core back: its clock runs, UnitModel's link answers
// again, its store is its own. (In the Emulator suite: the core is the process's one.)

import Foundation
import Testing
import FizzKit
@testable import FizzEngine
@testable import FizzUI

extension EmulatorTests {
    @MainActor @Test func aBackupPlaysInAThrowawayFizz() async throws {
        let d = FileManager.default.temporaryDirectory.appendingPathComponent("fizz-bkplay-\(UUID().uuidString)")
        defer { try? FileManager.default.removeItem(at: d) }
        let storage = FlashStorage(url: d.appendingPathComponent("Emulator/fizz-flash.bin"))
        let e = FizzEmulator(storage: storage)
        let host = EmulatorHost(emulator: e)
        let m = UnitModel(source: NoPorts(), emulator: host, backups: d.appendingPathComponent("Backups"),
                          projects: d.appendingPathComponent("Projects"), presets: d.appendingPathComponent("Presets"))
        host.launch()
        m.start()
        func until(_ s: Double = 15, _ ok: () -> Bool) async -> Bool {
            let end = Date().addingTimeInterval(s)
            while !ok() && Date() < end { try? await Task.sleep(for: .milliseconds(20)) }
            return ok()
        }
        func mine(_ link: FizzLink?) async -> [String] {
            ((try? await link?.list()) ?? []).filter { $0.source == "mine" }.map(\.name)
        }
        func store(_ name: String, on link: FizzLink?) async throws -> Bool {
            let l = try #require(link)
            let f = try #require(try await l.dump(source: .factory, slot: 4))
            let s = try #require(FizzSysEx.parseDump(f))
            return try await l.store(try FizzSysEx.storeSound(name: name, blob: s.blob, drive: s.drive)).isOK
        }
        #expect(await until { m.onEmulator && m.dumpsLoaded })

        // a backup of THIS iPHONE with BAK PLAY in it; AFTER added once it was made
        #expect(try await store("BAK PLAY", on: m.link))
        await m.refresh()
        m.backups.start(.full)
        #expect(await until(30) { !m.backups.busy && m.backups.store.entries.count == 1 })
        let entry = try #require(m.backups.store.entries.first)
        #expect(entry.isFull && m.backups.canPlay(entry))
        #expect(try await store("AFTER", on: m.link))

        // ▶: PLAY opens on a throwaway FIZZ booted from the backup
        host.playShown = true                                   // (PLAY shown: its clock heard, or on its timer)
        #expect(await m.backups.play(entry))
        let bk = try #require(host.backup)
        #expect(m.goTo == .play && m.backups.playingName == entry.name)
        #expect(bk.emulator !== e && bk.emulator.storage == nil && host.active === bk.emulator)
        #expect(bk.title.hasPrefix("BACKUP · "))
        #expect(!e.isBooted, "THIS iPHONE set aside: the core is the backup's")
        let kept = try #require(storage.load(size: e.info.flashSize), "THIS iPHONE's flash saved as it was set aside")
        #expect(await until { host.mode != .stopped })
        #expect(await until { !m.onEmulator }, "the manager's link to THIS iPHONE gone with its core")
        let tl = FizzLink(transport: bk.emulator.makeTransport(), portName: "backup")
        let there = await mine(tl)
        #expect(there.contains("BAK PLAY") && !there.contains("AFTER"))
        // played and changed in the throwaway: never in THIS iPHONE's file
        #expect(try await store("THROWAWAY", on: tl))
        try await Task.sleep(for: .milliseconds(300))
        await tl.close()
        // (the manager looking for THIS iPHONE meanwhile does not take the core back)
        try await Task.sleep(for: .milliseconds(2500))
        #expect(host.backup != nil && !e.isBooted && bk.emulator.isBooted)

        // ×: the copy thrown away, THIS iPHONE booted again from its file; its clock runs, its link answers
        await host.endBackup()
        #expect(host.backup == nil && host.active === e && m.backups.playingName == nil)
        #expect(e.isBooted && !bk.emulator.isBooted)
        #expect(storage.load(size: e.info.flashSize) == kept, "THIS iPHONE's flash file untouched while the backup played")
        host.playShown = false                                  // (back in the manager: the unit's timer clock)
        #expect(await until { m.onEmulator && m.dumpsLoaded }, "UnitModel's link to THIS iPHONE again")
        #expect(await until { host.mode == .timer })
        let t0 = e.deviceMs
        try await Task.sleep(for: .milliseconds(300))
        #expect(e.deviceMs - t0 >= 150, "THIS iPHONE's clock runs")
        let back = await mine(m.link)
        #expect(back.contains("BAK PLAY") && back.contains("AFTER") && !back.contains("THROWAWAY"), "\(back)")
        // and plays: a note on its keybed sounds through PLAY's model
        host.playShown = true
        #expect(await until { host.mode != .stopped })
        host.play.keyDown(12, velocity: 100)
        try await Task.sleep(for: .milliseconds(120))
        host.play.keyUp(12)
        #expect(host.play.emulator === e)

        host.playShown = false
        m.stop()
        await host.close()
    }

    @Test func aBackupsStoreBecomesTheWholeFlash() throws {
        let lib = (0..<3).map { k in
            StoreImage.Record(key: 5 << 24 | UInt32(k), bytes: (0..<151).map { UInt8(truncatingIfNeeded: $0 &+ k) })
        }
        let prj = [StoreImage.Record(key: 2 << 24 | 1 << 16, bytes: [UInt8](repeating: 9, count: 500))]
        for l in [StoreImage.device, StoreImage.ble] {
            let img = try StoreImage.build(l, lib: lib, prj: prj)
            #expect(StoreImage.layout(of: img)?.name == l.name)
            let flash = try StoreImage.hostFlash(from: img)
            #expect(flash.count == StoreImage.host.size)
            let r = StoreImage.records(flash, StoreImage.host)
            #expect(r.lib == lib && r.prj == prj)
        }
        // a device-sized image with the Bluetooth magic is no store; another size neither
        var odd = try StoreImage.build(StoreImage.ble, lib: lib, prj: prj)
        odd += [UInt8](repeating: 0xFF, count: StoreImage.device.size - odd.count)
        #expect(StoreImage.layout(of: odd) == nil)
        #expect(throws: StoreImage.Failure.notStore) { try StoreImage.hostFlash(from: [UInt8](repeating: 0xFF, count: 5000)) }
        #expect(BackupPlaying.day(Date(timeIntervalSince1970: 1_761_652_800)) == "28 OCT")
    }
}
