// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// SELECT's screens (mock-ups 41, 42, 47): SELECT / DONE; the phone's cream bar in the tab bar's place (the count and
// the ticked sounds' colours, ALL, EXPORT, DELETE; TAG ▾, + POOL n, − POOL n; SEND n TO the other unit); the TAG
// sheet (the seven tags with their icons and NONE dashed, dots: how many of the ticked have it); the DELETE sheet (the
// MY presets named, the ones a project uses marked, the projects and their steps); the iPad's panel in the detail
// column (the ticked named with where SEND puts them, TAG's row in place).

import SwiftUI
import FizzData
import FizzKit

extension Icons {
    /// NONE: a ring struck through
    static let none = VectorArt(24, 24, [.circle(12, 12, 8.5), .path("M6 18L18 6")])
}

// MARK: - SELECT / DONE

/// SELECT (an outline, the rows' empty tick before it) / DONE (cream, a tick), as KEPT's head has them.
struct SelectPill: View {
    let selecting: Bool
    var enabled = true
    var color: RGB = FZ.cream
    let action: () -> Void

    var body: some View {
        Button(action: action) {
            HStack(spacing: 7) {
                if selecting {
                    IconView(KeptArt.check, color: .black, lineWidth: 2.6, backdrop: color.color).frame(width: 15, height: 15)
                } else {
                    Circle().strokeBorder(color.color, lineWidth: 2).frame(width: 16, height: 16)
                }
                Text(selecting ? "DONE" : "SELECT").fz(14, 800, em: 0.08)
            }
            .foregroundStyle(selecting ? Color.black : FZ.text.color)
            .padding(.horizontal, 16).frame(height: 44)
            .background(Capsule().fill(selecting ? color.color : .clear))
            .overlay(Capsule().strokeBorder(selecting ? .clear : color.opacity(0.55), lineWidth: 1.5))
            .shadow(color: selecting ? color.opacity(0.4) : .clear, radius: 10)
            .contentShape(Capsule())
            .fixedSize()
        }
        .buttonStyle(PressStyle(scale: 0.95))
        .disabled(!enabled)
        .opacity(enabled ? 1 : 0.4)
    }
}

/// The ticked sounds' colours as rings, overlapping (the bar's and the sheets' heads).
struct BulkRings: View {
    let tints: [RGB]
    var size: CGFloat = 22
    var body: some View {
        HStack(spacing: -size * 0.32) {
            ForEach(Array(tints.enumerated()), id: \.offset) { _, c in
                Circle().strokeBorder(c.color, lineWidth: size * 0.14).frame(width: size, height: size)
            }
        }
        .animation(.spring(response: 0.3, dampingFraction: 0.75), value: tints)
    }
}

// MARK: - The bar's parts

/// The count, the rings, ALL, EXPORT, DELETE (manager.js .bkhead).
struct BulkHead: View {
    let counts: BulkCounts
    let can: Bool
    var numberSize: CGFloat = 46
    let all: () -> Void
    let export: () -> Void
    let delete: () -> Void

    var body: some View {
        HStack(spacing: 8) {
            Text("\(counts.n)").fz(numberSize, 800).foregroundStyle(FZ.text.color).contentTransition(.numericText())
                .shadow(color: FZ.cream.opacity(0.3), radius: 10)
                .fixedSize()
            BulkRings(tints: counts.tints, size: 20)
            Spacer(minLength: 0)
            Button(action: all) {
                Text("ALL").fz(14, 800, em: 0.08).foregroundStyle(FZ.text.color)
                    .padding(.horizontal, 16).frame(height: 40)
                    .overlay(Capsule().strokeBorder(FZ.line.color, lineWidth: 1.5))
                    .frame(height: 44).contentShape(Capsule())
                    .fixedSize()
            }
            .buttonStyle(PressStyle(scale: 0.95))
            RoundIcon(art: Icons.share, color: FZ.violet, enabled: counts.n > 0, size: 40, action: export)
            // (DELETE: MY PRESETS only, so greyed with only FACTORY rows ticked)
            RoundIcon(art: Icons.trash, color: FZ.red, enabled: !counts.mine.isEmpty && can, size: 40, action: delete)
        }
    }
}

