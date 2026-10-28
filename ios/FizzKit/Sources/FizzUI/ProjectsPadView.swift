// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// iPad PROJECTS (mock-up 22): the slots across the top, the project's 64 patterns as tall tiles (one row a bank),
// the selection's buttons in a fixed row (COPY, PASTE, CLEAR, .fizzpat; ⌘C ⌘V ⌘Z), the changes waiting in one
// row with the slide to write; KEEP and the slot's moves ("⋯": LOAD, NEW, RENAME, COPY TO, MOVE TO, DELETE, a file in)
// in the grid's header; the kept projects in a column beside (LOAD INTO, share).

import SwiftUI
import FizzData
import FizzKit

struct ProjectsPadView: View {
    let model: UnitModel
    let pm: ProjectsModel
    @Binding var importing: Bool
    @Binding var sharing: SharedFile?
    @State private var renaming = false
    @State private var newName = ""

    var body: some View {
        VStack(alignment: .leading, spacing: 14) {
            SlotPills(pm: pm)
            HStack(alignment: .top, spacing: 14) {
                main
                kept.frame(width: 296)
            }
            .frame(maxHeight: .infinity, alignment: .top)
        }
        .padding(.bottom, 20)
        .alert("RENAME", isPresented: $renaming) {
            TextField("", text: $newName).capsInput()
            Button("RENAME") { Task { await pm.rename(to: newName) } }
            Button("CANCEL", role: .cancel) {}
        }
    }

    // MARK: the project's patterns

