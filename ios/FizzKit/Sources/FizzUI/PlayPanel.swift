// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// PLAY's pull-down sheet (mock-up 53, RemoteFM1's PhoneSheet in its numbers): #17140F from the top, 20 pt margins,
// DM Sans in sentence case, the title in Unbounded. Sound source (This iPhone, or the FM-1's track when one is the
// unit), the five views (Steps among them), the six themes, FIZZ's version, the emulated FIZZ's memory with Reset,
// MIDI & audio, and the app's tab bar above the grabber. Swiped up, Done, the grabber or a tap on the scrim puts it away.

import SwiftUI
import FizzEngine
import FizzKit

struct PlaySheet: View {
    let model: UnitModel
    let host: EmulatorHost
    @Binding var tab: FizzTab
    let top: CGFloat
    let width: CGFloat
    let height: CGFloat
    @State private var ask: ConfirmAsk?
    @State private var resetting = false
    @State private var pull: CGFloat = 0

    private var play: PlayModel { host.play }
    /// the sheet's accent (RemoteFM1's #FF8A4C; FIZZ's cyan on the FIZZ theme)
    private var accent: RGB { play.theme.accent }

    var body: some View {
        let compact = height < 600
        VStack(spacing: 0) {
            Group {
                if compact {
                    ScrollView { content.padding(.bottom, 8) }.scrollIndicators(.hidden).frame(maxHeight: height - top - 90)
                } else {
                    content
                }
            }
            TabBar(tab: Binding(get: { tab }, set: { t in play.sheet = false; tab = t }))
                .padding(.horizontal, -4).padding(.top, 16)
            Capsule().fill(SheetInk.off.color).frame(width: 40, height: 5).frame(height: 34)
                .frame(maxWidth: .infinity).contentShape(Rectangle())
                .onTapGesture { play.sheet = false }
        }
        .padding(.top, max(top, 0) + 7).padding(.horizontal, 20)
        .frame(maxWidth: min(width, 520))
        .background(
            UnevenRoundedRectangle(bottomLeadingRadius: 34, bottomTrailingRadius: 34).fill(SheetInk.bg.color)
                .shadow(color: .black.opacity(0.45), radius: 20, y: 18)
        )
        .frame(maxWidth: .infinity)
        .offset(y: min(0, pull))
        .gesture(DragGesture(minimumDistance: 12).onChanged { pull = $0.translation.height }.onEnded { v in
            if v.translation.height < -60 || v.predictedEndTranslation.height < -160 { play.sheet = false }
            withAnimation(.spring(response: 0.3, dampingFraction: 0.85)) { pull = 0 }
        })
        .confirmCard($ask)
        .task { if !model.onEmulator { await host.readStatus() } }
    }

    @ViewBuilder private var content: some View {
        VStack(alignment: .leading, spacing: 0) {
            HStack {
                Text("Play").font(RFMType.unbounded(24, 700)).tracking(-0.24).foregroundStyle(SheetInk.text.color)
                Spacer(minLength: 0)
                Button { play.sheet = false } label: {
                    Text("Done").dm(15, 600).foregroundStyle(SheetInk.text.color)
                        .padding(.horizontal, 15).frame(height: 44)
                        .background(Capsule().fill(SheetInk.tile.color))
                }
                .buttonStyle(PressStyle())
            }
            .frame(height: 48)
            head("Sound source", first: true)
            sound
            head("View · or swipe sideways")
            views
            head("Theme")
            themes
            list.padding(.top, 20)
        }
    }

    /// A section's small spaced heading.
    private func head(_ t: String, first: Bool = false) -> some View {
        Text(t.uppercased()).dm(11, 600, em: 0.19).foregroundStyle(SheetInk.head.color)
            .frame(height: 11)
            .padding(.top, first ? 18 : 21.5).padding(.bottom, 11)
    }

    // MARK: Sound source

    private var sound: some View {
        HStack(spacing: 8) {
            source(title: EmulatorHost.hereName.sentenceCased, detail: "FIZZ, play by touch",
                   on: play.soundNow == .here, dot: accent) { play.sound = .here }
            // (the FM-1's card always there, as RemoteFM1's; dimmed while no FM-1 is the unit)
            if play.fm1Offered, let u = model.unit {
                let t = model.editor.trackShown
                source(title: "FM-1", detail: "\(u.via)\(t >= 0 ? " · track \(t + 1)" : "")", on: play.soundNow == .fm1,
                       dot: RGB(0x3BD47A)) { play.letGoAll(); play.sound = .fm1 }
            } else {
                source(title: "FM-1", detail: "Not connected", on: false, dot: SheetInk.off) {}
                    .disabled(true).opacity(0.5)
            }
        }
    }

