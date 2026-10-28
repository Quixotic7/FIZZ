// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// EDIT's state (site/manager/editor.js, live.js; phase I5): the sound edited, the place it came from, the track it
// follows. What is edited: a preset (LIBRARY's EDIT), a file, or the FM-1's track (SELECTED or 1..6), read back twice
// a second while EDIT shows (skipped while the PARAM pump is busy or within 0.7 s of a PARAM). TRACK on: each value
// turned a PARAM through ParamPump (a MIDI sound: the whole sound as an AUDITION in the same queue); TRACK off: the
// track only shown, a turn refused and the switch nudged. REVERT puts the track back exactly (its DUMP); SAVE / SAVE AS
// NEW through actions.save. The values <-> the blob are sound.js's, in JavaScriptCore (EditSound.swift).

import Foundation
import FizzData
import FizzKit
import Observation

/// What EDIT is asked to edit (LIBRARY's EDIT, a file opened, the unit's track).
public enum EditTarget: Sendable, Equatable {
    case preset(FizzListEntry)
    case sound(FizzSound, label: String)
    case track
    /// a KEPT preset (SAVE writes its file back) and + NEW on KEPT (INIT, saved to KEPT)
    case kept(KeptPreset)
    case newKept
}

/// Where the edited sound came from (editor.js E.place).
public struct EditPlace: Sendable, Equatable {
    public var label: String
    /// "mine", "factory", "track", "file", "new", "kept"
    public var source: String
    /// MY PRESETS' slot SAVE writes over (0: none)
    public var slot: Int
    /// the preset's id (0: none)
    public var id: Int
    /// the track it was read from (-1: not a track)
    public var track: Int
    /// read from the unit's track (followed)
    public var fromTarget: Bool
    /// KEPT: the preset's file (nil: + NEW, not saved yet)
    public var kept: String?

    public init(label: String, source: String, slot: Int = 0, id: Int = 0, track: Int = -1, fromTarget: Bool = false, kept: String? = nil) {
        self.label = label; self.source = source; self.slot = slot; self.id = id; self.track = track; self.fromTarget = fromTarget
        self.kept = kept
    }
    /// A KEPT preset's place (nil: + NEW).
    public static func kept(_ id: String?) -> EditPlace { EditPlace(label: "KEPT", source: "kept", kept: id) }
    public var isKept: Bool { source == "kept" }
    public static let new = EditPlace(label: "NEW SOUND", source: "new")
}

/// EDIT's pages (editor.js SECTIONS, VOICE its own page).
public enum EditPage: String, CaseIterable, Sendable, Identifiable {
    case ops, openv, play, filter, amp, drive, lfo, mod, voice
    public var id: String { rawValue }
    public var title: String {
        ["OPERATORS", "OP ENV", "PLAY", "FILTER", "AMP", "DRIVE · CRUSH", "LFO", "MOD", "VOICE"][Self.allCases.firstIndex(of: self)!]
    }
    public var tint: RGB {
        switch self {
        case .ops: FZ.k1
        case .openv: FZ.k2
        case .play: FZ.yellow
        case .filter: FZ.k4
        case .amp: FZ.k3
        case .drive: FZ.orange
        case .lfo: FZ.cyan
        case .mod: FZ.kalg
        case .voice: FZ.violet
        }
    }
}

/// A value being turned (it takes the screen while the finger is down).
public struct EditTurn: Sendable, Equatable {
    /// the value's id; a MIDI slot: -1 - k
    public var id: Int
    public var label: String
    public var tint: RGB
    /// shown at the top of the screen (the finger low), else at the bottom
    public var top: Bool
    public init(id: Int, label: String, tint: RGB, top: Bool = true) { self.id = id; self.label = label; self.tint = tint; self.top = top }
}

@MainActor @Observable
public final class EditorModel {
    public private(set) var table: EditTable?
    public private(set) var sound: EditSound?
    public private(set) var base: EditSound?
    public private(set) var place = EditPlace.new
    /// on the unit's track, read back while EDIT shows (live.js L.follow)
    public private(set) var following = false
    /// the track followed (0..5; -1 none yet) and the unit's selected one
    public private(set) var track = -1
    public private(set) var unitSelected = -1
    /// the followed track records (its REC mark)
    public private(set) var rec = false
    public var page: EditPage = .ops
    /// the operator lit in the drawing (0 C, 1 A, 2 B1, 3 B2)
    public var op = 0
    /// the LFO shown (0..2) and the MIDI page (0..6)
    public var lfo = 0
    public var midiPage = 0
    /// the phone's OP ENV: the envelope shown (0 A, 1 B); its FILTER: the curve (false) or the envelope (true)
    public var env = 0
    public var filterEnv = false
    public var turning: EditTurn?
    /// a long list being picked (a destination, a source)
    public var picking: Int?
    /// the keybed's octave (-2..2) and the notes down
    public private(set) var octave = 0
    public private(set) var held: Set<Int> = []
    /// the phone's keybed strip pulled up
    public var keysOpen = false
    /// a turn refused while TRACK is off: the switch pulses (counted)
    public private(set) var nudges = 0
    public private(set) var saving = false
    /// the name being typed (the read-back leaves it alone)
    public var naming = false

