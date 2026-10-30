// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2026 RemoteFM1 contributors
// Derived from RemoteFM1IOS af91bc5 RFMKit/Tests/RFMKitTests/OmxTests.swift, unchanged but for this line and the
// module it tests (FizzUI).
// The OMX-27 in REMOTE mode (RFMKit/OMX27): the website's web/site/test.mjs OMX checks, ported (the wire, the packing
// against remote_test.py's vectors, the session's pacing on a fake clock, the views, LEDs, zones, K5's pickup), the
// UMP SysEx7 codec and the reassembly, the screen, and OmxLink against a fake OMX-27 (as the website's
// tools/omx-fake.js: no MIDI device is touched) on a fake clock. With RFM_OMX_SHOTS=<folder> it also writes pictures
// of the fake OMX's LEDs and screen per view (omx-base.png ...).

import CoreGraphics
import CoreText
import CryptoKit
import Foundation
import ImageIO
import UniformTypeIdentifiers
import XCTest
@testable import FizzUI

private func hex<C: Sequence>(_ b: C) -> String where C.Element == UInt8 { b.map { String(format: "%02x", $0) }.joined() }

// MARK: the wire

final class OmxWireTests: XCTestCase {
    typealias X = OmxWire

    func testModeCommandAndLatch() {
        XCTAssertEqual(X.modeRemote, 9)
        XCTAssertEqual(hex(X.enterRemote()), "f07d0000510509f7")
        XCTAssertEqual(hex(X.sysex(0x5B)), "f07d00005bf7")
    }

    func testParse() {
        XCTAssertEqual(X.parse(X.keyMsg(13, 3)), .key(key: 13, ev: 3))
        XCTAssertEqual(X.parse([0xF0, 0x7D, 0, 0, 0x51, 1, 0, 4, 0xF7]), .enc(delta: -4))
        XCTAssertEqual(X.parse(X.encMsg(3)), .enc(delta: 3))
        XCTAssertEqual(X.parse(X.potMsg(4, 100, 12345)), .pot(pot: 4, v: 100, hires: 12345))
        XCTAssertEqual(X.parse([0xF0, 0x7D, 0, 0, 0x51, 2, 1, 0xF7]), .encBtn(z: 1))
        XCTAssertEqual(X.parse(X.statusMsg(4)), .status(4))
        XCTAssertNil(X.parse([0xF0, 0x7E, 0, 0, 0x51, 0, 1, 1, 0xF7]))
        XCTAssertNil(X.parse([0x90, 60, 100]))
        XCTAssertNil(X.parse(X.keyMsg(27, 1)), "a key past 26 is none")
        XCTAssertNil(X.parse(X.potMsg(5, 1)), "a pot past K5 is none")
        XCTAssertEqual(X.parse([0xF0, 0x7D, 0, 0, 0x51, 1, 2, 0, 0xF7]), .enc(delta: 1), "amt 0 is 1")
    }

    func testEnc7() {
        XCTAssertEqual(hex(X.enc7([0x80, 0x7F, 0xFF, 0, 0, 0, 0, 0x81])), "05007f7f00000000" + "0101")
        XCTAssertEqual(X.enc7([UInt8](repeating: 0, count: 32)).count, 37)
    }

    /// remote_test.py's _pack and _enc7 (PIL, rotate 180) on this image gave these bytes (the website's test.mjs)
    func testPackAsRemoteTestPy() {
        var bits = [UInt8](repeating: 0, count: 128 * 32)
        for y in 0..<32 { for x in 0..<128 where (x * 7 + y * 13) % 11 < 3 || x == y || (x < 9 && y < 4) { bits[y * 128 + x] = 1 } }
        let fb = X.pack(bits)
        XCTAssertEqual(hex(SHA256.hash(data: Data(fb))), "dab809b51637adc3ea0bdc5dfe376069213bb6c3a8f3349e3141860eb666a25f")
        XCTAssertEqual(hex(fb[0..<16]), "8410430c30c208218618618410430c30")
        XCTAssertEqual(hex(fb[496...]), "10430c30c20821f6f9f3f4f8f3fcf0f2")
        XCTAssertEqual(hex(X.enc7(fb[160..<192])),
                       "240821061861041048430c30420821060418610410430c3049420821061861040010430c30")
        XCTAssertEqual(X.unpack(fb), bits, "unpack undoes it")
        XCTAssertEqual(X.dec7(X.enc7(fb[0..<32]), 32), Array(fb[0..<32]), "dec7 undoes enc7")
    }

    func testPortName() {
        XCTAssertTrue(X.isOmxPortName("OMX-27"))
        XCTAssertTrue(X.isOmxPortName("omx-27-v3 MIDI 1"))
        XCTAssertFalse(X.isOmxPortName("FM-1"))
        XCTAssertFalse(X.isOmxPortName(nil))
    }

    func testLedMessages() {
        var want = [OmxRGB](repeating: .off, count: 27)
        XCTAssertEqual(X.ledMessages(want, want), [])
        let all = X.ledMessages(want, nil)
        XCTAssertEqual(all.map { $0[4] }, [0x5A, 0x5B], "unknown: one batch of all 27, then the latch")
        XCTAssertEqual(all[0].count, 5 + 2 + 81 + 1)
        let sent = want
        want[3] = OmxRGB(1, 2, 3)
        XCTAssertEqual(X.ledMessages(want, sent).map(hex), ["f07d00005903010203f7", "f07d00005bf7"])
        for i in 0..<9 { want[i] = OmxRGB(5, 5, 5) }
        XCTAssertEqual(X.ledMessages(want, sent).first?[4], 0x5A, "past 8 changed: a batch")
    }

    func testFrameMessages() {
        var fb = [UInt8](repeating: 0, count: 512)
        let first = X.frameMessages(fb, nil)
        XCTAssertEqual(first.count, 17)
        XCTAssertEqual(first[0].count, 5 + 1 + 37 + 1)
        XCTAssertEqual(X.frameMessages(fb, fb), [])
        let last = fb
        fb[33] = 0xFF
        let one = X.frameMessages(fb, last)
        XCTAssertEqual(one.map { $0[4] }, [0x5C, 0x5D])
        XCTAssertEqual(one[0][5], 1)
    }

