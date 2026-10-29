// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// PLAY's state and input (mock-ups 29-33): the emulated FIZZ's screen and LEDs as they come (frames()), the buttons
// pressed on touch-down and let go on touch-up as the hardware's are, the knobs (MASTER absolute, the others detents
// from a drag), the 27 keys with a velocity from where the finger lands; the view, the theme and SOUND remembered.
// The pure parts (labels, detents, velocity, the keybed's geometry) are PlayInput's and KeyLayout's, for the tests.

import CoreGraphics
import FizzEngine
import FizzKit
import Foundation
import Observation
#if os(iOS)
import UIKit
#endif

/// PLAY's four views (the sheet's VIEW; or a swipe sideways).
public enum PlayViewKind: String, CaseIterable, Sendable {
    case full, panel, keys, screen
    public var title: String { rawValue.uppercased() }
}

/// Where PLAY's keybed is heard.
public enum PlaySound: String, Sendable {
    case here       // the emulated FIZZ
    case fm1        // the FM-1's track, over MIDI
}

@MainActor @Observable
public final class PlayModel {
    @ObservationIgnored weak var host: EmulatorHost?
    /// For SOUND = FM-1 (its link, its track's channel).
    @ObservationIgnored weak var unit: UnitModel?
    @ObservationIgnored private let defaults: UserDefaults

    /// The LCD (240 x 240) and the LEDs, as last published.
    public private(set) var lcd: CGImage?
    public private(set) var leds: FizzLEDs?
    /// What the fingers hold.
    public private(set) var pressed: Set<FizzButton> = []
    public private(set) var held: Set<Int> = []
    /// The knobs' marks (degrees; detents turn them 15 each) and MASTER (0..1023).
    public private(set) var angles: [FizzKnob: Double] = [.select: -20, .algo: 30, .presets: -120, .k1: -60, .k2: 135,
                                                          .k3: 0, .k4: 40]
    public private(set) var master = 800

    public var view: PlayViewKind { didSet { defaults.set(view.rawValue, forKey: "play.view") } }
    public var theme: PlayTheme { didSet { defaults.set(theme.rawValue, forKey: "play.theme") } }
    public var sound: PlaySound = .here
    /// The linked views' knob row: MAIN (false) or KN 1-4 (true)
    public var knobsKN = true
    /// The pull-down sheet
    public var sheet = false
    /// The scrolling keybed's first key shown (white keys, 0..)
    public var keysOffset: Double = 4
    /// The LCD fills the display (a double tap on it; PlayNav.swift)
    public var lcdFull = false
    /// The manager's tab last shown (the FIZZ badge goes back to it; PlayNav.swift)
    public private(set) var lastTab: FizzTab?

    init(host: EmulatorHost?, defaults: UserDefaults = .standard) {
        self.host = host
        self.defaults = defaults
        view = PlayViewKind(rawValue: defaults.string(forKey: "play.view") ?? "") ?? .full
        theme = PlayTheme(rawValue: defaults.string(forKey: "play.theme") ?? "") ?? .tangerine
        if defaults.object(forKey: "play.master") != nil { master = defaults.integer(forKey: "play.master") }
    }

    var emulator: FizzEmulator? { host?.emulator }

    /// The screen and the LEDs while PLAY shows (a .task).
    func follow() async {
        guard let e = emulator else { return }
        e.setMaster(master)
        for await f in e.frames() {
            lcd = f.lcd
            if leds != f.leds { leds = f.leds }
        }
    }

    /// The screen and the LEDs as they are now (the snapshots).
    public func showNow() {
        lcd = emulator?.lcdImage
        leds = emulator?.leds
    }

    // MARK: the panel

    func press(_ b: FizzButton) {
        guard !pressed.contains(b) else { return }
        pressed.insert(b)
        emulator?.press(b)
        Haptics.press()
    }

    func release(_ b: FizzButton) {
        guard pressed.remove(b) != nil else { return }
        emulator?.release(b)
    }

    /// A detent knob turned (+ clockwise).
    func turn(_ k: FizzKnob, by n: Int) {
        guard n != 0 else { return }
        emulator?.turn(k, by: n)
        angles[k, default: 0] += Double(n) * 15
        Haptics.tick()
    }

    func setMaster(_ v: Int) {
        let c = max(0, min(1023, v))
        guard c != master else { return }
        if PlayInput.masterStep(master) != PlayInput.masterStep(c) { Haptics.tick() }
        master = c
        emulator?.setMaster(c)
        defaults.set(c, forKey: "play.master")
    }

    /// MASTER's mark: -135 .. 135 degrees.
    var masterAngle: Double { -135 + 270 * Double(master) / 1023 }

