// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// The app with no FM-1: UnitModel falls back to the emulated FIZZ's port and an FM-1 takes over when it comes (a demo
// unit stands in for each, so no core runs here); EmulatorHost's clock (a fake engine); PLAY's input mapping.

import CoreGraphics
import Foundation
import Testing
import FizzEngine
import FizzKit
@testable import FizzUI

@MainActor
private func waitFor(seconds: Double = 10, _ ok: () -> Bool) async -> Bool {
    let end = Date().addingTimeInterval(seconds)
    while Date() < end {
        if ok() { return true }
        try? await Task.sleep(for: .milliseconds(20))
    }
    return ok()
}

/// Ports that come and go: a demo unit plugged in or pulled out.
final class PlugSource: LinkSource, MIDIPortScanning, @unchecked Sendable {
    let inner: DemoScanner
    private let lock = NSLock()
    private var on: Bool
    let changes: AsyncStream<Void>
    private let changed: AsyncStream<Void>.Continuation
    var scanner: any MIDIPortScanning { self }
    let clock: any FizzClock = SystemClock()

    init(_ unit: DemoUnit, plugged: Bool) {
        inner = DemoScanner(units: [unit])
        on = plugged
        (changes, changed) = AsyncStream.makeStream(of: Void.self, bufferingPolicy: .bufferingNewest(1))
    }

    func candidates() -> [MIDIPortCandidate] { lock.withLock { on } ? inner.candidates() : [] }
    func open(_ c: MIDIPortCandidate) throws -> any MIDITransport { try inner.open(c) }

    func plug() { lock.withLock { on = true }; changed.yield() }
    func unplug() {
        lock.withLock { on = false }
        inner.unplugAll()
        changed.yield()
    }
}

/// The emulator's clock as asked for, with no core.
final class FakeEngine: EmulatorEngine, @unchecked Sendable {
    private let lock = NSLock()
    private var _booted = false, _mode = FizzEmulator.ClockMode.stopped, _playing = false
    private(set) var resets = 0
    var isBooted: Bool { lock.withLock { _booted } }
    var mode: FizzEmulator.ClockMode { lock.withLock { _mode } }
    var isPlaying: Bool {
        get { lock.withLock { _playing } }
        set { lock.withLock { _playing = newValue } }
    }
    func boot() { lock.withLock { _booted = true } }
    func start(sound: Bool) -> FizzEmulator.ClockMode { lock.withLock { _mode = sound ? .audio : .timer; return _mode } }
    func stop() { lock.withLock { _mode = .stopped } }
    func reset() { lock.withLock { resets += 1 } }
}

@Suite("Offline", .serialized) @MainActor
struct OfflineTests {
    static func dirs() -> (URL, URL) {
        let d = FileManager.default.temporaryDirectory.appendingPathComponent("fizz-offline-\(UUID().uuidString)")
        return (d.appendingPathComponent("Backups"), d.appendingPathComponent("Projects"))
    }

    @Test func noFM1ThenOneThenNone() async {
        let fm1 = PlugSource(.fizz(port: "FIZZ FM-1"), plugged: false)
        let emu = PlugSource(.fizz(port: "FIZZ (this iPhone)"), plugged: true)
        let (b, p) = Self.dirs()
        let m = UnitModel(source: fm1, fallback: emu, backups: b, projects: p)
        #expect(m.hasFallback)
        m.start()
        // no FM-1: the emulated FIZZ is the unit
        #expect(await waitFor { m.onEmulator && m.dumpsLoaded })
        #expect(m.unit?.portName == "FIZZ (this iPhone)")
        #expect(m.state.kind == "found" && m.notice == nil)
        // an FM-1 plugged in: it takes over, read as a connect reads it
        fm1.plug()
        #expect(await waitFor { m.unit?.portName == "FIZZ FM-1" && m.dumpsLoaded })
        #expect(!m.onEmulator && m.unit?.isEmulated == false)
        #expect(m.mine.count == 143 && m.status != nil)
        #expect(m.log.contains { $0.contains("takes over") })
        // pulled out: back to the emulated FIZZ
        fm1.unplug()
        #expect(await waitFor { m.onEmulator && m.dumpsLoaded })
        #expect(m.unit?.portName == "FIZZ (this iPhone)")
        m.stop()
    }

