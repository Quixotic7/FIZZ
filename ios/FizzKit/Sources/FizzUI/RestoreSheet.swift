// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// RESTORE (mock-up 08, manager.js #rs): the backup's cassette, when it was made, FM-1 NOW → BACKUP (projects ·
// presets), its projects and what changes (−4 PRESETS, another package, eight tracks), EVERYTHING ON THE FM-1 IS
// REPLACED, slide to restore. Another package's backup with more than this one holds: the projects and MY PRESETS
// to keep, chosen here. A backup that cannot be restored whole (presets only, another store): ADD PRESETS, the ones
// not on the FM-1 yet, as many as there are places. Over Bluetooth: USB CABLE NEEDED instead of the slide.

import SwiftUI
import FizzData
import FizzKit

public struct RestoreSheet: View {
    let model: UnitModel
    @Environment(\.fizzSnapshot) private var snapshot

    public init(model: UnitModel) { self.model = model }

    public var body: some View {
        let r = model.restoring
        Group {
            if snapshot {
                VStack(alignment: .leading, spacing: 0) {
                    content
                    footer.padding(.top, 16)
                }
                .padding(.horizontal, 18).padding(.top, 2).padding(.bottom, 30)
            } else {
                // what the backup holds scrolls; the warning, the slide (or ADD) and CANCEL stay pinned at the bottom
                ScrollView {
                    content.padding(.horizontal, 18).padding(.top, 22).padding(.bottom, 8)
                }
                .scrollIndicators(.hidden)
                .scrollBounceBehavior(.basedOnSize)
                .safeAreaInset(edge: .bottom, spacing: 0) {
                    footer
                        .padding(.horizontal, 18).padding(.top, 14).padding(.bottom, 12)
                        .background(
                            LinearGradient(stops: [.init(color: Self.sheet.opacity(0), location: 0), .init(color: Self.sheet, location: 0.14)],
                                           startPoint: .top, endPoint: .bottom)
                                .ignoresSafeArea(edges: .bottom)
                        )
                }
            }
        }
        .animation(.spring(response: 0.4, dampingFraction: 0.82), value: r.offer?.name)
        .animation(.spring(response: 0.35, dampingFraction: 0.8), value: r.added)
    }

    /// The sheet's own background (RootView's presentationBackground).
    static let sheet = RGB(0x0d0d11).color

    /// The backup: when, FM-1 NOW → BACKUP, its projects, the choosers.
    @ViewBuilder private var content: some View {
        let r = model.restoring
        let full = r.offer?.full ?? true
        let tint = full ? FZ.yellow : FZ.cream
        VStack(alignment: .leading, spacing: 0) {
            if let o = r.offer {
                head(o, tint: tint)
                diff(o, tint: tint)
                chips(o)
                if r.mustChoose { FitChooser(model: r).padding(.top, 16) }
                if !o.full && r.fresh.count > r.room { AddChooser(model: r, presets: o.backup.presets).padding(.top, 16) }
            } else if r.loading {
                Animated(still: 0) { t in
                    Cassette(color: FZ.yellow, drop: t).frame(width: 64, height: 64)
                }
                .frame(maxWidth: .infinity).frame(height: 200)
            } else {
                StateBox(icon: TransferIcons.warn, text: r.problem ?? "NOT A BACKUP").padding(.top, 20)
            }
        }
        .frame(maxWidth: .infinity, alignment: .leading)
    }

    /// What it costs and the one thing to do: the problem, EVERYTHING ... REPLACED (or USB CABLE NEEDED), the slide or
    /// ADD, CANCEL / DONE.
    @ViewBuilder private var footer: some View {
        let r = model.restoring
        VStack(spacing: 12) {
            if let o = r.offer {
                if let p = r.problem { StateBox(icon: TransferIcons.warn, text: p, color: FZ.orange) }
                if o.full {
                    if r.viaBle {
                        StateBox(icon: TransferIcons.plug, text: "USB CABLE NEEDED", color: FZ.orange)
                    } else {
                        StateBox(icon: TransferIcons.warn, text: "EVERYTHING ON \(model.onEmulator ? EmulatorHost.hereName : "THE FM-1") IS REPLACED")
                        SlideToConfirm("RESTORE", color: FZ.yellow, enabled: r.canRestore) { Task { await r.restore() } }
                            .id(o.name + (r.problem ?? ""))
                            .padding(.top, 4)
                    }
                } else {
                    addAction(r)
                }
            }
            PillButton(title: r.added != nil ? "DONE" : "CANCEL", ghost: true, height: 52, size: 16) { r.close() }
                .frame(width: 180)
                .frame(maxWidth: .infinity)
                .disabled(r.adding)
        }
    }

    private func head(_ o: RestoreModel.Offer, tint: RGB) -> some View {
        HStack(spacing: 14) {
            Animated(still: 0.45) { t in Cassette(color: tint, drop: t) }
                .frame(width: 64, height: 64)
            VStack(alignment: .leading, spacing: 2) {
                Text(o.full ? "RESTORE" : "ADD PRESETS").fz(13, 800, em: 0.12).foregroundStyle(tint.color)
                Text(whenText(o.date)).fz(32, 800).foregroundStyle(FZ.text.color).lineLimit(1).minimumScaleFactor(0.6)
            }
        }
    }

