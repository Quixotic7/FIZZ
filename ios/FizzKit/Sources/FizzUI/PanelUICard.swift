// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// Mock-up 57, the views: a RemoteFM1 unit's row in the unit picker (cyan: its drawing with RemoteFM1's screen, USB,
// the RemoteFM1 tag and its version, USE AS PANEL across the row, breathing; attached, PANEL with ×; over Bluetooth,
// USB only), and the panel's toast (PLAY's: #17140F, DM Sans, the linked one with a green edge), over every tab.

import SwiftUI
import FizzEngine
import FizzKit

/// A RemoteFM1 unit's card in the picker.
struct PanelPickCard: View {
    let row: PanelRow
    var enabled = true
    let use: () -> Void
    let release: () -> Void

    static let height: CGFloat = 162

    var body: some View {
        let c = FZ.cyan
        let on = row.state == .attached || row.state == .linking
        let shape = RoundedRectangle(cornerRadius: 24)
        Animated(still: 0.6, active: row.state != .bluetooth) { t in
            VStack(spacing: 10) {
                HStack(spacing: 14) {
                    FM1Drawing(color: row.state == .bluetooth ? c.mix(45, FZ.muted) : c, screen: .text("RFM1"))
                        .frame(width: 92, height: 80)
                    VStack(alignment: .leading, spacing: 6) {
                        HStack(spacing: 8) {
                            Text("FM-1").fz(22, 800, em: 0.01).foregroundStyle(FZ.text.color).lineLimit(1).fixedSize()
                            ViaChip(via: row.via)
                        }
                        HStack(spacing: 8) {
                            Text("RemoteFM1").fz(12, 800, em: 0.06).foregroundStyle(c.color)
                                .padding(.horizontal, 8).frame(height: 22)
                                .background(RoundedRectangle(cornerRadius: 6).fill(c.opacity(0.18)))
                                .fixedSize()
                            Text(row.state == .busy ? "in another host's session" : row.version ?? "")
                                .fz(14, 800, em: 0.02).foregroundStyle(FZ.muted.color)
                                .lineLimit(1).minimumScaleFactor(0.7)
                        }
                    }
                    Spacer(minLength: 0)
                }
                foot(c, t)
            }
            .padding(.top, 12).padding(.leading, 12).padding(.trailing, 14).padding(.bottom, 14)
            .frame(height: Self.height)
            .background(shape.fill(c.mix(on ? 14 : 6, FZ.panel).color))
            .overlay(shape.strokeBorder(on ? c.color : c.mix(row.state == .bluetooth ? 35 : 70, FZ.line).color, lineWidth: 2))
            .shadow(color: on ? c.opacity(0.25 + 0.3 * breathe(t, 2.6)) : .clear, radius: 10 + 10 * breathe(t, 2.6))
        }
        .opacity(enabled || on ? 1 : 0.55)
    }

    /// The row's foot: USE AS PANEL, PANEL with ×, or USB only.
    @ViewBuilder private func foot(_ c: RGB, _ t: Double) -> some View {
        switch row.state {
        case .offer, .busy:
            let b = breathe(t, 2.2)
            Button(action: use) {
                HStack(spacing: 9) {
                    IconView(TransferIcons.plug, color: .black, lineWidth: 2.4, backdrop: c.color).frame(width: 20, height: 20)
                    Text("USE AS PANEL").fz(16, 800, em: 0.06).foregroundStyle(.black)
                }
                .frame(maxWidth: .infinity).frame(height: 46)
                .background(Capsule().fill(c.color))
                .shadow(color: c.opacity(0.45 + 0.35 * b), radius: 9 + 11 * b)
                .scaleEffect(1 + 0.02 * b)
                .contentShape(Capsule())
            }
            .buttonStyle(PressStyle(scale: 0.96))
            .disabled(!enabled)
        case .attached, .linking:
            let live = row.state == .attached
            HStack(spacing: 10) {
                Circle().fill(live ? RGB(0x3BD47A).color : c.opacity(0.35 + 0.65 * breathe(t, 0.8)))
                    .frame(width: 10, height: 10)
                    .shadow(color: live ? RGB(0x3BD47A).color : .clear, radius: 5)
                Text("PANEL").fz(16, 800, em: 0.06).foregroundStyle(c.color)
                Spacer(minLength: 0)
                Button(action: release) {
                    IconView(TransferIcons.close, color: c.color, lineWidth: 3.2)
                        .frame(width: 12, height: 12)
                        .frame(width: 34, height: 34)
                        .background(Circle().fill(.black.opacity(0.82)))
                        .contentShape(Circle())
                }
                .buttonStyle(PressStyle(scale: 0.88))
                .accessibilityLabel("Let the panel go")
            }
            .padding(.leading, 18).padding(.trailing, 6)
            .frame(height: 46)
            .background(Capsule().fill(c.opacity(0.16)))
            .overlay(Capsule().strokeBorder(c.color, lineWidth: 2))
        case .bluetooth:
            HStack(spacing: 9) {
                IconView(UnitArt.usb, color: FZ.muted.color, lineWidth: 2.2).frame(width: 18, height: 18)
                Text("PANEL OVER USB ONLY").fz(14, 800, em: 0.06).foregroundStyle(FZ.muted.color)
            }
            .frame(maxWidth: .infinity).frame(height: 46)
            .overlay(Capsule().strokeBorder(FZ.line.mix(60, FZ.muted).color, style: StrokeStyle(lineWidth: 1.5, dash: [6, 5])))
        }
    }
}

/// The panel's toast: PLAY's dark pill (#17140F at .97, #3A352D edge), the linked one edged green.
public struct PanelToastView: View {
    let toast: PanelToast
    public init(toast: PanelToast) { self.toast = toast }

    public var body: some View {
        Text(toast.text).dm(13, 500).multilineTextAlignment(.center)
            .foregroundStyle(RGB(0xF3EBDC).color)
            .padding(.horizontal, 16).padding(.vertical, 10)
            .background(RoundedRectangle(cornerRadius: 14).fill(RGB(0x17140F).color.opacity(0.97)))
            .overlay(RoundedRectangle(cornerRadius: 14)
                .strokeBorder(toast.ok ? RGB(0x3BD47A).color.opacity(0.55) : RGB(0x3A352D).color, lineWidth: 1))
            .shadow(color: .black.opacity(0.45), radius: 14, y: 8)
    }
}

/// The panel's toast over the whole app (UnitPickerHost), `bottom` above the foot; it rises in and fades.
public struct PanelToastLayer: View {
    let model: UnitModel
    let bottom: CGFloat
    public init(model: UnitModel, bottom: CGFloat) { self.model = model; self.bottom = bottom }

    public var body: some View {
        ZStack {
            if let t = model.panelToast {
                PanelToastView(toast: t).id(t.id)
                    .transition(.move(edge: .bottom).combined(with: .opacity))
            }
        }
        .padding(.horizontal, 16).padding(.bottom, bottom)
        .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .bottom)
        .allowsHitTesting(false)
        .animation(.spring(response: 0.4, dampingFraction: 0.85), value: model.panelToast?.id)
    }
}
