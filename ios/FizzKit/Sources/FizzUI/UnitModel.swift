// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// The connected FM-1 as the screens see it (site/manager/manager.js attach, reread, loadDumps): finding it, reading
// it on connect (IDENTITY, FZBI, LIST, STATUS, PROJECTS, then the DUMPs for the rows' glyphs), reading it again on
// refresh. Read only: nothing here changes the unit.

import Foundation
import FizzData
import FizzEngine
import FizzKit
import Observation

/// The unit found and read.
public struct UnitInfo: Sendable, Equatable {
    public let idText: String
    public let portName: String
    public let candidate: MIDIPortCandidate
    /// FIZZ's version as it answers IDENTITY ("0.12.0-beta.11"); nil: FIZZ does not answer (safe mode, no data)
    public let version: String?
    /// The emulated FIZZ (THIS iPHONE), not an FM-1
    public var isEmulated = false
    public var via: String { portName.range(of: "bluetooth|\\bbt\\b|ble", options: [.regularExpression, .caseInsensitive]) != nil ? "BT" : "USB" }

    /// a development build's git-describe tail taken off (manager.js renderUnit)
    public var shortVersion: String? {
        version.map { $0.replacingOccurrences(of: "-\\d+-g[0-9a-f]+$", with: "", options: .regularExpression) }
    }
}

/// "0.12.0-beta.11" -> ("0.12.0", "BETA 11"); a release has no chip.
public func versionParts(_ v: String) -> (base: String, pre: String?) {
    guard let dash = v.firstIndex(of: "-") else { return (v, nil) }
    let pre = v[v.index(after: dash)...].replacingOccurrences(of: ".", with: " ").uppercased()
    return (String(v[..<dash]), pre)
}

/// A unit offered when two or more answered.
public struct UnitChoice: Sendable, Identifiable {
    public let id: Int
    public let name: String
    public let detail: String
    public let isFizz: Bool
}

public enum ConnectState: Sendable {
    case searching
    case found(UnitInfo)
    case updateMode(String)
    case notFizz(String)
    case choose([FizzProbe])
    case lost

    public var kind: String {
        switch self {
        case .searching: "searching"
        case .found: "found"
        case .updateMode: "update"
        case .notFizz: "notfizz"
        case .choose: "choose"
        case .lost: "lost"
        }
    }
}

/// The store's use from STATUS's pages: MY PRESETS' (lib) and the projects' (prj), each [used, all].
public struct StoreUse: Sendable, Equatable {
    public let lib: [Int], prj: [Int]
    public var total: Int { max(1, lib[1] + prj[1]) }
    public var percent: Int {
        let f = Double(lib[0] + prj[0]) / Double(total)
        return f > 0 ? max(1, Int((f * 100).rounded())) : 0
    }
    /// the meter's four parts as fractions: presets used, presets free, projects used, projects free
    public var parts: [Double] {
        [lib[0], lib[1] - lib[0], prj[0], prj[1] - prj[0]].map { Double(max(0, $0)) / Double(total) }
    }
}

