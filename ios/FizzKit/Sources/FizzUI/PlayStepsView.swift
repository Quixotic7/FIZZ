// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// STEPS' parts (mock-ups 38-40; the layouts are PlayView's): the 16 step pads in one or two rows, lit by FIZZ's LEDs
// for their keys, each finger holding the pad it lands on; the chromatic keyboard (several fingers, velocity from where
// each lands, the keys down tinted in FIZZ's note colours); OCT - / +.

import SwiftUI
import FizzEngine
import FizzKit

/// FIZZ's note colours: a step's notes in slot order take its knobs' colours (ui_held.c slot_col: KNOB 1..4, then
/// ALGORITHM's pink, the sixth grey).
enum NoteInk {
    static let slots = [FZ.k1, FZ.k2, FZ.k3, FZ.k4, FZ.kalg, RGB(0x8D8D96)]
    static func of(_ slot: Int?) -> RGB? { slot.map { slots[min($0, slots.count - 1)] } }
}

// MARK: - The pads

/// The 16 steps as pads (FIZZ's white keys): 8 across in two rows, or 16 in one.
struct StepPads: View {
    let model: PlayModel
    let w: CGFloat, h: CGFloat
    var rows = 2
    @State private var fingers: [Int: Int] = [:]

    private var pad: CGFloat { rows == 1 ? 12 : 10 }
    private var gap: CGFloat { rows == 1 ? 8 : 6 }
    private var per: Int { 16 / rows }
    private var pw: CGFloat { (w - 2 * pad - CGFloat(per - 1) * gap) / CGFloat(per) }
    private var ph: CGFloat { (h - 2 * pad - CGFloat(rows - 1) * gap) / CGFloat(rows) }

    var body: some View {
        let p = model.theme.palette
        ZStack(alignment: .topLeading) {
            ForEach(0..<16, id: \.self) { i in
                StepPad(n: i + 1, w: pw, h: ph, p: p, led: model.padLED(i), held: model.pads.contains(i),
                        notes: model.padNotes[i] ?? [])
                    .offset(x: pad + CGFloat(i % per) * (pw + gap), y: pad + CGFloat(i / per) * (ph + gap))
            }
            TouchSurface(began: { id, pt in
                guard let i = index(at: pt) else { return }
                fingers[id] = i
                model.padDown(i)
            }, ended: { id in
                guard let i = fingers.removeValue(forKey: id) else { return }
                if !fingers.values.contains(i) { model.padUp(i) }
            })
        }
        .frame(width: w, height: h, alignment: .topLeading)
        .background(RoundedRectangle(cornerRadius: (min(w, h) * 0.12).rounded()).fill(p.bed.color))
    }

    /// The pad under a point (the gaps belong to the nearest).
    func index(at pt: CGPoint) -> Int? {
        let c = Int(((pt.x - pad + gap / 2) / (pw + gap)).rounded(.down)), r = Int(((pt.y - pad + gap / 2) / (ph + gap)).rounded(.down))
        guard pt.x >= 0, pt.y >= 0, pt.x <= w, pt.y <= h else { return nil }
        return max(0, min(rows - 1, r)) * per + max(0, min(per - 1, c))
    }
}

/// One pad: its LED bar (the theme's LED colour: a trig dim, the playhead bright), its number; held, it glows and the
/// notes going into it pop as dots in their colours.
struct StepPad: View {
    let n: Int
    let w: CGFloat, h: CGFloat
    let p: PlayTheme.Palette
    let led: Double
    let held: Bool
    let notes: [Int]

