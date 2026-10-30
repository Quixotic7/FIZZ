// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// The PROJECTS tab's state (site/manager/projtab.js mountProjects, its J): the slots and their moves (the FM-1's own
// PROJECT / RENAME, done at once through UnitActions); the store read whole (FLUSH first when the open project has
// something not written and the unit is stopped; FZBR as a backup reads it, not kept) into projects.js's model in the
// engine; the patterns selected, copied, pasted, cleared, a kept project loaded into a slot, each change waiting
// (UNDO, DISCARD) until WRITE TO FM-1 writes the store in one go (UnitActions.writeProjects: the store as read kept
// first, then the restore), and the store read again after. The projects kept on the phone (KeptStore).

import Foundation
import FizzData
import FizzKit
import Observation

/// A short state the screen shows a moment (the page's toast and say, in a word or two and a colour).
public struct ProjectsNote: Sendable, Equatable {
    public var text: String
    public var tint: RGB
    public var id = UUID()
}

/// A question before a change (the page's confirmBox): its title, what it is about, the button, its colour.
public struct ProjectsAsk: Identifiable {
    public let id = UUID()
    public var title: String, what: String, go: String
    public var tint: RGB = FZ.red
    public var run: @MainActor () async -> Void
}

/// COPY TO / MOVE TO / LOAD INTO waiting for its slot.
public struct ProjectsArm: Sendable, Equatable {
    public enum Kind: Sendable { case copy, move, load }
    public var kind: Kind
    /// the slot it comes from (-1: a kept project)
    public var from: Int
    public var name: String
    public var kept: KeptProject?
}

/// WRITE TO FM-1 on its screen: what changes.
public struct ProjectsWrite: Sendable, Equatable {
    /// the projects changed, by name
    public var names: [String]
    /// the store's sectors that differ from the store as read
    public var changed: Set<Int>
    public var sectors: Int, libSectors: Int
    public var started: Date
    public init(names: [String], changed: Set<Int>, sectors: Int, libSectors: Int, started: Date) {
        self.names = names; self.changed = changed; self.sectors = sectors; self.libSectors = libSectors; self.started = started
    }
}

/// One slot as the screens draw it: PROJECTS's entry and the store's model together.
public struct ProjectSlot: Sendable, Equatable, Identifiable {
    public var p: Int
    public var there: Bool, current: Bool, demo: Bool
    public var name: String
    /// patterns used (the model's), else PROJECTS's pages
    public var used: Int
    public var changed: Bool
    public var model: FizzProjectModel?
    public var id: Int { p }
}

@MainActor @Observable
public final class ProjectsModel {
    public enum Mode: Sendable { case slots, patterns, kept }

    /// iPhone: the slots (mock-up 14), a project's patterns (18), the kept ones (19)
    public var mode: Mode = .slots
    /// The store as read, the changes applied (nil: not read yet)
    public internal(set) var view: FizzProjectsView?
    /// The slot looked at (0..7)
    public private(set) var sel = 0
    /// The patterns selected (0..63)
    public internal(set) var pats: Set<Int> = []
    public internal(set) var anchor = -1
    /// What PASTE pastes
    public private(set) var clip: FizzClip?
    public private(set) var reading = false
    public private(set) var readFraction = 0.0
    public var arm: ProjectsArm?
    public var keptSel: String?
    public var ask: ProjectsAsk?
    public private(set) var note: ProjectsNote?
    /// WRITE TO FM-1's screen (nil: not writing)
    public private(set) var writing: ProjectsWrite?
    public let kept: KeptStore
    /// A pattern lifted and dragged (ProjectsDrag.swift; projtab.js D)
    public internal(set) var lift: ProjectsLift?
    /// What dropping the lifted (or TO…'s) pattern does to each project's pool
    public internal(set) var plan: FizzDropPlan?
    /// The TO… sheet (projtab.js SH)
    public internal(set) var to: ProjectsTo?
    /// iPad: the project beside the selected one (nil: the next one there), and KEPT shown in its place
    public internal(set) var beside: Int?
    public var padKept = false
    @ObservationIgnored let zones = ProjectsDropZones()
    @ObservationIgnored var springTask: Task<Void, Never>?
    @ObservationIgnored var springKeys: Task<Void, Never>?

