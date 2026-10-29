// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// PLAY's pull-down sheet (mock-up 30, after RemoteFM1's PhoneSheet): SOUND (THIS iPHONE, or the FM-1's track when one
// is the unit), VIEW (STEPS among them, mock-up 38), THEME (the five and FIZZ), FIZZ's version, the emulated FIZZ's MEMORY with RESET, MIDI & AUDIO,
// and the app's tab bar at its foot. Swiped up, DONE or a tap on the scrim puts it away.

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
    private var accent: RGB { play.theme.accent }

    var body: some View {
        let compact = height < 600
        VStack(spacing: 0) {
            Group {
                if compact {
                    ScrollView { content.padding(.bottom, 8) }.scrollIndicators(.hidden).frame(maxHeight: height - top - 70)
                } else {
                    content
                }
            }
            TabBar(tab: Binding(get: { tab }, set: { t in play.sheet = false; tab = t }))
                .padding(.horizontal, -6).padding(.top, 18)
            Capsule().fill(SheetInk.off.color).frame(width: 40, height: 5).frame(height: 30)
                .frame(maxWidth: .infinity).contentShape(Rectangle())
                .onTapGesture { play.sheet = false }
        }
        .padding(.top, max(top, 12) + 8).padding(.horizontal, 18)
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
                HStack(spacing: 9) {
                    IconView(PlayIcons.fm1, color: accent.color, lineWidth: 2).frame(width: 28, height: 28)
                    Text("PLAY").fz(22, 800, em: 0.04).foregroundStyle(SheetInk.text.color)
                }
                Spacer(minLength: 0)
                Button { play.sheet = false } label: {
                    Text("DONE").fz(14, 800, em: 0.06).foregroundStyle(SheetInk.text.color)
                        .padding(.horizontal, 18).frame(height: 44)
                        .background(Capsule().fill(SheetInk.tile.color))
                }
                .buttonStyle(PressStyle())
            }
            .frame(height: 44)
            head("SOUND")
            sound
            head("VIEW")
            views
            head("THEME")
            themes
            list.padding(.top, 18)
        }
    }

    private func head(_ t: String) -> some View {
        Text(t).fz(12, 800, em: 0.16).foregroundStyle(SheetInk.head.color).padding(.top, 18).padding(.bottom, 9)
    }

    // MARK: SOUND

    private var sound: some View {
        HStack(spacing: 8) {
            source(title: EmulatorHost.hereName, detail: "FIZZ \(host.version)",
                   on: play.soundNow == .here, dot: accent) { play.sound = .here }
            if play.fm1Offered, let u = model.unit {
                let t = model.editor.trackShown
                source(title: "FM-1", detail: "\(u.via)\(t >= 0 ? " · TRACK \(t + 1)" : "")", on: play.soundNow == .fm1,
                       dot: RGB(0x3BD47A)) { play.letGoAll(); play.sound = .fm1 }
            }
        }
    }

    private func source(title: String, detail: String, on: Bool, dot: RGB, _ go: @escaping () -> Void) -> some View {
        Button(action: go) {
            VStack(alignment: .leading, spacing: 7) {
                HStack(spacing: 8) {
                    Circle().fill(on || dot != accent ? dot.color : SheetInk.off.color).frame(width: 9, height: 9)
                        .shadow(color: dot.opacity(on ? 1 : 0), radius: 4)
                    Text(title).fz(15, 800, em: 0.05).foregroundStyle(SheetInk.text.color).lineLimit(1)
                }
                Text(detail).fz(13, 700, em: 0.02).foregroundStyle(SheetInk.soft.color).lineLimit(1).minimumScaleFactor(0.7)
            }
            .padding(.horizontal, 14)
            .frame(maxWidth: .infinity, minHeight: 76, alignment: .leading)
            .background(RoundedRectangle(cornerRadius: 20).fill(on ? SheetInk.tile.color : .clear))
            .overlay(RoundedRectangle(cornerRadius: 20).strokeBorder(on ? accent.color : SheetInk.line.color, lineWidth: on ? 2 : 1))
            .shadow(color: accent.opacity(on ? 0.3 : 0), radius: 9)
            .contentShape(RoundedRectangle(cornerRadius: 20))
        }
        .buttonStyle(PressStyle())
    }

    // MARK: VIEW

    private var views: some View {
        HStack(spacing: 8) {
            ForEach(PlayViewKind.allCases, id: \.self) { v in
                let on = play.view == v
                Button { play.view = v } label: {
                    VStack(spacing: 9) {
                        ViewGlyph(kind: v, accent: accent).frame(width: 48, height: 40)
                        Text(v.title).fz(12, 800, em: 0.08).foregroundStyle(SheetInk.text.color)
                            .lineLimit(1).minimumScaleFactor(0.8)
                    }
                    .frame(maxWidth: .infinity).frame(height: 90)
                    .background(RoundedRectangle(cornerRadius: 18).fill(on ? SheetInk.tile.color : .clear))
                    .overlay(RoundedRectangle(cornerRadius: 18).strokeBorder(on ? accent.color : SheetInk.line.color, lineWidth: on ? 2 : 1))
                    .contentShape(RoundedRectangle(cornerRadius: 18))
                }
                .buttonStyle(PressStyle())
            }
        }
    }

    // MARK: THEME

    private var themes: some View {
        HStack(spacing: 8) {
            ForEach(PlayTheme.allCases) { t in
                let p = t.palette, on = play.theme == t
                Button { withAnimation(.easeInOut(duration: 0.25)) { play.theme = t } } label: {
                    Circle().fill(p.body.color)
                        .overlay(Circle().strokeBorder(Color(white: 0.23), lineWidth: t == .fizz ? 1.5 : 0))
                        .overlay(Circle().fill(p.bed.color).padding(9))
                        .overlay(Circle().fill(p.led.color).frame(width: 8, height: 8))
                        .frame(width: 40, height: 40)
                        .frame(width: 48, height: 48)
                        .overlay(Circle().strokeBorder(on ? t.accent.color : .clear, lineWidth: 2.5))
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
            row {
                Text("FIZZ")
                Spacer(minLength: 8)
                Text(host.version).foregroundStyle(SheetInk.soft.color).lineLimit(1)
            }
            rule
            HStack(spacing: 14) {
                Text("MEMORY")
                VStack(alignment: .leading, spacing: 7) {
                    StoreMeter(use: use, height: 12)
                    HStack(spacing: 6) {
                        RoundedRectangle(cornerRadius: 2).fill(FZ.green.color).frame(width: 9, height: 9)
                        Text(st.map { "\($0.projects)" } ?? "–").fz(16, 800).foregroundStyle(SheetInk.text.color)
                            .padding(.trailing, 8)
                        RoundedRectangle(cornerRadius: 2).fill(FZ.cream.color).frame(width: 9, height: 9)
                        Text(st.map { "\($0.mine)" } ?? "–").fz(16, 800).foregroundStyle(SheetInk.text.color)
                    }
                }
                Button { askReset() } label: {
                    Text(resetting ? "…" : "RESET").fz(13, 800, em: 0.08).foregroundStyle(FZ.red.color)
                        .padding(.horizontal, 16).frame(height: 44)
                        .overlay(Capsule().strokeBorder(FZ.red.opacity(0.6), lineWidth: 1.5))
                        .contentShape(Capsule())
                }
                .buttonStyle(PressStyle())
                .disabled(resetting)
            }
            .fz(14, 800, em: 0.08).foregroundStyle(SheetInk.text.color)
            .padding(.horizontal, 16).frame(minHeight: 76)
            rule
            row {
                Text("MIDI & AUDIO")
                Spacer(minLength: 8)
                Text(host.mode == .audio ? "44.1 kHz" : host.mode == .timer ? "SILENT" : "OFF")
                    .foregroundStyle(SheetInk.soft.color)
            }
        }
        .background(RoundedRectangle(cornerRadius: 20).fill(SheetInk.list.color))
        .clipShape(RoundedRectangle(cornerRadius: 20))
    }

    private func row<C: View>(@ViewBuilder _ c: () -> C) -> some View {
        HStack(spacing: 12) { c() }
            .fz(14, 800, em: 0.08).foregroundStyle(SheetInk.text.color)
            .padding(.horizontal, 16).frame(minHeight: 54)
    }

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

/// The VIEW tiles' small pictures (FULL, PANEL, KEYS, STEPS, SCREEN).
struct ViewGlyph: View {
    let kind: PlayViewKind
    let accent: RGB

    var body: some View {
        let lt = SheetInk.text.color, dk = SheetInk.off.color
        VStack(spacing: 4) {
            switch kind {
            case .full:
                HStack(spacing: 3) {
                    RoundedRectangle(cornerRadius: 3).fill(dk).frame(width: 12)
                    RoundedRectangle(cornerRadius: 3).fill(lt).frame(width: 20)
                    RoundedRectangle(cornerRadius: 3).fill(dk)
                }
                .frame(width: 48, height: 20)
                RoundedRectangle(cornerRadius: 3).fill(accent.color).frame(width: 48)
            case .panel:
                RoundedRectangle(cornerRadius: 3).fill(lt).frame(width: 22, height: 22)
                RoundedRectangle(cornerRadius: 3).fill(dk).frame(width: 48)
            case .keys:
                RoundedRectangle(cornerRadius: 3).fill(lt).frame(width: 20, height: 20)
                RoundedRectangle(cornerRadius: 3).fill(accent.color).frame(width: 48)
            case .steps:
                HStack(spacing: 3) {
                    RoundedRectangle(cornerRadius: 3).fill(lt).frame(width: 14, height: 14)
                    VStack(spacing: 2) {
                        ForEach(0..<2, id: \.self) { r in
                            HStack(spacing: 2) {
                                ForEach(0..<4, id: \.self) { c in
                                    RoundedRectangle(cornerRadius: 1.5).fill(r == 0 && c == 0 ? accent.color : dk)
                                }
                            }
                        }
                    }
                }
                .frame(width: 48, height: 14)
                HStack(spacing: 2) {
                    ForEach(0..<7, id: \.self) { _ in RoundedRectangle(cornerRadius: 1.5).fill(lt) }
                }
                .frame(width: 48)
            case .screen:
                RoundedRectangle(cornerRadius: 3).fill(lt).frame(width: 38, height: 38).padding(.top, 1)
            }
        }
        .frame(width: 48, height: 40, alignment: .top)
    }
}
