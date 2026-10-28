// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// PROJECTS' small parts (fizz-ios.html 14, 18-22: .pp .sch .sc .clip .emenu .pend .kc .kprow): the pattern tiles with
// each track's trigs, the slot chips, the clip, the edit menu, the yellow bar of changes waiting with its slide to
// write, the kept cards, the A-D dots, the buttons, the questions and the short states.

import SwiftUI
import FizzData
import FizzKit
#if canImport(UIKit)
import UIKit
#endif

/// The mock-ups' PROJECTS icons (fizz-ios.html IC), 24 x 24.
enum ProjIcons {
    static let copy = VectorArt(24, 24, [.rect(8, 8, 12, 12, 2.5), .path("M16 8V5.5Q16 4 14.5 4h-9Q4 4 4 5.5v9Q4 16 5.5 16H8")])
    static let paste = VectorArt(24, 24, [.rect(5, 5, 14, 16, 2.5), .path("M9 3h6v4H9z")])
    static let clear = VectorArt(24, 24, [.path("M6 6l12 12M18 6L6 18")])
    static let undo = VectorArt(24, 24, [.path("M9 14L4 9l5-5"), .path("M4 9h10.5C17.5 9 20 11.5 20 14.5S17.5 20 14.5 20H11")])
    static let keep = VectorArt(24, 24, [.rect(3, 4, 18, 5, 1.5), .path("M5 9v10.5h14V9M10 13.5h4")])
    static let look = VectorArt(24, 24, [.circle(10.5, 10.5, 6.5), .path("M15.5 15.5l5 5")])
    static let plug = VectorArt(24, 24, [.path("M9 2.5v4M15 2.5v4"), .rect(5.5, 6.5, 13, 7, 2), .path("M12 13.5v3.5C12 19 10.5 20.5 8.5 20.5H6")])
    static let more = VectorArt(24, 24, [.circle(5, 12, 1.2), .circle(12, 12, 1.2), .circle(19, 12, 1.2)])
    static let pencil = VectorArt(24, 24, [.path("M4 20l1-5L15.5 4.5l4 4L9 19z")])
    static let play = VectorArt(14, 14, [.path("M2 1l11 6-11 6z")])
    static let move = VectorArt(24, 24, [.path("M4 12h16M14 6l6 6-6 6")])
}

/// The six tracks' colours (projtab.js TC).
let trackTints: [RGB] = [FZ.blue, FZ.green, FZ.cream, FZ.orange, FZ.pink, FZ.violet]

/// projects.js colours' CSS names as the palette's.
func keptTint(_ css: String) -> RGB {
    switch css {
    case "--blue": FZ.blue
    case "--cream": FZ.cream
    case "--yellow": FZ.yellow
    case "--violet": FZ.violet
    case "--green": FZ.green
    case "--orange": FZ.orange
    case "--pink": FZ.pink
    default: FZ.muted
    }
}

/// projtab.js ago: "today", "yesterday", "3 days", "2 weeks", "4 months".
func agoText(_ d: Date, now: Date = Date()) -> String {
    let days = now.timeIntervalSince(d) / 86400
    if days < 1 { return "today" }
    if days < 2 { return "yesterday" }
    if days < 14 { return "\(Int(days)) days" }
    if days < 60 { return "\(Int(days / 7)) weeks" }
    return "\(Int(days / 30)) months"
}

// MARK: - The pattern tiles

/// A pattern's trigs: six rows (the tracks) of 16 steps, each a small square of its track's colour (stretched in a tall
/// tile: bars).
struct TrigArt: View {
    let rows: [UInt16]
    var body: some View {
        Canvas { ctx, size in
            let cw = size.width / 16, rh = size.height / 6
            for (t, m) in rows.prefix(6).enumerated() where m != 0 {
                let c = trackTints[t].color
                for x in 0..<16 where (m >> x) & 1 == 1 {
                    ctx.fill(Path(CGRect(x: (CGFloat(x) + 0.18) * cw, y: (CGFloat(t) + 0.18) * rh, width: 0.64 * cw, height: 0.64 * rh)),
                             with: .color(c))
                }
            }
        }
    }
}

