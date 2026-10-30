// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// A pattern dragged, as drawn (mock-ups 43, 48): the ghost under the finger with its COPY / MOVE badge, the card a
// project held under it springs open (the phone: its 64 patterns as dots, the first free one blinking), the "+n POOL"
// a project's head shows, and the TO… sheet (COPY / MOVE, the project, its 4 x 16 patterns or FIRST FREE, the pool line:
// the sounds that come along in their colours, the ones there, the count after; a full pool's project dashed red).

import SwiftUI
import FizzData
import FizzKit

// MARK: - The ghost

/// The lifted pattern over the finger (a little above it, so the finger does not hide it), tilted, its badge.
struct ProjectsGhost: View {
    let pm: ProjectsModel
    var body: some View {
        if let l = pm.lift, let t = pm.view?.project(l.p)?.pats[l.k] {
            let w = max(46, l.size.width), h = max(36, l.size.height)
            let tint = l.move ? FZ.orange : FZ.cyan
            PatternTileView(k: l.k, tile: t, big: true, selected: !l.move)
                .frame(width: w, height: h)
                .overlay(RoundedRectangle(cornerRadius: 9).strokeBorder(tint.color, lineWidth: 2))
                .scaleEffect(1.3)
                .rotationEffect(.degrees(-6))
                .shadow(color: tint.opacity(0.55), radius: 14)
                .shadow(color: .black.opacity(0.7), radius: 10, y: 8)
                .overlay(alignment: .topTrailing) { DragBadge(move: l.move).offset(x: 26, y: -22) }
                .position(x: l.at.x - 6, y: l.at.y - h * 1.05)
                .allowsHitTesting(false)
                .transition(.scale(scale: 0.8).combined(with: .opacity))
                .animation(.spring(response: 0.25, dampingFraction: 0.8), value: l.move)
        }
    }
}

/// COPY (cyan) or MOVE (orange), with its icon.
struct DragBadge: View {
    let move: Bool
    var body: some View {
        let tint = move ? FZ.orange : FZ.cyan
        HStack(spacing: 4) {
            IconView(move ? ProjIcons.move : ProjIcons.copy, color: .black, lineWidth: 2.6, backdrop: tint.color).frame(width: 12, height: 12)
            Text(move ? "MOVE" : "COPY").fz(12, 800, em: 0.06).foregroundStyle(.black)
        }
        .padding(.horizontal, 8).frame(height: 24)
        .background(Capsule().fill(tint.color))
        .overlay(Capsule().strokeBorder(.black, lineWidth: 1.5).padding(-1.5))
        .shadow(color: tint.opacity(0.6), radius: 8)
        .fixedSize()
    }
}

// MARK: - A project sprung open (the phone)

/// The card of the project held under the drag: its number and name, → its first free pattern, its 64 patterns as dots
/// (used grey, the first free blinking cyan, the one under the finger ringed). A drop target: on a dot, that pattern; on
/// the rest of it, the first free one.
struct SpringCard: View {
    let pm: ProjectsModel
    let q: Int
    static let width: CGFloat = 252, pad: CGFloat = 12, head: CGFloat = 34, row: CGFloat = 15, letter: CGFloat = 16

