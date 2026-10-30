// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// PLAY (mock-ups 49-54): the emulated FIZZ on an exact copy of RemoteFM1's panel, with FIZZ's names, its screen and
// its extras. Its views: FULL (MAIN's knobs, F1 / F2, the screen, KN1-4, the buttons, the keybed), CONTROLS (the
// screen big, one knob set, BUTTONS or KEYS below), STEPS (the 16 steps and a chromatic keybed; PlaySteps*.swift),
// SCREEN; four page dots, or a swipe sideways on the screen. The iPhone on its side and the iPad (no rail: the whole
// screen is the panel) lay FULL out in one band. The grabber (or a pull down) drops the sheet (PlayPanel.swift), which
// carries the app's tab bar; the FIZZ badge goes back to the manager. Where everything sits: PlayRFMLayout.swift.

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
    /// the iPad
    var wide = false
    @Environment(\.fizzInsets) private var insets
    @Environment(\.fizzSnapshot) private var snapshot
    /// The LCD's place on the panel (PlayNav.space): the full-screen LCD grows from it.
    @State private var lcdFrom: CGRect = .zero

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
                if play.lcdFull {
                    let f = FullLCD(play: play, size: full, from: lcdFrom)
                    f.backdrop
                    f.screen
                }
                if play.sheet {
                    Color(.sRGB, red: 10 / 255, green: 8 / 255, blue: 6 / 255, opacity: 0.5)
                        .onTapGesture { closeSheet() }
                        .transition(.opacity)
                    PlaySheet(model: model, host: host, tab: $tab, top: top, width: full.width, height: full.height)
                        .transition(.move(edge: .top))
                }
            }
            .frame(width: full.width, height: full.height)
            .coordinateSpace(.named(PlayNav.space))
            .ignoresSafeArea()
        }
        .animation(.spring(response: 0.45, dampingFraction: 0.86), value: play.sheet)
        .animation(PlayNav.spring, value: play.lcdFull)
        // (the iPad's panel has no status bar, as RemoteFM1's PadWide)
        .hidingStatusBar(play.lcdFull || wide)
        .animation(.spring(response: 0.4, dampingFraction: 0.85), value: play.view)
        .persistentSystemOverlays(.hidden)
        .deferringSystemGestures()
        // (a backup played or put away: the screen follows the FIZZ now driven)
        .task(id: host.backup?.id) { if !snapshot { await play.follow() } }
        .onAppear {
            host.playShown = true
            if play.view == .steps { play.stepsEntered() }      // (FIZZ in its step view, as on entering STEPS)
        }
        .onDisappear { play.left(); host.playShown = false }
    }

    private func closeSheet() { play.sheet = false }

    // MARK: the board

    /// The board for this size: the iPhone upright (the iPad standing too), on its side, or the iPad.
    static func board(for full: CGSize, wide: Bool) -> PlayBoard {
        full.width > full.height ? (wide ? .pad : .phoneWide) : .phone
    }

    @ViewBuilder
    private func board(full: CGSize, top: CGFloat) -> some View {
        let kind = Self.board(for: full, wide: wide)
        let L = PlayLayout.of(play.view, on: kind, knobsKN: play.knobsKN)
        let bs = L.size
        let u = min(full.width / bs.width, full.height / bs.height)
        // upright: the mock-ups' 844 include a 47 pt status bar; a taller one moves the board down as far as it fits
        let y0 = kind == .phone ? max(0, min(top - 47 * u, full.height - bs.height * u)) : (full.height - bs.height * u) / 2
        panel(L, u)
            .frame(width: bs.width * u, height: bs.height * u, alignment: .topLeading)
            .offset(x: (full.width - bs.width * u) / 2, y: y0)
    }

    @ViewBuilder
    private func panel(_ L: PlayLayout, _ u: CGFloat) -> some View {
        ZStack(alignment: .topLeading) {
            pullArea(L, u)
            if let c = L.chip { chip(c, u).offset(x: c.x * u, y: c.y * u) }
            grabber(L.grabber, u)
            badge(L, u)
            ForEach(L.knobs, id: \.name) { k in knob(k, u) }
            if let t = L.tray {
                Group {
                    if play.view == .steps { OctTray(model: play, well: t, u: u) }
                    else { ButtonWell(model: play, buttons: [.f1, .f2], well: t, u: u) }
                }
                .offset(x: t.x * u, y: t.y * u)
            }
            screen(L.screen.width * u).offset(x: L.screen.minX * u, y: L.screen.minY * u)
            if let r = L.led { PanelLED(size: r.width * u).offset(x: r.minX * u, y: r.minY * u) }
            if let w = L.well { ButtonWell(model: play, buttons: PlayInput.well, well: w, u: u).offset(x: w.x * u, y: w.y * u) }
            if let w = L.pads { StepPads(model: play, well: w, u: u).offset(x: w.x * u, y: w.y * u) }
            if let k = L.keybed {
                LEDKeybed(model: play, w: k.frame.width * u, h: k.frame.height * u, keyW: k.keyW.map { $0 * u },
                          numbers: k.numbers, plays: play.view == .steps ? .notes : .fizz)
                    .offset(x: k.frame.minX * u, y: k.frame.minY * u)
            }
            if let y = L.knobSwitchY { knobSwitch(u).frame(width: L.size.width * u).offset(y: (y - 16) * u) }
            if let d = L.deck { deck(u).frame(width: d.width * u, height: d.height * u).offset(x: d.minX * u, y: d.minY * u) }
            if let d = L.dots { dots(u).frame(width: d.width * u, height: d.height * u).offset(x: d.minX * u, y: d.minY * u) }
        }
    }

    // MARK: the header

    /// The header's row (from the top to under the chip): a pull down drops the sheet.
    private func pullArea(_ L: PlayLayout, _ u: CGFloat) -> some View {
        let h = (L.chip.map { $0.y + $0.height } ?? L.grabber.maxY) + 10
        return Color.clear.contentShape(Rectangle())
            .frame(width: L.size.width * u, height: h * u)
            .gesture(DragGesture(minimumDistance: 12).onEnded { v in if v.translation.height > 24 { play.sheet = true } })
    }

    /// RemoteFM1's status chip as FIZZ's unit: THIS iPHONE (THIS iPAD), FM-1 with a green dot while the keys play it,
    /// or a ▶-played backup's yellow pill (BackupPlay.swift). A tap: the unit picker.
    @ViewBuilder
    private func chip(_ c: PlayLayout.Chip, _ u: CGFloat) -> some View {
        let p = play.theme.palette
        if let bk = host.backup {
            BackupPill(backup: bk, height: c.height * u, font: c.font * u, pick: { model.openPicker(fromPlay: true) },
                       close: { Task { await host.endBackup() } })
        } else {
            let fm1 = play.soundNow == .fm1
            Button { model.openPicker(fromPlay: true) } label: {
                HStack(spacing: 9 * u) {
                    Circle().fill(fm1 ? RGB(0x3BD47A).color : p.label.color).frame(width: 7 * u, height: 7 * u)
                    Text(fm1 ? "FM-1" : EmulatorHost.hereName).dm(c.font * u, 500).lineLimit(1).fixedSize()
                }
                .foregroundStyle(p.label.color)
                .padding(.leading, 11 * u).padding(.trailing, 13 * u)
                .frame(height: c.height * u)
                .background(Capsule().fill(.black.opacity(0.14)))
                .fixedSize()
            }
            .buttonStyle(PressStyle())
        }
    }

    /// The grabber: a tap or a pull down drops the sheet.
    private func grabber(_ r: CGRect, _ u: CGFloat) -> some View {
        Capsule().fill(play.theme.palette.label.color).opacity(0.55).frame(width: r.width * u, height: r.height * u)
            .frame(width: 88 * u, height: 44 * u)
            .contentShape(Rectangle())
            .gesture(sheetPull)
            .offset(x: (r.midX - 44) * u, y: (r.midY - 22) * u)
    }

    /// A tap, or a pull down: the sheet.
    private var sheetPull: some Gesture {
        DragGesture(minimumDistance: 0).onEnded { v in
            if v.translation.height > 16 || hypot(v.translation.width, v.translation.height) < 10 { play.sheet = true }
        }
    }

    /// The FIZZ badge in RemoteFM1's wordmark's place (round, beside KN4, on the iPhone's side): the manager.
    private func badge(_ L: PlayLayout, _ u: CGFloat) -> some View {
        let b = L.badge
        return FizzBadge(height: b.height * u, round: b.round, scale: u) { toManager() }
            .fixedSize()
            .frame(width: L.size.width * u - b.right * u, height: b.height * u, alignment: .trailing)
            .offset(y: b.top * u)
    }

    /// The FIZZ badge: the manager's tab last shown.
    private func toManager() {
        withAnimation(.spring(response: 0.35, dampingFraction: 0.8)) { tab = play.managerTab }
    }

    // MARK: the parts placed

    /// The LCD; a swipe sideways on it moves to the next view.
    private func screen(_ s: CGFloat) -> some View {
        LiveLCD(play: play, size: s)
            .contentShape(Rectangle())
            .onGeometryChange(for: CGRect.self, of: { $0.frame(in: .named(PlayNav.space)) }) { lcdFrom = $0 }
            // a double tap: the LCD full screen
            .onTapGesture(count: 2) { play.lcdFull = true }
            .gesture(DragGesture(minimumDistance: 24).onEnded { v in
                guard abs(v.translation.width) > 50, abs(v.translation.width) > abs(v.translation.height) else { return }
                play.view = PlayModel.swiped(play.view, by: v.translation.width < 0 ? 1 : -1)
            })
    }

    /// A knob by its name: MASTER absolute, the others detents.
    private func knob(_ k: PlayLayout.Knob, _ u: CGFloat) -> some View {
        let p = play.theme.palette
        let fk: FizzKnob? = switch k.name {
        case "SELECT": .select
        case "PRESETS": .presets
        case "ALGORITHM": .algo
        case "KN1": .k1
        case "KN2": .k2
        case "KN3": .k3
        case "KN4": .k4
        default: nil
        }
        let play = self.play
        let w = (k.size + 44) * u, h = (k.labelSize + 6 + k.size) * u
        return Group {
            if let fk {
                PlayKnob(label: k.name, size: k.size * u, labelSize: k.labelSize * u, gap: 6 * u, shadow: k.shadow * u,
                         angle: play.angles[fk] ?? 0, p: p, turn: { play.turn(fk, by: $0) })
            } else {
                PlayKnob(label: k.name, size: k.size * u, labelSize: k.labelSize * u, gap: 6 * u, shadow: k.shadow * u,
                         angle: play.masterAngle, p: p, absolute: (start: { play.master }, set: { play.setMaster($0) }))
            }
        }
        .frame(width: w, height: h, alignment: .top)
        .offset(x: k.cx * u - w / 2, y: k.labelTop * u)
    }

    /// CONTROLS: MAIN | KN 1-4.
    private func knobSwitch(_ u: CGFloat) -> some View {
        let p = play.theme.palette
        return HStack(spacing: 4 * u) {
            ForEach([false, true], id: \.self) { kn in
                let on = play.knobsKN == kn
                Button { play.knobsKN = kn } label: {
                    Text(kn ? "KN 1–4" : "MAIN").petch(11 * u, em: 0.12).padding(.leading, 11 * u * 0.12)
                        .foregroundStyle(p.label.color)
                        .padding(.horizontal, 12 * u).frame(height: 32 * u)
                        .background(Capsule().fill(.black.opacity(on ? 0.14 : 0)))
                        .contentShape(Capsule())
                }
                .buttonStyle(.plain)
            }
        }
    }

    /// CONTROLS: BUTTONS | KEYS.
    private func deck(_ u: CGFloat) -> some View {
        let p = play.theme.palette
        return HStack(spacing: 0) {
            ForEach([PlayViewKind.panel, .keys], id: \.self) { v in
                let on = play.view == v
                Button { play.view = v } label: {
                    Text(v == .panel ? "BUTTONS" : "KEYS").petch(12.5 * u, em: 0.12).padding(.leading, 12.5 * u * 0.12)
                        .foregroundStyle((on ? p.keyText : p.label).color)
                        .frame(maxWidth: .infinity, maxHeight: .infinity)
                        .background(Capsule().fill(on ? p.key.color : .clear))
                        .contentShape(Capsule())
                }
                .buttonStyle(.plain)
            }
        }
        .padding(3 * u)
        .background(Capsule().fill(.black.opacity(0.14)))
    }

    /// FULL / CONTROLS / STEPS / SCREEN.
    private func dots(_ u: CGFloat) -> some View {
        let p = play.theme.palette
        let at = switch play.view {
        case .full: 0
        case .panel, .keys: 1
        case .steps: 2
        case .screen: 3
        }
        return HStack(spacing: 4 * u) {
            ForEach(0..<4, id: \.self) { i in
                Button {
                    play.view = switch i {
                    case 0: .full
                    case 2: .steps
                    case 3: .screen
                    default: play.view == .keys ? .keys : .panel
                    }
                } label: {
                    Capsule().fill(p.label.color).opacity(i == at ? 1 : 0.35)
                        .frame(width: (i == at ? 22 : 8) * u, height: 8 * u)
                        .frame(width: 36 * u, height: 24 * u)
                        .contentShape(Rectangle())
                }
                .buttonStyle(.plain)
            }
        }
    }
}