    @ObservationIgnored weak var unit: UnitModel?
    @ObservationIgnored private var baseFrame: [UInt8]?
    @ObservationIgnored private(set) var chan = 14
    @ObservationIgnored private var poll: Task<Void, Never>?
    @ObservationIgnored private(set) var shown = false
    @ObservationIgnored private var attaching = false
    @ObservationIgnored private var lastNudge = Date.distantPast
    @ObservationIgnored private let edits = SerialQueue()
    @ObservationIgnored private let notes = SerialQueue()
    /// set by LIBRARY / a file (UnitModel.editTarget), taken when EDIT shows
    var request: EditTarget?

    init(unit: UnitModel?) { self.unit = unit }

    var actions: UnitActions? { unit?.actions }
    private func engine() -> FizzDataEngine? { try? FizzDataShared.engine() }
    private func say(_ s: String) { unit?.say(s) }

    // MARK: state

    /// The track can be read (live.js canRead): a FIZZ that edits a track live, connected, no transfer running.
    var canRead: Bool { (actions?.trackCapable ?? false) && unit?.link != nil && !(actions?.busy ?? true) }
    /// Edits go to the track.
    public var live: Bool { following && (actions?.trackOn ?? false) && canRead }
    /// Shown only (TRACK off): a turn is refused.
    public var locked: Bool { following && !(actions?.trackOn ?? false) }
    public var dirty: Bool {
        guard let s = sound, let b = base else { return false }
        return !s.sameValues(b) || s.name != b.name || s.tags != b.tags
    }
    /// SAVE over the MY preset it came from; SAVE AS NEW (manager.js canSave).
    public var canSave: Bool {
        if place.isKept { return sound != nil }
        return (actions?.canWrite ?? false) && place.source == "mine" && place.slot > 0 && sound != nil
    }
    /// (KEPT: another kept preset, no unit needed)
    public var canSaveNew: Bool { sound != nil && (place.isKept || (actions?.canWrite ?? false)) }
    /// The track number the switch shows: the followed one, else the unit's selected one.
    public var trackShown: Int { track >= 0 ? track : unitSelected }

    public func value(_ id: Int) -> Int { sound?.values.indices.contains(id) == true ? sound!.values[id] : 0 }
    public func text(_ id: Int) -> String { table?.text(id, value(id)) ?? "" }

    // MARK: shown, hidden, asked

    /// The table read once (fizzvalues.js through JavaScriptCore).
    public func prepare() async {
        if table == nil { table = try? await engine()?.editTable() }
    }

    /// EDIT shown (manager.js tab("edit"): editor.show, live.show): a target asked for is opened; else the track is
    /// followed when it can be and nothing of the screen's own is open.
    public func show() async {
        shown = true
        await prepare()
        if let t = request {
            request = nil
            await open(t)
        } else if canRead && (following || (actions?.trackOn ?? false) || place.source == "new" || sound == nil) {
            await attach()
        }
        if sound == nil, let t = table { open(blank(t), place: .new) }
        if following { startPoll() }
    }

    public func hide() {
        shown = false
        stopPoll()
        for n in held { noteOff(n) }
    }

    /// Something to edit: a preset (TRACK on: heard on the track at once, KEEP / CANCEL, the editor on the track),
    /// a file, the track.
    public func open(_ target: EditTarget) async {
        await prepare()
        guard let e = engine() else { return }
        switch target {
        case .track:
            await attach()
        case .preset(let r):
            guard let a = actions, let f = await a.fetchFrame(of: r), var s = try? await e.editSound(frame: f) else { return }
            if a.trackOn, await a.pick(r).isOK, await attach() { return }
            detach()
            s.extras = EditExtras(id: r.id, sound: 0, edited: false)
            open(s, place: EditPlace(label: "\(r.source == "mine" ? "MY PRESETS" : "FACTORY") \(r.slot)", source: r.source,
                                     slot: r.slot, id: r.id))
        case .sound(let fs, let label):
            guard let s = try? await e.editSound(lib: fs) else { return }
            detach()
            open(s, place: EditPlace(label: String(label.uppercased().prefix(24)), source: "file"))
        case .kept(let k):
            guard let f = await unit?.kept.full(k), let s = try? await e.editSound(lib: f.sound) else { return }
            await openKept(s, lib: f.sound, place: .kept(k.id))
        case .newKept:
            guard let t = table else { return }
            let s = blank(t)
            await openKept(s, lib: try? await e.libSound(s), place: .kept(nil))
        }
    }

