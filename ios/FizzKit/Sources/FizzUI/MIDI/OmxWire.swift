// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2026 RemoteFM1 contributors
// Derived from RemoteFM1IOS af91bc5 RFMKit/Sources/RFMKit/OMX27/OmxWire.swift, unchanged but for this line.
// The OMX-27's REMOTE mode, the wire: the website's js/omx27.js, ported as is (OMX-27 firmware q7-2026-3:
// SYSEX_SPEC.md "REMOTE mode", omx_mode_remote.cpp, tools/remote_test.py).
//   every message    F0 7D 00 00 <cmd> <payload> F7
//   host -> OMX      51 05 <MODE_REMOTE> enter REMOTE mode (the input-injection MODE command)
//                    59 idx r g b / 5A start count rgb... stage LEDs (7-bit), 5B latch them
//                    5C chunk enc7(32 bytes) stage a screen chunk (SSD1306 pages, 180 deg), 5D latch the frame
//   OMX -> host      51 00 key ev (0 up 1 down 2 hold 3 quick click, after the up), 51 01 dir amt (0 CCW 2 CW),
//                    51 02 z (encoder button), 51 03 pot v7 hi lo; 52 02 entered, 52 03 left, 52 04 frame ACK
// OmxSession keeps the OMX's LEDs and screen as the app wants them: only what changed, LEDs at most every 30 ms,
// one frame in flight (its ACK, or 250 ms), at most 15 frames a second.

import Foundation

public enum OmxWire {
    public enum CMD {
        public static let input: UInt8 = 0x51, status: UInt8 = 0x52, led: UInt8 = 0x59, ledBatch: UInt8 = 0x5A
        public static let ledShow: UInt8 = 0x5B, draw: UInt8 = 0x5C, drawUpd: UInt8 = 0x5D
    }
    public enum STATUS {
        public static let activity = 0x01, on = 0x02, off = 0x03, ack = 0x04
    }
    /// config.h OMXMode: MIDI DRUM CHORDS FORM S1 S2 GRIDS EUCLID OM REMOTE CONFIG
    public static let modeRemote: UInt8 = 9
    public enum EV {
        public static let up = 0, down = 1, hold = 2, quick = 3
    }
    public static let nLED = 27, W = 128, H = 32, fbBytes = 512, chunk = 32, nChunk = 16

    /// the port: its name says OMX
    public static func isOmxPortName(_ name: String?) -> Bool { (name ?? "").range(of: "omx", options: .caseInsensitive) != nil }

    public static func sysex(_ cmd: UInt8, _ data: [Int] = []) -> [UInt8] {
        [0xF0, 0x7D, 0x00, 0x00, cmd] + data.map { UInt8($0 & 0x7F) } + [0xF7]
    }
    public static func enterRemote() -> [UInt8] { sysex(CMD.input, [0x05, Int(modeRemote)]) }

    /// 8-bit bytes -> 7-bit groups: a byte of high bits, then up to 7 low parts (remote_test.py _enc7)
    public static func enc7<C: Collection>(_ data: C) -> [UInt8] where C.Element == UInt8 {
        let d = Array(data)
        var out: [UInt8] = []
        out.reserveCapacity(d.count + (d.count + 6) / 7)
        var i = 0
        while i < d.count {
            let n = min(7, d.count - i)
            var hi: UInt8 = 0
            for j in 0..<n { hi |= ((d[i + j] >> 7) & 1) << UInt8(j) }
            out.append(hi)
            for j in 0..<n { out.append(d[i + j] & 0x7F) }
            i += 7
        }
        return out
    }

    public static func dec7(_ src: [UInt8], _ len: Int) -> [UInt8] {
        var out = [UInt8](repeating: 0, count: len)
        var s = 0
        func at(_ i: Int) -> UInt8 { i < src.count ? src[i] : 0 }
        var i = 0
        while i < len {
            let n = min(7, len - i), hi = at(s)
            s += 1
            for j in 0..<n {
                out[i + j] = (at(s) & 0x7F) | (((hi >> UInt8(j)) & 1) << 7)
                s += 1
            }
            i += 7
        }
        return out
    }

    /// a 128 x 32 image (bits[y * 128 + x], 1 lit) -> the SSD1306 buffer, 180 deg (remote_test.py _pack)
    public static func pack(_ bits: [UInt8]) -> [UInt8] {
        var fb = [UInt8](repeating: 0, count: fbBytes)
        for y in 0..<H {
            for x in 0..<W where bits[(H - 1 - y) * W + (W - 1 - x)] != 0 {
                fb[x + (y >> 3) * W] |= 1 << UInt8(y & 7)
            }
        }
        return fb
    }

