// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// The emulated FIZZ the app keeps (mock-ups 23-33): one FizzEmulator for the whole app, booted at launch from its
// saved flash. Its port is the app's unit when no FM-1 is plugged in (UnitModel's fallback); PLAY plays it. Its
// clock runs on a timer while it is the unit (FizzLink's SysEx needs it), with the sound while it is heard (PLAY
// shown, TRACK on it, a keybed note, a pattern left playing), and not at all otherwise. In the background
// FizzEmulator stops it itself and starts it again in the foreground.

import FizzEngine
import FizzKit
import Foundation
import Observation

/// What the host drives: FizzEmulator (a fake in the tests).
public protocol EmulatorEngine: AnyObject, Sendable {
    var isBooted: Bool { get }
    var mode: FizzEmulator.ClockMode { get }
    var isPlaying: Bool { get }
    func boot()
    @discardableResult func start(sound: Bool) -> FizzEmulator.ClockMode
    func stop()
    func reset()
}

extension FizzEmulator: EmulatorEngine {}

@MainActor @Observable
public final class EmulatorHost {
    /// The real emulator (PLAY's screen, buttons, knobs and keys); nil with a fake engine.
    @ObservationIgnored public let emulator: FizzEmulator?
    @ObservationIgnored let engine: any EmulatorEngine
    /// Its port: UnitModel's fallback.
    @ObservationIgnored public let source: any LinkSource
    /// PLAY's state (its theme, view, what is held), kept across the tabs.
    @ObservationIgnored public private(set) lazy var play = PlayModel(host: self)

    /// FIZZ's version as the core says it, a development build's git-describe tail taken off.
    public var version: String {
        (emulator?.info.version ?? "").replacingOccurrences(of: "-\\d+-g[0-9a-f]+$", with: "", options: .regularExpression)
    }

    /// The clock as last set.
    public private(set) var mode: FizzEmulator.ClockMode = .stopped
    public private(set) var booted = false
    /// PLAY on screen
    public var playShown = false { didSet { if playShown != oldValue { update() } } }
    /// The emulated FIZZ is UnitModel's unit.
    public private(set) var serving = false
    /// TRACK on, with the emulated FIZZ the unit
    public var trackOn = false { didSet { if trackOn != oldValue { update() } } }
    /// A note sent to it while it is the unit (EDIT's keybed): heard from then on.
    public private(set) var notes = false
    /// Its STATUS as PLAY's sheet last read it (while an FM-1 is the unit; else UnitModel's)
    public private(set) var status: FizzStatus?

    @ObservationIgnored private var wanted: FizzEmulator.ClockMode = .stopped
    @ObservationIgnored private let ops = DispatchQueue(label: "fizz.emulator.host", qos: .userInitiated)
    @ObservationIgnored private var launched = false
    @ObservationIgnored private var poke: (@Sendable () -> Void)?
    @ObservationIgnored private var lent = 0

    /// What the emulated FIZZ answers the update handshake (its backups carry it).
    public nonisolated static let identity = "FM-1_46999999"

    /// The app's one (FizzAppView).
    public static let app = EmulatorHost()

    /// The app's: the flash in Documents/Emulator, the port "FIZZ (this iPhone)".
    public convenience init(emulator e: FizzEmulator = FizzEmulator(identity: EmulatorHost.identity)) {
        let s = EmulatorLinkSource(emulator: e, portName: Self.portName)
        self.init(emulator: e, engine: e, source: s, poke: { s.poke() })
        e.log = { print("FIZZ: \($0)") }
    }

    /// A fake engine and source (the tests).
    convenience init(engine: any EmulatorEngine, source: any LinkSource, poke: (@Sendable () -> Void)? = nil) {
        self.init(emulator: nil, engine: engine, source: source, poke: poke)
    }

    private init(emulator: FizzEmulator?, engine: any EmulatorEngine, source: any LinkSource,
                 poke: (@Sendable () -> Void)?) {
        self.emulator = emulator
        self.engine = engine
        self.source = source
        self.poke = poke
    }

