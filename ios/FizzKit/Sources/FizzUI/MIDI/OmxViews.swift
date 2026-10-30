// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2026 RemoteFM1 contributors
// Derived from RemoteFM1IOS af91bc5 RFMKit/Sources/RFMKit/OMX27/OmxViews.swift, unchanged but for this line (PanelFeed.Knob / Label and LEDLevel are FIZZ's own: MidiPanel.swift).
// The OMX-27 as the FM-1's controller (REMOTE mode, OmxWire.swift): its views, what each key does in them, its LEDs
// and its screen's text. The website's js/omx27-views.js, ported as is (the owner's design:
// RemoteFM1 design/RemoteFM1---OMX-27-Remote.json).
//   keys      0 AUX, 1..10 the black keys, 11..26 the white keys (B3..C6: the design's keyMap)
//   K1        the view: Base, Seq, Keyboard, Quick Keys (four zones, a little hysteresis)
//   K2        the colour theme (six zones)
//   K3 / K4   Keyboard: the MIDI channel / the octave; Seq: K4 makes AUX the 11th black key or the button quick key
//   K5        MASTER, with pickup (MasterPot, below: the website's midi-map.js)
//   encoder   turns the selected knob (the last picked with a knob quick key, or the last turned); SELECT at first
// Base: 1 2 OCT- OCT+, 11 12 Range 1 / 2 (key 15 plays the FM-1's first F or its second), 13 the knob quick keys and 14
// the button quick keys (while held; a double click latches them, a click then closes them), the rest plays the FM-1's
// keys; AUX presses the selected button. Seq: the FM-1's keys 1:1. Keyboard: notes to the firmware's MIDI in. Quick
// Keys: every key a shortcut. Input -> actions (OmxLink applies them).

import Foundation

public typealias OmxKnob = PanelFeed.Knob
public typealias OmxLabel = PanelFeed.Label

/// what the OMX's input asks of the panel
public enum OmxAction: Equatable, Sendable {
    case key(Int, down: Bool)                     // the FM-1's key 0..26
    case button(OmxLabel, down: Bool)
    case turn(OmxKnob, steps: Int)
    case select(OmxKnob)
    case midi([UInt8])                            // to the firmware's MIDI in
    case master(Int)                              // 0..1023
    case pickup(pot: Int, at: Int, first: Bool)   // K5 away from MASTER (both 0..1023)
    case view(OmxView)
    case theme(Int)
}

public enum OmxView: Int, CaseIterable, Sendable {
    case base, seq, keys, quick
    public var name: String { ["Base", "Seq", "Keyboard", "Quick Keys"][rawValue] }
}

/// the views' choices kept between runs (the website's prefs.omx; bad values are ignored)
public struct OmxViewPrefs: Codable, Equatable, Sendable {
    public var theme: Int?, knob: Int?, button: Int?, auxBlack: Bool?, ch: Int?, oct: Int?
    public init(theme: Int? = nil, knob: Int? = nil, button: Int? = nil, auxBlack: Bool? = nil, ch: Int? = nil, oct: Int? = nil) {
        self.theme = theme; self.knob = knob; self.button = button; self.auxBlack = auxBlack; self.ch = ch; self.oct = oct
    }
}

// the website's panel.js, the parts the views need
enum OmxPanel {
    static let nKey = 27
    /// panel.js PANEL_LABELS (PanelFeed.Label order; ASCII, as the OMX's screen shows them)
    static let labels = ["FX", "SEL", "ENV", "LFO", "EDIT", "GLO", "HOME", "SAVE", "ARP", "SEQ", "PLAY", "REC", "OCT-", "OCT+"]
    static func label(_ l: OmxLabel) -> String { labels[l.rawValue] }
    static func keyBlack(_ k: Int) -> Bool { (0x54A >> ((k + 5) % 12)) & 1 == 1 }
    static let noteNames = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]
    /// the FM-1's key k (0 = F3)
    static func noteName(_ k: Int) -> String { let m = 53 + k; return noteNames[m % 12] + String(m / 12 - 1) }
}

