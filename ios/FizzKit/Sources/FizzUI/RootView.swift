// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// The app around the tabs: the connect screen until a unit is found, then the tabs. iPhone: the top bar and the
// floating bottom bar; iPad (wide): the left rail with the unit card and six tab pills.

import SwiftUI
import FizzData
import FizzKit

/// The whole app: looking for the unit, then its tabs.
public struct FizzAppView: View {
    @State private var model: UnitModel
    @State private var tab: FizzTab = .backup
    @State private var entered = false
    /// `-backup full|presets` (the demo unit only): that backup once connected (simulator screenshots)
    @State private var autoBackup: FizzBackupKind?
    /// `-kept` (the demo unit only): KEPT's demo presets, LIBRARY on KEPT (simulator screenshots)
    @State private var demoKept = false
    @Environment(\.scenePhase) private var phase

    /// emulated: the app keeps its emulated FIZZ (PLAY, and the unit while no FM-1 is plugged in).
    public init(source: any LinkSource = LinkSources.fromArguments(), emulated: Bool = true,
                arguments: [String] = ProcessInfo.processInfo.arguments) {
        FizzFont.register()
        _model = State(initialValue: UnitModel(source: source, emulator: emulated ? EmulatorHost.app : nil))
        // `-tab library` opens on that tab (simulator screenshots, UI tests)
        if let k = arguments.firstIndex(of: "-tab"), k + 1 < arguments.count, let t = FizzTab(rawValue: arguments[k + 1]) {
            _tab = State(initialValue: t)
        }
        if source is DemoLinkSource, arguments.contains("-kept") {
            _demoKept = State(initialValue: true)
            UserDefaults.standard.set(true, forKey: "fizz.library.kept")
        }
        if source is DemoLinkSource, let k = arguments.firstIndex(of: "-backup"), k + 1 < arguments.count {
            _autoBackup = State(initialValue: FizzBackupKind(rawValue: arguments[k + 1]))
        }
    }

    private var playLight: Bool {
        guard let h = model.emulator else { return false }
        return h.playShown && h.play.theme.lightBody && !h.play.sheet
    }

    public var body: some View {
        ZStack {
            // (with the emulated FIZZ the app opens straight to its tabs)
            if model.hasFallback || (entered && model.state.kind == "found") {
                RootView(model: model, tab: $tab).transition(.opacity)
            } else {
                ConnectView(model: model).transition(.opacity)
            }
            if let run = model.backups.run {
                BackingUpView(model: model, run: run).transition(.opacity).zIndex(2)
            }
            // I4: RESTORE's progress, DONE, failure (a WRITE TO FM-1 shows PROJECTS' own screen while it writes)
            if model.projectsTab.writing == nil { RestoreOverlay(model: model).zIndex(3) }
            if let w = model.projectsTab.writing {
                ProjectsWritingView(model: model, write: w).transition(.opacity).zIndex(3)
            }
        }
        .sheet(isPresented: Bindable(model.restoring).presented) {
            // (RestoreSheet scrolls its body itself, its slide and CANCEL pinned)
            RestoreSheet(model: model)
                .presentationDetents([.fraction(0.74), .large])
                .presentationDragIndicator(.visible)
                .presentationBackground(RGB(0x0d0d11).color)
                .preferredColorScheme(.dark)
        }
        .unitPicker(model)                          // (every unit pill opens it: UnitsPicker.swift)
        .animation(.easeInOut(duration: 0.3), value: model.backups.run == nil)
        .animation(.easeInOut(duration: 0.3), value: model.projectsTab.writing == nil)
        .background(Color.black)
        // (PLAY's light themes: the status bar's letters dark)
        .preferredColorScheme(playLight ? .light : .dark)
        .task {
            model.emulator?.launch()
            model.start()
            if demoKept { try? await DemoKept.seed(model.kept) }
        }
        .task(id: model.state.kind) {
            guard model.state.kind == "found" else { entered = false; return }
            try? await Task.sleep(for: .milliseconds(1600))
            if !Task.isCancelled { withAnimation(.easeInOut(duration: 0.5)) { entered = true } }
            if let k = autoBackup, model.dumpsLoaded || k == .full {
                autoBackup = nil
                tab = .backup
                model.backups.start(k)
            }
        }
        .onChange(of: phase) { _, p in if p == .active { model.becameActive() } }
        // the emulated FIZZ heard while TRACK is on it; read again after PLAY (its panel may have changed it)
        .onChange(of: model.actions.trackOn) { _, on in model.emulator?.trackOn = on }
        .onChange(of: tab) { old, _ in if old == .play && model.onEmulator { Task { await model.refresh(quick: true) } } }
        // (PLAY's FIZZ badge goes back to the manager's tab last shown)
        .onChange(of: tab) { old, new in model.emulator?.play.remember(old); model.emulator?.play.remember(new) }
        // a screen asking for another tab (LIBRARY's EDIT: model.editTarget)
        .onChange(of: model.goTo) { _, t in if let t { tab = t; model.goTo = nil } }
        // EDIT's borrow undone when EDIT is left or the app goes to the background (EditBorrow.swift)
        .onChange(of: tab) { old, new in model.editor.tabChanged(from: old, to: new) }
        .onChange(of: phase) { _, p in model.editor.scene(p, tab: tab) }
        // a .fizzbak / .bin opened from Files, AirDrop, Mail: kept with the backups, RESTORE offered; a sound file
        // (.fizzlib .fizzsnd .dnsnd .dn2pst .dn2snd): IMPORT
        .onOpenURL { url in
            Task {
                // (a .fizzprj / .fizzpat: PROJECTS')
                if await model.projectsTab.open(url) { tab = .projects; return }
                let ext = url.pathExtension.lowercased()
                // (a .fizzsnd: straight to KEPT, its sheet open)
                if ext == "fizzsnd", let k = try? await model.kept.open(url) {
                    model.kept.reveal = k.id
                    tab = .library
                    return
                }
                if importExtensions.contains(ext) && ext != "fizzbak" {
                    await model.importer.add(urls: [url])
                    tab = .importer
                } else {
                    tab = .backup
                    await model.restoring.open(url)
                }
            }
        }
    }
}