/// An empty tile's stripes (.pp:not(.u)::after).
struct Hatch: View {
    var color: Color = RGB(0x18181e).color
    var line: CGFloat = 1.6
    var step: CGFloat = 3.2
    var body: some View {
        Canvas { ctx, size in
            var y: CGFloat = 0
            while y < size.height {
                ctx.fill(Path(CGRect(x: 0, y: y, width: size.width, height: min(line, size.height - y))), with: .color(color))
                y += step
            }
        }
    }
}

/// One of A01..D16 (.pp): its number, its trigs; selected cyan, pasted yellow (dropping in), cleared dashed yellow.
struct PatternTileView: View {
    let k: Int
    let tile: FizzPatternTile?
    var big = false
    var selected = false
    var pasted = false
    var cleared = false
    var playing = false
    var delay = 0.0

    var body: some View {
        let used = tile != nil
        let r: CGFloat = big ? 8 : 9
        let tint: RGB? = selected ? FZ.cyan : pasted ? FZ.yellow : cleared ? FZ.yellow : nil
        let fill: Color = selected ? FZ.cyan.mix(13, FZ.raised).color : pasted ? FZ.yellow.mix(11, FZ.raised).color
            : cleared ? .clear : used ? FZ.raised.color : RGB(0x0f0f13).color
        let inset = big ? EdgeInsets(top: 18, leading: 3, bottom: 5, trailing: 3) : EdgeInsets(top: 15, leading: 4, bottom: 4, trailing: 4)
        ZStack(alignment: .topLeading) {
            RoundedRectangle(cornerRadius: r).fill(fill)
            Group {
                if let t = tile {
                    if t.demo || t.bad {
                        Hatch(color: (t.demo ? FZ.champagne : FZ.red).opacity(0.35), line: big ? 3 : 1.6, step: big ? 6 : 3.2)
                    } else {
                        TrigArt(rows: t.rows)
                    }
                } else {
                    Hatch(color: RGB(0x18181e).opacity(cleared ? 0.35 : 0.9), line: big ? 3 : 1.6, step: big ? 12.5 : 3.2)
                }
            }
            .padding(inset)
            Text(big ? FizzClip.patName(k) : String(format: "%02d", (k & 15) + 1))
                .fz(12, 800)
                .foregroundStyle((tint ?? (used ? FZ.text : RGB(0x3d3d46))).color)
                .padding(.leading, big ? 4 : 5).padding(.top, big ? 1 : 0)
                .frame(height: big ? 16 : 15)
        }
        .overlay {
            if cleared {
                RoundedRectangle(cornerRadius: r).strokeBorder(FZ.yellow.color, style: StrokeStyle(lineWidth: 1.5, dash: [4, 3]))
            } else {
                RoundedRectangle(cornerRadius: r).strokeBorder((tint ?? (used ? RGB(0x2a2a33) : RGB(0x1c1c22))).color, lineWidth: 1.5)
            }
        }
        .overlay {
            if playing {
                RoundedRectangle(cornerRadius: r).strokeBorder(.white, lineWidth: 2).padding(-3)
            }
        }
        .background(
            RoundedRectangle(cornerRadius: r).stroke(FZ.cyan.opacity(selected ? 0.45 : 0), lineWidth: 4)
                .shadow(color: FZ.cyan.opacity(selected ? 0.5 : 0), radius: 9)
        )
        .keyframeAnimator(initialValue: Drop(), trigger: pasted) { v, d in
            v.scaleEffect(d.scale).offset(y: d.dy)
        } keyframes: { _ in
            KeyframeTrack(\Drop.scale) {
                LinearKeyframe(pasted ? 0.7 : 1, duration: delay)
                SpringKeyframe(1.1, duration: 0.25)
                SpringKeyframe(1, duration: 0.3)
            }
            KeyframeTrack(\Drop.dy) {
                LinearKeyframe(pasted ? -14 : 0, duration: delay)
                SpringKeyframe(0, duration: 0.3)
            }
        }
    }

    struct Drop { var scale = 1.0, dy = 0.0 }
}

// MARK: - A grid of tiles: tap, drag across

/// A project's 64 patterns: four banks A..D, `cols` tiles a row (8: two rows a bank; 16: one); a tap selects one, a
/// drag across selects a range. Reports where each tile is (the edit menu's anchor).
struct PatternGrid: View {
    let model: ProjectsModel
    let project: FizzProjectModel
    var cols = 8
    var tileH: CGFloat = 40
    var big = false
    var gap: CGFloat = 5
    var bankGap: CGFloat = 12
    var label: CGFloat = 16
    /// the selection's first tile's frame (in the grid's space)
    var onAnchor: (CGRect?) -> Void = { _ in }
    @State private var dragFrom: Int?

