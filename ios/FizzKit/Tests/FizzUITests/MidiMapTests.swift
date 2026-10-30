// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2026 RemoteFM1 contributors
// Derived from RemoteFM1IOS af91bc5 RFMKit/Tests/RFMKitTests/MidiMapTests.swift, adapted: FIZZ's module, MidiLearn without
// an engine, FIZZ's store (no follower) and controller input in place of RemoteFM1's; a RemoteFM1 map file round trip.
// M8: the learned map without an engine. The website's web/site/test.mjs "MIDI learn" checks (targets, parsing, the
// mode guess, the list's lines, rebinding, learn, the stored form), the store and its follower, the controller input's
// UMP decoding and its FM-1 rule. (The map's running: MidiEngineTests.)

import Foundation
import CMidiMap
import XCTest
@testable import FizzUI

func cc(_ n: Int, _ v: Int, _ ch: Int = 1) -> [UInt8] { [0xB0 | UInt8(ch - 1), UInt8(n), UInt8(v)] }
func noteOn(_ n: Int, _ v: Int = 100, _ ch: Int = 1) -> [UInt8] { [0x90 | UInt8(ch - 1), UInt8(n), UInt8(v)] }
func noteOff(_ n: Int, _ ch: Int = 1) -> [UInt8] { [0x80 | UInt8(ch - 1), UInt8(n), 0] }

final class MidiMapTests: XCTestCase {
    func testTargets() {
        XCTAssertEqual(MidiTarget.all.count, 22)
        XCTAssertEqual(MidiTarget.all.filter(\.isEncoder).map(\.id), ["SELECT", "ALGORITHM", "PRESETS", "KN1", "KN2", "KN3", "KN4"])
        XCTAssertEqual(MidiTarget.named("KN1")?.kind, .encoder(.k1))
        XCTAssertEqual(MidiTarget.named("HOME")?.kind, .button(.home))
        XCTAssertEqual(MidiTarget.named("OCT+")?.kind, .button(.octUp))
        XCTAssertEqual(MidiTarget.named("OCT-")?.kind, .button(.octDown))
        XCTAssertEqual(MidiTarget.knob(.master).id, "MASTER")
        XCTAssertEqual(MidiTarget.button(.fx).id, "FX")
        // the engine's numbers: knob roles, MASTER, the buttons by label
        XCTAssertEqual(MidiTarget.named("KN4")?.engine, UInt8(RFME_E_K4))
        XCTAssertEqual(MidiTarget.named("MASTER")?.engine, UInt8(RFME_T_MASTER))
        XCTAssertEqual(MidiTarget.named("OCT+")?.engine, UInt8(RFME_T_BUTTON + RFME_B_OCTUP))
    }

    func testParse() {
        let p = MidiMessage(noteOn(60, 0, 3))
        XCTAssertEqual(MidiMessage(cc(21, 5))?.source, "cc:1:21")
        XCTAssertEqual(p?.kind, .note)
        XCTAssertEqual(p?.on, false)                     // (note-on at 0 is a note-off)
        XCTAssertEqual(p?.channel, 3)
        XCTAssertEqual(p?.source, "note:3:60")
        XCTAssertNil(MidiMessage([0xE0, 0, 64]))         // (pitch bend: none)
        XCTAssertNil(MidiMessage([0xB0, 1]))
        XCTAssertTrue(MidiSource.isValid("cc:16:127"))
        for bad in ["cc:0:1", "cc:17:1", "cc:1:128", "pc:1:1", "cc:01:1", "cc:1", "note:1:1:1"] {
            XCTAssertFalse(MidiSource.isValid(bad), bad)
        }
    }

    func testGuessMode() {
        let G = MidiMode.guess
        XCTAssertEqual([G([65, 65, 66]), G([63, 62, 63]), G([65])], [.rel64, .rel64, .rel64], "values hugging 64")
        XCTAssertEqual([G([1, 1, 2]), G([127, 126, 127]), G([1, 127, 1])], [.rel2c, .rel2c, .rel2c], "1..15 / 113..127")
        XCTAssertEqual([G([40, 42, 45]), G([10, 11, 12, 13]), G([60, 61, 62, 63, 64, 65]), G([0, 127]), G([])],
                       [.abs, .abs, .abs, .abs, .abs], "a pot sweeping (even at the bottom, or through 64)")
    }