@MainActor @Observable
public final class UnitModel {
    public internal(set) var state: ConnectState = .searching
    public internal(set) var choices: [UnitChoice] = []
    public private(set) var unit: UnitInfo?
    public private(set) var storeInfo: FizzStoreInfo?
    public private(set) var status: FizzStatus?
    public private(set) var rows: [FizzListEntry] = []
    public private(set) var projects: FizzProjects?
    /// "mine:12" / "factory:3" -> the algorithm 1..8, from the DUMPs
    public private(set) var algos: [String: Int] = [:]
    public private(set) var dumpsLoaded = false
    public private(set) var refreshing = false
    public private(set) var log: [String] = []
    /// What the last scan saw: every port, what was probed, what answered (the PORTS sheet)
    public private(set) var ports = PortReport()
    /// The store's layout (its sectors), from FZBI's size
    public private(set) var layout: FizzLayout?
    /// The store's tracks as the last full backup read them (manager.js unitTracks)
    public private(set) var storeTracks: Int?
    /// BACK UP, PRESETS ONLY and the backups kept
    public let backups: BackupModel
    /// Every write, then the page's re-reads (UnitActions.swift)
    public let actions: UnitActions
    /// The web tags (Documents/webtags.json; LibraryModel.swift)
    public let webTags: WebTagStore
    /// A tab a screen asks for (RootView switches to it)
    public var goTo: FizzTab?
    /// EDIT: the sound edited and the track it follows (EditModel.swift)
    @ObservationIgnored public private(set) lazy var editor = EditorModel(unit: self)
    /// I4: RESTORE / ADD PRESETS from a backup (RestoreModel.swift), IMPORT's files and SEND (ImportModel.swift)
    public let restoring = RestoreModel()
    public let importer = ImportModel()
    /// PROJECTS' store read, its changes waiting, the kept projects (I6, ProjectsModel.swift)
    public let projectsTab: ProjectsModel
    /// KEPT: the phone's own presets, Documents/Presets (KeptPresets.swift)
    public let kept: KeptPresets
    /// LIBRARY's SELECT on the unit's presets (LibrarySelect.swift)
    public let picks = LibraryPicks()
    /// The emulated FIZZ (EmulatorHost.swift): PLAY plays it, and it is the unit while no FM-1 is plugged in.
    public let emulator: EmulatorHost?
    /// An FM-1 found but not taken while the emulated FIZZ stays the unit: update mode, not FIZZ, two units
    /// (.updateMode / .notFizz / .choose; the pill shows it, its card opens as a sheet).
    public internal(set) var notice: ConnectState?
    /// The notice's card shown (it opens by itself when a new notice comes)
    public var noticeOpen = false
    /// An FM-1 waits for a write on the emulated FIZZ to finish before it takes over.
    public private(set) var switchWaiting = false
    /// The unit picked by hand (the picker's pin, Units.swift): it stays until it goes away or a new unit appears.
    public internal(set) var pinned: UnitKey?
    /// The unit picker shown (every unit pill opens it).
    public var pickerOpen = false
    /// The FM-1 ports seen besides the unit and the one kept beside it (the picker's rows; read while it shows).
    public internal(set) var seen: [SeenPort] = []
    /// The other unit SEND copies to: THIS iPHONE while an FM-1 is the unit, the FM-1 kept open while THIS iPHONE is.
    public internal(set) var other: OtherUnit?
    /// A SEND to the other unit running.
    public internal(set) var sendingOther = false
    /// A switch picked by hand running (the picker's card ripples).
    public internal(set) var switching: UnitKey?
    /// RemoteFM1 units seen: FM-1s that could be THIS iPHONE's panel (PanelUnits.swift; the picker's cyan rows).
    public internal(set) var panelUnits: [PanelUnit] = []
    /// What the panel last did, said a moment (linked, let go and why: PanelUI.swift).
    public internal(set) var panelToast: PanelToast?
    /// The RemoteFM1 units the picker was opened for by itself (once each while plugged in).
    @ObservationIgnored var panelOffered: Set<String> = []
    /// The panel's state and the backup playing as last seen (their changes make the toasts).
    @ObservationIgnored var panelWas = PanelSeen()
    /// The picker opened from PLAY (PLAY's SOUND follows the pick).
    @ObservationIgnored var pickerFromPlay = false
    /// The FM-1 kept open beside THIS iPHONE (picked by hand while one is plugged in).
    @ObservationIgnored var otherProbe: FizzProbe?
    /// The unit's own probe (its port and identity: kept beside when THIS iPHONE is picked).
    @ObservationIgnored var taken: FizzProbe?
    /// The ports there when the pin was made, less the ones gone since: one not in it is a new unit.
    @ObservationIgnored var pinSeen: Set<String> = []
    /// The FIZZ version of the FM-1 kept beside.
    @ObservationIgnored var otherVersion: String?
    /// The unit is the emulated FIZZ.
    public var onEmulator: Bool { unit?.isEmulated == true }
    /// The app has a unit of its own: it opens straight to its tabs.
    public var hasFallback: Bool { fallback != nil }

