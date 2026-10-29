// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// PLAY (mock-ups 29-33): the emulated FIZZ, full-screen, on the owner's RemoteFM1 panel with FIZZ's names. Its views:
// FULL (knobs, F1 / F2, the screen, KN1-4, the buttons, the keybed), the linked PANEL / KEYS (the screen big, one
// knob set, BUTTONS or KEYS below), SCREEN; a swipe sideways on the screen moves between them. Landscape and the iPad
// lay it all out in one band (PhoneWide, PadWide). The handle (or a swipe down) drops the sheet (PlayPanel.swift),
// which carries the app's tab bar.

import SwiftUI
import FizzEngine
import FizzKit
#if os(iOS)
import UIKit
#endif

public struct PlayView: View {
    let model: UnitModel
    let host: EmulatorHost
    @Binding var tab: FizzTab
    /// the iPad (its slim rail beside)
    var wide = false
    @Environment(\.fizzInsets) private var insets
    @Environment(\.fizzSnapshot) private var snapshot

    public init(model: UnitModel, host: EmulatorHost, tab: Binding<FizzTab>, wide: Bool = false) {
        self.model = model
        self.host = host
        _tab = tab
        self.wide = wide
    }

    private var play: PlayModel { host.play }

    public var body: some View {
        GeometryReader { g in
            let top = max(g.safeAreaInsets.top, insets.top)
            let full = CGSize(width: g.size.width + g.safeAreaInsets.leading + g.safeAreaInsets.trailing,
                              height: g.size.height + g.safeAreaInsets.top + g.safeAreaInsets.bottom)
            let p = play.theme.palette
            ZStack(alignment: .topLeading) {
                p.body.color
                board(full: full, top: top)
                if play.sheet {
                    Color(.sRGB, red: 10 / 255, green: 8 / 255, blue: 6 / 255, opacity: 0.5)
                        .onTapGesture { closeSheet() }
                        .transition(.opacity)
                    PlaySheet(model: model, host: host, tab: $tab, top: top, width: full.width, height: full.height)
                        .transition(.move(edge: .top))
                }
            }
            .frame(width: full.width, height: full.height)
            .ignoresSafeArea()
        }
        .animation(.spring(response: 0.45, dampingFraction: 0.86), value: play.sheet)
        .animation(.spring(response: 0.4, dampingFraction: 0.85), value: play.view)
        .persistentSystemOverlays(.hidden)
        .deferringSystemGestures()
        .task { if !snapshot { await play.follow() } }
        .onAppear { host.playShown = true; turnable(true) }
        .onDisappear { play.letGoAll(); play.sheet = false; host.playShown = false; turnable(false) }
    }

    private func closeSheet() { play.sheet = false }

    private func turnable(_ on: Bool) {
        #if os(iOS)
        if !snapshot { FizzAppDelegate.allowLandscape(on) }
        #endif
    }

    // MARK: the layouts

    @ViewBuilder
    private func board(full: CGSize, top: CGFloat) -> some View {
        if full.width > full.height {
            if wide { padWide(full, top) } else { phoneWide(full) }
        } else {
            phonePortrait(full, top)
        }
    }

    /// Portrait (390 x 798 below the status bar; the iPad standing too).
    @ViewBuilder
    private func phonePortrait(_ full: CGSize, _ top: CGFloat) -> some View {
        let y0 = max(0, top - 4)
        let u = min(full.width / 390, (full.height - y0) / 798, wide ? 1.6 : 1.2)
        let x0 = (full.width - 390 * u) / 2
        let b = Board(u: u)
        ZStack(alignment: .topLeading) {
            header(b, CGRect(x: 16, y: 4, width: 358, height: 36))
            switch play.view {
            case .full: full390(b)
            case .panel, .keys: linked390(b)
            case .screen: screen390(b)
            }
        }
        .frame(width: 390 * u, height: 798 * u, alignment: .topLeading)
        .offset(x: x0, y: y0)
    }

