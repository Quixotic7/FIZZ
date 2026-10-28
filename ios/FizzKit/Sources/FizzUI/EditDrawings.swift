// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// EDIT's drawings (editor.js algoSvg, adsr, openvPanel's env, curve, crushSvg, wavePath, the glide, the cables): each
// drawn in its viewBox's units as the page draws it, its handles dragged (a handle follows the finger: its value's
// whole range is the drawing's span), the modulation's sparks running and the feedback loop turning.

import SwiftUI
import FizzData

// MARK: - A drawing with handles

/// A handle in a drawing: its place (viewBox units), the values it moves sideways / up and down and the drawing's
/// units for each one's whole range.
struct DrawHandle {
    var at: CGPoint
    var xid: Int? = nil, yid: Int? = nil
    var xs: CGFloat = 300, ys: CGFloat = 120
    var tint: RGB = FZ.yellow
    var r: CGFloat = 9
    var label = ""
}

/// A drawing scaled to its width; a drag near a handle moves it.
struct HandleCanvas: View {
    let e: EditorModel
    let box: CGSize
    let handles: [DrawHandle]
    var still = 1.3
    let draw: (inout GraphicsContext, Double) -> Void
    @State private var grab: (k: Int, x0: Int, y0: Int)?

    var body: some View {
        GeometryReader { g in
            let s = g.size.width / box.width
            Animated(still: still) { t in
                Canvas { ctx, _ in
                    ctx.scaleBy(x: s, y: s)
                    draw(&ctx, t)
                    for (k, h) in handles.enumerated() {
                        let hot = grab?.k == k
                        let r = h.r * (hot ? 1.35 : 1)
                        let c = Path(ellipseIn: CGRect(x: h.at.x - r, y: h.at.y - r, width: 2 * r, height: 2 * r))
                        var gl = ctx
                        if hot { gl.addFilter(.shadow(color: h.tint.opacity(0.8), radius: 10 / s)) }
                        gl.fill(c, with: .color(h.tint.color))
                        ctx.stroke(c, with: .color(.black), lineWidth: 3)
                    }
                }
            }
            .contentShape(Rectangle())
            .onTapGesture(count: 2) { location in
                guard let k = nearest(location, s) else { return }
                if let x = handles[k].xid { e.reset(x) }
                if let y = handles[k].yid { e.reset(y) }
            }
            .highPriorityGesture(
                DragGesture(minimumDistance: 2)
                    .onChanged { d in
                        if grab == nil {
                            guard let k = nearest(d.startLocation, s) else { return }
                            let h = handles[k]
                            grab = (k, h.xid.map(e.value) ?? 0, h.yid.map(e.value) ?? 0)
                            if !e.locked { e.turning = EditTurn(id: h.yid ?? h.xid ?? 0, label: h.label, tint: h.tint) }
                        }
                        guard let gr = grab, handles.indices.contains(gr.k) else { return }
                        let h = handles[gr.k], dx = d.translation.width, dy = -d.translation.height
                        if let x = h.xid, let n = TurnModifier.turned(e, x, gr.x0, dx, h.xs * s), e.set(x, n), abs(dx) >= abs(dy) {
                            e.turning?.id = x
                        }
                        if let y = h.yid, let n = TurnModifier.turned(e, y, gr.y0, dy, h.ys * s), e.set(y, n), abs(dy) > abs(dx) {
                            e.turning?.id = y
                        }
                    }
                    .onEnded { _ in grab = nil; e.turning = nil }
            )
        }
        .aspectRatio(box.width / box.height, contentMode: .fit)
        .turnDim(e, handles.flatMap { [$0.xid, $0.yid].compactMap { $0 } })
    }

    /// The handle nearest a point (within 44 pt).
    func nearest(_ p: CGPoint, _ s: CGFloat) -> Int? {
        let q = CGPoint(x: p.x / s, y: p.y / s)
        let d = handles.enumerated().map { ($0.offset, hypot($0.element.at.x - q.x, $0.element.at.y - q.y)) }
        guard let b = d.min(by: { $0.1 < $1.1 }), b.1 * s < 44 else { return nil }
        return b.0
    }
}

func poly(_ pts: [CGPoint]) -> Path {
    var p = Path()
    for (i, q) in pts.enumerated() { if i == 0 { p.move(to: q) } else { p.addLine(to: q) } }
    return p
}

