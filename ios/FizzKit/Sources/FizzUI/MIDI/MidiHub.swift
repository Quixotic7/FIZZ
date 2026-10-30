// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// PLAY's MIDI (mock-ups 55, 56): the controller input (MidiInput) into the router (MidiRouter), the learned map
// (MidiMapStore, MidiMap) and learn (MidiLearn) with its state on the panel (the control picked, what it bound, MASTER's
// pickup mark and toast), the settings of the MIDI & audio page, and the OMX-27 (OmxLink, REMOTE mode). PlayModel owns
// one (`play.midi`), started the first time PLAY shows; PLAY's panel is the target of all of them (the extension at
// the foot: the FIZZ PLAY drives, as PLAY's own keys and knobs do).

import FizzEngine
import FizzKit
import Foundation
import Observation

@MainActor @Observable
public final class MidiHub {
    public let store: MidiMapStore
    public let input: MidiInput
    public let omx: OmxLink
    @ObservationIgnored public let learn: MidiLearn
    @ObservationIgnored public let router: MidiRouter
    @ObservationIgnored private let defaults: UserDefaults
    @ObservationIgnored private weak var play: PlayModel?
    @ObservationIgnored private var started = false
    @ObservationIgnored private var toastTask: Task<Void, Never>?
    @ObservationIgnored private var pickupTask: Task<Void, Never>?

    // MARK: the page's settings (kept)

    public var notesPlayKeys: Bool { didSet { keep(); defaults.set(notesPlayKeys, forKey: "play.midi.keys") } }
    public var reachFizz: Bool { didSet { keep(); defaults.set(reachFizz, forKey: "play.midi.reach") } }
    /// octaves, -2..2
    public var transpose: Int { didSet { keep(); defaults.set(transpose, forKey: "play.midi.transpose") } }
    public var masterPickup: Bool {
        didSet {
            keep()
            defaults.set(masterPickup, forKey: "play.midi.pickup")
            if !masterPickup { pickupPot = nil }
        }
    }

    // MARK: learn on the panel (56)

    /// learn mode: the learnable controls outlined, a tap picks one
    public private(set) var learning = false
    /// the control picked (its target id: MidiTarget.id)
    public private(set) var picked: String?
    /// what the picked control was just bound to
    public private(set) var bound: MidiMapping?
    /// why the last message could not bind (a note on a knob)
    public private(set) var refused: String?
    /// a learned MASTER pot away from MASTER: where it is (0..1023), for its mark
    public private(set) var pickupPot: Int?
    /// a moment's message over the panel (ok: a success)
    public private(set) var toast: (text: String, ok: Bool)?

    init(play: PlayModel?, defaults: UserDefaults, store: MidiMapStore? = nil, input: MidiInput? = nil,
         omx: OmxLink? = nil) {
        self.play = play
        self.defaults = defaults
        let s = store ?? MidiMapStore()
        self.store = s
        self.input = input ?? MidiInput(defaults: defaults)
        self.omx = omx ?? OmxLink(defaults: defaults)
        learn = MidiLearn(store: s)
        router = MidiRouter(learn: learn, target: play)
        notesPlayKeys = defaults.object(forKey: "play.midi.keys") as? Bool ?? true
        reachFizz = defaults.bool(forKey: "play.midi.reach")
        transpose = max(-2, min(2, defaults.integer(forKey: "play.midi.transpose")))
        masterPickup = defaults.object(forKey: "play.midi.pickup") as? Bool ?? true
        keep()
        s.onChange = { [weak self] m in self?.router.adopt(m) }
        self.input.onMessage = { [weak self] m in self?.router.receive(m) }
        router.onLearn = { [weak self] r in self?.learned(r) }
        self.omx.target = play
        self.omx.onToast = { [weak self] t, ok in self?.show(t, ok: ok) }
        self.omx.onActive = { [weak self] _ in self?.skipAgain() }
    }

    private func keep() {
        var s = MidiRouter.Settings()
        s.notesPlayKeys = notesPlayKeys
        s.reachFizz = reachFizz
        s.transpose = transpose
        s.masterPickup = masterPickup
        router.settings = s
    }

    /// CoreMIDI's sources listened to, and the OMX-27 connected again when it was last time (PLAY's first showing).
    public func start() {
        guard !started else { return }
        started = true
        skipAgain()
        input.start()
        omx.auto()
    }

    /// The sources never listened to, as they are now: the unit in use, the panel, the OMX-27 in REMOTE mode.
    public func skipAgain() {
        var k = MidiInput.Skip()
        if let u = play?.unit?.unit, !u.isEmulated {
            k.unitRef = u.candidate.source.ref
            k.unitName = u.portName
        }
        k.panelName = play?.host?.panelPort
        if omx.active, let p = omx.pair {
            k.omxID = p.input.id
            k.omxName = p.input.name
        }
        input.skip = k
    }

    // MARK: learn

    /// Learn (the page's button): the sheet goes, the panel's controls are outlined.
    public func startLearn() {
        play?.sheet = false
        play?.letGoAll()
        learning = true
        picked = nil
        bound = nil
        refused = nil
        pickupPot = nil
        learn.start()
    }

    /// A control tapped in learn mode (its target id).
    public func pick(_ target: String) {
        guard learning, MidiTarget.named(target) != nil else { return }
        picked = target
        bound = nil
        refused = nil
        learn.select(target)
    }

    /// Forget: the picked control's bindings gone.
    public func forget() {
        learn.forgetSelected()
        bound = nil
    }

    /// Done.
    public func doneLearn() {
        learn.stop()
        learning = false
        picked = nil
        bound = nil
        refused = nil
    }

