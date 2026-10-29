// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// PLAY's parts, drawn as RemoteFM1's Knob, Buttons, Keys and Screen (mock-ups 29-33): a knob turned by a drag (detent
// ticks), the button well (each touch its own button, pressed on touch-down, let go on touch-up), F1 / F2, the LCD
// (FIZZ's 240 x 240 frames, nearest-neighbour), the LED keybed (several fingers at once, velocity from where each lands).

import SwiftUI
import CoreGraphics
import FizzEngine
import FizzKit
#if os(iOS)
import UIKit
#endif

// MARK: - Touches

/// Every finger on a view, each with its own id: began, moved, ended (or cancelled). A plain surface on the Mac.
struct TouchSurface: View {
    var began: (Int, CGPoint) -> Void
    var moved: (Int, CGPoint) -> Void = { _, _ in }
    var ended: (Int) -> Void

    var body: some View {
        #if os(iOS)
        TouchSurfaceRep(began: began, moved: moved, ended: ended)
        #else
        Color.clear
        #endif
    }
}

#if os(iOS)
private struct TouchSurfaceRep: UIViewRepresentable {
    var began: (Int, CGPoint) -> Void
    var moved: (Int, CGPoint) -> Void
    var ended: (Int) -> Void

    func makeUIView(context: Context) -> TouchView {
        let v = TouchView()
        v.isMultipleTouchEnabled = true
        v.backgroundColor = .clear
        return v
    }

    func updateUIView(_ v: TouchView, context: Context) {
        v.began = began; v.moved = moved; v.ended = ended
    }

    final class TouchView: UIView {
        var began: (Int, CGPoint) -> Void = { _, _ in }
        var moved: (Int, CGPoint) -> Void = { _, _ in }
        var ended: (Int) -> Void = { _ in }
        private var ids: [ObjectIdentifier: Int] = [:]
        private var nextID = 1

        override func touchesBegan(_ touches: Set<UITouch>, with event: UIEvent?) {
            for t in touches {
                let id = nextID
                nextID += 1
                ids[ObjectIdentifier(t)] = id
                began(id, t.location(in: self))
            }
        }

        override func touchesMoved(_ touches: Set<UITouch>, with event: UIEvent?) {
            for t in touches { if let id = ids[ObjectIdentifier(t)] { moved(id, t.location(in: self)) } }
        }

        override func touchesEnded(_ touches: Set<UITouch>, with event: UIEvent?) { finish(touches) }
        override func touchesCancelled(_ touches: Set<UITouch>, with event: UIEvent?) { finish(touches) }

        private func finish(_ touches: Set<UITouch>) {
            for t in touches { if let id = ids.removeValue(forKey: ObjectIdentifier(t)) { ended(id) } }
        }
    }
}
#endif

// MARK: - A knob

/// RemoteFM1's knob: the label over a dark cap with its knurled ring and a mark in FIZZ's knob colour. A drag turns
/// it: detents (`turn`), or MASTER's absolute value (`absolute`).
struct PlayKnob: View {
    let label: String
    let size: CGFloat
    let angle: Double
    let mark: RGB
    let p: PlayTheme.Palette
    var turn: ((Int) -> Void)?
    var absolute: (start: () -> Int, set: (Int) -> Void)?

    @State private var last: CGSize = .zero
    @State private var acc = PlayInput.Detents()
    @State private var start: Int?

    var body: some View {
        VStack(spacing: size * 0.13) {
            Text(label).petch(max(11, size * 0.26), em: 0.07).foregroundStyle(p.label.color).lineLimit(1).fixedSize()
            cap.frame(width: size, height: size)
        }
        .frame(width: size + 28)
        .contentShape(Rectangle())
        .gesture(drag)
    }

    private var cap: some View {
        Canvas { ctx, s in
            let k = s.width / 100
            let c = CGPoint(x: s.width / 2, y: s.height / 2)
            func circle(_ r: CGFloat) -> Path { Path(ellipseIn: CGRect(x: c.x - r * k, y: c.y - r * k, width: 2 * r * k, height: 2 * r * k)) }
            ctx.fill(circle(50), with: .color(p.knob.color))
            ctx.stroke(circle(45), with: .color(.white.opacity(0.16)),
                       style: StrokeStyle(lineWidth: 8 * k, dash: [3.2 * k, 3.9 * k]))
            ctx.fill(circle(36), with: .color(p.knob.color))
            ctx.fill(circle(36), with: .color(.white.opacity(0.07)))
            var m = ctx
            m.translateBy(x: c.x, y: c.y)
            m.rotate(by: .degrees(angle))
            var line = Path()
            line.move(to: CGPoint(x: 0, y: -40 * k))
            line.addLine(to: CGPoint(x: 0, y: -20 * k))
            m.stroke(line, with: .color(mark.color), style: StrokeStyle(lineWidth: 7 * k, lineCap: .round))
        }
        .shadow(color: .black.opacity(0.28), radius: 0, y: max(3, size * 0.06))
    }

