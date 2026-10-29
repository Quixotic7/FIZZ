// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// The small parts the screens share (fizz-ios.html: .unit .card .h .tag .ptag .btn .fchip .seg .cas), the rising
// bubbles and the burst.

import SwiftUI
import FizzKit

// MARK: - Insets

/// Room kept for the status bar and the home indicator. Zero in the app (the safe area does it); a snapshot sets
/// the phone's.
private struct InsetsKey: EnvironmentKey { static let defaultValue = EdgeInsets() }

public extension EnvironmentValues {
    var fizzInsets: EdgeInsets {
        get { self[InsetsKey.self] }
        set { self[InsetsKey.self] = newValue }
    }
}

// MARK: - Top bar

/// The bubble logo, the word, and the unit pill when a unit is connected.
struct TopBar: View {
    let model: UnitModel
    var word = true
    var unit = true
    @State private var ports = false

    var body: some View {
        HStack(spacing: 10) {
            // a long press on the logo: the PORTS sheet (what the app sees, for when the FM-1 isn't found)
            HStack(spacing: 10) {
                BubbleLogo().frame(width: 32, height: 32)
                if word { FizzWord(size: 26) }
            }
            .contentShape(Rectangle())
            .onLongPressGesture(minimumDuration: 0.6) { ports = true }
            Spacer(minLength: 0)
            if unit, model.unit != nil { UnitBadge(model: model).onLongPressGesture(minimumDuration: 0.6) { ports = true } }
        }
        .frame(height: 56)
        .padding(.horizontal, 16)
        .sheet(isPresented: $ports) { PortsSheet(model: model) }
    }
}

/// The connected unit: a green dot, FIZZ's version, USB or BT.
struct UnitPill: View {
    let info: UnitInfo
    var body: some View {
        HStack(spacing: 8) {
            Circle().fill(FZ.green.color).frame(width: 10, height: 10).shadow(color: FZ.green.color, radius: 4)
            Text(info.shortVersion ?? info.idText).fz(13, 800, em: 0.02).foregroundStyle(FZ.text.color)
            Text(info.via).fz(12, 800, em: 0.08).foregroundStyle(FZ.green.color)
        }
        .lineLimit(1)
        .padding(.leading, 11).padding(.trailing, 13)
        .frame(height: 36)
        .background(Capsule().fill(FZ.green.mix(16, FZ.bg).color))
        .overlay(Capsule().strokeBorder(FZ.green.color, lineWidth: 2))
        .fixedSize()
    }
}

// MARK: - Cards, headers, tags

struct Card<Content: View>: View {
    var padding: CGFloat = 18
    @ViewBuilder let content: Content
    var body: some View {
        VStack(alignment: .leading, spacing: 0) { content }
            .padding(padding)
            .frame(maxWidth: .infinity, alignment: .leading)
            .background(RoundedRectangle(cornerRadius: 22).fill(FZ.panel.color))
            .overlay(RoundedRectangle(cornerRadius: 22).strokeBorder(FZ.line.color, lineWidth: 1))
    }
}

/// A section's heading (.h): small, heavy, spaced, muted; a number after it in white.
struct Heading: View {
    let text: String
    var count: String?
    var trailing: String?
    var color: RGB = FZ.muted
    var body: some View {
        HStack(spacing: 10) {
            Text(text).fz(13, 800, em: 0.12).foregroundStyle(color.color)
            if let count { Text(count).fz(15, 800).foregroundStyle(FZ.text.color) }
            if let trailing {
                Spacer(minLength: 0)
                Text(trailing).fz(15, 800).foregroundStyle(FZ.text.color)
            }
        }
        .lineLimit(1)
    }
}

struct TagPill: View {
    let tag: FizzTag
    var body: some View {
        Text(tag.name).fz(12, 800, em: 0.05).foregroundStyle(tag.ink)
            .padding(.horizontal, 8).frame(height: 22)
            .background(RoundedRectangle(cornerRadius: 6).fill(tag.tint.color))
            .fixedSize()
    }
}

