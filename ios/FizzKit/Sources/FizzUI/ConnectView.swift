// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// Finding the FM-1 (mock-ups 01..05): looking (bubbles rise behind the FM-1, a light runs down the cable), found
// (the burst, the green card), update mode (orange), not FIZZ (red, its id), two units (two cards to tap).

import SwiftUI
import FizzKit

public struct ConnectView: View {
    let model: UnitModel
    @Environment(\.fizzInsets) private var insets
    @State private var bluetooth = false

    public init(model: UnitModel) { self.model = model }

    public var body: some View {
        ZStack(alignment: .top) {
            Color.black
            background
            VStack(spacing: 0) {
                TopBar(model: model, unit: model.state.kind == "found")
                content.frame(maxWidth: .infinity, maxHeight: .infinity)
            }
            .padding(insets)
        }
        .sheet(isPresented: $bluetooth) { BluetoothSheet() }
    }

    @ViewBuilder private var background: some View {
        switch model.state {
        case .found: BubbleField(count: 14, seed: 5)
        case .updateMode: BubbleField(count: 10, seed: 7, colors: [FZ.orange, FZ.champagne])
        case .notFizz: BubbleField(count: 8, seed: 9, colors: [FZ.red, FZ.champagne])
        case .choose: BubbleField(count: 14, seed: 11)
        case .searching, .lost: BubbleField(count: 30, seed: 3)
        }
    }

    @ViewBuilder private var content: some View {
        switch model.state {
        case .searching: Searching(lost: false) { bluetooth = true }
        case .lost: Searching(lost: true) { bluetooth = true }
        case .found(let u): FoundCard(info: u)
        case .updateMode: UpdateCard()
        case .notFizz(let id): NotFizzCard(id: id).contentShape(Rectangle()).onTapGesture { model.rescan() }
        case .choose: ChooseUnits(model: model)
        }
    }
}

// MARK: - Looking

/// The FM-1, its cable, the adapter and the phone (fizz-ios.html .rig, a 390 x 600 box), and the pill.
struct Searching: View {
    let lost: Bool
    let onBluetooth: () -> Void

    var body: some View {
        VStack(spacing: 0) {
            Rig()
                .aspectRatio(390 / 600, contentMode: .fit)
                .frame(maxWidth: 390)
                .padding(.top, 8)
            Spacer(minLength: 0)
            HStack(spacing: 14) {
                LookingPill(lost: lost)
                Button(action: onBluetooth) {
                    IconView(Icons.bluetooth, color: FZ.blue.color, lineWidth: 2.6)
                        .frame(width: 26, height: 26)
                        .frame(width: 60, height: 60)
                        .background(Circle().fill(FZ.blue.mix(14, FZ.bg).color))
                        .overlay(Circle().strokeBorder(FZ.blue.color, lineWidth: 2))
                }
                .buttonStyle(.plain)
            }
            .padding(.bottom, 30)
        }
    }
}

struct LookingPill: View {
    let lost: Bool
    var body: some View {
        let c = lost ? FZ.red : FZ.yellow
        Animated(still: 0.5) { t in
            let p = (t.truncatingRemainder(dividingBy: 1.6)) / 1.6
            let tri = p < 0.5 ? p * 2 : 2 - 2 * p
            HStack(spacing: 12) {
                Circle().fill(c.color).frame(width: 12, height: 12)
                    .shadow(color: c.color, radius: 5)
                    .opacity(1 - 0.85 * tri)
                Text(lost ? "UNPLUGGED" : "LOOKING FOR FM-1").fz(16, 800, em: 0.1).foregroundStyle(FZ.text.color)
                    .lineLimit(1)
            }
            .padding(.horizontal, 28)
            .frame(height: 60)
            .background(Capsule().fill(c.mix(14, FZ.bg).color))
            .overlay(Capsule().strokeBorder(c.color, lineWidth: 2))
            .background(
                Capsule().stroke(c.opacity(0.6 * (1 - p)), lineWidth: 3)
                    .padding(-22 * p)
            )
            .fixedSize()
        }
    }
}