/// Math.round
@inline(__always) func jsRound(_ x: Double) -> Int { Int((x + 0.5).rounded(.down)) }

/// MASTER's pickup for one pot. s: the pot's state; v its value; at MASTER's position in the pot's units; tol how near
/// is there. True once it is picked up: near MASTER, or it crossed MASTER since its last value (midi-map.js pickedUp)
public struct PickupState: Sendable {
    public var last: Double?
    public var picked = false
    public init() {}
}

public func pickedUp(_ s: inout PickupState, _ v: Double, _ at: Double, _ tol: Double) -> Bool {
    let last = s.last
    s.last = v
    if !s.picked { s.picked = abs(v - at) <= tol || (last != nil && (last! - at) * (v - at) <= 0) }
    return s.picked
}

/// one pot on MASTER with that pickup (the OMX-27's K5; midi-map.js MasterPot): v 0..max, cur MASTER now (0..1023) ->
/// .master or .pickup. MASTER moved by anything else: it waits again
public final class MasterPot {
    var s = PickupState()
    var set: Int?
    var hinted = false
    public init() {}
    public func feed(_ v: Int, _ max: Int, _ cur: Int) -> [OmxAction] {
        let want = jsRound(Double(v) * 1023 / Double(max))
        if cur != set { set = cur; s.picked = false; hinted = false }
        if !pickedUp(&s, Double(v), Double(cur) * Double(max) / 1023, 2 * Double(max) / 127) {
            let first = !hinted
            hinted = true
            return [.pickup(pot: want, at: cur, first: first)]
        }
        set = want
        return [.master(want)]
    }
}

public final class OmxViews {
    /// the OMX's keys' notes (the design's keyMap; AUX none)
    public static let omxNotes: [Int?] = [nil, 61, 63, 66, 68, 70, 73, 75, 78, 80, 82, 59, 60, 62, 64, 65, 67, 69, 71, 72,
                                         74, 76, 77, 79, 81, 83, 84]
    /// the FM-1's 27 keys (F3..G5): its 16 whites and 11 sharps, in order
    public static let fm1Whites = (0..<OmxPanel.nKey).filter { !OmxPanel.keyBlack($0) }
    public static let fm1Sharps = (0..<OmxPanel.nKey).filter { OmxPanel.keyBlack($0) }
    public static let knobs: [(OmxKnob, String)] = [(.select, "SEL"), (.presets, "PRESETS"), (.algorithm, "ALGO"),
                                                   (.k1, "KN1"), (.k2, "KN2"), (.k3, "KN3"), (.k4, "KN4")]
    static func knobName(_ k: OmxKnob) -> String? { knobs.first { $0.0 == k }?.1 }
    static let btn6: [OmxLabel] = [.fx, .sel, .env, .lfo, .edit, .glo]
    static let btn6b: [OmxLabel] = [.home, .save, .arp, .seq, .play, .rec]

    // colours at full brightness (the design's hues); a shortcut shows at DIM and full while pressed or selected
    enum HUE {
        static let aux = [255, 0, 0], r1 = [128, 255, 0], r2 = [255, 128, 0], kqk = [255, 0, 128], bqk = [255, 128, 128]
        static let knob = [0, 0, 255], kn = [255, 0, 0]
        static let six = [[255, 128, 0], [255, 255, 0], [255, 0, 255], [255, 0, 128], [0, 255, 128], [255, 0, 0]]
        static func btn(_ b: OmxLabel) -> [Int] {
            if b == .octDown { return [0, 255, 255] }
            if b == .octUp { return [0, 127, 255] }
            if let i = btn6.firstIndex(of: b) { return six[i] }
            return six[btn6b.firstIndex(of: b)!]
        }
    }
    public static let DIM = 0.5, DEAD = 0.16   // DEAD: the dark red of a key past the FM-1's keyboard

