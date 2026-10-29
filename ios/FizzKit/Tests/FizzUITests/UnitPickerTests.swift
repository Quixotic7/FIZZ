// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// The unit picker (mock-ups 34-36): THIS iPHONE picked by hand while an FM-1 is plugged in (the FM-1 kept beside it),
// the FM-1 picked back, the pin's rules (a new unit ends it, the picked FM-1 gone ends it), SEND between the two,
// PLAY's SOUND following a pick from PLAY, a silent Bluetooth FM-1 greyed. Demo units stand in for both.

import Foundation
import Testing
import FizzKit
@testable import FizzUI

@MainActor
private func until(seconds: Double = 12, _ ok: () -> Bool) async -> Bool {
    let end = Date().addingTimeInterval(seconds)
    while Date() < end {
        if ok() { return true }
        try? await Task.sleep(for: .milliseconds(20))
    }
    return ok()
}

@Suite("Unit picker", .serialized) @MainActor
struct UnitPickerTests {
    static func model(_ src: DemoUnitsSource, host: EmulatorHost? = nil) -> UnitModel {
        let d = FileManager.default.temporaryDirectory.appendingPathComponent("fizz-units-\(UUID().uuidString)")
        let emu = PlugSource(.fizz(port: "FIZZ (this iPhone)", bulkMs: 0...0.2), plugged: true)
        return UnitModel(source: src, emulator: host, fallback: host == nil ? emu : nil,
                         backups: d.appendingPathComponent("Backups"), projects: d.appendingPathComponent("Projects"),
                         presets: d.appendingPathComponent("Presets"))
    }

    @Test func thisIPhonePickedKeepsTheFM1Beside() async {
        let src = DemoUnitsSource(bluetooth: false, bulkMs: 0...0.2)
        let m = Self.model(src)
        m.start()
        // a plugged-in FM-1 takes over by itself (no pin)
        #expect(await until { m.unitKey == .port("FIZZ FM-1") && m.dumpsLoaded })
        #expect(m.pinned == nil)
        #expect(m.unitRows.map(\.key) == [.here, .port("FIZZ FM-1")])
        #expect(m.unitRows.first { $0.inUse }?.key == .port("FIZZ FM-1"))
        #expect(await until { m.other?.key == .here && m.other?.taken != nil }, "THIS iPHONE beside, its MY PRESETS read")

        // THIS iPHONE picked: the manager switches, the FM-1 stays open beside it
        #expect(await m.pick(.here))
        #expect(m.onEmulator && m.pinned == .here)
        #expect(await until { m.dumpsLoaded && m.other?.key == .port("FIZZ FM-1") && m.other?.taken != nil })
        let rows = m.unitRows
        #expect(rows.count == 2 && rows[0].inUse && rows[0].pinned)
        #expect(rows[1].key == .port("FIZZ FM-1") && rows[1].answering && !rows[1].inUse && rows[1].via == .usb)
        #expect(m.other?.freeSlot == 144, "the demo FM-1's 143 presets: 144 is free")

        // the ports looked at again: the known FM-1 does not take over
        src.poke()
        try? await Task.sleep(for: .milliseconds(1500))
        #expect(m.onEmulator && m.pinned == .here)
        m.stop()
    }

    @Test func sendBetweenTheUnits() async {
        let src = DemoUnitsSource(bluetooth: false, bulkMs: 0...0.2)
        let m = Self.model(src)
        m.start()
        #expect(await until { m.unitKey == .port("FIZZ FM-1") && m.dumpsLoaded })
        #expect(await m.pick(.here))
        #expect(await until { m.dumpsLoaded && m.other?.freeSlot != nil })

        // THIS iPHONE -> FM-1: lands in the FM-1's first free place
        let glass = m.mine.first { $0.name == "GLASS BELL" }!
        let out = await m.sendToOther(rows: [glass])
        #expect(out == [.placed(144)])
        #expect(m.other?.taken?.contains(144) == true && m.other?.freeSlot == 145)
        #expect(!m.sendingOther && m.onEmulator, "nothing changes here")

        // the FM-1 picked back: it holds the sound; THIS iPHONE is the other one now
        #expect(await m.pick(.port("FIZZ FM-1")))
        #expect(await until { m.dumpsLoaded && m.mine.contains { $0.slot == 144 && $0.name == "GLASS BELL" } })
        #expect(m.pinned == .port("FIZZ FM-1"))
        #expect(await until { m.other?.key == .here && m.other?.freeSlot == 144 })

        // FM-1 -> THIS iPHONE, two at once
        let two = Array(m.mine.prefix(2))
        let back = await m.sendToOther(rows: two)
        #expect(back == [.placed(144), .placed(145)])
        #expect(m.other?.freeSlot == 146)
        m.stop()
    }

