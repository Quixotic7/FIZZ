// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// The unit picker's model (mock-ups 34-36): THIS iPHONE and every FM-1 port seen, the one in use, the one picked by
// hand (the pin: it stays until it goes away or a new unit appears; with no pin a plugged-in FM-1 takes over as
// before). The other unit beside the one in use (THIS iPHONE while an FM-1 is the unit, the FM-1 kept open while
// THIS iPHONE is) and SEND to it: the presets read from this unit, stored in the other's MY PRESETS.

import Foundation
import FizzData
import FizzKit

/// A unit the picker offers.
public enum UnitKey: Hashable, Sendable {
    case here                   // the emulated FIZZ (THIS iPHONE)
    case port(String)           // an FM-1, by its port's name
}

/// An FM-1 port seen besides the unit: what it answered (silent: paired, or plugged, but no FIZZ answer).
public struct SeenPort: Sendable, Equatable, Identifiable {
    public let name: String
    public var id: String { name }
    public var version: String?
    public var answering: Bool
}

/// One card of the picker.
public struct UnitRow: Sendable, Equatable, Identifiable {
    public enum Via: Sendable, Equatable { case here, usb, bluetooth }
    public let key: UnitKey
    public var id: UnitKey { key }
    public let via: Via
    public let title: String
    public let version: String?
    public let answering: Bool
    public let inUse: Bool
    public let pinned: Bool
}

/// The unit beside the one in use, where SEND copies to.
public struct OtherUnit: Sendable, Equatable {
    public let key: UnitKey
    /// FM-1, THIS iPHONE, THIS iPAD
    public let name: String
    public var version: String?
    /// its MY PRESETS' places taken (nil: not read yet), and how many places there are
    public var taken: Set<Int>?
    public var nlib = 256
    public var isEmulated: Bool { key == .here }
    /// where the next preset sent lands: its first free MY PRESETS place (nil: not read yet, or full)
    public var freeSlot: Int? { taken.flatMap { t in (1...max(1, nlib)).first { !t.contains($0) } } }
}

/// Some of a scanner's ports: the rest left alone (the one kept open, the one in use).
struct PortSubset: MIDIPortScanning {
    let inner: any MIDIPortScanning
    let without: MIDIPortCandidate?
    let only: Set<String>?

    init(_ inner: any MIDIPortScanning, without: MIDIPortCandidate?) { self.inner = inner; self.without = without; only = nil }
    init(_ inner: any MIDIPortScanning, only: Set<String>) { self.inner = inner; without = nil; self.only = only }

    func candidates() -> [MIDIPortCandidate] {
        inner.candidates().filter { c in c != without && (only.map { $0.contains(c.name) } ?? true) }
    }
    func open(_ c: MIDIPortCandidate) throws -> any MIDITransport { try inner.open(c) }
}

extension UnitInfo {
    /// A port over Bluetooth (UnitInfo.via's reading of the name).
    static func isBluetooth(_ name: String) -> Bool {
        name.range(of: "bluetooth|\\bbt\\b|ble", options: [.regularExpression, .caseInsensitive]) != nil
    }

    /// A development build's git-describe tail taken off.
    static func short(_ v: String?) -> String? {
        v.map { $0.replacingOccurrences(of: "-\\d+-g[0-9a-f]+$", with: "", options: .regularExpression) }
    }
}

extension UnitModel {
    /// The unit in use as the picker names it.
    public var unitKey: UnitKey? {
        guard let u = unit else { return nil }
        return u.isEmulated ? .here : .port(u.portName)
    }

    /// The picker's cards: THIS iPHONE (the app's emulated FIZZ), the FM-1 in use, the one kept beside, the others seen.
    public var unitRows: [UnitRow] {
        var out: [UnitRow] = []
        func fm1(_ name: String, _ version: String?, answering: Bool, inUse: Bool) -> UnitRow {
            UnitRow(key: .port(name), via: UnitInfo.isBluetooth(name) ? .bluetooth : .usb, title: "FM-1", version: version,
                    answering: answering, inUse: inUse, pinned: pinned == .port(name))
        }
        if hasFallback {
            let v = onEmulator ? unit?.shortVersion : emulator.map(\.version).flatMap { $0.isEmpty ? nil : $0 }
            out.append(UnitRow(key: .here, via: .here, title: EmulatorHost.hereName, version: v.map { "FIZZ \($0)" },
                               answering: true, inUse: onEmulator, pinned: pinned == .here))
        }
        if let u = unit, !u.isEmulated { out.append(fm1(u.portName, u.shortVersion, answering: true, inUse: true)) }
        if let h = otherProbe, onEmulator { out.append(fm1(h.candidate.name, other?.version, answering: true, inUse: false)) }
        for s in seen where !out.contains(where: { $0.key == .port(s.name) }) {
            out.append(fm1(s.name, s.version, answering: s.answering, inUse: false))
        }
        return out
    }