/// The searching drawing, in the mock-up's 390 x 600 coordinates.
struct Rig: View {
    static let cable = "M340 70 C 384 80, 384 250, 300 296 S 195 330, 195 392"

    var body: some View {
        GeometryReader { g in
            let s = g.size.width / 390
            ZStack(alignment: .topLeading) {
                FM1Drawing(color: FZ.cream, screen: .bubbles)
                    .frame(width: 318 * s, height: 318 * s / FM1Drawing.aspect)
                    .offset(x: 24 * s, y: 14 * s)
                Animated(still: 0.55) { t in
                    Canvas { ctx, _ in draw(ctx, s, t) }
                }
            }
        }
    }

    private func draw(_ ctx0: GraphicsContext, _ s: CGFloat, _ t: Double) {
        var ctx = ctx0
        ctx.scaleBy(x: s, y: s)
        let cable = SVGPath.parse(Self.cable)
        ctx.stroke(cable, with: .color(RGB(0x2e2e36).color), style: StrokeStyle(lineWidth: 7, lineCap: .round))
        // the light running down the cable
        let p = (t.truncatingRemainder(dividingBy: 1.8)) / 1.8
        var glow = ctx
        glow.addFilter(.shadow(color: FZ.cyan.color, radius: 6))
        glow.stroke(cable, with: .color(FZ.cyan.color),
                    style: StrokeStyle(lineWidth: 7, lineCap: .round, dash: [46, 560], dashPhase: 606 * (1 - p)))
        // the adapter (glowing), its plug, the phone with the logo
        let pulse = breathe(t, 1.8)
        var ad = ctx
        ad.addFilter(.shadow(color: FZ.cyan.opacity(pulse), radius: 6))
        let adp = Path(roundedRect: CGRect(x: 179, y: 392, width: 32, height: 46), cornerRadius: 8)
        ad.fill(adp, with: .color(RGB(0x121217).color))
        ad.stroke(adp, with: .color(FZ.cyan.color), lineWidth: 2.4)
        ctx.stroke(SVGPath.parse("M188 404h14M188 412h14"), with: .color(FZ.cyan.color),
                   style: StrokeStyle(lineWidth: 2.4, lineCap: .round))
        ctx.fill(Path(roundedRect: CGRect(x: 188, y: 438, width: 14, height: 10), cornerRadius: 2),
                 with: .color(RGB(0x2e2e36).color))
        let ph = Path(roundedRect: CGRect(x: 142, y: 448, width: 106, height: 148), cornerRadius: 22)
        ctx.fill(ph, with: .color(RGB(0x050507).color))
        ctx.stroke(ph, with: .color(FZ.muted.color), lineWidth: 2.4)
        ctx.fill(Path(roundedRect: CGRect(x: 178, y: 456, width: 34, height: 9), cornerRadius: 4.5),
                 with: .color(RGB(0x1c1c22).color))
        for (x, y, r, c) in Icons.bubbles {
            let k: CGFloat = 1.2
            let rect = CGRect(x: 176 + (x - r) * k, y: 504 + (y - r) * k, width: 2 * r * k, height: 2 * r * k)
            ctx.stroke(Path(ellipseIn: rect), with: .color(c.color), lineWidth: 2.4 * k)
        }
    }
}

// MARK: - Found

/// The card a found unit lands in (.found): ring, drawing, title lines.
struct UnitCard<Title: View>: View {
    let color: RGB
    let screen: FM1Screen
    var ripple = false
    var flicker = false
    @ViewBuilder let title: Title
    @Environment(\.fizzSnapshot) private var snapshot
    @State private var popped = false

