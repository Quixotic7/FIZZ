// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// LIBRARY (mock-ups 09, 10, 16; manager.js renderLibrary / renderSel / swapMine, presets.js): MY PRESETS n / 256 (128
// on the Bluetooth package) and FACTORY; ALL / MY PRESETS / FACTORY, the pool and the tags as filters. A row swiped
// right goes into the pool (out of it when it is in), swiped left offers DELETE; a MY preset dragged onto another
// swaps their places (SWAP). A tap opens the preset (the phone's sheet, the iPad's third column) and, with TRACK on,
// plays it on the track (KEEP / CANCEL).

import SwiftUI
import FizzData
import FizzKit

struct LibraryView: View {
    let model: UnitModel
    var wide = false
    @Environment(\.fizzSnapshot) private var snapshot
    @Environment(\.fizzShotSelection) private var shotSelection
    @State private var filter = LibraryFilter()
    @State private var selected: String?
    @State private var sheet: SheetKey?
    @State private var note: ScreenNote?
    @State private var ask: ConfirmAsk?
    @State private var exporting: ExportBox?

    struct SheetKey: Identifiable, Equatable { let id: String }

    var body: some View {
        Group {
            if wide { padBody } else { phoneBody }
        }
        .confirmCard($ask)
        .hardwareKeys(model.editor)
        .sheet(item: $exporting) { box in
            ExportSheet(sounds: box.sounds) { exporting = nil }
        }
        .onAppear { if snapshot, let s = shotSelection, s.hasPrefix("mine:") || s.hasPrefix("factory:") { selected = s } }
    }

    private func key(_ r: FizzListEntry) -> String { "\(r.source):\(r.slot)" }
    private var nlib: Int { model.status?.nlib ?? 256 }
    private var places: [Int: [Int]] { poolPlaces(model.rows) }

    private func entry(_ k: String?) -> FizzListEntry? {
        guard let k else { return nil }
        return model.rows.first { $0.source != "pool" && key($0) == k }
    }

    // MARK: filters

    private var chips: some View {
        Group {
            let p = filter.pool
            FilterChip(title: p == .notInPool ? "NOT IN POOL" : "POOL", color: p == .notInPool ? FZ.muted : FZ.green,
                       on: p != .any, poolIcon: true) { filter.turnPool() }
            ForEach(FizzTag.allCases) { t in
                FilterChip(title: t.name, color: t.tint, on: filter.tags.contains(t)) { filter.toggle(t) }
            }
        }
    }

    // MARK: rows

    private func tap(_ r: FizzListEntry) {
        let k = key(r)
        if wide {
            selected = selected == k ? nil : k
        } else {
            selected = k
            sheet = SheetKey(id: k)
        }
        if model.actions.trackOn {
            Task {
                let o = await model.actions.pick(r)
                if let n = ScreenNote.of(o) { note = n }
            }
        }
    }

    private func row(_ r: FizzListEntry, compact: Bool = false) -> some View {
        LibraryRow(entry: r, algorithm: model.algorithm(r), selected: selected == key(r), compact: compact,
                   places: places[r.id] ?? [], canWrite: model.actions.canWrite, nameSize: wide || compact ? 15 : 16,
                   open: snapshot && shotSelection == "swipe:\(key(r))",
                   onTap: { tap(r) },
                   onPool: { togglePool(r) },
                   onDelete: { askDelete(r) },
                   onDrop: { from in move(from, to: r.slot) })
            .padding(.bottom, 8)
    }

    /// The swipe right: + POOL, or − POOL when it is in.
    private func togglePool(_ r: FizzListEntry) {
        Task {
            if (places[r.id] ?? []).isEmpty {
                let o = await model.actions.addToPool(r)
                if case .placed(let k) = o { note = .done("\(r.name) → POOL \(k)") } else { note = ScreenNote.of(o) }
            } else {
                await removeFromPool(r, [])
            }
        }
    }

