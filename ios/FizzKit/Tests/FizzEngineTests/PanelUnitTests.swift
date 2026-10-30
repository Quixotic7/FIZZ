// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// A RemoteFM1 unit as UnitModel and EmulatorHost see it: the picker's survey finds it by HELLO (FIZZ's handshake and
// IDENTITY leave it as it was), nothing claims it by itself, usePanel claims it, the clock runs while it is attached,
// stopPanel lets it go. (In the Emulator suite: the core is the process's one.)

import CRemoteFM1
import FizzKit
import Foundation
import Testing
@testable import FizzEngine
@testable import FizzUI

/// One port with a fake RemoteFM1 unit behind it (Bluetooth-like: nothing but RemoteFM1 answers); each open a new one.
final class PanelPortSource: LinkSource, MIDIPortScanning, @unchecked Sendable {
    let name = "FM-1 7938"
    let changes: AsyncStream<Void>
    private let changed: AsyncStream<Void>.Continuation
    var scanner: any MIDIPortScanning { self }
    let clock: any FizzClock = SystemClock()
    private let lock = NSLock()
    private var units: [FakeRemoteFM1] = []
    var opened: [FakeRemoteFM1] { lock.withLock { units } }

    init() { (changes, changed) = AsyncStream.makeStream(of: Void.self, bufferingPolicy: .bufferingNewest(1)) }

    func candidates() -> [MIDIPortCandidate] {
        [MIDIPortCandidate(source: MIDIEndpointInfo(ref: 71, name: name), destination: MIDIEndpointInfo(ref: 72, name: name))]
    }

    func open(_ c: MIDIPortCandidate) throws -> any MIDITransport {
        let u = FakeRemoteFM1(name: c.name)
        lock.withLock { units.append(u) }
        return u
    }
}

extension EmulatorTests {
    @MainActor static func onMain(_ s: Double = 5, _ f: () -> Bool) async -> Bool {
        let end = Date().addingTimeInterval(s)
        while !f() && Date() < end { try? await Task.sleep(for: .milliseconds(10)) }
        return f()
    }

    @MainActor @Test func aRemoteFM1UnitBecomesThisPhonesPanel() async throws {
        let d = FileManager.default.temporaryDirectory.appendingPathComponent("fizz-panel-unit-\(UUID().uuidString)")
        let e = FizzEmulator(storage: nil)
        let host = EmulatorHost(emulator: e)
        let src = PanelPortSource()
        let m = UnitModel(source: src, emulator: host, backups: d.appendingPathComponent("Backups"),
                          projects: d.appendingPathComponent("Projects"))
        host.launch()
        #expect(await Self.onMain { host.booted })
        await host.settle()

        await m.survey()
        let u = try #require(m.panelUnits.first)
        #expect(m.panelUnits.count == 1 && u.name == src.name && u.hello?.isPanel == true && !u.busy)
        #expect(!m.seen.contains { $0.name == src.name }, "a panel unit, not a silent FM-1")
        // what the survey sent (the handshake, FIZZ's IDENTITY, HELLO) started no session anywhere
        #expect(src.opened.allSatisfy { Self.fakeStats($0).sessions_started == 0 })
        #expect(host.panel == nil)

        #expect(m.usePanel(src.name))
        #expect(await Self.onMain { host.panel == .active })
        let unit = try #require(src.opened.last)
        #expect(unit.with { rfm_fake_session($0) } == 1)
        #expect(host.panelPort == src.name && m.panelSkip == [src.name])
        await host.settle()
        #expect(host.mode != .stopped, "the clock (and the sound) run while the panel is attached")
        #expect(await Self.onMain { Self.lcd(e) != nil && Self.shown(unit).contains { $0 != 0 } })
        // the attached unit is not probed again, and stays a panel unit
        await m.survey()
        #expect(m.panelUnits.map(\.name) == [src.name])

        m.stopPanel()
        #expect(await Self.onMain { host.panel == .released(.byUser) })
        #expect(host.panelPort == nil)
        #expect(unit.with { rfm_fake_session($0) } == 0)
        await host.settle()
        #expect(host.mode == .stopped, "nothing else asks for the clock")
        m.stop()
        await host.close()
    }
}