    var body: some View {
        VStack(spacing: 0) {
            ZStack {
                Animated(still: 0.6) { t in
                    let p = (t.truncatingRemainder(dividingBy: 2.2)) / 2.2
                    ZStack {
                        Circle().strokeBorder(color.color, lineWidth: 3)
                            .shadow(color: color.opacity(0.5), radius: 12)
                            .frame(width: 270, height: 270)
                        if ripple {
                            Circle().strokeBorder(color.color, lineWidth: 3)
                                .frame(width: 270, height: 270)
                                .scaleEffect(0.85 + 0.5 * p)
                                .opacity(0.9 * (1 - p))
                        }
                    }
                }
                Animated(still: 0.5) { t in
                    let ph = (t.truncatingRemainder(dividingBy: 1.4)) / 1.4
                    let f = (t.truncatingRemainder(dividingBy: 2.4)) / 2.4
                    let op = !flicker ? 1 : (f >= 0.62 && f < 0.66) ? 0.35 : (f >= 0.70 && f < 0.74) ? 0.5 : 1
                    FM1Drawing(color: color, screen: screen == .update(0) ? .update(ph) : screen)
                        .frame(width: 260)
                        .opacity(op)
                }
            }
            .frame(width: 300, height: 230)
            title
        }
        .padding(.top, 34).padding(.bottom, 30).padding(.horizontal, 20)
        .frame(maxWidth: 350)
        .frame(maxWidth: .infinity)
        .background(RoundedRectangle(cornerRadius: 30).fill(color.mix(7, FZ.panel).color))
        .overlay(RoundedRectangle(cornerRadius: 30).strokeBorder(color.color, lineWidth: 2))
        .shadow(color: color.opacity(0.3), radius: 25)
        .scaleEffect(popped || snapshot ? 1 : 0.6)
        .opacity(popped || snapshot ? 1 : 0)
        .onAppear { withAnimation(.spring(response: 0.6, dampingFraction: 0.55)) { popped = true } }
    }
}

struct FoundCard: View {
    let info: UnitInfo
    var body: some View {
        VStack {
            UnitCard(color: FZ.green, screen: .bubbles, ripple: true) {
                FizzWord(size: 76).padding(.top, 22)
                Text(info.shortVersion ?? info.idText).fz(26, 800, em: 0.01).foregroundStyle(FZ.text.color)
                    .padding(.top, 8)
                Chip(text: info.via, color: FZ.green).padding(.top, 18)
            }
            .overlay { Burst(color: FZ.green).frame(width: 760, height: 760) }
            .padding(.horizontal, 20)
            .padding(.top, 38)
            Spacer(minLength: 0)
        }
    }
}

struct UpdateCard: View {
    var body: some View {
        VStack {
            UnitCard(color: FZ.orange, screen: .update(0)) {
                Text("UPDATE MODE").fz(40, 800, em: 0.03).foregroundStyle(FZ.orange.color).lineLimit(1)
                    .minimumScaleFactor(0.6).padding(.top, 22)
                Text("FM-1 LOADER").fz(18, 800).foregroundStyle(FZ.muted.color).padding(.top, 8)
            }
            .padding(.horizontal, 20).padding(.top, 38)
            Spacer(minLength: 0)
        }
    }
}

struct NotFizzCard: View {
    let id: String
    @Environment(\.fizzSnapshot) private var snapshot
    @State private var shook: CGFloat = 0

    var body: some View {
        VStack {
            UnitCard(color: FZ.red, screen: .question, flicker: true) {
                Text(id).fz(52, 800).foregroundStyle(FZ.red.color).lineLimit(1).minimumScaleFactor(0.5)
                    .padding(.top, 20)
                    .modifier(Shake(amount: shook))
                HStack(spacing: 10) {
                    Chip(text: "NOT FIZZ", color: FZ.red)
                    if let f = FirmwareName.of(id), f != "FIZZ" { Chip(text: f, color: FZ.champagne) }
                }
                .padding(.top, 18)
            }
            .padding(.horizontal, 20).padding(.top, 38)
            Spacer(minLength: 0)
        }
        .onAppear {
            guard !snapshot else { return }
            withAnimation(.linear(duration: 0.6).delay(0.5)) { shook = 1 }
        }
    }
}

