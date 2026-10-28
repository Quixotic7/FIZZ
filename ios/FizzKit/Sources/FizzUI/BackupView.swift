// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// BACKUP (mock-up 06): THIS FM-1 (its version, projects, presets, the store's use and meter), BACK UP and PRESETS
// ONLY, the last read's speed, and the backups kept: share, swipe to delete, open a file.

import SwiftUI
import FizzData
import FizzKit
import UniformTypeIdentifiers

struct BackupView: View {
    let model: UnitModel
    @State private var picking = false

    var body: some View {
        let b = model.backups, store = b.store
        ScreenScroll(refresh: { await model.refresh(); await store.reload() }) {
            ThisUnitCard(model: model)
            if let s = b.speed { SpeedLine(stats: s).padding(.top, 10).padding(.horizontal, 4) }
            HStack(spacing: 10) {
                Heading(text: "BACKUPS", count: "\(store.entries.count)")
                Spacer(minLength: 0)
                if !store.entries.isEmpty { Text(sizeText(store.totalSize)).fz(15, 800).foregroundStyle(FZ.text.color) }
                Button { picking = true } label: {
                    IconView(Icons.open, color: FZ.yellow.color, lineWidth: 2)
                        .frame(width: 19, height: 19)
                        .frame(width: 36, height: 36)
                        .overlay(Circle().strokeBorder(FZ.yellow.mix(55, FZ.line).color, lineWidth: 1.5))
                        .frame(width: 44, height: 44)
                        .contentShape(Rectangle())
                }
                .buttonStyle(.plain)
            }
            .padding(.top, 16).padding(.bottom, 8).padding(.horizontal, 2)
            if store.entries.isEmpty {
                EmptyBackups()
            } else {
                VStack(spacing: 9) {
                    ForEach(store.entries) { e in
                        BackupRow(entry: e, url: store.url(of: e),
                                  old: e.isOld(unitLayout: model.layout?.name, unitTracks: model.storeTracks),
                                  unitVersion: model.unit?.shortVersion, flash: b.added == e.name,
                                  onDelete: { b.delete(e) }, onShare: { b.shared(e) },
                                  onRestore: b.busy || model.actions.busy ? nil : { Task { await model.restoring.present(e) } })
                            .transition(.asymmetric(insertion: .move(edge: .top).combined(with: .opacity),
                                                    removal: .opacity))
                    }
                }
                .animation(.spring(response: 0.5, dampingFraction: 0.8), value: store.entries.map(\.name))
            }
        }
        .padding(.horizontal, 16).padding(.top, 4)
        .fileImporter(isPresented: $picking, allowedContentTypes: [.fizzBackup, .fizzLibrary, .data],
                      allowsMultipleSelection: true) { r in
            guard case .success(let urls) = r else { return }
            // (one file: RESTORE / ADD PRESETS offered, as the page does)
            Task { if urls.count == 1 { await model.restoring.open(urls[0]) } else { for u in urls { await b.open(u) } } }
        }
    }
}

/// "292 KB", "1.1 MB" (manager.js kb / mb)
func sizeText(_ n: Int) -> String {
    n >= 1_048_576 ? String(format: "%.1f MB", Double(n) / 1_048_576) : "\(Int((Double(n) / 1024).rounded())) KB"
}

/// "Today 14:02", "Yesterday 22:40", "24 Oct 09:15" (manager.js whenText)
func whenText(_ d: Date, now: Date = Date()) -> String {
    let cal = Calendar.current
    let hm = String(format: "%02d:%02d", cal.component(.hour, from: d), cal.component(.minute, from: d))
    let days = cal.dateComponents([.day], from: cal.startOfDay(for: d), to: cal.startOfDay(for: now)).day ?? 99
    if days == 0 { return "Today \(hm)" }
    if days == 1 { return "Yesterday \(hm)" }
    let f = DateFormatter()
    f.locale = Locale(identifier: "en")
    f.dateFormat = "d MMM"
    return "\(f.string(from: d)) \(hm)"
}

/// "m:ss"
func mmss(_ seconds: Double) -> String {
    let s = max(0, Int(seconds))
    return "\(s / 60):\(String(format: "%02d", s % 60))"
}

struct ThisUnitCard: View {
    let model: UnitModel

    var body: some View {
        let v = versionParts(model.unit?.shortVersion ?? "–")
        let b = model.backups
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
                    PillButton(title: "BACK UP", color: FZ.yellow, height: 64, size: 19, dot: true) { b.start(.full) }
                        .disabled(!b.canBackUp(.full))
                        .opacity(b.canBackUp(.full) || b.busy ? 1 : 0.45)
                    PillButton(title: "PRESETS ONLY", ghost: true, height: 64, size: 13, padding: 16) { b.start(.presets) }
                        .fixedSize()
                        .disabled(!b.canBackUp(.presets))
                        .opacity(b.canBackUp(.presets) || b.busy ? 1 : 0.45)
                }
                if let f = b.failure { FailureStrip(failure: f, model: b).transition(.scale(scale: 0.9).combined(with: .opacity)) }
            }
            .animation(.spring(response: 0.4, dampingFraction: 0.75), value: b.failure)
        }
    }
}

