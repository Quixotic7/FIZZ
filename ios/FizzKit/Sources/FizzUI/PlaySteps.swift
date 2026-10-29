// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// PLAY's STEPS (mock-ups 38-40): the 16 pads are FIZZ's 16 white keys (its steps), pressed and let go as the panel's
// keys are, so a pad held is a step held; the chromatic keyboard sends MIDI notes on channel 15, which FIZZ plays on
// the selected track (core/app/app.h QD_MIDI_SEL) and, with a step held, puts into that step (ctl_rec.c
// qd_rec_midi_note: the notes held together its chord, up to six; a fresh press after all let go replaces it).
// With PLAY's SOUND on the FM-1, the pads press the FM-1's keys through its remote (channel 16, notes 53..79;
// midi_map.h) and the notes go to it on channel 15 the same way.

import CoreGraphics
import FizzEngine
import FizzKit
import Foundation

extension PlayModel {
    // MARK: the pads

    public func padDown(_ i: Int) {
        guard (0..<16).contains(i), !pads.contains(i) else { return }
        pads.insert(i)
        padNotes[i] = nil
        padFade[i, default: 0] += 1
        let k = StepsInput.padKey(i)
        if soundNow == .fm1 {
            sendFM1(channel: StepsInput.remoteChannel, note: StepsInput.remoteNote(k), velocity: 100)
        } else if let e = emulator {
            if e.uiInfo().hasPrefix("idle:") { e.tap(.home) }       // (a press that wakes FIZZ does nothing else)
            e.noteOn(key: k, velocity: 100)
        }
        Haptics.press()
    }

    public func padUp(_ i: Int) {
        guard pads.remove(i) != nil else { return }
        let k = StepsInput.padKey(i)
        if soundNow == .fm1 {
            sendFM1(channel: StepsInput.remoteChannel, note: StepsInput.remoteNote(k), velocity: 0)
        } else {
            emulator?.noteOff(key: k)
        }
        // its notes stay a moment, then fade
        guard padNotes[i] != nil else { return }
        let gen = padFade[i, default: 0]
        Task { [weak self] in
            try? await Task.sleep(for: .milliseconds(900))
            guard let self, self.padFade[i, default: 0] == gen, !self.pads.contains(i) else { return }
            self.padNotes[i] = nil
        }
    }

    // MARK: the keyboard

    public func noteDown(_ n: Int, velocity: Int) {
        guard (0...127).contains(n), !notes.contains(n) else { return }
        if notes.isEmpty { chord = [] }
        notes.insert(n)
        chord = StepsInput.chordAdding(n, to: chord)
        for i in pads { padNotes[i] = chord }
        let v = max(1, min(127, velocity))
        if soundNow == .fm1 {
            sendFM1(channel: StepsInput.selChannel, note: n, velocity: v)
        } else {
            emulator?.midiIn([0x90 | UInt8(StepsInput.selChannel), UInt8(n), UInt8(v)])
        }
    }

    public func noteUp(_ n: Int) {
        guard notes.remove(n) != nil else { return }
        if soundNow == .fm1 {
            sendFM1(channel: StepsInput.selChannel, note: n, velocity: 0)
        } else {
            emulator?.midiIn([0x80 | UInt8(StepsInput.selChannel), UInt8(n), 0])
        }
    }

    /// OCT - / +: the keyboard an octave down or up (its lowest C, C2..C5); the notes held let go first.
    func stepsOctave(by d: Int) {
        let o = max(StepsInput.octaves.lowerBound, min(StepsInput.octaves.upperBound, stepsOctave + d))
        guard o != stepsOctave else { return }
        for n in notes { noteUp(n) }
        stepsOctave = o
        Haptics.tick()
    }

    /// A note's slot in the chord (its colour), nil: not in it.
    func slot(_ n: Int) -> Int? { chord.firstIndex(of: n) }

    /// A pad's light: the emulated FIZZ's LED for its key, or held (the FM-1's keys: only held).
    func padLED(_ i: Int) -> Double {
        if pads.contains(i) { return 1 }
        return soundNow == .fm1 ? 0 : keyLED(StepsInput.padKey(i))
    }

    /// STEPS shown: FIZZ put in its step view if it shows its live keys, the track map or its idle screen (HOME: back
    /// to the view in use); anything open in the step view stays as it is.
    func stepsEntered() {
        guard soundNow == .here, let e = emulator, e.isBooted else { return }
        let m = e.uiInfo()
        if m.hasPrefix("keys:") || m.hasPrefix("tracks:") || m.hasPrefix("idle:") { e.tap(.home) }
    }