/// TAG ▾, + POOL n, − POOL n (manager.js .bktools).
struct BulkTools: View {
    let counts: BulkCounts
    let can: Bool
    /// the iPad's TAG row shown (its caret turns up)
    var tagsOpen = false
    var height: CGFloat = 46
    let tag: () -> Void
    let poolAdd: () -> Void
    let poolRemove: () -> Void

    var body: some View {
        // (a narrow column: TAG on its own line, the pool's two under it)
        ViewThatFits(in: .horizontal) {
            HStack(spacing: 8) {
                tagButton
                pool("+ POOL", counts.outPool.count, poolAdd)
                pool("− POOL", counts.inPool.count, poolRemove)
            }
            VStack(spacing: 8) {
                tagButton
                HStack(spacing: 8) {
                    pool("+ POOL", counts.outPool.count, poolAdd)
                    pool("− POOL", counts.inPool.count, poolRemove)
                }
            }
        }
    }

    private var tagButton: some View {
            Button(action: tag) {
                Text(tagsOpen ? "TAG ▴" : "TAG ▾").fz(14, 800, em: 0.08)
                    .foregroundStyle(tagsOpen ? Color.black : FZ.text.color)
                    .frame(maxWidth: .infinity).frame(height: height)
                    .background(Capsule().fill(tagsOpen ? FZ.cream.color : .clear))
                    .overlay(Capsule().strokeBorder(tagsOpen ? .clear : FZ.cream.mix(45, FZ.line).color, lineWidth: 1.5))
                    .contentShape(Capsule())
            }
            .buttonStyle(PressStyle(scale: 0.95))
            .disabled(!(can && !counts.unit.isEmpty))
            .opacity(can && !counts.unit.isEmpty ? 1 : 0.4)
    }

    private func pool(_ t: String, _ n: Int, _ action: @escaping () -> Void) -> some View {
        let on = can && n > 0
        return Button(action: action) {
            HStack(spacing: 7) {
                Text(t).fz(13, 800, em: 0.06).lineLimit(1).fixedSize()
                if n > 0 {
                    Text("\(n)").fz(13, 800).foregroundStyle(.black).monospacedDigit()
                        .padding(.horizontal, 5).frame(minWidth: 24).frame(height: 24)
                        .background(Capsule().fill(FZ.green.color))
                        .contentTransition(.numericText())
                }
            }
            .foregroundStyle(FZ.text.color)
            .padding(.horizontal, 14)
            .frame(maxWidth: .infinity).frame(height: height)
            .background(Capsule().fill(FZ.green.opacity(0.08)))
            .overlay(Capsule().strokeBorder(FZ.green.mix(65, FZ.line).color, lineWidth: 1.5))
            .contentShape(Capsule())
        }
        .buttonStyle(PressStyle(scale: 0.95))
        .disabled(!on)
        .opacity(on ? 1 : 0.4)
    }
}

/// SEND n TO <the other unit> in its colour, breathing (→ KEPT n with no other unit); a bulk edit's count while one
/// runs.
struct BulkSendButton: View {
    let n: Int
    let other: OtherUnit?
    let run: BulkProgress?
    let enabled: Bool
    var height: CGFloat = 58
    let action: () -> Void
    @AppStorage("play.theme") private var theme = PlayTheme.tangerine.rawValue

