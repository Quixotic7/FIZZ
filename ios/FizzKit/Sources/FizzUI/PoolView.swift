// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// POOL (mock-up 13; site/manager/pool.js): the open project's 128 pool places (8 columns on the phone, 16 x 8 on the
// iPad), each the preset it references in its tag's colour with a dot a step that locks it (USERS, read in the
// background). The project chips load another project. A sound picked in SOUNDS (the phone's drawer, the iPad's
// column; or dragged there on the iPad) goes into the next place tapped. MOVE (or, on the iPad, a tile dragged onto
// another) swaps two places, every pattern's steps following. CLEAR, CLEAR UNUSED and SORT BY TAG ask first, as the
// page does (the standalone confirm, over the tab bar). The phone's bar floats above the tab bar; the grid scrolls
// clear of both.

import SwiftUI
import FizzData
import FizzKit

struct PoolView: View {
    let model: UnitModel
    var wide = false
    @Environment(\.fizzSnapshot) private var snapshot
    @Environment(\.fizzShotSelection) private var shotSelection
    @Environment(\.fizzBarClearance) private var clearance
    @State private var selected: Int?
    /// a sound picked (its id): the next place tapped takes it
    @State private var pick: Int?
    /// MOVE: the place moving; the next place tapped swaps with it
    @State private var moving: Int?
    @State private var drawer = false
    @State private var note: ScreenNote?
    @State private var ask: ConfirmAsk?
    @State private var filter = LibraryFilter(source: .all)
    /// the phone's bar's height (the grid scrolls clear of it)
    @State private var barHeight: CGFloat = 116
    @Namespace private var tiles

    private var actions: UnitActions { model.actions }
    private var can: Bool { actions.canWrite }
    private var pool: [Int: FizzListEntry] { Dictionary(model.pool.map { ($0.slot, $0) }, uniquingKeysWith: { a, _ in a }) }
    private var presets: [Int: FizzListEntry] {
        Dictionary(model.rows.filter { $0.source != "pool" && $0.id != 0 }.map { ($0.id, $0) }, uniquingKeysWith: { a, _ in a })
    }
    /// what changes the places (the users are read again after it)
    private var signature: String {
        model.pool.sorted { $0.slot < $1.slot }.map { "\($0.slot):\($0.id)" }.joined(separator: ",") + "#\(model.status?.written ?? 0)"
    }

    var body: some View {
        Group {
            if wide { padBody } else { phoneBody }
        }
        .confirmCard($ask)
        .hardwareKeys(model.editor)
        .animation(.spring(response: 0.35, dampingFraction: 0.85), value: note)
        .onAppear {
            if snapshot, let s = shotSelection, s.hasPrefix("pool:") { selected = Int(s.dropFirst(5)) }
        }
        // the steps on each place: USERS, one after another while POOL shows (pool.js fill)
        .task(id: signature) {
            guard !snapshot else { return }
            for k in model.pool.map(\.slot).sorted() {
                if Task.isCancelled || !can { break }
                if actions.users["pool:\(k)"] != nil { continue }
                _ = await actions.poolUsers(k)
            }
        }
    }

    // MARK: header

    private var header: some View {
        let cur = model.projects?.list.first { $0.current }
        return HStack(alignment: .lastTextBaseline, spacing: 10) {
            Text(cur?.name.isEmpty == false ? cur!.name : "–").fz(40, 800, em: -0.01).foregroundStyle(FZ.text.color)
                .lineLimit(1).minimumScaleFactor(0.5)
            Spacer(minLength: 0)
            Count(n: model.pool.count, of: 128)
        }
    }

    private var projectChips: some View {
        let list = model.projects?.list ?? []
        let n = max(1, min(8, model.status?.nproj ?? 8))
        return HStack(spacing: 6) {
            ForEach(1...n, id: \.self) { k in
                let p = k <= list.count ? list[k - 1] : nil
                ProjectChip(slot: k, there: p?.there == true, current: p?.current == true) { loadProject(k) }
            }
        }
    }