    private func source(title: String, detail: String, on: Bool, dot: RGB, _ go: @escaping () -> Void) -> some View {
        Button(action: go) {
            VStack(alignment: .leading, spacing: 6) {
                HStack(spacing: 9) {
                    Circle().fill(on || dot != accent ? dot.color : SheetInk.off.color).frame(width: 8, height: 8)
                    Text(title).dm(16.5, 600).foregroundStyle(SheetInk.text.color).lineLimit(1)
                }
                Text(detail).dm(13.5, 400).foregroundStyle(SheetInk.soft.color).lineLimit(1).minimumScaleFactor(0.7)
            }
            .padding(.horizontal, 13)
            .frame(maxWidth: .infinity, minHeight: 80, alignment: .leading)
            .background(RoundedRectangle(cornerRadius: 20).fill(on ? SheetInk.tile.color : .clear))
            .overlay(RoundedRectangle(cornerRadius: 20).strokeBorder(on ? accent.color : SheetInk.line.color, lineWidth: on ? 1.5 : 1))
            .contentShape(RoundedRectangle(cornerRadius: 20))
        }
        .buttonStyle(PressStyle())
    }

    // MARK: View

    private var views: some View {
        HStack(spacing: 8) {
            ForEach(PlayViewKind.allCases, id: \.self) { v in
                let on = play.view == v
                Button { play.view = v } label: {
                    VStack(spacing: 10) {
                        ViewGlyph(kind: v, accent: accent).frame(width: 44, height: 40)
                        Text(v.name).dm(13.5, 600).foregroundStyle(SheetInk.text.color).lineLimit(1).minimumScaleFactor(0.8)
                    }
                    .frame(maxWidth: .infinity).frame(height: 98)
                    .background(RoundedRectangle(cornerRadius: 20).fill(on ? SheetInk.tile.color : .clear))
                    .overlay(RoundedRectangle(cornerRadius: 20).strokeBorder(on ? accent.color : SheetInk.line.color, lineWidth: on ? 1.5 : 1))
                    .contentShape(RoundedRectangle(cornerRadius: 20))
                }
                .buttonStyle(PressStyle())
            }
        }
    }

    // MARK: Theme

    private var themes: some View {
        HStack(spacing: 10) {
            ForEach(PlayTheme.allCases) { t in
                let p = t.palette, on = play.theme == t
                Button { withAnimation(.easeInOut(duration: 0.25)) { play.theme = t } } label: {
                    Circle().fill(p.body.color)
                        .overlay(Circle().strokeBorder(RGB(0x3a3a44).color, lineWidth: t == .fizz ? 1.5 : 0))
                        .overlay(Circle().fill(p.bed.color).frame(width: 22, height: 22))
                        .overlay(Circle().fill(p.led.color).frame(width: 9, height: 9))
                        .frame(width: 40, height: 40)
                        .frame(width: 48, height: 48)
                        .overlay(Circle().strokeBorder(on ? accent.color : .clear, lineWidth: 4))
                        .contentShape(Circle())
                }
                .buttonStyle(PressStyle(scale: 0.9))
                .accessibilityLabel(t.name)
            }
        }
    }

    // MARK: the list

    private var list: some View {
        let st = model.onEmulator ? model.status : host.status
        let use = model.onEmulator ? model.storeUse : st.flatMap { s in
            s.lib.count == 2 && s.prj.count == 2 && s.lib[1] + s.prj[1] > 0 ? StoreUse(lib: s.lib, prj: s.prj) : nil
        }
        return VStack(spacing: 0) {
            row(56) {
                Text("Firmware")
                Spacer(minLength: 8)
                Text("FIZZ \(host.version)").foregroundStyle(SheetInk.head.color).lineLimit(1)
            }
            rule
            row(70) {
                Text("Memory")
                VStack(alignment: .leading, spacing: 7) {
                    SheetMeter(use: use)
                    Text(st.map { "\(Self.count($0.projects, "project")) · \(Self.count($0.mine, "preset"))" } ?? "–")
                        .dm(13, 400).foregroundStyle(SheetInk.soft.color).lineLimit(1).minimumScaleFactor(0.8)
                }
                .frame(maxWidth: .infinity)
                Button { askReset() } label: {
                    Text(resetting ? "…" : "Reset").dm(14, 600).foregroundStyle(RGB(0xFF6464).color)
                        .padding(.horizontal, 14).frame(height: 36)
                        .overlay(Capsule().strokeBorder(RGB(0xFF6464).opacity(0.6), lineWidth: 1))
                        .frame(height: 44)
                        .contentShape(Capsule())
                }
                .buttonStyle(PressStyle())
                .disabled(resetting)
            }
            rule
            row(56) {
                Text("MIDI & audio")
                Spacer(minLength: 8)
                Text(host.mode == .audio ? "44.1 kHz" : host.mode == .timer ? "Silent" : "Off")
                    .foregroundStyle(SheetInk.head.color)
                IconView(PlayIcons.chevron, color: SheetInk.soft.color, lineWidth: 2.4).frame(width: 15, height: 15)
            }
        }
        .background(RoundedRectangle(cornerRadius: 20).fill(SheetInk.list.color))
        .clipShape(RoundedRectangle(cornerRadius: 20))
    }