/// The tabs of a connected unit.
public struct RootView: View {
    let model: UnitModel
    @Binding var tab: FizzTab
    /// nil: by the width (720 pt and more: the iPad layout)
    var wide: Bool?

    public init(model: UnitModel, tab: Binding<FizzTab>, wide: Bool? = nil) {
        self.model = model
        _tab = tab
        self.wide = wide
    }

    public var body: some View {
        if let wide {
            if wide { PadRoot(model: model, tab: $tab) } else { PhoneRoot(model: model, tab: $tab) }
        } else {
            GeometryReader { g in
                // (an iPhone on its side, PLAY only, is wide but low: still the phone's)
                if g.size.width >= 720 && g.size.height >= 500 { PadRoot(model: model, tab: $tab) } else { PhoneRoot(model: model, tab: $tab) }
            }
        }
    }
}

/// One tab's screen.
struct TabScreen: View {
    let model: UnitModel
    let tab: FizzTab
    var wide = false
    var body: some View {
        switch tab {
        case .backup: BackupView(model: model)
        case .library: LibraryView(model: model, wide: wide)
        case .projects: ProjectsView(model: model, wide: wide)
        case .pool: PoolView(model: model, wide: wide)
        case .edit: EditView(model: model, wide: wide)
        case .importer: ImportSheet(model: model, inline: true)
        case .play: Color.clear      // (PhoneRoot and PadRoot show PLAY themselves)
        }
    }
}

// MARK: - iPhone

struct PhoneRoot: View {
    let model: UnitModel
    @Binding var tab: FizzTab
    @Environment(\.fizzInsets) private var insets

    /// the tab bar's distance from the screen's bottom edge
    static let barBottom: CGFloat = 24
    /// what the floating bars cover (fizzBarClearance): the tab bar and a gap, KEEP / CANCEL over it when it shows
    private var clearance: CGFloat {
        Self.barBottom + TabBar.height + 12 + (model.actions.pending != nil ? 52 + 10 : 0)
    }

    var body: some View {
        Group {
            // PLAY is full-screen: its sheet carries the tab bar
            if (tab == .importer ? model.importer.under : tab) == .play, let host = model.emulator {
                PlayView(model: model, host: host, tab: $tab)
            } else {
                tabs
            }
        }
        .onChange(of: tab) { old, new in if new == .importer && old != .importer { model.importer.under = old } }
        .sheet(isPresented: Binding(get: { tab == .importer }, set: { if !$0 && tab == .importer { tab = model.importer.under } })) {
            ImportSheet(model: model)
                .presentationDetents([.large])
                .presentationDragIndicator(.visible)
                .presentationBackground(RGB(0x0d0d11).color)
                .preferredColorScheme(.dark)
        }
    }

