// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// SEND between the units (mock-up 36): SEND TO FM-1 (green) / SEND TO THIS iPHONE (the theme's colour) beside
// → KEPT, its badge the place it lands; once sent the name flies up and the toast drops in (the unit, MY PRESETS n,
// the tick), then rises away. The sheet stays open.

import SwiftUI
import FizzKit

/// SEND TO <the other unit>: its glyph, the words, the landing place as a badge.
struct SendOtherButton: View {
    let other: OtherUnit
    var enabled = true
    var compact = false
    let action: () -> Void
    @AppStorage("play.theme") private var theme = PlayTheme.tangerine.rawValue

    var body: some View {
        let c = unitTint(other.key, theme: theme)
        Button(action: action) {
            Animated(still: 0.6, active: enabled) { t in
                HStack(spacing: 9) {
                    IconView(other.isEmulated ? KeptArt.phone : UnitArt.fm1, color: .black, lineWidth: 2.2, backdrop: c.color)
                        .frame(width: 20, height: 20)
                    Text("SEND TO \(other.name)").fz(compact ? 14 : 16, 800, em: 0.06).lineLimit(1).minimumScaleFactor(0.6)
                    if let k = other.freeSlot {
                        Text("\(k)").fz(13, 800).foregroundStyle(c.color).monospacedDigit()
                            .padding(.horizontal, 7).frame(minWidth: 26).frame(height: 24)
                            .background(Capsule().fill(.black))
                            .contentTransition(.numericText())
                    }
                }
                .foregroundStyle(.black)
                .padding(.horizontal, 12)
                .frame(maxWidth: .infinity).frame(height: 52)
                .background(Capsule().fill(c.color))
                .shadow(color: c.opacity(enabled ? 0.35 + 0.25 * breathe(t, 2.4) : 0), radius: 10 + 8 * breathe(t, 2.4))
                .contentShape(Capsule())
            }
        }
        .buttonStyle(PressStyle(scale: 0.96))
        .disabled(!enabled)
        .opacity(enabled ? 1 : 0.4)
        .animation(.spring(response: 0.3, dampingFraction: 0.8), value: other.freeSlot)
    }
}

/// A preset that landed on the other unit (the toast's).
struct Landed: Equatable, Identifiable {
    let id = UUID()
    let name: String
    let to: OtherUnit
    let slot: Int
    static func == (a: Landed, b: Landed) -> Bool { a.id == b.id }
}

/// The toast: the unit drawn, the name, <unit> · MY PRESETS n, the tick; it rises away by itself.
struct LandedToast: View {
    @Binding var landed: Landed?
    @AppStorage("play.theme") private var theme = PlayTheme.tangerine.rawValue

    var body: some View {
        if let l = landed {
            let c = unitTint(l.to.key, theme: theme)
            HStack(spacing: 14) {
                Group {
                    if l.to.isEmulated { PhoneArt(color: c) } else { FM1Drawing(color: c, screen: .bubbles) }
                }
                .frame(width: 70, height: 48)
                VStack(alignment: .leading, spacing: 4) {
                    Text(l.name).fz(20, 800, em: 0.02).foregroundStyle(FZ.text.color).lineLimit(1).minimumScaleFactor(0.7)
                    HStack(spacing: 7) {
                        Text("\(l.to.name) · MY PRESETS").fz(13, 800, em: 0.06).foregroundStyle(c.color).lineLimit(1)
                            .minimumScaleFactor(0.7)
                        Text("\(l.slot)").fz(13, 800).foregroundStyle(.black)
                            .padding(.horizontal, 6).frame(minWidth: 24).frame(height: 22)
                            .background(RoundedRectangle(cornerRadius: 6).fill(c.color))
                    }
                }
                Spacer(minLength: 4)
                IconView(KeptArt.check, color: .black, lineWidth: 3, backdrop: c.color).frame(width: 18, height: 18)
                    .frame(width: 38, height: 38)
                    .background(Circle().fill(c.color))
            }
            .padding(.leading, 12).padding(.trailing, 14)
            .frame(height: 74)
            .background(RoundedRectangle(cornerRadius: 24).fill(Color.black))
            .background(RoundedRectangle(cornerRadius: 24).fill(c.opacity(0.14)))
            .overlay(RoundedRectangle(cornerRadius: 24).strokeBorder(c.color, lineWidth: 2))
            .shadow(color: c.opacity(0.4), radius: 16)
            .contentShape(RoundedRectangle(cornerRadius: 24))
            .onTapGesture { withAnimation { landed = nil } }
            .task(id: l.id) {
                try? await Task.sleep(for: .seconds(2.4))
                if landed?.id == l.id { withAnimation(.easeIn(duration: 0.35)) { landed = nil } }
            }
            .transition(.move(edge: .top).combined(with: .opacity))
        }
    }
}