    var body: some View {
        let c = other.map { unitTint($0.key, theme: theme) } ?? FZ.champagne
        let icon = other.map { $0.isEmulated ? KeptArt.phone : UnitArt.fm1 } ?? KeptArt.phone
        Button(action: action) {
            Animated(still: 0.6, active: enabled && run == nil) { t in
                HStack(spacing: 10) {
                    if run == nil { IconView(icon, color: .black, lineWidth: 2.3, backdrop: c.color).frame(width: 20, height: 20) }
                    Text(run.map { "\($0.label) \(min($0.done + 1, $0.of)) / \($0.of)" }
                         ?? (other.map { "SEND \(n) TO \($0.name)" } ?? "\(n) → KEPT"))
                        .fz(17, 800, em: 0.07).lineLimit(1).minimumScaleFactor(0.6)
                        .contentTransition(.numericText())
                }
                .foregroundStyle(.black)
                .padding(.horizontal, 16)
                .frame(maxWidth: .infinity).frame(height: height)
                .background(Capsule().fill(c.color))
                .shadow(color: c.opacity(enabled ? 0.35 + 0.3 * breathe(t, 2.4) : 0), radius: 10 + 10 * breathe(t, 2.4))
                .contentShape(Capsule())
            }
        }
        .buttonStyle(PressStyle(scale: 0.96))
        .disabled(!enabled || run != nil)
        .opacity(enabled || run != nil ? 1 : 0.4)
    }
}

// MARK: - The phone's bar (mock-up 41)

/// The cream bar in the tab bar's place while SELECT is on (the unit's side).
struct BulkBar: View {
    let flow: BulkFlow
    let shown: [FizzListEntry]
    let tag: () -> Void

    var body: some View {
        let c = flow.counts, picks = flow.model.picks
        VStack(spacing: 12) {
            BulkHead(counts: c, can: flow.can, all: { picks.toggleAll(shown) }, export: flow.export, delete: flow.askDelete)
            BulkTools(counts: c, can: flow.can, tag: tag, poolAdd: flow.poolAdd, poolRemove: flow.poolRemove)
            BulkSendButton(n: c.n, other: flow.model.other, run: picks.run,
                           enabled: c.n > 0 && !flow.model.sendingOther && (flow.model.other != nil || flow.model.actions.link != nil),
                           action: flow.send)
        }
        .padding(14)
        .background(RoundedRectangle(cornerRadius: 28).fill(FZ.cream.mix(5, RGB(0x0d0d11)).color))
        .overlay(RoundedRectangle(cornerRadius: 28).strokeBorder(FZ.cream.color, lineWidth: 1.5))
        .shadow(color: FZ.cream.opacity(0.18), radius: 18)
        .shadow(color: .black.opacity(0.8), radius: 16, y: 10)
        .animation(.spring(response: 0.3, dampingFraction: 0.8), value: c.n)
    }
}

// MARK: - TAG (mock-up 42, left)

/// A tag's tile: its icon and name in its colour (NONE dashed); dots: how many of the ticked have it; lit when they
/// all have only it (a tap then changes nothing), and for a moment when tapped.
struct BulkTagTile: View {
    let tag: FizzTag?
    let have: Int
    let lit: Bool
    var height: CGFloat = 70
    let action: () -> Void

    var body: some View {
        let c = tag?.tint ?? FZ.muted
        let ink: Color = lit ? (tag?.ink ?? .black) : tag == nil ? FZ.muted.color : FZ.text.color
        Button(action: action) {
            HStack(spacing: 10) {
                Group {
                    if let tag { TagIcon(tag, color: lit ? ink : c.color, lineWidth: 2.4) } else {
                        IconView(Icons.none, color: lit ? ink : c.color, lineWidth: 2.4, backdrop: .clear)
                    }
                }
                .frame(width: 30, height: 30)
                Text(tag?.name ?? "NONE").fz(18, 800, em: 0.07).foregroundStyle(ink).lineLimit(1).minimumScaleFactor(0.7).layoutPriority(1)
                Spacer(minLength: 0)
                if have > 0 {
                    HStack(spacing: 3) {
                        ForEach(0..<min(have, 4), id: \.self) { _ in Circle().fill(lit ? ink : c.color).frame(width: 7, height: 7) }
                    }
                }
            }
            .padding(.horizontal, 14)
            .frame(maxWidth: .infinity).frame(height: height)
            .background(RoundedRectangle(cornerRadius: 16).fill(lit ? c.color : tag == nil ? Color.clear : c.opacity(0.1)))
            .overlay {
                if tag == nil {
                    RoundedRectangle(cornerRadius: 16).strokeBorder(FZ.muted.mix(60, FZ.line).color, style: StrokeStyle(lineWidth: 2, dash: [6, 5]))
                } else {
                    RoundedRectangle(cornerRadius: 16).strokeBorder(lit ? c.color : c.mix(70, FZ.line).color, lineWidth: 2)
                }
            }
            .shadow(color: lit ? c.opacity(0.55) : .clear, radius: 12)
            .contentShape(RoundedRectangle(cornerRadius: 16))
        }
        .buttonStyle(PressStyle(scale: 0.95))
    }
}

