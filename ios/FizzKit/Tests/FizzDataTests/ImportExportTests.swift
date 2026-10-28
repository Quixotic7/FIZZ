// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// Phase I4's JS calls in JavaScriptCore against Node (gen_fixtures.mjs "I4"): EXPORT's files byte for byte, IMPORT
// reading them back, the duplicates, the carry bars, the restore's plan and fit.

import CryptoKit
import Foundation
import Testing
@testable import FizzData

private enum I4Fx {
    nonisolated(unsafe) static let json: [String: Any] = {
        let url = Bundle.module.url(forResource: "fixtures", withExtension: "json")!
        return try! JSONSerialization.jsonObject(with: Data(contentsOf: url)) as! [String: Any]
    }()
    static var i4: [String: Any] { (json["expect"] as! [String: Any])["i4"] as! [String: Any] }
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
    static func engine() async throws -> FizzDataEngine {
        let e = try FizzDataEngine()
        _ = try await e.evaluate(json["utcDates"] as! String)
        return e
    }
}

private func sha(_ b: [UInt8]) -> String { SHA256.hash(data: Data(b)).map { String(format: "%02x", $0) }.joined() }

@Suite struct ImportExportTests {
    /// EXPORT's files as Node makes them; IMPORT reads them back as Node does; the duplicates against two DUMPs.
    @Test func exportThenImport() async throws {
        let e = try await I4Fx.engine()
        let all = I4Fx.sounds, one = Array(all.prefix(1))
        let want = I4Fx.i4["exportFile"] as! [String: [String: Any]]
        var files: [(name: String, bytes: [UInt8])] = []
        for k in FizzExportKind.allCases {
            for (label, ss) in [("one", one), ("all", all)] {
                let f = try await e.exportFile(ss, kind: k, date: I4Fx.date)
                let w = try #require(want["\(k.rawValue).\(label)"])
                #expect(f.name == w["name"] as? String && f.type == w["type"] as? String, "\(k) \(label)")
                #expect(f.bytes.count == w["length"] as? Int && sha(f.bytes) == w["sha256"] as? String, "\(k) \(label): the bytes")
                files.append((f.name, f.bytes))
            }
        }
        let bad: [(name: String, bytes: [UInt8])] = [("x.dnsnd", [UInt8](repeating: 0, count: 10)), ("y.fizzbak", [UInt8](repeating: 0, count: 10))]
        let args: FizzJSValue = .array((files + bad).map { ["name": .string($0.name), "bytes": .bytes($0.bytes)] })
        #expect(try await e.plainJSON("importRead", [args]) == I4Fx.i4["importRead"] as! String)

        let items = try await e.importRead(files + bad)
        let wantRead = try JSONSerialization.jsonObject(with: Data((I4Fx.i4["importRead"] as! String).utf8)) as! [Any]
        #expect(items.count == wantRead.count)
        let dn1 = try #require(items.first { $0.kind == .dn1 && $0.ok })
        #expect(dn1.sound?.name == "SYN BASS 1" && !dn1.notes.isEmpty && dn1.digitoneTags == ["BASS"] && dn1.sound?.tags == ["BASS"])
        #expect(items.contains { $0.kind == .dn2 && $0.ok } && items.suffix(2).map(\.kind) == [.dn1, .fizz])
        #expect(items.suffix(2).allSatisfy { !$0.ok && !$0.error.isEmpty })
        #expect(items.first { $0.kind == .fizz && $0.ok }?.notes == [])

        let dups = try await e.importDuplicates(items.compactMap(\.sound), mine: Array(I4Fx.frames.prefix(2)))
        let w = try JSONSerialization.jsonObject(with: Data((I4Fx.i4["importDuplicates"] as! String).utf8)) as! [String]
        #expect(dups == w)
        #expect(dups.first == "mine" && dups.contains("list") && dups.contains(""))
    }

    @Test func carryBars() async throws {
        let e = try await I4Fx.engine()
        let want = I4Fx.i4["exportCarry"] as! [String: String]
        for k in FizzExportKind.allCases {
            for (label, ss) in [("one", Array(I4Fx.sounds.prefix(1))), ("all", I4Fx.sounds)] {
                let args: [FizzJSValue] = [.array(ss.map(\.js)), .string(k.rawValue)]
                #expect(try await e.plainJSON("exportCarry", args) == want["\(k.rawValue).\(label)"], "\(k) \(label)")
            }
        }
        let fizz = try await e.exportCarry(I4Fx.sounds, kind: .fizz)
        #expect(fizz.bar == 10 && fizz.rows.isEmpty)
        let dn2 = try await e.exportCarry(I4Fx.sounds, kind: .dn2pst)
        #expect(dn2.bar < 10 && !dn2.rows.isEmpty && dn2.touched == 4 && dn2.rows.allSatisfy { !$0.how.isEmpty && $0.color.hasPrefix("--") })
    }

    /// A host-layout (USB) backup onto the Bluetooth package: the choice to make, the image made for it; too many
    /// projects refused; a backup of the unit's own layout restores whole; none: presets only.
    @Test func restorePlanAndFit() async throws {
        let e = try await I4Fx.engine()
        let host = try await e.demoStore(presets: I4Fx.sounds, projects: I4Fx.projects, layout: "host")
        let device = try await e.demoStore(presets: I4Fx.sounds, projects: I4Fx.projects)
        let ble = 49 * 4096
        let want = I4Fx.i4["restorePlan"] as! [String: String]
        #expect(try await e.plainJSON("restorePlan", [.bytes(host), .int(ble)]) == want["host"])
        #expect(try await e.plainJSON("restorePlan", [.bytes(device), .int(device.count)]) == want["same"])
        #expect(try await e.plainJSON("restorePlan", [.null, .int(device.count)]) == want["presets"])

        let plan = try await e.restorePlan(image: host, unitSize: ble)
        #expect(plan.mode == .full && plan.crossesPackages && plan.to?.nproj == 4 && plan.to?.nlib == 128 && plan.from?.name == "host")
        let fit = try #require(plan.fit)
        #expect(fit.projects.map(\.name) == ["NOIR", "SUNDAY 2"] && fit.presets.count == 4 && fit.choice.projects == [0, 2])
        let args: [FizzJSValue] = [.bytes(host), .int(ble), fit.choice.js, true]
        #expect(try await e.plainJSON("restoreFit", args) == I4Fx.i4["restoreFit"] as? String)
        let r = try await e.restoreFit(image: host, unitSize: ble, choice: FizzFitChoice(projects: [2], presets: [0, 3]), withImage: true)
        #expect(r.image?.count == ble && r.leftProjects == ["NOIR"] && r.leftPresets == ["A/B KEYS", "PAD*WIDE"])
        #expect(try await e.readRegion(r.image!).presets.map(\.name) == ["SYN BASS 1", "LEAD"])
        await #expect(throws: FizzDataError.js(message: "the Bluetooth package holds 4 projects and 128 MY PRESETS", code: "range")) {
            _ = try await e.restoreFit(image: host, unitSize: ble, choice: FizzFitChoice(projects: [0, 1, 2, 3, 4], presets: []), withImage: false)
        }
        #expect(try await e.restorePlan(image: device, unitSize: device.count).fit == nil)
        #expect(try await e.restorePlan(image: nil, unitSize: device.count).mode == .presets)
    }
}
