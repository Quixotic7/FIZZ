// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// The unit picker (mock-ups 34, 35): the app's dark sheet, one big card per unit. THIS iPHONE in the emulator theme's
// colour, an FM-1 green with its USB / BLUETOOTH chip; the one in use ringed, glowing, ticked, a pin when picked by
// hand; one that does not answer greyed, dashed, flickering. A tap switches the whole manager (the card ripples);
// + BLUETOOTH, pinned at the bottom, opens iOS's Bluetooth MIDI pairing. A RemoteFM1 unit gets its own cyan card,
// USE AS PANEL (mock-up 57, PanelUI*.swift), after the units answering.

import SwiftUI
import FizzKit

/// The picker's sheet over the app (FizzAppView), opened by any unit pill.
struct UnitPickerHost: ViewModifier {
    @Bindable var model: UnitModel

    func body(content: Content) -> some View {
        content
            // (the panel's toasts, on every tab: above PLAY's page dots, or above the tab bar)
            .overlay { PanelToastLayer(model: model, bottom: model.emulator?.playShown == true ? 30 : 84) }
            .sheet(isPresented: $model.pickerOpen, onDismiss: { model.pickerClosed() }) {
            UnitPicker(model: model)
                .presentationDetents([.height(UnitPicker.height(model))])
                .presentationDragIndicator(.visible)
                .presentationBackground(sheetBackground.color)
                .presentationCornerRadius(30)
                .preferredColorScheme(.dark)
        }
    }
}

extension View {
    /// The unit picker's sheet (once, round the whole app).
    func unitPicker(_ model: UnitModel) -> some View { modifier(UnitPickerHost(model: model)) }
}

/// The colour a unit is drawn in: THIS iPHONE the emulator theme's, an FM-1 green.
func unitTint(_ key: UnitKey, theme: String) -> RGB {
    key == .here ? (PlayTheme(rawValue: theme) ?? .tangerine).accent : FZ.green
}

struct UnitPicker: View {
    let model: UnitModel
    @Environment(\.dismiss) private var dismiss

    static let cardHeight: CGFloat = 100
    /// the sheet's height for so many cards (the heading, the cards, + BLUETOOTH)
    static func height(rows: Int, panels: Int = 0) -> CGFloat {
        30 + 34 + CGFloat(rows) * (cardHeight + 12) + CGFloat(panels) * (PanelPickCard.height + 12) + 8 + 60 + 34
    }
    static func height(_ model: UnitModel) -> CGFloat { height(rows: model.unitRows.count, panels: model.panelRows.count) }

    var body: some View {
        let rows = model.unitRows, panels = model.panelRows
        VStack(alignment: .leading, spacing: 0) {
            Heading(text: "UNIT", count: "\(rows.count + panels.count)").padding(.bottom, 14).padding(.leading, 4)
            // (a few cards: the sheet is made as high as they need; the RemoteFM1 units after the units answering)
            VStack(spacing: 12) {
                ForEach(rows.filter(\.answering)) { r in card(r) }
                ForEach(panels) { p in
                    PanelPickCard(row: p, enabled: model.switching == nil, use: { usePanel(p) },
                                  release: { model.stopPanel() })
                }
                ForEach(rows.filter { !$0.answering }) { r in card(r) }
            }
            Spacer(minLength: 8)
            BluetoothPill { BluetoothPairing.present() }
        }
        .padding(.horizontal, 18).padding(.top, 30).padding(.bottom, 12)
        .animation(.spring(response: 0.35, dampingFraction: 0.8), value: rows)
        .animation(.spring(response: 0.35, dampingFraction: 0.8), value: panels)
    }

    private func card(_ r: UnitRow) -> some View {
        UnitPickCard(row: r, track: track(r), switching: model.switching == r.key,
                     enabled: r.inUse || (model.idle && model.switching == nil)) { tap(r) }
    }

