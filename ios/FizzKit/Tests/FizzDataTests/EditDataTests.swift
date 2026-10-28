// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// The EDIT tab's sound model (phase I5) against Node: sound.js in JavaScriptCore gives what Node gives
// (gen_fixtures.mjs "edit"), and the rules ported to Swift (text, clamp) say what sound.js says for every value.

import Foundation
import Testing
import FizzKit
@testable import FizzData

private enum EFx {
    nonisolated(unsafe) static let edit: [String: Any] = {
        let url = Bundle.module.url(forResource: "fixtures", withExtension: "json")!
        let j = try! JSONSerialization.jsonObject(with: Data(contentsOf: url)) as! [String: Any]
        return j["edit"] as! [String: Any]
    }()
    nonisolated(unsafe) static let json: [String: Any] = {
        let url = Bundle.module.url(forResource: "fixtures", withExtension: "json")!
        return try! JSONSerialization.jsonObject(with: Data(contentsOf: url)) as! [String: Any]
    }()
    static func hex(_ s: String) -> [UInt8] {
        var out: [UInt8] = [], it = s.makeIterator()
        while let a = it.next(), let b = it.next() { out.append(UInt8(String([a, b]), radix: 16)!) }
        return out
    }
    static func parse(_ s: String) -> Any { try! JSONSerialization.jsonObject(with: Data(s.utf8), options: .fragmentsAllowed) }
    static var frames: [[UInt8]] { (json["frames"] as! [String]).map(hex) }
    static var sounds: [[String: Any]] { parse(edit["sounds"] as! String) as! [[String: Any]] }
    static var libSounds: [FizzSound] {
        (json["sounds"] as! [[String: Any]]).map { s in
            let sends = s["sends"] as! [String: Int], src = s["source"] as! [String: String]
            return FizzSound(name: s["name"] as! String, blob: [UInt8](Data(base64Encoded: s["blob"] as! String)!),
                             drive: s["drive"] as! Int, pan: s["pan"] as! Int,
                             sends: FizzSends(chorus: sends["chorus"]!, delay: sends["delay"]!, reverb: sends["reverb"]!),
                             tags: s["fizzTags"] as! [String], source: ["fizz": .string(src["fizz"]!), "from": .string(src["from"]!)])
        }
    }
}

/// The fields both shapes have, as Node wrote them.
private func same(_ s: EditSound, _ n: [String: Any]) -> Bool {
    let sends = n["sends"] as! [String: Int]
    return s.name == n["name"] as? String && s.values == n["values"] as? [Int] && s.pan == n["pan"] as? Int
        && s.tags == n["tags"] as? Int && s.source == n["source"] as? String && s.slot == n["slot"] as? Int
        && s.track == n["track"] as? Int
        && s.sends == FizzSends(chorus: sends["chorus"]!, delay: sends["delay"]!, reverb: sends["reverb"]!)
}

@Suite struct EditDataTests {
    @Test func tableIsNodes() async throws {
        let e = try FizzDataEngine()
        #expect(try await e.plainJSON("editTable", []) == EFx.edit["table"] as! String)
        let t = try await e.editTable()
        #expect(t.count == 115)
        #expect(t.algos.count == 8 && t.destOrder.count == 64)
        #expect(t.id("ALGO") == 0 && t.id("DRIVE") == 73 && t.id("MOD0") == 86)
        #expect(t.info(1)?.list.count == 19 && t.info(33)?.ex[127] == "INF")
    }

    /// text() ported to Swift says what sound.js says, across every value's range (and JavaScriptCore agrees).
    @Test func textsAreNodes() async throws {
        let e = try FizzDataEngine()
        let t = try await e.editTable()
        let rows = EFx.edit["texts"] as! [[Any]]
        #expect(rows.count > 700)
        for r in rows {
            let id = r[0] as! Int, v = r[1] as! Int, want = r[2] as! String
            #expect(t.text(id, v) == want, "text(\(id), \(v))")
        }
        let js = try await e.editTexts(rows.prefix(60).map { ($0[0] as! Int, $0[1] as! Int) })
        #expect(js == rows.prefix(60).map { $0[2] as! String })
        #expect(t.clamp(1, 99) == 18 && t.clamp(4, -9999) == -2600)
        #expect(t.isDest(50) && t.isDest(87) && !t.isDest(111) && !t.isDest(51))
        #expect(t.destStep(0, 1) == t.destOrder[1] && t.destStep(t.destOrder.last!, 1) == t.destOrder.last!)
    }

    /// DUMP replies as the editor's sounds, packed back into the same blobs, and the STORE6 frames SAVE and a track's
    /// AUDITION send.
    @Test func soundsAndFramesAreNodes() async throws {
        let e = try FizzDataEngine()
        let blobs = (EFx.edit["blobs"] as! [String]).map(EFx.hex)
        let save = (EFx.edit["save"] as! [String]).map(EFx.hex), aud = (EFx.edit["audition"] as! [String]).map(EFx.hex)
        for (k, f) in EFx.frames.enumerated() {
            let s = try #require(try await e.editSound(frame: f))
            #expect(same(s, EFx.sounds[k]))
            #expect(try await e.editPack(s).blob == blobs[k])
            #expect(try await e.storeFrame(s, slot: k, own: true) == save[k])
            #expect(try await e.storeFrame(s, track: 2, audition: true, extras: FizzTrackExtrasIn(id: 0x1234, sound: 0, edited: true)) == aud[k])
        }
        let lib = EFx.parse(EFx.edit["lib"] as! String) as! [[String: Any]]
        for (k, s) in EFx.libSounds.enumerated() {
            let x = try await e.editSound(lib: s)
            #expect(same(x, lib[k]))
            #expect(try await e.libSound(x).blob == (try await e.editPack(x)).blob)
        }
    }

    /// A MIDI sound: its slots read from its DUMP, packed back, its frame.
    @Test func midiSound() async throws {
        let e = try FizzDataEngine()
        let m = EFx.edit["midi"] as! [String: String]
        let s = try #require(try await e.editSound(frame: EFx.hex(m["dump"]!)))
        #expect(s.isMidi && s.name == "MIDI TEST")
        #expect(s.midi?[0] == EditMidiSlot(cc: 74, value: 100, touched: true))
        #expect(s.midi?[5] == EditMidiSlot(cc: 7, value: 0, touched: true))
        #expect(s.midi?[27] == EditMidiSlot(cc: 127, value: 127, touched: false))
        #expect(s.midi?[1] == EditMidiSlot(cc: 17, value: 0, touched: false))
        #expect(try await e.editPack(s).blob == EFx.hex(m["blob"]!))
        #expect(try await e.storeFrame(s, own: true) == EFx.hex(m["frame"]!))
        var t = s
        t.midi?[1].touched = true
        #expect(!t.sameValues(s) && s.sameValues(s))
    }
}