    /// K2: the hue of the keys that mirror the FM-1's (and Night: dimmer shortcuts, for a dark room)
    public struct Theme: Sendable { public let name: String; public let key: [Int]; public let dim: Double? }
    public static let themes = [
        Theme(name: "White", key: [255, 255, 255], dim: nil),
        Theme(name: "Ember", key: [255, 90, 0], dim: nil),
        Theme(name: "Ice", key: [0, 190, 255], dim: nil),
        Theme(name: "Violet", key: [150, 40, 255], dim: nil),
        Theme(name: "Lime", key: [110, 255, 0], dim: nil),
        Theme(name: "Night", key: [40, 60, 255], dim: 0.25),
    ]
    public static let doubleMs = 400.0, noteMs = 1500.0, velocity = 100

    /// a pot (0..127) in n zones; prev the zone now: it changes only past the zone's edge by a margin
    public static func zoneOf(_ v: Int, _ n: Int, _ prev: Int?) -> Int {
        let w = 128 / Double(n), z = max(0, min(n - 1, Int((Double(v) / w).rounded(.down))))
        guard let prev, prev >= 0, prev < n, z != prev else { return z }
        let m = min(3, w / 4)
        return Double(v) < Double(prev) * w - m || Double(v) >= Double(prev + 1) * w + m ? z : prev
    }

    static func c7(_ c: [Int], _ f: Double) -> OmxRGB {
        let v = c.map { max(0, min(127, jsRound(Double($0) * f / 2))) }
        return OmxRGB(v[0], v[1], v[2])
    }

    public enum QK: Sendable { case knob, btn }

    /// what a key does now
    public enum Role: Equatable, Sendable {
        case fm1(Int)                 // the FM-1's key
        case note(Int, ch: Int)       // a note to the firmware
        case oct(OmxLabel)            // OCT- / OCT+ (not selected)
        case btn(OmxLabel)            // a button (selected when pressed)
        case knob(OmxKnob)            // selects the encoder's knob
        case range(Int)
        case qk(QK)                   // a quick-key toggle
        case aux(OmxLabel)            // the selected button
    }

    public private(set) var view = OmxView.base
    public var theme: Int
    public private(set) var knob: OmxKnob
    public private(set) var button: OmxLabel
    public private(set) var auxBlack: Bool
    public private(set) var ch: Int
    public private(set) var oct: Int
    public var range = 0
    private var held: [(k: Int, role: Role?)] = []          // OMX key -> its role when it went down (in that order)
    private struct Click { var latched = false; var last = -1e9 }
    private var qkKnob = Click(), qkBtn = Click()
    private var qkHeld: [QK] = []                          // in the order held
    private var fm1 = [Int](repeating: 0, count: OmxPanel.nKey)   // how many OMX keys hold each FM-1 key
    private var btns = [Int](repeating: 0, count: OmxPanel.labels.count)
    let master = MasterPot()
    private var note: (text: String, until: Double)?       // the theme's name, MASTER's %

    public init(saved: OmxViewPrefs? = nil) {
        let s = saved ?? OmxViewPrefs()
        theme = s.theme.flatMap { (0..<Self.themes.count).contains($0) ? $0 : nil } ?? 0
        knob = s.knob.flatMap { OmxKnob(rawValue: $0) }.flatMap { Self.knobName($0) != nil ? $0 : nil } ?? .select
        button = s.button.flatMap { OmxLabel(rawValue: $0) } ?? .play
        auxBlack = s.auxBlack == true
        ch = s.ch.flatMap { (1...16).contains($0) ? $0 : nil } ?? 1
        oct = s.oct.flatMap { abs($0) <= 3 ? $0 : nil } ?? 0
    }