    public var mine: [FizzListEntry] { rows.filter { $0.source == "mine" } }
    public var factory: [FizzListEntry] { rows.filter { $0.source == "factory" } }
    public var pool: [FizzListEntry] { rows.filter { $0.source == "pool" } }
    public var storeUse: StoreUse? {
        guard let s = status, s.lib.count == 2, s.prj.count == 2, s.lib[1] + s.prj[1] > 0 else { return nil }
        return StoreUse(lib: s.lib, prj: s.prj)
    }
    public func algorithm(_ r: FizzListEntry) -> Int { algos["\(r.source):\(r.slot)"] ?? (r.slot % 8 + 1) }

    @ObservationIgnored let source: any LinkSource
    /// The emulated FIZZ's port (nil: the app waits for an FM-1, as before)
    @ObservationIgnored let fallback: (any LinkSource)?
    @ObservationIgnored private var fallbackListener: Task<Void, Never>?
    @ObservationIgnored private var joiningFallback = false
    @ObservationIgnored var link: FizzLink?
    @ObservationIgnored private var started = false
    @ObservationIgnored var scanning = false
    @ObservationIgnored private var scanAgain = false
    @ObservationIgnored private var listener: Task<Void, Never>?
    @ObservationIgnored private var debounce: Task<Void, Never>?
    @ObservationIgnored var retry: Task<Void, Never>?
    @ObservationIgnored private var lastActive = -Double.infinity

    /// backups: the folder the backups are kept in (Documents/Backups in the app). emulator: the emulated FIZZ, the
    /// unit while no FM-1 is found (fallback: another port for that, the tests).
    public init(source: any LinkSource, emulator: EmulatorHost? = nil, fallback: (any LinkSource)? = nil,
                backups: URL = BackupStore.defaultDirectory, projects: URL = KeptStore.defaultDirectory,
                presets: URL = KeptPresets.defaultDirectory) {
        self.source = source
        self.kept = KeptPresets(directory: presets, index: presets == KeptPresets.defaultDirectory ? KeptPresets.defaultIndex : nil)
        self.emulator = emulator
        self.fallback = fallback ?? emulator?.source
        self.backups = BackupModel(store: BackupStore(directory: backups))
        self.actions = UnitActions()
        self.webTags = WebTagStore(url: WebTagStore.url(besides: backups))
        self.projectsTab = ProjectsModel(kept: KeptStore(directory: projects))
        self.backups.unit = self
        self.actions.unit = self
        self.restoring.unit = self
        self.importer.unit = self
        self.projectsTab.unit = self
    }

    // MARK: finding the unit

    /// Starts looking (once): now, on every port change, and again a while after nothing was found.
    public func start() {
        guard !started else { return }
        started = true
        emulator?.play.unit = self
        let store = backups.store
        Task { await store.reload() }
        let kept = kept
        Task { await kept.reload() }
        kept.watch()
        let changes = source.changes
        listener = Task { [weak self] in
            for await _ in changes { self?.portsChanged() }
        }
        if let f = fallback {
            let fc = f.changes
            // (the emulated FIZZ powered on or off: looked at again at once)
            fallbackListener = Task { [weak self] in
                for await _ in fc { self?.fallbackChanged() }
            }
        }
        scan()
        demoPicks()
        watchPanel()
        demoPanel()
    }

    public func stop() {
        listener?.cancel(); debounce?.cancel(); retry?.cancel(); fallbackListener?.cancel()
        kept.unwatch()
        let l = link, o = otherProbe?.link
        link = nil
        otherProbe = nil
        Task { await l?.close(); await o?.close() }
        if case .choose(let ps) = state { for p in ps { Task { await p.link.close() } } }
        if case .choose(let ps) = notice { for p in ps { Task { await p.link.close() } } }
    }

    func scan() {
        if scanning { scanAgain = true; return }
        scanning = true
        retry?.cancel()
        Task {
            await scanOnce()
            if pickerOpen { await survey() }            // (the picker's rows: Units.swift)
            scanning = false
            if scanAgain { scanAgain = false; scan() }
        }
    }