    /// A KEPT sound opened: TRACK on, heard on the track at once (KEEP / CANCEL) and edited there live, its place KEPT.
    private func openKept(_ s: EditSound, lib: FizzSound?, place p: EditPlace) async {
        if let a = actions, a.trackOn, let lib, await a.pickSound(lib).isOK, await attach() {
            place = p
            say("edit: \(s.name) (KEPT, on TRACK \(track + 1))")
            return
        }
        detach()
        open(s, place: p)
    }

    /// editor.js open: the sound and the one to go back to.
    func open(_ s: EditSound, place p: EditPlace) {
        sound = s
        base = s
        place = p
        if s.isMidi { midiPage = min(midiPage, 6) }
        say("edit: \(s.name) (\(p.label))")
    }

    private func blank(_ t: EditTable) -> EditSound { EditSound(name: "INIT", values: t.defaults) }

    // MARK: the track

    /// The editor on the track: read, opened, followed (TRACK on or off; off: only shown).
    @discardableResult public func attach() async -> Bool {
        guard canRead, !attaching, let a = actions, let e = engine() else { return false }
        attaching = true
        defer { attaching = false }
        await prepare()
        guard let r = await a.readTrack(), let s = try? await e.editSound(frame: r.frame) else { return false }
        took(s)
        baseFrame = r.frame
        // (a KEPT sound heard on the track stays KEPT's)
        let keep = following && place.isKept ? place : nil
        following = true
        open(s, place: keep ?? placeOf(s))
        if shown { startPoll() }
        return true
    }

    /// Off the track (a preset or a file opened here): the read-back stops.
    public func detach() {
        guard following else { return }
        following = false
        stopPoll()
        Task { await actions?.pump?.clear() }
    }

    /// The followed track changed (SELECTED, 1..6): a pending audition cancelled first.
    public func follow(slot k: Int) async {
        guard let a = actions else { return }
        a.trackSlot = k
        if a.pending != nil { _ = await a.cancelAudition() }
        await attach()
    }

    /// TRACK switched (the pill): on, EDIT takes the track when it shows.
    public func trackSwitched() async {
        if shown, actions?.trackOn == true { await attach() }
    }

    /// After CANCEL put the track's own sound back: read again (live.js cancel).
    public func auditionEnded() async {
        if shown && following { await attach() }
    }

    private func took(_ s: EditSound) {
        if let p = actions?.pump { let t = s.track; edits.run { await p.setTrack(t) } }
        track = s.track
        if let x = s.extras { unitSelected = x.sel; rec = x.rec; chan = x.chan }
    }

    func placeOf(_ s: EditSound) -> EditPlace {
        let id = s.extras?.id ?? 0
        let row = id >= 0x100 ? unit?.mine.first { $0.id == id } : nil
        let fac = (1...23).contains(id)
        let what = row.map { "MY PRESETS \($0.slot)" } ?? (fac ? "FACTORY \(id)" : "ITS OWN SOUND")
        return EditPlace(label: what + (s.extras?.edited == true ? " · EDITED ON THE FM-1" : ""), source: row == nil ? "track" : "mine",
                         slot: row?.slot ?? 0, id: id, track: s.track, fromTarget: true)
    }

    private func startPoll() {
        guard poll == nil else { return }
        poll = Task { [weak self] in
            while !Task.isCancelled {
                try? await Task.sleep(for: .milliseconds(500))
                guard let self, !Task.isCancelled else { return }
                await self.readBack()
            }
        }
    }

    private func stopPoll() {
        poll?.cancel()
        poll = nil
    }

    private func blocked() async -> Bool {
        guard let p = actions?.pump else { return false }
        return await p.followBlocked()
    }

