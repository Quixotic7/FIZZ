// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// The FM-1's update framing (F0 pack7(00 59 cmd len16 00 payload ~sum) F7, LSB-first bit stream), its handshake and
// read requests: a port of fm1-base's web/fm1ota.js as site/install/fm1lib.js carries it (Copyright (C) 2026 Charles
// Vestal; Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments; MPL-2.0). FIZZ's bulk channel
// (FZBI, FZBR / FZBD, FZSR, FZOK / FZER) rides on it: site/manager/fizzlink.js bulkFrame / parseBulk.

import Foundation

/// The identity a unit gives the update handshake ("FM-1_46001100": model FM-1, version 46001100).
public struct FM1Identity: Sendable, Equatable, Hashable {
    public let text: String
    public let model: String
    public let version: Int
    public let open: Bool

    /// The update loader ("ota-..").
    public var isOTA: Bool { model.lowercased().hasPrefix("ota-") }
    /// A unit running FIZZ.
    public var isFizz: Bool { !isOTA && text.hasPrefix(FM1Bulk.fizzPrefix) }
}

/// A 0x30 read request from the unit (a restore pulls the image with these).
public struct FM1ReadRequest: Sendable, Equatable {
    public let fl: UInt8
    public let addr: UInt32
    public let len: Int
}

/// A bulk reply: its 4-letter tag and its data.
public struct FizzBulkReply: Sendable, Equatable {
    public let tag: String
    public let data: [UInt8]
}

/// FZBI: the store's region and CRC-32.
public struct FizzStoreInfo: Sendable, Equatable {
    public let lo: UInt32
    public let size: UInt32
    public let crc: UInt32
}

public enum FM1Bulk {
    public static let handshakeQuery: [UInt8] = [0xf0, 0x00, 0x32, 0x45, 0x00, 0x00, 0x00, 0x40, 0x7f, 0xf7]
    public static let upgrade: [UInt8] = [0xf0, 0x22, 0x24, 0x35, 0x7f, 0xf7]
    public static let fizzPrefix = "FM-1_46"     // firmware/app/fm1.toml id_prefix
    public static let bulkCmd: UInt8 = 0x46      // app_usb_cmd's command byte
    public static let chunk = 256                // FZBR's largest read
    public static let fzer: [UInt32: String] = [
        10: "the FM-1 is playing: stop it first", 11: "the backup's size is not this FM-1's store",
        12: "the image did not arrive whole (nothing was changed)", 13: "a write failed: FIZZ cleared its store",
        14: "the store did not read back as the image", 15: "this needs the USB cable (not over Bluetooth)",
        1: "refused",
    ]

    // MARK: bit-stream 7-bit packing (LSB first)

    public static func pack7(_ data: [UInt8]) -> [UInt8] {
        var out: [UInt8] = []
        out.reserveCapacity(data.count * 8 / 7 + 1)
        var acc: UInt32 = 0, nb: UInt32 = 0
        for b in data {
            acc |= UInt32(b) << nb; nb += 8
            while nb >= 7 { out.append(UInt8(acc & 0x7f)); acc >>= 7; nb -= 7 }
        }
        if nb > 0 { out.append(UInt8(acc & 0x7f)) }
        return out
    }

    public static func unpack7(_ s: some Collection<UInt8>) -> [UInt8] {
        var out: [UInt8] = []
        out.reserveCapacity(s.count * 7 / 8)
        var acc: UInt32 = 0, nb: UInt32 = 0
        for b in s {
            acc |= UInt32(b & 0x7f) << nb; nb += 7
            while nb >= 8 { out.append(UInt8(acc & 0xff)); acc >>= 8; nb -= 8 }
        }
        return out
    }

    // MARK: frames

    public static func updateFrame(cmd: UInt8, payload: [UInt8] = []) -> [UInt8] {
        let s = payload.reduce(0) { $0 + Int($1) }
        let n = payload.count
        let body: [UInt8] = [0x00, 0x59, cmd, UInt8(n & 0xff), UInt8(truncatingIfNeeded: n >> 8), 0] + payload +
            [UInt8(~s & 0xff)]
        return [0xf0] + pack7(body) + [0xf7]
    }

    /// The answer to a read request: `data` from `addr`.
    public static func response(addr: UInt32, data: [UInt8], fl: UInt8 = 0) -> [UInt8] {
        let len = data.count
        var body: [UInt8] = [0x00, 0x59, 0x30, UInt8((len + 8) & 0xff), UInt8(((len + 8) >> 8) & 0xff), 0, fl]
        body += le32(addr)
        body += [UInt8(len & 0xff), UInt8((len >> 8) & 0xff), 0]
        body += data
        let s = body[6...].reduce(0) { $0 + Int($1) }
        body.append(UInt8(~s & 0xff))
        return [0xf0] + pack7(body) + [0xf7]
    }

    /// A read request as the unit sends it (the demo unit's and the tests' side of a restore).
    public static func readRequest(addr: UInt32, len: Int, fl: UInt8 = 0) -> [UInt8] {
        updateFrame(cmd: 0x30, payload: [fl] + le32(addr) + [UInt8(len & 0xff), UInt8((len >> 8) & 0xff),
                                                             UInt8((len >> 16) & 0xff)])
    }