    private func scanOnce() async {
        if onFM1 { return }
        var report = PortReport(at: Date())
        #if canImport(CoreMIDI)
        if let s = source.scanner as? MIDIPortScanner {
            report.sources = s.sources().map { $0.offline ? "\($0.name) (offline)" : $0.name }
            report.destinations = s.destinations().map { $0.offline ? "\($0.name) (offline)" : $0.name }
        }
        #endif
        // (the FM-1 kept beside THIS iPHONE is open already: not probed again)
        // (nor the RemoteFM1 unit attached as THIS iPHONE's panel)
        let scanner = PortSubset(source.scanner, without: otherProbe?.candidate, skipping: panelSkip)
        report.probed = scanner.candidates().map(\.name)
        let probes = await FizzLink.probe(scanner: scanner, clock: source.clock)
        notePanelUnits(probes)
        report.answers = probes.map { PortReport.Answer(port: $0.candidate.name, id: $0.identity.text) }
        if report.probed != ports.probed || report.answers.map(\.id) != ports.answers.map(\.id)
            || report.sources != ports.sources {
            say("ports: \(report.sources.joined(separator: ", ")); probed \(report.probed.joined(separator: ", ")); "
                + "answered \(report.answers.map { "\($0.port) = \($0.id)" }.joined(separator: ", "))")
        }
        ports = report
        if onFM1 { for p in probes { await p.link.close() }; return }
        if fallback != nil { await offline(probes); return }
        if probes.isEmpty {
            state = .searching
            retry = Task { [weak self] in
                try? await Task.sleep(for: .seconds(2))
                if !Task.isCancelled { self?.scan() }
            }
            return
        }
        // the same FM-1 on USB and over Bluetooth (or a second one only over Bluetooth): USB taken, no picker
        if probes.count > 1, let k = Self.usbOverBluetooth(probes.map { ($0.candidate.name, $0.identity.isFizz) }) {
            for (j, p) in probes.enumerated() where j != k { await p.link.close() }
            say("USB taken: \(probes[k].candidate.name) (Bluetooth left)")
            await take(probes[k])
            return
        }
        if probes.count > 1 {
            var cs: [UnitChoice] = []
            for (k, p) in probes.enumerated() {
                let v = p.identity.isFizz ? (try? await p.link.identity()) ?? nil : nil
                cs.append(UnitChoice(id: k, name: p.candidate.name, detail: v ?? p.identity.text, isFizz: p.identity.isFizz))
            }
            choices = cs
            state = .choose(probes)
            say("\(probes.count) units: \(probes.map(\.identity.text).joined(separator: ", "))")
            return
        }
        await take(probes[0])
    }

    /// Of several ports answering: the one FIZZ on USB when every other is a Bluetooth port; nil: the picker.
    static func usbOverBluetooth(_ ports: [(name: String, isFizz: Bool)]) -> Int? {
        let usb = ports.indices.filter { !FizzLink.isBlePort(ports[$0].name) }
        guard usb.count == 1, ports[usb[0]].isFizz else { return nil }
        return usb[0]
    }

    /// One of the units offered (`.choose`): the others' links closed.
    public func choose(_ index: Int) {
        if case .choose(let ps) = notice, ps.indices.contains(index) {
            for (k, p) in ps.enumerated() where k != index { Task { await p.link.close() } }
            choices = []
            notice = nil
            Task { await takeOver(ps[index]) }
            return
        }
        guard case .choose(let ps) = state, ps.indices.contains(index) else { return }
        for (k, p) in ps.enumerated() where k != index { Task { await p.link.close() } }
        choices = []
        state = .searching
        Task { await take(ps[index]) }
    }

    func take(_ p: FizzProbe) async {
        if p.identity.isFizz { await attach(p); return }
        await p.link.close()
        say("\(p.identity.text) on \"\(p.candidate.name)\": \(p.identity.isOTA ? "update mode" : "not FIZZ")")
        state = p.identity.isOTA ? .updateMode(p.identity.text) : .notFizz(p.identity.text)
    }

    private func attach(_ p: FizzProbe, emulated: Bool = false) async {
        let link = p.link
        self.link = link
        taken = p
        await link.setOnLost { [weak self] in Task { @MainActor in self?.lost(link) } }
        do {
            let version = try await link.identity()
            storeInfo = try await link.storeInfo()
            layout = await Self.layout(storeInfo)
            rows = version != nil ? await readList(link) : []
            status = version != nil ? await link.status() : nil
            projects = status != nil ? await link.projects() : nil
            guard self.link === link else { return }
            let u = UnitInfo(idText: p.identity.text, portName: p.candidate.name, candidate: p.candidate, version: version,
                             isEmulated: emulated)
            unit = u
            state = .found(u)
            otherChanged()
            say("connected: \(u.idText) on \"\(u.portName)\", FIZZ \(version ?? "(no answer)"), \(mine.count) my presets")
            await loadDumps(link)
        } catch {
            say("connect: \(error)")
            if self.link === link { lost(link) }
        }
    }

