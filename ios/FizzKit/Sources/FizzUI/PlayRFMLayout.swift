// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// PLAY's panel as RemoteFM1's (mock-ups 49-54), in points on three boards: the iPhone upright (390 x 844, the status
// bar included), the iPhone on its side (844 x 390) and the iPad (1180 x 820). Every number is the approved
// mock-ups' (ios/mockups/fizz-ios.html cpFull, cpLand, cpPad, cpSteps, cpPanel), measured on RemoteFM1's own screens;
// none of RemoteFM1's code is used. What the mock-ups do not draw is laid on the same grid: STEPS on its side and on
// the iPad (the 16 steps where KN1-4 and the buttons were, KN1-4 in MAIN's 2 x 2), SCREEN (the LCD alone).
// PlayView draws a layout; the tests check it (PlayLayoutTests).

import CoreGraphics
import Foundation

/// One of PLAY's boards.
public enum PlayBoard: Sendable, CaseIterable {
    case phone, phoneWide, pad

    public var size: CGSize {
        switch self {
        case .phone: CGSize(width: 390, height: 844)
        case .phoneWide: CGSize(width: 844, height: 390)
        case .pad: CGSize(width: 1180, height: 820)
        }
    }
}

/// Where everything of one view sits on one board.
public struct PlayLayout: Sendable, Equatable {
    /// A knob: its centre, its top, its size; the label (Chakra Petch, 17 % of the knob, 9 at least) over it.
    public struct Knob: Sendable, Equatable {
        public let name: String
        public let cx: CGFloat, top: CGFloat, size: CGFloat
        public var labelSize: CGFloat { max(9, (size * 0.34).rounded() / 2) }
        /// the label line's top: the knob's top less the label's size less 6
        public var labelTop: CGFloat { top - labelSize - 6 }
        public var cap: CGRect { CGRect(x: cx - size / 2, y: top, width: size, height: size) }
        /// the hard shadow under the cap
        public var shadow: CGFloat { max(3, (size * 0.06).rounded()) }
    }

    /// A well of keys: `cols` across, each bw x bh, the gaps, the padding; the legends' base size and the radii.
    public struct Well: Sendable, Equatable {
        public let x: CGFloat, y: CGFloat
        public let cols: Int, count: Int
        public let bw: CGFloat, bh: CGFloat, gap: CGFloat, pad: CGFloat
        public let legend: CGFloat, radius: CGFloat, wellRadius: CGFloat

        init(_ x: CGFloat, _ y: CGFloat, cols: Int, count: Int, _ bw: CGFloat, _ bh: CGFloat, gap: CGFloat, pad: CGFloat,
             legend: CGFloat? = nil, radius: CGFloat? = nil, wellRadius: CGFloat? = nil) {
            self.x = x; self.y = y; self.cols = cols; self.count = count
            self.bw = bw; self.bh = bh; self.gap = gap; self.pad = pad
            // (the legend: 26 % of the key's height, to the half point, 10 at least)
            self.legend = legend ?? max(10, (bh * 0.52).rounded() / 2)
            self.radius = radius ?? (bh * 0.24).rounded()
            self.wellRadius = wellRadius ?? (bh * 0.34).rounded() + 6
        }

        public var rows: Int { (count + cols - 1) / cols }
        public var frame: CGRect {
            CGRect(x: x, y: y, width: CGFloat(cols) * bw + CGFloat(cols - 1) * gap + 2 * pad,
                   height: CGFloat(rows) * bh + CGFloat(rows - 1) * gap + 2 * pad)
        }
        /// key i's frame in the well
        public func key(_ i: Int) -> CGRect {
            CGRect(x: pad + CGFloat(i % cols) * (bw + gap), y: pad + CGFloat(i / cols) * (bh + gap), width: bw, height: bh)
        }
    }

    /// The header's chip (THIS iPHONE, FM-1, a backup's pill): its left, top and height, its type size.
    public struct Chip: Sendable, Equatable {
        public let x: CGFloat, y: CGFloat, height: CGFloat, font: CGFloat
    }