    private func removeFromPool(_ r: FizzListEntry, _ confirmed: Set<Int>) async {
        let o = await model.actions.removeFromPool(r, confirmed: confirmed)
        if case .needsConfirm(let k, _, let steps) = o {
            ask = ConfirmAsk(title: "CLEAR POOL \(k)", what: r.name, detail: stepsText(steps), go: "CLEAR", color: FZ.red) {
                Task { await removeFromPool(r, confirmed.union([k])) }
            }
        } else if o.isOK {
            note = .done("\(r.name) OUT OF THE POOL", FZ.muted)
        } else {
            note = ScreenNote.of(o)
        }
    }

    private func askDelete(_ r: FizzListEntry) {
        let pl = places[r.id] ?? []
        ask = ConfirmAsk(title: "DELETE · MY PRESETS \(r.slot)", what: r.name,
                         detail: pl.isEmpty ? nil : "OUT OF POOL \(pl.map(String.init).joined(separator: ", "))", go: "DELETE") {
            Task {
                let o = await model.actions.delete(r)
                if o.isOK {
                    if selected == key(r) { selected = nil }
                    note = .done("DELETED \(r.name)", FZ.red)
                } else { note = ScreenNote.of(o) }
            }
        }
    }

    /// A MY preset dropped on another: their places swapped (manager.js swapMine); the selection follows.
    private func move(_ a: Int, to b: Int) {
        guard a != b else { return }
        Task {
            let o = await model.actions.moveMine(a, to: b)
            if o.isOK {
                if selected == "mine:\(a)" { selected = "mine:\(b)" } else if selected == "mine:\(b)" { selected = "mine:\(a)" }
            } else { note = ScreenNote.of(o) }
        }
    }

    private func export(_ rows: [FizzListEntry]) {
        Task {
            let s = await model.actions.exportSounds(rows)
            if s.isEmpty { note = ScreenNote(text: "NOT READ YET", color: FZ.muted, kind: .refused) } else { exporting = ExportBox(sounds: s) }
        }
    }

    private func group(_ name: String, _ n: Int) -> some View {
        HStack(spacing: 10) {
            Text(name).fz(13, 800, em: 0.12).foregroundStyle(FZ.muted.color)
            Text("\(n)").fz(15, 800).foregroundStyle(FZ.text.color)
        }
        .padding(.top, 6).padding(.bottom, 10)
    }

    // MARK: iPhone

    private var phoneBody: some View {
        let shown = filter.shown(model.rows)
        return VStack(alignment: .leading, spacing: 0) {
            HStack(spacing: 10) {
                SourceSeg(selected: $filter.source)
                Spacer(minLength: 0)
                Count(n: filter.source == .factory ? model.factory.count : model.mine.count,
                      of: filter.source == .factory ? nil : nlib)
            }
            .padding(.horizontal, 16).padding(.top, 4).padding(.bottom, 12)
            ChipScroll { chips }
                .padding(.bottom, 14)
            ScreenScroll(refresh: { await model.refresh() }) {
                if filter.source == .all && !shown.mine.isEmpty { group("MY PRESETS", shown.mine.count) }
                ForEach(shown.mine, id: \.slot) { row($0) }
                if filter.source == .all && !shown.factory.isEmpty { group("FACTORY", shown.factory.count) }
                ForEach(shown.factory, id: \.slot) { row($0) }
                if !shown.mine.isEmpty || !shown.factory.isEmpty {
                    IconPill(title: "EXPORT ALL", icon: Icons.share, ghost: true, height: 48, size: 14) {
                        export(shown.mine + shown.factory)
                    }
                    .padding(.top, 8).padding(.horizontal, 40)
                }
            }
            .padding(.horizontal, 16)
            .overlay(alignment: .top) { if sheet == nil { NoteStrip(note: $note).padding(.top, 4) } }
            // (the keybed slides up over the rows; the KEEP / CANCEL bar is the root's)
            .overlay(alignment: .bottom) {
                if model.editor.keysOpen {
                    KeybedPanel(e: model.editor).transition(.move(edge: .bottom).combined(with: .opacity))
                }
            }
            .clipped()
            if model.actions.trackOn {
                KeybedGrip(open: Binding(get: { model.editor.keysOpen }, set: { model.editor.keysOpen = $0 }))
                    .padding(.bottom, 96)
            }
        }
        .animation(.spring(response: 0.35, dampingFraction: 0.85), value: note)
        .sheet(item: $sheet) { s in
            PresetSheet(model: model, entry: entry(s.id), note: $note, ask: $ask, onExport: export) { sheet = nil }
        }
    }

