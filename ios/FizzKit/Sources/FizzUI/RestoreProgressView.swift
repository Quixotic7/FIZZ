// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// A store written to the FM-1 (mock-up 20; manager.js restoreImage, progOpen / progSet): RESTORE and PROJECTS' WRITE
// TO FM-1. Full screen over the tabs: the three steps (BACKUP: the safety backup; CHECKING: the unit reads the image to
// check it, nothing changed yet; WRITING), the step's % counting up, the store's sectors filling in (MY PRESETS cream,
// the projects green, the subject's own cyan), the time, DO NOT UNPLUG while writing (CANCEL before), the safety
// backup kept. Then DONE for a moment, or the failure as a short state with its colour and drawing (FZER 10-15,
// cut off: STORE CLEARED and the safety backup to restore).

import SwiftUI
import FizzData
import FizzKit

/// Shown over everything while UnitActions.restoreRun runs, a moment after it ends well, and while a failure waits.
struct RestoreOverlay: View {
    let model: UnitModel
    @State private var lastPhase: RestoreRun.Phase?
    @State private var done: RestoreRun?

    var body: some View {
        let a = model.actions
        ZStack {
            if let run = a.restoreRun {
                RestoreProgressView(model: model, run: run).transition(.opacity)
            } else if let d = done {
                RestoreProgressView(model: model, run: d, done: true).transition(.opacity)
            } else if let f = a.restoreFailure, f != .cancelled {
                RestoreFailureView(model: model, failure: f).transition(.opacity)
            }
        }
        .animation(.easeInOut(duration: 0.3), value: a.restoreRun == nil)
        .animation(.easeInOut(duration: 0.3), value: done == nil)
        .onChange(of: a.restoreRun) { old, new in
            if let p = new?.phase { lastPhase = p }
            // (ended in the write pass, nothing failed: DONE for a moment)
            guard new == nil, let old, lastPhase == .writing else { return }
            Task {
                try? await Task.sleep(for: .milliseconds(50))
                guard a.restoreFailure == nil else { return }
                done = RestoreRun(phase: .writing, fraction: 1, name: old.name, started: old.started)
                try? await Task.sleep(for: .seconds(1.8))
                done = nil
            }
        }
        .onChange(of: a.restoreFailure) { _, f in
            // (cancelled: nothing changed, nothing to say)
            if f == .cancelled { a.dismissRestoreFailure() }
        }
    }
}

public struct RestoreProgressView: View {
    let model: UnitModel
    let run: RestoreRun
    var done = false
    @Environment(\.fizzSnapshot) private var snapshot
    @Environment(\.fizzInsets) private var insets

    /// run: UnitActions.restoreRun (or one made for a snapshot); done: the store was written (DONE).
    public init(model: UnitModel, run: RestoreRun, done: Bool = false) {
        self.model = model
        self.run = run
        self.done = done
    }

    /// The step's own fraction: the safety backup's, the check pass's, the write pass's.
    private var stepFraction: Double {
        if done { return 1 }
        switch run.phase {
        case .saving: return model.backups.run?.fraction ?? 0
        case .checking: return min(1, max(0, run.fraction * 2))
        case .writing: return min(1, max(0, run.fraction * 2 - 1))
        }
    }

    private var label: (String, String?) {
        if done { return ("WRITTEN", nil) }
        switch run.phase {
        case .saving: return (model.onEmulator ? "SAVING \(EmulatorHost.hereName)" : "SAVING THE FM-1", nil)
        case .checking: return ("CHECKING", nil)
        case .writing: return ("WRITING", model.restoring.subject)
        }
    }

    private var tint: RGB { done ? FZ.green : FZ.yellow }