/// The area under a line and the line (editor.js .env .area / .line).
func strokeArea(_ ctx: inout GraphicsContext, _ line: Path, closeTo bottom: CGFloat? = nil, _ tint: RGB, width: CGFloat = 3) {
    var area = line
    if let bottom, let last = line.currentPoint {
        area.addLine(to: CGPoint(x: last.x, y: bottom))
    }
    area.closeSubpath()
    ctx.fill(area, with: .color(tint.opacity(0.16)))
    ctx.stroke(line, with: .color(tint.color), style: StrokeStyle(lineWidth: width, lineCap: .round, lineJoin: .round))
}

func label(_ ctx: inout GraphicsContext, _ s: String, _ at: CGPoint, size: CGFloat = 14, color: RGB = FZ.muted, anchor: UnitPoint = .leading) {
    ctx.draw(Text(s).font(FizzFont.font(size, 800)).foregroundColor(color.color), at: at, anchor: anchor)
}

// MARK: - OPERATORS: the algorithm

/// The algorithm drawn (editor.js algoSvg, the mock-ups' sizes): modulators above what they modulate, carriers on
/// the bus as thick as their MIX share, the modulation's sparks running, the feedback a turning loop; a circle dragged
/// up / down turns its RATIO, a tap lights it.
struct AlgoDrawing: View {
    let e: EditorModel
    /// the part of the 600 x 400 drawing shown
    var crop = CGRect(x: 0, y: 44, width: 600, height: 356)
    static let names = ["C", "A", "B1", "B2"]
    static let ratios = ["RC", "RA", "RB1", "RB2"]

    struct Layout { var x: [CGFloat], y: [CGFloat], dep: [Int] }
    static let R: CGFloat = 40, BUS: CGFloat = 372

    static func layout(_ a: EditAlgo) -> Layout {
        var dep = [0, 0, 0, 0]
        for _ in 0..<4 { for t in 0..<4 { for o in 0..<4 where o != t && (a.mods[t] >> o) & 1 == 1 && dep[o] < dep[t] + 1 { dep[o] = dep[t] + 1 } } }
        var n = [0, 0, 0, 0], row = [0, 0, 0, 0], md = 0
        for o in 0..<4 { row[o] = n[min(3, dep[o])]; n[min(3, dep[o])] += 1; md = max(md, dep[o]) }
        let sp: CGFloat = md >= 3 ? 82 : md == 2 ? 108 : 124
        var x = [CGFloat](repeating: 0, count: 4), y = x
        for o in 0..<4 {
            let d = min(3, dep[o]), cnt = n[d]
            x[o] = 340 + (CGFloat(row[o]) - CGFloat(cnt - 1) / 2) * 130 + (md >= 3 && cnt == 1 ? (d % 2 == 1 ? 34 : -34) : 0)
            y[o] = BUS - 66 - CGFloat(d) * sp
        }
        return Layout(x: x, y: y, dep: dep)
    }

    var body: some View {
        let t = e.table
        let al = max(1, min(8, e.value(0)))
        let a = t?.algos.indices.contains(al - 1) == true ? t!.algos[al - 1] : EditAlgo(mods: [0, 0, 0, 0], feedback: 4, x: 1, y: 0)
        let L = Self.layout(a)
        GeometryReader { g in
            let s = min(g.size.width / crop.width, g.size.height / crop.height)
            let ox = (g.size.width - crop.width * s) / 2 - crop.minX * s, oy = (g.size.height - crop.height * s) / 2 - crop.minY * s
            ZStack(alignment: .topLeading) {
                Animated(still: 0.55) { now in
                    Canvas { ctx, _ in
                        ctx.translateBy(x: ox, y: oy)
                        ctx.scaleBy(x: s, y: s)
                        draw(&ctx, a, L, now)
                    }
                }
                ForEach(0..<4, id: \.self) { o in
                    let rid = t?.id(Self.ratios[o]) ?? 0
                    Circle().fill(Color.white.opacity(0.001))
                        .frame(width: 2 * Self.R * s + 16, height: 2 * Self.R * s + 16)
                        .turns(e, y: rid, label: "\(Self.names[o]) RATIO", tint: FZ.operators[o], tap: { e.op = o })
                        .position(x: ox + L.x[o] * s, y: oy + L.y[o] * s)
                }
            }
        }
        .turnDim(e, [])
    }

