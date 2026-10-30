// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// LIBRARY (mock-ups 09, 10, 16; manager.js renderLibrary / renderSel / swapMine, presets.js): MY PRESETS n / 256 (128
// on the Bluetooth package) and FACTORY; ALL / MY PRESETS / FACTORY, the pool and the tags as filters. A row swiped
// right goes into the pool (out of it when it is in), swiped left offers DELETE (sideways only: a vertical swipe
// always scrolls). MOVE in the preset swaps its place with the next MY preset tapped (as POOL's MOVE); on the iPad a
// MY preset can also be dragged onto another. A tap opens the preset (the phone's sheet, the iPad's third column)
// and, with TRACK on, plays it on the track (KEEP / CANCEL). The search (manager.js l-q) narrows MY PRESETS, FACTORY
// and KEPT by name as it is typed: on the phone a magnifier before the filter chips opens it under the sections, the
// iPad's sits in the header; it stays while the sections change and goes with the tab. SELECT (mock-ups 41, 42, 47;
// LibrarySelect.swift) ticks MY PRESETS and FACTORY rows for TAG, + / − POOL, DELETE, EXPORT and SEND: on the phone
// its bar takes the tab bar's place, on the iPad its panel the detail column; one side at a time (KEPT's or the
// unit's), the other side greyed on the iPad.

import SwiftUI
import FizzData
import FizzKit

struct LibraryView: View {
    let model: UnitModel
    var wide = false
    /// the iPhone on its side: the phone's list, the preset beside it (LandscapeRoot.swift)
    var sideways = false
    @Environment(\.fizzSnapshot) private var snapshot
    @Environment(\.fizzShotSelection) private var shotSelection
    @Environment(\.fizzBarClearance) private var clearance
    @State private var filter = LibraryFilter()
    @State private var selected: String?
    @State private var sheet: SheetKey?
    @State private var note: ScreenNote?
    @State private var ask: ConfirmAsk?
    @State private var exporting: ExportBox?
    /// MOVE: a MY preset's slot; the next MY preset tapped swaps places with it
    @State private var moving: Int?
    /// the row swiped open (one at a time; a tap anywhere in the list shuts it)
    @State private var openRow: String?
    /// KEPT, the phone's presets, shown (remembered; first whenever there is no FM-1)
    @AppStorage("fizz.library.kept") private var keptOn = false
    /// the iPad's KEPT column: a kept preset shown in the third column ("kept:<file>") and the drop target lit
    @State private var keptDrop = false
    @State private var unitDrop = false
    @State private var padPicked: Set<String> = []
    @State private var padTarget: KeptTarget = .mine
    /// the phone's search field shown (the iPad's always is)
    @State private var searching = false
    @FocusState private var searchFocus: Bool
    /// SELECT's sheets (TAG, DELETE) and the phone's bar's height
    @State private var tagging: BulkTagAsk?
    @State private var deleting: BulkDeleteAsk?
    @State private var bulkHeight: CGFloat = 200

    struct SheetKey: Identifiable, Equatable { let id: String }

