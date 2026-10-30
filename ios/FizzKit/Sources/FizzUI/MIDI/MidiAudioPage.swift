// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// PLAY's sheet › MIDI & audio (mock-up 55), in RemoteFM1's sheet style: the back line and the Unbounded title fixed,
// the rest scrolling, rows 52 high in the #211E19 card. AUDIO: Sound. MIDI: MIDI input (its own page), MIDI notes play
// keys, ...and reach FIZZ's MIDI in, Transpose, Bluetooth MIDI devices (iOS's pairing). KNOBS FROM MIDI: Learn (the
// sheet goes: learn mode on the panel), Knob speed, MASTER pickup, the learned list (an encoder's mode, ×), the map's
// import and export (RemoteFM1's file), Clear all (asked again inline). OMX-27: REMOTE mode, connected or not.

import SwiftUI
import FizzEngine
import UniformTypeIdentifiers
#if os(iOS)
import AVFoundation
#endif

/// The sheet's pages under its list.
public enum MidiSheetPage: Sendable { case midi, input }

struct MidiAudioPage: View {
    let host: EmulatorHost
    /// the scrolling part's height
    let bodyHeight: CGFloat
    @State private var askClear = false
    @State private var importing = false
    @State private var importFailed = false

    @Environment(\.fizzSnapshot) private var snapshot
    private var play: PlayModel { host.play }
    private var page: MidiSheetPage? {
        get { play.sheetPage }
        nonmutating set { play.sheetPage = newValue }
    }
    private var midi: MidiHub { play.midi }
    private var accent: RGB { play.theme.accent }

    var body: some View {
        VStack(alignment: .leading, spacing: 0) {
            MidiPageHead(back: page == .input ? "MIDI & audio" : "Play", title: page == .input ? "MIDI input" : "MIDI & audio") {
                withAnimation(.easeInOut(duration: 0.2)) { page = page == .input ? .midi : nil }
            }
            if snapshot {
                // (a snapshot: the page as scrolled by play.midiShotScroll; ImageRenderer draws no scroll offset)
                pageBody.offset(y: -play.midiShotScroll)
                    .frame(maxWidth: .infinity, maxHeight: bodyHeight, alignment: .top).frame(height: bodyHeight).clipped()
            } else {
                ScrollViewReader { r in
                    ScrollView { pageBody }.scrollIndicators(.hidden).frame(height: bodyHeight)
                        .onAppear { if let to = play.midiScrollTo { r.scrollTo(to, anchor: .top) } }
                }
            }
        }
        .fileImporter(isPresented: $importing, allowedContentTypes: [.json]) { r in
            guard case .success(let url) = r else { return }
            let scoped = url.startAccessingSecurityScopedResource()
            defer { if scoped { url.stopAccessingSecurityScopedResource() } }
            do {
                try midi.store.importMap(try Data(contentsOf: url))
                importFailed = false
            } catch {
                importFailed = true
            }
        }
    }

    // MARK: the page

    private var pageBody: some View {
        VStack(alignment: .leading, spacing: 0) {
            if page == .input { inputPage } else { mainPage }
        }
        .padding(.bottom, 24)
    }

    @ViewBuilder private var mainPage: some View {
        MidiHeading("Audio", first: true)
        MidiCard {
            MidiRow { Text("Sound"); Spacer(minLength: 8); MidiToggle(on: host.soundOn, accent: accent) { host.soundOn.toggle() } }
        }
        MidiNote(audioLine)
        MidiHeading("MIDI")
        MidiCard {
            Button { withAnimation(.easeInOut(duration: 0.2)) { page = .input } } label: {
                MidiRow {
                    Text("MIDI input")
                    Spacer(minLength: 8)
                    Text(midi.input.selectionName).foregroundStyle(SheetInk.soft.color).lineLimit(1)
                    MidiChevron()
                }
            }
            .buttonStyle(PressStyle())
            MidiRule()
            MidiRow { Text("MIDI notes play keys"); Spacer(minLength: 8); MidiToggle(on: midi.notesPlayKeys, accent: accent) { midi.notesPlayKeys.toggle() } }
            MidiRule()
            MidiRow { Text("…and reach FIZZ's MIDI in"); Spacer(minLength: 8); MidiToggle(on: midi.reachFizz, accent: accent) { midi.reachFizz.toggle() } }
            MidiRule()
            MidiRow {
                Text("Transpose")
                Spacer(minLength: 8)
                Text(Self.octaves(midi.transpose)).dm(15.5, 500).foregroundStyle(SheetInk.head.color)
                MidiStepper(down: midi.transpose > -2 ? { midi.transpose -= 1 } : nil,
                            up: midi.transpose < 2 ? { midi.transpose += 1 } : nil)
            }
            MidiRule()
            Button { pair() } label: { MidiRow { Text("Bluetooth MIDI devices"); Spacer(minLength: 8); MidiChevron() } }
                .buttonStyle(PressStyle())
        }
        MidiNote("A MIDI keyboard plays the keys (transposed by octaves); its other notes, CCs and clock, and every note when they don't play the keys, go to FIZZ's MIDI in.")
        knobs
        omx
    }