    func draw(_ ctx: inout GraphicsContext, _ a: EditAlgo, _ L: Layout, _ now: Double) {
        let R = Self.R, BUS = Self.BUS
        let mx = Double(e.value(7)), sx = (63 - mx) / 127, sy = 1 - sx
        ctx.stroke(poly([CGPoint(x: 150, y: BUS), CGPoint(x: 520, y: BUS)]), with: .color(RGB(0x3a3a44).color), lineWidth: 3)
        ctx.stroke(SVGPath.parse("M528 \(BUS - 12)h12v24h-12zM540 \(BUS - 12)l16-14v52l-16-14"), with: .color(FZ.muted.color), lineWidth: 3)
        ctx.stroke(SVGPath.parse("M566 \(BUS - 16)C578 \(BUS - 8) 578 \(BUS + 8) 566 \(BUS + 16)"),
                   with: .color(FZ.cream.opacity(0.35 + 0.35 * breathe(now, 1.6))), lineWidth: 3)
        for o in 0..<4 where (a.out >> o) & 1 == 1 {          // a carrier into the bus, as thick as its MIX share
            let both = (a.x >> o) & 1 == 1 && (a.y >> o) & 1 == 1
            let sh = both ? 1 : (a.x >> o) & 1 == 1 ? sx : sy
            let lw = 2 + 6 * sh, cx = L.dep[o] > 0 ? L.x[o] + R + 10 : L.x[o]
            let p = L.dep[o] > 0 ? poly([CGPoint(x: L.x[o] + R, y: L.y[o]), CGPoint(x: cx, y: L.y[o]), CGPoint(x: cx, y: BUS)])
                : poly([CGPoint(x: cx, y: L.y[o] + R), CGPoint(x: cx, y: BUS)])
            ctx.stroke(p, with: .color(FZ.cream.opacity(0.35 + 0.65 * sh)), lineWidth: lw)
            let jr = 3 + 2 * sh
            ctx.fill(Path(ellipseIn: CGRect(x: cx - jr, y: BUS - jr, width: 2 * jr, height: 2 * jr)), with: .color(FZ.cream.color))
        }
        for t in 0..<4 { for o in 0..<4 where o != t && (a.mods[t] >> o) & 1 == 1 {   // o modulates t
            let dx = L.x[t] - L.x[o], dy = L.y[t] - L.y[o], l = hypot(dx, dy), ux = dx / l, uy = dy / l
            let s0 = CGPoint(x: L.x[o] + ux * (R + 2), y: L.y[o] + uy * (R + 2)), e0 = CGPoint(x: L.x[t] - ux * (R + 6), y: L.y[t] - uy * (R + 6))
            ctx.stroke(poly([s0, e0]), with: .color(FZ.muted.color), style: StrokeStyle(lineWidth: 4, lineCap: .round))
            var ar = Path()
            ar.move(to: e0)
            ar.addLine(to: CGPoint(x: e0.x - ux * 14 - uy * 8, y: e0.y - uy * 14 + ux * 8))
            ar.addLine(to: CGPoint(x: e0.x - ux * 14 + uy * 8, y: e0.y - uy * 14 - ux * 8))
            ar.closeSubpath()
            ctx.fill(ar, with: .color(FZ.muted.color))
            let dur = 1.1 + 0.15 * Double(o + t), f = (now / dur).truncatingRemainder(dividingBy: 1)
            let sp = CGPoint(x: s0.x + (e0.x - s0.x) * f, y: s0.y + (e0.y - s0.y) * f)
            ctx.fill(Path(ellipseIn: CGRect(x: sp.x - 6, y: sp.y - 6, width: 12, height: 12)), with: .color(FZ.cream.color))
        } }
        if a.feedback < 4 {                                   // the feedback loop, turning
            let fb = a.feedback
            var p = Path()
            let st = CGPoint(x: L.x[fb] + R * 0.7, y: L.y[fb] - R * 0.7), en = CGPoint(x: st.x + R * 0.5, y: st.y + R * 0.95)
            let c = CGPoint(x: (st.x + en.x) / 2 + 22, y: (st.y + en.y) / 2 - 14)
            p.addArc(center: c, radius: hypot(st.x - c.x, st.y - c.y), startAngle: .radians(atan2(st.y - c.y, st.x - c.x)),
                     endAngle: .radians(atan2(en.y - c.y, en.x - c.x)), clockwise: false)
            ctx.stroke(p, with: .color(FZ.operators[fb].color),
                       style: StrokeStyle(lineWidth: 4.5, dash: [9, 7], dashPhase: -32 * CGFloat((now / 1.2).truncatingRemainder(dividingBy: 1))))
        }
        for o in 0..<4 {
            let c = FZ.operators[o], on = e.op == o || e.turning?.id == e.table?.id(Self.ratios[o])
            let r = CGRect(x: L.x[o] - R, y: L.y[o] - R, width: 2 * R, height: 2 * R)
            var g = ctx
            if on { g.addFilter(.shadow(color: FZ.yellow.opacity(0.8), radius: 8)) }
            g.fill(Path(ellipseIn: r), with: .color(c.mix(22, FZ.panel).color))
            g.stroke(Path(ellipseIn: r), with: .color(on ? FZ.yellow.color : c.color), lineWidth: 5)
            ctx.draw(Text(Self.names[o]).font(FizzFont.font(32, 800)).foregroundColor(c.color), at: CGPoint(x: L.x[o], y: L.y[o]))
        }
    }
}