    private func loadProject(_ k: Int) {
        let list = model.projects?.list ?? []
        guard k <= list.count, list[k - 1].there, !list[k - 1].current else { return }
        let name = list[k - 1].name.isEmpty ? "PROJECT \(k)" : list[k - 1].name
        let from = list.first { $0.current }?.name ?? ""
        ask = ConfirmAsk(title: "LOAD PROJECT", what: name, detail: from.isEmpty ? nil : "FROM \(from)", go: "LOAD", color: FZ.green,
                         undo: .cleared) {
            Task {
                selected = nil; pick = nil; moving = nil
                let o = await actions.loadProject(k)
                note = o.isOK ? .done("\(name) ON THE FM-1") : ScreenNote.of(o)
            }
        }
    }

    // MARK: the grid

    private func grid(columns: Int, height: CGFloat) -> some View {
        let pm = pool, ps = presets
        return LazyVGrid(columns: Array(repeating: GridItem(.flexible(minimum: 10), spacing: 5), count: columns), spacing: 5) {
            ForEach(1...128, id: \.self) { k in
                let e = pm[k]
                Button { tapTile(k) } label: {
                    PoolTile(place: k, entry: e, algorithm: e.flatMap { ps[$0.id] }.map(model.algorithm) ?? 1,
                             steps: actions.poolSteps(k), selected: selected == k, here: pick != nil && e?.id == pick,
                             moving: moving == k, height: height, namespace: tiles,
                             nth: e.map { x in pm.filter { $0.key < k && $0.value.id == x.id }.count } ?? 0)
                }
                .buttonStyle(PressStyle(scale: 0.9))
                .modifier(TileDrop(place: k, enabled: can, drags: wide, onPreset: { place(k, id: $0) }, onPlace: { askSwap($0, k) }))
            }
        }
        .opacity(model.status == nil ? 0.4 : 1)
        .animation(.spring(response: 0.55, dampingFraction: 0.8), value: signature)
    }

    private func tapTile(_ k: Int) {
        guard can else { return }
        if let id = pick { place(k, id: id); return }
        if let m = moving {
            moving = nil
            if m != k { askSwap(m, k) }
            return
        }
        withAnimation(.spring(response: 0.3, dampingFraction: 0.6)) { selected = selected == k ? nil : k }
        // TRACK on: the place's sound heard on the track (pool.js previewId), as LIBRARY's tap
        if actions.trackOn, selected == k, let r = pool[k] { Task { _ = await actions.pick(r) } }
    }

    private func place(_ k: Int, id: Int) {
        pick = nil
        Task {
            let o = await actions.place(k, id: id)
            if case .placed(let at) = o { withAnimation { selected = at } } else { note = ScreenNote.of(o) }
        }
    }

    private func askSwap(_ a: Int, _ b: Int) {
        guard a != b else { return }
        let ea = pool[a], eb = pool[b]
        ask = ConfirmAsk(title: eb != nil ? "SWAP POOL \(a) ↔ \(b)" : "MOVE POOL \(a) → \(b)",
                         what: eb != nil ? "\(ea?.name ?? "—") ↔ \(eb!.name)" : ea?.name ?? "—",
                         detail: "PATTERNS FOLLOW", go: eb != nil ? "SWAP" : "MOVE", color: FZ.yellow, undo: .cleared) {
            Task {
                let o = await actions.swapPool(a, b)
                if o.isOK { withAnimation { selected = b } } else { note = ScreenNote.of(o) }
            }
        }
    }