    // MARK: the keys

    func keyDown(_ k: Int, velocity: Int) {
        guard (0..<27).contains(k), !held.contains(k) else { return }
        held.insert(k)
        if sound == .fm1, let u = unit, u.unit?.isEmulated == false {
            let c = u.editor.chan, n = PlayInput.note(k, octave: u.editor.octave)
            Task { await u.actions.note(channel: c, note: n, velocity: velocity) }
        } else {
            emulator?.noteOn(key: k, velocity: velocity)
        }
    }

    func keyUp(_ k: Int) {
        guard held.remove(k) != nil else { return }
        if sound == .fm1, let u = unit, u.unit?.isEmulated == false {
            let c = u.editor.chan, n = PlayInput.note(k, octave: u.editor.octave)
            Task { await u.actions.note(channel: c, note: n, velocity: 0) }
        } else {
            emulator?.noteOff(key: k)
        }
    }

    /// Everything let go (PLAY hidden, the app to the background, the sheet down).
    func letGoAll() {
        for b in pressed { release(b) }
        for k in held { keyUp(k) }
    }

    /// PLAY left: everything let go, the sheet up, the LCD back in its place.
    func left() {
        letGoAll()
        sheet = false
        lcdFull = false
    }

    /// A tab shown (FizzAppView): the manager's ones remembered (not PLAY, not IMPORT's sheet).
    public func remember(_ t: FizzTab) {
        if t != .play && t != .importer { lastTab = t }
    }

    /// Where the FIZZ badge goes: the manager's tab shown before PLAY, else LIBRARY.
    public var managerTab: FizzTab { lastTab ?? .library }

    /// SOUND = FM-1 offered: an FM-1 is the unit.
    var fm1Offered: Bool { unit?.unit.map { !$0.isEmulated } ?? false }
    /// SOUND as it applies (FM-1 only while one is the unit).
    var soundNow: PlaySound { sound == .fm1 && fm1Offered ? .fm1 : .here }

    // MARK: the views

    /// The view after a swipe (FULL, PANEL / KEYS, SCREEN; -1 left, +1 right).
    static func swiped(_ v: PlayViewKind, by d: Int) -> PlayViewKind {
        let order: [PlayViewKind] = [.full, .keys, .screen]
        let at = v == .panel ? 1 : order.firstIndex(of: v) ?? 0
        return order[max(0, min(order.count - 1, at + d))].with(deck: v)
    }

    /// The emulated FIZZ's LED for a key, 0..1.
    func keyLED(_ k: Int) -> Double { Double(leds?.keys[safe: k] ?? 0) / 255 }
    func buttonLED(_ b: FizzButton) -> Double { Double(leds?.buttons[safe: b.rawValue] ?? 0) / 255 }
}

private extension PlayViewKind {
    /// PANEL and KEYS are one place: a swipe into it keeps the deck it had.
    func with(deck old: PlayViewKind) -> PlayViewKind { self == .keys && old == .panel ? .panel : self }
}

fileprivate extension Array {
    subscript(safe k: Int) -> Element? { indices.contains(k) ? self[k] : nil }
}

// MARK: - The pure parts

public enum PlayInput {
    /// A button by its label (FIZZ's, or the FM-1's printed one).
    public static func button(_ label: String) -> FizzButton? { FizzButton(label: label) }

    /// The 12 buttons of the well, in the panel's order (FX .. REC), then F1 and F2.
    public static let well: [FizzButton] = [.fx, .tracks, .mix, .latch, .edit, .shift, .home, .sounds, .midiFX,
                                            .patterns, .play, .rec]

    /// A button's label on the panel (FIZZ's own; "MIDI FX" on two lines).
    public static func label(_ b: FizzButton) -> String {
        let n = b.fizzName.isEmpty ? b.panelName : b.fizzName
        return n.replacingOccurrences(of: " ", with: "\n")
    }

    /// Points of drag per detent.
    public static let detentPoints: CGFloat = 14

    /// A drag's detents: up and right turn clockwise; what is left over is carried to the next move.
    public struct Detents: Sendable {
        public var carried: CGFloat = 0
        public init() {}
        /// dx, dy: the move since the last (points; y grows downwards). Returns the detents to send.
        public mutating func move(dx: CGFloat, dy: CGFloat, step: CGFloat = PlayInput.detentPoints) -> Int {
            carried += dx - dy
            let n = Int((carried / step).rounded(.towardZero))
            carried -= CGFloat(n) * step
            return n
        }
    }

