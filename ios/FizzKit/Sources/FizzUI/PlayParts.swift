// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// PLAY's parts as RemoteFM1's (mock-ups 49-54, their numbers; PlayRFMLayout.swift places them): a knob turned by a
// drag (detent ticks; MASTER absolute), a well of keys (each touch its own key, pressed on touch-down, let go on
// touch-up), the LCD (FIZZ's 240 x 240 frames, nearest-neighbour), the LED keybed (several fingers at once, velocity
// from where each lands; FIZZ's 27 keys, or in STEPS the notes from the octave chosen).

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

// MARK: - A key's face

/// RemoteFM1's key: the face with a 2 pt line of `hi` along its top (inset 0 2 0) and a hard `lo` shadow under it
/// (0 4 0; 1 while held down).
struct KeyFace: View {
    let radius: CGFloat
    let face: Color, hi: Color, lo: Color
    var drop: CGFloat = 4

    var body: some View {
        let s = RoundedRectangle(cornerRadius: radius, style: .circular)
        ZStack {
            s.fill(lo).offset(y: drop)
            ZStack {
                s.fill(hi)
                s.fill(face).offset(y: 2)
            }
            .clipShape(s)
        }
    }
}

/// A well's (or the keybed's) bed: the theme's bed with a 3 pt shade along its top inside (inset 0 3 0, black 20 %).
struct Bed: View {
    let radius: CGFloat
    let color: Color
    var shade: CGFloat = 0

    var body: some View {
        let s = RoundedRectangle(cornerRadius: radius, style: .circular)
        if shade > 0 {
            ZStack {
                s.fill(color)
                s.fill(.black.opacity(0.2))
                s.fill(color).offset(y: shade)
            }
            .clipShape(s)
        } else {
            s.fill(color)
        }
    }
}

// MARK: - A knob

/// RemoteFM1's knob: the label (Chakra Petch, its line 6 pt over the cap) and the dark cap with its knurled ring, the
/// theme's white pointer and a hard shadow. A drag turns it: detents (`turn`), or MASTER's absolute value (`absolute`).
struct PlayKnob: View {
    let label: String
    let size: CGFloat
    let labelSize: CGFloat
    let gap: CGFloat
    let shadow: CGFloat
    let angle: Double
    let p: PlayTheme.Palette
    var turn: ((Int) -> Void)?
    var absolute: (start: () -> Int, set: (Int) -> Void)?

    @State private var last: CGSize = .zero
    @State private var acc = PlayInput.Detents()
    @State private var start: Int?

    /// its whole height (the label's line, the gap, the cap)
    var height: CGFloat { labelSize + gap + size }

    var body: some View {
        VStack(spacing: gap) {
            Text(label).petch(labelSize, em: 0.08).foregroundStyle(p.label.color).lineLimit(1).fixedSize()
                .padding(.leading, labelSize * 0.08)
                .frame(height: labelSize)
            cap.frame(width: size, height: size)
        }
        .frame(minWidth: size + 20)
        .contentShape(Rectangle())
        .gesture(drag)
    }

    private var cap: some View {
        ZStack {
            Circle().fill(.black.opacity(0.28)).offset(y: shadow)
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
                m.stroke(line, with: .color(p.mark.color), style: StrokeStyle(lineWidth: 7 * k, lineCap: .round))
            }
        }
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

// MARK: - Legends

/// A key's legend: FIZZ's name at RemoteFM1's size (PlayLegend.fit), a two-word name on two lines (line height 1.1),
/// PLAY's green triangle or REC's red dot over it.
struct KeyLegend: View {
    let name: String
    let keyWidth: CGFloat
    let base: CGFloat
    let glyph: CGFloat
    let ink: Color
    var glow: Color = .clear
    var mark: FizzButton?

