// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// Slide to confirm (mock-up 08, fizz-ios.html .slide): the web's typed RESTORE as a gesture. The knob is dragged along
// its track, the fill follows it, the label shimmers ahead of it; let go before the end and it springs back, at the end
// it locks and the action runs once. RESTORE uses it, and PROJECTS' WRITE TO FM-1.

import SwiftUI

public struct SlideToConfirm: View {
    let title: String
    let color: RGB
    let enabled: Bool
    let action: () -> Void

    @Environment(\.fizzSnapshot) private var snapshot
    @State private var dx: CGFloat = 0
    @State private var locked = false
    @GestureState private var dragging = false

    static let height: CGFloat = 74
    static let knob: CGFloat = 60

    /// title: the action's word ("RESTORE"); color: its colour; the action runs once when the knob reaches the end.
    public init(_ title: String, color: RGB = FZ.yellow, enabled: Bool = true, action: @escaping () -> Void) {
        self.title = title; self.color = color; self.enabled = enabled; self.action = action
    }

    public var body: some View {
        GeometryReader { g in
            let travel = max(1, g.size.width - Self.knob - 10)
            let x = locked ? travel : min(travel, max(0, dx))
            ZStack(alignment: .leading) {
                Capsule().fill(color.mix(9, FZ.bg).color)
                Capsule().fill(color.opacity(0.3))
                    .frame(width: x + Self.knob + 10)
                label.padding(.leading, 50).frame(maxWidth: .infinity)
                    .opacity(1 - min(1, x / travel) * 0.8)
                knob
                    .offset(x: 5 + x)
                    .gesture(drag(travel))
            }
            .overlay(Capsule().strokeBorder(color.color, lineWidth: 2))
            .clipShape(Capsule())
        }
        .frame(height: Self.height)
        .opacity(enabled ? 1 : 0.4)
        .allowsHitTesting(enabled && !locked)
        .sensoryFeedback(.success, trigger: locked) { _, now in now }
        .accessibilityElement(children: .ignore)
        .accessibilityLabel(title)
        .accessibilityAddTraits(.isButton)
        .accessibilityAction { if enabled && !locked { fire() } }
    }

    private var knob: some View {
        Animated(still: 0) { t in
            IconView(TransferIcons.chev, color: .black, lineWidth: 3, backdrop: color.color)
                .frame(width: 28, height: 28)
                .offset(x: dragging || locked || snapshot ? 0 : 3 * breathe(t, 1.6))
        }
        .frame(width: Self.knob, height: Self.knob)
        .background(Circle().fill(color.color))
        .shadow(color: color.opacity(0.6), radius: 11)
        .contentShape(Circle().inset(by: -8))
    }

    /// The word and three chevrons, a light running through them.
    private var label: some View {
        Animated(still: 0.6) { t in
            let p = (t.truncatingRemainder(dividingBy: 2.2)) / 2.2
            let c = 1.6 - 2.2 * p                                     // (the light's centre, right to left)
            let dark = color.mix(40, RGB(0x2a2a20)).color
            let at = { (x: Double) in min(1, max(0, x)) }
            HStack(spacing: 10) {
                Text(title).fz(19, 800, em: 0.14)
                Text("›››").fz(19, 800, em: -0.1)
            }
            .foregroundStyle(LinearGradient(stops: [
                .init(color: dark, location: 0), .init(color: color.color, location: at(c - 0.12)),
                .init(color: RGB(0xfff6d0).color, location: at(c)), .init(color: color.color, location: at(c + 0.12)),
                .init(color: dark, location: 1),
            ], startPoint: .leading, endPoint: .trailing))
            .lineLimit(1)
        }
    }

    private func drag(_ travel: CGFloat) -> some Gesture {
        DragGesture(minimumDistance: 2)
            .updating($dragging) { _, s, _ in s = true }
            .onChanged { v in dx = v.translation.width }
            .onEnded { v in
                if v.translation.width >= travel * 0.92 {
                    withAnimation(.spring(response: 0.25, dampingFraction: 0.9)) { dx = travel }
                    fire()
                } else {
                    withAnimation(.spring(response: 0.45, dampingFraction: 0.62)) { dx = 0 }
                }
            }
    }

    private func fire() {
        guard !locked else { return }
        locked = true
        action()
    }
}
