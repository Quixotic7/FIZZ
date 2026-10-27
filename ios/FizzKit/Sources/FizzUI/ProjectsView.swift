// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// PROJECTS (mock-up 14), from PROJECTS only: the open project's name and pages, the 8 slots (the open one glowing,
// the demo marked, the empty dashed). The patterns need the store read (phase I6).

import SwiftUI
import FizzKit

struct ProjectsView: View {
    let model: UnitModel

    var body: some View {
        let list = model.projects?.list ?? []
        let cur = list.first { $0.current }
        ScreenScroll(refresh: { await model.refresh() }) {
            HStack(alignment: .lastTextBaseline, spacing: 12) {
                Text(cur?.name ?? "–").fz(52, 800, em: -0.01).foregroundStyle(FZ.cyan.color)
                    .shadow(color: FZ.cyan.opacity(0.45), radius: 13)
                    .lineLimit(1).minimumScaleFactor(0.5)
                if let cur {
                    HStack(alignment: .firstTextBaseline, spacing: 5) {
                        Text("\(cur.pages)").fz(22, 800).foregroundStyle(FZ.text.color)
                        Text("pages").fz(15, 800).foregroundStyle(FZ.muted.color)
                    }
                    .fixedSize()
                }
            }
            .padding(.top, 2).padding(.bottom, 14)
            Grid(horizontalSpacing: 8, verticalSpacing: 8) {
                ForEach(0..<2, id: \.self) { r in
                    GridRow {
                        ForEach(0..<4, id: \.self) { c in
                            let k = r * 4 + c
                            ProjectTile(slot: k + 1, project: k < list.count ? list[k] : nil,
                                        most: max(64, list.map(\.pages).max() ?? 0))
                        }
                    }
                }
            }
            .frame(maxWidth: 560)
        }
        .padding(.horizontal, 16).padding(.top, 4)
    }
}

struct ProjectTile: View {
    let slot: Int
    let project: FizzProject?
    let most: Int

    var body: some View {
        let there = project?.there == true
        let cur = project?.current == true, demo = project?.demo == true
        let badge = cur ? FZ.cyan : demo ? FZ.champagne : there ? FZ.green : FZ.track
        Animated(still: 0.9) { t in
            VStack(alignment: .leading, spacing: 0) {
                Text("\(slot)").fz(15, 800).foregroundStyle(there ? Color.black : RGB(0x5d5d66).color)
                    .frame(width: 26, height: 26)
                    .background(RoundedRectangle(cornerRadius: 8).fill(badge.color))
                if there, let p = project {
                    Spacer(minLength: 0)
                    Text(p.name).fz(14, 800, em: 0.01).foregroundStyle(FZ.text.color).lineLimit(2)
                        .fixedSize(horizontal: false, vertical: true)
                    if demo {
                        Text("DEMO").fz(12, 800, em: 0.06).foregroundStyle(FZ.champagne.color)
                            .padding(.horizontal, 5).frame(height: 16)
                            .overlay(RoundedRectangle(cornerRadius: 6).strokeBorder(FZ.champagne.opacity(0.7),
                                                                                     style: StrokeStyle(lineWidth: 1.5, dash: [3, 2])))
                            .padding(.top, 5)
                    } else {
                        GeometryReader { g in
                            Capsule().fill(FZ.track.color)
                                .overlay(alignment: .leading) {
                                    Capsule().fill((cur ? FZ.cyan : FZ.green).color)
                                        .frame(width: max(5, g.size.width * min(1, Double(p.pages) / Double(most))))
                                }
                        }
                        .frame(height: 5).padding(.top, 7)
                    }
                } else {
                    Text("+").fz(30, 500).foregroundStyle(RGB(0x3a3a44).color)
                        .frame(maxWidth: .infinity, maxHeight: .infinity)
                }
            }
            .padding(.top, 9).padding(.bottom, 9).padding(.leading, 10).padding(.trailing, 9)
            .frame(maxWidth: .infinity, alignment: .leading)
            .frame(height: 100)
            .background(RoundedRectangle(cornerRadius: 18).fill(cur ? FZ.cyan.mix(14, FZ.raised).color : there ? FZ.raised.color : .clear))
            .overlay {
                if there {
                    RoundedRectangle(cornerRadius: 18).strokeBorder(cur ? FZ.cyan.color : FZ.line.color, lineWidth: cur ? 2 : 1.5)
                } else {
                    RoundedRectangle(cornerRadius: 18).strokeBorder(FZ.dim.color, style: StrokeStyle(lineWidth: 1.5, dash: [5, 4]))
                }
            }
            .shadow(color: cur ? FZ.cyan.opacity(0.45 + 0.3 * breathe(t, 2.2)) : .clear, radius: cur ? 13 + 7 * breathe(t, 2.2) : 0)
        }
    }
}