    /// changed LEDs only, a batch past 8, one frame in flight (ACK or 250 ms), 15 fps, only changed chunks
    func testSessionPacing() {
        var t = 0.0
        var sent: [[UInt8]] = []
        var delays: [Double] = []
        let S = OmxSession(send: { b, d in sent.append(b); delays.append(d) }, now: { t })
        func cmds() -> [UInt8] { sent.map { $0[4] } }
        S.enter()
        XCTAssertEqual(sent.map(hex), [hex(X.enterRemote())])
        XCTAssertFalse(S.tick(), "nothing more before 52 02")
        S.feed(X.statusMsg(2)); sent = []; delays = []
        S.tick()
        XCTAssertTrue(S.active)
        XCTAssertEqual(cmds().filter { $0 == 0x5A }.count, 1)
        XCTAssertEqual(cmds().filter { $0 == 0x5C }.count, 16)
        XCTAssertEqual(cmds().filter { $0 == 0x5D }.count, 1)
        XCTAssertEqual(delays.suffix(17), (0..<17).map { Double($0) * 2 }, "the chunks 2 ms apart")
        sent = []
        var leds = [OmxRGB](repeating: .off, count: 27)
        leds[5] = OmxRGB(127, 0, 0)
        S.setLeds(leds)
        var f2 = [UInt8](repeating: 0, count: 512)
        f2[100] = 1
        S.setFrame(f2)
        t = 10; S.tick()
        XCTAssertEqual(sent.count, 0, "within 30 ms nothing more goes; the frame waits for its ACK")
        t = 40; S.tick()
        XCTAssertEqual(cmds(), [89, 91], "one changed LED: 59 5 r g b, then 5B")
        S.feed(X.statusMsg(4)); t = 80; sent = []; S.tick()
        XCTAssertEqual(cmds(), [92, 93], "after the ACK, only the changed chunk (3) and 5D")
        XCTAssertEqual(sent[0][5], 3)
        sent = []; f2[0] = 9; S.setFrame(f2); t = 100; S.tick()
        XCTAssertEqual(sent.count, 0, "no ACK yet: no frame")
        t = 340; S.tick()
        XCTAssertEqual(cmds(), [92, 93], "a lost ACK: the next frame after 250 ms")
        S.feed(X.statusMsg(4)); sent = []; t = 360; S.tick()
        XCTAssertEqual(sent.count, 0, "the same frame and LEDs: nothing")
        S.clear()
        XCTAssertEqual(cmds().filter { $0 == 0x5A }.count, 1)
        XCTAssertEqual(cmds().filter { $0 == 0x5C }.count, 16)
        XCTAssertTrue(sent.first { $0[4] == 0x5A }![7..<(sent.first { $0[4] == 0x5A }!.count - 1)].allSatisfy { $0 == 0 },
                      "clear() turns every LED off and blanks the screen")
        S.feed(X.statusMsg(3)); sent = []; t = 1000; S.setFrame(f2); S.tick()
        XCTAssertFalse(S.active)
        XCTAssertEqual(sent.count, 0, "after 52 03 (REMOTE left) nothing goes")
        S.feed(X.statusMsg(2)); t = 1100; S.tick()
        XCTAssertTrue(S.active)
        XCTAssertEqual(sent.filter { $0[4] == 0x5C }.count, 16, "52 02 again: everything sent again")
    }

    func testUMPSysex7() {
        let short: [UInt8] = [0xF0, 0x7D, 0, 0, 0x52, 0x02, 0xF7]
        let w = OmxUMP.sysex7(short)
        XCTAssertEqual(w.count, 2, "5 data bytes: one complete message")
        XCTAssertEqual(w[0] >> 28, 3)
        XCTAssertEqual((w[0] >> 20) & 0xF, 0)
        XCTAssertEqual((w[0] >> 16) & 0xF, 5)
        XCTAssertEqual(OmxUMP.bytes(w), short)
        let long = OmxWire.frameMessages([UInt8](repeating: 0xA5, count: 512), nil)[0]   // 42 data bytes
        let lw = OmxUMP.sysex7(long)
        XCTAssertEqual(lw.count, 14)
        XCTAssertEqual(lw.enumerated().filter { $0.offset % 2 == 0 }.map { ($0.element >> 20) & 0xF }, [1, 2, 2, 2, 2, 2, 3])
        XCTAssertEqual(OmxUMP.bytes(lw), long)
        XCTAssertEqual(OmxUMP.bytes(OmxUMP.sysex7([0xF0, 0xF7])), [0xF0, 0xF7])
        // a channel voice message (type 2) and a utility (type 0) in between are skipped
        XCTAssertEqual(OmxUMP.bytes([0x0000_0000, 0x2090_3C64] + w), short)
    }

    func testAssemblerReassemblesSplitSysex() {
        var a = OmxSysexAssembler()
        let m = OmxWire.keyMsg(15, 1), n = OmxWire.statusMsg(2)
        XCTAssertEqual(a.feed(m[0..<3]), [])
        XCTAssertEqual(a.feed(m[3..<6] + [0xF8] + m[6...]), [m], "split, a clock byte inside")
        XCTAssertEqual(a.feed([0x90, 60, 100] + n + n), [n, n])
        XCTAssertEqual(a.feed([0xF0, 0x7D, 0x90, 60, 0xF7]), [], "a status byte inside drops it")
        XCTAssertEqual(a.feed([0xF0, 1, 0xF0] + n), [n], "a new F0 starts again")
    }

    func testPair() {
        let a = OmxPort(id: 1, name: "OMX-27"), b = OmxPort(id: 2, name: "OMX-27"), c = OmxPort(id: 3, name: "FM-1")
        XCTAssertNil(omxPair(inputs: [c], outputs: [c]))
        XCTAssertNil(omxPair(inputs: [], outputs: [b]))
        XCTAssertEqual(omxPair(inputs: [c, a], outputs: [c, b]), OmxPair(input: a, output: b))
        let d = OmxPort(id: 4, name: "omx other")
        XCTAssertEqual(omxPair(inputs: [d, a], outputs: [b])?.input, a, "the input of the same name first")
        XCTAssertEqual(omxPair(inputs: [d], outputs: [b])?.input, d, "else the first OMX input")
    }
}

// MARK: the views

final class OmxViewsTests: XCTestCase {
    typealias V = OmxViews
    let D = 1, U = 0, Q = 3

    func down(_ v: V, _ k: Int, _ t: Double = 0) -> [OmxAction] { v.key(k, D, t) }
    func up(_ v: V, _ k: Int, _ t: Double = 0) -> [OmxAction] { v.key(k, U, t) }
    @discardableResult func click(_ v: V, _ k: Int, _ t: Double) -> [OmxAction] { v.key(k, D, t) + v.key(k, U, t + 40) + v.key(k, Q, t + 40) }

    func testFM1Keys() {
        XCTAssertEqual(V.fm1Whites.count, 16)
        XCTAssertEqual(V.fm1Sharps.count, 11)
        XCTAssertEqual(V.fm1Sharps.map { 53 + $0 }, [54, 56, 58, 61, 63, 66, 68, 70, 73, 75, 78])
        XCTAssertEqual(OmxPanel.noteName(0), "F3")
        XCTAssertEqual(OmxPanel.noteName(26), "G5")
        XCTAssertEqual(OmxPanel.noteName(V.fm1Sharps[10]), "F#5")
    }

    func testPanelOrdersByName() {
        // the website's panel.js E and B orders are PanelFeed.Knob's and PanelFeed.Label's
        let E = ["SELECT": 0, "ALGO": 1, "PRESETS": 2, "K1": 3, "K2": 4, "K3": 5, "K4": 6, "MASTER": 7]
        let K: [String: OmxKnob] = ["SELECT": .select, "ALGO": .algorithm, "PRESETS": .presets, "K1": .k1, "K2": .k2,
                                    "K3": .k3, "K4": .k4, "MASTER": .master]
        for (n, r) in E { XCTAssertEqual(K[n]!.rawValue, r, n) }
        let B = ["FX", "SEL", "ENV", "LFO", "EDIT", "GLO", "HOME", "SAVE", "ARP", "SEQ", "PLAY", "REC", "OCT-", "OCT+"]
        for l in OmxLabel.allCases { XCTAssertEqual(l.legend.replacingOccurrences(of: "−", with: "-"), B[l.rawValue]) }
        XCTAssertEqual(OmxPanel.labels, B)
    }