    /// USE AS PANEL: claimed, then the sheet goes (PLAY's chip and the toast say the rest).
    private func usePanel(_ p: PanelRow) {
        Task {
            guard await model.usePanelTapped(p.name) else { return }
            try? await Task.sleep(for: .milliseconds(420))
            dismiss()
        }
    }

    /// PLAY's keys on the FM-1's track: the track shown on its card.
    private func track(_ r: UnitRow) -> Int? {
        guard model.pickerFromPlay, r.inUse, r.key != .here, model.emulator?.play.soundNow == .fm1,
              model.editor.trackShown >= 0 else { return nil }
        return model.editor.trackShown + 1
    }

    private func tap(_ r: UnitRow) {
        if !r.answering {
            // (a paired Bluetooth FM-1 asleep: iOS's pairing panel connects it)
            if r.via == .bluetooth { BluetoothPairing.present() }
            return
        }
        Task {
            guard await model.pick(r.key) else { return }
            try? await Task.sleep(for: .milliseconds(420))      // (the tick lands, then the sheet goes)
            dismiss()
        }
    }
}

/// One unit's card.
struct UnitPickCard: View {
    let row: UnitRow
    var track: Int?
    var switching = false
    var enabled = true
    let action: () -> Void
    @AppStorage("play.theme") private var theme = PlayTheme.tangerine.rawValue

    var body: some View {
        let c = row.answering ? unitTint(row.key, theme: theme) : FZ.muted
        let shape = RoundedRectangle(cornerRadius: 22)
        Button(action: action) {
            Animated(still: 0.6, active: row.inUse || switching || !row.answering) { t in
                let glow = row.inUse ? breathe(t, 2.6) : 0
                let flick = flicker(t)
                HStack(spacing: 12) {
                    art(c).frame(width: 86, height: 70).opacity(row.answering ? 1 : 0.45 * flick)
                    VStack(alignment: .leading, spacing: 5) {
                        HStack(spacing: 8) {
                            Text(row.title).fz(21, 800, em: 0.01).foregroundStyle(FZ.text.color.opacity(row.answering ? 1 : 0.5))
                                .lineLimit(1).fixedSize()
                            if row.via != .here { ViaChip(via: row.via, dim: !row.answering) }
                            if row.pinned {
                                IconView(UnitArt.pin, color: c.color, lineWidth: 2.2).frame(width: 16, height: 16)
                            }
                        }
                        HStack(spacing: 8) {
                            Text(row.version ?? "—").fz(13, 700, em: 0.02).foregroundStyle(FZ.muted.color.opacity(row.answering ? 1 : 0.6))
                                .lineLimit(1).minimumScaleFactor(0.7)
                            if let track {
                                Text("\(track)").fz(13, 800).foregroundStyle(.white)
                                    .frame(width: 24, height: 24).background(Circle().fill(FZ.pink.color))
                                    .shadow(color: FZ.pink.opacity(0.6), radius: 6)
                            }
                        }
                    }
                    Spacer(minLength: 4)
                    tick(c)
                }
                .padding(.leading, 12).padding(.trailing, 16)
                .frame(height: UnitPicker.cardHeight)
                .background(shape.fill(row.inUse ? c.mix(14, FZ.panel).color : row.answering ? c.mix(5, FZ.panel).color : FZ.panel.color))
                .overlay {
                    if row.answering {
                        shape.strokeBorder(row.inUse ? c.color : c.mix(55, FZ.line).color, lineWidth: 2)
                    } else {
                        shape.strokeBorder(FZ.line.mix(60, FZ.muted).color, style: StrokeStyle(lineWidth: 1.5, dash: [6, 5]))
                    }
                }
                .shadow(color: row.inUse ? c.opacity(0.25 + 0.3 * glow) : .clear, radius: 10 + 10 * glow)
                .background {
                    // the tap's ripple while the manager switches
                    if switching {
                        let p = (t.truncatingRemainder(dividingBy: 1.1)) / 1.1
                        shape.stroke(c.opacity(0.8 * (1 - p)), lineWidth: 3).padding(-10 * p)
                    }
                }
                .contentShape(shape)
            }
        }
        .buttonStyle(PressStyle(scale: 0.97))
        .disabled(!enabled)
        .opacity(enabled || !row.answering ? 1 : 0.55)
    }

