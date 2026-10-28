// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// The emulated FIZZ, headless on the Mac: its screen, sound and panel driven by hand (run(ms:), render(frames:)); FizzLink
// over EmulatorTransport with the clock on its timer (no audio device); the flash saved and loaded again.

import CoreGraphics
import FizzKit
import Foundation
import Testing
@testable import FizzEngine

@Suite("Emulator", .serialized)
struct EmulatorTests {
    static func tempStorage() -> FlashStorage {
        FlashStorage(url: FileManager.default.temporaryDirectory
            .appendingPathComponent("fizz-emu-\(UUID().uuidString)/fizz-flash.bin"))
    }

    static func pixels(_ img: CGImage?) -> [UInt8] {
        guard let img, let d = img.dataProvider?.data else { return [] }
        return [UInt8](d as Data)
    }

    static func colours(_ px: [UInt8]) -> Int {
        var s = Set<UInt32>()
        for i in stride(from: 0, to: px.count, by: 4) {
            s.insert(UInt32(px[i]) << 16 | UInt32(px[i + 1]) << 8 | UInt32(px[i + 2]))
        }
        return s.count
    }

    static func rms(_ x: [Float]) -> Float {
        (x.reduce(0) { $0 + $1 * $1 } / Float(max(x.count, 1))).squareRoot()
    }

    @Test func bootsAndDrawsItsFirstScreen() {
        let e = FizzEmulator(storage: nil)
        e.boot(flash: nil)
        #expect(e.isBooted)
        #expect(e.info.name == "FIZZ" && e.info.license == "MPL-2.0")
        #expect(e.info.buttonNames[FizzButton.tracks.rawValue] == "TRACKS")
        e.run(ms: 400)
        let img = e.lcdImage
        #expect(img?.width == 240 && img?.height == 240)
        let px = Self.pixels(img)
        #expect(px.count == 240 * 240 * 4)
        #expect(Self.colours(px) > 8, "a drawn screen, not a blank one")
        #expect(e.deviceMs == 400)
    }

    @Test func aNoteSounds() {
        let e = FizzEmulator(storage: nil)
        e.boot(flash: nil)
        e.run(ms: 300)
        e.tap(.rec)                                         // (REC: the keybed plays the track, not the steps)
        e.run(ms: 100)
        #expect(e.uiInfo().hasPrefix("keys:"))
        let quiet = e.render(frames: 128 * 20)
        #expect(Self.rms(quiet.left) == 0)
        e.noteOn(key: 12, velocity: 110)
        let (l, r) = e.render(frames: 128 * 60)
        e.noteOff(key: 12)
        #expect(Self.rms(l) > 0.005 && Self.rms(r) > 0.005, "rms \(Self.rms(l)) \(Self.rms(r))")
        // a MIDI note too, heard at 48 kHz (resampled), as many frames as asked
        _ = e.render(frames: 44100)
        e.midiIn([0x90, 64, 100])
        let h = e.render(frames: 4800, rate: 48000)
        e.midiIn([0x80, 64, 0])
        #expect(h.left.count == 4800 && Self.rms(h.left) > 0.005, "rms \(Self.rms(h.left))")
    }

    @Test func aButtonChangesTheScreen() {
        // the same power-on twice (the clock is simulated: the same screens at the same ms), EDIT pressed in one
        let e = FizzEmulator(storage: nil)
        e.boot(flash: nil)
        e.run(ms: 1200)
        let plain = Self.pixels(e.lcdImage)
        e.boot(flash: nil)
        e.run(ms: 600)
        e.press(.edit)
        e.run(ms: 60)
        e.release(.edit)
        e.run(ms: 540)
        let pressed = Self.pixels(e.lcdImage)
        #expect(plain.count == pressed.count && !plain.isEmpty)
        #expect(plain != pressed)
        // and a power-on again draws what the first one drew
        e.boot(flash: nil)
        e.run(ms: 1200)
        #expect(Self.pixels(e.lcdImage) == plain)
    }

    @Test func theLEDsFollowTheTransport() {
        let e = FizzEmulator(storage: nil)
        e.boot(flash: nil)
        e.run(ms: 300)
        let before = e.leds
        e.tap(.play)
        e.run(ms: 300)
        #expect(e.isPlaying)
        let after = e.leds
        #expect(before != after)
        e.tap(.play)
        e.run(ms: 100)
        #expect(!e.isPlaying)
    }

