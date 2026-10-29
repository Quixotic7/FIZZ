// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// The web Manager's parity items (branch ios-parity-a): LIBRARY's search by name (manager.js l-q, presets.js nameOk;
// blind to accents too), across MY PRESETS, FACTORY and KEPT; the seven tag icons (ui.js TAG_ICON) and the arcs the
// SVG reader needed for PAD's cloud; IMPORT's EDIT on the demo unit (a file's sound in EDIT, KEEP onto the phone).

import Foundation
import SwiftUI
import Testing
import FizzData
import FizzKit
@testable import FizzUI

private func row(_ src: String, _ slot: Int, _ name: String, tags: Int = 0, id: Int? = nil) -> FizzListEntry {
    FizzListEntry(source: src, slot: slot, id: id ?? (src == "factory" ? slot : 0x100 + slot), name: name, tags: tags,
                  imported: false, inPool: false)
}

@MainActor
private func waitFor(seconds: Double = 30, _ ok: () -> Bool) async -> Bool {
    let end = Date().addingTimeInterval(seconds)
    while Date() < end {
        if ok() { return true }
        try? await Task.sleep(for: .milliseconds(20))
    }
    return ok()
}

@Suite("Web parity: search, tag icons, IMPORT's EDIT", .serialized) @MainActor
struct ParityTests {
    // MARK: 1. the search

    @Test func searchMatchesLikeTheWeb() {
        // presets.js nameOk: a part of the name, any case, the query trimmed; empty: everything
        #expect(NameSearch.matches("SUB DROP", ""))
        #expect(NameSearch.matches("SUB DROP", "   "))
        #expect(NameSearch.matches("SUB DROP", "drop"))
        #expect(NameSearch.matches("SUB DROP", " B D "))
        #expect(NameSearch.matches("SUB DROP", "Sub dr"))
        #expect(!NameSearch.matches("SUB DROP", "drops"))
        #expect(!NameSearch.matches("SUB DROP", "subdrop"))
        // (and blind to accents, both ways)
        #expect(NameSearch.matches("CAFÉ PAD", "cafe"))
        #expect(NameSearch.matches("CAFE PAD", "café"))
        #expect(NameSearch.matches("Ångström", "ANGSTROM"))
    }

    @Test func searchNarrowsMyPresetsAndFactory() {
        let rows = [row("mine", 1, "SUB DROP", tags: 1), row("mine", 2, "GLASS BELL", tags: 4), row("mine", 3, "DEEP BASS", tags: 1),
                    row("factory", 1, "FAT BASS", tags: 1), row("factory", 2, "BELL TREE", tags: 4), row("pool", 1, "SUB DROP", id: 0x101)]
        var f = LibraryFilter()
        f.source = .all
        f.query = "bass"
        #expect(f.shown(rows).mine.map(\.slot) == [3] && f.shown(rows).factory.map(\.slot) == [1])
        f.query = "BELL"
        #expect(f.shown(rows).mine.map(\.slot) == [2] && f.shown(rows).factory.map(\.slot) == [2])
        // with the other filters: all must pass
        f.toggle(.bass)
        #expect(f.shown(rows).mine.isEmpty && f.shown(rows).factory.isEmpty)
        f.toggle(.bass)
        f.turnPool()            // IN POOL: SUB DROP only, and it is not a BELL
        #expect(f.shown(rows).mine.isEmpty)
        f.query = "sub"
        #expect(f.shown(rows).mine.map(\.slot) == [1])
        // the section changed, the search stays (it is the filter's)
        f.pool = .any
        f.source = .factory
        f.query = "bell"
        #expect(f.shown(rows).mine.isEmpty && f.shown(rows).factory.map(\.slot) == [2])
        // cleared: everything back
        f.query = ""
        #expect(f.shown(rows).factory.count == 2)
    }

