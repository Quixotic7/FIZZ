// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// The EDIT tab (mock-ups 11, 12, 17, 37; editor.js renderHead, the panel, the keybed; phase I5). iPhone: the place
// (KEEP ON Tn while the sound is borrowed on a track) and the name, the page chips, one page, REVERT / SAVE / SAVE AS
// NEW, the keybed's strip (swipe up). iPad: the TRACK n chip and the page list on the left, the page in a card with
// its actions, the keybed across the bottom, a hardware keyboard plays it. A value turned takes the screen while the
// finger is down.

import SwiftUI
import FizzData
import FizzKit

struct EditView: View {
    let model: UnitModel
    var wide = false
    @State private var confirmSave = false
    @State private var tagsOpen = false
    @State private var flash: String?
    @State private var nameText = ""
    /// the phone's keybed panel, measured
    @State private var keysHeight: CGFloat = 200
    @FocusState private var nameFocus: Bool
    @Environment(\.fizzSnapshot) private var snapshot

    private var e: EditorModel { model.editor }

    var body: some View {
        Group { if wide { padBody } else { phoneBody } }
            .overlay { turnOverlay }
            .overlay(alignment: .top) { flashView }
            // (the name typed: the keyboard covers the bottom, the page keeps its size)
            .ignoresSafeArea(.keyboard)
            .task {
                await BorrowLaunch.apply(model)
                await e.show()
            }
            // (the borrow undone; RootView's tab hook does it too)
            .onDisappear { e.hide() }
            // (a unit ready while EDIT shows: its track followed, or the sound opened here heard on it)
            .onChange(of: model.actions.trackCapable) { _, on in if on { Task { await e.unitReady() } } }
            .onChange(of: e.request) { _, t in if t != nil { Task { await e.show() } } }
            .sheet(isPresented: Binding(get: { e.picking != nil }, set: { if !$0 { e.picking = nil } })) {
                if let id = e.picking { EditPickerSheet(e: e, id: id) }
            }
            .sheet(isPresented: $tagsOpen) { EditTagsSheet(e: e) }
            .alert("SAVE · MY PRESETS \(e.place.slot)", isPresented: $confirmSave) {
                Button("SAVE") { Task { await save(asNew: false) } }
                Button("CANCEL", role: .cancel) {}
            } message: {
                Text("\(e.sound?.name ?? "") over it. The tracks that loaded it keep their own copies.")
            }
            .hardwareKeys(e)
    }

    // MARK: iPhone

    private var phoneBody: some View {
        VStack(alignment: .leading, spacing: 0) {
            header(nameSize: 32).padding(.horizontal, 16).padding(.top, 2).padding(.bottom, 10)
                .turnDim(e, [])
            pageChipRow
                .padding(.bottom, 6)
                .turnDim(e, [])
            pageBody
                .padding(.horizontal, 16)
                // (the keybed open: the page's drawings give way so it sits above it where it can; what still
                // doesn't fit goes under it)
                .padding(.bottom, e.keysOpen ? keysHeight : 0)
                .frame(maxHeight: .infinity, alignment: .top)
                // (the keybed slides up over the page)
                .overlay(alignment: .bottom) {
                    if e.keysOpen {
                        KeybedPanel(e: e)
                            .onGeometryChange(for: CGFloat.self) { $0.size.height } action: { keysHeight = $0 }
                            .transition(.move(edge: .bottom).combined(with: .opacity))
                    }
                }
                .clipped()
            HStack(spacing: 8) {
                keysButton
                actions(height: 46)
            }
            .padding(.horizontal, 16).padding(.top, 8)
            .turnDim(e, [])
        }
        // (above the tab bar, and above the KEEP / CANCEL bar while it shows)
        .padding(.bottom, auditionBar ? 96 + 66 : 96)
    }

