// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// EDIT's small parts (editor.js chip, dial, bar, seg, tog; fizz-ios.html .opc .dial .turn): every value is turned by a
// drag on it (up / right: more; a chip's whole range a 240..600 pt drag, a drawing's handle follows the finger), a
// double tap is its default, a step a haptic tick; while the finger is down the turned value takes the screen (the
// FM-1's own rule: everything else dims, its arc and number big at the top).

import SwiftUI
import FizzData
#if os(iOS)
import UIKit
#endif

// MARK: - Turning

extension View {
    /// Dimmed while another value is turned (fizz-ios.html .dragmode .dimme).
    func turnDim(_ e: EditorModel, _ ids: [Int]) -> some View {
        let dim = e.turning.map { !ids.contains($0.id) } ?? false
        return opacity(dim ? 0.13 : 1).blur(radius: dim ? 1.2 : 0)
    }

    /// A value (y: up / down, x: sideways) turned by a drag on this view; a double tap its default(s); a tap `tap`.
    func turns(_ e: EditorModel, y: Int? = nil, x: Int? = nil, label: String, tint: RGB, ySpan: CGFloat? = nil,
               xSpan: CGFloat? = nil, tap: (() -> Void)? = nil) -> some View {
        modifier(TurnModifier(e: e, y: y, x: x, label: label, tint: tint, ySpan: ySpan, xSpan: xSpan, tap: tap))
    }
}

/// The drag that turns one or two values: it starts after 3 pt along its axis (a value turned up / down takes a
/// vertical drag, one turned sideways lets a vertical drag scroll the page); counted from there (no jump); a one-axis
/// value turns finer as the finger moves away sideways (a quarter of the speed 180 pt off).
struct TurnModifier: ViewModifier {
    let e: EditorModel
    var y: Int?, x: Int?
    let label: String
    let tint: RGB
    var ySpan: CGFloat?, xSpan: CGFloat?
    var tap: (() -> Void)?
    @State private var run: TurnRun?

    struct TurnRun {
        var x0 = 0, y0 = 0
        /// the travel counted (fine: slowed), up positive
        var ax: CGFloat = 0, ay: CGFloat = 0
        var last = CGSize.zero
    }

    var axis: TurnAxis { y != nil && x != nil ? .both : y != nil ? .vertical : .horizontal }

    func body(content: Content) -> some View {
        let reset = {
            if let y { e.reset(y) }
            if let x { e.reset(x) }
        }
        Group {
            if let tap {
                content.contentShape(Rectangle()).onTapGesture(count: 2, perform: reset).onTapGesture(perform: tap)
            } else {
                content.contentShape(Rectangle()).onTapGesture(count: 2, perform: reset)
            }
        }
        .turnDrag(axis, began: begin, changed: move, ended: end)
    }

    private func begin(_ overlayTop: Bool) {
        run = TurnRun(x0: x.map(e.value) ?? 0, y0: y.map(e.value) ?? 0)
        if !e.locked { e.turning = EditTurn(id: y ?? x ?? 0, label: label, tint: tint, top: overlayTop) }
    }

    /// t: the finger's travel since the turn began (screen points, down positive).
    private func move(_ t: CGSize) {
        guard var r = run else { return }
        let ddx = t.width - r.last.width, ddy = -(t.height - r.last.height)
        r.last = t
        switch axis {
        case .vertical: r.ay += ddy * Self.fine(abs(t.width))
        case .horizontal: r.ax += ddx * Self.fine(abs(t.height))
        case .both: r.ax += ddx; r.ay += ddy
        }
        run = r
        if let y, let n = Self.turned(e, y, r.y0, r.ay, ySpan) {
            if e.set(y, n), abs(t.height) >= abs(t.width) { e.turning?.id = y }
        }
        if let x, let n = Self.turned(e, x, r.x0, r.ax, xSpan) {
            if e.set(x, n), abs(t.width) > abs(t.height) { e.turning?.id = x }
        }
    }

    private func end() {
        run = nil
        e.turning = nil
    }

    /// The speed of a one-axis turn with the finger `off` points away sideways: 1 up to 30 pt, then slower (0.25 at
    /// 180 pt and beyond).
    static func fine(_ off: CGFloat) -> CGFloat { max(0.25, 1 / (1 + max(0, off - 30) / 50)) }

