// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// Telling a RemoteFM1 unit (an FM-1 running RemoteFM1's firmware: a screen, LEDs and controls for a host) from the
// FM-1s FIZZ talks to, without disturbing it. Two ways, per RemoteFM1's protocol v2 (its docs/PROTOCOL.md):
//   - passively: 0.2 and later answer the update handshake, which the app's scan sends every FM-1 port anyway, as
//     themselves, "FM-1_52XXYYZZ" (section 6; the updater's path works at any time, in a session or not);
//   - with the protocol's own HELLO (F0 7D 52 46 01 .. F7), which a host may send to any FM-1 port: the unit answers
//     HELLO_R (name "RemoteFM1", proto 2, its version, whether a host has a session) and changes nothing. Over
//     Bluetooth, where the updater does not listen, HELLO is the only way. FIZZ's own SysEx (7D 46 5A) and plain MIDI
//     are ignored by RemoteFM1, and RemoteFM1's messages are ignored by FIZZ and the updater.

import CRemoteFM1
import FizzKit
import Foundation

/// What a RemoteFM1 unit says about itself (HELLO_R).
public struct RemoteFM1Hello: Sendable, Equatable {
    /// "RemoteFM1"
    public let name: String
    public let proto: Int
    /// the firmware's version ("0.2.3"), and RemoteFM1's own version string
    public let version: String, fwVersion: String
    /// the update identity's first 8 characters ("FM-1_520")
    public let identity: String
    /// RFM_CAP_* (0x1AF from 0.2.2)
    public let caps: Int
    /// a host has a session on it now (this one or another)
    public let inSession: Bool
    /// the reply came over Bluetooth MIDI
    public let bluetooth: Bool
    public let screenWidth: Int, screenHeight: Int

    /// The unit FIZZ drives: RemoteFM1, protocol 2, with the v2 fields.
    public let isPanel: Bool

    init(_ h: rfm_hello_t) {
        var h = h
        func str<T>(_ t: inout T) -> String {
            withUnsafeBytes(of: &t) { b in String(decoding: b.prefix { $0 != 0 }, as: UTF8.self) }
        }
        name = str(&h.name)
        proto = Int(h.proto)
        version = str(&h.ver)
        fwVersion = str(&h.fw_ver)
        identity = str(&h.identity)
        caps = Int(h.caps)
        inSession = h.session != 0
        screenWidth = Int(h.screen_w)
        screenHeight = Int(h.screen_h)
        bluetooth = withUnsafePointer(to: &h) { rfm_hello_is_ble($0) != 0 }
        isPanel = withUnsafePointer(to: &h) { rfm_hello_is_remotefm1($0) != 0 }
    }
}

public enum RemoteFM1Unit {
    /// What FIZZ calls itself in HELLO (at most 16 bytes).
    public static let hostName = "FIZZ iOS"

    /// The update identity of RemoteFM1 0.2 and later ("FM-1_52XXYYZZ", a development build "FM-1_5200"). 0.1 answered
    /// as the official firmware (FM-1_015): only HELLO tells that one.
    public static func isRemoteFM1(identity: String) -> Bool { identity.hasPrefix("FM-1_52") }

    /// HELLO, framed (F0 7D 52 46 01 <7-in-8 host name> F7).
    public static var helloMessage: [UInt8] {
        var body = [UInt8](repeating: 0, count: 16)
        let n = hostName.withCString { rfm_build_hello(&body, $0) }
        var out = [UInt8](repeating: 0, count: Int(rfm_sx_size(n)))
        let k = rfm_sx_frame(UInt8(RFM_HELLO), body, n, &out)
        return Array(out[0..<Int(k)])
    }

    /// The HELLO_R in a complete message from a port (nil: something else).
    public static func parseHelloReply(_ message: [UInt8]) -> RemoteFM1Hello? {
        RFMMessages.parse(message).lazy.compactMap { m -> RemoteFM1Hello? in
            guard m.cmd == UInt8(RFM_HELLO_R) else { return nil }
            var h = rfm_hello_t()
            guard rfm_parse_hello(m.body, UInt32(m.body.count), &h) == 0 else { return nil }
            return RemoteFM1Hello(h)
        }.first
    }

    /// HELLO on a transport just opened (its messages read here): the unit's HELLO_R, nil when nothing answered within
    /// `timeoutMs`. Harmless to any FM-1 and to a RemoteFM1 unit in another host's session.
    public static func hello(on transport: any MIDITransport, timeoutMs: Int = 400) async -> RemoteFM1Hello? {
        do { try transport.send(helloMessage) } catch { return nil }
        let messages = transport.messages
        return await withTaskGroup(of: RemoteFM1Hello?.self) { g in
            g.addTask {
                for await m in messages { if let h = parseHelloReply(m) { return h } }
                return nil
            }
            g.addTask {
                try? await Task.sleep(for: .milliseconds(timeoutMs))
                return nil
            }
            let first = await g.next() ?? nil
            g.cancelAll()
            return first
        }
    }
}

/// RemoteFM1's messages in a byte stream (libremotefm1's SysEx parser), for one-off reads.
enum RFMMessages {
    struct Message { let cmd: UInt8; let body: [UInt8] }

    private final class Box { var got: [Message] = [] }

    static func parse(_ bytes: [UInt8]) -> [Message] {
        let box = Box()
        let ctx = Unmanaged.passUnretained(box).toOpaque()
        guard let rx = rfm_rx_new(Int32(RFM_LINK_SX), RFM_SX_BODY_MAX + 64, { ctx, cmd, body, n in
            let b = Unmanaged<Box>.fromOpaque(ctx!).takeUnretainedValue()
            b.got.append(Message(cmd: cmd, body: body.map { Array(UnsafeBufferPointer(start: $0, count: Int(n))) } ?? []))
        }, nil, ctx) else { return [] }
        rfm_rx_feed(rx, bytes, UInt32(bytes.count))
        rfm_rx_free(rx)
        return withExtendedLifetime(box) { box.got }
    }
}