    private func diff(_ o: RestoreModel.Offer, tint: RGB) -> some View {
        let r = model.restoring
        return HStack(spacing: 6) {
            DiffCard(title: model.onEmulator ? "\(EmulatorHost.hereName) NOW" : "FM-1 NOW", value: "\(r.now.projects) · \(r.now.presets)", tint: nil)
            Animated(still: 0) { t in
                Text("→").fz(28, 800).foregroundStyle(tint.color).offset(x: 5 * breathe(t, 1.2))
            }
            .frame(width: 30)
            DiffCard(title: o.full ? "BACKUP" : "AFTER", value: "\(r.then.projects) · \(r.then.presets)", tint: tint)
        }
        .padding(.top, 18).padding(.bottom, 14)
    }

    /// The projects in it, and what changes (manager.js openRestore's chips).
    private func chips(_ o: RestoreModel.Offer) -> some View {
        let r = model.restoring, b = o.backup
        let d = b.presetCount - r.now.presets
        return ChipFlow {
            if o.full && !r.mustChoose {
                ForEach(Array((b.projects ?? []).enumerated()), id: \.offset) { _, p in
                    NameChip(text: p.name.isEmpty ? "PROJECT \(p.slot)" : p.name)
                }
            }
            if o.full && d != 0 && o.plan.fit == nil {
                NameChip(text: "\(d > 0 ? "+" : "−")\(abs(d)) PRESETS", fill: d < 0 ? FZ.orange : FZ.green)
            }
            if o.full && b.tracks == 8 { NameChip(text: "8 TRACKS · PROJECTS START FRESH", fill: FZ.orange) }
            if o.full && o.plan.crossesPackages, let f = o.plan.from, let t = o.plan.to {
                NameChip(text: "\(f.packageName.uppercased()) → \(t.packageName.uppercased())", fill: FZ.cyan)
            }
            if !o.full && b.image != nil { NameChip(text: "ANOTHER STORE · PRESETS ONLY", fill: FZ.orange) }
            if !o.full && r.fresh.count < b.presetCount {
                NameChip(text: "\(b.presetCount - r.fresh.count) HAVE", fill: nil)
            }
        }
    }

    @ViewBuilder private func addAction(_ r: RestoreModel) -> some View {
        if let a = r.added {
            HStack(spacing: 10) {
                Text("+\(a.added)").fz(46, 800).foregroundStyle(FZ.green.color)
                Text("presets").fz(22, 800).foregroundStyle(FZ.muted.color)
                Spacer(minLength: 0)
                if a.same > 0 { NameChip(text: "\(a.same) HAVE") }
                if a.full { NameChip(text: "FULL", fill: FZ.orange) }
                if a.outcome != .ok { NameChip(text: "STOPPED", fill: FZ.red) }
            }
            .transition(.scale(scale: 0.9).combined(with: .opacity))
        } else {
            let job = r.adding ? model.actions.job : nil
            PillButton(title: job.map { "ADDING \($0.done) / \($0.of)" } ?? "ADD \(r.picked.count)", color: FZ.cream, height: 64, size: 19) {
                Task { await r.add() }
            }
            .disabled(!r.canAdd)
            .opacity(r.canAdd || r.adding ? 1 : 0.4)
        }
    }
}

/// FM-1 NOW / BACKUP: projects · presets (.diff .col).
struct DiffCard: View {
    let title: String
    let value: String
    let tint: RGB?

    var body: some View {
        VStack(alignment: .leading, spacing: 2) {
            Text(title).fz(12, 800, em: 0.12).foregroundStyle((tint ?? FZ.muted).color)
            Text(value).fz(34, 800).foregroundStyle((tint ?? FZ.text).color).lineLimit(1).minimumScaleFactor(0.5)
                .contentTransition(.numericText())
            Text("projects · presets").fz(13, 700).foregroundStyle(FZ.muted.color).lineLimit(1)
        }
        .padding(.horizontal, 14).padding(.vertical, 12)
        .frame(maxWidth: .infinity, alignment: .leading)
        .background(RoundedRectangle(cornerRadius: 16).fill(tint.map { $0.mix(7, FZ.raised) }?.color ?? FZ.raised.color))
        .overlay(RoundedRectangle(cornerRadius: 16).strokeBorder((tint ?? FZ.line).color, lineWidth: 1.5))
    }
}

/// The backup's cassette with the cyan arrow dropping into it (fizz-ios.html svg.cas2, @keyframes drop).
struct Cassette: View {
    let color: RGB
    /// the moment, seconds
    let drop: Double