    public func saved() -> OmxViewPrefs {
        OmxViewPrefs(theme: theme, knob: knob.rawValue, button: button.rawValue, auxBlack: auxBlack, ch: ch, oct: oct)
    }

    public var heldCount: Int { held.count }
    public func isHeld(_ k: Int) -> Bool { held.contains { $0.k == k } }
    public var knobLatched: Bool { qkKnob.latched }
    public var buttonLatched: Bool { qkBtn.latched }

    /// the quick keys showing in Base
    public func overlay() -> QK? {
        guard view == .base else { return nil }
        if let last = qkHeld.last { return last }
        return qkKnob.latched ? .knob : qkBtn.latched ? .btn : nil
    }

    /// the FM-1 key an OMX key plays in Base (-1: past the FM-1's keys)
    public func baseKey(_ k: Int) -> Int {
        guard let n = Self.omxNotes[k] else { return -1 }
        let f = n - 65 + 12 * range
        return f >= 0 && f < OmxPanel.nKey ? f : -1
    }

    /// what key k does now (nil nothing)
    public func roleOf(_ k: Int) -> Role? {
        func fm1(_ f: Int) -> Role? { f >= 0 && f < OmxPanel.nKey ? .fm1(f) : nil }
        let aux = Role.aux(button)
        switch view {
        case .seq:
            if k == 0 { return auxBlack ? fm1(Self.fm1Sharps[10]) : aux }
            return fm1(k <= 10 ? Self.fm1Sharps[k - 1] : Self.fm1Whites[k - 11])
        case .keys:
            if k == 0 { return aux }
            let n = Self.omxNotes[k]! + 12 * oct
            return n >= 0 && n <= 127 ? .note(n, ch: ch) : nil
        case .quick:
            if k == 0 { return aux }
            if k == 1 || k == 2 { return .btn(k == 1 ? .octDown : .octUp) }
            if k >= 3 && k <= 9 { return .knob(Self.knobs[k - 3].0) }
            if k >= 11 && k <= 16 { return .btn(Self.btn6[k - 11]) }
            if k >= 19 && k <= 24 { return .btn(Self.btn6b[k - 19]) }
            return nil
        case .base:
            break
        }
        let ov = overlay()
        if k == 0 { return aux }
        if k == 1 || k == 2 { let b: OmxLabel = k == 1 ? .octDown : .octUp; return ov == .btn ? .btn(b) : .oct(b) }
        if k == 11 || k == 12 { return .range(k - 11) }
        if k == 13 || k == 14 { return .qk(k == 13 ? .knob : .btn) }
        if k <= 10 {
            if ov == .knob { return k <= 9 ? .knob(Self.knobs[k - 3].0) : nil }
            if ov == .btn { return k >= 5 ? .btn(Self.btn6[k - 5]) : nil }
        } else if ov == .btn { return k >= 21 ? .btn(Self.btn6b[k - 21]) : nil }
        return fm1(baseKey(k))
    }

    // MARK: input

    /// a key event (OmxWire.EV); now: milliseconds
    public func key(_ k: Int, _ ev: Int, _ now: Double = 0) -> [OmxAction] {
        guard k >= 0 && k < 27 else { return [] }
        if ev == OmxWire.EV.down {
            if isHeld(k) { return [] }
            let r = roleOf(k)
            held.append((k, r))
            return press(r, true)
        }
        if ev == OmxWire.EV.up {
            guard let i = held.firstIndex(where: { $0.k == k }) else { return [] }
            let r = held[i].role
            held.remove(at: i)
            return press(r, false)
        }
        if ev == OmxWire.EV.quick && view == .base && (k == 13 || k == 14) { click(k == 13 ? .knob : .btn, now) }
        return []
    }

