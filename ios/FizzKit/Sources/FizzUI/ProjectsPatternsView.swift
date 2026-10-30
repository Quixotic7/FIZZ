// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// iPhone: a project's patterns (mock-up 18): A01..D16 as tiles with each track's trigs; a tap selects one, a drag
// across selects more, the edit menu comes over the selection (COPY, PASTE, CLEAR, .fizzpat); pasted tiles drop in
// yellow, cleared ones dashed; the yellow bar counts the changes and WRITE TO FM-1 is a slide. The chevron (or a swipe
// right from the screen's edge) goes back to the slots, the slot chips switch projects, the box chip the kept ones.

import SwiftUI
import FizzData
import FizzKit

struct ProjectsPatternsView: View {
    let model: UnitModel
    let pm: ProjectsModel
    @Binding var importing: Bool
    @Binding var sharing: SharedFile?
    @State private var anchor: CGRect?
    @State private var gridW: CGFloat = 358
    /// the screen's height under the top bar (the tiles made shorter when the grid would reach the yellow bar)
    @State private var screenH: CGFloat = 760

    var body: some View {
        let s = pm.selected
        VStack(alignment: .leading, spacing: 0) {
            HStack(alignment: .center, spacing: 10) {
                ProjBackButton { pm.back() }
                Text(s.there ? s.name : "\(s.p + 1)").fz(46, 800, em: -0.01)
                    .foregroundStyle((s.there ? FZ.cyan : FZ.dim).color)
                    .shadow(color: FZ.cyan.opacity(s.there ? 0.45 : 0), radius: 12)
                    .lineLimit(1).minimumScaleFactor(0.6)
                    .layoutPriority(1)
                if let m = s.model, m.there {
                    HStack(alignment: .firstTextBaseline, spacing: 3) {
                        Text("\(m.used)").fz(22, 800).foregroundStyle(FZ.text.color).contentTransition(.numericText())
                        Text("/ 64").fz(15, 800).foregroundStyle(FZ.muted.color)
                    }
                    .fixedSize()
                    .frame(maxHeight: .infinity, alignment: .bottom).padding(.bottom, 6)
                }
                Spacer(minLength: 0)
                if let c = pm.clip { ClipChip(clip: c) } else {
                    ProjRoundIcon(icon: Icons.open, color: FZ.cyan, ring: FZ.cyan.mix(45, FZ.line), size: 36) { importing = true }
                }
            }
            .frame(height: 56)
            .padding(.bottom, 5)
            .animation(.spring(response: 0.45, dampingFraction: 0.75), value: pm.clip)
            SlotChips(model: pm)
            if let pr = s.model, pr.there {
                PatternGrid(model: pm, project: pr, tileH: tileH, onAnchor: { anchor = $0; gridW = $1 })
                    .overlay(alignment: .topLeading) { menu }
                    .opacity(pm.reading ? 0.3 : 1)
                    .overlay { if pm.reading { ReadingMeter(fraction: pm.readFraction) } }
            } else if pm.reading || (s.there && pm.view == nil) {
                EmptyGrid()
                    .overlay {
                        if pm.reading { ReadingMeter(fraction: pm.readFraction) } else {
                            ProjButton(title: "READ", icon: Icons.down, color: FZ.cyan, height: 52, size: 16, enabled: pm.online) {
                                Task { await pm.read(force: true) }
                            }
                            .frame(width: 160)
                        }
                    }
            } else {
                Button { Task { await pm.newProject() } } label: {
                    EmptyGrid().overlay {
                        Text("+").fz(64, 500).foregroundStyle(FZ.dim.color)
                    }
                }
                .buttonStyle(ProjPress())
                .disabled(!pm.online)
            }
            Spacer(minLength: 0)
        }
        .padding(.horizontal, 16).padding(.top, 4)
        .onGeometryChange(for: CGFloat.self) { $0.size.height } action: { screenH = $0 }
        .animation(.spring(response: 0.4, dampingFraction: 0.82), value: pm.ops.isEmpty)
        .contentShape(Rectangle())
        // a swipe right from the screen's edge (left of the bank letters, never a tile's drag): back to the slots
        .simultaneousGesture(
            DragGesture(minimumDistance: 30)
                .onEnded { v in
                    if v.startLocation.x < 22, v.translation.width > 80, abs(v.translation.height) < v.translation.width / 2 { pm.back() }
                }
        )
    }