    @ObservationIgnored weak var unit: UnitModel?
    @ObservationIgnored var session: Int?
    @ObservationIgnored private(set) var baseImage: [UInt8]?
    @ObservationIgnored private var baseWritten: Int?
    @ObservationIgnored private var unnote: Task<Void, Never>?

    public init(kept: KeptStore = KeptStore(directory: KeptStore.defaultDirectory)) { self.kept = kept }

    // MARK: state

    /// The unit there, read, nothing else running (projtab.js on()).
    public var online: Bool {
        guard let u = unit else { return false }
        return u.link != nil && u.status != nil && !u.actions.busy && !u.backups.busy && !reading && writing == nil
    }
    public var ops: [FizzProjectsOp] { view?.ops ?? [] }
    /// The store as read (what WRITE keeps first)
    public var storeImage: [UInt8]? { baseImage }
    var list: [FizzProject] { unit?.projects?.list ?? [] }
    public var nproj: Int {
        if let v = view { return v.nproj }
        if !list.isEmpty { return list.count }
        return unit?.status?.nproj ?? 8
    }
    /// Changed on the unit since it was read (STATUS's records written).
    public var stale: Bool { baseImage == nil || unit?.status == nil || unit?.status?.written != baseWritten }
    /// Over Bluetooth: WRITE needs the cable.
    public var viaBle: Bool { unit?.unit?.via == "BT" }
    /// The FM-1 playing (its open project's pattern pulses)
    public var playing: Bool { unit?.status?.playing == true }

    public func slot(_ p: Int) -> ProjectSlot {
        let e = list.indices.contains(p) ? list[p] : nil, m = view?.project(p)
        // (a kept project loaded into it waits: the model's; else PROJECTS's)
        let mine = m?.changed == true
        let there = mine ? m!.there : e?.there ?? m?.there ?? false
        let name = (mine ? m?.name : nil) ?? (e?.there == true ? e?.name : nil) ?? (m?.there == true ? m?.name : nil) ?? ""
        return ProjectSlot(p: p, there: there, current: e?.current ?? false, demo: e?.demo ?? m?.demo ?? false,
                           name: name.isEmpty && there ? "PROJECT \(p + 1)" : name, used: m?.used ?? e?.pages ?? 0,
                           changed: m?.changed ?? false, model: m)
    }
    public var slots: [ProjectSlot] { (0..<nproj).map(slot) }
    public var selected: ProjectSlot { slot(sel) }
    /// Patterns pasted (yellow) and cleared (dashed) in slot p by the changes waiting.
    public func pasted(_ p: Int) -> Set<Int> { Set(ops.filter { $0.q == p && ($0.kind == .paste || $0.kind == .drop) }.flatMap(\.at)) }
    public func cleared(_ p: Int) -> Set<Int> {
        let now = view?.project(p)
        let moved = ops.filter { $0.kind == .drop && $0.move && $0.fromP == p }.map(\.fromK)
        return Set((ops.filter { $0.q == p && $0.kind == .clear }.flatMap(\.ks) + moved).filter { now?.pats[$0] == nil })
    }

    // MARK: showing the tab

    /// The tab shown (projtab.js show): the open project selected the first time, the kept ones read, the store read
    /// when it changed on the unit (not while changes wait).
    public func show() async {
        await kept.reload()
        if view == nil, let cur = list.firstIndex(where: \.current) { sel = cur }
        if online && stale && ops.isEmpty { await read() }
    }

    /// iPhone: back to the slots (the back chevron, a swipe from the edge).
    public func back() { mode = .slots }

    /// The patterns or the kept projects looked at: the store read first when it changed.
    public func enter(_ m: Mode) async {
        mode = m
        if m != .slots, online, stale, ops.isEmpty { await read() }
    }