    /// a quick click on a quick-key toggle: a second one soon after latches it; when latched, one closes it
    func click(_ which: QK, _ now: Double) {
        var q = which == .knob ? qkKnob : qkBtn
        if q.latched {
            q.latched = false; q.last = -1e9
        } else if now - q.last <= Self.doubleMs {
            q.latched = true; q.last = -1e9
            if which == .knob { qkBtn.latched = false } else { qkKnob.latched = false }
        } else {
            q.last = now
        }
        if which == .knob { qkKnob = q } else { qkBtn = q }
    }

    func press(_ r: Role?, _ down: Bool) -> [OmxAction] {
        guard let r else { return [] }
        switch r {
        case .fm1(let k): return Self.count(&fm1, k, down) { .key(k, down: $0) }
        case .note(let n, let ch):
            return [.midi([UInt8((down ? 0x90 : 0x80) | (ch - 1)), UInt8(n), UInt8(down ? Self.velocity : 0)])]
        case .oct(let b), .aux(let b): return Self.count(&btns, b.rawValue, down) { .button(b, down: $0) }
        case .btn(let b):
            if down { button = b }
            return Self.count(&btns, b.rawValue, down) { .button(b, down: $0) }
        case .knob(let role):
            guard down else { return [] }
            knob = role
            return [.select(role)]
        case .range(let i):
            if down { range = i }
            return []
        case .qk(let which):
            if down { qkHeld.append(which) } else if let i = qkHeld.lastIndex(of: which) { qkHeld.remove(at: i) }
            return []
        }
    }

    /// a key or button held by several OMX keys: down with the first, up with the last
    static func count(_ arr: inout [Int], _ i: Int, _ down: Bool, _ act: (Bool) -> OmxAction) -> [OmxAction] {
        let was = arr[i]
        arr[i] = down ? was + 1 : max(0, was - 1)
        return (was == 0) != (arr[i] == 0) ? [act(down)] : []
    }

    public func enc(_ delta: Int) -> [OmxAction] { delta != 0 ? [.turn(knob, steps: delta)] : [] }

    /// a knob turned elsewhere (the panel, a MIDI controller, the FM-1): the encoder turns it next
    public func knobUsed(_ role: OmxKnob) { if Self.knobName(role) != nil { knob = role } }

    /// a pot: v 0..127, hires 0..16383, now milliseconds; master MASTER now (0..1023)
    public func pot(_ p: Int, _ v: Int, _ hires: Int = 0, _ now: Double = 0, _ master: Int = 768) -> [OmxAction] {
        if p == 0 {
            let z = Self.zoneOf(v, 4, view.rawValue)
            if z == view.rawValue { return [] }
            view = OmxView(rawValue: z)!
            return [.view(view)]
        }
        if p == 1 {
            let z = Self.zoneOf(v, Self.themes.count, theme)
            note = ("THEME " + Self.themes[z].name.uppercased(), now + Self.noteMs)
            if z == theme { return [] }
            theme = z
            return [.theme(z)]
        }
        if p == 2 && view == .keys { ch = Self.zoneOf(v, 16, ch - 1) + 1 }
        if p == 3 && view == .keys { oct = Self.zoneOf(v, 7, oct + 3) - 3 }
        if p == 3 && view == .seq { auxBlack = Self.zoneOf(v, 2, auxBlack ? 1 : 0) == 1 }
        if p == 4 {
            let acts = self.master.feed(hires, 16383, master)
            func pct(_ x: Int) -> String { "\(jsRound(Double(x) / 1023 * 100))%" }
            switch acts[0] {
            case .master(let value): note = ("MASTER " + pct(value), now + Self.noteMs)
            case .pickup(let pot, let at, _): note = ("K5 \(pct(pot)) TO \(pct(at))", now + Self.noteMs)
            default: break
            }
            return acts
        }
        return []
    }

    /// everything the OMX holds, let go (it left REMOTE mode, went away, or was disconnected)
    public func releaseAll() -> [OmxAction] {
        var out: [OmxAction] = []
        for h in held { out += press(h.role, false) }
        held.removeAll()
        qkHeld.removeAll()
        fm1 = fm1.map { _ in 0 }
        btns = btns.map { _ in 0 }
        return out
    }

