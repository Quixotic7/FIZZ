// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// WRITE TO FM-1 on its screen (mock-up 20), over everything: as a restore, the store as read is kept first (BACKUP),
// then the unit pulls the new store twice (CHECKING: nothing changed yet, CANCEL still there; WRITING: DO NOT UNPLUG, no
// CANCEL). The % counts the pass, the sectors fill (MY PRESETS cream, projects green, the changed ones cyan), the plug
// blinks, the safety backup's card shows once kept.
// FizzAppView shows this one while projectsTab.writing (RESTORE's RestoreOverlay, on the same UnitActions.restoreRun,
// stays hidden meanwhile and shows a failure after).

import SwiftUI
import FizzData
import FizzKit

public struct ProjectsWritingView: View {
    let model: UnitModel
    let write: ProjectsWrite
    /// (the snapshots: a phase and fraction to draw)
    var still: RestoreRun?
    @Environment(\.fizzSnapshot) private var snapshot
    @Environment(\.fizzInsets) private var insets

    public init(model: UnitModel, write: ProjectsWrite, still: RestoreRun? = nil) {
        self.model = model
        self.write = write
        self.still = still
    }

    public var body: some View {
        let run = still ?? model.actions.restoreRun
        let phase = run?.phase ?? .saving
        let pass = run.map { $0.phase == .writing ? $0.fraction * 2 - 1 : $0.phase == .checking ? $0.fraction * 2 : 0 } ?? 0
        let f = min(1, max(0, pass))
        ZStack(alignment: .top) {
            Color.black.ignoresSafeArea()
            BubbleField(count: 10, seed: 17, colors: [FZ.yellow, FZ.cyan, FZ.champagne]).ignoresSafeArea()
            VStack(spacing: 0) {
                TopBar(model: model)
                VStack(spacing: 0) {
                    WriteSteps(phase: phase).padding(.top, 2)
                    HStack(alignment: .firstTextBaseline, spacing: 0) {
                        Text("\(Int((f * 100).rounded(.down)))").fz(120, 800, em: -0.03).monospacedDigit()
                        Text("%").fz(56, 800)
                    }
                    .foregroundStyle(FZ.yellow.color)
                    .shadow(color: FZ.yellow.opacity(0.35), radius: 18)
                    .lineLimit(1)
                    HStack(spacing: 6) {
                        Text(phase == .writing ? "WRITING" : phase == .checking ? "CHECKING" : "SAVING").foregroundStyle(FZ.yellow.color)
                        Text(write.names.joined(separator: " · ")).foregroundStyle(FZ.cyan.color).lineLimit(1)
                    }
                    .font(FizzFont.font(15, 800)).tracking(15 * 0.14)
                    WriteSectors(write: write, fraction: f, writing: phase == .writing).padding(.top, 18)
                    HStack(spacing: 14) {
                        Legend(color: FZ.cream, text: "MY PRESETS")
                        Legend(color: FZ.green, text: "PROJECTS")
                        if !write.names.isEmpty { Legend(color: FZ.cyan, text: write.names.count == 1 ? write.names[0] : "CHANGED") }
                        Spacer(minLength: 0)
                        Animated(still: 0) { t in
                            let now = snapshot ? write.started.addingTimeInterval(9) : Date(timeIntervalSinceReferenceDate: t)
                            Text(mmss(now.timeIntervalSince(write.started))).fz(18, 800).foregroundStyle(FZ.text.color).monospacedDigit()
                        }
                    }
                    .padding(.top, 12)
                    if phase == .writing {
                        Unplug().padding(.top, 16).padding(.bottom, 12)
                    } else {
                        PillButton(title: "CANCEL", ghost: true, height: 52, size: 16) { model.actions.cancelRestore() }
                            .frame(width: 180).padding(.top, 16).padding(.bottom, 12)
                    }
                    if let e = safety { SafetyCard(entry: e) }
                }
                .padding(.horizontal, 16)
                .frame(maxWidth: 560)
                Spacer(minLength: 0)
            }
            .padding(.top, insets.top)
        }
    }

    /// The store as read, kept before this write.
    private var safety: BackupEntry? {
        model.backups.store.entries.first { $0.name.hasPrefix("FIZZ before projects") && $0.date >= write.started.addingTimeInterval(-120) }
    }
}

/// BACKUP → CHECKING → WRITING: done ones green with a tick, the one now yellow and ringing.
struct WriteSteps: View {
    let phase: RestoreRun.Phase
    var body: some View {
        let k = phase == .saving ? 0 : phase == .checking ? 1 : 2
        HStack(alignment: .top, spacing: 0) {
            step(0, k, Icons.backup, "BACKUP")
            line(k > 0 ? (k > 1 ? .done : .run) : .idle)
            step(1, k, ProjIcons.look, "CHECKING")
            line(k > 1 ? .run : .idle)
            step(2, k, Icons.down, "WRITING")
        }
    }

    enum LineState { case idle, run, done }

    private func line(_ s: LineState) -> some View {
        Capsule()
            .fill(s == .done ? AnyShapeStyle(FZ.green.color)
                  : s == .run ? AnyShapeStyle(LinearGradient(colors: [FZ.green.color, FZ.yellow.color], startPoint: .leading, endPoint: .trailing))
                  : AnyShapeStyle(FZ.line.color))
            .frame(height: 3).frame(maxWidth: .infinity)
            .padding(.top, 24)
    }