    /// A paired unit asleep: a flicker now and then.
    private func flicker(_ t: Double) -> Double {
        let f = (t.truncatingRemainder(dividingBy: 2.4)) / 2.4
        return (f >= 0.62 && f < 0.66) ? 0.5 : (f >= 0.70 && f < 0.74) ? 0.7 : 1
    }

    @ViewBuilder private func art(_ c: RGB) -> some View {
        if row.via == .here {
            PhoneArt(color: c)
        } else {
            FM1Drawing(color: c, screen: .bubbles).frame(width: 86)
        }
    }

    @ViewBuilder private func tick(_ c: RGB) -> some View {
        if row.inUse {
            IconView(KeptArt.check, color: .black, lineWidth: 3, backdrop: c.color).frame(width: 18, height: 18)
                .frame(width: 34, height: 34)
                .background(Circle().fill(c.color))
                .shadow(color: c.opacity(0.6), radius: 8)
        } else {
            Circle().strokeBorder(FZ.muted.opacity(row.answering ? 0.8 : 0.35), lineWidth: 2).frame(width: 30, height: 30)
                .frame(width: 34, height: 34)
        }
    }
}

/// USB (green, the plug) or BLUETOOTH (blue, the rune), filled.
struct ViaChip: View {
    let via: UnitRow.Via
    var dim = false
    var body: some View {
        let usb = via == .usb
        let c = usb ? FZ.green : FZ.blue
        HStack(spacing: 5) {
            IconView(usb ? UnitArt.usb : Icons.bluetooth, color: usb ? .black : .white, lineWidth: 2.2,
                     backdrop: c.color).frame(width: 13, height: 13)
            Text(usb ? "USB" : "BLUETOOTH").fz(12, 800, em: 0.03).foregroundStyle(usb ? Color.black : .white)
        }
        .padding(.horizontal, 7).frame(height: 24)
        .background(RoundedRectangle(cornerRadius: 7).fill(c.color))
        .opacity(dim ? 0.55 : 1)
        .fixedSize()
    }
}

/// + BLUETOOTH: iOS's Bluetooth MIDI pairing.
struct BluetoothPill: View {
    let action: () -> Void
    var body: some View {
        Button(action: action) {
            HStack(spacing: 12) {
                IconView(Icons.bluetooth, color: .black, lineWidth: 2.6, backdrop: FZ.blue.color).frame(width: 20, height: 20)
                Text("+ BLUETOOTH").fz(17, 800, em: 0.08).foregroundStyle(.black)
            }
            .frame(maxWidth: .infinity).frame(height: 58)
            .background(Capsule().fill(FZ.blue.color))
            .shadow(color: FZ.blue.opacity(0.45), radius: 12)
            .contentShape(Capsule())
        }
        .buttonStyle(PressStyle(scale: 0.96))
    }
}