struct PoolBadge: View {
    var body: some View {
        Text("POOL").fz(12, 800, em: 0.03).foregroundStyle(.black)
            .padding(.horizontal, 6).frame(height: 20)
            .background(RoundedRectangle(cornerRadius: 5).fill(FZ.green.color))
            .fixedSize()
    }
}

// MARK: - Buttons

/// The page's .btn: a coloured capsule with black letters and a glow; ghost: outlined.
struct PillButton: View {
    let title: String
    var color: RGB = FZ.cyan
    var ghost = false
    var height: CGFloat = 52
    var size: CGFloat = 16
    var dot = false
    var padding: CGFloat = 20
    var action: () -> Void = {}

    var body: some View {
        Button(action: action) {
            HStack(spacing: 9) {
                if dot { Circle().fill(ghost ? FZ.text.color : .black).frame(width: size * 0.95, height: size * 0.95) }
                Text(title).fz(size, 800, em: 0.08).lineLimit(1)
            }
            .foregroundStyle(ghost ? FZ.text.color : .black)
            .padding(.horizontal, padding)
            .frame(maxWidth: .infinity)
            .frame(height: height)
            .background(Capsule().fill(ghost ? Color.clear : color.color))
            .overlay(Capsule().strokeBorder(ghost ? FZ.line.color : .clear, lineWidth: 1.5))
            .shadow(color: ghost ? .clear : color.opacity(0.4), radius: 11)
            .contentShape(Capsule())
        }
        .buttonStyle(PressStyle(scale: 0.96))
    }
}

/// A filter chip (.fchip): a square of its colour (a tag's chip: the tag's icon), its name; lit when on.
struct FilterChip: View {
    let title: String
    let color: RGB
    var on = false
    var poolIcon = false
    var tag: FizzTag?
    var height: CGFloat = 36
    var action: () -> Void = {}

    var body: some View {
        Button(action: action) {
            HStack(spacing: 8) {
                if poolIcon {
                    IconView(Icons.poolChip, color: on ? .black : color.color, fill: true).frame(width: 14, height: 14)
                } else if let tag {
                    TagIcon(tag, color: on ? .black : color.color, lineWidth: 2.4).frame(width: 16, height: 16)
                } else {
                    RoundedRectangle(cornerRadius: 3).fill(on ? Color.black : color.color).frame(width: 10, height: 10)
                }
                Text(title).fz(14, 800, em: 0.06)
            }
            .foregroundStyle(on ? Color.black : FZ.text.color)
            .padding(.horizontal, 14).frame(height: height)
            .background(Capsule().fill(on ? color.color : color.opacity(0.10)))
            .overlay(Capsule().strokeBorder(on ? color.color : color.mix(55, FZ.line).color, lineWidth: 1.5))
            .shadow(color: on ? color.opacity(0.45) : .clear, radius: 7)
            .contentShape(Capsule())
            .fixedSize()
        }
        .buttonStyle(PressStyle(scale: 0.94))
    }
}

/// A two-way segmented control (.seg).
struct Segmented: View {
    let items: [String]
    @Binding var selected: Int
    var color: RGB = FZ.cream

    var body: some View {
        HStack(spacing: 0) {
            ForEach(items.indices, id: \.self) { k in
                Button { selected = k } label: {
                    Text(items[k]).fz(13, 800, em: 0.07)
                        .foregroundStyle(k == selected ? Color.black : FZ.muted.color)
                        .padding(.horizontal, 15).frame(height: 38)
                        .background(k == selected ? color.color : .clear)
                }
                .buttonStyle(.plain)
            }
        }
        .clipShape(Capsule())
        .overlay(Capsule().strokeBorder(FZ.line.color, lineWidth: 1.5))
        .fixedSize()
    }
}

// MARK: - The preset row

/// One preset (.cas): its colour's stripe, slot, algorithm glyph, NAME, POOL badge, first tag.
struct PresetRow: View {
    let entry: FizzListEntry
    let algorithm: Int
    var selected = false
    var marks = true
    var nameSize: CGFloat = 16

