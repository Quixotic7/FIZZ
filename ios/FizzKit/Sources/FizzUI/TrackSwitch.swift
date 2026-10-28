// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// The TRACK switch (live.js, fizz-ios.html .livesw; phase I5), for every screen that places it: the pink pill (off:
// the tracks are left alone; on: EDIT edits the FM-1's track, LIBRARY and IMPORT audition on it), its number the track
// followed (a tap on it: SELECTED or 1..6), its ring pulsing until first used (the "seen" flag); and the KEEP / CANCEL
// bar while another sound is heard on a track.

import SwiftUI
import FizzKit

/// The "seen" flag of the TRACK switch's hint (live.js HINT_KEY).
enum TrackHint {
    static let key = "fizz-manager-track-seen"
    static var seen: Bool {
        get { UserDefaults.standard.bool(forKey: key) }
        set { UserDefaults.standard.set(newValue, forKey: key) }
    }
}

/// The TRACK pill.
public struct TrackPill: View {
    let model: UnitModel
    /// the iPad's page list: as wide as it is
    var wide = false
    @State private var seen = TrackHint.seen
    @State private var shake = 0.0
    @Environment(\.fizzSnapshot) private var snapshot

    public init(model: UnitModel, wide: Bool = false) {
        self.model = model
        self.wide = wide
    }

    /// The tabs whose top bar shows it (WIRE AT MERGE: .library (I3) and IMPORT's sheet (I4) place it too).
    public static func shows(on tab: FizzTab) -> Bool { [.edit].contains(tab) }

    public var body: some View {
        let a = model.actions, on = a.trackOn, cap = a.trackCapable
        let n = model.editor.trackShown
        HStack(spacing: 8) {
            Button {
                Task {
                    seen = true
                    TrackHint.seen = true
                    await a.setTrackOn(!on)
                    await model.editor.trackSwitched()
                }
            } label: {
                HStack(spacing: 8) {
                    Capsule().fill(on ? FZ.pink.color : FZ.track.color).frame(width: 32, height: 20)
                        .overlay(alignment: on ? .trailing : .leading) {
                            Circle().fill(on ? Color.white : FZ.muted.color).frame(width: 14, height: 14).padding(3)
                        }
                    Text("TRACK").fz(13, 800, em: 0.1).foregroundStyle(FZ.text.color)
                    if wide { Spacer(minLength: 0) }
                }
                .contentShape(Rectangle())
            }
            .buttonStyle(.plain)
            .disabled(!cap)
            numberMenu(n: n, on: on)
        }
        .padding(.leading, 8).padding(.trailing, 5)
        .frame(height: wide ? 48 : 40)
        .frame(maxWidth: wide ? .infinity : nil)
        .background(Capsule().fill(FZ.pink.opacity(on ? 0.14 : 0.05)))
        .overlay(Capsule().strokeBorder(on ? FZ.pink.color : FZ.pink.mix(45, FZ.line).color, lineWidth: 2))
        .shadow(color: on ? FZ.pink.opacity(0.3) : .clear, radius: 8)
        .background {
            if !seen && cap && !on {
                Animated(still: 0.5) { t in
                    let f = (t / 1.6).truncatingRemainder(dividingBy: 1)
                    Capsule().stroke(FZ.pink.opacity(0.55 * (1 - f)), lineWidth: 2 + 10 * f).padding(-6 * f)
                }
            }
        }
        .opacity(cap ? 1 : 0.45)
        .offset(x: 6 * sin(shake * .pi * 6) * (1 - shake))
        .onChange(of: model.editor.nudges) { _, _ in
            shake = 0
            withAnimation(.linear(duration: 0.5)) { shake = 1 }
        }
        .fixedSize(horizontal: !wide, vertical: false)
    }

    /// The followed track's number; a tap: SELECTED or 1..6 (live.js strip).
    @ViewBuilder func numberMenu(n: Int, on: Bool) -> some View {
        let badge = Text(n >= 0 ? "\(n + 1)" : "–").fz(17, 800).foregroundStyle(on ? FZ.pink.color : FZ.text.color)
            .frame(minWidth: 30, minHeight: 30).padding(.horizontal, 2)
            .background(Capsule().fill(on ? Color.white : FZ.track.color))
        if snapshot {
            badge
        } else {
            Menu {
                let k = model.actions.trackSlot
                Button { Task { await model.editor.follow(slot: 0) } } label: {
                    Label("SELECTED", systemImage: k == 0 ? "checkmark" : "scope")
                }
                ForEach(1...6, id: \.self) { t in
                    Button { Task { await model.editor.follow(slot: t) } } label: {
                        if k == t { Label("TRACK \(t)", systemImage: "checkmark") } else { Text("TRACK \(t)") }
                    }
                }
            } label: { badge }
            .menuStyle(.button)
            .buttonStyle(.plain)
            .disabled(!model.actions.trackCapable)
        }
    }
}

/// KEEP / CANCEL: a track plays another sound (a preset picked, an IMPORT row); KEEP leaves it, CANCEL puts the
/// track's own sound back.
public struct AuditionBar: View {
    let model: UnitModel
    @State private var working = false

    public init(model: UnitModel) { self.model = model }

    public var body: some View {
        if let p = model.actions.pending {
            HStack(spacing: 10) {
                Text("\(p.track + 1)").fz(15, 800).foregroundStyle(.white)
                    .frame(width: 26, height: 26).background(Circle().fill(FZ.pink.color))
                Text("←").fz(15, 800).foregroundStyle(FZ.muted.color)
                Text(p.name).fz(16, 800, em: 0.03).foregroundStyle(FZ.text.color).lineLimit(1)
                Spacer(minLength: 4)
                Button {
                    Task { await model.actions.keepAudition() }
                } label: {
                    Text("KEEP").fz(13, 800, em: 0.08).foregroundStyle(.white)
                        .padding(.horizontal, 16).frame(height: 38)
                        .background(Capsule().fill(FZ.pink.color))
                        .shadow(color: FZ.pink.opacity(0.45), radius: 8)
                        .fixedSize()
                }
                .buttonStyle(.plain)
                Button {
                    guard !working else { return }
                    working = true
                    Task {
                        if await model.actions.cancelAudition() { await model.editor.auditionEnded() }
                        working = false
                    }
                } label: {
                    Text("CANCEL").fz(13, 800, em: 0.08).foregroundStyle(FZ.text.color)
                        .padding(.horizontal, 14).frame(height: 38)
                        .overlay(Capsule().strokeBorder(FZ.line.color, lineWidth: 1.5))
                        .opacity(working ? 0.5 : 1)
                        .fixedSize()
                }
                .buttonStyle(.plain)
            }
            .padding(.leading, 16).padding(.trailing, 7)
            .frame(height: 52)
            .background(Capsule().fill(FZ.pink.mix(10, RGB(0x111115)).color))
            .overlay(Capsule().strokeBorder(FZ.pink.color, lineWidth: 1.5))
            .shadow(color: .black.opacity(0.7), radius: 14, y: 8)
            .transition(.move(edge: .bottom).combined(with: .opacity))
        }
    }
}

/// The top bar of the tabs that place the TRACK switch (mock-ups 09, 11): the logo, the pill, the unit.
struct TrackTopBar: View {
    let model: UnitModel
    var body: some View {
        HStack(spacing: 10) {
            BubbleLogo().frame(width: 32, height: 32)
            Spacer(minLength: 0)
            TrackPill(model: model)
            if let u = model.unit { UnitPill(info: u) }
        }
        .frame(height: 56)
        .padding(.horizontal, 16)
        .turnDim(model.editor, [])
    }
}
