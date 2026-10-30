// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// A RemoteFM1 unit for the demo (`-demo panel`, fizzshots, mock-up 57): libremotefm1's fake unit (CRemoteFM1's
// fake.c: the device side of the protocol, its panel, its shadow and its LEDs) behind a MIDITransport, as CoreMIDI
// would carry it. On USB it also answers the update handshake as a RemoteFM1 0.2+ unit does ("FM-1_52000402"; over
// Bluetooth the updater does not listen), and its HELLO_R says version 0.4.2 (the fake says "fake"). The fake's timers run on a pump every 5 ms while it is open.

import CRemoteFM1
import FizzKit
import Foundation

public final class DemoRemoteFM1Port: MIDITransport, @unchecked Sendable {
    /// What the demo unit answers the update handshake (RemoteFM1 0.4.2).
    public static let identity = "FM-1_52000402"
    public static let version = "0.4.2"

    public let name: String
    public let messages: AsyncStream<[UInt8]>
    public var isLost: Bool { false }
    private let cont: AsyncStream<[UInt8]>.Continuation
    private let lock = NSLock()
    private let fake: OpaquePointer
    private let t0 = DispatchTime.now().uptimeNanoseconds
    private var pump: Task<Void, Never>?
    private var closed = false
    private var partial: [UInt8] = []
    private let bluetooth: Bool

    /// bluetooth: the unit heard over Bluetooth MIDI (its HELLO_R says so).
    public init(name: String, bluetooth: Bool = false) {
        self.name = name
        self.bluetooth = bluetooth
        (messages, cont) = AsyncStream.makeStream(of: [UInt8].self)
        var c = rfm_fake_cfg_t()
        rfm_fake_cfg_default(&c)
        c.ble = bluetooth ? 1 : 0
        fake = rfm_fake_new(&c)
        pump = Task.detached { [weak self] in
            while !Task.isCancelled {
                guard let self, self.tick() else { return }
                try? await Task.sleep(for: .milliseconds(5))
            }
        }
    }

    deinit {
        pump?.cancel()
        rfm_fake_free(fake)
    }

    private var now: UInt32 { UInt32(truncatingIfNeeded: (DispatchTime.now().uptimeNanoseconds - t0) / 1_000_000) }

    public func send(_ bytes: [UInt8]) throws {
        if bytes == FM1Bulk.handshakeQuery {
            guard !bluetooth else { return }
            let t = Array(Self.identity.utf8)
            cont.yield(FM1Bulk.updateFrame(cmd: 0x11, payload: t + [UInt8](repeating: 0, count: 27 - t.count)))
            return
        }
        try lock.withLock {
            if closed { throw MIDITransportError.closed }
            if bytes.first == 0xF0 { rfm_fake_rx_sx(fake, bytes, UInt32(bytes.count), now) }
        }
        tick()
    }

    public func close() {
        lock.withLock {
            guard !closed else { return }
            closed = true
            cont.finish()
        }
        pump?.cancel()
    }

    /// The unit's panel: a key, a button pressed or let go (as its hardware sends them).
    public func key(_ k: Int, down: Bool) { lock.withLock { rfm_fake_key(fake, UInt32(k), down ? 1 : 0) }; tick() }
    public func button(_ b: Int, down: Bool) { lock.withLock { rfm_fake_button(fake, UInt32(b), down ? 1 : 0) }; tick() }

    /// A host has a session on the unit (its own).
    public var inSession: Bool { lock.withLock { rfm_fake_session(fake) != 0 } }

    /// false: closed.
    @discardableResult
    private func tick() -> Bool {
        var out: [[UInt8]] = []
        let open: Bool = lock.withLock {
            guard !closed else { return false }
            rfm_fake_poll(fake, now)
            var buf = [UInt8](repeating: 0, count: 65536)
            while true {
                let n = Int(rfm_fake_take_sx(fake, &buf, UInt32(buf.count)))
                if n == 0 { break }
                for b in buf[0..<n] { split(b, into: &out) }
            }
            return true
        }
        for m in out { cont.yield(Self.versioned(m)) }
        return open
    }

    /// A HELLO_R with the demo's version in place of the fake's.
    static func versioned(_ m: [UInt8]) -> [UInt8] {
        guard m.count > 5, m[0] == 0xF0, m[4] == UInt8(RFM_HELLO_R),
              let msg = RFMMessages.parse(m).first, msg.cmd == UInt8(RFM_HELLO_R) else { return m }
        var h = rfm_hello_t()
        guard rfm_parse_hello(msg.body, UInt32(msg.body.count), &h) == 0 else { return m }
        withUnsafeMutableBytes(of: &h.ver) { b in
            b.initializeMemory(as: UInt8.self, repeating: 0)
            for (i, c) in version.utf8.prefix(8).enumerated() { b[i] = c }
        }
        var body = [UInt8](repeating: 0, count: Int(RFM_HELLO_LEN))
        let n = rfm_build_hello_r(&body, &h)
        var out = [UInt8](repeating: 0, count: Int(rfm_sx_size(n)))
        let k = rfm_sx_frame(UInt8(RFM_HELLO_R), body, n, &out)
        return Array(out[0..<Int(k)])
    }

    // the MIDI byte stream into whole messages: SysEx F0 .. F7, channel messages by their length
    private func split(_ b: UInt8, into out: inout [[UInt8]]) {
        if b >= 0xF8 { return }
        if b == 0xF0 { partial = [b]; return }
        if partial.first == 0xF0 {
            partial.append(b)
            if b == 0xF7 { out.append(partial); partial = [] }
            return
        }
        if b & 0x80 != 0 { partial = [b] } else if !partial.isEmpty { partial.append(b) }
        guard let st = partial.first else { return }
        let len = st >> 4 == 0xC || st >> 4 == 0xD ? 2 : 3
        if partial.count == len { out.append(partial); partial = [] }
    }
}
