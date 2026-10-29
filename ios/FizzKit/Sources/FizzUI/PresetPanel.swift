// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// A preset (mock-ups 10, 16; manager.js renderSel): its glyph and name (a tap renames a MY preset), FIZZ's seven tags
// and the web tags, USED BY with each project's steps, LOAD ON FM-1 on one of the six tracks (the unit's own browser:
// OCT- cancels, OCT+ loads), + POOL / − POOL, EDIT, EXPORT, MOVE, DELETE. The phone's sheet (a FizzSheet: the
// buttons pinned at its bottom) and the iPad's third column.

import SwiftUI
import FizzData
import FizzKit

struct PresetPanel: View {
    let model: UnitModel
    let entry: FizzListEntry
    /// the iPad's column (its own order: the buttons at the bottom)
    var wide = false
    @Binding var note: ScreenNote?
    @Binding var ask: ConfirmAsk?
    var onExport: ([FizzListEntry]) -> Void = { _ in }
    /// MOVE (a MY preset): its place swapped with the next MY preset tapped
    var onMove: ((FizzListEntry) -> Void)?
    /// the preset went (deleted) or the screen moved on (EDIT)
    var onClose: () -> Void = {}

    @Environment(\.fizzSnapshot) private var snapshot
    @State private var naming = false
    @State private var name = ""
    @FocusState private var nameFocus: Bool
    @State private var webKey: String?
    @State private var addingTag = false
    @State private var newTag = ""
    @FocusState private var tagFocus: Bool
    @State private var working = false
    @State private var lastBrowse: Browsing?

    private var actions: UnitActions { model.actions }
    private var mine: Bool { entry.source == "mine" }
    private var can: Bool { actions.canWrite && !working }
    private var tint: RGB { FizzTag.tint(mask: entry.tags) }
    private var places: [Int] { actions.places(of: entry.id) }

    var body: some View {
        Group {
            if wide {
                VStack(alignment: .leading, spacing: 0) {
                    header
                    tags.padding(.top, 16)
                    if mine { usedBy.padding(.top, 18) }
                    load.padding(.top, 18)
                    Spacer(minLength: 18)
                    mainButtons
                    lastButtons.padding(.top, 10)
                }
            } else {
                // (the phone's sheet: everything above the fold at its first height, the buttons pinned)
                FizzSheet {
                    header
                } content: {
                    VStack(alignment: .leading, spacing: 0) {
                        tags
                        if mine { usedBy.padding(.top, 18) }
                        load.padding(.top, 18)
                    }
                } actions: {
                    VStack(spacing: 10) {
                        mainButtons
                        lastButtons
                    }
                }
            }
        }
        .task(id: "\(entry.source):\(entry.slot):\(entry.id):\(entry.name):\(actions.frame(of: entry) != nil)") {
            webKey = await actions.soundKey(of: entry)
        }
        .onChange(of: actions.browsing) { old, new in
            guard let old, new == nil, old.id == entry.id, let loaded = actions.browseLoaded else { return }
            note = loaded ? .done("TRACK \(old.track + 1) · \(old.name)") : .done("TRACK \(old.track + 1) KEPT ITS OWN", FZ.muted)
        }
    }

    // MARK: name

    private var header: some View {
        HStack(spacing: 14) {
            AlgoGlyph(algorithm: model.algorithm(entry), color: tint.color).frame(width: 64, height: 64)
            VStack(alignment: .leading, spacing: 2) {
                HStack(spacing: 8) {
                    Heading(text: "\(mine ? "MY PRESETS" : "FACTORY") \(entry.slot)")
                    if !places.isEmpty { PlacesBadge(places: places) }
                    if entry.imported { Text("↓").fz(14, 800).foregroundStyle(FZ.violet.color) }
                }
                if naming {
                    TextField("", text: $name)
                        .font(FizzFont.font(wide ? 36 : 40, 800))
                        .foregroundStyle(FZ.text.color)
                        .tint(FZ.cyan.color)
                        .focused($nameFocus)
                        .submitLabel(.done)
                        .autocorrectionDisabled()
                        #if os(iOS)
                        .textInputAutocapitalization(.characters)
                        #endif
                        .onChange(of: name) { _, v in
                            let c = String(v.uppercased().filter { $0.isASCII && !$0.isNewline }.prefix(12))
                            if c != v { name = c }
                        }
                        .onSubmit { rename() }
                        .onChange(of: nameFocus) { _, f in if !f && naming { rename() } }
                } else {
                    HStack(spacing: 4) {
                        Text(entry.name).fz(wide ? 36 : 40, 800, em: -0.01).foregroundStyle(FZ.text.color)
                            .lineLimit(1).minimumScaleFactor(0.5)
                        if mine && can { Caret(height: wide ? 34 : 38) }
                    }
                    .contentShape(Rectangle())
                    .onTapGesture {
                        guard mine, can else { return }
                        name = entry.name
                        naming = true
                        nameFocus = true
                    }
                }
            }
        }
    }

