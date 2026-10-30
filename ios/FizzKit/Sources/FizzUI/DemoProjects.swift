// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// The demo unit's projects with patterns (phase I6), so PROJECTS has something to show: synthetic records made by the
// engine (entry.js demoPatterns), each project's as many patterns as its pages (WINTER LP's pool full); what a write
// put in the store kept when the image is made again (the same project name, else the same slot).

import Foundation
import FizzData

extension DemoStoreImage {
    /// The projects whose pool is full (128 places; mock-up 43: a pattern dropped there is refused).
    static let fullPools: Set<String> = ["WINTER LP"]

    static func withPatterns(_ image: [UInt8], prev: [UInt8]?, projects: [DemoProject?]) async -> [UInt8] {
        let ps = projects.enumerated().compactMap { k, p in p.map { (slot: k + 1, name: $0.name, pats: $0.pages, demo: $0.demo) } }
        let pools = Dictionary(uniqueKeysWithValues: ps.filter { fullPools.contains($0.name) }.map { ($0.slot, 128) })
        guard let e = try? FizzDataShared.engine(),
              let b = try? await e.demoPatterns(image, prev: prev, projects: ps, pools: pools) else { return image }
        return b
    }
}