    var rowsPerBank: Int { 64 / 4 / cols }
    var height: CGFloat { 4 * (CGFloat(rowsPerBank) * tileH + CGFloat(rowsPerBank - 1) * gap) + 3 * bankGap }

    var body: some View {
        GeometryReader { g in
            let w = (g.size.width - label - CGFloat(cols) * gap) / CGFloat(cols)
            let pasted = model.pasted(project.p), cleared = model.cleared(project.p)
            let playK = model.playing && model.selected.current ? model.unit?.status?.pattern : nil
            ZStack(alignment: .topLeading) {
                ForEach(0..<4, id: \.self) { b in
                    Text(["A", "B", "C", "D"][b]).fz(big ? 16 : 15, 800).foregroundStyle(FZ.muted.color)
                        .frame(width: label, height: bankHeight, alignment: .leading)
                        .offset(y: bankTop(b))
                }
                ForEach(0..<64, id: \.self) { k in
                    let f = frame(k, w)
                    let pk = pasted.contains(k)
                    PatternTileView(k: k, tile: project.pats.indices.contains(k) ? project.pats[k] : nil, big: big,
                                    selected: model.pats.contains(k), pasted: pk, cleared: cleared.contains(k),
                                    playing: playK == k, delay: pk ? Double(pasted.sorted().firstIndex(of: k) ?? 0) * 0.12 : 0)
                        .frame(width: f.width, height: f.height)
                        .offset(x: f.minX, y: f.minY)
                }
            }
            .contentShape(Rectangle())
            .gesture(
                DragGesture(minimumDistance: 0)
                    .onChanged { v in
                        guard let k = hit(v.location, w) else { return }
                        if dragFrom == nil {
                            guard let k0 = hit(v.startLocation, w) else { return }
                            dragFrom = k0
                        }
                        if let k0 = dragFrom, k != k0 || hypot(v.translation.width, v.translation.height) > 8 {
                            model.selectRange(from: k0, to: k)
                        }
                    }
                    .onEnded { v in
                        defer { dragFrom = nil }
                        if hypot(v.translation.width, v.translation.height) <= 8, let k = hit(v.startLocation, w) { model.tap(k) }
                    }
            )
            .onChange(of: model.pats, initial: true) { _, p in onAnchor(p.min().map { frame($0, w) }) }
            .onChange(of: g.size.width) { _, _ in onAnchor(model.pats.min().map { frame($0, w) }) }
        }
        .frame(height: height)
    }

    private var bankHeight: CGFloat { CGFloat(rowsPerBank) * tileH + CGFloat(rowsPerBank - 1) * gap }
    private func bankTop(_ b: Int) -> CGFloat { CGFloat(b) * (bankHeight + bankGap) }

    func frame(_ k: Int, _ w: CGFloat) -> CGRect {
        let b = k / 16, i = k % 16, row = i / cols, col = i % cols
        return CGRect(x: label + gap + CGFloat(col) * (w + gap), y: bankTop(b) + CGFloat(row) * (tileH + gap), width: w, height: tileH)
    }

    func hit(_ p: CGPoint, _ w: CGFloat) -> Int? {
        guard p.x >= label, p.y >= 0, p.y <= height else { return nil }
        let col = min(cols - 1, max(0, Int((p.x - label - gap / 2) / (w + gap))))
        let b = min(3, max(0, Int(p.y / (bankHeight + bankGap))))
        let y = p.y - bankTop(b)
        let row = min(rowsPerBank - 1, max(0, Int((y + gap / 2) / (tileH + gap))))
        return b * 16 + row * cols + col
    }
}

// MARK: - Slot chips, the clip, the kept chip

/// 1..8 as chips (.sch .sc): the one looked at filled, empty ones dashed, a yellow dot where changes wait; armed:
/// every other slot a target (champagne), the kept chip at the end.
struct SlotChips: View {
    let model: ProjectsModel
    var keptOn = false