    func testLabelsAndConsumption() {
        let m = MidiMap([MidiMapping(src: "note:1:60", target: "HOME"), MidiMapping(src: "cc:1:21", target: "KN1"),
                         MidiMapping(src: "note:1:61", target: "KN1")])
        XCTAssertEqual(m.mappings.count, 2, "a stored note on a knob is dropped")
        XCTAssertEqual(m.mappings[1].mode, .abs)
        XCTAssertEqual(m.mappings[1].description, "KN1 ← CC 21 ch 1 · absolute")
        XCTAssertEqual(m.mappings[0].description, "HOME ← note 60 ch 1")
        XCTAssertEqual(MidiSource.label("cc:2:7", long: true), "CC 7,\u{a0}channel\u{a0}2")
        XCTAssertEqual(MidiMode.allCases.map(\.label), ["absolute", "relative 64±", "relative 2's comp.", "relative sign bit"])
        XCTAssertEqual(MidiMap.canBind("KN1", MidiMessage(noteOn(60))), "KN1 takes a CC: move a knob or a pot")
        XCTAssertEqual(MidiMap.canBind("HOME", MidiMessage(noteOn(60))), "")
        XCTAssertEqual(MidiMap.canBind("NOPE", MidiMessage(cc(1, 1))), "no such control")
        // the engine's entries
        let e = m.entries
        XCTAssertEqual(e.count, 2)
        XCTAssertEqual([e[0].src, e[0].channel, e[0].number, e[0].target], [UInt8(RFME_SRC_NOTE), 0, 60, UInt8(RFME_T_BUTTON + RFME_B_HOME)])
        XCTAssertEqual([e[1].src, e[1].number, e[1].target, e[1].mode], [UInt8(RFME_SRC_CC), 21, UInt8(RFME_E_K1), UInt8(RFME_MAP_ABS)])
    }

    /// test.mjs's "rebinding and forgetting; learn" (its one feed: MidiEngineTests.testLearnSeedsThePot)
    @MainActor func testLearn() throws {
        let store = MidiMapStore(url: Self.tempFile())
        let L = MidiLearn(store: store)
        XCTAssertNil(L.take(cc(21, 5)), "off, nothing taken")
        L.start()
        L.select("KN1")
        guard case .bound(let b1, let f1)? = L.take(cc(21, 40)) else { return XCTFail("CC 21 binds") }
        XCTAssertTrue(f1)
        XCTAssertEqual(b1, MidiMapping(src: "cc:1:21", target: "KN1", mode: .abs))
        XCTAssertEqual(store.mappings.count, 1)
        _ = L.take(cc(21, 41)); _ = L.take(cc(21, 42)); _ = L.take(cc(21, 43))
        XCTAssertEqual(store.map.get("cc:1:21")?.mode, .abs, "a pot sweeping stays absolute")
        L.select("KN2")
        let r2 = L.take(cc(30, 65)); _ = L.take(cc(30, 65)); _ = L.take(cc(30, 66))
        XCTAssertEqual(r2, .bound(MidiMapping(src: "cc:1:30", target: "KN2", mode: .rel64), fresh: true))
        XCTAssertEqual(store.map.get("cc:1:30")?.mode, .rel64, "an encoder sending 65s is binary offset")
        L.select("KN3")
        _ = L.take(cc(21, 44))
        XCTAssertEqual(store.map.get("cc:1:21")?.target, "KN3", "binding a source already bound moves it")
        XCTAssertEqual(store.mappings.count, 2)
        XCTAssertEqual(store.mappings.last?.src, "cc:1:21", "the newest last")
        _ = L.take(cc(22, 50))
        XCTAssertEqual(store.sourcesOf("KN3").count, 2, "one target takes several sources")
        L.select("KN4")
        XCTAssertEqual(L.take(noteOn(60)), .rejected("KN4 takes a CC: move a knob or a pot"))
        XCTAssertFalse(store.map.has("note:1:60"))
        L.select("HOME")
        let rb = L.take(noteOn(60)), ro = L.take(noteOff(60)), rx = L.take(noteOff(61))
        XCTAssertEqual(rb, .bound(MidiMapping(src: "note:1:60", target: "HOME"), fresh: true))
        XCTAssertEqual(ro, MidiLearn.Result.none, "its note-off is consumed")
        XCTAssertNil(rx, "another note-off flows on")
        L.select("KN3")
        XCTAssertEqual(L.forgetSelected(), 2)
        XCTAssertFalse(store.map.has("cc:1:21"))
        XCTAssertFalse(store.map.has("cc:1:22"))
        XCTAssertTrue(store.map.has("cc:1:30"))
        // the stored form round-trips; bad records are left out
        let back = MidiMap(json: store.map.json)
        XCTAssertEqual(back.mappings.count, 2)
        XCTAssertEqual(back.get("cc:1:30")?.mode, .rel64)
        XCTAssertEqual(back, store.map)
        let bad = #"{"midiMap":[{"src":"cc:0:1","target":"KN1"},{"src":"cc:1:1","target":"NOPE"},null,{"src":"cc:1:2","target":"KN1","mode":"zz"},{"src":"note:1:3","target":"FX","mode":"rel64"}],"knobSpeed":3}"#
        let b = MidiMap(json: Data(bad.utf8))
        XCTAssertEqual(b.mappings, [MidiMapping(src: "cc:1:2", target: "KN1", mode: .abs), MidiMapping(src: "note:1:3", target: "FX")])
        XCTAssertEqual(b.speed, 1)
        // the website's prefs read as they are
        let web = #"{"midiMap":[{"src":"cc:1:21","target":"KN1","mode":"rel2c"},{"src":"note:10:36","target":"OCT-"}],"knobSpeed":0.5}"#
        let w = MidiMap(json: Data(web.utf8))
        XCTAssertEqual(w.mappings, [MidiMapping(src: "cc:1:21", target: "KN1", mode: .rel2c), MidiMapping(src: "note:10:36", target: "OCT-")])
        XCTAssertEqual(w.speed, 0.5)
    }