    /// FULL (mock-up 29).
    @ViewBuilder
    private func full390(_ b: Board) -> some View {
        mainKnobs(b, size: 46, gap: 8).place(b, 16, 50, 148, 140)
        FPair(model: play, w: b.s(148), h: b.s(52), font: b.s(14)).place(b, 16, 200, 148, 52)
        screen(b, 202).place(b, 172, 50, 202, 202)
        HStack(spacing: 0) {
            PanelLED(size: b.s(10))
            Spacer(minLength: 0)
            knRow(b, size: 46, spaced: true)
        }
        .place(b, 16, 270, 358, 70)
        well(b, bw: 52, bh: 42, gap: 6, font: 12).place(b, 14, 354, 362, 110)
        LEDKeybed(model: play, w: b.s(358), h: b.s(272), keyW: b.s(40)).place(b, 16, 484, 358, 272)
    }

    /// PANEL / KEYS (mock-up 31): the screen big, one knob set, BUTTONS or KEYS.
    @ViewBuilder
    private func linked390(_ b: Board) -> some View {
        let p = play.theme.palette
        screen(b, 340).place(b, 25, 46, 340, 340)
        Group {
            if play.knobsKN { knRow(b, size: 50, spaced: true) } else { mainRow(b, size: 50) }
        }
        .place(b, 12, 398, 366, 74)
        HStack(spacing: b.s(6)) {
            setChip(b, "MAIN", on: !play.knobsKN) { play.knobsKN = false }
            setChip(b, "KN 1–4", on: play.knobsKN) { play.knobsKN = true }
        }
        .place(b, 0, 476, 390, 36)
        HStack(spacing: 0) {
            deckChip(b, "BUTTONS", on: play.view == .panel) { play.view = .panel }
            deckChip(b, "KEYS", on: play.view == .keys) { play.view = .keys }
        }
        .padding(b.s(3))
        .background(Capsule().fill(.black.opacity(0.16)))
        .place(b, 95, 520, 200, 40)
        if play.view == .keys {
            LEDKeybed(model: play, w: b.s(358), h: b.s(184), keyW: b.s(40), numbers: true).place(b, 16, 568, 358, 184)
        } else {
            well(b, bw: 50, bh: 48, gap: 8, font: 12).place(b, 15, 566, 360, 124)
            FPair(model: play, w: b.s(200), h: b.s(48), font: b.s(14)).place(b, 95, 702, 200, 48)
        }
        dots(b, p).place(b, 0, 756, 390, 28)
    }

    /// SCREEN: the screen alone.
    @ViewBuilder
    private func screen390(_ b: Board) -> some View {
        screen(b, 358).place(b, 16, 197, 358, 358)
        dots(b, play.theme.palette).place(b, 0, 756, 390, 28)
    }

    /// iPhone landscape (mock-up 32, PhoneWide): 844 x 390, the whole screen.
    @ViewBuilder
    private func phoneWide(_ full: CGSize) -> some View {
        let u = min(full.width / 844, full.height / 390)
        let b = Board(u: u)
        ZStack(alignment: .topLeading) {
            handle(b).place(b, 378, 0, 88, 32)
            mainKnobs(b, size: 38, gap: 4).place(b, 40, 18, 156, 122)
            FPair(model: play, w: b.s(136), h: b.s(48), font: b.s(14)).place(b, 56, 144, 136, 48)
            screen(b, 172).place(b, 202, 20, 172, 172)
            HStack(spacing: 0) {
                PanelLED(size: b.s(10))
                Spacer(minLength: 0)
                knRow(b, size: 38, spaced: true)
            }
            .place(b, 386, 16, 408, 60)
            well(b, bw: 58, bh: 38, gap: 8, font: 12).place(b, 386, 88, 408, 104)
            LEDKeybed(model: play, w: b.s(744), h: b.s(170)).place(b, 50, 206, 744, 170)
        }
        .frame(width: 844 * u, height: 390 * u, alignment: .topLeading)
        .offset(x: (full.width - 844 * u) / 2, y: (full.height - 390 * u) / 2)
    }