/// The name flying up out of the sheet to the toast.
struct FlyName: View {
    let name: String
    let color: RGB
    @State private var up = false

    var body: some View {
        Text(name).fz(14, 800, em: 0.06).foregroundStyle(color.color)
            .padding(.horizontal, 10).frame(height: 28)
            .background(Capsule().fill(.black))
            .overlay(Capsule().strokeBorder(color.color, lineWidth: 1.5))
            .shadow(color: color.opacity(0.6), radius: 8)
            .scaleEffect(up ? 0.7 : 1)
            .offset(y: up ? -360 : 0)
            .opacity(up ? 0 : 1)
            .allowsHitTesting(false)
            .onAppear { withAnimation(.easeIn(duration: 0.75)) { up = true } }
    }
}

/// A preset's sheet just after SEND TO the other unit (fizzshots): the toast over the sheet's top, as the app shows it
/// (a system sheet: nothing is drawn above it).
public struct SendToastShot: View {
    let model: UnitModel
    let key: String
    public init(model: UnitModel, key: String) { self.model = model; self.key = key }

    public var body: some View {
        ZStack(alignment: .bottom) {
            RootView(model: model, tab: .constant(.library), wide: false)
                .blur(radius: 2).brightness(-0.3)
            Color.black.opacity(0.45)
            if let r = model.rows.first(where: { "\($0.source):\($0.slot)" == key }) {
                let l = model.other.map { Landed(name: r.name, to: $0, slot: max(1, ($0.freeSlot ?? 2) - 1)) }
                PresetPanel(model: model, entry: r, note: .constant(nil), ask: .constant(nil), onMove: { _ in }, shotLanded: l)
                    .padding(.horizontal, 18).padding(.top, 30).padding(.bottom, 44)
                    .background(SheetShape().fill(sheetBackground.color).shadow(color: .black, radius: 30, y: -10))
                    .overlay(alignment: .top) { Capsule().fill(RGB(0x3a3a44).color).frame(width: 40, height: 5).padding(.top, 10) }
            }
        }
        .environment(\.fizzShotSelection, key)
    }
}

/// A glyph-only button of the folded row (EXPORT, MOVE, DELETE).
struct GlyphPill: View {
    let icon: VectorArt
    var color: RGB = FZ.text
    var ghost = false
    var enabled = true
    let action: () -> Void

    var body: some View {
        Button(action: action) {
            IconView(icon, color: ghost ? FZ.text.color : color.color, lineWidth: 2.3).frame(width: 20, height: 20)
                .frame(width: 52, height: 52)
                .background(Circle().fill(ghost ? Color.clear : color.opacity(0.06)))
                .overlay(Circle().strokeBorder(ghost ? FZ.line.color : color.mix(70, FZ.line).color, lineWidth: 1.5))
                .contentShape(Circle())
        }
        .buttonStyle(PressStyle(scale: 0.92))
        .disabled(!enabled)
        .opacity(enabled ? 1 : 0.4)
    }
}
