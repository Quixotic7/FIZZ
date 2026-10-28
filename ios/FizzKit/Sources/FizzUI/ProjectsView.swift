// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// PROJECTS (cyan; site/manager/projtab.js). iPhone: the slots (mock-up 14: the open project glowing, its patterns as
// dots on a card that opens them, LOAD / NEW / COPY TO / DELETE; the name or its pencil: RENAME; COPY TO turns MOVE
// TO with one tap; KEPT n opens the kept ones), a project's patterns (18, ProjectsPatternsView) and the kept ones (19,
// ProjectsKeptView), each with a chevron back to the slots. iPad: all of it on one screen (22,
// ProjectsPadView). The store is read when the tab shows and the unit changed it (projtab.js show).

import SwiftUI
import FizzData
import FizzKit

struct ProjectsView: View {
    let model: UnitModel
    var wide = false
    @State private var importing = false
    @State private var sharing: SharedFile?

    var body: some View {
        let pm = model.projectsTab
        Group {
            if wide {
                ProjectsPadView(model: model, pm: pm, importing: $importing, sharing: $sharing)
            } else {
                switch pm.mode {
                case .slots: ProjectsSlotsView(model: model, pm: pm)
                case .patterns: ProjectsPatternsView(model: model, pm: pm, importing: $importing, sharing: $sharing)
                case .kept: ProjectsKeptView(model: model, pm: pm, importing: $importing)
                }
            }
        }
        .animation(.spring(response: 0.4, dampingFraction: 0.86), value: pm.mode)
        .overlay(alignment: .top) {
            if let n = pm.note { ProjNoteStrip(note: n).padding(.top, 4).onTapGesture { pm.dismissNote() } }
        }
        .animation(.spring(response: 0.35, dampingFraction: 0.8), value: pm.note)
        .modifier(ProjectsAskAlert(model: pm))
        .fileImporter(isPresented: $importing, allowedContentTypes: [ProjectFileTypes.project, ProjectFileTypes.patterns],
                      allowsMultipleSelection: false) { r in
            if case .success(let urls) = r, let u = urls.first { Task { await pm.open(u) } }
        }
        .shareSheet($sharing)
        .task {
            await pm.show()
            await ProjectsScene.apply(pm)
        }
    }
}

/// `-projects <scene>` with the demo unit: PROJECTS opened in that state (simulator screenshots): patterns, select
/// (C02-C03), top (A10), changes (SUNDAY A03-A05 pasted at NOIR C01), write (those changes written), kept, load (LOAD
/// INTO armed), copyto.
@MainActor enum ProjectsScene {
    private static var done = false

    static func apply(_ pm: ProjectsModel, arguments: [String] = ProcessInfo.processInfo.arguments) async {
        guard !done, pm.unit?.source is DemoLinkSource, let i = arguments.firstIndex(of: "-projects"), i + 1 < arguments.count
        else { return }
        done = true
        switch arguments[i + 1] {
        case "patterns": await pm.enter(.patterns)
        case "select": pm.selectRange(from: 33, to: 34); await pm.enter(.patterns)
        case "top": pm.tap(9); await pm.enter(.patterns)
        case "changes", "write":
            pm.select(1); pm.selectRange(from: 2, to: 4); await pm.copy()
            pm.select(0); pm.tap(32); pm.paste()
            if let a = pm.ask { pm.ask = nil; await a.run() }
            pm.selectRange(from: 33, to: 34)
            await pm.enter(.patterns)
            if arguments[i + 1] == "write" { await pm.write() }
        case "kept", "load":
            if pm.kept.items.count < 4 { for p in [6, 0, 5, 3] { pm.select(p); await pm.keep() } }
            pm.select(0)
            await pm.enter(.kept)
            if arguments[i + 1] == "load", let k = pm.kept.items.first { pm.keptSel = k.id; pm.startLoad(k) }
        case "copyto": pm.startCopy()
        default: break
        }
        pm.dismissNote()
    }
}

// MARK: - iPhone: the slots (mock-up 14)

struct ProjectsSlotsView: View {
    let model: UnitModel
    let pm: ProjectsModel
    @State private var renaming = false
    @State private var newName = ""

