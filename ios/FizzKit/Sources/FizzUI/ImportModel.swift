// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// IMPORT (importer.js mountImport): files read and converted by the Manager's JS (FizzData importRead: .fizzlib
// .fizzsnd .fizzbak, Digitone .dnsnd, Digitone II .dn2pst / .dn2snd), each sound a row with its fidelity (10 less
// what did not carry over), its notes, its duplicate mark (already in MY PRESETS, or twice in these files: not
// ticked); a row heard on the track (TRACK on); SEND stores the ticked ones into MY PRESETS (POOL: the pool too;
// TRACK: the first onto that track) through UnitActions.send, stopping at FULL.

import Foundation
import FizzData
import FizzKit
import Observation

@MainActor @Observable
public final class ImportModel {
    /// SEND's target (importer.js I.dest).
    public enum Target: String, Sendable, CaseIterable { case mine, pool, track }

    /// What came of a row sent.
    public enum Result: Sendable, Equatable {
        case sent(Int)                  // MY PRESETS' slot (or the pool's place)
        case failed(String)             // FULL, BUSY, ..
    }

    public struct Row: Identifiable, Sendable, Equatable {
        public let id: Int
        public var item: FizzImportItem
        /// "mine" (already in MY PRESETS), "list" (earlier in these files), ""
        public var dup: String
        /// ticked for SEND
        public var on: Bool
        public var result: Result?
        /// importer.js fid: 10 segments less one a note (3 at least)
        public var fidelity: Int { item.notes.isEmpty ? 10 : max(3, 10 - item.notes.count) }
    }

    /// The tab under the sheet (the phone: IMPORT is a sheet there, a tab on the iPad).
    public var under: FizzTab = .backup
    public private(set) var rows: [Row] = []
    public var target: Target = .mine
    /// TRACK's track, 0..5
    public var track = 0
    public private(set) var reading = false
    public private(set) var sending = false
    /// The row heard on the track (its id), while the KEEP / CANCEL is pending.
    public private(set) var heard: Int?
    /// A file that was not a sound file (its name).
    public private(set) var refused: [String] = []

    @ObservationIgnored weak var unit: UnitModel?
    @ObservationIgnored private var nextID = 0

    init() {}

    // MARK: what the sheet shows

    /// The extensions read (the file tabs lit).
    public var formats: Set<String> {
        Set(rows.map { r -> String in
            let f = r.item.file.lowercased()
            let member = f.range(of: ".fizzlib:") != nil ? "fizzlib" : nil
            return member ?? (f as NSString).pathExtension
        })
    }

    public var ticked: [Row] { rows.filter { $0.on && $0.item.ok } }

    /// MY PRESETS' free places for the ticked (importer.js render: "only n places free").
    public var free: Int {
        guard let u = unit else { return 0 }
        let nlib = u.status.flatMap { $0.nlib > 0 ? $0.nlib : nil } ?? 256
        return max(0, nlib - u.mine.count)
    }

    /// TRACK only with the TRACK switch on (P2-B: off, the tracks are left alone).
    public var trackOn: Bool { unit?.actions.trackOn ?? false }

    public var canSend: Bool {
        guard let u = unit, u.unit?.version != nil, !sending, !ticked.isEmpty else { return false }
        return u.actions.canTransfer
    }

    // MARK: files in

    /// Files from the picker, Open in FIZZ, AirDrop or a drop: read under their security scope, then added.
    public func add(urls: [URL]) async {
        var files: [(name: String, bytes: [UInt8])] = []
        for url in urls {
            let scoped = url.startAccessingSecurityScopedResource()
            defer { if scoped { url.stopAccessingSecurityScopedResource() } }
            var data: Data?
            var err: NSError?
            NSFileCoordinator().coordinate(readingItemAt: url, options: .withoutChanges, error: &err) { u in data = try? Data(contentsOf: u) }
            if let data { files.append((url.lastPathComponent, [UInt8](data))) } else { unit?.say("import \(url.lastPathComponent): \(err?.localizedDescription ?? "not read")") }
        }
        await add(files: files)
    }