    var body: some View {
        let r = (min(w, h) * 0.26).rounded()
        let lw = max(4, (w * 0.09).rounded())
        VStack(spacing: h * 0.07) {
            RoundedRectangle(cornerRadius: 2)
                .fill(led > 0.04 ? p.led.color : p.ledOff.color)
                .opacity(led > 0.04 ? 0.4 + 0.6 * led : 1)
                .frame(width: lw, height: (h * 0.36).rounded())
                .shadow(color: p.led.opacity(led > 0.5 ? led : 0), radius: 6)
            Text("\(n)").petch(max(12, min(15, h * 0.2)), em: 0).foregroundStyle(p.keyText.color)
        }
        .frame(width: w, height: h)
        .overlay(alignment: .top) {
            if !notes.isEmpty {
                VStack(spacing: -4) {
                    ForEach(Array(notes.prefix(6).enumerated()), id: \.offset) { k, _ in
                        Circle().fill((NoteInk.of(k) ?? p.led).color)
                            .overlay(Circle().strokeBorder(.black.opacity(0.55), lineWidth: 1.5))
                            .frame(width: 13, height: 13)
                            .shadow(color: (NoteInk.of(k) ?? p.led).opacity(0.8), radius: 4)
                            .transition(.scale(scale: 0.1).combined(with: .opacity))
                    }
                }
                .offset(x: lw / 2 + 8, y: 4)
            }
        }
        .animation(.spring(response: 0.28, dampingFraction: 0.55), value: notes)
        .background(
            RoundedRectangle(cornerRadius: r).fill((held ? p.hi : p.key).color)
                .overlay(RoundedRectangle(cornerRadius: r).strokeBorder(p.hi.color, lineWidth: 2)
                    .mask(VStack(spacing: 0) { Rectangle().frame(height: 3); Spacer(minLength: 0) }))
                .overlay(RoundedRectangle(cornerRadius: r).strokeBorder(p.led.color, lineWidth: held ? 2.5 : 0))
                .shadow(color: p.led.opacity(held ? 0.75 : 0), radius: 10)
                .shadow(color: p.lo.color, radius: 0, y: held ? 1 : 4)
        )
        .offset(y: held ? 3 : 0)
    }
}

// MARK: - The keyboard

/// The chromatic keyboard: `whites` white keys from C<the model's octave>; every finger plays the key it lands on
/// (sliding onto another plays that), its velocity from where it lands.
struct StepsKeyboard: View {
    let model: PlayModel
    let w: CGFloat, h: CGFloat
    let whites: Int
    @State private var fingers: [Int: Int] = [:]
    @State private var depth: [Int: CGFloat] = [:]

    private var pad: CGFloat { 9 }

    var body: some View {
        let p = model.theme.palette
        let L = PianoLayout(size: CGSize(width: w - 2 * pad, height: h - 2 * pad), whites: whites, octave: model.stepsOctave)
        ZStack(alignment: .topLeading) {
            ForEach(Array(L.whiteNotes.enumerated()), id: \.offset) { i, n in white(n, L.whiteFrame(i), p) }
            ForEach(L.blacks, id: \.note) { b in black(b.note, b.rect, p) }
            TouchSurface(began: { id, pt in
                guard let (n, d) = L.note(at: pt) else { return }
                fingers[id] = n
                depth[id] = d
                model.noteDown(n, velocity: PlayInput.velocity(at: d))
            }, moved: { id, pt in
                guard let old = fingers[id], let (n, _) = L.note(at: pt), n != old else { return }
                fingers[id] = n
                if !fingers.values.contains(old) { model.noteUp(old) }
                model.noteDown(n, velocity: PlayInput.velocity(at: depth[id] ?? 0.6))
            }, ended: { id in
                guard let n = fingers.removeValue(forKey: id) else { return }
                depth[id] = nil
                if !fingers.values.contains(n) { model.noteUp(n) }
            })
        }
        .frame(width: L.size.width, height: L.size.height, alignment: .topLeading)
        .padding(pad)
        .frame(width: w, height: h)
        .background(RoundedRectangle(cornerRadius: (min(w, h) * 0.08).rounded()).fill(p.bed.color))
        .onChange(of: model.stepsOctave) { fingers = [:]; depth = [:] }
    }

    private var ivory: RGB { RGB(0xFBF3EA) }

