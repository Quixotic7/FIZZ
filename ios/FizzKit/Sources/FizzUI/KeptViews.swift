// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// LIBRARY's KEPT (mock-ups 23, 24, 26, 28): the phone's presets in champagne. The list (FIZZ's tags filter it), + NEW
// (EDIT on an INIT sound, saved to KEPT), SELECT (single taps tick rows; the violet bar takes the tab bar's place:
// the count, ALL, SHARE as a .fizzlib, DELETE, MY PRESETS / POOL / TRACK and SEND n TO the unit), EXPORT ALL. A row
// tapped opens its sheet (with TRACK on it is heard on the track first): its name, tags, SEND TO the unit (one tap a
// target), DUPLICATE, EDIT, SHARE (its .fizzsnd), DELETE. The iPad shows the sheet as LIBRARY's third column.

import SwiftUI
import FizzData
import FizzKit

// MARK: - The phone's list

struct KeptList: View {
    let model: UnitModel
    @Binding var tags: Set<FizzTag>
    /// LIBRARY's search, and its magnifier before the chips
    var query = ""
    var search: SearchToggle?
    @Environment(\.fizzSnapshot) private var snapshot
    @Environment(\.fizzShotSelection) private var shotSelection
    @Environment(\.fizzBarClearance) private var clearance
    @State private var sheet: KeptKey?
    @State private var note: ScreenNote?
    @State private var ask: ConfirmAsk?
    @State private var exporting: ExportBox?
    @State private var picked: Set<String> = []
    @State private var target: KeptTarget = .mine
    @State private var barHeight: CGFloat = 190

    struct KeptKey: Identifiable, Equatable { let id: String }

    private var store: KeptPresets { model.kept }
    private var shown: [KeptPreset] { keptShown(store.items, tags: tags, query: query) }
    private var selecting: Bool { store.selecting }

    var body: some View {
        VStack(alignment: .leading, spacing: 0) {
            KeptHead(count: store.items.count, selecting: selecting, size: 46,
                     select: { setSelecting(!selecting) }, new: newSound)
                .padding(.horizontal, 16).padding(.bottom, 12)
            ChipScroll {
                if let search { search }
                tagChips($tags)
            }
                .padding(.bottom, 14)
            ScreenScroll(bottom: (selecting ? barHeight + 10 : clearance) + 20, lazy: true, refresh: { await store.reload() }) {
                if store.loaded && store.items.isEmpty { KeptEmpty(new: newSound) }
                if !NameSearch.clean(query).isEmpty && shown.isEmpty && !store.items.isEmpty { NothingCalled(query: query) }
                ForEach(shown) { k in
                    KeptRow(item: k, selecting: selecting, picked: picked.contains(k.id),
                            heard: false) { tap(k) }
                        .padding(.bottom, 8)
                        .transition(.opacity)
                }
                if !shown.isEmpty && !selecting {
                    IconPill(title: "EXPORT ALL", icon: Icons.share, ghost: true, height: 48, size: 14) { export(shown) }
                        .padding(.top, 8).padding(.horizontal, 40)
                }
            }
            .padding(.horizontal, 16)
            .overlay(alignment: .top) { if sheet == nil { NoteStrip(note: $note).padding(.top, 4) } }
        }
        .overlay(alignment: .bottom) {
            if selecting {
                KeptSelectBar(model: model, picked: picked, all: shown.map(\.id), target: $target, note: $note,
                              toggleAll: toggleAll, delete: askDelete, sent: { setSelecting(false) })
                    .onGeometryChange(for: CGFloat.self) { $0.size.height } action: { barHeight = $0 }
                    .padding(.horizontal, 12).padding(.bottom, PhoneRoot.barBottom - 6)
                    .transition(.move(edge: .bottom).combined(with: .opacity))
            }
        }
        .animation(.spring(response: 0.35, dampingFraction: 0.85), value: selecting)
        .animation(.spring(response: 0.35, dampingFraction: 0.85), value: note)
        .animation(.spring(response: 0.3, dampingFraction: 0.85), value: shown.map(\.id))
        .confirmCard($ask)
        .sheet(item: $sheet) { s in
            KeptSheet(model: model, id: s.id, note: $note, onExport: export) { sheet = nil }
        }
        .sheet(item: $exporting) { box in ExportSheet(sounds: box.sounds) { exporting = nil } }
        .onAppear {
            if snapshot, let s = shotSelection, s.hasPrefix("kept-select:") {
                picked = Set(s.dropFirst(12).split(separator: ",").map(String.init))
            }
        }
        .onChange(of: store.reveal) { _, _ in takeReveal() }
        .task { takeReveal() }
        .onDisappear { if selecting { setSelecting(false) } }
    }