/// The TAG sheet's body: TAG, the rings, the count; the seven tiles and NONE. One tap tags them all.
struct BulkTagGrid: View {
    let counts: BulkCounts
    let pick: (FizzTag?) -> Void
    @State private var tapped: Int?

    var body: some View {
        LazyVGrid(columns: [GridItem(.flexible(), spacing: 10), GridItem(.flexible(), spacing: 10)], spacing: 10) {
            ForEach(FizzTag.allCases) { t in
                BulkTagTile(tag: t, have: counts.have(t), lit: tapped == t.rawValue || (tapped == nil && counts.all(1 << t.rawValue))) { go(t) }
            }
            BulkTagTile(tag: nil, have: 0, lit: tapped == -1) { go(nil) }
        }
    }

    /// The tile rings, then the sheet drops and the tags go out.
    private func go(_ t: FizzTag?) {
        withAnimation(.spring(response: 0.2, dampingFraction: 0.7)) { tapped = t?.rawValue ?? -1 }
        Task {
            try? await Task.sleep(for: .milliseconds(180))
            pick(t)
        }
    }
}

struct BulkTagHeader: View {
    let counts: BulkCounts
    var body: some View {
        HStack(spacing: 12) {
            Text("TAG").fz(34, 800, em: -0.01).foregroundStyle(FZ.text.color)
            BulkRings(tints: counts.tints, size: 24)
            Spacer(minLength: 0)
            Text("\(counts.unit.count)").fz(34, 800).foregroundStyle(FZ.text.color)
        }
    }
}

struct BulkTagSheet: View {
    let flow: BulkFlow
    let close: () -> Void

    var body: some View {
        FizzSheet {
            BulkTagHeader(counts: flow.counts)
        } content: {
            BulkTagGrid(counts: flow.counts) { t in
                close()
                flow.tag(t)
            }
            .padding(.bottom, 14)
        }
    }
}

/// The iPad's TAG row in the panel: the tags as chips with their icons, NONE last.
struct BulkTagRow: View {
    let counts: BulkCounts
    let pick: (FizzTag?) -> Void

    var body: some View {
        WrapLayout(spacing: 8) {
            ForEach(FizzTag.allCases) { t in
                chip(t.name, t.tint, ink: t.ink, lit: true) { TagIcon(t, color: t.ink, lineWidth: 2.2) } action: { pick(t) }
            }
            chip("NONE", FZ.muted, ink: FZ.muted.color, lit: false) {
                IconView(Icons.none, color: FZ.muted.color, lineWidth: 2.2, backdrop: .clear)
            } action: { pick(nil) }
        }
    }

    private func chip<I: View>(_ t: String, _ c: RGB, ink: Color, lit: Bool, @ViewBuilder icon: () -> I,
                               action: @escaping () -> Void) -> some View {
        Button(action: action) {
            HStack(spacing: 7) {
                icon().frame(width: 15, height: 15)
                Text(t).fz(14, 800, em: 0.06)
            }
            .foregroundStyle(ink)
            .padding(.horizontal, 12).frame(height: 38)
            .background(RoundedRectangle(cornerRadius: 9).fill(lit ? c.color : .clear))
            .overlay(RoundedRectangle(cornerRadius: 9).strokeBorder(lit ? .clear : FZ.line.color, lineWidth: 1.5))
            .contentShape(RoundedRectangle(cornerRadius: 9))
            .fixedSize()
        }
        .buttonStyle(PressStyle(scale: 0.93))
    }
}

