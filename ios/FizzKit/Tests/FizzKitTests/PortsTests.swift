// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.

import Testing
@testable import FizzKit

@Suite("Ports")
struct PortsTests {
    func ep(_ ref: UInt32, _ name: String, entity: UInt32 = 0) -> MIDIEndpointInfo {
        MIDIEndpointInfo(ref: ref, name: name, entity: entity, device: entity)
    }

    @Test func fmNamesFirst() {
        let src = [ep(1, "Elektron Digitone", entity: 10), ep(2, "FM-1", entity: 20)]
        let dst = [ep(3, "Elektron Digitone", entity: 10), ep(4, "FM-1", entity: 20)]
        #expect(MIDIPorts.candidates(sources: src, destinations: dst).map(\.name) == ["FM-1"])
    }

    @Test func genericNamesProbedWhenNothingMatches() {
        // a host that names the USB device's port generically: probe it, never the host's own buses
        let src = [ep(1, "Network Session 1", entity: 10), ep(2, "Port 1", entity: 20), ep(5, "IAC Driver Bus 1", entity: 30)]
        let dst = [ep(3, "Network Session 1", entity: 10), ep(4, "Port 1", entity: 20), ep(6, "IAC Driver Bus 1", entity: 30)]
        let c = MIDIPorts.candidates(sources: src, destinations: dst)
        #expect(c.map(\.name) == ["Port 1"])
        #expect(c.first?.destination.ref == 4)
    }

    @Test func nothingAtAll() {
        #expect(MIDIPorts.candidates(sources: [], destinations: []).isEmpty)
    }
}