    /// One read-back (live.js startPoll's tick): the unit's knobs taken, another track selected on the unit taken.
    func readBack() async {
        guard shown, following else { return }
        guard unit?.link != nil else { following = false; stopPoll(); return }
        guard canRead, !attaching, (actions?.editing ?? 1) == 0, turning == nil, !(await blocked()),
              let a = actions, let e = engine() else { return }
        let t0 = track
        guard let r = await a.readTrack(), let s = try? await e.editSound(frame: r.frame), following else { return }
        took(s)
        if s.track != t0 {
            baseFrame = r.frame
            open(s, place: placeOf(s))
            say("EDIT follows TRACK \(s.track + 1) (selected on the FM-1)")
            return
        }
        if await blocked() || turning != nil { return }            // (an edit went out meanwhile: not the read's)
        let on = a.trackOn
        if follow(s, rebase: !on) && !on { baseFrame = r.frame }
        if place.fromTarget, let x = s.extras, place.id != x.id {   // (loaded on the unit)
            place = placeOf(s)
            baseFrame = r.frame
        }
    }

    /// editor.js follow: the values read back taken, not sent back; rebase: also the sound to go back to.
    @discardableResult func follow(_ s: EditSound, rebase: Bool) -> Bool {
        guard var cur = sound, turning == nil else { return false }
        if s.isMidi != cur.isMidi {                     // (ENGINE changed on the unit: the other kind)
            var n = s
            n.tags = cur.tags
            sound = n
            if rebase || base?.isMidi != s.isMidi { base = n }
            return true
        }
        var changed = cur.values != s.values || cur.midi != s.midi
        cur.values = s.values
        cur.midi = s.midi
        if s.name != cur.name && !naming { cur.name = s.name; changed = true }
        guard changed else { return false }
        sound = cur
        if rebase { var b = cur; b.tags = base?.tags ?? cur.tags; base = b }
        return true
    }

    // MARK: values

    /// A value turned: clamped, kept, and on the track a PARAM. false: unchanged (or refused: TRACK off).
    @discardableResult public func set(_ id: Int, _ v: Int) -> Bool {
        guard var s = sound, let t = table, s.values.indices.contains(id) else { return false }
        let x = t.clamp(id, v)
        guard s.values[id] != x else { return false }
        if locked { nudge(); return false }
        s.values[id] = x
        sound = s
        if live, let p = actions?.pump { edits.run { await p.param(id: id, value: x) } }
        return true
    }

    /// A value back to its default (a double tap).
    public func reset(_ id: Int) { if let d = table?.info(id)?.def { set(id, d) } }

    /// A MIDI sound's slot k changed (the whole sound to the track: an AUDITION).
    @discardableResult public func setSlot(_ k: Int, cc: Int? = nil, value: Int? = nil, touched: Bool? = nil) -> Bool {
        guard var s = sound, var m = s.midi, m.indices.contains(k) else { return false }
        let o = m[k], c7 = { (x: Int) in max(0, min(127, x)) }
        let n = EditMidiSlot(cc: c7(cc ?? o.cc), value: c7(value ?? o.value), touched: touched ?? o.touched)
        guard n != o else { return false }
        if locked { nudge(); return false }
        m[k] = n
        s.midi = m
        sound = s
        if live, let p = actions?.pump, let e = engine() {
            let t = track
            var x = s.extras ?? EditExtras()
            x.edited = true
            let snd = s
            edits.run {
                guard let f = try? await e.storeFrame(snd, track: t, audition: true,
                                                      extras: FizzTrackExtrasIn(id: x.id, sound: x.sound, edited: true)) else { return }
                await p.sound(f)
            }
        }
        return true
    }

    public func untouch(_ k: Int) { setSlot(k, value: 0, touched: false) }

    public func setName(_ n: String) {
        guard var s = sound else { return }
        let up = String(n.uppercased().unicodeScalars.filter { $0.value >= 0x20 && $0.value <= 0x7e }.map(Character.init))
        s.name = String(up.trimmingCharacters(in: .whitespaces).prefix(12))
        if s.name.isEmpty { s.name = "SOUND" }
        sound = s
    }

    public func toggleTag(_ k: Int) {
        guard var s = sound else { return }
        s.tags ^= 1 << k
        sound = s
    }

    private func nudge() {
        guard Date().timeIntervalSince(lastNudge) > 0.9 else { return }
        lastNudge = Date()
        nudges += 1
    }

    // MARK: REVERT, SAVE

    /// The sound it came from again; on the track (TRACK on): the track put back exactly (its DUMP), a preset opened:
    /// that preset again.
    public func revert() async {
        guard let b = base else { return }
        sound = b
        guard live, let a = actions else { return }
        if place.fromTarget, let f = baseFrame {
            _ = await a.revert(to: f)
        } else if let e = engine(), let f = try? await e.storeFrame(b, track: track, audition: true,
                                                                  extras: FizzTrackExtrasIn(id: b.extras?.id ?? 0, sound: b.extras?.sound ?? 0,
                                                                                            edited: b.extras?.edited ?? false)) {
            _ = await a.auditionOnTrack(f)
        }
    }