    /// The whole store read into the model (again: the changes waiting made again on it).
    @discardableResult public func read(force: Bool = false) async -> Bool {
        guard let u = unit, let link = u.link, !reading, writing == nil, u.status != nil, !u.actions.busy, !u.backups.busy
        else { return view != nil }
        guard force || stale else { return view != nil }
        reading = true
        readFraction = 0
        defer { reading = false }
        if let st = u.status, st.dirty && !st.playing { _ = await u.actions.flush() }
        do {
            let r = try await link.backup(depth: 2) { [weak self] f, _ in
                Task { @MainActor [weak self] in if let s = self, s.reading { s.readFraction = max(s.readFraction, f) } }
            }
            let st = await link.status()
            if let st { u.took(status: st) }
            let e = try FizzDataShared.engine()
            if let h = session, !ops.isEmpty {
                let (v, refused) = try await e.projectsRebase(h, image: r.image)
                view = v
                if refused != nil { say("CHANGES DROPPED", FZ.orange) }
            } else {
                if let h = session { await e.projectsClose(h) }
                let (h, v) = try await e.projectsOpen(r.image)
                session = h
                view = v
            }
            baseImage = r.image
            baseWritten = st?.written
            if sel >= nproj { sel = 0 }
            u.say("projects: the store read (\(r.image.count) B)")
            return true
        } catch FizzBackupError.cancelled {
            return false
        } catch {
            u.say("projects: the store could not be read: \(error)")
            say("NOT READ", FZ.orange)
            return false
        }
    }

    // MARK: the slots (the FM-1's own commands)

    /// A slot tapped: the armed move's target, else selected.
    public func tapSlot(_ p: Int) async {
        if arm != nil { await armedTo(p); return }
        if p != sel { sel = p; pats = []; anchor = -1 }
    }

    public func select(_ p: Int) { if p != sel { sel = p; pats = []; anchor = -1 } }

    public func load() async {
        guard let a = unit?.actions, selected.there, !selected.current else { return }
        tell(await a.loadProject(sel + 1), ok: "OPEN", what: "LOAD")
    }

    public func newProject() async {
        guard let a = unit?.actions, !selected.there else { return }
        tell(await a.newProject(sel + 1), ok: "NEW", what: "NEW")
        await afterMove()
    }

    public func rename(to name: String) async {
        guard let a = unit?.actions, selected.there else { return }
        let v = FizzSysEx.cleanName(name)
        guard !v.isEmpty, v != selected.name else { return }
        tell(await a.renameProject(sel + 1, to: v), ok: nil, what: "RENAME")
        await afterMove()
    }

    /// COPY TO / MOVE TO: the slots light up for the target.
    public func startCopy(move: Bool = false) {
        guard selected.there else { return }
        arm = ProjectsArm(kind: move ? .move : .copy, from: sel, name: selected.name)
    }

    public func setArmMove(_ move: Bool) {
        guard var a = arm, a.kind != .load else { return }
        a.kind = move ? .move : .copy
        arm = a
    }

    /// LOAD INTO: a kept project waits for its slot.
    public func startLoad(_ k: KeptProject) {
        arm = ProjectsArm(kind: .load, from: -1, name: k.name, kept: k)
    }

    public func disarm() { arm = nil }

    func armedTo(_ q: Int) async {
        guard let a = arm else { return }
        arm = nil
        if q == a.from || q >= nproj { return }
        let target = slot(q)
        let go: @MainActor () async -> Void = { [weak self] in
            guard let self else { return }
            if a.kind == .load { await self.loadKept(a, into: q); return }
            guard let acts = self.unit?.actions else { return }
            let o = a.kind == .move ? await acts.moveProject(a.from + 1, to: q + 1) : await acts.copyProject(a.from + 1, to: q + 1)
            self.tell(o, ok: a.kind == .move ? "MOVED" : "COPIED", what: a.kind == .move ? "MOVE" : "COPY")
            if o.isOK { self.select(q) }
            await self.afterMove()
        }
        if target.there {
            ask = ProjectsAsk(title: a.kind == .load ? "LOAD INTO \(q + 1)" : a.kind == .move ? "MOVE ONTO \(q + 1)" : "COPY ONTO \(q + 1)",
                              what: target.name, go: "REPLACE", run: go)
        } else {
            await go()
        }
    }

    private func loadKept(_ a: ProjectsArm, into q: Int) async {
        guard let k = a.kept, let h = session, let bytes = kept.bytes(k) else { say("NOT READ", FZ.orange); return }
        do {
            switch try await FizzDataShared.engine().projectsLoadKept(h, q: q, file: bytes, fileName: k.id) {
            case .success(let v):
                view = v
                select(q)
                say(k.name, FZ.champagne)
            case .failure(let r): refuse(r)
            }
        } catch { say("NOT A PROJECT", FZ.red) }
    }