    /// The keybed up over the page, or put away.
    private var keysButton: some View {
        Button {
            withAnimation(.spring(response: 0.35, dampingFraction: 0.85)) { e.keysOpen.toggle() }
        } label: {
            Image(systemName: "pianokeys").font(.system(size: 19, weight: .bold))
                .foregroundStyle(e.keysOpen ? Color.black : FZ.text.color)
                .frame(width: 46, height: 46)
                .background(Circle().fill(e.keysOpen ? FZ.pink.color : FZ.raised.color))
                .overlay(Circle().strokeBorder(e.keysOpen ? FZ.pink.color : FZ.line.color, lineWidth: 1.5))
                .shadow(color: e.keysOpen ? FZ.pink.opacity(0.45) : .clear, radius: 8)
                .contentShape(Circle())
        }
        .buttonStyle(.plain)
    }

    // MARK: iPad

    private var padBody: some View {
        VStack(spacing: 14) {
            HStack(alignment: .top, spacing: 14) {
                VStack(spacing: 6) {
                    TrackChip(model: model, wide: true).padding(.bottom, 6)
                    if e.sound?.isMidi == true {
                        ForEach(0..<7, id: \.self) { n in pageItem("MIDI \(n + 1)", FZ.magenta, nil, on: e.midiPage == n) { e.midiPage = n } }
                    } else {
                        ForEach(EditPage.allCases) { p in pageItem(p.title, p.tint, PageIcons.art(p), on: e.page == p) { e.page = p } }
                    }
                    Spacer(minLength: 0)
                }
                .frame(width: 166)
                .turnDim(e, [])
                VStack(alignment: .leading, spacing: 18) {
                    HStack(alignment: .center, spacing: 12) {
                        header(nameSize: 40)
                        Spacer(minLength: 8)
                        actions(height: 46, fixed: true)
                    }
                    .turnDim(e, [])
                    pageBody.frame(maxHeight: .infinity, alignment: .top)
                }
                .padding(20)
                .frame(maxWidth: .infinity, maxHeight: .infinity)
                .background(RoundedRectangle(cornerRadius: 22).fill(FZ.panel.color))
                .overlay(RoundedRectangle(cornerRadius: 22).strokeBorder(FZ.line.color, lineWidth: 1))
            }
            Keybed(e: e, height: 104).turnDim(e, [])
        }
        // (the KEEP / CANCEL bar floats over the bottom: the keybed above it)
        .padding(.bottom, auditionBar ? 20 + 66 : 20)
    }

    /// LIBRARY's KEEP / CANCEL bar shows (EDIT's own borrow has none).
    private var auditionBar: Bool { model.actions.pending != nil && !model.actions.borrowed }

