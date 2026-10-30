// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// Where a MIDI controller's messages go on PLAY (mock-up 55; RemoteFM1's PLAN.md section 10 for behaviour), in order:
//   1. learn, while it is armed (a control picked): CCs and note-ons bind (MidiLearn.take), and play nothing
//   2. the learned map (RFMMidiMap.cpp's MidiMapper through fzmm_*): a mapped source turns a knob, holds a button or
//      moves MASTER (with pickup, unless MASTER pickup is off) and is consumed
//   3. notes, when "MIDI notes play keys" is on: PLAY's keys (key k = note 53 + k, transposed -2..+2 octaves; in STEPS
//      the chromatic keybed's note), and also FIZZ's MIDI in with "...and reach FIZZ's MIDI in"; a note past the keys
//      goes to FIZZ's MIDI in
//   4. everything else (other channel messages, clock and transport) to FIZZ's MIDI in; SysEx and the rest dropped
// The panel it plays is a MidiPanelTarget (PlayModel: whichever FIZZ PLAY drives, THIS iPHONE's or the FM-1).
// Written for FIZZ: RemoteFM1's ControllerInput and engine are not used.

import CMidiMap
import FizzEngine
import Foundation

/// What PLAY's panel offers a controller.
@MainActor public protocol MidiPanelTarget: AnyObject {
    /// PLAY's key 0..26 (velocity 0: let go)
    func midiKey(_ k: Int, velocity: Int)
    /// STEPS: a note of the chromatic keybed (velocity 0: let go)
    func midiStepsNote(_ n: Int, velocity: Int)
    /// the keys are STEPS' notes (else FIZZ's 27 keys)
    var midiStepsKeys: Bool { get }
    /// one message to FIZZ's MIDI in
    func midiToFizz(_ bytes: [UInt8])
    func midiTurn(_ k: FizzKnob, _ detents: Int)
    func midiButton(_ b: FizzButton, down: Bool)
    /// MASTER, 0..1023
    func midiMaster(_ v: Int)
    /// a MASTER pot away from MASTER: where it is (0..1023); first: once per wait
    func midiPickup(pot: Int, first: Bool)
    /// MASTER now, 0..1023
    var midiMasterNow: Int { get }
}

@MainActor
public final class MidiRouter {
    /// What became of one message (the tests read it).
    public enum Route: Equatable, Sendable {
        case learn(MidiLearn.Result)
        case mapped
        case key(Int)
        case stepsNote(Int)
        case fizz
        case dropped
    }

    public struct Settings: Equatable, Sendable {
        public var notesPlayKeys = true
        /// the key notes go to FIZZ's MIDI in too
        public var reachFizz = false
        /// octaves, -2..2
        public var transpose = 0
        /// a learned MASTER pot waits until it reaches MASTER (off: it jumps)
        public var masterPickup = true
        public init() {}
    }

    public weak var target: (any MidiPanelTarget)?
    public let learn: MidiLearn
    public var settings = Settings()
    /// learn's outcome (the bar shows it)
    public var onLearn: ((MidiLearn.Result) -> Void)?

    nonisolated(unsafe) private let mapper: OpaquePointer
    /// each note's key while it is held (by channel and note), so its note-off lets go of that key whatever the
    /// transpose is by then
    private var noteKeys: [Int: Route] = [:]

    public init(learn: MidiLearn, target: (any MidiPanelTarget)?) {
        self.learn = learn
        self.target = target
        mapper = fzmm_new()
        learn.store.map.apply(to: mapper)
    }

    deinit { fzmm_free(mapper) }

    /// The store's map (a change, an import): the mapper takes it; what the old one held is let go.
    public func adopt(_ map: MidiMap) {
        let released = map.apply(to: mapper)
        for l in PanelFeed.Label.allCases where released & (1 << UInt32(l.rawValue)) != 0 {
            target?.midiButton(l.button, down: false)
        }
    }