    var body: some View {
        HStack(spacing: 5) {
            ForEach(model.slots) { s in
                let on = s.p == model.sel && !keptOn && model.arm == nil
                let target = model.arm != nil && s.p != model.arm?.from
                Button {
                    Task {
                        if model.arm != nil { await model.tapSlot(s.p); return }
                        if s.p == model.sel && model.mode == .patterns { withAnimation(.spring(response: 0.4, dampingFraction: 0.85)) { model.mode = .slots }; return }
                        model.select(s.p)
                        await model.enter(.patterns)
                    }
                } label: {
                    Text("\(s.p + 1)").fz(14, 800)
                        .foregroundStyle(on ? Color.black : target ? FZ.champagne.color : s.there ? FZ.cyan.color : RGB(0x4a4a54).color)
                        .frame(maxWidth: .infinity).frame(height: 34)
                        .background(Capsule().fill(on ? FZ.cyan.color : .clear))
                        .overlay {
                            if target {
                                Capsule().strokeBorder(FZ.champagne.opacity(0.6), style: StrokeStyle(lineWidth: 1.5, dash: [4, 3]))
                            } else if s.there {
                                Capsule().strokeBorder(on ? FZ.cyan.color : FZ.cyan.mix(50, FZ.line).color, lineWidth: 1.5)
                            } else {
                                Capsule().strokeBorder(FZ.dim.color, style: StrokeStyle(lineWidth: 1.5, dash: [4, 3]))
                            }
                        }
                        .overlay(alignment: .topTrailing) { if s.changed { ChangeDot().offset(x: 1, y: -3) } }
                        .shadow(color: on ? FZ.cyan.opacity(0.5) : .clear, radius: 7)
                        .contentShape(Capsule())
                }
                .buttonStyle(.plain)
            }
            KeptChip(count: model.kept.items.count, on: keptOn) {
                Task { await model.enter(keptOn ? .patterns : .kept) }
            }
        }
        .padding(.bottom, 14)
    }
}

/// The yellow dot of a slot with changes waiting.
struct ChangeDot: View {
    var body: some View {
        Circle().fill(FZ.yellow.color).frame(width: 11, height: 11)
            .overlay(Circle().strokeBorder(.black, lineWidth: 2).padding(-2))
            .shadow(color: FZ.yellow.color, radius: 4)
    }
}

/// The kept projects' chip (.sc.kept): the box, how many.
struct KeptChip: View {
    let count: Int
    var on = false
    var height: CGFloat = 34
    var action: () -> Void = {}
    var body: some View {
        Button(action: action) {
            HStack(spacing: 6) {
                IconView(ProjIcons.keep, color: on ? .black : FZ.champagne.color, lineWidth: 2.3, backdrop: on ? FZ.champagne.color : .black)
                    .frame(width: 17, height: 17)
                Text("\(count)").fz(14, 800)
            }
            .foregroundStyle(on ? Color.black : FZ.champagne.color)
            .padding(.horizontal, 12).frame(height: height)
            .background(Capsule().fill(on ? FZ.champagne.color : .clear))
            .overlay(Capsule().strokeBorder(on ? FZ.champagne.color : FZ.champagne.mix(55, FZ.line).color, lineWidth: 1.5))
            .shadow(color: on ? FZ.champagne.opacity(0.45) : .clear, radius: 7)
            .contentShape(Capsule())
            .fixedSize()
        }
        .buttonStyle(.plain)
    }
}

/// What PASTE pastes (.clip): dashed cyan, how many, from where.
struct ClipChip: View {
    let clip: FizzClip
    var body: some View {
        HStack(spacing: 7) {
            IconView(ProjIcons.copy, color: FZ.cyan.color, lineWidth: 2.3, backdrop: FZ.cyan.mix(9, FZ.bg).color).frame(width: 16, height: 16)
            Text("\(clip.n)").fz(16, 800).foregroundStyle(FZ.text.color)
            Text("\(clip.from) \(clip.range)").fz(13, 800).foregroundStyle(FZ.cyan.color).lineLimit(1)
        }
        .padding(.leading, 10).padding(.trailing, 12).frame(height: 36)
        .background(Capsule().fill(FZ.cyan.opacity(0.09)))
        .overlay(Capsule().strokeBorder(FZ.cyan.color, style: StrokeStyle(lineWidth: 1.5, dash: [4, 3])))
        .fixedSize()
        .transition(.asymmetric(insertion: .scale(scale: 0.4, anchor: .bottomLeading).combined(with: .opacity), removal: .opacity))
    }
}

// MARK: - Buttons

