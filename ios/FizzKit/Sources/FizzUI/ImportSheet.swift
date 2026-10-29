// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// IMPORT (mock-up 15): the file tabs (lit for the formats read; a tap opens the picker), the count, a row per sound
// (its maker's chip, glyph, name and fidelity bar, its tag, or HAVE / TWICE, or what came of SEND; ▶ heard on the
// track), MY PRESETS / POOL / TRACK, and ↓ SEND n TO FM-1. A tap on a row ticks it; a long press lists its notes.
// Files come from the picker, Open in FIZZ, AirDrop and, on the iPad, a drop.

import SwiftUI
import FizzData
import FizzKit
import UniformTypeIdentifiers

public struct ImportSheet: View {
    let model: UnitModel
    /// the iPad's tab (no close button)
    var inline = false
    @Environment(\.fizzSnapshot) private var snapshot
    @Environment(\.fizzInsets) private var insets
    @State private var picking = false
    @State private var over = false
    @Environment(\.dismiss) private var dismiss

    public init(model: UnitModel, inline: Bool = false) {
        self.model = model
        self.inline = inline
    }

    public var body: some View {
        let im = model.importer
        ZStack(alignment: .bottom) {
            VStack(alignment: .leading, spacing: 0) {
                head
                FileTabs(formats: im.formats) { picking = true }
                    .padding(.top, 14).padding(.bottom, 14)
                if im.rows.isEmpty {
                    EmptyImport(reading: im.reading, over: over) { picking = true }
                    Spacer(minLength: 0)
                } else {
                    HStack(alignment: .firstTextBaseline, spacing: 10) {
                        Text("\(im.rows.count)").fz(46, 800).foregroundStyle(FZ.text.color).contentTransition(.numericText())
                        Text(im.rows.count == 1 ? "sound" : "sounds").fz(22, 800).foregroundStyle(FZ.muted.color)
                        Spacer(minLength: 0)
                        if !im.refused.isEmpty { NameChip(text: "\(im.refused.count) NOT SOUNDS", fill: FZ.orange) }
                    }
                    .padding(.bottom, 12)
                    rows
                }
            }
            .padding(.horizontal, 16)
            // (clear of the sheet's grab handle)
            .padding(.top, inline ? 4 : snapshot ? 8 : 20)
            if !im.rows.isEmpty { footer }
        }
        .frame(maxWidth: inline ? 760 : .infinity)
        .frame(maxWidth: .infinity)
        .animation(.spring(response: 0.4, dampingFraction: 0.82), value: im.rows.map(\.id))
        .fileImporter(isPresented: $picking, allowedContentTypes: UTType.importable, allowsMultipleSelection: true) { r in
            guard case .success(let urls) = r else { return }
            Task { await im.add(urls: urls) }
        }
        .task {
            await im.loadDemo()
            // (`-import-edit "WARM SUB"` with the demo unit: that row's EDIT, for the simulator's screenshots)
            let a = ProcessInfo.processInfo.arguments
            if !snapshot, model.source is DemoLinkSource, let k = a.firstIndex(of: "-import-edit"), k + 1 < a.count,
               let r = im.rows.first(where: { $0.item.sound?.name == a[k + 1] }) {
                try? await Task.sleep(for: .milliseconds(800))
                im.edit(r.id)
            }
        }
        .modifier(DropFiles(on: !snapshot, over: $over) { files in await im.add(files: files) })
    }

    private var head: some View {
        HStack(spacing: 4) {
            Text("IMPORT").fz(15, 800, em: 0.14).foregroundStyle(FZ.violet.color)
            Spacer(minLength: 0)
            if model.actions.trackCapable { TrackPill(model: model) }
            if !model.importer.rows.isEmpty && !model.importer.sending {
                Button { model.importer.clear() } label: {
                    IconView(Icons.trash, color: FZ.muted.color, lineWidth: 2, backdrop: .clear)
                        .frame(width: 17, height: 17).frame(width: 36, height: 36)
                        .overlay(Circle().strokeBorder(FZ.line.color, lineWidth: 1.5))
                        .frame(width: 44, height: 44)
                        .contentShape(Rectangle())
                }
                .buttonStyle(.plain)
            }
            if !inline { SheetCloseButton { dismiss() } }
        }
        .frame(height: 44)
    }