    /// The FIZZ badge: the pill (its right edge) or the round one sideways.
    public struct Badge: Sendable, Equatable {
        public let right: CGFloat, top: CGFloat, height: CGFloat
        public let round: Bool
    }

    /// The LED keybed: its frame; scrolling (`keyW`) or all 16 in a row; step numbers on the white keys.
    public struct Keybed: Sendable, Equatable {
        public let frame: CGRect
        public let keyW: CGFloat?
        public var numbers = false
    }

    public let board: PlayBoard
    public var chip: Chip?
    public var grabber: CGRect
    public var badge: Badge
    public var knobs: [Knob] = []
    /// F1 / F2 (OCT - / + in STEPS)
    public var tray: Well?
    public var screen: CGRect
    public var led: CGRect?
    /// the 12 buttons
    public var well: Well?
    /// STEPS' 16 steps
    public var pads: Well?
    public var keybed: Keybed?
    /// CONTROLS: the MAIN | KN 1-4 switch's middle line, the BUTTONS | KEYS deck
    public var knobSwitchY: CGFloat?
    public var deck: CGRect?
    /// the page dots' row (four), nil: none
    public var dots: CGRect?

    public var size: CGSize { board.size }

    static let main = ["MASTER", "SELECT", "PRESETS", "ALGORITHM"]
    static let kn = ["KN1", "KN2", "KN3", "KN4"]

    /// The layout of `view` on `board` (`knobsKN`: CONTROLS' knob row shows KN 1-4).
    public static func of(_ view: PlayViewKind, on board: PlayBoard, knobsKN: Bool = false) -> PlayLayout {
        switch (board, view) {
        case (.phone, .full): phoneFull()
        case (.phone, .panel): phoneControls(keys: false, kn: knobsKN)
        case (.phone, .keys): phoneControls(keys: true, kn: knobsKN)
        case (.phone, .steps): phoneSteps()
        case (.phone, .screen): phoneScreen()
        case (.phoneWide, .steps): wideSteps()
        case (.phoneWide, .screen): wideScreen()
        case (.phoneWide, _): wideFull()
        case (.pad, .steps): padSteps()
        case (.pad, .screen): padScreen()
        case (.pad, _): padFull()
        }
    }

    // MARK: the iPhone upright (49, 52, 54)

    private static func phoneHead(_ v: PlayViewKind) -> PlayLayout {
        PlayLayout(board: .phone, chip: Chip(x: 16, y: 52, height: 28, font: 13.5),
                   grabber: CGRect(x: 175, y: 63.5, width: 40, height: 5),
                   badge: Badge(right: 16, top: 52, height: 28, round: false),
                   screen: .zero, dots: dotsRow(.phone))
    }

    static func dotsRow(_ b: PlayBoard) -> CGRect {
        CGRect(x: 0, y: b.size.height - 18 - 24, width: b.size.width, height: 24)
    }

    /// 49: FULL.
    static func phoneFull() -> PlayLayout {
        var L = phoneHead(.full)
        L.knobs = [Knob(name: "MASTER", cx: 53, top: 111, size: 46), Knob(name: "SELECT", cx: 127, top: 111, size: 46),
                   Knob(name: "PRESETS", cx: 53, top: 186, size: 46), Knob(name: "ALGORITHM", cx: 127, top: 186, size: 46)]
            + zip(kn, [76, 163, 250, 337] as [CGFloat]).map { Knob(name: $0, cx: $1, top: 331, size: 46) }
        L.tray = Well(16, 246, cols: 2, count: 2, 66, 42, gap: 6, pad: 5, legend: 13, radius: 10, wellRadius: 14)
        L.screen = CGRect(x: 172, y: 96, width: 202, height: 202)
        L.led = CGRect(x: 16, y: 345, width: 10, height: 10)
        L.well = Well(21, 400, cols: 6, count: 12, 48, 42, gap: 8, pad: 10)
        L.keybed = Keybed(frame: CGRect(x: 16, y: 530, width: 358, height: 272), keyW: 40)
        return L
    }

