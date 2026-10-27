// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// BACKUP (mock-up 06): THIS FM-1 (its version, projects, presets, the store's use and meter), BACK UP and PRESETS
// ONLY (not yet: phase I2), the backups kept (none yet).

import SwiftUI
import FizzKit

struct BackupView: View {
    let model: UnitModel

    var body: some View {
        ScreenScroll(refresh: { await model.refresh() }) {
            ThisUnitCard(model: model)
            HStack(spacing: 10) {
                Heading(text: "BACKUPS", count: "0")
            }
            .padding(.top, 20).padding(.bottom, 12).padding(.horizontal, 2)
            EmptyBackups()
        }
        .padding(.horizontal, 16).padding(.top, 4)
    }
}

struct ThisUnitCard: View {
    let model: UnitModel

    var body: some View {
        let v = versionParts(model.unit?.shortVersion ?? "–")
        Card {
            VStack(alignment: .leading, spacing: 16) {
                Heading(text: "THIS FM-1")
                HStack(alignment: .center, spacing: 10) {
                    Text(v.base).fz(50, 800, em: -0.01).foregroundStyle(FZ.text.color).lineLimit(1)
                        .minimumScaleFactor(0.6)
                    if let pre = v.pre {
                        Text(pre).fz(15, 800, em: 0.04).foregroundStyle(.black)
                            .padding(.horizontal, 9).padding(.vertical, 3)
                            .background(RoundedRectangle(cornerRadius: 8).fill(FZ.yellow.color))
                            .fixedSize()
                    }
                }
                HStack(alignment: .top, spacing: 26) {
                    Stat(value: model.status.map { "\($0.projects)" } ?? "–", label: "projects")
                    Stat(value: model.rows.isEmpty ? "–" : "\(model.mine.count)", label: "my presets")
                    Stat(value: model.storeUse.map { "\($0.percent)" } ?? "–", unit: model.storeUse == nil ? nil : " %",
                         label: "store used")
                }
                VStack(spacing: 9) {
                    StoreMeter(use: model.storeUse, height: 26)
                    HStack(spacing: 16) {
                        Legend(color: FZ.cream, text: "MY PRESETS")
                        Legend(color: FZ.green, text: "PROJECTS")
                        Spacer(minLength: 0)
                        if let i = model.storeInfo {
                            Text("\(Int(i.size) / 1024) KB").fz(13, 700).foregroundStyle(FZ.text.color)
                        }
                    }
                }
                HStack(spacing: 10) {
                    PillButton(title: "BACK UP", color: FZ.yellow, height: 64, size: 19, dot: true)
                    PillButton(title: "PRESETS ONLY", ghost: true, height: 64, size: 13, padding: 16)
                        .fixedSize()
                }
                .disabled(true)
                .opacity(0.45)
            }
        }
    }
}

/// A big number over its name (.kv).
struct Stat: View {
    let value: String
    var unit: String?
    let label: String
    var size: CGFloat = 32

    var body: some View {
        VStack(alignment: .leading, spacing: 0) {
            HStack(alignment: .firstTextBaseline, spacing: 0) {
                Text(value).fz(size, 800).foregroundStyle(FZ.text.color)
                if let unit { Text(unit).fz(size * 0.62, 800).foregroundStyle(FZ.text.color) }
            }
            Text(label).fz(13, 700).foregroundStyle(FZ.muted.color)
        }
        .lineLimit(1)
        .fixedSize()
    }
}

struct Legend: View {
    let color: RGB
    let text: String
    var body: some View {
        HStack(spacing: 6) {
            RoundedRectangle(cornerRadius: 3).fill(color.color).frame(width: 10, height: 10)
            Text(text).fz(13, 700).foregroundStyle(FZ.muted.color)
        }
        .fixedSize()
    }
}

/// The store's meter: presets' pages cream (free ones grey), projects' green; it fills in on arrival.
struct StoreMeter: View {
    let use: StoreUse?
    var height: CGFloat = 26
    @Environment(\.fizzSnapshot) private var snapshot
    @State private var shown = false

    var body: some View {
        let parts = use?.parts ?? [0, 0, 0, 0]
        let colors = [FZ.cream.color, RGB(0x45454d).color, FZ.green.color, Color.clear]
        GeometryReader { g in
            HStack(spacing: 0) {
                ForEach(0..<4, id: \.self) { k in
                    Rectangle().fill(colors[k]).frame(width: g.size.width * parts[k])
                }
                Spacer(minLength: 0)
            }
            .scaleEffect(x: shown || snapshot ? 1 : 0.001, anchor: .leading)
        }
        .frame(height: height)
        .background(FZ.well.color)
        .clipShape(Capsule())
        .overlay(Capsule().strokeBorder(FZ.line.color, lineWidth: 1))
        .onAppear { withAnimation(.timingCurve(0.2, 0.8, 0.2, 1, duration: 1.2)) { shown = true } }
    }
}

/// No backups kept yet: an empty cassette.
struct EmptyBackups: View {
    var body: some View {
        IconView(Icons.backup, color: FZ.dim.color, lineWidth: 1.6)
            .frame(width: 56, height: 56)
            .frame(maxWidth: .infinity)
            .frame(height: 96)
            .overlay(RoundedRectangle(cornerRadius: 16).strokeBorder(FZ.dim.color, style: StrokeStyle(lineWidth: 1.5, dash: [6, 5])))
    }
}