    func testViews() {
        let v = V()
        XCTAssertEqual(v.view, .base); XCTAssertEqual(v.knob, .select); XCTAssertEqual(v.range, 0)
        var a = down(v, 15)
        XCTAssertEqual(a, [.key(0, down: true)], "Base R1: key 15 (F4) plays the FM-1's first F (key 0, F3)")
        XCTAssertEqual(up(v, 15), [.key(0, down: false)], "Base: its release lets it go")
        XCTAssertEqual(down(v, 3), [.key(1, down: true)])
        XCTAssertEqual(up(v, 3), [.key(1, down: false)])
        XCTAssertEqual(down(v, 26), [.key(19, down: true)], "Base R1: key 26 (C6) plays C5")
        _ = up(v, 26)
        _ = down(v, 12); _ = up(v, 12)
        XCTAssertEqual(v.range, 1)
        XCTAssertEqual(down(v, 15), [.key(12, down: true)], "Base R2: key 15 plays the second F (key 12)")
        XCTAssertEqual(down(v, 23), [.key(26, down: true)], "Base R2: key 23 (G5) the FM-1's top G5")
        _ = up(v, 15); _ = up(v, 23)
        XCTAssertEqual(down(v, 24), []); XCTAssertEqual(down(v, 9), []); XCTAssertNil(v.roleOf(26), "Base R2: past the FM-1's")
        _ = up(v, 24); _ = up(v, 9)
        XCTAssertEqual(down(v, 1), [.button(.octDown, down: true)])
        XCTAssertEqual(up(v, 1), [.button(.octDown, down: false)])
        XCTAssertEqual(down(v, 2), [.button(.octUp, down: true)], "Base: keys 1 and 2 hold OCT- and OCT+")
        _ = up(v, 2)
        XCTAssertEqual(v.button, .play, "Base: OCT- and OCT+ pressed are not selected")
        // the knob quick keys: held, KN2 with key 7, the whites still play
        _ = down(v, 13)
        XCTAssertEqual(v.overlay(), .knob); XCTAssertEqual(v.screen().title, "KNOBS")
        a = down(v, 7)
        XCTAssertEqual(a, [.select(.k2)]); XCTAssertEqual(v.knob, .k2, "knob quick keys: key 7 selects KN2")
        _ = up(v, 7)
        XCTAssertEqual(v.enc(3), [.turn(.k2, steps: 3)])
        XCTAssertEqual(v.enc(-2), [.turn(.k2, steps: -2)], "the encoder turns the selected knob, its amt as detents")
        XCTAssertEqual(v.enc(0), [])
        if case .key = down(v, 20).first {} else { XCTFail("knob quick keys: the whites still play") }
        XCTAssertEqual(down(v, 10), [], "key 10 does nothing")
        _ = up(v, 20); _ = up(v, 10); _ = up(v, 13)
        XCTAssertNil(v.overlay(), "knob quick keys: let go, they close")
        // double click latches, a click closes
        click(v, 13, 1000)
        XCTAssertNil(v.overlay(), "a single click on 13 does not latch")
        click(v, 13, 3000); click(v, 13, 3200)
        XCTAssertEqual(v.overlay(), .knob); XCTAssertTrue(v.knobLatched, "a double click on 13 latches")
        XCTAssertEqual(down(v, 3), [.select(.select)], "latched: key 3 selects SEL")
        _ = up(v, 3)
        click(v, 13, 5000)
        XCTAssertNil(v.overlay(), "latched: a single click closes them")
        // the button quick keys
        _ = down(v, 14)
        XCTAssertEqual(v.overlay(), .btn); XCTAssertEqual(v.screen().title, "BUTTONS")
        XCTAssertEqual(down(v, 25), [.button(.play, down: true)], "button quick keys: key 25 presses PLAY")
        XCTAssertEqual(up(v, 25), [.button(.play, down: false)])
        XCTAssertEqual(down(v, 21), [.button(.home, down: true)]); XCTAssertEqual(v.button, .home)
        XCTAssertEqual(down(v, 5), [.button(.fx, down: true)]); XCTAssertEqual(v.button, .fx)
        _ = up(v, 21); _ = up(v, 5)
        XCTAssertEqual(down(v, 16), []); XCTAssertEqual(down(v, 3), [], "the white keys (16) and keys 3, 4 do not play")
        _ = up(v, 16); _ = up(v, 3)
        _ = down(v, 1); _ = up(v, 1)
        XCTAssertEqual(v.button, .octDown, "button quick keys: OCT- is selectable here")
        _ = up(v, 14)
        XCTAssertEqual(down(v, 0), [.button(.octDown, down: true)])
        XCTAssertEqual(up(v, 0), [.button(.octDown, down: false)], "AUX presses the selected button (OCT-) while held")
        // Seq: the 1:1 map, AUX toggled by K4
        _ = v.pot(0, 40)
        XCTAssertEqual(v.view, .seq); XCTAssertEqual(v.screen().title, "SEQ")
        var seqKeys: [Int] = []
        for k in 1...26 { if case .key(let f, _) = down(v, k).first { seqKeys.append(f) }; _ = up(v, k) }
        XCTAssertEqual(Array(seqKeys.prefix(10)), Array(V.fm1Sharps.prefix(10)))
        XCTAssertEqual(Array(seqKeys.dropFirst(10)), V.fm1Whites, "Seq: blacks 1..10 the first 10 sharps, whites 11..26 the whites")
        XCTAssertEqual(down(v, 0), [.button(.octDown, down: true)], "Seq: AUX is the button quick key at first"); _ = up(v, 0)
        _ = v.pot(3, 100)
        XCTAssertTrue(v.auxBlack)
        XCTAssertEqual(down(v, 0), [.key(V.fm1Sharps[10], down: true)])
        XCTAssertTrue(v.screen().line.contains("AUX F#5"), "Seq: K4 up, AUX is the 11th sharp (F#5)")
        _ = up(v, 0); _ = v.pot(3, 10)
        XCTAssertFalse(v.auxBlack, "Seq: K4 down, the button quick key again")
        // Keyboard: notes, channel K3, octave K4
        _ = v.pot(0, 70)
        XCTAssertEqual(v.view, .keys)
        _ = v.pot(2, 20); _ = v.pot(3, 95)
        XCTAssertEqual(down(v, 12), [.midi([0x92, 60 + 24, 100])])
        XCTAssertTrue(v.screen().line.hasPrefix("CH 3  OCT +2"), "Keyboard: channel 3, octave +2: key 12 sends C6 on ch 3")
        _ = v.pot(3, 64)
        XCTAssertEqual(up(v, 12), [.midi([0x82, 84, 0])], "the note-off is the note that went down, after an octave change")
        XCTAssertEqual(down(v, 0), [.button(.octDown, down: true)], "Keyboard: AUX is the button quick key"); _ = up(v, 0)
        // Quick Keys
        _ = v.pot(0, 120)
        XCTAssertEqual(v.view, .quick); XCTAssertEqual(v.screen().title, "QUICK KEYS")
        XCTAssertEqual(down(v, 9), [.select(.k4)]); XCTAssertEqual(v.knob, .k4); _ = up(v, 9)
        XCTAssertEqual(down(v, 23), [.button(.play, down: true)]); XCTAssertEqual(v.button, .play)
        XCTAssertEqual(up(v, 23), [.button(.play, down: false)], "Quick Keys: key 23 selects PLAY and presses it")
        XCTAssertEqual(down(v, 11), [.button(.fx, down: true)])
        XCTAssertEqual(down(v, 19), [.button(.home, down: true)])
        XCTAssertEqual(down(v, 17), [], "Quick Keys: 11 FX, 19 HOME, 17 nothing")
        let rel = v.releaseAll()
        XCTAssertEqual(rel, [.button(.fx, down: false), .button(.home, down: false)])
        XCTAssertEqual(v.heldCount, 0, "releaseAll lets every held key and button go")
    }

    func testSharedHolds() {
        // two OMX keys on one FM-1 key (Seq AUX as F#5 and key 10 is not it; Base R1 and R2 on the same key): one press
        let v = V()
        XCTAssertEqual(down(v, 26), [.key(19, down: true)])     // C6 -> C5 in R1
        _ = down(v, 12)                                           // R2
        XCTAssertEqual(down(v, 19), [], "key 19 (C5) in R2 is the FM-1's C5 too: already down")
        XCTAssertEqual(up(v, 26), [])
        XCTAssertEqual(up(v, 19), [.key(19, down: false)], "up with the last")
        XCTAssertEqual(up(v, 19), [], "an up without a down: nothing")
        XCTAssertEqual(v.key(27, D), [])
    }