    /// LIST, up to 4 tries 0.3 s apart (refreshRows); [] when none answered.
    private func readList(_ link: FizzLink) async -> [FizzListEntry] {
        for k in 0..<4 {
            if let r = try? await link.list(factory: true, pool: true) { return r }
            if k < 3 { try? await Task.sleep(for: .milliseconds(300)) }
        }
        return rows
    }

    /// MY PRESETS' and the factory's DUMPs (all of each at once): the algorithm of each row.
    private func loadDumps(_ link: FizzLink) async {
        guard unit?.version != nil else { return }
        do {
            var a: [String: Int] = [:]
            let m = try await link.dumpAll(source: .mine, total: mine.count)
            for f in m { if let d = FizzSysEx.parseDump(f), d.blob.count > 2 { a["mine:\(d.slot)"] = Int(d.blob[2]) + 1 } }
            let fac = try await link.dumpAll(source: .factory, total: factory.count)
            for (k, f) in fac.enumerated() {
                if let d = FizzSysEx.parseDump(f), d.blob.count > 2 { a["factory:\(d.slot > 0 ? d.slot : k + 1)"] = Int(d.blob[2]) + 1 }
            }
            guard self.link === link else { return }
            algos = a
            dumpsLoaded = true
            actions.dumpsRead(mine: m, factory: fac)
            say("DUMP: \(m.count) of \(mine.count) my presets, \(fac.count) factory")
        } catch {
            say("library: \(error)")
        }
    }

    private func lost(_ l: FizzLink) {
        guard link === l else { return }
        let was = unit
        link = nil
        forget()
        state = .lost
        // (the FM-1 picked by hand gone: the pick with it; THIS iPHONE comes back at once)
        if let w = was, !w.isEmulated, pinned == .port(w.portName) { pinned = nil }
        say(was?.isEmulated == true ? "\(EmulatorHost.hereName) went away" : "the FM-1 was disconnected")
        if was?.isEmulated == true { Task { await emulator?.serve(false) } }
        retry?.cancel()
        // (with the emulated FIZZ to fall back on: back to it soon)
        let ms = fallback != nil ? 300 : 1200
        retry = Task { [weak self] in
            try? await Task.sleep(for: .milliseconds(ms))
            if !Task.isCancelled { self?.scan() }
        }
    }

    /// What was read from the unit, dropped.
    func forget() {
        unit = nil
        rows = []; status = nil; projects = nil; storeInfo = nil; algos = [:]; dumpsLoaded = false
        layout = nil; storeTracks = nil
        actions.lost()
        otherChanged()
    }

    private func portsChanged() {
        debounce?.cancel()
        debounce = Task { [weak self] in
            try? await Task.sleep(for: .milliseconds(800))
            if !Task.isCancelled { self?.afterPortChange() }
        }
    }

    private func afterPortChange() {
        portsMoved()                                            // (a new unit ends a pick by hand: Units.swift)
        if case .choose(let ps) = notice {
            for p in ps { Task { await p.link.close() } }
            choices = []
            notice = nil
        }
        switch state {
        case .found(let u) where u.isEmulated:
            scan()                                              // (an FM-1 plugged in?)
        case .found(let u):
            if !source.scanner.candidates().contains(u.candidate), let l = link { Task { await l.close() }; lost(l) }
            else if pickerOpen { scan() }                       // (the picker's rows read again)
        case .choose(let ps):
            for p in ps { Task { await p.link.close() } }
            choices = []
            state = .searching
            scan()
        default:
            scan()
        }
    }

    // MARK: reading again

    /// What tells a changed unit (manager.js unitSig): records written, the counts, the project, dirty, no store.
    public nonisolated static func changed(_ old: FizzStatus?, _ new: FizzStatus?) -> Bool {
        func sig(_ s: FizzStatus?) -> [Int] {
            guard let s else { return [] }
            return [s.written, s.mine, s.pool, s.project, s.projects, s.dirty ? 1 : 0, s.noStore ? 1 : 0]
        }
        return sig(old) != sig(new)
    }