// MARK: - DELETE (mock-up 42, right)

/// The DELETE sheet's body: the MY presets named (a yellow mark: a project uses it), the box naming the projects and
/// the steps that go back to their track's own sound; the factory's stay.
struct BulkDeleteBody: View {
    let model: UnitModel
    let ask: BulkDeleteAsk

    private var usage: BulkUsage { BulkUsage(ask.mine, users: model.actions.users) }

    var body: some View {
        let u = usage
        VStack(alignment: .leading, spacing: 8) {
            ForEach(ask.mine.prefix(8), id: \.slot) { r in
                HStack(spacing: 12) {
                    Text("\(r.slot)").fz(14, 700).monospacedDigit().foregroundStyle(FZ.muted.color).frame(width: 30, alignment: .trailing)
                    AlgoGlyph(algorithm: model.algorithm(r), color: FizzTag.tint(mask: r.tags).color).frame(width: 26, height: 26)
                    Text(r.name).fz(15, 800, em: 0.03).foregroundStyle(FZ.text.color).lineLimit(1)
                    Spacer(minLength: 0)
                    if u.used.contains(r) {
                        IconView(TransferIcons.warn, color: .black, lineWidth: 2.4, backdrop: FZ.yellow.color).frame(width: 14, height: 14)
                            .frame(width: 26, height: 26).background(Circle().fill(FZ.yellow.color))
                    }
                }
                .padding(.horizontal, 12).frame(height: 48)
                .background(RoundedRectangle(cornerRadius: 12).fill(FZ.raised.color))
                .overlay(RoundedRectangle(cornerRadius: 12).strokeBorder(FZ.line.color, lineWidth: 1))
            }
            if ask.mine.count > 8 {
                Text("+\(ask.mine.count - 8)").fz(14, 800).foregroundStyle(FZ.muted.color).padding(.leading, 12)
            }
            if !u.used.isEmpty { usedBox(u).padding(.top, 6) }
            if ask.factory > 0 {
                Text("THE FACTORY'S \(ask.factory) STAY").fz(13, 800, em: 0.06).foregroundStyle(FZ.muted.color).padding(.top, 4)
            }
        }
    }

    private func usedBox(_ u: BulkUsage) -> some View {
        VStack(alignment: .leading, spacing: 12) {
            HStack(spacing: 8) {
                IconView(TransferIcons.warn, color: FZ.yellow.color, lineWidth: 2.4, backdrop: .clear).frame(width: 18, height: 18)
                Text("\(u.used.count)").fz(18, 800).foregroundStyle(FZ.yellow.color)
                Text("USED BY").fz(13, 800, em: 0.1).foregroundStyle(FZ.yellow.color)
            }
            WrapLayout(spacing: 8) {
                ForEach(u.projects, id: \.project) { p in UsedChip(name: projectName(p.project), steps: p.steps) }
            }
            Text(u.steps > 0 ? "\(u.steps) STEP\(u.steps == 1 ? "" : "S") → THEIR TRACK'S OWN SOUND" : "OUT OF THEIR POOLS")
                .fz(12, 800, em: 0.06).foregroundStyle(FZ.body.color)
        }
        .padding(14)
        .frame(maxWidth: .infinity, alignment: .leading)
        .background(RoundedRectangle(cornerRadius: 16).fill(FZ.yellow.opacity(0.08)))
        .overlay(RoundedRectangle(cornerRadius: 16).strokeBorder(FZ.yellow.color, lineWidth: 2))
    }

    private func projectName(_ p: Int) -> String {
        let l = model.projects?.list ?? []
        let n = p >= 1 && p <= l.count ? l[p - 1].name : ""
        return n.isEmpty ? "PROJECT \(p)" : n
    }
}

