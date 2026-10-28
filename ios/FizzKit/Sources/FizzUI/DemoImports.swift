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
}
