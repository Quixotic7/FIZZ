// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// The EDIT tab's sound (site/manager/sound.js, phase I5): FIZZ's 115 values, its name, tags, pan and sends, or a MIDI
// sound's 28 slots. The values <-> the 128-byte blob stay JavaScript (sound.js soundFromFrame, soundFromLib, blob6, in
// JavaScriptCore); the table (fizzvalues.js: names, ranges, texts) is read from it once, and the small rules the
// screen needs on every drag (text, clamp, the range's fraction, the destinations' order) are ported here and checked
// against Node's (FizzDataTests).

import Foundation
import FizzKit

/// One of FIZZ's values as its pages name and show it (fizzvalues.js VALUES).
public struct EditValue: Sendable, Equatable {
    public enum Rule: String, Sendable { case int, ofs, fix2, fix2s, list }
    public let name: String
    public let min: Int, max: Int, def: Int
    public let rule: Rule
    /// texts for some values (OFF, INF, NOTE ..)
    public let ex: [Int: String]
    /// a list's texts, min first
    public let list: [String]
    public var span: Int { max - min }
}

/// An algorithm (fizzvalues.js ALGOS): each operator's modulators (bit 0 C .. 3 B2), the feedback operator, the X and
/// Y outputs.
public struct EditAlgo: Sendable, Equatable {
    public let mods: [Int]
    public let feedback: Int
    public let x: Int, y: Int
    public var out: Int { x | y }
    public init(mods: [Int], feedback: Int, x: Int, y: Int) { self.mods = mods; self.feedback = feedback; self.x = x; self.y = y }
}

/// fizzvalues.js: the 115 values, the 8 algorithms, the LFO destinations' order.
public struct EditTable: Sendable, Equatable {
    public let values: [EditValue]
    public let algos: [EditAlgo]
    public let destOrder: [Int]
    /// fizz2dn.js Q: ALGO, RC, RA, .. by name
    public let q: [String: Int]
    /// The mix (sound.js MIX_IDS, info, panText): PAN and the sends under PARAM's ids 115..118, PAN's texts as a list
    /// (-64 first: L64 .. C .. R63); the sends 0..127.
    public let mix: [Int: EditValue]
    /// sound.js MIX_DEFAULTS: a track's mix (a sound without one gets it)
    public let mixDefaults: (pan: Int, sends: FizzSends)

    public var count: Int { values.count }
    public func id(_ name: String) -> Int { q[name] ?? -1 }

    public func info(_ id: Int) -> EditValue? { values.indices.contains(id) ? values[id] : mix[id] }

    public static func == (a: EditTable, b: EditTable) -> Bool {
        a.values == b.values && a.algos == b.algos && a.destOrder == b.destOrder && a.q == b.q && a.mix == b.mix
            && a.mixDefaults == b.mixDefaults
    }

    /// sound.js text(id, v)
    public func text(_ id: Int, _ v: Int) -> String {
        guard let r = info(id) else { return String(v) }
        if r.rule == .list { let k = v - r.min; return r.list.indices.contains(k) ? r.list[k] : String(v) }
        if let e = r.ex[v] { return e }
        let a = abs(v), f2 = "\(a / 100).\(a % 100 < 10 ? "0" : "")\(a % 100)"
        switch r.rule {
        case .fix2: return f2
        case .fix2s: return (v < 0 ? "-" : "+") + f2
        case .ofs: return v > 0 ? "+\(v)" : String(v)
        default: return String(v)
        }
    }

    /// sound.js clampV
    public func clamp(_ id: Int, _ v: Int) -> Int {
        guard let r = info(id) else { return v }
        return Swift.max(r.min, Swift.min(r.max, v))
    }