    var body: some View {
        let s = pm.slot(q), pr = pm.view?.project(q), free = pm.firstFree(q), over = pm.lift?.over?.q == q ? pm.lift?.over?.j : nil
        let cell = (Self.width - 2 * Self.pad - Self.letter) / 16
        Animated(still: 0.3) { t in
            VStack(alignment: .leading, spacing: 6) {
                HStack(spacing: 8) {
                    Text("\(q + 1)").fz(14, 800).foregroundStyle(.black).frame(width: 26, height: 26)
                        .background(Circle().fill((s.current ? FZ.cyan : FZ.green).color))
                    Text(s.name).fz(18, 800).foregroundStyle(FZ.text.color).lineLimit(1).minimumScaleFactor(0.6)
                    Spacer(minLength: 4)
                    if let j = over ?? free {
                        HStack(spacing: 4) {
                            IconView(ProjIcons.move, color: .black, lineWidth: 2.8, backdrop: FZ.cyan.color).frame(width: 11, height: 11)
                            Text(FizzClip.patName(j)).fz(13, 800).foregroundStyle(.black)
                        }
                        .padding(.horizontal, 8).frame(height: 24)
                        .background(RoundedRectangle(cornerRadius: 7).fill(FZ.cyan.color))
                    }
                }
                .frame(height: Self.head)
                VStack(spacing: 0) {
                    ForEach(0..<4, id: \.self) { b in
                        HStack(spacing: 0) {
                            Text(["A", "B", "C", "D"][b]).fz(11, 800).foregroundStyle(FZ.muted.color).frame(width: Self.letter, alignment: .leading)
                            ForEach(0..<16, id: \.self) { i in
                                let k = b * 16 + i, used = pr?.pats[k] != nil
                                let blink = k == free && over == nil, on = k == over
                                Circle()
                                    .fill(on ? (used ? FZ.yellow : FZ.cyan).color : blink ? FZ.cyan.opacity(0.35 + 0.65 * breathe(t, 0.9))
                                          : used ? RGB(0x6a6a74).color : .clear)
                                    .overlay(Circle().strokeBorder(used || on || blink ? .clear : RGB(0x3a3a44).color, lineWidth: 1.2))
                                    .frame(width: 9, height: 9)
                                    .shadow(color: blink || on ? FZ.cyan.opacity(0.8) : .clear, radius: 4)
                                    .frame(width: cell, height: Self.row)
                            }
                        }
                    }
                }
            }
            .padding(Self.pad)
            .frame(width: Self.width, alignment: .leading)
            .background(RoundedRectangle(cornerRadius: 18).fill(RGB(0x0d1418).color))
            .overlay(RoundedRectangle(cornerRadius: 18).strokeBorder(FZ.cyan.color, lineWidth: 2))
            .shadow(color: FZ.cyan.opacity(0.45), radius: 16)
        }
        .onGeometryChange(for: CGRect.self) { $0.frame(in: .named(ProjectsDropZones.space)) } action: { r in
            pm.zones.card = .init(q: q, rect: r) { p in
                let x = p.x - pad - letter, y = p.y - pad - head - 6
                guard x >= 0, y >= 0, x < cell * 16, y < row * 4 else { return nil }
                return Int(y / row) * 16 + Int(x / cell)
            }
        }
        .onDisappear { if pm.zones.card?.q == q { pm.zones.card = nil } }
        .allowsHitTesting(false)
    }
    private var pad: CGFloat { Self.pad }
    private var letter: CGFloat { Self.letter }
    private var head: CGFloat { Self.head }
    private var row: CGFloat { Self.row }
}

/// "+n POOL" in a project's head while a pattern is held over it (its sounds that come along), "FULL" when it refuses.
struct DropPoolPill: View {
    let pm: ProjectsModel
    let q: Int
    var body: some View {
        if pm.lift != nil, let t = pm.plan?.target(q), t.added > 0 || t.full {
            let tint = t.full ? FZ.red : FZ.cyan
            HStack(spacing: 6) {
                IconView(ProjIcons.slots, color: tint.color, lineWidth: 2.2).frame(width: 15, height: 15)
                Text(t.full ? "POOL FULL" : "+\(t.added) POOL").fz(13, 800, em: 0.04).foregroundStyle(tint.color)
            }
            .padding(.horizontal, 12).frame(height: 36)
            .background(Capsule().fill(tint.opacity(0.1)))
            .overlay(Capsule().strokeBorder(tint.color, lineWidth: 1.5))
            .fixedSize()
            .transition(.scale.combined(with: .opacity))
        }
    }
}

// MARK: - The TO… sheet

/// The phone: TO… over everything (the tab bar too), from the bottom; PhoneRoot draws it.
public struct ProjectsToLayer: View {
    let model: UnitModel
    public init(model: UnitModel) { self.model = model }
    public var body: some View {
        let pm = model.projectsTab
        ZStack(alignment: .bottom) {
            if pm.to != nil {
                Color.black.opacity(0.6).ignoresSafeArea().onTapGesture { pm.closeTo() }
                    .transition(.opacity)
                ProjectsToSheet(pm: pm)
                    .transition(.move(edge: .bottom))
            }
        }
        .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .bottom)
        .animation(.spring(response: 0.4, dampingFraction: 0.86), value: pm.to == nil)
    }
}

