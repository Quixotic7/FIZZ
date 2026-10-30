// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// AMP's mix (mock-up 45; editor.js mixTiles): in LEVEL's card, PAN (L64 .. C .. R63: the bar fills from the centre to
// the knob; a double tap snaps it to C) and the three sends, CHO / DEL / REV (0..127, each in its own colour: chorus
// cyan, delay yellow, reverb violet; dim at 0). Each is dragged sideways; on the track each change is PARAM 115..118
// (EditorModel.setMix; an older FIZZ: the whole sound, 120 ms after the last change). A MIDI sound has none.

import SwiftUI
import FizzData

/// LEVEL, PAN and the sends: AMP's group under the envelope.
struct AmpMixCard: View {
    let e: EditorModel

    var body: some View {
        let mix = e.table?.info(EditMix.pan) != nil && e.sound?.isMidi != true
        VStack(alignment: .leading, spacing: 10) {
            EditBar(e: e, id: QV.level, label: "LEVEL", tint: FZ.k3)
            if mix {
                PanBar(e: e)
                HStack(spacing: 8) {
                    SendTile(e: e, id: EditMix.chorus, label: "CHO", tint: FZ.cyan)
                    SendTile(e: e, id: EditMix.delay, label: "DEL", tint: FZ.yellow)
                    SendTile(e: e, id: EditMix.reverb, label: "REV", tint: FZ.violet)
                }
            }
        }
        .padding(12)
        .frame(maxWidth: .infinity, alignment: .leading)
        .background(RoundedRectangle(cornerRadius: 16).fill(FZ.panel.color))
        .overlay(RoundedRectangle(cornerRadius: 16).strokeBorder(FZ.k3.mix(25, FZ.line).color, lineWidth: 1.5))
    }
}

/// PAN: L .. C .. R over a line with ticks, the fill from the centre to the knob, the value (L12, C, R63) big.
struct PanBar: View {
    let e: EditorModel

    var body: some View {
        let id = EditMix.pan, hot = e.turning?.id == id
        let f = e.table.map { $0.frac(id, e.value(id)) } ?? 0.5, z = e.table.map { $0.frac(id, 0) } ?? 0.5
        HStack(spacing: 12) {
            Text("PAN").fz(12, 800, em: 0.08).foregroundStyle(FZ.text.color)
            GeometryReader { g in
                let w = g.size.width, y = g.size.height * 0.62
                ZStack(alignment: .topLeading) {
                    HStack {
                        Text("L"); Spacer(); Text("C"); Spacer(); Text("R")
                    }
                    .fz(11, 800).foregroundStyle(FZ.muted.color)
                    .padding(.horizontal, -3)
                    Canvas { ctx, sz in
                        var line = Path()
                        line.move(to: CGPoint(x: 0, y: y)); line.addLine(to: CGPoint(x: sz.width, y: y))
                        ctx.stroke(line, with: .color(FZ.dim.color), lineWidth: 2)
                        for k in 0...4 {
                            let x = sz.width * CGFloat(k) / 4
                            var t = Path()
                            t.move(to: CGPoint(x: x, y: y - (k == 2 ? 8 : 5))); t.addLine(to: CGPoint(x: x, y: y + (k == 2 ? 8 : 5)))
                            ctx.stroke(t, with: .color(FZ.dim.color), lineWidth: 2)
                        }
                        if abs(f - z) > 0.002 {
                            var fill = Path()
                            fill.move(to: CGPoint(x: sz.width * z, y: y)); fill.addLine(to: CGPoint(x: sz.width * f, y: y))
                            ctx.stroke(fill, with: .color(FZ.text.color), style: StrokeStyle(lineWidth: 4, lineCap: .round))
                        }
                    }
                    Circle().fill(FZ.text.color)
                        .frame(width: 18, height: 18)
                        .background(Circle().fill(FZ.text.opacity(hot ? 0.22 : 0.1)).frame(width: 38, height: 38))
                        .overlay(Circle().strokeBorder(FZ.text.opacity(hot ? 0.7 : 0.35), lineWidth: 2).frame(width: 38, height: 38))
                        .position(x: w * f, y: y)
                }
            }
            .frame(height: 40)
            Text(e.text(id)).fz(22, 800).monospacedDigit().foregroundStyle(FZ.text.color)
                .frame(minWidth: 52, alignment: .trailing)
        }
        .padding(.horizontal, 12).frame(height: 58)
        .background(RoundedRectangle(cornerRadius: 12).fill(FZ.text.mix(hot ? 10 : 3, FZ.panel).color))
        .overlay(RoundedRectangle(cornerRadius: 12).strokeBorder(FZ.text.mix(hot ? 90 : 45, FZ.line).color, lineWidth: 1.5))
        .sensoryFeedback(.selection, trigger: e.value(id))
        .turns(e, x: id, label: "PAN", tint: FZ.text, xSpan: 280)
        .turnDim(e, [id])
    }
}

/// A send (CHO, DEL, REV): its name in its colour, the value, a bar; dim at 0.
struct SendTile: View {
    let e: EditorModel
    let id: Int
    let label: String
    let tint: RGB

    var body: some View {
        let v = e.value(id), hot = e.turning?.id == id, off = v == 0 && !hot
        let f = e.table.map { $0.frac(id, v) } ?? 0
        VStack(alignment: .leading, spacing: 8) {
            HStack(alignment: .firstTextBaseline) {
                Text(label).fz(12, 800, em: 0.08).foregroundStyle(tint.color)
                Spacer(minLength: 2)
                Text(e.text(id)).fz(20, 800).monospacedDigit().foregroundStyle(off ? FZ.muted.color : FZ.text.color)
            }
            GeometryReader { g in
                ZStack(alignment: .leading) {
                    Capsule().fill(RGB(0x1d1d23).color)
                    if v > 0 { Capsule().fill(tint.color).frame(width: max(8, g.size.width * f)) }
                }
            }
            .frame(height: 8)
        }
        .padding(.horizontal, 10).padding(.vertical, 9)
        .frame(maxWidth: .infinity)
        .background(RoundedRectangle(cornerRadius: 12).fill(tint.mix(hot ? 22 : off ? 4 : 10, FZ.panel).color))
        .overlay(RoundedRectangle(cornerRadius: 12).strokeBorder(tint.mix(hot ? 100 : off ? 25 : 55, FZ.line).color, lineWidth: 1.5))
        .shadow(color: hot ? tint.opacity(0.4) : .clear, radius: 8)
        .opacity(off ? 0.55 : 1)
        .sensoryFeedback(.selection, trigger: v)
        .turns(e, x: id, label: label == "CHO" ? "CHORUS" : label == "DEL" ? "DELAY" : "REVERB", tint: tint, xSpan: 220)
        .turnDim(e, [id])
    }
}