    var body: some View {
        let s = pm.selected
        ScreenScroll(bottom: pm.ops.isEmpty ? 120 : 270, refresh: { await model.refresh(); if pm.ops.isEmpty { await pm.read(force: true) } }) {
            VStack(alignment: .leading, spacing: 0) {
                HStack(alignment: .center, spacing: 10) {
                    // the name tapped (or its pencil): RENAME
                    Button { newName = s.name; renaming = true } label: {
                        HStack(alignment: .center, spacing: 10) {
                            Text(s.there ? s.name : "\(s.p + 1)").fz(52, 800, em: -0.01)
                                .foregroundStyle((s.there ? FZ.cyan : FZ.dim).color)
                                .shadow(color: FZ.cyan.opacity(s.there ? 0.45 : 0), radius: 13)
                                .lineLimit(1).minimumScaleFactor(0.5)
                            if s.there && pm.online && pm.arm == nil {
                                IconView(ProjIcons.pencil, color: FZ.cyan.color, lineWidth: 2.3, backdrop: .black).frame(width: 16, height: 16)
                                    .frame(width: 34, height: 34)
                                    .overlay(Circle().strokeBorder(FZ.cyan.mix(50, FZ.line).color, lineWidth: 1.5))
                                    .transition(.scale.combined(with: .opacity))
                            }
                        }
                        .frame(minHeight: 60)
                        .contentShape(Rectangle())
                    }
                    .buttonStyle(ProjPress())
                    .disabled(!(s.there && pm.online && pm.arm == nil))
                    Spacer(minLength: 0)
                    KeptChip(count: pm.kept.items.count, height: 40, label: true, hitPad: 2) { Task { await pm.enter(.kept) } }
                }
                .padding(.bottom, 10)
                SlotTiles(pm: pm).frame(maxWidth: 560)
                // the dots: a card that opens the patterns
                Button { Task { await pm.enter(.patterns) } } label: {
                    VStack(alignment: .leading, spacing: 8) {
                        PatternsHeading(pm: pm, chevron: true)
                        PatternDots(project: s.model, playing: pm.playing && s.current ? model.status?.pattern : nil)
                            .opacity(pm.reading ? 0.25 : 1)
                            .overlay { if pm.reading { ReadingMeter(fraction: pm.readFraction) } }
                    }
                    .padding(.top, 10).padding(.bottom, 8).padding(.leading, 12).padding(.trailing, 8)
                    .background(RoundedRectangle(cornerRadius: 18).fill(RGB(0x0c0c10).color))
                    .overlay(RoundedRectangle(cornerRadius: 18).strokeBorder(FZ.cyan.mix(28, FZ.line).color, lineWidth: 1.5))
                    .contentShape(RoundedRectangle(cornerRadius: 18))
                }
                .buttonStyle(ProjPress(scale: 0.97))
                .frame(maxWidth: 560)
                .padding(.top, 14)
                SlotActions(pm: pm).padding(.top, 14).frame(maxWidth: 560)
            }
            // (inside the scroll: the open slot's glow not cut at the edges)
            .padding(.horizontal, 16).padding(.top, 4)
        }
        .animation(.spring(response: 0.35, dampingFraction: 0.8), value: s.there && pm.online && pm.arm == nil)
        .alert("RENAME", isPresented: $renaming) {
            TextField("", text: $newName).capsInput()
            Button("RENAME") { Task { await pm.rename(to: newName) } }
            Button("CANCEL", role: .cancel) {}
        }
    }
}

/// PATTERNS n / 64 and the open pattern (▶ while playing).
struct PatternsHeading: View {
    let pm: ProjectsModel
    /// iPhone's slots: a chevron at the end (the card opens the patterns)
    var chevron = false
    var body: some View {
        let s = pm.selected
        let pat = s.current ? pm.unit?.status?.pattern : nil
        HStack(spacing: 10) {
            Text("PATTERNS").fz(13, 800, em: 0.12).foregroundStyle(FZ.muted.color)
            if s.model != nil, s.there { Text("\(s.used) / 64").fz(15, 800).foregroundStyle(FZ.text.color) }
            Spacer(minLength: 0)
            if let pat, (0..<64).contains(pat) {
                Text("\(pm.playing ? "▶ " : "")\(FizzClip.patName(pat))").fz(15, 800, em: 0.04)
                    .foregroundStyle((pm.playing ? FZ.cyan : FZ.muted).color)
            }
            if chevron {
                IconView(ProjIcons.next, color: .black, lineWidth: 2.8, backdrop: FZ.cyan.color).frame(width: 15, height: 15)
                    .frame(width: 30, height: 30)
                    .background(Circle().fill(FZ.cyan.color))
                    .shadow(color: FZ.cyan.opacity(0.4), radius: 6)
            }
        }
        .lineLimit(1)
    }
}