    /// A file opened in FIZZ: its sheet.
    private func takeReveal() {
        guard let id = store.reveal, store.item(id) != nil, !snapshot else { return }
        store.reveal = nil
        if selecting { setSelecting(false) }
        sheet = KeptKey(id: id)
    }

    private func setSelecting(_ on: Bool) {
        picked = []
        store.selecting = on
    }

    private func tap(_ k: KeptPreset) {
        if selecting {
            if picked.contains(k.id) { picked.remove(k.id) } else { picked.insert(k.id) }
            return
        }
        sheet = KeptKey(id: k.id)
        hear(k, model: model) { note = $0 }
    }

    private func toggleAll() {
        let all = Set(shown.map(\.id))
        picked = picked.isSuperset(of: all) ? [] : all
    }

    private func newSound() {
        if selecting { setSelecting(false) }
        model.editTarget = .newKept
        model.goTo = .edit
    }

    private func export(_ ks: [KeptPreset]) {
        Task {
            var s: [FizzSound] = []
            for k in ks { if let f = await store.full(k) { s.append(f.sound) } }
            if !s.isEmpty { exporting = ExportBox(sounds: s) }
        }
    }

    private func askDelete() {
        let ks = store.items.filter { picked.contains($0.id) }
        guard !ks.isEmpty else { return }
        ask = ConfirmAsk(title: "DELETE · KEPT", what: ks.count == 1 ? ks[0].name : "\(ks.count) PRESETS",
                         detail: "OFF THIS PHONE", go: "DELETE", color: FZ.red) {
            store.delete(ks.map(\.id))
            picked = []
            note = .done("DELETED \(ks.count == 1 ? ks[0].name : "\(ks.count)")", FZ.red)
        }
    }
}

/// The kept presets the tags let through (any of them) and the search (LIBRARY's), by name.
func keptShown(_ items: [KeptPreset], tags: Set<FizzTag>, query: String = "") -> [KeptPreset] {
    items.filter { k in
        (tags.isEmpty || FizzTag.of(mask: k.tagMask).contains { tags.contains($0) }) && NameSearch.matches(k.name, query)
    }
}

/// FIZZ's seven tags as filters.
@MainActor @ViewBuilder func tagChips(_ tags: Binding<Set<FizzTag>>) -> some View {
    ForEach(FizzTag.allCases) { t in
        FilterChip(title: t.name, color: t.tint, on: tags.wrappedValue.contains(t), tag: t) {
            if tags.wrappedValue.contains(t) { tags.wrappedValue.remove(t) } else { tags.wrappedValue.insert(t) }
        }
    }
}

/// A kept preset tapped with TRACK on: heard on the track (KEEP / CANCEL), as IMPORT's rows are.
@MainActor func hear(_ k: KeptPreset, model: UnitModel, note: @escaping @MainActor (ScreenNote) -> Void) {
    guard model.actions.trackOn else { return }
    Task {
        let s = await model.kept.full(k)?.sound ?? k.sound
        let o = await model.actions.pickSound(s)
        if let n = ScreenNote.of(o) { note(n) }
    }
}

// MARK: - Parts

/// The phone's glyph, champagne by default.
struct PhoneGlyph: View {
    var size: CGFloat = 16
    var color: RGB = FZ.champagne
    var body: some View {
        IconView(KeptArt.phone, color: color.color, lineWidth: max(1.8, size / 8), backdrop: .clear)
            .frame(width: size, height: size * 1.15)
    }
}

/// KEPT n, SELECT / DONE, + NEW.
struct KeptHead: View {
    let count: Int
    let selecting: Bool
    var size: CGFloat = 46
    var compact = false
    let select: () -> Void
    let new: () -> Void
    @Environment(\.fizzSnapshot) private var snapshot

