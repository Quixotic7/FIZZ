// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2026 RemoteFM1 contributors
// Derived from RemoteFM1IOS af91bc5 RFMKit/Sources/RFMKit/MIDI/MidiMap.swift, adapted: the numbers from FIZZ's
// CMidiMap (FizzMidiMap.h), apply(to:) gives the map to FIZZ's mapper (fzmm_adopt) instead of RemoteFM1's engine.
// MIDI learn's map (M8): a controller's knobs, pots and buttons on the panel's knobs, MASTER and buttons. The website's
// js/midi-map.js, with its ids and stored form, so a map reads the same in both:
//   sources   "cc:<channel>:<number>" or "note:<channel>:<number>", channel 1..16 (notes: buttons only)
//   targets   SELECT ALGORITHM PRESETS KN1..KN4 (encoders), MASTER, and the 14 buttons by their printed names
//   modes     a CC on an encoder: abs, rel64, rel2c, relsign (rfme_map_entry_t's RFME_MAP_*)
//   stored    {"midiMap": [{src, target, mode?}], "knobSpeed": 0.5 | 1 | 2}
// Only the list lives here; RFMMidiMap.cpp's MidiMapper runs it (midi-map.js MidiMap.feed), fed by FIZZ's controller
// input (MidiRouter.swift).

import Foundation
import CMidiMap

/// how a CC turns an encoder
public enum MidiMode: String, CaseIterable, Sendable {
    case abs, rel64, rel2c, relsign

    /// midi-map.js MODES
    public var label: String {
        switch self {
        case .abs: "absolute"
        case .rel64: "relative 64±"
        case .rel2c: "relative 2's comp."
        case .relsign: "relative sign bit"
        }
    }

    var engine: UInt8 {
        switch self {
        case .abs: UInt8(RFME_MAP_ABS)
        case .rel64: UInt8(RFME_MAP_REL64)
        case .rel2c: UInt8(RFME_MAP_REL2C)
        case .relsign: UInt8(RFME_MAP_RELSIGN)
        }
    }

    /// the mode, from the values a CC sent while it moved (learn). A pot sweeping (four or more values, each different,
    /// one way) is absolute; values hugging 64 are binary offset; values only in 1..15 or 113..127 two's complement
    public static func guess(_ values: [Int]) -> MidiMode {
        let v = values
        guard !v.isEmpty else { return .abs }
        func sign(_ x: Int) -> Int { x > 0 ? 1 : x < 0 ? -1 : 0 }
        var sweep = v.count >= 4
        var i = 2
        while i < v.count && sweep { sweep = sign(v[i] - v[i - 1]) == sign(v[1] - v[0]); i += 1 }
        i = 1
        while i < v.count && sweep { sweep = v[i] != v[i - 1]; i += 1 }
        if sweep { return .abs }
        if v.contains(where: { $0 != 64 }) && v.allSatisfy({ Swift.abs($0 - 64) <= 7 }) { return .rel64 }
        if v.allSatisfy({ (1...15).contains($0) || (113...127).contains($0) }) { return .rel2c }
        return .abs
    }
}

/// a control a source can drive
public struct MidiTarget: Hashable, Sendable {
    public enum Kind: Hashable, Sendable {
        case encoder(PanelFeed.Knob)
        case master
        case button(PanelFeed.Label)
    }

    public let id: String
    public let kind: Kind

    /// midi-map.js TARGETS: the 7 encoders, MASTER, the 14 buttons (panel.js PANEL_LABELS)
    public static let all: [MidiTarget] = {
        let enc: [(String, PanelFeed.Knob)] = [("SELECT", .select), ("ALGORITHM", .algorithm), ("PRESETS", .presets),
                                               ("KN1", .k1), ("KN2", .k2), ("KN3", .k3), ("KN4", .k4)]
        return enc.map { MidiTarget(id: $0.0, kind: .encoder($0.1)) } + [MidiTarget(id: "MASTER", kind: .master)]
            + PanelFeed.Label.allCases.map { MidiTarget(id: buttonNames[$0.rawValue], kind: .button($0)) }
    }()

    /// panel.js PANEL_LABELS (the printed names; plain "OCT-", not the legend's minus sign)
    static let buttonNames = ["FX", "SEL", "ENV", "LFO", "EDIT", "GLO", "HOME", "SAVE", "ARP", "SEQ", "PLAY", "REC",
                              "OCT-", "OCT+"]
    private static let byID = Dictionary(uniqueKeysWithValues: all.map { ($0.id, $0) })

