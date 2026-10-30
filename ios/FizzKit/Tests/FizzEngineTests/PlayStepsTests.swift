// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// PLAY's STEPS against the emulated FIZZ, headless (its clock run by hand): a pad held and notes played put the chord
// into that step, read back from FIZZ's store (the pattern record's step, core/seq/pattern.c qd_step_wire); a fresh
// chord replaces it; notes with no pad held leave the steps alone; two pads held both take it. Then the pure parts:
// the pads' keys, the chord's rule, the keybed's notes, the swipe order. (The emulator tests are in the
// Emulator suite: the core is the process's one.)

import CoreGraphics
import Foundation
import Testing
import FizzKit
@testable import FizzEngine
@testable import FizzUI

/// A step's notes in FIZZ's store: the current pattern's record, `track`'s step `step` (page 1).
private func stepNotes(_ e: FizzEmulator, track: Int, step: Int) -> [Int]? {
    let info = e.uiInfo()
    // "steps: A01 stopped, 120 BPM, track 1 .."
    guard let r = info.range(of: #": [A-D]\d\d "#, options: .regularExpression) else { return nil }
    let pat = info[r].dropFirst(2).prefix(3)
    let k = Int(pat.first!.asciiValue! - 65) * 16 + Int(pat.dropFirst())! - 1
    let recs = StoreImage.records(e.flashImage(), StoreImage.host).prj.filter { $0.key >> 24 == 3 && Int($0.key & 0xFFFF) == k }
    for rec in recs {
        let b = rec.bytes
        guard b.count >= 12, b[0] == 0x50, (2...6).contains(b[1]), b[2] == 20 else { continue }
        let g16 = { (o: Int) in Int(b[o]) | Int(b[o + 1]) << 8 }
        var o = 12 + (b[1] >= 5 ? 12 : 0) + Int(b[3]) * 6 + 4 * 6
        for t in 0..<6 {
            let mask = b[o]
            o += 1
            for p in 0..<8 where (mask >> p) & 1 == 1 {
                let sm = g16(o)
                o += 2
                for s in 0..<16 where (sm >> s) & 1 == 1 {
                    if t == track && p == 0 && s == step { return b[(o + 4)..<(o + 10)].filter { $0 != 0 }.map(Int.init) }
                    o += 20
                }
            }
        }
        return []
    }
    return nil
}

/// PLAY on a fresh emulated FIZZ, its clock run by hand (the host kept: PlayModel holds it weakly).
@MainActor
private func stepsPlay(_ e: FizzEmulator) -> (PlayModel, EmulatorHost) {
    let h = EmulatorHost(emulator: e)
    let p = PlayModel(host: h, defaults: UserDefaults(suiteName: "play.steps.\(UUID().uuidString)")!)
    e.boot(flash: nil)
    e.run(ms: 400)
    p.view = .steps
    e.run(ms: 100)
    return (p, h)
}

extension EmulatorTests {
    @MainActor @Test func aHeldPadTakesTheChord() {
        let e = FizzEmulator(storage: nil)
        let (p, h) = stepsPlay(e)
        defer { withExtendedLifetime(h) {} }
        #expect(e.uiInfo().hasPrefix("steps:"), "\(e.uiInfo())")
        // step 1 held, C4 and G4 played together
        p.padDown(0)
        e.run(ms: 300)
        #expect(e.uiInfo().hasPrefix("step held:"), "\(e.uiInfo())")
        p.noteDown(60, velocity: 100)
        e.run(ms: 20)
        p.noteDown(67, velocity: 90)
        e.run(ms: 20)
        #expect(p.padNotes[0] == [60, 67])
        p.noteUp(60)
        p.noteUp(67)
        e.run(ms: 20)
        p.padUp(0)
        e.run(ms: 50)
        #expect(stepNotes(e, track: 0, step: 0) == [60, 67])
        // again, E4 alone: the fresh chord replaces it
        p.padDown(0)
        e.run(ms: 300)
        p.noteDown(64, velocity: 100)
        e.run(ms: 20)
        p.noteUp(64)
        p.padUp(0)
        e.run(ms: 50)
        #expect(stepNotes(e, track: 0, step: 0) == [64])
        // no pad held: the notes only play
        p.noteDown(72, velocity: 100)
        e.run(ms: 20)
        p.noteUp(72)
        e.run(ms: 50)
        #expect(stepNotes(e, track: 0, step: 0) == [64])
        #expect(p.pads.isEmpty && p.notes.isEmpty)
    }

    @MainActor @Test func twoHeldPadsBothTakeIt() {
        let e = FizzEmulator(storage: nil)
        let (p, h) = stepsPlay(e)
        defer { withExtendedLifetime(h) {} }
        p.padDown(2)
        p.padDown(9)
        e.run(ms: 300)
        for n in [57, 60, 64] { p.noteDown(n, velocity: 100); e.run(ms: 10) }
        p.letGoAll()
        e.run(ms: 50)
        #expect(stepNotes(e, track: 0, step: 2) == [57, 60, 64])
        #expect(stepNotes(e, track: 0, step: 9) == [57, 60, 64])
        #expect(stepNotes(e, track: 0, step: 3) != [57, 60, 64])
    }

    @MainActor @Test func stepsPutsFIZZBackInItsStepView() {
        let e = FizzEmulator(storage: nil)
        let h = EmulatorHost(emulator: e)
        defer { withExtendedLifetime(h) {} }
        let p = PlayModel(host: h, defaults: UserDefaults(suiteName: "play.steps.\(UUID().uuidString)")!)
        e.boot(flash: nil)
        e.run(ms: 400)
        e.tap(.home)
        e.run(ms: 50)
        e.tap(.rec)                                         // (REC: its live keys)
        e.run(ms: 100)
        #expect(e.uiInfo().hasPrefix("keys:"))
        p.view = .steps
        e.run(ms: 100)
        #expect(e.uiInfo().hasPrefix("steps:"), "\(e.uiInfo())")
        // a pad tapped: step 3 toggled on, its LED lit
        p.padDown(2)
        e.run(ms: 40)
        p.padUp(2)
        e.run(ms: 100)
        #expect(stepNotes(e, track: 0, step: 2) != nil)
        p.showNow()
        #expect(p.padLED(2) > 0)
    }
}

@MainActor
@Suite struct PlayStepsTests {
    @Test func padsAreTheWhiteKeys() {
        #expect((0..<16).map(StepsInput.padKey) == KeyLayout.whites)
        #expect(StepsInput.padKey(0) == 0 && StepsInput.padKey(15) == 26)
        #expect(StepsInput.remoteNote(0) == 53 && StepsInput.remoteNote(26) == 79)
    }

    @Test func theChordAsFIZZBuildsIt() {
        var c: [Int] = []
        for n in [60, 67, 60, 64, 71, 74, 77, 79] { c = StepsInput.chordAdding(n, to: c) }
        #expect(c == [60, 67, 64, 71, 74, 77])
        #expect(StepsInput.chordAdding(110, to: [60]) == [60])     // (over four octaves from the first)
    }

    @Test func theKeybed() {
        // STEPS' keybed is RemoteFM1's (F to G): at the first octave F3 .. G5, C4 its fifth white key, G4 its ninth
        #expect(StepsInput.keyNote(0, octave: 4) == 53 && StepsInput.keyNote(26, octave: 4) == 79)
        #expect(StepsInput.keyNote(KeyLayout.whites[4], octave: 4) == 60 && StepsInput.keyNote(KeyLayout.whites[8], octave: 4) == 67)
        // OCT - / +: an octave each
        #expect(StepsInput.keyNote(0, octave: 2) == 29 && StepsInput.keyNote(0, octave: 5) == 65)
        #expect(StepsInput.name(61) == "C#4" && StepsInput.name(36) == "C2")
    }

    @Test func octaveAndSwipes() {
        let p = PlayModel(host: nil, defaults: UserDefaults(suiteName: "play.steps.\(UUID().uuidString)")!)
        #expect(p.stepsOctave == 4)
        for _ in 0..<5 { p.stepsOctave(by: 1) }
        #expect(p.stepsOctave == 5)
        for _ in 0..<5 { p.stepsOctave(by: -1) }
        #expect(p.stepsOctave == 2)
        // FULL, KEYS, STEPS, SCREEN
        #expect(PlayModel.swiped(.keys, by: 1) == .steps)
        #expect(PlayModel.swiped(.panel, by: 1) == .steps)
        #expect(PlayModel.swiped(.steps, by: 1) == .screen)
        #expect(PlayModel.swiped(.steps, by: -1) == .keys)
        #expect(PlayViewKind.allCases.count == 5)
        // the pads' notes and the slots
        p.padDown(0)
        p.noteDown(60, velocity: 100)
        p.noteDown(67, velocity: 100)
        #expect(p.padNotes[0] == [60, 67] && p.slot(67) == 1)
        p.letGoAll()
        #expect(p.pads.isEmpty && p.notes.isEmpty)
        // a fresh press after all let go: a new chord
        p.noteDown(64, velocity: 100)
        #expect(p.chord == [64])
    }
}
