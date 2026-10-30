// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// iPad PROJECTS (mock-up 22): the slots across the top, the project's 64 patterns as tall tiles (one row a bank),
// the selection's buttons in a fixed row (COPY, PASTE, CLEAR, .fizzpat; ⌘C ⌘V ⌘Z), the changes waiting in one
// row with the slide to write; KEEP and the slot's moves ("⋯": LOAD, NEW, RENAME, COPY TO, MOVE TO, DELETE, a file in)
// in the grid's header; the kept projects in a column beside (LOAD INTO, share). Landscape (mock-up 48): the project
// beside the open one in that column instead (its pill ringed green; a pill tapped opens it and the one open before goes
// beside; the KEPT chip at the strip's end shows the kept ones there), the changes in a row under both. A pattern dragged
// from either onto the other (a drag lifts it, ⌥ held moves it), or onto a pill (held: it opens beside).

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
    /// the screen's width: narrow (portrait) puts the kept projects under the patterns
    @State private var width: CGFloat = 1134
    /// the panels' height (two projects side by side: the tiles fit it)
    @State private var panelH: CGFloat = 640

    /// Two projects side by side (landscape, another project there, KEPT not chosen).
    private var side: Bool { width >= 900 && !pm.padKept && pm.besideSlot != nil }
    /// The tiles' height side by side: the grid (8 a row) and the panel's head and tools fit the panel.
    private var sideTileH: CGFloat { min(60, max(30, (panelH - 152 - 56) / 8)) }

    var body: some View {
        VStack(alignment: .leading, spacing: 14) {
            HStack(spacing: 8) {
                SlotPills(pm: pm, beside: side ? pm.besideSlot : nil)
                if width >= 900 {
                    KeptChip(count: pm.kept.items.count, on: pm.padKept, height: 42, label: true) { pm.padKept.toggle() }
                }
            }
            if width >= 900 {
                HStack(alignment: .top, spacing: 14) {
                    main
                    if side, let q = pm.besideSlot { besidePanel(q) } else { kept.frame(width: 296) }
                }
                .frame(maxHeight: .infinity, alignment: .top)
                .onGeometryChange(for: CGFloat.self) { $0.size.height } action: { panelH = $0 }
                if side && !pm.ops.isEmpty {
                    PendingBar(model: pm, inline: true)
                        .transition(.move(edge: .bottom).combined(with: .opacity))
                }
            } else {
                VStack(alignment: .leading, spacing: 14) {
                    main.fixedSize(horizontal: false, vertical: true)
                    kept.frame(maxHeight: .infinity)
                }
                .frame(maxHeight: .infinity, alignment: .top)
            }
        }
        .onGeometryChange(for: CGFloat.self) { $0.size.width } action: { width = $0; pm.zones.spring = $0 >= 900 ? .beside : .open }
        .animation(.spring(response: 0.4, dampingFraction: 0.82), value: side)
        .animation(.spring(response: 0.4, dampingFraction: 0.82), value: pm.ops.isEmpty)
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
                    .buttonStyle(ProjPress())
                    .disabled(!s.there || pm.view == nil && !pm.online)
                    .opacity(s.there ? 1 : 0.35)
                    moreMenu
                }
            }
            .frame(height: 52)
            .padding(.bottom, 12)
            .animation(.spring(response: 0.45, dampingFraction: 0.75), value: pm.clip)
            if let pr = s.model, pr.there {
                Group {
                    if side {
                        PatternGrid(model: pm, project: pr, tileH: sideTileH, liftDelay: nil)
                    } else {
                        PatternGrid(model: pm, project: pr, cols: 16, tileH: 110, big: true, gap: 4, bankGap: 8, label: 20, liftDelay: nil)
                    }
                }
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
            if !pm.ops.isEmpty && !side {
                PendingBar(model: pm, inline: width >= 900).padding(.top, 12)
                    .transition(.move(edge: .bottom).combined(with: .opacity))
            }
            Spacer(minLength: 0)
        }
        .padding(.top, 16).padding(.horizontal, 18).padding(.bottom, 18)
        .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .top)
        .background(RoundedRectangle(cornerRadius: 22).fill(FZ.panel.color))
        .overlay(RoundedRectangle(cornerRadius: 22).strokeBorder(dropOver(pm.sel) ? FZ.cyan.color : FZ.line.color, lineWidth: dropOver(pm.sel) ? 1.5 : 1))
        .shadow(color: dropOver(pm.sel) ? FZ.cyan.opacity(0.3) : .clear, radius: 14)
        .animation(.spring(response: 0.4, dampingFraction: 0.82), value: pm.ops.isEmpty)
    }

    /// A pattern held over slot q's grid (from the other project).
    private func dropOver(_ q: Int) -> Bool { pm.lift.map { $0.p != q && $0.over?.q == q && !$0.overChip } ?? false }

    // MARK: the project beside (landscape)

    /// The project beside the open one (green): its patterns as drop targets (and lifted from), "+n POOL" while a pattern
    /// is held over it; a tile tapped opens it (the open one goes beside).
    private func besidePanel(_ q: Int) -> some View {
        let s = pm.slot(q), over = pm.lift.map { $0.over?.q == q && !$0.overChip } ?? false
        return VStack(alignment: .leading, spacing: 0) {
            HStack(alignment: .center, spacing: 10) {
                Button { pm.swapBeside() } label: {
                    Text(s.name).fz(40, 800, em: -0.01)
                        .foregroundStyle(FZ.green.color)
                        .shadow(color: FZ.green.opacity(0.4), radius: 12)
                        .lineLimit(1).minimumScaleFactor(0.5)
                }
                .buttonStyle(ProjPress())
                if let m = s.model, m.there {
                    HStack(alignment: .firstTextBaseline, spacing: 3) {
                        Text("\(m.used)").fz(22, 800).foregroundStyle(FZ.text.color).contentTransition(.numericText())
                        Text("/ 64").fz(15, 800).foregroundStyle(FZ.muted.color)
                    }
                    .fixedSize()
                }
                Spacer(minLength: 0)
                DropPoolPill(pm: pm, q: q)
            }
            .frame(height: 52)
            .padding(.bottom, 12)
            .animation(.spring(response: 0.35, dampingFraction: 0.75), value: pm.lift == nil)
            if let pr = s.model, pr.there {
                PatternGrid(model: pm, project: pr, tileH: sideTileH, place: "beside", liftDelay: nil) { k in
                    pm.swapBeside()
                    pm.tap(k)
                }
                .opacity(pm.reading ? 0.3 : 1)
            } else {
                RoundedRectangle(cornerRadius: 16).strokeBorder(FZ.dim.color, style: StrokeStyle(lineWidth: 1.5, dash: [6, 5]))
                    .frame(height: 8 * sideTileH + 56)
            }
            Spacer(minLength: 0)
        }
        .padding(.top, 16).padding(.horizontal, 18).padding(.bottom, 18)
        .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .top)
        .background(RoundedRectangle(cornerRadius: 22).fill(over ? FZ.cyan.mix(5, FZ.panel).color : FZ.panel.color))
        .overlay(RoundedRectangle(cornerRadius: 22).strokeBorder(over ? FZ.cyan.color : FZ.green.mix(30, FZ.line).color, lineWidth: over ? 1.5 : 1))
        .shadow(color: over ? FZ.cyan.opacity(0.3) : .clear, radius: 14)
        .animation(.easeOut(duration: 0.2), value: over)
    }

    /// The selection's buttons (.ptools): its name, COPY, PASTE, CLEAR, TO…, .fizzpat. Side by side (half the width): icons
    /// only; portrait: .fizzpat as its icon.
    private var tools: some View {
        let has = !pm.pats.isEmpty && pm.canEdit, icons = side, narrow = width < 900
        let t = { (title: String) in icons ? "" : title }, pad: CGFloat = icons ? 12 : 14
        return HStack(spacing: 8) {
            if has {
                Text(FizzClip.range(Array(pm.pats))).fz(icons ? 22 : 26, 800, em: -0.01).foregroundStyle(FZ.cyan.color).lineLimit(1).fixedSize()
                    .padding(.trailing, 4)
            }
            ProjButton(title: t("COPY"), icon: ProjIcons.copy, color: FZ.cyan, padding: pad, enabled: has) { Task { await pm.copy() } }
                .fixedSize()
                .keyboardShortcut("c", modifiers: .command)
            ProjButton(title: t("PASTE"), icon: ProjIcons.paste, color: FZ.yellow, style: pm.clip != nil ? .fill : .ghost, padding: pad,
                       enabled: has && pm.clip != nil) { pm.paste() }
                .fixedSize()
                .keyboardShortcut("v", modifiers: .command)
            ProjButton(title: t("CLEAR"), icon: ProjIcons.clear, style: .ghost, padding: pad, enabled: has) { pm.clear() }
                .fixedSize()
                .keyboardShortcut(.delete, modifiers: [])
            ProjButton(title: t("TO…"), icon: ProjIcons.move, style: .ghost, padding: pad, enabled: has && pm.canTo) {
                Task { await pm.openToSelection() }
            }
            .fixedSize()
            Spacer(minLength: 0)
            ProjButton(title: icons || narrow ? "" : ".fizzpat", icon: Icons.share, style: .ghost, padding: pad, enabled: has) {
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

/// The slots across the top (.spills): number, name, the open one cyan, a yellow dot for changes; armed: targets; the
/// project beside ringed green. A pattern dragged: the projects there its drop targets (as the phone's chips).
struct SlotPills: View {
    let pm: ProjectsModel
    var beside: Int?
    @Environment(\.fizzSnapshot) private var snapshot
    var body: some View {
        // (a narrow screen: they scroll sideways rather than run off its edge)
        if snapshot { pills.frame(maxWidth: .infinity, alignment: .leading).clipped() } else {
            ScrollView(.horizontal) { pills.padding(.vertical, 8).padding(.horizontal, 4) }
                .scrollIndicators(.hidden)
                .padding(.vertical, -8).padding(.horizontal, -4)
        }
    }

    private var pills: some View {
        HStack(spacing: 7) {
            ForEach(pm.slots) { s in
                let on = s.p == pm.sel && pm.arm == nil, target = pm.arm != nil && s.p != pm.arm?.from
                let badge = s.current ? FZ.cyan : s.demo ? FZ.champagne : s.there ? FZ.green : FZ.track
                let drop = pm.lift != nil && s.there, full = drop && pm.plan?.target(s.p)?.full == true
                let over = pm.lift?.overChip == true && pm.lift?.over?.q == s.p
                Button { Task { await pm.tapPill(s.p) } } label: {
                    HStack(spacing: 9) {
                        Text("\(s.p + 1)").fz(14, 800).foregroundStyle(s.there ? Color.black : RGB(0x5d5d66).color)
                            .frame(width: 30, height: 30).background(Circle().fill(badge.color))
                        if s.there { Text(s.name).fz(14, 800, em: 0.02).foregroundStyle(FZ.text.color).lineLimit(1) }
                    }
                    .padding(.leading, 6).padding(.trailing, s.there ? 15 : 6).frame(height: 42)
                    .background(Capsule().fill(on ? FZ.cyan.mix(16, FZ.raised).color : s.there ? FZ.raised.color : .clear))
                    .overlay {
                        if over {
                            Capsule().strokeBorder((full ? FZ.red : FZ.cyan).color, lineWidth: 2.5)
                        } else if drop && !on {
                            Capsule().strokeBorder((full ? FZ.red : FZ.cyan).color, style: StrokeStyle(lineWidth: 1.5, dash: [5, 4]))
                        } else if target {
                            Capsule().strokeBorder(FZ.champagne.opacity(0.75), style: StrokeStyle(lineWidth: 2, dash: [5, 4]))
                        } else if s.there {
                            Capsule().strokeBorder(on ? FZ.cyan.color : s.p == beside ? FZ.green.color : FZ.line.color,
                                                   lineWidth: on || s.p == beside ? 2 : 1.5)
                        } else {
                            Capsule().strokeBorder(on ? FZ.text.color : FZ.dim.color, style: StrokeStyle(lineWidth: 1.5, dash: [4, 3]))
                        }
                    }
                    .overlay(alignment: .topTrailing) { if s.changed { ChangeDot().offset(x: 2, y: -3) } }
                    .shadow(color: on || over ? FZ.cyan.opacity(0.45) : s.p == beside ? FZ.green.opacity(0.3) : .clear, radius: 10)
                    .scaleEffect(over ? 1.08 : 1)
                    .contentShape(Capsule())
                    .fixedSize()
                    .onGeometryChange(for: CGRect.self) { $0.frame(in: .named(ProjectsDropZones.space)) } action: { pm.zones.chips[s.p] = $0 }
                }
                .buttonStyle(ProjPress())
            }
        }
        .frame(height: 44)
    }
}