    /// iPad landscape (mock-up 33, PadWide): 1096 x 780 beside the slim rail.
    @ViewBuilder
    private func padWide(_ full: CGSize, _ top: CGFloat) -> some View {
        let y0 = top
        let u = min(full.width / 1096, (full.height - y0) / 780)
        let b = Board(u: u)
        ZStack(alignment: .topLeading) {
            header(b, CGRect(x: 36, y: 14, width: 1024, height: 36), badge: false)
            mainKnobs(b, size: 64, gap: 24).place(b, 36, 70, 192, 216)
            FPair(model: play, w: b.s(192), h: b.s(56), font: b.s(16)).place(b, 36, 300, 192, 56)
            screen(b, 300).place(b, 256, 66, 300, 300)
            HStack(spacing: 0) {
                PanelLED(size: b.s(12))
                Spacer(minLength: 0)
                knRow(b, size: 64, spaced: true)
            }
            .place(b, 588, 66, 472, 96)
            well(b, bw: 67, bh: 60, gap: 10, font: 14).place(b, 588, 214, 472, 150)
            LEDKeybed(model: play, w: b.s(1024), h: b.s(356)).place(b, 36, 404, 1024, 356)
        }
        .frame(width: 1096 * u, height: 780 * u, alignment: .topLeading)
        .offset(x: (full.width - 1096 * u) / 2, y: y0)
    }

    // MARK: the parts placed

    /// The pill (THIS iPHONE, or FM-1 while the keys play it), the handle, FIZZ.
    @ViewBuilder
    private func header(_ b: Board, _ r: CGRect, badge: Bool = true) -> some View {
        let p = play.theme.palette
        ZStack {
            HStack {
                Button { play.sheet = true } label: {
                    HStack(spacing: b.s(7)) {
                        if play.soundNow == .fm1 {
                            Circle().fill(RGB(0x3BD47A).color).frame(width: b.s(8), height: b.s(8))
                                .shadow(color: RGB(0x3BD47A).color, radius: 4)
                            Text("FM-1").fz(b.s(13), 800, em: 0.06)
                        } else {
                            IconView(PlayIcons.phone, color: p.label.color, lineWidth: 2.4).frame(width: b.s(16), height: b.s(16))
                            Text(EmulatorHost.hereName).fz(b.s(13), 800, em: 0.06)
                        }
                    }
                    .foregroundStyle(p.label.color)
                    .padding(.leading, b.s(10)).padding(.trailing, b.s(13))
                    .frame(height: b.s(32))
                    .background(Capsule().fill(.black.opacity(0.14)))
                    .fixedSize()
                }
                .buttonStyle(PressStyle())
                Spacer(minLength: 0)
                if badge {
                    HStack(spacing: b.s(6)) {
                        BubbleLogo().frame(width: b.s(26), height: b.s(26))
                        FizzWord(size: b.s(17))
                    }
                    .padding(.leading, b.s(6)).padding(.trailing, b.s(13))
                    .frame(height: b.s(36))
                    .background(Capsule().fill(RGB(0x0b0b0e).color).shadow(color: .black.opacity(0.22), radius: 0, y: 3))
                }
            }
            handle(b).frame(width: b.s(88), height: b.s(44))
        }
        .background(Color.clear.contentShape(Rectangle()).gesture(DragGesture(minimumDistance: 12).onEnded { v in
            if v.translation.height > 24 { play.sheet = true }
        }))
        .place(b, r.minX, r.minY, r.width, r.height)
    }

    /// The handle: a tap or a swipe down drops the sheet.
    private func handle(_ b: Board) -> some View {
        Capsule().fill(play.theme.palette.label.color).opacity(0.55).frame(width: b.s(40), height: b.s(5))
            .frame(width: b.s(88), height: b.s(44))
            .contentShape(Rectangle())
            .gesture(sheetPull)
    }

    /// A tap, or a pull down: the sheet.
    private var sheetPull: some Gesture {
        DragGesture(minimumDistance: 0).onEnded { v in
            if v.translation.height > 16 || hypot(v.translation.width, v.translation.height) < 10 { play.sheet = true }
        }
    }

    /// The LCD; a swipe sideways on it moves to the next view.
    private func screen(_ b: Board, _ s: CGFloat) -> some View {
        LCDScreen(image: play.lcd ?? (snapshot ? host.emulator?.lcdImage : nil), size: b.s(s), p: play.theme.palette)
            .contentShape(Rectangle())
            .gesture(DragGesture(minimumDistance: 24).onEnded { v in
                guard abs(v.translation.width) > 50, abs(v.translation.width) > abs(v.translation.height) else { return }
                play.view = PlayModel.swiped(play.view, by: v.translation.width < 0 ? 1 : -1)
            })
    }