    private func askClear(_ k: Int) {
        guard let e = pool[k] else { return }
        let steps = actions.poolSteps(k) ?? 0
        ask = ConfirmAsk(title: "CLEAR POOL \(k)", what: e.name, detail: steps > 0 ? stepsText(steps) : "ITS STEPS → TRACK SOUND",
                         go: "CLEAR", color: FZ.red, undo: .kept) {
            Task {
                let o = await actions.clearPlace(k)
                if o.isOK { selected = nil; note = .done("POOL \(k) CLEARED", FZ.muted) } else { note = ScreenNote.of(o) }
            }
        }
    }

    private func askSort() {
        let want = actions.sortedPool()
        let moves = want.enumerated().filter { $0.element != $0.offset + 1 }.count
        guard moves > 0 else { note = .done("SORTED ALREADY", FZ.muted); return }
        ask = ConfirmAsk(title: "SORT BY TAG", what: "\(want.count) SOUNDS", detail: "UP TO \(moves) SWAPS · PATTERNS FOLLOW",
                         go: "SORT", color: FZ.yellow, undo: .cleared) {
            Task {
                selected = nil
                let o = await actions.sortPoolByTag()
                if case .placed(let n) = o { note = .done("SORTED · \(n) MOVES") } else { note = ScreenNote.of(o) }
            }
        }
    }

    private func askClearUnused() {
        let n = model.pool.count
        ask = ConfirmAsk(title: "CLEAR UNUSED", what: "\(n) SOUNDS", detail: "UNUSED ONES OUT OF THE POOL", go: "CLEAR UNUSED",
                         color: FZ.red, undo: .kept) {
            Task {
                selected = nil
                let o = await actions.clearUnused()
                if case .placed(let c) = o { note = .done("\(c) CLEARED", c > 0 ? FZ.green : FZ.muted) } else { note = ScreenNote.of(o) }
            }
        }
    }

    // MARK: the bar

    private var bar: some View {
        VStack(alignment: .leading, spacing: 10) {
            barTop.frame(height: 40)
            if let j = actions.job {
                JobLine(done: j.done, of: j.of)
            } else {
                HStack(spacing: 10) {
                    IconPill(title: "SORT BY TAG", ghost: true, height: 42, size: 13, enabled: can && !model.pool.isEmpty) { askSort() }
                    IconPill(title: "CLEAR UNUSED", ghost: true, height: 42, size: 13, enabled: can && !model.pool.isEmpty) { askClearUnused() }
                }
            }
        }
        .padding(.horizontal, 14).padding(.vertical, 12)
        .background(RoundedRectangle(cornerRadius: 22).fill(RGB(0x0e0e12).color))
        .overlay(RoundedRectangle(cornerRadius: 22).strokeBorder(FZ.nav.color, lineWidth: 1.5))
        .shadow(color: .black, radius: 15, y: -5)
    }

