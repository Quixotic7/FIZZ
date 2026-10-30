// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// `fizzshots OUTDIR panel`: mock-up 57, a RemoteFM1 unit (the demo's: libremotefm1's fake) as THIS iPHONE's panel.
// The picker offered by itself (USE AS PANEL; the Bluetooth FM-1 asleep), PLAY with the panel attached (the chip, the
// toast), the picker again (PANEL with ×), PLAY's sheet (FM-1 greyed), a release's toast over LIBRARY, and a
// RemoteFM1 unit over Bluetooth (USB only).

#if os(macOS)
import FizzData
import FizzEngine
import FizzKit
import FizzUI
import SwiftUI

extension Shots {
    @MainActor
    static func panelShots(_ out: URL) async {
        let tmp = FileManager.default.temporaryDirectory
        let e = FizzEmulator(storage: nil)
        let host = EmulatorHost(emulator: e)
        let m = UnitModel(source: DemoUnitsSource(usb: false, remoteFM1: true, bulkMs: 0...0.2), emulator: host,
                          backups: tmp.appendingPathComponent("fizzshots-pn-\(UUID().uuidString)"),
                          projects: tmp.appendingPathComponent("fizzshots-pnp-\(UUID().uuidString)"),
                          presets: tmp.appendingPathComponent("fizzshots-pnk-\(UUID().uuidString)"))
        host.launch()
        m.start()
        await until { m.onEmulator && m.dumpsLoaded && m.pickerOpen }
        for _ in 0..<3 {
            await until { m.panelRows.first?.version == DemoRemoteFM1Port.version
                && m.seen.contains { $0.name == DemoUnitsSource.silentName } }
        }
        // PLAY behind it: the demo pattern playing
        await host.serve(false)
        e.tap(.home)
        e.run(ms: 600)
        e.tap(.play)
        e.run(ms: 1450)
        let p = host.play
        p.theme = .tangerine
        p.view = .full
        p.showNow()
        save(UnitPickerShot(model: m, tab: .play), phone, out, "panel-picker")

        // USE AS PANEL: the chip, the toast
        m.pickerClosed()
        await m.usePanelTapped(DemoUnitsSource.panelName)
        await until { host.panel == .active && m.panelToast != nil }
        try? await Task.sleep(for: .milliseconds(300))
        p.showNow()
        save(ZStack { RootView(model: m, tab: .constant(.play), wide: false); PanelToastLayer(model: m, bottom: 64) },
             phone, out, "panel-play-linked")

        // the picker again: PANEL with ×
        m.openPicker(fromPlay: true)
        try? await Task.sleep(for: .milliseconds(600))
        await until { !m.panelRows.isEmpty }
        p.showNow()
        save(UnitPickerShot(model: m, tab: .play), phone, out, "panel-picker-attached")
        m.pickerClosed()

        // PLAY's sheet: This iPhone the source, FM-1 greyed
        p.sheet = true
        p.showNow()
        save(RootView(model: m, tab: .constant(.play), wide: false), phone, out, "panel-play-sheet")
        p.sheet = false

        // a release's quiet toast over a manager tab (the cable pulled)
        if let t = PanelToast.released(.lost) {
            save(ZStack(alignment: .bottom) {
                RootView(model: m, tab: .constant(.library), wide: false)
                PanelToastView(toast: t).padding(.horizontal, 16).padding(.bottom, 84 + 34)
            }, phone, out, "panel-released-toast")
        }
        m.stopPanel()
        await until { host.panelPort == nil }
        m.stop()
        await host.close()

        // a RemoteFM1 unit over Bluetooth beside one on USB: shown, the panel USB only
        let e2 = FizzEmulator(storage: nil)
        let host2 = EmulatorHost(emulator: e2)
        let m2 = UnitModel(source: DemoUnitsSource(usb: false, bluetooth: false, remoteFM1: true, remoteFM1Bluetooth: true,
                                                   bulkMs: 0...0.2), emulator: host2,
                           backups: tmp.appendingPathComponent("fizzshots-pb-\(UUID().uuidString)"),
                           projects: tmp.appendingPathComponent("fizzshots-pbp-\(UUID().uuidString)"),
                           presets: tmp.appendingPathComponent("fizzshots-pbk-\(UUID().uuidString)"))
        host2.launch()
        m2.start()
        await until { m2.onEmulator && m2.dumpsLoaded && m2.pickerOpen }
        for _ in 0..<4 { await until { m2.panelRows.count == 2 && m2.panelRows.allSatisfy { $0.version != nil } } }
        host2.play.theme = .tangerine
        host2.play.showNow()
        save(UnitPickerShot(model: m2, tab: .play), phone, out, "panel-picker-bluetooth")
        m2.pickerClosed()
        m2.stop()
        await host2.close()
    }
}
#endif
