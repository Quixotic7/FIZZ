// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// IMPORT's Digitone .syx dumps (FizzData SyxImport) and a whole Digitone pack: a .syx that is not Elektron SysEx
// refused with its own chip, one that is refused NOT READ YET until the bundle reads .syx; the Haunted Hearts pack's
// 128 .dnsnd and its one 128-sound .syx, read from this Mac (never committed or bundled: the tests are skipped
// without it; FIZZ_DIGITONE_PACK names another copy of its folder).

import Foundation
import Testing
import FizzData
import FizzKit
@testable import FizzUI

private enum Pack {
    static let dir = URL(fileURLWithPath: ProcessInfo.processInfo.environment["FIZZ_DIGITONE_PACK"]
        ?? "/Volumes/Q7Media-2025/Projects/Github/ChoralRootFM1/Presets/Digitone/haunted-hearts-acbift/Haunted-Hearts")
    static let syx = dir.appendingPathComponent("SysEx/Haunted Hearts Digitone Soundpack.syx")
    static let sounds = dir.appendingPathComponent("Sounds")
    static var here: Bool { FileManager.default.fileExists(atPath: syx.path) }

    static func dnsnd() throws -> [(name: String, bytes: [UInt8])] {
        try FileManager.default.contentsOfDirectory(atPath: sounds.path).filter { $0.lowercased().hasSuffix(".dnsnd") }.sorted()
            .map { ($0, [UInt8](try Data(contentsOf: sounds.appendingPathComponent($0)))) }
    }
}

@MainActor
@Suite struct SyxImportTests {
    /// Not Elektron SysEx (garbage, another maker's dump): NOT A DIGITONE DUMP; Elektron SysEx with this bundle (no
    /// .syx reader yet): NOT READ YET. Either way a row, in the files' order, not a NOT SOUNDS file; nothing ticked.
    @Test func refusals() async throws {
        let im = ImportModel()
        let dx7: [UInt8] = [0xF0, 0x43, 0x00, 0x09, 0x20, 0x00] + [UInt8](repeating: 0x10, count: 32) + [0xF7]
        let elektron: [UInt8] = [0xF0, 0x00, 0x20, 0x3C, 0x0D, 0x00, 0x53, 0x01, 0x01, 0x00, 0xF7]
        let files: [(name: String, bytes: [UInt8])] = [("garbage.syx", [1, 2, 3, 4]), ("DX7 VOICES.SYX", dx7),
                                                       ("empty.syx", []), ("digitone.syx", elektron)]
        await im.add(files: files)
        #expect(im.refused.isEmpty)
        #expect(im.rows.map(\.item.file) == files.map(\.name))
        #expect(im.rows.allSatisfy { !$0.item.ok && !$0.on && $0.item.sound == nil && $0.item.kind == .dn1 })
        #expect(im.formats == ["syx"])
        let chips = im.rows.map { ImportRowView.reason($0.item.error) }
        #expect(Array(chips.prefix(3)) == Array(repeating: "NOT A DIGITONE DUMP", count: 3))
        let reads = try await FizzDataShared.engine().readsSyx()
        if !reads { #expect(chips.last == "NOT READ YET" && im.rows.last?.item.error == SyxImport.notReadYet) }
        #expect(im.ticked.isEmpty)
    }

    /// A refused .syx among other files: the others read as before, in order.
    @Test func syxAmongOthers() async throws {
        let im = ImportModel()
        var files = try await DemoImports.files()
        files.insert(("other synth.syx", [0xF0, 0x41, 0x10, 0xF7]), at: 1)
        await im.add(files: files)
        #expect(im.rows.count == 13 && im.rows[1].item.file == "other synth.syx" && !im.rows[1].item.ok)
        #expect(im.rows.filter(\.item.ok).count == 12 && im.ticked.count == 12)
        #expect(im.formats == ["dnsnd", "dn2pst", "dn2snd", "fizzlib", "syx"])
    }

    /// The pack's 128 .dnsnd: every one read, all ticked (ALL), each with a name. The ones not read are listed, with
    /// what the reader said.
    @Test(.enabled(if: Pack.here)) func packDnsnd() async throws {
        let files = try Pack.dnsnd()
        #expect(files.count == 128)
        let im = ImportModel()
        let t0 = Date()
        await im.add(files: files)
        let took = Date().timeIntervalSince(t0)
        let bad = im.rows.filter { !$0.item.ok }
        print("Haunted Hearts .dnsnd: \(im.rows.count - bad.count)/\(files.count) read in \(String(format: "%.1f", took)) s")
        for r in bad { print("  \(r.item.file): \(r.item.error)") }
        let marks = Dictionary(grouping: im.rows.compactMap { $0.item.ok ? $0.item.mark : nil }, by: { $0 }).mapValues(\.count)
        print("  marks: \(marks)")
        #expect(im.rows.count == 128)
        #expect(bad.isEmpty, "\(bad.map { "\($0.item.file): \($0.item.error)" })")
        #expect(im.rows.allSatisfy { !$0.item.ok || ($0.item.sound?.name.isEmpty == false) })
        #expect(im.ticked.count == im.rows.count - bad.count - im.rows.filter { !$0.dup.isEmpty }.count)
    }

    /// The pack's one .syx: refused NOT READ YET while the bundle has no .syx reader; then its 128 sounds, their names
    /// those of the .dnsnd.
    /// Flip on when the bundle is rebuilt with fm1dnsnd.js's reader: delete the `withKnownIssue { }` wrapper (with the
    /// reader in, the wrapper itself fails the test: "Known issue was not recorded").
    @Test(.enabled(if: Pack.here)) func packSyx() async throws {
        let bytes = [UInt8](try Data(contentsOf: Pack.syx))
        let im = ImportModel()
        await im.add(files: [(Pack.syx.lastPathComponent, bytes)])
        #expect(im.refused.isEmpty && !im.rows.isEmpty)
        if try await !FizzDataShared.engine().readsSyx() {
            #expect(im.rows.count == 1 && im.rows[0].item.error == SyxImport.notReadYet)
        }
        let names = try await {
            let d = ImportModel()
            await d.add(files: try Pack.dnsnd())
            return Set(d.rows.compactMap { $0.item.sound?.name })
        }()
        withKnownIssue("the bundle has no Digitone .syx reader yet (fm1dnsnd.js)") {
            #expect(im.rows.count == 128)
            #expect(im.rows.allSatisfy { $0.item.ok })
            #expect(im.ticked.count == 128)
            #expect(Set(im.rows.compactMap { $0.item.sound?.name }) == names)
        }
    }
}
