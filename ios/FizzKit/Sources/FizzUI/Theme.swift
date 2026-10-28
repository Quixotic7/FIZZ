// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// The web Manager's look (site/manager/manager.css :root, ios/mockups/fizz-ios.html): the palette, the tabs' and the
// tags' colours, the operators' colours, and Inter Tight.

import CoreText
import Foundation
import SwiftUI

// MARK: - Colours

/// An sRGB colour token; `mix` is CSS color-mix(in srgb, self p%, other).
public struct RGB: Sendable, Hashable {
    public let r: Double, g: Double, b: Double

    public init(_ hex: UInt32) {
        r = Double((hex >> 16) & 0xff) / 255
        g = Double((hex >> 8) & 0xff) / 255
        b = Double(hex & 0xff) / 255
    }

    init(r: Double, g: Double, b: Double) { self.r = r; self.g = g; self.b = b }

    public func mix(_ pct: Double, _ other: RGB) -> RGB {
        let p = pct / 100
        return RGB(r: r * p + other.r * (1 - p), g: g * p + other.g * (1 - p), b: b * p + other.b * (1 - p))
    }

    public var color: Color { Color(.sRGB, red: r, green: g, blue: b, opacity: 1) }
    public func opacity(_ o: Double) -> Color { Color(.sRGB, red: r, green: g, blue: b, opacity: o) }
}

public enum FZ {
    public static let bg = RGB(0x000000), panel = RGB(0x0b0b0e), raised = RGB(0x131318), line = RGB(0x22222a)
    public static let text = RGB(0xECECEF), body = RGB(0xC9C9D1), muted = RGB(0x8D8D96)
    public static let cyan = RGB(0x28E1FF), blue = RGB(0x3F7DFF), green = RGB(0x1FD47E), cream = RGB(0xF1F0EA)
    public static let orange = RGB(0xFF6A2C), pink = RGB(0xFF3CA0), yellow = RGB(0xFFD23F), violet = RGB(0x9664FF)
    public static let magenta = RGB(0xF03CFF), champagne = RGB(0xF3D9A4), red = RGB(0xFF6464)
    /// the knobs' colours: the operators C A B1 B2 take them in order
    public static let k1 = RGB(0x3F7DFF), k2 = RGB(0x1FD47E), k3 = RGB(0xF1F0EA), k4 = RGB(0xFF6A2C), kalg = RGB(0xFF8FC8)
    public static let operators = [k1, k2, k3, k4]
    /// the FIZZ word's gradient
    public static let word = Gradient(stops: [
        .init(color: RGB(0x28E1FF).color, location: 0), .init(color: RGB(0x3F7DFF).color, location: 0.30),
        .init(color: RGB(0xF03CFF).color, location: 0.62), .init(color: RGB(0xFF6A2C).color, location: 0.85),
        .init(color: RGB(0xFFD23F).color, location: 1),
    ])
    public static let well = RGB(0x15151a), track = RGB(0x24242b), dim = RGB(0x34343c), nav = RGB(0x2a2a33)
}

// MARK: - Tags

/// FIZZ's seven tags, in the protocol's order (bit k = tag k).
public enum FizzTag: Int, CaseIterable, Sendable, Identifiable {
    case bass, keys, pluck, pad, lead, drums, fx

    public var id: Int { rawValue }
    public var name: String { ["BASS", "KEYS", "PLUCK", "PAD", "LEAD", "DRUMS", "FX"][rawValue] }
    public var tint: RGB { [FZ.blue, FZ.cream, FZ.yellow, FZ.violet, FZ.green, FZ.orange, FZ.pink][rawValue] }
    /// white letters on these
    public var darkTint: Bool { self == .bass || self == .pad || self == .fx }
    public var ink: Color { darkTint ? .white : .black }

    public static func of(mask: Int) -> [FizzTag] { allCases.filter { (mask >> $0.rawValue) & 1 != 0 } }
    /// a preset's colour: its first tag's, else cream (ui.js colOf)
    public static func tint(mask: Int) -> RGB { of(mask: mask).first?.tint ?? FZ.cream }
}