    public var body: some View {
        ZStack(alignment: .top) {
            Color.black.ignoresSafeArea()
            BubbleField(count: 10, seed: 17, colors: [FZ.yellow, FZ.cyan, FZ.champagne]).ignoresSafeArea()
            VStack(spacing: 0) {
                TopBar(model: model)
                VStack(spacing: 0) {
                    Steps(phase: run.phase, done: done).padding(.top, 2)
                    HStack(alignment: .firstTextBaseline, spacing: 0) {
                        Text("\(Int((stepFraction * 100).rounded(.down)))").fz(120, 800, em: -0.03).monospacedDigit()
                        Text("%").fz(56, 800)
                    }
                    .foregroundStyle(tint.color)
                    .shadow(color: tint.opacity(0.35), radius: 18)
                    .lineLimit(1)
                    .padding(.top, 10)
                    HStack(spacing: 8) {
                        Text(label.0).foregroundStyle(tint.color)
                        if let s = label.1 { Text(s).foregroundStyle(FZ.cyan.color) }
                    }
                    .fz(15, 800, em: 0.14)
                    .padding(.top, 14)
                    RestoreSectors(layout: model.layout ?? .device, phase: run.phase, fraction: stepFraction, done: done,
                                   saving: model.backups.run, highlight: model.restoring.highlight)
                        .padding(.top, 18)
                    HStack(spacing: 16) {
                        Legend(color: FZ.cream, text: "MY PRESETS")
                        Legend(color: FZ.green, text: "PROJECTS")
                        if let s = model.restoring.subject, model.restoring.highlight != nil { Legend(color: FZ.cyan, text: s) }
                        Spacer(minLength: 0)
                        Animated(still: 0) { t in
                            let now = snapshot ? run.started.addingTimeInterval(9) : Date(timeIntervalSinceReferenceDate: t)
                            Text(mmss(now.timeIntervalSince(run.started))).fz(18, 800).foregroundStyle(FZ.text.color).monospacedDigit()
                        }
                    }
                    .padding(.top, 12)
                    if done {
                        Color.clear.frame(height: 16)
                    } else if run.phase == .writing {
                        StateBox(icon: TransferIcons.plug, text: "DO NOT UNPLUG", blink: true, centred: true)
                            .padding(.top, 16).padding(.bottom, 12)
                    } else {
                        PillButton(title: "CANCEL", ghost: true, height: 52, size: 16) { model.actions.cancelRestore() }
                            .frame(width: 180)
                            .padding(.top, 22).padding(.bottom, 12)
                    }
                    if let s = model.restoring.safety, run.phase != .saving, s.date >= run.started.addingTimeInterval(-120) {
                        SafetyRow(entry: s)
                    }
                }
                .padding(.horizontal, 16)
                .frame(maxWidth: 560)
                Spacer(minLength: 0)
            }
            .padding(.top, insets.top)
        }
    }
}

/// BACKUP — CHECKING — WRITING (.wsteps): done ones green with a tick, the one now yellow and ringing.
struct Steps: View {
    let phase: RestoreRun.Phase
    var done = false

    private func state(_ k: Int) -> Int {          // 0 to come, 1 now, 2 done
        if done { return 2 }
        let now = phase == .saving ? 0 : phase == .checking ? 1 : 2
        return k < now ? 2 : k == now ? 1 : 0
    }

    var body: some View {
        HStack(alignment: .top, spacing: 0) {
            step(0, Icons.backup, "BACKUP")
            line(0)
            step(1, TransferIcons.look, "CHECKING")
            line(1)
            step(2, TransferIcons.down, "WRITING")
        }
    }

    private func line(_ k: Int) -> some View {
        let a = state(k), b = state(k + 1)
        return Capsule()
            .fill(a == 2 && b == 2 ? AnyShapeStyle(FZ.green.color)
                  : a == 2 && b == 1 ? AnyShapeStyle(LinearGradient(colors: [FZ.green.color, FZ.yellow.color], startPoint: .leading, endPoint: .trailing))
                  : AnyShapeStyle(FZ.line.color))
            .frame(height: 3)
            .padding(.top, 24)
    }