    private var drag: some Gesture {
        DragGesture(minimumDistance: 0)
            .onChanged { v in
                if let absolute {
                    let s = start ?? absolute.start()
                    start = s
                    absolute.set(PlayInput.master(from: s, dx: v.translation.width, dy: v.translation.height))
                } else if let turn {
                    let d = acc.move(dx: v.translation.width - last.width, dy: v.translation.height - last.height)
                    last = v.translation
                    if d != 0 { turn(d) }
                }
            }
            .onEnded { _ in last = .zero; acc = PlayInput.Detents(); start = nil }
    }
}

// MARK: - Buttons

/// One button's face (Buttons.dc): its label, lit by its LED, sunk while held.
struct PanelKeyFace: View {
    let b: FizzButton
    let w: CGFloat, h: CGFloat
    let p: PlayTheme.Palette
    /// its LED 0..1, PLAY's green 0..1
    let led: Double
    var green: Double = 0
    let down: Bool
    var font: CGFloat = 12

    var body: some View {
        let lit = max(0, min(1, (led - 0.05) / 0.7))
        let ink: RGB = b == .play && green > 0.05 ? p.keyText.mix(100 * (1 - green), RGB(0x4FE08A))
            : p.keyText.mix(100 * (1 - lit), p.led)
        VStack(spacing: 3) {
            if b == .play {
                Image(systemName: "play.fill").font(.system(size: font * 0.7)).foregroundStyle(RGB(0x4FE08A).color)
            } else if b == .rec {
                Circle().fill(RGB(0xFF4436).color).frame(width: font * 0.7, height: font * 0.7)
                    .shadow(color: RGB(0xFF4436).opacity(lit), radius: 6)
            }
            Text(PlayInput.label(b)).petch(font, em: 0.02).multilineTextAlignment(.center).lineSpacing(-2)
                .foregroundStyle(ink.color)
                .shadow(color: (b == .play ? RGB(0x4FE08A) : p.led).opacity(b == .play ? green : lit), radius: 6)
                .lineLimit(2).minimumScaleFactor(0.7)
        }
        .frame(width: w, height: h)
        .background(
            RoundedRectangle(cornerRadius: h * 0.24)
                .fill((lit > 0.5 ? p.hi : p.key).color)
                .overlay(alignment: .top) {
                    RoundedRectangle(cornerRadius: h * 0.24).strokeBorder(p.hi.color, lineWidth: 2).mask(
                        VStack(spacing: 0) { Rectangle().frame(height: 3); Spacer(minLength: 0) })
                }
                .shadow(color: p.lo.color, radius: 0, y: down ? 1 : 4)
                .shadow(color: RGB(0x4FE08A).opacity(b == .play ? green * 0.9 : 0), radius: 10)
        )
        .offset(y: down ? 3 : 0)
    }
}

/// The well of buttons in rows (6 across): every finger presses the button it lands on until it lifts.
struct ButtonWell: View {
    let model: PlayModel
    let buttons: [FizzButton]
    let cols: Int
    let bw: CGFloat, bh: CGFloat, gap: CGFloat
    var pad: CGFloat = 10
    var font: CGFloat = 12
    var radius: CGFloat?
    var bed = true
    @State private var fingers: [Int: FizzButton] = [:]

    var rows: Int { (buttons.count + cols - 1) / cols }
    var size: CGSize {
        CGSize(width: CGFloat(cols) * bw + CGFloat(cols - 1) * gap + 2 * pad, height: CGFloat(rows) * bh + CGFloat(rows - 1) * gap + 2 * pad)
    }

    var body: some View {
        let p = model.theme.palette
        let green = Double(model.leds?.playGreen ?? 0) / 255
        ZStack(alignment: .topLeading) {
            ForEach(Array(buttons.enumerated()), id: \.offset) { k, b in
                PanelKeyFace(b: b, w: bw, h: bh, p: p, led: model.buttonLED(b), green: green,
                             down: model.pressed.contains(b), font: font)
                    .offset(x: pad + CGFloat(k % cols) * (bw + gap), y: pad + CGFloat(k / cols) * (bh + gap))
            }
            TouchSurface(began: { id, pt in
                guard let b = button(at: pt) else { return }
                fingers[id] = b
                model.press(b)
            }, ended: { id in
                guard let b = fingers.removeValue(forKey: id) else { return }
                if !fingers.values.contains(b) { model.release(b) }
            })
        }
        .frame(width: size.width, height: size.height, alignment: .topLeading)
        .background(
            RoundedRectangle(cornerRadius: radius ?? (bh * 0.34 + 6)).fill(bed ? p.bed.color : .clear)
                .overlay(RoundedRectangle(cornerRadius: radius ?? (bh * 0.34 + 6)).stroke(.black.opacity(bed ? 0.18 : 0), lineWidth: 2)
                    .blur(radius: 1).offset(y: 1).mask(RoundedRectangle(cornerRadius: radius ?? (bh * 0.34 + 6))))
        )
    }

