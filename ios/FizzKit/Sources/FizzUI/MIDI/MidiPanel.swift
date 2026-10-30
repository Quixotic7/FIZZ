// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// What the ported MIDI map and OMX-27 files (MidiMap, MidiLearn, Omx*: RemoteFM1IOS af91bc5, MPL-2.0) need of a panel,
// written for FIZZ: the panel's knob roles and button labels in the core ABI's order (FM1P_E_* / FM1P_B_*: FizzKnob's
// and FizzButton's), MASTER being the pot; an LED level as the share of its lit colour; the OMX screen's typeface
// (Chakra Petch Bold, bundled in FizzUI). None of RemoteFM1's panel code is used: these are FIZZ's own.

import CoreText
import FizzEngine
import Foundation

/// The panel as the ported files name its parts (FIZZ's own namespace for them).
public enum PanelFeed {
    /// The buttons by the FM-1's printed label (the core ABI's order: FizzButton's; OCT- / OCT+ are F1 / F2).
    public enum Label: Int, CaseIterable, Sendable {
        case fx, sel, env, lfo, edit, glo, home, save, arp, seq, play, rec, octDown, octUp

        /// the printed legend (OCT− with a minus sign)
        public var legend: String {
            ["FX", "SEL", "ENV", "LFO", "EDIT", "GLO", "HOME", "SAVE", "ARP", "SEQ", "PLAY", "REC", "OCT−", "OCT+"][rawValue]
        }

        /// FIZZ's button
        public var button: FizzButton { FizzButton(rawValue: rawValue)! }
        public init(_ b: FizzButton) { self.init(rawValue: b.rawValue)! }
    }

    /// The knob roles (the core ABI's order: FizzKnob's), then MASTER (the pot).
    public enum Knob: Int, CaseIterable, Sendable {
        case select, algorithm, presets, k1, k2, k3, k4, master

        /// FIZZ's knob (nil: MASTER)
        public var knob: FizzKnob? { FizzKnob(rawValue: rawValue) }
        public init(_ k: FizzKnob) { self.init(rawValue: k.rawValue)! }
    }
}

/// An LED's level (the core's 0..255: 12 faint, 40 dim, 96 mid, 255 lit) as the share of its lit colour, the way
/// PLAY's keybed shows it: faint, dim and mid well apart, and nothing at 0.
public enum LEDLevel {
    static let points: [(level: Double, share: Double)] = [(0, 0), (12, 0.2), (40, 0.38), (96, 0.62), (255, 1)]

    public static func alpha(_ v: UInt8) -> Double {
        let x = Double(v)
        for (a, b) in zip(points, points.dropFirst()) where x <= b.level {
            return a.share + (b.share - a.share) * (x - a.level) / (b.level - a.level)
        }
        return 1
    }
}

/// The OMX-27 screen's typeface: Chakra Petch Bold from FizzUI's bundle, registered for the process once.
public enum RFMFonts {
    static let hardwareName = "ChakraPetch-Bold"

    private static let registered: Bool = {
        guard let url = Bundle.module.url(forResource: "ChakraPetch-Bold", withExtension: "ttf") else { return false }
        var err: Unmanaged<CFError>?
        if CTFontManagerRegisterFontsForURL(url as CFURL, .process, &err) { return true }
        let e = err?.takeRetainedValue()
        return e.map { CFErrorGetCode($0) == CTFontManagerError.alreadyRegistered.rawValue } ?? false
    }()

    /// true: Chakra Petch is there
    @discardableResult public static func register() -> Bool { registered }

    /// a font of this PostScript name can be made
    public static func available(_ name: String) -> Bool {
        let f = CTFontCreateWithName(name as CFString, 12, nil)
        return CTFontCopyPostScriptName(f) as String == name
    }
}