    private func pageItem(_ title: String, _ c: RGB, _ art: VectorArt?, on: Bool, _ go: @escaping () -> Void) -> some View {
        Button(action: go) {
            HStack(spacing: 11) {
                if let art { IconView(art, color: c.color, lineWidth: 2.4, backdrop: .clear).frame(width: 24, height: 22) }
                Text(title).fz(14, 800, em: 0.08).foregroundStyle(on ? FZ.text.color : FZ.muted.color).lineLimit(1)
                    .minimumScaleFactor(0.75)
                Spacer(minLength: 0)
            }
            .padding(.horizontal, 14).frame(height: 48)
            .background(RoundedRectangle(cornerRadius: 14).fill(on ? c.mix(14, FZ.raised).color : FZ.raised.color))
            .overlay(RoundedRectangle(cornerRadius: 14).strokeBorder(on ? c.color : FZ.line.color, lineWidth: 1.5))
            .shadow(color: on ? c.opacity(0.35) : .clear, radius: 9)
            .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
    }

    // MARK: the parts

    /// The place (KEEP ON Tn while borrowed), the name (a tap types it), its first tag (a tap: the tags), the EDITED
    /// dot.
    private func header(nameSize: CGFloat) -> some View {
        VStack(alignment: .leading, spacing: 5) {
            HStack(spacing: 8) {
                if e.sound?.isMidi == true {
                    Text("MIDI").fz(11, 800, em: 0.08).foregroundStyle(.black)
                        .padding(.horizontal, 6).frame(height: 18).background(RoundedRectangle(cornerRadius: 5).fill(FZ.magenta.color))
                }
                if e.place.isKept {
                    PhoneGlyph(size: 13)
                    Text(e.place.kept == nil ? "KEPT · NEW" : "KEPT").fz(13, 800, em: 0.12).foregroundStyle(FZ.champagne.color)
                } else {
                    Text(e.place.label).fz(13, 800, em: 0.12).foregroundStyle(FZ.muted.color).lineLimit(1)
                }
                if !wide { Spacer(minLength: 4) }
                KeepOnButton(model: model)
            }
            .frame(height: 22)
            .animation(.spring(response: 0.35, dampingFraction: 0.8), value: e.heard)
            HStack(alignment: .center, spacing: 10) {
                if e.naming {
                    TextField("", text: $nameText)
                        .font(FizzFont.font(nameSize, 800))
                        .foregroundStyle(FZ.text.color)
                        .textFieldStyle(.plain)
                        .autocorrectionDisabled()
                        .focused($nameFocus)
                        .onSubmit { endNaming() }
                        .onChange(of: nameFocus) { _, f in if !f { endNaming() } }
                        .onChange(of: nameText) { _, t in if t.count > 12 { nameText = String(t.prefix(12)) } }
                        .frame(maxWidth: 260)
                } else {
                    Text(e.sound?.name ?? "").fz(nameSize, 800, em: -0.01).foregroundStyle(FZ.text.color).lineLimit(1)
                        .minimumScaleFactor(0.6)
                        .onTapGesture {
                            nameText = e.sound?.name ?? ""
                            e.naming = true
                            nameFocus = true
                        }
                }
                Button { tagsOpen = true } label: {
                    if let t = FizzTag.of(mask: e.sound?.tags ?? 0).first { TagPill(tag: t) } else {
                        Text("TAG").fz(12, 800, em: 0.05).foregroundStyle(FZ.muted.color)
                            .padding(.horizontal, 8).frame(height: 22)
                            .overlay(RoundedRectangle(cornerRadius: 6).strokeBorder(FZ.line.color, style: StrokeStyle(lineWidth: 1.5, dash: [4, 3])))
                    }
                }
                .buttonStyle(.plain)
                if e.dirty {
                    Circle().fill(FZ.pink.color).frame(width: 10, height: 10).shadow(color: FZ.pink.color, radius: 4)
                }
            }
        }
    }

    private func endNaming() {
        guard e.naming else { return }
        e.setName(nameText)
        e.naming = false
    }

    /// The page chips, the chosen one scrolled into view.
    @ViewBuilder private var pageChipRow: some View {
        let midi = e.sound?.isMidi == true
        let sel = midi ? "m\(e.midiPage)" : e.page.rawValue
        if snapshot {
            // (as the scroll view shows it: from the chip before the chosen one)
            let k = midi ? e.midiPage : EditPage.allCases.firstIndex(of: e.page) ?? 0
            ChipScroll { pageChips(from: max(0, min(k - 1, (midi ? 7 : EditPage.allCases.count) - 4))) }
                .frame(height: 44)
        } else {
            ScrollViewReader { px in
                ScrollView(.horizontal) {
                    HStack(spacing: 7) { pageChips(from: 0) }.padding(.horizontal, 16)
                }
                .scrollIndicators(.hidden)
                .scrollClipDisabled()
                .onAppear { px.scrollTo(sel, anchor: .center) }
                .onChange(of: sel) { _, k in
                    withAnimation(.spring(response: 0.35, dampingFraction: 0.85)) { px.scrollTo(k, anchor: .center) }
                }
            }
            .frame(height: 44)
        }
    }

    @ViewBuilder private func pageChips(from first: Int) -> some View {
        if e.sound?.isMidi == true {
            ForEach(first..<7, id: \.self) { n in
                pageChip("MIDI \(n + 1)", FZ.magenta, nil, on: e.midiPage == n) { e.midiPage = n }.id("m\(n)")
            }
        } else {
            ForEach(Array(EditPage.allCases.dropFirst(first))) { p in
                pageChip(p.title, p.tint, PageIcons.art(p), on: e.page == p) { e.page = p }.id(p.rawValue)
            }
        }
    }

    private func pageChip(_ title: String, _ c: RGB, _ art: VectorArt?, on: Bool, _ go: @escaping () -> Void) -> some View {
        Button(action: go) {
            HStack(spacing: 7) {
                if let art { IconView(art, color: on ? .black : c.color, lineWidth: 2.4, backdrop: on ? c.color : .black).frame(width: 20, height: 18) }
                Text(title).fz(13, 800, em: 0.07)
            }
            .foregroundStyle(on ? Color.black : FZ.muted.color)
            .padding(.leading, art == nil ? 13 : 10).padding(.trailing, 13).frame(height: 38)
            .background(Capsule().fill(on ? c.color : .clear))
            .overlay(Capsule().strokeBorder(on ? c.color : FZ.line.color, lineWidth: 1.5))
            .shadow(color: on ? c.opacity(0.45) : .clear, radius: 8)
            .frame(height: 44)
            .contentShape(Rectangle())
            .fixedSize()
        }
        .buttonStyle(.plain)
    }

    @ViewBuilder private var pageBody: some View {
        let content = Group {
            if e.table == nil || e.sound == nil {
                Color.clear
            } else if e.sound?.isMidi == true {
                if wide {
                    MidiPage(e: e, height: 380)
                } else {
                    // (the bars as tall as the page leaves them: CC and the value below)
                    GeometryReader { g in MidiPage(e: e, height: max(90, g.size.height - 98)) }
                }
            } else {
                EditPageView(e: e, page: e.page, wide: wide)
            }
        }
        if !wide {
            // (the phone: each page fits the height between the chips and the actions, nothing scrolls)
            content.frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .top)
        } else if e.page == .ops && e.sound?.isMidi != true {
            content
        } else if snapshot {
            // (as a scroll view lays it out: each drawing as tall as its width makes it, then cut)
            Color.clear.overlay(alignment: .top) { content.fixedSize(horizontal: false, vertical: true) }.clipShape(OpenTop())
        } else {
            // (each page from its top: its own scroll view)
            ScrollView { content.padding(.bottom, 16) }.scrollIndicators(.hidden)
                .id(e.sound?.isMidi == true ? "midi" : e.page.rawValue)
        }
    }