    /// MASTER from a drag: the value at the start plus 1023 per `span` points (up and right raise it).
    public static func master(from start: Int, dx: CGFloat, dy: CGFloat, span: CGFloat = 220) -> Int {
        max(0, min(1023, start + Int(((dx - dy) / span * 1023).rounded())))
    }

    /// MASTER's haptic steps (one tick each 1/16 of its turn).
    static func masterStep(_ v: Int) -> Int { v * 16 / 1024 }

    /// A key's velocity from where the finger lands on it: 0 its far end (soft) .. 1 its near end (hard).
    public static func velocity(at f: CGFloat) -> Int {
        let c = max(0, min(1, f))
        return max(1, min(127, Int((24 + c * 103).rounded())))
    }

    /// The MIDI note of key k (0 = F3) at the FM-1's keybed octave (-2..2).
    public static func note(_ k: Int, octave: Int) -> Int { 53 + 12 * octave + k }
}

/// The LED keybed: 16 white keys (F3 .. G5's whites, FIZZ's 16 steps) in a row, the 11 black keys above them, as
/// RemoteFM1's Keys part draws them. Its keys are FizzEmulator's 0 (F3) .. 26 (G5).
public struct KeyLayout: Sendable, Equatable {
    /// the white keys' semitones from F3
    public static let whites = [0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26]
    /// the white key each black key follows, and its semitone
    public static let blacks: [(after: Int, key: Int)] = [(0, 1), (1, 3), (2, 5), (4, 8), (5, 10), (7, 13), (8, 15),
                                                          (9, 17), (11, 20), (12, 22), (14, 25)]

    public let size: CGSize
    /// the white keys' pitch (key + gap) and the gap
    public let pitch: CGFloat, gap: CGFloat
    /// where white key 0 starts (scrolled: negative)
    public let originX: CGFloat
    public let blackHeight: CGFloat, whiteTop: CGFloat

    /// row: all 16 across `size.width`; scroll: keys `keyW` wide, the row moved `offset` white keys left.
    public init(size: CGSize, keyW: CGFloat? = nil, offset: Double = 0) {
        self.size = size
        let g = max(3, min(10, (size.width * 0.008).rounded()))
        gap = g
        if let keyW {
            pitch = keyW + g
            originX = -CGFloat(offset) * pitch
        } else {
            pitch = (size.width + g) / 16
            originX = 0
        }
        blackHeight = (size.height * 0.4).rounded()
        whiteTop = blackHeight + g + 2
    }

    /// How many white keys show (scroll layout).
    public var visible: Double { Double((size.width + gap) / pitch) }
    public var keyWidth: CGFloat { pitch - gap }
    public var blackWidth: CGFloat { keyWidth * 0.86 }

    public func whiteFrame(_ i: Int) -> CGRect {
        CGRect(x: originX + CGFloat(i) * pitch, y: whiteTop, width: keyWidth, height: size.height - whiteTop)
    }

    public func blackFrame(after a: Int) -> CGRect {
        let cx = originX + CGFloat(a + 1) * pitch - gap / 2
        return CGRect(x: cx - blackWidth / 2, y: 0, width: blackWidth, height: blackHeight)
    }

    /// Every key's frame (FizzEmulator's key ids).
    public var frames: [(key: Int, rect: CGRect, black: Bool)] {
        Self.whites.enumerated().map { (key: $1, rect: whiteFrame($0), black: false) }
            + Self.blacks.map { (key: $0.key, rect: blackFrame(after: $0.after), black: true) }
    }

    /// The key under a point, and where on it (0 far end .. 1 near end) for the velocity; black keys first.
    public func key(at p: CGPoint) -> (key: Int, depth: CGFloat)? {
        for b in Self.blacks {
            let r = blackFrame(after: b.after)
            if r.contains(p) { return (b.key, (p.y - r.minY) / max(1, r.height)) }
        }
        // a white key: its column (the gaps between belong to the nearer key), anywhere under the black row too
        let i = Int(((p.x - originX + gap / 2) / pitch).rounded(.down))
        guard (0..<16).contains(i), p.y >= 0, p.y <= size.height else { return nil }
        let r = whiteFrame(i)
        return (Self.whites[i], max(0, (p.y - r.minY)) / max(1, r.height))
    }
}

// MARK: - Haptics

enum Haptics {
    #if os(iOS)
    @MainActor private static let selection = UISelectionFeedbackGenerator()
    @MainActor private static let impact = UIImpactFeedbackGenerator(style: .light)
    @MainActor static func tick() { selection.selectionChanged() }
    @MainActor static func press() { impact.impactOccurred(intensity: 0.7) }
    #else
    static func tick() {}
    static func press() {}
    #endif
}