    private var rows: some View {
        let im = model.importer
        return ScreenScroll(bottom: 200) {
            VStack(spacing: 7) {
                ForEach(im.rows) { r in
                    ImportRowView(row: r, heard: im.isHeard(r.id), canPlay: im.trackOn && !im.sending,
                                  target: im.target, toggle: { im.toggle(r.id) }, play: { Task { await im.play(r.id) } },
                                  edit: im.sending ? nil : { im.edit(r.id) })
                        .transition(.asymmetric(insertion: .move(edge: .bottom).combined(with: .opacity), removal: .opacity))
                }
            }
        }
    }

    /// MY PRESETS / POOL / TRACK and SEND, over a fade.
    private var footer: some View {
        let im = model.importer
        let n = im.ticked.count
        return VStack(spacing: 12) {
            if n > im.free && model.unit != nil && im.target != .kept {
                NameChip(text: "ONLY \(im.free) FREE", fill: FZ.orange)
            }
            TargetSwitch(target: im.target, track: im.track, trackOn: im.trackOn) { im.pick($0) }
            SendButton(n: n, progress: im.sendProgress, enabled: im.canSend, to: im.target == .kept ? "KEPT" : model.unitName,
                       color: im.target == .kept ? FZ.champagne : FZ.violet) { Task { await im.send() } }
        }
        .padding(.horizontal, 16)
        .padding(.top, 40)
        .padding(.bottom, max(24, insets.bottom))
        .background(
            LinearGradient(stops: [.init(color: .clear, location: 0), .init(color: RGB(0x0d0d11).color, location: 0.3)],
                           startPoint: .top, endPoint: .bottom)
                .allowsHitTesting(false).ignoresSafeArea(edges: .bottom)
        )
    }
}

/// The six file tabs (.files): lit for the formats read; a tap opens the picker.
struct FileTabs: View {
    let formats: Set<String>
    let open: () -> Void

    static let all: [(String, String, RGB)] = [
        ("FIZZ", "fizzlib", FZ.cream), ("FIZZ", "fizzsnd", FZ.cream), ("FIZZ", "fizzbak", FZ.cream),
        ("DN1", "dnsnd", FZ.cyan), ("DN II", "dn2pst", FZ.magenta), ("DN II", "dn2snd", FZ.magenta),
    ]

    var body: some View {
        Button(action: open) {
            LazyVGrid(columns: Array(repeating: GridItem(.flexible(), spacing: 7), count: 3), spacing: 7) {
                ForEach(Self.all, id: \.1) { t in
                    FileTab(maker: t.0, ext: ".\(t.1)", color: t.2, on: formats.isEmpty || formats.contains(t.1))
                        .frame(maxWidth: .infinity, alignment: .leading)
                }
            }
        }
        .buttonStyle(.plain)
    }
}

/// Nothing read yet: a big "+" in a dashed box (a drop lights it).
struct EmptyImport: View {
    let reading: Bool
    let over: Bool
    let open: () -> Void

    var body: some View {
        Button(action: open) {
            Animated(still: 0) { t in
                IconView(reading ? TransferIcons.down : Icons.plus, color: FZ.violet.color, lineWidth: 2.4, backdrop: .clear)
                    .frame(width: 64, height: 64)
                    .offset(y: reading ? 8 * breathe(t, 0.9) : 0)
                    .shadow(color: FZ.violet.opacity(0.3 + 0.4 * breathe(t, 2.4)), radius: 16)
            }
            .frame(maxWidth: .infinity)
            .frame(height: 220)
            .background(RoundedRectangle(cornerRadius: 22).fill(FZ.violet.opacity(over ? 0.16 : 0.05)))
            .overlay(RoundedRectangle(cornerRadius: 22).strokeBorder(FZ.violet.opacity(over ? 1 : 0.55),
                                                                     style: StrokeStyle(lineWidth: 2, dash: [7, 6])))
            .contentShape(RoundedRectangle(cornerRadius: 22))
        }
        .buttonStyle(.plain)
    }
}

/// One sound (.ir): ticked or not, heard or not, sent or not.
struct ImportRowView: View {
    let row: ImportModel.Row
    let heard: Bool
    let canPlay: Bool
    let target: ImportModel.Target
    let toggle: () -> Void
    let play: () -> Void
    /// EDIT: the sound opened in EDIT (nil: not now)
    var edit: (() -> Void)?

