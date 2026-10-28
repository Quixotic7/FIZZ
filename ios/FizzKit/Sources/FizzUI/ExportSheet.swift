// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// EXPORT (mock-up 21, exporter.js mountExport): the sounds, then a card a format: FIZZ's own (.fizzsnd for one sound,
// a .fizzlib for several), Digitone .dnsnd, Digitone II .dn2pst / .dn2snd (both EXPERIMENTAL), each with its carry bar
// (10 less what that format drops or changes for these sounds); the card picked opens on what does not carry. The
// file's name (several Digitone sounds: a ZIP), then SHARE in the format's colour: the share sheet (Files, AirDrop,
// Mail). FIZZ is picked first. MIDI sounds: no Digitone file (their row "MIDI SOUND · not written").

import SwiftUI
import FizzData
import FizzKit

/// The sheet LIBRARY opens for one preset, a selection or MY PRESETS (phase I3 places the buttons).
public struct ExportSheet: View {
    let sounds: [FizzSound]
    var onClose: (() -> Void)?
    @State private var pick: FizzExportKind = .fizz
    @State private var dn2: FizzExportKind = .dn2pst
    @State private var carry: [FizzExportKind: FizzCarry] = [:]
    @State private var file: (kind: FizzExportKind, url: URL, name: String, skipped: Int)?
    @State private var failed = false
    @Environment(\.fizzSnapshot) private var snapshot
    @Environment(\.fizzInsets) private var insets
    @Environment(\.dismiss) private var dismiss

    /// sounds: js/library.js's shape (a DUMP read with FizzDataEngine.soundFromStoreFrame); onClose: the X (nil: the
    /// sheet's own dismiss).
    public init(sounds: [FizzSound], onClose: (() -> Void)? = nil) {
        self.sounds = sounds
        self.onClose = onClose
    }

    /// The sheet with its carry bars and file already made, `pick` picked (the snapshots: a still image runs no task).
    @MainActor public static func prepared(sounds: [FizzSound], pick: FizzExportKind = .fizz) async -> ExportSheet {
        var v = ExportSheet(sounds: sounds)
        let k: FizzExportKind = pick == .dn2snd ? .dn2pst : pick
        v._pick = State(initialValue: k)
        v._dn2 = State(initialValue: pick == .dn2snd ? .dn2snd : .dn2pst)
        guard let e = try? FizzDataShared.engine() else { return v }
        var c: [FizzExportKind: FizzCarry] = [:]
        for x in [FizzExportKind.fizz, .dnsnd, .dn2pst] { c[x] = try? await e.exportCarry(sounds, kind: x) }
        v._carry = State(initialValue: c)
        if let f = try? await e.exportFile(sounds, kind: pick) {
            v._file = State(initialValue: (pick, FileManager.default.temporaryDirectory.appendingPathComponent(f.name), f.name, f.skipped.count))
        }
        return v
    }

    /// The card's kind and the file's (DN II: .dn2pst or .dn2snd).
    private var kind: FizzExportKind { pick == .dn2pst || pick == .dn2snd ? dn2 : pick }

    public var body: some View {
        ZStack(alignment: .bottom) {
            VStack(alignment: .leading, spacing: 0) {
                HStack {
                    Text("EXPORT").fz(15, 800, em: 0.14).foregroundStyle(FZ.cream.color)
                    Spacer(minLength: 0)
                    SheetCloseButton { if let onClose { onClose() } else { dismiss() } }
                }
                .frame(height: 40)
                HStack(alignment: .firstTextBaseline, spacing: 10) {
                    Text("\(sounds.count)").fz(46, 800).foregroundStyle(FZ.text.color)
                    Text(sounds.count == 1 ? "sound" : "sounds").fz(22, 800).foregroundStyle(FZ.muted.color)
                }
                .padding(.top, 10).padding(.bottom, 12)
                ScreenScroll(bottom: 190) {
                    ChipFlow(spacing: 6, lineSpacing: 6) {
                        ForEach(Array(sounds.prefix(12).enumerated()), id: \.offset) { _, s in SoundChip(sound: s) }
                        if sounds.count > 12 { NameChip(text: "+\(sounds.count - 12)") }
                    }
                    .padding(.bottom, 14)
                    VStack(spacing: 8) {
                        card(.fizz, maker: "FIZZ", ext: sounds.count == 1 ? ".fizzsnd" : ".fizzlib", color: FZ.cream, experimental: false)
                        card(.dnsnd, maker: "DN1", ext: ".dnsnd", color: FZ.cyan, experimental: true)
                        card(.dn2pst, maker: "DN II", ext: ".\(dn2.rawValue)", color: FZ.magenta, experimental: true)
                    }
                }
            }
            .padding(.horizontal, 16)
            .padding(.top, 8)
            footer
        }
        .task(id: sounds.map(\.name)) { await readCarry() }
        .task(id: kind) { await make() }
    }