    // MARK: the picker

    /// The picker (a unit pill, the iPad's unit card, PLAY's header pill): its rows read again.
    public func openPicker(fromPlay: Bool = false) {
        pickerFromPlay = fromPlay
        pickerOpen = true
        scan()
    }

    /// The picker went (its sheet closed).
    public func pickerClosed() {
        pickerOpen = false
        pickerFromPlay = false
    }

    /// A unit picked: the whole manager switches to it (not while a write runs), and it is pinned. From PLAY's
    /// picker, PLAY's SOUND follows (an FM-1: its track over MIDI; THIS iPHONE: the emulated FIZZ). true: it is the unit.
    @discardableResult public func pick(_ key: UnitKey) async -> Bool {
        guard switching == nil else { return false }
        // (a backup playing in PLAY: a pick there, or THIS iPHONE picked, puts it away first: BackupPlay.swift)
        if let h = emulator, h.backup != nil, pickerFromPlay || key == .here { await h.endBackup() }
        if key == unitKey {
            pin(key)
            follow(key)
            return true
        }
        guard idle else { say("a write runs: no switch now"); return false }
        switching = key
        defer { switching = nil }
        if actions.pending != nil { _ = await actions.cancelAudition() }     // (the track's own sound back first)
        await editor.unitGoes()                                             // (EDIT's borrow undone while the link is open)
        switch key {
        case .here:
            guard hasFallback else { return false }
            let was = pinned
            pin(.here)                                  // (before: a scan meanwhile leaves the FM-1 beside)
            await toHere()
            if unitKey != .here { pinned = was }
        case .port(let name):
            guard await toPort(name) else { return false }
            pin(key)
        }
        guard unitKey == key else { return false }
        follow(key)
        return true
    }

    private func follow(_ key: UnitKey) {
        guard pickerFromPlay, let p = emulator?.play else { return }
        p.sound = key == .here ? .here : .fm1
    }

    func pin(_ key: UnitKey) {
        pinned = key
        pinSeen = Set(source.scanner.candidates().map(\.name))
    }

    /// The pick by hand ends (a new unit came): the FM-1 kept beside THIS iPHONE let go (a scan then lets it take over).
    func unpin() {
        pinned = nil
        pinSeen = []
        if let h = otherProbe {
            otherProbe = nil
            let editor = editor
            Task { await editor.unitGoes(); await h.link.close() }     // (a new unit takes over next)
            otherChanged()
        }
    }

    /// The ports changed: one that was not there when the pin was made is a new unit, and ends the pick by hand.
    func portsMoved() {
        guard pinned != nil else { return }
        let now = Set(source.scanner.candidates().map(\.name))
        let fresh = now.subtracting(pinSeen)
        pinSeen.formIntersection(now)
        if !fresh.isEmpty {
            say("a new unit (\(fresh.sorted().joined(separator: ", "))): the unit picked by hand gives way")
            unpin()
        }
    }

    /// THIS iPHONE picked while an FM-1 is the unit: the FM-1's link kept open beside it, the emulated FIZZ taken.
    private func toHere() async {
        if let l = link, let u = unit, !u.isEmulated {
            say("\(EmulatorHost.hereName) picked: the FM-1 stays beside it")
            let p = taken?.link === l ? taken : nil
            link = nil
            forget()
            state = .searching
            if let p, otherProbe == nil { await hold(p, version: u.version) } else { await l.close() }
        }
        await useFallback()
    }

    /// An FM-1 picked: the one kept beside taken over at once, another port probed first.
    private func toPort(_ name: String) async -> Bool {
        if let h = otherProbe, h.candidate.name == name {
            otherProbe = nil
            say("\(name) picked")
            await takeOver(h)
            return unitKey == .port(name)
        }
        let ps = await FizzLink.probe(scanner: PortSubset(source.scanner, only: [name]), clock: source.clock)
        guard let p = ps.first(where: { $0.identity.isFizz }) else {
            for q in ps { await q.link.close() }
            say("\(name): no FIZZ answers")
            return false
        }
        for q in ps where q.link !== p.link { await q.link.close() }
        say("\(name) picked")
        if let h = otherProbe { otherProbe = nil; await h.link.close() }
        if onEmulator {
            await takeOver(p)
        } else {
            if let l = link {
                link = nil
                forget()
                state = .searching
                await l.close()
            }
            await take(p)
        }
        return unitKey == .port(name)
    }