// MARK: - Tabs

public enum FizzTab: String, CaseIterable, Sendable, Identifiable {
    case backup, library, edit, pool, projects, importer

    public var id: String { rawValue }
    public var title: String { self == .importer ? "IMPORT" : rawValue.uppercased() }
    public var tint: RGB {
        switch self {
        case .backup: FZ.yellow
        case .library: FZ.cream
        case .edit: FZ.pink
        case .pool: FZ.green
        case .projects: FZ.cyan
        case .importer: FZ.violet
        }
    }
    public var icon: VectorArt {
        switch self {
        case .backup: Icons.backup
        case .library: Icons.library
        case .edit: Icons.edit
        case .pool: Icons.pool
        case .projects: Icons.projects
        case .importer: Icons.plus
        }
    }
    /// the five pills of the phone's bar (IMPORT is the "+")
    public static let bar: [FizzTab] = [.backup, .library, .edit, .pool, .projects]
}

// MARK: - Inter Tight

public enum FizzFont {
    static let family = "InterTight-Regular"
    private static let registered: Bool = {
        guard let url = Bundle.module.url(forResource: "InterTight", withExtension: "ttf") else { return false }
        var err: Unmanaged<CFError>?
        if CTFontManagerRegisterFontsForURL(url as CFURL, .process, &err) { return true }
        // (already registered by this process: fine)
        let e = err?.takeRetainedValue()
        return e.map { CFErrorGetCode($0) == CTFontManagerError.alreadyRegistered.rawValue } ?? false
    }()

    /// Registers the font once; false when it could not be (the system's condensed heavy font is used then).
    @discardableResult public static func register() -> Bool { registered }

    /// Inter Tight at `weight` (100..900) on its variable axis.
    public static func ctFont(_ size: CGFloat, _ weight: Double = 800) -> CTFont? {
        guard registered else { return nil }
        let wght = 0x7767_6874     // 'wght'
        let attrs: [CFString: Any] = [
            kCTFontNameAttribute: family,
            kCTFontVariationAttribute: [wght: weight],
        ]
        let d = CTFontDescriptorCreateWithAttributes(attrs as CFDictionary)
        return CTFontCreateWithFontDescriptor(d, size, nil)
    }

    public static func font(_ size: CGFloat, _ weight: Double = 800) -> Font {
        if let f = ctFont(size, weight) { return Font(f) }
        let w: Font.Weight = weight >= 750 ? .heavy : weight >= 650 ? .bold : weight >= 550 ? .semibold : .medium
        return Font.system(size: size, weight: w).width(.condensed)
    }
}

public extension View {
    /// Inter Tight, `size` pt, heavy by default; `em`: CSS letter-spacing in em.
    func fz(_ size: CGFloat, _ weight: Double = 800, em: CGFloat = 0) -> some View {
        font(FizzFont.font(size, weight)).tracking(size * em)
    }
}

public extension Text {
    func fzText(_ size: CGFloat, _ weight: Double = 800, em: CGFloat = 0) -> Text {
        font(FizzFont.font(size, weight)).tracking(size * em)
    }
}

// MARK: - Snapshot mode

/// ImageRenderer draws no List / ScrollView / platform views: in snapshot mode the screens lay out in plain stacks
/// and the motion stands still at a fixed moment.
private struct SnapshotKey: EnvironmentKey { static let defaultValue = false }

public extension EnvironmentValues {
    var fizzSnapshot: Bool {
        get { self[SnapshotKey.self] }
        set { self[SnapshotKey.self] = newValue }
    }
}

/// A moment in seconds that runs on screen and stands still in a snapshot.
public struct Animated<Content: View>: View {
    @Environment(\.fizzSnapshot) private var snapshot
    let still: Double
    let content: (Double) -> Content

    public init(still: Double = 1.3, @ViewBuilder content: @escaping (Double) -> Content) {
        self.still = still
        self.content = content
    }

    public var body: some View {
        if snapshot {
            content(still)
        } else {
            TimelineView(.animation) { ctx in content(ctx.date.timeIntervalSinceReferenceDate) }
        }
    }
}
