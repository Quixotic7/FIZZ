// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// `fizzshots OUTDIR landscape`: every tab on the iPhone held sideways (LandscapeRoot.swift), at an iPhone 13 Pro
// Max's 926 x 428 and an iPhone 14's 844 x 390, the notch's and the home indicator's room kept.

#if os(macOS)
import FizzData
import FizzKit
import FizzUI
import ImageIO
import SwiftUI
import UniformTypeIdentifiers

extension Shots {
    static let sides: [(CGSize, String)] = [(CGSize(width: 926, height: 428), "max"), (CGSize(width: 844, height: 390), "14")]

    @MainActor
    static func landscapeShots(_ out: URL) async {
        let tmp = FileManager.default.temporaryDirectory
        let m = UnitModel(source: DemoLinkSource(.fizz, bulkMs: 0...0.2), backups: tmp.appendingPathComponent("fizzshots-b-\(UUID().uuidString)"),
                          projects: tmp.appendingPathComponent("fizzshots-p-\(UUID().uuidString)"))
        m.start()
        await until { m.dumpsLoaded && m.layout != nil }
        do { try await DemoBackups.seed(m.backups.store) } catch { print("demo backups: \(error)") }
        await m.backups.store.reload()
        for k in m.pool.map(\.slot) { _ = await m.actions.poolUsers(k) }
        let side = { (tab: FizzTab, sel: String?, name: String) in
            for (size, tag) in sides {
                saveSideways(RootView(model: m, tab: .constant(tab), layout: .phoneWide).environment(\.fizzShotSelection, sel),
                             size, out, "land-\(name)-\(tag)")
            }
        }
        side(.backup, nil, "backup")
        side(.library, nil, "library")
        if let r = m.mine.first(where: { $0.slot == 2 }) { _ = await m.actions.usedBy(r) }
        side(.library, "mine:2", "library-preset")
        let glass = m.mine.filter { $0.name.contains("GLASS") }.prefix(3).map { "mine:\($0.slot)" }
        m.picks.on = true
        m.picks.set(Set(glass))
        _ = await m.actions.bulkUsers(m.picks.rows(m.rows))
        side(.library, "select: search:glass", "library-select")
        m.picks.clear()
        side(.pool, "pool:3", "pool")
        side(.importer, nil, "import")
        let pm = m.projectsTab
        await pm.show()
        side(.projects, nil, "projects")
        pm.select(1)
        pm.selectRange(from: 2, to: 4)
        pm.mode = .patterns
        side(.projects, nil, "projects-patterns")
        // SUNDAY A03-A05 pasted at NOIR C01: the changes waiting (the yellow bar), on the slots and the patterns
        await pm.copy()
        pm.select(0)
        pm.tap(32)
        pm.paste()
        if let a = pm.ask { pm.ask = nil; await a.run() }
        try? await Task.sleep(for: .milliseconds(300))
        pm.dismissNote()
        side(.projects, nil, "projects-changes-patterns")
        pm.clearSelection()
        pm.mode = .slots
        side(.projects, nil, "projects-changes")
        side(.library, "kept", "library-kept")
        let e = m.editor
        await m.actions.setTrackOn(true)
        m.editTarget = .preset(m.mine[7])
        await e.show()
        await e.keepOn()
        side(.edit, nil, "edit")
        e.page = .filter
        side(.edit, nil, "edit-filter")
        e.page = .ops
        _ = await m.actions.pick(m.mine[1])
        side(.edit, nil, "edit-audition")
        side(.library, nil, "library-audition")
        m.stop()
    }

    /// As save, the iPhone on its side: the notch's room at both sides (the app is turned either way), the home
    /// indicator's below.
    @MainActor static func saveSideways(_ v: some View, _ size: CGSize, _ dir: URL, _ name: String) {
        let content = v
            .environment(\.fizzSnapshot, true)
            .environment(\.fizzInsets, EdgeInsets(top: 0, leading: 47, bottom: 21, trailing: 47))
            .frame(width: size.width, height: size.height)
            .background(Color.black)
            .environment(\.colorScheme, .dark)
        let r = ImageRenderer(content: content)
        r.scale = 2
        guard let img = r.cgImage else { print("\(name): nothing rendered"); return }
        let url = dir.appendingPathComponent("fizz-\(name).png")
        guard let dst = CGImageDestinationCreateWithURL(url as CFURL, UTType.png.identifier as CFString, 1, nil) else { return }
        CGImageDestinationAddImage(dst, img, nil)
        CGImageDestinationFinalize(dst)
        print(url.path)
    }
}
#endif