// MARK: - Envelopes

/// OP ENV's envelope (editor.js openvPanel env): DELAY a handle at the start, ATK to the top, DEC to END.
struct OpEnvDrawing: View {
    let e: EditorModel
    /// 0: operator A's; 1: B1 and B2's
    let k: Int

    var body: some View {
        let ids = k == 1 ? [16, 17, 18, 24] : [12, 13, 14, 20]      // ATK DEC END DLY
        let c = k == 1 ? FZ.k3 : FZ.k2
        let w: CGFloat = 420, h: CGFloat = 150, X = (w - 30) * 0.3 / 127, top: CGFloat = 18, bot = h - 12
        let Y = { (lv: Int) in bot - (bot - top) * CGFloat(lv) / 127 }
        let v = { (i: Int) in CGFloat(e.value(i)) }
        let p0 = CGPoint(x: 12 + v(ids[3]) * X, y: bot), p1 = CGPoint(x: p0.x + v(ids[0]) * X, y: top)
        let p2 = CGPoint(x: p1.x + v(ids[1]) * X, y: Y(e.value(ids[2]))), p3 = CGPoint(x: w - 12, y: Y(e.value(ids[2])))
        let name = k == 1 ? "ENV B" : "ENV A"
        HandleCanvas(e: e, box: CGSize(width: w, height: h), handles: [
            DrawHandle(at: p0, xid: ids[3], xs: 127 * X, tint: FZ.muted, r: 8, label: "\(name) DLY"),
            DrawHandle(at: p1, xid: ids[0], xs: 127 * X, tint: c, r: 10, label: "\(name) ATK"),
            DrawHandle(at: p2, xid: ids[1], yid: ids[2], xs: 127 * X, ys: bot - top, tint: FZ.yellow, r: 10, label: "\(name) DEC · END"),
        ]) { ctx, _ in
            strokeArea(&ctx, poly([CGPoint(x: 12, y: bot), p0, p1, p2, p3]), closeTo: bot, c)
        }
    }
}

/// An ADSR (or AHD) envelope (editor.js adsr): attack to the top, decay to the sustain, held, released.
struct ADSRDrawing: View {
    let e: EditorModel
    /// ATK DEC SUS REL
    let ids: [Int]
    let tint: RGB
    var w: CGFloat = 420, h: CGFloat = 150
    var ahd = false
    var name = ""