/// THIS iPHONE drawn: the phone with FIZZ's panel on its screen (the bubbles, the buttons, the keys).
struct PhoneArt: View {
    let color: RGB
    var body: some View {
        Canvas { ctx, size in
            let h = min(size.height, size.width * 1.75), w = h / 1.75
            let r = CGRect(x: (size.width - w) / 2, y: (size.height - h) / 2, width: w, height: h)
            let c = color.color
            let body = Path(roundedRect: r.insetBy(dx: 1.2, dy: 1.2), cornerRadius: w * 0.2)
            ctx.fill(body, with: .color(color.mix(10, RGB(0x050507)).color))
            ctx.stroke(body, with: .color(c), lineWidth: 2.4)
            let s = w / 40
            // the screen with the bubbles
            let scr = CGRect(x: r.minX + 24 * s, y: r.minY + 6 * s, width: 11 * s, height: 11 * s)
            ctx.fill(Path(roundedRect: scr, cornerRadius: 2 * s), with: .color(.black))
            ctx.stroke(Path(roundedRect: scr, cornerRadius: 2 * s), with: .color(c), lineWidth: 1.2)
            for (x, y, rr, col) in [(28.0, 10.0, 2.2, FZ.cyan), (31.5, 13.0, 1.6, FZ.pink), (27.5, 14.5, 1.3, FZ.yellow)] {
                ctx.stroke(Path(ellipseIn: CGRect(x: r.minX + (x - rr) * s, y: r.minY + (y - rr) * s, width: 2 * rr * s,
                                                  height: 2 * rr * s)), with: .color(col.color), lineWidth: 1)
            }
            // the knobs, the buttons' well, the keys
            for k in 0..<3 {
                let kr = CGRect(x: r.minX + (5 + CGFloat(k) * 6) * s, y: r.minY + 7 * s, width: 4.5 * s, height: 4.5 * s)
                ctx.fill(Path(ellipseIn: kr), with: .color(c))
            }
            for row in 0..<3 {
                for col in 0..<4 {
                    let b = CGRect(x: r.minX + (5 + CGFloat(col) * 7.8) * s, y: r.minY + (22 + CGFloat(row) * 7) * s,
                                   width: 6 * s, height: 5 * s)
                    ctx.stroke(Path(roundedRect: b, cornerRadius: 1.2 * s), with: .color(c), lineWidth: 1.1)
                }
            }
            let keys = CGRect(x: r.minX + 5 * s, y: r.minY + 46 * s, width: 30 * s, height: 20 * s)
            ctx.fill(Path(roundedRect: keys, cornerRadius: 2 * s), with: .color(c.opacity(0.85)))
            for k in 1..<6 {
                let x = keys.minX + CGFloat(k) * keys.width / 6
                ctx.stroke(Path { p in p.move(to: CGPoint(x: x, y: keys.minY)); p.addLine(to: CGPoint(x: x, y: keys.maxY)) },
                           with: .color(.black), lineWidth: 1.2)
            }
        }
        .aspectRatio(40 / 70, contentMode: .fit)
    }
}

/// The picker's and SEND's glyphs.
enum UnitArt {
    static let pin = VectorArt(24, 24, [.path("M9 3h6l-1 6 3.5 3.5V14h-11v-1.5L10 9z"), .path("M12 14v7")])
    static let usb = VectorArt(24, 24, [.path("M12 21V4M12 4l-2.5 3h5zM12 16l-4.5-3.5V10M12 13.5l4.5-3V8"),
                                       .circle(12, 20, 1.6), .rect(6, 8, 3, 2.5, 0.5), .circle(16.5, 7, 1.5)])
    static let fm1 = VectorArt(24, 24, [.rect(2.5, 6, 19, 12.5, 2.5), .rect(9.5, 8, 5, 4.5, 1), .path("M5 15.5h14M5 9h2M17 9h2")])
}

/// The picker over a screen as the phone shows it (fizzshots: ImageRenderer draws no sheets).
public struct UnitPickerShot: View {
    let model: UnitModel
    let tab: FizzTab
    public init(model: UnitModel, tab: FizzTab = .library) { self.model = model; self.tab = tab }

    public var body: some View {
        ZStack(alignment: .bottom) {
            RootView(model: model, tab: .constant(tab), wide: false)
                .blur(radius: 2).brightness(-0.3)
            Color.black.opacity(0.45)
            UnitPicker(model: model)
                .frame(height: UnitPicker.height(model))
                .padding(.bottom, 20)
                .background(SheetShape().fill(sheetBackground.color).shadow(color: .black, radius: 30, y: -10))
                .overlay(alignment: .top) { Capsule().fill(RGB(0x3a3a44).color).frame(width: 40, height: 5).padding(.top, 10) }
        }
    }
}
