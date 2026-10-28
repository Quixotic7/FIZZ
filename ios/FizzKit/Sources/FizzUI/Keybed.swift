// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// The keybed (editor.js renderKeys; phase I5): the FM-1's 27 keys from F (F3 .. G5 at OCT 0), OCT − / +, each key a
// Note On while held, Off when let go, on the followed track's MIDI channel. A hardware keyboard plays it too, with the
// web's key map (A .. ' the white keys, W E R Y U O P [ the black ones, Z / X the octave).

import SwiftUI

enum KeyMap {
    /// a white key's and a black key's note from F
    static let white = [0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26]
    static let black: [(after: Int, note: Int)] = [(0, 1), (1, 3), (2, 5), (4, 8), (5, 10), (7, 13), (8, 15), (9, 17), (11, 20), (12, 22), (14, 25)]
    static let pcWhite = ["a", "s", "d", "f", "g", "h", "j", "k", "l", ";", "'"]
    static let pcBlack: [String: Int] = ["w": 1, "e": 3, "r": 5, "y": 8, "u": 10, "o": 13, "p": 15, "[": 17]

    /// A computer key's note at this lowest note, or nil.
    static func note(_ key: String, low: Int) -> Int? {
        if let w = pcWhite.firstIndex(of: key) { return low + white[w] }
        return pcBlack[key].map { low + $0 }
    }
}

/// The 27 keys and OCT − / +.
struct Keybed: View {
    let e: EditorModel
    var height: CGFloat = 120

    var body: some View {
        HStack(spacing: 8) {
            octButton(-1, "−")
            GeometryReader { g in
                let w = g.size.width / 16, lo = e.lowNote
                ZStack(alignment: .topLeading) {
                    ForEach(Array(KeyMap.white.enumerated()), id: \.offset) { i, o in
                        KeyCap(e: e, note: lo + o, black: false)
                            .frame(width: w - 2, height: g.size.height)
                            .offset(x: CGFloat(i) * w + 1)
                    }
                    ForEach(KeyMap.black, id: \.note) { k in
                        KeyCap(e: e, note: lo + k.note, black: true)
                            .frame(width: w * 0.62, height: g.size.height * 0.6)
                            .offset(x: CGFloat(k.after + 1) * w - w * 0.31)
                    }
                }
            }
            .padding(3)
            .background(RoundedRectangle(cornerRadius: 12).fill(RGB(0x0d0d10).color))
            .overlay(RoundedRectangle(cornerRadius: 12).strokeBorder(e.rec ? FZ.red.color : FZ.line.color, lineWidth: e.rec ? 2 : 1.5))
            .shadow(color: e.rec ? FZ.red.opacity(0.35) : .clear, radius: 8)
            VStack(spacing: 6) {
                octButton(1, "+")
                Text(e.octave > 0 ? "+\(e.octave)" : "\(e.octave)").fz(13, 800).foregroundStyle(FZ.muted.color)
            }
        }
        .frame(height: height)
    }

    func octButton(_ d: Int, _ s: String) -> some View {
        Button { e.setOctave(e.octave + d) } label: {
            Text(s).fz(22, 800).foregroundStyle(FZ.text.color)
                .frame(width: 40, height: 40)
                .background(Circle().fill(FZ.raised.color))
                .overlay(Circle().strokeBorder(FZ.line.color, lineWidth: 1.5))
        }
        .buttonStyle(.plain)
    }
}

/// One key: down while a finger holds it.
struct KeyCap: View {
    let e: EditorModel
    let note: Int
    let black: Bool
    @State private var down = false

    var body: some View {
        let on = e.held.contains(note)
        RoundedRectangle(cornerRadius: black ? 5 : 7)
            .fill(on ? FZ.pink.color : black ? RGB(0x1b1b21).color : FZ.cream.mix(88, FZ.panel).color)
            .overlay(RoundedRectangle(cornerRadius: black ? 5 : 7).strokeBorder(black ? FZ.line.color : .black.opacity(0.4), lineWidth: 1))
            .shadow(color: on ? FZ.pink.opacity(0.6) : .clear, radius: 8)
            .contentShape(Rectangle())
            .gesture(
                DragGesture(minimumDistance: 0)
                    .onChanged { _ in if !down { down = true; e.noteOn(note) } }
                    .onEnded { _ in down = false; e.noteOff(note) }
            )
    }
}

/// The phone's keybed grip: a swipe up (or a tap) slides the keybed up over the page, a swipe down puts it away.
struct KeybedGrip: View {
    @Binding var open: Bool

    var body: some View {
        Capsule().fill(open ? FZ.pink.color : FZ.dim.color).frame(width: 44, height: 5)
            .frame(maxWidth: .infinity).frame(height: 18)
            .contentShape(Rectangle())
            .onTapGesture { withAnimation(.spring(response: 0.35, dampingFraction: 0.85)) { open.toggle() } }
            .gesture(DragGesture(minimumDistance: 8).onEnded { g in
                withAnimation(.spring(response: 0.35, dampingFraction: 0.85)) { open = g.translation.height < 0 }
            })
    }
}

/// The phone's keybed, over the bottom of the page.
struct KeybedPanel: View {
    let e: EditorModel
    var body: some View {
        Keybed(e: e, height: 132)
            .padding(.horizontal, 12).padding(.vertical, 12)
            .background(RGB(0x09090c).color)
            .overlay(alignment: .top) { Rectangle().fill(FZ.line.color).frame(height: 1) }
    }
}

/// A hardware keyboard plays the keybed (editor.js pcNote): held keys sound until let go; Z / X the octave.
struct HardwareKeys: ViewModifier {
    let e: EditorModel
    @FocusState private var focused: Bool
    @State private var down: [String: Int] = [:]

    func body(content: Content) -> some View {
        content
            .focusable()
            .focusEffectDisabled()
            .focused($focused)
            .onAppear { focused = true }
            .onKeyPress(phases: [.down, .up]) { k in
                guard k.modifiers.intersection([.command, .control, .option]).isEmpty, !e.naming else { return .ignored }
                let c = k.characters.lowercased()
                if k.phase == .up {
                    guard let n = down.removeValue(forKey: c) else { return .ignored }
                    e.noteOff(n)
                    return .handled
                }
                if c == "z" || c == "x" { e.setOctave(e.octave + (c == "z" ? -1 : 1)); return .handled }
                guard down[c] == nil, let n = KeyMap.note(c, low: e.lowNote) else { return down[c] != nil ? .handled : .ignored }
                down[c] = n
                e.noteOn(n)
                return .handled
            }
    }
}

extension View {
    /// The keybed played from a hardware keyboard (iPad). (WIRE AT MERGE: LIBRARY, POOL and IMPORT take it too.)
    func hardwareKeys(_ e: EditorModel) -> some View { modifier(HardwareKeys(e: e)) }
}