/// The page's .btn with an icon: filled (black letters, a glow), ghost (outlined), or red-lined.
struct ProjButton: View {
    enum Style { case fill, ghost, red }
    let title: String
    var icon: VectorArt?
    var color: RGB = FZ.cyan
    var style: Style = .fill
    var height: CGFloat = 42
    var size: CGFloat = 13
    var padding: CGFloat = 14
    var enabled = true
    var iconFill = false
    var action: () -> Void

    var body: some View {
        let ink: Color = style == .fill ? .black : style == .red ? FZ.red.color : FZ.text.color
        Button(action: action) {
            HStack(spacing: 6) {
                if let icon {
                    IconView(icon, color: ink, lineWidth: 2.3, backdrop: style == .fill ? color.color : .black, fill: iconFill)
                        .frame(width: iconFill ? 12 : 16, height: iconFill ? 12 : 16)
                }
                Text(title).fz(size, 800, em: 0.06).lineLimit(1).minimumScaleFactor(0.75)
            }
            .foregroundStyle(ink)
            .padding(.horizontal, padding)
            .frame(maxWidth: .infinity).frame(height: height)
            .background(Capsule().fill(style == .fill ? color.color : .clear))
            .overlay(Capsule().strokeBorder(style == .ghost ? FZ.line.color : style == .red ? FZ.red.opacity(0.55) : .clear, lineWidth: 1.5))
            .shadow(color: style == .fill ? color.opacity(0.4) : .clear, radius: 10)
            .contentShape(Capsule())
        }
        .buttonStyle(.plain)
        .disabled(!enabled)
        .opacity(enabled ? 1 : 0.4)
    }
}

/// A round icon button (.icon).
struct RoundIcon: View {
    let icon: VectorArt
    var color: RGB = FZ.body
    var ring: RGB = FZ.line
    var size: CGFloat = 40
    var action: () -> Void
    var body: some View {
        Button(action: action) {
            IconView(icon, color: color.color, lineWidth: 2.2, backdrop: .black).frame(width: size * 0.47, height: size * 0.47)
                .frame(width: size, height: size)
                .overlay(Circle().strokeBorder(ring.color, lineWidth: 1.5))
                .contentShape(Circle())
        }
        .buttonStyle(.plain)
    }
}

// MARK: - The edit menu over a selection

/// COPY, PASTE, CLEAR, .fizzpat over the selection (.emenu): it comes with the selection, no hold.
struct EditMenu: View {
    let model: ProjectsModel
    var share: () -> Void

    var body: some View {
        HStack(spacing: 6) {
            ProjButton(title: "COPY", icon: ProjIcons.copy, color: FZ.cyan) { Task { await model.copy() } }
                .fixedSize()
            ProjButton(title: "PASTE", icon: ProjIcons.paste, color: FZ.yellow, enabled: model.clip != nil) { model.paste() }
                .fixedSize()
            ProjButton(title: "CLEAR", icon: ProjIcons.clear, style: .red) { model.clear() }
                .fixedSize()
            RoundIcon(icon: Icons.share, size: 42, action: share)
        }
        .padding(6)
        .background(Capsule().fill(RGB(0x1b1b21).color))
        .overlay(Capsule().strokeBorder(RGB(0x3a3a44).color, lineWidth: 1.5))
        .shadow(color: .black, radius: 15, y: 12)
    }
}

// MARK: - The changes waiting, and WRITE

/// The yellow bar (.pend): how many changes, the pool places they add, UNDO, DISCARD, and the slide to write.
struct PendingBar: View {
    let model: ProjectsModel
    /// iPad: one row (.pend.inl)
    var inline = false