    @ViewBuilder private var barTop: some View {
        if let id = pick {
            let r = presets[id]
            HStack(spacing: 10) {
                AlgoGlyph(algorithm: r.map(model.algorithm) ?? 1, color: FizzTag.tint(mask: r?.tags ?? 0).color).frame(width: 30, height: 30)
                Text(r?.name ?? "?").fz(22, 800).foregroundStyle(FZ.text.color).lineLimit(1).minimumScaleFactor(0.6)
                IconView(Icons.down, color: FZ.green.color, lineWidth: 2.6).frame(width: 20, height: 20).rotationEffect(.degrees(-90))
                IconView(Icons.pool, color: FZ.green.color, lineWidth: 2.2).frame(width: 24, height: 24)
                Spacer(minLength: 0)
                RoundIcon(art: Icons.close, color: FZ.muted) { pick = nil }
            }
        } else if let m = moving, let e = pool[m] {
            HStack(spacing: 10) {
                IconView(Icons.swap, color: FZ.yellow.color, lineWidth: 2.4).frame(width: 24, height: 24)
                Text("\(m)").fz(15, 800).foregroundStyle(FZ.yellow.color)
                Text(e.name).fz(22, 800).foregroundStyle(FZ.text.color).lineLimit(1).minimumScaleFactor(0.6)
                Text("→ ?").fz(22, 800).foregroundStyle(FZ.yellow.color)
                Spacer(minLength: 0)
                RoundIcon(art: Icons.close, color: FZ.muted) { moving = nil }
            }
        } else if let k = selected, let e = pool[k] {
            let r = presets[e.id]
            HStack(spacing: 10) {
                AlgoGlyph(algorithm: r.map(model.algorithm) ?? 1, color: FizzTag.tint(mask: e.tags).color).frame(width: 30, height: 30)
                Text("\(k)").fz(15, 800).foregroundStyle(FZ.cyan.color)
                Text(e.name).fz(22, 800).foregroundStyle(FZ.text.color).lineLimit(1).minimumScaleFactor(0.6)
                Spacer(minLength: 0)
                RoundIcon(art: KeptArt.phone, color: FZ.champagne, enabled: r != nil) { if let r { keep(r) } }
                RoundIcon(art: Icons.swap, color: FZ.yellow, enabled: can) { moving = k; selected = nil }
                RoundIcon(art: Icons.trash, color: FZ.red, enabled: can) { askClear(k) }
                VStack(alignment: .trailing, spacing: 0) {
                    Text(actions.poolSteps(k).map(String.init) ?? "…").fz(28, 800).foregroundStyle(FZ.green.color)
                    Text("STEPS").fz(12, 800, em: 0.1).foregroundStyle(FZ.muted.color)
                }
                .fixedSize()
            }
        } else {
            IconPill(title: "SOUNDS", icon: Icons.plus, color: FZ.green, height: 40, size: 14, enabled: can) {
                if wide { return }
                drawer = true
            }
        }
    }

    /// → KEPT: the place's preset copied onto the phone.
    private func keep(_ r: FizzListEntry) {
        Task {
            switch await model.keep(r) {
            case .kept(let k): note = .done("→ KEPT · \(k.name)", FZ.champagne)
            case .already(let k): note = ScreenNote(text: "KEPT ALREADY · \(k.name)", color: FZ.champagne, kind: .note)
            case .failed: note = ScreenNote(text: "NOT READ YET", color: FZ.muted, kind: .refused)
            }
        }
    }

    // MARK: iPhone

    private var phoneBody: some View {
        VStack(alignment: .leading, spacing: 0) {
            header.padding(.top, 2).padding(.bottom, 10)
            projectChips.padding(.bottom, 8)
            ScreenScroll(bottom: clearance + barHeight + 16, refresh: { await model.refresh() }) {
                // (room above the first row for the selected tile's ring)
                grid(columns: 8, height: 46).padding(.top, 6)
            }
        }
        .padding(.horizontal, 16)
        .overlay(alignment: .top) { NoteStrip(note: $note).padding(.top, 60) }
        .overlay(alignment: .bottom) {
            bar.onGeometryChange(for: CGFloat.self) { $0.size.height } action: { barHeight = $0 }
                .padding(.horizontal, 12).padding(.bottom, clearance)
        }
        .sheet(isPresented: $drawer) {
            SoundsDrawer(model: model, filter: $filter, picked: pick) { id in
                pick = id
                selected = nil; moving = nil
                drawer = false
            }
        }
    }

    // MARK: iPad

    private var padBody: some View {
        HStack(alignment: .top, spacing: 14) {
            VStack(alignment: .leading, spacing: 0) {
                header.padding(.bottom, 10)
                projectChips.frame(maxWidth: 560).padding(.bottom, 14)
                grid(columns: 16, height: 52)
                Spacer(minLength: 14)
                bar
            }
            .frame(maxWidth: .infinity)
            Card(padding: 14) {
                Heading(text: "SOUNDS", count: "\(model.mine.count + model.factory.count)").padding(.bottom, 10)
                SoundsList(model: model, filter: $filter, picked: pick, draggable: can, marks: false) { id in
                    pick = pick == id ? nil : id
                    selected = nil; moving = nil
                }
            }
            .frame(width: 300)
        }
        .padding(.bottom, 26)
        .overlay(alignment: .top) { NoteStrip(note: $note).padding(.top, 4) }
    }
}