    private func step(_ i: Int, _ k: Int, _ icon: VectorArt, _ label: String) -> some View {
        let done = i < k, now = i == k
        let c = done ? FZ.green : now ? FZ.yellow : FZ.muted
        return Animated(still: 0.4) { t in
            VStack(spacing: 7) {
                IconView(icon, color: now ? .black : c.color, lineWidth: 2.3, backdrop: now ? FZ.yellow.color : FZ.green.mix(done ? 14 : 0, RGB(0x0d0d11)).color)
                    .frame(width: 24, height: 24)
                    .frame(width: 50, height: 50)
                    .background(Circle().fill(now ? FZ.yellow.color : done ? FZ.green.mix(14, RGB(0x0d0d11)).color : RGB(0x0d0d11).color))
                    .overlay(Circle().strokeBorder(done ? FZ.green.color : now ? FZ.yellow.color : FZ.line.color, lineWidth: 2))
                    .background {
                        if now {
                            let p = (t.truncatingRemainder(dividingBy: 1.3)) / 1.3
                            Circle().strokeBorder(FZ.yellow.opacity(0.75 * (1 - p)), lineWidth: 3).padding(-16 * p)
                        }
                    }
                    .shadow(color: now ? FZ.yellow.color : .clear, radius: 10)
                    .overlay(alignment: .bottomTrailing) {
                        if done {
                            IconView(Icons.tap, color: .black, lineWidth: 2.8, backdrop: FZ.green.color).frame(width: 12, height: 12)
                                .frame(width: 20, height: 20).background(Circle().fill(FZ.green.color))
                                .overlay(Circle().strokeBorder(.black, lineWidth: 3).padding(-3))
                                .offset(x: 5, y: 5)
                        }
                    }
                Text(label).fz(12, 800, em: 0.1).foregroundStyle(c.color)
            }
            .frame(width: 92)
        }
    }
}

/// The store's sectors, 12 a row: filled as the pass goes (MY PRESETS cream, projects green, changed cyan), the one now
/// ringed yellow.
struct WriteSectors: View {
    let write: ProjectsWrite
    let fraction: Double
    var writing = true

    var body: some View {
        let n = max(1, write.sectors), cols = 12, now = min(n - 1, Int(fraction * Double(n)))
        VStack(spacing: 5) {
            ForEach(0..<((n + cols - 1) / cols), id: \.self) { r in
                HStack(spacing: 5) {
                    ForEach(0..<cols, id: \.self) { c in
                        let k = r * cols + c
                        if k < n { cell(k, lit: k < now, now: k == now) } else { Color.clear.frame(height: 22) }
                    }
                }
            }
        }
        .animation(.easeOut(duration: 0.2), value: now)
    }

    private func cell(_ k: Int, lit: Bool, now: Bool) -> some View {
        let chg = write.changed.contains(k)
        let c: RGB = chg ? FZ.cyan : k < write.libSectors ? FZ.cream : FZ.green
        return RoundedRectangle(cornerRadius: 5)
            .fill(lit ? c.color : now ? FZ.yellow.mix(30, RGB(0x18181d)).color : RGB(0x18181d).color)
            .frame(maxWidth: .infinity).frame(height: 22)
            .overlay(RoundedRectangle(cornerRadius: 5).strokeBorder(FZ.yellow.color, lineWidth: now ? 2 : 0))
            .shadow(color: lit && chg ? FZ.cyan.opacity(0.5) : now ? FZ.yellow.color : .clear, radius: now ? 7 : 5)
    }
}

/// DO NOT UNPLUG: the plug blinking, red.
struct Unplug: View {
    var body: some View {
        Animated(still: 0) { t in
            HStack(spacing: 14) {
                IconView(ProjIcons.plug, color: FZ.red.color, lineWidth: 2.4, backdrop: FZ.red.mix(12, RGB(0x0b0b0e)).color)
                    .frame(width: 30, height: 30)
                    .opacity(0.2 + 0.8 * abs(cos(.pi * t / 0.7)))
                Text("DO NOT UNPLUG").fz(20, 800, em: 0.04).foregroundStyle(FZ.red.color)
            }
            .frame(maxWidth: .infinity).frame(height: 68)
            .background(RoundedRectangle(cornerRadius: 18).fill(FZ.red.mix(12, RGB(0x0b0b0e)).color))
            .overlay(RoundedRectangle(cornerRadius: 18).strokeBorder(FZ.red.mix(70, FZ.line).color, lineWidth: 2))
        }
    }
}

/// The safety backup kept before the write (.bk with SAFETY and a tick).
struct SafetyCard: View {
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
                    Text(whenText(entry.date)).fz(18, 800).foregroundStyle(FZ.text.color).lineLimit(1)
                    Text("SAFETY").fz(12, 800, em: 0.06).foregroundStyle(.black)
                        .padding(.horizontal, 7).padding(.vertical, 2)
                        .background(RoundedRectangle(cornerRadius: 6).fill(FZ.yellow.color))
                }
                HStack(spacing: 11) {
                    if let p = entry.projects { Dot(color: FZ.green, text: "\(p.count)") }
                    Dot(color: FZ.cream, text: "\(entry.presets)")
                    Text(sizeText(entry.size)).fz(13, 700).foregroundStyle(FZ.muted.color)
                }
            }
            Spacer(minLength: 4)
            IconView(Icons.tap, color: .black, lineWidth: 3, backdrop: FZ.green.color).frame(width: 22, height: 22)
                .frame(width: 40, height: 40).background(Circle().fill(FZ.green.color))
                .shadow(color: FZ.green.opacity(0.45), radius: 7)
        }
        .padding(.horizontal, 12).frame(height: 70)
        .background(RoundedRectangle(cornerRadius: 16).fill(FZ.raised.color))
        .overlay(RoundedRectangle(cornerRadius: 16).strokeBorder(FZ.line.color, lineWidth: 1.5))
        .transition(.scale(scale: 0.8).combined(with: .opacity))
    }
}