/// A backup that did not finish: its state in its colour; AGAIN where trying again helps; a tap clears it.
struct FailureStrip: View {
    let failure: BackupFailure
    let model: BackupModel

    var body: some View {
        HStack(spacing: 10) {
            Circle().fill(failure.color.color).frame(width: 10, height: 10).shadow(color: failure.color.color, radius: 4)
            Text(failure.text).fz(15, 800, em: 0.06).foregroundStyle(failure.color.color).lineLimit(1)
                .minimumScaleFactor(0.7)
            Spacer(minLength: 0)
            if failure.again {
                Button { model.start(model.lastKind) } label: {
                    Text("AGAIN").fz(13, 800, em: 0.08).foregroundStyle(.black)
                        .padding(.horizontal, 14).frame(height: 32)
                        .background(Capsule().fill(failure.color.color))
                        .frame(height: 44)
                        .contentShape(Rectangle())
                }
                .buttonStyle(.plain)
            }
        }
        .padding(.leading, 16).padding(.trailing, 6)
        .frame(height: 44)
        .background(Capsule().fill(failure.color.opacity(0.12)))
        .overlay(Capsule().strokeBorder(failure.color.color, lineWidth: 1.5))
        .contentShape(Capsule())
        .onTapGesture { model.dismissFailure() }
    }
}

/// The last store read (manager.js showSpeed): KB/s, ms a read, the time, sent again, depth.
struct SpeedLine: View {
    let stats: FizzBackupStats

    var body: some View {
        let b = { (s: String) in Text(s).fzText(13, 800).foregroundColor(FZ.text.color) }
        let m = { (s: String) in Text(s).fzText(13, 700).foregroundColor(FZ.muted.color) }
        var t = b(String(format: "%.1f KB/s", stats.kbps)) + m("  ·  ") + b(String(format: "%.1f ms", stats.perFrame)) +
            m(" a read  ·  ") + b(mmss(stats.ms / 1000))
        if stats.resent > 0 { t = t + m("  ·  ") + b("\(stats.resent)") + m(" sent again") }
        t = t + m("  ·  depth ") + b("\(stats.depth)")
        return t.lineLimit(1).minimumScaleFactor(0.8)
    }
}

/// A row of the BACKUPS list (.bk): the cassette, when, projects · presets · size, the version if it differs, share.
/// Swiped left: DELETE (asks first when it was never shared).
struct BackupRow: View {
    let entry: BackupEntry
    let url: URL
    var old = false
    var unitVersion: String?
    var flash = false
    /// (the snapshots: shown swiped)
    var open = false
    var onDelete: () -> Void = {}
    var onShare: () -> Void = {}
    /// RESTORE beside DELETE (I4; nil: not now)
    var onRestore: (() -> Void)?

    @Environment(\.fizzSnapshot) private var snapshot
    @State private var dx: CGFloat = 0
    @State private var drag: CGFloat = 0
    @State private var asking = false
    static let reveal: CGFloat = 92
    private var reveal: CGFloat { onRestore == nil ? Self.reveal : 2 * Self.reveal }

    private var tint: RGB { entry.isFull ? (old ? FZ.orange : FZ.yellow) : FZ.cream }
    private var version: String {
        entry.fizz.replacingOccurrences(of: "-\\d+-g[0-9a-f]+$", with: "", options: .regularExpression)
    }

    var body: some View {
        let x = open ? -reveal : min(0, dx + drag)
        ZStack(alignment: .trailing) {
            HStack(spacing: 6) {
                if let onRestore {
                    Button { withAnimation(.spring(response: 0.35, dampingFraction: 0.8)) { dx = 0 }; onRestore() } label: {
                        VStack(spacing: 4) {
                            IconView(TransferIcons.restore, color: .black, lineWidth: 2.3, backdrop: FZ.yellow.color).frame(width: 22, height: 22)
                            Text("RESTORE").fz(12, 800, em: 0.08).foregroundStyle(.black)
                        }
                        .frame(width: 84).frame(maxHeight: .infinity)
                        .background(RoundedRectangle(cornerRadius: 16).fill(FZ.yellow.color))
                    }
                    .buttonStyle(.plain)
                }
                Button { if entry.saved { onDelete() } else { asking = true } } label: {
                    VStack(spacing: 4) {
                        IconView(Icons.trash, color: .black, lineWidth: 2.3, backdrop: FZ.red.color).frame(width: 22, height: 22)
                        Text("DELETE").fz(12, 800, em: 0.08).foregroundStyle(.black)
                    }
                    .frame(width: 84).frame(maxHeight: .infinity)
                    .background(RoundedRectangle(cornerRadius: 16).fill(FZ.red.color))
                }
                .buttonStyle(.plain)
            }
            .opacity(x < -4 ? 1 : 0)
            card
                .offset(x: x)
                .modifier(SideSwipe(changed: pull, ended: letGo))
                .onTapGesture { if dx != 0 { withAnimation(.spring(response: 0.35, dampingFraction: 0.8)) { dx = 0 } } }
        }
        .frame(height: 70)
        .alert("NEVER SHARED", isPresented: $asking) {
            Button("DELETE", role: .destructive) { onDelete() }
            Button("KEEP", role: .cancel) { withAnimation { dx = 0 } }
        }
    }