    public func delete() {
        guard selected.there else { return }
        let p = sel, name = selected.name
        ask = ProjectsAsk(title: "DELETE PROJECT", what: name, go: "DELETE") { [weak self] in
            guard let self, let a = self.unit?.actions else { return }
            self.tell(await a.deleteProject(p + 1), ok: "DELETED", what: "DELETE")
            await self.afterMove()
        }
    }

    /// KEEP: the selected project (with the changes waiting) as a .fizzprj on the phone.
    public func keep(date: Date = Date()) async {
        guard selected.there else { return }
        if session == nil || stale && ops.isEmpty { await read() }
        guard let h = session, view?.project(sel)?.there == true else { return }
        do {
            let f = try await FizzDataShared.engine().projectsKeep(h, p: sel, fizz: unit?.unit?.shortVersion ?? "", date: date)
            if let k = try await kept.add(f.bytes, fileName: f.fileName) { keptSel = k.id }
            say("KEPT \(f.name)", FZ.champagne)
        } catch { say("NOT KEPT", FZ.red) }
    }

    /// After a slot's move on the unit: the store read again when the patterns are looked at.
    private func afterMove() async {
        if mode != .slots, ops.isEmpty { await read() }
    }

    // MARK: the patterns (they wait for WRITE)

    /// A tile tapped: it alone selected, or nothing when it was.
    public func tap(_ k: Int) {
        pats = pats == [k] ? [] : [k]
        anchor = k
    }

    /// A drag across the tiles: from the first to this one.
    public func selectRange(from a: Int, to b: Int) {
        pats = Set(min(a, b)...max(a, b))
        anchor = a
    }

    /// iPad: one more (or one less).
    public func toggle(_ k: Int) {
        if pats.contains(k) { pats.remove(k) } else { pats.insert(k) }
        anchor = k
    }

    public func clearSelection() { pats = []; anchor = -1 }

    public var canEdit: Bool { view?.project(sel)?.there == true && writing == nil }

    public func copy() async {
        guard let h = session, canEdit, !pats.isEmpty else { return }
        guard let c = try? await FizzDataShared.engine().projectsCopy(h, p: sel, ks: Array(pats)) else {
            say("NOTHING TO COPY", FZ.orange)
            return
        }
        await take(c)
    }

    private func take(_ c: FizzClip) async {
        if let old = clip, old.c != c.c { await (try? FizzDataShared.engine())?.projectsFreeClip(old.c) }
        clip = c
        say("\(c.n) COPIED", FZ.cyan)
    }

    /// PASTE: the clip from the first pattern selected on (asked first over patterns there).
    public func paste() {
        guard let c = clip, canEdit, let j = pats.min(), let k0 = c.ks.first, let pr = view?.project(sel) else { return }
        let at = c.ks.map { j + $0 - k0 }
        let used = at.filter { $0 < 64 && pr.pats[$0] != nil }
        let q = sel
        let go: @MainActor () async -> Void = { [weak self] in
            guard let self, let h = self.session else { return }
            do {
                switch try await FizzDataShared.engine().projectsPaste(h, q: q, j: j, clip: c.c) {
                case .success(let v):
                    self.view = v
                    self.pats = Set(at.filter { $0 < 64 })
                case .failure(let r): self.refuse(r)
                }
            } catch { self.say("NOT PASTED", FZ.red) }
        }
        if used.isEmpty { Task { await go() } } else {
            ask = ProjectsAsk(title: "PASTE OVER", what: FizzClip.range(used), go: "PASTE", tint: FZ.yellow, run: go)
        }
    }

    public func clear() {
        guard canEdit, let pr = view?.project(sel) else { return }
        let used = pats.filter { pr.pats[$0] != nil }.sorted()
        guard !used.isEmpty else { return }
        let q = sel
        ask = ProjectsAsk(title: "CLEAR", what: FizzClip.range(used), go: "CLEAR") { [weak self] in
            guard let self, let h = self.session else { return }
            do {
                switch try await FizzDataShared.engine().projectsClear(h, q: q, ks: used) {
                case .success(let v): self.view = v
                case .failure(let r): self.refuse(r)
                }
            } catch { self.say("NOT CLEARED", FZ.red) }
        }
    }