    @Test func fizzLinkTalksToTheEmulatedUnit() async throws {
        let e = FizzEmulator(storage: nil)
        e.boot(flash: nil)
        #expect(e.start(sound: false) == .timer)
        defer { e.stop() }
        let src = EmulatorLinkSource(emulator: e)
        let found = try await FizzLink.find(scanner: src.scanner)
        #expect(found.idText == "FM-1_46999999")
        #expect(found.portName == EmulatorLinkSource.defaultPortName)
        let link = found.link
        defer { Task { await link.close() } }
        // IDENTITY: FIZZ's own version
        #expect(try await link.identity() == e.info.version)
        // LIST: the factory's 23, no MY PRESETS on a fresh flash
        let all = try await link.list(factory: true)
        #expect(all.filter { $0.source == "factory" }.count == 23)
        #expect(all.filter { $0.source == "mine" }.isEmpty)
        // DUMP one
        let frame = try #require(try await link.dump(source: .factory, slot: 3))
        let d = try #require(FizzSysEx.parseDump(frame))
        #expect(d.name == all.first { $0.source == "factory" && $0.slot == 3 }?.name)
        // STORE it to MY PRESETS under another name: LIST shows it
        let r = try await link.store(try FizzSysEx.storeSound(name: "EMU TEST", blob: d.blob, drive: d.drive))
        #expect(r.isOK)
        let mine = try await link.list()
        #expect(mine.contains { $0.source == "mine" && $0.name == "EMU TEST" })
        #expect(await link.status() != nil)

        // a whole backup: the USB package's region, its CRC FZBI's; the preset in it
        let read = try await link.backup()
        #expect(read.size == StoreImage.device.size && read.lo == Int(StoreImage.deviceRegion))
        #expect(read.crc == FM1Bulk.crc32(read.image))
        let lib = StoreImage.records(read.image, StoreImage.device).lib
        #expect(lib.contains { String(decoding: $0.bytes[2..<10], as: UTF8.self) == "EMU TEST" })

        // a fresh flash, then the backup restored: the preset back
        e.reset()
        #expect(try await link.list().filter { $0.source == "mine" }.isEmpty)
        let done = try await link.restore(read.image)
        _ = done
        let back = try await link.list()
        #expect(back.contains { $0.source == "mine" && $0.name == "EMU TEST" })
        // FZSR of another size: FZER 11
        await #expect(throws: FizzRestoreError.self) { try await link.restore([UInt8](repeating: 0xFF, count: 4096)) }
    }

    @Test func theFlashSurvivesASaveAndAReload() async throws {
        let storage = Self.tempStorage()
        defer { try? FileManager.default.removeItem(at: storage.url.deletingLastPathComponent()) }
        let e = FizzEmulator(storage: storage)
        e.saveQuietMs = 300
        e.boot()                                            // (nothing saved: a fresh flash)
        e.start(sound: false)
        let link = FizzLink(transport: e.makeTransport(), portName: "FIZZ")
        let f = try #require(try await link.dump(source: .factory, slot: 5))
        let d = try #require(FizzSysEx.parseDump(f))
        #expect(try await link.store(try FizzSysEx.storeSound(name: "KEPT", blob: d.blob)).isOK)
        // saved by itself once the flash has been quiet a while
        let kept = { (img: [UInt8]?) in
            img.map { StoreImage.records($0, StoreImage.host).lib.contains {
                String(decoding: $0.bytes[2..<6], as: UTF8.self) == "KEPT" } } ?? false
        }
        var saved = false
        for _ in 0..<60 where !saved {
            try await Task.sleep(for: .milliseconds(100))
            saved = kept(storage.load(size: e.info.flashSize))
        }
        #expect(saved)
        await link.close()
        e.stop()

        let e2 = FizzEmulator(storage: storage)
        e2.boot()
        e2.run(ms: 300)
        #expect(kept(e2.flashImage()))
        e2.reset()
        #expect(storage.load(size: e2.info.flashSize) == nil)
        #expect(!kept(e2.flashImage()))
    }

    @Test func theStoreMovesBetweenLayouts() throws {
        let recs = (0..<40).map { k in
            StoreImage.Record(key: 5 << 24 | UInt32(k), bytes: (0..<151).map { UInt8(truncatingIfNeeded: $0 &* k) })
        }
        let prj = [StoreImage.Record(key: 2 << 24 | 3 << 16, bytes: [UInt8](repeating: 7, count: 700))]
        let host = try StoreImage.build(StoreImage.host, lib: recs, prj: prj)
        let dev = try StoreImage.convert(host, from: StoreImage.host, to: StoreImage.device)
        #expect(dev.count == StoreImage.device.size)
        let back = StoreImage.records(dev, StoreImage.device)
        #expect(back.lib == recs && back.prj == prj)
        #expect(try StoreImage.convert(dev, from: StoreImage.device, to: StoreImage.host) == host)
    }

    @Test func cpuPerBlock() {
        let e = FizzEmulator(storage: nil)
        e.boot(flash: nil)
        e.run(ms: 300)
        e.tap(.rec)                                         // (the keyboard: chords held over the demo pattern)
        e.run(ms: 100)
        e.tap(.play)
        for k in [0, 4, 7, 12] { e.noteOn(key: k, velocity: 100) }
        e.run(ms: 100)
        e.resetStats()
        let (l, _) = e.render(frames: 44100 * 5)
        let s = e.stats
        print(String(format: "FIZZ core on this Mac: %llu blocks, audio() %.1f us mean / %.1f us max per 128-frame block " +
                     "(%.1f %% of 2902 us); a device ms %.1f us mean / %.1f us max (load %.1f %%); rms %.3f; %@",
                     s.blocks, s.audioMeanUs, s.audioMaxUs, s.audioMeanUs / 2902 * 100, s.msMeanUs, s.msMaxUs, s.load * 100,
                     Self.rms(l), e.uiInfo()))
        #expect(Self.rms(l) > 0.01)
        #expect(s.blocks > 1700)
        #expect(s.audioMeanUs < 2902)
    }
}