    private var kindColor: RGB {
        switch row.item.kind {
        case .fizz: FZ.cream
        case .dn1: FZ.cyan
        case .dn2: FZ.magenta
        }
    }
    private var kindLabel: String {
        switch row.item.kind {
        case .fizz: "FIZZ"
        case .dn1: "DN1"
        case .dn2: "DN II"
        }
    }
    private var tag: FizzTag? { row.item.sound?.tags.first.flatMap { n in FizzTag.allCases.first { $0.name == n } } }

    var body: some View {
        let ok = row.item.ok, lit = ok && (row.on || row.result != nil)
        HStack(spacing: 10) {
            Text(kindLabel).fz(12, 800)
                .foregroundStyle(row.on ? Color.black : kindColor.color)
                .frame(width: 42, height: 22)
                .background(RoundedRectangle(cornerRadius: 6).fill(row.on ? kindColor.color : .clear))
                .overlay(RoundedRectangle(cornerRadius: 6).strokeBorder(kindColor.color, lineWidth: row.on ? 0 : 1.5))
            if let s = row.item.sound {
                AlgoGlyph(algorithm: s.blob.count > 2 ? Int(s.blob[2]) + 1 : 1, color: (tag?.tint ?? FZ.cream).color)
                    .frame(width: 26, height: 26)
            } else {
                IconView(TransferIcons.warn, color: FZ.red.color, lineWidth: 2, backdrop: .clear).frame(width: 24, height: 24)
            }
            VStack(alignment: .leading, spacing: 5) {
                Text(name).fz(16, 800, em: 0.03).foregroundStyle(FZ.text.color).lineLimit(1)
                if ok { CarryBar(n: row.fidelity, height: 4, spacing: 2).frame(width: 66) }
            }
            .opacity(lit ? 1 : 0.5)
            Spacer(minLength: 4)
            trailing
            if ok && row.item.sound != nil { editButton }
            if ok && row.result == nil { playButton }
        }
        .padding(.leading, 10).padding(.trailing, 8)
        .frame(height: 58)
        .background(RoundedRectangle(cornerRadius: 14).fill(heard ? FZ.violet.mix(15, FZ.raised).color : FZ.raised.color))
        .overlay(RoundedRectangle(cornerRadius: 14).strokeBorder(heard ? FZ.violet.color : FZ.line.color, lineWidth: 1.5))
        .shadow(color: heard ? FZ.violet.opacity(0.32) : .clear, radius: 9)
        .contentShape(RoundedRectangle(cornerRadius: 14))
        .onTapGesture(perform: toggle)
        .contextMenu { notes }
        .animation(.easeOut(duration: 0.2), value: row.on)
        .animation(.spring(response: 0.35, dampingFraction: 0.8), value: heard)
    }

    private var name: String {
        if let s = row.item.sound { return s.name }
        let f = row.item.file
        return ((f.components(separatedBy: ": ").last ?? f) as NSString).deletingPathExtension.uppercased()
    }

    @ViewBuilder private var trailing: some View {
        if let r = row.result {
            switch r {
            case .sent(let slot):
                HStack(spacing: 4) {
                    IconView(TransferIcons.check, color: .black, lineWidth: 2.8, backdrop: FZ.green.color).frame(width: 13, height: 13)
                    Text("\(target == .pool ? "POOL" : "MY") \(slot)").fz(12, 800, em: 0.04)
                }
                .foregroundStyle(.black)
                .padding(.horizontal, 8).frame(height: 24)
                .background(RoundedRectangle(cornerRadius: 6).fill(FZ.green.color))
                .fixedSize()
            case .failed(let why):
                StateChip(text: why, color: why == "HAVE" ? FZ.orange : FZ.red, solid: true)
            case .kept:
                HStack(spacing: 4) {
                    IconView(KeptArt.phone, color: .black, lineWidth: 2.4, backdrop: FZ.champagne.color).frame(width: 13, height: 13)
                    Text("KEPT").fz(12, 800, em: 0.04)
                }
                .foregroundStyle(.black)
                .padding(.horizontal, 8).frame(height: 24)
                .background(RoundedRectangle(cornerRadius: 6).fill(FZ.champagne.color))
                .fixedSize()
            }
        } else if !row.item.ok {
            StateChip(text: Self.reason(row.item.error), color: FZ.red, solid: false)
        } else if heard {
            LevelBars()
        } else if row.dup == "mine" {
            StateChip(text: "HAVE", color: FZ.orange, solid: false)
        } else if row.dup == "list" {
            StateChip(text: "TWICE", color: FZ.orange, solid: false)
        } else if row.item.midi {
            StateChip(text: "MIDI", color: FZ.violet, solid: true)
        } else if let tag {
            // (the tag's icon, not its name: the row keeps room for the name beside EDIT and ▶)
            TagIcon(tag, lineWidth: 2.2, glow: true).frame(width: 24, height: 24).padding(.horizontal, 4)
        }
    }