    /// targetOf
    public static func named(_ id: String) -> MidiTarget? { byID[id] }
    /// knobTarget: the panel's knob (MASTER too)
    public static func knob(_ k: PanelFeed.Knob) -> MidiTarget {
        k == .master ? named("MASTER")! : all.first { $0.kind == .encoder(k) }!
    }
    /// buttonTarget
    public static func button(_ l: PanelFeed.Label) -> MidiTarget { named(buttonNames[l.rawValue])! }

    public var isButton: Bool { if case .button = kind { true } else { false } }
    public var isEncoder: Bool { if case .encoder = kind { true } else { false } }

    /// rfme_map_entry_t.target
    var engine: UInt8 {
        switch kind {
        case .encoder(let k): UInt8(k.rawValue)
        case .master: UInt8(RFME_T_MASTER)
        case .button(let l): UInt8(RFME_T_BUTTON) + UInt8(l.rawValue)
        }
    }
}

/// a channel message, as learn and the map read it (midi-map.js parseMsg)
public struct MidiMessage: Equatable, Sendable {
    public enum Kind: String, Sendable { case cc, note }
    public var kind: Kind
    public var channel: Int                     // 1..16
    public var number: Int
    public var value: Int
    public var on = false                       // a note-on (at velocity 0 it is a note-off)

    /// a CC or a note (3 bytes); nil otherwise
    public init?(_ d: [UInt8]) {
        guard d.count >= 3 else { return nil }
        let s = d[0] & 0xF0
        channel = Int(d[0] & 15) + 1
        number = Int(d[1] & 127)
        value = Int(d[2] & 127)
        if s == 0xB0 {
            kind = .cc
        } else if s == 0x90 || s == 0x80 {
            kind = .note
            on = s == 0x90 && d[2] > 0
        } else {
            return nil
        }
    }

    /// srcKey: "cc:1:21"
    public var source: String { "\(kind.rawValue):\(channel):\(number)" }
}

/// the source ids
public enum MidiSource {
    /// srcLabel: "CC 21 ch 1"; long: "CC 21, channel 1" (no break before the channel)
    public static func label(_ key: String, long: Bool = false) -> String {
        let p = key.split(separator: ":", omittingEmptySubsequences: false).map(String.init)
        let type = p.first ?? "", ch = p.count > 1 ? p[1] : "", num = p.count > 2 ? p[2] : ""
        return "\(type == "cc" ? "CC" : "note") \(num)\(long ? ",\u{a0}channel\u{a0}" : " ch ")\(ch)"
    }

    /// validSrc: cc or note, channel 1..16, number 0..127, written plainly
    public static func isValid(_ key: String) -> Bool { parse(key) != nil }

    static func parse(_ key: String) -> (note: Bool, channel: Int, number: Int)? {
        let p = key.split(separator: ":", omittingEmptySubsequences: false).map(String.init)
        guard p.count == 3, p[0] == "cc" || p[0] == "note", plain(p[1]), plain(p[2]),
              let ch = Int(p[1]), let n = Int(p[2]), (1...16).contains(ch), (0...127).contains(n) else { return nil }
        return (p[0] == "note", ch, n)
    }

    private static func plain(_ s: String) -> Bool {     // (digits, no leading zero)
        !s.isEmpty && s.allSatisfy { $0.isASCII && $0.isNumber } && (s.count == 1 || s.first != "0")
    }
}

/// one binding: a source on a target; the mode only for an encoder's CC
public struct MidiMapping: Equatable, Hashable, Sendable, Identifiable {
    public var src: String
    public var target: String
    public var mode: MidiMode?
    public var id: String { src }

    public init(src: String, target: String, mode: MidiMode? = nil) {
        self.src = src
        self.target = target
        self.mode = mode
    }

    /// one line of the list: "KN1 ← CC 21 ch 1 · absolute"
    public var description: String { "\(target) ← \(MidiSource.label(src))\(mode.map { " · " + $0.label } ?? "")" }
}

/// the learned map: the bindings in order (the newest last) and the knob speed
public struct MidiMap: Equatable, Sendable {
    public static let speeds: [Double] = [0.5, 1, 2]
    public static func speedOf(_ v: Double) -> Double { speeds.contains(v) ? v : 1 }

    public private(set) var mappings: [MidiMapping] = []
    public private(set) var speed: Double = 1

    public init(_ list: [MidiMapping] = [], speed: Double = 1) {
        self.speed = Self.speedOf(speed)
        load(list.map { ($0.src, $0.target, $0.mode?.rawValue) })
    }