    // MARK: iPad

    private var padBody: some View {
        let shown = LibraryFilter(source: .all, tags: filter.tags, pool: filter.pool).shown(model.rows)
        return VStack(alignment: .leading, spacing: 14) {
            HStack(spacing: 10) {
                // (scrolls sideways: the iPad held upright has no room for them all)
                ChipScroll(inset: 0, spacing: 6) { chips }
                    .frame(height: 44, alignment: .leading)
                TrackPill(model: model, wide: true)
            }
            HStack(alignment: .top, spacing: 14) {
                Card(padding: 18) {
                    HStack(spacing: 10) {
                        Heading(text: "MY PRESETS", count: "\(model.mine.count) / \(nlib)")
                        Spacer(minLength: 0)
                        Button { export(shown.mine) } label: {
                            IconView(Icons.share, color: FZ.text.color, lineWidth: 2.2).frame(width: 18, height: 18)
                                .frame(width: 36, height: 36)
                                .overlay(Circle().strokeBorder(FZ.line.color, lineWidth: 1.5))
                        }
                        .buttonStyle(.plain)
                    }
                    .padding(.bottom, 12)
                    ScreenScroll(bottom: 10, refresh: { await model.refresh() }) {
                        ForEach(shown.mine, id: \.slot) { row($0, compact: false) }
                    }
                }
                .frame(width: 362)
                Card(padding: 14) {
                    Heading(text: "FACTORY", count: "\(model.factory.count)").padding(.bottom, 12)
                    ScreenScroll(bottom: 10) { ForEach(shown.factory, id: \.slot) { row($0, compact: true) } }
                }
                .frame(width: 236)
                Card(padding: 18) {
                    if let r = entry(selected) {
                        PresetPanel(model: model, entry: r, wide: true, note: $note, ask: $ask, onExport: export) { selected = nil }
                            .id(key(r))
                    } else {
                        TabPlaceholder(tab: .library)
                    }
                }
                .frame(maxWidth: .infinity, maxHeight: .infinity)
                .overlay(alignment: .top) { NoteStrip(note: $note).padding(.top, 10) }
            }
            .frame(maxHeight: .infinity)
            if model.actions.trackOn { Keybed(e: model.editor, height: 104) }
        }
        .padding(.bottom, 26)
        .animation(.spring(response: 0.35, dampingFraction: 0.85), value: note)
    }
}

// MARK: - The phone's sheet

/// The preset sheet (mock-up 10): medium, pulled up to large; its own confirm card and outcome strip.
struct PresetSheet: View {
    let model: UnitModel
    let entry: FizzListEntry?
    @Binding var note: ScreenNote?
    @Binding var ask: ConfirmAsk?
    var onExport: ([FizzListEntry]) -> Void
    let close: () -> Void
    @State private var sheetAsk: ConfirmAsk?

    var body: some View {
        ZStack(alignment: .top) {
            RGB(0x0d0d11).color.ignoresSafeArea()
            if let e = entry {
                ScrollView {
                    PresetPanel(model: model, entry: e, note: $note, ask: $sheetAsk, onExport: { r in close(); onExport(r) }, onClose: close)
                        .padding(.horizontal, 18).padding(.top, 24).padding(.bottom, 30)
                }
                .scrollIndicators(.hidden)
            }
            NoteStrip(note: $note).padding(.top, 8)
        }
        .confirmCard($sheetAsk)
        .animation(.spring(response: 0.35, dampingFraction: 0.85), value: note)
        .presentationDetents([.fraction(0.68), .large])
        .presentationDragIndicator(.visible)
        .presentationCornerRadius(30)
        .presentationBackground(RGB(0x0d0d11).color)
        .preferredColorScheme(.dark)
        .onChange(of: entry == nil) { _, gone in if gone { close() } }
    }
}

