// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// FIZZ's seven tags as the web Manager draws them (site/manager/ui.js TAG_ICON, 24 x 24, stroked round): BASS a slow
// low wave, KEYS piano keys, PLUCK a plucked string, PAD a soft cloud, LEAD a sharp saw, DRUMS a drum and its sticks,
// FX a sparkle. POOL's tiles show a tagged sound's first tag this way; the tag filter chips carry them too.

import SwiftUI

extension FizzTag {
    /// ui.js TAG_ICON[tag], element for element.
    public var art: VectorArt {
        switch self {
        case .bass:
            VectorArt(24, 24, [.path("M2 12c3.3-8 6.7-8 10 0s6.7 8 10 0"), .path("M2 20h20")])
        case .keys:
            VectorArt(24, 24, [.rect(3, 5, 18, 14, 2), .path("M9 12.5V19M15 12.5V19"), .path("M7.5 5v7.5h3V5M13.5 5v7.5h3V5")])
        case .pluck:
            VectorArt(24, 24, [.path("M2 7h5l5 8 5-8h5"), .circle(12, 18.5, 1.6)])
        case .pad:
            VectorArt(24, 24, [.path("M7 18h10a4 4 0 0 0 .4-8 5.5 5.5 0 0 0-10.6.6A3.7 3.7 0 0 0 7 18z")])
        case .lead:
            VectorArt(24, 24, [.path("M2 18l6-12v12l6-12v12l6-12")])
        case .drums:
            VectorArt(24, 24, [.ellipse(12, 11, 8, 3), .path("M4 11v6c0 1.7 3.6 3 8 3s8-1.3 8-3v-6"), .path("M7 3l4 6M17 3l-4 6")])
        case .fx:
            VectorArt(24, 24, [.path("M11 3c.7 4.6 3.3 7.3 8 8-4.7.7-7.3 3.3-8 8-.7-4.7-3.3-7.3-8-8 4.7-.7 7.3-3.4 8-8z"), .path("M19 3v4M17 5h4")])
        }
    }

    /// The icon a preset shows for its tags (ui.js tagsOf(mask)[0]): its first tag's; nil when it has none.
    public static func icon(mask: Int) -> FizzTag? { of(mask: mask).first }
}

/// A tag's icon (pool.css .ps .ti: stroke 2 in 24, round caps and joins), in the tag's colour unless told otherwise.
public struct TagIcon: View {
    let tag: FizzTag
    let color: Color?
    let lineWidth: CGFloat
    let glow: Bool

    public init(_ tag: FizzTag, color: Color? = nil, lineWidth: CGFloat = 2, glow: Bool = false) {
        self.tag = tag; self.color = color; self.lineWidth = lineWidth; self.glow = glow
    }

    public var body: some View {
        let c = color ?? tag.tint.color
        IconView(tag.art, color: c, lineWidth: lineWidth, backdrop: .clear)
            .shadow(color: glow ? c.opacity(0.7) : .clear, radius: glow ? 3 : 0)
    }
}