    private func row<C: View>(_ h: CGFloat, @ViewBuilder _ c: () -> C) -> some View {
        HStack(spacing: 12) { c() }
            .dm(15.5, 500).foregroundStyle(SheetInk.text.color)
            .padding(.leading, 17).padding(.trailing, 16).frame(minHeight: h)
    }

    /// "1 project", "6 projects"
    static func count(_ n: Int, _ what: String) -> String { "\(n) \(what)\(n == 1 ? "" : "s")" }

    private var rule: some View { Rectangle().fill(SheetInk.rule.color).frame(height: 1) }

    private func askReset() {
        ask = ConfirmAsk(title: "RESET \(EmulatorHost.hereName)", what: "A fresh FIZZ",
                         detail: "Its presets and projects are wiped.", go: "RESET", color: FZ.red) {
            resetting = true
            Task {
                play.letGoAll()
                await host.reset()
                if model.onEmulator { await model.refresh() } else { await host.readStatus() }
                resetting = false
            }
        }
    }
}

/// The memory meter (mock-up 53): presets used in cream, projects used in green, on the sheet's dark.
struct SheetMeter: View {
    let use: StoreUse?
    @Environment(\.fizzSnapshot) private var snapshot
    @State private var shown = false

    var body: some View {
        let parts = use?.parts ?? [0, 0, 0, 0]
        GeometryReader { g in
            HStack(spacing: 0) {
                Rectangle().fill(FZ.cream.color).frame(width: g.size.width * parts[0])
                Rectangle().fill(FZ.green.color).frame(width: g.size.width * parts[2])
                Spacer(minLength: 0)
            }
            .scaleEffect(x: shown || snapshot ? 1 : 0.001, anchor: .leading)
        }
        .frame(height: 10)
        .background(RGB(0x15120F).color)
        .clipShape(Capsule())
        .onAppear { withAnimation(.timingCurve(0.2, 0.8, 0.2, 1, duration: 1.2)) { shown = true } }
    }
}

/// The view tiles' small pictures (Full, Panel, Keys, Steps, Screen; mock-up 53).
struct ViewGlyph: View {
    let kind: PlayViewKind
    let accent: RGB

    var body: some View {
        let lt = SheetInk.text.color, dk = SheetInk.off.color, ac = accent.color
        let r: CGFloat = 3
        VStack(spacing: 4) {
            switch kind {
            case .full:
                HStack(spacing: 3) {
                    RoundedRectangle(cornerRadius: r).fill(dk).frame(width: 10)
                    RoundedRectangle(cornerRadius: r).fill(lt)
                    RoundedRectangle(cornerRadius: r).fill(dk).frame(width: 10)
                }
                .frame(width: 42, height: 19)
                RoundedRectangle(cornerRadius: r).fill(ac).frame(width: 42, height: 14)
            case .panel:
                RoundedRectangle(cornerRadius: r).fill(lt).frame(width: 20, height: 20)
                RoundedRectangle(cornerRadius: r).fill(dk).frame(width: 42, height: 12)
            case .keys:
                RoundedRectangle(cornerRadius: r).fill(lt).frame(width: 20, height: 20)
                RoundedRectangle(cornerRadius: r).fill(ac).frame(width: 42, height: 12)
            case .steps:
                HStack(spacing: 3) {
                    RoundedRectangle(cornerRadius: r).fill(lt).frame(width: 16)
                    RoundedRectangle(cornerRadius: r).fill(dk)
                }
                .frame(width: 42, height: 16)
                HStack(spacing: 2) {
                    ForEach(0..<4, id: \.self) { i in RoundedRectangle(cornerRadius: r).fill(i % 2 == 0 ? ac : dk).frame(height: 5) }
                }
                .frame(width: 42)
                RoundedRectangle(cornerRadius: r).fill(ac).frame(width: 42, height: 8)
            case .screen:
                RoundedRectangle(cornerRadius: r).fill(lt).frame(width: 34, height: 34)
            }
        }
        .frame(width: 44, height: 40)
    }
}

extension PlayViewKind {
    /// The sheet's tile name (sentence case).
    var name: String {
        switch self {
        case .full: "Full"
        case .panel: "Panel"
        case .keys: "Keys"
        case .steps: "Steps"
        case .screen: "Screen"
        }
    }
}

extension String {
    /// "THIS iPHONE" → "This iPhone" (the sheet's sentence case; the device's own casing kept).
    var sentenceCased: String {
        replacingOccurrences(of: "THIS ", with: "This ").replacingOccurrences(of: "iPHONE", with: "iPhone")
            .replacingOccurrences(of: "iPAD", with: "iPad")
    }
}