    /// A response() as the unit reads it: its flags, address and data; nil when it is not one or its sum is wrong.
    public static func parseResponse(_ pkt: [UInt8]) -> (fl: UInt8, addr: UInt32, data: [UInt8])? {
        guard pkt.count >= 2, pkt[0] == 0xf0, pkt[pkt.count - 1] == 0xf7 else { return nil }
        let u = unpack7(pkt[1..<(pkt.count - 1)])
        guard u.count >= 15, u[0] == 0, u[1] == 0x59, u[2] == 0x30 else { return nil }
        let n = Int(u[3]) | (Int(u[4]) << 8), len = Int(u[11]) | (Int(u[12]) << 8)
        guard n == len + 8, u.count >= 6 + n + 1 else { return nil }
        let s = u[6..<(6 + n)].reduce(0) { $0 + Int($1) }
        guard UInt8(~s & 0xff) == u[6 + n] else { return nil }
        return (u[6], rd32(u, 7), Array(u[14..<(14 + len)]))
    }

    public static func parseRequest(_ pkt: [UInt8]) -> FM1ReadRequest? {
        guard pkt.count >= 2, pkt[0] == 0xf0, pkt[pkt.count - 1] == 0xf7 else { return nil }
        let u = unpack7(pkt[1..<(pkt.count - 1)])
        guard u.count == 15, u[0] == 0, u[1] == 0x59, u[2] == 0x30 else { return nil }
        let s = u[6..<14].reduce(0) { $0 + Int($1) }
        guard UInt8(~s & 0xff) == u[14] else { return nil }
        return FM1ReadRequest(fl: u[6], addr: rd32(u, 7), len: Int(u[11]) | (Int(u[12]) << 8) | (Int(u[13]) << 16))
    }

    public static func parseIdentity(_ pkt: [UInt8]) -> FM1Identity? {
        let d = unpack7(pkt.count >= 2 ? pkt[1..<(pkt.count - 1)] : [])
        guard d.count == 34, d[0] == 0, d[1] == 0x59, d[2] == 0x11 else { return nil }
        var t = Array(d[6..<33])
        while t.last == 0 { t.removeLast() }
        let txt = latin1(t)
        if isOpen(txt) { return FM1Identity(text: txt, model: "FM-1", version: 0, open: true) }
        guard let us = txt.firstIndex(of: "_") else { return nil }
        let model = String(txt[..<us]), digits = txt[txt.index(after: us)...]
        guard !model.isEmpty, !digits.isEmpty, digits.unicodeScalars.allSatisfy({ $0.value >= 0x30 && $0.value <= 0x39 })
        else { return nil }
        return FM1Identity(text: txt, model: model, version: Int(digits) ?? Int.max, open: false)
    }

    // /^FM1_(OFFICIAL_OPEN|COMMUNITY_\d+)$/
    static func isOpen(_ t: String) -> Bool {
        if t == "FM1_OFFICIAL_OPEN" { return true }
        let p = "FM1_COMMUNITY_"
        guard t.hasPrefix(p) else { return false }
        let rest = t.unicodeScalars.dropFirst(p.unicodeScalars.count)
        return !rest.isEmpty && rest.allSatisfy { $0.value >= 0x30 && $0.value <= 0x39 }
    }

    // MARK: FIZZ's bulk channel

    public static func bulkFrame(tag: String, words: [UInt32] = []) -> [UInt8] {
        updateFrame(cmd: bulkCmd, payload: tag.utf16.map { UInt8(truncatingIfNeeded: $0) } + words.flatMap(le32))
    }

    /// F0 pack7(00 59 46 len16 00 tag[4] data ~sum) F7 -> its tag and data.
    public static func parseBulk(_ p: [UInt8]) -> FizzBulkReply? {
        guard p.count >= 3, p[0] == 0xf0, p[p.count - 1] == 0xf7 else { return nil }
        let u = unpack7(p[1..<(p.count - 1)])
        guard u.count >= 11, u[0] == 0, u[1] == 0x59, u[2] == bulkCmd else { return nil }
        let n = Int(u[3]) | (Int(u[4]) << 8)
        guard u.count >= 7 + n, n >= 4 else { return nil }
        let s = u[6..<(6 + n)].reduce(0) { $0 + Int($1) }
        guard UInt8(~s & 0xff) == u[6 + n] else { return nil }
        return FizzBulkReply(tag: latin1(u[6..<10]), data: Array(u[10..<(6 + n)]))
    }

    // MARK: CRC-32 (IEEE, as zlib)

    private static let crcTable: [UInt32] = (0..<256).map { n in
        var c = UInt32(n)
        for _ in 0..<8 { c = c & 1 != 0 ? 0xedb8_8320 ^ (c >> 1) : c >> 1 }
        return c
    }

    public static func crc32(_ b: some Sequence<UInt8>, _ c0: UInt32 = 0) -> UInt32 {
        var c = c0 ^ 0xffff_ffff
        for x in b { c = crcTable[Int((c ^ UInt32(x)) & 0xff)] ^ (c >> 8) }
        return c ^ 0xffff_ffff
    }

    // MARK: little-endian words

    public static func le32(_ v: UInt32) -> [UInt8] {
        [UInt8(v & 0xff), UInt8((v >> 8) & 0xff), UInt8((v >> 16) & 0xff), UInt8(v >> 24)]
    }

    public static func rd32(_ b: [UInt8], _ o: Int) -> UInt32 {
        UInt32(b[o]) | (UInt32(b[o + 1]) << 8) | (UInt32(b[o + 2]) << 16) | (UInt32(b[o + 3]) << 24)
    }
}
