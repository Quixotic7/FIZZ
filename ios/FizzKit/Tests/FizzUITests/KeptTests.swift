// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// KEPT, the phone's presets: the store (add, rename, tags, duplicate, delete; files appearing and vanishing on disk;
// names unique; the index), → KEPT from the demo unit, SEND n TO the demo unit, EDIT's SAVE / SAVE AS NEW on a kept
// preset, IMPORT's KEPT target.

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
    let u = FileManager.default.temporaryDirectory.appendingPathComponent("fizz-kept-\(UUID().uuidString)", isDirectory: true)
    try? FileManager.default.createDirectory(at: u, withIntermediateDirectories: true)
    return u
}

private func sound(_ name: String, _ seed: Int, tags: [String] = ["LEAD"]) -> FizzSound {
    var b = [UInt8](repeating: 0, count: 128)
    b[0] = 0x51; b[1] = 6; b[2] = UInt8(seed % 8)
    for i in 3..<124 { b[i] = UInt8((i * 7 + seed * 13) & 0x7f) }
    b[126] = 20; b[127] = 64
    return FizzSound(name: name, blob: b, drive: 3, pan: -4, sends: FizzSends(chorus: 5, delay: 6, reverb: 7), tags: tags)
}

@MainActor
private func connected() async -> UnitModel {
    let m = UnitModel(source: DemoLinkSource(.fizz, bulkMs: 0...0.3), backups: tempDir(), presets: tempDir())
    m.start()
    _ = await waitFor { m.dumpsLoaded && m.layout != nil && m.actions.trackCapable }
    return m
}

@Suite("KEPT", .serialized) @MainActor
struct KeptTests {
    @Test func addRenameTagsDuplicateDelete() async throws {
        let dir = tempDir()
        let k = KeptPresets(directory: dir)
        await k.reload()
        #expect(k.items.isEmpty && k.loaded)

        let a = try await k.add(sound("GLASS BELL", 1, tags: ["PLUCK"]), webTags: ["SHIMMER"])
        #expect(a.id == "GLASS BELL.fizzsnd" && a.webTags == ["SHIMMER"] && a.tagMask == 1 << FizzTag.pluck.rawValue)
        // the same name again: another file, both kept
        let b = try await k.add(sound("GLASS BELL", 2))
        #expect(b.id == "GLASS BELL 2.fizzsnd" && k.items.count == 2)
        #expect(k.same(as: sound("GLASS BELL", 2))?.id == b.id && k.same(as: sound("GLASS BELL", 3)) == nil)

        // RENAME: the name in the file and the file's name
        let r = try await k.rename(b, to: "sub drop")
        #expect(r.name == "SUB DROP" && r.id == "SUB DROP.fizzsnd")
        #expect(!FileManager.default.fileExists(atPath: b.url.path) && FileManager.default.fileExists(atPath: r.url.path))
        // FIZZ's tags and the web tags written in the file
        let t = try await k.toggle(r, tag: .bass)
        #expect(t.sound.tags == ["BASS", "LEAD"])
        let w = try await k.addWebTag(t, " warm  pad ")
        #expect(w.webTags == ["WARM PAD"])
        let read = try #require(await k.full(w))
        #expect(read.webTags == ["WARM PAD"] && read.sound.tags == ["BASS", "LEAD"] && read.sound.blob == sound("x", 2).blob)

        // DUPLICATE: a name not kept yet
        let d1 = try await k.duplicate(w), d2 = try await k.duplicate(d1)
        #expect(d1.name == "SUB DROP 2" && d2.name == "SUB DROP 3" && d2.webTags == ["WARM PAD"])
        #expect(k.uniqueName("GLASS BELL") == "GLASS BELL 2" && k.uniqueName("ABCDEFGHIJKL") == "ABCDEFGHIJKL")
        _ = try await k.add(sound("ABCDEFGHIJKL", 4))
        #expect(k.uniqueName("ABCDEFGHIJKL") == "ABCDEFGHIJ 2")

        // by name
        #expect(k.items.map(\.name) == ["ABCDEFGHIJKL", "GLASS BELL", "SUB DROP", "SUB DROP 2", "SUB DROP 3"])
        k.delete([d1.id, d2.id])
        #expect(k.items.count == 3 && !FileManager.default.fileExists(atPath: d1.url.path))

        // SHARE: a .fizzlib IMPORT reads
        let lib = try await k.library(k.items)
        let items = try await FizzDataShared.engine().importRead([(lib.lastPathComponent, [UInt8](Data(contentsOf: lib)))])
        #expect(items.compactMap { $0.sound?.name } == k.items.map(\.name))
    }