/// The store being read: the % over the dots, cyan.
struct ReadingMeter: View {
    let fraction: Double
    var body: some View {
        VStack(spacing: 8) {
            HStack(alignment: .firstTextBaseline, spacing: 0) {
                Text("\(Int((min(1, max(0, fraction)) * 100).rounded(.down)))").fz(48, 800).monospacedDigit()
                Text("%").fz(24, 800)
            }
            .foregroundStyle(FZ.cyan.color)
            .shadow(color: FZ.cyan.opacity(0.4), radius: 12)
            GeometryReader { g in
                Capsule().fill(FZ.track.color)
                    .overlay(alignment: .leading) { Capsule().fill(FZ.cyan.color).frame(width: max(6, g.size.width * fraction)) }
            }
            .frame(width: 180, height: 6)
        }
    }
}

/// The 8 slots (4 on the Bluetooth package) as tiles: the open one glowing, the selected ringed, targets of a move
/// dashed champagne, the demo marked, empty ones dashed with a "+".
struct SlotTiles: View {
    let pm: ProjectsModel
    var body: some View {
        let n = pm.nproj, rows = (n + 3) / 4
        Grid(horizontalSpacing: 8, verticalSpacing: 8) {
            ForEach(0..<rows, id: \.self) { r in
                GridRow {
                    ForEach(0..<4, id: \.self) { c in
                        let k = r * 4 + c
                        if k < n {
                            Button { Task { await pm.tapSlot(k) } } label: {
                                ProjectTile(slot: pm.slot(k), selected: k == pm.sel && pm.arm == nil,
                                            target: pm.arm != nil && k != pm.arm?.from, from: pm.arm?.from == k)
                            }
                            .buttonStyle(ProjPress())
                        } else {
                            Color.clear.frame(height: 94)
                        }
                    }
                }
            }
        }
    }
}

struct ProjectTile: View {
    let slot: ProjectSlot
    var selected = false
    var target = false
    var from = false

    var body: some View {
        let there = slot.there, cur = slot.current, demo = slot.demo
        let badge = cur ? FZ.cyan : demo ? FZ.champagne : there ? FZ.green : FZ.track
        AnimatedIf(on: cur || target, still: 0.9) { t in
            VStack(alignment: .leading, spacing: 0) {
                Text("\(slot.p + 1)").fz(15, 800).foregroundStyle(there ? Color.black : RGB(0x5d5d66).color)
                    .frame(width: 26, height: 26)
                    .background(RoundedRectangle(cornerRadius: 8).fill(badge.color))
                if there {
                    Spacer(minLength: 0)
                    Text(slot.name).fz(14, 800, em: 0.01).foregroundStyle(FZ.text.color).lineLimit(2)
                        .fixedSize(horizontal: false, vertical: true)
                    if demo {
                        Text("DEMO").fz(12, 800, em: 0.06).foregroundStyle(FZ.champagne.color)
                            .padding(.horizontal, 5).frame(height: 16)
                            .overlay(RoundedRectangle(cornerRadius: 6).strokeBorder(FZ.champagne.opacity(0.7),
                                                                                     style: StrokeStyle(lineWidth: 1.5, dash: [3, 2])))
                            .padding(.top, 5)
                    } else {
                        GeometryReader { g in
                            Capsule().fill(FZ.track.color)
                                .overlay(alignment: .leading) {
                                    Capsule().fill((cur ? FZ.cyan : FZ.green).color)
                                        .frame(width: max(5, g.size.width * min(1, Double(slot.used) / 64)))
                                }
                        }
                        .frame(height: 5).padding(.top, 7)
                    }
                } else {
                    Text("+").fz(30, 500).foregroundStyle((target ? FZ.champagne : RGB(0x3a3a44)).color)
                        .frame(maxWidth: .infinity, maxHeight: .infinity)
                }
            }
            .padding(.top, 9).padding(.bottom, 9).padding(.leading, 10).padding(.trailing, 9)
            .frame(maxWidth: .infinity, alignment: .leading)
            .frame(height: 94)
            .background(RoundedRectangle(cornerRadius: 18).fill(cur ? FZ.cyan.mix(14, FZ.raised).color : there ? FZ.raised.color : .clear))
            .overlay {
                if target {
                    RoundedRectangle(cornerRadius: 18).strokeBorder(FZ.champagne.opacity(0.5 + 0.5 * breathe(t, 1.4)),
                                                                    style: StrokeStyle(lineWidth: 2, dash: [6, 4]))
                } else if there {
                    RoundedRectangle(cornerRadius: 18).strokeBorder(cur ? FZ.cyan.color : selected ? FZ.text.color : FZ.line.color,
                                                                    lineWidth: cur || selected ? 2 : 1.5)
                } else {
                    RoundedRectangle(cornerRadius: 18).strokeBorder(selected ? FZ.text.color : FZ.dim.color,
                                                                    style: StrokeStyle(lineWidth: selected ? 2 : 1.5, dash: [5, 4]))
                }
            }
            .overlay(alignment: .topTrailing) { if slot.changed { ChangeDot().offset(x: 3, y: -3) } }
            .shadow(color: cur ? FZ.cyan.opacity(0.45 + 0.3 * breathe(t, 2.2)) : .clear, radius: cur ? 13 + 7 * breathe(t, 2.2) : 0)
            .opacity(from ? 0.45 : 1)
            .contentShape(RoundedRectangle(cornerRadius: 18))
        }
    }
}

