// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// I6: PROJECTS' calls (entry.js pjOpen ..) in JavaScriptCore against Node: the session gen_fixtures.mjs ran (its "i6"),
// call by call, the same results; and the Swift wrappers read them.

import CryptoKit
import Foundation
import Testing
@testable import FizzData

private func sha(_ b: [UInt8]) -> String { SHA256.hash(data: Data(b)).map { String(format: "%02x", $0) }.joined() }

@Suite struct ProjectsDataTests {
    nonisolated(unsafe) static let fx: [String: Any] = {
        let url = Bundle.module.url(forResource: "fixtures", withExtension: "json")!
        let all = try! JSONSerialization.jsonObject(with: Data(contentsOf: url)) as! [String: Any]
        return all["i6"] as! [String: Any]
    }()
    static var spec: [(slot: Int, name: String, pats: Int, demo: Bool)] {
        (fx["projects"] as! [[String: Any]]).map { ($0["slot"] as! Int, $0["name"] as! String, $0["pats"] as! Int, ($0["demo"] as? Bool) ?? false) }
    }

    /// The fixtures' engine and store image (demoStore + demoPatterns).
    static func setUp() async throws -> (FizzDataEngine, [UInt8], [String: Any]) {
        let all = try JSONSerialization.jsonObject(with: Data(contentsOf: Bundle.module.url(forResource: "fixtures", withExtension: "json")!)) as! [String: Any]
        let e = try FizzDataEngine()
        _ = try await e.evaluate(all["utcDates"] as! String)
        let sounds = (all["sounds"] as! [[String: Any]]).map { s in
            let sends = s["sends"] as! [String: Int], src = s["source"] as! [String: String]
            return FizzSound(name: s["name"] as! String, blob: [UInt8](Data(base64Encoded: s["blob"] as! String)!),
                             drive: s["drive"] as! Int, pan: s["pan"] as! Int,
                             sends: FizzSends(chorus: sends["chorus"]!, delay: sends["delay"]!, reverb: sends["reverb"]!),
                             tags: s["fizzTags"] as! [String], source: ["fizz": .string(src["fizz"]!), "from": .string(src["from"]!)])
        }
        let base = try await e.demoStore(presets: sounds, projects: spec.map { FizzProjectName(slot: $0.slot, name: $0.name) })
        let img = try await e.demoPatterns(base, prev: nil, projects: spec)
        return (e, img, all)
    }