/// TO… (projtab.js sheet): the pattern and its project, COPY / MOVE, the projects (a full pool's dashed red, FULL),
/// the target project's 64 patterns (used grey: asked first; its own place dashed), the pool line, CANCEL, FIRST FREE.
struct ProjectsToSheet: View {
    let pm: ProjectsModel
    /// iPad: a card in the middle (no grabber, all corners round)
    var card = false

    var body: some View {
        if let t = pm.to {
            let free = pm.firstFree(t.q), target = pm.plan?.target(t.q)
            VStack(alignment: .leading, spacing: 14) {
                if !card { Capsule().fill(RGB(0x3a3a44).color).frame(width: 38, height: 5).frame(maxWidth: .infinity) }
                head(t)
                projects(t)
                patterns(t, free: free)
                poolLine(target)
                HStack(spacing: 10) {
                    Button { pm.closeTo() } label: {
                        Text("CANCEL").fz(15, 800, em: 0.08).foregroundStyle(FZ.text.color)
                            .frame(maxWidth: .infinity).frame(height: 56)
                            .overlay(Capsule().strokeBorder(FZ.line.color, lineWidth: 1.5))
                            .contentShape(Capsule())
                    }
                    .buttonStyle(ProjPress())
                    .frame(width: 118)
                    .keyboardShortcut(.escape, modifiers: [])
                    let ok = free != nil && target?.full != true
                    Button { Task { await pm.toPick(nil) } } label: {
                        HStack(spacing: 10) {
                            IconView(t.move ? ProjIcons.move : ProjIcons.copy, color: .black, lineWidth: 2.5, backdrop: FZ.cyan.color)
                                .frame(width: 18, height: 18)
                            Text("FIRST FREE").fz(16, 800, em: 0.08).foregroundStyle(.black)
                            if let free {
                                Text(FizzClip.patName(free)).fz(15, 800).foregroundStyle(.black)
                                    .padding(.horizontal, 9).frame(height: 30)
                                    .background(RoundedRectangle(cornerRadius: 9).fill(.black.opacity(0.14)))
                            }
                        }
                        .frame(maxWidth: .infinity).frame(height: 56)
                        .background(Capsule().fill(FZ.cyan.color))
                        .shadow(color: FZ.cyan.opacity(0.5), radius: 14)
                        .contentShape(Capsule())
                    }
                    .buttonStyle(ProjPress())
                    .disabled(!ok)
                    .opacity(ok ? 1 : 0.4)
                    .keyboardShortcut(.return, modifiers: [])
                }
            }
            .padding(.horizontal, 18).padding(.top, card ? 18 : 10).padding(.bottom, card ? 18 : 34)
            .frame(maxWidth: card ? 560 : .infinity)
            .background(
                UnevenRoundedRectangle(topLeadingRadius: 30, bottomLeadingRadius: card ? 30 : 0, bottomTrailingRadius: card ? 30 : 0,
                                       topTrailingRadius: 30)
                    .fill(RGB(0x0d0d11).color)
                    .shadow(color: FZ.cyan.opacity(0.25), radius: 18, y: -4)
            )
            .overlay(
                UnevenRoundedRectangle(topLeadingRadius: 30, bottomLeadingRadius: card ? 30 : 0, bottomTrailingRadius: card ? 30 : 0,
                                       topTrailingRadius: 30)
                    .strokeBorder(FZ.cyan.mix(55, FZ.line).color, lineWidth: 1.5)
            )
            .animation(.spring(response: 0.35, dampingFraction: 0.8), value: t)
            .animation(.easeOut(duration: 0.2), value: pm.plan)
        }
    }

    private func head(_ t: ProjectsTo) -> some View {
        HStack(spacing: 10) {
            Text(FizzClip.patName(t.k)).fz(20, 800).foregroundStyle(.black)
                .padding(.horizontal, 12).frame(height: 40)
                .background(RoundedRectangle(cornerRadius: 11).fill(FZ.cyan.color))
                .shadow(color: FZ.cyan.opacity(0.45), radius: 9)
            Text(pm.slot(t.p).name).fz(15, 800, em: 0.04).foregroundStyle(FZ.muted.color).lineLimit(1).minimumScaleFactor(0.6)
            Spacer(minLength: 4)
            HStack(spacing: 0) {
                mode("COPY", ProjIcons.copy, on: !t.move) { pm.setToMove(false) }
                mode("MOVE", ProjIcons.move, on: t.move) { pm.setToMove(true) }
            }
            .padding(3)
            .overlay(Capsule().strokeBorder(FZ.line.color, lineWidth: 1.5))
        }
    }

