// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// The mock-ups' small line drawings (their SVG icons) as SwiftUI paths: a tiny SVG path reader (M L H V C S Q Z,
// absolute and relative) and the icons themselves.

import SwiftUI

public enum SVGPath {
    /// Path data in its own coordinates.
    public static func parse(_ d: String) -> Path {
        var p = Path()
        let toks = tokens(d)
        var i = 0, cmd: Character = "M"
        var cur = CGPoint.zero, start = CGPoint.zero, lastCtl: CGPoint?
        func num() -> CGFloat {
            guard i < toks.count, case .num(let v) = toks[i] else { return 0 }
            i += 1
            return v
        }
        func hasNum() -> Bool { i < toks.count && { if case .num = toks[i] { true } else { false } }() }
        while i < toks.count {
            if case .cmd(let c) = toks[i] { cmd = c; i += 1 }
            let rel = cmd.isLowercase
            let o = rel ? cur : .zero
            switch cmd.uppercased().first! {
            case "M":
                cur = CGPoint(x: o.x + num(), y: o.y + num())
                p.move(to: cur); start = cur; lastCtl = nil
                cmd = rel ? "l" : "L"           // more pairs after M are lines
            case "L":
                cur = CGPoint(x: o.x + num(), y: o.y + num()); p.addLine(to: cur); lastCtl = nil
            case "H":
                cur = CGPoint(x: (rel ? cur.x : 0) + num(), y: cur.y); p.addLine(to: cur); lastCtl = nil
            case "V":
                cur = CGPoint(x: cur.x, y: (rel ? cur.y : 0) + num()); p.addLine(to: cur); lastCtl = nil
            case "C":
                let c1 = CGPoint(x: o.x + num(), y: o.y + num()), c2 = CGPoint(x: o.x + num(), y: o.y + num())
                cur = CGPoint(x: o.x + num(), y: o.y + num())
                p.addCurve(to: cur, control1: c1, control2: c2); lastCtl = c2
            case "S":
                let c1 = lastCtl.map { CGPoint(x: 2 * cur.x - $0.x, y: 2 * cur.y - $0.y) } ?? cur
                let c2 = CGPoint(x: o.x + num(), y: o.y + num())
                cur = CGPoint(x: o.x + num(), y: o.y + num())
                p.addCurve(to: cur, control1: c1, control2: c2); lastCtl = c2
            case "Q":
                let c = CGPoint(x: o.x + num(), y: o.y + num())
                cur = CGPoint(x: o.x + num(), y: o.y + num())
                p.addQuadCurve(to: cur, control: c); lastCtl = nil
            case "Z":
                p.closeSubpath(); cur = start; lastCtl = nil
                if hasNum() { cmd = "L" }
            default:
                i += 1
            }
        }
        return p
    }

    enum Tok { case cmd(Character), num(CGFloat) }

    static func tokens(_ d: String) -> [Tok] {
        var out: [Tok] = []
        var buf = ""
        func flush() { if let v = Double(buf) { out.append(.num(CGFloat(v))) }; buf = "" }
        for ch in d {
            if ch.isLetter && ch != "e" && ch != "E" { flush(); out.append(.cmd(ch)) }
            else if ch == "-" && !buf.isEmpty && buf.last != "e" { flush(); buf = "-" }
            else if ch == "." && buf.contains(".") { flush(); buf = "." }
            else if ch == "," || ch == " " || ch == "\n" || ch == "\t" { flush() }
            else { buf.append(ch) }
        }
        flush()
        return out
    }
}

/// A line drawing in a view box: strokes, with optional fills.
public struct VectorArt: Sendable {
    public enum El: Sendable {
        case path(String)
        case rect(CGFloat, CGFloat, CGFloat, CGFloat, CGFloat)     // x y w h rx
        case circle(CGFloat, CGFloat, CGFloat)                     // cx cy r
        case filledCircle(CGFloat, CGFloat, CGFloat)               // filled with the backdrop (covers lines)
    }
    public let box: CGSize
    public let els: [El]

    public init(_ w: CGFloat, _ h: CGFloat, _ els: [El]) { box = CGSize(width: w, height: h); self.els = els }

    func path(_ e: El) -> Path {
        switch e {
        case .path(let d): SVGPath.parse(d)
        case .rect(let x, let y, let w, let h, let r):
            Path(roundedRect: CGRect(x: x, y: y, width: w, height: h), cornerRadius: r)
        case .circle(let x, let y, let r), .filledCircle(let x, let y, let r):
            Path(ellipseIn: CGRect(x: x - r, y: y - r, width: 2 * r, height: 2 * r))
        }
    }

    /// Draws into a context: scaled to fit `size`, centred.
    func draw(_ ctx: GraphicsContext, size: CGSize, color: Color, lineWidth: CGFloat, backdrop: Color = .black,
              fill: Bool = false) {
        let s = min(size.width / box.width, size.height / box.height)
        var c = ctx
        c.translateBy(x: (size.width - box.width * s) / 2, y: (size.height - box.height * s) / 2)
        c.scaleBy(x: s, y: s)
        let style = StrokeStyle(lineWidth: lineWidth, lineCap: .round, lineJoin: .round)
        for e in els {
            let p = path(e)
            if case .filledCircle = e { c.fill(p, with: .color(backdrop)) }
            if fill { c.fill(p, with: .color(color)) } else { c.stroke(p, with: .color(color), style: style) }
        }
    }
}