    @ViewBuilder
    private func white(_ n: Int, _ r: CGRect, _ p: PlayTheme.Palette) -> some View {
        let ink = model.notes.contains(n) ? (NoteInk.of(model.slot(n)) ?? p.led) : nil
        let rad = min(10, r.width * 0.2)
        ZStack(alignment: .bottom) {
            UnevenRoundedRectangle(bottomLeadingRadius: rad, bottomTrailingRadius: rad)
                .fill(ink.map { LinearGradient(colors: [ivory.mix(70, $0).color, ivory.mix(35, $0).color], startPoint: .top, endPoint: .bottom) }
                      ?? LinearGradient(colors: [ivory.color, ivory.color], startPoint: .top, endPoint: .bottom))
                .shadow(color: (ink ?? p.lo).opacity(ink == nil ? 0.5 : 0.85), radius: ink == nil ? 0 : 12, y: ink == nil ? 3 : 0)
            VStack(spacing: 5) {
                if let ink {
                    Circle().strokeBorder(RGB(0x141414).color, lineWidth: 2).background(Circle().fill(ink.color))
                        .frame(width: 15, height: 15)
                        .transition(.scale(scale: 0.2).combined(with: .opacity))
                }
                if n % 12 == 0 || ink != nil {
                    Text(StepsInput.name(n)).petch(12, em: 0.02)
                        .foregroundStyle((ink != nil ? RGB(0x1A1A1A) : p.key).color)
                        .lineLimit(1).minimumScaleFactor(0.6)
                }
            }
            .padding(.bottom, 10)
        }
        .frame(width: r.width, height: r.height)
        .offset(x: r.minX, y: r.minY + (ink != nil ? 2 : 0))
        .animation(.spring(response: 0.2, dampingFraction: 0.7), value: ink != nil)
    }

    @ViewBuilder
    private func black(_ n: Int, _ r: CGRect, _ p: PlayTheme.Palette) -> some View {
        let ink = model.notes.contains(n) ? (NoteInk.of(model.slot(n)) ?? p.led) : nil
        let face = model.theme == .fizz ? RGB(0x101014) : p.key
        UnevenRoundedRectangle(bottomLeadingRadius: 6, bottomTrailingRadius: 6)
            .fill((ink.map { face.mix(45, $0) } ?? face).color)
            .overlay(alignment: .bottom) {
                if let ink {
                    Circle().fill(ink.color).frame(width: 11, height: 11).padding(.bottom, 8)
                        .shadow(color: ink.color, radius: 5)
                }
            }
            .shadow(color: (ink ?? RGB(0x000000)).opacity(ink == nil ? 0.35 : 0.8), radius: ink == nil ? 0 : 10, y: ink == nil ? 3 : 0)
            .frame(width: r.width, height: r.height)
            .offset(x: r.minX, y: r.minY)
    }
}

// MARK: - OCT - / +

/// OCT - or OCT + (the FM-1's F1 / F2 face).
struct OctButton: View {
    let model: PlayModel
    let up: Bool
    let w: CGFloat, h: CGFloat
    var font: CGFloat = 14
    var stacked = false

    var body: some View {
        let p = model.theme.palette
        let can = up ? model.stepsOctave < StepsInput.octaves.upperBound : model.stepsOctave > StepsInput.octaves.lowerBound
        Button { model.stepsOctave(by: up ? 1 : -1) } label: {
            Text(stacked ? "OCT\n\(up ? "+" : "−")" : "OCT \(up ? "+" : "−")")
                .petch(font, em: 0.08).multilineTextAlignment(.center)
                .foregroundStyle(p.keyText.color.opacity(can ? 1 : 0.4))
                .frame(width: w, height: h)
                .background(
                    RoundedRectangle(cornerRadius: min(14, h * 0.3)).fill(p.key.color)
                        .overlay(RoundedRectangle(cornerRadius: min(14, h * 0.3)).strokeBorder(p.hi.color, lineWidth: 2)
                            .mask(VStack(spacing: 0) { Rectangle().frame(height: 3); Spacer(minLength: 0) }))
                        .shadow(color: p.lo.color, radius: 0, y: 4)
                )
                .contentShape(Rectangle())
        }
        .buttonStyle(PressStyle(scale: 0.95))
        .accessibilityLabel(up ? "Octave up" : "Octave down")
    }
}

/// OCT - and OCT + in one tray (the phone on its side).
struct OctPair: View {
    let model: PlayModel
    let w: CGFloat, h: CGFloat
    var font: CGFloat = 14

    var body: some View {
        let pad = (h * 0.1).rounded(), gap: CGFloat = 6
        let bw = (w - 2 * pad - gap) / 2
        HStack(spacing: gap) {
            OctButton(model: model, up: false, w: bw, h: h - 2 * pad, font: font)
            OctButton(model: model, up: true, w: bw, h: h - 2 * pad, font: font)
        }
        .padding(pad)
        .frame(width: w, height: h)
        .background(RoundedRectangle(cornerRadius: 14 + pad).fill(model.theme.palette.bed.color))
    }
}