    var body: some View {
        HStack(alignment: .center, spacing: 10) {
            if compact {
                PhoneGlyph(size: 15)
                Text("KEPT").fz(13, 800, em: 0.12).foregroundStyle(FZ.champagne.color)
                Text("\(count)").fz(15, 800).foregroundStyle(FZ.text.color).contentTransition(.numericText())
            } else {
                HStack(alignment: .lastTextBaseline, spacing: 6) {
                    Text("KEPT").fz(size, 800, em: -0.01).foregroundStyle(FZ.champagne.color)
                        .shadow(color: FZ.champagne.opacity(0.4), radius: 12)
                    Text("\(count)").fz(22, 800).foregroundStyle(FZ.muted.color).contentTransition(.numericText())
                }
                .fixedSize()
            }
            Spacer(minLength: 4)
            if compact {
                RoundIcon(art: KeptArt.check, color: selecting ? FZ.champagne : FZ.text, size: 36, action: select)
                    .background(Circle().fill(selecting ? FZ.champagne.opacity(0.18) : .clear).frame(width: 36, height: 36))
                Button(action: new) {
                    IconView(Icons.plus, color: .black, lineWidth: 2.6, backdrop: FZ.champagne.color).frame(width: 16, height: 16)
                        .frame(width: 36, height: 36).background(Circle().fill(FZ.champagne.color))
                        .shadow(color: FZ.champagne.opacity(0.45), radius: 8)
                        .frame(width: 44, height: 44).contentShape(Circle())
                }
                .buttonStyle(PressStyle(scale: 0.9))
            } else if selecting {
                small("DONE", icon: KeptArt.check, filled: true, action: select)
            } else {
                small("SELECT", icon: nil, filled: false, action: select)
                NewButton(action: new)
            }
        }
    }

    private func small(_ t: String, icon: VectorArt?, filled: Bool, action: @escaping () -> Void) -> some View {
        Button(action: action) {
            HStack(spacing: 6) {
                if let icon { IconView(icon, color: .black, lineWidth: 2.6, backdrop: FZ.champagne.color).frame(width: 15, height: 15) }
                Text(t).fz(14, 800, em: 0.08)
            }
            .foregroundStyle(filled ? Color.black : FZ.text.color)
            .padding(.horizontal, 16).frame(height: 44)
            .background(Capsule().fill(filled ? FZ.champagne.color : .clear))
            .overlay(Capsule().strokeBorder(filled ? .clear : FZ.line.color, lineWidth: 1.5))
            .shadow(color: filled ? FZ.champagne.opacity(0.45) : .clear, radius: 10)
            .contentShape(Capsule())
            .fixedSize()
        }
        .buttonStyle(PressStyle(scale: 0.95))
    }
}

/// + NEW, champagne, breathing.
struct NewButton: View {
    let action: () -> Void
    var body: some View {
        Animated(still: 0.6) { t in
            Button(action: action) {
                HStack(spacing: 6) {
                    IconView(Icons.plus, color: .black, lineWidth: 2.6, backdrop: FZ.champagne.color).frame(width: 15, height: 15)
                    Text("NEW").fz(14, 800, em: 0.08)
                }
                .foregroundStyle(.black)
                .padding(.horizontal, 18).frame(height: 44)
                .background(Capsule().fill(FZ.champagne.color))
                .shadow(color: FZ.champagne.opacity(0.3 + 0.35 * breathe(t, 2.4)), radius: 8 + 10 * breathe(t, 2.4))
                .contentShape(Capsule())
                .fixedSize()
            }
            .buttonStyle(PressStyle(scale: 0.95))
        }
    }
}

/// Nothing kept yet: the phone, big, and + NEW.
struct KeptEmpty: View {
    let new: () -> Void
    var body: some View {
        VStack(spacing: 18) {
            Animated(still: 0) { t in
                PhoneGlyph(size: 84).shadow(color: FZ.champagne.opacity(0.2 + 0.3 * breathe(t, 3)), radius: 20)
            }
            NewButton(action: new)
        }
        .frame(maxWidth: .infinity).padding(.top, 60)
    }
}

/// One kept preset (.cas, champagne): its colour's stripe, the tick when selecting, glyph, NAME, first tag.
struct KeptRow: View {
    let item: KeptPreset
    var selecting = false
    var picked = false
    var heard = false
    var selected = false
    var compact = false
    let tap: () -> Void

    var body: some View {
        Button(action: tap) { face }
            .buttonStyle(PressStyle(scale: 0.98))
    }