    @Test func anFM1InUpdateModeLeavesTheEmulatorInPlace() async {
        let fm1 = PlugSource(.updateMode(), plugged: true)
        let emu = PlugSource(.fizz(port: "FIZZ (this iPhone)"), plugged: true)
        let (b, p) = Self.dirs()
        let m = UnitModel(source: fm1, fallback: emu, backups: b, projects: p)
        m.start()
        #expect(await waitFor { m.onEmulator && m.notice?.kind == "update" })
        #expect(m.state.kind == "found" && m.noticeOpen, "its card opens by itself")
        // gone: the notice too
        fm1.unplug()
        #expect(await waitFor { m.notice == nil })
        #expect(m.onEmulator)
        m.stop()
    }

    @Test func idleWhenNothingWrites() async {
        let fm1 = PlugSource(.fizz(port: "FIZZ FM-1"), plugged: false)
        let emu = PlugSource(.fizz(port: "FIZZ (this iPhone)"), plugged: true)
        let (b, p) = Self.dirs()
        let m = UnitModel(source: fm1, fallback: emu, backups: b, projects: p)
        m.start()
        #expect(await waitFor { m.onEmulator && m.dumpsLoaded })
        #expect(m.idle)
        m.stop()
    }

    @Test func withoutAFallbackTheAppWaitsAsBefore() async {
        let m = UnitModel(source: PlugSource(.fizz(), plugged: false))
        #expect(!m.hasFallback)
        m.start()
        try? await Task.sleep(for: .milliseconds(300))
        #expect(m.state.kind == "searching" && m.unit == nil)
        m.stop()
    }

    // MARK: the host

    @Test func theHostsClock() async {
        let e = FakeEngine()
        let h = EmulatorHost(engine: e, source: PlugSource(.fizz(), plugged: true))
        h.launch()
        await h.settle()
        #expect(e.isBooted && e.mode == .timer, "launched: the timer, for FizzLink")
        await h.serve(false)
        #expect(e.mode == .stopped, "an FM-1 the unit, PLAY hidden: nothing runs")
        h.playShown = true
        await h.settle()
        #expect(e.mode == .audio, "PLAY: heard")
        e.isPlaying = true
        h.playShown = false
        await h.settle()
        #expect(e.mode == .audio, "a pattern left playing goes on")
        e.isPlaying = false
        h.trackOn = true                                    // (TRACK on an FM-1: not the emulator's business)
        await h.settle()
        h.playShown = true; h.playShown = false
        await h.settle()
        #expect(e.mode == .stopped)
        await h.serve(true)
        #expect(e.mode == .audio, "the unit, TRACK on: heard")
        h.trackOn = false
        await h.settle()
        #expect(e.mode == .timer)
        h.heard()
        await h.settle()
        #expect(e.mode == .audio, "a keybed note: heard")
        await h.reset()
        #expect(e.resets == 1)
    }

    @Test func theClocksRule() {
        let c = EmulatorHost.clock
        #expect(c(false, false, false, false, false, .stopped) == .stopped)
        #expect(c(false, true, false, false, false, .stopped) == .timer)
        #expect(c(true, false, false, false, false, .stopped) == .audio)
        #expect(c(false, true, true, false, false, .timer) == .audio)
        #expect(c(false, true, false, true, false, .timer) == .audio)
        #expect(c(false, false, false, false, true, .audio) == .audio)
        #expect(c(false, false, false, false, true, .timer) == .stopped)
    }

    // MARK: PLAY's input

