// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// MIDI learn end to end on PLAY against the emulated FIZZ (mock-up 56): learn on, KN1 picked, a controller's CC binds
// it; after Done the same CC turns KN1 in the emulator (its screen as FIZZ's own turn of K1 draws it); a controller's
// note plays FIZZ's key, and a CC no map takes reaches FIZZ's MIDI in. (In the Emulator suite: the core is the
// process's one.)

import Foundation
import Testing
import FizzKit
@testable import FizzEngine
@testable import FizzUI

extension EmulatorTests {
    @MainActor @Test func learnBindsACCAndItTurnsTheKnobInTheEmulator() async throws {
        let e = FizzEmulator(storage: nil)
        // what K1 turned 3 clockwise draws at 900 ms, and what no turn draws
        e.boot(flash: nil)
        e.run(ms: 600)
        e.turn(.k1, by: 3)
        e.run(ms: 300)
        let turned = Self.lcd(e)
        e.boot(flash: nil)
        e.run(ms: 900)
        let plain = Self.lcd(e)
        #expect(turned != nil && turned != plain)

        e.boot(flash: nil)
        e.run(ms: 600)
        let h = EmulatorHost(emulator: e)
        let play = PlayModel(host: h, defaults: UserDefaults(suiteName: "midi.learn.\(UUID().uuidString)")!, midiFile: nil)
        let midi = play.midi
        midi.startLearn()
        #expect(midi.learning && midi.learnLine == "Tap a knob or a button on the panel")
        midi.pick("KN1")
        #expect(midi.learnLine == "KN1: now turn a knob or a pot on your MIDI device")
        #expect(midi.mark("KN1") == .picked && midi.mark("KN2") == .learnable)
        // an encoder sending 65 for each clockwise step: bound, binary offset
        #expect(midi.router.receive([0xB0, 21, 65]) == .learn(.bound(MidiMapping(src: "cc:1:21", target: "KN1", mode: .rel64), fresh: true)))
        #expect(midi.learnLine == "KN1 ← CC 21 ch 1" && midi.canForget)
        #expect(play.angles[.k1] == 0, "learn's CC turns nothing")
        midi.doneLearn()
        #expect(!midi.learning && midi.mark("KN1") == .bound)
        for _ in 0..<3 { midi.router.receive([0xB0, 21, 65]) }
        #expect(play.angles[.k1] == 45, "PLAY's KN1 turned 3 detents")
        e.run(ms: 300)
        #expect(Self.lcd(e) == turned, "FIZZ's KN1 turned 3 clockwise")

        // a note plays FIZZ's key; an unmapped CC goes to FIZZ's MIDI in
        #expect(midi.router.receive([0x90, 62, 100]) == .key(9))
        #expect(play.held == [9])
        midi.router.receive([0x80, 62, 0])
        #expect(play.held.isEmpty)
        #expect(midi.router.receive([0xB0, 74, 10]) == .fizz)
        await h.close()
    }
}