    /// SAVE (over the MY preset it came from: the screen asked first) or SAVE AS NEW (the first free place); TRACK on:
    /// the track then plays it (LOAD) and the editor reads it again.
    @discardableResult public func save(asNew: Bool) async -> UnitOutcome {
        if place.isKept { return await saveKept(asNew: asNew) }
        guard let s = sound, let a = actions, let e = engine(), !saving else { return .notNow }
        saving = true
        defer { saving = false }
        guard let fs = try? await e.libSound(s) else { return .refused(.badPatch) }
        let onTrack = live && track >= 0
        let x = await a.save(fs, slot: asNew ? 0 : place.slot, track: onTrack ? track : nil)
        guard case .placed(let n) = x else { return x }
        base = s
        if onTrack { await attach() } else {
            let row = unit?.mine.first { $0.slot == n }
            place = EditPlace(label: "MY PRESETS \(n)", source: "mine", slot: n, id: row?.id ?? 0)
        }
        return x
    }

    /// KEPT's SAVE (the preset's file written again, its source and web tags kept; + NEW: a new one) and SAVE AS NEW
    /// (another kept preset, its name made unique). Nothing goes to the unit.
    private func saveKept(asNew: Bool) async -> UnitOutcome {
        guard var s = sound, let store = unit?.kept, let e = engine(), !saving else { return .notNow }
        saving = true
        defer { saving = false }
        let old = place.kept.flatMap { store.item($0) }
        if asNew || old == nil {
            s.name = store.uniqueName(s.name)
        }
        guard let fs = try? await e.libSound(s) else { return .refused(.badPatch) }
        let k: KeptPreset?
        if !asNew, let old {
            k = try? await store.update(old) { f in
                var n = fs
                n.source = f.sound.source
                n.favourite = f.sound.favourite
                f.sound = n
            }
        } else {
            k = try? await store.add(fs)
        }
        guard let k else { return .notNow }
        sound = s
        base = s
        place = .kept(k.id)
        store.reveal = k.id
        say("\(s.name) -> KEPT (\(k.id))")
        return .ok
    }

    // MARK: the keybed

    /// The FM-1's 27 keys from F (F3 .. G5 at OCT 0): the lowest note.
    public var lowNote: Int { 53 + 12 * octave }

    public func setOctave(_ o: Int) { octave = max(-2, min(2, o)) }

    /// A note on the followed track's channel (its DUMP's; 14 until read).
    public func noteOn(_ n: Int) {
        guard !held.contains(n), let a = actions else { return }
        held.insert(n)
        let c = chan
        notes.run { await a.note(channel: c, note: n, velocity: 100) }
    }

    public func noteOff(_ n: Int) {
        guard held.contains(n), let a = actions else { return }
        held.remove(n)
        let c = chan
        notes.run { await a.note(channel: c, note: n, velocity: 0) }
    }

    /// Returns when the queued edits and notes went out (the tests).
    func settle() async {
        await edits.drain()
        await notes.drain()
        await actions?.pump?.idle()
    }
}

/// Jobs run one after another, in the order queued (the PARAMs and the notes keep their order).
@MainActor final class SerialQueue {
    private var jobs: [@MainActor () async -> Void] = []
    private var running = false

    func run(_ f: @escaping @MainActor () async -> Void) {
        jobs.append(f)
        guard !running else { return }
        running = true
        Task { @MainActor in
            while !jobs.isEmpty { await jobs.removeFirst()() }
            running = false
        }
    }

    func drain() async { while running { try? await Task.sleep(for: .milliseconds(2)) } }
}

// MARK: - The edit target (LIBRARY's EDIT, a file)

extension UnitModel {
    /// What EDIT edits next: set by LIBRARY's EDIT (TRACK on: heard on the track first) or a file opened, taken when
    /// EDIT shows.
    public var editTarget: EditTarget? {
        get { editor.request }
        set { editor.request = newValue }
    }
}

extension UnitActions {
    /// A sound onto the followed track, nothing stored (REVERT of a preset opened with TRACK on: live.js audition).
    func auditionOnTrack(_ frame: [UInt8]) async -> UnitOutcome {
        guard canWrite, trackOn, let link else { return .notNow }
        let r = try? await link.store(frame)
        await pump?.touch()
        return r.map { Self.outcome($0) } ?? .noAnswer
    }
}