    /// importer.js add: sound files read (others refused by name), the duplicates marked against MY PRESETS and the
    /// rows not sent yet; the new ones ticked unless a duplicate.
    public func add(files: [(name: String, bytes: [UInt8])]) async {
        let ok = files.filter { importExtensions.contains(($0.name as NSString).pathExtension.lowercased()) }
        refused = files.filter { f in !ok.contains { $0.name == f.name } }.map(\.name)
        guard !ok.isEmpty, let e = try? FizzDataShared.engine() else { return }
        reading = true
        defer { reading = false }
        do {
            let got = try await e.importRead(ok)
            var fresh: [Row] = []
            for it in got {
                fresh.append(Row(id: nextID, item: it, dup: "", on: false, result: nil))
                nextID += 1
            }
            rows += fresh
            await markDuplicates()
            let ids = Set(fresh.map(\.id))
            for k in rows.indices where ids.contains(rows[k].id) { rows[k].on = rows[k].item.ok && rows[k].dup.isEmpty }
            unit?.say("import: \(got.count) read (\(got.filter { !$0.ok }.count) refused, \(fresh.filter { !$0.dup.isEmpty }.count) already there)")
        } catch {
            unit?.say("import: \(error)")
        }
    }

    /// markDuplicates over the rows not sent, against MY PRESETS' DUMPs.
    private func markDuplicates() async {
        guard let e = try? FizzDataShared.engine() else { return }
        if let u = unit, u.link != nil, !u.dumpsLoaded { await u.reloadDumps() }
        let mine = (unit?.actions.frames ?? [:]).filter { $0.key.hasPrefix("mine:") }.sorted { $0.key < $1.key }.map(\.value)
        let open = rows.indices.filter { rows[$0].item.ok && rows[$0].result.map { if case .sent = $0 { false } else { true } } ?? true }
        let marks = (try? await e.importDuplicates(open.compactMap { rows[$0].item.sound }, mine: mine)) ?? []
        for (j, k) in open.enumerated() where j < marks.count { rows[k].dup = marks[j] }
    }

    // MARK: the rows

    /// A row ticked or not (not while sending).
    public func toggle(_ id: Int) {
        guard !sending, let k = rows.firstIndex(where: { $0.id == id }), rows[k].item.ok else { return }
        rows[k].on.toggle()
    }

    public func clear() {
        guard !sending else { return }
        rows = []
        refused = []
        heard = nil
    }

    /// TRACK picked, or picked again: the next track.
    public func pick(_ t: Target) {
        guard t != .track || trackOn else { return }
        if t == .track && target == .track { track = (track + 1) % 6 } else { target = t }
    }

    // MARK: heard on the track

    /// ▶: the row's sound on the track (TRACK on), nothing stored; ■ (the row heard): the track's own sound back.
    public func play(_ id: Int) async {
        guard let a = unit?.actions else { return }
        if heard == id && a.pending != nil {
            if await a.cancelAudition() { heard = nil }
            return
        }
        guard let s = rows.first(where: { $0.id == id })?.item.sound else { return }
        let o = await a.pickSound(s)
        heard = o.isOK ? id : nil
    }

    /// The row still heard (the KEEP / CANCEL bar not answered yet).
    public func isHeard(_ id: Int) -> Bool { heard == id && unit?.actions.pending != nil }

    // MARK: SEND

    /// ↓ SEND n TO FM-1: the ticked rows stored in order (the first onto the track for TRACK), each row's result;
    /// FULL stops the rest.
    public func send() async {
        guard canSend, let a = unit?.actions else { return }
        let go = ticked
        let dest: SendDestination = target == .pool ? .pool : target == .track && trackOn ? .track(track) : .mine
        sending = true
        let out = await a.send(go.compactMap(\.item.sound), to: dest)
        sending = false
        for (j, r) in go.enumerated() {
            guard let k = rows.firstIndex(where: { $0.id == r.id }) else { continue }
            guard j < out.count else { break }
            switch out[j] {
            case .placed(let slot):
                rows[k].result = .sent(slot)
                rows[k].on = false
            case .refused(.full): rows[k].result = .failed("FULL")
            case .refused(let x): rows[k].result = .failed(x.name)
            case .noAnswer: rows[k].result = .failed("NO ANSWER")
            default: rows[k].result = .failed("NOT NOW")
            }
        }
    }

    /// The demo unit: IMPORT opens on demo files (once).
    func loadDemo() async {
        guard !demoShown, rows.isEmpty, unit?.source is DemoLinkSource else { return }
        demoShown = true
        if let f = try? await DemoImports.files() { await add(files: f) }
    }
    @ObservationIgnored private var demoShown = false

    /// The count SEND is on (sending: the one going).
    public var sendProgress: (done: Int, of: Int)? { sending ? unit?.actions.job : nil }
}