/// The LCD as frames come (only it redraws ~60 times a second, not the whole panel).
struct LiveLCD: View {
    let play: PlayModel
    let size: CGFloat
    var body: some View { LCDScreen(image: play.lcd, size: size, p: play.theme.palette) }
}

private extension View {
    /// (the LCD full screen, the iPad's panel: the status bar hidden)
    @ViewBuilder func hidingStatusBar(_ on: Bool) -> some View {
        #if os(iOS)
        statusBarHidden(on)
        #else
        self
        #endif
    }

    /// The edges' system gestures wait for a second swipe (the keys and knobs sit at the edges).
    @ViewBuilder func deferringSystemGestures() -> some View {
        #if os(iOS)
        defersSystemGestures(on: .all)
        #else
        self
        #endif
    }
}

// MARK: - Turning sideways

#if os(iOS)
/// The app's delegate (FIZZApp): the iPhone turns with the hand on every tab, PLAY and the manager alike (the
/// manager's sideways layout: LandscapeRoot.swift); so leaving PLAY keeps the way it is held (LandscapeTurning.swift).
public final class FizzAppDelegate: NSObject, UIApplicationDelegate {
    public func application(_ application: UIApplication,
                            supportedInterfaceOrientationsFor window: UIWindow?) -> UIInterfaceOrientationMask {
        Turning.allowed(pad: UIDevice.current.userInterfaceIdiom == .pad).mask
    }
}
#endif
