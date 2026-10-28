// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// LOG (PLAN §1.1): the app's last 400 lines, newest at the bottom, to copy or share with a bug report. Opened from
// the PORTS sheet (the unit pill's long press opens PORTS).

import SwiftUI
#if canImport(UIKit)
import UIKit
#elseif canImport(AppKit)
import AppKit
#endif

struct LogSheet: View {
    let model: UnitModel
    @Environment(\.fizzSnapshot) private var snapshot
    @State private var copied = false

    private var text: String { model.log.suffix(400).joined(separator: "\n") }

    var body: some View {
        VStack(alignment: .leading, spacing: 14) {
            HStack(spacing: 10) {
                Text("LOG").fz(24, 800, em: 0.08).foregroundStyle(FZ.text.color)
                Text("\(min(400, model.log.count))").fz(15, 800).foregroundStyle(FZ.muted.color)
                Spacer()
                Button(action: copy) {
                    Text(copied ? "COPIED" : "COPY").fz(13, 800, em: 0.08).foregroundStyle(.black)
                        .padding(.horizontal, 16).frame(height: 36)
                        .background(Capsule().fill((copied ? FZ.green : FZ.cyan).color))
                }
                .buttonStyle(.plain)
                ShareLink(item: text) {
                    IconView(Icons.share, color: FZ.text.color, lineWidth: 2.2).frame(width: 20, height: 20)
                        .frame(width: 40, height: 40)
                        .overlay(Circle().strokeBorder(FZ.line.color, lineWidth: 2))
                }
                .buttonStyle(.plain)
            }
            if snapshot {
                VStack(alignment: .leading, spacing: 5) {
                    ForEach(Array(model.log.suffix(40).enumerated()), id: \.offset) { _, l in
                        Text(l).font(.system(size: 12, design: .monospaced)).foregroundStyle(FZ.body.color)
                    }
                    Spacer(minLength: 0)
                }
            } else {
                ScrollViewReader { sv in
                    ScrollView {
                        LazyVStack(alignment: .leading, spacing: 5) {
                            ForEach(Array(model.log.suffix(400).enumerated()), id: \.offset) { k, l in
                                Text(l).font(.system(size: 12, design: .monospaced)).foregroundStyle(FZ.body.color)
                                    .textSelection(.enabled).id(k)
                            }
                        }
                    }
                    .onAppear { sv.scrollTo(min(400, model.log.count) - 1, anchor: .bottom) }
                }
            }
        }
        .padding(20)
        .background(Color.black)
        .preferredColorScheme(.dark)
    }

    private func copy() {
        #if canImport(UIKit)
        UIPasteboard.general.string = text
        #elseif canImport(AppKit)
        NSPasteboard.general.clearContents()
        NSPasteboard.general.setString(text, forType: .string)
        #endif
        withAnimation { copied = true }
    }
}

/// The LOG sheet by itself (fizzshots).
public struct LogSheetShot: View {
    let model: UnitModel
    public init(model: UnitModel) { self.model = model }
    public var body: some View { LogSheet(model: model) }
}
