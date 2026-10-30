// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// EDIT's borrow (mock-up 37): a sound opened in EDIT is heard on the track at once, no KEEP / CANCEL bar. The top
// bar's TRACK n chip says where (the track's colour, the speaker's waves pulsing while borrowed; a tap picks another
// track), KEEP ON Tn on the place line leaves it there. The track's own sound comes back when EDIT is left (another
// tab, the app in the background), the unit changes or a restore starts (EditorModel.hide / unitGoes).

import SwiftUI
import FizzKit

// MARK: - The app's hooks (RootView)

extension EditorModel {
    /// The tab changed: EDIT left (its borrow undone), or EDIT back from the phone's IMPORT sheet (its view stayed).
    public func tabChanged(from old: FizzTab, to new: FizzTab) {
        if old == .edit && new != .edit { hide() }
        if new == .edit && old == .importer { Task { await show() } }
    }

    /// The app in the background: as EDIT left; back to the front on EDIT: heard again.
    public func scene(_ p: ScenePhase, tab: FizzTab) {
        if p == .background { hide() }
        if p == .active && tab == .edit { Task { await show() } }
    }
}

/// `-borrow` (the demo unit only): EDIT opens on MY PRESETS 3 borrowed on TRACK 2, as mock-up 37 (simulator
/// screenshots).
@MainActor enum BorrowLaunch {
    static var pending = ProcessInfo.processInfo.arguments.contains("-borrow")

    static func apply(_ model: UnitModel) async {
        guard pending, model.source is DemoLinkSource else { return }
        pending = false
        for _ in 0..<500 where !model.dumpsLoaded { try? await Task.sleep(for: .milliseconds(20)) }
        guard let r = model.mine.first(where: { $0.slot == 3 }) else { return }
        model.actions.trackSlot = 2
        model.editor.page = .openv
        model.editTarget = .preset(r)
    }
}

// MARK: - The TRACK n chip

/// EDIT's TRACK n chip (in place of the TRACK switch): where the sound is heard; a tap picks another track, or (a
/// sound opened here) the track's own sound again.
struct TrackChip: View {
    let model: UnitModel
    /// the iPad's page list: as wide as it is
    var wide = false
    @Environment(\.fizzSnapshot) private var snapshot

    var body: some View {
        if snapshot { chip } else {
            Menu { menu } label: { chip.contentShape(Capsule()) }
                .menuStyle(.button)
                .buttonStyle(.plain)
                .disabled(!model.actions.trackCapable)
        }
    }

    private var heard: EditHeard { model.editor.heard }

    private var tint: RGB {
        switch heard {
        case .borrowed(let t), .track(let t): trackTints[max(0, min(5, t))]
        case .silent: FZ.muted
        }
    }

    private var chip: some View {
        let h = heard, c = tint
        let n: Int? = switch h { case .borrowed(let t), .track(let t): t + 1; case .silent: nil }
        return HStack(spacing: 8) {
            SpeakerWaves(color: c, pulsing: h.isBorrowed, muted: n == nil).frame(width: 22, height: 22)
            Text(n == nil ? "SILENT" : "TRACK").fz(13, 800, em: 0.1).foregroundStyle(n == nil ? FZ.muted.color : FZ.text.color)
            if wide { Spacer(minLength: 0) }
            if let n {
                Text("\(n)").fz(17, 800).foregroundStyle(.black)
                    .frame(minWidth: 30, minHeight: 30)
                    .background(Capsule().fill(c.color))
                    .contentTransition(.numericText())
            }
        }
        .padding(.leading, 10).padding(.trailing, n == nil ? 14 : 5)
        .frame(height: wide ? 48 : 40)
        .frame(maxWidth: wide ? .infinity : nil)
        .background(Capsule().fill(c.opacity(n == nil ? 0.05 : 0.14)))
        .overlay(Capsule().strokeBorder(n == nil ? FZ.line.color : c.color, lineWidth: 2))
        .shadow(color: n == nil ? .clear : c.opacity(0.3), radius: 8)
        .frame(minHeight: 44)
        .fixedSize(horizontal: !wide, vertical: false)
        .animation(.spring(response: 0.35, dampingFraction: 0.8), value: h)
    }

