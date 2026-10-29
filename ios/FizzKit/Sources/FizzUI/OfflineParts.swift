// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// The app with no FM-1 (mock-ups 23, 27, 28): the unit pill reads THIS iPHONE in the emulator's colour (a tap opens
// PLAY); an FM-1 the app could not take (update mode, not FIZZ, two units) marks the pill and opens its card as a
// sheet, over the tabs instead of in their place.

import SwiftUI
import FizzKit

/// The unit pill: the FM-1's (green, its version, USB / BT) or THIS iPHONE's; a dot when an FM-1 waits on a notice.
struct UnitBadge: View {
    @Bindable var model: UnitModel

    var body: some View {
        Group {
            if let u = model.unit {
                if u.isEmulated {
                    EmulatorPill().contentShape(Capsule()).onTapGesture { model.goTo = .play }
                } else {
                    UnitPill(info: u)
                }
            }
        }
        .overlay(alignment: .topTrailing) {
            if let n = model.notice {
                Button { model.noticeOpen = true } label: {
                    Circle().fill(NoticeSheet.color(n).color).frame(width: 14, height: 14)
                        .overlay(Circle().strokeBorder(.black, lineWidth: 2))
                        .shadow(color: NoticeSheet.color(n).color, radius: 5)
                        .frame(width: 30, height: 30)
                        .contentShape(Rectangle())
                }
                .buttonStyle(.plain)
                .offset(x: 10, y: -10)
            }
        }
        .sheet(isPresented: $model.noticeOpen) { NoticeSheet(model: model) }
    }
}

/// THIS iPHONE: the emulated FIZZ as the unit, in the emulator theme's colour.
struct EmulatorPill: View {
    @AppStorage("play.theme") private var themeName = PlayTheme.tangerine.rawValue
    var size: CGFloat = 13

    var body: some View {
        let c = (PlayTheme(rawValue: themeName) ?? .tangerine).accent
        HStack(spacing: 7) {
            IconView(PlayIcons.phone, color: c.color, lineWidth: 2.4).frame(width: size + 4, height: size + 4)
            Text(EmulatorHost.hereName).fz(size, 800, em: 0.04).foregroundStyle(FZ.text.color)
        }
        .lineLimit(1)
        .padding(.leading, 10).padding(.trailing, 13)
        .frame(height: 36)
        .background(Capsule().fill(c.mix(16, FZ.bg).color))
        .overlay(Capsule().strokeBorder(c.color, lineWidth: 2))
        .fixedSize()
    }
}

/// An FM-1 found but not taken: its card (update mode, not FIZZ, the two to choose from), over the tabs.
struct NoticeSheet: View {
    let model: UnitModel
    @Environment(\.dismiss) private var dismiss

    static func color(_ n: ConnectState) -> RGB {
        switch n {
        case .updateMode: FZ.orange
        case .notFizz: FZ.red
        default: FZ.green
        }
    }

    var body: some View {
        ZStack(alignment: .top) {
            Color.black.ignoresSafeArea()
            if let n = model.notice {
                ScrollView {
                    switch n {
                    case .updateMode: UpdateCard()
                    case .notFizz(let id): NotFizzCard(id: id).contentShape(Rectangle()).onTapGesture { model.rescan() }
                    case .choose: ChooseUnits(model: model)
                    default: EmptyView()
                    }
                }
                .scrollIndicators(.hidden)
            } else {
                Color.clear.onAppear { dismiss() }
            }
        }
        .presentationDetents([.large])
        .presentationDragIndicator(.visible)
        .presentationBackground(Color.black)
        .preferredColorScheme(.dark)
        // (taken, or gone: nothing left to show)
        .onChange(of: model.notice == nil) { _, gone in if gone { dismiss() } }
    }
}
