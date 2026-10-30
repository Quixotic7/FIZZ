// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// MIDI learn on PLAY's panel (mock-up 56), laid over the board in its own points: every learnable control outlined
// (the knobs with a ring in the label colour, r = knob / 2 + 6, dashed; the buttons and F1 / F2 with an outline in the
// LED colour, 3 pt out, dashed), solid when bound (a dot on a knob's ring), thick and pulsing when picked; a tap picks
// one. The bar says what to move, then what it bound (Forget, Done): at the foot, or over the top in CONTROLS (the
// buttons reach the foot there). After learn: MASTER's pickup mark (a hollow dot where the pot is) and the toast.
// The keybed is not learnable (MIDI notes play it), nor STEPS' pads and octave keys (the app's, not FIZZ's buttons).
// Written for FIZZ (RemoteFM1's LearnRing / LearnBar / PotMark / ToastView are redrawn from the mock-up, not copied).

import SwiftUI
import FizzEngine

struct MidiLearnLayer: View {
    let play: PlayModel
    let L: PlayLayout
    let u: CGFloat
    @Environment(\.fizzSnapshot) private var snapshot

    private var midi: MidiHub { play.midi }

    var body: some View {
        let p = play.theme.palette
        ZStack(alignment: .topLeading) {
            if midi.learning {
                ForEach(L.knobs, id: \.name) { k in knobRing(k, p) }
                if let w = L.well { buttons(w, PlayInput.well, p) }
                if let t = L.tray, play.view != .steps { buttons(t, [.f1, .f2], p) }
                bar.frame(width: barWidth * u).offset(x: (L.size.width - barWidth) / 2 * u, y: barY * u)
                    .transition(.opacity)
            }
            if let pot = midi.pickupPot, let m = L.knobs.first(where: { $0.name == "MASTER" }) {
                potMark(m, Double(pot) / 1023, p)
            }
            if let t = midi.toast, !midi.learning {
                MidiToast(text: t.text, ok: t.ok, u: u)
                    .frame(width: L.size.width * u)
                    .offset(y: (barY + 7) * u)
                    .transition(.opacity)
            }
        }
        .animation(.easeInOut(duration: 0.2), value: midi.learning)
        .animation(.easeInOut(duration: 0.2), value: midi.toast?.text)
    }

    // MARK: where

    /// The bar's top: over the top in CONTROLS (upright), else at the foot (FULL's y 737).
    private var barY: CGFloat {
        if L.board == .phone && (play.view == .panel || play.view == .keys) { return 54 }
        return L.size.height - (L.dots != nil ? 107 : 72)
    }

    private var barWidth: CGFloat { min(L.size.width - 32, 520) }

    // MARK: the controls

    private func knobRing(_ k: PlayLayout.Knob, _ p: PlayTheme.Palette) -> some View {
        let id = MidiHub.target(knob: k.name) ?? k.name
        let r = k.size / 2 + 6, cy = k.top + k.size / 2
        return MidiKnobRing(mark: midi.mark(id), radius: r * u, color: p.label.color, body: p.body.color, snapshot: snapshot)
            .frame(width: 2 * r * u, height: 2 * r * u)
            .contentShape(Circle())
            .onTapGesture { midi.pick(id) }
            .accessibilityLabel("Learn \(k.name)")
            .accessibilityAddTraits(.isButton)
            .offset(x: (k.cx - r) * u, y: (cy - r) * u)
    }

    private func buttons(_ w: PlayLayout.Well, _ list: [FizzButton], _ p: PlayTheme.Palette) -> some View {
        ZStack(alignment: .topLeading) {
            ForEach(Array(list.enumerated()), id: \.offset) { i, b in
                let r = w.key(i), id = MidiHub.target(b)
                MidiButtonOutline(mark: midi.mark(id), radius: (w.radius + 3) * u, color: p.led.color, snapshot: snapshot)
                    .frame(width: (r.width + 6) * u, height: (r.height + 6) * u)
                    .contentShape(Rectangle())
                    .onTapGesture { midi.pick(id) }
                    .accessibilityLabel("Learn \(PlayInput.name(b))")
                    .accessibilityAddTraits(.isButton)
                    .offset(x: (w.x + r.minX - 3) * u, y: (w.y + r.minY - 3) * u)
            }
        }
    }

    /// MASTER's pickup mark: a hollow dot at the pot's place on a ring r = knob / 2 + 5.
    private func potMark(_ k: PlayLayout.Knob, _ frac: Double, _ p: PlayTheme.Palette) -> some View {
        let a = (-135 + frac * 270) * .pi / 180, r = k.size / 2 + 5
        let x = k.cx + r * sin(a), y = k.top + k.size / 2 - r * cos(a)
        return Circle().strokeBorder(p.label.color.opacity(0.75), lineWidth: 1.5 * u)
            .frame(width: 5.2 * u, height: 5.2 * u)
            .offset(x: (x - 2.6) * u, y: (y - 2.6) * u)
            .allowsHitTesting(false)
    }

    // MARK: the bar

    private var bar: some View {
        HStack(spacing: 12 * u) {
            MidiBreathingDot(size: 9 * u, snapshot: snapshot)
            VStack(alignment: .leading, spacing: 3 * u) {
                Text("MIDI LEARN").petch(11 * u, em: 0.14).foregroundStyle(RGB(0xFF8A4C).color)
                Text(midi.learnLine).dm(13.5 * u, 500).foregroundStyle(SheetInk.text.color)
                    .lineLimit(2).fixedSize(horizontal: false, vertical: true)
            }
            .frame(maxWidth: .infinity, alignment: .leading)
            if midi.bound != nil && midi.canForget {
                barPill("Forget", accent: false) { midi.forget() }
            }
            barPill("Done", accent: true) { midi.doneLearn() }
        }
        .padding(.leading, 16 * u).padding(.trailing, 8 * u).padding(.vertical, 8 * u)
        .background(RoundedRectangle(cornerRadius: 18 * u).fill(SheetInk.bg.color)
            .shadow(color: .black.opacity(0.45), radius: 18 * u, y: 14 * u))
        .overlay(RoundedRectangle(cornerRadius: 18 * u).strokeBorder(RGB(0xFF8A4C).color, lineWidth: 1))
    }

