// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// IMPORT's demo files (the demo unit, the snapshots): made up sounds written by EXPORT's own writers as a Digitone
// owner's .dnsnd files, a Digitone II's .dn2pst and .dn2snd, and a FIZZ library, so IMPORT reads them back as it
// reads anyone's (synthetic: no owner's or factory sounds).

import Foundation
import FizzData
import FizzKit

public enum DemoImports {
    static let t = DemoNames.t
    /// name, tags, algorithm, the file
    static let sounds: [(String, Int, Int, FizzExportKind)] = [
        ("AIRY KEYS", t.keys, 2, .dnsnd), ("BROKEN BELL", t.pluck, 4, .dnsnd), ("WARM SUB", t.bass, 1, .dn2pst),
        ("COMB TOWER", t.pad, 3, .dn2pst), ("RIDE 909", t.drums, 5, .dn2snd), ("ORBIT LEAD", t.lead, 6, .fizz),
        ("GLASS CHOIR", t.pad, 7, .dnsnd), ("METAL TOM", t.drums, 2, .dn2pst), ("DUST FX", t.fx, 8, .fizz),
        ("SODA PLUCK", t.pluck, 1, .fizz), ("NEON HORN", t.lead, 3, .dn2snd), ("FOG VEIL", t.pad, 4, .dnsnd),
    ]

    /// The files, in the order the rows show them; FIZZ's together as one .fizzlib.
    public static func files(date: Date = Date()) async throws -> [(name: String, bytes: [UInt8])] {
        let e = try FizzDataShared.engine()
        let names = e.tagNames
        func sound(_ k: Int) -> FizzSound {
            let s = sounds[k]
            var p = DemoPreset(slot: k + 1, id: 0x2000 + k * 11, name: s.0, tags: s.1, algorithm: s.2)
            p.drive = 0
            var b = DemoUnit.blob(p)
            // (calmer values than the demo's random ones: what a Digitone owner's sounds look like after conversion)
            for i in 3..<74 { b[i] = b[i] & 0x3f }
            return FizzSound(name: s.0, blob: b, sends: FizzSends(chorus: 10, delay: 24, reverb: 40),
                             tags: names.enumerated().filter { (s.1 >> $0.offset) & 1 == 1 }.map(\.element))
        }
        var out: [(name: String, bytes: [UInt8])] = []
        var fizz: [FizzSound] = []
        for (k, s) in sounds.enumerated() {
            if s.3 == .fizz { fizz.append(sound(k)); continue }
            let f = try await e.exportFile([sound(k)], kind: s.3, date: date)
            out.append((f.name, f.bytes))
        }
        let lib = try await e.exportFile(fizz, kind: .fizz, date: date)
        out.insert((lib.name.replacingOccurrences(of: "FIZZ presets", with: "ORBIT"), lib.bytes), at: min(5, out.count))
        return out
    }

    /// A made-up Digitone .syx dump: one sound (a version-1 record: its magic, name and end marker, the rest zeros)
    /// and one kit message, which the reader skips with a note (IMPORT's info row).
    public static func syxWithKit(name: String = "SYNTH TEST") -> [UInt8] {
        var record = [UInt8](repeating: 0, count: 283)
        record.replaceSubrange(0..<8, with: [0xBE, 0xEF, 0xBA, 0xCE, 0, 0, 0, 1])
        let n = Array(name.utf8.prefix(15))
        record.replaceSubrange(12..<(12 + n.count), with: n)
        record.replaceSubrange(279..<283, with: [0xBA, 0xCE, 0xF0, 0x0C])
        return syxMessage(0x53, record) + syxMessage(0x52, [UInt8](repeating: 0, count: 40))
    }

    /// A Digitone SysEx message (fm1dnsnd.js parseDnSyx): F0 00 20 3C 0D 00 type 01 01 00, the data 8:7 packed, its
    /// checksum and length (7 + 7 bits each), F7.
    static func syxMessage(_ type: UInt8, _ data: [UInt8]) -> [UInt8] {
        var packed: [UInt8] = []
        for i in stride(from: 0, to: data.count, by: 7) {
            let g = data[i..<min(i + 7, data.count)]
            packed.append(g.enumerated().reduce(0) { $0 | ($1.element >> 7) << (6 - $1.offset) })
            packed += g.map { $0 & 0x7F }
        }
        let sum = packed.reduce(0) { $0 + Int($1) } & 0x3FFF, len = packed.count + 5
        return [0xF0, 0x00, 0x20, 0x3C, 0x0D, 0x00, type, 0x01, 0x01, 0x00] + packed
            + [UInt8(sum >> 7), UInt8(sum & 0x7F), UInt8(len >> 7 & 0x7F), UInt8(len & 0x7F), 0xF7]
    }
}