    /// EDIT (pink, as LIBRARY's): the sound in EDIT.
    private var editButton: some View {
        Button { edit?() } label: {
            IconView(Icons.pencil, color: FZ.pink.color, lineWidth: 2, backdrop: .clear)
                .frame(width: 15, height: 15)
                .frame(width: 38, height: 38)
                .overlay(Circle().strokeBorder(FZ.pink.mix(65, FZ.line).color, lineWidth: 2))
                .frame(width: 40, height: 44)
                .contentShape(Circle())
        }
        .buttonStyle(.plain)
        .disabled(edit == nil)
        .opacity(edit == nil ? 0.35 : 1)
    }

    private var playButton: some View {
        Button(action: play) {
            IconView(heard ? TransferIcons.stop : TransferIcons.play, color: heard ? .black : FZ.violet.color, fill: true)
                .frame(width: 14, height: 14)
                .offset(x: heard ? 0 : 1.5)
                .frame(width: 44, height: 44)
                .background(Circle().fill(heard ? FZ.violet.color : .clear))
                .overlay(Circle().strokeBorder(heard ? FZ.violet.color : FZ.violet.mix(65, FZ.line).color, lineWidth: 2))
                .contentShape(Circle())
        }
        .buttonStyle(.plain)
        .disabled(!canPlay && !heard)
        .opacity(canPlay || heard ? 1 : 0.35)
    }

    /// The notes, the file's mark, or why it did not read (a long press).
    @ViewBuilder private var notes: some View {
        if !row.item.ok { Text(row.item.error) }
        ForEach(Array(row.item.notes.prefix(12).enumerated()), id: \.offset) { _, n in Text(n) }
        if !row.item.mark.isEmpty { Text(row.item.mark) }
        Text(row.item.file)
    }

    /// The reader's error as a short state.
    static func reason(_ e: String) -> String {
        let l = e.lowercased()
        if l.contains("not fm tone") || l.contains("wavetone") || l.contains("fm drum") || l.contains("swarmer") { return "NOT FM TONE" }
        if l.contains("no presets") { return "EMPTY" }
        if l.contains("newer") { return "NEWER FILE" }
        return "UNREADABLE"
    }
}

/// A small chip: filled, or outlined in its colour.
struct StateChip: View {
    let text: String
    let color: RGB
    var solid = true
    var body: some View {
        Text(text).fz(12, 800, em: 0.04)
            .foregroundStyle(solid ? Color.black : color.color)
            .lineLimit(1)
            .padding(.horizontal, 8).frame(height: 24)
            .background(RoundedRectangle(cornerRadius: 6).fill(solid ? color.color : .clear))
            .overlay(RoundedRectangle(cornerRadius: 6).strokeBorder(color.color, lineWidth: solid ? 0 : 1.5))
            .fixedSize()
    }
}

/// The level of the sound heard (.lvl): five bars bouncing.
struct LevelBars: View {
    static let speeds = [0.5, 0.37, 0.44, 0.31, 0.55]
    var body: some View {
        Animated(still: 0.21) { t in
            HStack(alignment: .bottom, spacing: 3) {
                ForEach(0..<5, id: \.self) { k in
                    let p = (t + Double(k) * 0.13).truncatingRemainder(dividingBy: 2 * Self.speeds[k]) / Self.speeds[k]
                    let h = 0.2 + 0.8 * (p < 1 ? p : 2 - p)
                    RoundedRectangle(cornerRadius: 2).fill(FZ.violet.color).frame(width: 4, height: 22 * h)
                }
            }
            .frame(height: 22, alignment: .bottom)
        }
    }
}

/// MY PRESETS / POOL / TRACK (.dest .seg); TRACK carries its track's number.
struct TargetSwitch: View {
    let target: ImportModel.Target
    let track: Int
    let trackOn: Bool
    let pick: (ImportModel.Target) -> Void

