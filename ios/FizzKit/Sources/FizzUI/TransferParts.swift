// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// The parts RESTORE, IMPORT and EXPORT share (fizz-ios.html .psheet .grab .ft .carry .warn .pchip): their icons, the
// file tab chip, the 10-segment carry bar, a chip flow, the red / orange state box, the sheet's look in a snapshot.

import SwiftUI
import FizzData
import FizzKit
import UniformTypeIdentifiers

// MARK: - The files

extension UTType {
    /// .fizzsnd (one preset, JSON)
    public static let fizzSound = UTType(exportedAs: "com.quixotic7.fizz.sound", conformingTo: .json)
    /// .fizzprj and .fizzpat (PROJECTS' kept project and patterns: phase I6)
    public static let fizzProject = UTType(exportedAs: "com.quixotic7.fizz.project", conformingTo: .data)
    public static let fizzPatterns = UTType(exportedAs: "com.quixotic7.fizz.patterns", conformingTo: .data)
    /// Elektron Digitone .dnsnd, Digitone II .dn2pst / .dn2snd (read only)
    public static let digitoneSound = UTType(importedAs: "com.elektron.digitone.dnsnd", conformingTo: .data)
    public static let digitone2Preset = UTType(importedAs: "com.elektron.digitone2.dn2pst", conformingTo: .data)
    public static let digitone2Sound = UTType(importedAs: "com.elektron.digitone2.dn2snd", conformingTo: .data)

    /// What IMPORT opens (importer.js IMPORT_FILES).
    static let importable: [UTType] = [.fizzLibrary, .fizzSound, .fizzBackup, .digitoneSound, .digitone2Preset, .digitone2Sound]
}

/// importer.js IMPORT_FILES: .fizzlib .fizzsnd .fizzbak .dnsnd .dn2pst .dn2snd (never .syx).
let importExtensions: Set<String> = ["fizzlib", "fizzsnd", "fizzbak", "dnsnd", "dn2pst", "dn2snd"]

// MARK: - Icons (fizz-ios.html IC)

enum TransferIcons {
    static let restore = VectorArt(24, 24, [.path("M12 3v11M7.5 9.5L12 14l4.5-4.5M4 20h16")])
    static let warn = VectorArt(24, 24, [.path("M12 3.5L2.5 20h19z"), .path("M12 10v4.5M12 17.4v.1")])
    static let chev = VectorArt(24, 24, [.path("M6 6l6 6-6 6M13 6l6 6-6 6")])
    static let look = VectorArt(24, 24, [.circle(10.5, 10.5, 6.5), .path("M15.5 15.5l5 5")])
    static let plug = VectorArt(24, 24, [.path("M9 2.5v4M15 2.5v4"), .rect(5.5, 6.5, 13, 7, 2), .path("M12 13.5v3.5a3.5 3.5 0 0 1-3.5 3.5H6")])
    static let play = VectorArt(16, 16, [.path("M4 2.5l9.5 5.5L4 13.5z")])
    static let stop = VectorArt(16, 16, [.rect(3.5, 3.5, 9, 9, 1.5)])
    static let close = VectorArt(24, 24, [.path("M6.5 6.5l11 11M17.5 6.5l-11 11")])
    static let check = VectorArt(24, 24, [.path("M5 12.5l5 5L19.5 7")])
    static let down = VectorArt(24, 24, [.path("M12 4v14M6 12l6 6 6-6")])
}

// MARK: - Small parts

/// The page's colour tokens as the JS names them ("--red", "--orange", ..; "--midi" violet).
func tokenColor(_ t: String) -> RGB {
    switch t {
    case "--red": FZ.red
    case "--orange": FZ.orange
    case "--yellow": FZ.yellow
    case "--green": FZ.green
    case "--cyan": FZ.cyan
    case "--magenta": FZ.magenta
    case "--violet", "--midi": FZ.violet
    case "--pink": FZ.pink
    default: FZ.muted
    }
}