    /// How far through its range (0..1), and back (sound.js frac, fromFrac).
    public func frac(_ id: Int, _ v: Int) -> Double {
        guard let r = info(id) else { return 0 }
        return Double(v - r.min) / Double(Swift.max(1, r.span))
    }
    public func fromFrac(_ id: Int, _ f: Double) -> Int {
        guard let r = info(id) else { return 0 }
        return clamp(id, Int((Double(r.min) + f * Double(r.span)).rounded()))
    }

    /// A hundredths value (fix2, fix2s).
    public func isHundredths(_ id: Int) -> Bool { info(id).map { $0.rule == .fix2 || $0.rule == .fix2s } ?? false }

    /// An LFO's or a modulation slot's destination (editor.js isDest; the VOICE page's 110.. are not).
    public func isDest(_ id: Int) -> Bool {
        (47...70).contains(id) && (id - 47) % 8 == 3 || (86...109).contains(id) && (id - 86) % 3 == 1
    }

    /// A destination stepped in the editor's (the device's) order.
    public func destStep(_ x: Int, _ d: Int) -> Int {
        let i = destOrder.firstIndex(of: x) ?? 0
        return destOrder[Swift.max(0, Swift.min(destOrder.count - 1, i + d))]
    }

    /// The values a long list is picked from, in the editor's order.
    public func order(_ id: Int) -> [Int] {
        if isDest(id) { return destOrder }
        guard let r = info(id) else { return [] }
        return Array(r.min...r.max)
    }

    public var defaults: [Int] { values.map(\.def) }

    init(js v: FizzJSValue) {
        values = (v["values"]?.array ?? []).map { r in
            let ex = r[5]
            var map: [Int: String] = [:]
            for m in ex?.members ?? [] { if let k = Int(m.key), let s = m.value.string { map[k] = s } }
            return EditValue(name: r[0]?.string ?? "", min: r[1]?.int ?? 0, max: r[2]?.int ?? 0, def: r[3]?.int ?? 0,
                             rule: EditValue.Rule(rawValue: r[4]?.string ?? "") ?? .int, ex: map,
                             list: ex?.array?.compactMap(\.string) ?? [])
        }
        algos = (v["algos"]?.array ?? []).map { a in
            EditAlgo(mods: a[0]?.array?.compactMap(\.int) ?? [0, 0, 0, 0], feedback: a[1]?.int ?? 4, x: a[2]?.int ?? 0, y: a[3]?.int ?? 0)
        }
        destOrder = v["destOrder"]?.array?.compactMap(\.int) ?? []
        var q: [String: Int] = [:]
        for m in v["q"]?.members ?? [] { if let n = m.value.int { q[m.key] = n } }
        self.q = q
        let pans = v["mix"]?["panTexts"]?.array?.compactMap(\.string) ?? []
        var mix: [Int: EditValue] = [:]
        for r in v["mix"]?["rows"]?.array ?? [] {
            guard let id = r["id"]?.int else { continue }
            let pan = r["rule"]?.string == "pan"
            mix[id] = EditValue(name: r["name"]?.string ?? "", min: r["min"]?.int ?? 0, max: r["max"]?.int ?? 127,
                                def: r["def"]?.int ?? 0, rule: pan ? .list : .int, ex: [:], list: pan ? pans : [])
        }
        self.mix = mix
        let d = v["mix"]?["defaults"]
        mixDefaults = (d?["pan"]?.int ?? 0, FizzSends(chorus: d?["chorus"]?.int ?? 0, delay: d?["delay"]?.int ?? 0,
                                                     reverb: d?["reverb"]?.int ?? 20))
    }
}

/// PARAM's ids for the mix (sound.js MIX_IDS; core/seq/pattern.h QD_LK_*).
public enum EditMix {
    public static let pan = 115, delay = 116, reverb = 117, chorus = 118
    public static func isMix(_ id: Int) -> Bool { (115...118).contains(id) }
}

/// A MIDI sound's slot (FIZZ MIDI (e)): its CC number, its value, touched (untouched: "—", nothing sent).
public struct EditMidiSlot: Sendable, Equatable {
    public var cc: Int, value: Int, touched: Bool
    public init(cc: Int, value: Int, touched: Bool) { self.cc = cc; self.value = value; self.touched = touched }
}