    var body: some View {
        HStack(spacing: 0) {
            ForEach(ImportModel.Target.allCases, id: \.self) { t in
                let on = t == target
                Button { pick(t) } label: {
                    HStack(spacing: 7) {
                        if t == .kept {
                            IconView(KeptArt.phone, color: on ? .black : FZ.champagne.color, lineWidth: 2.2,
                                     backdrop: on ? FZ.champagne.color : .black).frame(width: 14, height: 16)
                        }
                        Text(t == .mine ? "MY PRESETS" : t.rawValue.uppercased()).fz(13, 800, em: 0.07)
                            .lineLimit(1).minimumScaleFactor(0.8)
                        if t == .track {
                            Text("\(track + 1)").fz(13, 800).foregroundStyle(.white)
                                .frame(width: 22, height: 22).background(Circle().fill(FZ.pink.color))
                                .contentTransition(.numericText())
                        }
                    }
                    .foregroundStyle(on ? Color.black : t == .kept ? FZ.champagne.color : FZ.muted.color)
                    .frame(maxWidth: .infinity).frame(height: 44)
                    .background(on ? (t == .kept ? FZ.champagne : FZ.violet).color : .clear)
                    .contentShape(Rectangle())
                }
                .buttonStyle(.plain)
                .disabled(t == .track && !trackOn)
                .opacity(t == .track && !trackOn ? 0.4 : 1)
            }
        }
        .background(RGB(0x0d0d11).color)
        .clipShape(Capsule())
        .overlay(Capsule().strokeBorder(FZ.line.color, lineWidth: 1.5))
        .animation(.spring(response: 0.3, dampingFraction: 0.8), value: target)
        .animation(.spring(response: 0.3, dampingFraction: 0.8), value: track)
    }
}

/// ↓ SEND n TO FM-1, breathing violet; while sending its count.
struct SendButton: View {
    let n: Int
    let progress: (done: Int, of: Int)?
    let enabled: Bool
    /// the unit's name (FM-1, THIS iPHONE), or KEPT
    var to = "FM-1"
    var color: RGB = FZ.violet
    let action: () -> Void

    var body: some View {
        Animated(still: 0.6) { t in
            Button(action: action) {
                HStack(spacing: 10) {
                    IconView(TransferIcons.down, color: .black, lineWidth: 3, backdrop: color.color).frame(width: 20, height: 20)
                    Text(progress.map { "\($0.done) / \($0.of)" } ?? "SEND \(n) TO \(to)").fz(19, 800, em: 0.08)
                        .contentTransition(.numericText())
                }
                .foregroundStyle(.black)
                .frame(maxWidth: .infinity).frame(height: 64)
                .background(Capsule().fill(color.color))
                .shadow(color: color.opacity(enabled ? 0.35 + 0.35 * breathe(t, 2) : 0), radius: 10 + 10 * breathe(t, 2))
                .contentShape(Capsule())
            }
            .buttonStyle(.plain)
        }
        .disabled(!enabled)
        .opacity(enabled || progress != nil ? 1 : 0.4)
    }
}

/// The drop target (the iPad); none in a snapshot (ImageRenderer draws no platform view).
struct DropFiles: ViewModifier {
    let on: Bool
    @Binding var over: Bool
    let take: @MainActor ([(name: String, bytes: [UInt8])]) async -> Void

    func body(content: Content) -> some View {
        if on {
            content.onDrop(of: [.item], isTargeted: $over) { providers in
                Task { await take(await DroppedFiles.read(providers)) }
                return true
            }
        } else {
            content
        }
    }
}

/// Files dropped on the iPad (Files, another app): their bytes and names.
enum DroppedFiles {
    @MainActor
    static func read(_ providers: [NSItemProvider]) async -> [(name: String, bytes: [UInt8])] {
        var out: [(name: String, bytes: [UInt8])] = []
        for p in providers {
            let suggested = p.suggestedName
            let got: (String, [UInt8])? = await withCheckedContinuation { k in
                _ = p.loadFileRepresentation(forTypeIdentifier: UTType.item.identifier) { url, _ in
                    guard let url, let d = try? Data(contentsOf: url) else { k.resume(returning: nil); return }
                    let ext = url.pathExtension
                    let name = suggested.map { ($0 as NSString).pathExtension.isEmpty && !ext.isEmpty ? "\($0).\(ext)" : $0 } ?? url.lastPathComponent
                    k.resume(returning: (name, [UInt8](d)))
                }
            }
            if let got { out.append(got) }
        }
        return out
    }
}
