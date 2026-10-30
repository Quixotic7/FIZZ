// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// Mock-up 57 on the emulated FIZZ and libremotefm1's fake unit: the picker's RemoteFM1 rows (USE AS PANEL, in another
// host's session, over Bluetooth, PANEL), USE AS PANEL and ×, the toasts (linked once; the cable pulled; none for ×),
// and ▶ a backup letting the panel go. (In the Emulator suite: the core is the process's one.)

import CRemoteFM1
import FizzKit
import Foundation
import Testing
@testable import FizzEngine
@testable import FizzUI

/// One port whose fake RemoteFM1 unit is made by `make` at each open (another host's session, Bluetooth).
final class OnePanelSource: LinkSource, MIDIPortScanning, @unchecked Sendable {
    let name: String
    let make: () -> FakeRemoteFM1
    let changes: AsyncStream<Void>
    var scanner: any MIDIPortScanning { self }
    let clock: any FizzClock = SystemClock()

    init(name: String, make: @escaping () -> FakeRemoteFM1) {
        self.name = name
        self.make = make
        changes = AsyncStream { _ in }
    }

    func candidates() -> [MIDIPortCandidate] {
        [MIDIPortCandidate(source: MIDIEndpointInfo(ref: 81, name: name), destination: MIDIEndpointInfo(ref: 82, name: name))]
    }
    func open(_ c: MIDIPortCandidate) throws -> any MIDITransport { make() }
}

extension EmulatorTests {
    @MainActor static func panelModel(_ src: any LinkSource) async -> (UnitModel, EmulatorHost, FizzEmulator) {
        let d = FileManager.default.temporaryDirectory.appendingPathComponent("fizz-panel-ui-\(UUID().uuidString)")
        let e = FizzEmulator(storage: nil)
        let host = EmulatorHost(emulator: e)
        let m = UnitModel(source: src, emulator: host, backups: d.appendingPathComponent("Backups"),
                          projects: d.appendingPathComponent("Projects"), presets: d.appendingPathComponent("Presets"))
        host.launch()
        _ = await onMain { host.booted }
        await host.settle()
        return (m, host, e)
    }

    @MainActor @Test func panelRowsUseAndRelease() async throws {
        let src = PanelPortSource()
        let (m, host, _) = await Self.panelModel(src)
        m.watchPanel()

        // the picker's row: USE AS PANEL, the fake's version
        await m.survey()
        #expect(m.panelRows == [PanelRow(name: src.name, via: .usb, version: "fake", state: .offer)])
        #expect(!m.unitRows.contains { $0.key == .port(src.name) }, "no greyed FM-1 row for it")
        #expect(UnitPicker.height(m) == UnitPicker.height(rows: m.unitRows.count) + PanelPickCard.height + 12)

        // USE AS PANEL: claimed, then linked; PLAY's sound stays here; the toast once
        host.play.sound = .fm1
        #expect(await m.usePanelTapped(src.name))
        #expect(host.play.sound == .here)
        #expect(m.panelAttached)
        #expect(await Self.onMain { m.panelRows.first?.state == .attached })
        #expect(await Self.onMain { m.panelToast?.text == PanelToast.linked.text })
        #expect(m.panelToast?.ok == true)

        // ×: let go, no toast for it; the row offers again
        m.panelToast = nil
        m.stopPanel()
        #expect(await Self.onMain { host.panel == .released(.byUser) })
        try await Task.sleep(for: .milliseconds(100))
        #expect(m.panelToast == nil, "let go by hand: nothing to say")
        #expect(!m.panelAttached && m.panelRows.map(\.state) == [.offer])

        // again, and the cable pulled: the quiet toast, the row no longer attached
        #expect(await m.usePanelTapped(src.name))
        #expect(await Self.onMain { host.panel == .active })
        let unit = try #require(src.opened.last)
        unit.unplug()
        #expect(await Self.onMain { host.panel == .released(.lost) })
        #expect(await Self.onMain { m.panelToast?.text == PanelToast.released(.lost)?.text })
        #expect(m.panelToast?.ok == false && !m.panelAttached)
        #expect(m.panelRows.allSatisfy { $0.state != .attached && $0.state != .linking })
        m.stop()
        await host.close()
    }

    @MainActor @Test func panelRowsBusyAndBluetooth() async throws {
        // another host's session on it: the row says so (USE AS PANEL still there)
        let busy = OnePanelSource(name: "FM-1 4410") {
            let u = FakeRemoteFM1(name: "FM-1 4410")
            u.inject(RFM_SESSION, [1, 0xB8, 0x0B])
            return u
        }
        let (m, host, _) = await Self.panelModel(busy)
        await m.survey()
        #expect(m.panelRows.map(\.state) == [.busy])
        m.stop()
        await host.close()

        // over Bluetooth: shown, USB only for the panel
        let ble = OnePanelSource(name: "FM-1 7938 Bluetooth") { FakeRemoteFM1(name: "FM-1 7938 Bluetooth") { $0.ble = 1 } }
        let (m2, host2, _) = await Self.panelModel(ble)
        await m2.survey()
        #expect(m2.panelRows.map(\.state) == [.bluetooth] && m2.panelRows.first?.via == .bluetooth)
        m2.stop()
        await host2.close()
    }

    @MainActor @Test func aBackupPlayedLetsThePanelGo() async throws {
        let src = PanelPortSource()
        let (m, host, e) = await Self.panelModel(src)
        m.watchPanel()
        await m.survey()
        #expect(await m.usePanelTapped(src.name))
        #expect(await Self.onMain { host.panel == .active })

        // ▶: the panel lets go and says why
        await host.playBackup(flash: [UInt8](repeating: 0xFF, count: e.info.flashSize), name: "blank.fizzbak", date: Date())
        #expect(host.backup != nil)
        #expect(await Self.onMain { host.panel == .released(.byUser) })
        #expect(await Self.onMain { m.panelToast?.text == PanelToast.backup.text })
        #expect(!m.panelAttached)

        // USE AS PANEL again: the backup put away first, the panel on THIS iPHONE
        #expect(await m.usePanelTapped(src.name))
        #expect(host.backup == nil && e.isBooted)
        #expect(await Self.onMain { host.panel == .active })
        m.stopPanel()
        #expect(await Self.onMain { host.panel == .released(.byUser) })
        m.stop()
        await host.close()
    }
}