    private var face: some View {
        let tint = item.tint
        let ring = picked ? FZ.champagne.color : selected ? FZ.cyan.color : FZ.line.color
        return HStack(spacing: 10) {
            if selecting {
                ZStack {
                    Circle().strokeBorder(picked ? .clear : FZ.dim.color, lineWidth: 2)
                    if picked {
                        Circle().fill(FZ.champagne.color)
                        IconView(KeptArt.check, color: .black, lineWidth: 3, backdrop: FZ.champagne.color).frame(width: 14, height: 14)
                    }
                }
                .frame(width: 28, height: 28)
                .scaleEffect(picked ? 1 : 0.92)
                .animation(.spring(response: 0.25, dampingFraction: 0.5), value: picked)
            }
            AlgoGlyph(algorithm: item.algorithm, color: tint.color).frame(width: 30, height: 30)
            Text(item.name).fz(compact ? 15 : 16, 800, em: 0.03).foregroundStyle(FZ.text.color).lineLimit(1)
            Spacer(minLength: 4)
            if let t = FizzTag.of(mask: item.tagMask).first { TagPill(tag: t) }
        }
        .padding(.leading, 14).padding(.trailing, 12)
        .frame(height: 56)
        .background(RoundedRectangle(cornerRadius: 12).fill(picked ? FZ.champagne.mix(10, FZ.raised).color : FZ.raised.color))
        .overlay(alignment: .leading) {
            RoundedRectangle(cornerRadius: 2).fill(tint.color).frame(width: 4).padding(.vertical, 9)
        }
        .overlay(RoundedRectangle(cornerRadius: 12).strokeBorder(ring, lineWidth: 1.5))
        .background(
            RoundedRectangle(cornerRadius: 12).stroke((picked ? FZ.champagne : FZ.cyan).opacity(picked || selected ? 0.3 : 0), lineWidth: 4)
                .shadow(color: (picked ? FZ.champagne : FZ.cyan).opacity(picked || selected ? 0.3 : 0), radius: 10)
        )
        .contentShape(RoundedRectangle(cornerRadius: 12))
    }
}

/// SEND's targets (KEPT's sheet and its bar).
enum KeptTarget: Hashable, CaseIterable { case mine, pool, track }

extension UnitModel {
    /// The track TRACK sends to (the one followed, else the selected one, else 1): 0..5.
    var sendTrack: Int { max(0, editor.trackShown) }
    func destination(_ t: KeptTarget) -> SendDestination {
        switch t {
        case .mine: .mine
        case .pool: .pool
        case .track: .track(sendTrack)
        }
    }
}

// MARK: - SELECT's bar (mock-up 26)

/// The violet bar while selecting: the count, ALL, SHARE (.fizzlib), DELETE; MY PRESETS / POOL / TRACK; SEND n TO
/// the unit.
struct KeptSelectBar: View {
    let model: UnitModel
    let picked: Set<String>
    let all: [String]
    @Binding var target: KeptTarget
    @Binding var note: ScreenNote?
    let toggleAll: () -> Void
    let delete: () -> Void
    let sent: () -> Void
    @Environment(\.fizzSnapshot) private var snapshot
    @State private var share: URL?
    @State private var sending = false
    @AppStorage("play.theme") private var theme = PlayTheme.tangerine.rawValue

    private var items: [KeptPreset] { model.kept.items.filter { picked.contains($0.id) } }
    private var n: Int { items.count }
    private var trackOn: Bool { model.actions.trackOn }
    private var canSend: Bool { n > 0 && !sending && model.unit != nil && model.actions.canTransfer && (target != .track || trackOn) }

    var body: some View {
        VStack(spacing: 12) {
            HStack(spacing: 8) {
                Text("\(n)").fz(40, 800).foregroundStyle(FZ.champagne.color).contentTransition(.numericText())
                    .shadow(color: FZ.champagne.opacity(0.35), radius: 10)
                Spacer(minLength: 0)
                Button(action: toggleAll) {
                    Text("ALL").fz(14, 800, em: 0.08).foregroundStyle(FZ.text.color)
                        .padding(.horizontal, 14).frame(height: 40)
                        .overlay(Capsule().strokeBorder(FZ.line.color, lineWidth: 1.5))
                        .frame(height: 44).contentShape(Capsule())
                }
                .buttonStyle(PressStyle(scale: 0.95))
                shareButton
                RoundIcon(art: Icons.trash, color: FZ.red, enabled: n > 0, size: 40, action: delete)
            }
            KeptTargetSeg(target: $target, track: model.sendTrack, trackOn: trackOn)
            SendButton(n: n, progress: sending ? model.actions.job : nil, enabled: canSend, to: model.unitName) { send() }
            // (another unit there: SEND n TO it too, into its MY PRESETS: UnitsSend.swift)
            if let o = model.other {
                IconPill(title: "SEND \(n) TO \(o.name)", icon: TransferIcons.down, color: unitTint(o.key, theme: theme),
                         redline: true, height: 50, size: 15, enabled: n > 0 && !sending && !model.sendingOther) { sendOther(o) }
            }
        }
        .padding(14)
        .background(RoundedRectangle(cornerRadius: 26).fill(FZ.violet.mix(14, RGB(0x0d0d11)).color))
        .overlay(RoundedRectangle(cornerRadius: 26).strokeBorder(FZ.violet.color, lineWidth: 1.5))
        .shadow(color: FZ.violet.opacity(0.35), radius: 18)
        .shadow(color: .black.opacity(0.8), radius: 16, y: 10)
        .task(id: picked) {
            share = nil
            guard !snapshot, n > 0 else { return }
            share = try? await model.kept.library(items, fizz: model.unit?.version ?? "")
        }
    }