    var body: some View {
        let n = model.ops.count, pool = model.view?.poolAdded ?? 0
        let top = HStack(spacing: 10) {
            Text("\(n)").fz(46, 800).monospacedDigit().foregroundStyle(FZ.yellow.color)
                .contentTransition(.numericText())
            VStack(alignment: .leading, spacing: 1) {
                Text(n == 1 ? "CHANGE" : "CHANGES").fz(13, 800, em: 0.12).foregroundStyle(FZ.yellow.color)
                if pool > 0 { Text("+\(pool) POOL").fz(13, 800, em: 0.03).foregroundStyle(FZ.body.color) }
            }
            .fixedSize()
            Spacer(minLength: 4)
            ProjButton(title: "UNDO", icon: ProjIcons.undo, style: .ghost, height: 40, size: 12, padding: 12) {
                Task { await model.undo() }
            }
            .fixedSize()
            .keyboardShortcut("z", modifiers: .command)
            ProjButton(title: "DISCARD", style: .ghost, height: 40, size: 12, padding: 12) { model.discard() }
                .fixedSize()
        }
        Group {
            if inline {
                HStack(spacing: 12) {
                    top
                    WriteSlide(model: model, height: 50).frame(width: 230)
                }
                .padding(.vertical, 10).padding(.leading, 18).padding(.trailing, 14)
            } else {
                VStack(spacing: 10) {
                    top
                    WriteSlide(model: model, height: 54)
                }
                .padding(.top, 12).padding(.bottom, 12).padding(.leading, 16).padding(.trailing, 12)
            }
        }
        .background(RoundedRectangle(cornerRadius: 24).fill(FZ.yellow.mix(10, RGB(0x0e0e12)).color))
        .overlay(RoundedRectangle(cornerRadius: 24).strokeBorder(FZ.yellow.color, lineWidth: 2))
        .shadow(color: FZ.yellow.opacity(0.25), radius: 13)
        .shadow(color: inline ? .clear : .black, radius: 15, y: -10)
        .animation(.spring(response: 0.35, dampingFraction: 0.8), value: n)
    }
}

/// WRITE TO FM-1 as a slide (the owner's default: written only after a slide, as RESTORE): the knob dragged to the
/// end starts it; it breathes while waiting. Over Bluetooth: WRITE NEEDS USB.
struct WriteSlide: View {
    let model: ProjectsModel
    var height: CGFloat = 54
    @State private var dx: CGFloat = 0
    @Environment(\.fizzSnapshot) private var snapshot

    var body: some View {
        let ble = model.viaBle, tint = ble ? FZ.orange : FZ.yellow
        GeometryReader { g in
            let knob = height - 8, travel = max(1, g.size.width - knob - 8), f = min(1, dx / travel)
            Animated(still: 0.6) { t in
                ZStack(alignment: .leading) {
                    Capsule().fill(tint.color)
                        .shadow(color: tint.opacity(0.35 + (ble ? 0 : 0.35 * breathe(t, 2))), radius: 12 + (ble ? 0 : 8 * breathe(t, 2)))
                    Capsule().fill(.white.opacity(0.25)).frame(width: knob + 8 + dx)
                    HStack(spacing: 8) {
                        Text(ble ? "WRITE NEEDS USB" : "WRITE TO FM-1").fz(height > 52 ? 17 : 15, 800, em: 0.08).foregroundStyle(.black)
                            .lineLimit(1).minimumScaleFactor(0.7)
                    }
                    .frame(maxWidth: .infinity)
                    .padding(.leading, knob * 0.6)
                    .opacity(1 - Double(f) * 0.8)
                    Circle().fill(.black)
                        .overlay(IconView(Icons.down, color: tint.color, lineWidth: 2.8, backdrop: .black).frame(width: knob * 0.45, height: knob * 0.45))
                        .frame(width: knob, height: knob)
                        .offset(x: 4 + dx)
                        .gesture(ble || !model.online ? nil : slide(travel))
                }
            }
        }
        .frame(height: height)
        .opacity(model.online || snapshot ? 1 : 0.4)
        .onTapGesture { if ble { model.say("USB CABLE NEEDED", FZ.orange) } }
    }

    private func slide(_ travel: CGFloat) -> some Gesture {
        DragGesture(minimumDistance: 0)
            .onChanged { v in dx = min(travel, max(0, v.translation.width)) }
            .onEnded { _ in
                let go = dx >= travel * 0.88
                withAnimation(.spring(response: 0.35, dampingFraction: 0.8)) { dx = 0 }
                if go { Task { await model.write() } }
            }
    }
}

// MARK: - The kept cards

/// A project kept on the phone (.kc): name, when, patterns and sounds, its sounds' colours; open: its patterns (a dot
/// tapped copies that pattern into the clip) and its actions.
struct KeptCard: View {
    let model: ProjectsModel
    let item: KeptProject
    var open = false
    var compact = false
    @State private var renaming = false
    @State private var newName = ""