    /// The value a drag of `delta` points makes of v0 (editor.js pointermove): a chip's range a 240..600 pt drag,
    /// a drawing's `span` points; a destination steps in the editor's order.
    static func turned(_ e: EditorModel, _ id: Int, _ v0: Int, _ delta: CGFloat, _ span: CGFloat?) -> Int? {
        guard let t = e.table, let r = t.info(id) else { return nil }
        if t.isDest(id) {
            let n = t.destOrder.count - 1, px = span ?? 600
            let i0 = t.destOrder.firstIndex(of: v0) ?? 0
            return t.destOrder[max(0, min(n, i0 + Int((delta / px * CGFloat(n)).rounded())))]
        }
        let px = span ?? max(240, min(CGFloat(r.span) * 12, 600))
        return v0 + Int((delta / px * CGFloat(r.span)).rounded())
    }
}

/// The way a value is dragged.
enum TurnAxis: Equatable { case vertical, horizontal, both }

extension View {
    /// A turning drag: `began` (true: the finger is low on the screen, the turned value shows at the top), `changed`
    /// with the travel since it began (down positive), `ended`.
    func turnDrag(_ axis: TurnAxis, began: @escaping (Bool) -> Void, changed: @escaping (CGSize) -> Void,
                  ended: @escaping () -> Void) -> some View {
        modifier(TurnDrag(axis: axis, began: began, changed: changed, ended: ended))
    }
}

struct TurnDrag: ViewModifier {
    let axis: TurnAxis
    let began: (Bool) -> Void
    let changed: (CGSize) -> Void
    let ended: () -> Void
    @State private var from: CGSize?

    func body(content: Content) -> some View {
        #if os(iOS)
        if #available(iOS 18, *) {
            content.gesture(TurnPan(axis: axis, began: began, changed: changed, ended: ended))
        } else {
            fallback(content)
        }
        #else
        fallback(content)
        #endif
    }

    /// iOS 17 (and the Mac's shots): SwiftUI's drag, which holds the page's scroll while on a value.
    private func fallback(_ content: Content) -> some View {
        content.highPriorityGesture(
            DragGesture(minimumDistance: 4, coordinateSpace: .global)
                .onChanged { g in
                    if from == nil { from = g.translation; began(true) }
                    guard let f = from else { return }
                    changed(CGSize(width: g.translation.width - f.width, height: g.translation.height - f.height))
                }
                .onEnded { _ in
                    from = nil
                    ended()
                }
        )
    }
}

#if os(iOS)
/// A UIKit pan for a value (iOS 18): begins after `slop` points if the finger moves along its axis, else fails at
/// once and the page's scroll view takes the drag. Measured in the window (the view may move under the finger).
final class TurnPanRecognizer: UIGestureRecognizer {
    var axis = TurnAxis.both
    var slop: CGFloat = 3
    private(set) var travel = CGSize.zero
    /// the finger began low on the screen
    private(set) var low = false
    private var touch: UITouch?
    private var origin = CGPoint.zero, start = CGPoint.zero

    override func touchesBegan(_ touches: Set<UITouch>, with event: UIEvent) {
        for t in touches where touch != nil && t !== touch { ignore(t, for: event) }
        guard touch == nil, let t = touches.first else { return }
        touch = t
        origin = t.location(in: nil)
        low = origin.y > (t.window?.bounds.midY ?? 0)
    }

    override func touchesMoved(_ touches: Set<UITouch>, with event: UIEvent) {
        guard let t = touch, touches.contains(t) else { return }
        let p = t.location(in: nil)
        if state == .possible {
            let dx = p.x - origin.x, dy = p.y - origin.y
            guard hypot(dx, dy) >= slop else { return }
            if axis == .vertical && abs(dx) > abs(dy) || axis == .horizontal && abs(dy) >= abs(dx) {
                state = .failed
                return
            }
            start = p
            travel = .zero
            state = .began
        } else if state == .began || state == .changed {
            travel = CGSize(width: p.x - start.x, height: p.y - start.y)
            state = .changed
        }
    }