    /// Everything read again; quick (the app coming back): a STATUS, and the rest only when it says the unit changed.
    /// true when the lists were read again.
    @discardableResult public func refresh(quick: Bool = false) async -> Bool {
        guard let link, let u = unit, !refreshing, !actions.busy, !backups.busy else { return false }
        if quick && (u.version == nil || actions.editing > 0) { return false }
        refreshing = true
        defer { refreshing = false }
        if quick {
            let st = await link.status()
            guard let st, Self.changed(status, st) else {
                if let st { status = st }
                return false
            }
            say("the FM-1 changed: read again")
        }
        do {
            if u.version == nil, let v = try await link.identity() {
                unit = UnitInfo(idText: u.idText, portName: u.portName, candidate: u.candidate, version: v,
                                isEmulated: u.isEmulated)
            }
            storeInfo = try await link.storeInfo()
            layout = await Self.layout(storeInfo)
            rows = await readList(link)
            status = await link.status()
            projects = await link.projects()
            await loadDumps(link)
            return true
        } catch {
            say("refresh: \(error)")
            return false
        }
    }

    /// The app came to the front (scenePhase .active): a quick refresh, at most every 2 s.
    public func becameActive() {
        let t = Date().timeIntervalSinceReferenceDate
        guard t - lastActive >= 2 else { return }
        lastActive = t
        let store = backups.store
        Task { await store.reload() }
        let kept = kept
        Task { await kept.reload() }
        kept.watch()
        Task { await refresh(quick: true) }
    }

    private static func layout(_ info: FizzStoreInfo?) async -> FizzLayout? {
        guard let info else { return nil }
        return try? await FizzDataShared.engine().layout(size: Int(info.size))
    }

    /// After a backup: the LIST it read, and the store's tracks (full backups).
    func backedUp(rows: [FizzListEntry]?, tracks: Int?) {
        if let rows, !rows.isEmpty { self.rows = rows }
        if let tracks { storeTracks = tracks }
    }

    /// Looks again now (a tap on the not-FIZZ card, the PORTS sheet)
    public func rescan() {
        if case .choose = notice { return }
        switch state {
        case .found(let u) where u.isEmulated: scan()
        case .found, .choose: return
        default: scan()
        }
    }

    // MARK: the emulated FIZZ as the unit (offline mode)

    /// An FM-1 is the unit.
    var onFM1: Bool { if case .found(let u) = state { !u.isEmulated } else { false } }

    /// Nothing is being written to the unit (a switch waits for it).
    var idle: Bool {
        !actions.busy && !backups.busy && actions.restoreRun == nil && projectsTab.writing == nil && !refreshing
            && !sendingOther
    }

    /// With the emulated FIZZ to fall back on: an FM-1 running FIZZ takes over (once nothing is being written); none,
    /// or one in update mode, not FIZZ, or two to choose from: the emulated FIZZ stays the unit, the notice says why.
    private func offline(_ probes0: [FizzProbe]) async {
        // (a RemoteFM1 unit is no FIZZ to take: no notice, the picker offers it as THIS iPHONE's panel instead)
        let probes = await offerPanels(probes0)
        // THIS iPHONE picked by hand: the FM-1s known stay beside it (one kept open for SEND), a new one ends the pick
        if pinned == .here, await keepHere(probes) { return }
        var pick: Int?
        if probes.count == 1 {
            pick = 0
        } else if probes.count > 1, let k = Self.usbOverBluetooth(probes.map { ($0.candidate.name, $0.identity.isFizz) }) {
            for (j, p) in probes.enumerated() where j != k { await p.link.close() }
            say("USB taken: \(probes[k].candidate.name) (Bluetooth left)")
            pick = k
        }
        if probes.isEmpty {
            notice = nil; choices = []; switchWaiting = false
            await useFallback()
            retryLater(ms: 2000)
            return
        }
        if let k = pick {
            let p = probes[k]
            guard p.identity.isFizz else {
                await p.link.close()
                let n: ConnectState = p.identity.isOTA ? .updateMode(p.identity.text) : .notFizz(p.identity.text)
                if notice?.kind != n.kind {
                    noticeOpen = true
                    say("\(p.identity.text) on \"\(p.candidate.name)\": \(p.identity.isOTA ? "update mode" : "not FIZZ")")
                }
                notice = n
                await useFallback()
                return
            }
            if onEmulator && !idle {
                // a write on the emulated FIZZ runs: the FM-1 takes over once it is done
                await p.link.close()
                if !switchWaiting { say("FM-1 found: it takes over once the write finishes") }
                switchWaiting = true
                retryLater(ms: 500)
                return
            }
            await takeOver(p)
            return
        }
        var cs: [UnitChoice] = []
        for (k, p) in probes.enumerated() {
            let v = p.identity.isFizz ? (try? await p.link.identity()) ?? nil : nil
            cs.append(UnitChoice(id: k, name: p.candidate.name, detail: v ?? p.identity.text, isFizz: p.identity.isFizz))
        }
        choices = cs
        if notice?.kind != "choose" { noticeOpen = true }
        notice = .choose(probes)
        say("\(probes.count) units: \(probes.map(\.identity.text).joined(separator: ", "))")
        await useFallback()
    }

