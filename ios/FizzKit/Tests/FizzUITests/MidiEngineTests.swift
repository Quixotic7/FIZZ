// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2026 RemoteFM1 contributors
// Derived from RemoteFM1IOS af91bc5 RFMKit/Tests/RFMKitTests/MidiEngineTests.swift, adapted: FIZZ has no RemoteFM1
// engine, so the same checks run on FIZZ's MidiRouter (RFMMidiMap.cpp's MidiMapper through fzmm_*) against a panel
// that records what it is asked: the map's feed (test.mjs's MidiMap.feed checks: absolute and relative encoders at
// each speed, buttons, consumption, MASTER's pickup), learn taking messages, and the controller's notes on PLAY's keys
// (transposed), the rest to FIZZ's MIDI in. The emulated FIZZ turning: FizzEngineTests' MidiLearnEmulatorTests.

import CMidiMap
import FizzEngine
import Foundation
import XCTest
@testable import FizzUI

/// PLAY's panel as the router sees it, recording.
@MainActor final class RecordingPanel: MidiPanelTarget {
    var turns: [FizzKnob: Int] = [:]
    var buttons = Set<FizzButton>()
    var keys: [Int: Int] = [:]
    var stepsNotes = Set<Int>()
    var steps = false
    var fizz: [[UInt8]] = []
    var master = 768
    var masterSets = 0
    var pickups: [(pot: Int, first: Bool)] = []

    func midiKey(_ k: Int, velocity: Int) { if velocity > 0 { keys[k] = velocity } else { keys[k] = nil } }
    func midiStepsNote(_ n: Int, velocity: Int) { if velocity > 0 { stepsNotes.insert(n) } else { stepsNotes.remove(n) } }
    var midiStepsKeys: Bool { steps }
    func midiToFizz(_ bytes: [UInt8]) { fizz.append(bytes) }
    func midiTurn(_ k: FizzKnob, _ detents: Int) { turns[k, default: 0] += detents }
    func midiButton(_ b: FizzButton, down: Bool) { if down { buttons.insert(b) } else { buttons.remove(b) } }
    func midiMaster(_ v: Int) { master = v; masterSets += 1 }
    func midiPickup(pot: Int, first: Bool) { pickups.append((pot, first)) }
    var midiMasterNow: Int { master }
}

@MainActor final class MidiEngineTests: XCTestCase {
    private var panel: RecordingPanel!
    private var store: MidiMapStore!
    private var router: MidiRouter!

    override func setUp() async throws {
        panel = RecordingPanel()
        store = MidiMapStore(url: nil)
        router = MidiRouter(learn: MidiLearn(store: store), target: panel)
        store.onChange = { [unowned self] m in router.adopt(m) }
    }

    private func map(_ list: [MidiMapping], speed: Double = 1) {
        store.clear()
        for mp in list { store.bind(mp.src, to: mp.target, mode: mp.mode) }
        store.setSpeed(speed)
    }

    /// an absolute pot on KN1 at each speed: 40 41 43 42 -> its detents (the first value only sets the start)
    func testAbsolutePotTurnsAnEncoder() {
        for (n, (sp, want)) in [(1.0, [0, 1, 3, 2]), (2.0, [0, 2, 6, 4]), (0.5, [0, 0, 1, 1])].enumerated() {
            map([MidiMapping(src: "cc:1:\(21 + n)", target: "KN1", mode: .abs)], speed: sp)
            let base = panel.turns[.k1] ?? 0
            let got = [40, 41, 43, 42].map { v -> Int in router.receive(cc(21 + n, v)); return (panel.turns[.k1] ?? 0) - base }
            XCTAssertEqual(got, want, "absolute at \(sp)x")
        }
    }

    /// a pot learn saw at 10, mapped at 1/2x: 11 12 13 -> 0 1 0, back to 10: -1 (the half step carries over)
    func testHalfSpeedCarriesTheFraction() {
        store.setSpeed(0.5)
        router.learn.start()
        router.learn.select("KN2")
        XCTAssertEqual(router.receive(cc(24, 10)), .learn(.bound(MidiMapping(src: "cc:1:24", target: "KN2", mode: .rel2c), fresh: true)))
        router.learn.stop()
        store.setMode("cc:1:24", .abs)                    // (one value in 1..15 reads as two's complement: a pot here)
        var last = panel.turns[.k2] ?? 0
        var steps: [Int] = []
        for v in [11, 12, 13, 10] {
            router.receive(cc(24, v))
            let now = panel.turns[.k2] ?? 0
            steps.append(now - last)
            last = now
        }
        XCTAssertEqual(steps, [0, 1, 0, -1])
    }

