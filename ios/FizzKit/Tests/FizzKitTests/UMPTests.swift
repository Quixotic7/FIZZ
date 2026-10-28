// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.

import Testing
@testable import FizzKit

@Suite("UMP SysEx7 and port pairing")
struct UMPTests {
    private func message(_ n: Int, seed: Int) -> [UInt8] {
        [0xf0] + (0..<n).map { UInt8(($0 * 37 + seed * 11 + 5) & 0x7f) } + [0xf7]
    }

    @Test func packetShapes() {
        #expect(UMP.sysEx7Words([0xf0, 0xf7]) == [0x3000_0000, 0])
        // 01 02 03: complete, 3 bytes
        #expect(UMP.sysEx7Words([0xf0, 1, 2, 3, 0xf7]) == [0x3003_0102, 0x0300_0000])
        // 7 bytes: start (6) then end (1)
        let w = UMP.sysEx7Words([0xf0, 1, 2, 3, 4, 5, 6, 7, 0xf7], group: 2)
        #expect(w == [0x3216_0102, 0x0304_0506, 0x3231_0700, 0])
        // 13 bytes: start, continue, end
        let s = UMP.sysEx7Words(message(13, seed: 1)).enumerated().filter { $0.offset % 2 == 0 }.map { ($0.element >> 20) & 0xf }
        #expect(s == [1, 2, 3])
    }

    @Test func roundTrips() {
        for n in 0...200 {
            let m = message(n, seed: n)
            var r = SysEx7Reassembler()
            #expect(r.feed(UMP.sysEx7Words(m)) == [m], "length \(n)")
        }
    }

    @Test func splitAcrossEventLists() {
        let a = message(40, seed: 3), b = message(5, seed: 4), c = message(186, seed: 5)
        let words = UMP.sysEx7Words(a) + UMP.sysEx7Words(b) + UMP.sysEx7Words(c)
        for chunk in [2, 4, 6, 64] {
            var r = SysEx7Reassembler()
            var got: [[UInt8]] = []
            var i = 0
            while i < words.count {
                got += r.feed(words[i..<min(i + chunk, words.count)])
                i += chunk
            }
            #expect(got == [a, b, c], "chunks of \(chunk) words")
        }
    }

    @Test func otherMessagesSkipped() {
        let m = message(20, seed: 9)
        let w = UMP.sysEx7Words(m)
        let noteOn: UInt32 = 0x2090_3c64            // MIDI 1.0 channel voice (1 word)
        let midi2: [UInt32] = [0x4090_3c00, 0xffff_0000]   // MIDI 2.0 channel voice (2 words)
        let data128: [UInt32] = [0x5000_0000, 1, 2, 3]
        var r = SysEx7Reassembler()
        let got = r.feed([noteOn] + Array(w[0..<2]) + midi2 + Array(w[2..<4]) + data128 + Array(w[4...]))
        #expect(got == [m])
    }

    @Test func orphansDropped() {
        let m = message(20, seed: 2)
        let w = UMP.sysEx7Words(m)
        var r = SysEx7Reassembler()
        #expect(r.feed(w[2...]).isEmpty)            // continue / end without a start
        #expect(r.feed(w) == [m])
        // a new start abandons an unfinished one
        #expect(r.feed(Array(w[0..<4]) + w).count == 1)
    }

    @Test func shortMessages() {
        #expect(MIDIPorts.shortMessageWord([0x90, 60, 100]) == 0x2090_3c64)
        #expect(MIDIPorts.shortMessageWord([0xc1, 5]) == 0x20c1_0500)
        #expect(MIDIPorts.shortMessageWord([0xf0, 1]) == nil)
    }

    @Test func candidateNames() {
        for n in ["FM-1", "FIZZ FM-1", "ChoralRoot FM-1", "fm1", "USB-MIDI", "Felucca", "OTA loader", "Composite Device",
                  "SINCO"] {
            #expect(MIDIPorts.isCandidateName(n), "\(n)")
        }
        for n in ["IAC Driver Bus 1", "Network Session 1", "Digitone", ""] { #expect(!MIDIPorts.isCandidateName(n), "\(n)") }
    }

    @Test func pairing() {
        let s1 = MIDIEndpointInfo(ref: 1, name: "FM-1", entity: 10, device: 100)
        let s2 = MIDIEndpointInfo(ref: 2, name: "FM-1 Port 2", entity: 11, device: 100)
        let s3 = MIDIEndpointInfo(ref: 3, name: "IAC Bus", entity: 20, device: 200)
        let s4 = MIDIEndpointInfo(ref: 4, name: "FIZZ FM-1", entity: 0, device: 0)
        let s5 = MIDIEndpointInfo(ref: 5, name: "FM-1 gone", entity: 12, device: 101, offline: true)
        let d1 = MIDIEndpointInfo(ref: 51, name: "FM-1 out", entity: 10, device: 100)
        let d2 = MIDIEndpointInfo(ref: 52, name: "FM-1 Port 2", entity: 0, device: 0)
        let d4 = MIDIEndpointInfo(ref: 54, name: "FIZZ FM-1", entity: 0, device: 0)
        let p = MIDIPorts.pair(sources: [s1, s2, s3, s4, s5], destinations: [d1, d2, d4])
        #expect(p.map(\.source.ref) == [1, 2, 4])
        #expect(p.map(\.destination.ref) == [51, 52, 54])        // entity, then name, then name
        // the only destination, whatever its name
        let only = MIDIPorts.pair(sources: [MIDIEndpointInfo(ref: 7, name: "USB-MIDI")],
                                  destinations: [MIDIEndpointInfo(ref: 8, name: "Something")])
        #expect(only.map(\.destination.ref) == [8])
        #expect(MIDIPorts.pair(sources: [MIDIEndpointInfo(ref: 7, name: "USB-MIDI")], destinations: [d1, d2]).isEmpty)
    }
}