    var body: some View {
        let (fs, tr) = PlayLegend.fit(name, keyWidth: keyWidth, base: base)
        VStack(spacing: 0) {
            if mark == .play {
                Path { p in
                    p.move(to: CGPoint(x: 0.2 * glyph, y: 0.1 * glyph))
                    p.addLine(to: CGPoint(x: 0.9 * glyph, y: 0.5 * glyph))
                    p.addLine(to: CGPoint(x: 0.2 * glyph, y: 0.9 * glyph))
                    p.closeSubpath()
                }
                .fill(RGB(0x4FE08A).color)
                .frame(width: glyph, height: glyph).padding(.bottom, 2)
            } else if mark == .rec {
                Circle().fill(RGB(0xFF4436).color).frame(width: glyph * 0.8, height: glyph * 0.8)
                    .frame(width: glyph, height: glyph).padding(.bottom, 2)
            }
            ForEach(Array(name.split(separator: " ").enumerated()), id: \.offset) { _, line in
                Text(String(line)).font(PlayFont.font(fs)).tracking(fs * tr).padding(.leading, fs * tr)
                    .lineLimit(1).fixedSize()
                    .frame(height: fs * 1.1)
            }
        }
        .foregroundStyle(ink)
        .shadow(color: glow, radius: 4)
    }
}

// MARK: - Buttons

/// One button (Buttons.dc): its legend, lit by its LED, sunk while held; PLAY's green while FIZZ plays.
struct PanelKeyFace: View {
    let b: FizzButton
    let w: CGFloat, h: CGFloat
    let radius: CGFloat
    let legend: CGFloat
    let p: PlayTheme.Palette
    /// its LED 0..1, PLAY's green 0..1
    let led: Double
    var green: Double = 0
    let down: Bool

    var body: some View {
        let lit = max(0, min(1, (led - 0.05) / 0.7))
        let ink: RGB = b == .play && green > 0.05 ? p.keyText.mix(100 * (1 - green), RGB(0x4FE08A))
            : p.keyText.mix(100 * (1 - lit), p.led)
        let glow = (b == .play ? RGB(0x4FE08A) : p.led).opacity(b == .play ? green : lit)
        KeyLegend(name: PlayInput.name(b), keyWidth: w, base: legend, glyph: max(6, (h * 0.15).rounded()),
                  ink: ink.color, glow: glow, mark: b == .play || b == .rec ? b : nil)
            .frame(width: w, height: h)
            .background(
                KeyFace(radius: radius, face: (lit > 0.5 ? p.hi : p.key).color, hi: p.hi.color, lo: p.lo.color, drop: down ? 1 : 4)
                    .shadow(color: RGB(0x4FE08A).opacity(b == .play ? green * 0.9 : 0), radius: 9)
            )
            .offset(y: down ? 3 : 0)
    }
}

/// A well of buttons (`cols` across): every finger presses the button it lands on until it lifts.
struct ButtonWell: View {
    let model: PlayModel
    let buttons: [FizzButton]
    let well: PlayLayout.Well
    /// the board's unit
    let u: CGFloat
    @State private var fingers: [Int: FizzButton] = [:]