    @ViewBuilder private var shareButton: some View {
        let face = IconView(Icons.share, color: FZ.text.color, lineWidth: 2.2).frame(width: 18, height: 18)
            .frame(width: 40, height: 40)
            .overlay(Circle().strokeBorder(FZ.line.color, lineWidth: 1.5))
            .frame(width: 44, height: 44).contentShape(Circle())
        if let share, !snapshot {
            ShareLink(item: share) { face }.buttonStyle(PressStyle(scale: 0.9))
        } else {
            face.opacity(n > 0 ? 1 : 0.4)
        }
    }

    private func send() {
        let go = items, dest = model.destination(target)
        sending = true
        Task {
            let out = await model.send(kept: go, to: dest)
            sending = false
            note = keptSendNote(out, to: dest, unit: model.unitName)
            if !out.isEmpty && out.allSatisfy(\.isOK) { sent() }
        }
    }

    private func sendOther(_ o: OtherUnit) {
        let go = items
        sending = true
        Task {
            let out = await model.sendToOther(kept: go)
            sending = false
            note = keptSendNote(out, to: .mine, unit: o.name)
            if !out.isEmpty && out.allSatisfy(\.isOK) { sent() }
        }
    }
}

/// MY PRESETS / POOL / TRACK n (.dest .seg, violet).
struct KeptTargetSeg: View {
    @Binding var target: KeptTarget
    let track: Int
    let trackOn: Bool

    var body: some View {
        HStack(spacing: 0) {
            ForEach(KeptTarget.allCases, id: \.self) { t in
                let on = t == target, off = t == .track && !trackOn
                Button { if !off { target = t } } label: {
                    HStack(spacing: 7) {
                        Text(t == .mine ? "MY PRESETS" : t == .pool ? "POOL" : "TRACK").fz(13, 800, em: 0.07)
                        if t == .track {
                            Text("\(track + 1)").fz(13, 800).foregroundStyle(.white)
                                .frame(width: 22, height: 22).background(Circle().fill(FZ.pink.color))
                        }
                    }
                    .foregroundStyle(on ? Color.black : FZ.muted.color)
                    .frame(maxWidth: .infinity).frame(height: 44)
                    .background(on ? FZ.violet.color : .clear)
                    .contentShape(Rectangle())
                }
                .buttonStyle(.plain)
                .opacity(off ? 0.4 : 1)
            }
        }
        .background(RGB(0x0d0d11).color)
        .clipShape(Capsule())
        .overlay(Capsule().strokeBorder(FZ.line.color, lineWidth: 1.5))
        .animation(.spring(response: 0.3, dampingFraction: 0.8), value: target)
        .onChange(of: trackOn) { _, on in if !on && target == .track { target = .mine } }
    }
}

// MARK: - A kept preset (mock-up 24; the iPad's third column)

/// The phone's sheet: KeptPanel in a FizzSheet, its confirms standalone over it, the outcome strip on top.
struct KeptSheet: View {
    let model: UnitModel
    @State var id: String?
    @Binding var note: ScreenNote?
    var onExport: ([KeptPreset]) -> Void
    let close: () -> Void
    @State private var ask: ConfirmAsk?

    var body: some View {
        Group {
            if model.kept.item(id) != nil {
                KeptPanel(model: model, id: $id, note: $note, ask: $ask, onClose: close)
            } else {
                sheetBackground.color.ignoresSafeArea()
            }
        }
        .overlay(alignment: .top) { NoteStrip(note: $note).padding(.top, 8) }
        .confirmCard($ask)
        .animation(.spring(response: 0.35, dampingFraction: 0.85), value: note)
        .onChange(of: model.kept.item(id) == nil) { _, gone in if gone { close() } }
    }
}

struct KeptPanel: View {
    let model: UnitModel
    @Binding var id: String?
    var wide = false
    @Binding var note: ScreenNote?
    @Binding var ask: ConfirmAsk?
    var onClose: () -> Void = {}

    @Environment(\.fizzSnapshot) private var snapshot
    @Environment(\.fizzShotSelection) private var shotSelection
    @State private var naming = false
    @State private var name = ""
    @FocusState private var nameFocus: Bool
    @State private var addingTag = false
    @State private var newTag = ""
    @FocusState private var tagFocus: Bool
    @State private var working = false
    /// the target tapped last (it rings)
    @State private var rung: KeptTarget?

    private var store: KeptPresets { model.kept }
    private var item: KeptPreset? { store.item(id) }

