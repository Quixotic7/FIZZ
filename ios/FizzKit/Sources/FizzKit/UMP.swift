// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// SysEx as Universal MIDI Packets: type 3 (SysEx7), 64 bits each, up to 6 data bytes. Pure: CoreMIDITransport uses
// these, the tests run them without CoreMIDI.

public enum UMP {
    public enum SysExStatus: UInt32, Sendable {
        case complete = 0, start = 1, `continue` = 2, end = 3
    }

    /// A SysEx message (F0 .. F7; F0 / F7 optional) as SysEx7 UMP words, two per packet.
    public static func sysEx7Words(_ message: [UInt8], group: UInt8 = 0) -> [UInt32] {
        var data = message[...]
        if data.first == 0xf0 { data = data.dropFirst() }
        if data.last == 0xf7 { data = data.dropLast() }
        let bytes = Array(data)
        var words: [UInt32] = []
        let n = max(1, (bytes.count + 5) / 6)
        words.reserveCapacity(n * 2)
        for k in 0..<n {
            let chunk = bytes[min(k * 6, bytes.count)..<min(k * 6 + 6, bytes.count)]
            let status: SysExStatus = n == 1 ? .complete : k == 0 ? .start : k == n - 1 ? .end : .continue
            var b = [UInt8](repeating: 0, count: 6)
            for (i, x) in chunk.enumerated() { b[i] = x & 0x7f }
            let w0 = (UInt32(0x3) << 28) | (UInt32(group & 0xf) << 24) | (status.rawValue << 20) |
                (UInt32(chunk.count) << 16) | (UInt32(b[0]) << 8) | UInt32(b[1])
            let w1 = (UInt32(b[2]) << 24) | (UInt32(b[3]) << 16) | (UInt32(b[4]) << 8) | UInt32(b[5])
            words += [w0, w1]
        }
        return words
    }

    /// A UMP's length in words, by its message type.
    public static func wordCount(_ w0: UInt32) -> Int {
        switch w0 >> 28 {
        case 0x0, 0x1, 0x2, 0x6, 0x7: 1
        case 0x3, 0x4, 0x8, 0x9, 0xa: 2
        case 0xb, 0xc: 3
        default: 4
        }
    }
}

/// Collects SysEx7 packets back into complete F0 .. F7 messages (per group); everything else is skipped.
public struct SysEx7Reassembler: Sendable {
    private var buffers: [UInt8: [UInt8]] = [:]
    /// The longest message kept (a runaway start without an end is dropped).
    public var maxLength = 65536

    public init() {}

    /// Feeds words (one packet's, or several); returns the messages they complete.
    public mutating func feed(_ words: some Collection<UInt32>) -> [[UInt8]] {
        let w = Array(words)
        var out: [[UInt8]] = []
        var i = 0
        while i < w.count {
            let w0 = w[i], n = UMP.wordCount(w0)
            defer { i += n }
            guard w0 >> 28 == 0x3, i + 1 < w.count else { continue }
            let w1 = w[i + 1]
            let group = UInt8((w0 >> 24) & 0xf), status = (w0 >> 20) & 0xf, count = min(6, Int((w0 >> 16) & 0xf))
            let all: [UInt8] = [UInt8((w0 >> 8) & 0xff), UInt8(w0 & 0xff), UInt8(w1 >> 24), UInt8((w1 >> 16) & 0xff),
                                UInt8((w1 >> 8) & 0xff), UInt8(w1 & 0xff)]
            let data = all[0..<count]
            switch status {
            case UMP.SysExStatus.complete.rawValue:
                buffers[group] = nil
                out.append([0xf0] + data + [0xf7])
            case UMP.SysExStatus.start.rawValue:
                buffers[group] = Array(data)
            case UMP.SysExStatus.continue.rawValue:
                guard var b = buffers[group] else { break }
                b += data
                buffers[group] = b.count > maxLength ? nil : b
            case UMP.SysExStatus.end.rawValue:
                guard let b = buffers[group] else { break }
                buffers[group] = nil
                out.append([0xf0] + b + data + [0xf7])
            default:
                break
            }
        }
        return out
    }

    public mutating func reset() { buffers.removeAll() }
}