/// The sheet's top (a snapshot's): rounded above.
struct SheetShape: Shape {
    func path(in r: CGRect) -> Path {
        Path(roundedRect: r, cornerRadii: RectangleCornerRadii(topLeading: 30, bottomLeading: 0, bottomTrailing: 0, topTrailing: 30))
    }
}

// MARK: - A row

/// One preset row (PresetRow) that swipes: right, + POOL (− POOL when in it; pulled all the way, done at once); left,
/// DELETE (MY PRESETS); a MY preset is dragged onto another to swap their places.
struct LibraryRow: View {
    let entry: FizzListEntry
    let algorithm: Int
    var selected = false
    var compact = false
    var places: [Int] = []
    var canWrite = true
    var nameSize: CGFloat = 16
    /// (the snapshots: shown swiped right)
    var open = false
    let onTap: () -> Void
    let onPool: () -> Void
    let onDelete: () -> Void
    let onDrop: (Int) -> Void

    @State private var dx: CGFloat = 0
    @State private var drag: CGFloat = 0
    @State private var horizontal: Bool?
    @State private var target = false
    static let reveal: CGFloat = 132
    static let full: CGFloat = 220

    private var mine: Bool { entry.source == "mine" }
    private var inPool: Bool { !places.isEmpty }

    var body: some View {
        let x = open ? Self.reveal : dx + drag
        ZStack {
            if x > 4 { poolAction(x) }
            if x < -4 { deleteAction }
            PresetRow(entry: entry, algorithm: algorithm, selected: selected || target, marks: !compact, nameSize: nameSize)
                .offset(x: x)
                .onTapGesture {
                    if dx != 0 { withAnimation(.spring(response: 0.35, dampingFraction: 0.8)) { dx = 0 } } else { onTap() }
                }
                .simultaneousGesture(canWrite && !compact ? swipe : nil)
        }
        .frame(height: 56)
        .modifier(Reorder(entry: entry, enabled: mine && canWrite, target: $target, onDrop: onDrop))
    }

    /// Green, under the row: + POOL / − POOL; a tap does it.
    private func poolAction(_ x: CGFloat) -> some View {
        Button { withAnimation(.spring(response: 0.35, dampingFraction: 0.8)) { dx = 0 }; onPool() } label: {
            HStack(spacing: 8) {
                IconView(inPool ? Icons.minus : Icons.plus, color: .black, lineWidth: 3, backdrop: FZ.green.color)
                    .frame(width: 22, height: 22)
                Text("POOL").fz(16, 800, em: 0.08).foregroundStyle(.black)
            }
            .padding(.leading, 20)
            .frame(width: max(Self.reveal, x + 8), alignment: .leading).frame(maxHeight: .infinity)
            .background(RoundedRectangle(cornerRadius: 12).fill(FZ.green.color))
            .shadow(color: FZ.green.opacity(0.5), radius: 12)
        }
        .buttonStyle(.plain)
        .frame(maxWidth: .infinity, alignment: .leading)
    }

    private var deleteAction: some View {
        Button { withAnimation(.spring(response: 0.35, dampingFraction: 0.8)) { dx = 0 }; onDelete() } label: {
            VStack(spacing: 4) {
                IconView(Icons.trash, color: .black, lineWidth: 2.3, backdrop: FZ.red.color).frame(width: 22, height: 22)
                Text("DELETE").fz(12, 800, em: 0.08).foregroundStyle(.black)
            }
            .frame(width: 92).frame(maxHeight: .infinity)
            .background(RoundedRectangle(cornerRadius: 12).fill(FZ.red.color))
        }
        .buttonStyle(.plain)
        .frame(maxWidth: .infinity, alignment: .trailing)
    }