    /// The selection as a .fizzpat (a file in the temporary folder, for the share sheet).
    public func fizzpatFile() async -> URL? {
        guard let h = session, canEdit, !pats.isEmpty else { return nil }
        do {
            let e = try FizzDataShared.engine()
            guard let c = try await e.projectsCopy(h, p: sel, ks: Array(pats)) else { say("NOTHING TO SAVE", FZ.orange); return nil }
            let f = try await e.projectsFizzpat(c.c, fizz: unit?.unit?.shortVersion ?? "")
            await e.projectsFreeClip(c.c)
            return try Self.shareFile(f.bytes, f.fileName)
        } catch { say("NOT SAVED", FZ.red); return nil }
    }

    static func shareFile(_ bytes: [UInt8], _ name: String) throws -> URL {
        let dir = FileManager.default.temporaryDirectory.appendingPathComponent("FIZZ-share-\(UUID().uuidString.prefix(8))", isDirectory: true)
        try FileManager.default.createDirectory(at: dir, withIntermediateDirectories: true)
        let u = dir.appendingPathComponent(name)
        try Data(bytes).write(to: u, options: .atomic)
        return u
    }

    public func undo() async {
        guard let h = session, !ops.isEmpty else { return }
        if let v = try? await FizzDataShared.engine().projectsUndo(h) { view = v }
    }

    public func discard() {
        guard !ops.isEmpty else { return }
        let n = ops.count
        ask = ProjectsAsk(title: "DISCARD", what: "\(n) CHANGE\(n == 1 ? "" : "S")", go: "DISCARD") { [weak self] in
            guard let self, let h = self.session else { return }
            if let v = try? await FizzDataShared.engine().projectsDiscard(h) { self.view = v }
        }
    }

    // MARK: kept on the phone

    /// Patterns copied straight out of a kept project (it is not loaded anywhere).
    public func copy(from k: KeptProject, ks: [Int]) async {
        guard let bytes = kept.bytes(k) else { return }
        do {
            switch try await FizzDataShared.engine().projectsCopyKept(session, file: bytes, fileName: k.id, ks: ks) {
            case .success(let c?): await take(c)
            case .success(nil): say("NOTHING TO COPY", FZ.orange)
            case .failure(let r): refuse(r)
            }
        } catch { say("NOT A PROJECT", FZ.red) }
    }

    public func renameKept(_ k: KeptProject, to name: String) async {
        let v = FizzSysEx.cleanName(name)
        guard !v.isEmpty, v != k.name else { return }
        do { try await kept.rename(k, to: v, fizz: unit?.unit?.shortVersion ?? "") } catch { say("NOT RENAMED", FZ.red) }
    }

    public func deleteKept(_ k: KeptProject) {
        ask = ProjectsAsk(title: "DELETE", what: k.name, go: "DELETE") { [weak self] in
            self?.kept.delete(k)
            if self?.keptSel == k.id { self?.keptSel = nil }
        }
    }

    /// A .fizzprj (kept) or .fizzpat (into the clip, pasted at once over a selection) from Files or AirDrop. false: not
    /// one of these.
    @discardableResult public func open(_ url: URL) async -> Bool {
        let ext = url.pathExtension.lowercased()
        if ext == "fizzprj" {
            do {
                if let k = try await kept.open(url) { keptSel = k.id; mode = .kept; say("KEPT \(k.name)", FZ.champagne) }
            } catch { say("NOT A PROJECT", FZ.red) }
            return true
        }
        guard ext == "fizzpat" else { return false }
        let scoped = url.startAccessingSecurityScopedResource()
        defer { if scoped { url.stopAccessingSecurityScopedResource() } }
        do {
            let bytes = try [UInt8](Data(contentsOf: url))
            switch try await FizzDataShared.engine().projectsReadFizzpat(bytes, fileName: url.lastPathComponent) {
            case .success(let c?):
                await take(c)
                if !pats.isEmpty, canEdit { paste() }
            case .success(nil): say("NOTHING IN IT", FZ.orange)
            case .failure: say("NOT PATTERNS", FZ.red)
            }
        } catch { say("NOT PATTERNS", FZ.red) }
        return true
    }

