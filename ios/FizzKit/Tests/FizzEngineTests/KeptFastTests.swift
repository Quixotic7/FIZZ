// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// A project kept on the phone loaded into THIS iPHONE (the emulated FIZZ) and written: near-instant, as nothing on
// that link is paced for the FM-1. (In the Emulator suite: the core is the process's one.)

import Foundation
import Testing
import FizzData
import FizzKit
@testable import FizzEngine
@testable import FizzUI

@MainActor
private func waitFor(seconds: Double = 30, _ ok: () -> Bool) async -> Bool {
    let end = Date().addingTimeInterval(seconds)
    while Date() < end {
        if ok() { return true }
        try? await Task.sleep(for: .milliseconds(10))
    }
    return ok()
}

private func tempDir() -> URL {
    let u = FileManager.default.temporaryDirectory.appendingPathComponent("fizz-kept-fast-\(UUID().uuidString)", isDirectory: true)
    try? FileManager.default.createDirectory(at: u, withIntermediateDirectories: true)
    return u
}

/// A 20-pattern .fizzprj: the demo FM-1's NOIR (18) with two of its patterns pasted on, kept.
@MainActor
private func twentyPatternProject() async throws -> (bytes: [UInt8], name: String) {
    let src = DemoLinkSource(.fizz, bulkMs: 0...0)
    let m = UnitModel(source: src, backups: tempDir(), projects: tempDir())
    m.start()
    _ = await waitFor { m.dumpsLoaded && m.layout != nil }
    let pm = m.projectsTab
    await pm.show()
    pm.select(0)
    pm.selectRange(from: 0, to: 1)
    await pm.copy()
    pm.tap(40)
    pm.paste()
    _ = await waitFor { pm.ops.count == 1 }
    await pm.keep()
    let k = try #require(pm.kept.items.first)
    #expect(k.info.pats == 20)
    let b = try #require(pm.kept.bytes(k))
    m.stop()
    return (b, k.name)
}

/// A port that only records when each frame left (on the link's clock).
private final class Recorder: MIDITransport, @unchecked Sendable {
    let name = "REC"
    let messages: AsyncStream<[UInt8]>
    private let cont: AsyncStream<[UInt8]>.Continuation
    private let lock = NSLock()
    private let clock: any FizzClock
    private let emulated: Bool
    private var times: [Double] = []
    init(clock: any FizzClock, emulated: Bool) {
        self.clock = clock
        self.emulated = emulated
        (messages, cont) = AsyncStream.makeStream(of: [UInt8].self)
    }
    var isLost: Bool { false }
    var isEmulated: Bool { emulated }
    var sent: [Double] { lock.withLock { times } }
    func send(_ bytes: [UInt8]) throws { let t = clock.now; lock.withLock { times.append(t) } }
    func close() { cont.finish() }
}

/// The FM-1's pacing as it was (GAP_MS between frames); none on the emulated FIZZ.
@Test func pacingOnlyForAnFM1() async throws {
    for emulated in [false, true] {
        let clock = SystemClock()
        let r = Recorder(clock: clock, emulated: emulated)
        let l = FizzLink(transport: r, clock: clock)
        #expect(l.isEmulated == emulated)
        let t0 = clock.now
        for k in 0..<40 { try await l.note(channel: 0, note: 60, velocity: k & 1 == 0 ? 100 : 0) }
        let ms = clock.now - t0, t = r.sent
        #expect(t.count == 40)
        if emulated {
            #expect(ms < 40 * FizzLink.gapMs, "\(ms) ms for 40 frames")
        } else {
            #expect(FizzLink.gapMs == 3)
            for i in 1..<t.count { #expect(t[i] - t[i - 1] >= FizzLink.gapMs - 0.001, "gap \(i): \(t[i] - t[i - 1])") }
        }
        await l.close()
    }
    // a port that says nothing (CoreMIDI's, a Bluetooth one): an FM-1's, paced
    #expect(Plain().isEmulated == false)
}

private final class Plain: MIDITransport, @unchecked Sendable {
    let name = "P"
    let messages = AsyncStream<[UInt8]> { $0.finish() }
    var isLost: Bool { false }
    func send(_ bytes: [UInt8]) throws {}
    func close() {}
}

extension EmulatorTests {
    @MainActor @Test func keptProjectLoadsIntoThisPhoneFast() async throws {
        let (bytes, name) = try await twentyPatternProject()
        let d = tempDir()
        let e = FizzEmulator(storage: nil)
        let host = EmulatorHost(emulator: e)
        let m = UnitModel(source: NoPorts(), emulator: host, backups: d.appendingPathComponent("Backups"),
                          projects: d.appendingPathComponent("Projects"))
        host.launch()
        m.start()
        #expect(await waitFor { m.onEmulator && m.dumpsLoaded && m.layout != nil })
        let pm = m.projectsTab
        _ = try await pm.kept.add(bytes, fileName: "\(name).fizzprj")

        let t0 = Date()
        await pm.show()                                     // (the store read into the model)
        let tRead = Date().timeIntervalSince(t0)
        let k = try #require(pm.kept.items.first)
        let q = try #require((0..<pm.nproj).first { !pm.slot($0).there })
        let t1 = Date()
        pm.startLoad(k)
        await pm.tapSlot(q)
        let tLoad = Date().timeIntervalSince(t1)
        #expect(pm.ops.count == 1 && pm.slot(q).name == name)
        let t2 = Date()
        await pm.write()
        let tWrite = Date().timeIntervalSince(t2)
        #expect(pm.ops.isEmpty && pm.note?.text == "1 CHANGE WRITTEN", "\(String(describing: pm.note))")
        #expect(m.actions.restoreFailure == nil)
        #expect(pm.slot(q).there && pm.slot(q).name == name && pm.slot(q).used == 20)
        print(String(format: "KEPT-FAST read %.3f s, LOAD INTO %.3f s, WRITE %.3f s, total %.3f s",
                     tRead, tLoad, tWrite, tRead + tLoad + tWrite))
        // (near-instant on a phone; generous here for a loaded Mac)
        #expect(tRead + tLoad + tWrite < 6, "read \(tRead) s, load \(tLoad) s, write \(tWrite) s")
        m.stop()
        await host.close()
    }
}
