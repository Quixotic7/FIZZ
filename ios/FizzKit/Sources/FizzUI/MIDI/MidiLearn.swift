// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2026 RemoteFM1 contributors
// Derived from RemoteFM1IOS af91bc5 RFMKit/Sources/RFMKit/MIDI/MidiLearn.swift, adapted: no engine. FIZZ's controller
// input (MidiRouter.swift) hands learn each message itself while it is armed (`armed`), instead of RemoteFM1's engine
// diverting them to a queue (rfme_learn, poll()); the store is FIZZ's MidiMapStore. take() is unchanged.
// MIDI learn (M8): js/midi-map.js Learn. A control picked on the panel, then the next CC or note binds to it; while
// the control keeps moving, the mode is guessed again from its values. While learn is on and a control is picked, CCs
// and note-ons from a controller go to take() and play nothing.

import Foundation

@MainActor
public final class MidiLearn {
    public enum Result: Equatable, Sendable {
        case bound(MidiMapping, fresh: Bool)    // fresh: just bound (else the mode guess changed as it kept moving)
        case rejected(String)                   // a note on a knob: why
        case none                               // consumed, nothing new
    }

    public let store: MidiMapStore
    public private(set) var active = false
    public private(set) var selected: String?
    private var cur: (src: String, target: String, values: [Int])?

    public init(store: MidiMapStore) {
        self.store = store
    }

    /// learn takes the controller's messages (on, and a control picked)
    public var armed: Bool { active && selected != nil }

    public func start() {
        active = true
        selected = nil
        cur = nil
    }

    public func stop() {
        active = false
        selected = nil
        cur = nil
    }

    /// a target id (MidiTarget.id) to bind to, or nil
    public func select(_ target: String?) {
        selected = target.flatMap { MidiTarget.named($0) != nil ? $0 : nil }
        cur = nil
    }

    /// the picked control's bindings removed; how many
    @discardableResult public func forgetSelected() -> Int { selected.map { store.forget($0) } ?? 0 }

    /// what learn makes of one message; nil: it leaves it alone (nothing picked, or a note-off of a source not bound)
    public func take(_ d: [UInt8]) -> Result? {
        guard active, let sel = selected, let m = MidiMessage(d) else { return nil }
        let key = m.source
        if m.kind == .note && !m.on { return store.map.has(key) ? Result.none : nil }
        let why = MidiMap.canBind(sel, m)
        if !why.isEmpty { return .rejected(why) }
        if var c = cur, c.src == key, c.target == sel {
            if c.values.count < 24 { c.values.append(m.value) }
            cur = c
            guard let mp = store.map.get(key), let mode = mp.mode else { return Result.none }
            let g = MidiMode.guess(c.values)
            if g == mode { return Result.none }
            store.setMode(key, g)
            return store.map.get(key).map { .bound($0, fresh: false) } ?? Result.none
        }
        let isEnc = MidiTarget.named(sel)?.isEncoder == true
        let mode: MidiMode? = isEnc && m.kind == .cc ? MidiMode.guess([m.value]) : nil
        guard let mp = store.bind(key, to: sel, mode: mode) else { return Result.none }
        cur = (key, sel, [m.value])
        return .bound(mp, fresh: true)
    }
}