    /// REVERT, SAVE (over the MY preset it came from: asked first), SAVE AS NEW.
    @ViewBuilder private func actions(height: CGFloat, fixed: Bool = false) -> some View {
        if fixed {
            actionRow(height: height, fixed: true, widths: [nil, nil, nil])
        } else {
            // (the mock-up's shares: 1, 1, 1.45)
            GeometryReader { g in
                let u = (g.size.width - 16) / 3.45
                actionRow(height: height, fixed: false, widths: [u, u, 1.45 * u])
            }
            .frame(height: height)
        }
    }

    private func actionRow(height: CGFloat, fixed: Bool, widths w: [CGFloat?]) -> some View {
        HStack(spacing: 8) {
            action("↺ REVERT", ghost: true, on: e.dirty, fixed: fixed, height: height) { Task { await e.revert() } }
                .frame(width: w[0])
            // (KEPT's SAVE writes the phone's file at once)
            action("SAVE", ghost: false, on: e.canSave && !e.saving, fixed: fixed, height: height) {
                if e.place.isKept { Task { await save(asNew: false) } } else { confirmSave = true }
            }
                .frame(width: w[1])
            action("SAVE AS NEW", ghost: true, on: e.canSaveNew && !e.saving, fixed: fixed, height: height) {
                Task { await save(asNew: true) }
            }
            .frame(width: w[2])
        }
    }