    /// The button under a point (the gaps belong to the nearest).
    func button(at pt: CGPoint) -> FizzButton? {
        let c = Int(((pt.x - pad + gap / 2) / (bw + gap)).rounded(.down)), r = Int(((pt.y - pad + gap / 2) / (bh + gap)).rounded(.down))
        let cc = max(0, min(cols - 1, c)), rr = max(0, min(rows - 1, r))
        let k = rr * cols + cc
        return buttons.indices.contains(k) ? buttons[k] : nil
    }
}

// MARK: - The screen

/// FIZZ's 240 x 240 LCD in its bezel, scaled without smoothing.
struct LCDScreen: View {
    let image: CGImage?
    let size: CGFloat
    let p: PlayTheme.Palette

    var body: some View {
        let inset = (size * 0.045).rounded()
        ZStack {
            RoundedRectangle(cornerRadius: size * 0.09).fill(p.bezel.color)
            Group {
                if let image {
                    Image(decorative: image, scale: 1).resizable().interpolation(.none)
                } else {
                    Color.black
                }
            }
            .frame(width: size - 2 * inset, height: size - 2 * inset)
            .clipShape(RoundedRectangle(cornerRadius: size * 0.035))
            RoundedRectangle(cornerRadius: size * 0.09)
                .fill(LinearGradient(colors: [.white.opacity(0.06), .clear], startPoint: .topLeading, endPoint: UnitPoint(x: 0.45, y: 0.45)))
                .allowsHitTesting(false)
        }
        .frame(width: size, height: size)
        .shadow(color: .black.opacity(0.25), radius: 0, y: 4)
    }
}

// MARK: - The keybed

/// The LED keybed: a row of all 16 white keys, or (`keyW`) a scrolling stage under a strip of all 16 with its window.
struct LEDKeybed: View {
    let model: PlayModel
    let w: CGFloat, h: CGFloat
    /// the scrolling layout's key width (nil: the row)
    var keyW: CGFloat?
    /// the white keys' step numbers (the KEYS view)
    var numbers = false
    @State private var fingers: [Int: Int] = [:]
    @State private var depth: [Int: CGFloat] = [:]
    @State private var stripStart: Double?

    private var pad: CGFloat { max(8, min(18, (min(w, h) * 0.05).rounded())) }
    private var mh: CGFloat { keyW == nil ? 0 : max(22, min(34, (h * 0.11).rounded())) }
    private var stage: CGRect {
        if keyW == nil { return CGRect(x: pad, y: pad, width: w - 2 * pad, height: h - 2 * pad) }
        let g = max(3, min(10, (w * 0.008).rounded()))
        let top = pad + mh + max(8, g + 4)
        return CGRect(x: pad, y: top, width: w - 2 * pad, height: h - top - pad)
    }

    var layout: KeyLayout {
        let s = stage
        if let keyW {
            let probe = KeyLayout(size: s.size, keyW: keyW)
            let off = max(0, min(16 - probe.visible, model.keysOffset))
            return KeyLayout(size: s.size, keyW: keyW, offset: off)
        }
        return KeyLayout(size: s.size)
    }

    var body: some View {
        let p = model.theme.palette
        let L = layout, s = stage
        ZStack(alignment: .topLeading) {
            if keyW != nil { strip(L, p) }
            ZStack(alignment: .topLeading) {
                ForEach(L.frames, id: \.key) { f in key(f.key, f.rect, f.black, L, p) }
                TouchSurface(began: { id, pt in
                    guard let (k, d) = L.key(at: pt) else { return }
                    fingers[id] = k
                    depth[id] = d
                    model.keyDown(k, velocity: PlayInput.velocity(at: d))
                }, moved: { id, pt in
                    // a finger sliding onto another key plays it (as from one key to the next)
                    guard let old = fingers[id], let (k, _) = L.key(at: pt), k != old else { return }
                    fingers[id] = k
                    if !fingers.values.contains(old) { model.keyUp(old) }
                    model.keyDown(k, velocity: PlayInput.velocity(at: depth[id] ?? 0.6))
                }, ended: { id in
                    guard let k = fingers.removeValue(forKey: id) else { return }
                    depth[id] = nil
                    if !fingers.values.contains(k) { model.keyUp(k) }
                })
            }
            .frame(width: s.width, height: s.height, alignment: .topLeading)
            .clipped()
            .offset(x: s.minX, y: s.minY)
        }
        .frame(width: w, height: h, alignment: .topLeading)
        .background(RoundedRectangle(cornerRadius: (min(w, h) * 0.08).rounded()).fill(p.bed.color))
    }

