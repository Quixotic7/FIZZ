// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// The emulated FIZZ as the app's unit, end to end: UnitModel with no FM-1 connects to EmulatorHost's port and reads
// it as it reads an FM-1; RESET wipes it. (In the Emulator suite: the core is the process's one.)

import Foundation
import Testing
import FizzKit
@testable import FizzEngine
@testable import FizzUI

extension EmulatorTests {
    @MainActor @Test func theAppsUnitWithNoFM1() async throws {
        let d = FileManager.default.temporaryDirectory.appendingPathComponent("fizz-emu-unit-\(UUID().uuidString)")
        let e = FizzEmulator(storage: nil)
        let host = EmulatorHost(emulator: e)
        let m = UnitModel(source: NoPorts(), emulator: host, backups: d.appendingPathComponent("Backups"),
                          projects: d.appendingPathComponent("Projects"))
        host.launch()
        m.start()
        let end = Date().addingTimeInterval(15)
        while !(m.onEmulator && m.dumpsLoaded) && Date() < end { try await Task.sleep(for: .milliseconds(20)) }
        #expect(m.onEmulator && m.dumpsLoaded)
        #expect(host.serving && host.mode == .timer)
        #expect(m.unit?.version == e.info.version)
        #expect(m.factory.count == 23 && m.mine.isEmpty)
        #expect(m.status != nil)
        // PLAY shown: heard (or the timer, with no audio device)
        host.playShown = true
        await host.settle()
        try await Task.sleep(for: .milliseconds(50))
        #expect(host.mode != .stopped)
        host.playShown = false
        m.stop()
        await host.close()
    }
}