    public static func unpack(_ fb: [UInt8]) -> [UInt8] {
        var bits = [UInt8](repeating: 0, count: W * H)
        for y in 0..<H {
            for x in 0..<W where fb[x + (y >> 3) * W] & (1 << UInt8(y & 7)) != 0 {
                bits[(H - 1 - y) * W + (W - 1 - x)] = 1
            }
        }
        return bits
    }

    /// a message from the OMX
    public enum Message: Equatable, Sendable {
        case key(key: Int, ev: Int)
        case enc(delta: Int)
        case encBtn(z: Int)
        case pot(pot: Int, v: Int, hires: Int)
        case status(Int)
    }

    /// a message from the OMX, decoded; anything else nil
    public static func parse(_ d: [UInt8]) -> Message? {
        guard d.count >= 7, d[0] == 0xF0, d[1] == 0x7D, d[2] == 0, d[3] == 0, d[d.count - 1] == 0xF7 else { return nil }
        let cmd = d[4], a = d[5..<(d.count - 1)].map { Int($0) }
        if cmd == CMD.status { return .status(a[0]) }
        guard cmd == CMD.input else { return nil }
        let sub = a[0]
        if sub == 0x00 && a.count >= 3 { return a[1] < nLED ? .key(key: a[1], ev: a[2]) : nil }
        if sub == 0x01 && a.count >= 3 { let amt = max(1, a[2]); return .enc(delta: a[1] == 2 ? amt : -amt) }
        if sub == 0x02 && a.count >= 2 { return .encBtn(z: a[1] != 0 ? 1 : 0) }
        if sub == 0x03 && a.count >= 5 { return a[1] < 5 ? .pot(pot: a[1], v: a[2], hires: (a[3] << 7) | a[4]) : nil }
        return nil
    }

    // the OMX's side (the fake OMX-27 in the tests speaks it)
    public static func keyMsg(_ k: Int, _ ev: Int) -> [UInt8] { sysex(CMD.input, [0x00, k, ev]) }
    public static func encMsg(_ delta: Int) -> [UInt8] { sysex(CMD.input, [0x01, delta < 0 ? 0 : 2, min(127, abs(delta))]) }
    public static func potMsg(_ p: Int, _ v: Int, _ hires: Int? = nil) -> [UInt8] {
        let h = hires ?? (v << 7)
        return sysex(CMD.input, [0x03, p, v, (h >> 7) & 0x7F, h & 0x7F])
    }
    public static func statusMsg(_ s: Int) -> [UInt8] { sysex(CMD.status, [s]) }

    /// the LEDs as they should be (27 colours, 0..127) against what the OMX has (nil: unknown) -> the messages
    public static func ledMessages(_ want: [OmxRGB], _ sent: [OmxRGB]?) -> [[UInt8]] {
        var changed: [Int] = []
        for i in 0..<nLED where sent == nil || want[i] != sent![i] { changed.append(i) }
        if changed.isEmpty { return [] }
        var out: [[UInt8]]
        if changed.count > 8 {
            out = [sysex(CMD.ledBatch, [0, nLED] + want.flatMap(\.ints))]
        } else {
            out = changed.map { sysex(CMD.led, [$0] + want[$0].ints) }
        }
        out.append(sysex(CMD.ledShow))
        return out
    }

    /// a frame (512 bytes) against the last one sent (nil: none) -> its changed chunks and the latch, or [] when the same
    public static func frameMessages(_ fb: [UInt8], _ last: [UInt8]?) -> [[UInt8]] {
        var out: [[UInt8]] = []
        for c in 0..<nChunk {
            let a = fb[(c * chunk)..<((c + 1) * chunk)]
            if let last, a.elementsEqual(last[(c * chunk)..<((c + 1) * chunk)]) { continue }
            out.append(sysex(CMD.draw, [c] + enc7(a).map { Int($0) }))
        }
        if !out.isEmpty { out.append(sysex(CMD.drawUpd)) }
        return out
    }

    public static let ledMs = 30.0, frameMs = 1000.0 / 15, ackMs = 250.0, chunkGapMs = 2.0
}