    override func touchesEnded(_ touches: Set<UITouch>, with event: UIEvent) {
        guard let t = touch, touches.contains(t) else { return }
        state = state == .began || state == .changed ? .ended : .failed
    }

    override func touchesCancelled(_ touches: Set<UITouch>, with event: UIEvent) {
        guard let t = touch, touches.contains(t) else { return }
        state = state == .began || state == .changed ? .cancelled : .failed
    }

    override func reset() {
        super.reset()
        touch = nil
        travel = .zero
    }
}

@available(iOS 18, *)
struct TurnPan: UIGestureRecognizerRepresentable {
    let axis: TurnAxis
    let began: (Bool) -> Void
    let changed: (CGSize) -> Void
    let ended: () -> Void

    func makeUIGestureRecognizer(context: Context) -> TurnPanRecognizer {
        let g = TurnPanRecognizer()
        g.axis = axis
        return g
    }

    func updateUIGestureRecognizer(_ g: TurnPanRecognizer, context: Context) { g.axis = axis }

    func handleUIGestureRecognizerAction(_ g: TurnPanRecognizer, context: Context) {
        switch g.state {
        case .began: began(g.low)
        case .changed: changed(g.travel)
        case .ended, .cancelled, .failed: ended()
        default: break
        }
    }
}
#endif

/// "3.50" -> ("3", ".50"); "+0.40" -> ("+0", ".40"); "LP12" -> ("LP12", "").
func splitValue(_ s: String) -> (String, String) {
    guard let dot = s.firstIndex(of: "."), s[s.index(after: dot)...].allSatisfy(\.isNumber) else { return (s, "") }
    return (String(s[..<dot]), String(s[dot...]))
}

/// The turned value taking the screen (mock-up 12): its name, a big arc with ticks, the number, the range's ends.
struct TurnOverlay: View {
    let e: EditorModel
    let turn: EditTurn
    var size: CGFloat = 330

    var body: some View {
        let id = turn.id, t = e.table
        // (a MIDI slot: -1 - k, 0..127)
        let slot = id < 0 ? e.sound?.midi.flatMap { m in m.indices.contains(-1 - id) ? m[-1 - id] : nil } : nil
        let f = slot.map { Double($0.value) / 127 } ?? t.map { $0.frac(id, e.value(id)) } ?? 0
        let z = id < 0 ? 0 : t.flatMap { $0.info(id) }.map { $0.min < 0 && $0.max > 0 ? t!.frac(id, 0) : 0 } ?? 0
        let (w, fr) = splitValue(id < 0 ? (slot.map { $0.touched ? "\($0.value)" : "—" } ?? "") : e.text(id))
        let ends: (String, String)? = id < 0 ? ("0", "127") : t?.info(id).map { (t!.text(id, $0.min), t!.text(id, $0.max)) }
        VStack(spacing: 0) {
            HStack(spacing: 10) {
                Circle().strokeBorder(turn.tint.color, lineWidth: 3).frame(width: 14, height: 14)
                Text(turn.label).fz(18, 800, em: 0.16).foregroundStyle(turn.tint.color)
            }
            .padding(.top, 6)
            ZStack {
                Canvas { ctx, sz in
                    let c = CGPoint(x: sz.width / 2, y: sz.height / 2 + 10 * sz.width / 330), rad = 130 * sz.width / 330
                    let lw = 26 * sz.width / 330
                    ctx.stroke(arc(c, rad, 0, 1), with: .color(RGB(0x1f1f26).color), style: StrokeStyle(lineWidth: lw, lineCap: .round))
                    for i in 0...12 {
                        let a = (-135 + Double(i) * 22.5) * .pi / 180, r0 = 152 * sz.width / 330, r1 = 162 * sz.width / 330
                        var p = Path()
                        p.move(to: CGPoint(x: c.x + r0 * sin(a), y: c.y - r0 * cos(a)))
                        p.addLine(to: CGPoint(x: c.x + r1 * sin(a), y: c.y - r1 * cos(a)))
                        ctx.stroke(p, with: .color(RGB(0x3a3a44).color), lineWidth: 3)
                    }
                    if abs(f - z) > 0.002 {
                        var g = ctx
                        g.addFilter(.shadow(color: turn.tint.opacity(0.7), radius: 14))
                        g.stroke(arc(c, rad, min(z, f), max(z, f)), with: .color(turn.tint.color), style: StrokeStyle(lineWidth: lw, lineCap: .round))
                    }
                }
                HStack(alignment: .firstTextBaseline, spacing: 0) {
                    Text(w).fz(fr.isEmpty && w.count > 4 ? 64 : 104, 800, em: -0.03).foregroundStyle(.white)
                    if !fr.isEmpty { Text(fr).fz(54, 800).foregroundStyle(turn.tint.color) }
                }
                .monospacedDigit()
                .lineLimit(1).minimumScaleFactor(0.4)
                .padding(.horizontal, 60)
                .offset(y: 8)
                if let ends {
                    HStack {
                        Text(ends.0); Spacer(); Text(ends.1)
                    }
                    .fz(14, 800).foregroundStyle(FZ.muted.color)
                    .padding(.horizontal, size * 0.22)
                    .offset(y: size * 0.43)
                }
            }
            .frame(width: size, height: size)
        }
        .background(
            RadialGradient(colors: [turn.tint.opacity(0.13), .clear], center: .center, startRadius: 0, endRadius: size * 0.6)
        )
        .allowsHitTesting(false)
    }