    // MARK: WRITE TO FM-1

    /// The changes written: refused while playing (and over Bluetooth); the store read again first when the unit
    /// changed it since; the store as read kept as a backup, then written as a restore; then read again.
    public func write() async {
        guard let u = unit, let link = u.link, !ops.isEmpty, writing == nil, !reading else { return }
        if viaBle { say("USB CABLE NEEDED", FZ.orange); return }
        if let st = await link.status() {
            u.took(status: st)
            if st.playing { say("STOP THE FM-1", FZ.orange); return }
        }
        if stale {
            guard await read(force: true), !ops.isEmpty else { return }
        }
        guard let h = session, let before = baseImage else { return }
        let image: [UInt8]
        do {
            switch try await FizzDataShared.engine().projectsImage(h) {
            case .success(let b): image = b
            case .failure(let r): refuse(r); return
            }
        } catch { say("NOT WRITTEN", FZ.red); return }
        let n = ops.count
        let names = Array(Set(ops.map(\.q))).sorted().map { slot($0).name }
        writing = ProjectsWrite(names: names, changed: Self.changedSectors(before, image), sectors: (image.count + 4095) / 4096,
                                libSectors: u.layout?.libSectors ?? 22, started: Date())
        let o = await u.actions.writeProjects(image: image, before: before)
        writing = nil
        guard o.isOK else {
            if let f = u.actions.restoreFailure { say(f.text, f.nothingChanged ? FZ.orange : FZ.red) } else { tell(o, ok: nil, what: "WRITE") }
            return
        }
        // (written: what the unit has now, read back)
        if let e = try? FizzDataShared.engine() {
            if let old = session { await e.projectsClose(old) }
            if let (h2, v) = try? await e.projectsOpen(image) { session = h2; view = v }
        }
        baseImage = image
        baseWritten = nil
        await read(force: true)
        say("\(n) CHANGE\(n == 1 ? "" : "S") WRITTEN", FZ.green)
    }

    /// The 4 KB sectors that differ.
    static func changedSectors(_ a: [UInt8], _ b: [UInt8]) -> Set<Int> {
        var out = Set<Int>()
        let n = max(a.count, b.count)
        var k = 0
        while k * 4096 < n {
            let lo = k * 4096, hi = min(n, lo + 4096)
            if lo >= a.count || lo >= b.count || hi > a.count || hi > b.count || a[lo..<hi] != b[lo..<hi] { out.insert(k) }
            k += 1
        }
        return out
    }

    // MARK: saying

    func say(_ text: String, _ tint: RGB) {
        note = ProjectsNote(text: text, tint: tint)
        unnote?.cancel()
        unnote = Task { [weak self] in
            try? await Task.sleep(for: .seconds(2.6))
            if !Task.isCancelled { self?.note = nil }
        }
    }

    public func dismissNote() { note = nil }

    private func tell(_ o: UnitOutcome, ok: String?, what: String) {
        switch o {
        case .ok, .placed: if let ok { say(ok, FZ.cyan) }
        case .refused(.busy): say("STOP THE FM-1", FZ.orange)
        case .refused(.updateFizz): say("UPDATE FIZZ", FZ.orange)
        case .refused(let r): say(r.name, FZ.orange)
        case .noAnswer: say("NO ANSWER", FZ.red)
        case .notNow: say("NOT NOW", FZ.muted)
        case .needsConfirm: say(what, FZ.orange)
        }
    }

    /// projtab.js refuse, as a short state.
    func refuse(_ r: FizzProjectsRefusal) {
        switch r.code {
        case "poolfull": say("POOL FULL", FZ.orange)
        case "full", "libfull": say("STORE FULL", FZ.orange)
        case "range": say(r.message.contains("slot") ? "NO SUCH SLOT" : "PAST D16", FZ.orange)
        case "demo": say("THE DEMOS' OWN", FZ.champagne)
        default: say(r.message.isEmpty ? "NOT DONE" : r.message.uppercased(), FZ.red)
        }
    }
}
