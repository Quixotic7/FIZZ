// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// FizzData in JavaScriptCore against Node: the same calls on the same synthetic inputs as ios/jsbundle/gen_fixtures.mjs
// made with the site modules in Node (fixtures.json): byte-equal files, equal results, the errors as Swift errors.

import CryptoKit
import Foundation
import Testing
@testable import FizzData

private enum Fx {
    nonisolated(unsafe) static let json: [String: Any] = {
        let url = Bundle.module.url(forResource: "fixtures", withExtension: "json")!
        return try! JSONSerialization.jsonObject(with: Data(contentsOf: url)) as! [String: Any]
    }()
    static func file(_ name: String, _ ext: String) -> [UInt8] {
        [UInt8](try! Data(contentsOf: Bundle.module.url(forResource: name, withExtension: ext)!))
    }
    static var expect: [String: Any] { json["expect"] as! [String: Any] }
    static var date: Date { FizzDataEngine.parseDate(json["date"] as! String)! }

    static func hex(_ s: String) -> [UInt8] {
        var out: [UInt8] = [], it = s.makeIterator()
        while let a = it.next(), let b = it.next() { out.append(UInt8(String([a, b]), radix: 16)!) }
        return out
    }
    static var sounds: [FizzSound] {
        (json["sounds"] as! [[String: Any]]).map { s in
            let sends = s["sends"] as! [String: Int], src = s["source"] as! [String: String]
            return FizzSound(name: s["name"] as! String, blob: [UInt8](Data(base64Encoded: s["blob"] as! String)!),
                             drive: s["drive"] as! Int, pan: s["pan"] as! Int,
                             sends: FizzSends(chorus: sends["chorus"]!, delay: sends["delay"]!, reverb: sends["reverb"]!),
                             tags: s["fizzTags"] as! [String], source: ["fizz": .string(src["fizz"]!), "from": .string(src["from"]!)])
        }
    }
    static var frames: [[UInt8]] { (json["frames"] as! [String]).map(hex) }
    static var projects: [FizzProjectName] {
        (json["projects"] as! [[String: Any]]).map { FizzProjectName(slot: $0["slot"] as! Int, name: $0["name"] as! String) }
    }
    static var stats: FizzTransferStats {
        let s = json["stats"] as! [String: Any], r = s["rtt"] as! [String: Double]
        func d(_ k: String) -> Double { (s[k] as! NSNumber).doubleValue }
        return FizzTransferStats(bytes: s["bytes"] as! Int, ms: d("ms"), kbps: d("kbps"), frames: s["frames"] as! Int,
                               resent: s["resent"] as! Int, depth: s["depth"] as! Int, gapMs: d("gapMs"), perFrame: d("perFrame"),
                               rttAvg: r["avg"]!, rttMedian: r["median"]!, rttP95: r["p95"]!, rttMax: r["max"]!)
    }
    static func input(_ kind: FizzBackupKind, image: [UInt8]) -> FizzbakInput {
        let unit = json["unit"] as! [String: String], region = json["region"] as! [Int]
        return FizzbakInput(kind: kind, image: image, region: FizzStoreRegion(lo: region[0], size: region[1]), presets: frames,
                            fizz: json["fizz"] as! String, unitIdentity: unit["identity"]!, unitPort: unit["port"]!,
                            webTags: json["webTags"] as! [String: [String]], stats: kind == .full ? stats : nil, date: date)
    }

    /// An engine whose Dates read UTC (the fixtures were made so: the ZIP's DOS times and the file names are local time).
    static func engine() async throws -> FizzDataEngine {
        let e = try FizzDataEngine()
        _ = try await e.evaluate(json["utcDates"] as! String)
        return e
    }
}

private func sha(_ b: [UInt8]) -> String { SHA256.hash(data: Data(b)).map { String(format: "%02x", $0) }.joined() }
private func expected(_ k: String) -> (length: Int, sha: String) {
    let e = Fx.expect[k] as! [String: Any]
    return (e["length"] as! Int, e["sha256"] as! String)
}