/// The file tab (.ft): the maker and the extension, its corner cut like a folder's tab; dim when absent.
struct FileTab: View {
    let maker: String
    let ext: String
    let color: RGB
    var on = true
    var size: CGFloat = 13
    var height: CGFloat = 38

    var body: some View {
        let shape = UnevenRoundedRectangle(topLeadingRadius: 4, bottomLeadingRadius: 10, bottomTrailingRadius: 10, topTrailingRadius: 14)
        HStack(spacing: 6) {
            Text(maker).fz(size, 800)
            Text(ext).fz(size, 700).opacity(0.85)
        }
        .foregroundStyle(color.color)
        .lineLimit(1)
        .padding(.horizontal, 10)
        .frame(height: height)
        .background(shape.fill(color.opacity(0.09)))
        .overlay(shape.strokeBorder(color.color, style: StrokeStyle(lineWidth: 2, dash: on ? [] : [5, 4])))
        .shadow(color: on ? color.opacity(0.4) : .clear, radius: 7)
        .opacity(on ? 1 : 0.35)
    }
}

/// The carry / fidelity bar: 10 segments, `n` lit in its colour (10 green, 8-9 yellow, less orange: importer.js fidC).
struct CarryBar: View {
    let n: Int
    var height: CGFloat = 9
    var spacing: CGFloat = 4
    @Environment(\.fizzSnapshot) private var snapshot
    @State private var shown = false

    static func color(_ n: Int) -> RGB { n >= 10 ? FZ.green : n >= 8 ? FZ.yellow : FZ.orange }

    var body: some View {
        HStack(spacing: spacing) {
            ForEach(0..<10, id: \.self) { k in
                RoundedRectangle(cornerRadius: height / 3)
                    .fill(FZ.track.color)
                    .overlay(alignment: .leading) {
                        RoundedRectangle(cornerRadius: height / 3).fill(Self.color(n).color)
                            .scaleEffect(x: k < n && (shown || snapshot) ? 1 : 0.001, anchor: .leading)
                            .animation(.spring(response: 0.35, dampingFraction: 0.8).delay(Double(k) * 0.055), value: shown)
                    }
            }
        }
        .frame(height: height)
        .onAppear { shown = true }
    }
}

/// The red (or orange) state box (.warn): an icon and a short state in capitals.
struct StateBox: View {
    let icon: VectorArt
    let text: String
    var color: RGB = FZ.red
    var blink = false
    /// (centred: DO NOT UNPLUG)
    var centred = false

    var body: some View {
        HStack(spacing: 14) {
            if centred { Spacer(minLength: 0) }
            Animated(still: 0.7) { t in
                IconView(icon, color: color.color, lineWidth: 2.4, backdrop: .clear)
                    .frame(width: 38, height: 38)
                    .opacity(blink ? 0.2 + 0.8 * breathe(t, 1.4) : 1)
            }
            .frame(width: 38, height: 38)
            Text(text).fz(21, 800, em: 0.01).foregroundStyle(color.color).lineLimit(2).minimumScaleFactor(0.7)
                .fixedSize(horizontal: false, vertical: true)
            Spacer(minLength: 0)
        }
        .padding(.horizontal, 16).padding(.vertical, 14)
        .background(RoundedRectangle(cornerRadius: 16).fill(color.opacity(0.13)))
        .overlay(RoundedRectangle(cornerRadius: 16).strokeBorder(color.opacity(0.7), lineWidth: 2))
    }
}

/// A chip (.pchip): a name, or a coloured state.
struct NameChip: View {
    let text: String
    var fill: RGB?
    var on = true
    var body: some View {
        Text(text).fz(13, 800, em: 0.02)
            .foregroundStyle(fill != nil ? Color.black : FZ.text.color)
            .lineLimit(1)
            .padding(.horizontal, 12).frame(height: 34)
            .background(Capsule().fill(fill?.color ?? FZ.raised.color))
            .overlay(Capsule().strokeBorder(fill?.color ?? FZ.line.color, lineWidth: 1.5))
            .opacity(on ? 1 : 0.4)
            .fixedSize()
    }
}

