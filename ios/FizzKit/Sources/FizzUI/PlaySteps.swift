// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// PLAY's STEPS (mock-up 52): the 16 pads are FIZZ's 16 white keys (its steps), pressed and let go as the panel's
// keys are, so a pad held is a step held; the keybed under them (RemoteFM1's, chromatic) sends MIDI notes on
// channel 15, which FIZZ plays on the selected track (core/app/app.h QD_MIDI_SEL) and, with a step held, puts into
// that step (ctl_rec.c qd_rec_midi_note: the notes held together its chord, up to six; a fresh press after all let go
// replaces it).
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

    /// OCT - / +: the keybed an octave down or up (its lowest F, F1..F4); the notes held let go first.
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
    /// OCT - / +'s range (the keybed's lowest F: F1 .. F4)
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

    /// The note of STEPS' keybed key k (RemoteFM1's keys, F to G: k 0 is F3 at octave 4); OCT - / + move it an
    /// octave (octave 2..5: F1 .. F4 its lowest).
    public static func keyNote(_ k: Int, octave: Int) -> Int { 53 + 12 * (octave - 4) + k }
}