    /// 54: CONTROLS (BUTTONS, or KEYS: RemoteFM1's keys deck in the well's place).
    static func phoneControls(keys: Bool, kn: Bool) -> PlayLayout {
        var L = phoneHead(keys ? .keys : .panel)
        L.screen = CGRect(x: 25, y: 92, width: 340, height: 340)
        L.knobs = zip(kn ? Self.kn : main, [50.5, 146.5, 242.5, 338.5] as [CGFloat]).map { Knob(name: $0, cx: $1, top: 459, size: 50) }
        L.knobSwitchY = 523 + 16
        L.deck = CGRect(x: 95, y: 565, width: 200, height: 36)
        if keys {
            L.keybed = Keybed(frame: CGRect(x: 16, y: 611, width: 358, height: 186), keyW: 40, numbers: true)
        } else {
            L.well = Well(15, 611, cols: 6, count: 12, 50, 48, gap: 8, pad: 10)
            L.tray = Well(95, 747, cols: 2, count: 2, 92, 38, gap: 6, pad: 5, legend: 13, radius: 10, wellRadius: 14)
        }
        return L
    }

    /// 52: STEPS on FULL's grid: KN 1-4 in MAIN's 2 x 2, OCT - / + in the tray, the 16 steps in a well, the keybed.
    static func phoneSteps() -> PlayLayout {
        var L = phoneHead(.steps)
        L.knobs = [Knob(name: "KN1", cx: 53, top: 111, size: 46), Knob(name: "KN2", cx: 127, top: 111, size: 46),
                   Knob(name: "KN3", cx: 53, top: 186, size: 46), Knob(name: "KN4", cx: 127, top: 186, size: 46)]
        L.tray = Well(16, 246, cols: 2, count: 2, 66, 42, gap: 6, pad: 5, legend: 13, radius: 10, wellRadius: 14)
        L.screen = CGRect(x: 172, y: 96, width: 202, height: 202)
        L.pads = Well(16, 316, cols: 8, count: 16, 35.25, 64, gap: 8, pad: 10, radius: 12, wellRadius: 22)
        L.keybed = Keybed(frame: CGRect(x: 16, y: 484, width: 358, height: 318), keyW: 40)
        return L
    }

    /// SCREEN (not drawn): the LCD alone, as wide as FULL's panel, in the middle.
    static func phoneScreen() -> PlayLayout {
        var L = phoneHead(.screen)
        L.screen = CGRect(x: 16, y: 243, width: 358, height: 358)
        return L
    }

    // MARK: the iPhone on its side (50)

    private static func wideHead() -> PlayLayout {
        PlayLayout(board: .phoneWide, chip: nil, grabber: CGRect(x: 402, y: 19.7, width: 40, height: 5),
                   badge: Badge(right: 844 - 799 - 40, top: 16, height: 40, round: true), screen: .zero)
    }

    static func wideFull() -> PlayLayout {
        var L = wideHead()
        L.knobs = [Knob(name: "MASTER", cx: 87, top: 35, size: 38), Knob(name: "SELECT", cx: 153, top: 35, size: 38),
                   Knob(name: "PRESETS", cx: 87, top: 96, size: 38), Knob(name: "ALGORITHM", cx: 153, top: 96, size: 38)]
            + zip(kn, [461.5, 561.5, 661.3, 761] as [CGFloat]).map { Knob(name: $0, cx: $1, top: 35, size: 38) }
        L.tray = Well(54, 144, cols: 2, count: 2, 58, 38, gap: 6, pad: 5, legend: 12.5, radius: 10, wellRadius: 14)
        L.screen = CGRect(x: 198, y: 20, width: 172, height: 172)
        L.led = CGRect(x: 386, y: 45, width: 9, height: 9)
        L.well = Well(386, 88, cols: 6, count: 12, 58, 38, gap: 8, pad: 10)
        L.keybed = Keybed(frame: CGRect(x: 50, y: 206, width: 744, height: 170), keyW: nil)
        return L
    }

    /// STEPS sideways (not drawn): KN 1-4 in MAIN's place, the steps where KN1-4 and the buttons were.
    static func wideSteps() -> PlayLayout {
        var L = wideFull()
        L.knobs = zip(kn, L.knobs.prefix(4)).map { Knob(name: $0, cx: $1.cx, top: $1.top, size: $1.size) }
        L.led = nil
        L.well = nil
        L.pads = Well(386, 36, cols: 8, count: 16, 41.5, 64, gap: 8, pad: 10, radius: 12, wellRadius: 22)
        return L
    }