    /// "FIZZ (this iPhone)" / "FIZZ (this iPad)"
    static var portName: String { device == "iPAD" ? "FIZZ (this iPad)" : EmulatorLinkSource.defaultPortName }

    /// THIS iPHONE / THIS iPAD (the unit pill, PLAY's SOUND)
    public static var hereName: String { "THIS \(device)" }
    static var device: String {
        #if os(iOS)
        isPad ? "iPAD" : "iPHONE"
        #else
        "iPHONE"                // (the Mac only draws the phone's screens: fizzshots)
        #endif
    }

    // MARK: power

    /// Power on from the saved flash, the clock on its timer until UnitModel says whether it is the unit; then its
    /// port is announced (UnitModel looks again). Once.
    public func launch() {
        guard !launched else { return }
        launched = true
        wanted = .timer
        let e = engine, p = poke
        ops.async { [weak self] in
            e.boot()
            let m = e.start(sound: false)
            Task { @MainActor [weak self] in
                self?.booted = true
                self?.mode = m
                if self?.playShown == true { self?.update() }   // (PLAY shown before it was up)
                p?()
            }
        }
    }

    /// The emulated FIZZ becomes the unit (or stops being it): its clock runs for FizzLink first.
    public func serve(_ on: Bool) async {
        serving = on
        if !on { notes = false }
        update()
        await settle()
    }

    /// A note played on the emulated FIZZ through its port (EDIT's keybed): heard from now on.
    public func heard() {
        guard serving, !notes else { return }
        notes = true
        update()
    }

    /// RESET: a fresh flash (the saved one removed); the clock as it was.
    public func reset() async {
        let e = engine
        await withCheckedContinuation { (k: CheckedContinuation<Void, Never>) in
            ops.async { e.reset(); k.resume() }
        }
    }

    /// A link of the app's own on it while an FM-1 is the unit (SEND between the units): the clock runs meanwhile.
    public func lend(_ on: Bool) async {
        lent = max(0, lent + (on ? 1 : -1))
        update()
        await settle()
    }

    /// Its STATUS through a port of its own (PLAY's MEMORY while an FM-1 is the unit).
    public func readStatus() async {
        guard let e = emulator, e.isBooted, e.mode != .stopped else { return }
        let link = FizzLink(transport: e.makeTransport(name: Self.portName), portName: Self.portName)
        status = await link.status()
        await link.close()
    }

    /// The clock each need asks for.
    nonisolated static func clock(playShown: Bool, serving: Bool, trackOn: Bool, notes: Bool, playing: Bool,
                                  now: FizzEmulator.ClockMode) -> FizzEmulator.ClockMode {
        if playShown || (serving && (trackOn || notes)) { return .audio }
        if now == .audio && playing { return .audio }        // (a pattern left playing goes on being heard)
        return serving ? .timer : .stopped
    }

    func update() {
        guard launched else { return }
        let e = engine
        let c = Self.clock(playShown: playShown, serving: serving, trackOn: trackOn && serving, notes: notes,
                           playing: wanted == .audio && e.isPlaying, now: wanted)
        let w = c == .stopped && lent > 0 ? .timer : c          // (lend: SEND's link needs its clock)
        guard w != wanted else { return }
        wanted = w
        ops.async { [weak self] in
            switch w {
            case .stopped: e.stop()
            case .timer, .audio: e.start(sound: w == .audio)
            }
            let m = e.mode
            Task { @MainActor [weak self] in self?.mode = m }
        }
    }

    /// Returns when the clock changes asked for are made.
    func settle() async {
        await withCheckedContinuation { (k: CheckedContinuation<Void, Never>) in ops.async { k.resume() } }
        await Task.yield()
    }
}

#if os(iOS)
import UIKit
extension EmulatorHost {
    static var isPad: Bool { UIDevice.current.userInterfaceIdiom == .pad }
}
#endif