    var body: some View {
        Group {
            if wide { padBody } else if sideways { sideBody } else { phoneBody }
        }
        .overlay(alignment: .bottom) {
            if let m = moving {
                MoveBar(slot: m, name: model.mine.first { $0.slot == m }?.name ?? "—") { moving = nil }
                    .frame(maxWidth: 560)
                    .padding(.horizontal, wide ? 0 : 12).padding(.bottom, wide ? 26 : clearance)
                    .transition(.move(edge: .bottom).combined(with: .opacity))
            }
        }
        .animation(.spring(response: 0.35, dampingFraction: 0.85), value: moving)
        .confirmCard($ask)
        .hardwareKeys(model.editor)
        .sheet(item: $exporting) { box in
            ExportSheet(sounds: box.sounds) { exporting = nil }
        }
        .sheet(item: $tagging) { _ in BulkTagSheet(flow: bulk) { tagging = nil } }
        .sheet(item: $deleting) { a in BulkDeleteSheet(flow: bulk, ask: a) { deleting = nil } }
        .onAppear {
            if snapshot, let s = shotSelection, s.hasPrefix("mine:") || s.hasPrefix("factory:") || s.hasPrefix("kept:") { selected = s }
            if snapshot { keptOn = shotSelection?.hasPrefix("kept") == true } else if model.noFM1 { keptOn = true }
            // (fizzshots: "select:mine:2,factory:14" ticks those, " search:<name>" after it types a search too)
            if snapshot, let s = shotSelection, s.hasPrefix("select:") {
                let ks = s.dropFirst(7).prefix { $0 != " " }.split(separator: ",").map(String.init)
                var t = Transaction()
                t.disablesAnimations = true
                withTransaction(t) {
                    model.picks.on = true
                    if !ks.isEmpty { model.picks.set(Set(ks)) }
                    filter.source = .all
                }
            }
            // (fizzshots: "search:<name>" on ALL, "kept-search:<name>" on KEPT)
            if snapshot, let s = shotSelection, let r = s.range(of: "search:") {
                var t = Transaction()
                t.disablesAnimations = true
                withTransaction(t) {
                    searching = true
                    filter.query = String(s[r.upperBound...])
                    if !keptOn { filter.source = .all }
                }
            }
        }
        // (the search and SELECT go with the tab)
        .onDisappear {
            closeSearch()
            if !snapshot { model.picks.clear() }
        }
        .onChange(of: model.noFM1) { _, no in if no { keptOn = true } }
        // (one side at a time: KEPT shown on the phone ends the unit's SELECT; another unit too)
        .onChange(of: keptOn) { _, k in if k && !wide { model.picks.clear() } }
        .onChange(of: model.unitKey) { _, _ in model.picks.clear() }
        // (`-select mine:2,mine:19` with a demo unit: SELECT on ALL with those ticked, for the simulator's screenshots)
        .task(id: model.dumpsLoaded) {
            let a = ProcessInfo.processInfo.arguments
            guard !snapshot, model.dumpsLoaded, model.source is DemoUnitsSource || model.source is DemoLinkSource,
                  let k = a.firstIndex(of: "-select"), k + 1 < a.count else { return }
            try? await Task.sleep(for: .milliseconds(1200))
            keptOn = false
            filter.source = .all
            model.picks.on = true
            model.picks.set(Set(a[k + 1].split(separator: ",").map(String.init)))
            if a.contains("-select-tag") { tagging = BulkTagAsk() }
            if a.contains("-select-delete") { bulk.askDelete() }
        }
        // (`-preset mine:2` with a demo unit: that preset's sheet open, for the simulator's screenshots)
        .task(id: model.dumpsLoaded) {
            let a = ProcessInfo.processInfo.arguments
            guard !snapshot, model.dumpsLoaded, model.source is DemoUnitsSource || model.source is DemoLinkSource,
                  let k = a.firstIndex(of: "-preset"), k + 1 < a.count, entry(a[k + 1]) != nil else { return }
            try? await Task.sleep(for: .milliseconds(1200))
            keptOn = false
            selected = a[k + 1]
            if !wide { sheet = SheetKey(id: a[k + 1]) }
        }
        // (`-search bass` with a demo unit: the search open on ALL with that typed, for the simulator's screenshots)
        .task(id: model.dumpsLoaded) {
            let a = ProcessInfo.processInfo.arguments
            guard !snapshot, model.dumpsLoaded, model.source is DemoUnitsSource || model.source is DemoLinkSource,
                  let k = a.firstIndex(of: "-search"), k + 1 < a.count else { return }
            try? await Task.sleep(for: .milliseconds(1200))
            if !a.contains("-kept") { keptOn = false; filter.source = .all }
            searching = true
            filter.query = a[k + 1]
        }
        // (a .fizzsnd opened in FIZZ: KEPT, its sheet)
        .onChange(of: model.kept.reveal) { _, id in
            guard let id else { return }
            keptOn = true
            if wide { selected = "kept:\(id)"; model.kept.reveal = nil }
        }
    }

    private func key(_ r: FizzListEntry) -> String { "\(r.source):\(r.slot)" }
    private var picks: LibraryPicks { model.picks }
    private var bulk: BulkFlow { BulkFlow(model: model, note: $note, ask: $ask, deleting: $deleting, exporting: $exporting) }

    /// SELECT / DONE on the unit's side (KEPT's selection ends: one side at a time).
    private func toggleSelect() {
        if picks.active { picks.clear(); return }
        if model.kept.selecting { setPadSelecting(false) }
        moving = nil
        openRow = nil
        sheet = nil
        picks.start()
    }
    private var hasQuery: Bool { !NameSearch.clean(filter.query).isEmpty }

    private func toggleSearch() {
        if searching { closeSearch() } else {
            searching = true
            searchFocus = true
        }
    }