    @Test func aSessionAsNode() async throws {
        let (e, img, all) = try await Self.setUp()
        let x = Self.fx, date = FizzDataEngine.parseDate(all["date"] as! String)!
        let opts: FizzJSValue = ["fizz": .string(all["fizz"] as! String), "date": .date(date)]
        let d = x["demoPatterns"] as! [String: Any]
        #expect(img.count == d["length"] as! Int && sha(img) == d["sha256"] as! String)

        func same(_ key: String, _ name: String, _ args: [FizzJSValue]) async throws {
            let got = try await e.plainJSON(name, args)
            #expect(got == x[key] as! String, "\(name)")
        }
        try await same("open", "pjOpen", [.bytes(img)])
        try await same("copy", "pjCopy", [1, 0, [4, 2, 3]])
        try await same("paste", "pjPaste", [1, 3, 16, 1])
        try await same("clearDemo", "pjClear", [1, 2, [0]])
        try await same("clear", "pjClear", [1, 0, [5]])
        try await same("keep", "pjKeep", [1, 0, opts])
        let keep = try await e.call("pjKeep", [1, 0, opts])
        let kb = try #require(keep["bytes"]?.bytes), kn = keep["fileName"]?.string ?? ""
        try await same("keptInfo", "pjKeptInfo", [.bytes(kb), .string(kn)])
        try await same("copyKept", "pjCopyKept", [1, .bytes(kb), .string(kn), [1, 0]])
        try await same("loadKept", "pjLoadKept", [1, 4, .bytes(kb), .string(kn)])
        let pat = try await e.projectsFizzpat(1, fizz: all["fizz"] as! String, date: date)
        let fp = x["fizzpat"] as! [String: Any]
        #expect(pat.fileName == fp["fileName"] as! String && sha(pat.bytes) == fp["sha256"] as! String)
        try await same("readFizzpat", "pjReadFizzpat", [.bytes(pat.bytes), .string(pat.fileName)])
        try await same("renameKept", "pjRenameKept", [.bytes(kb), .string(kn), "noir two", opts])
        let im = try await e.projectsImage(1)
        let ie = x["image"] as! [String: Any]
        if case .success(let b) = im { #expect(sha(b) == ie["sha256"] as! String) } else { Issue.record("pjImage refused") }
        try await same("undo", "pjUndo", [1])
        try await same("rebase", "pjRebase", [1, .bytes(img)])
        try await same("discard", "pjDiscard", [1])
    }

    @Test func theWrappersRead() async throws {
        let (e, img, _) = try await Self.setUp()
        let (h, v) = try await e.projectsOpen(img)
        #expect(v.nproj == 8 && v.layout == "device" && v.ops.isEmpty)
        let noir = try #require(v.project(0)), tour = try #require(v.project(2))
        #expect(noir.there && noir.name == "NOIR" && noir.used == 18 && noir.pats.count == 64)
        #expect(noir.pats[0]?.rows.count == 6 && (noir.pats[0]?.trigs ?? 0) > 0 && noir.pats[18] == nil)
        #expect(tour.demo && tour.pats[0]?.demo == true && tour.used == 16)

        let clip = try #require(try await e.projectsCopy(h, p: 0, ks: [2, 3, 4]))
        #expect(clip.n == 3 && clip.from == "NOIR" && clip.range == "A03–A05")
        #expect(try await e.projectsCopy(h, p: 1, ks: [0]) == nil, "an empty slot: nothing")
        guard case .success(let v2) = try await e.projectsPaste(h, q: 3, j: 16, clip: clip.c) else { Issue.record("paste"); return }
        #expect(v2.ops.count == 1 && v2.ops[0].kind == .paste && v2.ops[0].at == [16, 17, 18] && v2.project(3)?.changed == true)
        #expect(v2.project(3)?.pats[16] == noir.pats[2], "the same trigs")
        guard case .failure(let r) = try await e.projectsClear(h, q: 2, ks: [0]) else { Issue.record("the demos' own cleared"); return }
        #expect(r.code == "demo")
        guard case .failure(let r2) = try await e.projectsPaste(h, q: 0, j: 63, clip: clip.c) else { Issue.record("past D16"); return }
        #expect(r2.code == "range")

        let kept = try await e.projectsKeep(h, p: 0, fizz: "t")
        #expect(kept.fileName == "NOIR.fizzprj" && kept.pats == 18 && !kept.colours.isEmpty)
        let info = try await e.projectsKeptInfo(kept.bytes, fileName: kept.fileName)
        #expect(info.name == "NOIR" && info.used.filter { $0 }.count == 18 && info.date != nil)
        let ren = try await e.projectsRenameKept(kept.bytes, fileName: kept.fileName, to: "night", fizz: "t")
        #expect(ren.name == "NIGHT" && ren.fileName == "NIGHT.fizzprj")
        guard case .success(let v3) = try await e.projectsLoadKept(h, q: 4, file: ren.bytes, fileName: ren.fileName) else { Issue.record("load"); return }
        #expect(v3.project(4)?.name == "NIGHT" && v3.project(4)?.used == 18 && v3.ops.count == 2)

        let fp = try await e.projectsFizzpat(clip.c, fizz: "t")
        guard case .success(let c2?) = try await e.projectsReadFizzpat(fp.bytes, fileName: fp.fileName) else { Issue.record("fizzpat"); return }
        #expect(c2.ks == [2, 3, 4] && c2.c != clip.c)
        guard case .failure = try await e.projectsReadFizzpat([1, 2, 3], fileName: "x.fizzpat") else { Issue.record("not a file"); return }
        guard case .success(let k2?) = try await e.projectsCopyKept(h, file: kept.bytes, fileName: kept.fileName, ks: [0]) else { Issue.record("kept copy"); return }
        #expect(k2.n == 1 && k2.from == "NOIR")

        guard case .success(let out) = try await e.projectsImage(h) else { Issue.record("image"); return }
        #expect(out.count == img.count && out != img)
        #expect(try await e.projectsUndo(h).ops.count == 1)
        let (rb, refused) = try await e.projectsRebase(h, image: img)
        #expect(refused == nil && rb.ops.count == 1)
        #expect(try await e.projectsDiscard(h).ops.isEmpty)
        await e.projectsClose(h)
        await #expect(throws: FizzDataError.self) { _ = try await e.projectsView(h) }
    }
}
