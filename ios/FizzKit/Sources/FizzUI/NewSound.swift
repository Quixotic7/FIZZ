// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// LIBRARY's + NEW (mock-up 46): beside SELECT on the unit's lists, a sheet with two big cards, FM SOUND (a new INIT:
// FIZZ's defaults, a track's mix) or MIDI SOUND (28 slots, slot k's CC 16 + k, untouched): sound.js newSound in
// JavaScriptCore. Either opens in EDIT unsaved (the "NEW" label): SAVE has nothing to save over, SAVE AS NEW breathes
// and names its place (the first free MY PRESETS place on the unit in use); nothing is stored until then. Leaving EDIT
// for another tab (the tab guarded before it changes: FizzAppView) or opening another sound into EDIT asks first:
// SAVE AS NEW (then on), DISCARD (then on), or STAY (the new sound kept, nothing else opened).

import SwiftUI
import FizzData

// MARK: - The model's part

extension EditorModel {
    /// + NEW's sound is in EDIT, stored nowhere yet.
    public var unsavedNew: Bool { place.unsaved && sound != nil }

    /// Where SAVE AS NEW puts a sound on the unit now: its first free MY PRESETS place (nil: no unit, or full).
    public var firstFreeMine: Int? {
        guard let u = unit, u.status != nil else { return nil }
        let used = Set(u.mine.map(\.slot)), n = max(1, u.status?.nlib ?? 256)
        return (1...n).first { !used.contains($0) }
    }

    /// The tab about to change (before it does): false while + NEW's sound is unsaved in EDIT, the question asked
    /// instead (the tab switches on SAVE AS NEW or DISCARD). IMPORT's sheet over EDIT is not leaving it.
    public func mayLeave(from old: FizzTab, to new: FizzTab) -> Bool {
        guard old == .edit, new != .edit, new != .importer, unsavedNew else { return true }
        if asking == nil { asking = .leave(new) }
        return false
    }

    /// Another sound asked into EDIT over + NEW's unsaved sound: asked first. true: asked (the target waits).
    func asksBeforeReplacing(with t: EditTarget) -> Bool {
        guard unsavedNew else { return false }
        if asking == nil { asking = .replace(t) }
        return true
    }

    /// The question answered: SAVE AS NEW (then on: the tab, or the other sound opened), DISCARD (the new sound
    /// dropped, then on), or STAY (the new sound kept, nothing else opened).
    public func answerLeaving(_ a: LeaveAnswer) async {
        guard let q = asking else { return }
        switch a {
        case .stay:
            asking = nil
            return
        case .save:
            let x = await save(asNew: true)
            asking = nil
            guard case .placed = x else { return }
        case .discard:
            asking = nil
            discardNew()
        }
        switch q {
        case .leave(let t): unit?.goTo = t
        case .replace(let t):
            request = t
            await show()
        }
    }

    /// + NEW's sound dropped: EDIT follows the track again (or a blank INIT) when it next shows.
    public func discardNew() {
        guard place.unsaved else { return }
        opened = nil
        silent = nil
        dropSound()
    }

    public enum LeaveAnswer: Sendable { case save, discard, stay }
}

/// What waits on the question about + NEW's unsaved sound: another tab, or another sound opened into EDIT.
public enum NewSoundAsk: Sendable, Equatable {
    case leave(FizzTab)
    case replace(EditTarget)
}

// MARK: - LIBRARY's + NEW

/// + NEW beside SELECT (LIBRARY, the unit's lists): the FM SOUND / MIDI SOUND sheet.
struct NewSoundButton: View {
    let model: UnitModel
    var height: CGFloat = 36
    @State private var open = NewSoundLaunch.sheet && NewSoundLaunch.pending