    /// An arc from fraction a to b of the 270° sweep (-135° .. 135°, clockwise from the top).
    func arc(_ c: CGPoint, _ r: CGFloat, _ a: Double, _ b: Double) -> Path {
        var p = Path()
        let s = Angle.degrees(-225 + 270 * a), e = Angle.degrees(-225 + 270 * b)
        p.addArc(center: c, radius: r, startAngle: s, endAngle: e, clockwise: false)
        return p
    }
}

// MARK: - Dial, chip, bar, segments

/// A dial (editor.js dial, fizz-ios.html .dial): an arc from the left end, or from the middle for a signed value.
struct EditDial: View {
    @Environment(\.fizzSnapshot) private var snapshot
    let e: EditorModel
    let id: Int
    let label: String
    let tint: RGB
    var size: CGFloat = 52
    var valueSize: CGFloat = 18

    var body: some View {
        let t = e.table, f = t?.frac(id, e.value(id)) ?? 0
        let z = t?.info(id).map { $0.min < 0 ? t!.frac(id, 0) : 0 } ?? 0
        VStack(spacing: 0) {
            Animated(still: 9) { now in
                let grow = snapshot ? 1 : min(1, max(0, (now - DialClock.start) / 1.2))
                Canvas { ctx, sz in
                    let c = CGPoint(x: sz.width / 2, y: sz.height * 0.56), r = sz.width * 0.4, lw = sz.width * 0.135
                    ctx.stroke(dialArc(c, r, 0, 1), with: .color(FZ.track.color), style: StrokeStyle(lineWidth: lw, lineCap: .round))
                    let g = z + (f - z) * (1 - pow(1 - grow, 3))
                    if abs(g - z) > 0.002 {
                        ctx.stroke(dialArc(c, r, min(z, g), max(z, g)), with: .color(tint.color), style: StrokeStyle(lineWidth: lw, lineCap: .round))
                    }
                }
            }
            .frame(width: size, height: size * 0.82)
            Text(e.text(id)).fz(valueSize, 800).monospacedDigit().foregroundStyle(FZ.text.color).lineLimit(1).minimumScaleFactor(0.6)
                .padding(.top, -6)
            Text(label).fz(12, 800, em: 0.1).foregroundStyle(FZ.muted.color).lineLimit(1)
        }
        .frame(minWidth: size + 14)
        .sensoryFeedback(.selection, trigger: e.value(id))
        .turns(e, y: id, label: label, tint: tint)
        .turnDim(e, [id])
    }
}

/// The dials sweep in once, when EDIT first shows.
@MainActor enum DialClock { static let start = Date().timeIntervalSinceReferenceDate + 0.2 }

/// An arc of a dial: fraction a..b of 270°, the gap at the bottom.
func dialArc(_ c: CGPoint, _ r: CGFloat, _ a: Double, _ b: Double) -> Path {
    var p = Path()
    p.addArc(center: c, radius: r, startAngle: .degrees(135 + 270 * a), endAngle: .degrees(135 + 270 * b), clockwise: false)
    return p
}