    @Test func aNewUnitEndsThePick() async {
        let src = DemoUnitsSource(bluetooth: false, bulkMs: 0...0.2)
        let m = Self.model(src)
        m.start()
        #expect(await until { m.unitKey == .port("FIZZ FM-1") && m.dumpsLoaded })
        #expect(await m.pick(.here))
        #expect(await until { m.other?.key == .port("FIZZ FM-1") })
        // the FM-1 pulled out: nothing beside THIS iPHONE
        src.setUSB(false)
        #expect(await until { m.other == nil })
        #expect(m.onEmulator)
        // plugged in again: a new unit, the pick by hand gives way and it takes over
        src.setUSB(true)
        #expect(await until { m.unitKey == .port("FIZZ FM-1") && m.dumpsLoaded })
        #expect(m.pinned == nil)
        m.stop()
    }

    @Test func thePickedFM1GoneFallsBackToThisIPhone() async {
        let src = DemoUnitsSource(bluetooth: false, bulkMs: 0...0.2)
        let m = Self.model(src)
        m.start()
        #expect(await until { m.unitKey == .port("FIZZ FM-1") && m.dumpsLoaded })
        #expect(await m.pick(.port("FIZZ FM-1")), "the one in use: pinned")
        #expect(m.pinned == .port("FIZZ FM-1"))
        src.setUSB(false)
        #expect(await until { m.onEmulator && m.dumpsLoaded })
        #expect(m.pinned == nil)
        m.stop()
    }

    @Test func aPickFromPlayMovesItsSound() async {
        let src = DemoUnitsSource(bluetooth: false, bulkMs: 0...0.2)
        let host = EmulatorHost(engine: FakeEngine(), source: PlugSource(.fizz(port: "FIZZ (this iPhone)", bulkMs: 0...0.2),
                                                                         plugged: true))
        host.launch()
        let m = Self.model(src, host: host)
        m.start()
        #expect(await until { m.unitKey == .port("FIZZ FM-1") && m.dumpsLoaded })
        m.openPicker(fromPlay: true)
        #expect(await m.pick(.here))
        #expect(host.play.sound == .here)
        #expect(await m.pick(.port("FIZZ FM-1")))
        #expect(host.play.sound == .fm1 && host.play.soundNow == .fm1)
        m.pickerClosed()
        // (not from PLAY: its SOUND left as it is)
        #expect(await m.pick(.here))
        #expect(host.play.sound == .fm1 && host.play.soundNow == .here, "no FM-1 the unit: heard here")
        m.stop()
    }

    @Test func aSilentBluetoothFM1IsGreyed() async {
        let src = DemoUnitsSource(bluetooth: true, bulkMs: 0...0.2)
        let m = Self.model(src)
        m.start()
        #expect(await until { m.unitKey == .port("FIZZ FM-1") && m.dumpsLoaded })
        m.openPicker()
        #expect(await until(seconds: 20) { m.seen.contains { $0.name == DemoUnitsSource.silentName } })
        let bt = m.unitRows.first { $0.key == .port(DemoUnitsSource.silentName) }
        #expect(bt?.answering == false && bt?.via == .bluetooth && bt?.inUse == false)
        // picked: no FIZZ answers, nothing changes
        #expect(!(await m.pick(.port(DemoUnitsSource.silentName))))
        #expect(m.unitKey == .port("FIZZ FM-1"))
        m.pickerClosed()
        m.stop()
    }
}