    private var main: some View {
        let s = pm.selected
        return VStack(alignment: .leading, spacing: 0) {
            HStack(alignment: .center, spacing: 10) {
                Text(s.there ? s.name : "\(s.p + 1)").fz(40, 800, em: -0.01)
                    .foregroundStyle((s.there ? FZ.cyan : FZ.dim).color)
                    .shadow(color: FZ.cyan.opacity(s.there ? 0.45 : 0), radius: 12)
                    .lineLimit(1).minimumScaleFactor(0.5)
                if let m = s.model, m.there {
                    HStack(alignment: .firstTextBaseline, spacing: 3) {
                        Text("\(m.used)").fz(22, 800).foregroundStyle(FZ.text.color)
                        Text("/ 64").fz(15, 800).foregroundStyle(FZ.muted.color)
                    }
                    .fixedSize()
                }
                if let c = pm.clip { ClipChip(clip: c).padding(.leading, 8) }
                Spacer(minLength: 0)
                if let a = pm.arm { armed(a) } else {
                    Button { Task { await pm.keep() } } label: {
                        HStack(spacing: 6) {
                            IconView(ProjIcons.keep, color: FZ.champagne.color, lineWidth: 2.3).frame(width: 17, height: 17)
                            Text("KEEP").fz(13, 800, em: 0.06)
                        }
                        .foregroundStyle(FZ.champagne.color)
                        .padding(.horizontal, 16).frame(height: 42)
                        .overlay(Capsule().strokeBorder(FZ.champagne.mix(50, FZ.line).color, lineWidth: 1.5))
                        .contentShape(Capsule())
                    }
                    .buttonStyle(.plain)
                    .disabled(!s.there || pm.view == nil && !pm.online)
                    .opacity(s.there ? 1 : 0.35)
                    moreMenu
                }
            }
            .frame(height: 52)
            .padding(.bottom, 12)
            .animation(.spring(response: 0.45, dampingFraction: 0.75), value: pm.clip)
            if let pr = s.model, pr.there {
                PatternGrid(model: pm, project: pr, cols: 16, tileH: 110, big: true, gap: 4, bankGap: 8, label: 20)
                    .opacity(pm.reading ? 0.3 : 1)
                    .overlay { if pm.reading { ReadingMeter(fraction: pm.readFraction) } }
                tools.padding(.top, 12)
            } else {
                RoundedRectangle(cornerRadius: 16).strokeBorder(FZ.dim.color, style: StrokeStyle(lineWidth: 1.5, dash: [6, 5]))
                    .frame(height: 4 * 110 + 3 * 8)
                    .overlay {
                        if pm.reading { ReadingMeter(fraction: pm.readFraction) }
                        else if s.there {
                            ProjButton(title: "READ", icon: Icons.down, color: FZ.cyan, height: 52, size: 16, enabled: pm.online) {
                                Task { await pm.read(force: true) }
                            }
                            .frame(width: 160)
                        } else {
                            ProjButton(title: "NEW", icon: Icons.plus, style: .ghost, height: 52, size: 16, enabled: pm.online) {
                                Task { await pm.newProject() }
                            }
                            .frame(width: 160)
                        }
                    }
            }
            if !pm.ops.isEmpty {
                PendingBar(model: pm, inline: true).padding(.top, 12)
                    .transition(.move(edge: .bottom).combined(with: .opacity))
            }
            Spacer(minLength: 0)
        }
        .padding(.top, 16).padding(.horizontal, 18).padding(.bottom, 18)
        .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .top)
        .background(RoundedRectangle(cornerRadius: 22).fill(FZ.panel.color))
        .overlay(RoundedRectangle(cornerRadius: 22).strokeBorder(FZ.line.color, lineWidth: 1))
        .animation(.spring(response: 0.4, dampingFraction: 0.82), value: pm.ops.isEmpty)
    }

    /// The selection's buttons (.ptools): its name, COPY, PASTE, CLEAR, .fizzpat.
    private var tools: some View {
        let has = !pm.pats.isEmpty && pm.canEdit
        return HStack(spacing: 8) {
            if has {
                Text(FizzClip.range(Array(pm.pats))).fz(26, 800, em: -0.01).foregroundStyle(FZ.cyan.color).lineLimit(1).fixedSize()
                    .padding(.trailing, 4)
            }
            ProjButton(title: "COPY", icon: ProjIcons.copy, color: FZ.cyan, enabled: has) { Task { await pm.copy() } }
                .fixedSize()
                .keyboardShortcut("c", modifiers: .command)
            ProjButton(title: "PASTE", icon: ProjIcons.paste, color: FZ.yellow, style: pm.clip != nil ? .fill : .ghost,
                       enabled: has && pm.clip != nil) { pm.paste() }
                .fixedSize()
                .keyboardShortcut("v", modifiers: .command)
            ProjButton(title: "CLEAR", icon: ProjIcons.clear, style: .ghost, enabled: has) { pm.clear() }
                .fixedSize()
                .keyboardShortcut(.delete, modifiers: [])
            Spacer(minLength: 0)
            ProjButton(title: ".fizzpat", icon: Icons.share, style: .ghost, enabled: has) {
                Task { if let u = await pm.fizzpatFile() { sharing = SharedFile(url: u) } }
            }
            .fixedSize()
        }
        .frame(height: 42)
    }

    private func armed(_ a: ProjectsArm) -> some View {
        HStack(spacing: 8) {
            if a.kind == .load {
                Text("LOAD INTO").fz(13, 800, em: 0.08).foregroundStyle(FZ.champagne.color)
            } else {
                Segmented(items: ["COPY", "MOVE"], selected: Binding(get: { a.kind == .move ? 1 : 0 }, set: { pm.setArmMove($0 == 1) }),
                          color: FZ.champagne)
            }
            Text(a.name).fz(15, 800).foregroundStyle(FZ.text.color).lineLimit(1)
            ProjButton(title: "CANCEL", icon: ProjIcons.clear, style: .ghost) { pm.disarm() }.fixedSize()
                .keyboardShortcut(.escape, modifiers: [])
        }
    }

    /// "⋯": the slot's own moves on the FM-1 (done at once) and a file in.
    @ViewBuilder private var moreMenu: some View {
        let s = pm.selected, idle = pm.online && pm.ops.isEmpty
        let ring = IconView(ProjIcons.more, color: FZ.text.color, lineWidth: 2.6).frame(width: 20, height: 20)
            .frame(width: 42, height: 42).overlay(Circle().strokeBorder(FZ.line.color, lineWidth: 1.5)).contentShape(Circle())
        if snapshot { ring } else {
            Menu {
                Button("LOAD") { Task { await pm.load() } }.disabled(!(idle && s.there && !s.current))
                Button("NEW") { Task { await pm.newProject() } }.disabled(!(idle && !s.there))
                Button("RENAME") { newName = s.name; renaming = true }.disabled(!(idle && s.there))
                Button("COPY TO") { pm.startCopy() }.disabled(!(idle && s.there))
                Button("MOVE TO") { pm.startCopy(move: true) }.disabled(!(idle && s.there))
                Button("DELETE", role: .destructive) { pm.delete() }.disabled(!(idle && s.there))
                Divider()
                Button(".fizzpat  ·  .fizzprj") { importing = true }
            } label: { ring }
            .menuStyle(.button)
            .buttonStyle(.plain)
        }
    }

    @Environment(\.fizzSnapshot) private var snapshot

    // MARK: kept

    private var kept: some View {
        let sel = pm.kept.items.first { $0.id == pm.keptSel }
        return VStack(alignment: .leading, spacing: 0) {
            HStack(spacing: 10) {
                Text("KEPT").fz(13, 800, em: 0.12).foregroundStyle(FZ.champagne.color)
                Text("\(pm.kept.items.count)").fz(15, 800).foregroundStyle(FZ.text.color)
                Spacer(minLength: 0)
                Text(sizeText(pm.kept.totalSize)).fz(13, 800).foregroundStyle(FZ.muted.color)
                ProjRoundIcon(icon: Icons.open, color: FZ.champagne, ring: FZ.champagne.mix(45, FZ.line), size: 32) { importing = true }
            }
            .padding(.bottom, 12)
            ScreenScroll(bottom: 12) {
                VStack(spacing: 7) {
                    ForEach(pm.kept.items) { k in
                        KeptCard(model: pm, item: k, open: pm.keptSel == k.id, compact: true)
                        if pm.keptSel == k.id {
                            KeptDots(model: pm, item: k).padding(.horizontal, 6).padding(.bottom, 4)
                        }
                    }
                }
            }
            HStack(spacing: 8) {
                ProjButton(title: pm.arm?.kind == .load ? "LOAD INTO …" : "LOAD INTO", icon: ProjIcons.play, color: FZ.champagne,
                           height: 48, size: 14, enabled: sel != nil && pm.view != nil && pm.online, iconFill: true) {
                    if pm.arm?.kind == .load { pm.disarm() } else if let k = sel { pm.startLoad(k) }
                }
                if let k = sel {
                    ShareIcon(url: k.url).scaleEffect(1.2).frame(width: 48, height: 48)
                    ProjRoundIcon(icon: Icons.trash, color: FZ.red, ring: FZ.red.mix(55, FZ.bg), size: 48) { pm.deleteKept(k) }
                }
            }
            .padding(.top, 10)
        }
        .padding(.vertical, 16).padding(.horizontal, 14)
        .frame(maxHeight: .infinity, alignment: .top)
        .background(RoundedRectangle(cornerRadius: 22).fill(FZ.panel.color))
        .overlay(RoundedRectangle(cornerRadius: 22).strokeBorder(FZ.line.color, lineWidth: 1))
    }
}