    private func step(_ k: Int, _ icon: VectorArt, _ title: String) -> some View {
        let s = state(k)
        let c = s == 2 ? FZ.green : s == 1 ? FZ.yellow : FZ.muted
        return VStack(spacing: 7) {
            Animated(still: 0.3) { t in
                let p = (t.truncatingRemainder(dividingBy: 1.3)) / 1.3
                IconView(icon, color: s == 1 ? .black : c.color, lineWidth: 2.3, backdrop: s == 1 ? FZ.yellow.color : RGB(0x0d0d11).color)
                    .frame(width: 24, height: 24)
                    .frame(width: 50, height: 50)
                    .background(Circle().fill(s == 1 ? FZ.yellow.color : s == 2 ? FZ.green.mix(14, RGB(0x0d0d11)).color : RGB(0x0d0d11).color))
                    .overlay(Circle().strokeBorder(s == 0 ? FZ.line.color : c.color, lineWidth: 2))
                    .background(Circle().stroke(FZ.yellow.opacity(s == 1 ? 0.75 * (1 - p) : 0), lineWidth: 16 * p).padding(-8 * p))
                    .shadow(color: s == 1 ? FZ.yellow.color : .clear, radius: 10)
                    .overlay(alignment: .bottomTrailing) {
                        if s == 2 {
                            IconView(TransferIcons.check, color: .black, lineWidth: 2.8, backdrop: FZ.green.color)
                                .frame(width: 12, height: 12)
                                .frame(width: 20, height: 20)
                                .background(Circle().fill(FZ.green.color))
                                .overlay(Circle().stroke(.black, lineWidth: 3))
                                .offset(x: 5, y: 5)
                        }
                    }
            }
            .frame(width: 50, height: 50)
            Text(title).fz(12, 800, em: 0.1).foregroundStyle(c.color)
        }
        .frame(width: 92)
    }
}

/// The store's sectors (.sgrid): the safety backup's as BACKING UP draws them; then those checked (dim) and written
/// (cream, green, the subject's cyan) up to the step's fraction, the next one ringed.
struct RestoreSectors: View {
    let layout: FizzLayout
    let phase: RestoreRun.Phase
    let fraction: Double
    var done = false
    var saving: BackupRun?
    var highlight: Range<Int>?

    var body: some View {
        let n = max(1, layout.sectors), cols = n > 120 ? 16 : 12, h: CGFloat = n > 120 ? 12 : 22, gap: CGFloat = n > 120 ? 3 : 5
        let k = Int(Double(n) * fraction)
        VStack(spacing: gap) {
            ForEach(0..<((n + cols - 1) / cols), id: \.self) { r in
                HStack(spacing: gap) {
                    ForEach(0..<cols, id: \.self) { c in
                        let i = r * cols + c
                        if i < n { cell(i, k, h) } else { Color.clear.frame(height: h) }
                    }
                }
            }
        }
        .animation(.easeOut(duration: 0.25), value: k)
    }

    @ViewBuilder private func cell(_ i: Int, _ k: Int, _ h: CGFloat) -> some View {
        let base = RGB(0x18181d)
        let written = i < layout.libSectors ? FZ.cream : (highlight?.contains(i) == true ? FZ.cyan : FZ.green)
        let (lit, now): (Bool, Bool) = {
            if done { return (true, false) }
            if phase == .saving, let s = saving { return (s.lit.indices.contains(i) && s.lit[i], s.now == i) }
            return (i < k, i == k)
        }()
        let fill: RGB = !lit ? (now ? FZ.yellow.mix(30, base) : base)
            : phase == .checking && !done ? FZ.yellow.mix(22, base) : written
        RoundedRectangle(cornerRadius: h / 4)
            .fill(fill.color)
            .frame(maxWidth: .infinity).frame(height: h)
            .overlay(RoundedRectangle(cornerRadius: h / 4).strokeBorder(FZ.yellow.color, lineWidth: now ? 2 : 0))
            .shadow(color: now ? FZ.yellow.color : lit && written == FZ.cyan && phase == .writing ? FZ.cyan.opacity(0.5) : .clear, radius: now ? 7 : 5)
    }
}