    /// The panel let go of everything: the map holds nothing either.
    public func releaseAll() {
        fzmm_release(mapper)
        for (_, r) in noteKeys {
            if case .key(let k) = r { target?.midiKey(k, velocity: 0) }
            if case .stepsNote(let n) = r { target?.midiStepsNote(n, velocity: 0) }
        }
        noteKeys = [:]
    }

    /// One whole message from a controller.
    @discardableResult
    public func receive(_ d: [UInt8]) -> Route {
        guard let st = d.first, st >= 0x80 else { return .dropped }
        if st >= 0xF0 {
            // (clock, start, continue, stop; not SysEx, system common, active sensing or reset)
            guard [0xF8, 0xFA, 0xFB, 0xFC].contains(st) else { return .dropped }
            target?.midiToFizz([st])
            return .fizz
        }
        let kind = st & 0xF0
        let need = kind == 0xC0 || kind == 0xD0 ? 2 : 3
        guard d.count >= need else { return .dropped }
        let msg = Array(d.prefix(need))

        // 1. learn
        if learn.armed {
            if kind == 0xB0 { fzmm_seed(mapper, st & 15, msg[1] & 127, msg[2] & 127) }
            if let r = learn.take(msg) {
                onLearn?(r)
                return .learn(r)
            }
        }
        // 2. the learned map
        var act = fzmm_act_t()
        let master = settings.masterPickup ? Int32(target?.midiMasterNow ?? -1) : -1
        let took = msg.withUnsafeBufferPointer { fzmm_feed(mapper, $0.baseAddress, UInt32($0.count), master, &act) }
        if took != 0 {
            apply(act)
            return .mapped
        }
        // 3. notes
        if kind == 0x90 || kind == 0x80 {
            let on = kind == 0x90 && msg[2] > 0
            let id = Int(st & 15) << 7 | Int(msg[1] & 127)
            if on {
                if let old = noteKeys[id] { let _ = letGo(old) }
                let r = settings.notesPlayKeys ? keyRoute(Int(msg[1])) : .fizz
                switch r {
                case .key(let k): target?.midiKey(k, velocity: Int(msg[2]))
                case .stepsNote(let n): target?.midiStepsNote(n, velocity: Int(msg[2]))
                default: break
                }
                if r != .fizz { noteKeys[id] = r }
                if r == .fizz || settings.reachFizz { target?.midiToFizz(msg) }
                return r
            }
            if let r = noteKeys.removeValue(forKey: id) {
                let _ = letGo(r)
                if settings.reachFizz { target?.midiToFizz(msg) }
                return r
            }
        }
        // 4. FIZZ's MIDI in
        target?.midiToFizz(msg)
        return .fizz
    }

    /// The key (or STEPS' note) a note plays now, or .fizz when it is past the keys.
    public func keyRoute(_ note: Int) -> Route {
        let n = note + 12 * max(-2, min(2, settings.transpose))
        if target?.midiStepsKeys == true { return (0...127).contains(n) ? .stepsNote(n) : .fizz }
        let k = n - 53
        return (0..<27).contains(k) ? .key(k) : .fizz
    }

    private func letGo(_ r: Route) -> Bool {
        switch r {
        case .key(let k): target?.midiKey(k, velocity: 0)
        case .stepsNote(let n): target?.midiStepsNote(n, velocity: 0)
        default: return false
        }
        return true
    }

    private func apply(_ a: fzmm_act_t) {
        switch Int(a.kind) {
        case FZMM_TURN:
            if let k = FizzKnob(rawValue: Int(a.index)) { target?.midiTurn(k, Int(a.value)) }
        case FZMM_BUTTON:
            if let b = FizzButton(rawValue: Int(a.index)) { target?.midiButton(b, down: a.value != 0) }
        case FZMM_MASTER:
            target?.midiMaster(Int(a.value))
        case FZMM_PICKUP:
            target?.midiPickup(pot: Int(a.value), first: a.first != 0)
        default:
            break
        }
    }
}