    private var tileH: CGFloat { Self.tileH(screenH, pending: !pm.ops.isEmpty) }

    /// The tiles' height: 40, less when the grid would reach under the yellow bar (or the tab bar), never under 28.
    static func tileH(_ screenH: CGFloat, pending: Bool) -> CGFloat {
        // (above the grid: the title 61, the chips 53; below it: the tab bar 88, the yellow bar ~140, a gap)
        let below: CGFloat = pending ? 100 + 140 + 10 : 100
        return min(40, max(28, ((screenH - 4 - 61 - 53 - below) - 4 * 5 - 3 * 12) / 8))
    }

    /// The edit menu above the selection's first tile (below it near the top), its point at the tile. (No
    /// GeometryReader here: one over the grid took its taps.)
    @ViewBuilder private var menu: some View {
        if !pm.pats.isEmpty, let a = anchor, pm.canEdit {
            // (above only when it stays on the grid: never over the slot chips)
            let h: CGFloat = 54, above = a.minY - h - 10 >= -2
            let y = above ? a.minY - h - 10 : a.maxY + 10
            let px = min(max(a.midX, 28), gridW - 28)
            ZStack(alignment: .topLeading) {
                Rectangle().fill(RGB(0x1b1b21).color).frame(width: 16, height: 16)
                    .overlay(Rectangle().strokeBorder(RGB(0x3a3a44).color, lineWidth: 1.5))
                    .rotationEffect(.degrees(45))
                    .padding(.leading, px - 8).padding(.top, above ? y + h - 9 : y - 7)
                    .allowsHitTesting(false)
                EditMenu(model: pm) {
                    Task { if let u = await pm.fizzpatFile() { sharing = SharedFile(url: u) } }
                }
                .fixedSize()
                .frame(width: gridW)
                // (laid out where it shows, not offset: an offset menu's taps fell through to the tiles under it)
                .padding(.top, y)
                .transition(.scale(scale: 0.5, anchor: UnitPoint(x: px / max(1, gridW), y: above ? 1 : 0)).combined(with: .opacity))
            }
            .frame(width: gridW, alignment: .topLeading)
            .animation(.spring(response: 0.5, dampingFraction: 0.62), value: a)
        }
    }
}

/// iPhone: the yellow bar of changes waiting, over the bottom fade and above the tab bar (PhoneRoot draws it), on the
/// slots, the patterns and the kept projects.
struct ProjectsPhoneBar: View {
    let model: UnitModel
    var body: some View {
        let pm = model.projectsTab
        ZStack(alignment: .bottom) {
            if !pm.ops.isEmpty {
                PendingBar(model: pm).padding(.horizontal, 12).padding(.bottom, 100)
                    .transition(.move(edge: .bottom).combined(with: .opacity))
            }
        }
        .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .bottom)
        .animation(.spring(response: 0.4, dampingFraction: 0.82), value: pm.ops.isEmpty)
    }
}

/// The grid with nothing in it (not read yet, an empty slot).
struct EmptyGrid: View {
    var body: some View {
        VStack(spacing: 12) {
            ForEach(0..<4, id: \.self) { b in
                HStack(spacing: 0) {
                    Text(["A", "B", "C", "D"][b]).fz(15, 800).foregroundStyle(FZ.dim.color).frame(width: 21, alignment: .leading)
                    VStack(spacing: 5) {
                        ForEach(0..<2, id: \.self) { _ in
                            HStack(spacing: 5) {
                                ForEach(0..<8, id: \.self) { _ in
                                    RoundedRectangle(cornerRadius: 9).fill(RGB(0x0f0f13).color)
                                        .overlay(Hatch().padding(EdgeInsets(top: 15, leading: 4, bottom: 4, trailing: 4)))
                                        .overlay(RoundedRectangle(cornerRadius: 9).strokeBorder(RGB(0x1c1c22).color, lineWidth: 1.5))
                                        .frame(height: 40)
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
