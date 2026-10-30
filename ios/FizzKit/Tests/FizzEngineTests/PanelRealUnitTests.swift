// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// A real RemoteFM1 unit as the panel of the emulated FIZZ on this Mac: OFF unless FIZZ_PANEL_PORT is set. Every CoreMIDI
// port pair whose name holds it is asked HELLO (RemoteFM1's own; FIZZ and the updater ignore it); exactly one must
// answer as RemoteFM1 v2, and that one is claimed for FIZZ_PANEL_SECONDS (30), FIZZ's sound on the Mac's output, then
// let go (SESSION 0). Nothing else is sent to any unit.
//
//   cd ios/FizzKit && FIZZ_PANEL_PORT="FM-1" FIZZ_PANEL_SECONDS=60 swift test --filter realUnitAsThePanel

import FizzKit
import Foundation
import Testing
@testable import FizzEngine

extension EmulatorTests {
    static var realPanelPort: String? { ProcessInfo.processInfo.environment["FIZZ_PANEL_PORT"] }

    @Test(.enabled(if: realPanelPort != nil)) func realUnitAsThePanel() async throws {
        #if canImport(CoreMIDI)
        let want = try #require(Self.realPanelPort)
        let secs = Double(ProcessInfo.processInfo.environment["FIZZ_PANEL_SECONDS"] ?? "") ?? 30
        let scanner = try MIDIPortScanner(clientName: "FIZZ panel test")
        let cands = scanner.candidates().filter { $0.name.localizedCaseInsensitiveContains(want) }
        print("panel test: ports \(cands.map(\.name))")
        var found: [(MIDIPortCandidate, RemoteFM1Hello)] = []
        for c in cands {
            let t = try scanner.open(c)
            let h = await RemoteFM1Unit.hello(on: t, timeoutMs: 500)
            t.close()
            print("panel test: \(c.name): \(h.map { "\($0.name) proto \($0.proto) fw \($0.fwVersion) caps 0x\(String($0.caps, radix: 16)) session \($0.inSession) ble \($0.bluetooth)" } ?? "no HELLO_R")")
            if let h, h.isPanel { found.append((c, h)) }
        }
        try #require(found.count == 1, "exactly one RemoteFM1 unit among \(cands.map(\.name))")
        let e = FizzEmulator(storage: nil)
        e.boot(flash: nil)
        print("panel test: FIZZ's clock: \(e.start(sound: true))")
        defer { e.stop() }
        let link = PanelLink(transport: try scanner.open(found[0].0, channelMessages: true), emulator: e)
        link.log = { print($0) }
        link.claim()
        #expect(await Self.until(5) { link.state == .active })
        let end = Date().addingTimeInterval(secs)
        while Date() < end, link.state == .active {
            try await Task.sleep(for: .seconds(5))
            print("panel test: \(link.stats); FIZZ \(e.uiInfo())")
        }
        print("panel test: state \(link.state)")
        link.release()
        _ = await Self.until(2) { if case .released = link.state { true } else { false } }
        print("panel test: \(link.state)")
        #endif
    }
}