    private func rename() {
        let v = name
        naming = false
        nameFocus = false
        guard FizzSysEx.presetName(v) != entry.name, !FizzSysEx.presetName(v).isEmpty else { return }
        run { await actions.rename(entry, to: v, webTags: model.webTags) } done: { _ in .done("RENAMED") }
    }

    // MARK: tags

    private var tags: some View {
        let web = model.webTags.of(webKey)
        return WrapLayout(spacing: 7) {
            ForEach(FizzTag.allCases) { t in
                let on = (entry.tags >> t.rawValue) & 1 != 0
                Button {
                    run { await actions.toggleTag(entry, t.rawValue) } done: { _ in nil }
                } label: {
                    Text(t.name).fz(14, 800, em: 0.05)
                        .foregroundStyle(on ? t.ink : FZ.muted.color)
                        .padding(.horizontal, 14).frame(height: 36)
                        .background(RoundedRectangle(cornerRadius: 9).fill(on ? t.tint.color : .clear))
                        .overlay(RoundedRectangle(cornerRadius: 9).strokeBorder(on ? .clear : FZ.line.color, lineWidth: 1.5))
                        .shadow(color: on ? t.tint.opacity(0.35) : .clear, radius: 6)
                }
                .buttonStyle(.plain)
                .disabled(!can)
            }
            ForEach(web, id: \.self) { w in
                Button { if let k = webKey { model.webTags.remove(w, from: k) } } label: {
                    HStack(spacing: 6) {
                        Text(w).fz(14, 800, em: 0.05)
                        IconView(Icons.close, color: FZ.champagne.opacity(0.7), lineWidth: 2.4).frame(width: 10, height: 10)
                    }
                    .foregroundStyle(FZ.champagne.color)
                    .padding(.horizontal, 12).frame(height: 36)
                    .overlay(RoundedRectangle(cornerRadius: 9).strokeBorder(FZ.champagne.opacity(0.7),
                                                                         style: StrokeStyle(lineWidth: 1.5, dash: [4, 3])))
                }
                .buttonStyle(.plain)
            }
            if webKey != nil {
                if addingTag {
                    TextField("", text: $newTag)
                        .font(FizzFont.font(14, 800))
                        .foregroundStyle(FZ.champagne.color)
                        .tint(FZ.champagne.color)
                        .focused($tagFocus)
                        .submitLabel(.done)
                        .autocorrectionDisabled()
                        #if os(iOS)
                        .textInputAutocapitalization(.characters)
                        #endif
                        .frame(width: 130, height: 36)
                        .padding(.horizontal, 12)
                        .overlay(RoundedRectangle(cornerRadius: 9).strokeBorder(FZ.champagne.color, lineWidth: 1.5))
                        .onSubmit(addTag)
                        .onChange(of: tagFocus) { _, f in if !f && addingTag { addTag() } }
                } else {
                    Button { newTag = ""; addingTag = true; tagFocus = true } label: {
                        IconView(Icons.plus, color: FZ.champagne.color, lineWidth: 2.6).frame(width: 16, height: 16)
                            .frame(width: 44, height: 36)
                            .overlay(RoundedRectangle(cornerRadius: 9).strokeBorder(FZ.champagne.opacity(0.7),
                                                                                 style: StrokeStyle(lineWidth: 1.5, dash: [4, 3])))
                    }
                    .buttonStyle(.plain)
                }
            }
        }
    }

    private func addTag() {
        if let k = webKey { model.webTags.add(newTag, to: k) }
        addingTag = false
        tagFocus = false
        newTag = ""
    }

    // MARK: USED BY

    private var usedBy: some View {
        let u = actions.users["mine:\(entry.id)"]
        let chips = HStack(spacing: 8) {
            if let u {
                if u.projects.isEmpty {
                    Text("—").fz(16, 800).foregroundStyle(FZ.muted.color)
                } else {
                    ForEach(u.projects, id: \.project) { p in UsedChip(name: projectName(p.project), steps: p.steps) }
                }
            } else if actions.canWrite {
                Text("…").fz(16, 800).foregroundStyle(FZ.muted.color)
            }
        }
        return Group {
            if wide {
                VStack(alignment: .leading, spacing: 10) {
                    Heading(text: "USED BY")
                    ChipScroll(inset: 0, spacing: 8) { chips }.frame(height: 44)
                }
            } else {
                HStack(spacing: 12) {
                    Heading(text: "USED BY")
                    ChipScroll(inset: 0, spacing: 8) { chips }.frame(height: 44)
                }
            }
        }
        .task(id: "\(entry.id):\(u == nil)") {
            if u == nil { _ = await actions.usedBy(entry) }
        }
    }