    var body: some View {
        if let k = item {
            Group {
                if wide {
                    VStack(alignment: .leading, spacing: 0) {
                        header(k)
                        tags(k).padding(.top, 18)
                        send(k).padding(.top, 22)
                        Spacer(minLength: 18)
                        IconPill(title: "EDIT", icon: Icons.pencil, color: FZ.pink, ink: .white) { edit(k) }
                        HStack(spacing: 10) {
                            IconPill(title: "", icon: KeptArt.copy, ghost: true, enabled: !working) { duplicate(k) }
                            shareButton(k, short: true)
                            IconPill(title: "", icon: Icons.trash, color: FZ.red, redline: true) { askDelete(k) }
                        }
                        .padding(.top, 10)
                    }
                } else {
                    FizzSheet {
                        header(k)
                    } content: {
                        VStack(alignment: .leading, spacing: 0) {
                            tags(k)
                            send(k).padding(.top, 20)
                        }
                    } actions: {
                        VStack(spacing: 10) {
                            HStack(spacing: 10) {
                                IconPill(title: "DUPLICATE", icon: KeptArt.copy, ghost: true, enabled: !working) { duplicate(k) }
                                IconPill(title: "EDIT", icon: Icons.pencil, color: FZ.pink, ink: .white) { edit(k) }
                            }
                            HStack(spacing: 10) {
                                shareButton(k, short: false)
                                IconPill(title: "DELETE", icon: Icons.trash, color: FZ.red, redline: true) { askDelete(k) }
                            }
                        }
                    }
                }
            }
            .onAppear { if snapshot, shotSelection == "kept-rung" { rung = .mine } }
        }
    }

    // MARK: name

    private func header(_ k: KeptPreset) -> some View {
        HStack(spacing: 14) {
            AlgoGlyph(algorithm: k.algorithm, color: k.tint.color).frame(width: 64, height: 64)
            VStack(alignment: .leading, spacing: 2) {
                HStack(spacing: 6) {
                    PhoneGlyph(size: 13)
                    Text("KEPT").fz(13, 800, em: 0.12).foregroundStyle(FZ.champagne.color)
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
                        .onSubmit { rename(k) }
                        .onChange(of: nameFocus) { _, f in if !f && naming { rename(k) } }
                } else {
                    HStack(spacing: 4) {
                        Text(k.name).fz(wide ? 36 : 40, 800, em: -0.01).foregroundStyle(FZ.text.color)
                            .lineLimit(1).minimumScaleFactor(0.5)
                        Caret(height: wide ? 34 : 38)
                    }
                    .contentShape(Rectangle())
                    .onTapGesture {
                        name = k.name
                        naming = true
                        nameFocus = true
                    }
                }
            }
        }
    }

    private func rename(_ k: KeptPreset) {
        let v = name
        naming = false
        nameFocus = false
        guard KeptPresets.clean(v) != k.name, !KeptPresets.clean(v).isEmpty else { return }
        change { try await store.rename(k, to: v) }
    }

    /// A change to the file; the panel follows the file's new name.
    private func change(_ f: @escaping @MainActor () async throws -> KeptPreset) {
        working = true
        Task {
            do { id = try await f().id } catch { note = ScreenNote(text: "NOT WRITTEN", color: FZ.red, kind: .refused) }
            working = false
        }
    }

    // MARK: tags

