// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// The drawings: the algorithm glyph of a preset row (site/manager/ui.js algo) and the FM-1 as a line drawing
// (fizz-ios.html fm1(), a 480 x 325 view box).

import SwiftUI

/// Four operators, the carriers below (ui.js algo: the glyph by the algorithm's index mod 4).
public struct AlgoGlyph: View {
    static let points: [[CGPoint]] = [
        [CGPoint(x: 6, y: 6), CGPoint(x: 24, y: 6), CGPoint(x: 6, y: 24), CGPoint(x: 24, y: 24)],
        [CGPoint(x: 15, y: 5), CGPoint(x: 6, y: 24), CGPoint(x: 15, y: 15), CGPoint(x: 24, y: 24)],
        [CGPoint(x: 6, y: 6), CGPoint(x: 15, y: 15), CGPoint(x: 24, y: 6), CGPoint(x: 15, y: 26)],
        [CGPoint(x: 6, y: 15), CGPoint(x: 15, y: 6), CGPoint(x: 24, y: 15), CGPoint(x: 15, y: 25)],
    ]
    /// the algorithm, 1..8 (blob[2] + 1)
    let algorithm: Int
    let color: Color

    public init(algorithm: Int, color: Color) { self.algorithm = algorithm; self.color = color }

    public var body: some View {
        Canvas { ctx, size in
            let s = min(size.width, size.height) / 30
            let P = Self.points[((algorithm - 1) % 4 + 4) % 4].map { CGPoint(x: $0.x * s, y: $0.y * s) }
            for (a, b) in [(0, 2), (1, 3), (0, 1)] {
                var l = Path()
                l.move(to: P[a]); l.addLine(to: P[b])
                ctx.stroke(l, with: .color(color.opacity(0.55)), lineWidth: 2 * s)
            }
            for (i, p) in P.enumerated() {
                let r = (i >= 2 ? 4.2 : 3.2) * s
                let c = Path(ellipseIn: CGRect(x: p.x - r, y: p.y - r, width: 2 * r, height: 2 * r))
                if i >= 2 { ctx.fill(c, with: .color(color)) }
                ctx.stroke(c, with: .color(color), lineWidth: 2 * s)
            }
        }
    }
}

/// What the drawn FM-1's screen shows.
public enum FM1Screen: Sendable, Equatable {
    case bubbles
    case update(Double)       // the arrow dropping, phase 0..1
    case question
    case text(String)
}

/// The FM-1, drawn: knobs, the screen, the 12 keys, the keybed.
public struct FM1Drawing: View {
    let color: RGB
    let screen: FM1Screen
    public init(color: RGB, screen: FM1Screen = .bubbles) { self.color = color; self.screen = screen }

    public static let aspect: CGFloat = 480 / 325

    public var body: some View {
        Canvas { ctx, size in draw(ctx, size) }
            .aspectRatio(Self.aspect, contentMode: .fit)
    }

    private func draw(_ ctx0: GraphicsContext, _ size: CGSize) {
        let s = min(size.width / 480, size.height / 325)
        var ctx = ctx0
        ctx.scaleBy(x: s, y: s)
        let c = color.color
        let line = StrokeStyle(lineWidth: 2.4, lineCap: .round, lineJoin: .round)
        func rr(_ x: CGFloat, _ y: CGFloat, _ w: CGFloat, _ h: CGFloat, _ r: CGFloat) -> Path {
            Path(roundedRect: CGRect(x: x, y: y, width: w, height: h), cornerRadius: r)
        }
        // the body
        let body = rr(3, 3, 474, 319, 26)
        ctx.fill(body, with: .color(color.mix(6, RGB(0x050507)).color))
        ctx.stroke(body, with: .color(c), lineWidth: 2.8)
        // knobs
        var knobs = Path()
        func knob(_ x: CGFloat, _ y: CGFloat, _ r: CGFloat = 13) {
            knobs.addEllipse(in: CGRect(x: x - r, y: y - r, width: 2 * r, height: 2 * r))
            knobs.move(to: CGPoint(x: x, y: y - r + 3)); knobs.addLine(to: CGPoint(x: x, y: y - r + 10))
        }
        for (x, y) in [(37.0, 48.0), (90, 48), (37, 108), (90, 108)] { knob(x, y) }
        for x in [287.0, 339, 391, 443] { knob(x, 48) }
        knobs.addPath(rr(22, 146, 32, 14, 5)); knobs.addPath(rr(74, 146, 32, 14, 5))
        var lit = Path()
        for y in [110.0, 148] {
            for (i, x) in [284.0, 317, 350, 383, 416, 449].enumerated() {
                let r = rr(x - 13, y - 10, 26, 20, 5)
                knobs.addPath(r)
                if y == 148 && i >= 4 { lit.addPath(r) }
            }
        }
        ctx.fill(lit, with: .color(c.opacity(0.9 * 0.35)))
        ctx.stroke(knobs, with: .color(c.opacity(0.75)), style: line)
        // the keybed
        var sep = Path()
        sep.move(to: CGPoint(x: 3, y: 190)); sep.addLine(to: CGPoint(x: 477, y: 190))
        ctx.stroke(sep, with: .color(c.opacity(0.45)), style: line)
        var kb = Path()
        for x in [37.0, 66, 95, 153, 182, 240, 269, 298, 356, 385, 443] { kb.addPath(rr(x - 12, 198, 24, 40, 7)) }
        for i in 0..<16 { kb.addPath(rr(12 + CGFloat(i) * 29, 248, 24, 64, 7)) }
        ctx.stroke(kb, with: .color(c.opacity(0.55)), style: line)
        // the screen
        let lcd = rr(126, 22, 126, 126, 6)
        ctx.fill(lcd, with: .color(.black))
        ctx.stroke(lcd, with: .color(c), lineWidth: 3)
        var sc = ctx
        sc.translateBy(x: 126, y: 22)
        switch screen {
        case .bubbles:
            for (x, y, r, col) in [(48.0, 74.0, 22.0, FZ.cyan), (82, 42, 14, FZ.pink), (86, 88, 9, FZ.yellow)] {
                sc.stroke(Path(ellipseIn: CGRect(x: x - r, y: y - r, width: 2 * r, height: 2 * r)),
                          with: .color(col.color), lineWidth: 5)
            }
        case .update(let ph):
            // drop: from -26 (hidden) to +8, fading in then out (fizz-ios.html @keyframes drop)
            let dy = ph < 0.8 ? -26 + 34 * ph / 0.8 : 8
            let op = ph < 0.3 ? ph / 0.3 : ph < 0.8 ? 1 : 1 - (ph - 0.8) / 0.2
            var a = sc
            a.translateBy(x: 0, y: dy)
            a.opacity = op
            let style = StrokeStyle(lineWidth: 9, lineCap: .round, lineJoin: .round)
            a.stroke(SVGPath.parse("M63 24v56M40 58l23 23 23-23"), with: .color(FZ.orange.color), style: style)
            sc.stroke(SVGPath.parse("M34 102h58"), with: .color(FZ.orange.color), style: style)
        case .question:
            sc.draw(Text("?").font(FizzFont.font(80)).foregroundStyle(FZ.red.color), at: CGPoint(x: 63, y: 70))
        case .text(let t):
            sc.draw(Text(t).font(FizzFont.font(30)).foregroundStyle(c), at: CGPoint(x: 63, y: 66))
        }
    }
}
