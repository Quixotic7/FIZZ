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
        await editShots(m, out)
        m.stop()
    }

    /// EDIT (phase I5): TRACK on, ROUND BASS heard on track 3 and kept; its pages; B1's RATIO turned; the keybed; the
    /// KEEP / CANCEL bar; a MIDI sound; the iPad.
    @MainActor
    static func editShots(_ m: UnitModel, _ out: URL) async {
        let e = m.editor
        await m.actions.setTrackOn(true)
        m.editTarget = .preset(m.mine[7])
        await e.show()
        await m.actions.keepAudition()
        let edit = { (name: String) in save(RootView(model: m, tab: .constant(.edit), wide: false), phone, out, name) }
        edit("edit")
        for p in EditPage.allCases where p != .ops {
            e.page = p
            edit("edit-\(p.rawValue)")
        }
        e.page = .ops
        e.turning = EditTurn(id: 3, label: "B1 RATIO", tint: FZ.k3)
        edit("edit-drag")
        e.turning = nil
        e.keysOpen = true
        edit("edit-keys")
        e.keysOpen = false
        save(RootView(model: m, tab: .constant(.edit), wide: true), pad, out, "ipad-edit")
        e.page = .filter
        save(RootView(model: m, tab: .constant(.edit), wide: true), pad, out, "ipad-edit-filter")
        e.page = .ops
        _ = await m.actions.pick(m.mine[1])
        edit("edit-audition")
        save(RootView(model: m, tab: .constant(.edit), wide: true), pad, out, "ipad-edit-audition")
        _ = await m.actions.cancelAudition()
        await m.actions.setTrackOn(false)
        var b = [UInt8](repeating: 0, count: 128)
        b[0] = 0x51; b[1] = 6
        for k in 0..<28 { b[2 + 28 + k] = UInt8(16 + k); b[2 + k] = k % 3 == 2 ? 0 : UInt8(20 + k * 4) | 128 }
        b[124] = 0x4d; b[125] = 0x49; b[126] = 0x44; b[127] = 0x49
        m.editTarget = .sound(FizzSound(name: "CC MOTION", blob: b), label: "cc motion.fizzsnd")
        await e.show()
        edit("edit-midi")
        e.hide()
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