    @ViewBuilder private var menu: some View {
        let e = model.editor, k = model.actions.trackSlot
        if e.opened != nil, case .borrowed(let t) = heard {
            Button { Task { await e.ownSound() } } label: {
                Label("TRACK \(t + 1): ITS OWN SOUND", systemImage: "arrow.uturn.backward")
            }
            Divider()
        }
        Button { Task { await e.follow(slot: 0) } } label: {
            Label("SELECTED", systemImage: k == 0 ? "checkmark" : "scope")
        }
        ForEach(1...6, id: \.self) { t in
            Button { Task { await e.follow(slot: t) } } label: {
                if k == t { Label("TRACK \(t)", systemImage: "checkmark") } else { Text("TRACK \(t)") }
            }
        }
    }
}

extension EditHeard {
    var isBorrowed: Bool { if case .borrowed = self { true } else { false } }
}

/// A speaker, its two waves pulsing one after the other (borrowed), or struck through (not heard).
struct SpeakerWaves: View {
    let color: RGB
    var pulsing = false
    var muted = false

    var body: some View {
        AnimatedIf(on: pulsing, still: 0.4) { t in
            Canvas { g, size in
                let s = size.width / 24
                func p(_ x: Double, _ y: Double) -> CGPoint { CGPoint(x: x * s, y: y * s) }
                let st = StrokeStyle(lineWidth: 2.4 * s, lineCap: .round, lineJoin: .round)
                var body = Path()
                body.move(to: p(3, 9)); body.addLine(to: p(7, 9)); body.addLine(to: p(12, 4.5))
                body.addLine(to: p(12, 19.5)); body.addLine(to: p(7, 15)); body.addLine(to: p(3, 15)); body.closeSubpath()
                g.stroke(body, with: .color(color.color), style: st)
                if muted {
                    var x = Path()
                    x.move(to: p(16, 9)); x.addLine(to: p(22, 15)); x.move(to: p(22, 9)); x.addLine(to: p(16, 15))
                    g.stroke(x, with: .color(color.color), style: st)
                    return
                }
                for (k, r) in [(0, 4.0), (1, 8.0)] {
                    let a = pulsing ? 0.25 + 0.75 * breathe(t - 0.25 * Double(k), 1) : 1
                    var w = Path()
                    w.addArc(center: p(12, 12), radius: r * s, startAngle: .degrees(-45), endAngle: .degrees(45), clockwise: false)
                    g.stroke(w, with: .color(color.color.opacity(a)), style: st)
                }
            }
        }
    }
}

// MARK: - KEEP ON Tn

/// KEEP ON Tn: the borrowed sound stays on the track.
struct KeepOnButton: View {
    let model: UnitModel
    @State private var working = false

    var body: some View {
        if case .borrowed(let t) = model.editor.heard, model.editor.opened != nil {
            let c = trackTints[max(0, min(5, t))]
            Button {
                guard !working else { return }
                working = true
                Task {
                    await model.editor.keepOn()
                    working = false
                }
            } label: {
                HStack(spacing: 6) {
                    Image(systemName: "pin").font(.system(size: 13, weight: .bold))
                    Text("KEEP ON T\(t + 1)").fz(12, 800, em: 0.08)
                }
                .foregroundStyle(c.color)
                .padding(.leading, 9).padding(.trailing, 11)
                .frame(height: 32)
                .overlay(Capsule().strokeBorder(c.opacity(0.7), lineWidth: 1.5))
                .frame(minHeight: 44)
                .contentShape(Rectangle())
                .opacity(working ? 0.5 : 1)
                .fixedSize()
            }
            .buttonStyle(.plain)
            .transition(.scale(scale: 0.8).combined(with: .opacity))
        }
    }
}
