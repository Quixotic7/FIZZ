// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// UnitModel against the demo unit: connect reads it all, the update / not FIZZ / two units states, the refresh rule.

import Foundation
import Testing
import FizzKit
@testable import FizzUI

@MainActor
private func waitFor(_ m: UnitModel, seconds: Double = 10, _ ok: () -> Bool) async -> Bool {
    let end = Date().addingTimeInterval(seconds)
    while Date() < end {
        if ok() { return true }
        try? await Task.sleep(for: .milliseconds(20))
    }
    return ok()
}

@Suite("UnitModel", .serialized) @MainActor
struct UnitModelTests {
    @Test func usbTakenOverBluetooth() {
        let pick = UnitModel.usbOverBluetooth
        #expect(pick([("FM-1", true), ("FM-1 C1F0 Bluetooth", true)]) == 0)
        #expect(pick([("FM-1 C1F0 Bluetooth", true), ("FIZZ FM-1", true)]) == 1)
        #expect(pick([("FM-1", true), ("FM-1 Port 2", true)]) == nil, "two on USB: the picker")
        #expect(pick([("FM-1 C1F0 Bluetooth", true), ("FM-1 A2B3 Bluetooth", true)]) == nil, "two over Bluetooth: the picker")
        #expect(pick([("FM-1", false), ("FM-1 C1F0 Bluetooth", true)]) == nil, "the USB one not FIZZ: the picker")
    }

    @Test func connectsAndReadsTheDemoUnit() async {
        let m = UnitModel(source: DemoLinkSource(.fizz))
        m.start()
        #expect(await waitFor(m) { m.dumpsLoaded })
        guard case .found(let u) = m.state else { Issue.record("state \(m.state.kind)"); return }
        #expect(u.version == "0.12.0-beta.11")
        #expect(u.idText == "FM-1_46001100")
        #expect(m.mine.count == 143)
        #expect(m.factory.count == 23)
        #expect(m.pool.count == 19)
        #expect(m.projects?.list.count == 8)
        #expect(m.projects?.list.filter(\.there).count == 6)
        #expect(m.projects?.list.contains { $0.demo } == true)
        #expect(m.projects?.current == 1)
        #expect(m.status?.mine == 143)
        #expect(m.storeInfo?.size == 299_008)
        #expect(m.storeUse?.percent == 60)
        #expect(m.mine.first?.name == "SUB DROP")
        // the glyphs: the algorithm from each DUMP's blob
        #expect(m.algos.count == 143 + 23)
        #expect(m.algorithm(m.mine[1]) == 2)
        #expect(m.mine.filter(\.inPool).count == 17)
        m.stop()
    }

    @Test func updateMode() async {
        let m = UnitModel(source: DemoLinkSource(.update))
        m.start()
        #expect(await waitFor(m) { m.state.kind == "update" })
        if case .updateMode(let id) = m.state { #expect(id.lowercased().hasPrefix("ota-")) }
        m.stop()
    }

    @Test func notFizz() async {
        let m = UnitModel(source: DemoLinkSource(.notfizz))
        m.start()
        #expect(await waitFor(m) { m.state.kind == "notfizz" })
        if case .notFizz(let id) = m.state { #expect(id == "FM-1_016") }
        m.stop()
    }

    @Test func twoUnitsThenPickOne() async {
        let m = UnitModel(source: DemoLinkSource(.two))
        m.start()
        #expect(await waitFor(m) { m.state.kind == "choose" })
        #expect(m.choices.count == 2)
        #expect(m.choices.first?.isFizz == true && m.choices.first?.detail == "0.12.0-beta.11")
        #expect(m.choices.last?.name == "ChoralRoot FM-1" && m.choices.last?.isFizz == false)
        m.choose(0)
        #expect(await waitFor(m) { m.dumpsLoaded })
        #expect(m.state.kind == "found")
        #expect(m.mine.count == 143)
        m.stop()
    }

    @Test func lostThenFoundAgain() async {
        let src = DemoLinkSource(.fizz)
        let m = UnitModel(source: src)
        m.start()
        #expect(await waitFor(m) { m.dumpsLoaded })
        src.demo.unplugAll()
        #expect(await waitFor(m) { m.state.kind == "lost" })
        #expect(m.rows.isEmpty)
        #expect(await waitFor(m) { m.state.kind == "found" })
        m.stop()
    }

    @Test func refreshRule() async {
        func st(written: Int = 10, mine: Int = 143, pool: Int = 19, project: Int = 1, projects: Int = 6,
                dirty: Bool = false, noStore: Bool = false, playing: Bool = false, pattern: Int = 0) -> FizzStatus {
            // (FizzStatus has no public init: made from a STATUS frame, as the unit sends it)
            let flags = (playing ? 1 : 0) | (noStore ? 2 : 0) | (dirty ? 4 : 0)
            let body = [1, 6, flags, project - 1, pattern, projects] + DemoReplies.r14(mine) + DemoReplies.r14(pool) +
                DemoReplies.r21(written) + DemoReplies.r14(1) + DemoReplies.r14(2) + DemoReplies.r14(3) +
                DemoReplies.r14(4) + [0, 0]
            return FizzSysEx.parseStatus(DemoReplies.frame(FizzSysEx.REPLY.status, body))!
        }
        let a = st()
        #expect(!UnitModel.changed(a, st()))
        #expect(!UnitModel.changed(a, st(playing: true, pattern: 5)))      // playing / the pattern: not a change
        #expect(UnitModel.changed(a, st(written: 11)))
        #expect(UnitModel.changed(a, st(mine: 144)))
        #expect(UnitModel.changed(a, st(pool: 18)))
        #expect(UnitModel.changed(a, st(project: 2)))
        #expect(UnitModel.changed(a, st(projects: 7)))
        #expect(UnitModel.changed(a, st(dirty: true)))
        #expect(UnitModel.changed(a, st(noStore: true)))
        #expect(UnitModel.changed(nil, a))

        // a quick refresh on an unchanged unit reads only STATUS; a full one reads everything
        let m = UnitModel(source: DemoLinkSource(.fizz))
        m.start()
        #expect(await waitFor(m) { m.dumpsLoaded })
        #expect(await m.refresh(quick: true) == false)
        #expect(await m.refresh() == true)
        #expect(m.mine.count == 143)
        m.stop()
    }

    @Test func demoFramesParse() {
        let u = DemoUnit.fizz()
        let f = DemoReplies.dump(u.mine[1])
        let d = FizzSysEx.parseDump(f)
        #expect(d?.name == "GLASS BELL")
        #expect(d?.slot == 2)
        #expect(d?.six == true)
        #expect(d?.blob[2] == 1)
        #expect(d?.tags == 4)
        #expect(versionParts("0.12.0-beta.11") == ("0.12.0", "BETA 11"))
        #expect(versionParts("0.11.0") == ("0.11.0", nil))
    }
}