/// The safety backup kept before the write (.bk with SAFETY and a green tick).
struct SafetyRow: View {
    let entry: BackupEntry
    var body: some View {
        HStack(spacing: 12) {
            IconView(Icons.backup, color: FZ.yellow.color, lineWidth: 2.2, backdrop: FZ.yellow.mix(18, FZ.raised).color)
                .frame(width: 24, height: 24)
                .frame(width: 44, height: 44)
                .background(RoundedRectangle(cornerRadius: 13).fill(FZ.yellow.mix(18, FZ.raised).color))
                .overlay(RoundedRectangle(cornerRadius: 13).strokeBorder(FZ.yellow.color, lineWidth: 1.5))
            VStack(alignment: .leading, spacing: 2) {
                HStack(spacing: 8) {
                    Text(whenText(entry.date)).fz(18, 800).foregroundStyle(FZ.text.color)
                    if entry.name.hasPrefix("FIZZ before") {
                        Text("SAFETY").fz(12, 800, em: 0.06).foregroundStyle(.black)
                            .padding(.horizontal, 7).padding(.vertical, 2)
                            .background(RoundedRectangle(cornerRadius: 6).fill(FZ.yellow.color))
                    }
                }
                HStack(spacing: 11) {
                    if let p = entry.projects { Dot(color: FZ.green, text: "\(p.count)") }
                    Dot(color: FZ.cream, text: "\(entry.presets)")
                    Text(sizeText(entry.size)).fz(13, 700).foregroundStyle(FZ.muted.color)
                }
            }
            .lineLimit(1)
            Spacer(minLength: 4)
            IconView(TransferIcons.check, color: .black, lineWidth: 3, backdrop: FZ.green.color)
                .frame(width: 22, height: 22)
                .frame(width: 40, height: 40)
                .background(Circle().fill(FZ.green.color))
                .shadow(color: FZ.green.opacity(0.45), radius: 7)
        }
        .padding(.horizontal, 12)
        .frame(height: 70)
        .background(RoundedRectangle(cornerRadius: 16).fill(FZ.raised.color))
        .overlay(RoundedRectangle(cornerRadius: 16).strokeBorder(FZ.line.color, lineWidth: 1.5))
    }
}

/// A restore that did not end well (manager.js restoreImage's catch): the state big in its colour, its drawing, and
/// the one thing to do: AGAIN (stopped), ADD PRESETS (another store size), RESTORE the safety backup (cleared), OK.
public struct RestoreFailureView: View {
    let model: UnitModel
    let failure: RestoreFailure
    @Environment(\.fizzInsets) private var insets

    public init(model: UnitModel, failure: RestoreFailure) {
        self.model = model
        self.failure = failure
    }

    private var color: RGB {
        switch failure {
        case .cleared, .other, .notSavedFirst: FZ.red
        default: FZ.orange
        }
    }
    private var icon: VectorArt {
        switch failure {
        case .needsUSB, .cleared: TransferIcons.plug
        case .playing: TransferIcons.stop
        default: TransferIcons.warn
        }
    }

    public var body: some View {
        let r = model.restoring, a = model.actions
        ZStack(alignment: .top) {
            Color.black.ignoresSafeArea()
            VStack(spacing: 0) {
                TopBar(model: model)
                VStack(spacing: 22) {
                    Spacer(minLength: 0)
                    Animated(still: 0) { t in
                        IconView(icon, color: color.color, lineWidth: 1.8, backdrop: .black, fill: failure == .playing)
                            .frame(width: 120, height: 120)
                            .shadow(color: color.opacity(0.3 + 0.35 * breathe(t, 2.4)), radius: 24)
                    }
                    .frame(width: 140, height: 140)
                    Text(failure.text).fz(34, 800, em: 0.02).foregroundStyle(color.color)
                        .multilineTextAlignment(.center).lineLimit(3).minimumScaleFactor(0.6)
                    if failure.nothingChanged {
                        NameChip(text: "NOTHING CHANGED", fill: FZ.green)
                    }
                    if case .cleared = failure, let s = r.wayBack { SafetyRow(entry: s) }
                    Spacer(minLength: 0)
                    VStack(spacing: 12) {
                        switch failure {
                        case .playing where r.last != nil:
                            PillButton(title: "AGAIN", color: FZ.yellow, height: 60, size: 18) { Task { await r.again() } }
                        case .otherSize where r.offer != nil:
                            PillButton(title: "ADD PRESETS", color: FZ.cream, height: 60, size: 18) { Task { await r.offerPresetsOnly() } }
                        case .cleared:
                            if let s = r.wayBack {
                                PillButton(title: "RESTORE", color: FZ.yellow, height: 60, size: 18) {
                                    a.dismissRestoreFailure()
                                    Task { await r.present(s) }
                                }
                            }
                        default:
                            EmptyView()
                        }
                        PillButton(title: "OK", ghost: true, height: 52, size: 16) { a.dismissRestoreFailure() }
                            .frame(width: 180)
                    }
                    .padding(.bottom, 40)
                }
                .padding(.horizontal, 24)
                .frame(maxWidth: 560)
            }
            .padding(.top, insets.top)
        }
    }
}