    /// "0", "+1 oct", "−2 oct"
    static func octaves(_ t: Int) -> String { t == 0 ? "0" : "\(t > 0 ? "+" : "−")\(abs(t)) oct" }

    private var audioLine: String {
        guard host.soundOn else { return "Off: FIZZ plays silently." }
        #if os(iOS)
        let s = AVAudioSession.sharedInstance()
        let out = s.currentRoute.outputs.first?.portName ?? "no output"
        return String(format: "%.0f Hz, io buffer %.1f ms, %@", s.sampleRate, s.ioBufferDuration * 1000, out)
        #else
        return "48000 Hz, io buffer 5.3 ms, Speaker"
        #endif
    }

    private func pair() {
        #if os(iOS)
        BluetoothPairing.present()
        #endif
    }

    // MARK: Knobs from MIDI

    @ViewBuilder private var knobs: some View {
        let list = midi.store.mappings
        MidiHeading("Knobs from MIDI").id("knobs")
        MidiCard {
            MidiRow(height: 60) {
                VStack(alignment: .leading, spacing: 3) {
                    Text("MIDI learn")
                    Text(list.isEmpty ? "Nothing learned" : "\(list.count) learned").dm(12.5, 400).foregroundStyle(SheetInk.soft.color)
                }
                Spacer(minLength: 8)
                MidiPill("Learn", accent: accent, filled: true) { midi.startLearn() }
            }
            MidiRule()
            MidiRow {
                Text("Knob speed")
                Spacer(minLength: 8)
                MidiSpeeds(speed: midi.store.speed, accent: accent) { midi.store.setSpeed($0) }
            }
            MidiRule()
            MidiRow { Text("MASTER pickup"); Spacer(minLength: 8); MidiToggle(on: midi.masterPickup, accent: accent) { midi.masterPickup.toggle() } }
        }
        MidiNote("Tap Learn, then a knob or button on the panel, then move a control on your MIDI device.")
        if !list.isEmpty {
            MidiCard {
                ForEach(Array(list.enumerated()), id: \.element.src) { i, mp in
                    if i > 0 { MidiRule() }
                    MidiMapRow(mapping: mp, pickup: mp.target == "MASTER" && midi.masterPickup,
                               mode: { midi.store.setMode(mp.src, $0) }, remove: { midi.store.remove(mp.src) })
                }
            }
            .padding(.top, 12)
        }
        HStack(spacing: 8) {
            MidiPill("Import", accent: accent) { importing = true }
            if !list.isEmpty {
                ShareLink(item: MidiMapFile(data: midi.store.exported), preview: SharePreview("FIZZ MIDI map")) {
                    MidiPillLabel("Export", accent: accent)
                }
                .buttonStyle(PressStyle())
            }
            Spacer(minLength: 0)
            if !list.isEmpty {
                if askClear {
                    MidiPill("Cancel", accent: accent) { askClear = false }
                    MidiPill("Clear \(list.count)", accent: RGB(0xFF6464), filled: true) { midi.store.clear(); askClear = false }
                } else {
                    MidiPill("Clear all", accent: accent) { askClear = true }
                }
            }
        }
        .padding(.top, 10)
        if importFailed { MidiNote("That file is not a MIDI map.") }
    }

    // MARK: OMX-27