@Suite struct FizzDataTests {
    @Test func loadsTheBundle() async throws {
        let e = try FizzDataEngine()
        #expect(e.tagNames == ["BASS", "KEYS", "PLUCK", "PAD", "LEAD", "DRUMS", "FX"])
        let polyfilled = try await e.evaluate("[typeof TextEncoder, typeof TextDecoder, typeof atob, typeof btoa, typeof __fizzInflateRaw].join()")
        #expect(polyfilled == "function,function,function,function,function")
    }

    @Test func crc32AsNode() async throws {
        let e = try await Fx.engine()
        for c in Fx.expect["crc32"] as! [[String: Any]] {
            #expect(try await e.crc32(Fx.hex(c["hex"] as! String)) == (c["crc"] as! NSNumber).uint32Value)
        }
        #expect(try await e.hex8(0x1a2b) == "00001a2b")
    }

    @Test func fileNamesAndKeysAsNode() async throws {
        let e = try await Fx.engine()
        let names = Fx.expect["backupFileName"] as! [String: String]
        #expect(try await e.backupFileName(date: Fx.date, kind: .full) == names["full"])
        #expect(try await e.backupFileName(date: Fx.date, kind: .presets) == names["presets"])
        var keys: [String] = []
        for s in Fx.sounds { keys.append(try await e.soundKey(s)) }
        #expect(keys == Fx.expect["soundKeys"] as! [String])
    }

    @Test func demoStoreAsNodeAndReadsBack() async throws {
        let e = try await Fx.engine()
        let img = try await e.demoStore(presets: Fx.sounds, projects: Fx.projects)
        let x = expected("demoStore")
        #expect(img.count == x.length && img.count == 299_008)
        #expect(sha(img) == x.sha)
        #expect(try await e.plainJSON("readRegion", [.bytes(img)]) == Fx.expect["readRegion"] as! String)
        let r = try await e.readRegion(img)
        #expect(r.layout == "device" && r.tracks == 6 && r.format == 2 && r.magic == "Z2FS" && !r.blank)
        #expect(r.projects == Fx.projects.map { FizzProjectSlot(slot: $0.slot, name: $0.name) })
        #expect(r.presets.map(\.name) == Fx.sounds.map(\.name))
        #expect(r.presets.map(\.tags) == Fx.sounds.map(\.tags))
        #expect(r.presets.map(\.blob) == Fx.sounds.map(\.blob))
        #expect(r.presets.map(\.pan) == Fx.sounds.map(\.pan) && r.presets.map(\.sends) == Fx.sounds.map(\.sends))
        #expect(r.presets.map(\.slot) == [1, 2, 3, 4] && r.presets.map(\.id) == [256, 257, 258, 259])
        #expect(r.used.libUsed == 4 && r.used.libTotal == 330 && r.used.prjUsed == 3)
        #expect(try await e.layoutOf(size: 299_008) == "device")
        #expect(try await e.layoutOf(size: 5) == nil)
    }

    @Test func makeFizzbakAsNode() async throws {
        let e = try await Fx.engine()
        let img = try await e.demoStore(presets: Fx.sounds, projects: Fx.projects)
        let full = try await e.makeFizzbak(Fx.input(.full, image: img))
        let x = expected("fullFizzbak")
        #expect(full.count == x.length && sha(full) == x.sha)
        let presets = try await e.makeFizzbak(Fx.input(.presets, image: img))
        #expect(presets == Fx.file("presets", "fizzbak"))
        var noDump = Fx.input(.full, image: img)
        noDump.presets = nil; noDump.fizz = ""; noDump.webTags = [:]; noDump.stats = nil
        let y = expected("noDumpFizzbak")
        let nd = try await e.makeFizzbak(noDump)
        #expect(nd.count == y.length && sha(nd) == y.sha)
    }

