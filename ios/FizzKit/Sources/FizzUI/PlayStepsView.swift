// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// STEPS' parts (mock-up 52, built of RemoteFM1's parts; the layouts are PlayRFMLayout.swift's): the 16 steps as keys
// in a well (8 x 2), each with the keybed's LED slot and its number, lit by FIZZ's LEDs for their keys, each finger
// holding the step it lands on (held: the head's face and glow, its notes as dots in FIZZ's note colours); OCT - / +
// in the tray. The keybed under them is PLAY's own (LEDKeybed, playing notes).

import SwiftUI
import FizzEngine
import FizzKit

/// FIZZ's note colours: a step's notes in slot order take its knobs' colours (ui_held.c slot_col: KNOB 1..4, then
/// ALGORITHM's pink, the sixth grey).
enum NoteInk {
    static let slots = [FZ.k1, FZ.k2, FZ.k3, FZ.k4, FZ.kalg, RGB(0x8D8D96)]
    static func of(_ slot: Int?) -> RGB? { slot.map { slots[min($0, slots.count - 1)] } }
}

// MARK: - The steps

/// The 16 steps (FIZZ's white keys) as keys in a well.
struct StepPads: View {
    let model: PlayModel
    let well: PlayLayout.Well
    /// the board's unit
    let u: CGFloat
    @State private var fingers: [Int: Int] = [:]

    var body: some View {
        let p = model.theme.palette
        let f = well.frame
        ZStack(alignment: .topLeading) {
            ForEach(0..<16, id: \.self) { i in
                let r = well.key(i)
                StepPad(n: i + 1, w: r.width * u, h: r.height * u, radius: well.radius * u, u: u, p: p, led: model.padLED(i),
                        held: model.pads.contains(i), notes: model.padNotes[i] ?? [])
                    .offset(x: r.minX * u, y: r.minY * u)
            }
            TouchSurface(began: { id, pt in
                let i = PlayInput.cell(at: CGPoint(x: pt.x / u, y: pt.y / u), in: well)
                fingers[id] = i
                model.padDown(i)
            }, ended: { id in
                guard let i = fingers.removeValue(forKey: id) else { return }
                if !fingers.values.contains(i) { model.padUp(i) }
            })
        }
        .frame(width: f.width * u, height: f.height * u, alignment: .topLeading)
        .background(Bed(radius: well.wellRadius * u, color: p.bed.color))
    }
}

/// One step: a key face with the keybed's LED slot (a trig dim, the playhead bright) and its number; held, it takes
/// the head's face, an LED ring and glow, and the notes going into it pop as dots in their colours.
struct StepPad: View {
    let n: Int
    let w: CGFloat, h: CGFloat
    let radius: CGFloat
    let u: CGFloat
    let p: PlayTheme.Palette
    let led: Double
    let held: Bool
    let notes: [Int]

    var body: some View {
        // (the mock-up's 3 x 16 slot on a 64 high key, its 11 pt number 5 under it, the dots 4 under that)
        let k = u * max(0.75, min(1.6, h / u / 64))
        VStack(spacing: 0) {
            RoundedRectangle(cornerRadius: 3)
                .fill(led > 0.04 ? p.led.color : p.ledOff.color)
                .opacity(held ? 1 : led > 0.04 ? 0.45 + 0.55 * led : 1)
                .frame(width: 3 * k, height: 16 * k)
                .shadow(color: p.led.opacity(held || led > 0.5 ? 1 : 0), radius: 5)
                .shadow(color: p.led.opacity(held || led > 0.5 ? 1 : 0), radius: 1.5)
            Text("\(n)").petch(11 * k, em: 0).foregroundStyle(p.keyText.color).frame(height: 11 * k).padding(.top, 5 * k)
            if !notes.isEmpty {
                HStack(spacing: 3 * k) {
                    ForEach(Array(notes.prefix(6).enumerated()), id: \.offset) { s, _ in
                        Circle().fill((NoteInk.of(s) ?? p.led).color).frame(width: 6 * k, height: 6 * k)
                            .transition(.scale(scale: 0.1).combined(with: .opacity))
                    }
                }
                .padding(.top, 4 * k)
            }
        }
        .frame(width: w, height: h)
        .animation(.spring(response: 0.28, dampingFraction: 0.55), value: notes)
        .background(
            KeyFace(radius: radius, face: (held ? p.hi : p.key).color, hi: p.hi.color, lo: p.lo.color)
                .overlay(RoundedRectangle(cornerRadius: radius).strokeBorder(p.led.color, lineWidth: held ? 2 : 0).padding(-2))
                .shadow(color: p.led.opacity(held ? 0.55 : 0), radius: 8)
        )
    }
}

// MARK: - OCT - / +

/// OCT - and OCT + in the tray (F1 / F2's place in STEPS): the keybed an octave down or up.
struct OctTray: View {
    let model: PlayModel
    let well: PlayLayout.Well
    let u: CGFloat

    var body: some View {
        let p = model.theme.palette
        let f = well.frame
        ZStack(alignment: .topLeading) {
            ForEach(0..<2, id: \.self) { i in
                let r = well.key(i), up = i == 1
                let can = up ? model.stepsOctave < StepsInput.octaves.upperBound : model.stepsOctave > StepsInput.octaves.lowerBound
                Button { model.stepsOctave(by: up ? 1 : -1) } label: {
                    KeyLegend(name: up ? "OCT+" : "OCT−", keyWidth: r.width * u, base: well.legend * u, glyph: 0,
                              ink: p.keyText.color.opacity(can ? 1 : 0.4))
                        .frame(width: r.width * u, height: r.height * u)
                        .background(KeyFace(radius: well.radius * u, face: p.key.color, hi: p.hi.color, lo: p.lo.color))
                        .contentShape(Rectangle())
                }
                .buttonStyle(PressStyle(scale: 0.95))
                .accessibilityLabel(up ? "Octave up" : "Octave down")
                .offset(x: r.minX * u, y: r.minY * u)
            }
        }
        .frame(width: f.width * u, height: f.height * u, alignment: .topLeading)
        .background(Bed(radius: well.wellRadius * u, color: p.bed.color))
    }
}