    // the stored list, checked (anything odd is left out; a source twice: the later binding in the first's place)
    private mutating func load(_ list: [(String?, String?, String?)]) {
        mappings = []
        for (src, target, mode) in list {
            guard let src, let target, MidiSource.isValid(src), let t = MidiTarget.named(target) else { continue }
            if src.hasPrefix("note:") && !t.isButton { continue }
            let def = Self.modeFor(t, src)
            let mp = MidiMapping(src: src, target: target, mode: def == nil ? nil : mode.flatMap(MidiMode.init) ?? def)
            if let i = mappings.firstIndex(where: { $0.src == src }) { mappings[i] = mp } else { mappings.append(mp) }
        }
    }

    private static func modeFor(_ t: MidiTarget, _ src: String) -> MidiMode? { t.isEncoder && src.hasPrefix("cc:") ? .abs : nil }

    // MARK: reading

    public func has(_ src: String) -> Bool { mappings.contains { $0.src == src } }
    public func get(_ src: String) -> MidiMapping? { mappings.first { $0.src == src } }
    public var boundTargets: Set<String> { Set(mappings.map(\.target)) }
    public func sourcesOf(_ target: String) -> [MidiMapping] { mappings.filter { $0.target == target } }

    /// why a message can't drive a target ("" when it can)
    public static func canBind(_ targetID: String, _ m: MidiMessage?) -> String {
        guard let t = MidiTarget.named(targetID), let m else { return "no such control" }
        if m.kind == .note && !t.isButton { return "\(t.id) takes a CC: move a knob or a pot" }
        return ""
    }

    // MARK: changes

    /// a source on a target (a source already bound moves; the list keeps the newest last); nil: not allowed
    @discardableResult public mutating func bind(_ src: String, to target: String, mode: MidiMode? = nil) -> MidiMapping? {
        guard MidiSource.isValid(src), let t = MidiTarget.named(target) else { return nil }
        if src.hasPrefix("note:") && !t.isButton { return nil }
        let def = Self.modeFor(t, src)
        let mp = MidiMapping(src: src, target: target, mode: def == nil ? nil : mode ?? def)
        mappings.removeAll { $0.src == src }
        mappings.append(mp)
        return mp
    }

    /// an encoder's CC only
    public mutating func setMode(_ src: String, _ mode: MidiMode) {
        guard let i = mappings.firstIndex(where: { $0.src == src }), mappings[i].mode != nil else { return }
        mappings[i].mode = mode
    }

    @discardableResult public mutating func remove(_ src: String) -> Bool {
        let n = mappings.count
        mappings.removeAll { $0.src == src }
        return mappings.count != n
    }

    /// a target's bindings removed; how many
    @discardableResult public mutating func forget(_ target: String) -> Int {
        let n = mappings.count
        mappings.removeAll { $0.target == target }
        return n - mappings.count
    }

    public mutating func clear() { mappings = [] }
    public mutating func setSpeed(_ v: Double) { speed = Self.speedOf(v) }

    // MARK: the engine's form

    public var entries: [rfme_map_entry_t] {
        mappings.compactMap { mp in
            guard let s = MidiSource.parse(mp.src), let t = MidiTarget.named(mp.target) else { return nil }
            return rfme_map_entry_t(src: UInt8(s.note ? RFME_SRC_NOTE : RFME_SRC_CC), channel: UInt8(s.channel - 1),
                                    number: UInt8(s.number), target: t.engine, mode: mp.mode?.engine ?? 0)
        }
    }

    /// the map to a mapper (fzmm_adopt); the buttons the old map held (bit per label), to let go
    @discardableResult public func apply(to m: OpaquePointer) -> UInt32 {
        let list = entries
        return list.withUnsafeBufferPointer { fzmm_adopt(m, $0.baseAddress, UInt32($0.count), speed) }
    }

    // MARK: the stored form (the website's prefs.midiMap and prefs.knobSpeed)

    public init(json: Data) {
        let o = (try? JSONSerialization.jsonObject(with: json)) as? [String: Any]
        let list = (o?["midiMap"] as? [Any] ?? []).map { x -> (String?, String?, String?) in
            let d = x as? [String: Any]
            return (d?["src"] as? String, d?["target"] as? String, d?["mode"] as? String)
        }
        speed = Self.speedOf((o?["knobSpeed"] as? NSNumber)?.doubleValue ?? 1)
        load(list)
    }

    public var json: Data {
        let list: [[String: Any]] = mappings.map { mp in
            var d: [String: Any] = ["src": mp.src, "target": mp.target]
            if let m = mp.mode { d["mode"] = m.rawValue }
            return d
        }
        let o: [String: Any] = ["midiMap": list, "knobSpeed": speed]
        return (try? JSONSerialization.data(withJSONObject: o, options: [.sortedKeys])) ?? Data()
    }
}
