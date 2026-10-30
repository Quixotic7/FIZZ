// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// fizzshots for mock-ups 55 and 56 (`fizzshots OUTDIR midi` alone): PLAY's sheet › MIDI & audio (its top, scrolled to
// Knobs from MIDI, to the OMX-27 connected), its MIDI input page; learn on the panel (CONTROLS with KN3 picked, FULL
// with KN3 bound, after Done with MASTER waiting for its pot). The demo map, two controllers and an OMX-27 that
// answers, in memory (PlayModel.useMidiDemo).

#if os(macOS)
import FizzEngine
import FizzKit
import FizzUI
import SwiftUI

extension Shots {
    @MainActor
    static func midiShots(_ out: URL) async {
        let tmp = FileManager.default.temporaryDirectory
        let e = FizzEmulator(storage: nil)
        let host = EmulatorHost(emulator: e)
        let m = UnitModel(source: NoPorts(), emulator: host, backups: tmp.appendingPathComponent("fizzshots-m-\(UUID().uuidString)"),
                          projects: tmp.appendingPathComponent("fizzshots-mp-\(UUID().uuidString)"))
        host.launch()
        m.start()
        await until { m.onEmulator && m.dumpsLoaded && m.layout != nil }
        await host.serve(false)
        e.tap(.home)
        e.run(ms: 600)
        let p = host.play
        p.theme = .tangerine
        p.view = .full
        p.knobsKN = false
        p.showNow()
        await p.useMidiDemo(omx: true)
        let play = { (name: String) in save(RootView(model: m, tab: .constant(.play), wide: false), phone, out, name) }

        // 55: the page (a render leaves PLAY, which puts the sheet away: shown again each time)
        for (state, scroll, name) in [("page", 0.0, "midi-audio"), ("knobs", 470.0, "midi-audio-knobs"),
                                      ("omx", 760.0, "midi-audio-omx"), ("input", 0.0, "midi-input")] {
            p.showMidiDemo(state)
            p.midiShotScroll = scroll
            play(name)
        }
        p.midiShotScroll = 0

        // 56: learn on the panel
        p.view = .panel
        p.knobsKN = true
        p.showMidiDemo("pick:KN3")
        play("midi-learn-controls-picked")
        p.view = .full
        p.knobsKN = false
        p.showMidiDemo("bound:KN3")
        play("midi-learn-full-bound")
        p.midi.doneLearn()
        p.showMidiDemo("pickup")
        play("midi-learn-pickup")
        p.view = .steps
        p.showMidiDemo("learn")
        play("midi-learn-steps")
        p.midi.doneLearn()
        p.view = .full
        p.showMidiDemo("learn")
        save(RootView(model: m, tab: .constant(.play), wide: true), pad, out, "ipad-midi-learn")
        p.midi.doneLearn()
        m.stop()
        e.stop()
    }
}
#endif