    var body: some View {
        Button { open = true } label: {
            HStack(spacing: 6) {
                Image(systemName: "plus").font(.system(size: 13, weight: .heavy))
                Text("NEW").fz(13, 800, em: 0.08)
            }
            .foregroundStyle(FZ.text.color)
            .padding(.horizontal, 14).frame(height: height)
            .background(Capsule().fill(FZ.raised.color))
            .overlay(Capsule().strokeBorder(FZ.line.color, lineWidth: 1.5))
            .frame(height: max(height, 44))
            .contentShape(Rectangle())
            .fixedSize()
        }
        .buttonStyle(PressStyle(scale: 0.94))
        .accessibilityLabel("New sound")
        .sheet(isPresented: $open) {
            NewSoundSheet(model: model) { open = false }
                .presentationDetents([.height(NewSoundSheet.height)])
                .presentationDragIndicator(.visible)
                .presentationBackground(FZ.panel.color)
                .preferredColorScheme(.dark)
        }
    }
}

/// The sheet: NEW, two big cards, where SAVE AS NEW will put it.
struct NewSoundSheet: View {
    let model: UnitModel
    var done: () -> Void = {}
    static let height: CGFloat = 400

    var body: some View {
        VStack(alignment: .leading, spacing: 16) {
            Text("NEW").fz(32, 800, em: 0.02).foregroundStyle(FZ.text.color)
            HStack(spacing: 12) {
                card(midi: false)
                card(midi: true)
            }
            .frame(height: 236)
            HStack(spacing: 10) {
                Text("SAVE AS NEW → MY PRESETS").fz(13, 800, em: 0.1).foregroundStyle(FZ.muted.color)
                if let n = model.editor.firstFreeMine { SlotBadge(n: n) }
            }
            .frame(maxWidth: .infinity)
        }
        .padding(.horizontal, 18).padding(.top, 30).padding(.bottom, 16)
        .frame(maxHeight: .infinity, alignment: .top)
    }

    private func card(midi: Bool) -> some View {
        let c = midi ? FZ.magenta : FZ.cyan
        return Button {
            model.editTarget = .new(midi: midi)
            model.goTo = .edit
            done()
        } label: {
            VStack(spacing: 14) {
                Group { if midi { DinGlyph(color: c) } else { FMGlyph(color: c) } }
                    .frame(width: 92, height: 92)
                Text(midi ? "MIDI SOUND" : "FM SOUND").fz(19, 800, em: 0.04).foregroundStyle(FZ.text.color)
                    .lineLimit(1).minimumScaleFactor(0.7)
                Text(midi ? "28 CC" : "INIT").fz(12, 800, em: 0.08).foregroundStyle(c.color)
                    .padding(.horizontal, 10).frame(height: 24)
                    .overlay(RoundedRectangle(cornerRadius: 6).strokeBorder(c.mix(60, FZ.line).color, style: StrokeStyle(lineWidth: 1.5, dash: [4, 3])))
            }
            .frame(maxWidth: .infinity, maxHeight: .infinity)
            .background(RoundedRectangle(cornerRadius: 20).fill(c.mix(14, FZ.panel).color))
            .overlay(RoundedRectangle(cornerRadius: 20).strokeBorder(c.color, lineWidth: 2))
            .shadow(color: c.opacity(0.45), radius: 12)
            .contentShape(RoundedRectangle(cornerRadius: 20))
        }
        .buttonStyle(PressStyle(scale: 0.96))
        .accessibilityLabel(midi ? "New MIDI sound" : "New FM sound")
    }
}

/// The sheet over LIBRARY as presented (fizzshots).
public struct NewSoundSheetShot: View {
    let model: UnitModel
    public init(model: UnitModel) { self.model = model }
    public var body: some View { NewSoundSheet(model: model) }
}

/// A MY PRESETS place number in a white disc (SAVE AS NEW's badge).
struct SlotBadge: View {
    let n: Int
    var ink: RGB = RGB(0x000000)
    var fill: RGB = FZ.text
    var body: some View {
        Text("\(n)").fz(12, 800).monospacedDigit().foregroundStyle(ink.color)
            .padding(.horizontal, 6).frame(minWidth: 24, minHeight: 24)
            .background(Capsule().fill(fill.color))
    }
}