    /// THIS iPHONE picked by hand, a scan's FM-1s: true when it stays the unit (they were all there when it was picked;
    /// one kept open beside it for SEND). A new one ends the pick (false: it may take over).
    func keepHere(_ probes: [FizzProbe]) async -> Bool {
        if probes.contains(where: { !pinSeen.contains($0.candidate.name) }) {
            say("a new unit: the unit picked by hand gives way")
            unpin()
            return false
        }
        var keep: Int?
        if otherProbe == nil {
            let fizz = probes.indices.filter { probes[$0].identity.isFizz }
            keep = fizz.first { !FizzLink.isBlePort(probes[$0].candidate.name) } ?? fizz.first
        }
        for (k, p) in probes.enumerated() where k != keep { await p.link.close() }
        if let k = keep { await hold(probes[k], version: nil) }
        notice = nil; choices = []
        await useFallback()
        if probes.isEmpty { retryLater(ms: 2000) }
        return true
    }

    /// An FM-1 kept open beside THIS iPHONE: the other unit.
    func hold(_ p: FizzProbe, version: String?) async {
        otherProbe = p
        let l = p.link
        await l.setOnLost { [weak self] in Task { @MainActor in self?.otherLost(l) } }
        var v = version
        if v == nil { v = (try? await l.identity()) ?? nil }
        otherVersion = UnitInfo.short(v)
        if other?.key == .port(p.candidate.name) { other?.version = otherVersion }
        otherChanged()
    }

    private func otherLost(_ l: FizzLink) {
        guard let h = otherProbe, h.link === l else { return }
        otherProbe = nil
        pinSeen.remove(h.candidate.name)                // (back again: a new unit)
        say("the FM-1 beside \(EmulatorHost.hereName) went away")
        otherChanged()
    }

    /// The other unit as things are now (its MY PRESETS read when it changes).
    func otherChanged() {
        var o: OtherUnit?
        if let u = unit, u.isEmulated, let h = otherProbe {
            o = OtherUnit(key: .port(h.candidate.name), name: "FM-1", version: otherVersion)
        } else if let u = unit, !u.isEmulated, hasFallback {
            o = OtherUnit(key: .here, name: EmulatorHost.hereName, version: emulator?.version)
        }
        guard o?.key != other?.key else { return }
        other = o
        if o != nil { Task { await readOther() } }
    }

    /// The picker's other rows: each FM-1 port besides the unit and the one kept beside it, probed (answering, its
    /// version) or silent; a paired Bluetooth FM-1 not connected (CoreMIDI shows it offline) silent too.
    func survey() async {
        var skip = Set([unit?.portName, otherProbe?.candidate.name].compactMap { $0 })
        let busy = Set([taken?.candidate.device, otherProbe?.candidate.device].compactMap { $0 }.filter { $0 != 0 })
        let cands = source.scanner.candidates().filter { !skip.contains($0.name) && !(unit != nil && !onEmulator && busy.contains($0.device)) }
        let probes = await FizzLink.probe(scanner: PortSubset(source.scanner, only: Set(cands.map(\.name))), clock: source.clock)
        var out: [SeenPort] = []
        for p in probes {
            let v = p.identity.isFizz ? (try? await p.link.identity()) ?? nil : nil
            await p.link.close()
            if p.identity.isFizz { out.append(SeenPort(name: p.candidate.name, version: UnitInfo.short(v), answering: true)) }
            skip.insert(p.candidate.name)
        }
        let found = Set(probes.map(\.candidate.device)).subtracting([0])
        for c in cands where !skip.contains(c.name) && !found.contains(c.device) {
            out.append(SeenPort(name: c.name, version: seen.first { $0.name == c.name }?.version, answering: false))
            skip.insert(c.name)
        }
        #if canImport(CoreMIDI)
        if let s = source.scanner as? MIDIPortScanner {
            for e in s.sources() where e.offline && MIDIPorts.isCandidateName(e.name) && !skip.contains(e.name) {
                out.append(SeenPort(name: e.name, version: seen.first { $0.name == e.name }?.version, answering: false))
                skip.insert(e.name)
            }
        }
        #endif
        if out != seen { seen = out }
    }