    private func barPill(_ t: String, accent: Bool, _ go: @escaping () -> Void) -> some View {
        Button(action: go) {
            Text(t).dm(13 * u, 700).foregroundStyle((accent ? SheetInk.bg : SheetInk.text).color)
                .padding(.horizontal, 14 * u).frame(height: 36 * u)
                .background(Capsule().fill(accent ? RGB(0xFF8A4C).color : SheetInk.tile.color))
                .contentShape(Capsule())
        }
        .buttonStyle(PressStyle())
    }
}

/// A knob's learn ring: dashed (learnable), solid with a dot (bound), thick, glowing and pulsing (picked).
struct MidiKnobRing: View {
    let mark: MidiHub.Mark
    let radius: CGFloat
    let color: Color
    let body_: Color
    let snapshot: Bool
    @State private var pulse = false

    init(mark: MidiHub.Mark, radius: CGFloat, color: Color, body: Color, snapshot: Bool) {
        self.mark = mark; self.radius = radius; self.color = color; body_ = body; self.snapshot = snapshot
    }

    var body: some View {
        let k = radius / 29                 // (the mock-up's rings: r 29 on a 46 knob)
        let lw: CGFloat = switch mark {
        case .picked: (pulse ? 4 : 2.5) * k
        case .bound: 2 * k
        case .learnable: 1.5 * k
        }
        ZStack {
            Circle()
                .stroke(color.opacity(mark == .picked ? (pulse ? 1 : 0.6) : mark == .bound ? 0.95 : 0.6),
                        style: StrokeStyle(lineWidth: lw, dash: mark == .learnable ? [4 * k, 4 * k] : []))
                .padding(lw / 2)
                .shadow(color: mark == .picked ? color : .clear, radius: 5 * k)
            if mark == .bound {
                Circle().fill(body_).frame(width: 9 * k, height: 9 * k)
                    .overlay(Circle().fill(color).frame(width: 6 * k, height: 6 * k))
                    .offset(x: radius * 0.71, y: -radius * 0.71)
            }
        }
        .onAppear { if mark == .picked && !snapshot { start() } }
        .onChange(of: mark) { _, m in if m == .picked && !snapshot { start() } else { pulse = false } }
    }

    private func start() {
        pulse = false
        withAnimation(.easeInOut(duration: 0.6).repeatForever(autoreverses: true)) { pulse = true }
    }
}

/// A button's learn outline in the LED colour, 3 pt out.
struct MidiButtonOutline: View {
    let mark: MidiHub.Mark
    let radius: CGFloat
    let color: Color
    let snapshot: Bool
    @State private var pulse = false

    var body: some View {
        let k = radius / 13
        let s = RoundedRectangle(cornerRadius: radius)
        Group {
            switch mark {
            case .learnable:
                s.strokeBorder(color.opacity(0.55), style: StrokeStyle(lineWidth: 1.2 * k, dash: [4 * k, 3 * k]))
            case .bound:
                s.strokeBorder(color.opacity(0.95), lineWidth: 2 * k)
            case .picked:
                s.strokeBorder(color, lineWidth: 3 * k).shadow(color: color, radius: (pulse ? 10 : 5) * k)
            }
        }
        .onAppear { if mark == .picked && !snapshot { start() } }
        .onChange(of: mark) { _, m in if m == .picked && !snapshot { start() } else { pulse = false } }
    }

    private func start() {
        pulse = false
        withAnimation(.easeInOut(duration: 0.6).repeatForever(autoreverses: true)) { pulse = true }
    }
}

/// The bar's dot, breathing.
struct MidiBreathingDot: View {
    let size: CGFloat
    let snapshot: Bool
    @State private var on = false
    var body: some View {
        Circle().fill(RGB(0xFF8A4C).color).frame(width: size, height: size)
            .shadow(color: RGB(0xFF8A4C).color, radius: size * 0.6)
            .opacity(on ? 0.45 : 1)
            .onAppear {
                guard !snapshot else { return }
                withAnimation(.easeInOut(duration: 0.6).repeatForever(autoreverses: true)) { on = true }
            }
    }
}

/// A moment's message over the panel (MASTER's pickup, the OMX-27).
struct MidiToast: View {
    let text: String
    let ok: Bool
    let u: CGFloat
    var body: some View {
        Text(text).dm(13 * u, 500).foregroundStyle(SheetInk.text.color).multilineTextAlignment(.center)
            .padding(.horizontal, 16 * u).padding(.vertical, 10 * u)
            .background(RoundedRectangle(cornerRadius: 14 * u).fill(SheetInk.bg.color.opacity(0.97))
                .shadow(color: .black.opacity(0.45), radius: 14 * u, y: 8 * u))
            .overlay(RoundedRectangle(cornerRadius: 14 * u)
                .strokeBorder(ok ? RGB(0x3BD47A).color.opacity(0.55) : SheetInk.line.color, lineWidth: 1))
            .frame(maxWidth: 358 * u)
            .fixedSize(horizontal: false, vertical: true)
            .allowsHitTesting(false)
    }
}
