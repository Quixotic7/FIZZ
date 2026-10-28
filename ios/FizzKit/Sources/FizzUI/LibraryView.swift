// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// LIBRARY (mock-ups 09, 16): MY PRESETS / FACTORY, the tag and pool filters, the rows (presets.js casHtml). A tap
// selects; nothing is written. iPad: both lists side by side and the selected preset beside them.

import SwiftUI
import FizzKit

/// The filters (presets.js newFilter): tags (any of them), in the pool.
struct LibraryFilter: Equatable {
    var tags: Set<FizzTag> = []
    var inPool = false

    func ok(_ r: FizzListEntry) -> Bool {
        (tags.isEmpty || FizzTag.of(mask: r.tags).contains { tags.contains($0) }) && (!inPool || r.inPool)
    }
}

struct LibraryView: View {
    let model: UnitModel
    var wide = false
    @State private var factory = 0
    @State private var filter = LibraryFilter()
    @State private var selected: String?

    var body: some View {
        if wide { padBody } else { phoneBody }
    }

    private func key(_ r: FizzListEntry) -> String { "\(r.source):\(r.slot)" }

    private var chips: some View {
        Group {
            FilterChip(title: "POOL", color: FZ.green, on: filter.inPool, poolIcon: true) { filter.inPool.toggle() }
            ForEach(FizzTag.allCases) { t in
                FilterChip(title: t.name, color: t.tint, on: filter.tags.contains(t)) {
                    if filter.tags.contains(t) { filter.tags.remove(t) } else { filter.tags.insert(t) }
                }
            }
        }
    }

    private func rows(_ list: [FizzListEntry], marks: Bool = true, nameSize: CGFloat = 16) -> some View {
        ForEach(list.filter(filter.ok), id: \.slot) { r in
            PresetRow(entry: r, algorithm: model.algorithm(r), selected: selected == key(r), marks: marks, nameSize: nameSize)
                .onTapGesture { selected = selected == key(r) ? nil : key(r) }
                .padding(.bottom, 8)
        }
    }

    // MARK: iPhone

    private var phoneBody: some View {
        VStack(alignment: .leading, spacing: 0) {
            HStack(spacing: 10) {
                Segmented(items: ["MY PRESETS", "FACTORY"], selected: $factory)
                Spacer(minLength: 0)
                Count(n: factory == 0 ? model.mine.count : model.factory.count, of: factory == 0 ? 256 : nil)
            }
            .padding(.horizontal, 16).padding(.top, 4).padding(.bottom, 12)
            ChipScroll { chips }
                .padding(.bottom, 14)
            ScreenScroll(refresh: { await model.refresh() }) {
                rows(factory == 0 ? model.mine : model.factory)
            }
            .padding(.horizontal, 16)
        }
    }

    // MARK: iPad

    private var padBody: some View {
        VStack(alignment: .leading, spacing: 14) {
            // (scrolls sideways: the iPad held upright has no room for them all)
            ChipScroll(inset: 0, spacing: 6) { chips }
                .frame(height: 44, alignment: .leading)
            HStack(alignment: .top, spacing: 14) {
                Card(padding: 18) {
                    Heading(text: "MY PRESETS", count: "\(model.mine.count) / 256").padding(.bottom, 12)
                    ScreenScroll(bottom: 10, refresh: { await model.refresh() }) {
                        rows(model.mine, nameSize: 15)
                    }
                }
                .frame(width: 362)
                Card(padding: 14) {
                    Heading(text: "FACTORY", count: "\(model.factory.count)").padding(.bottom, 12)
                    ScreenScroll(bottom: 10) { rows(model.factory, marks: false, nameSize: 15) }
                }
                .frame(width: 236)
                Card(padding: 18) {
                    PresetDetail(entry: selectedEntry, algorithm: selectedEntry.map(model.algorithm) ?? 1)
                    Spacer(minLength: 0)
                }
                .frame(maxWidth: .infinity)
            }
            .frame(maxHeight: .infinity)
        }
        .padding(.bottom, 26)
    }

    private var selectedEntry: FizzListEntry? {
        guard let s = selected else { return nil }
        return model.rows.first { key($0) == s }
    }
}

/// "143 / 256"
struct Count: View {
    let n: Int
    var of: Int?
    var body: some View {
        HStack(alignment: .firstTextBaseline, spacing: 0) {
            Text("\(n)").fz(24, 800).foregroundStyle(FZ.text.color)
            if let of { Text(" / \(of)").fz(15, 800).foregroundStyle(FZ.muted.color) }
        }
        .fixedSize()
    }
}

/// The selected preset on the iPad (read only for now): its glyph, where it lives, its name, its tags.
struct PresetDetail: View {
    let entry: FizzListEntry?
    let algorithm: Int

    var body: some View {
        if let e = entry {
            let tint = FizzTag.tint(mask: e.tags)
            VStack(alignment: .leading, spacing: 16) {
                HStack(spacing: 14) {
                    AlgoGlyph(algorithm: algorithm, color: tint.color).frame(width: 60, height: 60)
                    VStack(alignment: .leading, spacing: 2) {
                        Heading(text: "\(e.source == "mine" ? "MY PRESETS" : "FACTORY") \(e.slot)")
                        Text(e.name).fz(34, 800, em: -0.01).foregroundStyle(FZ.text.color).lineLimit(1)
                            .minimumScaleFactor(0.6)
                    }
                }
                FlowTags(mask: e.tags)
            }
        }
    }
}

/// The seven tags, the preset's lit.
struct FlowTags: View {
    let mask: Int
    var body: some View {
        let rows = [Array(FizzTag.allCases.prefix(4)), Array(FizzTag.allCases.dropFirst(4))]
        VStack(alignment: .leading, spacing: 7) {
            ForEach(rows.indices, id: \.self) { k in
                HStack(spacing: 7) {
                    ForEach(rows[k]) { t in
                        let on = (mask >> t.rawValue) & 1 != 0
                        Text(t.name).fz(14, 800, em: 0.05)
                            .foregroundStyle(on ? t.ink : FZ.muted.color)
                            .padding(.horizontal, 14).frame(height: 36)
                            .background(RoundedRectangle(cornerRadius: 9).fill(on ? t.tint.color : .clear))
                            .overlay(RoundedRectangle(cornerRadius: 9).strokeBorder(on ? .clear : FZ.line.color, lineWidth: 1.5))
                            .fixedSize()
                    }
                }
            }
        }
    }
}