    private func knob(_ b: Board, _ label: String, _ k: FizzKnob?, size: CGFloat) -> some View {
        let p = play.theme.palette
        let mark: RGB = switch k {
        case .select: KnobInk.select
        case .algo: KnobInk.algorithm
        case .k1: KnobInk.kn[0]
        case .k2: KnobInk.kn[1]
        case .k3: KnobInk.kn[2]
        case .k4: KnobInk.kn[3]
        default: p.mark
        }
        let play = self.play
        if let k {
            return PlayKnob(label: label, size: b.s(size), angle: play.angles[k] ?? 0, mark: mark, p: p,
                            turn: { play.turn(k, by: $0) })
        }
        return PlayKnob(label: label, size: b.s(size), angle: play.masterAngle, mark: mark, p: p,
                        absolute: (start: { play.master }, set: { play.setMaster($0) }))
    }

    /// MASTER, SELECT / PRESETS, ALGORITHM in two rows.
    private func mainKnobs(_ b: Board, size: CGFloat, gap: CGFloat) -> some View {
        VStack(spacing: b.s(gap)) {
            HStack(spacing: 0) {
                knob(b, "MASTER", nil, size: size).frame(maxWidth: .infinity)
                knob(b, "SELECT", .select, size: size).frame(maxWidth: .infinity)
            }
            HStack(spacing: 0) {
                knob(b, "PRESETS", .presets, size: size).frame(maxWidth: .infinity)
                knob(b, "ALGORITHM", .algo, size: size).frame(maxWidth: .infinity)
            }
        }
    }

    /// The main set in one row (the linked views).
    private func mainRow(_ b: Board, size: CGFloat) -> some View {
        HStack(spacing: 0) {
            knob(b, "MASTER", nil, size: size)
            Spacer(minLength: 0)
            knob(b, "SELECT", .select, size: size)
            Spacer(minLength: 0)
            knob(b, "PRESETS", .presets, size: size)
            Spacer(minLength: 0)
            knob(b, "ALGORITHM", .algo, size: size)
        }
    }

    private func knRow(_ b: Board, size: CGFloat, spaced: Bool) -> some View {
        HStack(spacing: 0) {
            ForEach(Array([FizzKnob.k1, .k2, .k3, .k4].enumerated()), id: \.offset) { i, k in
                if i > 0 { Spacer(minLength: 0) }
                knob(b, "KN\(i + 1)", k, size: size)
            }
        }
    }

    private func well(_ b: Board, bw: CGFloat, bh: CGFloat, gap: CGFloat, font: CGFloat) -> some View {
        ButtonWell(model: play, buttons: PlayInput.well, cols: 6, bw: b.s(bw), bh: b.s(bh), gap: b.s(gap), pad: b.s(10),
                   font: b.s(font))
    }

    private func setChip(_ b: Board, _ t: String, on: Bool, _ go: @escaping () -> Void) -> some View {
        Button(action: go) {
            Text(t).petch(b.s(12), em: 0.1).foregroundStyle(play.theme.palette.label.color)
                .padding(.horizontal, b.s(12)).frame(height: b.s(36))
                .background(Capsule().fill(.black.opacity(on ? 0.18 : 0)))
        }
        .buttonStyle(.plain)
    }

    private func deckChip(_ b: Board, _ t: String, on: Bool, _ go: @escaping () -> Void) -> some View {
        let p = play.theme.palette
        return Button(action: go) {
            Text(t).petch(b.s(13), em: 0.1).foregroundStyle((on ? p.keyText : p.label).color)
                .frame(maxWidth: .infinity, maxHeight: .infinity)
                .background(Capsule().fill(on ? p.key.color : .clear))
                .contentShape(Capsule())
        }
        .buttonStyle(.plain)
    }