/// A shake that dies away as `amount` goes 0 -> 1.
struct Shake: GeometryEffect {
    var amount: CGFloat
    var animatableData: CGFloat {
        get { amount }
        set { amount = newValue }
    }
    func effectValue(size: CGSize) -> ProjectionTransform {
        guard amount > 0, amount < 1 else { return ProjectionTransform(.identity) }
        return ProjectionTransform(CGAffineTransform(translationX: 10 * sin(amount * .pi * 5) * (1 - amount), y: 0))
    }
}

/// A small outlined chip with a dot (.found .chip).
struct Chip: View {
    let text: String
    let color: RGB
    var body: some View {
        HStack(spacing: 9) {
            Circle().fill(color.color).frame(width: 9, height: 9).shadow(color: color.color, radius: 4)
            Text(text).fz(14, 800, em: 0.1).foregroundStyle(color.color)
        }
        .padding(.horizontal, 16).frame(height: 36)
        .overlay(Capsule().strokeBorder(color.color, lineWidth: 1.5))
        .fixedSize()
    }
}

// MARK: - Two units

struct ChooseUnits: View {
    let model: UnitModel

    var body: some View {
        VStack(spacing: 0) {
            Text("\(model.choices.count) × FM-1").fz(44, 800).foregroundStyle(FZ.text.color)
                .padding(.top, 14)
            HStack(spacing: 12) {
                ForEach(model.choices) { c in
                    UnitChoiceCard(choice: c) { model.choose(c.id) }
                }
            }
            .padding(.horizontal, 16)
            .padding(.top, 22)
            .frame(maxWidth: 560)
            Spacer(minLength: 0)
        }
    }
}

struct UnitChoiceCard: View {
    let choice: UnitChoice
    let action: () -> Void

    var body: some View {
        let c = choice.isFizz ? FZ.green : FZ.champagne
        Animated(still: 0) { t in
            Button(action: action) {
                VStack(spacing: 14) {
                    FM1Drawing(color: c, screen: choice.isFizz ? .bubbles : .text(choice.name.lowercased().contains("choralroot") ? "CR" : "?"))
                        .frame(width: 150)
                    Circle().fill(c.color).frame(width: 12, height: 12).shadow(color: c.color, radius: 5)
                    VStack(spacing: 8) {
                        Text(choice.name).fz(21, 800).foregroundStyle(FZ.text.color).multilineTextAlignment(.center)
                            .lineLimit(2).minimumScaleFactor(0.7)
                        Text(choice.detail).fz(15, 800, em: 0.06).foregroundStyle(c.color).lineLimit(1)
                            .minimumScaleFactor(0.6)
                    }
                    .padding(.top, 8)
                    Spacer(minLength: 0)
                    IconView(Icons.tap, color: .black, lineWidth: 3).frame(width: 32, height: 32)
                        .frame(width: 74, height: 74)
                        .background(Circle().fill(c.color))
                        .shadow(color: c.opacity(0.55), radius: 13)
                }
                .padding(.top, 26).padding(.bottom, 22).padding(.horizontal, 10)
                .frame(maxWidth: .infinity)
                .frame(height: 470)
                .background(RoundedRectangle(cornerRadius: 26).fill(c.mix(8, FZ.panel).color))
                .overlay(RoundedRectangle(cornerRadius: 26).strokeBorder(c.color, lineWidth: 2))
                .shadow(color: c.opacity(0.28), radius: 17)
                .offset(y: -8 * breathe(t + Double(choice.id) * 1.8, 3.6))
            }
            .buttonStyle(.plain)
        }
    }
}