    @ViewBuilder
    private func key(_ k: Int, _ r: CGRect, _ black: Bool, _ L: KeyLayout, _ p: PlayTheme.Palette) -> some View {
        let led = model.keyLED(k)
        let down = model.held.contains(k)
        let on = down ? 1 : led
        let lw = max(3, (L.keyWidth * 0.08).rounded())
        let rad = (min(r.width * 0.42, 18)).rounded()
        VStack(spacing: r.height * 0.08) {
            RoundedRectangle(cornerRadius: 3)
                .fill(on > 0.04 ? p.led.color : p.ledOff.color)
                .opacity(on > 0.04 ? 0.45 + 0.55 * on : 1)
                .frame(width: lw, height: (r.height * (black ? 0.42 : 0.42)).rounded())
                .shadow(color: p.led.opacity(on > 0.5 ? on : 0), radius: 6)
            if numbers && !black, let i = KeyLayout.whites.firstIndex(of: k) {
                Text("\(i + 1)").petch(12, em: 0).foregroundStyle(p.keyText.color)
            }
        }
        .frame(width: r.width, height: r.height)
        .background(
            RoundedRectangle(cornerRadius: rad).fill((down ? p.hi : p.key).color)
                .overlay(RoundedRectangle(cornerRadius: rad).strokeBorder(p.hi.color, lineWidth: 2)
                    .mask(VStack(spacing: 0) { Rectangle().frame(height: 3); Spacer(minLength: 0) }))
                .shadow(color: p.lo.color, radius: 0, y: down ? 1 : 4)
        )
        .offset(x: r.minX, y: r.minY + (down ? 3 : 0))
    }

    /// The strip of all 16 steps: its window shows what the stage shows; a drag moves it.
    @ViewBuilder
    private func strip(_ L: KeyLayout, _ p: PlayTheme.Palette) -> some View {
        let sw = w - 2 * pad, vis = min(16, L.visible), off = -Double(L.originX / L.pitch)
        let cw = (sw - 15 * 2) / 16
        ZStack(alignment: .topLeading) {
            HStack(spacing: 2) {
                ForEach(0..<16, id: \.self) { i in
                    let inside = Double(i) >= off.rounded(.down) && Double(i) < off + vis
                    let lit = model.keyLED(KeyLayout.whites[i]) > 0.3 || model.held.contains(KeyLayout.whites[i])
                    RoundedRectangle(cornerRadius: 4).fill((inside ? p.hi : p.key).color)
                        .overlay(alignment: .bottom) {
                            RoundedRectangle(cornerRadius: 2).fill(lit ? p.led.color : p.ledOff.color)
                                .frame(width: cw * 0.55, height: 3).padding(.bottom, 4)
                                .shadow(color: p.led.opacity(lit ? 1 : 0), radius: 4)
                        }
                        .frame(height: mh)
                }
            }
            RoundedRectangle(cornerRadius: 7).strokeBorder(p.keyText.color, lineWidth: 2)
                .frame(width: sw * vis / 16, height: mh + 6)
                .offset(x: sw * off / 16, y: -3)
        }
        .frame(width: sw, height: mh, alignment: .topLeading)
        .contentShape(Rectangle())
        .gesture(DragGesture(minimumDistance: 0).onChanged { v in
            let s0 = stripStart ?? {
                // a touch outside the window: the window centred there first
                let at = Double(v.startLocation.x / sw * 16)
                return at >= off && at <= off + vis ? off : at - vis / 2
            }()
            stripStart = s0
            model.keysOffset = max(0, min(16 - vis, s0 + Double(v.translation.width / sw * 16)))
        }.onEnded { _ in stripStart = nil })
        .offset(x: pad, y: pad)
    }
}

// MARK: - Small parts

/// F1 / F2 (the FM-1's OCT- / OCT+) in their tray: pressed and let go as the well's buttons are.
struct FPair: View {
    let model: PlayModel
    let w: CGFloat, h: CGFloat
    var font: CGFloat = 14

    var body: some View {
        let pad = (h * 0.1).rounded(), gap: CGFloat = 6
        let bw = (w - 2 * pad - gap) / 2
        ButtonWell(model: model, buttons: [.f1, .f2], cols: 2, bw: bw, bh: h - 2 * pad, gap: gap, pad: pad, font: font,
                   radius: 14)
            .frame(width: w, height: h)
    }
}

/// The red LED beside KN1.
struct PanelLED: View {
    var size: CGFloat = 10
    var body: some View {
        RoundedRectangle(cornerRadius: 2).fill(RGB(0xFF3B2F).color).frame(width: size, height: size)
            .shadow(color: RGB(0xFF3B2F).color, radius: 4)
    }
}