/// an OMX-27 LED colour (7-bit on the wire)
public struct OmxRGB: Equatable, Hashable, Sendable, CustomStringConvertible {
    public var r: UInt8, g: UInt8, b: UInt8
    public init(_ r: Int, _ g: Int, _ b: Int) {
        self.r = UInt8(clamping: r)
        self.g = UInt8(clamping: g)
        self.b = UInt8(clamping: b)
    }
    public static let off = OmxRGB(0, 0, 0)
    public var ints: [Int] { [Int(r), Int(g), Int(b)] }
    public var lit: Bool { r > 0 || g > 0 || b > 0 }
    /// "r,g,b" (as the website's tests write it)
    public var description: String { "\(r),\(g),\(b)" }
}

/// the OMX's LEDs and screen as the app wants them, sent paced (js/omx27.js OmxSession)
public final class OmxSession {
    /// to the OMX's output (delayMs: when, from now; the screen's chunks are paced)
    public let send: ([UInt8], Double) -> Void
    public let now: () -> Double
    /// in REMOTE mode (52 02 seen, no 52 03 since)
    public private(set) var active = false
    public private(set) var wantLeds = [OmxRGB](repeating: .off, count: OmxWire.nLED)
    private var sentLeds: [OmxRGB]?
    private var ledsAt = -1e9
    public private(set) var wantFrame = [UInt8](repeating: 0, count: OmxWire.fbBytes)
    private var lastFrame: [UInt8]?
    private var frameAt = -1e9
    public private(set) var pending = false
    public struct Stats: Equatable, Sendable { public var frames = 0, acks = 0, chunks = 0, ledMsgs = 0 }
    public private(set) var stats = Stats()

    /// now: milliseconds, any origin
    public init(send: @escaping ([UInt8], Double) -> Void, now: @escaping () -> Double) {
        self.send = send
        self.now = now
    }

    /// the MODE command; what the OMX shows is unknown until it says it is in REMOTE mode
    public func enter() { send(OmxWire.enterRemote(), 0) }

    /// a message from the OMX: its event (nil: none); the status ones change the session
    @discardableResult
    public func feed(_ d: [UInt8]) -> OmxWire.Message? {
        guard let m = OmxWire.parse(d) else { return nil }
        if case .status(let s) = m {
            if s == OmxWire.STATUS.ack { pending = false; stats.acks += 1 }
            else if s == OmxWire.STATUS.on { active = true; reset() }
            else if s == OmxWire.STATUS.off { active = false; pending = false }
        }
        return m
    }

    public func reset() {
        sentLeds = nil
        lastFrame = nil
        pending = false
        ledsAt = -1e9
        frameAt = -1e9
    }

    public func setLeds(_ colors: [OmxRGB]) { wantLeds = colors.map { OmxRGB(Int($0.r & 127), Int($0.g & 127), Int($0.b & 127)) } }
    public func setFrame(_ fb: [UInt8]) { wantFrame = fb }

    /// sends what is due; true when something went
    @discardableResult
    public func tick() -> Bool {
        guard active else { return false }
        let t = now()
        var sent = false
        if t - ledsAt >= OmxWire.ledMs {
            let ms = OmxWire.ledMessages(wantLeds, sentLeds)
            if !ms.isEmpty {
                for m in ms { send(m, 0) }
                sentLeds = wantLeds
                ledsAt = t
                stats.ledMsgs += ms.count
                sent = true
            }
        }
        if pending && t - frameAt >= OmxWire.ackMs { pending = false }   // (a lost ACK: no wedge)
        if !pending && t - frameAt >= OmxWire.frameMs {
            let ms = OmxWire.frameMessages(wantFrame, lastFrame)
            if !ms.isEmpty {
                for (i, m) in ms.enumerated() { send(m, Double(i) * OmxWire.chunkGapMs) }
                lastFrame = wantFrame
                frameAt = t
                pending = true
                stats.frames += 1
                stats.chunks += ms.count - 1
                sent = true
            }
        }
        return sent
    }

    /// every LED off and a blank screen, now (Disconnect, the app closing); the OMX stays in REMOTE mode
    public func clear() {
        let dark = [OmxRGB](repeating: .off, count: OmxWire.nLED), blank = [UInt8](repeating: 0, count: OmxWire.fbBytes)
        for m in OmxWire.ledMessages(dark, nil) { send(m, 0) }
        for (i, m) in OmxWire.frameMessages(blank, nil).enumerated() { send(m, Double(i) * OmxWire.chunkGapMs) }
        wantLeds = dark
        wantFrame = blank
        reset()
    }
}