    var body: some View {
        let tint = FizzTag.tint(mask: entry.tags)
        HStack(spacing: marks ? 10 : 8) {
            // (room for three digits: MY PRESETS 100..256 broke onto two lines)
            Text("\(entry.slot)").fz(13, 700).monospacedDigit().foregroundStyle(FZ.muted.color).lineLimit(1)
                .frame(width: marks ? 30 : 27, alignment: .trailing)
            AlgoGlyph(algorithm: algorithm, color: tint.color).frame(width: 30, height: 30)
            Text(entry.name).fz(nameSize, 800, em: 0.03).foregroundStyle(FZ.text.color).lineLimit(1)
            Spacer(minLength: 4)
            if marks {
                HStack(spacing: 6) {
                    if entry.inPool { PoolBadge() }
                    if let t = FizzTag.of(mask: entry.tags).first { TagPill(tag: t) }
                }
            }
        }
        .padding(.leading, marks ? 14 : 10).padding(.trailing, marks ? 12 : 8)
        .frame(height: 56)
        .background(RoundedRectangle(cornerRadius: 12).fill(FZ.raised.color))
        .overlay(alignment: .leading) {
            RoundedRectangle(cornerRadius: 2).fill(tint.color).frame(width: 4).padding(.vertical, 9)
        }
        .overlay(RoundedRectangle(cornerRadius: 12).strokeBorder(selected ? FZ.cyan.color : FZ.line.color, lineWidth: 1.5))
        .background(
            RoundedRectangle(cornerRadius: 12).stroke(FZ.cyan.opacity(selected ? 0.35 : 0), lineWidth: 4)
                .shadow(color: FZ.cyan.opacity(selected ? 0.3 : 0), radius: 10)
        )
        .contentShape(RoundedRectangle(cornerRadius: 12))
    }
}

// MARK: - Motion

/// The mock-ups' seeded random (the same bubbles every time).
struct Seeded {
    var s: Double
    init(_ seed: Int) { s = Double(seed) * 9301 + 49297 }
    mutating func next() -> Double {
        s = (s * 9301 + 49297).truncatingRemainder(dividingBy: 233_280)
        return s / 233_280
    }
}

/// Champagne bubbles rising (fizz-ios.html bubbles(), @keyframes rise).
struct BubbleField: View {
    struct B { let x, size, o, d, dl, w: Double; let c: RGB }
    let bubbles: [B]

    init(count: Int, seed: Int, colors: [RGB] = [FZ.champagne, FZ.champagne, FZ.champagne, FZ.cyan, FZ.pink, FZ.yellow]) {
        var r = Seeded(seed)
        var out: [B] = []
        for _ in 0..<count {
            let sz = 6 + pow(r.next(), 2) * 34, d = 7 + r.next() * 8
            let x = r.next() * 380, c = colors[Int(r.next() * Double(colors.count))]
            let o = 0.25 + r.next() * 0.5, dl = r.next() * d, w = (r.next() - 0.5) * 60
            out.append(B(x: x, size: sz, o: o, d: d, dl: dl, w: w, c: c))
        }
        bubbles = out
    }

    var body: some View {
        Animated(still: 0) { t in
            Canvas { ctx, size in
                let sx = size.width / 390, rise = size.height + 96
                for b in bubbles {
                    let p = ((t + b.dl).truncatingRemainder(dividingBy: b.d)) / b.d
                    let op = p < 0.08 ? b.o * p / 0.08 : p > 0.88 ? b.o * (1 - p) / 0.12 : b.o
                    let wob = b.w * (p < 0.5 ? p / 0.5 : (1 - p) / 0.5)
                    let y = size.height + 40 - b.size - rise * p
                    let rect = CGRect(x: b.x * sx + wob, y: y, width: b.size, height: b.size)
                    ctx.stroke(Path(ellipseIn: rect.insetBy(dx: 1, dy: 1)), with: .color(b.c.opacity(op)), lineWidth: 2)
                }
            }
        }
        .allowsHitTesting(false)
    }
}

