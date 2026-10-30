// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// Getting around PLAY: a double tap on the LCD fills the display with it (grown from its place, a double tap anywhere
// puts it back); the FIZZ badge (RemoteFM1's wordmark's place) goes straight to the manager's tab last shown.

import SwiftUI

enum PlayNav {
    /// The LCD growing and shrinking back.
    static let spring = Animation.spring(response: 0.42, dampingFraction: 0.8)
    /// PlayView's coordinate space (the LCD's place in it).
    static let space = "play.board"
}

/// The LCD full screen, its two layers: the theme's body, then the LCD as large as fits (square, centred).
@MainActor struct FullLCD {
    let play: PlayModel
    /// PlayView's whole size
    let size: CGSize
    /// the LCD's place on the panel (PlayNav.space)
    let from: CGRect

    var side: CGFloat { min(size.width, size.height) }

    /// The body around it (takes the double tap that puts it back).
    var backdrop: some View {
        play.theme.palette.body.color
            .frame(width: size.width, height: size.height)
            .contentShape(Rectangle())
            .onTapGesture(count: 2) { play.lcdFull = false }
            .transition(.opacity)
    }

    /// The LCD; it grows from (and shrinks back to) its place on the panel.
    var screen: some View {
        let s = side
        let k = from.width > 0 ? from.width / s : 0.5
        let d = from.width > 0 ? CGSize(width: from.midX - size.width / 2, height: from.midY - size.height / 2) : .zero
        return LiveLCD(play: play, size: s)
            .frame(width: size.width, height: size.height)
            .allowsHitTesting(false)
            .transition(.modifier(active: Zoom(scale: k, offset: d), identity: Zoom(scale: 1, offset: .zero)))
    }

    struct Zoom: ViewModifier {
        let scale: CGFloat
        let offset: CGSize
        func body(content: Content) -> some View { content.scaleEffect(scale).offset(offset) }
    }
}

/// The black FIZZ badge where RemoteFM1 has its wordmark (mock-ups 49-54): the bubbles and FIZZ in a pill, or the
/// bubbles alone in a round one (the iPhone on its side, beside KN4). A tap: the manager.
struct FizzBadge: View {
    let height: CGFloat
    var round = false
    /// the board's unit (the bubbles 20 and the word 14 at any height)
    var scale: CGFloat = 1
    let go: () -> Void

    var body: some View {
        let k = scale
        Button(action: go) {
            Group {
                if round {
                    BubbleLogo(lineWidth: 2.8).frame(width: height * 0.65, height: height * 0.65)
                        .frame(width: height, height: height)
                        .background(Circle().fill(RGB(0x0b0b0e).color))
                } else {
                    HStack(spacing: 5 * k) {
                        BubbleLogo(lineWidth: 2.8).frame(width: 20 * k, height: 20 * k)
                        Text("FIZZ").fz(14 * k, 800, em: 0.1)
                            .foregroundStyle(LinearGradient(gradient: FZ.word, startPoint: .leading, endPoint: .trailing))
                            .fixedSize()
                    }
                    .padding(.leading, 5 * k).padding(.trailing, 11 * k - 1.4 * k)
                    .frame(height: height)
                    .background(Capsule().fill(RGB(0x0b0b0e).color))
                }
            }
            .frame(minWidth: 44, minHeight: 44)
            .contentShape(Rectangle())
        }
        .buttonStyle(PressStyle(scale: 0.92))
        .accessibilityLabel("Manager")
    }
}
