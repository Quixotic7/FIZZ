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
    @Environment(\.scenePhase) private var phase

    public init(source: any LinkSource = LinkSources.fromArguments(),
                arguments: [String] = ProcessInfo.processInfo.arguments) {
        FizzFont.register()
        _model = State(initialValue: UnitModel(source: source))
        // `-tab library` opens on that tab (simulator screenshots, UI tests)
        if let k = arguments.firstIndex(of: "-tab"), k + 1 < arguments.count, let t = FizzTab(rawValue: arguments[k + 1]) {
            _tab = State(initialValue: t)
        }
        if source is DemoLinkSource, let k = arguments.firstIndex(of: "-backup"), k + 1 < arguments.count {
            _autoBackup = State(initialValue: FizzBackupKind(rawValue: arguments[k + 1]))
        }
    }

    public var body: some View {
        ZStack {
            if entered, case .found = model.state {
                RootView(model: model, tab: $tab).transition(.opacity)
            } else {
                ConnectView(model: model).transition(.opacity)
            }
            if let run = model.backups.run {
                BackingUpView(model: model, run: run).transition(.opacity).zIndex(2)
            }
            if let w = model.projectsTab.writing {
                ProjectsWritingView(model: model, write: w).transition(.opacity).zIndex(3)
            }
        }
        .animation(.easeInOut(duration: 0.3), value: model.backups.run == nil)
        .animation(.easeInOut(duration: 0.3), value: model.projectsTab.writing == nil)
        .background(Color.black)
        .preferredColorScheme(.dark)
        .task { model.start() }
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
        // a .fizzbak / .fizzlib / .bin opened from Files, AirDrop, Mail: kept with the backups
        .onOpenURL { url in
            Task {
                // (a .fizzprj / .fizzpat: PROJECTS')
                if await model.projectsTab.open(url) { tab = .projects; return }
                if await model.backups.open(url) { tab = .backup }
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
                if g.size.width >= 720 { PadRoot(model: model, tab: $tab) } else { PhoneRoot(model: model, tab: $tab) }
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
        case .edit, .pool, .importer: TabPlaceholder(tab: tab)
        }
    }
}

// MARK: - iPhone

struct PhoneRoot: View {
    let model: UnitModel
    @Binding var tab: FizzTab
    @Environment(\.fizzInsets) private var insets

    var body: some View {
        ZStack(alignment: .bottom) {
            Color.black
            VStack(spacing: 0) {
                TopBar(model: model)
                TabScreen(model: model, tab: tab).frame(maxWidth: .infinity, maxHeight: .infinity)
            }
            .padding(.top, insets.top)
            LinearGradient(stops: [.init(color: .clear, location: 0), .init(color: .black, location: 0.62)],
                           startPoint: .top, endPoint: .bottom)
                .frame(height: 150)
                .allowsHitTesting(false)
            if tab == .projects { ProjectsPhoneBar(model: model) }
            TabBar(tab: $tab).padding(.horizontal, 12).padding(.bottom, 24)
        }
        .ignoresSafeArea(edges: .bottom)
    }
}

/// The floating bar: the selected tab a wide pill with its name, the others round icons, IMPORT's violet "+".
struct TabBar: View {
    @Binding var tab: FizzTab

    var body: some View {
        HStack(spacing: 4) {
            ForEach(FizzTab.bar) { t in
                let on = t == tab
                Button { withAnimation(.spring(response: 0.35, dampingFraction: 0.8)) { tab = t } } label: {
                    HStack(spacing: 6) {
                        IconView(t.icon, color: on ? .black : t.tint.color, backdrop: on ? t.tint.color : .black)
                            .frame(width: 21, height: 21)
                        if on { Text(t.title).fz(13, 800, em: 0.05).foregroundStyle(.black).lineLimit(1).fixedSize() }
                    }
                    .frame(maxWidth: on ? .infinity : 42)
                    .frame(width: on ? nil : 42, height: 48)
                    .background(Capsule().fill(on ? t.tint.color : t.tint.opacity(0.09)))
                    .overlay(Capsule().strokeBorder(on ? t.tint.color : t.tint.mix(38, FZ.nav).color, lineWidth: 1.5))
                    .shadow(color: on ? t.tint.opacity(0.5) : .clear, radius: 9)
                    .contentShape(Capsule())
                }
                .buttonStyle(.plain)
            }
            Button { tab = .importer } label: {
                IconView(Icons.plus, color: .black, lineWidth: 3).frame(width: 26, height: 26)
                    .frame(width: 52, height: 52)
                    .background(Circle().fill(FZ.violet.color))
                    .overlay(Circle().strokeBorder(.white.opacity(tab == .importer ? 0.9 : 0), lineWidth: 2))
                    .shadow(color: FZ.violet.opacity(0.55), radius: 10)
            }
            .buttonStyle(.plain)
            .padding(.leading, 2)
        }
        .padding(.horizontal, 6)
        .frame(height: 64)
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
        HStack(spacing: 0) {
            Rail(model: model, tab: $tab)
                .frame(width: 204)
            Rectangle().fill(FZ.line.color).frame(width: 1)
            TabScreen(model: model, tab: tab, wide: true)
                .padding(.top, 14).padding(.leading, 18).padding(.trailing, 20)
                .frame(maxWidth: .infinity, maxHeight: .infinity)
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
    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            FM1Drawing(color: FZ.green, screen: .bubbles).frame(width: 146)
            HStack(spacing: 8) {
                Circle().fill(FZ.green.color).frame(width: 10, height: 10).shadow(color: FZ.green.color, radius: 4)
                Text(model.unit?.shortVersion ?? "").fz(15, 800).foregroundStyle(FZ.text.color).lineLimit(1)
            }
            StoreMeter(use: model.storeUse, height: 14)
            HStack {
                Stat(value: "\(model.status?.projects ?? 0)", label: "projects", size: 18)
                Spacer(minLength: 0)
                Stat(value: "\(model.mine.count)", label: "presets", size: 18)
            }
        }
        .padding(14)
        .background(RoundedRectangle(cornerRadius: 20).fill(FZ.green.mix(6, FZ.panel).color))
        .overlay(RoundedRectangle(cornerRadius: 20).strokeBorder(FZ.green.mix(50, FZ.line).color, lineWidth: 1.5))
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