    @Test func readBackupAsNode() async throws {
        let e = try await Fx.engine()
        let want = Fx.expect["readBackup"] as! [String: String]
        let img = try await e.demoStore(presets: Fx.sounds, projects: Fx.projects)
        let full = try await e.makeFizzbak(Fx.input(.full, image: img))
        let cases: [(String, [UInt8], String)] = [
            ("full", full, "a.fizzbak"), ("presets", Fx.file("presets", "fizzbak"), "presets.fizzbak"),
            ("fizzlib", Fx.file("lib", "fizzlib"), "lib.fizzlib"), ("deflated", Fx.file("deflated", "fizzlib"), "deflated.fizzlib"),
            ("bin", img, "store.bin"), ("notBackup", [UInt8](repeating: 0, count: 1000), "x.bin"),
        ]
        for (k, bytes, file) in cases {
            let got = try await e.plainJSON("readBackup", [.bytes(bytes), .string(file)])
            #expect(got == want[k], "readBackup \(k)")
        }
    }

    @Test func backupSummary() async throws {
        let e = try await Fx.engine()
        let clock = ContinuousClock()
        var img: [UInt8] = [], file: [UInt8] = [], b: FizzBackupSummary!
        let t0 = try await clock.measure { img = try await e.demoStore(presets: Fx.sounds, projects: Fx.projects) }
        let t1 = try await clock.measure { file = try await e.makeFizzbak(Fx.input(.full, image: img)) }
        let t2 = try await clock.measure { b = try await e.readBackup(file, fileName: "a.fizzbak") }
        let t3 = try clock.measure { _ = try FizzDataEngine() }
        print("FizzData: engine \(t3), demoStore \(t0), makeFizzbak full \(t1), readBackup full \(t2)")
        #expect(b.kind == .full && b.source == .fizzbak)
        #expect(b.date == Fx.date && b.dateText == "2026-10-27T14:32:10.250Z")
        #expect(b.fizz == "0.12.0-beta.11" && b.unitIdentity == "FM-1_46001100" && b.unitPort == "FIZZ FM-1")
        #expect(b.layout == "device" && b.tracks == 6 && b.magic == "Z2FS")
        #expect(b.projects == [FizzProjectSlot(slot: 1, name: "NOIR"), FizzProjectSlot(slot: 3, name: "SUNDAY 2")])
        #expect(b.presetCount == 4 && b.presets.map(\.name) == Fx.sounds.map(\.name))
        #expect(b.presets.map(\.source) == (1...4).map { ["fizz": "0.12.0-beta.11", "from": .string("MY PRESETS \($0)")] })
        #expect(b.image == img && b.storeSize == 299_008)
        #expect(b.storeCRC == (try await e.hex8(try await e.crc32(img))))
        #expect(b.region == FizzStoreRegion(lo: 0x97000, size: 299_008))
        #expect(b.webTags == [try await e.soundKey(Fx.sounds[0]): ["SHIMMER", "DARK"]])
        #expect(b.transfer?["kbps"] == 67.6 && b.transfer?["rttMs"]?["p95"] == 5.68 && b.transfer?["ms"] == 4322)
        #expect(!b.isOld(unitLayout: "device", unitTracks: 6))
        #expect(b.isOld(unitLayout: "host", unitTracks: 6) && b.isOld(unitLayout: "device", unitTracks: 8))
        let p = try await e.readBackup(Fx.file("presets", "fizzbak"), fileName: "p.fizzbak")
        #expect(p.kind == .presets); #expect(p.image == nil); #expect(p.projects == nil); #expect(p.presetCount == 4)
        #expect(!p.isOld(unitLayout: "host", unitTracks: 8))
        let l = try await e.readBackup(Fx.file("deflated", "fizzlib"), fileName: "d.fizzlib")
        #expect(l.source == .fizzlib && l.presetCount == 5 && l.skipped.isEmpty && l.presets.last?.blobVersion == 5)
        let bin = try await e.readBackup(img, fileName: "store.bin")
        #expect(bin.source == .bin && bin.presetCount == 4 && bin.date == nil)
        let s = try await e.soundFromStoreFrame(Fx.frames[1])
        #expect(s.name == "A/B KEYS" && s.tags == ["KEYS", "PLUCK"] && s.blob == Fx.sounds[1].blob)
        #expect(try await e.plainJSON("soundFromStoreFrame", [.bytes(Fx.frames[0])]) != "")
    }