    static func wideScreen() -> PlayLayout {
        var L = wideHead()
        L.screen = CGRect(x: 249, y: 34, width: 346, height: 346)
        return L
    }

    // MARK: the iPad (51)

    private static func padHead() -> PlayLayout {
        PlayLayout(board: .pad, chip: Chip(x: 47.5, y: 26.5, height: 30, font: 14.5),
                   grabber: CGRect(x: (1180 - 47) / 2, y: 26.5 + 15 - 2.5, width: 47, height: 5),
                   badge: Badge(right: 47.5, top: 26.5, height: 30, round: false), screen: .zero, dots: dotsRow(.pad))
    }

    static func padFull() -> PlayLayout {
        var L = padHead()
        L.knobs = [Knob(name: "MASTER", cx: 95, top: 103.5, size: 63), Knob(name: "SELECT", cx: 189.8, top: 103.5, size: 63),
                   Knob(name: "PRESETS", cx: 95, top: 213, size: 63), Knob(name: "ALGORITHM", cx: 189.8, top: 213, size: 63)]
            + zip(kn, [694.3, 825, 956, 1087] as [CGFloat]).map { Knob(name: $0, cx: $1, top: 104, size: 63) }
        L.tray = Well(47.5, 328.5, cols: 2, count: 2, 86.75, 43, gap: 6, pad: 5, legend: 15, radius: 11, wellRadius: 15)
        L.screen = CGRect(x: 269.5, y: 86.5, width: 295, height: 295)
        L.led = CGRect(x: 598, y: 125, width: 10, height: 10)
        L.well = Well(599, 230, cols: 6, count: 12, 76.9, 59, gap: 10, pad: 10)
        L.keybed = Keybed(frame: CGRect(x: 47.5, y: 417, width: 1085, height: 356), keyW: nil)
        return L
    }

    /// STEPS on the iPad (not drawn): as sideways, the steps from the screen's top to its foot.
    static func padSteps() -> PlayLayout {
        var L = padFull()
        L.knobs = zip(kn, L.knobs.prefix(4)).map { Knob(name: $0, cx: $1.cx, top: $1.top, size: $1.size) }
        L.led = nil
        L.well = nil
        L.pads = Well(599, 86.5, cols: 8, count: 16, 55.2, 132.5, gap: 10, pad: 10, radius: 14, wellRadius: 26)
        return L
    }

    static func padScreen() -> PlayLayout {
        var L = padHead()
        L.screen = CGRect(x: 270, y: 92, width: 640, height: 640)
        return L
    }
}

/// The legends' fit (mock-ups 49-54): RemoteFM1's size with 0.06 em tracking; a name too long for its key drops the
/// tracking, then shrinks to fit, to the quarter point. Widths in em of Chakra Petch Bold, no tracking.
public enum PlayLegend {
    static let widths: [String: CGFloat] = [
        "FX": 1.228, "TRACKS": 3.848, "MIX": 1.729, "LATCH": 3.144, "EDIT": 2.154, "SHIFT": 2.771, "HOME": 2.805,
        "SOUNDS": 4.059, "MIDI FX": 2.041, "PATTERNS": 5.075, "PLAY": 2.487, "REC": 1.936,
    ]

    /// A name's width in em (its widest line), tracking 0.
    public static func width(_ name: String) -> CGFloat {
        if let w = widths[name] { return w }
        return name.split(separator: " ").map { CGFloat($0.count) * 0.62 }.max() ?? 3
    }

    /// The legend's size and tracking (em) for `name` on a key `bw` wide at `base` points.
    public static func fit(_ name: String, keyWidth bw: CGFloat, base: CGFloat) -> (size: CGFloat, tracking: CGFloat) {
        let room = bw - 6, w = width(name)
        let len = CGFloat(name.split(separator: " ").first?.count ?? name.count)
        if (w + 0.06 * len) * base <= room { return (base, 0.06) }
        return (min(base, (room / w * 4).rounded(.down) / 4), 0)
    }
}