    func testRelativeModes() {
        func rel(_ n: Int, _ mode: MidiMode, _ vals: [Int], speed: Double = 1) -> [Int] {
            map([MidiMapping(src: "cc:1:\(n)", target: "SELECT", mode: mode)], speed: speed)
            var last = panel.turns[.select] ?? 0
            return vals.map { v in
                router.receive(cc(n, v))
                let now = panel.turns[.select] ?? 0
                defer { last = now }
                return now - last
            }
        }
        XCTAssertEqual(rel(30, .rel64, [65, 66, 63, 60, 64]), [1, 2, -1, -4, 0], "binary offset")
        XCTAssertEqual(rel(31, .rel2c, [1, 3, 127, 125, 64]), [1, 3, -1, -3, -64], "two's complement")
        XCTAssertEqual(rel(32, .relsign, [1, 3, 65, 67]), [1, 3, -1, -3], "sign bit")
        XCTAssertEqual(rel(33, .rel64, [65], speed: 2), [2], "the speed scales a relative encoder too")
    }

    /// a note and a CC on HOME: each presses and releases it; two sources hold it until both let go. A mapped message
    /// is consumed (no key, nothing to FIZZ's MIDI in); another number or channel flows on
    func testButtonsAndConsumption() {
        map([MidiMapping(src: "note:1:60", target: "HOME"), MidiMapping(src: "cc:1:64", target: "HOME"),
             MidiMapping(src: "cc:1:21", target: "KN1")])
        let held = [noteOn(60), cc(64, 127), noteOff(60), cc(64, 0), cc(64, 63)].map { m -> Bool in
            router.receive(m)
            return panel.buttons.contains(.home)
        }
        XCTAssertEqual(held, [true, true, true, false, false])
        XCTAssertTrue(panel.keys.isEmpty, "a mapped note plays no key")
        XCTAssertTrue(panel.fizz.isEmpty, "nothing mapped reaches FIZZ's MIDI in")
        XCTAssertEqual(router.receive(cc(21, 3)), .mapped)
        XCTAssertEqual(router.receive(cc(21, 3, 2)), .fizz, "the same CC on another channel flows on")
        XCTAssertEqual(panel.fizz, [cc(21, 3, 2)])
        // a note stored on a knob is ignored by the mapper too: it plays its key
        var bad = MidiMap([MidiMapping(src: "note:1:62", target: "HOME")]).entries
        bad[0].target = UInt8(RFME_E_K1)
        let m = fzmm_new()!
        defer { fzmm_free(m) }
        _ = bad.withUnsafeBufferPointer { fzmm_adopt(m, $0.baseAddress, 1, 1) }
        var act = fzmm_act_t()
        XCTAssertEqual(noteOn(62).withUnsafeBufferPointer { fzmm_feed(m, $0.baseAddress, 3, 512, &act) }, 0)
    }

    /// a new map lets go of what the old one held
    func testNewMapReleasesItsButtons() {
        map([MidiMapping(src: "cc:1:80", target: "FX")])
        router.receive(cc(80, 127))
        XCTAssertTrue(panel.buttons.contains(.fx))
        store.clear()
        XCTAssertFalse(panel.buttons.contains(.fx))
    }

    /// test.mjs's pickup checks: a pot away from MASTER waits (where it is, said once), crossing or reaching it
    /// picks MASTER up, MASTER moved by anything else: it waits again. MASTER pickup off: a pot sets MASTER at once
    func testMasterPickup() {
        map([MidiMapping(src: "cc:1:22", target: "MASTER"), MidiMapping(src: "cc:1:23", target: "MASTER")])
        router.settings.masterPickup = false
        router.receive(cc(22, 64))
        XCTAssertEqual(panel.master, 516, "pickup off: set at once")
        router.settings.masterPickup = true
        panel.master = 768                                  // (96.0 in CC steps)
        for v in [20, 40, 60] { router.receive(cc(22, v)) }
        XCTAssertEqual(panel.master, 768, "a pot below MASTER is ignored")
        XCTAssertEqual(panel.pickups.map(\.pot), [161, 322, 483], "where the pot is")
        XCTAssertEqual(panel.pickups.map(\.first), [true, false, false], "the first ignore says so once")
        router.receive(cc(22, 100))
        XCTAssertEqual(panel.master, 806, "crossing MASTER's position (60 -> 100) picks it up")
        router.receive(cc(22, 127))
        XCTAssertEqual(panel.master, 1023)
        router.receive(cc(22, 0))
        XCTAssertEqual(panel.master, 0, "then it follows directly")
        panel.master = 512                                  // (the screen moved MASTER)
        router.receive(cc(22, 10))
        XCTAssertEqual(panel.master, 512, "after MASTER moves by anything else, the pot waits again")
        XCTAssertEqual(panel.pickups.last?.first, true)
        router.receive(cc(22, 63))
        XCTAssertEqual(panel.master, 507, "within 2 steps of MASTER (63 ~ 512) picks it up")
        panel.master = 768
        router.receive(cc(23, 95))
        XCTAssertEqual(panel.master, 765, "a pot not seen before picks up at once within 2 steps")
    }

