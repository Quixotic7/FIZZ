// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// PLAY's looks (mock-ups 29-33, after the owner's RemoteFM1 design): its six themes (Tangerine, Midnight, Mint, Lilac,
// Cream as RemoteFM1's P, and FIZZ, black and neon), the emulator's accent (the THIS iPHONE pill, the sheet's picks),
// Chakra Petch for the panel's labels (SIL OFL 1.1, Resources/OFL-ChakraPetch.txt), the FM-1 and phone glyphs.

import CoreText
import Foundation
import SwiftUI

/// One of PLAY's six themes.
public enum PlayTheme: String, CaseIterable, Sendable, Identifiable {
    case tangerine, midnight, mint, lilac, cream, fizz

    public var id: String { rawValue }
    public var name: String { self == .fizz ? "FIZZ" : rawValue.capitalized }

    public struct Palette: Sendable {
        public let body, label, bed, key, hi, lo, keyText, knob, mark, bezel, led, ledOff: RGB
    }

    public var palette: Palette {
        switch self {
        case .tangerine: Palette(body: RGB(0xEE7A3C), label: RGB(0x3A1606), bed: RGB(0x874332), key: RGB(0x924E3A),
                                 hi: RGB(0xA9614B), lo: RGB(0x64301F), keyText: RGB(0xFBEDE3), knob: RGB(0x1B1715),
                                 mark: RGB(0xF4EDE6), bezel: RGB(0x141210), led: RGB(0xFFE6A8), ledOff: RGB(0x6F6668))
        case .midnight: Palette(body: RGB(0x1E2230), label: RGB(0xC9D2E6), bed: RGB(0x12151D), key: RGB(0x2B3244),
                                hi: RGB(0x3B4359), lo: RGB(0x0A0C12), keyText: RGB(0xE6ECF8), knob: RGB(0x0A0B0F),
                                mark: RGB(0x6CF0C2), bezel: RGB(0x05060A), led: RGB(0x6CF0C2), ledOff: RGB(0x454B5C))
        case .mint: Palette(body: RGB(0xA8DCC8), label: RGB(0x143329), bed: RGB(0x2F5B52), key: RGB(0x376A60),
                            hi: RGB(0x4A7F74), lo: RGB(0x1E3F38), keyText: RGB(0xEAF7F1), knob: RGB(0x1A2421),
                            mark: RGB(0xF2FFF9), bezel: RGB(0x121816), led: RGB(0xFFF3B0), ledOff: RGB(0x6E8580))
        case .lilac: Palette(body: RGB(0xC9AEE6), label: RGB(0x2A1642), bed: RGB(0x573B77), key: RGB(0x634486),
                             hi: RGB(0x77589B), lo: RGB(0x3B2655), keyText: RGB(0xF6EFFC), knob: RGB(0x1C1624),
                             mark: RGB(0xF7F0FF), bezel: RGB(0x120E18), led: RGB(0xFFD6F0), ledOff: RGB(0x8A7A9A))
        case .cream: Palette(body: RGB(0xEFE6D2), label: RGB(0x2A231C), bed: RGB(0x2A2520), key: RGB(0x3A332C),
                             hi: RGB(0x4B433A), lo: RGB(0x141110), keyText: RGB(0xF3EBDC), knob: RGB(0xE8622A),
                             mark: RGB(0xFFF4E8), bezel: RGB(0x15120F), led: RGB(0xFF7A3D), ledOff: RGB(0x6A6158))
        case .fizz: Palette(body: RGB(0x0E0E13), label: RGB(0xC9C9D1), bed: RGB(0x050507), key: RGB(0x1C1C24),
                            hi: RGB(0x2C2C37), lo: RGB(0x000000), keyText: RGB(0xECECEF), knob: RGB(0x000000),
                            mark: RGB(0x28E1FF), bezel: RGB(0x000000), led: RGB(0x28E1FF), ledOff: RGB(0x34343E))
        }
    }

    /// The theme PLAY last kept (UserDefaults "play.theme").
    static var saved: PlayTheme { PlayTheme(rawValue: UserDefaults.standard.string(forKey: "play.theme") ?? "") ?? .tangerine }

    /// The emulator's colour in the app (the pill, the sheet's picks): tangerine, or FIZZ's cyan.
    public var accent: RGB { self == .fizz ? FZ.cyan : RGB(0xFF8A4C) }

    /// The status bar's letters: dark on the light bodies.
    public var lightBody: Bool { self == .tangerine || self == .mint || self == .lilac || self == .cream }
}

/// The sheet's own colours (PhoneSheet: a warm black).
enum SheetInk {
    static let bg = RGB(0x17140F), tile = RGB(0x2A251F), line = RGB(0x3A352D), list = RGB(0x211E19), rule = RGB(0x2E2A23)
    static let text = RGB(0xF3EBDC), soft = RGB(0xB5AB99), head = RGB(0xA69C8A), off = RGB(0x6E6557)
}

/// FIZZ's knob colours (PLAYING.md: SELECT yellow, KN1..4 blue, green, white, orange, ALGORITHM pink).
enum KnobInk {
    static let select = FZ.yellow, algorithm = FZ.pink
    static let kn = [FZ.k1, FZ.k2, FZ.k3, FZ.k4]
}

// MARK: - Chakra Petch

enum PlayFont {
    static let name = "ChakraPetch-Bold"
    private static let registered: Bool = {
        guard let url = Bundle.module.url(forResource: "ChakraPetch-Bold", withExtension: "ttf") else { return false }
        var err: Unmanaged<CFError>?
        if CTFontManagerRegisterFontsForURL(url as CFURL, .process, &err) { return true }
        let e = err?.takeRetainedValue()
        return e.map { CFErrorGetCode($0) == CTFontManagerError.alreadyRegistered.rawValue } ?? false
    }()

    static func font(_ size: CGFloat) -> Font {
        registered ? Font.custom(name, fixedSize: size) : Font.system(size: size, weight: .bold).width(.condensed)
    }
}

extension View {
    /// The panel's lettering: Chakra Petch Bold, `size` pt, spaced `em`.
    func petch(_ size: CGFloat, em: CGFloat = 0.06) -> some View {
        font(PlayFont.font(size)).tracking(size * em)
    }
}

// MARK: - Glyphs

enum PlayIcons {
    /// the FM-1 (PLAY's tab)
    static let fm1 = VectorArt(24, 24, [.rect(2, 4.5, 20, 15, 3.2), .rect(8.5, 7.2, 7, 5.8, 1.2), .circle(5.4, 8.6, 0.9),
                                        .circle(18.6, 8.6, 0.9), .path("M5 16.3h14")])
    /// a phone (THIS iPHONE)
    static let phone = VectorArt(24, 24, [.rect(6.5, 2.5, 11, 19, 2.8), .path("M10.5 18.3h3")])
    static let chevron = VectorArt(24, 24, [.path("M9 5l7 7-7 7")])
}