/// An FM sound: three operators, one modulating the others (the cards' icon).
struct FMGlyph: View {
    let color: RGB
    var body: some View {
        Canvas { ctx, sz in
            let s = min(sz.width, sz.height) / 100
            let p = { (x: CGFloat, y: CGFloat) in CGPoint(x: x * s, y: y * s) }
            var l = Path()
            l.move(to: p(38, 18)); l.addLine(to: p(38, 44)); l.addLine(to: p(18, 70))
            l.move(to: p(38, 44)); l.addLine(to: p(76, 70)); l.move(to: p(18, 70)); l.addLine(to: p(76, 70))
            ctx.stroke(l, with: .color(color.color), lineWidth: 6 * s)
            ctx.fill(Path(ellipseIn: CGRect(x: 26 * s, y: 32 * s, width: 24 * s, height: 24 * s)), with: .color(color.color))
            ctx.fill(Path(ellipseIn: CGRect(x: 60 * s, y: 54 * s, width: 32 * s, height: 32 * s)), with: .color(color.color))
            for (x, y, r) in [(38.0, 16.0, 10.0), (18.0, 70.0, 10.0)] {
                let rc = CGRect(x: (x - r) * s, y: (y - r) * s, width: 2 * r * s, height: 2 * r * s)
                ctx.fill(Path(ellipseIn: rc), with: .color(FZ.panel.color))
                ctx.stroke(Path(ellipseIn: rc), with: .color(color.color), lineWidth: 6 * s)
                ctx.fill(Path(ellipseIn: rc.insetBy(dx: 5 * s, dy: 5 * s)), with: .color(color.color))
            }
        }
    }
}

/// A MIDI sound: a DIN socket, five pins and its key (the cards' icon).
struct DinGlyph: View {
    let color: RGB
    var body: some View {
        Canvas { ctx, sz in
            let s = min(sz.width, sz.height) / 100, c = CGPoint(x: 50 * s, y: 50 * s)
            ctx.stroke(Path(ellipseIn: CGRect(x: 8 * s, y: 8 * s, width: 84 * s, height: 84 * s)), with: .color(color.color), lineWidth: 7 * s)
            ctx.fill(Path(CGRect(x: 43 * s, y: 6 * s, width: 14 * s, height: 10 * s)), with: .color(color.color))
            for a in [-90.0, -45, 0, 45, 90] {
                let r = 27 * s, rad = (a - 90) * .pi / 180
                let x = c.x + r * cos(rad), y = c.y + 6 * s + r * sin(rad)
                ctx.fill(Path(ellipseIn: CGRect(x: x - 5.5 * s, y: y - 5.5 * s, width: 11 * s, height: 11 * s)), with: .color(color.color))
            }
        }
    }
}

// MARK: - Simulator screenshots

/// `-new fm` / `-new midi` (the demo unit only): EDIT opens on + NEW's sound; `-new sheet`: LIBRARY's sheet open;
/// `-page amp -mix`: AMP with PAN L12, DEL 24, REV 40 (mock-up 45).
@MainActor enum NewSoundLaunch {
    static let args = ProcessInfo.processInfo.arguments
    static var pending = args.contains("-new") || args.contains("-mix") || args.contains("-page")
    static var sheet: Bool { args.firstIndex(of: "-new").map { $0 + 1 < args.count && args[$0 + 1] == "sheet" } ?? false }

    static func apply(_ model: UnitModel) async {
        guard pending, model.source is DemoLinkSource else { return }
        pending = false
        for _ in 0..<500 where !model.dumpsLoaded { try? await Task.sleep(for: .milliseconds(20)) }
        if let k = args.firstIndex(of: "-page"), k + 1 < args.count, let p = EditPage(rawValue: args[k + 1]) { model.editor.page = p }
        if let k = args.firstIndex(of: "-new"), k + 1 < args.count, ["fm", "midi"].contains(args[k + 1]) {
            model.editTarget = .new(midi: args[k + 1] == "midi")
        }
    }

    /// After EDIT shows: the mix of mock-up 45.
    static func mix(_ e: EditorModel) {
        guard args.contains("-mix") else { return }
        e.set(EditMix.pan, -12); e.set(EditMix.chorus, 0); e.set(EditMix.delay, 24); e.set(EditMix.reverb, 40)
    }
}