    private var swipe: some Gesture {
        DragGesture(minimumDistance: 14)
            .onChanged { v in
                if horizontal == nil { horizontal = abs(v.translation.width) > abs(v.translation.height) }
                guard horizontal == true else { return }
                var t = v.translation.width
                if !mine && dx + t < 0 { t = -dx }                       // (the factory's: nothing on the left)
                let left = -92 - (dx + t)
                drag = left > 0 ? t + left - left / 3 : t                 // (past DELETE it gives way slowly)
            }
            .onEnded { v in
                defer { horizontal = nil }
                guard horizontal == true else { return }
                let end = dx + v.translation.width
                withAnimation(.spring(response: 0.35, dampingFraction: 0.8)) {
                    drag = 0
                    if end > Self.full { dx = 0; onPool() }               // (a full swipe: done)
                    else if end > Self.reveal / 2 { dx = Self.reveal }
                    else if end < -46 && mine { dx = -92 }
                    else { dx = 0 }
                }
            }
    }
}

/// A MY preset dragged onto another (long press, then drag): SWAP.
private struct Reorder: ViewModifier {
    let entry: FizzListEntry
    let enabled: Bool
    @Binding var target: Bool
    let onDrop: (Int) -> Void
    @Environment(\.fizzSnapshot) private var snapshot

    func body(content: Content) -> some View {
        if enabled && !snapshot {
            content
                .draggable("mine:\(entry.slot)") {
                    PresetRow(entry: entry, algorithm: 1, selected: true).frame(width: 300).opacity(0.9)
                }
                .dropDestination(for: String.self) { items, _ in
                    guard let s = items.first, s.hasPrefix("mine:"), let from = Int(s.dropFirst(5)), from != entry.slot else { return false }
                    onDrop(from)
                    return true
                } isTargeted: { target = $0 }
        } else {
            content
        }
    }
}

/// ALL / MY PRESETS / FACTORY (.seg), the mock-up's two first.
struct SourceSeg: View {
    @Binding var selected: LibraryFilter.Source
    var body: some View {
        HStack(spacing: 0) {
            ForEach([LibraryFilter.Source.mine, .factory, .all], id: \.self) { s in
                Button { selected = s } label: {
                    Text(s == .mine ? "MY PRESETS" : s == .factory ? "FACTORY" : "ALL").fz(13, 800, em: 0.07)
                        .foregroundStyle(s == selected ? Color.black : FZ.muted.color)
                        .padding(.horizontal, 12).frame(height: 38)
                        .background(s == selected ? FZ.cream.color : .clear)
                }
                .buttonStyle(.plain)
            }
        }
        .clipShape(Capsule())
        .overlay(Capsule().strokeBorder(FZ.line.color, lineWidth: 1.5))
        .fixedSize()
    }
}

/// "143 / 256"
struct Count: View {
    let n: Int
    var of: Int?
    var body: some View {
        HStack(alignment: .firstTextBaseline, spacing: 0) {
            Text("\(n)").fz(24, 800).foregroundStyle(FZ.text.color)
            if let of { Text(" / \(of)").fz(15, 800).foregroundStyle(FZ.muted.color) }
        }
        .fixedSize()
    }
}

/// EXPORT's sounds (the EXPORT sheet's).
struct ExportBox: Identifiable {
    let id = UUID()
    let sounds: [FizzSound]
}

/// The preset sheet over LIBRARY as the phone shows it (fizzshots: ImageRenderer draws no sheets).
public struct PresetSheetShot: View {
    let model: UnitModel
    let key: String
    public init(model: UnitModel, key: String) { self.model = model; self.key = key }

    public var body: some View {
        ZStack(alignment: .bottom) {
            RootView(model: model, tab: .constant(.library), wide: false)
                .blur(radius: 2).brightness(-0.3)
            Color.black.opacity(0.45)
            if let r = model.rows.first(where: { "\($0.source):\($0.slot)" == key }) {
                PresetPanel(model: model, entry: r, note: .constant(nil), ask: .constant(nil))
                    .padding(.horizontal, 18).padding(.top, 30).padding(.bottom, 44)
                    .background(SheetShape().fill(RGB(0x0d0d11).color).shadow(color: .black, radius: 30, y: -10))
                    .overlay(alignment: .top) { Capsule().fill(RGB(0x3a3a44).color).frame(width: 40, height: 5).padding(.top, 10) }
            }
        }
        .environment(\.fizzShotSelection, key)
    }
}