    /// the store: saved at once, read back by another store; FIZZ's file is RemoteFM1's stored form (adapted: FIZZ has
    /// no App Group follower, the router takes each change instead: MidiEngineTests)
    @MainActor func testStoreSavesAndReadsBack() throws {
        let url = Self.tempFile()
        let a = MidiMapStore(url: url)
        var seen: [MidiMap] = []
        a.onChange = { seen.append($0) }
        a.bind("cc:1:21", to: "KN1", mode: .rel64)
        a.setSpeed(2)
        XCTAssertEqual(seen.last, a.map, "each change goes to the router at once")
        let b = MidiMapStore(url: url)
        XCTAssertEqual(b.map, a.map)
        XCTAssertEqual(b.speed, 2)
        XCTAssertEqual(a.forget("KN1"), 1)
        b.load()
        XCTAssertTrue(b.mappings.isEmpty)
        a.setSpeed(3)
        XCTAssertEqual(a.speed, 1, "0.5, 1 or 2")
        a.bind("cc:1:9", to: "MASTER", mode: .rel64)
        XCTAssertNil(a.map.get("cc:1:9")?.mode, "a mode only on an encoder's CC")
        XCTAssertNil(a.bind("note:1:9", to: "MASTER"))
        a.clear()
        XCTAssertEqual(MidiMapStore(url: url).mappings, [])
    }

