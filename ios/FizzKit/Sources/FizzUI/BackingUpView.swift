// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// Backing up (mock-up 07, manager.js progOpen / progSet): full screen over the tabs. The % counts up, the store's
// sectors fill cream (MY PRESETS) then green (projects) as each is read whole, the one being read ringed; the time
// runs; bubbles rise; CANCEL keeps nothing.

import SwiftUI
import FizzData
import FizzKit

public struct BackingUpView: View {
    let model: UnitModel
    let run: BackupRun
    @Environment(\.fizzSnapshot) private var snapshot
    @Environment(\.fizzInsets) private var insets

    public init(model: UnitModel, run: BackupRun) {
        self.model = model
        self.run = run
    }

    public var body: some View {
        ZStack(alignment: .top) {
            Color.black.ignoresSafeArea()
            BubbleField(count: 12, seed: 13).ignoresSafeArea()
            VStack(spacing: 0) {
                TopBar(model: model)
                VStack(spacing: 0) {
                    Risers().frame(width: 220, height: 120).padding(.top, 20)
                    HStack(alignment: .firstTextBaseline, spacing: 0) {
                        Text("\(Int((min(1, max(0, run.fraction)) * 100).rounded(.down)))").fz(132, 800, em: -0.03)
                            .monospacedDigit()
                            // (no digit roll: the % changes every ~60 ms, a 0.25 s roll kept it blurred all the time)
                        Text("%").fz(62, 800)
                    }
                    .foregroundStyle(FZ.yellow.color)
                    .shadow(color: FZ.yellow.opacity(0.35), radius: 18)
                    .lineLimit(1)
                    .padding(.top, 4)
                    Text(run.kind == .presets ? "PRESETS" : "BACKING UP").fz(15, 800, em: 0.14)
                        .foregroundStyle(FZ.yellow.color)
                        .padding(.top, 14)
                    SectorGrid(run: run).padding(.top, 26)
                    HStack(spacing: 16) {
                        Legend(color: FZ.cream, text: "MY PRESETS")
                        if run.kind == .full { Legend(color: FZ.green, text: "PROJECTS") }
                        Spacer(minLength: 0)
                        Animated(still: 0) { t in
                            let now = snapshot ? Date() : Date(timeIntervalSinceReferenceDate: t)
                            Text(mmss(now.timeIntervalSince(run.started))).fz(18, 800).foregroundStyle(FZ.text.color)
                                .monospacedDigit()
                        }
                    }
                    .padding(.top, 14)
                    PillButton(title: "CANCEL", ghost: true, height: 52, size: 16) { model.backups.cancel() }
                        .frame(width: 180)
                        .padding(.top, 30)
                }
                .padding(.horizontal, 16)
                .frame(maxWidth: 560)
                Spacer(minLength: 0)
            }
            .padding(.top, insets.top)
        }
    }
}

/// The region's sectors, 12 a row (16 for a big store), MY PRESETS' first.
struct SectorGrid: View {
    let run: BackupRun

    var body: some View {
        let n = run.lit.count, cols = n > 120 ? 16 : 12, h: CGFloat = n > 120 ? 12 : 24, gap: CGFloat = n > 120 ? 3 : 5
        VStack(spacing: gap) {
            ForEach(0..<((n + cols - 1) / cols), id: \.self) { r in
                HStack(spacing: gap) {
                    ForEach(0..<cols, id: \.self) { c in
                        let k = r * cols + c
                        if k < n { cell(k, h) } else { Color.clear.frame(height: h) }
                    }
                }
            }
        }
        .animation(.easeOut(duration: 0.25), value: run.lit)
        .animation(.easeOut(duration: 0.2), value: run.now)
    }

    @ViewBuilder private func cell(_ k: Int, _ h: CGFloat) -> some View {
        let lit = run.lit[k], now = run.now == k
        let fill = lit ? (k < run.libSectors ? FZ.cream : FZ.green).color
            : now ? FZ.yellow.mix(30, RGB(0x18181d)).color : RGB(0x18181d).color
        RoundedRectangle(cornerRadius: h / 4)
            .fill(fill)
            .frame(maxWidth: .infinity).frame(height: h)
            .overlay(RoundedRectangle(cornerRadius: h / 4).strokeBorder(FZ.yellow.color, lineWidth: now ? 2 : 0))
            .shadow(color: now ? FZ.yellow.color : .clear, radius: 7)
    }
}

/// Five rings rising and fading (fizz-ios.html .risers, @keyframes rise2).
struct Risers: View {
    static let rings: [(x: CGFloat, y: CGFloat, r: CGFloat, c: RGB, delay: Double)] = [
        (70, 60, 26, FZ.cyan, 0), (130, 40, 18, FZ.pink, 0.7), (160, 76, 11, FZ.yellow, 1.3),
        (100, 96, 6, FZ.green, 1.9), (182, 26, 5, FZ.cream, 0.3),
    ]

    var body: some View {
        Animated(still: 1.1) { t in
            Canvas { ctx, size in
                let s = size.width / 220
                for g in Self.rings {
                    let p = ((t + g.delay).truncatingRemainder(dividingBy: 2.6)) / 2.6
                    let e = p * p                                      // (ease-in)
                    let dy = 30 - 70 * e
                    let op = p < 0.2 ? p / 0.2 : 1 - (p - 0.2) / 0.8
                    let rect = CGRect(x: (g.x - g.r) * s, y: (g.y - g.r + dy) * s, width: 2 * g.r * s, height: 2 * g.r * s)
                    ctx.stroke(Path(ellipseIn: rect), with: .color(g.c.opacity(op)), lineWidth: 3)
                }
            }
        }
        .allowsHitTesting(false)
    }
}