    @Test func filesAppearingAndVanishing() async throws {
        let dir = tempDir(), index = tempDir().appendingPathComponent("index.json")
        let k = KeptPresets(directory: dir, index: index)
        await k.reload()
        k.watch()
        defer { k.unwatch() }
        // a file dropped in from outside (Files, AirDrop): the web Manager's own .fizzsnd
        let e = try FizzDataShared.engine()
        let f = try await e.exportFile([sound("SODA LEAD", 5)], kind: .fizz)
        try Data(f.bytes).write(to: dir.appendingPathComponent(f.name))
        try Data("not a sound".utf8).write(to: dir.appendingPathComponent("junk.fizzsnd"))
        #expect(await waitFor(seconds: 5) { k.items.map(\.name) == ["SODA LEAD"] }, "seen without a reload")
        // the index keeps the cards: a store made again reads them without the files changing
        #expect(FileManager.default.fileExists(atPath: index.path))
        let again = KeptPresets(directory: dir, index: index)
        await again.reload()
        #expect(again.items.map(\.name) == ["SODA LEAD"] && again.items.first?.sound.blob == sound("x", 5).blob)
        // removed in Files
        try FileManager.default.removeItem(at: dir.appendingPathComponent(f.name))
        #expect(await waitFor(seconds: 5) { k.items.isEmpty }, "gone without a reload")
        // a file changed outside: read again
        try Data(try await e.keptFile(sound("NIGHT GLASS", 6))).write(to: dir.appendingPathComponent("x.fizzsnd"))
        await again.reload()
        try Data(try await e.keptFile(sound("NIGHT GLASS", 7), webTags: ["DARK"])).write(to: dir.appendingPathComponent("x.fizzsnd"))
        await again.reload()
        #expect(again.items.first?.webTags == ["DARK"] && again.items.first?.sound.blob == sound("x", 7).blob)
    }

    @Test func openFromOutside() async throws {
        let k = KeptPresets(directory: tempDir())
        await k.reload()
        let e = try FizzDataShared.engine()
        let out = tempDir().appendingPathComponent("RAIN FX.fizzsnd")
        try Data(try await e.keptFile(sound("RAIN FX", 3, tags: ["FX"]), webTags: ["WET"])).write(to: out)
        let a = try await k.open(out)
        #expect(a.name == "RAIN FX" && a.webTags == ["WET"] && k.items.count == 1)
        // the same sound again: not kept twice
        let b = try await k.open(out)
        #expect(b.id == a.id && k.items.count == 1)
        let bad = tempDir().appendingPathComponent("bad.fizzsnd")
        try Data("{}".utf8).write(to: bad)
        await #expect(throws: (any Error).self) { _ = try await k.open(bad) }
        #expect(k.items.count == 1)
    }

    @Test func unitToKeptAndBack() async throws {
        let m = await connected()
        await m.kept.reload()
        let r = try #require(m.mine.first)
        // a web tag on the unit's preset goes with it
        let key = try #require(await m.actions.soundKey(of: r))
        m.webTags.add("GRIT", to: key)
        guard case .kept(let k) = await m.keep(r) else { Issue.record("not kept"); return }
        #expect(k.name == r.name && k.webTags == ["GRIT"] && k.tagMask == r.tags)
        let full = try #require(await m.kept.full(k))
        #expect(full.sound.source["from"]?.string == "MY PRESETS \(r.slot)")
        guard case .already(let same) = await m.keep(r) else { Issue.record("kept twice"); return }
        #expect(same.id == k.id && m.kept.items.count == 1)
        if let f = m.factory.first { if case .kept = await m.keep(f) {} else { Issue.record("factory not kept") } }

        // SEND 2 TO FM-1: MY PRESETS, then the pool
        let a = try await m.kept.add(sound("KEPT ONE", 11), webTags: ["AIRY"])
        let b = try await m.kept.add(sound("KEPT TWO", 12))
        let n = m.mine.count
        let out = await m.send(kept: [a, b], to: .mine)
        #expect(out.count == 2 && out.allSatisfy(\.isOK))
        #expect(m.mine.count == n + 2 && m.mine.contains { $0.name == "KEPT ONE" } && m.mine.contains { $0.name == "KEPT TWO" })
        let sent = try #require(m.mine.first { $0.name == "KEPT ONE" })
        #expect(m.webTags.of(await m.actions.soundKey(of: sent)) == ["AIRY"], "its web tags joined the unit's")
        #expect(keptSendNote(out, to: .mine, unit: m.unitName).text == "2 → FM-1 · MY PRESETS")
        let p = m.pool.count
        let pout = await m.send(kept: [b], to: .pool)
        #expect(pout.count == 1 && pout[0].isOK && m.pool.count == p + 1)
        #expect(m.unitName == "FM-1" && !m.unitIsEmulated)
        m.stop()
    }