    @Test func searchNarrowsKept() throws {
        let dir = FileManager.default.temporaryDirectory.appendingPathComponent("fizz-search-\(UUID().uuidString)")
        let items = ["GLASS BELL", "NIGHT GLASS", "SUB DROP 2"].map { n in
            KeptPreset(id: "\(n).fizzsnd", url: dir.appendingPathComponent("\(n).fizzsnd"),
                       sound: FizzSound(name: n, blob: [UInt8](repeating: 0, count: 128), tags: [n == "SUB DROP 2" ? "BASS" : "PLUCK"]),
                       webTags: [], size: 0, modified: Date())
        }
        #expect(keptShown(items, tags: []).count == 3)
        #expect(keptShown(items, tags: [], query: "glass").map(\.name) == ["GLASS BELL", "NIGHT GLASS"])
        #expect(keptShown(items, tags: [.bass], query: "glass").isEmpty)
        #expect(keptShown(items, tags: [.pluck], query: " bell ").map(\.name) == ["GLASS BELL"])
        #expect(keptShown(items, tags: [], query: "zebra").isEmpty)
    }

    // MARK: 2. the tag icons

    /// ui.js TAG_ICON, read from the web Manager in this checkout.
    private func webIcons() throws -> [String: String] {
        let ui = URL(fileURLWithPath: #filePath).deletingLastPathComponent().appendingPathComponent("../../../../site/manager/ui.js")
        let src = try String(contentsOf: ui, encoding: .utf8)
        let start = try #require(src.range(of: "export const TAG_ICON = {"))
        let end = try #require(src.range(of: "};", range: start.upperBound..<src.endIndex))
        var out: [String: String] = [:]
        for line in src[start.upperBound..<end.lowerBound].split(separator: "\n") {
            let t = line.trimmingCharacters(in: .whitespaces)
            guard let colon = t.firstIndex(of: ":"), let q1 = t.firstIndex(of: "'"), let q2 = t.lastIndex(of: "'"), q1 < q2 else { continue }
            out[String(t[..<colon])] = String(t[t.index(after: q1)..<q2])
        }
        return out
    }

    /// One element as ui.js writes it.
    private func svg(_ e: VectorArt.El) -> String {
        func n(_ v: CGFloat) -> String { v == v.rounded() ? String(Int(v)) : String(format: "%g", Double(v)) }
        switch e {
        case .path(let d): return "<path d=\"\(d)\"/>"
        case .rect(let x, let y, let w, let h, let r): return "<rect x=\"\(n(x))\" y=\"\(n(y))\" width=\"\(n(w))\" height=\"\(n(h))\" rx=\"\(n(r))\"/>"
        case .circle(let x, let y, let r), .filledCircle(let x, let y, let r): return "<circle cx=\"\(n(x))\" cy=\"\(n(y))\" r=\"\(n(r))\"/>"
        case .ellipse(let x, let y, let rx, let ry): return "<ellipse cx=\"\(n(x))\" cy=\"\(n(y))\" rx=\"\(n(rx))\" ry=\"\(n(ry))\"/>"
        }
    }

    @Test func tagIconsAreTheWebsElementForElement() throws {
        let web = try webIcons()
        #expect(web.count == 7)
        for t in FizzTag.allCases {
            #expect(t.art.box == CGSize(width: 24, height: 24))
            #expect(t.art.els.map(svg).joined() == web[t.name], "\(t.name)")
        }
    }

    @Test func aPresetShowsItsFirstTagsIcon() {
        #expect(FizzTag.icon(mask: 0) == nil)                    // (untagged: the tile keeps the algorithm's glyph)
        #expect(FizzTag.icon(mask: 1) == .bass)
        #expect(FizzTag.icon(mask: 0b0001000) == .pad)
        #expect(FizzTag.icon(mask: 0b1100100) == .pluck)          // (PLUCK, DRUMS and FX: the first, as ui.js tagsOf()[0])
        #expect(FizzTag.icon(mask: 0b1000000) == .fx)
        #expect(FizzTag.icon(mask: 0x80) == nil)                  // (no eighth tag)
        // each icon draws inside its 24 x 24 box
        for t in FizzTag.allCases {
            let b = t.art.els.map { t.art.path($0).boundingRect }.reduce(CGRect.null) { $0.union($1) }
            #expect(b.minX >= 1.9 && b.minY >= 2.9 && b.maxX <= 22.1 && b.maxY <= 21.1, "\(t.name) \(b)")
        }
    }

    @Test func svgArcs() {
        // a half circle over the top (y down: sweep 1 turns clockwise on screen)
        let top = SVGPath.parse("M0 10A5 5 0 0 1 10 10").boundingRect
        #expect(abs(top.minY - 5) < 0.01 && abs(top.maxY - 10) < 0.01 && abs(top.width - 10) < 0.01)
        let under = SVGPath.parse("M0 10a5 5 0 0 0 10 0").boundingRect
        #expect(abs(under.minY - 10) < 0.01 && abs(under.maxY - 15) < 0.01)
        // a radius too small grows to fit (the spec's scaling); the end point is reached
        let p = SVGPath.parse("M0 0A1 1 0 0 1 10 0L10 0")
        #expect(abs(p.boundingRect.width - 10) < 0.01 && abs(p.boundingRect.minY + 5) < 0.01)
        // PAD's cloud: its big arc (r 5.5 from 17.4,10 to 6.8,10.6, the short way over the top) centred about 12.1,11.7,
        // so its top at y 6.24; the small arcs reach x 3.2 and 21.2; the floor at y 18
        let cloud = FizzTag.pad.art.path(FizzTag.pad.art.els[0]).boundingRect
        #expect(abs(cloud.minY - 6.24) < 0.02 && abs(cloud.maxY - 18) < 0.01 && abs(cloud.minX - 3.2) < 0.02
                && abs(cloud.maxX - 21.2) < 0.02, "\(cloud)")
    }

