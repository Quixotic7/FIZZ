// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// The demo unit's sounds made believable for EDIT (phase I5): each preset's values set in its blob as a sound of its
// tag would have them (ratios from the lists, small offsets, an envelope for a bass, a pad, a pluck ..), varied by its
// id, so the editor's pages show what a real sound looks like. Synthetic: no owner's or factory sound.

import FizzKit

enum DemoVoicing {
    /// Sets the believable values in a version-6 blob (DemoBlob.set packs each as FM TONE does).
    static func tune(_ b: inout [UInt8], _ p: DemoPreset) {
        var s = UInt32(truncatingIfNeeded: p.id &* 2_246_822_519 &+ 7)
        func r(_ n: Int) -> Int {
            s = s &* 1_664_525 &+ 1_013_904_223
            return Int((s >> 8) % UInt32(max(1, n)))
        }
        func pick(_ a: [Int]) -> Int { a[r(a.count)] }
        let tag = FizzTagMask(p.tags)
        var v: [Int: Int] = [
            0: max(1, min(8, p.algorithm)),
            1: pick([3, 3, 4, 5]), 2: pick([3, 7, 11, 15, 9]), 3: pick([3, 4, 5, 6, 8]), 71: pick([1, 3, 4, 5]),
            4: pick([0, 40, -25, 120, 0]), 5: pick([0, 12, 35, 8]), 6: 10 + r(80), 7: -40 + r(60),
            8: 0, 9: 0, 10: pick([0, 12, -8]), 11: pick([0, -3, 5]),
            12: r(6), 13: 30 + r(50), 14: 40 + r(70), 15: 60 + r(60), 16: r(8), 17: 30 + r(60), 18: 20 + r(60), 19: 50 + r(60),
            20: 0, 21: 1, 22: 1, 23: 1, 24: pick([0, 0, 10]), 25: 1, 26: 1, 27: 64, 28: 0, 29: r(30),
            30: 0, 31: 30 + r(50), 32: 20 + r(60), 33: 30 + r(40), 34: 3000 + r(80) * 100, 35: r(40) * 100, 36: pick([1, 1, 3, 4]),
            37: -1600 + r(48) * 100, 38: 0, 39: pick([0, 64, 100]), 40: 0, 41: 127,
            42: 2 + r(6), 43: 40 + r(40), 44: 70 + r(50), 45: 20 + r(40), 46: 96 + r(20),
            72: 0, 74: 0, 75: 127, 76: 0, 77: 32, 78: 0, 79: 0, 80: r(64), 81: 0, 82: 0,
            83: pick([0, 0, 0, 40]), 84: pick([0, 0, 18]), 85: 1,
            110: 0, 111: 1, 112: 20, 113: 64, 114: 1,
        ]
        for k in 0..<3 {                            // the LFOs: SPEED MULT FADE DEST WAVE PHASE TRIG DEPTH
            let o = 47 + 8 * k
            v[o] = 1600 + r(40) * 100; v[o + 1] = 5; v[o + 2] = 0; v[o + 3] = k == 0 ? pick([25, 1, 13]) : 0
            v[o + 4] = r(7); v[o + 5] = 0; v[o + 6] = 0; v[o + 7] = k == 0 ? 600 + r(30) * 100 : 0
        }
        for k in 0..<8 {                            // the modulation slots: SOURCE DEST DEPTH
            let o = 86 + 3 * k
            v[o] = k == 0 ? 1 : k == 1 ? 3 : 0; v[o + 1] = k == 0 ? 25 : k == 1 ? 13 : 0; v[o + 2] = k == 0 ? 40 : k == 1 ? -30 : 0
        }
        if tag.bass { v[36] = 3; v[34] = 2200 + r(30) * 100; v[44] = 100; v[43] = 60 }
        if tag.pad { v[42] = 70 + r(30); v[45] = 80 + r(30); v[44] = 110; v[47] = 900; v[49] = 0 }
        if tag.pluck || tag.drums { v[44] = 0; v[43] = 20 + r(30); v[45] = 15 + r(20); v[13] = 15 + r(20) }
        if tag.lead { v[76] = 1; v[77] = 20 + r(40); v[110] = 1; v[111] = 2 }
        if tag.fx { v[83] = 50 + r(40); v[84] = 20 + r(40) }
        for (id, x) in v { DemoBlob.set(&b, id: id, value: x) }
    }

    /// The tags of a preset, by name.
    struct FizzTagMask {
        let m: Int
        init(_ m: Int) { self.m = m }
        var bass: Bool { m & 1 != 0 }
        var pluck: Bool { m & 4 != 0 }
        var pad: Bool { m & 8 != 0 }
        var lead: Bool { m & 16 != 0 }
        var drums: Bool { m & 32 != 0 }
        var fx: Bool { m & 64 != 0 }
    }
}
