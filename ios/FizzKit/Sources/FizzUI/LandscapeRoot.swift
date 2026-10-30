// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// The iPhone held sideways (the web Manager's phone landscape: shell.css, the tabs a column at the left): a slim rail
// of the tab icons instead of the floating bar, a low top bar, the tab beside; the tabs with a list and its detail
// lay them out in two columns (LIBRARY, PROJECTS, BACKUP, EDIT, POOL). PLAY keeps its own band (mock-up 32).

import SwiftUI
import FizzKit

/// Which of the app's layouts a size gets. Decided on the height, not the device: an iPhone Pro Max on its side is
/// wider than 720 pt (a regular width) but only 428 pt high.
public enum RootLayout: String, Sendable, Equatable {
    /// the iPhone upright: the top bar, the floating tab bar
    case phone
    /// the iPhone on its side: the slim rail, the tabs in two columns
    case phoneWide
    /// the iPad (or any window at least 720 x 500): the big rail with the unit card
    case pad

    public static func pick(_ size: CGSize) -> RootLayout {
        if size.width > size.height && size.height < 500 { return .phoneWide }
        return size.width >= 720 && size.height >= 500 ? .pad : .phone
    }

}

/// The rail at the left of the iPhone on its side: the six tabs as icons (the shown one lit in its colour), IMPORT's
/// violet "+" at the foot. The buttons shrink to fit a low screen (an iPhone SE: 320 pt).
struct LandscapeRail: View {
    @Binding var tab: FizzTab
    static let width: CGFloat = 66
    /// the room kept at the bottom (the screens run to the edge): the home indicator's, the bars' clearance
    static let bottom: CGFloat = 16

    var body: some View {
        GeometryReader { g in
            let n = CGFloat(FizzTab.bar.count + 1)
            let gap: CGFloat = 6
            let d = max(36, min(48, (g.size.height - 6 - Self.bottom - gap * (n - 1)) / n))
            VStack(spacing: gap) {
                ForEach(FizzTab.bar) { t in
                    let on = t == tab
                    Button { withAnimation(.spring(response: 0.35, dampingFraction: 0.8)) { tab = t } } label: {
                        IconView(t.icon, color: on ? .black : t.tint.color, lineWidth: 2, backdrop: on ? t.tint.color : .black)
                            .frame(width: d * 0.44, height: d * 0.44)
                            .frame(width: d, height: d)
                            .background(Circle().fill(on ? t.tint.color : t.tint.opacity(0.09)))
                            .overlay(Circle().strokeBorder(on ? t.tint.color : t.tint.mix(38, FZ.nav).color, lineWidth: 1.5))
                            .shadow(color: on ? t.tint.opacity(0.5) : .clear, radius: 8)
                            .contentShape(Circle())
                    }
                    .buttonStyle(.plain)
                    .accessibilityLabel(t.title)
                }
                Spacer(minLength: 0)
                Button { tab = .importer } label: {
                    IconView(Icons.plus, color: .black, lineWidth: 3).frame(width: d * 0.5, height: d * 0.5)
                        .frame(width: d, height: d)
                        .background(Circle().fill(FZ.violet.color))
                        .overlay(Circle().strokeBorder(.white.opacity(tab == .importer ? 0.9 : 0), lineWidth: 2))
                        .shadow(color: FZ.violet.opacity(0.55), radius: 9)
                }
                .buttonStyle(.plain)
                .accessibilityLabel(FizzTab.importer.title)
            }
            .padding(.top, 6).padding(.bottom, Self.bottom)
            .frame(width: g.size.width, height: g.size.height)
        }
        .frame(width: Self.width)
    }
}

/// The top bar on the iPhone's side: lower (44 pt), the logo and the word, the TRACK pill or chip, the unit.
struct LandscapeTopBar: View {
    let model: UnitModel
    let tab: FizzTab
    @State private var ports = false
    static let height: CGFloat = 42

    var body: some View {
        HStack(spacing: 10) {
            // (a long press on the logo: the PORTS sheet, as the upright bar's)
            HStack(spacing: 8) {
                BubbleLogo().frame(width: 28, height: 28)
                FizzWord(size: 21)
            }
            .contentShape(Rectangle())
            .onLongPressGesture(minimumDuration: 0.6) { ports = true }
            Spacer(minLength: 0)
            if TrackPill.shows(on: tab) {
                if model.editor.shown { TrackChip(model: model) } else { TrackPill(model: model) }
            }
            if model.unit != nil { UnitBadge(model: model).onLongPressGesture(minimumDuration: 0.6) { ports = true } }
        }
        .frame(height: Self.height)
        .padding(.leading, 12).padding(.trailing, 14)
        .turnDim(model.editor, [])
        .sheet(isPresented: $ports) { PortsSheet(model: model) }
    }
}

// MARK: - Sideways, for the screens

private struct SidewaysKey: EnvironmentKey { static let defaultValue = false }

extension EnvironmentValues {
    /// The iPhone on its side (PhoneRoot sets it): the screens' bars in one row.
    var fizzSideways: Bool {
        get { self[SidewaysKey.self] }
        set { self[SidewaysKey.self] = newValue }
    }
}

// MARK: - PROJECTS' changes waiting, short

/// The changes waiting in the selection's bar's height, beside it (the iPhone on its side: ProjectsPhoneBar): the
/// count, UNDO, DISCARD, the WRITE TO FM-1 slide.
struct LandscapePendingStrip: View {
    let model: ProjectsModel

    var body: some View {
        let n = model.ops.count
        HStack(spacing: 6) {
            Text("\(n)").fz(32, 800).monospacedDigit().foregroundStyle(FZ.yellow.color)
                .contentTransition(.numericText())
                .fixedSize()
                .padding(.trailing, 2)
            ProjRoundIcon(icon: ProjIcons.undo, color: FZ.text, ring: FZ.line, size: 38) { Task { await model.undo() } }
                .keyboardShortcut("z", modifiers: .command)
                .accessibilityLabel("UNDO")
            ProjRoundIcon(icon: ProjIcons.clear, color: FZ.red, ring: FZ.red.mix(45, FZ.line), size: 38) { model.discard() }
                .accessibilityLabel("DISCARD")
            WriteSlide(model: model, height: 46).frame(minWidth: 150)
        }
        .padding(.leading, 12).padding(.trailing, 6)
        .frame(height: ProjectsSelectionBar.height)
        .background(RoundedRectangle(cornerRadius: 22).fill(FZ.yellow.mix(10, RGB(0x0e0e12)).color))
        .overlay(RoundedRectangle(cornerRadius: 22).strokeBorder(FZ.yellow.color, lineWidth: 2))
        .shadow(color: FZ.yellow.opacity(0.25), radius: 12)
        .animation(.spring(response: 0.35, dampingFraction: 0.8), value: n)
    }
}