    // MARK: 3. IMPORT's EDIT (the demo unit)

    @Test func importRowOpensItsSoundInEditAndKeepsIt() async throws {
        let d = FileManager.default.temporaryDirectory.appendingPathComponent("fizz-parity-\(UUID().uuidString)")
        let m = UnitModel(source: DemoLinkSource(.fizz, bulkMs: 0...0.3), backups: d.appendingPathComponent("Backups"),
                          presets: d.appendingPathComponent("Presets"))
        m.start()
        #expect(await waitFor { m.dumpsLoaded && m.layout != nil && m.actions.trackCapable })
        await m.kept.reload()
        let r = try #require(m.mine.first)
        var s = try #require(await m.actions.sound(of: r))
        s.name = "FILE SOUND"
        let bytes = try await FizzDataShared.engine().keptFile(s)
        await m.importer.add(files: [(name: "file sound.fizzsnd", bytes: bytes)])
        let ir = try #require(m.importer.rows.first)
        #expect(ir.item.ok && ir.item.sound?.name == "FILE SOUND")

        // EDIT on the row: EDIT asked for, the sound as a file's
        m.importer.edit(ir.id)
        #expect(m.goTo == .edit)
        guard case .sound(let fs, let label) = m.editTarget else { Issue.record("not a sound"); return }
        #expect(fs.name == "FILE SOUND" && label == "file sound.fizzsnd")
        let ed = m.editor
        await ed.show()
        #expect(ed.place.isFile && ed.place.label == "FILE SOUND.FIZZSND" && ed.sound?.name == "FILE SOUND")
        #expect(ed.canKeep && !ed.canSave && ed.canSaveNew)

        // KEEP: onto the phone once; again: the same copy written again
        let v = ed.value(6) == 40 ? 41 : 40
        #expect(ed.set(6, v))
        #expect(await ed.keep() == .ok)
        let k = try #require(m.kept.items.first { $0.name == "FILE SOUND" })
        #expect(ed.place.isFile && ed.place.kept == k.id && !ed.dirty)
        #expect(ed.set(6, v == 40 ? 42 : 40))
        #expect(await ed.keep() == .ok)
        #expect(m.kept.items.filter { $0.name.hasPrefix("FILE SOUND") }.count == 1)
        let again = try #require(await m.kept.full(k))
        let back = try await FizzDataShared.engine().editSound(lib: again.sound)
        #expect(back.values[6] == ed.value(6))
        ed.hide()
        m.stop()
    }
}
