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
import FizzEngine
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
        // (`fizzshots OUTDIR units`: the unit picker's alone)
        if CommandLine.arguments.dropFirst(2).contains("units") { await unitShots(out); return }

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

        // ---- I3: LIBRARY (a row swiped, the preset sheet with its USED BY and the unit's browser open on TRACK 3), POOL
        for k in m.pool.map(\.slot) { _ = await m.actions.poolUsers(k) }
        if let r = m.mine.first(where: { $0.slot == 2 }) {
            _ = await m.actions.usedBy(r)
            save(RootView(model: m, tab: .constant(.library), wide: false).environment(\.fizzShotSelection, "swipe:mine:4"),
                 phone, out, "library-swipe")
            _ = await m.actions.loadOnUnit(r, track: 3)
            save(PresetSheetShot(model: m, key: "mine:2"), phone, out, "preset-sheet")
            save(RootView(model: m, tab: .constant(.library), wide: true).environment(\.fizzShotSelection, "mine:2"),
                 pad, out, "ipad-library-preset")
        }
        // LIBRARY's search: across MY PRESETS and FACTORY (ALL), nothing found, the iPad's header
        save(RootView(model: m, tab: .constant(.library), wide: false).environment(\.fizzShotSelection, "search:bass"),
             phone, out, "library-search")
        save(RootView(model: m, tab: .constant(.library), wide: false).environment(\.fizzShotSelection, "search:zebra"),
             phone, out, "library-search-none")
        save(RootView(model: m, tab: .constant(.library), wide: true).environment(\.fizzShotSelection, "search:glass"),
             pad, out, "ipad-library-search")
        // the seven tag icons (ui.js TAG_ICON) big, in their colours, to set beside the web's
        save(HStack(spacing: 20) {
            ForEach(FizzTag.allCases) { t in TagIcon(t).frame(width: 80, height: 80) }
        }.padding(20).background(Color.black), CGSize(width: 760, height: 120), out, "tag-icons")
        save(RootView(model: m, tab: .constant(.pool), wide: false).environment(\.fizzShotSelection, "pool:3"), phone, out, "pool")
        save(RootView(model: m, tab: .constant(.pool), wide: true).environment(\.fizzShotSelection, "pool:3"), pad, out, "ipad-pool")
        save(LogSheetShot(model: m), phone, out, "log")

        // ---- I5: EDIT
        await editShots(m, out)
        await editMixShots(m, out)

        // ---- I4: RESTORE, IMPORT, EXPORT
        await i4(m, out)
        m.stop()
        await projectShots(out)
        await keptShots(out)
        await playShots(out)
        await unitShots(out)
    }

    // ---- the unit picker and SEND between the units (mock-ups 34-36): the demo FM-1 on USB, a silent one over
    // Bluetooth, the emulated FIZZ
    @MainActor
    static func unitShots(_ out: URL) async {
        let tmp = FileManager.default.temporaryDirectory
        let e = FizzEmulator(storage: nil)
        let host = EmulatorHost(emulator: e)
        let m = UnitModel(source: DemoUnitsSource(bulkMs: 0...0.2), emulator: host,
                          backups: tmp.appendingPathComponent("fizzshots-u-\(UUID().uuidString)"),
                          projects: tmp.appendingPathComponent("fizzshots-up-\(UUID().uuidString)"),
                          presets: tmp.appendingPathComponent("fizzshots-uk-\(UUID().uuidString)"))
        host.launch()
        m.start()
        await until { m.unitKey == .port("FIZZ FM-1") && m.dumpsLoaded && m.other?.taken != nil }
        // (the emulated FIZZ starts empty: the FM-1's first fourteen sent to it, as SEND TO THIS iPHONE does)
        _ = await m.sendToOther(rows: Array(m.mine.prefix(14)))
        // 34: THIS iPHONE picked by hand, the FM-1 beside it, the Bluetooth one asleep
        await m.pick(.here)
        await until { m.onEmulator && m.dumpsLoaded && m.other?.taken != nil }
        m.openPicker()
        for _ in 0..<3 { await until { m.seen.contains { $0.name == DemoUnitsSource.silentName } } }
        save(UnitPickerShot(model: m), phone, out, "units-picker")
        m.pickerClosed()
        // 36: a preset's sheet with SEND TO FM-1, then the toast; KEPT's SELECT with SEND n TO FM-1; the iPad
        save(PresetSheetShot(model: m, key: "mine:2"), phone, out, "units-send-sheet")
        save(SendToastShot(model: m, key: "mine:2"), phone, out, "units-send-toast")
        save(RootView(model: m, tab: .constant(.library), wide: true).environment(\.fizzShotSelection, "mine:2"),
             pad, out, "units-ipad-send")
        do { try await DemoKept.seed(m.kept) } catch { print("demo kept: \(error)") }
        m.kept.selecting = true
        let pick = m.kept.items.prefix(3).map(\.id).joined(separator: ",")
        save(RootView(model: m, tab: .constant(.library), wide: false).environment(\.fizzShotSelection, "kept-select:\(pick)"),
             phone, out, "units-kept-select")
        m.kept.selecting = false
        // 35: from PLAY, the FM-1 picked: PLAY's keys play its track
        m.openPicker(fromPlay: true)
        await m.pick(.port("FIZZ FM-1"))
        await until { m.dumpsLoaded && m.other?.taken != nil }
        save(UnitPickerShot(model: m, tab: .play), phone, out, "units-picker-play")
        m.pickerClosed()
        // the other way round: SEND TO THIS iPHONE
        save(PresetSheetShot(model: m, key: "mine:2"), phone, out, "units-send-here")
        m.stop()
        e.stop()
    }

    // ---- KEPT (mock-ups 23-26, 28): the phone's presets, a kept preset's sheet, SELECT and SEND, EDIT on one, the iPad
    @MainActor
    static func keptShots(_ out: URL) async {
        let tmp = FileManager.default.temporaryDirectory
        let m = UnitModel(source: DemoLinkSource(.fizz, bulkMs: 0...0.2), backups: tmp.appendingPathComponent("fizzshots-b-\(UUID().uuidString)"),
                          presets: tmp.appendingPathComponent("fizzshots-k-\(UUID().uuidString)"))
        m.start()
        await until { m.dumpsLoaded && m.layout != nil }
        do { try await DemoKept.seed(m.kept) } catch { print("demo kept: \(error)") }
        let ids = m.kept.items.map(\.id)
        save(RootView(model: m, tab: .constant(.library), wide: false).environment(\.fizzShotSelection, "kept"), phone, out, "kept")
        save(RootView(model: m, tab: .constant(.library), wide: false).environment(\.fizzShotSelection, "kept-search:glass"),
             phone, out, "kept-search")
        if let k = m.kept.items.first(where: { $0.name == "GLASS BELL" }) {
            save(KeptSheetShot(model: m, id: k.id), phone, out, "kept-sheet")
        }
        m.kept.selecting = true
        let pick = [0, 2, 3, 5].compactMap { ids.indices.contains($0) ? ids[$0] : nil }.joined(separator: ",")
        save(RootView(model: m, tab: .constant(.library), wide: false).environment(\.fizzShotSelection, "kept-select:\(pick)"),
             phone, out, "kept-select")
        m.kept.selecting = false
        if let k = m.kept.items.first(where: { $0.name == "RAIN FX" }) {
            save(RootView(model: m, tab: .constant(.library), wide: true).environment(\.fizzShotSelection, "kept:\(k.id)"),
                 pad, out, "ipad-kept")
        }
        // EDIT on a kept preset, TRACK on (heard on the track, kept)
        await m.actions.setTrackOn(true)
        if let k = m.kept.items.first(where: { $0.name == "NIGHT GLASS" }) {
            m.editTarget = .kept(k)
            await m.editor.show()
            await m.editor.keepOn()
            m.editor.set(6, m.editor.value(6) + 9)
            save(RootView(model: m, tab: .constant(.edit), wide: false), phone, out, "kept-edit")
            m.editor.hide()
        }
        // a unit preset's sheet with → KEPT; IMPORT's KEPT target
        save(PresetSheetShot(model: m, key: "mine:3"), phone, out, "preset-sheet-kept")
        await m.actions.setTrackOn(false)
        m.stop()
    }

    // ---- PLAY and the app with no FM-1 (mock-ups 23, 27, 29-33): the emulated FIZZ itself, its screen drawn by its core
    @MainActor
    static func playShots(_ out: URL) async {
        let tmp = FileManager.default.temporaryDirectory
        let e = FizzEmulator(storage: nil)
        let host = EmulatorHost(emulator: e)
        let m = UnitModel(source: NoPorts(), emulator: host, backups: tmp.appendingPathComponent("fizzshots-e-\(UUID().uuidString)"),
                          projects: tmp.appendingPathComponent("fizzshots-ep-\(UUID().uuidString)"))
        host.launch()
        m.start()
        await until { m.onEmulator && m.dumpsLoaded && m.layout != nil }
        save(RootView(model: m, tab: .constant(.library), wide: false), phone, out, "offline-library")
        save(RootView(model: m, tab: .constant(.backup), wide: false), phone, out, "offline-backup")
        save(RootView(model: m, tab: .constant(.library), wide: true), pad, out, "ipad-offline-library")
        // the demo pattern playing: the core's own screen and LEDs
        await host.serve(false)
        e.tap(.home)
        e.run(ms: 600)
        e.tap(.play)
        e.run(ms: 1450)
        let p = host.play
        p.theme = .tangerine
        p.showNow()
        for (v, name) in [(PlayViewKind.full, "play-full"), (.keys, "play-keys"), (.panel, "play-panel"), (.screen, "play-screen")] {
            p.view = v
            save(RootView(model: m, tab: .constant(.play), wide: false), phone, out, name)
        }
        // STEPS (mock-ups 38-40): step 1 held, C4 and G4 down (a render leaves PLAY, which lets go: held again each time)
        p.view = .steps
        e.run(ms: 100)
        let hold = {
            p.padDown(0)
            e.run(ms: 300)
            p.noteDown(60, velocity: 100)
            p.noteDown(67, velocity: 96)
            e.run(ms: 150)
            p.showNow()
        }
        hold()
        save(RootView(model: m, tab: .constant(.play), wide: false), phone, out, "play-steps")
        p.theme = .fizz
        hold()
        save(RootView(model: m, tab: .constant(.play), wide: false), CGSize(width: 844, height: 390), out, "play-steps-landscape")
        p.theme = .tangerine
        hold()
        save(RootView(model: m, tab: .constant(.play), wide: true), pad, out, "ipad-play-steps")
        p.letGoAll()
        e.run(ms: 50)
        p.view = .full
        p.sheet = true
        save(RootView(model: m, tab: .constant(.play), wide: false), phone, out, "play-sheet")
        p.sheet = false
        p.theme = .fizz
        save(RootView(model: m, tab: .constant(.play), wide: false), CGSize(width: 844, height: 390), out, "play-landscape")
        p.theme = .tangerine
        save(RootView(model: m, tab: .constant(.play), wide: true), pad, out, "ipad-play")
        // the LCD full screen (a double tap on it)
        p.lcdFull = true
        save(RootView(model: m, tab: .constant(.play), wide: false), phone, out, "play-lcd-full")
        p.lcdFull = true        // (a render's PLAY is left after it: put back)
        save(RootView(model: m, tab: .constant(.play), wide: true), pad, out, "ipad-play-lcd-full")
        p.lcdFull = false
        m.stop()
        e.stop()
    }

    // ---- I6: PROJECTS (mock-ups 14, 18-20, 22): the demo unit's store read, changes waiting, kept projects
    @MainActor
    static func projectShots(_ out: URL) async {
        let tmp = FileManager.default.temporaryDirectory
        let m = UnitModel(source: DemoLinkSource(.fizz, bulkMs: 0...0.2), backups: tmp.appendingPathComponent("fizzshots-b-\(UUID().uuidString)"),
                          projects: tmp.appendingPathComponent("fizzshots-p-\(UUID().uuidString)"))
        m.start()
        await until { m.dumpsLoaded && m.layout != nil }
        let pm = m.projectsTab
        await pm.show()
        save(RootView(model: m, tab: .constant(.projects), wide: false), phone, out, "projects-slots")
        // kept: four projects at different dates
        let day = 86400.0
        for (p, ago) in [(6, 0.2), (0, 1.3), (5, 2.5), (3, 8.0)] {
            pm.select(p)
            await pm.keep(date: Date().addingTimeInterval(-ago * day))
        }
        // SUNDAY A03-A05 copied, pasted at NOIR C01, NOIR A11 cleared, C02-C03 selected
        pm.select(1)
        pm.selectRange(from: 2, to: 4)
        await pm.copy()
        pm.select(0)
        pm.tap(32)
        pm.paste()
        if let a = pm.ask { pm.ask = nil; await a.run() }
        try? await Task.sleep(for: .milliseconds(300))
        pm.tap(10)
        pm.clear()
        if let a = pm.ask { pm.ask = nil; await a.run() }
        pm.selectRange(from: 33, to: 34)
        pm.dismissNote()
        pm.mode = .patterns
        save(RootView(model: m, tab: .constant(.projects), wide: false), phone, out, "projects-patterns")
        save(RootView(model: m, tab: .constant(.projects), wide: true), pad, out, "ipad-projects")
        pm.mode = .kept
        if let k = pm.kept.items.first {
            pm.keptSel = k.id
            await pm.copy(from: k, ks: [2])
            pm.dismissNote()
            pm.startLoad(k)
        }
        save(RootView(model: m, tab: .constant(.projects), wide: false), phone, out, "projects-kept")
        pm.disarm()
        pm.mode = .slots
        save(RootView(model: m, tab: .constant(.projects), wide: false), phone, out, "projects-slots-changes")
        // writing: the store as read kept first (its card), the write at 48 %
        if let e = try? FizzDataShared.engine(), let unit = m.unit, let before = pm.storeImage,
           let bytes = try? await e.makeFizzbak(FizzbakInput(kind: .full, image: before, region: FizzStoreRegion(lo: 0x97000, size: before.count),
                                                             presets: nil, fizz: "0.12.0-beta.11", unitIdentity: unit.idText,
                                                             unitPort: unit.portName, date: Date())) {
            _ = try? await m.backups.store.add(bytes, name: "FIZZ before projects 2026-10-27 1431.fizzbak")
        }
        let w = ProjectsWrite(names: ["NOIR"], changed: Set(33..<41), sectors: m.layout?.sectors ?? 73,
                              libSectors: m.layout?.libSectors ?? 22, started: Date())
        save(ProjectsWritingView(model: m, write: w, still: RestoreRun(phase: .writing, fraction: 0.74, name: "the projects", started: Date())),
             phone, out, "projects-writing")
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
        await e.keepOn()
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
        // (every page fits above KEEP / CANCEL: the phone's shortest case)
        for p in EditPage.allCases where p != .ops {
            e.page = p
            edit("edit-audition-\(p.rawValue)")
        }
        e.page = .openv
        e.env = 1
        edit("edit-audition-openv-b")
        e.env = 0
        e.page = .filter
        e.filterEnv = true
        edit("edit-audition-filter-env")
        e.filterEnv = false
        e.page = .ops
        _ = await m.actions.cancelAudition()
        await m.actions.setTrackOn(false)
        var b = [UInt8](repeating: 0, count: 128)
        b[0] = 0x51; b[1] = 6
        for k in 0..<28 { b[2 + 28 + k] = UInt8(16 + k); b[2 + k] = k % 3 == 2 ? 0 : UInt8(20 + k * 4) | 128 }
        b[124] = 0x4d; b[125] = 0x49; b[126] = 0x44; b[127] = 0x49
        m.editTarget = .sound(FizzSound(name: "CC MOTION", blob: b), label: "cc motion.fizzsnd")
        await e.show()
        edit("edit-midi")
        // IMPORT's EDIT: a file's sound, REVERT · KEEP · SAVE AS NEW
        if let r = m.mine.first(where: { $0.slot == 2 }), var fs = await m.actions.sound(of: r) {
            fs.name = "WARM SUB"
            m.editTarget = .sound(fs, label: "warm sub.dnsnd")
            await e.show()
            e.set(6, e.value(6) + 9)
            edit("edit-file")
            save(RootView(model: m, tab: .constant(.edit), wide: true), pad, out, "ipad-edit-file")
        }
        // mock-up 37: NIGHT PAD (MY PRESETS 3) borrowed on TRACK 2, no TRACK switch, no KEEP / CANCEL bar
        m.actions.trackSlot = 2
        if let r = m.mine.first(where: { $0.slot == 3 }) {
            m.editTarget = .preset(r)
            await e.show()
            e.page = .openv
            e.set(6, e.value(6) + 5)
            edit("edit-borrow")
            save(RootView(model: m, tab: .constant(.edit), wide: true), pad, out, "ipad-edit-borrow")
            e.page = .ops
        }
        e.hide()
        await until { !m.actions.borrowed && m.actions.pending == nil }
        m.actions.trackSlot = 0
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
