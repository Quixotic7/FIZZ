// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// POOL's SELECT bar (PoolSelect.swift), in the bottom panel's place on the phone and the iPad: the count and the
// ticked sounds' colours, DONE (LIBRARY's SELECT bar's language: cream, mock-up 41); CLEAR n, MOVE, → KEPT n; MOVE
// waiting for its place (yellow, as the single MOVE); a run's count while the writes go.

import SwiftUI
import FizzKit

struct PoolBulkBar: View {
    let count: Int
    let tints: [RGB]
    /// the writes may start
    let can: Bool
    let moving: Bool
    let run: BulkProgress?
    let done: () -> Void
    let clear: () -> Void
    let move: () -> Void
    let cancelMove: () -> Void
    let keep: () -> Void

    var body: some View {
        VStack(spacing: 10) {
            HStack(spacing: 8) {
                Text("\(count)").fz(40, 800).foregroundStyle(FZ.text.color).contentTransition(.numericText())
                    .shadow(color: FZ.cream.opacity(0.3), radius: 10)
                    .fixedSize()
                BulkRings(tints: tints, size: 20)
                Spacer(minLength: 0)
                SelectPill(selecting: true, action: done)
            }
            .frame(height: 44)
            if let run {
                JobLine(done: run.done, of: run.of)
            } else if moving {
                HStack(spacing: 10) {
                    IconView(Icons.swap, color: FZ.yellow.color, lineWidth: 2.4).frame(width: 24, height: 24)
                    Text("MOVE \(count)").fz(20, 800).foregroundStyle(FZ.text.color)
                    Text("→ ?").fz(22, 800).foregroundStyle(FZ.yellow.color)
                    Spacer(minLength: 0)
                    RoundIcon(art: Icons.close, color: FZ.muted, action: cancelMove)
                }
                .frame(height: 46)
            } else {
                HStack(spacing: 8) {
                    action("CLEAR", n: count, color: FZ.red, fill: false, enabled: can && count > 0, clear)
                    action("MOVE", icon: Icons.swap, color: FZ.yellow, fill: false, enabled: can && count > 0, move)
                    action("→ KEPT", n: count, color: FZ.champagne, fill: true, enabled: count > 0, keep)
                }
            }
        }
        .padding(14)
        .background(RoundedRectangle(cornerRadius: 26).fill(FZ.cream.mix(5, RGB(0x0d0d11)).color))
        .overlay(RoundedRectangle(cornerRadius: 26).strokeBorder(FZ.cream.color, lineWidth: 1.5))
        .shadow(color: FZ.cream.opacity(0.16), radius: 16)
        .shadow(color: .black.opacity(0.8), radius: 14, y: 8)
        .animation(.spring(response: 0.3, dampingFraction: 0.8), value: count)
        .animation(.spring(response: 0.3, dampingFraction: 0.8), value: moving)
    }

    private func action(_ title: String, n: Int? = nil, icon: VectorArt? = nil, color: RGB, fill: Bool, enabled: Bool,
                        _ go: @escaping () -> Void) -> some View {
        let ink: Color = fill ? .black : FZ.text.color
        return Button(action: go) {
            HStack(spacing: 6) {
                if let icon { IconView(icon, color: fill ? .black : color.color, lineWidth: 2.3).frame(width: 16, height: 16) }
                Text(title).fz(13, 800, em: 0.06).lineLimit(1).fixedSize()
                if let n, n > 0 {
                    Text("\(n)").fz(13, 800).monospacedDigit()
                        .foregroundStyle(fill ? color.color : .black)
                        .padding(.horizontal, 5).frame(minWidth: 22).frame(height: 22)
                        .background(Capsule().fill(fill ? Color.black : color.color))
                        .contentTransition(.numericText())
                }
            }
            .foregroundStyle(ink)
            .padding(.horizontal, 8)
            .frame(maxWidth: .infinity).frame(height: 46)
            .background(Capsule().fill(fill ? color.color : color.opacity(0.08)))
            .overlay(Capsule().strokeBorder(fill ? .clear : color.mix(65, FZ.line).color, lineWidth: 1.5))
            .contentShape(Capsule())
        }
        .buttonStyle(PressStyle(scale: 0.95))
        .disabled(!enabled)
        .opacity(enabled ? 1 : 0.4)
    }
}