    private func projectName(_ p: Int) -> String {
        let l = model.projects?.list ?? []
        let n = p >= 1 && p <= l.count ? l[p - 1].name : ""
        return n.isEmpty ? "PROJECT \(p)" : n
    }

    // MARK: buttons

    private var mainButtons: some View {
        HStack(spacing: 10) {
            if places.isEmpty {
                IconPill(title: "+ POOL", color: FZ.green, enabled: can && entry.id != 0) { addToPool() }
            } else {
                IconPill(title: "− POOL", color: FZ.green, ghost: true, enabled: can) { removeFromPool([]) }
            }
            // → KEPT: a copy on the phone
            IconPill(title: "KEPT", icon: KeptArt.phone, color: FZ.champagne, redline: true,
                     enabled: !working && (actions.frame(of: entry) != nil || snapshot)) { keep() }
            IconPill(title: "EDIT", icon: Icons.pencil, color: FZ.pink, ink: .white, enabled: actions.frame(of: entry) != nil || snapshot) {
                model.editTarget = .preset(entry)
                model.goTo = .edit       // (the EDIT tab, I5's screen, opens on model.editTarget)
                onClose()
            }
        }
    }

    private var lastButtons: some View {
        // (the iPad's narrow column has no room for three words: the icons alone)
        let short = wide && mine && onMove != nil
        return HStack(spacing: 10) {
            IconPill(title: short ? "" : "EXPORT", icon: Icons.share, ghost: true, enabled: actions.frame(of: entry) != nil || snapshot) {
                onExport([entry])
            }
            if mine, let onMove {
                IconPill(title: short ? "" : "MOVE", icon: Icons.swap, color: FZ.yellow, redline: true, enabled: can) { onMove(entry) }
            }
            if mine {
                IconPill(title: short ? "" : "DELETE", icon: Icons.trash, color: FZ.red, redline: true, enabled: can) { askDelete() }
            }
        }
    }

    private func keep() {
        working = true
        Task {
            let o = await model.keep(entry)
            working = false
            switch o {
            case .kept(let k): note = .done("→ KEPT · \(k.name)", FZ.champagne)
            case .already(let k): note = ScreenNote(text: "KEPT ALREADY · \(k.name)", color: FZ.champagne, kind: .note)
            case .failed: note = ScreenNote(text: "NOT READ YET", color: FZ.muted, kind: .refused)
            }
        }
    }

    private func addToPool() {
        run { await actions.addToPool(entry) } done: { o in
            if case .placed(let k) = o { return .done("POOL \(k)") }
            return nil
        }
    }

    /// − POOL: every place cleared; a used one asks first (CLEAR POOL k), then the rest go on.
    private func removeFromPool(_ confirmed: Set<Int>) {
        working = true
        Task {
            let o = await actions.removeFromPool(entry, confirmed: confirmed)
            working = false
            if case .needsConfirm(let k, _, let steps) = o {
                ask = ConfirmAsk(title: "CLEAR POOL \(k)", what: entry.name, detail: stepsText(steps), go: "CLEAR",
                                 color: FZ.red, undo: .kept) { removeFromPool(confirmed.union([k])) }
            } else if o.isOK {
                note = .done("OUT OF THE POOL", FZ.muted)
            } else {
                note = ScreenNote.of(o)
            }
        }
    }

    private func askDelete() {
        let u = actions.users["mine:\(entry.id)"]
        let steps = u?.projects.reduce(0) { $0 + $1.steps } ?? 0
        var detail: String?
        if let u, !u.projects.isEmpty {
            detail = "\(u.projects.count) PROJECT\(u.projects.count == 1 ? "" : "S")" + (steps > 0 ? " · \(stepsText(steps))" : "")
        } else if !places.isEmpty {
            detail = "OUT OF POOL \(places.map(String.init).joined(separator: ", "))"
        }
        let r = entry
        ask = ConfirmAsk(title: "DELETE · MY PRESETS \(r.slot)", what: r.name, detail: detail, go: "DELETE", color: FZ.red,
                         undo: .kept) {
            run { await actions.delete(r) } done: { o in
                if o.isOK { onClose(); return .done("DELETED \(r.name)", FZ.red) }
                return nil
            }
        }
    }