    /// a map RemoteFM1 for iOS wrote (its MidiMap.json, keys sorted) reads in FIZZ, and FIZZ writes it back byte for byte;
    /// import replaces the map, a file that is not a map changes nothing
    @MainActor func testRemoteFM1MapFileRoundTrips() throws {
        let rfm = #"{"knobSpeed":0.5,"midiMap":[{"mode":"rel64","src":"cc:1:21","target":"KN1"},{"mode":"abs","src":"cc:1:22","target":"ALGORITHM"},{"src":"cc:1:7","target":"MASTER"},{"src":"note:10:36","target":"PLAY"},{"src":"cc:2:64","target":"OCT+"}]}"#
        let m = MidiMap(json: Data(rfm.utf8))
        XCTAssertEqual(m.mappings.map(\.target), ["KN1", "ALGORITHM", "MASTER", "PLAY", "OCT+"])
        XCTAssertEqual(m.get("cc:1:21")?.mode, .rel64)
        XCTAssertEqual(m.speed, 0.5)
        XCTAssertEqual(String(decoding: m.json, as: UTF8.self), rfm, "the same file, byte for byte")
        let store = MidiMapStore(url: Self.tempFile())
        store.bind("cc:3:3", to: "KN4")
        try store.importMap(Data(rfm.utf8))
        XCTAssertEqual(store.map, m, "an import replaces the map")
        XCTAssertEqual(String(decoding: store.exported, as: UTF8.self), rfm)
        XCTAssertThrowsError(try store.importMap(Data(#"{"presets":[]}"#.utf8)))
        XCTAssertThrowsError(try store.importMap(Data("not json".utf8)))
        XCTAssertEqual(store.map, m, "nothing changed")
        // FIZZ's names for the targets on its panel (the ids stay RemoteFM1's)
        XCTAssertEqual(MidiHub.name("SEL"), "TRACKS")
        XCTAssertEqual(MidiHub.name("OCT-"), "F1")
        XCTAssertEqual(MidiHub.name("KN1"), "KN1")
        XCTAssertEqual(MidiHub.target(.tracks), "SEL")
        XCTAssertEqual(MidiHub.target(.f2), "OCT+")
        XCTAssertEqual(MidiHub.target(knob: "ALGORITHM"), "ALGORITHM")
    }

    /// FIZZ's controller input (adapted: RemoteFM1's ControllerInput is not ported): UMP to MIDI 1.0 bytes, the input
    /// picked as RemoteFM1 keeps it, and the sources never listened to
    @MainActor func testControllerInput() {
        let words: [UInt32] = [0x2090_3C64,              // note-on 60 100
                               0x10F8_0000,              // clock (system real time): passed, for FIZZ's MIDI in
                               0x3016_0102, 0x0304_0506, // SysEx (two words): not passed
                               0x20C1_0500,              // program change, channel 2: one data byte
                               0x4090_3C00, 0xC000_0000, // MIDI 2.0 note-on (two words): not passed
                               0x20B0_1540]              // CC 21 64
        XCTAssertEqual(MidiUMP.messages(words), [[0x90, 0x3C, 0x64], [0xF8], [0xC1, 0x05], [0xB0, 0x15, 0x40]])
        XCTAssertEqual(MidiInput.Selection(stored: nil), .all)
        XCTAssertEqual(MidiInput.Selection(stored: "*"), .all)
        XCTAssertEqual(MidiInput.Selection(stored: ""), MidiInput.Selection.none)
        XCTAssertEqual(MidiInput.Selection(stored: "-42"), .one(-42))
        XCTAssertEqual(MidiInput.Selection.one(7).stored, "7")
        // never the FM-1 in use, the FM-1 used as the panel, or the OMX-27 in REMOTE mode
        let keystep = MidiDevice(id: 1, name: "KeyStep 37", ref: 11), nano = MidiDevice(id: 2, name: "nanoKONTROL2", ref: 12)
        let fm1 = MidiDevice(id: 3, name: "FM-1", ref: 13), panel = MidiDevice(id: 4, name: "FM-1 Bluetooth", ref: 14)
        let omx = MidiDevice(id: 5, name: "OMX-27", ref: 15)
        var k = MidiInput.Skip()
        k.unitRef = 13
        k.panelName = "FM-1 Bluetooth"
        k.omxID = 5
        k.omxName = "OMX-27"
        let all = [keystep, fm1, nano, panel, omx]
        XCTAssertEqual(all.filter { !k.skips($0) }, [keystep, nano])
        var byName = MidiInput.Skip()
        byName.unitName = "FM-1"
        XCTAssertTrue(byName.skips(MidiDevice(id: 9, name: "FM-1")), "the unit's port by name too")
        XCTAssertFalse(MidiInput.Skip().skips(omx), "an OMX-27 not in REMOTE mode is a controller like any other")
        let input = MidiInput(demo: [keystep, nano], selection: .one(2),
                              defaults: UserDefaults(suiteName: "midi.input.\(UUID().uuidString)")!)
        XCTAssertEqual(input.listened, [nano])
        XCTAssertEqual(input.selectionName, "nanoKONTROL2")
        XCTAssertEqual(MidiInput.listened([keystep, nano], .all), [keystep, nano])
        XCTAssertEqual(MidiInput.listened([keystep, nano], .none), [])
        XCTAssertEqual(MidiInput.listened([keystep], .one(2)), [], "a device away: nothing")
    }

    static func tempFile() -> URL {
        FileManager.default.temporaryDirectory.appendingPathComponent("fizz-midimap-\(UUID().uuidString).json")
    }
}