    private func learned(_ r: MidiLearn.Result) {
        switch r {
        case .bound(let mp, _): bound = mp; refused = nil
        case .rejected(let why): refused = why
        case .none: break
        }
    }

    /// The learn bar's line.
    public var learnLine: String {
        guard let t = picked else { return "Tap a knob or a button on the panel" }
        let name = Self.name(t)
        if let refused { return refused.replacingOccurrences(of: t, with: name) }
        if let b = bound { return "\(name) ← \(MidiSource.label(b.src))" }
        return MidiTarget.named(t)?.isButton == true ? "\(name): now press a button or a key on your MIDI device"
            : "\(name): now turn a knob or a pot on your MIDI device"
    }

    /// Forget is offered (with the binding just made): the picked control has bindings.
    public var canForget: Bool { picked.map { !store.sourcesOf($0).isEmpty } ?? false }

    /// A target's name on FIZZ's panel: the knobs as printed, a button by FIZZ's label (SEL is TRACKS, OCT- is F1).
    public static func name(_ id: String) -> String {
        guard let t = MidiTarget.named(id), case .button(let l) = t.kind else { return id }
        return PlayInput.name(l.button).replacingOccurrences(of: "\n", with: " ")
    }

    /// A PLAY control's target id: a knob by its label, a button by its FizzButton.
    public static func target(knob label: String) -> String? { MidiTarget.named(label)?.id }
    public static func target(_ b: FizzButton) -> String { MidiTarget.button(PanelFeed.Label(b)).id }

    /// How a control shows in learn mode: learnable (dashed), bound (solid), picked (thick, pulsing).
    public enum Mark: Sendable { case learnable, bound, picked }
    public func mark(_ target: String) -> Mark {
        picked == target ? .picked : store.map.boundTargets.contains(target) ? .bound : .learnable
    }

    // MARK: MASTER's pickup, the toast

    func pickup(pot: Int, first: Bool) {
        pickupPot = pot
        if first, let m = play?.master {
            show("MASTER: turn the pot to \(Int((Double(m) / 1023 * 100).rounded()))% to pick it up")
        }
        pickupTask?.cancel()
        pickupTask = Task { [weak self] in
            try? await Task.sleep(for: .seconds(4))
            if !Task.isCancelled { self?.pickupPot = nil }
        }
    }

    func pickedUp() {
        if pickupPot != nil { pickupPot = nil }
    }

    public func show(_ text: String, ok: Bool = false) {
        toast = (text, ok)
        toastTask?.cancel()
        toastTask = Task { [weak self] in
            try? await Task.sleep(for: .seconds(2.6))
            if !Task.isCancelled { self?.toast = nil }
        }
    }

    // MARK: the snapshots

    /// Learn mode as drawn (56): `picked` picked, `bound` its binding; or after Done, MASTER waiting at `pot`.
    public func demoLearn(picked: String?, bound: MidiMapping? = nil) {
        learning = true
        learn.start()
        self.picked = picked
        if let picked { learn.select(picked) }
        self.bound = bound
    }

    public func demoPickup(pot: Int, toast: String) {
        pickupPot = pot
        self.toast = (toast, false)
    }
}

// MARK: - PLAY's panel as the target

extension PlayModel: MidiPanelTarget {
    public func midiKey(_ k: Int, velocity: Int) {
        if velocity > 0 { keyDown(k, velocity: velocity) } else { keyUp(k) }
    }

    public func midiStepsNote(_ n: Int, velocity: Int) {
        if velocity > 0 { noteDown(n, velocity: velocity) } else { noteUp(n) }
    }

    public var midiStepsKeys: Bool { view == .steps }

    public func midiToFizz(_ bytes: [UInt8]) {
        if soundNow == .fm1, let u = unit, let link = u.link {
            let before = fm1Sends
            fm1Sends = Task { @MainActor in
                await before?.value
                try? await link.midiIn(bytes)
            }
        } else {
            emulator?.midiIn(bytes)
        }
    }

    public func midiTurn(_ k: FizzKnob, _ detents: Int) {
        turn(k, by: detents, haptic: false)
        midi.omx.knobUsed(PanelFeed.Knob(k))
    }

    public func midiButton(_ b: FizzButton, down: Bool) {
        if down { press(b, haptic: false) } else { release(b) }
    }

    public func midiMaster(_ v: Int) {
        setMaster(v, haptic: false)
        midi.pickedUp()
    }

    public func midiPickup(pot: Int, first: Bool) { midi.pickup(pot: pot, first: first) }

    public var midiMasterNow: Int { master }
}

// MARK: - The OMX-27's target

extension PlayModel: OmxTarget {
    public func omxKey(_ k: Int, down: Bool) {
        if down { keyDown(k, velocity: OmxViews.velocity) } else { keyUp(k) }
    }

    public func omxButton(_ l: PanelFeed.Label, down: Bool) {
        if down { press(l.button, haptic: false) } else { release(l.button) }
    }

    public func omxTurn(_ k: PanelFeed.Knob, _ detents: Int) {
        if let fk = k.knob { turn(fk, by: detents, haptic: false) }
    }

    public func omxSelect(_ k: PanelFeed.Knob) {}

    public func omxMaster(_ v: Int) {
        setMaster(v, haptic: false)
        midi.pickedUp()
    }

    public func omxPickup(pot: Int, at: Int, first: Bool) { midi.pickup(pot: pot, first: first) }

    public func omxFirmwareMIDI(_ bytes: [UInt8]) { midiToFizz(bytes) }

    public var omxMasterNow: Int { master }

    public func omxKeyLevel(_ k: Int) -> UInt8 { leds.map { $0.keys.indices.contains(k) ? $0.keys[k] : 0 } ?? 0 }

    public func omxReleaseAll() { letGoAll() }
}