/// What a track's DUMP carries after the sound (sound.js extrasOf).
public struct EditExtras: Sendable, Equatable {
    public var id: Int, sound: Int
    public var edited: Bool, selected: Bool, rec: Bool, playing: Bool, browse: Bool
    public var chan: Int, sel: Int
    public init(id: Int = 0, sound: Int = 0, edited: Bool = false, selected: Bool = false, rec: Bool = false,
                playing: Bool = false, browse: Bool = false, chan: Int = 14, sel: Int = 0) {
        self.id = id; self.sound = sound; self.edited = edited; self.selected = selected; self.rec = rec
        self.playing = playing; self.browse = browse; self.chan = chan; self.sel = sel
    }
}

/// The editor's sound (sound.js): the values, or a MIDI sound's slots (its values then FM TONE's defaults).
public struct EditSound: Sendable, Equatable {
    public var name: String
    public var values: [Int]
    public var pan: Int
    public var sends: FizzSends
    /// FIZZ's tags, bit k
    public var tags: Int
    public var midi: [EditMidiSlot]?
    /// "mine", "factory", "pool", "track", "file", "new"
    public var source: String
    public var slot: Int
    /// a track's DUMP: its track (0..5), else -1
    public var track: Int
    public var extras: EditExtras?

    public init(name: String, values: [Int], pan: Int = 0, sends: FizzSends = .init(), tags: Int = 0,
                midi: [EditMidiSlot]? = nil, source: String = "new", slot: Int = 0, track: Int = -1, extras: EditExtras? = nil) {
        self.name = name; self.values = values; self.pan = pan; self.sends = sends; self.tags = tags; self.midi = midi
        self.source = source; self.slot = slot; self.track = track; self.extras = extras
    }

    public var isMidi: Bool { midi != nil }

    /// sound.js mixValue / setMix / sameMix: PAN and the sends by PARAM id (115..118).
    public func mixValue(_ id: Int) -> Int {
        switch id {
        case EditMix.pan: pan
        case EditMix.delay: sends.delay
        case EditMix.reverb: sends.reverb
        case EditMix.chorus: sends.chorus
        default: 0
        }
    }
    public mutating func setMix(_ id: Int, _ v: Int) {
        switch id {
        case EditMix.pan: pan = v
        case EditMix.delay: sends.delay = v
        case EditMix.reverb: sends.reverb = v
        case EditMix.chorus: sends.chorus = v
        default: break
        }
    }
    public func sameMix(_ o: EditSound) -> Bool { pan == o.pan && sends == o.sends }

    /// sound.js sameValues: the same values (a MIDI sound: its slots; a MIDI and an FM TONE sound never)
    public func sameValues(_ o: EditSound) -> Bool {
        (midi == nil) == (o.midi == nil) && (midi.map { $0 == o.midi } ?? (values == o.values))
    }

    init(js v: FizzJSValue) {
        name = v["name"]?.string ?? ""
        values = v["values"]?.array?.compactMap(\.int) ?? []
        pan = v["pan"]?.int ?? 0
        sends = FizzSends(v["sends"])
        tags = v["tags"]?.int ?? 0
        midi = v["midi"]?["slots"]?.array.map { $0.map { EditMidiSlot(cc: $0["cc"]?.int ?? 0, value: $0["value"]?.int ?? 0, touched: $0["touched"]?.bool ?? false) } }
        source = v["source"]?.string ?? "new"
        slot = v["slot"]?.int ?? 0
        track = v["track"]?.int ?? -1
        extras = v["extras"]?.nonNull.map { x in
            EditExtras(id: x["id"]?.int ?? 0, sound: x["sound"]?.int ?? 0, edited: x["edited"]?.bool ?? false,
                       selected: x["selected"]?.bool ?? false, rec: x["rec"]?.bool ?? false, playing: x["playing"]?.bool ?? false,
                       browse: x["browse"]?.bool ?? false, chan: x["chan"]?.int ?? 14, sel: x["sel"]?.int ?? 0)
        }
    }