    func testZones() {
        XCTAssertEqual(V.zoneOf(0, 4, nil), 0); XCTAssertEqual(V.zoneOf(31, 4, nil), 0)
        XCTAssertEqual(V.zoneOf(32, 4, nil), 1); XCTAssertEqual(V.zoneOf(127, 4, nil), 3)
        XCTAssertEqual(V.zoneOf(33, 4, 0), 0); XCTAssertEqual(V.zoneOf(35, 4, 0), 1)
        XCTAssertEqual(V.zoneOf(30, 4, 1), 1); XCTAssertEqual(V.zoneOf(28, 4, 1), 0, "a zone changes only 3 past its edge")
        let v = V()
        XCTAssertEqual(v.pot(0, 33), [])
        XCTAssertEqual(v.pot(0, 36), [.view(.seq)])
        XCTAssertEqual(v.pot(0, 30), [], "K1: no view flicker around an edge")
        let t = v.pot(1, 127, 0, 1000)
        XCTAssertEqual(t, [.theme(5)])
        XCTAssertEqual(v.screen(1500).line, "THEME NIGHT")
        XCTAssertNotEqual(v.screen(3000).line, "THEME NIGHT")
        XCTAssertEqual(V.themes.count, 6)
        XCTAssertEqual(V(saved: v.saved()).theme, 5)
        XCTAssertEqual(V(saved: OmxViewPrefs(theme: 99, knob: 42)).theme, 0, "bad values ignored")
        XCTAssertEqual(V(saved: OmxViewPrefs(theme: 99, knob: 42)).knob, .select)
        XCTAssertEqual(V(saved: OmxViewPrefs(knob: OmxKnob.master.rawValue)).knob, .select, "MASTER is no encoder knob")
        let p = OmxViewPrefs(theme: 2, knob: OmxKnob.k3.rawValue, button: OmxLabel.rec.rawValue, auxBlack: true, ch: 9, oct: -2)
        XCTAssertEqual(V(saved: p).saved(), p, "every choice kept")
        XCTAssertEqual(V(saved: OmxViewPrefs(ch: 17, oct: 4)).saved().ch, 1)
        XCTAssertEqual(V(saved: OmxViewPrefs(ch: 17, oct: 4)).saved().oct, 0)
    }

    /// Range 2: the playing keys past the FM-1's G5 lit dark red; Range 1 has none
    func testDeadKeys() {
        let v = V()
        func red(_ l: [OmxRGB], _ k: Int) -> Bool { l[k].r > 0 && l[k].g == 0 && l[k].b == 0 }
        func dead(_ l: [OmxRGB]) -> [Int] { (1..<27).filter { red(l, $0) } }
        XCTAssertEqual(dead(v.leds()), [], "Range 1: no dead keys")
        v.range = 1
        let l = v.leds()
        XCTAssertEqual(dead(l), [9, 10, 24, 25, 26])
        XCTAssertLessThan(l[24].r, 64, "dimmer than a shortcut")
        _ = v.key(14, 1, 0)
        XCTAssertFalse(red(v.leds(), 24), "the button quick keys cover the dead keys")
    }

    /// shortcuts at 50% and full when pressed or selected; mirrored keys dark unless the FM-1 lights them
    func testLeds() {
        let v = V()
        var lv = [UInt8](repeating: 0, count: 27)
        func L() -> [OmxRGB] { v.leds { lv[$0] } }
        var l = L()
        XCTAssertEqual(l[0].description, "64,0,0"); XCTAssertEqual(l[1].description, "0,64,64")
        XCTAssertEqual(l[11].description, "64,127,0"); XCTAssertEqual(l[12].description, "64,32,0")
        XCTAssertEqual(l[15], .off); XCTAssertEqual(l[3], .off)
        lv[0] = 255; lv[1] = 40
        l = L()
        XCTAssertEqual(l[15].description, "127,127,127")
        XCTAssertTrue(l[3].ints.allSatisfy { $0 > 0 && $0 < 60 } && l[3].r == l[3].b, "the level sets the brightness")
        v.theme = 1; l = L()
        XCTAssertEqual(l[15].ints, V.themes[1].key.map { min(127, jsRound(Double($0) / 2)) }, "another theme, another hue")
        v.theme = 0
        _ = v.key(0, 1); l = L()
        XCTAssertEqual(l[0].description, "127,0,0", "AUX full while pressed"); _ = v.key(0, 0)
        _ = v.key(13, 1); l = L()
        XCTAssertEqual(l[13].description, "127,0,64"); XCTAssertEqual(l[3].description, "0,0,127")
        XCTAssertEqual(l[4].description, "0,0,64"); XCTAssertEqual(l[6].description, "64,0,0")
        XCTAssertEqual(l[15].description, "127,127,127", "knob quick keys: the toggle full, SEL full, the others 50%; F mirrors")
        _ = v.key(13, 0); _ = v.key(14, 1); l = L()
        XCTAssertEqual(l[25].description, "0,127,64"); XCTAssertEqual(l[21].description, "64,32,0")
        XCTAssertEqual(l[15], .off); XCTAssertEqual(l[3], .off, "button quick keys: PLAY full, HOME 50%; the whites dark")
        _ = v.key(14, 0)
        _ = v.pot(0, 70); _ = v.key(12, 1); l = L()
        XCTAssertEqual(l[12].description, "127,127,127"); XCTAssertEqual(l[13], .off, "Keyboard: a held key lights")
        _ = v.key(12, 0)
        _ = v.pot(0, 40); lv = lv.map { _ in 0 }; lv[V.fm1Whites[3]] = 255; lv[V.fm1Sharps[0]] = 96; l = L()
        XCTAssertEqual(l[14].description, "127,127,127"); XCTAssertEqual(l[1].description, "79,79,79")
        XCTAssertEqual(l.filter(\.lit).count, 3, "Seq mirrors each FM-1 key's LED (and AUX the button quick key)")
        // Night: dimmer shortcuts
        v.theme = 5
        XCTAssertEqual(v.leds()[0].description, "32,0,0")
    }