    private func mode(_ title: String, _ icon: VectorArt, on: Bool, _ action: @escaping () -> Void) -> some View {
        Button(action: action) {
            HStack(spacing: 7) {
                IconView(icon, color: on ? .black : FZ.muted.color, lineWidth: 2.4, backdrop: on ? FZ.cyan.color : .black)
                    .frame(width: 15, height: 15)
                Text(title).fz(13, 800, em: 0.08).foregroundStyle(on ? Color.black : FZ.muted.color)
            }
            .padding(.horizontal, 14).frame(height: 38)
            .background(Capsule().fill(on ? FZ.cyan.color : .clear))
            .contentShape(Capsule())
        }
        .buttonStyle(ProjPress())
    }

    private func projects(_ t: ProjectsTo) -> some View {
        let ps = pm.slots.filter(\.there)
        return LazyVGrid(columns: Array(repeating: GridItem(.flexible(), spacing: 8), count: 3), spacing: 8) {
            ForEach(ps) { s in
                let on = s.p == t.q, full = pm.plan?.target(s.p)?.full == true
                Button { pm.setToProject(s.p) } label: {
                    HStack(spacing: 8) {
                        Text("\(s.p + 1)").fz(13, 800).foregroundStyle(.black).frame(width: 24, height: 24)
                            .background(Circle().fill((full ? RGB(0x5d5d66) : s.current ? FZ.cyan : FZ.green).color))
                        Text(s.name).fz(13, 800, em: 0.02).foregroundStyle((full ? FZ.muted : FZ.text).color)
                            .lineLimit(1).minimumScaleFactor(0.7)
                            .overlay(alignment: .bottom) { if full { Rectangle().fill(FZ.red.color).frame(height: 2).offset(y: 5) } }
                        Spacer(minLength: 0)
                    }
                    .padding(.leading, 8).padding(.trailing, 6).frame(height: 46)
                    .background(RoundedRectangle(cornerRadius: 14).fill(on ? FZ.cyan.mix(14, FZ.raised).color : FZ.raised.color))
                    .overlay {
                        if full {
                            RoundedRectangle(cornerRadius: 14).strokeBorder(FZ.red.color, style: StrokeStyle(lineWidth: 1.5, dash: [5, 4]))
                        } else {
                            RoundedRectangle(cornerRadius: 14).strokeBorder(on ? FZ.cyan.color : FZ.line.color, lineWidth: on ? 2 : 1.5)
                        }
                    }
                    .overlay(alignment: .topTrailing) {
                        if full {
                            Text("FULL").fz(11, 800, em: 0.06).foregroundStyle(.white).padding(.horizontal, 5).frame(height: 17)
                                .background(RoundedRectangle(cornerRadius: 5).fill(FZ.red.color)).offset(x: 4, y: -7)
                        }
                    }
                    .shadow(color: on ? FZ.cyan.opacity(0.35) : .clear, radius: 8)
                    .contentShape(RoundedRectangle(cornerRadius: 14))
                }
                .buttonStyle(ProjPress())
            }
        }
    }