/// A value chip (editor.js chip): its name small, its text; dragged up / down. pick: a long list, a tap lists it.
struct EditChip: View {
    let e: EditorModel
    let id: Int
    var label: String? = nil
    var tint: RGB = FZ.text
    var big = false
    var pick = false

    var body: some View {
        let name = label ?? e.table?.info(id)?.name ?? ""
        let hot = e.turning?.id == id
        VStack(alignment: .leading, spacing: 1) {
            Text(name).fz(12, 800, em: 0.08).foregroundStyle(tint.color).lineLimit(1)
            HStack(spacing: 4) {
                Text(e.text(id)).fz(big ? 26 : 17, 800).monospacedDigit().foregroundStyle(FZ.text.color).lineLimit(1)
                    .minimumScaleFactor(0.6)
                if pick { Image(systemName: "chevron.down").font(.system(size: 11, weight: .heavy)).foregroundStyle(tint.color) }
            }
        }
        .padding(.horizontal, 11).padding(.vertical, 7)
        .frame(minWidth: 66, alignment: .leading)
        .background(RoundedRectangle(cornerRadius: 12).fill(tint.mix(hot ? 20 : 7, FZ.panel).color))
        .overlay(RoundedRectangle(cornerRadius: 12).strokeBorder(tint.mix(hot ? 100 : 45, FZ.line).color, lineWidth: 1.5))
        .sensoryFeedback(.selection, trigger: e.value(id))
        .turns(e, y: id, label: name, tint: tint, tap: pick ? { e.picking = id } : nil)
        .turnDim(e, [id])
    }
}

/// A horizontal bar dragged sideways (editor.js bar): a signed value fills from the middle.
struct EditBar: View {
    let e: EditorModel
    let id: Int
    let label: String
    let tint: RGB
    var height: CGFloat = 10

    var body: some View {
        let t = e.table, f = t?.frac(id, e.value(id)) ?? 0
        let z = t?.info(id).map { $0.min < 0 ? t!.frac(id, 0) : 0 } ?? 0
        VStack(alignment: .leading, spacing: 5) {
            HStack {
                Text(label).fz(12, 800, em: 0.08).foregroundStyle(tint.color)
                Spacer(minLength: 4)
                Text(e.text(id)).fz(15, 800).monospacedDigit().foregroundStyle(FZ.text.color)
            }
            GeometryReader { g in
                ZStack(alignment: .leading) {
                    Capsule().fill(RGB(0x1d1d23).color)
                    Capsule().fill(tint.color)
                        .frame(width: max(height, g.size.width * abs(f - z)))
                        .offset(x: g.size.width * min(z, f))
                }
            }
            .frame(height: height)
        }
        .padding(.vertical, 4)
        .sensoryFeedback(.selection, trigger: e.value(id))
        .turns(e, x: id, label: label, tint: tint, xSpan: 280)
        .turnDim(e, [id])
    }
}

/// A segmented choice (editor.js seg): pairs of value and label, the value's colour when on.
struct EditSeg: View {
    let e: EditorModel
    let id: Int
    let tint: RGB
    var pairs: [(Int, String)]? = nil
    var height: CGFloat = 36

    var body: some View {
        let items = pairs ?? (e.table?.info(id).map { r in (r.min...r.max).map { ($0, e.table!.text(id, $0)) } } ?? [])
        HStack(spacing: 0) {
            ForEach(items, id: \.0) { (x, l) in
                let on = e.value(id) == x
                Button { e.set(id, x) } label: {
                    Text(l).fz(12, 800, em: 0.05).lineLimit(1).minimumScaleFactor(0.7)
                        .foregroundStyle(on ? Color.black : FZ.muted.color)
                        .padding(.horizontal, 10).frame(maxWidth: .infinity).frame(height: height)
                        .background(on ? tint.color : .clear)
                        .contentShape(Rectangle())
                }
                .buttonStyle(.plain)
            }
        }
        .clipShape(Capsule())
        .overlay(Capsule().strokeBorder(FZ.line.color, lineWidth: 1.5))
        .sensoryFeedback(.selection, trigger: e.value(id))
        .turnDim(e, [id])
    }
}