    /// K5: MASTER with pickup (MasterPot, as a learned MASTER pot)
    func testMasterPickup() {
        let v = V()
        var a = v.pot(4, 10, 10 << 7, 0, 768)
        guard case .pickup(_, let at, let first) = a[0] else { return XCTFail("a pickup") }
        XCTAssertTrue(first); XCTAssertEqual(at, 768)
        XCTAssertNotNil(v.screen(10).line.range(of: #"^K5 \d+% TO 75%$"#, options: .regularExpression), v.screen(10).line)
        a = v.pot(4, 50, 50 << 7, 0, 768)
        XCTAssertEqual(a, [.pickup(pot: jsRound(Double(50 << 7) * 1023 / 16383), at: 768, first: false)], "still waiting")
        a = v.pot(4, 100, 100 << 7, 0, 768)
        XCTAssertEqual(a, [.master(jsRound(12800 * 1023 / 16383))], "crossing MASTER picks it up")
        guard case .master(let m) = a[0] else { return XCTFail() }
        a = v.pot(4, 64, 64 << 7, 0, m)
        guard case .master(let m2) = a[0] else { return XCTFail("K5 then follows") }
        XCTAssertLessThanOrEqual(abs(m2 - 512), 1)
        XCTAssertEqual(v.screen(10).line, "MASTER 50%")
        a = v.pot(4, 66, 66 << 7, 0, 300)
        if case .pickup = a[0] {} else { XCTFail("MASTER moved by something else: pickup again") }
        XCTAssertEqual(MasterPot().feed(767, 1023, 768), [.master(767)], "a pot already at MASTER takes it at once")
        var s = PickupState()
        XCTAssertFalse(pickedUp(&s, 10, 50, 2))
        XCTAssertTrue(pickedUp(&s, 60, 50, 2), "crossed")
        XCTAssertTrue(pickedUp(&s, 0, 50, 2), "stays picked")
    }

    func testLabels() {
        let v = V()
        let l = v.labels()
        XCTAssertEqual(l[0], "AUX"); XCTAssertEqual(l[1], "OCT-"); XCTAssertEqual(l[11], "R1"); XCTAssertEqual(l[13], "KNOBS")
        XCTAssertEqual(l[14], "BTNS"); XCTAssertEqual(l[15], "F3")
        _ = v.pot(0, 70)
        XCTAssertEqual(v.labels()[12], "C4")
    }
}

// MARK: the screen

final class OmxScreenTests: XCTestCase {
    func testScreen() {
        RFMFonts.register()
        XCTAssertTrue(RFMFonts.available("ChakraPetch-Bold"))
        let v = OmxViews()
        let s = v.screen()
        XCTAssertEqual(s, OmxViews.Screen(title: "FM-1", line: "R1  SEL  AUX PLAY", view: .base))
        let bits = OmxScreen.bits(s)
        XCTAssertGreaterThan(bits.reduce(0) { $0 + Int($1) }, 100, "the screen shows something")
        func on(_ x: Int, _ y: Int) -> Bool { bits[y * 128 + x] != 0 }
        XCTAssertTrue(on(104, 2) && on(107, 7), "view 0: a block")
        XCTAssertTrue(on(110, 7) && !on(110, 2) && !on(110, 6), "the others: a line")
        // the title in the top part, the line in the bottom one, both from x 1
        let rows = (0..<32).map { y in (0..<100).contains { on($0, y) } }
        XCTAssertTrue(rows[4...17].contains(true)); XCTAssertTrue(rows[22...30].contains(true))
        XCTAssertFalse((0..<128).contains { on($0, 31) }, "nothing below the line's baseline but descenders")
        XCTAssertEqual(OmxWire.unpack(OmxScreen.render(s)), bits)
        // a long title shrinks to fit
        let long = OmxScreen.bits(.init(title: "QUICK KEYS", line: "KN4  AUX PLAY", view: .quick))
        func lit(_ b: [UInt8], _ x: Int, _ y: Int) -> Bool { b[y * 128 + x] != 0 }
        XCTAssertFalse((0..<18).contains { y in (102..<104).contains { lit(long, $0, y) } }, "the title stays left of the marks")
        XCTAssertFalse((8..<18).contains { y in (104..<128).contains { lit(long, $0, y) } })
        XCTAssertTrue((0..<18).contains { y in (60..<101).contains { lit(long, $0, y) } }, "and still reaches across")
    }
}

// MARK: the link, against a fake OMX-27

/// the clock: timers fire as time is moved on
@MainActor final class FakeClock: OmxClock {
    var now = 0.0
    final class T: OmxTimer {
        var at: Double
        let every: Double?
        let f: @MainActor () -> Void
        var dead = false
        let seq: Int
        init(at: Double, every: Double?, seq: Int, f: @escaping @MainActor () -> Void) { self.at = at; self.every = every; self.seq = seq; self.f = f }
        func cancel() { dead = true }
    }
    private var timers: [T] = []
    private var seq = 0
    func after(_ ms: Double, _ f: @escaping @MainActor () -> Void) -> any OmxTimer { add(ms, nil, f) }
    func every(_ ms: Double, _ f: @escaping @MainActor () -> Void) -> any OmxTimer { add(ms, ms, f) }
    private func add(_ ms: Double, _ every: Double?, _ f: @escaping @MainActor () -> Void) -> T {
        seq += 1
        let t = T(at: now + max(0, ms), every: every, seq: seq, f: f)
        timers.append(t)
        return t
    }
    func advance(_ ms: Double) {
        let end = now + ms
        while true {
            timers.removeAll { $0.dead }
            guard let t = timers.filter({ $0.at <= end }).min(by: { ($0.at, $0.seq) < ($1.at, $1.seq) }) else { break }
            now = t.at
            if let e = t.every { t.at += e } else { t.dead = true }
            t.f()
        }
        now = end
    }
}

/// a fake OMX-27 behind a fake transport (the website's tools/omx-fake.js: REMOTE mode as the firmware speaks it)
@MainActor final class FakeOmx: OmxTransport {
    let clock: FakeClock
    var connected = true
    var remote = false
    var leds = [OmxRGB](repeating: .off, count: 27)
    private var ledStage = [OmxRGB](repeating: .off, count: 27)
    var frame = [UInt8](repeating: 0, count: 512)
    private var stage = [UInt8](repeating: 0, count: 512)
    var frames = 0, acks = 0, modes = 0
    var got: [[UInt8]] = []
    var pots = [0, 0, 0, 0, 0]
    var onSetup: (@MainActor () -> Void)?
    var onMessage: (@MainActor ([UInt8]) -> Void)?
    var started = false, opened = false
    let inPort = OmxPort(id: 271, name: "OMX-27", ref: 1), outPort = OmxPort(id: 272, name: "OMX-27", ref: 2)
    let other = OmxPort(id: 9, name: "Some keyboard", ref: 3)

    init(clock: FakeClock) { self.clock = clock }

    func start(onSetupChange: @escaping @MainActor () -> Void) -> String? { onSetup = onSetupChange; started = true; return nil }
    func stop() { onSetup = nil; started = false; close() }
    func ports() -> (inputs: [OmxPort], outputs: [OmxPort]) { connected ? ([other, inPort], [other, outPort]) : ([other], [other]) }
    func open(input: OmxPort, output: OmxPort, onMessage: @escaping @MainActor ([UInt8]) -> Void) {
        self.onMessage = onMessage
        opened = true
    }
    func close() { onMessage = nil; opened = false }
    func send(_ bytes: [UInt8], delayMs: Double) {
        guard connected, opened else { return }
        if delayMs > 0 { _ = clock.after(delayMs) { [weak self] in self?.receive(bytes) } } else { receive(bytes) }
    }

    /// to the host, after ms
    func out(_ d: [UInt8], _ ms: Double = 0) {
        _ = clock.after(ms) { [weak self] in
            guard let self, self.connected, let m = self.onMessage else { return }
            // (split in two, as a byte stream may be: the transport hands over whole messages, so whole here)
            m(d)
        }
    }

    private func sx(_ b: UInt8...) -> [UInt8] { [0xF0, 0x7D, 0, 0] + b + [0xF7] }

    /// the host's messages, as omx_mode_remote.cpp takes them
    func receive(_ d: [UInt8]) {
        got.append(d)
        guard d.count >= 6, d[0] == 0xF0, d[1] == 0x7D, d[2] == 0, d[3] == 0 else { return }
        let cmd = d[4], p = Array(d[5..<(d.count - 1)])
        if cmd == 0x51 && p.first == 0x05 {
            if p.count > 1 && p[1] == 9 {
                let was = remote
                if was { out(sx(0x52, 0x03)) }
                remote = true
                out(sx(0x52, 0x02), was ? 3 : 0)
                modes += 1
            } else if remote { remote = false; out(sx(0x52, 0x03)) }
            return
        }
        guard remote else { return }
        if cmd == 0x59 && p.count >= 4 && p[0] < 27 { ledStage[Int(p[0])] = OmxRGB(Int(p[1]), Int(p[2]), Int(p[3])) }
        else if cmd == 0x5A && p.count >= 2 {
            var i = 0
            while i < Int(p[1]) && Int(p[0]) + i < 27 {
                ledStage[Int(p[0]) + i] = OmxRGB(Int(p[2 + 3 * i]), Int(p[3 + 3 * i]), Int(p[4 + 3 * i]))
                i += 1
            }
        } else if cmd == 0x5B { leds = ledStage }
        else if cmd == 0x5C && p.count >= 1 && p[0] < 16 {
            var s = 1, i = 0
            while i < 32 {
                let n = min(7, 32 - i), hi = p[s]
                s += 1
                for j in 0..<n { stage[Int(p[0]) * 32 + i + j] = (p[s] & 0x7F) | (((hi >> UInt8(j)) & 1) << 7); s += 1 }
                i += 7
            }
        } else if cmd == 0x5D {
            frame = stage
            frames += 1
            _ = clock.after(3) { [weak self] in self?.acks += 1; self?.out([0xF0, 0x7D, 0, 0, 0x52, 0x04, 0xF7]) }
        }
    }

    // the OMX's own events
    func key(_ k: Int, _ ev: Int) { out(OmxWire.keyMsg(k, ev)) }
    func press(_ k: Int) { key(k, 1) }
    func release(_ k: Int) { key(k, 0); key(k, 3) }
    func enc(_ d: Int) { out(OmxWire.encMsg(d)) }
    func pot(_ p: Int, _ v: Int) { pots[p] = v; out(OmxWire.potMsg(p, v)) }
    func leave() { remote = false; out(sx(0x52, 0x03)) }
    func pickRemote() { remote = true; out(sx(0x52, 0x02)) }
    func unplug() { connected = false; remote = false; onSetup?() }
    func plug() { connected = true; onSetup?() }
    var screenBits: [UInt8] { OmxWire.unpack(frame) }
}

/// the panel, recorded
@MainActor final class FakeTarget: OmxTarget {
    var keys = Set<Int>(), buttons = Set<OmxLabel>(), turns: [OmxKnob: Int] = [:], selected: OmxKnob?
    var masterNow = 768, masterSets = 0, pickups: [(Int, Int, Bool)] = [], midi: [[UInt8]] = [], releases = 0
    var levels = [UInt8](repeating: 0, count: 27)
    weak var link: OmxLink?
    func omxKey(_ k: Int, down: Bool) { if down { keys.insert(k) } else { keys.remove(k) } }
    func omxButton(_ l: OmxLabel, down: Bool) { if down { buttons.insert(l) } else { buttons.remove(l) } }
    func omxTurn(_ k: OmxKnob, _ detents: Int) { turns[k, default: 0] += detents; link?.knobUsed(k) }
    func omxSelect(_ k: OmxKnob) { selected = k }
    func omxMaster(_ v: Int) { masterNow = v; masterSets += 1 }
    func omxPickup(pot: Int, at: Int, first: Bool) { pickups.append((pot, at, first)) }
    func omxFirmwareMIDI(_ bytes: [UInt8]) { midi.append(bytes) }
    var omxMasterNow: Int { masterNow }
    func omxKeyLevel(_ k: Int) -> UInt8 { levels[k] }
    func omxReleaseAll() { releases += 1; keys.removeAll(); buttons.removeAll() }
}

@MainActor final class OmxLinkTests: XCTestCase {
    var clock: FakeClock!
    var omx: FakeOmx!
    var target: FakeTarget!
    var link: OmxLink!
    var defaults: UserDefaults!
    var suite = ""
    var toasts: [(String, Bool)] = []
    var actives: [Bool] = []

    override func setUp() async throws {
        suite = "rfm1.omx.tests.\(UUID().uuidString)"
        defaults = UserDefaults(suiteName: suite)
        clock = FakeClock()
        omx = FakeOmx(clock: clock)
        target = FakeTarget()
        make()
    }

    override func tearDown() async throws { defaults.removePersistentDomain(forName: suite) }

    private func make() {
        link = OmxLink(target: target, transport: omx, clock: clock, defaults: defaults)
        target.link = link
        toasts = []; actives = []
        link.onToast = { [unowned self] t, ok in toasts.append((t, ok)) }
        link.onActive = { [unowned self] a in actives.append(a) }
    }

    private func pass(_ ms: Double = 120) { clock.advance(ms) }

    private func connectLive() {
        link.connect()
        pass(300)
        XCTAssertTrue(link.active)
    }

    func testHandshakeLedsAndScreen() {
        XCTAssertEqual(link.detail, "Not connected.")
        XCTAssertTrue(omx.got.isEmpty, "before Connect, nothing goes to the OMX-27")
        link.connect()
        XCTAssertEqual(omx.got.first.map(hex), "f07d0000510509f7", "Connect sends the MODE command (51 05 09)")
        XCTAssertEqual(link.tone, .busy); XCTAssertEqual(link.detail, "Asking OMX-27 for remote mode...")
        XCTAssertTrue(defaults.bool(forKey: "rfm1.omxAuto"))
        pass(300)
        XCTAssertTrue(link.active); XCTAssertEqual(link.tone, .on)
        XCTAssertEqual(link.detail, "OMX-27 in remote mode: Base view. To leave remote mode on the OMX-27, hold AUX and the encoder.")
        XCTAssertEqual(toasts.map(\.0), ["OMX-27 in remote mode: its keys, knobs and screen play the FM-1."])
        XCTAssertEqual(toasts.first?.1, true)
        XCTAssertEqual(actives, [true])
        XCTAssertEqual(link.buttonTitle, "Disconnect")
        XCTAssertGreaterThan(omx.frames, 0)
        XCTAssertEqual(omx.acks, omx.frames)
        XCTAssertTrue(omx.leds[11].lit && omx.leds[0].r > 0 && omx.leds[0].r < 100, "Base: Range 1 lit, AUX at half")
        XCTAssertGreaterThan(omx.screenBits.reduce(0) { $0 + Int($1) }, 100, "the screen shows something")
        XCTAssertEqual(omx.screenBits, OmxScreen.bits(link.views.screen(clock.now)))
        XCTAssertTrue(link.owns(id: 271)); XCTAssertTrue(link.owns(id: 0, name: "OMX-27")); XCTAssertFalse(link.owns(id: 9))
    }

    func testNoAnswer() {
        omx.connected = true
        // an OMX without REMOTE mode: it ignores the MODE command
        let quiet = QuietOmx(clock: clock)
        link = OmxLink(target: target, transport: quiet, clock: clock, defaults: defaults)
        link.connect()
        pass(2400)
        XCTAssertEqual(link.tone, .busy)
        pass(200)
        XCTAssertEqual(link.tone, .bad)
        XCTAssertEqual(link.detail, "OMX-27 did not answer: it needs the OMX-27 firmware with REMOTE mode. Connect asks again.")
        XCTAssertEqual(link.buttonTitle, "Try again")
    }

    func testNoOmxThenPluggedIn() {
        omx.connected = false
        link.connect()
        XCTAssertEqual(link.detail, "Plug in your OMX-27: it connects when it appears.")
        XCTAssertTrue(omx.got.isEmpty)
        omx.plug()
        pass(300)
        XCTAssertTrue(link.active, "plugged in: remote mode by itself")
    }

    func testKeysEncoderAndButtons() {
        connectLive()
        omx.press(15); pass()
        XCTAssertEqual(target.keys, [0], "Base: OMX key 15 holds the FM-1's first F (key 0) in Range 1")
        omx.release(15); pass()
        XCTAssertEqual(target.keys, [], "...and lets it go")
        omx.press(12); omx.release(12); omx.press(23); pass()
        XCTAssertEqual(target.keys, [26], "Range 2: key 23 holds the FM-1's top key (G5)")
        omx.release(23); omx.press(11); omx.release(11); pass()
        omx.press(13); pass()
        omx.press(7); pass()
        omx.release(7); omx.release(13); pass()
        XCTAssertEqual(target.selected, .k2)
        omx.enc(1); omx.enc(2); pass()
        XCTAssertEqual(target.turns, [.k2: 3], "13 + 7 picks KN2; the encoder turns it 3 detents")
        omx.press(14); pass()
        omx.press(25); pass()
        XCTAssertEqual(target.buttons, [.play], "14 held: key 25 holds PLAY")
        omx.release(25); pass()
        XCTAssertEqual(target.buttons, [])
        omx.release(14); omx.press(0); pass()
        XCTAssertEqual(target.buttons, [.play], "AUX then presses the selected button (PLAY)")
        omx.release(0); pass()
        // the panel turns a knob: the encoder turns that one next
        link.knobUsed(.k4)
        omx.enc(-1); pass()
        XCTAssertEqual(target.turns[.k4], -1)
        // the prefs kept
        let saved = defaults.data(forKey: "rfm1.omx").flatMap { try? JSONDecoder().decode(OmxViewPrefs.self, from: $0) }
        XCTAssertEqual(saved?.button, OmxLabel.play.rawValue)
        XCTAssertEqual(saved?.knob, OmxKnob.k2.rawValue, "kept on select (knobUsed alone is not saved, as the website)")
    }

    func testSeqMirrorsTheFM1sLeds() {
        connectLive()
        omx.pot(0, 40); pass(250)
        XCTAssertEqual(link.views.view, .seq)
        XCTAssertEqual(link.detail, "OMX-27 in remote mode: Seq view. To leave remote mode on the OMX-27, hold AUX and the encoder.")
        for (k, v) in [(0, 255), (1, 255), (12, 96), (22, 40), (25, 255), (26, 255)] { target.levels[k] = UInt8(v) }
        pass(200)
        let l = omx.leds
        XCTAssertTrue(l[11].lit && l[1].lit && l[18].lit && l[26].lit && l[10].lit && !l[12].lit && !l[2].lit,
                      "Seq: OMX 11 (F3), 1 (F#3), 18 (F4), 10 (D#5), 26 (G5) light with their FM-1 keys, the rest dark")
        XCTAssertTrue(l[11].r > l[18].r && l[18].r > 0, "the FM-1's level sets the brightness")
    }

    func testKeyboardToFirmware() {
        connectLive()
        omx.pot(0, 70); pass(200)
        omx.press(12); pass(); omx.release(12); pass()
        XCTAssertEqual(target.midi, [[144, 60, 100], [128, 60, 0]], "Keyboard: key 12 sends C4 on and off to the firmware")
        XCTAssertEqual(target.keys, [], "...and holds no panel key")
    }

    func testPotsCoalescedAndMasterPickup() {
        connectLive()
        let m0 = target.masterNow
        omx.pot(4, 10); pass()
        XCTAssertEqual(target.masterNow, m0, "K5 far from MASTER: MASTER stays")
        XCTAssertEqual(target.pickups.count, 1)
        XCTAssertEqual(target.pickups.first?.2, true)
        let at = jsRound(Double(m0) * 127 / 1023)
        omx.pot(4, at - 5); pass(); omx.pot(4, at + 1); pass()
        omx.pot(4, 64); pass()
        XCTAssertEqual(target.masterNow, jsRound(Double(64 << 7) * 1023 / 16383), "K5 picks MASTER up, then sets it")
        XCTAssertEqual(link.views.screen(clock.now).line, "MASTER 50%")
        // a stream: 20 messages within one tick: one application
        let sets = target.masterSets
        for v in 65...84 { omx.pot(4, v) }
        clock.advance(0)          // (delivered)
        clock.advance(10)         // (one tick)
        XCTAssertEqual(target.masterSets - sets, 1, "the latest pot value per 10 ms tick")
        XCTAssertEqual(target.masterNow, jsRound(Double(84 << 7) * 1023 / 16383))
    }

    func testModeAgainStaysConnected() {
        connectLive()
        toasts = []
        link.connect(); pass(300)
        XCTAssertTrue(link.active); XCTAssertEqual(link.tone, .on)
        XCTAssertEqual(toasts.count, 0, "MODE again: 52 03 + 52 02 is still connected (no toast)")
        XCTAssertGreaterThanOrEqual(omx.modes, 2)
        XCTAssertEqual(actives, [true])
    }

    func testLeftRemoteModeReleases() {
        connectLive()
        omx.press(15); omx.press(0); pass()
        XCTAssertEqual(target.keys, [0]); XCTAssertEqual(target.buttons, [.play])
        toasts = []
        omx.leave(); pass(30)
        XCTAssertTrue(link.active, "within 50 ms: not yet")
        pass(100)
        XCTAssertFalse(link.active)
        XCTAssertEqual(target.keys, []); XCTAssertEqual(target.buttons, [], "release on leave")
        XCTAssertGreaterThanOrEqual(target.releases, 1)
        XCTAssertEqual(link.detail, "OMX-27 left remote mode: pick REMOTE on it again, or Connect.")
        XCTAssertEqual(toasts.map(\.0), ["OMX-27 left remote mode."])
        XCTAssertEqual(actives, [true, false])
        XCTAssertFalse(link.owns(id: 271), "out of REMOTE mode: its port is the MIDI input's again")
        let frames = omx.frames
        omx.pickRemote(); pass(400)
        XCTAssertTrue(link.active); XCTAssertGreaterThan(omx.frames, frames, "52 02 again: the screen drawn again")
    }

    func testUnpluggedAndBack() {
        connectLive()
        omx.press(15); pass()
        omx.unplug(); pass(300)
        XCTAssertFalse(link.active)
        XCTAssertEqual(link.detail, "OMX-27 unplugged: it connects again when it is back.")
        XCTAssertEqual(target.keys, [])
        omx.plug(); pass(600)
        XCTAssertTrue(link.active, "plugged in again: remote mode by itself")
        omx.press(15); pass()
        XCTAssertEqual(target.keys, [0], "...and its keys play again")
    }

    func testDisconnectClearsAndAuto() {
        connectLive()
        omx.press(15); pass()
        link.disconnect(); pass(400)
        XCTAssertTrue(omx.leds.allSatisfy { $0 == .off } && omx.frame.allSatisfy { $0 == 0 }, "Disconnect: every LED off, the screen blank")
        XCTAssertEqual(link.detail, "Disconnected. The OMX-27 stays in remote mode: to leave remote mode on the OMX-27, hold AUX and the encoder.")
        XCTAssertEqual(link.tone, .off); XCTAssertFalse(link.want); XCTAssertEqual(target.keys, [])
        XCTAssertFalse(defaults.bool(forKey: "rfm1.omxAuto")); XCTAssertFalse(omx.started)
        XCTAssertEqual(link.buttonTitle, "Connect my OMX-27")
        link.auto(); pass(300)
        XCTAssertFalse(link.active, "not connected last time: auto does nothing")
        link.connect(); pass(300)
        // a new start (the OMX still in REMOTE mode: 52 03 + 52 02): connected by itself
        link.willClose()
        make()
        link.auto(); pass(300)
        XCTAssertTrue(link.active, "connected last time: auto connects")
    }

    func testPictures() throws {
        guard let dir = ProcessInfo.processInfo.environment["RFM_OMX_SHOTS"] else { throw XCTSkip("RFM_OMX_SHOTS not set") }
        let url = URL(fileURLWithPath: dir, isDirectory: true)
        try FileManager.default.createDirectory(at: url, withIntermediateDirectories: true)
        connectLive()
        func shot(_ name: String, _ title: String) throws {
            pass(250)
            try OmxPicture.png(omx, title: title, labels: link.views.labels())
                .write(to: url.appendingPathComponent("\(name).png"))
        }
        try shot("omx-base", "Base view (FM-1)")
        omx.press(13); try shot("omx-knobs", "Base view, knob quick keys (key 13 held)")
        omx.release(13); omx.press(14); try shot("omx-buttons", "Base view, button quick keys (key 14 held)")
        omx.release(14); omx.pot(0, 40)
        for (k, v) in [(0, 255), (1, 255), (12, 96), (22, 40), (25, 255), (26, 255)] { target.levels[k] = UInt8(v) }
        try shot("omx-seq", "Seq view (FM-1 F3, F#3, F4, D#5, F#5, G5 lit)")
        target.levels = target.levels.map { _ in 0 }
        omx.pot(0, 70); omx.press(12); try shot("omx-keyboard", "Keyboard view (key 12, C4, held)")
        omx.release(12); omx.pot(0, 120); try shot("omx-quick", "Quick Keys view")
        omx.pot(0, 0); pass(); omx.pot(1, 127); try shot("omx-night", "Base view, Night theme (K2 up)")
    }
}

/// an OMX-27 without REMOTE mode: it takes the MODE command and says nothing
@MainActor final class QuietOmx: OmxTransport {
    let clock: FakeClock
    init(clock: FakeClock) { self.clock = clock }
    func start(onSetupChange: @escaping @MainActor () -> Void) -> String? { nil }
    func stop() {}
    func ports() -> (inputs: [OmxPort], outputs: [OmxPort]) { ([OmxPort(id: 1, name: "OMX-27")], [OmxPort(id: 2, name: "OMX-27")]) }
    func open(input: OmxPort, output: OmxPort, onMessage: @escaping @MainActor ([UInt8]) -> Void) {}
    func close() {}
    func send(_ bytes: [UInt8], delayMs: Double) {}
}

/// the fake OMX as a picture (the website's omx-fake.js picture): its latched LEDs and screen, each key named
@MainActor enum OmxPicture {
    static func png(_ omx: FakeOmx, title: String, labels: [String]) throws -> Data {
        RFMFonts.register()
        let KW = 60.0, X0 = 30.0, Wd = Int(X0 * 2 + 16 * KW), Hd = 470
        let g = CGContext(data: nil, width: Wd, height: Hd, bitsPerComponent: 8, bytesPerRow: 0,
                          space: CGColorSpace(name: CGColorSpace.sRGB)!, bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue)!
        g.translateBy(x: 0, y: CGFloat(Hd)); g.scaleBy(x: 1, y: -1)   // (y down, as the canvas)
        func rgb(_ r: Int, _ gg: Int, _ b: Int) -> CGColor { CGColor(srgbRed: CGFloat(r) / 255, green: CGFloat(gg) / 255, blue: CGFloat(b) / 255, alpha: 1) }
        func text(_ s: String, _ x: Double, _ y: Double, _ size: Double, _ c: CGColor, center: Bool = false) {
            let font = CTFontCreateWithName("ChakraPetch-Bold" as CFString, size, nil)
            let a = NSAttributedString(string: s, attributes: [NSAttributedString.Key(kCTFontAttributeName as String): font,
                                                                NSAttributedString.Key(kCTForegroundColorAttributeName as String): c])
            let line = CTLineCreateWithAttributedString(a)
            let w = CTLineGetTypographicBounds(line, nil, nil, nil)
            g.saveGState()
            g.textMatrix = CGAffineTransform(scaleX: 1, y: -1)
            g.textPosition = CGPoint(x: center ? x - w / 2 : x, y: y)
            CTLineDraw(line, g)
            g.restoreGState()
        }
        g.setFillColor(rgb(0x16, 0x18, 0x1b)); g.fill(CGRect(x: 0, y: 0, width: Wd, height: Hd))
        g.setFillColor(rgb(0x24, 0x27, 0x2c))
        g.addPath(CGPath(roundedRect: CGRect(x: 12, y: 40, width: Double(Wd) - 24, height: Double(Hd) - 52), cornerWidth: 18, cornerHeight: 18, transform: nil)); g.fillPath()
        text(title, 16, 28, 18, rgb(0xe8, 0xea, 0xed))
        // the OLED, x3
        g.setFillColor(rgb(0, 0, 0)); g.fill(CGRect(x: X0, y: 60, width: 128 * 3 + 12, height: 32 * 3 + 12))
        let bits = omx.screenBits
        g.setFillColor(rgb(0xcf, 0xe8, 0xff))
        for y in 0..<32 { for x in 0..<128 where bits[y * 128 + x] != 0 { g.fill(CGRect(x: X0 + 6 + Double(x) * 3, y: 66 + Double(y) * 3, width: 3, height: 3)) } }
        // the encoder and the pots
        func knob(_ cx: Double, _ cy: Double, _ r: Double, _ label: String, _ v: Int?) {
            g.setFillColor(rgb(0x3a, 0x3e, 0x45)); g.fillEllipse(in: CGRect(x: cx - r, y: cy - r, width: 2 * r, height: 2 * r))
            if let v {
                let a = (Double(v) / 127 - 0.5) * 1.5 * Double.pi - Double.pi / 2
                g.setStrokeColor(rgb(0xe8, 0xea, 0xed)); g.setLineWidth(3)
                g.move(to: CGPoint(x: cx, y: cy)); g.addLine(to: CGPoint(x: cx + cos(a) * r * 0.8, y: cy + sin(a) * r * 0.8)); g.strokePath()
            }
            text(label, cx, cy + r + 15, 12, rgb(0x9a, 0xa0, 0xa8), center: true)
        }
        knob(X0 + 128 * 3 + 60, 112, 26, "ENC", nil)
        for i in 0..<5 { knob(X0 + 128 * 3 + 150 + Double(i) * 92, 112, 30, "K\(i + 1)", omx.pots[i]) }
        func cap(_ k: Int, _ x: Double, _ y: Double, _ w: Double, _ h: Double) {
            let c = omx.leds[k], v = [Int(c.r), Int(c.g), Int(c.b)].map { min(255, $0 * 2) }, lit = c.lit
            let path = CGPath(roundedRect: CGRect(x: x, y: y, width: w, height: h), cornerWidth: 8, cornerHeight: 8, transform: nil)
            g.saveGState()
            if lit { g.setShadow(offset: .zero, blur: 22, color: rgb(v[0], v[1], v[2])) }
            g.setFillColor(lit ? rgb(v[0], v[1], v[2]) : rgb(0x33, 0x37, 0x3d)); g.addPath(path); g.fillPath()
            g.restoreGState()
            g.setStrokeColor(CGColor(gray: 0, alpha: 0.6)); g.setLineWidth(2); g.addPath(path); g.strokePath()
            let lum = 0.3 * Double(v[0]) + 0.59 * Double(v[1]) + 0.11 * Double(v[2])
            text(labels[k], x + w / 2, y + h / 2 + 4, 11, lum > 140 ? rgb(0x11, 0x11, 0x11) : rgb(0xe8, 0xea, 0xed), center: true)
            text(String(k), x + w / 2, y + h - 6, 9, lum > 140 ? CGColor(gray: 0, alpha: 0.55) : CGColor(gray: 1, alpha: 0.4), center: true)
        }
        let WY = 330.0, BY = 230.0, S = 52.0
        for i in 0..<16 { cap(11 + i, X0 + Double(i) * KW + 4, WY, S, S + 40) }
        let blackAfter = [1, 2, 4, 5, 6, 8, 9, 11, 12, 13]   // the white key each black one follows (B3 = 0)
        for (j, wi) in blackAfter.enumerated() { cap(1 + j, X0 + Double(wi + 1) * KW - S / 2 + 2, BY, S, S + 30) }
        cap(0, X0 + 4, BY, S, S + 30)
        let img = g.makeImage()!
        let out = NSMutableData()
        let dst = CGImageDestinationCreateWithData(out, UTType.png.identifier as CFString, 1, nil)!
        CGImageDestinationAddImage(dst, img, nil)
        CGImageDestinationFinalize(dst)
        return out as Data
    }
}