    /// FULL / the linked view / SCREEN.
    private func dots(_ b: Board, _ p: PlayTheme.Palette) -> some View {
        let at = play.view == .full ? 0 : play.view == .screen ? 2 : 1
        return HStack(spacing: 0) {
            ForEach(0..<3, id: \.self) { i in
                Button {
                    play.view = i == 0 ? .full : i == 2 ? .screen : (play.view == .panel ? .panel : .keys)
                } label: {
                    Capsule().fill(p.label.color).opacity(i == at ? 1 : 0.35)
                        .frame(width: b.s(i == at ? 22 : 8), height: b.s(8))
                        .frame(width: b.s(36), height: b.s(28))
                        .contentShape(Rectangle())
                }
                .buttonStyle(.plain)
            }
        }
    }
}

/// A layout's unit: its sizes are the mock-up's points times `u`.
struct Board {
    let u: CGFloat
    func s(_ v: CGFloat) -> CGFloat { v * u }
}

extension View {
    /// At (x, y), w x h of the mock-up's points, in a top-left aligned container.
    func place(_ b: Board, _ x: CGFloat, _ y: CGFloat, _ w: CGFloat, _ h: CGFloat) -> some View {
        frame(width: b.s(w), height: b.s(h)).offset(x: b.s(x), y: b.s(y))
    }
}

/// The iPad's rail while PLAY shows: the logo, the tabs as icons (PLAY lit), IMPORT's "+" at the foot.
struct SlimRail: View {
    @Binding var tab: FizzTab

    var body: some View {
        VStack(spacing: 10) {
            BubbleLogo().frame(width: 34, height: 34).padding(.bottom, 8)
            ForEach(FizzTab.bar) { t in
                let on = t == tab
                Button { tab = t } label: {
                    IconView(t.icon, color: on ? .black : t.tint.color, lineWidth: 2, backdrop: on ? t.tint.color : .black)
                        .frame(width: 24, height: 24)
                        .frame(width: 56, height: 56)
                        .background(Circle().fill(on ? t.tint.color : t.tint.opacity(0.09)))
                        .overlay(Circle().strokeBorder(on ? t.tint.color : t.tint.mix(38, FZ.nav).color, lineWidth: 1.5))
                        .shadow(color: on ? t.tint.opacity(0.5) : .clear, radius: 9)
                        .contentShape(Circle())
                }
                .buttonStyle(.plain)
            }
            Spacer(minLength: 0)
            Button { tab = .importer } label: {
                IconView(Icons.plus, color: .black, lineWidth: 3).frame(width: 26, height: 26)
                    .frame(width: 52, height: 52)
                    .background(Circle().fill(FZ.violet.color))
                    .shadow(color: FZ.violet.opacity(0.55), radius: 10)
            }
            .buttonStyle(.plain)
        }
        .padding(.top, 14).padding(.bottom, 24)
        .frame(maxWidth: .infinity)
        .background(Color.black)
    }
}

private extension View {
    /// The edges' system gestures wait for a second swipe (the keys and knobs sit at the edges).
    @ViewBuilder func deferringSystemGestures() -> some View {
        #if os(iOS)
        defersSystemGestures(on: .all)
        #else
        self
        #endif
    }
}

// MARK: - Turning sideways (the iPhone: PLAY only)

#if os(iOS)
/// The app's delegate (FIZZApp): the iPhone's orientations, landscape only while PLAY shows.
public final class FizzAppDelegate: NSObject, UIApplicationDelegate {
    @MainActor static var landscape = false

    public func application(_ application: UIApplication,
                            supportedInterfaceOrientationsFor window: UIWindow?) -> UIInterfaceOrientationMask {
        if UIDevice.current.userInterfaceIdiom == .pad { return .all }
        return Self.landscape ? .allButUpsideDown : .portrait
    }

    /// PLAY shown or left: the iPhone may turn, or turns back upright.
    @MainActor static func allowLandscape(_ on: Bool) {
        guard UIDevice.current.userInterfaceIdiom != .pad, landscape != on else { return }
        landscape = on
        for case let s as UIWindowScene in UIApplication.shared.connectedScenes {
            s.keyWindow?.rootViewController?.setNeedsUpdateOfSupportedInterfaceOrientations()
            if !on { s.requestGeometryUpdate(.iOS(interfaceOrientations: .portrait)) { _ in } }
        }
    }
}
#endif