    /// To the FM-1 in the order played (each send waits for the one before).
    private func sendFM1(channel: Int, note: Int, velocity: Int) {
        guard let u = unit else { return }
        let before = fm1Sends
        fm1Sends = Task { @MainActor in
            await before?.value
            await u.actions.note(channel: channel, note: note, velocity: velocity)
        }
    }
}

// MARK: - The pure parts

public enum StepsInput {
    /// FIZZ's channel for the selected track (channel 15; app.h QD_MIDI_SEL)
    public static let selChannel = 14
    /// .. and its remote's, the panel as if by hand (channel 16; QD_MIDI_REMOTE)
    public static let remoteChannel = 15
    /// The keyboard's lowest C: C2 .. C5
    public static let octaves = 2...5

    /// Pad i's key (FizzEmulator's 0 F3 .. 26 G5): the i-th white key.
    public static func padKey(_ i: Int) -> Int { KeyLayout.whites[max(0, min(15, i))] }
    /// The remote's note for key k (midi_map.h: notes 53..79 the 27 keys)
    public static func remoteNote(_ k: Int) -> Int { 53 + k }

    /// The chord as FIZZ builds it (ctl_rec.c chord_add): a new note at the end, none twice, six at most, none more
    /// than four octaves from the first.
    public static func chordAdding(_ n: Int, to c: [Int]) -> [Int] {
        guard c.count < 6, !c.contains(n) else { return c }
        if let f = c.first, abs(n - f) > 48 { return c }
        return c + [n]
    }

    /// "C4" (MIDI 60), "F#5".
    public static func name(_ n: Int) -> String {
        let l = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]
        return "\(l[((n % 12) + 12) % 12])\(n / 12 - 1)"
    }

    /// The range a keyboard of `whites` white keys from C<octave> shows ("C4–F5").
    public static func range(octave: Int, whites: Int) -> String {
        let lo = 12 * (octave + 1)
        return "\(name(lo))–\(name(PianoLayout(size: CGSize(width: 100, height: 100), whites: whites, octave: octave).whiteNotes.last ?? lo))"
    }
}

/// A chromatic keyboard from C<octave>: `whites` white keys across, the black keys over their gaps (the mock-up's
/// piano: black keys 62 % of a white's pitch wide, 58 % of the height).
public struct PianoLayout: Sendable, Equatable {
    public let size: CGSize
    public let whites: Int
    public let octave: Int
    public let gap: CGFloat

    public init(size: CGSize, whites: Int, octave: Int, gap: CGFloat = 4) {
        self.size = size
        self.whites = max(1, whites)
        self.octave = octave
        self.gap = gap
    }

    static let whiteSemis = [0, 2, 4, 5, 7, 9, 11]

    public var pitch: CGFloat { (size.width + gap) / CGFloat(whites) }
    public var keyWidth: CGFloat { pitch - gap }
    public var blackWidth: CGFloat { pitch * 0.62 }
    public var blackHeight: CGFloat { (size.height * 0.58).rounded() }

    /// The white keys' MIDI notes, left to right.
    public var whiteNotes: [Int] {
        (0..<whites).map { 12 * (octave + 1) + 12 * ($0 / 7) + Self.whiteSemis[$0 % 7] }
    }

    public func whiteFrame(_ i: Int) -> CGRect {
        CGRect(x: CGFloat(i) * pitch, y: 0, width: keyWidth, height: size.height)
    }

    /// The black keys: (the note, its frame); one after each C, D, F, G, A that is not the last white.
    public var blacks: [(note: Int, rect: CGRect)] {
        whiteNotes.enumerated().compactMap { i, n in
            guard i < whites - 1, [0, 2, 5, 7, 9].contains(n % 12) else { return nil }
            let cx = CGFloat(i + 1) * pitch - gap / 2
            return (n + 1, CGRect(x: cx - blackWidth / 2, y: 0, width: blackWidth, height: blackHeight))
        }
    }

    /// The note under a point and where on its key (0 far end .. 1 near end); black keys first.
    public func note(at p: CGPoint) -> (note: Int, depth: CGFloat)? {
        guard p.y >= 0, p.y <= size.height else { return nil }
        for b in blacks where b.rect.contains(p) { return (b.note, (p.y - b.rect.minY) / max(1, b.rect.height)) }
        let i = Int(((p.x + gap / 2) / pitch).rounded(.down))
        guard (0..<whites).contains(i) else { return nil }
        return (whiteNotes[i], p.y / max(1, size.height))
    }
}