    private func color(_ k: FizzExportKind) -> RGB { k == .fizz ? FZ.cream : k == .dnsnd ? FZ.cyan : FZ.magenta }

    private func card(_ k: FizzExportKind, maker: String, ext: String, color: RGB, experimental: Bool) -> some View {
        let on = pick == k, c = carry[k == .dn2pst ? .dn2pst : k]
        return Button { withAnimation(.spring(response: 0.4, dampingFraction: 0.82)) { pick = k } } label: {
            VStack(alignment: .leading, spacing: 0) {
                HStack(spacing: 10) {
                    FileTab(maker: maker, ext: ext, color: color, size: 14, height: 36).fixedSize()
                    if experimental {
                        Text("EXPERIMENTAL").fz(12, 800, em: 0.06).foregroundStyle(FZ.orange.color)
                            .padding(.horizontal, 6).padding(.vertical, 1)
                            .overlay(RoundedRectangle(cornerRadius: 6).strokeBorder(FZ.orange.color, style: StrokeStyle(lineWidth: 1.5, dash: [4, 3])))
                    }
                    Spacer(minLength: 0)
                    ZStack {
                        Circle().fill(on ? color.color : .clear)
                        Circle().strokeBorder(RGB(0x131318).color, lineWidth: 5).padding(2).opacity(on ? 1 : 0)
                        Circle().strokeBorder(on ? color.color : RGB(0x3a3a44).color, lineWidth: 2)
                    }
                    .frame(width: 26, height: 26)
                }
                CarryBar(n: c?.bar ?? 0).padding(.top, 11).id("\(k)-\(c?.bar ?? -1)")
                if on && k != .fizz { lostRows(k, c) }
            }
            .padding(.horizontal, 14).padding(.top, 12).padding(.bottom, 13)
            .background(RoundedRectangle(cornerRadius: 18).fill(on ? color.mix(8, FZ.raised).color : FZ.raised.color))
            .overlay(RoundedRectangle(cornerRadius: 18).strokeBorder(on ? color.color : FZ.line.color, lineWidth: on ? 2 : 1.5))
            .shadow(color: on ? color.opacity(0.28) : .clear, radius: 13)
            .contentShape(RoundedRectangle(cornerRadius: 18))
        }
        .buttonStyle(.plain)
    }

    /// What does not carry (.lr): its subject, how many sounds, its verdict; DN II's two files first.
    @ViewBuilder private func lostRows(_ k: FizzExportKind, _ c: FizzCarry?) -> some View {
        VStack(spacing: 0) {
            if k == .dn2pst {
                HStack(spacing: 0) {
                    ForEach([FizzExportKind.dn2pst, .dn2snd], id: \.self) { d in
                        Button { dn2 = d } label: {
                            Text(".\(d.rawValue)").fz(13, 800, em: 0.07)
                                .foregroundStyle(dn2 == d ? Color.black : FZ.muted.color)
                                .frame(maxWidth: .infinity).frame(height: 38)
                                .background(dn2 == d ? FZ.magenta.color : .clear)
                                .contentShape(Rectangle())
                        }
                        .buttonStyle(.plain)
                    }
                }
                .clipShape(Capsule())
                .overlay(Capsule().strokeBorder(FZ.line.color, lineWidth: 1.5))
                .padding(.top, 12).padding(.bottom, 10)
            }
            let rows = c?.rows ?? []
            ForEach(Array(rows.prefix(6).enumerated()), id: \.offset) { _, r in
                HStack(spacing: 10) {
                    Text(r.what).fz(14, 800, em: 0.03).foregroundStyle(FZ.text.color).lineLimit(1)
                    if r.count > 1 { Text("×\(r.count)").fz(13, 700).foregroundStyle(FZ.muted.color) }
                    Spacer(minLength: 4)
                    Text(r.how).fz(12, 800, em: 0.04).foregroundStyle(.black)
                        .padding(.horizontal, 8).padding(.vertical, 2)
                        .background(RoundedRectangle(cornerRadius: 6).fill(tokenColor(r.color).color))
                        .lineLimit(1).fixedSize()
                }
                .frame(height: 34)
                .overlay(alignment: .top) { Rectangle().fill(FZ.line.color).frame(height: 1) }
            }
            if rows.count > 6 {
                HStack {
                    Text("+\(rows.count - 6)").fz(14, 800).foregroundStyle(FZ.muted.color)
                    Spacer(minLength: 0)
                }
                .frame(height: 30)
                .overlay(alignment: .top) { Rectangle().fill(FZ.line.color).frame(height: 1) }
            }
        }
        .padding(.top, k == .dn2pst ? 0 : 10)
    }