/// LOAD, NEW, COPY TO, DELETE (.pjacts); armed: COPY | MOVE and CANCEL.
struct SlotActions: View {
    let pm: ProjectsModel
    var body: some View {
        let s = pm.selected, idle = pm.online && pm.ops.isEmpty && pm.arm == nil
        if let a = pm.arm {
            HStack(spacing: 8) {
                if a.kind == .load {
                    HStack(spacing: 8) {
                        IconView(ProjIcons.play, color: FZ.champagne.color, fill: true).frame(width: 13, height: 13)
                        Text(a.name).fz(16, 800, em: 0.04).foregroundStyle(FZ.champagne.color).lineLimit(1)
                    }
                    .frame(maxWidth: .infinity)
                } else {
                    Segmented(items: ["COPY", "MOVE"], selected: Binding(get: { a.kind == .move ? 1 : 0 }, set: { pm.setArmMove($0 == 1) }),
                              color: FZ.champagne)
                    Text(a.name).fz(16, 800, em: 0.04).foregroundStyle(FZ.champagne.color).lineLimit(1).minimumScaleFactor(0.6)
                        .frame(maxWidth: .infinity)
                }
                ProjButton(title: "CANCEL", icon: ProjIcons.clear, style: .ghost, height: 48) { pm.disarm() }.frame(width: 120)
            }
        } else {
            GeometryReader { g in
                // (.pjacts: 1.1fr 1fr 1.25fr 1.15fr)
                let u = (g.size.width - 21) / 4.5
                HStack(spacing: 7) {
                    ProjButton(title: "LOAD", icon: ProjIcons.play, color: FZ.cyan, height: 48, padding: 6,
                               enabled: idle && s.there && !s.current, iconFill: true) { Task { await pm.load() } }
                        .frame(width: u * 1.1)
                    ProjButton(title: "NEW", icon: Icons.plus, style: .ghost, height: 48, padding: 6, enabled: idle && !s.there) {
                        Task { await pm.newProject() }
                    }
                    .frame(width: u)
                    ProjButton(title: "COPY TO", icon: ProjIcons.copy, style: .ghost, height: 48, padding: 6, enabled: idle && s.there) {
                        pm.startCopy()
                    }
                    .frame(width: u * 1.25)
                    ProjButton(title: "DELETE", icon: Icons.trash, style: .red, height: 48, padding: 6, enabled: idle && s.there) {
                        pm.delete()
                    }
                    .frame(width: u * 1.15)
                }
            }
            .frame(height: 48)
        }
    }
}

extension View {
    /// Capitals as FIZZ's names are (iOS's keyboard; nothing on the Mac).
    @ViewBuilder func capsInput() -> some View {
        #if os(iOS)
        textInputAutocapitalization(.characters).autocorrectionDisabled()
        #else
        self
        #endif
    }
}
