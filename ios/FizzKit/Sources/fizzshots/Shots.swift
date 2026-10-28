// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// PNGs of the app's screens with the demo unit's data, without a simulator: SwiftUI's ImageRenderer on the Mac, the
// screens in snapshot mode (plain stacks, motion standing still). No window is opened.
//
//   swift run fizzshots OUTDIR

#if os(macOS)
import AppKit
import FizzData
import FizzKit
import FizzUI
import ImageIO
import SwiftUI
import UniformTypeIdentifiers

@main
struct Shots {
    static let phone = CGSize(width: 390, height: 844)
    static let pad = CGSize(width: 1180, height: 820)

    @MainActor
    static func main() async {
        NSApplication.shared.setActivationPolicy(.prohibited)
        let out = URL(fileURLWithPath: CommandLine.arguments.count > 1 ? CommandLine.arguments[1] : "shots")
        try? FileManager.default.createDirectory(at: out, withIntermediateDirectories: true)
        if !FizzFont.register() { print("Inter Tight not registered: the system font is used") }

        // looking: a model never started
        let looking = UnitModel(source: DemoLinkSource(.fizz))
        save(ConnectView(model: looking), phone, out, "connect-search")

        for (v, name) in [(DemoVariant.update, "connect-update"), (.notfizz, "connect-notfizz"), (.two, "connect-two")] {
            let m = UnitModel(source: DemoLinkSource(v))
            m.start()
            await until { m.state.kind != "searching" }
            save(ConnectView(model: m), phone, out, name)
            m.stop()
        }

        // (the backups kept: a fresh folder, nothing in it yet)
        let baks = FileManager.default.temporaryDirectory.appendingPathComponent("fizzshots-\(UUID().uuidString)")
        let m = UnitModel(source: DemoLinkSource(.fizz), backups: baks)
        m.start()
        await until { m.dumpsLoaded && m.layout != nil }
        save(ConnectView(model: m), phone, out, "connect-found")
        for tab in [FizzTab.backup, .library, .projects, .edit] {
            save(RootView(model: m, tab: .constant(tab), wide: false), phone, out, tab.rawValue)
        }
        save(RootView(model: m, tab: .constant(.library), wide: true), pad, out, "ipad-library")
        save(RootView(model: m, tab: .constant(.backup), wide: true), pad, out, "ipad-backup")

        // BACKUP with backups kept (one old), and a backup at 60 %
        do { try await DemoBackups.seed(m.backups.store) } catch { print("demo backups: \(error)") }
        await m.backups.store.reload()
        save(RootView(model: m, tab: .constant(.backup), wide: false), phone, out, "backup-rows")
        save(RootView(model: m, tab: .constant(.backup), wide: true), pad, out, "ipad-backup-rows")
        save(BackingUpView(model: m, run: .still(fraction: 0.6, layout: m.layout ?? .device, elapsed: 7)), phone, out, "backing-up")
        await i4(m, out)
        m.stop()
    }

    /// I4: RESTORE's sheet (yesterday's backup), writing, a failure; IMPORT with the demo files (one heard on the
    /// track); EXPORT of three presets as .dn2pst; the iPad's IMPORT tab.
    @MainActor
    static func i4(_ m: UnitModel, _ out: URL) async {
        let backupTab = RootView(model: m, tab: .constant(.backup), wide: false)
        if let e = m.backups.store.entries.first(where: { Calendar.current.isDateInYesterday($0.date) }) {
            await m.restoring.present(e)
            save(SnapshotSheet(top: 255, color: FZ.yellow) { backupTab } content: { RestoreSheet(model: m) }, phone, out, "restore")
            m.restoring.close()
        }
        let started = Date().addingTimeInterval(-9)
        save(RestoreProgressView(model: m, run: RestoreRun(phase: .writing, fraction: 0.74, name: "x", started: started)), phone, out, "restore-writing")
        m.restoring.subject = "NOIR"
        m.restoring.highlight = 33..<41
        save(RestoreProgressView(model: m, run: RestoreRun(phase: .writing, fraction: 0.74, name: "x", started: started)), phone, out, "projects-writing")
        m.restoring.subject = nil
        m.restoring.highlight = nil
        save(RestoreFailureView(model: m, failure: .cleared("the FM-1 was disconnected")), phone, out, "restore-cleared")

        if let files = try? await DemoImports.files() { await m.importer.add(files: files) }
        await m.actions.setTrackOn(true)
        m.importer.pick(.track)
        m.importer.pick(.track)
        m.importer.pick(.track)
        m.importer.pick(.mine)
        if let r = m.importer.rows.first(where: { $0.item.sound?.name == "WARM SUB" }) { await m.importer.play(r.id) }
        save(SnapshotSheet(top: 62) { backupTab } content: { ImportSheet(model: m) }, phone, out, "import")
        save(RootView(model: m, tab: .constant(.importer), wide: true), pad, out, "ipad-import")
        _ = await m.actions.cancelAudition()
        await m.actions.setTrackOn(false)

        let three = await exportSounds(m)
        let sheet = await ExportSheet.prepared(sounds: three, pick: .dn2pst)
        save(SnapshotSheet(top: 62) { RootView(model: m, tab: .constant(.library), wide: false) } content: { sheet }, phone, out, "export")
    }

    /// GLASS BELL, SUB DROP, NIGHT PAD as their DUMPs give them.
    @MainActor
    static func exportSounds(_ m: UnitModel) async -> [FizzSound] {
        guard let e = try? FizzDataShared.engine() else { return [] }
        var out: [FizzSound] = []
        for slot in [2, 1, 3] {
            guard let r = m.mine.first(where: { $0.slot == slot }), let f = m.actions.frame(of: r),
                  let s = try? await e.soundFromStoreFrame(f) else { continue }
            out.append(s)
        }
        return out
    }

    @MainActor
    static func until(_ ok: () -> Bool) async {
        for _ in 0..<500 where !ok() { try? await Task.sleep(for: .milliseconds(20)) }
    }

    @MainActor
    static func save(_ v: some View, _ size: CGSize, _ dir: URL, _ name: String) {
        let phone = size.width < 700
        let insets = phone ? EdgeInsets(top: 54, leading: 0, bottom: 34, trailing: 0)
            : EdgeInsets(top: 30, leading: 0, bottom: 20, trailing: 0)
        let content = v
            .environment(\.fizzSnapshot, true)
            .environment(\.fizzInsets, insets)
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
#else
// (a Mac tool: on other platforms it only has to compile, for the package's test scheme)
@main
struct Shots { static func main() {} }
#endif
