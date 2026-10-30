// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// RemoteFM1 units as UnitModel sees them: FM-1s running RemoteFM1's firmware, which can be THIS iPHONE's panel
// (EmulatorHost.attachPanel, PanelLink). Data only: the picker's "USE AS PANEL" and the chip wait for mock-up 57.
//   - The scan finds them passively: 0.2 and later answer the update handshake it sends every FM-1 port anyway as
//     "FM-1_52.." (RemoteFM1Unit.isRemoteFM1); nothing more is sent.
//   - The picker's survey asks each one HELLO (RemoteFM1's own, harmless: the version, a session of another host), and
//     asks HELLO of the ports silent to the handshake and to FIZZ (a unit over Bluetooth, where the updater does not
//     listen).
//   - Nothing is claimed by itself: usePanel(_:) does it, from the user's tap. The panel's port is left out of the
//     scans while it is attached.

import FizzEngine
import FizzKit
import Foundation

/// A RemoteFM1 unit seen.
public struct PanelUnit: Sendable, Equatable, Identifiable {
    /// its port
    public let name: String
    public var id: String { name }
    /// what it answered the update handshake ("FM-1_52000203"); nil: found by HELLO alone
    public var identity: String?
    /// its HELLO_R (nil: not asked yet, or no answer)
    public var hello: RemoteFM1Hello?
    public var via: UnitRow.Via { (hello?.bluetooth ?? false) || FizzLink.isBlePort(name) ? .bluetooth : .usb }
    /// another host has a session on it (as HELLO last said)
    public var busy: Bool { hello?.inSession == true }
}

extension UnitModel {
    /// The panel's port, left out of the scans while it is attached.
    var panelSkip: Set<String> { Set([emulator?.panelPort].compactMap { $0 }) }

    /// The scan's answers: the RemoteFM1 ones (by their identity) are the units seen; what HELLO said of them kept.
    /// The one attached as the panel stays (it is not scanned), and so do the ones HELLO found while their port is there.
    func notePanelUnits(_ probes: [FizzProbe]) {
        let names = Set(source.scanner.candidates().map(\.name))
        var out: [PanelUnit] = []
        for p in probes where RemoteFM1Unit.isRemoteFM1(identity: p.identity.text) {
            var u = panelUnits.first { $0.name == p.candidate.name } ?? PanelUnit(name: p.candidate.name)
            u.identity = p.identity.text
            out.append(u)
        }
        for u in panelUnits where !out.contains(where: { $0.name == u.name })
            && (panelSkip.contains(u.name) || (u.identity == nil && names.contains(u.name))) {
            out.append(u)
        }
        if out != panelUnits { panelUnits = out }
    }

    /// The survey's RemoteFM1 units, HELLO asked; the attached one kept.
    func surveyed(_ found: [PanelUnit]) {
        var out = found
        for u in panelUnits where panelSkip.contains(u.name) && !out.contains(where: { $0.name == u.name }) {
            out.append(u)
        }
        if out != panelUnits { panelUnits = out }
    }

    /// One port asked HELLO (a transport of its own, closed after).
    func panelUnit(_ c: MIDIPortCandidate, identity: String?) async -> PanelUnit {
        var u = PanelUnit(name: c.name, identity: identity)
        if let t = try? source.scanner.open(c) {
            u.hello = await RemoteFM1Unit.hello(on: t, timeoutMs: 300)
            t.close()
        }
        return u
    }

    /// Several ports asked HELLO at once.
    func helloAll(_ cands: [MIDIPortCandidate]) async -> [PanelUnit] {
        await withTaskGroup(of: PanelUnit.self) { g in
            for c in cands { g.addTask { await self.panelUnit(c, identity: nil) } }
            var out: [PanelUnit] = []
            for await u in g { out.append(u) }
            return cands.compactMap { c in out.first { $0.name == c.name } }
        }
    }

    /// Claim the RemoteFM1 unit on port `name` as THIS iPHONE's panel (the user's tap): false when the port is not
    /// there or does not open. emulator.panel says how it goes.
    @discardableResult
    public func usePanel(_ name: String) -> Bool {
        guard let host = emulator, let c = source.scanner.candidates().first(where: { $0.name == name }),
              let t = try? source.scanner.open(c, channelMessages: true) else { return false }
        say("\(name): claimed as \(EmulatorHost.hereName)'s panel")
        host.attachPanel(t)
        return true
    }

    /// Let the panel go (the unit back to its waiting screen).
    public func stopPanel() { emulator?.detachPanel() }
}