    // MARK: LOAD ON FM-1

    private var load: some View {
        let b = actions.browsing
        let here = b?.id == entry.id ? b : nil
        return VStack(alignment: .leading, spacing: 10) {
            HStack(spacing: 10) {
                Heading(text: "LOAD ON FM-1", color: FZ.pink)
                Spacer(minLength: 0)
                if here != nil {
                    OctCap(text: "OCT−", color: FZ.muted)
                    OctCap(text: "OCT+", color: FZ.green)
                }
            }
            HStack(spacing: 7) {
                ForEach(1...6, id: \.self) { n in
                    TrackButton(n: n, live: here?.track == n - 1) {
                        guard can, entry.id != 0 else { return }
                        working = true
                        Task {
                            let o = await actions.loadOnUnit(entry, track: n)
                            working = false
                            if o == .refused(.busy) { note = ScreenNote(text: "NAMING ON THE FM-1", color: FZ.orange, kind: .refused) }
                            else { note = ScreenNote.of(o) }
                        }
                    }
                    .disabled(!can || entry.id == 0)
                }
            }
        }
    }

    // MARK: running a write

    /// A write: the buttons wait, then its state (a refusal's own, else `done`'s).
    private func run(_ w: @escaping @MainActor () async -> UnitOutcome, done: @escaping @MainActor (UnitOutcome) -> ScreenNote?) {
        working = true
        Task {
            let o = await w()
            working = false
            if let n = ScreenNote.of(o) { note = n } else if let n = done(o) { note = n }
        }
    }
}

func stepsText(_ n: Int) -> String { "\(n) STEP\(n == 1 ? "" : "S") → TRACK SOUND" }

// MARK: - Parts

/// The name's caret (a tap renames): cyan, blinking.
struct Caret: View {
    let height: CGFloat
    @Environment(\.fizzSnapshot) private var snapshot
    var body: some View {
        let bar = RoundedRectangle(cornerRadius: 1.5).fill(FZ.cyan.color).frame(width: 3, height: height)
        if snapshot {
            bar
        } else {
            // (twice a second, not every frame)
            TimelineView(.periodic(from: .distantPast, by: 0.55)) { ctx in
                bar.opacity(Int(ctx.date.timeIntervalSinceReferenceDate / 0.55) % 2 == 0 ? 1 : 0)
            }
        }
    }
}

/// The pool's places a preset is in: the four squares, the first place, +n.
struct PlacesBadge: View {
    let places: [Int]
    var body: some View {
        HStack(spacing: 4) {
            IconView(Icons.poolChip, color: .black, fill: true).frame(width: 10, height: 10)
            Text("\(places[0])").fz(12, 800)
            if places.count > 1 { Text("+\(places.count - 1)").fz(12, 700) }
        }
        .foregroundStyle(.black)
        .padding(.horizontal, 6).frame(height: 20)
        .background(RoundedRectangle(cornerRadius: 5).fill(FZ.green.color))
        .fixedSize()
    }
}

/// A project that uses the preset, a dot a step (six at most, then +).
struct UsedChip: View {
    let name: String
    let steps: Int
    var body: some View {
        HStack(spacing: 10) {
            Text(name).fz(15, 800).foregroundStyle(FZ.text.color).lineLimit(1)
            if steps > 0 { StepDots(n: steps) }
        }
        .padding(.horizontal, 14).frame(height: 40)
        .background(RoundedRectangle(cornerRadius: 13).fill(FZ.green.opacity(0.14)))
        .overlay(RoundedRectangle(cornerRadius: 13).strokeBorder(FZ.green.color, lineWidth: 1.5))
        .fixedSize()
    }
}

struct StepDots: View {
    let n: Int
    var color: Color = FZ.green.color
    var body: some View {
        HStack(spacing: 4) {
            ForEach(0..<min(6, n), id: \.self) { _ in Circle().fill(color).frame(width: 9, height: 9) }
            if n > 6 { Text("+").fz(13, 800).foregroundStyle(color) }
        }
    }
}