    private var tabs: some View {
        ZStack(alignment: .bottom) {
            Color.black
            VStack(spacing: 0) {
                // (IMPORT is a sheet on the phone: the tab under it stays)
                if TrackPill.shows(on: tab == .importer ? model.importer.under : tab) { TrackTopBar(model: model) } else { TopBar(model: model) }
                TabScreen(model: model, tab: tab == .importer ? model.importer.under : tab).frame(maxWidth: .infinity, maxHeight: .infinity)
                    .environment(\.fizzBarClearance, clearance)
            }
            .padding(.top, insets.top)
            // (only round the tab bar: EDIT's and PROJECTS' action rows sit just above it, undimmed)
            LinearGradient(stops: [.init(color: .clear, location: 0), .init(color: .black, location: 0.45)],
                           startPoint: .top, endPoint: .bottom)
                .frame(height: Self.barBottom + TabBar.height + 12)
                .allowsHitTesting(false)
                .opacity(tab == .library && model.kept.selecting ? 0 : 1)
            if tab == .projects { ProjectsPhoneBar(model: model) }
            // (KEPT's SELECT bar takes its place)
            if !(tab == .library && model.kept.selecting) {
                TabBar(tab: $tab).padding(.horizontal, 12).padding(.bottom, Self.barBottom)
                    .transition(.move(edge: .bottom).combined(with: .opacity))
            }
            // KEEP / CANCEL while a track plays another sound (TrackSwitch.swift)
            AuditionBar(model: model).padding(.horizontal, 12).padding(.bottom, Self.barBottom + TabBar.height + 10)
        }
        .animation(.spring(response: 0.35, dampingFraction: 0.85), value: model.actions.pending)
        .animation(.spring(response: 0.35, dampingFraction: 0.85), value: model.kept.selecting)
        .ignoresSafeArea(edges: .bottom)
    }
}

/// The floating bar: the selected tab a wide pill with its name, the others round icons, IMPORT's violet "+".
struct TabBar: View {
    @Binding var tab: FizzTab
    static let height: CGFloat = 64

    var body: some View {
        HStack(spacing: 3) {
            ForEach(FizzTab.bar) { t in
                let on = t == tab
                Button { withAnimation(.spring(response: 0.35, dampingFraction: 0.8)) { tab = t } } label: {
                    HStack(spacing: 6) {
                        IconView(t.icon, color: on ? .black : t.tint.color, backdrop: on ? t.tint.color : .black)
                            .frame(width: 21, height: 21)
                        if on { Text(t.title).fz(13, 800, em: 0.05).foregroundStyle(.black).lineLimit(1).fixedSize() }
                    }
                    .padding(.horizontal, on ? 8 : 0)
                    .frame(maxWidth: on ? .infinity : 39)
                    .frame(width: on ? nil : 39, height: 48)
                    .background(Capsule().fill(on ? t.tint.color : t.tint.opacity(0.09)))
                    .overlay(Capsule().strokeBorder(on ? t.tint.color : t.tint.mix(38, FZ.nav).color, lineWidth: 1.5))
                    .shadow(color: on ? t.tint.opacity(0.5) : .clear, radius: 9)
                    .contentShape(Capsule())
                }
                .buttonStyle(.plain)
            }
            Button { tab = .importer } label: {
                IconView(Icons.plus, color: .black, lineWidth: 3).frame(width: 26, height: 26)
                    .frame(width: 50, height: 50)
                    .background(Circle().fill(FZ.violet.color))
                    .overlay(Circle().strokeBorder(.white.opacity(tab == .importer ? 0.9 : 0), lineWidth: 2))
                    .shadow(color: FZ.violet.opacity(0.55), radius: 10)
            }
            .buttonStyle(.plain)
            .padding(.leading, 2)
        }
        .padding(.horizontal, 5)
        .frame(height: Self.height)
        .background(Capsule().fill(RGB(0x111115).opacity(0.92)))
        .overlay(Capsule().strokeBorder(FZ.nav.color, lineWidth: 1))
        .shadow(color: .black.opacity(0.8), radius: 17, y: 12)
    }
}

// MARK: - iPad

struct PadRoot: View {
    let model: UnitModel
    @Binding var tab: FizzTab
    @Environment(\.fizzInsets) private var insets

    var body: some View {
        if tab == .play, let host = model.emulator {
            // PLAY: the rail slims to its icons, the panel takes the rest (mock-up 33)
            // (the LCD full screen: the rail slides away)
            let full = host.play.lcdFull
            HStack(spacing: 0) {
                SlimRail(tab: $tab).frame(width: 84).padding(.top, insets.top).frame(width: full ? 0 : 84).clipped()
                Rectangle().fill(FZ.line.color).frame(width: full ? 0 : 1)
                PlayView(model: model, host: host, tab: $tab, wide: true)
            }
            .background(Color.black)
            .animation(PlayNav.spring, value: full)
        } else {
            tabs
        }
    }