    var body: some View {
        let X = (w - 30) * 0.27 / 127, top: CGFloat = 14, bot = h - 10
        let Y = { (lv: Int) in bot - (bot - top) * CGFloat(lv) / 127 }
        let v = { (i: Int) in CGFloat(e.value(i)) }
        let p0 = CGPoint(x: 10, y: bot), p1 = CGPoint(x: 10 + v(ids[0]) * X, y: top)
        var pts: [CGPoint], hs: [DrawHandle]
        if ahd {                                          // AHD: ATK, HOLD (NOTE: as long as the key), DEC to nothing
            let hold = e.value(75)
            let p2 = CGPoint(x: p1.x + (hold >= 127 ? 0.22 * (w - 30) : CGFloat(hold) * X), y: top), p3 = CGPoint(x: p2.x + v(43) * X, y: bot)
            pts = [p0, p1, p2, p3]
            hs = [DrawHandle(at: p1, xid: ids[0], xs: 127 * X, tint: tint, label: "\(name) ATK"),
                  DrawHandle(at: p2, xid: 75, xs: 127 * X, tint: FZ.yellow, label: "\(name) HOLD"),
                  DrawHandle(at: p3, xid: 43, xs: 127 * X, tint: tint, label: "\(name) DEC")]
        } else {
            let p2 = CGPoint(x: p1.x + v(ids[1]) * X, y: Y(e.value(ids[2]))), p3 = CGPoint(x: p2.x + 0.16 * (w - 30), y: p2.y)
            let p4 = CGPoint(x: p3.x + v(ids[3]) * X, y: bot)
            pts = [p0, p1, p2, p3, p4]
            hs = [DrawHandle(at: p1, xid: ids[0], xs: 127 * X, tint: tint, label: "\(name) ATK"),
                  DrawHandle(at: p2, xid: ids[1], yid: ids[2], xs: 127 * X, ys: bot - top, tint: FZ.yellow, label: "\(name) DEC · SUS"),
                  DrawHandle(at: p4, xid: ids[3], xs: 127 * X, tint: tint, label: "\(name) REL")]
        }
        let ahdHold = ahd && e.value(75) >= 127
        return HandleCanvas(e: e, box: CGSize(width: w, height: h), handles: hs) { ctx, _ in
            strokeArea(&ctx, poly(pts), closeTo: nil, tint)
            if ahdHold { label(&ctx, "NOTE", CGPoint(x: pts[2].x - 6, y: top + 22), anchor: .trailing) }
        }
    }
}

// MARK: - FILTER

/// The filter's curve (editor.js curve): FREQ sideways, RESO (the EQ's GAIN) up and down on its handle.
struct FilterCurve: View {
    let e: EditorModel
    var w: CGFloat = 600, h: CGFloat = 300

    var body: some View {
        let ft = e.value(36), f = Double(e.value(34)) / 12700, r = Double(e.value(35)) / 12700, sh = Double(e.value(80)) / 127
        let N = 90
        var db: [Double] = []
        for i in 0...N {
            let lr = (Double(i) / Double(N) - f) * 9, w2 = pow(2, lr)
            let lp = { (n: Double) in -10 * log10(1 + pow(w2, 2 * n)) }, hp = { (n: Double) in -10 * log10(1 + pow(1 / w2, 2 * n)) }
            let peak = 18 * r * exp(-lr * lr * 3)
            var d = 0.0
            switch ft {
            case 1: d = lp(2) + peak
            case 2: d = hp(2) + peak
            case 3: d = lp(4) + peak
            case 4: d = (sh < 0.5 ? lp(2) * (1 - 2 * sh) + min(lp(1), hp(1)) * 2 * sh : min(lp(1), hp(1)) * (2 - 2 * sh) + hp(2) * (2 * sh - 1)) + peak
            case 5, 6:
                let ph = pow(2, Double(i) / Double(N) * 9) / pow(2, f * 9) * 2 * .pi
                d = 9 * r * (ft == 5 ? cos(ph) : -cos(ph)) - 3 * sh * Double(i) / Double(N)
            case 7: d = Double(e.value(81)) / 64 * 15 * exp(-lr * lr * (0.4 + 3 * sh))
            default: d = 0
            }
            db.append(max(-36, min(20, d)))
        }
        let Y = { (d: Double) in 30 + (h - 50) * CGFloat(20 - d) / 56 }, xs = { (i: Int) in 10 + (w - 20) * CGFloat(i) / CGFloat(N) }
        let pts = db.enumerated().map { CGPoint(x: xs($0.offset), y: Y($0.element)) }
        let hx = xs(Int((f * Double(N)).rounded())), hy = Y(db[Int((f * Double(N)).rounded())]) - 2
        let yid: Int? = ft == 7 ? 81 : ft == 0 ? nil : 35
        let sub = ft == 7 ? "GAIN" : ft >= 5 ? "FDBK" : "RESO"
        let tx = min(w - 170, hx + 22), ty = hy > 80 ? hy - 40 : hy + 46
        let freq = e.text(34), yt = yid.map { "\(sub) \(e.text($0))" }
        return HandleCanvas(e: e, box: CGSize(width: w, height: h), handles: [
            DrawHandle(at: CGPoint(x: hx, y: max(16, hy)), xid: 34, yid: yid, xs: w - 20, ys: (h - 50) * 0.8, tint: FZ.k4, r: 14,
                       label: yid == nil ? "FREQ" : "FREQ · \(sub)"),
        ]) { ctx, _ in
            for k in 1...5 {
                let x = w * CGFloat(k) / 6
                ctx.stroke(poly([CGPoint(x: x, y: 20), CGPoint(x: x, y: h - 18)]), with: .color(FZ.line.color), lineWidth: 1.5)
            }
            var line = poly(pts)
            let area = { () -> Path in var a = line; a.addLine(to: CGPoint(x: w - 10, y: h - 18)); a.addLine(to: CGPoint(x: 10, y: h - 18)); a.closeSubpath(); return a }()
            ctx.fill(area, with: .color(FZ.k4.opacity(0.16)))
            ctx.stroke(line, with: .color(FZ.k4.color), style: StrokeStyle(lineWidth: 4, lineCap: .round, lineJoin: .round))
            line = Path()
            label(&ctx, freq, CGPoint(x: tx, y: ty), size: 30, color: FZ.text)
            if let yt { label(&ctx, yt, CGPoint(x: tx, y: ty + 30), size: 20, color: FZ.muted) }
        }
    }
}