    var body: some View {
        Canvas { ctx, size in
            let s = size.width / 64
            var c = ctx
            c.scaleBy(x: s, y: s)
            let style = StrokeStyle(lineWidth: 2.6, lineCap: .round, lineJoin: .round)
            c.stroke(Path(roundedRect: CGRect(x: 6, y: 22, width: 52, height: 34), cornerRadius: 8), with: .color(color.color), style: style)
            for x in [22.0, 42.0] { c.stroke(Path(ellipseIn: CGRect(x: x - 6, y: 33, width: 12, height: 12)), with: .color(color.color), style: style) }
            let p = (drop.truncatingRemainder(dividingBy: 1.5)) / 1.5
            let y = -6 + 14 * p * p, op = p < 0.7 ? 1 : (1 - p) / 0.3
            let a = SVGPath.parse("M32 2v14M26 10l6 6 6-6").applying(CGAffineTransform(translationX: 0, y: y))
            c.stroke(a, with: .color(FZ.cyan.opacity(op)), style: style)
        }
    }
}

/// A chip to tick (a project or a preset kept, a preset added).
struct PickChip: View {
    let text: String
    var dot: RGB?
    let on: Bool
    var color: RGB = FZ.cyan
    let action: () -> Void

    var body: some View {
        Button(action: action) {
            HStack(spacing: 6) {
                if let dot { RoundedRectangle(cornerRadius: 2).fill(on ? Color.black : dot.color).frame(width: 8, height: 8) }
                Text(text).fz(13, 800, em: 0.02).lineLimit(1)
            }
            .foregroundStyle(on ? Color.black : FZ.muted.color)
            .padding(.horizontal, 11).frame(height: 32)
            .background(Capsule().fill(on ? color.color : .clear))
            .overlay(Capsule().strokeBorder(on ? color.color : FZ.line.color, lineWidth: 1.5))
            .fixedSize()
        }
        .buttonStyle(.plain)
    }
}

/// A heading with its count against the most it may be (red when over).
struct CountHeading: View {
    let title: String
    let n: Int
    let max: Int
    var body: some View {
        HStack(spacing: 8) {
            Text(title).fz(13, 800, em: 0.12).foregroundStyle(FZ.muted.color)
            Text("\(n) / \(max)").fz(15, 800).foregroundStyle((n > max ? FZ.red : FZ.text).color).contentTransition(.numericText())
        }
    }
}

/// Another package's backup that holds more than this one: the projects and MY PRESETS kept (rsFit).
struct FitChooser: View {
    let model: RestoreModel

    var body: some View {
        if let o = model.offer, let fit = o.plan.fit, let c = model.choice, let to = o.plan.to {
            VStack(alignment: .leading, spacing: 10) {
                CountHeading(title: "PROJECTS", n: c.projects.count, max: to.nproj)
                ChipFlow {
                    ForEach(fit.projects, id: \.p) { p in
                        PickChip(text: "\(p.p + 1) \(p.name.isEmpty ? "PROJECT" : p.name)", on: c.projects.contains(p.p)) {
                            Task { await model.toggleProject(p.p) }
                        }
                    }
                }
                CountHeading(title: "MY PRESETS", n: c.presets.count, max: to.nlib).padding(.top, 6)
                PickList {
                    ChipFlow(spacing: 5, lineSpacing: 5) {
                        ForEach(fit.presets, id: \.i) { x in
                            PickChip(text: x.name, dot: FizzTag.tint(mask: x.tags), on: c.presets.contains(x.i), color: FZ.cream) {
                                Task { await model.togglePreset(x.i) }
                            }
                        }
                    }
                }
                if let l = model.left, !(l.leftProjects.isEmpty && l.leftPresets.isEmpty) {
                    HStack(spacing: 7) {
                        if !l.leftProjects.isEmpty { NameChip(text: "−\(l.leftProjects.count) PROJECTS", fill: FZ.orange) }
                        if !l.leftPresets.isEmpty { NameChip(text: "−\(l.leftPresets.count) PRESETS", fill: FZ.orange) }
                    }
                }
            }
        }
    }
}

/// ADD: more new presets than places: the ones to add (rsPresets).
struct AddChooser: View {
    let model: RestoreModel
    let presets: [FizzSound]

    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            CountHeading(title: "ADD", n: model.picked.count, max: model.room)
            PickList {
                ChipFlow(spacing: 5, lineSpacing: 5) {
                    ForEach(model.fresh, id: \.self) { i in
                        let s = presets[i], tag = s.tags.first.flatMap { n in FizzTag.allCases.first { $0.name == n } }
                        PickChip(text: s.name, dot: tag?.tint ?? FZ.cream, on: model.picked.contains(i), color: FZ.cream) {
                            model.togglePick(i)
                        }
                    }
                }
            }
        }
    }
}

/// A list of chips that scrolls within a few rows (flat in a snapshot).
struct PickList<Content: View>: View {
    @Environment(\.fizzSnapshot) private var snapshot
    @ViewBuilder let content: Content
    var body: some View {
        if snapshot {
            content.frame(maxHeight: 150, alignment: .top).clipped()
        } else {
            ScrollView { content }.frame(maxHeight: 170).scrollIndicators(.hidden)
        }
    }
}