    private func patterns(_ t: ProjectsTo, free: Int?) -> some View {
        let pr = pm.view?.project(t.q)
        return Animated(still: 0.3) { tm in
            VStack(spacing: 6) {
                ForEach(0..<4, id: \.self) { b in
                    HStack(spacing: 4) {
                        Text(["A", "B", "C", "D"][b]).fz(12, 800).foregroundStyle(FZ.muted.color).frame(width: 14, alignment: .leading)
                        ForEach(0..<16, id: \.self) { i in
                            let k = b * 16 + i, used = pr?.pats[k] != nil, own = t.q == t.p && k == t.k, first = k == free
                            Button { Task { await pm.toPick(k) } } label: {
                                RoundedRectangle(cornerRadius: 5)
                                    .fill(first ? FZ.cyan.opacity(0.55 + 0.45 * breathe(tm, 1.1)) : used ? RGB(0x2e2e36).color
                                          : RGB(0x111116).color)
                                    .overlay {
                                        if own {
                                            RoundedRectangle(cornerRadius: 5).strokeBorder(FZ.cyan.color, style: StrokeStyle(lineWidth: 1.5, dash: [3, 2]))
                                        } else {
                                            RoundedRectangle(cornerRadius: 5).strokeBorder((first ? FZ.cyan : used ? RGB(0x3a3a44) : RGB(0x26262e)).color,
                                                                                           lineWidth: 1.2)
                                        }
                                    }
                                    .shadow(color: first ? FZ.cyan.opacity(0.7) : .clear, radius: 6)
                                    .frame(height: 22)
                                    .contentShape(Rectangle())
                            }
                            .buttonStyle(ProjPress(scale: 0.85))
                            .disabled(own)
                        }
                    }
                }
            }
        }
    }

    @ViewBuilder private func poolLine(_ t: FizzDropPlan.Target?) -> some View {
        let pl = pm.plan
        HStack(spacing: 8) {
            IconView(ProjIcons.slots, color: FZ.green.color, lineWidth: 2.2).frame(width: 18, height: 18)
                .frame(width: 38, height: 38)
                .background(RoundedRectangle(cornerRadius: 11).fill(FZ.green.opacity(0.1)))
                .overlay(RoundedRectangle(cornerRadius: 11).strokeBorder(FZ.green.mix(60, FZ.line).color, lineWidth: 1.5))
            if let t, let pl {
                let cols = pl.colours.map(keptTint)
                let added = cols.indices.filter { !(t.had[safe: $0] ?? false) }.map { cols[$0] }
                let there = cols.indices.filter { t.had[safe: $0] ?? false }.map { cols[$0] }
                if t.full {
                    chip("POOL FULL", [], tint: FZ.red, filled: true)
                } else if t.added > 0 {
                    chip("+\(t.added)", added, tint: FZ.green, filled: true)
                }
                if t.there > 0 { chip("\(t.there)", there, tint: FZ.body, filled: false, after: "THERE") }
                if pl.sounds == 0 { Text("NO POOL SOUNDS").fz(12, 800, em: 0.06).foregroundStyle(FZ.muted.color) }
                Spacer(minLength: 4)
                VStack(alignment: .trailing, spacing: 5) {
                    HStack(alignment: .firstTextBaseline, spacing: 2) {
                        Text("\(t.after)").fz(16, 800).foregroundStyle((t.full ? FZ.red : FZ.text).color).contentTransition(.numericText())
                        Text("/ 128").fz(13, 800).foregroundStyle(FZ.muted.color)
                    }
                    GeometryReader { g in
                        Capsule().fill(FZ.track.color)
                            .overlay(alignment: .leading) {
                                Capsule().fill((t.full ? FZ.red : FZ.green).color).frame(width: max(4, g.size.width * min(1, Double(t.after) / 128)))
                            }
                    }
                    .frame(width: 74, height: 6)
                }
                .fixedSize()
            } else {
                Spacer(minLength: 0)
            }
        }
        .frame(height: 40)
    }

    private func chip(_ n: String, _ dots: [RGB], tint: RGB, filled: Bool, after: String? = nil) -> some View {
        HStack(spacing: 5) {
            Text(n).fz(14, 800).foregroundStyle(tint.color)
            ForEach(Array(dots.prefix(6).enumerated()), id: \.offset) { _, c in Circle().fill(c.color).frame(width: 8, height: 8) }
            if let after { Text(after).fz(13, 800, em: 0.06).foregroundStyle(FZ.text.color) }
        }
        .padding(.horizontal, 12).frame(height: 34)
        .background(Capsule().fill(filled ? tint.opacity(0.1) : .clear))
        .overlay(Capsule().strokeBorder((filled ? tint : FZ.line).color, lineWidth: 1.5))
        .fixedSize()
    }
}

fileprivate extension Array {
    subscript(safe i: Int) -> Element? { indices.contains(i) ? self[i] : nil }
}