// MARK: - CRUSH, LFO, glide, cables

/// CRUSH's wave (editor.js crushSvg): fewer bits, steps; a lower rate, held longer.
struct CrushDrawing: View {
    let e: EditorModel
    var body: some View {
        let w: CGFloat = 380, h: CGFloat = 90
        let bits = e.value(83), srr = e.value(84)
        let q = bits > 0 ? max(2, Int((2 + 14 * (1 - Double(bits) / 127)).rounded())) : 0, hold = srr > 0 ? 1 + srr / 9 : 1
        var pts: [CGPoint] = [], last = 0.0
        for i in 0...120 {
            if i % hold == 0 { var s = sin(Double(i) / 120 * .pi * 4); if q > 0 { s = (s * Double(q)).rounded() / Double(q) }; last = s }
            pts.append(CGPoint(x: 10 + (w - 20) * CGFloat(i) / 120, y: h / 2 - CGFloat(last) * (h / 2 - 10)))
        }
        return Canvas { ctx, sz in
            ctx.scaleBy(x: sz.width / w, y: sz.width / w)
            ctx.stroke(poly(pts), with: .color(FZ.pink.color), style: StrokeStyle(lineWidth: 3, lineJoin: .round))
        }
        .aspectRatio(w / h, contentMode: .fit)
        .turnDim(e, [83, 84])
    }
}

/// An LFO's wave (editor.js wavePath), a spark running along it.
struct LFOWave: View {
    let e: EditorModel
    let k: Int
    let tint: RGB

    static func points(_ e: EditorModel, _ k: Int, _ w: CGFloat, _ h: CGFloat) -> [CGPoint] {
        let lf = { (f: Int) in 47 + 8 * k + f }
        let wv = e.value(lf(4)), sp = Double(abs(e.value(lf(0)))) / 6400, dp = Double(e.value(lf(7))) / 6400
        let ph = Double(e.value(lf(5))) / 128, fade = e.value(lf(2))
        let cyc = 1.2 + 3.3 * sp + Double(e.value(lf(1)) % 12) / 8, amp = (0.12 + 0.88 * abs(dp)) * (dp < 0 ? -1 : 1)
        var seed = Int64(7 + k)
        let rnd = { () -> Double in seed = (seed &* 1_103_515_245 &+ 12345) & 0x7fff_ffff; return Double(seed) / Double(0x7fff_ffff) * 2 - 1 }
        var held = rnd(), lastStep = -1
        return (0...160).map { i in
            let t = Double(i) / 160, p = (t * cyc + ph).truncatingRemainder(dividingBy: 1)
            var y: Double
            switch wv {
            case 0: y = 1 - 4 * abs(p - 0.5)
            case 1: y = sin(p * 2 * .pi)
            case 2: y = p < 0.5 ? 1 : -1
            case 3: y = 1 - 2 * p
            case 4: y = 2 * p - 1
            case 5: y = 2 * exp(-4 * p) - 1
            default:
                let st = Int(floor(t * cyc * 2 + ph * 2))
                if st != lastStep { held = rnd(); lastStep = st }
                y = held
            }
            let fm = fade < 0 ? min(1, t * 64 / Double(max(1, -fade)))
                : fade > 0 ? max(0, 1 - t * 64 / Double(max(1, 64 - fade + 1)) * 0.5 * (Double(fade) / 63)) : 1
            return CGPoint(x: 10 + (w - 20) * CGFloat(t), y: h / 2 - CGFloat(y * amp * fm) * (h / 2 - 8))
        }
    }