    private func action(_ t: String, ghost: Bool, on: Bool, fixed: Bool, height: CGFloat,
                        _ go: @escaping () -> Void) -> some View {
        Button(action: go) {
            Text(t).fz(13, 800, em: 0.08).lineLimit(1).minimumScaleFactor(0.8)
                .foregroundStyle(ghost ? FZ.text.color : .black)
                .padding(.horizontal, fixed ? 16 : 10)
                .frame(maxWidth: fixed ? nil : .infinity)
                .frame(height: height)
                .background(Capsule().fill(ghost ? Color.clear : FZ.pink.color))
                .overlay(Capsule().strokeBorder(ghost ? FZ.line.color : .clear, lineWidth: 1.5))
                .shadow(color: ghost ? .clear : FZ.pink.opacity(0.45), radius: 10)
                .contentShape(Capsule())
                .fixedSize(horizontal: fixed, vertical: false)
        }
        .buttonStyle(.plain)
        .disabled(!on)
        .opacity(on ? 1 : 0.4)
    }

    private func save(asNew: Bool) async {
        let x = await e.save(asNew: asNew)
        switch x {
        case .ok where e.place.isKept: show("KEPT · \(e.sound?.name ?? "")")
        case .placed: break
        case .refused(.busy): show("THE FM-1 IS PLAYING")
        case .refused(.full): show("MY PRESETS FULL")
        case .refused(.updateFizz): show("UPDATE FIZZ")
        case .refused(let r): show(r.name)
        case .noAnswer: show("NO ANSWER")
        default: break
        }
    }

    private func show(_ s: String) {
        withAnimation { flash = s }
        Task {
            try? await Task.sleep(for: .seconds(2.5))
            withAnimation { if flash == s { flash = nil } }
        }
    }

    @ViewBuilder private var flashView: some View {
        if let flash {
            Text(flash).fz(14, 800, em: 0.1).foregroundStyle(.black)
                .padding(.horizontal, 18).frame(height: 40)
                .background(Capsule().fill(FZ.orange.color))
                .shadow(color: FZ.orange.opacity(0.5), radius: 10)
                .padding(.top, 8)
                .transition(.move(edge: .top).combined(with: .opacity))
        }
    }

    /// The turned value over the dimmed screen, away from the finger: at the top (the phone: over the top bar too)
    /// or, the finger high on the screen, at the bottom (above the actions).
    @ViewBuilder private var turnOverlay: some View {
        if let t = e.turning {
            TurnOverlay(e: e, turn: t, size: wide ? 300 : 330)
                .padding(.top, t.top && wide ? 120 : 0)
                .padding(.bottom, t.top ? 0 : 150)
                .offset(y: t.top && !wide ? -40 : 0)
                .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: t.top ? .top : .bottom)
                .transition(.opacity)
        }
    }
}

/// FIZZ's seven tags, toggled; the sheet as tall as they are.
struct EditTagsSheet: View {
    let e: EditorModel
    @State private var height: CGFloat = 230

    var body: some View {
        VStack(alignment: .leading, spacing: 16) {
            Text("TAGS").fz(16, 800, em: 0.12).foregroundStyle(FZ.muted.color)
            LazyVGrid(columns: [GridItem(.adaptive(minimum: 84), spacing: 8)], alignment: .leading, spacing: 8) {
                ForEach(FizzTag.allCases) { t in
                    let on = (e.sound?.tags ?? 0) >> t.rawValue & 1 == 1
                    Button { e.toggleTag(t.rawValue) } label: {
                        Text(t.name).fz(14, 800, em: 0.05).foregroundStyle(on ? t.ink : FZ.muted.color)
                            .frame(maxWidth: .infinity).frame(height: 44)
                            .background(RoundedRectangle(cornerRadius: 10).fill(on ? t.tint.color : .clear))
                            .overlay(RoundedRectangle(cornerRadius: 10).strokeBorder(on ? t.tint.color : FZ.line.color, lineWidth: 1.5))
                            .contentShape(Rectangle())
                    }
                    .buttonStyle(.plain)
                }
            }
        }
        .padding(.horizontal, 20).padding(.top, 26).padding(.bottom, 16)
        .onGeometryChange(for: CGFloat.self) { $0.size.height } action: { height = $0 }
        .frame(maxHeight: .infinity, alignment: .top)
        .presentationDetents([.height(height)])
        .presentationDragIndicator(.visible)
        .presentationBackground(FZ.panel.color)
        .preferredColorScheme(.dark)
    }
}