    private func closeSearch() {
        searchFocus = false
        searching = false
        filter.query = ""
    }
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
                FilterChip(title: t.name, color: t.tint, on: filter.tags.contains(t), tag: t) { filter.toggle(t) }
            }
        }
    }

    // MARK: rows

    private func tap(_ r: FizzListEntry) {
        openRow = nil
        // (SELECT on: a tap ticks; the iPad's KEPT selection gives way, as the web's pick of the other side)
        if wide && model.kept.selecting {
            setPadSelecting(false)
            picks.start()
        }
        if picks.active {
            picks.toggle(r)
            return
        }
        if let m = moving {
            guard r.source == "mine" else { return }
            moving = nil
            move(m, to: r.slot)
            return
        }
        let k = key(r)
        if wide || sideways {
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
                   moving: moving != nil && r.source == "mine" && r.slot == moving, dragReorder: wide && !picks.active,
                   ticking: picks.active, ticked: picks.picked(r),
                   isOpen: openRow == key(r), setOpen: { o in openRow = o ? key(r) : openRow == key(r) ? nil : openRow },
                   open: snapshot && shotSelection == "swipe:\(key(r))",
                   onTap: { tap(r) },
                   onPool: { togglePool(r) },
                   onDelete: { askDelete(r) },
                   onDrop: { from in move(from, to: r.slot) },
                   onKept: { id in sendDropped(id) })
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

    /// MOVE in the preset: the sheet goes, the next MY preset tapped swaps places with it.
    private func startMove(_ r: FizzListEntry) {
        sheet = nil
        moving = r.slot
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
        VStack(alignment: .leading, spacing: 0) {
            LibrarySourceSeg(kept: $keptOn, source: $filter.source)
                .padding(.horizontal, 16).padding(.top, 4).padding(.bottom, 12)
            if !keptOn {
                // (the unit's count, SELECT; Part E's + NEW beside it)
                HStack(spacing: 10) {
                    Count(n: filter.source == .factory ? model.factory.count : model.mine.count,
                          of: filter.source == .factory ? nil : nlib)
                    Spacer(minLength: 0)
                    SelectPill(selecting: picks.active, enabled: model.unit != nil) { toggleSelect() }
                    NewSoundButton(model: model)
                }
                .padding(.horizontal, 16).padding(.bottom, 12)
            }
            if searching {
                SearchField(text: $filter.query, focus: $searchFocus)
                    .padding(.horizontal, 16).padding(.bottom, 12)
                    .transition(.move(edge: .top).combined(with: .opacity))
            }
            if keptOn {
                KeptList(model: model, tags: $filter.tags, query: filter.query,
                         search: SearchToggle(open: searching, active: hasQuery, action: toggleSearch))
                    .overlay(alignment: .bottom) {
                        if model.editor.keysOpen && !model.kept.selecting {
                            KeybedPanel(e: model.editor).transition(.move(edge: .bottom).combined(with: .opacity))
                        }
                    }
                    .clipped()
            } else {
                unitList
            }
            if model.actions.trackOn && !(keptOn && model.kept.selecting) && !(!keptOn && picks.active) {
                KeybedGrip(open: Binding(get: { model.editor.keysOpen }, set: { model.editor.keysOpen = $0 }))
                    .padding(.bottom, clearance - 4)
            }
        }
        .animation(.spring(response: 0.35, dampingFraction: 0.85), value: note)
        .animation(.spring(response: 0.35, dampingFraction: 0.85), value: keptOn)
        .animation(.spring(response: 0.35, dampingFraction: 0.85), value: searching)
        .sheet(item: $sheet) { s in
            PresetSheet(model: model, entry: entry(s.id), note: $note, onExport: export, onMove: startMove) { sheet = nil }
        }
    }

    /// The unit's MY PRESETS / FACTORY / ALL (the phone).
    @ViewBuilder private var unitList: some View {
        let shown = filter.shown(model.rows)
        VStack(alignment: .leading, spacing: 0) {
            ChipScroll {
                SearchToggle(open: searching, active: hasQuery, action: toggleSearch)
                chips
            }
                .padding(.bottom, 14)
            ScreenScroll(bottom: (picks.active && !sideways ? bulkHeight + 10 : clearance) + 20 + (moving != nil ? 62 : 0), lazy: true,
                         refresh: { await model.refresh() }) {
                if filter.source == .all && !shown.mine.isEmpty { group("MY PRESETS", shown.mine.count) }
                ForEach(shown.mine, id: \.slot) { row($0) }
                if filter.source == .all && !shown.factory.isEmpty { group("FACTORY", shown.factory.count) }
                ForEach(shown.factory, id: \.slot) { row($0) }
                if hasQuery && shown.mine.isEmpty && shown.factory.isEmpty { NothingCalled(query: filter.query) }
                if (!shown.mine.isEmpty || !shown.factory.isEmpty) && !picks.active {
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
                if model.editor.keysOpen && !picks.active {
                    KeybedPanel(e: model.editor).transition(.move(edge: .bottom).combined(with: .opacity))
                }
            }
            .clipped()
        }
        // (SELECT's bar in the tab bar's place, as KEPT's)
        .overlay(alignment: .bottom) {
            if picks.active && !sideways {
                BulkBar(flow: bulk, shown: shown.mine + shown.factory) { tagging = BulkTagAsk() }
                    .onGeometryChange(for: CGFloat.self) { $0.size.height } action: { bulkHeight = $0 }
                    .padding(.horizontal, 12).padding(.bottom, PhoneRoot.barBottom - 6)
                    .transition(.move(edge: .bottom).combined(with: .opacity))
            }
        }
        .animation(.spring(response: 0.35, dampingFraction: 0.85), value: picks.active)
    }

    // MARK: iPhone on its side

    /// The phone's list; beside it the preset tapped, or SELECT's panel (as the iPad's third column). KEPT: its list
    /// alone, as wide as the screen.
    private var sideBody: some View {
        HStack(alignment: .top, spacing: 12) {
            phoneBody
            if !keptOn {
                Card(padding: 14) {
                    let shown = filter.shown(model.rows)
                    if picks.active {
                        ScreenScroll(bottom: 4) { BulkPanel(flow: bulk, shown: shown.mine + shown.factory) }
                    } else if let r = entry(selected) {
                        ScreenScroll(bottom: 4) {
                            PresetPanel(model: model, entry: r, wide: true, note: $note, ask: $ask, onExport: export, onMove: startMove) {
                                selected = nil
                            }
                        }
                        .id(key(r))
                    } else {
                        TabPlaceholder(tab: .library)
                    }
                }
                .frame(width: 330)
                .frame(maxHeight: .infinity)
                .padding(.top, 4).padding(.bottom, clearance)
            }
        }
    }

    // MARK: iPad

    /// The unit's column (MY PRESETS / FACTORY / ALL), KEPT's, the preset; a row dragged from one list to the other
    /// copies it (onto the unit: SEND to MY PRESETS; onto KEPT: → KEPT).
    private var padBody: some View {
        let shown = filter.shown(model.rows)
        let kshown = keptShown(model.kept.items, tags: filter.tags, query: filter.query)
        return VStack(alignment: .leading, spacing: 14) {
            HStack(spacing: 10) {
                SearchField(text: $filter.query, height: 44, focus: $searchFocus)
                    .frame(width: 230)
                // (scrolls sideways: the iPad held upright has no room for them all)
                ChipScroll(inset: 0, spacing: 6) { chips }
                    .frame(height: 44, alignment: .leading)
                TrackPill(model: model, wide: true)
            }
            HStack(alignment: .top, spacing: 14) {
                Card(padding: 16) {
                    SourceSeg(selected: $filter.source).padding(.bottom, 14)
                    HStack(spacing: 8) {
                        if model.unitIsEmulated { PhoneGlyph(size: 13, color: FZ.orange) }
                        Heading(text: model.unitName, count: filter.source == .factory ? "\(model.factory.count)" : "\(model.mine.count) / \(nlib)",
                                color: model.unitIsEmulated ? FZ.orange : FZ.green)
                        Spacer(minLength: 0)
                        // SELECT (cream while on); Part E's + NEW beside it
                        RoundIcon(art: KeptArt.check, color: picks.active ? FZ.cream : FZ.text, enabled: model.unit != nil, size: 36) {
                            toggleSelect()
                        }
                        .background(Circle().fill(picks.active ? FZ.cream.opacity(0.18) : .clear).frame(width: 36, height: 36))
                        NewSoundButton(model: model)
                        Button { export(shown.mine + shown.factory) } label: {
                            IconView(Icons.share, color: FZ.text.color, lineWidth: 2.2).frame(width: 18, height: 18)
                                .frame(width: 36, height: 36)
                                .overlay(Circle().strokeBorder(FZ.line.color, lineWidth: 1.5))
                                .frame(width: 44, height: 44).contentShape(Circle())
                        }
                        .buttonStyle(.plain)
                    }
                    .padding(.bottom, 8)
                    ScreenScroll(bottom: 10, refresh: { await model.refresh() }) {
                        if filter.source == .all && !shown.mine.isEmpty { group("MY PRESETS", shown.mine.count) }
                        ForEach(shown.mine, id: \.slot) { row($0) }
                        if filter.source == .all && !shown.factory.isEmpty { group("FACTORY", shown.factory.count) }
                        ForEach(shown.factory, id: \.slot) { row($0) }
                        if hasQuery && shown.mine.isEmpty && shown.factory.isEmpty { NothingCalled(query: filter.query) }
                    }
                }
                .overlay(RoundedRectangle(cornerRadius: 22).strokeBorder(FZ.violet.color, lineWidth: 2).opacity(unitDrop ? 1 : 0))
                // (KEPT's SELECT: the unit's side greyed, a tap takes it back)
                .opacity(model.kept.selecting ? 0.45 : 1)
                .modifier(KeptDrop(accept: { s in
                    guard s.hasPrefix("kept:") else { return false }
                    sendDropped(String(s.dropFirst(5)))
                    return true
                }, lit: $unitDrop))
                // (SELECT on: the unit's column takes room from KEPT's, greyed, for the ticks)
                .frame(width: picks.active ? 360 : 340)
                Card(padding: 14) {
                    KeptHead(count: model.kept.items.count, selecting: model.kept.selecting, compact: true,
                             select: { setPadSelecting(!model.kept.selecting) }, new: newKept)
                        .padding(.bottom, 10)
                    ScreenScroll(bottom: model.kept.selecting ? 220 : 10, refresh: { await model.kept.reload() }) {
                        ForEach(kshown) { k in
                            KeptRow(item: k, selecting: model.kept.selecting, picked: padPicked.contains(k.id),
                                    selected: selected == "kept:\(k.id)", compact: true) { tapKept(k) }
                                .modifier(KeptDrag(id: k.id, name: k.name))
                                .padding(.bottom, 8)
                        }
                        if model.kept.loaded && model.kept.items.isEmpty { KeptEmpty(new: newKept) }
                        if hasQuery && kshown.isEmpty && !model.kept.items.isEmpty { NothingCalled(query: filter.query) }
                    }
                }
                .opacity(picks.active ? 0.4 : 1)
                .background(RoundedRectangle(cornerRadius: 22).fill(FZ.champagne.opacity(0.04)))
                .overlay(RoundedRectangle(cornerRadius: 22).strokeBorder(FZ.champagne.opacity(keptDrop ? 1 : 0.45), lineWidth: keptDrop ? 2 : 1))
                .modifier(KeptDrop(accept: { s in
                    guard let r = entry(s), r.source != "pool" else { return false }
                    keepRow(r)
                    return true
                }, lit: $keptDrop))
                .frame(width: picks.active ? 252 : 272)
                Card(padding: 18) {
                    if picks.active {
                        BulkPanel(flow: bulk, shown: shown.mine + shown.factory)
                    } else if let r = entry(selected) {
                        PresetPanel(model: model, entry: r, wide: true, note: $note, ask: $ask, onExport: export, onMove: startMove) {
                            selected = nil
                        }
                        .id(key(r))
                    } else if let s = selected, s.hasPrefix("kept:"), model.kept.item(String(s.dropFirst(5))) != nil {
                        KeptPanel(model: model, id: Binding(get: { selected.map { String($0.dropFirst(5)) } },
                                                            set: { selected = $0.map { "kept:\($0)" } }),
                                  wide: true, note: $note, ask: $ask) { selected = nil }
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
        .overlay(alignment: .bottom) {
            if model.kept.selecting {
                KeptSelectBar(model: model, picked: padPicked, all: kshown.map(\.id), target: $padTarget, note: $note,
                              toggleAll: {
                                  let all = Set(kshown.map(\.id))
                                  padPicked = padPicked.isSuperset(of: all) ? [] : all
                              },
                              delete: askDeleteKept, sent: { setPadSelecting(false) })
                    .frame(maxWidth: 560).padding(.bottom, 26)
                    .transition(.move(edge: .bottom).combined(with: .opacity))
            }
        }
        .animation(.spring(response: 0.35, dampingFraction: 0.85), value: note)
        .animation(.spring(response: 0.35, dampingFraction: 0.85), value: model.kept.selecting)
        .animation(.spring(response: 0.35, dampingFraction: 0.85), value: picks.active)
        .onDisappear { if model.kept.selecting { setPadSelecting(false) } }
    }

    // MARK: KEPT (both)

    private func setPadSelecting(_ on: Bool) {
        padPicked = []
        if on { picks.clear() }
        model.kept.selecting = on
    }

    private func tapKept(_ k: KeptPreset) {
        // (the unit's rows ticked: a KEPT row takes the selection to KEPT's side, as the web's pick of the other side)
        if picks.active {
            setPadSelecting(true)
            padPicked = [k.id]
            return
        }
        if model.kept.selecting {
            if padPicked.contains(k.id) { padPicked.remove(k.id) } else { padPicked.insert(k.id) }
            return
        }
        let key = "kept:\(k.id)"
        selected = selected == key ? nil : key
        hear(k, model: model) { note = $0 }
    }

    private func newKept() {
        if model.kept.selecting { setPadSelecting(false) }
        model.editTarget = .newKept
        model.goTo = .edit
    }

    /// A unit preset dropped on KEPT (or its → KEPT): copied onto the phone.
    private func keepRow(_ r: FizzListEntry) {
        Task {
            switch await model.keep(r) {
            case .kept(let k): note = .done("KEPT · \(k.name)", FZ.champagne)
            case .already(let k): note = ScreenNote(text: "KEPT ALREADY · \(k.name)", color: FZ.champagne, kind: .note)
            case .failed: note = ScreenNote(text: "NOT READ YET", color: FZ.muted, kind: .refused)
            }
        }
    }

    /// A kept preset dropped on the unit's list: SEND to MY PRESETS.
    private func sendDropped(_ id: String) {
        guard let k = model.kept.item(id) else { return }
        Task {
            let out = await model.send(kept: [k], to: .mine)
            note = keptSendNote(out, to: .mine, unit: model.unitName)
        }
    }

    private func askDeleteKept() {
        let ks = model.kept.items.filter { padPicked.contains($0.id) }
        guard !ks.isEmpty else { return }
        ask = ConfirmAsk(title: "DELETE · KEPT", what: ks.count == 1 ? ks[0].name : "\(ks.count) PRESETS",
                         detail: "OFF THIS PHONE", go: "DELETE", color: FZ.red) {
            model.kept.delete(ks.map(\.id))
            padPicked = []
            note = .done("DELETED \(ks.count == 1 ? ks[0].name : "\(ks.count)")", FZ.red)
        }
    }
}

// MARK: - The phone's sheet

/// The preset sheet (mock-up 10): a FizzSheet opened at its content's height with the buttons pinned at the bottom;
/// its confirms (DELETE, CLEAR POOL) standalone over it; the outcome strip on top.
struct PresetSheet: View {
    let model: UnitModel
    let entry: FizzListEntry?
    @Binding var note: ScreenNote?
    var onExport: ([FizzListEntry]) -> Void
    var onMove: (FizzListEntry) -> Void
    let close: () -> Void
    @State private var ask: ConfirmAsk?

    var body: some View {
        Group {
            if let e = entry {
                PresetPanel(model: model, entry: e, note: $note, ask: $ask, onExport: { r in close(); onExport(r) },
                            onMove: onMove, onClose: close)
            } else {
                sheetBackground.color.ignoresSafeArea()
            }
        }
        .overlay(alignment: .top) { NoteStrip(note: $note).padding(.top, 8) }
        .confirmCard($ask)
        .animation(.spring(response: 0.35, dampingFraction: 0.85), value: note)
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

/// One preset row (PresetRow): a tap opens it (on touch-up, at once); swiped right, + POOL (− POOL when in it; pulled
/// all the way, done at once); swiped left, DELETE (MY PRESETS). The swipes start only on a sideways move, so a
/// vertical swipe always scrolls the list.
struct LibraryRow: View {
    let entry: FizzListEntry
    let algorithm: Int
    var selected = false
    var compact = false
    var places: [Int] = []
    var canWrite = true
    var nameSize: CGFloat = 16
    /// MOVE: this one is moving (yellow)
    var moving = false
    /// the iPad: a MY preset dragged onto another swaps them
    var dragReorder = false
    /// SELECT on: a tick before it (a tap ticks; no swipes)
    var ticking = false
    var ticked = false
    /// open (only one row at a time: another one opening shuts this)
    var isOpen = false
    var setOpen: (Bool) -> Void = { _ in }
    /// (the snapshots: shown swiped right)
    var open = false
    let onTap: () -> Void
    let onPool: () -> Void
    let onDelete: () -> Void
    let onDrop: (Int) -> Void
    /// the iPad: a KEPT preset dropped on this row (SEND to MY PRESETS)
    var onKept: ((String) -> Void)?

    @Environment(\.fizzSnapshot) private var snapshot
    @State private var dx: CGFloat = 0
    @State private var drag: CGFloat = 0
    @State private var target = false
    static let reveal: CGFloat = 132
    static let full: CGFloat = 220
    static let deleteWidth: CGFloat = 92

    private var mine: Bool { entry.source == "mine" }
    private var inPool: Bool { !places.isEmpty }
    private var snap: Animation { .spring(response: 0.35, dampingFraction: 0.8) }

    var body: some View {
        let x = open ? Self.reveal : dx + drag
        ZStack {
            if x > 4 { poolAction(x) }
            if x < -4 { deleteAction }
            Button {
                if dx != 0 { withAnimation(snap) { dx = 0 } } else { onTap() }
            } label: {
                PresetRow(entry: entry, algorithm: algorithm, selected: (selected && !ticking) || target, marks: !compact, nameSize: nameSize,
                          ticking: ticking, ticked: ticked)
                    .overlay {
                        if moving {
                            RoundedRectangle(cornerRadius: 12).strokeBorder(FZ.yellow.color, lineWidth: 2.5)
                                .shadow(color: FZ.yellow.color, radius: 8)
                        }
                    }
            }
            .buttonStyle(PressStyle(scale: 0.98))
            .offset(x: x)
        }
        .frame(height: 56)
        .sidewaysPan(enabled: canWrite && !compact && !snapshot && !ticking, changed: swiped, ended: letGo)
        .modifier(Reorder(entry: entry, enabled: dragReorder && mine && canWrite, target: $target, onDrop: onDrop, onKept: onKept))
        .modifier(KeptDrag(id: nil, entry: entry, enabled: dragReorder && !(mine && canWrite)))
        .onChange(of: dx != 0) { _, o in if o != isOpen { setOpen(o) } }
        .onChange(of: isOpen) { _, o in if !o && dx != 0 { withAnimation(snap) { dx = 0 } } }
        .onChange(of: ticking) { _, t in if t && dx != 0 { withAnimation(snap) { dx = 0 } } }
    }

    /// Green, under the row: + POOL / − POOL; a tap does it.
    private func poolAction(_ x: CGFloat) -> some View {
        Button { withAnimation(snap) { dx = 0 }; onPool() } label: {
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
        .buttonStyle(PressStyle())
        .frame(maxWidth: .infinity, alignment: .leading)
    }

    private var deleteAction: some View {
        Button { withAnimation(snap) { dx = 0 }; onDelete() } label: {
            VStack(spacing: 4) {
                IconView(Icons.trash, color: .black, lineWidth: 2.3, backdrop: FZ.red.color).frame(width: 22, height: 22)
                Text("DELETE").fz(12, 800, em: 0.08).foregroundStyle(.black)
            }
            .frame(width: Self.deleteWidth).frame(maxHeight: .infinity)
            .background(RoundedRectangle(cornerRadius: 12).fill(FZ.red.color))
        }
        .buttonStyle(PressStyle())
        .frame(maxWidth: .infinity, alignment: .trailing)
    }

    private func swiped(_ t: CGFloat) {
        var t = t
        if !mine && dx + t < 0 { t = -dx }                                   // (the factory's: nothing on the left)
        let left = -Self.deleteWidth - (dx + t)
        drag = left > 0 ? t + left - left / 3 : t                             // (past DELETE it gives way slowly)
    }

    /// Where a let-go row ends up.
    enum Settle: Equatable { case shut, poolShown, deleteShown, pool }

    /// Let go at `dx` (where it was) + `moved`: pulled all the way right, + / − POOL at once; else open or shut, a
    /// flick (the speed's extra travel) counting; DELETE only for a MY preset.
    static func settle(dx: CGFloat, moved: CGFloat, flick: CGFloat, mine: Bool) -> Settle {
        let now = dx + moved, end = now + flick
        if now > full { return .pool }
        if end > reveal / 2 { return .poolShown }
        if end < -46 && mine { return .deleteShown }
        return .shut
    }

    private func letGo(_ t: CGFloat?, _ flick: CGFloat) {
        guard let t else { withAnimation(snap) { drag = 0 }; return }
        let s = Self.settle(dx: dx, moved: t, flick: flick, mine: mine)
        withAnimation(snap) {
            drag = 0
            switch s {
            case .pool: dx = 0; onPool()
            case .poolShown: dx = Self.reveal
            case .deleteShown: dx = -Self.deleteWidth
            case .shut: dx = 0
            }
        }
    }
}

/// The iPad: a MY preset dragged onto another (long press, then drag): SWAP.
private struct Reorder: ViewModifier {
    let entry: FizzListEntry
    let enabled: Bool
    @Binding var target: Bool
    let onDrop: (Int) -> Void
    var onKept: ((String) -> Void)?
    @Environment(\.fizzSnapshot) private var snapshot

    func body(content: Content) -> some View {
        if enabled && !snapshot {
            content
                .draggable("mine:\(entry.slot)") {
                    PresetRow(entry: entry, algorithm: 1, selected: true).frame(width: 300).opacity(0.9)
                }
                .dropDestination(for: String.self) { items, _ in
                    if let s = items.first, s.hasPrefix("kept:"), let onKept {
                        onKept(String(s.dropFirst(5)))
                        return true
                    }
                    guard let s = items.first, s.hasPrefix("mine:"), let from = Int(s.dropFirst(5)), from != entry.slot else { return false }
                    onDrop(from)
                    return true
                } isTargeted: { target = $0 }
        } else {
            content
        }
    }
}

/// MOVE's bar (as POOL's): the moving preset, → ?, × to stop.
struct MoveBar: View {
    let slot: Int
    let name: String
    let cancel: () -> Void
    var body: some View {
        HStack(spacing: 10) {
            IconView(Icons.swap, color: FZ.yellow.color, lineWidth: 2.4).frame(width: 24, height: 24)
            Text("\(slot)").fz(15, 800).foregroundStyle(FZ.yellow.color)
            Text(name).fz(20, 800).foregroundStyle(FZ.text.color).lineLimit(1).minimumScaleFactor(0.6)
            Text("→ ?").fz(20, 800).foregroundStyle(FZ.yellow.color)
            Spacer(minLength: 0)
            RoundIcon(art: Icons.close, color: FZ.muted, size: 44, action: cancel)
        }
        .padding(.leading, 18).padding(.trailing, 4)
        .frame(height: 52)
        .background(Capsule().fill(FZ.yellow.mix(10, RGB(0x111115)).color))
        .overlay(Capsule().strokeBorder(FZ.yellow.color, lineWidth: 1.5))
        .shadow(color: .black.opacity(0.7), radius: 14, y: 8)
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
                PresetPanel(model: model, entry: r, note: .constant(nil), ask: .constant(nil), onMove: { _ in })
                    .padding(.horizontal, 18).padding(.top, 30).padding(.bottom, 44)
                    .background(SheetShape().fill(RGB(0x0d0d11).color).shadow(color: .black, radius: 30, y: -10))
                    .overlay(alignment: .top) { Capsule().fill(RGB(0x3a3a44).color).frame(width: 40, height: 5).padding(.top, 10) }
            }
        }
        .environment(\.fizzShotSelection, key)
    }
}

// MARK: - The search

extension Icons {
    /// a magnifier (manager index.html .find)
    static let find = VectorArt(24, 24, [.circle(11, 11, 6), .path("M20 20l-4.5-4.5")])
}

/// The magnifier before LIBRARY's filter chips (the header's seg has no room beside it on a phone): opens the search,
/// shuts it (the search cleared); lit while a search is typed.
struct SearchToggle: View {
    let open: Bool
    let active: Bool
    let action: () -> Void
    var body: some View {
        Button(action: action) {
            IconView(Icons.find, color: active ? .black : FZ.text.color, lineWidth: 2.4, backdrop: .clear)
                .frame(width: 17, height: 17)
                .frame(width: 36, height: 36)
                .background(Circle().fill(active ? FZ.cream.color : open ? FZ.cream.opacity(0.12) : .clear))
                .overlay(Circle().strokeBorder(active ? FZ.cream.color : open ? FZ.cream.mix(60, FZ.line).color : FZ.line.color,
                                               lineWidth: 1.5))
                .shadow(color: active ? FZ.cream.opacity(0.4) : .clear, radius: 7)
                .frame(width: 44, height: 36)
                .contentShape(Rectangle())
        }
        .buttonStyle(PressStyle(scale: 0.9))
    }
}

/// The search field (manager index.html #l-q): the magnifier, the name typed, × clears it.
struct SearchField: View {
    @Binding var text: String
    var height: CGFloat = 44
    var focus: FocusState<Bool>.Binding
    @Environment(\.fizzSnapshot) private var snapshot

    var body: some View {
        HStack(spacing: 10) {
            IconView(Icons.find, color: FZ.muted.color, lineWidth: 2.4, backdrop: .clear).frame(width: 17, height: 17)
            if snapshot {
                // (ImageRenderer draws no text field)
                Text(text.isEmpty ? "FIND A SOUND" : text.uppercased()).fz(16, 800)
                    .foregroundStyle(text.isEmpty ? FZ.muted.opacity(0.7) : FZ.text.color)
                    .frame(maxWidth: .infinity, alignment: .leading)
            } else {
                field
            }
            if !text.isEmpty {
                Button { text = "" } label: {
                    IconView(Icons.close, color: .black, lineWidth: 2.8, backdrop: FZ.muted.color).frame(width: 10, height: 10)
                        .frame(width: 22, height: 22).background(Circle().fill(FZ.muted.color))
                        .frame(width: 40, height: height).contentShape(Rectangle())
                }
                .buttonStyle(.plain)
            }
        }
        .padding(.leading, 16).padding(.trailing, text.isEmpty ? 16 : 4)
        .frame(height: height)
        .background(Capsule().fill(FZ.raised.color))
        .overlay(Capsule().strokeBorder(text.isEmpty ? FZ.line.color : FZ.cream.mix(60, FZ.line).color, lineWidth: 1.5))
    }
}

extension SearchField {
    private var field: some View {
        TextField("", text: $text, prompt: Text("FIND A SOUND").foregroundStyle(FZ.muted.opacity(0.7)))
            .font(FizzFont.font(16, 800))
            .foregroundStyle(FZ.text.color)
            .textFieldStyle(.plain)
            .autocorrectionDisabled()
            #if os(iOS)
            .textInputAutocapitalization(.characters)
            #endif
            .submitLabel(.search)
            .focused(focus)
    }
}

/// Nothing in the list goes by that name: one quiet line.
struct NothingCalled: View {
    let query: String
    var body: some View {
        HStack(spacing: 10) {
            IconView(Icons.find, color: FZ.muted.color, lineWidth: 2.2, backdrop: .clear).frame(width: 18, height: 18)
            Text("NOTHING CALLED “\(NameSearch.clean(query).uppercased())”").fz(14, 800, em: 0.06)
                .foregroundStyle(FZ.muted.color).lineLimit(2)
        }
        .frame(maxWidth: .infinity).padding(.vertical, 28)
    }
}
