// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// The EDIT tab (mock-ups 11, 12, 17; editor.js renderHead, the panel, the keybed; phase I5). iPhone: the place and the
// name, the page chips, one page, REVERT / SAVE / SAVE AS NEW, the keybed's strip (swipe up). iPad: the TRACK switch
// and the page list on the left, the page in a card with its actions, the keybed across the bottom, a hardware
// keyboard plays it. A value turned takes the screen while the finger is down.

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
    @FocusState private var nameFocus: Bool
    @Environment(\.fizzSnapshot) private var snapshot

    private var e: EditorModel { model.editor }

    var body: some View {
        Group { if wide { padBody } else { phoneBody } }
            .overlay(alignment: .top) { turnOverlay }
            .overlay(alignment: .top) { flashView }
            .task { await e.show() }
            .onDisappear { e.hide() }
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
            ChipScroll { pageChips }
                .padding(.bottom, 8)
                .turnDim(e, [])
            pageBody
                .padding(.horizontal, 16)
                .frame(maxHeight: .infinity, alignment: .top)
                // (the keybed slides up over the page)
                .overlay(alignment: .bottom) {
                    if e.keysOpen {
                        KeybedPanel(e: e).transition(.move(edge: .bottom).combined(with: .opacity))
                    }
                }
                .clipped()
            KeybedGrip(open: Binding(get: { e.keysOpen }, set: { e.keysOpen = $0 }))
                .turnDim(e, [])
            actions(height: 46)
                .padding(.horizontal, 16)
                .turnDim(e, [])
        }
        .padding(.bottom, 94)
    }

    // MARK: iPad

    private var padBody: some View {
        VStack(spacing: 14) {
            HStack(alignment: .top, spacing: 14) {
                VStack(spacing: 8) {
                    TrackPill(model: model, wide: true).padding(.bottom, 6)
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
        .padding(.bottom, 20)
    }

    private func pageItem(_ title: String, _ c: RGB, _ art: VectorArt?, on: Bool, _ go: @escaping () -> Void) -> some View {
        Button(action: go) {
            HStack(spacing: 11) {
                if let art { IconView(art, color: c.color, lineWidth: 2.4, backdrop: .clear).frame(width: 24, height: 22) }
                Text(title).fz(14, 800, em: 0.08).foregroundStyle(on ? FZ.text.color : FZ.muted.color).lineLimit(1)
                    .minimumScaleFactor(0.75)
                Spacer(minLength: 0)
            }
            .padding(.horizontal, 14).frame(height: 54)
            .background(RoundedRectangle(cornerRadius: 14).fill(on ? c.mix(14, FZ.raised).color : FZ.raised.color))
            .overlay(RoundedRectangle(cornerRadius: 14).strokeBorder(on ? c.color : FZ.line.color, lineWidth: 1.5))
            .shadow(color: on ? c.opacity(0.35) : .clear, radius: 9)
            .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
    }

    // MARK: the parts

    /// The place, the name (a tap types it), its first tag (a tap: the tags), the EDITED dot.
    private func header(nameSize: CGFloat) -> some View {
        VStack(alignment: .leading, spacing: 5) {
            HStack(spacing: 8) {
                if e.sound?.isMidi == true {
                    Text("MIDI").fz(11, 800, em: 0.08).foregroundStyle(.black)
                        .padding(.horizontal, 6).frame(height: 18).background(RoundedRectangle(cornerRadius: 5).fill(FZ.magenta.color))
                }
                Text(e.place.label).fz(13, 800, em: 0.12).foregroundStyle(FZ.muted.color).lineLimit(1)
            }
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

    private var pageChips: some View {
        Group {
            if e.sound?.isMidi == true {
                ForEach(0..<7, id: \.self) { n in pageChip("MIDI \(n + 1)", FZ.magenta, nil, on: e.midiPage == n) { e.midiPage = n } }
            } else {
                ForEach(EditPage.allCases) { p in pageChip(p.title, p.tint, PageIcons.art(p), on: e.page == p) { e.page = p } }
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
            .fixedSize()
        }
        .buttonStyle(.plain)
    }

    @ViewBuilder private var pageBody: some View {
        let content = Group {
            if e.table == nil || e.sound == nil {
                Color.clear
            } else if e.sound?.isMidi == true {
                MidiPage(e: e, height: wide ? 380 : 250)
            } else {
                EditPageView(e: e, page: e.page, wide: wide)
            }
        }
        if snapshot {
            // (as a scroll view lays it out: each drawing as tall as its width makes it, then cut)
            Color.clear.overlay(alignment: .top) { content.fixedSize(horizontal: false, vertical: true) }.clipShape(OpenTop())
        } else if e.page == .ops && e.sound?.isMidi != true {
            content
        } else {
            ScrollView { content.padding(.bottom, 16) }.scrollIndicators(.hidden)
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
            action("↺ REVERT", ghost: true, on: e.dirty && !e.locked, fixed: fixed, height: height) { Task { await e.revert() } }
                .frame(width: w[0])
            action("SAVE", ghost: false, on: e.canSave && !e.saving, fixed: fixed, height: height) { confirmSave = true }
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
            Text(t).fz(13, 800, em: 0.08).lineLimit(1)
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

    /// The turned value at the top, over the dimmed screen (the phone: over the top bar too).
    @ViewBuilder private var turnOverlay: some View {
        if let t = e.turning {
            TurnOverlay(e: e, turn: t, size: wide ? 300 : 330)
                .padding(.top, wide ? 120 : 0)
                .offset(y: wide ? 0 : -40)
                .transition(.opacity)
        }
    }
}

/// FIZZ's seven tags, toggled.
struct EditTagsSheet: View {
    let e: EditorModel
    var body: some View {
        VStack(alignment: .leading, spacing: 16) {
            Text("TAGS").fz(16, 800, em: 0.12).foregroundStyle(FZ.muted.color)
            LazyVGrid(columns: [GridItem(.adaptive(minimum: 84), spacing: 8)], alignment: .leading, spacing: 8) {
                ForEach(FizzTag.allCases) { t in
                    let on = (e.sound?.tags ?? 0) >> t.rawValue & 1 == 1
                    Button { e.toggleTag(t.rawValue) } label: {
                        Text(t.name).fz(14, 800, em: 0.05).foregroundStyle(on ? t.ink : FZ.muted.color)
                            .frame(maxWidth: .infinity).frame(height: 40)
                            .background(RoundedRectangle(cornerRadius: 10).fill(on ? t.tint.color : .clear))
                            .overlay(RoundedRectangle(cornerRadius: 10).strokeBorder(on ? t.tint.color : FZ.line.color, lineWidth: 1.5))
                    }
                    .buttonStyle(.plain)
                }
            }
            Spacer(minLength: 0)
        }
        .padding(20)
        .presentationDetents([.height(220)])
        .presentationBackground(FZ.panel.color)
        .preferredColorScheme(.dark)
    }
}