    /// The card follows the finger (a pull past the buttons gives way slowly).
    private func pull(_ t: CGFloat) {
        let past = dx + t + reveal
        drag = past < 0 ? t - past + past / 3 : t
    }

    private func letGo(_ t: CGFloat, _ speed: CGFloat) {
        withAnimation(.spring(response: 0.35, dampingFraction: 0.8)) {
            dx = Self.opens(at: dx + t, speed: speed, reveal: reveal) ? -reveal : 0
            drag = 0
        }
    }

    /// Let go at `end` (the card's x) at `speed` (pt/s): a flick opens or shuts it, else past half way opens it.
    static func opens(at end: CGFloat, speed: CGFloat, reveal: CGFloat) -> Bool {
        speed < -600 ? true : speed > 600 ? false : end < -reveal / 2
    }

    private var card: some View {
        HStack(spacing: 12) {
            IconView(Icons.backup, color: tint.color, lineWidth: 2.2, backdrop: tint.mix(18, FZ.raised).color)
                .frame(width: 24, height: 24)
                .frame(width: 44, height: 44)
                .background(RoundedRectangle(cornerRadius: 13).fill(tint.mix(18, FZ.raised).color))
                .overlay(RoundedRectangle(cornerRadius: 13).strokeBorder(tint.color, lineWidth: 1.5))
            VStack(alignment: .leading, spacing: 2) {
                Text(whenText(entry.date)).fz(18, 800).foregroundStyle(FZ.text.color).lineLimit(1)
                HStack(spacing: 11) {
                    if let p = entry.projects { Dot(color: FZ.green, text: "\(p.count)") }
                    Dot(color: FZ.cream, text: "\(entry.presets)")
                    Text(sizeText(entry.size)).fz(13, 700).foregroundStyle(FZ.muted.color)
                    if !version.isEmpty, old || (unitVersion != nil && version != unitVersion) {
                        let c = old ? FZ.orange : FZ.muted
                        Text(version).fz(12, 800).foregroundStyle(c.color)
                            .padding(.horizontal, 6).padding(.vertical, 2)
                            .overlay(RoundedRectangle(cornerRadius: 6).strokeBorder(c.color, lineWidth: 1.5))
                    }
                }
                .lineLimit(1)
            }
            Spacer(minLength: 4)
            ShareIcon(url: url, onShare: onShare)
        }
        .padding(.leading, 12).padding(.trailing, 8)
        .frame(height: 70)
        .background(RoundedRectangle(cornerRadius: 16).fill(FZ.raised.color))
        .overlay(RoundedRectangle(cornerRadius: 16).strokeBorder(FZ.line.color, lineWidth: 1.5))
        .overlay(
            RoundedRectangle(cornerRadius: 16).strokeBorder(FZ.yellow.color, lineWidth: 2)
                .shadow(color: FZ.yellow.color, radius: 10)
                .opacity(flash ? 1 : 0)
                .animation(.easeOut(duration: flash ? 0.25 : 1.2), value: flash)
        )
        .contentShape(RoundedRectangle(cornerRadius: 16))
    }
}

/// A count after a small square of its colour (.what i).
struct Dot: View {
    let color: RGB
    let text: String
    var body: some View {
        HStack(spacing: 5) {
            RoundedRectangle(cornerRadius: 2).fill(color.color).frame(width: 8, height: 8)
            Text(text).fz(14, 800).foregroundStyle(FZ.text.color)
        }
        .fixedSize()
    }
}

/// The share glyph in its ring: the share sheet with the file (Files, AirDrop, Mail).
struct ShareIcon: View {
    let url: URL
    var onShare: () -> Void = {}
    @Environment(\.fizzSnapshot) private var snapshot

    var body: some View {
        if snapshot {
            ring
        } else {
            ShareLink(item: url) { ring }
                .buttonStyle(.plain)
                .simultaneousGesture(TapGesture().onEnded { onShare() })
        }
    }

    private var ring: some View {
        IconView(Icons.share, color: FZ.body.color, lineWidth: 2.2, backdrop: FZ.raised.color)
            .frame(width: 19, height: 19)
            .frame(width: 40, height: 40)
            .overlay(Circle().strokeBorder(FZ.line.color, lineWidth: 1.5))
            .frame(width: 48, height: 48)
            .contentShape(Rectangle())
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