    @Test func editSaveAndSaveAsNew() async throws {
        let m = await connected()
        await m.kept.reload()
        let k = try await m.kept.add(sound("NIGHT GLASS", 3, tags: ["PAD"]), webTags: ["DARK"])
        let e = m.editor
        await e.open(.kept(k))
        #expect(e.place.source == "kept" && e.place.kept == k.id && e.sound?.name == "NIGHT GLASS")
        #expect(e.canSave && e.canSaveNew)
        let id = 6
        let v = e.value(id)
        #expect(e.set(id, v == 0 ? 5 : 0))
        #expect(e.dirty)
        // SAVE: the file written again (its web tags kept), nothing on the unit
        let n = m.mine.count
        #expect(await e.save(asNew: false) == .ok)
        #expect(!e.dirty && m.mine.count == n && m.kept.items.count == 1)
        let saved = try #require(m.kept.item(k.id))
        let back = try #require(await m.kept.full(saved))
        #expect(back.webTags == ["DARK"] && back.sound.tags == ["PAD"])
        let reopened = try await FizzDataShared.engine().editSound(lib: back.sound)
        #expect(reopened.values[id] == e.value(id))
        // SAVE AS NEW: another kept preset, the editor on it
        e.setName("NIGHT GLASS")
        #expect(await e.save(asNew: true) == .ok)
        // (12 characters at most)
        #expect(m.kept.items.map(\.name) == ["NIGHT GLAS 2", "NIGHT GLASS"])
        #expect(e.place.kept == "NIGHT GLAS 2.fizzsnd" && e.sound?.name == "NIGHT GLAS 2")
        // + NEW: INIT, saved to KEPT
        await e.open(.newKept)
        #expect(e.place.source == "kept" && e.place.kept == nil && e.sound?.name == "INIT" && e.canSave)
        #expect(await e.save(asNew: false) == .ok)
        #expect(m.kept.items.contains { $0.name == "INIT" } && e.place.kept == "INIT.fizzsnd")
        m.stop()
    }

    @Test func editKeptLiveOnTheTrack() async throws {
        let m = await connected()
        await m.kept.reload()
        let k = try await m.kept.add(sound("TAPE KEYS", 5, tags: ["KEYS"]))
        await m.actions.setTrackOn(true)
        let e = m.editor
        await e.open(.kept(k))
        #expect(e.following && e.live, "heard on the track")
        #expect(e.place.source == "kept" && e.place.kept == k.id && e.sound?.name == "TAPE KEYS")
        #expect(m.actions.pending != nil, "KEEP / CANCEL")
        await m.actions.keepAudition()
        m.stop()
    }

    @Test func importToKept() async throws {
        let m = await connected()
        await m.kept.reload()
        let im = m.importer
        let e = try FizzDataShared.engine()
        let lib = try await e.exportFile([sound("ORBIT LEAD", 1), sound("DUST FX", 2, tags: ["FX"])], kind: .fizz)
        await im.add(files: [(lib.name, lib.bytes)])
        #expect(im.ticked.count == 2)
        im.pick(.kept)
        #expect(im.target == .kept && im.canSend)
        let n = m.mine.count
        await im.send()
        #expect(m.kept.items.map(\.name) == ["DUST FX", "ORBIT LEAD"] && m.mine.count == n)
        #expect(im.rows.allSatisfy { $0.result == .kept })
        m.stop()
    }
}