    var body: some View {
        let sel = open
        VStack(alignment: .leading, spacing: 0) {
            HStack(alignment: .firstTextBaseline, spacing: 8) {
                Text(item.name).fz(sel ? (compact ? 18 : 24) : (compact ? 16 : 18), 800, em: 0.02).foregroundStyle(FZ.text.color).lineLimit(1)
                Spacer(minLength: 4)
                Text(agoText(item.date)).fz(13, 700).foregroundStyle(FZ.muted.color).lineLimit(1).fixedSize()
            }
            HStack(spacing: 12) {
                count(item.info.pats, "patterns")
                count(item.info.sounds, "sounds")
                Spacer(minLength: 4)
                HStack(spacing: 4) {
                    ForEach(Array(item.info.colours.prefix(compact ? 6 : 8).enumerated()), id: \.offset) { _, c in
                        RoundedRectangle(cornerRadius: 3).fill(keptTint(c).color).frame(width: 11, height: 11)
                    }
                }
            }
            .padding(.top, 3)
            if sel && !compact {
                KeptDots(model: model, item: item).padding(.top, 10)
                HStack(spacing: 8) {
                    ProjButton(title: model.arm?.kind == .load && model.arm?.kept?.id == item.id ? "LOAD INTO …" : "LOAD INTO",
                               icon: ProjIcons.play, color: FZ.champagne, height: 50, size: 15, enabled: model.view != nil && model.online,
                               iconFill: true) {
                        if model.arm?.kind == .load { model.disarm() } else { model.startLoad(item) }
                    }
                    ShareIcon(url: item.url).scaleEffect(1.25).frame(width: 50, height: 50)
                    RoundIcon(icon: ProjIcons.pencil, size: 50) { newName = item.name; renaming = true }
                    RoundIcon(icon: Icons.trash, color: FZ.red, ring: FZ.red.mix(55, FZ.bg), size: 50) { model.deleteKept(item) }
                }
                .padding(.top, 12)
            }
        }
        .padding(.top, compact ? 10 : 11).padding(.bottom, compact ? 11 : 12).padding(.horizontal, compact ? 13 : 14)
        .background(RoundedRectangle(cornerRadius: 18).fill(sel ? FZ.champagne.mix(8, FZ.raised).color : FZ.raised.color))
        .overlay(RoundedRectangle(cornerRadius: 18).strokeBorder(sel ? FZ.champagne.color : FZ.line.color, lineWidth: sel ? 2 : 1.5))
        .shadow(color: sel ? FZ.champagne.opacity(0.25) : .clear, radius: 13)
        .contentShape(RoundedRectangle(cornerRadius: 18))
        .onTapGesture {
            withAnimation(.spring(response: 0.4, dampingFraction: 0.85)) { model.keptSel = model.keptSel == item.id ? nil : item.id }
        }
        .alert("RENAME", isPresented: $renaming) {
            TextField("", text: $newName).capsInput()
            Button("RENAME") { Task { await model.renameKept(item, to: newName) } }
            Button("CANCEL", role: .cancel) {}
        }
    }

    private func count(_ n: Int, _ what: String) -> some View {
        HStack(alignment: .firstTextBaseline, spacing: 4) {
            Text("\(n)").fz(15, 800).foregroundStyle(FZ.text.color)
            Text(what).fz(13, 700).foregroundStyle(FZ.muted.color)
        }
        .fixedSize()
    }
}

/// A kept project's A..D dots (.kprow): used ones champagne; a tap copies that pattern, a drag a range of them.
struct KeptDots: View {
    let model: ProjectsModel
    let item: KeptProject
    @State private var flash: Int?

    var body: some View {
        VStack(spacing: 0) {
            ForEach(0..<4, id: \.self) { b in
                HStack(spacing: 0) {
                    Text(["A", "B", "C", "D"][b]).fz(13, 800).foregroundStyle(FZ.muted.color).frame(width: 18, alignment: .leading)
                    ForEach(0..<16, id: \.self) { x in
                        let k = b * 16 + x, used = item.info.used.indices.contains(k) && item.info.used[k]
                        let hot = flash == k || (model.clip?.from == item.name && model.clip?.ks == [k])
                        Group {
                            if hot {
                                Circle().fill(FZ.cyan.color).frame(width: 16, height: 16)
                                    .overlay(Circle().strokeBorder(.black, lineWidth: 3).padding(-3))
                                    .overlay(Circle().strokeBorder(FZ.cyan.color, lineWidth: 2).padding(-5))
                                    .shadow(color: FZ.cyan.color, radius: 8)
                            } else {
                                Circle().fill(used ? FZ.champagne.color : .clear).frame(width: 12, height: 12)
                                    .overlay(Circle().strokeBorder(used ? FZ.champagne.color : RGB(0x2e2e36).color, lineWidth: 2))
                            }
                        }
                        .frame(maxWidth: .infinity).frame(height: 19)
                        .contentShape(Rectangle())
                        .onTapGesture {
                            guard used else { return }
                            flash = k
                            Task { await model.copy(from: item, ks: [k]) }
                        }
                    }
                }
            }
        }
    }
}

