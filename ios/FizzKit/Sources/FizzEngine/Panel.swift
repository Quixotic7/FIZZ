// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// The emulated FM-1's panel as the screens see it: its 14 buttons, 7 knobs, the MASTER pot, 27 note keys, the LEDs.

import CFizzCore
import CoreGraphics
import Foundation

/// A matrix button, in the core ABI's label order (FM1P_B_*). `fizzName` is FIZZ's own label for it.
public enum FizzButton: Int, CaseIterable, Sendable {
    case fx, tracks, mix, latch, edit, shift, home, sounds, midiFX, patterns, play, rec, f1, f2

    /// The FM-1's printed label (FX SEL ENV LFO EDIT GLO HOME SAVE ARP SEQ PLAY REC OCT- OCT+).
    public var panelName: String {
        ["FX", "SEL", "ENV", "LFO", "EDIT", "GLO", "HOME", "SAVE", "ARP", "SEQ", "PLAY", "REC", "OCT-", "OCT+"][rawValue]
    }

    /// FIZZ's label (the descriptor's button_names: TRACKS, MIX, LATCH, SHIFT, SOUNDS, MIDI FX, PATTERNS, F1, F2 ..).
    public var fizzName: String { FizzCoreInfo.current.buttonNames[rawValue] }

    /// The button FIZZ labels `name` (its own label or the FM-1's printed one, any case).
    public init?(label name: String) {
        let n = name.uppercased()
        guard let b = Self.allCases.first(where: { $0.fizzName == n || $0.panelName == n }) else { return nil }
        self = b
    }
}

/// A knob by its role (the core ABI's FM1P_E_*): SELECT, ALGO, PRESETS, K1..K4. MASTER is the pot (setMaster).
public enum FizzKnob: Int, CaseIterable, Sendable {
    case select, algo, presets, k1, k2, k3, k4
}

/// The LEDs: a brightness 0..255 each (the core's levels: 12 faint, 40 dim, 96 mid, 255 lit).
public struct FizzLEDs: Sendable, Equatable {
    /// per FizzButton
    public var buttons: [UInt8]
    /// per note key 0..26
    public var keys: [UInt8]
    /// PLAY's green LED
    public var playGreen: UInt8
    /// the panel's own array: [column * 4 + line]
    public var raw: [UInt8]

    public static let litMin: UInt8 = 176
    public func isLit(_ b: FizzButton) -> Bool { buttons[b.rawValue] >= Self.litMin }
    public func isLit(key: Int) -> Bool { keys[key] >= Self.litMin }

    static let off = FizzLEDs(buttons: .init(repeating: 0, count: 14), keys: .init(repeating: 0, count: 27), playGreen: 0,
                              raw: .init(repeating: 0, count: 44))
}

/// One published picture of the panel: the LCD and the LEDs.
public struct FizzPanelFrame: @unchecked Sendable {
    /// 240 x 240, RGBA
    public let lcd: CGImage
    public let leds: FizzLEDs
    /// LCD frames published since power-on
    public let seq: UInt32
}

/// The compiled-in core: what it says about itself, and the panel's wiring after a boot.
public struct FizzCoreInfo: Sendable {
    public let id: String, name: String, version: String, sourceURL: String, family: String, license: String
    public let credits: String
    /// why FM1VST keeps it out of public builds (nil: not gated)
    public let gated: String?
    /// FIZZ's labels for the 14 buttons, FizzButton order
    public let buttonNames: [String]
    public let flashSize: Int
    public static let sampleRate = Double(FZH_FS)
    public static let blockFrames = Int(FZH_BLOCK)

    public static let current: FizzCoreInfo = {
        _ = fzh_open()
        func s(_ k: UInt32) -> String? { fzh_str(k).map { String(cString: $0) } }
        return FizzCoreInfo(id: s(0) ?? "", name: s(1) ?? "", version: s(2) ?? "", sourceURL: s(3) ?? "",
                            family: s(4) ?? "", license: s(5) ?? "", credits: s(6) ?? "", gated: s(7),
                            buttonNames: (0..<14).map { s(100 + UInt32($0)) ?? "" }, flashSize: Int(fzh_flash_size()))
    }()
}

/// The panel's wiring (read after init): label -> matrix button, and the LED position of each key id.
struct PanelTable: Sendable {
    let btnID: [UInt8], encID: [UInt8], encDir: [Int8], keymap: [Int8]

    /// The caller holds the clock token (the core is up).
    static func read() -> PanelTable {
        var b = [UInt8](repeating: 0, count: 14), e = [UInt8](repeating: 0, count: 7)
        var d = [Int8](repeating: 0, count: 7), k = [Int8](repeating: -1, count: 66)
        fzh_panel_table(&b, &e, &d, &k)
        return PanelTable(btnID: b, encID: e, encDir: d, keymap: k)
    }

    /// The index into the LED array of key id `id` (0..13 matrix button, 14 + n note key n): rows 1..4 are lines 0..3.
    func ledIndex(_ id: Int) -> Int? {
        for r in 1...4 {
            for c in 0..<11 where Int(keymap[r * 11 + c]) == id { return c * 4 + (r - 1) }
        }
        return nil
    }

    func leds(raw: [UInt8], playGreen: UInt8) -> FizzLEDs {
        let at = { (id: Int) -> UInt8 in self.ledIndex(id).map { raw[$0] } ?? 0 }
        return FizzLEDs(buttons: (0..<14).map { at(Int(btnID[$0])) }, keys: (0..<27).map { at(14 + $0) },
                        playGreen: playGreen, raw: raw)
    }
}