struct BulkDeleteHeader: View {
    let n: Int
    let unit: String
    var body: some View {
        HStack(spacing: 14) {
            IconView(Icons.trash, color: FZ.red.color, lineWidth: 2.4).frame(width: 26, height: 26)
                .frame(width: 58, height: 58)
                .background(RoundedRectangle(cornerRadius: 16).fill(FZ.red.opacity(0.14)))
                .overlay(RoundedRectangle(cornerRadius: 16).strokeBorder(FZ.red.color, lineWidth: 2))
            VStack(alignment: .leading, spacing: 2) {
                Text("DELETE \(n)").fz(32, 800, em: -0.01).foregroundStyle(FZ.text.color)
                Text("MY PRESETS · \(unit)").fz(13, 800, em: 0.08).foregroundStyle(FZ.muted.color)
            }
        }
    }
}

/// CANCEL / DELETE n.
struct BulkDeleteButtons: View {
    let n: Int
    var ready = true
    let cancel: () -> Void
    let go: () -> Void
    var body: some View {
        HStack(spacing: 10) {
            PillButton(title: "CANCEL", ghost: true, height: 56, action: cancel)
            IconPill(title: "DELETE \(n)", icon: Icons.trash, color: FZ.red, height: 56, enabled: ready, action: go)
        }
    }
}

struct BulkDeleteSheet: View {
    let flow: BulkFlow
    let ask: BulkDeleteAsk
    let close: () -> Void
    @State private var read = false

    var body: some View {
        FizzSheet {
            BulkDeleteHeader(n: ask.mine.count, unit: flow.model.unitName)
        } content: {
            BulkDeleteBody(model: flow.model, ask: ask)
        } actions: {
            BulkDeleteButtons(n: ask.mine.count, ready: read || !flow.model.actions.canWrite, cancel: close) {
                close()
                flow.delete(ask)
            }
        }
        // (who uses them: USERS, one each, before DELETE is offered)
        .task {
            _ = await flow.model.actions.bulkUsers(ask.mine)
            read = true
        }
    }
}

// MARK: - The iPad's panel (mock-up 47)

/// The detail column while rows are ticked: the head, the ticked named with where SEND puts them, TAG ▴ with its
/// row in place, + POOL / − POOL, SEND n TO the other unit at the foot.
struct BulkPanel: View {
    let flow: BulkFlow
    let shown: [FizzListEntry]
    @AppStorage("play.theme") private var theme = PlayTheme.tangerine.rawValue

    var body: some View {
        let c = flow.counts, picks = flow.model.picks, model = flow.model
        VStack(alignment: .leading, spacing: 14) {
            BulkHead(counts: c, can: flow.can, numberSize: 52, all: { picks.toggleAll(shown) }, export: flow.export, delete: flow.askDelete)
            if c.n > 0 {
                VStack(spacing: 8) {
                    let to = landing(c.n)
                    ForEach(Array(c.rows.prefix(6).enumerated()), id: \.offset) { i, r in
                        HStack(spacing: 12) {
                            Text("\(r.slot)").fz(14, 700).monospacedDigit().foregroundStyle(FZ.muted.color).frame(width: 30, alignment: .trailing)
                            AlgoGlyph(algorithm: model.algorithm(r), color: FizzTag.tint(mask: r.tags).color).frame(width: 26, height: 26)
                            Text(r.name).fz(15, 800, em: 0.03).foregroundStyle(FZ.text.color).lineLimit(1)
                            Spacer(minLength: 0)
                            if let o = model.other, i < to.count {
                                let tint = unitTint(o.key, theme: theme)
                                HStack(spacing: 4) {
                                    IconView(o.isEmulated ? KeptArt.phone : UnitArt.fm1, color: tint.color, lineWidth: 2, backdrop: .clear)
                                        .frame(width: 13, height: 13)
                                    Text("\(to[i])").fz(14, 800).monospacedDigit().foregroundStyle(tint.color)
                                }
                            }
                        }
                        .padding(.horizontal, 12).frame(height: 46)
                        .background(RoundedRectangle(cornerRadius: 12).fill(FZ.raised.color))
                        .overlay(RoundedRectangle(cornerRadius: 12).strokeBorder(FZ.line.color, lineWidth: 1))
                    }
                    if c.n > 6 {
                        Text("+\(c.n - 6)").fz(14, 800).foregroundStyle(FZ.muted.color).frame(maxWidth: .infinity, alignment: .leading)
                            .padding(.leading, 12)
                    }
                }
            }
            BulkTools(counts: c, can: flow.can, tagsOpen: picks.tagsOpen, height: 48,
                      tag: { withAnimation(.spring(response: 0.3, dampingFraction: 0.8)) { picks.tagsOpen.toggle() } },
                      poolAdd: flow.poolAdd, poolRemove: flow.poolRemove)
            if picks.tagsOpen && flow.can && !c.unit.isEmpty {
                BulkTagRow(counts: c) { flow.tag($0) }
                    .transition(.move(edge: .top).combined(with: .opacity))
            }
            Spacer(minLength: 12)
            BulkSendButton(n: c.n, other: model.other, run: picks.run,
                           enabled: c.n > 0 && !model.sendingOther && (model.other != nil || model.actions.link != nil),
                           action: flow.send)
        }
        .animation(.spring(response: 0.3, dampingFraction: 0.8), value: c.n)
        .task(id: model.other?.key) { if model.other != nil && model.other?.taken == nil { await model.readOther() } }
    }