    /// The Bluetooth package's store (49 sectors from 0xAF000, "Z2FB"): its layout known, a full backup made and read
    /// back (a bundle older than ble-site refused it: the app's NOT SAVED).
    @Test func bluetoothPackageBackup() async throws {
        let e = try await Fx.engine()
        let l = try #require(try await e.layout(size: 49 * 4096))
        #expect(l == FizzLayout(name: "ble", size: 200_704, libSectors: 11, prjSectors: 38))
        let img = try await e.demoStore(presets: Fx.sounds, projects: Array(Fx.projects.prefix(2)), layout: "ble")
        #expect(img.count == 200_704)
        var input = Fx.input(.full, image: img)
        input.region = FizzStoreRegion(lo: 0xAF000, size: 200_704)
        input.crc = try await e.crc32(img)
        let file = try await e.makeFizzbak(input)
        let b = try await e.readBackup(file, fileName: "ble.fizzbak")
        #expect(b.kind == .full && b.layout == "ble" && b.magic == "Z2FB" && b.storeSize == 200_704)
        #expect(b.region == FizzStoreRegion(lo: 0xAF000, size: 200_704) && b.image == img)
        #expect(b.presetCount == 4)
        #expect(!b.isOld(unitLayout: "ble", unitTracks: 6) && b.isOld(unitLayout: "device", unitTracks: 6))
    }

    @Test func jsExceptionsAreSwiftErrors() async throws {
        let e = try await Fx.engine()
        await #expect(throws: FizzDataError.js(message: "x.bin: not a FIZZ backup", code: nil)) {
            _ = try await e.readBackup([UInt8](repeating: 0, count: 1000), fileName: "x.bin")
        }
        await #expect(throws: FizzDataError.js(message: "a full backup needs the store's image", code: nil)) {
            _ = try await e.makeFizzbak(FizzbakInput(kind: .full))
        }
        await #expect(throws: FizzDataError.js(message: "the records do not fit (16 sectors)", code: "full")) {
            _ = try await e.call("writeStore", [.bytes([UInt8](repeating: 0, count: 22 * 4096)), 0, 22, .array((0..<300).map { i in
                ["key": .int(0x0500_0000 + i), "bytes": .bytes([UInt8](repeating: 1, count: 200))] }), "Z2FS"])
        }
        do { _ = try await e.readRegion([1, 2, 3]); Issue.record("no error") } catch let x as FizzDataError {
            #expect(x.message.hasPrefix("not a FIZZ store image"))
        }
        await #expect(throws: FizzDataError.bridge("FizzData has no function nothing")) { _ = try await e.call("nothing") }
        await #expect(throws: FizzDataError.self) { _ = try await e.evaluate("throw new TypeError('boom')") }
    }

    @Test func aStoreImageCrossesQuickly() async throws {
        let e = try await Fx.engine()
        var img = [UInt8](repeating: 0xFF, count: 299_008)
        for i in stride(from: 0, to: img.count, by: 7) { img[i] = UInt8(i & 0xFF) }
        _ = try await e.roundTrip(img)                          // (warm)
        let clock = ContinuousClock()
        var back: [UInt8] = []
        let t = try await clock.measure { back = try await e.roundTrip(img) }
        #expect(back == img)
        #expect(t < .milliseconds(100), "a 299,008-byte image across the bridge and back: \(t)")
        print("FizzData: 299,008 bytes into JS and back in \(t)")
        #expect(try await e.roundTrip(img, begin: 1000, end: 1010) == Array(img[1000..<1010]))   // (a view's offset)
        #expect(try await e.roundTrip([]) == [])
    }

    @Test func nativeInflate() throws {
        // node: zlib.deflateRawSync("FIZZ FIZZ FIZZ")
        let z: [UInt8] = [0x73, 0xf3, 0x8c, 0x8a, 0x52, 0x70, 0x83, 0x11, 0x00]
        #expect(String(decoding: try RawInflate.inflate(z), as: UTF8.self) == "FIZZ FIZZ FIZZ")
        #expect(throws: (any Error).self) { _ = try RawInflate.inflate([0xFF, 0xFF, 0xFF]) }
        #expect(throws: (any Error).self) { _ = try RawInflate.inflate(Array(z.prefix(3))) }
    }
}
