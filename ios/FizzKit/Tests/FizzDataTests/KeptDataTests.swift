// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// KEPT's JS calls in JavaScriptCore against Node (gen_fixtures.mjs "kept"): a kept preset's .fizzsnd (without web
// tags the web Manager's file byte for byte), read back with its web tags, a .fizzlib of them, the file names.

import CryptoKit
import Foundation
import Testing
@testable import FizzData

private enum KeptFx {
    nonisolated(unsafe) static let json: [String: Any] = {
        let url = Bundle.module.url(forResource: "fixtures", withExtension: "json")!
        return try! JSONSerialization.jsonObject(with: Data(contentsOf: url)) as! [String: Any]
    }()
    static var kept: [String: Any] { json["kept"] as! [String: Any] }
    static var date: Date { FizzDataEngine.parseDate(json["date"] as! String)! }
    static var fizz: String { json["fizz"] as! String }
    static var sounds: [FizzSound] {
        (json["sounds"] as! [[String: Any]]).map { s in
            let sends = s["sends"] as! [String: Int], src = s["source"] as! [String: String]
            return FizzSound(name: s["name"] as! String, blob: [UInt8](Data(base64Encoded: s["blob"] as! String)!),
                             drive: s["drive"] as! Int, pan: s["pan"] as! Int,
                             sends: FizzSends(chorus: sends["chorus"]!, delay: sends["delay"]!, reverb: sends["reverb"]!),
                             tags: s["fizzTags"] as! [String], source: ["fizz": .string(src["fizz"]!), "from": .string(src["from"]!)])
        }
    }
    static func engine() async throws -> FizzDataEngine {
        let e = try FizzDataEngine()
        _ = try await e.evaluate(json["utcDates"] as! String)
        return e
    }
}

private func sha(_ b: [UInt8]) -> String { SHA256.hash(data: Data(b)).map { String(format: "%02x", $0) }.joined() }

@Suite struct KeptDataTests {
    @Test func keptFilesAsNode() async throws {
        let e = try await KeptFx.engine()
        let s = KeptFx.sounds
        let k = KeptFx.kept
        let plain = try await e.keptFile(s[0])
        let wantPlain = k["plain"] as! [String: Any]
        #expect(plain.count == wantPlain["length"] as? Int && sha(plain) == wantPlain["sha256"] as? String)
        // (no web tags: the web Manager's EXPORT file, byte for byte)
        #expect(plain == (try await e.exportFile([s[0]], kind: .fizz, date: KeptFx.date)).bytes)

        let tagged = try await e.keptFile(s[1], webTags: ["SHIMMER", "DARK"], source: ["from": "KEPT"])
        let wantTagged = k["tagged"] as! [String: Any]
        #expect(sha(tagged) == wantTagged["sha256"] as? String)
        #expect(try await e.plainJSON("keptRead", [.bytes(tagged), "B.fizzsnd"]) == k["readTagged"] as! String)
        #expect(try await e.plainJSON("keptRead", [.bytes(plain), "A.fizzsnd"]) == k["readPlain"] as! String)

        let back = try await e.keptRead(tagged, fileName: "B.fizzsnd")
        #expect(back.webTags == ["SHIMMER", "DARK"] && back.sound.name == s[1].name && back.sound.blob == s[1].blob)
        #expect(back.sound.tags == s[1].tags && back.sound.source["from"]?.string == "KEPT" && back.sound.file == "B.fizzsnd")
        #expect(try await e.keptRead(plain).webTags == [])
        // (written back as read: the same bytes)
        #expect(try await e.keptFile(back.sound, webTags: back.webTags) == tagged)
    }

    @Test func keptLibraryAsNode() async throws {
        let e = try await KeptFx.engine()
        let s = KeptFx.sounds
        let lib = try await e.keptLib([FizzKeptSound(sound: s[0], webTags: ["SHIMMER"]), FizzKeptSound(sound: s[2])],
                                      name: "SYN", fizz: KeptFx.fizz, date: KeptFx.date)
        let want = KeptFx.kept["lib"] as! [String: Any]
        #expect(lib.count == want["length"] as? Int && sha(lib) == want["sha256"] as? String)
        // IMPORT reads it as any library
        let items = try await e.importRead([("kept.fizzlib", lib)])
        #expect(items.map { $0.sound?.name } == [s[0].name, s[2].name])
    }

    @Test func fileNamesAndRefusals() async throws {
        let e = try await KeptFx.engine()
        var names: [String] = []
        for n in ["PLUCK 2", "A/B:C", "  ", "BASS*"] { names.append(try await e.keptFileName(n)) }
        #expect(names == KeptFx.kept["fileNames"] as! [String])
        #expect(names.first == "PLUCK 2.fizzsnd")
        await #expect(throws: (any Error).self) {
            _ = try await e.keptRead(Array("{\"format\":\"fizz-library\"}".utf8), fileName: "x.fizzsnd")
        }
    }
}
