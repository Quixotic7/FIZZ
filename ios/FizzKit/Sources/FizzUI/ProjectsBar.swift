// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// PROJECTS' selection bar: COPY, PASTE, CLEAR, SHARE (.fizzpat), TO… in a fixed row (the owner: "static on bottom of
// the view", never over the patterns). The phone: at the bottom of a project's patterns, above the yellow bar when it
// shows (ProjectsPhoneBar stacks them; the grid's tiles leave room for both). The iPad: its row under the grid (the
// mock-up 22 .ptools) takes the same states. Everything off while nothing is selected; PASTE also with nothing copied;
// TO… with exactly one pattern there.

import SwiftUI
import FizzData
import FizzKit

/// What the selection's buttons may do now.
public struct ProjectsBarState: Equatable, Sendable {
    public var copy = false, paste = false, clear = false, share = false, to = false
    /// something selected in a project that can be edited
    public var any: Bool { copy }

    public init(copy: Bool = false, paste: Bool = false, clear: Bool = false, share: Bool = false, to: Bool = false) {
        self.copy = copy; self.paste = paste; self.clear = clear; self.share = share; self.to = to
    }

    @MainActor public init(_ pm: ProjectsModel) {
        let has = !pm.pats.isEmpty && pm.canEdit
        self.init(copy: has, paste: has && pm.clip != nil, clear: has, share: has, to: has && pm.canTo)
    }
}

/// The phone's bar: five equal cells, each an icon over its name (COPY cyan, PASTE yellow once something is copied,
/// CLEAR red, SHARE, TO… cyan-ringed); a cell that can't act now dims.
struct ProjectsSelectionBar: View {
    let pm: ProjectsModel
    var share: () -> Void
    static let height: CGFloat = 60

    var body: some View {
        let st = ProjectsBarState(pm)
        HStack(spacing: 5) {
            BarCell(title: "COPY", icon: ProjIcons.copy, color: FZ.cyan, fill: true, enabled: st.copy) { Task { await pm.copy() } }
            BarCell(title: "PASTE", icon: ProjIcons.paste, color: FZ.yellow, fill: pm.clip != nil, enabled: st.paste) { pm.paste() }
            BarCell(title: "CLEAR", icon: ProjIcons.clear, color: FZ.red, enabled: st.clear) { pm.clear() }
            BarCell(title: "SHARE", icon: Icons.share, color: FZ.text, enabled: st.share, action: share)
            BarCell(title: "TO…", icon: ProjIcons.move, color: FZ.cyan, enabled: st.to) { Task { await pm.openToSelection() } }
        }
        .padding(5)
        .frame(height: Self.height)
        .background(RoundedRectangle(cornerRadius: 22).fill(RGB(0x1b1b21).color))
        .overlay(RoundedRectangle(cornerRadius: 22).strokeBorder(RGB(0x3a3a44).color, lineWidth: 1.5))
        .shadow(color: .black, radius: 12, y: -6)
        .animation(.easeOut(duration: 0.18), value: st)
    }
}

/// One of the bar's cells: filled (black ink, a glow) or outlined in its colour.
private struct BarCell: View {
    let title: String
    let icon: VectorArt
    let color: RGB
    var fill = false
    let enabled: Bool
    var action: () -> Void

    var body: some View {
        let on = fill && enabled, ink: Color = on ? .black : color.color
        Button(action: action) {
            VStack(spacing: 3) {
                IconView(icon, color: ink, lineWidth: 2.3, backdrop: on ? color.color : RGB(0x1b1b21).color)
                    .frame(width: 18, height: 18)
                Text(title).fz(12, 800, em: 0.06).lineLimit(1).minimumScaleFactor(0.8)
            }
            .foregroundStyle(ink)
            .frame(maxWidth: .infinity, maxHeight: .infinity)
            .background(RoundedRectangle(cornerRadius: 17).fill(on ? color.color : .clear))
            .overlay(RoundedRectangle(cornerRadius: 17).strokeBorder(on ? .clear : color.mix(45, FZ.line).color, lineWidth: 1.5))
            .shadow(color: on ? color.opacity(0.35) : .clear, radius: 8)
            .contentShape(RoundedRectangle(cornerRadius: 17))
        }
        .buttonStyle(ProjPress())
        .disabled(!enabled)
        .opacity(enabled ? 1 : 0.35)
        .accessibilityLabel(title == "TO…" ? "TO" : title)
    }
}