    @ViewBuilder private var omx: some View {
        let o = midi.omx
        MidiHeading("OMX-27").id("omx")
        Text("An OMX-27 becomes PLAY's controller: its keys, encoder and pots play the panel; FIZZ lights its keys and draws its screen. K1 picks the view, K2 the colours, K5 is MASTER.")
            .dm(14.5, 400).foregroundStyle(SheetInk.soft.color).lineSpacing(3).fixedSize(horizontal: false, vertical: true)
            .padding(.bottom, 12)
        MidiPill(o.buttonTitle, accent: accent, filled: !(o.want && o.tone != .bad), tile: true) { o.toggle() }
        HStack(alignment: .firstTextBaseline, spacing: 10) {
            Circle().fill(Self.toneColor(o.tone).color).frame(width: 9, height: 9)
                .shadow(color: o.tone == .on ? Self.toneColor(.on).color : .clear, radius: 4)
            Text(Self.status(o)).dm(15, 500).foregroundStyle((o.tone == .on ? Self.toneColor(.on) : SheetInk.soft).color)
                .fixedSize(horizontal: false, vertical: true)
        }
        .padding(.top, 12)
        MidiNote("Needs the OMX-27 firmware with REMOTE mode, over USB. To leave remote mode, hold AUX and the encoder.")
    }

    static func toneColor(_ t: OmxLink.Tone) -> RGB {
        switch t {
        case .off: SheetInk.off
        case .busy: RGB(0xF5C04A)
        case .on: RGB(0x3BD47A)
        case .bad: RGB(0xFF6464)
        }
    }

    /// "Connected · REMOTE mode · Base view", or the link's own line ("Not connected." ..).
    static func status(_ o: OmxLink) -> String {
        o.tone == .on && o.active ? "Connected · REMOTE mode · \(o.views.view.name) view" : o.detail
    }

    // MARK: the MIDI input page

    @ViewBuilder private var inputPage: some View {
        let inp = midi.input
        MidiCard {
            choice("All inputs", inp.selection == .all) { inp.selection = .all }
            MidiRule()
            choice("None", inp.selection == .none) { inp.selection = .none }
            ForEach(inp.devices) { d in
                MidiRule()
                choice(d.name, inp.selection == .one(d.id)) { inp.selection = .one(d.id) }
            }
        }
        .padding(.top, 14)
        MidiNote(inp.problem ?? (inp.devices.isEmpty
            ? "No MIDI device found: plug one in over USB, or pair one over Bluetooth."
            : "\(inp.devices.count) found. The FM-1 in use, an FM-1 used as the panel and an OMX-27 in REMOTE mode are not listed."))
    }

    private func choice(_ t: String, _ on: Bool, _ go: @escaping () -> Void) -> some View {
        Button(action: go) {
            MidiRow {
                Text(t).lineLimit(1)
                Spacer(minLength: 8)
                if on { IconView(MidiGlyph.tick, color: accent.color, lineWidth: 2.8).frame(width: 16, height: 16) }
            }
        }
        .buttonStyle(PressStyle())
    }
}

/// The map as a file for the share sheet ("FIZZ MIDI map.json", RemoteFM1's form).
struct MidiMapFile: Transferable {
    let data: Data
    static var transferRepresentation: some TransferRepresentation {
        FileRepresentation(exportedContentType: .json) { f in
            let url = FileManager.default.temporaryDirectory.appendingPathComponent("FIZZ MIDI map.json")
            try f.data.write(to: url, options: .atomic)
            return SentTransferredFile(url)
        }
    }
}

// MARK: - The page's parts (RemoteFM1's sheet, redrawn)

enum MidiGlyph {
    static let back = VectorArt(24, 24, [.path("M15 5l-7 7 7 7")])
    static let tick = VectorArt(24, 24, [.path("M5 12.5l5 5L19.5 7")])
    static let x = VectorArt(24, 24, [.path("M6 6l12 12M18 6L6 18")])
    static let upDown = VectorArt(12, 12, [.path("M3 4.5L6 1.8 9 4.5M3 7.5L6 10.2 9 7.5")])
}

/// The back line ("‹ Play") and the title.
struct MidiPageHead: View {
    let back: String
    let title: String
    let go: () -> Void