    // MARK: SEND to the other unit

    /// A link to the other unit for a moment: the FM-1 kept beside, or one of the emulated FIZZ's own (its clock lent).
    func withOtherLink<T: Sendable>(_ f: @MainActor (FizzLink) async throws -> T) async -> T? {
        guard let o = other else { return nil }
        if o.isEmulated {
            guard let fb = fallback else { return nil }
            await emulator?.lend(true)
            let ps = await FizzLink.probe(scanner: fb.scanner, clock: fb.clock)
            var r: T?
            if let p = ps.first(where: { $0.identity.isFizz }) { r = try? await f(p.link) }
            for p in ps { await p.link.close() }
            await emulator?.lend(false)
            return r
        }
        guard let h = otherProbe else { return nil }
        return try? await f(h.link)
    }

    /// The other unit's MY PRESETS (where a SEND lands).
    public func readOther() async {
        guard let o = other else { return }
        let got: (Set<Int>, Int)? = await withOtherLink { l in
            let rows = try await l.list()
            let st = await l.status()
            return (Set(rows.filter { $0.source == "mine" }.map(\.slot)), st?.nlib ?? 256)
        }
        guard let got, other?.key == o.key else { return }
        other?.taken = got.0
        other?.nlib = got.1
    }

    /// SEND TO <the other unit>: the presets read from this one (MY PRESETS, FACTORY), stored in the other's MY PRESETS
    /// (each in its first free place; FULL stops the rest). One outcome each; none: not read yet, or no other unit.
    public func sendToOther(rows: [FizzListEntry]) async -> [UnitOutcome] {
        guard other != nil, !sendingOther else { return [] }
        let sounds = await actions.exportSounds(rows)
        guard !sounds.isEmpty else { return [] }
        return await sendToOther(sounds: sounds)
    }

    /// SEND n TO <the other unit> from KEPT's SELECT.
    public func sendToOther(kept ks: [KeptPreset]) async -> [UnitOutcome] {
        var sounds: [FizzSound] = []
        for k in ks { sounds.append(await kept.full(k)?.sound ?? k.sound) }
        return await sendToOther(sounds: sounds)
    }

    /// The sounds stored in the other unit's MY PRESETS, as IMPORT's SEND stores them (STORE, BATCH after the first).
    public func sendToOther(sounds: [FizzSound]) async -> [UnitOutcome] {
        guard let o = other, !sendingOther, !sounds.isEmpty else { return [] }
        sendingOther = true
        defer { sendingOther = false }
        let out: [UnitOutcome] = await withOtherLink { link in
            var out: [UnitOutcome] = []
            for (i, s) in sounds.enumerated() {
                guard let f = try? FizzSysEx.storeFrame(name: s.name, blob: s.blob, drive: s.drive, pan: s.pan,
                                                       chorus: s.sends.chorus, delay: s.sends.delay, reverb: s.sends.reverb,
                                                       fizzTags: s.tags, pool: false, batch: i > 0, track: -1)
                else { out.append(.refused(.badPatch)); continue }
                do {
                    let r = try await link.store(f)
                    if let v = r.value { out.append(.placed(v.slot)); continue }
                    out.append(UnitActions.outcome(r))
                    if r.refusal == .full { break }
                } catch {
                    out.append(.noAnswer)
                    break
                }
            }
            return out
        } ?? [.noAnswer]
        say("sent \(out.filter(\.isOK).count) of \(sounds.count) to \(o.name)")
        await readOther()
        return out
    }

    // MARK: the demo's picks (simulator screenshots)

    /// `-demo units`: `-pick-here` picks THIS iPHONE once the demo FM-1 is in; `-picker` / `-picker-play` opens the picker.
    func demoPicks(_ args: [String] = ProcessInfo.processInfo.arguments) {
        guard source is DemoUnitsSource else { return }
        let here = args.contains("-pick-here"), open = args.contains("-picker") || args.contains("-picker-play")
        guard here || open else { return }
        Task {
            for _ in 0..<300 where !(onFM1 && dumpsLoaded) { try? await Task.sleep(for: .milliseconds(50)) }
            if here { await pick(.here) }
            for _ in 0..<100 where other?.taken == nil { try? await Task.sleep(for: .milliseconds(50)) }
            if open { openPicker(fromPlay: args.contains("-picker-play")) }
        }
    }
}