/// The slots across the top (.spills): number, name, the open one cyan, a yellow dot for changes; armed: targets.
struct SlotPills: View {
    let pm: ProjectsModel
    var body: some View {
        HStack(spacing: 7) {
            ForEach(pm.slots) { s in
                let on = s.p == pm.sel && pm.arm == nil, target = pm.arm != nil && s.p != pm.arm?.from
                let badge = s.current ? FZ.cyan : s.demo ? FZ.champagne : s.there ? FZ.green : FZ.track
                Button { Task { await pm.tapSlot(s.p) } } label: {
                    HStack(spacing: 9) {
                        Text("\(s.p + 1)").fz(14, 800).foregroundStyle(s.there ? Color.black : RGB(0x5d5d66).color)
                            .frame(width: 30, height: 30).background(Circle().fill(badge.color))
                        if s.there { Text(s.name).fz(14, 800, em: 0.02).foregroundStyle(FZ.text.color).lineLimit(1) }
                    }
                    .padding(.leading, 6).padding(.trailing, s.there ? 15 : 6).frame(height: 42)
                    .background(Capsule().fill(on ? FZ.cyan.mix(16, FZ.raised).color : s.there ? FZ.raised.color : .clear))
                    .overlay {
                        if target {
                            Capsule().strokeBorder(FZ.champagne.opacity(0.75), style: StrokeStyle(lineWidth: 2, dash: [5, 4]))
                        } else if s.there {
                            Capsule().strokeBorder(on ? FZ.cyan.color : FZ.line.color, lineWidth: on ? 2 : 1.5)
                        } else {
                            Capsule().strokeBorder(on ? FZ.text.color : FZ.dim.color, style: StrokeStyle(lineWidth: 1.5, dash: [4, 3]))
                        }
                    }
                    .overlay(alignment: .topTrailing) { if s.changed { ChangeDot().offset(x: 2, y: -3) } }
                    .shadow(color: on ? FZ.cyan.opacity(0.45) : .clear, radius: 10)
                    .contentShape(Capsule())
                    .fixedSize()
                }
                .buttonStyle(.plain)
            }
        }
        .frame(height: 44)
    }
}