/// One of the six tracks (.trk): pink outline; the one the unit's browser is open on breathes, filled.
struct TrackButton: View {
    let n: Int
    var live = false
    let action: () -> Void
    var body: some View {
        Button(action: action) {
            Animated(still: 0.9, active: live) { t in
                let b = live ? breathe(t, 1.8) : 0
                Text("\(n)").fz(24, 800).foregroundStyle(live ? .white : FZ.text.color)
                    .frame(maxWidth: .infinity).frame(height: 54)
                    .background(RoundedRectangle(cornerRadius: 16).fill(live ? FZ.pink.color : FZ.pink.opacity(0.05)))
                    .overlay(RoundedRectangle(cornerRadius: 16).strokeBorder(live ? FZ.pink.color : FZ.pink.mix(45, FZ.line).color,
                                                                          lineWidth: 2))
                    .shadow(color: live ? FZ.pink.opacity(0.4 + 0.3 * b) : .clear, radius: 14 + 10 * b)
                    .contentShape(RoundedRectangle(cornerRadius: 16))
            }
        }
        .buttonStyle(PressStyle(scale: 0.93))
    }
}

/// A key of the FM-1 named (OCT−, OCT+): what it does now.
struct OctCap: View {
    let text: String
    let color: RGB
    var body: some View {
        Text(text).fz(12, 800, em: 0.06).foregroundStyle(color.color)
            .padding(.horizontal, 8).frame(height: 24)
            .background(RoundedRectangle(cornerRadius: 6).fill(color.opacity(0.12)))
            .overlay(RoundedRectangle(cornerRadius: 6).strokeBorder(color.color, lineWidth: 1.5))
            .fixedSize()
    }
}

/// A button with an icon (.btn with an svg): filled, ghost, or red-lined.
struct IconPill: View {
    let title: String
    var icon: VectorArt?
    var color: RGB = FZ.cyan
    var ink: Color = .black
    var ghost = false
    var redline = false
    var height: CGFloat = 52
    var size: CGFloat = 16
    var enabled = true
    let action: () -> Void

    var body: some View {
        let filled = !ghost && !redline
        let fg: Color = filled ? ink : redline ? color.color : FZ.text.color
        Button(action: action) {
            HStack(spacing: 9) {
                if let icon { IconView(icon, color: fg, lineWidth: 2.3, backdrop: filled ? color.color : .black).frame(width: 18, height: 18) }
                if !title.isEmpty { Text(title).fz(size, 800, em: 0.08).lineLimit(1).minimumScaleFactor(0.7) }
            }
            .foregroundStyle(fg)
            .padding(.horizontal, 14)
            .frame(maxWidth: .infinity).frame(height: height)
            .background(Capsule().fill(filled ? color.color : redline ? color.opacity(0.06) : .clear))
            .overlay(Capsule().strokeBorder(filled ? .clear : redline ? color.mix(70, FZ.line).color : FZ.line.color, lineWidth: 1.5))
            .shadow(color: filled ? color.opacity(0.4) : .clear, radius: 11)
            .contentShape(Capsule())
        }
        .buttonStyle(PressStyle(scale: 0.96))
        .disabled(!enabled)
        .opacity(enabled ? 1 : 0.4)
    }
}

/// Chips that wrap onto more lines (the tags).
struct WrapLayout: Layout {
    var spacing: CGFloat = 7

    func sizeThatFits(proposal: ProposedViewSize, subviews: Subviews, cache: inout ()) -> CGSize {
        let rows = arrange(proposal.width ?? .infinity, subviews)
        let w = rows.map { $0.width }.max() ?? 0
        let h = rows.reduce(0) { $0 + $1.height } + spacing * CGFloat(max(0, rows.count - 1))
        return CGSize(width: proposal.width ?? w, height: h)
    }

    func placeSubviews(in bounds: CGRect, proposal: ProposedViewSize, subviews: Subviews, cache: inout ()) {
        var y = bounds.minY
        for row in arrange(bounds.width, subviews) {
            var x = bounds.minX
            for k in row.items {
                let s = subviews[k].sizeThatFits(.unspecified)
                subviews[k].place(at: CGPoint(x: x, y: y + (row.height - s.height) / 2), proposal: ProposedViewSize(s))
                x += s.width + spacing
            }
            y += row.height + spacing
        }
    }

    private struct Row { var items: [Int] = []; var width: CGFloat = 0; var height: CGFloat = 0 }

    private func arrange(_ maxW: CGFloat, _ subviews: Subviews) -> [Row] {
        var rows: [Row] = [], cur = Row()
        for (k, v) in subviews.enumerated() {
            let s = v.sizeThatFits(.unspecified)
            let add = cur.items.isEmpty ? s.width : cur.width + spacing + s.width
            if add > maxW && !cur.items.isEmpty { rows.append(cur); cur = Row() }
            cur.width = cur.items.isEmpty ? s.width : cur.width + spacing + s.width
            cur.height = max(cur.height, s.height)
            cur.items.append(k)
        }
        if !cur.items.isEmpty { rows.append(cur) }
        return rows
    }
}