    var body: some View {
        VStack(alignment: .leading, spacing: 0) {
            Button(action: go) {
                HStack(spacing: 6) {
                    IconView(MidiGlyph.back, color: SheetInk.soft.color, lineWidth: 2.6).frame(width: 15, height: 15)
                    Text(back).dm(16, 500).foregroundStyle(SheetInk.soft.color)
                }
                .frame(height: 30).contentShape(Rectangle())
            }
            .buttonStyle(PressStyle())
            .padding(.top, 8)
            Text(title).font(RFMType.unbounded(23, 700)).tracking(-0.23).foregroundStyle(SheetInk.text.color)
                .frame(height: 30).padding(.top, 9)
        }
    }
}

struct MidiHeading: View {
    let t: String
    var first = false
    init(_ t: String, first: Bool = false) { self.t = t; self.first = first }
    var body: some View {
        Text(t.uppercased()).dm(11, 600, em: 0.19).foregroundStyle(SheetInk.head.color)
            .frame(height: 11).padding(.top, first ? 12 : 21.5).padding(.bottom, 11)
    }
}

struct MidiCard<C: View>: View {
    @ViewBuilder let content: C
    var body: some View {
        VStack(spacing: 0) { content }
            .background(RoundedRectangle(cornerRadius: 20).fill(SheetInk.list.color))
            .clipShape(RoundedRectangle(cornerRadius: 20))
    }
}

struct MidiRow<C: View>: View {
    var height: CGFloat = 52
    @ViewBuilder let content: C
    var body: some View {
        HStack(spacing: 12) { content }
            .dm(15.5, 500).foregroundStyle(SheetInk.text.color)
            .padding(.leading, 17).padding(.trailing, 14).frame(minHeight: height)
            .contentShape(Rectangle())
    }
}

struct MidiRule: View {
    var body: some View { Rectangle().fill(SheetInk.rule.color).frame(height: 1) }
}

struct MidiNote: View {
    let t: String
    init(_ t: String) { self.t = t }
    var body: some View {
        Text(t).dm(13, 400).foregroundStyle(SheetInk.head.color).lineSpacing(2.5)
            .fixedSize(horizontal: false, vertical: true).padding(.top, 9)
    }
}

struct MidiChevron: View {
    var body: some View { IconView(PlayIcons.chevron, color: SheetInk.soft.color, lineWidth: 2.4).frame(width: 15, height: 15) }
}

/// RemoteFM1's switch: 51 x 31, the accent when on.
struct MidiToggle: View {
    let on: Bool
    let accent: RGB
    let go: () -> Void
    var body: some View {
        Button(action: { withAnimation(.easeInOut(duration: 0.18)) { go() } }) {
            Capsule().fill(on ? accent.color : RGB(0x4A453E).color)
                .frame(width: 51, height: 31)
                .overlay(alignment: on ? .trailing : .leading) {
                    Circle().fill(.white).frame(width: 27, height: 27).shadow(color: .black.opacity(0.25), radius: 2, y: 2).padding(2)
                }
                .frame(height: 44).contentShape(Rectangle())
        }
        .buttonStyle(.plain)
        .accessibilityAddTraits(on ? [.isSelected] : [])
    }
}

/// − | + in one capsule.
struct MidiStepper: View {
    let down: (() -> Void)?
    let up: (() -> Void)?
    var body: some View {
        HStack(spacing: 0) {
            half("−", down)
            Rectangle().fill(SheetInk.line.color).frame(width: 1, height: 34)
            half("+", up)
        }
        .background(Capsule().fill(SheetInk.tile.color))
        .clipShape(Capsule())
    }
    private func half(_ t: String, _ go: (() -> Void)?) -> some View {
        Button { go?() } label: {
            Text(t).dm(20, 500).foregroundStyle(SheetInk.text.color.opacity(go == nil ? 0.35 : 1))
                .frame(width: 46, height: 34).contentShape(Rectangle())
        }
        .buttonStyle(.plain).disabled(go == nil)
    }
}

