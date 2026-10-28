// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// iPhone: the projects kept on the phone (mock-up 19; .fizzprj files in Documents/Projects). KEEP keeps the slot
// looked at; a card tapped opens: its patterns as dots (one tapped flies into the clip, ready to PASTE in any slot),
// LOAD INTO (the slots light up, the one tapped takes it, as a change waiting for WRITE), share, rename, delete.

import SwiftUI
import FizzData
import FizzKit

struct ProjectsKeptView: View {
    let model: UnitModel
    let pm: ProjectsModel
    @Binding var importing: Bool

    var body: some View {
        let s = pm.selected
        ScreenScroll(bottom: pm.ops.isEmpty ? 120 : 270, refresh: { await pm.kept.reload() }) {
            HStack(alignment: .center, spacing: 10) {
                Text("KEPT").fz(46, 800, em: -0.01).foregroundStyle(FZ.champagne.color)
                    .shadow(color: FZ.champagne.opacity(0.4), radius: 12)
                Text("\(pm.kept.items.count)").fz(22, 800).foregroundStyle(FZ.text.color)
                    .frame(maxHeight: .infinity, alignment: .bottom).padding(.bottom, 6)
                Spacer(minLength: 0)
                if let c = pm.clip { ClipChip(clip: c) } else {
                    RoundIcon(icon: Icons.open, color: FZ.champagne, ring: FZ.champagne.mix(45, FZ.line), size: 36) { importing = true }
                }
            }
            .frame(height: 56)
            .padding(.bottom, 10)
            .animation(.spring(response: 0.45, dampingFraction: 0.72), value: pm.clip)
            SlotChips(model: pm, keptOn: true)
            if s.there {
                KeepCard(name: s.name, enabled: pm.online || pm.view != nil) { Task { await pm.keep() } }
                    .padding(.bottom, 8)
            }
            VStack(spacing: 8) {
                ForEach(pm.kept.items) { k in
                    KeptCard(model: pm, item: k, open: pm.keptSel == k.id)
                }
            }
        }
        .padding(.horizontal, 16).padding(.top, 4)
    }
}

/// KEEP the slot looked at: a dashed champagne card with its name.
struct KeepCard: View {
    let name: String
    var enabled = true
    var height: CGFloat = 56
    var action: () -> Void
    var body: some View {
        Button(action: action) {
            HStack(spacing: 10) {
                IconView(ProjIcons.keep, color: FZ.champagne.color, lineWidth: 2.3).frame(width: 22, height: 22)
                Text("KEEP").fz(15, 800, em: 0.1).foregroundStyle(FZ.champagne.color)
                Text(name).fz(18, 800, em: 0.02).foregroundStyle(FZ.text.color).lineLimit(1)
                Spacer(minLength: 0)
                IconView(Icons.plus, color: FZ.champagne.color, lineWidth: 2.6).frame(width: 20, height: 20)
            }
            .padding(.horizontal, 16).frame(height: height)
            .background(RoundedRectangle(cornerRadius: 18).fill(FZ.champagne.opacity(0.06)))
            .overlay(RoundedRectangle(cornerRadius: 18).strokeBorder(FZ.champagne.mix(70, FZ.line).color,
                                                                     style: StrokeStyle(lineWidth: 1.5, dash: [6, 4])))
            .contentShape(RoundedRectangle(cornerRadius: 18))
        }
        .buttonStyle(.plain)
        .disabled(!enabled)
        .opacity(enabled ? 1 : 0.4)
    }
}