    /// a Base-view playing key past the end of the FM-1's keyboard (Range 2's top): lit dark red, so it reads as not there
    func deadKey(_ k: Int, _ ov: QK?) -> Bool {
        view == .base && ov != .btn && ((k >= 3 && k <= 10) || k >= 15) && baseKey(k) < 0
    }

    // MARK: what the OMX shows

    /// the 27 LEDs (7-bit); keyLevel(k): the FM-1's LED of key k (0..255)
    public func leds(_ keyLevel: (Int) -> UInt8 = { _ in 0 }) -> [OmxRGB] {
        let th = Self.themes[theme], dim = th.dim ?? Self.DIM, ov = overlay()
        var out: [OmxRGB] = []
        for k in 0..<27 {
            let pressed = isHeld(k)
            func sc(_ hue: [Int], _ on: Bool) -> OmxRGB { Self.c7(hue, on ? 1 : dim) }
            switch roleOf(k) {
            case nil: out.append(deadKey(k, ov) ? Self.c7(HUE.aux, Self.DEAD * dim / Self.DIM) : .off)
            case .fm1(let f): out.append(Self.c7(th.key, LEDLevel.alpha(keyLevel(f))))
            case .note: out.append(pressed ? Self.c7(th.key, 1) : .off)
            case .aux: out.append(sc(HUE.aux, pressed))
            case .oct(let b): out.append(sc(HUE.btn(b), pressed))
            case .btn(let b): out.append(sc(HUE.btn(b), pressed || button == b))
            case .knob(let role):
                out.append(sc(role == .select || role == .presets || role == .algorithm ? HUE.knob : HUE.kn, knob == role))
            case .range(let r): out.append(sc(r != 0 ? HUE.r2 : HUE.r1, range == r))
            case .qk(let which): out.append(sc(which == .knob ? HUE.kqk : HUE.bqk, pressed || ov == which))
            }
        }
        return out
    }

    /// the screen's text (line: a moment's note, else the view's context)
    public struct Screen: Equatable, Sendable {
        public var title: String, line: String, view: OmxView
    }

    /// now: milliseconds
    public func screen(_ now: Double = 0) -> Screen {
        let ov = overlay(), kn = Self.knobName(knob) ?? "", aux = "AUX " + OmxPanel.label(button)
        let title: String
        switch view {
        case .seq: title = "SEQ"
        case .keys: title = "KEYBOARD"
        case .quick: title = "QUICK KEYS"
        case .base: title = ov == .knob ? "KNOBS" : ov == .btn ? "BUTTONS" : "FM-1"
        }
        let line: String
        if let note, now < note.until { line = note.text }
        else {
            switch view {
            case .seq: line = "\(kn)  \(auxBlack ? "AUX " + OmxPanel.noteName(Self.fm1Sharps[10]) : aux)"
            case .keys: line = "CH \(ch)  OCT \(oct > 0 ? "+" : "")\(oct)  \(kn)"
            case .quick: line = "\(kn)  \(aux)"
            case .base: line = "R\(range + 1)  \(kn)  \(aux)"
            }
        }
        return Screen(title: title, line: line, view: view)
    }

    /// a short name for each key now (the tests' pictures of the OMX)
    public func labels() -> [String] {
        (0..<27).map { k in
            switch roleOf(k) {
            case nil: ""
            case .fm1(let f): OmxPanel.noteName(f)
            case .note(let n, _): OmxPanel.noteName(n - 53)
            case .aux: "AUX"
            case .oct(let b), .btn(let b): OmxPanel.label(b)
            case .knob(let role): Self.knobName(role) ?? ""
            case .range(let r): "R\(r + 1)"
            case .qk(let which): which == .knob ? "KNOBS" : "BTNS"
            }
        }
    }
}