/// An icon: its art stroked in a colour.
public struct IconView: View {
    let art: VectorArt
    let color: Color
    let lineWidth: CGFloat
    let backdrop: Color
    let fill: Bool

    public init(_ art: VectorArt, color: Color, lineWidth: CGFloat = 2.2, backdrop: Color = .black, fill: Bool = false) {
        self.art = art; self.color = color; self.lineWidth = lineWidth; self.backdrop = backdrop; self.fill = fill
    }

    public var body: some View {
        Canvas { ctx, size in
            art.draw(ctx, size: size, color: color, lineWidth: lineWidth, backdrop: backdrop, fill: fill)
        }
    }
}

/// The mock-ups' icons (fizz-ios.html IC), 24 x 24.
public enum Icons {
    public static let backup = VectorArt(24, 24, [.rect(2.5, 6, 19, 12.5, 3), .circle(8.5, 12.2, 2.2), .circle(15.5, 12.2, 2.2)])
    public static let library = VectorArt(24, 24, [.path("M3 5.5h3M9 5.5h12M3 12h3M9 12h12M3 18.5h3M9 18.5h12")])
    public static let edit = VectorArt(24, 24, [.path("M6 3v18M12 3v18M18 3v18"), .filledCircle(6, 15, 2.6),
                                                .filledCircle(12, 8, 2.6), .filledCircle(18, 13, 2.6)])
    public static let pool = VectorArt(24, 24, [.rect(3, 3, 7.5, 7.5, 2), .rect(13.5, 3, 7.5, 7.5, 2),
                                                .rect(3, 13.5, 7.5, 7.5, 2), .rect(13.5, 13.5, 7.5, 7.5, 2)])
    public static let projects = VectorArt(24, 24, [.rect(2.5, 4.5, 19, 15, 3),
                                                    .path("M7 9.5h1M11.5 9.5h1M16 9.5h1M7 14.5h1M11.5 14.5h1")])
    public static let plus = VectorArt(24, 24, [.path("M12 5v14M5 12h14")])
    public static let share = VectorArt(24, 24, [.path("M12 3v12M8 7l4-4 4 4"), .path("M6 11H5v10h14V11h-1")])
    public static let bluetooth = VectorArt(24, 24, [.path("M6.5 7l11 10-5.5 4.5v-19L17.5 7l-11 10")])
    public static let tap = VectorArt(24, 24, [.path("M5 12.5l5 5L19.5 7")])
    public static let down = VectorArt(24, 24, [.path("M12 4v14M6 12l6 6 6-6")])
    public static let trash = VectorArt(24, 24, [.path("M4 6.5h16M9.5 6.5V4h5v2.5M6.5 6.5l1 14h9l1-14")])
    /// a folder, an arrow into it (open a file)
    public static let open = VectorArt(24, 24, [.path("M3 5.5h6l2 2.5h10v11.5H3z"), .path("M12 10.5v6.5M9.3 14.3l2.7 2.7 2.7-2.7")])
    /// the pool chip's four squares (12 x 12, filled)
    public static let poolChip = VectorArt(12, 12, [.rect(0, 0, 5, 5, 1.5), .rect(7, 0, 5, 5, 1.5),
                                                    .rect(0, 7, 5, 5, 1.5), .rect(7, 7, 5, 5, 1.5)])
    /// the bubble logo (32 x 32): cyan, pink, yellow rings
    public static let bubbles: [(CGFloat, CGFloat, CGFloat, RGB)] = [(11, 20, 7, FZ.cyan), (22, 10, 5, FZ.pink),
                                                                     (23, 24, 3, FZ.yellow)]
}

/// The FIZZ bubble logo.
public struct BubbleLogo: View {
    var lineWidth: CGFloat = 2.4
    public init(lineWidth: CGFloat = 2.4) { self.lineWidth = lineWidth }
    public var body: some View {
        Canvas { ctx, size in
            let s = min(size.width, size.height) / 32
            for (x, y, r, c) in Icons.bubbles {
                let rect = CGRect(x: (x - r) * s, y: (y - r) * s, width: 2 * r * s, height: 2 * r * s)
                ctx.stroke(Path(ellipseIn: rect), with: .color(c.color), lineWidth: lineWidth * s)
            }
        }
    }
}

/// The FIZZ word in its gradient.
public struct FizzWord: View {
    let size: CGFloat
    public init(size: CGFloat = 26) { self.size = size }
    public var body: some View {
        Text("FIZZ").fz(size, 800, em: 0.06)
            .foregroundStyle(LinearGradient(gradient: FZ.word, startPoint: .leading, endPoint: .trailing))
            .fixedSize()
    }
}