    private var footer: some View {
        let c = color(kind)
        let none = carry[kind == .dn2snd ? .dn2pst : kind]?.none ?? false
        let ready = file?.kind == kind && !none
        return VStack(spacing: 10) {
            HStack(spacing: 8) {
                Text(fileText).fz(14, 800).foregroundStyle(FZ.text.color).lineLimit(1)
                if sounds.count > 1 || none {
                    Text("→").fz(14, 800).foregroundStyle(c.color)
                    Text(none ? "–" : kind == .fizz ? ".fizzlib" : ".zip").fz(14, 800).foregroundStyle(FZ.text.color)
                }
            }
            .padding(.horizontal, 14).frame(height: 32)
            .overlay(Capsule().strokeBorder(c.mix(50, FZ.line).color, lineWidth: 1.5))
            ShareButton(url: ready ? file?.url : nil, color: c, failed: failed || none)
        }
        .padding(.horizontal, 16)
        .padding(.top, 40)
        .padding(.bottom, max(24, insets.bottom))
        .background(LinearGradient(stops: [.init(color: .clear, location: 0), .init(color: RGB(0x0d0d11).color, location: 0.3)],
                                   startPoint: .top, endPoint: .bottom).allowsHitTesting(false).ignoresSafeArea(edges: .bottom))
    }

    /// "3 × .dn2pst", one sound's file name.
    private var fileText: String {
        let ext = kind == .fizz ? "fizzsnd" : kind.rawValue
        let midi = file?.kind == kind ? file?.skipped ?? 0 : 0
        if sounds.count == 1 { return file?.kind == kind ? file?.name ?? ".\(ext)" : ".\(ext)" }
        return "\(sounds.count - midi) × .\(ext)"
    }

    private func readCarry() async {
        guard let e = try? FizzDataShared.engine() else { return }
        var out: [FizzExportKind: FizzCarry] = [:]
        for k in [FizzExportKind.fizz, .dnsnd, .dn2pst] { out[k] = try? await e.exportCarry(sounds, kind: k) }
        withAnimation { carry = out }
    }

    /// The file for the format picked, written where the share sheet takes it from.
    private func make() async {
        failed = false
        guard let e = try? FizzDataShared.engine(), !sounds.isEmpty else { return }
        do {
            let f = try await e.exportFile(sounds, kind: kind)
            let dir = FileManager.default.temporaryDirectory.appendingPathComponent("FIZZ-export", isDirectory: true)
            try FileManager.default.createDirectory(at: dir, withIntermediateDirectories: true)
            let url = dir.appendingPathComponent(f.name)
            if !snapshot { try Data(f.bytes).write(to: url, options: .atomic) }
            file = (kind, url, f.name, f.skipped.count)
        } catch {
            file = nil
            failed = true
        }
    }
}

/// A sound's chip (.xs): its glyph in its tag's colour, its name.
struct SoundChip: View {
    let sound: FizzSound
    var body: some View {
        let tint = sound.tags.first.flatMap { n in FizzTag.allCases.first { $0.name == n } }?.tint ?? FZ.cream
        HStack(spacing: 5) {
            AlgoGlyph(algorithm: sound.blob.count > 2 ? Int(sound.blob[2]) + 1 : 1, color: tint.color).frame(width: 22, height: 22)
            Text(sound.name).fz(13, 800, em: 0.02).foregroundStyle(FZ.text.color).lineLimit(1)
        }
        .padding(.leading, 5).padding(.trailing, 11).frame(height: 36)
        .background(RoundedRectangle(cornerRadius: 12).fill(tint.mix(10, FZ.raised).color))
        .overlay(RoundedRectangle(cornerRadius: 12).strokeBorder(tint.mix(60, FZ.line).color, lineWidth: 1.5))
        .fixedSize()
    }
}

/// SHARE in the format's colour, breathing: the share sheet with the file (none yet, or none to make: dim).
struct ShareButton: View {
    let url: URL?
    let color: RGB
    var failed = false
    @Environment(\.fizzSnapshot) private var snapshot

    var body: some View {
        if let url, !snapshot {
            ShareLink(item: url) { face(true) }.buttonStyle(.plain)
        } else {
            face(snapshot && !failed)
        }
    }

    private func face(_ on: Bool) -> some View {
        Animated(still: 0.6) { t in
            HStack(spacing: 10) {
                IconView(Icons.share, color: .black, lineWidth: 2.4, backdrop: color.color).frame(width: 20, height: 20)
                Text("SHARE").fz(19, 800, em: 0.08)
            }
            .foregroundStyle(.black)
            .frame(maxWidth: .infinity).frame(height: 64)
            .background(Capsule().fill(color.color))
            .shadow(color: color.opacity(on ? 0.35 + 0.35 * breathe(t, 2) : 0), radius: 10 + 10 * breathe(t, 2))
            .opacity(on ? 1 : 0.4)
            .animation(.easeInOut(duration: 0.3), value: color)
        }
    }
}