    private func tags(_ k: KeptPreset) -> some View {
        WrapLayout(spacing: 7) {
            ForEach(FizzTag.allCases) { t in
                let on = (k.tagMask >> t.rawValue) & 1 != 0
                Button { change { try await store.toggle(k, tag: t) } } label: {
                    Text(t.name).fz(14, 800, em: 0.05)
                        .foregroundStyle(on ? t.ink : FZ.muted.color)
                        .padding(.horizontal, 14).frame(height: 36)
                        .background(RoundedRectangle(cornerRadius: 9).fill(on ? t.tint.color : .clear))
                        .overlay(RoundedRectangle(cornerRadius: 9).strokeBorder(on ? .clear : FZ.line.color, lineWidth: 1.5))
                        .shadow(color: on ? t.tint.opacity(0.35) : .clear, radius: 6)
                }
                .buttonStyle(.plain)
                .disabled(working)
            }
            ForEach(k.webTags, id: \.self) { w in
                Button { change { try await store.removeWebTag(k, w) } } label: {
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
                    .onSubmit { addTag(k) }
                    .onChange(of: tagFocus) { _, f in if !f && addingTag { addTag(k) } }
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

    private func addTag(_ k: KeptPreset) {
        let t = newTag
        addingTag = false
        tagFocus = false
        newTag = ""
        guard !WebTagStore.clean(t).isEmpty else { return }
        change { try await store.addWebTag(k, t) }
    }

    // MARK: SEND TO the unit

    private func send(_ k: KeptPreset) -> some View {
        let trackOn = model.actions.trackOn
        let can = model.unit != nil && model.actions.canTransfer && !working
        return VStack(alignment: .leading, spacing: 10) {
            HStack(spacing: 8) {
                Text("SEND TO").fz(13, 800, em: 0.12).foregroundStyle(FZ.violet.color)
                HStack(spacing: 5) {
                    if model.unitIsEmulated { PhoneGlyph(size: 12, color: FZ.orange) }
                    Text(model.unitName).fz(13, 800, em: 0.12)
                }
                .foregroundStyle((model.unitIsEmulated ? FZ.orange : FZ.green).color)
            }
            if wide {
                target(.mine, k, can: can)
                HStack(spacing: 8) {
                    target(.pool, k, can: can)
                    target(.track, k, can: can && trackOn)
                }
            } else {
                HStack(spacing: 8) {
                    target(.mine, k, can: can).layoutPriority(1)
                    target(.pool, k, can: can)
                    target(.track, k, can: can && trackOn)
                }
            }
        }
    }

    private func target(_ t: KeptTarget, _ k: KeptPreset, can: Bool) -> some View {
        let r = rung == t
        return Button { go(t, k) } label: {
            HStack(spacing: 8) {
                switch t {
                case .mine: RoundedRectangle(cornerRadius: 2).fill(FZ.cream.color).frame(width: 10, height: 10)
                case .pool: RoundedRectangle(cornerRadius: 2).fill(FZ.green.color).frame(width: 10, height: 10)
                case .track: EmptyView()
                }
                Text(t == .mine ? "MY PRESETS" : t == .pool ? "POOL" : "TRACK").fz(14, 800, em: 0.06).lineLimit(1).fixedSize()
                if t == .track {
                    Text("\(model.sendTrack + 1)").fz(13, 800).foregroundStyle(.white)
                        .frame(width: 22, height: 22).background(Circle().fill(FZ.pink.color))
                }
            }
            .foregroundStyle(FZ.text.color)
            .padding(.horizontal, 12)
            .frame(maxWidth: .infinity).frame(height: 54)
            .background(RoundedRectangle(cornerRadius: 14).fill(FZ.violet.opacity(r ? 0.22 : 0.08)))
            .overlay(RoundedRectangle(cornerRadius: 14).strokeBorder(FZ.violet.opacity(r ? 1 : 0.7), lineWidth: r ? 2.5 : 1.5))
            .background(RoundedRectangle(cornerRadius: 18).fill(FZ.violet.opacity(r ? 0.2 : 0)).padding(-6))
            .shadow(color: FZ.violet.opacity(r ? 0.5 : 0), radius: 12)
            .contentShape(RoundedRectangle(cornerRadius: 14))
        }
        .buttonStyle(PressStyle(scale: 0.95))
        .disabled(!can)
        .opacity(can ? 1 : 0.4)
    }

    private func go(_ t: KeptTarget, _ k: KeptPreset) {
        let dest = model.destination(t)
        working = true
        withAnimation(.spring(response: 0.3, dampingFraction: 0.6)) { rung = t }
        Task {
            let out = await model.send(kept: [k], to: dest)
            working = false
            note = keptSendNote(out, to: dest, unit: model.unitName)
            try? await Task.sleep(for: .seconds(1.2))
            withAnimation(.easeOut(duration: 0.4)) { if rung == t { rung = nil } }
        }
    }

    // MARK: buttons

    @ViewBuilder private func shareButton(_ k: KeptPreset, short: Bool) -> some View {
        let face = IconPill(title: short ? "" : "SHARE", icon: Icons.share, ghost: true) {}
        if snapshot {
            face
        } else {
            ShareLink(item: k.url) {
                HStack(spacing: 9) {
                    IconView(Icons.share, color: FZ.text.color, lineWidth: 2.3).frame(width: 18, height: 18)
                    if !short { Text("SHARE").fz(16, 800, em: 0.08) }
                }
                .foregroundStyle(FZ.text.color)
                .padding(.horizontal, 14)
                .frame(maxWidth: .infinity).frame(height: 52)
                .overlay(Capsule().strokeBorder(FZ.line.color, lineWidth: 1.5))
                .contentShape(Capsule())
            }
            .buttonStyle(PressStyle(scale: 0.96))
        }
    }

    private func duplicate(_ k: KeptPreset) {
        change {
            let d = try await store.duplicate(k)
            note = .done("DUPLICATE · \(d.name)", FZ.champagne)
            return d
        }
    }

    private func edit(_ k: KeptPreset) {
        model.editTarget = .kept(k)
        model.goTo = .edit
        onClose()
    }

    private func askDelete(_ k: KeptPreset) {
        ask = ConfirmAsk(title: "DELETE · KEPT", what: k.name, detail: "OFF THIS PHONE", go: "DELETE", color: FZ.red) {
            store.delete([k.id])
            note = .done("DELETED \(k.name)", FZ.red)
            onClose()
        }
    }
}

// MARK: - LIBRARY's sources

/// The phone's KEPT, then MY PRESETS / FACTORY / ALL (the unit's): the phone lit champagne.
struct LibrarySourceSeg: View {
    @Binding var kept: Bool
    @Binding var source: LibraryFilter.Source
    var body: some View {
        HStack(spacing: 0) {
            Button { kept = true } label: {
                PhoneGlyph(size: 17, color: kept ? RGB(0x000000) : FZ.champagne)
                    .frame(width: 48, height: 38)
                    .background(kept ? FZ.champagne.color : .clear)
                    .contentShape(Rectangle())
            }
            .buttonStyle(.plain)
            ForEach([LibraryFilter.Source.mine, .factory, .all], id: \.self) { s in
                let on = !kept && s == source
                Button { kept = false; source = s } label: {
                    Text(s == .mine ? "MY PRESETS" : s == .factory ? "FACTORY" : "ALL").fz(13, 800, em: 0.06)
                        .foregroundStyle(on ? Color.black : FZ.muted.color)
                        .lineLimit(1).fixedSize()
                        .padding(.horizontal, 9).frame(maxWidth: .infinity).frame(height: 38)
                        .background(on ? FZ.cream.color : .clear)
                        .contentShape(Rectangle())
                }
                .buttonStyle(.plain)
            }
        }
        .clipShape(Capsule())
        .overlay(Capsule().strokeBorder(FZ.line.color, lineWidth: 1.5))
        .shadow(color: kept ? FZ.champagne.opacity(0.35) : .clear, radius: 10)
        .animation(.spring(response: 0.3, dampingFraction: 0.8), value: kept)
        .animation(.spring(response: 0.3, dampingFraction: 0.8), value: source)
    }
}

/// The KEPT preset sheet over LIBRARY as the phone shows it (fizzshots: ImageRenderer draws no sheets).
public struct KeptSheetShot: View {
    let model: UnitModel
    let id: String
    public init(model: UnitModel, id: String) { self.model = model; self.id = id }

    public var body: some View {
        ZStack(alignment: .bottom) {
            RootView(model: model, tab: .constant(.library), wide: false)
                .blur(radius: 2).brightness(-0.3)
            Color.black.opacity(0.45)
            KeptPanel(model: model, id: .constant(id), note: .constant(nil), ask: .constant(nil))
                .padding(.horizontal, 18).padding(.top, 30).padding(.bottom, 44)
                .background(SheetShape().fill(RGB(0x0d0d11).color).shadow(color: .black, radius: 30, y: -10))
                .overlay(SheetShape().stroke(FZ.champagne.opacity(0.35), lineWidth: 1))
                .overlay(alignment: .top) { Capsule().fill(RGB(0x3a3a44).color).frame(width: 40, height: 5).padding(.top, 10) }
        }
        .environment(\.fizzShotSelection, "kept-rung")
    }
}

/// The iPad: a row dragged between the unit's list and KEPT ("kept:<file>", or a unit preset's "mine:n" /
/// "factory:n").
struct KeptDrag: ViewModifier {
    let id: String?
    var name = ""
    var entry: FizzListEntry?
    var enabled = true
    @Environment(\.fizzSnapshot) private var snapshot

    func body(content: Content) -> some View {
        if enabled && !snapshot, let payload = id.map({ "kept:\($0)" }) ?? entry.map({ "\($0.source):\($0.slot)" }) {
            content.draggable(payload) {
                Text(entry?.name ?? name)
                    .fz(15, 800).foregroundStyle(FZ.text.color)
                    .padding(.horizontal, 16).frame(height: 44)
                    .background(Capsule().fill(FZ.raised.color))
                    .overlay(Capsule().strokeBorder(FZ.champagne.color, lineWidth: 1.5))
            }
        } else {
            content
        }
    }
}

/// A drop target (none in a snapshot: ImageRenderer draws no platform view).
struct KeptDrop: ViewModifier {
    let accept: (String) -> Bool
    @Binding var lit: Bool
    @Environment(\.fizzSnapshot) private var snapshot

    func body(content: Content) -> some View {
        if snapshot {
            content
        } else {
            content.dropDestination(for: String.self) { items, _ in items.first.map(accept) ?? false } isTargeted: { lit = $0 }
        }
    }
}