    /// An FM-1 in place of the emulated FIZZ (its link closed first, as an unplug would).
    func takeOver(_ p: FizzProbe) async {
        notice = nil; choices = []; switchWaiting = false
        if onEmulator, let l = link {
            await editor.unitGoes()                         // (EDIT's borrow undone while the link is open)
            say("an FM-1 takes over from \(EmulatorHost.hereName)")
            link = nil
            forget()
            state = .searching
            await l.close()
            await emulator?.serve(false)
        }
        await take(p)
    }

    /// The emulated FIZZ as the unit, when nothing else is.
    func useFallback() async {
        guard let fallback, !joiningFallback else { return }
        if case .found = state { return }
        joiningFallback = true
        defer { joiningFallback = false }
        await emulator?.serve(true)                         // (its clock running: FizzLink's SysEx needs it)
        let ps = await FizzLink.probe(scanner: fallback.scanner, clock: fallback.clock)
        guard let p = ps.first(where: { $0.identity.isFizz }) else {
            for p in ps { await p.link.close() }
            return                                          // (not powered on yet: its poke looks again)
        }
        for q in ps where q.link !== p.link { await q.link.close() }
        if case .found = state { await p.link.close(); return }
        await attach(p, emulated: true)
    }

    func retryLater(ms: Int) {
        retry?.cancel()
        retry = Task { [weak self] in
            try? await Task.sleep(for: .milliseconds(ms))
            if !Task.isCancelled { self?.scan() }
        }
    }

    /// The emulated FIZZ's port came or went (powered on, off).
    private func fallbackChanged() {
        if case .found(let u) = state, u.isEmulated {
            if fallback?.scanner.candidates().contains(u.candidate) == false, let l = link {
                Task { await l.close() }
                lost(l)
            }
            return
        }
        scan()
    }

    func say(_ s: String) {
        print("FIZZ: \(s)")   // (the console: `devicectl device process launch --console`, Xcode)
        log.append(s)
        if log.count > 400 { log.removeFirst(log.count - 400) }
    }

    // MARK: what the writes read again (UnitActions)

    func took(rows: [FizzListEntry]) { self.rows = rows }
    func took(status: FizzStatus?) { self.status = status }
    func took(projects: FizzProjects?) { self.projects = projects }
    func took(storeInfo: FizzStoreInfo?) async {
        self.storeInfo = storeInfo
        layout = await Self.layout(storeInfo)
    }
    /// FIZZ's version again (it starts again after a restore).
    func took(version: String?) {
        guard let u = unit else { return }
        let n = UnitInfo(idText: u.idText, portName: u.portName, candidate: u.candidate, version: version,
                         isEmulated: u.isEmulated)
        unit = n
        if case .found = state { state = .found(n) }
    }
    func took(algorithm a: Int?, for key: String) { algos[key] = a }
    /// MY PRESETS' and the factory's DUMPs read again (the glyphs).
    func reloadDumps() async {
        guard let link else { return }
        await loadDumps(link)
    }
}