/// The round close button of a sheet (an X in a ring).
struct SheetCloseButton: View {
    let action: () -> Void
    var body: some View {
        Button(action: action) {
            IconView(TransferIcons.close, color: FZ.body.color, lineWidth: 2.2, backdrop: .clear)
                .frame(width: 16, height: 16)
                .frame(width: 36, height: 36)
                .overlay(Circle().strokeBorder(FZ.line.color, lineWidth: 1.5))
                .contentShape(Circle())
        }
        .buttonStyle(.plain)
    }
}

/// Chips left to right, wrapping (.pchips).
struct ChipFlow: Layout {
    var spacing: CGFloat = 7
    var lineSpacing: CGFloat = 7

    func sizeThatFits(proposal: ProposedViewSize, subviews: Subviews, cache: inout ()) -> CGSize {
        let w = proposal.width ?? .infinity
        var x: CGFloat = 0, y: CGFloat = 0, line: CGFloat = 0, widest: CGFloat = 0
        for s in subviews {
            let z = s.sizeThatFits(.unspecified)
            if x > 0 && x + z.width > w { y += line + lineSpacing; x = 0; line = 0 }
            x += z.width + spacing
            line = max(line, z.height)
            widest = max(widest, x - spacing)
        }
        return CGSize(width: proposal.width ?? widest, height: y + line)
    }

    func placeSubviews(in b: CGRect, proposal: ProposedViewSize, subviews: Subviews, cache: inout ()) {
        var x = b.minX, y = b.minY, line: CGFloat = 0
        for s in subviews {
            let z = s.sizeThatFits(.unspecified)
            if x > b.minX && x + z.width > b.maxX { y += line + lineSpacing; x = b.minX; line = 0 }
            s.place(at: CGPoint(x: x, y: y), proposal: ProposedViewSize(z))
            x += z.width + spacing
            line = max(line, z.height)
        }
    }
}

/// The sheet's grab handle.
struct SheetGrab: View {
    var body: some View {
        Capsule().fill(RGB(0x3a3a44).color).frame(width: 40, height: 5).frame(maxWidth: .infinity)
    }
}

/// A sheet as it shows over a screen (the snapshots: ImageRenderer draws no real sheet): the screen dimmed under a
/// scrim, the sheet's panel from `top` with its rounded top, its colour's edge and glow, the grab handle.
public struct SnapshotSheet<Under: View, Content: View>: View {
    let top: CGFloat
    let color: RGB?
    let under: Under
    let content: Content
    @Environment(\.fizzInsets) private var insets

    public init(top: CGFloat = 62, color: RGB? = nil, @ViewBuilder under: () -> Under, @ViewBuilder content: () -> Content) {
        self.top = top; self.color = color; self.under = under(); self.content = content()
    }

    public var body: some View {
        let shape = UnevenRoundedRectangle(topLeadingRadius: 30, topTrailingRadius: 30)
        ZStack(alignment: .top) {
            under.blur(radius: 2).brightness(-0.3)
            Color.black.opacity(0.6)
            VStack(spacing: 0) {
                SheetGrab().padding(.top, 10).padding(.bottom, 4)
                content.environment(\.fizzInsets, EdgeInsets(top: 0, leading: 0, bottom: insets.bottom, trailing: 0))
            }
            .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .top)
            .background(shape.fill(RGB(0x0d0d11).color))
            .overlay(shape.stroke((color ?? RGB(0x2a2a33)).color, lineWidth: 1.5).mask(LinearGradient(colors: [.white, .clear], startPoint: .top, endPoint: .init(x: 0.5, y: 0.08))))
            .shadow(color: (color ?? FZ.bg).opacity(0.22), radius: 15, y: -2)
            .padding(.top, insets.top + top - 54)
        }
    }
}
