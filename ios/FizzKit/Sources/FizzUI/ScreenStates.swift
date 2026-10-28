// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// LIBRARY's and POOL's small parts (phase I3): a write's outcome as a coloured strip that goes by itself, the confirm
// card (ui.js confirmBox: what, the button, the unit's undo), the icons the mock-ups add, and the snapshots' selection.

import SwiftUI
import FizzKit

// MARK: - The outcome strip

/// What came of a write: a dot, a few words in its colour; it goes after a while or at a tap.
struct NoteStrip: View {
    @Binding var note: ScreenNote?

    var body: some View {
        if let n = note {
            HStack(spacing: 10) {
                Circle().fill(n.color.color).frame(width: 10, height: 10).shadow(color: n.color.color, radius: 4)
                Text(n.text).fz(15, 800, em: 0.06).foregroundStyle(n.color.color).lineLimit(1).minimumScaleFactor(0.7)
            }
            .padding(.horizontal, 18)
            .frame(height: 44)
            .background(Capsule().fill(Color.black))
            .background(Capsule().fill(n.color.opacity(0.14)))
            .overlay(Capsule().strokeBorder(n.color.color, lineWidth: 1.5))
            .shadow(color: n.color.opacity(0.35), radius: 12)
            .contentShape(Capsule())
            .onTapGesture { withAnimation { note = nil } }
            .task(id: n.id) {
                try? await Task.sleep(for: .seconds(n.kind == .refused ? 4 : 2.2))
                if note?.id == n.id { withAnimation(.easeOut(duration: 0.3)) { note = nil } }
            }
            .transition(.move(edge: .top).combined(with: .opacity))
        }
    }
}

// MARK: - The confirm card

/// ui.js confirmBox as a card over a scrim: the title in its colour, what, a short line, UNDO KEPT / CLEARED, the
/// button and CANCEL.
struct ConfirmCard: View {
    let ask: ConfirmAsk
    let close: () -> Void

    var body: some View {
        ZStack(alignment: .bottom) {
            Color.black.opacity(0.62).ignoresSafeArea().onTapGesture(perform: close)
            VStack(alignment: .leading, spacing: 14) {
                HStack(spacing: 10) {
                    Text(ask.title).fz(14, 800, em: 0.12).foregroundStyle(ask.color.color).lineLimit(1).minimumScaleFactor(0.7)
                    Spacer(minLength: 6)
                    let c = ask.undo == .kept ? FZ.muted : FZ.orange
                    Text(ask.undo == .kept ? "UNDO KEPT" : "UNDO CLEARED").fz(12, 800, em: 0.08).foregroundStyle(c.color)
                        .padding(.horizontal, 8).frame(height: 24)
                        .overlay(Capsule().strokeBorder(c.color, lineWidth: 1.5))
                        .fixedSize()
                }
                Text(ask.what).fz(32, 800, em: -0.01).foregroundStyle(FZ.text.color).lineLimit(2).minimumScaleFactor(0.5)
                if let d = ask.detail {
                    Text(d).fz(15, 800, em: 0.04).foregroundStyle(FZ.body.color).lineLimit(2)
                }
                HStack(spacing: 10) {
                    PillButton(title: "CANCEL", ghost: true, height: 54, action: close)
                    PillButton(title: ask.go, color: ask.color, height: 54) {
                        close()
                        ask.action()
                    }
                }
                .padding(.top, 4)
            }
            .padding(.horizontal, 20).padding(.top, 20).padding(.bottom, 26)
            .frame(maxWidth: 520)
            .background(RoundedRectangle(cornerRadius: 28).fill(RGB(0x0d0d11).color))
            .overlay(RoundedRectangle(cornerRadius: 28).strokeBorder(ask.color.mix(60, FZ.line).color, lineWidth: 1.5))
            .shadow(color: ask.color.opacity(0.25), radius: 30)
            .padding(.horizontal, 12).padding(.bottom, 24)
            .transition(.move(edge: .bottom).combined(with: .opacity))
        }
    }
}

extension View {
    /// The confirm card over this view while `ask` is set.
    func confirmCard(_ ask: Binding<ConfirmAsk?>) -> some View {
        overlay {
            if let a = ask.wrappedValue {
                ConfirmCard(ask: a) { withAnimation(.spring(response: 0.35, dampingFraction: 0.85)) { ask.wrappedValue = nil } }
            }
        }
        .animation(.spring(response: 0.4, dampingFraction: 0.85), value: ask.wrappedValue?.id)
    }
}

// MARK: - Icons

extension Icons {
    /// a pencil (fizz-ios.html IC.pencil)
    static let pencil = VectorArt(20, 20, [.path("M3 15l9-9 3 3-9 9H3zM13 5l2-2 3 3-2 2")])
    static let minus = VectorArt(24, 24, [.path("M5 12h14")])
    /// two arrows crossing (MOVE / SWAP a place)
    static let swap = VectorArt(24, 24, [.path("M4 8h14M14 4l4 4-4 4M20 16H6M10 12l-4 4 4 4")])
    static let close = VectorArt(24, 24, [.path("M6 6l12 12M18 6L6 18")])
    /// sort: bars getting longer
    static let sort = VectorArt(24, 24, [.path("M4 6h6M4 12h10M4 18h16")])
    /// the log: lines of text
    static let log = VectorArt(24, 24, [.rect(3.5, 3, 17, 18, 3), .path("M7.5 8h9M7.5 12h9M7.5 16h5")])
}

// MARK: - Snapshots

private struct ShotSelectionKey: EnvironmentKey { static let defaultValue: String? = nil }

public extension EnvironmentValues {
    /// fizzshots: the preset ("mine:2") or pool place ("pool:3") a screen shows selected (its sheet open on a phone).
    var fizzShotSelection: String? {
        get { self[ShotSelectionKey.self] }
        set { self[ShotSelectionKey.self] = newValue }
    }
}