    /// Where SEND puts them: the other unit's first free MY PRESETS places (manager.js freeOf).
    private func landing(_ n: Int) -> [Int] {
        guard let o = flow.model.other, let t = o.taken else { return [] }
        var out: [Int] = []
        for k in 1...max(1, o.nlib) where !t.contains(k) {
            out.append(k)
            if out.count == n { break }
        }
        return out
    }
}

// MARK: - fizzshots (ImageRenderer draws no sheets)

/// LIBRARY under a sheet as the phone shows it.
private struct UnderSheet<Sheet: View>: View {
    let model: UnitModel
    let selection: String
    @ViewBuilder let sheet: Sheet

    var body: some View {
        ZStack(alignment: .bottom) {
            RootView(model: model, tab: .constant(.library), wide: false)
                .environment(\.fizzShotSelection, selection)
                .blur(radius: 2).brightness(-0.3)
            Color.black.opacity(0.45)
            sheet
                .padding(.horizontal, 20).padding(.top, 30).padding(.bottom, 44)
                .background(SheetShape().fill(sheetBackground.color).shadow(color: .black, radius: 30, y: -10))
                .overlay(alignment: .top) { Capsule().fill(RGB(0x3a3a44).color).frame(width: 40, height: 5).padding(.top, 10) }
        }
    }
}

/// The TAG sheet over LIBRARY's SELECT (the ticked: model.picks).
public struct BulkTagSheetShot: View {
    let model: UnitModel
    let selection: String
    public init(model: UnitModel, selection: String) { self.model = model; self.selection = selection }

    public var body: some View {
        let c = BulkCounts(model.picks.rows(model.rows), places: poolPlaces(model.rows))
        UnderSheet(model: model, selection: selection) {
            VStack(alignment: .leading, spacing: 16) {
                BulkTagHeader(counts: c)
                BulkTagGrid(counts: c) { _ in }
            }
        }
    }
}

/// The DELETE sheet over LIBRARY's SELECT (USED BY read before).
public struct BulkDeleteSheetShot: View {
    let model: UnitModel
    let selection: String
    public init(model: UnitModel, selection: String) { self.model = model; self.selection = selection }

    public var body: some View {
        let rows = model.picks.rows(model.rows), mine = rows.filter { $0.source == "mine" }
        let ask = BulkDeleteAsk(mine: mine, factory: rows.count - mine.count)
        UnderSheet(model: model, selection: selection) {
            VStack(alignment: .leading, spacing: 14) {
                BulkDeleteHeader(n: mine.count, unit: model.unitName)
                BulkDeleteBody(model: model, ask: ask)
                BulkDeleteButtons(n: mine.count, cancel: {}, go: {}).padding(.top, 4)
            }
        }
    }
}
