// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// Mock-up 57: a RemoteFM1 unit plugged in beside THIS iPHONE (the demo's, libremotefm1's fake answering the update
// handshake as RemoteFM1 0.4.2) raises no "not FIZZ" notice; the picker opens by itself once with its row (USE AS
// PANEL), and again only after it was pulled out and plugged in again. (A fake engine: no core runs here.)

import Foundation
import Testing
import FizzEngine
import FizzKit
@testable import FizzUI

@MainActor
struct PanelOfferTests {
    func until(seconds: Double = 15, _ ok: () -> Bool) async -> Bool {
        let end = Date().addingTimeInterval(seconds)
        while Date() < end {
            if ok() { return true }
            try? await Task.sleep(for: .milliseconds(20))
        }
        return ok()
    }

    @Test func aRemoteFM1UnitIsOfferedInTheNoticesPlace() async {
        let src = DemoUnitsSource(usb: false, bluetooth: false, remoteFM1: true, bulkMs: 0...0.2)
        let host = EmulatorHost(engine: FakeEngine(), source: PlugSource(.fizz(port: "FIZZ (this iPhone)", bulkMs: 0...0.2),
                                                                         plugged: true))
        host.launch()
        let d = FileManager.default.temporaryDirectory.appendingPathComponent("fizz-panel-offer-\(UUID().uuidString)")
        let m = UnitModel(source: src, emulator: host, backups: d.appendingPathComponent("Backups"),
                          projects: d.appendingPathComponent("Projects"), presets: d.appendingPathComponent("Presets"))
        m.start()
        #expect(await until { m.onEmulator && m.pickerOpen })
        #expect(m.notice == nil && !m.noticeOpen, "no not-FIZZ notice")
        #expect(m.panelUnits.map(\.name) == [DemoUnitsSource.panelName])
        // the picker's survey asked HELLO: its row, USE AS PANEL with RemoteFM1's version
        #expect(await until { m.panelUnits.first?.hello != nil })
        #expect(m.panelRows == [PanelRow(name: DemoUnitsSource.panelName, via: .usb, version: DemoRemoteFM1Port.version,
                                         state: .offer)])
        #expect(m.unitRows.map(\.key) == [.here], "THIS iPHONE stays the unit; no greyed row for it")

        // closed: not offered again while it stays plugged in
        m.pickerClosed()
        src.poke()
        try? await Task.sleep(for: .milliseconds(800))
        #expect(!m.pickerOpen && m.notice == nil)

        // pulled out and plugged in again: offered again
        src.setRemoteFM1(false)
        #expect(await until { m.panelUnits.isEmpty })
        src.setRemoteFM1(true)
        #expect(await until { m.pickerOpen })
        #expect(m.notice == nil)
        m.pickerClosed()
        m.stop()
    }

    @Test func versionFromTheUpdateIdentity() {
        #expect(PanelUnit(name: "a", identity: "FM-1_52000402").version == "0.4.2")
        #expect(PanelUnit(name: "a", identity: "FM-1_52011203").version == "1.12.3")
        #expect(PanelUnit(name: "a", identity: "FM-1_5200").version == nil)
        #expect(PanelUnit(name: "a").version == nil)
    }
}