// MARK: - The A..D dots of the slots screen

/// A project's patterns as dots (mock-up 14's .prow): used cyan, the playing one white and pulsing.
struct PatternDots: View {
    let project: FizzProjectModel?
    var playing: Int?
    var body: some View {
        Animated(still: 0.5) { t in
            VStack(spacing: 0) {
                ForEach(0..<4, id: \.self) { b in
                    HStack(spacing: 0) {
                        Text(["A", "B", "C", "D"][b]).fz(14, 800).foregroundStyle(FZ.muted.color).frame(width: 22, alignment: .leading)
                        ForEach(0..<16, id: \.self) { x in
                            let k = b * 16 + x, used = project?.pats.indices.contains(k) == true && project?.pats[k] != nil
                            Group {
                                if playing == k {
                                    Circle().fill(.white).frame(width: 17, height: 17)
                                        .overlay(Circle().strokeBorder(.black, lineWidth: 3).padding(-3))
                                        .overlay(Circle().strokeBorder(FZ.cyan.color, lineWidth: 2).padding(-5))
                                        .shadow(color: FZ.cyan.opacity(0.4 + 0.6 * breathe(t, 1)), radius: 4 + 10 * breathe(t, 1))
                                } else {
                                    Circle().fill(used ? FZ.cyan.color : .clear).frame(width: 13, height: 13)
                                        .overlay(Circle().strokeBorder(used ? FZ.cyan.color : RGB(0x26262e).color, lineWidth: 2))
                                }
                            }
                            .frame(maxWidth: .infinity).frame(height: 24)
                        }
                    }
                }
            }
        }
    }
}

// MARK: - Questions, short states, sharing

/// The model's question as an alert (the page's confirmBox).
struct ProjectsAskAlert: ViewModifier {
    let model: ProjectsModel
    func body(content: Content) -> some View {
        content.alert(model.ask?.title ?? "", isPresented: Binding(get: { model.ask != nil }, set: { if !$0 { model.ask = nil } }),
                      presenting: model.ask) { a in
            Button(a.go, role: a.tint == FZ.red ? .destructive : nil) { Task { await a.run() } }
            Button("CANCEL", role: .cancel) {}
        } message: { a in
            Text(a.what)
        }
    }
}

/// The short state a moment (a coloured capsule).
struct NoteStrip: View {
    let note: ProjectsNote
    var body: some View {
        HStack(spacing: 10) {
            Circle().fill(note.tint.color).frame(width: 10, height: 10).shadow(color: note.tint.color, radius: 4)
            Text(note.text).fz(15, 800, em: 0.06).foregroundStyle(note.tint.color).lineLimit(1).minimumScaleFactor(0.6)
        }
        .padding(.horizontal, 16).frame(height: 40)
        .background(Capsule().fill(RGB(0x101014).color))
        .background(Capsule().fill(note.tint.opacity(0.14)))
        .overlay(Capsule().strokeBorder(note.tint.color, lineWidth: 1.5))
        .shadow(color: .black, radius: 10)
        .fixedSize()
        .transition(.move(edge: .top).combined(with: .opacity))
    }
}

/// A file for the share sheet.
struct SharedFile: Identifiable { let url: URL; var id: String { url.path } }

#if canImport(UIKit)
/// The system share sheet (a .fizzpat made on demand).
struct ActivitySheet: UIViewControllerRepresentable {
    let url: URL
    func makeUIViewController(context: Context) -> UIActivityViewController {
        UIActivityViewController(activityItems: [url], applicationActivities: nil)
    }
    func updateUIViewController(_ vc: UIActivityViewController, context: Context) {}
}
#endif

extension View {
    /// The share sheet for a file made on demand.
    @ViewBuilder func shareSheet(_ file: Binding<SharedFile?>) -> some View {
        #if canImport(UIKit)
        sheet(item: file) { f in ActivitySheet(url: f.url).presentationDetents([.medium, .large]) }
        #else
        self
        #endif
    }
}
