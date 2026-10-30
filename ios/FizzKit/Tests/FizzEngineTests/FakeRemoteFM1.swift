// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// libremotefm1's fake RemoteFM1 unit (fake.c: the device side of the protocol, its panel, its shadow and its LEDs)
// behind a MIDITransport, as CoreMIDI would carry it: what PanelLink sends goes into the fake, what the fake sends
// comes out as complete messages (SysEx and the panel's channel messages). The fake's timers run on a pump every 2 ms.
// `inject` plays another host on the same port (its messages reach the fake; the replies go to every listener).

import CRemoteFM1
import FizzKit
import Foundation

final class FakeRemoteFM1: MIDITransport, @unchecked Sendable {
    let name: String
    let messages: AsyncStream<[UInt8]>
    private let cont: AsyncStream<[UInt8]>.Continuation
    private let lock = NSLock()
    private let fake: OpaquePointer
    private let t0 = DispatchTime.now().uptimeNanoseconds
    private var pump: Task<Void, Never>?
    private var closed = false, lostFlag = false
    private var partial: [UInt8] = []
    private(set) var sent: [[UInt8]] = []          // (lock) what the host sent

    init(name: String = "FM-1", configure: (inout rfm_fake_cfg_t) -> Void = { _ in }) {
        self.name = name
        (messages, cont) = AsyncStream.makeStream(of: [UInt8].self)
        var c = rfm_fake_cfg_t()
        rfm_fake_cfg_default(&c)
        configure(&c)
        fake = rfm_fake_new(&c)
        pump = Task.detached { [weak self] in
            while !Task.isCancelled {
                self?.tick()
                try? await Task.sleep(for: .milliseconds(2))
            }
        }
    }

    deinit {
        pump?.cancel()
        rfm_fake_free(fake)
    }

    var now: UInt32 { UInt32(truncatingIfNeeded: (DispatchTime.now().uptimeNanoseconds - t0) / 1_000_000) }
    var isLost: Bool { lock.withLock { lostFlag } }

    func send(_ bytes: [UInt8]) throws {
        try lock.withLock {
            if lostFlag { throw MIDITransportError.lost }
            if closed { throw MIDITransportError.closed }
            sent.append(bytes)
            rfm_fake_rx_sx(fake, bytes, UInt32(bytes.count), now)
        }
        tick()
    }

    func close() {
        lock.withLock {
            guard !closed else { return }
            closed = true
            cont.finish()
        }
    }

    /// The port unplugged.
    func unplug() {
        lock.withLock { lostFlag = true }
        cont.finish()
    }

    /// Another host's message on the same port (its reply heard by everyone on it).
    func inject(_ cmd: Int, _ body: [UInt8] = []) {
        var out = [UInt8](repeating: 0, count: Int(rfm_sx_size(UInt32(body.count))))
        let n = rfm_sx_frame(UInt8(cmd), body, UInt32(body.count), &out)
        lock.withLock { rfm_fake_rx_sx(fake, out, n, now) }
        tick()
    }

    /// The fake's own calls (its panel, faults, what it shows), under the lock.
    func with<T>(_ f: (OpaquePointer) -> T) -> T {
        let r = lock.withLock { f(fake) }
        tick()
        return r
    }

    var hostMessages: [[UInt8]] { lock.withLock { sent } }

    private func tick() {
        var out: [[UInt8]] = []
        lock.withLock {
            guard !closed && !lostFlag else { return }
            rfm_fake_poll(fake, now)
            var buf = [UInt8](repeating: 0, count: 65536)
            while true {
                let n = Int(rfm_fake_take_sx(fake, &buf, UInt32(buf.count)))
                if n == 0 { break }
                for b in buf[0..<n] { split(b, into: &out) }
            }
        }
        for m in out { cont.yield(m) }
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