// MARK: - A tile

/// One pool place (.pt): empty, or its sound in its tag's colour with the glyph and a dot a step (four at most);
/// selected: a cyan ring, popping, bubbles rising.
struct PoolTile: View {
    let place: Int
    let entry: FizzListEntry?
    let algorithm: Int
    var steps: Int?
    var selected = false
    var here = false
    var moving = false
    var height: CGFloat = 46
    let namespace: Namespace.ID
    /// the same sound's earlier places (its id among the matched tiles)
    var nth = 0

    var body: some View {
        Animated(still: 0.25, active: selected) { t in
            let p = selected ? (t.truncatingRemainder(dividingBy: 2.4)) / 2.4 : 1
            let s: Double = p < 0.14 ? 0.86 + (1.1 - 0.86) * p / 0.14 : p < 0.3 ? 1.1 - 0.1 * (p - 0.14) / 0.16 : 1
            ZStack(alignment: .topLeading) {
                RoundedRectangle(cornerRadius: 10).fill(RGB(0x101014).color)
                    .overlay(RoundedRectangle(cornerRadius: 10).strokeBorder(RGB(0x1c1c22).color, lineWidth: 1.5))
                if let e = entry {
                    full(e).matchedGeometryEffect(id: "s\(e.id)#\(nth)", in: namespace)
                        .transition(.scale(scale: 0.3).combined(with: .opacity))
                } else {
                    Text("\(place)").fz(12, 800).foregroundStyle(RGB(0x4d4d56).color).padding(.leading, 5).padding(.top, 2)
                }
            }
            .frame(height: height)
            .overlay {
                if selected || here || moving {
                    let c = moving ? FZ.yellow : here ? FZ.green : FZ.cyan
                    RoundedRectangle(cornerRadius: 12).inset(by: -3).strokeBorder(Color.black, lineWidth: 2.5)
                        .overlay(RoundedRectangle(cornerRadius: 13).inset(by: -5.5).strokeBorder(c.color, lineWidth: 2.5))
                        .shadow(color: c.color, radius: 10)
                }
            }
            .overlay { if selected { TileBubbles(t: t) } }
            .scaleEffect(selected ? s : 1)
            .zIndex(selected ? 2 : 0)
            .contentShape(RoundedRectangle(cornerRadius: 10))
        }
    }

    private func full(_ e: FizzListEntry) -> some View {
        let tag = FizzTag.of(mask: e.tags).first
        let c = FizzTag.tint(mask: e.tags), ink: Color = tag?.ink ?? .black
        return ZStack(alignment: .topLeading) {
            RoundedRectangle(cornerRadius: 10).fill(c.color)
            Text("\(place)").fz(12, 800).foregroundStyle(ink.opacity(0.62)).padding(.leading, 5).padding(.top, 2)
            AlgoGlyph(algorithm: algorithm, color: ink).frame(width: 20, height: 20)
                .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .bottomLeading).padding(.leading, 4).padding(.bottom, 3)
            if let n = steps, n > 0 {
                VStack(spacing: 2) {
                    ForEach(0..<min(4, n), id: \.self) { _ in Circle().fill(ink.opacity(0.7)).frame(width: 5, height: 5) }
                }
                .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .bottomTrailing).padding(.trailing, 5).padding(.bottom, 5)
            }
        }
    }
}