    /// learn armed (KN1 picked): its CCs bind and play nothing, a note is refused and plays nothing, pitch bend flows
    /// on; disarmed, notes play keys again and the pot turns from what learn saw (90)
    func testLearnTakesMessages() {
        map([MidiMapping(src: "cc:1:21", target: "KN2")])
        router.learn.start()
        XCTAssertEqual(router.receive(cc(21, 5)), .mapped, "nothing picked: learn leaves it alone")
        router.learn.select("KN1")
        let k1 = panel.turns[.k1] ?? 0
        XCTAssertEqual(router.receive(noteOn(62)), .learn(.rejected("KN1 takes a CC: move a knob or a pot")))
        XCTAssertTrue(panel.keys.isEmpty, "a note-on learn takes plays nothing")
        XCTAssertEqual(router.receive(cc(21, 40)), .learn(.bound(MidiMapping(src: "cc:1:21", target: "KN1", mode: .abs), fresh: true)))
        router.receive(cc(21, 90))
        XCTAssertEqual(panel.turns[.k1] ?? 0, k1, "nor does a CC learn takes")
        XCTAssertEqual(router.receive([0xE0, 0, 64]), .fizz, "pitch bend: not learn's")
        XCTAssertEqual(store.map.get("cc:1:21")?.target, "KN1", "the source moved from KN2 to KN1")
        router.learn.stop()
        XCTAssertEqual(router.receive(noteOn(62)), .key(9), "disarmed: notes play again")
        router.receive(noteOff(62))
        router.receive(cc(21, 91))
        XCTAssertEqual((panel.turns[.k1] ?? 0) - k1, 1, "the pot turns from what learn saw (90)")
    }

    /// notes play PLAY's keys (53 + k, transposed), the rest to FIZZ's MIDI in; "...and reach" sends the key notes there
    /// too; a note-off lets go of the key its note-on pressed even after the transpose changed; STEPS takes the notes
    func testControllerNotes() {
        XCTAssertEqual(router.receive(noteOn(62, 90)), .key(9), "D4 holds key 9")
        XCTAssertEqual(panel.keys, [9: 90])
        router.settings.transpose = 1
        XCTAssertEqual(router.receive(noteOff(62)), .key(9), "its note-off lets go of key 9")
        XCTAssertTrue(panel.keys.isEmpty)
        XCTAssertEqual(router.receive(noteOn(50)), .key(9), "+1 octave: D3 plays key 9")
        router.receive(noteOff(50))
        XCTAssertEqual(router.receive(noteOn(30)), .fizz, "a note past the keys: FIZZ's MIDI in")
        router.receive(noteOff(30))
        XCTAssertEqual(panel.fizz, [noteOn(30), noteOff(30)])
        router.settings.transpose = 0
        router.settings.reachFizz = true
        router.receive(noteOn(60))
        router.receive(noteOff(60))
        XCTAssertEqual(panel.fizz.suffix(2), [noteOn(60), noteOff(60)], "the key notes reach FIZZ's MIDI in too")
        router.settings.reachFizz = false
        router.settings.notesPlayKeys = false
        XCTAssertEqual(router.receive(noteOn(60)), .fizz, "notes not playing the keys go to FIZZ's MIDI in")
        XCTAssertTrue(panel.keys.isEmpty)
        router.settings.notesPlayKeys = true
        panel.steps = true
        XCTAssertEqual(router.receive(noteOn(40)), .stepsNote(40), "STEPS: any note, its chromatic keybed")
        router.receive(noteOff(40))
        XCTAssertTrue(panel.stepsNotes.isEmpty)
        // the rest
        XCTAssertEqual(router.receive(cc(74, 10)), .fizz)
        XCTAssertEqual(router.receive([0xE0, 0, 64]), .fizz)
        XCTAssertEqual(router.receive([0xC0, 5]), .fizz)
        XCTAssertEqual(router.receive([0xF8]), .fizz, "clock")
        XCTAssertEqual(router.receive([0xFE]), .dropped, "active sensing")
        XCTAssertEqual(router.receive([0xF0, 1, 2, 0xF7]), .dropped, "SysEx: not passed")
        XCTAssertEqual(router.receive([0x40]), .dropped, "no status byte")
        XCTAssertEqual(panel.fizz.suffix(4), [cc(74, 10), [0xE0, 0, 64], [0xC0, 5], [0xF8]])
    }

    /// the page's settings are kept
    func testMidiSettingsAreKept() throws {
        let suite = "fizz-midi-\(UUID().uuidString)"
        let d = try XCTUnwrap(UserDefaults(suiteName: suite))
        defer { d.removePersistentDomain(forName: suite) }
        let a = MidiHub(play: nil, defaults: d, store: MidiMapStore(url: nil))
        XCTAssertTrue(a.notesPlayKeys && !a.reachFizz && a.transpose == 0 && a.masterPickup)
        a.transpose = -1
        a.reachFizz = true
        a.masterPickup = false
        XCTAssertEqual(a.router.settings.transpose, -1)
        let b = MidiHub(play: nil, defaults: d, store: MidiMapStore(url: nil))
        XCTAssertEqual(b.transpose, -1)
        XCTAssertTrue(b.reachFizz)
        XCTAssertFalse(b.masterPickup)
        XCTAssertTrue(b.notesPlayKeys)
        XCTAssertFalse(b.router.settings.masterPickup)
    }
}