    /// What blob6 needs: the values and the MIDI slots.
    var packJS: FizzJSValue {
        var m: [FizzJSMember] = [.init(key: "values", value: .array(values.map { .int($0) }))]
        if let midi {
            m.append(.init(key: "midi", value: ["slots": .array(midi.map { ["cc": .int($0.cc), "value": .int($0.value), "touched": .bool($0.touched)] })]))
        }
        return .object(m)
    }
}

extension FizzDataEngine {
    /// fizzvalues.js: names, ranges, texts, algorithms, the destinations' order.
    public func editTable() async throws -> EditTable { EditTable(js: try await call("editTable")) }

    /// sound.js soundFromFrame: a DUMP reply (MY PRESETS, the factory's, the pool's, a track's) as the editor's sound.
    public func editSound(frame: [UInt8]) async throws -> EditSound? {
        try await call("soundFromFrame", [.bytes(frame)]).nonNull.map(EditSound.init(js:))
    }

    /// sound.js soundFromLib: a .fizzsnd / a backup's preset as the editor's sound.
    public func editSound(lib s: FizzSound) async throws -> EditSound { EditSound(js: try await call("soundFromLib", [s.js])) }

    /// sound.js blob6: the sound packed back into FIZZ's version-6 blob, and its DRIVE.
    public func editPack(_ s: EditSound) async throws -> (blob: [UInt8], drive: Int) {
        let r = try await call("editPack", [s.packJS])
        guard let b = r["blob"]?.bytes else { throw FizzDataError.bridge("editPack: not bytes") }
        return (b, r["drive"]?.int ?? 0)
    }

    /// The sound in js/library.js's shape (SAVE, EXPORT): its blob packed, its tags by name.
    public func libSound(_ s: EditSound, source: FizzJSValue = [:]) async throws -> FizzSound {
        let p = try await editPack(s)
        return FizzSound(name: s.name, blob: p.blob, drive: p.drive, pan: s.pan, sends: s.sends,
                         tags: tagNames.enumerated().filter { (s.tags >> $0.offset) & 1 == 1 }.map(\.element), source: source)
    }

    /// sound.js storeFrameOf: a STORE6 frame of the sound (slot: MY PRESETS 1..256, 0 the first free; track: LOAD;
    /// own: the owner's preset; audition: onto the track only; extras: the track's preset id, pool sound, edited mark).
    public func storeFrame(_ s: EditSound, slot: Int = 0, track: Int? = nil, own: Bool = false, audition: Bool = false,
                           extras: FizzTrackExtrasIn? = nil) async throws -> [UInt8] {
        let p = try await editPack(s)
        return try FizzSysEx.storeSound(name: s.name, blob: p.blob, drive: p.drive, slot: slot, track: track, pan: s.pan,
                                        chorus: s.sends.chorus, delay: s.sends.delay, reverb: s.sends.reverb,
                                        tags: .mask(s.tags), audition: audition, own: own, extras: extras)
    }

    /// sound.js newSound: + NEW's sound ("fm": FM TONE's defaults, named INIT; "midi": a new MIDI sound, named MIDI, its
    /// 28 slots untouched, slot k's CC 16 + k), the mix a track's defaults (reverb 20).
    public func newSound(midi: Bool) async throws -> EditSound {
        EditSound(js: try await call("newSound", [.string(midi ? "midi" : "fm")]))
    }

    /// sound.js text for each pair (the tests).
    func editTexts(_ pairs: [(Int, Int)]) async throws -> [String] {
        try await call("editTexts", [.array(pairs.map { [.int($0.0), .int($0.1)] })]).array?.compactMap(\.string) ?? []
    }
}