/// The selected tile's bubbles (pool.css .ps.sel::after): three rising out of it, every 2.4 s.
private struct TileBubbles: View {
    let t: Double
    var body: some View {
        let p = t.truncatingRemainder(dividingBy: 2.4) / 2.4
        let k = min(1, p / 0.4), y = 10 - 44 * k, o = p < 0.4 ? 1 - k : 0
        Canvas { ctx, size in
            for (fx, fy, r) in [(0.3, 0.8, 3.0), (0.75, 0.55, 2.5), (0.5, 0.95, 4.0)] {
                let c = CGPoint(x: size.width * fx, y: size.height * fy + y)
                ctx.fill(Path(ellipseIn: CGRect(x: c.x - r, y: c.y - r, width: 2 * r, height: 2 * r)), with: .color(.white.opacity(0.8 * o)))
            }
        }
        .allowsHitTesting(false)
    }
}

/// A place takes a sound dropped on it ("preset:<id>") or another place ("pool:<k>": a swap); its sound drags.
private struct TileDrop: ViewModifier {
    let place: Int
    let enabled: Bool
    /// its sound drags (the iPad; the phone's MOVE is a tap: a hold-to-drag would fight the grid's scroll)
    var drags = true
    let onPreset: (Int) -> Void
    let onPlace: (Int) -> Void
    @State private var target = false
    @Environment(\.fizzSnapshot) private var snapshot

    @ViewBuilder private func dragged(_ content: Content) -> some View {
        if drags { content.draggable("pool:\(place)") } else { content }
    }

    func body(content: Content) -> some View {
        if enabled && !snapshot {
            dragged(content)
                .dropDestination(for: String.self) { items, _ in
                    guard let s = items.first else { return false }
                    if s.hasPrefix("preset:"), let id = Int(s.dropFirst(7)) { onPreset(id); return true }
                    if s.hasPrefix("pool:"), let k = Int(s.dropFirst(5)), k != place { onPlace(k); return true }
                    return false
                } isTargeted: { target = $0 }
                .overlay {
                    if target { RoundedRectangle(cornerRadius: 12).strokeBorder(FZ.green.color, lineWidth: 3).shadow(color: FZ.green.color, radius: 8) }
                }
        } else {
            content
        }
    }
}

// MARK: - Parts

/// A project's chip (.pj): its number; the open one lit, an empty slot dashed.
struct ProjectChip: View {
    let slot: Int
    let there: Bool
    let current: Bool
    let action: () -> Void
    var body: some View {
        Button(action: action) {
            Text("\(slot)").fz(14, 800)
                .foregroundStyle(current ? Color.black : there ? FZ.green.color : RGB(0x4a4a54).color)
                .frame(maxWidth: .infinity).frame(height: 34)
                .background(Capsule().fill(current ? FZ.green.color : .clear))
                .overlay {
                    if there {
                        Capsule().strokeBorder(current ? FZ.green.color : FZ.green.mix(55, FZ.line).color, lineWidth: 1.5)
                    } else {
                        Capsule().strokeBorder(FZ.dim.color, style: StrokeStyle(lineWidth: 1.5, dash: [4, 3]))
                    }
                }
                .shadow(color: current ? FZ.green.opacity(0.5) : .clear, radius: 7)
                .contentShape(Capsule())
        }
        .buttonStyle(.plain)
    }
}

/// A round icon button (the bar's MOVE, CLEAR, ×).
struct RoundIcon: View {
    let art: VectorArt
    var color: RGB = FZ.text
    var enabled = true
    /// the ring's size (the touch is 44 at least)
    var size: CGFloat = 38
    let action: () -> Void
    var body: some View {
        Button(action: action) {
            IconView(art, color: color.color, lineWidth: 2.3).frame(width: 18, height: 18)
                .frame(width: size, height: size)
                .overlay(Circle().strokeBorder(color.mix(55, FZ.line).color, lineWidth: 1.5))
                .frame(width: max(44, size), height: max(44, size))
                .contentShape(Circle())
        }
        .buttonStyle(PressStyle(scale: 0.9))
        .disabled(!enabled)
        .opacity(enabled ? 1 : 0.4)
    }
}

