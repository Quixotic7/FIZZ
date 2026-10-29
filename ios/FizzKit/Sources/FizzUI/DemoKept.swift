// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// KEPT's demo presets (the demo unit's `-kept` launch, the snapshots): made-up sounds (the demo unit's random patches)
// written as kept files, so LIBRARY's KEPT shows a phone's worth (synthetic: no owner's or factory sounds).

import Foundation
import FizzData
import FizzKit

public enum DemoKept {
    static let t = DemoNames.t
    /// name, tags, algorithm, web tags
    static let sounds: [(String, Int, Int, [String])] = [
        ("GLASS BELL", t.pluck, 2, ["SHIMMER"]), ("NIGHT GLASS", t.pad, 3, []), ("SUB DROP 2", t.bass, 1, []),
        ("TAPE KEYS", t.keys, 6, ["LOFI"]), ("ACID WORM", t.bass, 3, []), ("FIZZ HAT", t.drums, 6, []),
        ("SODA LEAD", t.lead, 4, []), ("RAIN FX", t.fx, 7, ["WET", "DARK"]), ("WOOD PLUCK", t.pluck, 4, []),
        ("DRONE 7", t.pad, 2, []), ("CLAP 808", t.drums, 5, []),
    ]

    /// The demo presets kept in `store` (none twice).
    @MainActor public static func seed(_ store: KeptPresets) async throws {
        let names = try FizzDataShared.engine().tagNames
        await store.reload()
        for (k, s) in sounds.enumerated() where store.item("\(s.0).fizzsnd") == nil {
            let p = DemoPreset(slot: k + 1, id: 0x3000 + k * 7, name: s.0, tags: s.1, algorithm: s.2)
            var b = DemoUnit.blob(p)
            for i in 3..<74 { b[i] = b[i] & 0x5f }
            let snd = FizzSound(name: s.0, blob: b, sends: FizzSends(chorus: 12, delay: 20, reverb: 36),
                                tags: names.enumerated().filter { (s.1 >> $0.offset) & 1 == 1 }.map(\.element))
            try await store.add(snd, webTags: s.3, source: ["from": "DEMO"])
        }
    }
}