/// Rings bursting out of a centre (fizz-ios.html burst()).
struct Burst: View {
    struct R { let x, y, size, dl: Double; let c: RGB }
    let rings: [R]

    init(color: RGB) {
        var r = Seeded(21)
        let cols = [color, FZ.champagne, FZ.cyan, FZ.pink, FZ.yellow]
        rings = (0..<22).map { i in
            let a = Double(i) / 22 * 2 * .pi + r.next() * 0.3, d = 200 + r.next() * 120, sz = 8 + r.next() * 22
            return R(x: cos(a) * d, y: sin(a) * d, size: sz, dl: r.next() * 2.6, c: cols[i % 5])
        }
    }

    var body: some View {
        Animated(still: 0.9) { t in
            Canvas { ctx, size in
                let c = CGPoint(x: size.width / 2, y: size.height / 2)
                for g in rings {
                    let p = ((t + g.dl).truncatingRemainder(dividingBy: 2.6)) / 2.6
                    let e = 1 - pow(1 - p, 3)
                    let s = 0.2 + 0.8 * e
                    let op = p < 0.7 ? 1 - 0.1 * p / 0.7 : 0.9 * (1 - p) / 0.3
                    let d = g.size * s
                    let rect = CGRect(x: c.x + g.x * e - d / 2, y: c.y + g.y * e - d / 2, width: d, height: d)
                    ctx.stroke(Path(ellipseIn: rect), with: .color(g.c.opacity(op)), lineWidth: 2.5)
                }
            }
        }
        .allowsHitTesting(false)
    }
}

/// A breathing glow: 0..1..0 over `period` seconds.
func breathe(_ t: Double, _ period: Double) -> Double { (1 - cos(2 * .pi * t / period)) / 2 }

/// A content stack that scrolls in the app and lays out flat in a snapshot. Its end clears the phone's floating bars
/// (fizzBarClearance) by `bottom`'s default; `lazy`: rows made as they scroll in (long lists).
struct ScreenScroll<Content: View>: View {
    @Environment(\.fizzSnapshot) private var snapshot
    @Environment(\.fizzBarClearance) private var clearance
    /// room after the content; nil: the floating bars' and a little more
    var bottom: CGFloat?
    var lazy = false
    var refresh: (() async -> Void)?
    @ViewBuilder let content: Content

    private var end: CGFloat { bottom ?? (clearance > 0 ? clearance + 20 : 24) }

    var body: some View {
        if snapshot {
            // (an overlay: content longer than the screen neither grows nor moves the layout)
            Color.clear.overlay(alignment: .top) {
                VStack(alignment: .leading, spacing: 0) { content }
            }
            .clipShape(OpenTop())
        } else if let refresh {
            scroll.refreshable { await refresh() }
        } else {
            scroll
        }
    }

    private var scroll: some View {
        ScrollView {
            Group {
                if lazy {
                    LazyVStack(alignment: .leading, spacing: 0) { content }
                } else {
                    VStack(alignment: .leading, spacing: 0) { content }
                }
            }
            .padding(.bottom, end)
        }
        .scrollIndicators(.hidden)
    }
}

/// A row of chips that scrolls sideways in the app and is cut at the edge in a snapshot.
struct ChipScroll<Content: View>: View {
    @Environment(\.fizzSnapshot) private var snapshot
    /// room before the first chip and after the last
    var inset: CGFloat = 16
    var spacing: CGFloat = 7
    @ViewBuilder let content: Content
    var body: some View {
        if snapshot {
            Color.clear.frame(height: 40).overlay(alignment: .leading) {
                HStack(spacing: spacing) { content }.padding(.horizontal, inset).fixedSize()
            }
            .clipped()
        } else {
            ScrollView(.horizontal) { HStack(spacing: spacing) { content }.padding(.horizontal, inset) }
                .scrollIndicators(.hidden)
        }
    }
}

/// A clip that cuts only at the bottom (glows above and beside stay).
struct OpenTop: Shape {
    func path(in r: CGRect) -> Path { Path(CGRect(x: r.minX - 60, y: r.minY - 60, width: r.width + 120, height: r.height + 60)) }
}