    var body: some View {
        let w: CGFloat = 420, h: CGFloat = 80
        let pts = Self.points(e, k, w, h), dur = 3.2 - 2.4 * Double(abs(e.value(47 + 8 * k))) / 6400
        return Animated(still: 0.8) { now in
            Canvas { ctx, sz in
                ctx.scaleBy(x: sz.width / w, y: sz.width / w)
                ctx.stroke(poly([CGPoint(x: 10, y: h / 2), CGPoint(x: w - 10, y: h / 2)]), with: .color(FZ.line.color), lineWidth: 2)
                ctx.stroke(poly(pts), with: .color(tint.color), style: StrokeStyle(lineWidth: 3.5, lineCap: .round, lineJoin: .round))
                let f = (now / max(0.4, dur)).truncatingRemainder(dividingBy: 1), q = pts[min(pts.count - 1, Int(f * Double(pts.count - 1)))]
                var g = ctx
                g.addFilter(.shadow(color: tint.opacity(0.9), radius: 6))
                g.fill(Path(ellipseIn: CGRect(x: q.x - 6, y: q.y - 6, width: 12, height: 12)), with: .color(.white))
            }
        }
        .aspectRatio(w / h, contentMode: .fit)
        .turnDim(e, (0..<8).map { 47 + 8 * k + $0 })
    }
}

/// PLAY's portamento: the glide drawn, TIME its handle (editor.js playPanel).
struct GlideDrawing: View {
    let e: EditorModel
    var body: some View {
        let w: CGFloat = 380, h: CGFloat = 90, on = e.value(76) != 0, gx = 60 + 260 * CGFloat(e.value(77)) / 127
        return HandleCanvas(e: e, box: CGSize(width: w, height: h),
                            handles: on ? [DrawHandle(at: CGPoint(x: gx, y: 20), xid: 77, xs: 260, tint: FZ.yellow, r: 10, label: "PORTAMENTO TIME")] : []) { ctx, _ in
            var p = Path()
            p.move(to: CGPoint(x: 10, y: 70))
            p.addLine(to: CGPoint(x: 60, y: 70))
            if on { p.addCurve(to: CGPoint(x: gx, y: 20), control1: CGPoint(x: (60 + gx) / 2, y: 70), control2: CGPoint(x: (60 + gx) / 2, y: 20)) } else { p.addLine(to: CGPoint(x: 60, y: 20)) }
            p.addLine(to: CGPoint(x: w - 10, y: 20))
            ctx.stroke(p, with: .color(FZ.yellow.opacity(on ? 1 : 0.3)), style: StrokeStyle(lineWidth: 3.5, lineCap: .round, lineJoin: .round))
        }
    }
}

/// A modulation slot's cable (editor.js modPanel): as thick as its depth, bent by its sign; dragged sideways.
struct ModCable: View {
    let e: EditorModel
    let id: Int
    let tint: RGB
    var body: some View {
        let dp = e.value(id), wv = max(1.5, CGFloat(abs(dp)) / 14)
        Canvas { ctx, sz in
            ctx.scaleBy(x: sz.width / 160, y: sz.height / 34)
            var p = Path()
            p.move(to: CGPoint(x: 4, y: 17))
            p.addCurve(to: CGPoint(x: 156, y: 17), control1: CGPoint(x: 50, y: dp >= 0 ? 0 : 34), control2: CGPoint(x: 110, y: dp >= 0 ? 34 : 0))
            ctx.stroke(p, with: .color(tint.color), style: StrokeStyle(lineWidth: wv, lineCap: .round))
        }
        .frame(height: 34)
        .sensoryFeedback(.selection, trigger: dp)
        .turns(e, x: id, label: "DEPTH", tint: tint, xSpan: 200)
        .turnDim(e, [id])
    }
}