/// An on / off value as one pill, lit when on (editor.js tog).
struct EditToggle: View {
    let e: EditorModel
    let id: Int
    let label: String
    let tint: RGB

    var body: some View {
        let on = e.value(id) != 0
        Button { e.set(id, on ? 0 : 1) } label: {
            Text(label).fz(12, 800, em: 0.08)
                .foregroundStyle(on ? Color.black : FZ.muted.color)
                .padding(.horizontal, 12).frame(height: 32)
                .background(Capsule().fill(on ? tint.color : .clear))
                .overlay(Capsule().strokeBorder(on ? tint.color : FZ.line.color, lineWidth: 1.5))
                // (touched 6 pt beyond its edge)
                .contentShape(Rectangle().inset(by: -6))
        }
        .buttonStyle(.plain)
        .sensoryFeedback(.selection, trigger: on)
        .turnDim(e, [id])
    }
}

/// A caption over a part.
struct Caption<Content: View>: View {
    let label: String
    @ViewBuilder let content: Content
    var body: some View {
        VStack(alignment: .leading, spacing: 5) {
            Text(label).fz(12, 800, em: 0.12).foregroundStyle(FZ.muted.color)
            content
        }
    }
}

/// A sub-card of a page (editor.js .sub): its colour's line, its name.
struct EditCard<Content: View>: View {
    var title: String?
    let tint: RGB
    var trailing: AnyView? = nil
    @ViewBuilder let content: Content
    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            if title != nil || trailing != nil {
                HStack(spacing: 8) {
                    if let title { Text(title).fz(14, 800, em: 0.12).foregroundStyle(tint.color) }
                    Spacer(minLength: 0)
                    trailing
                }
            }
            content
        }
        .padding(12)
        .frame(maxWidth: .infinity, alignment: .leading)
        .background(RoundedRectangle(cornerRadius: 16).fill(FZ.panel.color))
        .overlay(RoundedRectangle(cornerRadius: 16).strokeBorder(tint.mix(35, FZ.line).color, lineWidth: 1.5))
    }
}

// MARK: - The picker

/// A long list's every entry as a chip (editor.js openPicker): the destinations in the device's order.
struct EditPickerSheet: View {
    let e: EditorModel
    let id: Int
    @Environment(\.dismiss) private var dismiss

    var body: some View {
        let t = e.table
        VStack(alignment: .leading, spacing: 14) {
            Text(t?.info(id)?.name ?? "").fz(16, 800, em: 0.12).foregroundStyle(FZ.muted.color)
                .padding(.horizontal, 18)
            ScrollViewReader { px in
                ScrollView {
                    LazyVGrid(columns: [GridItem(.adaptive(minimum: 74), spacing: 8)], spacing: 8) {
                        ForEach(t?.order(id) ?? [], id: \.self) { x in
                            let on = e.value(id) == x
                            Button {
                                e.set(id, x)
                                dismiss()
                            } label: {
                                Text(t?.text(id, x) ?? "").fz(14, 800).lineLimit(1).minimumScaleFactor(0.7)
                                    .foregroundStyle(on ? Color.black : FZ.text.color)
                                    .frame(maxWidth: .infinity).frame(height: 44)
                                    .background(RoundedRectangle(cornerRadius: 10).fill(on ? FZ.pink.color : FZ.raised.color))
                                    .overlay(RoundedRectangle(cornerRadius: 10).strokeBorder(on ? FZ.pink.color : FZ.line.color, lineWidth: 1.5))
                                    .contentShape(Rectangle())
                            }
                            .buttonStyle(.plain)
                            .id(x)
                        }
                    }
                    .padding(.horizontal, 18).padding(.bottom, 18)
                }
                // (the chosen one in view)
                .onAppear { px.scrollTo(e.value(id), anchor: .center) }
            }
        }
        .padding(.top, 26)
        .background(FZ.panel.color)
        .presentationDetents([.medium, .large])
        .presentationDragIndicator(.visible)
        .presentationBackground(FZ.panel.color)
        .preferredColorScheme(.dark)
    }
}