    private var tabs: some View {
        HStack(spacing: 0) {
            Rail(model: model, tab: $tab)
                .frame(width: 204)
            Rectangle().fill(FZ.line.color).frame(width: 1)
            TabScreen(model: model, tab: tab, wide: true)
                .padding(.top, 14).padding(.leading, 18).padding(.trailing, 20)
                .frame(maxWidth: .infinity, maxHeight: .infinity)
                // KEEP / CANCEL while a track plays another sound (TrackSwitch.swift)
                .overlay(alignment: .bottom) { AuditionBar(model: model).frame(maxWidth: 560).padding(.bottom, 24) }
                .animation(.spring(response: 0.35, dampingFraction: 0.85), value: model.actions.pending)
        }
        .padding(.top, insets.top)
        .background(Color.black)
    }
}

struct Rail: View {
    let model: UnitModel
    @Binding var tab: FizzTab

    var body: some View {
        VStack(alignment: .leading, spacing: 14) {
            HStack(spacing: 10) {
                BubbleLogo().frame(width: 32, height: 32)
                FizzWord(size: 26)
            }
            .padding(.horizontal, 4).padding(.top, 4)
            RailUnitCard(model: model)
            VStack(spacing: 8) {
                ForEach(FizzTab.allCases) { t in
                    let on = t == tab
                    Button { tab = t } label: {
                        HStack(spacing: 12) {
                            IconView(t.icon, color: on ? .black : t.tint.color, backdrop: on ? t.tint.color : .black)
                                .frame(width: 22, height: 22)
                            Text(t.title).fz(15, 800, em: 0.08).foregroundStyle(on ? Color.black : FZ.muted.color)
                            Spacer(minLength: 0)
                        }
                        .padding(.horizontal, 18)
                        .frame(height: 52)
                        .background(Capsule().fill(on ? t.tint.color : .clear))
                        .overlay(Capsule().strokeBorder(on ? t.tint.color : FZ.line.color, lineWidth: 1.5))
                        .shadow(color: on ? t.tint.opacity(0.45) : .clear, radius: 10)
                        .contentShape(Capsule())
                    }
                    .buttonStyle(.plain)
                }
            }
            Spacer(minLength: 0)
        }
        .padding(.top, 14).padding(.leading, 18).padding(.trailing, 12).padding(.bottom, 30)
    }
}

struct RailUnitCard: View {
    let model: UnitModel
    @AppStorage("play.theme") private var themeName = PlayTheme.tangerine.rawValue
    var body: some View {
        // (the emulated FIZZ: THIS iPAD in the emulator's colour)
        let c = model.onEmulator ? (PlayTheme(rawValue: themeName) ?? .tangerine).accent : FZ.green
        VStack(alignment: .leading, spacing: 10) {
            FM1Drawing(color: c, screen: .bubbles).frame(width: 146)
            HStack(spacing: 8) {
                Circle().fill(c.color).frame(width: 10, height: 10).shadow(color: c.color, radius: 4)
                Text(model.onEmulator ? EmulatorHost.hereName : model.unit?.shortVersion ?? "").fz(15, 800)
                    .foregroundStyle(FZ.text.color).lineLimit(1)
            }
            StoreMeter(use: model.storeUse, height: 14)
            HStack {
                Stat(value: "\(model.status?.projects ?? 0)", label: "projects", size: 18)
                Spacer(minLength: 0)
                Stat(value: "\(model.mine.count)", label: "presets", size: 18)
            }
        }
        .padding(14)
        .background(RoundedRectangle(cornerRadius: 20).fill(c.mix(6, FZ.panel).color))
        .overlay(RoundedRectangle(cornerRadius: 20).strokeBorder(c.mix(50, FZ.line).color, lineWidth: 1.5))
        .contentShape(RoundedRectangle(cornerRadius: 20))
        .onTapGesture { model.openPicker() }        // (the unit picker)
    }
}

// MARK: - The other tabs (later phases)

/// EDIT, POOL, IMPORT for now: the tab's icon, big, in its colour.
struct TabPlaceholder: View {
    let tab: FizzTab
    var body: some View {
        Animated(still: 0) { t in
            IconView(tab.icon, color: tab.tint.color, lineWidth: 1.6, backdrop: .black)
                .frame(width: 140, height: 140)
                .shadow(color: tab.tint.opacity(0.25 + 0.3 * breathe(t, 3)), radius: 24)
        }
        .frame(maxWidth: .infinity, maxHeight: .infinity)
    }
}