    var body: some View {
        let p = model.theme.palette
        let green = Double(model.leds?.playGreen ?? 0) / 255
        let f = well.frame
        ZStack(alignment: .topLeading) {
            ForEach(Array(buttons.enumerated()), id: \.offset) { k, b in
                let r = well.key(k)
                PanelKeyFace(b: b, w: r.width * u, h: r.height * u, radius: well.radius * u, legend: well.legend * u, p: p,
                             led: model.buttonLED(b), green: green, down: model.pressed.contains(b))
                    .offset(x: r.minX * u, y: r.minY * u)
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
        .frame(width: f.width * u, height: f.height * u, alignment: .topLeading)
        .background(Bed(radius: well.wellRadius * u, color: p.bed.color))
    }

    /// The button under a point (the gaps belong to the nearest).
    func button(at pt: CGPoint) -> FizzButton? {
        let k = PlayInput.cell(at: CGPoint(x: pt.x / u, y: pt.y / u), in: well)
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
        let inset = (size * 0.045).rounded(), r = (size * 0.09).rounded()
        ZStack {
            RoundedRectangle(cornerRadius: r).fill(.black.opacity(0.25)).offset(y: 4)
            RoundedRectangle(cornerRadius: r).fill(p.bezel.color)
            Group {
                if let image {
                    Image(decorative: image, scale: 1).resizable().interpolation(.none)
                } else {
                    Color.black
                }
            }
            .frame(width: size - 2 * inset, height: size - 2 * inset)
            .clipShape(RoundedRectangle(cornerRadius: (size * 0.035).rounded()))
            RoundedRectangle(cornerRadius: r)
                .fill(LinearGradient(colors: [.white.opacity(0.06), .clear], startPoint: .topLeading, endPoint: UnitPoint(x: 0.4, y: 0.4)))
                .allowsHitTesting(false)
        }
        .frame(width: size, height: size)
    }
}

// MARK: - The keybed

/// What the keybed plays: FIZZ's 27 keys (its LEDs light them), or (STEPS) notes from the octave chosen, sent to the
/// selected track (the keys held lit).
enum KeybedPlays { case fizz, notes }

/// The LED keybed (Keys.dc, the mock-ups' numbers): a row of all 16 white keys, or (`keyW`) a scrolling stage under a
/// strip of all 16 with its window.
struct LEDKeybed: View {
    let model: PlayModel
    let w: CGFloat, h: CGFloat
    /// the scrolling layout's key width (nil: the row)
    var keyW: CGFloat?
    /// the white keys' step numbers (the KEYS deck)
    var numbers = false
    var plays: KeybedPlays = .fizz
    @State private var fingers: [Int: Int] = [:]
    @State private var depth: [Int: CGFloat] = [:]
    @State private var stripStart: Double?

    private var pad: CGFloat { max(8, min(18, (min(w, h) * 0.05).rounded())) }
    private var gap: CGFloat { max(3, min(10, (w * 0.008).rounded())) }
    private var mh: CGFloat { keyW == nil ? 0 : max(22, min(34, (h * 0.11).rounded())) }
    private var stage: CGRect {
        if keyW == nil { return CGRect(x: pad, y: pad, width: w - 2 * pad, height: h - 2 * pad) }
        let top = pad + mh + max(8, gap + 4)
        return CGRect(x: pad, y: top, width: w - 2 * pad, height: h - top - pad)
    }

    private var offset: Double {
        get { plays == .notes ? model.stepsKeysOffset : model.keysOffset }
        nonmutating set { if plays == .notes { model.stepsKeysOffset = newValue } else { model.keysOffset = newValue } }
    }

    var layout: KeyLayout {
        let s = stage
        if let keyW {
            let probe = KeyLayout(size: s.size, keyW: keyW, gap: gap)
            return KeyLayout(size: s.size, keyW: keyW, offset: max(0, min(16 - probe.visible, offset)), gap: gap)
        }
        return KeyLayout(size: s.size, gap: gap)
    }

    /// what key k sounds: FIZZ's key, or the note
    private func sound(_ k: Int) -> Int { plays == .notes ? StepsInput.keyNote(k, octave: model.stepsOctave) : k }
    private func down(_ s: Int, _ v: Int) { if plays == .notes { model.noteDown(s, velocity: v) } else { model.keyDown(s, velocity: v) } }
    private func up(_ s: Int) { if plays == .notes { model.noteUp(s) } else { model.keyUp(s) } }
    private func isDown(_ k: Int) -> Bool { plays == .notes ? model.notes.contains(sound(k)) : model.held.contains(k) }
    private func led(_ k: Int) -> Double { plays == .notes ? 0 : model.keyLED(k) }

    var body: some View {
        let p = model.theme.palette
        let L = layout, s = stage
        ZStack(alignment: .topLeading) {
            if keyW != nil { strip(L, p) }
            ZStack(alignment: .topLeading) {
                ForEach(L.frames, id: \.key) { f in key(f.key, f.rect, f.black, L, p) }
                TouchSurface(began: { id, pt in
                    guard let (k, d) = L.key(at: pt) else { return }
                    let n = sound(k)
                    fingers[id] = n
                    depth[id] = d
                    down(n, PlayInput.velocity(at: d))
                }, moved: { id, pt in
                    // a finger sliding onto another key plays it (as from one key to the next)
                    guard let old = fingers[id], let (k, _) = L.key(at: pt), sound(k) != old else { return }
                    let n = sound(k)
                    fingers[id] = n
                    if !fingers.values.contains(old) { up(old) }
                    down(n, PlayInput.velocity(at: depth[id] ?? 0.6))
                }, ended: { id in
                    guard let n = fingers.removeValue(forKey: id) else { return }
                    depth[id] = nil
                    if !fingers.values.contains(n) { up(n) }
                })
            }
            .frame(width: s.width, height: s.height, alignment: .topLeading)
            .clipped()
            .offset(x: s.minX, y: s.minY)
        }
        .frame(width: w, height: h, alignment: .topLeading)
        .background(Bed(radius: (min(w, h) * 0.08).rounded(), color: p.bed.color, shade: 3))
        .onChange(of: model.stepsOctave) { fingers = [:]; depth = [:] }
    }

    @ViewBuilder
    private func key(_ k: Int, _ r: CGRect, _ black: Bool, _ L: KeyLayout, _ p: PlayTheme.Palette) -> some View {
        let held = isDown(k)
        let on = held ? 1 : led(k)
        let lw = max(3, (L.keyWidth * 0.08).rounded())
        let rad = (min(r.width * 0.42, 18)).rounded()
        VStack(spacing: (r.height * 0.08).rounded()) {
            RoundedRectangle(cornerRadius: 3)
                .fill(on > 0.04 ? p.led.color : p.ledOff.color)
                .opacity(held ? 1 : on > 0.04 ? 0.45 + 0.55 * on : 1)
                .frame(width: lw, height: (r.height * 0.42).rounded())
                .shadow(color: p.led.opacity(held || on > 0.5 ? 1 : 0), radius: 5)
                .shadow(color: p.led.opacity(held || on > 0.5 ? 1 : 0), radius: 1.5)
            if numbers && !black, let i = KeyLayout.whites.firstIndex(of: k) {
                Text("\(i + 1)").petch(12, em: 0).foregroundStyle(p.keyText.color).frame(height: 12)
            }
        }
        .frame(width: r.width, height: r.height)
        .background(KeyFace(radius: rad, face: (held ? p.hi : p.key).color, hi: p.hi.color, lo: p.lo.color, drop: held ? 1 : 4))
        .offset(x: r.minX, y: r.minY + (held ? 3 : 0))
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
                    let k = KeyLayout.whites[i]
                    let lit = led(k) > 0.3 || isDown(k)
                    RoundedRectangle(cornerRadius: 4).fill((inside ? p.hi : p.key).color)
                        .overlay(alignment: .bottom) {
                            RoundedRectangle(cornerRadius: 2).fill(lit ? p.led.color : p.ledOff.color)
                                .frame(width: cw * 0.55, height: 3).padding(.bottom, 4)
                                .shadow(color: p.led.opacity(lit ? 1 : 0), radius: 3)
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
            offset = max(0, min(16 - vis, s0 + Double(v.translation.width / sw * 16)))
        }.onEnded { _ in stripStart = nil })
        .offset(x: pad, y: pad)
    }
}

// MARK: - Small parts

/// The red LED beside KN1.
struct PanelLED: View {
    var size: CGFloat = 10
    var body: some View {
        RoundedRectangle(cornerRadius: 2).fill(RGB(0xFF3B2F).color).frame(width: size, height: size)
            .shadow(color: RGB(0xFF3B2F).color, radius: 4)
    }
}