    @Test func buttonLabels() {
        #expect(PlayInput.button("TRACKS") == .tracks)
        #expect(PlayInput.button("midi fx") == .midiFX)
        #expect(PlayInput.button("SEL") == .tracks, "the FM-1's printed label too")
        #expect(PlayInput.button("F1") == .f1 && PlayInput.button("OCT+") == .f2)
        #expect(PlayInput.button("NOPE") == nil)
        #expect(PlayInput.well.map { PlayInput.label($0) } ==
                ["FX", "TRACKS", "MIX", "LATCH", "EDIT", "SHIFT", "HOME", "SOUNDS", "MIDI\nFX", "PATTERNS", "PLAY", "REC"])
    }

    @Test func knobDragsToDetents() {
        var d = PlayInput.Detents()
        #expect(d.move(dx: 0, dy: -13) == 0)
        #expect(d.move(dx: 0, dy: -2) == 1, "the 1 point left over carried")
        #expect(d.move(dx: 0, dy: -28) == 2)
        #expect(d.move(dx: -42, dy: 0) == -2, "left: anticlockwise (the 1 point carried kept)")
        #expect(d.move(dx: -1, dy: 0) == -1)
        #expect(d.move(dx: 7, dy: 7) == 0, "right and down cancel")
        #expect(PlayInput.master(from: 512, dx: 0, dy: -110) == 1023)
        #expect(PlayInput.master(from: 100, dx: 0, dy: 400) == 0)
        #expect(PlayInput.master(from: 0, dx: 0, dy: -220) == 1023)
    }

    @Test func keybedVelocityAndKeys() {
        #expect(PlayInput.velocity(at: 0) == 24)
        #expect(PlayInput.velocity(at: 1) == 127)
        #expect(PlayInput.velocity(at: 0.5) > 60 && PlayInput.velocity(at: 0.5) < 90)
        #expect(PlayInput.velocity(at: -1) == 24 && PlayInput.velocity(at: 3) == 127)
        #expect(PlayInput.note(0, octave: 0) == 53 && PlayInput.note(26, octave: 1) == 91)

        // all 16 across 800 x 200: the whites are 0 2 4 6 7 .., the blacks between
        let L = KeyLayout(size: CGSize(width: 800, height: 200))
        #expect(L.frames.count == 27 && Set(L.frames.map(\.key)) == Set(0..<27))
        let w0 = L.whiteFrame(0), w4 = L.whiteFrame(4)
        #expect(L.key(at: CGPoint(x: w0.midX, y: w0.maxY - 1))?.key == 0)
        #expect(L.key(at: CGPoint(x: w4.midX, y: w4.midY))?.key == 7, "the fifth white: C4")
        let b0 = L.blackFrame(after: 0)
        #expect(L.key(at: CGPoint(x: b0.midX, y: b0.minY + 2))?.key == 1, "the black above: F#")
        let near = L.key(at: CGPoint(x: w0.midX, y: w0.maxY - 1))!.depth, far = L.key(at: CGPoint(x: w0.midX, y: w0.minY + 1))!.depth
        #expect(near > 0.95 && far < 0.05)
        // under the black row, between two blacks: the white key
        #expect(L.key(at: CGPoint(x: L.whiteFrame(3).midX, y: 10))?.key == 6, "B3: no black after it")
        #expect(L.key(at: CGPoint(x: 900, y: 100)) == nil)

        // scrolled 4 white keys: C4 first
        let S = KeyLayout(size: CGSize(width: 330, height: 180), keyW: 40, offset: 4)
        #expect(S.key(at: CGPoint(x: 5, y: 170))?.key == 7)
        #expect(S.visible > 7 && S.visible < 8.5)
    }

    @Test func viewsBySwipe() {
        #expect(PlayModel.swiped(.full, by: 1) == .keys)
        #expect(PlayModel.swiped(.panel, by: 1) == .screen)
        #expect(PlayModel.swiped(.screen, by: -1) == .keys)
        #expect(PlayModel.swiped(.full, by: -1) == .full)
        #expect(PlayModel.swiped(.screen, by: 1) == .screen)
    }
}