/// A long write's count (SORT BY TAG, CLEAR UNUSED): a filling bar and n / of.
struct JobLine: View {
    let done: Int
    let of: Int
    var body: some View {
        HStack(spacing: 12) {
            GeometryReader { g in
                Capsule().fill(FZ.track.color)
                    .overlay(alignment: .leading) {
                        Capsule().fill(FZ.green.color).frame(width: max(8, g.size.width * Double(done) / Double(max(1, of))))
                            .shadow(color: FZ.green.color, radius: 6)
                    }
            }
            .frame(height: 10)
            Text("\(done) / \(of)").fz(15, 800).monospacedDigit().foregroundStyle(FZ.text.color).fixedSize()
        }
        .frame(height: 42)
        .animation(.easeOut(duration: 0.2), value: done)
    }
}

/// The sounds to put in the pool (pool.js's list): MY PRESETS and the factory's, LIBRARY's filters; one tapped is
/// picked, dragged (the iPad) goes onto a place.
struct SoundsList: View {
    let model: UnitModel
    @Binding var filter: LibraryFilter
    var picked: Int?
    var draggable = false
    /// (the iPad's narrow column: the name over the marks)
    var marks = true
    let onPick: (Int) -> Void

    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            SoundsChips(filter: $filter, inset: 0).frame(height: 40)
            ScreenScroll(bottom: 10, lazy: true) {
                SoundsRows(model: model, filter: filter, picked: picked, draggable: draggable, marks: marks, onPick: onPick)
            }
        }
    }
}

/// The SOUNDS filters: the pool and the tags.
struct SoundsChips: View {
    @Binding var filter: LibraryFilter
    var inset: CGFloat = 0
    var body: some View {
        ChipScroll(inset: inset, spacing: 6) {
            FilterChip(title: "POOL", color: FZ.green, on: filter.pool != .any, poolIcon: true) { filter.turnPool() }
            ForEach(FizzTag.allCases) { t in
                FilterChip(title: t.name, color: t.tint, on: filter.tags.contains(t)) { filter.toggle(t) }
            }
        }
    }
}

/// The SOUNDS rows: a tap picks one (at once; a scroll wins), the iPad's drag onto a place.
struct SoundsRows: View {
    let model: UnitModel
    let filter: LibraryFilter
    var picked: Int?
    var draggable = false
    var marks = true
    let onPick: (Int) -> Void
    @Environment(\.fizzSnapshot) private var snapshot

    var body: some View {
        let shown = filter.shown(model.rows)
        ForEach(shown.mine + shown.factory, id: \.id) { r in
            let row = Button { if r.id != 0 { onPick(r.id) } } label: {
                PresetRow(entry: r, algorithm: model.algorithm(r), selected: picked == r.id, marks: marks, nameSize: 15)
            }
            .buttonStyle(PressStyle(scale: 0.98))
            .padding(.bottom, 8)
            if draggable && r.id != 0 && !snapshot {
                row.draggable("preset:\(r.id)") {
                    PresetRow(entry: r, algorithm: model.algorithm(r), selected: true).frame(width: 260)
                }
            } else {
                row
            }
        }
    }
}

/// The phone's SOUNDS drawer (a FizzSheet, half height, pulled up to large): pick one, then tap a place.
struct SoundsDrawer: View {
    let model: UnitModel
    @Binding var filter: LibraryFilter
    var picked: Int?
    let onPick: (Int) -> Void

    var body: some View {
        FizzSheet(size: .medium, inset: 16) {
            VStack(alignment: .leading, spacing: 12) {
                Heading(text: "SOUNDS", count: "\(model.mine.count + model.factory.count)", color: FZ.green)
                // (the chips run to the sheet's edges)
                SoundsChips(filter: $filter, inset: 16).frame(height: 40).padding(.horizontal, -16)
            }
        } content: {
            LazyVStack(alignment: .leading, spacing: 0) {
                SoundsRows(model: model, filter: filter, picked: picked, onPick: onPick)
            }
            .padding(.bottom, 24)
        }
    }
}