/// ½× 1× 2×.
struct MidiSpeeds: View {
    let speed: Double
    let accent: RGB
    let set: (Double) -> Void
    var body: some View {
        HStack(spacing: 0) {
            ForEach(MidiMap.speeds, id: \.self) { v in
                let on = v == speed
                Button { set(v) } label: {
                    Text(v == 0.5 ? "½×" : v == 1 ? "1×" : "2×").dm(14, 600)
                        .foregroundStyle((on ? SheetInk.bg : SheetInk.text).color)
                        .frame(width: 50, height: 34)
                        .background(Capsule().fill(on ? accent.color : .clear))
                        .contentShape(Capsule())
                }
                .buttonStyle(.plain)
            }
        }
        .background(Capsule().fill(SheetInk.tile.color))
    }
}

struct MidiPillLabel: View {
    let t: String
    let accent: RGB
    var filled = false
    /// not filled: on the tile colour (else outlined)
    var tile = false
    init(_ t: String, accent: RGB, filled: Bool = false, tile: Bool = false) {
        self.t = t; self.accent = accent; self.filled = filled; self.tile = tile
    }
    var body: some View {
        Text(t).dm(15, 600).lineLimit(1).foregroundStyle((filled ? SheetInk.bg : SheetInk.text).color)
            .padding(.horizontal, 17).frame(height: 44)
            .background(Capsule().fill(filled ? accent.color : tile ? SheetInk.tile.color : .clear))
            .overlay(Capsule().strokeBorder(filled || tile ? .clear : SheetInk.line.color, lineWidth: 1))
            .contentShape(Capsule())
    }
}

struct MidiPill: View {
    let t: String
    let accent: RGB
    var filled = false
    var tile = false
    let go: () -> Void
    init(_ t: String, accent: RGB, filled: Bool = false, tile: Bool = false, _ go: @escaping () -> Void) {
        self.t = t; self.accent = accent; self.filled = filled; self.tile = tile; self.go = go
    }
    var body: some View {
        Button(action: go) { MidiPillLabel(t, accent: accent, filled: filled, tile: tile) }.buttonStyle(PressStyle())
    }
}

/// One learned binding: "KN1 ← CC 21 ch 1", an encoder's mode menu, ×.
struct MidiMapRow: View {
    let mapping: MidiMapping
    let pickup: Bool
    let mode: (MidiMode) -> Void
    let remove: () -> Void

    var body: some View {
        MidiRow {
            (Text(MidiHub.name(mapping.target)).fontWeight(.bold) + Text(" ← ").foregroundColor(SheetInk.off.color)
                + Text(MidiSource.label(mapping.src)).foregroundColor(SheetInk.soft.color))
                .dm(15, 500).foregroundStyle(SheetInk.text.color).lineLimit(1).minimumScaleFactor(0.8)
            if pickup {
                Text("picks up").dm(11.5, 600).foregroundStyle(SheetInk.soft.color)
                    .padding(.horizontal, 8).frame(height: 22)
                    .overlay(Capsule().strokeBorder(SheetInk.line.color, lineWidth: 1))
            }
            Spacer(minLength: 4)
            if let m = mapping.mode {
                Menu {
                    ForEach(MidiMode.allCases, id: \.self) { x in
                        Button { mode(x) } label: { Text(x.label) }
                    }
                } label: {
                    HStack(spacing: 5) {
                        Text(m.short).dm(13.5, 600).foregroundStyle(SheetInk.text.color)
                        IconView(MidiGlyph.upDown, color: SheetInk.text.color, lineWidth: 2.6).frame(width: 11, height: 11)
                    }
                    .padding(.horizontal, 10).frame(height: 30)
                    .background(Capsule().fill(SheetInk.tile.color))
                }
                .menuStyle(.button).buttonStyle(.plain).fixedSize()
            }
            Button(action: remove) {
                IconView(MidiGlyph.x, color: SheetInk.soft.color, lineWidth: 2.4).frame(width: 15, height: 15)
                    .frame(width: 32, height: 44).contentShape(Rectangle())
            }
            .buttonStyle(.plain)
            .accessibilityLabel("Forget \(mapping.target)")
        }
    }
}

extension MidiMode {
    /// The list's short name (mock-up 55): absolute, rel. 64 ± n ..
    var short: String {
        switch self {
        case .abs: "absolute"
        case .rel64: "rel. 64 ± n"
        case .rel2c: "rel. 2's comp."
        case .relsign: "rel. sign bit"
        }
    }
}
