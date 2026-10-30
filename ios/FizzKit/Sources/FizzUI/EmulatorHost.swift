// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// The emulated FIZZ the app keeps (mock-ups 23-33): one FizzEmulator for the whole app, booted at launch from its
// saved flash. Its port is the app's unit when no FM-1 is plugged in (UnitModel's fallback); PLAY plays it. Its
// clock runs on a timer while it is the unit (FizzLink's SysEx needs it), with the sound while it is heard (PLAY
// shown, TRACK on it, a keybed note, a pattern left playing), and not at all otherwise. In the background
// FizzEmulator stops it itself and starts it again in the foreground.
// A backup played (mock-up 44, BackupPlay.swift) runs on a throwaway FizzEmulator with no flash file: the core is the
// process's one, so THIS iPHONE's is stopped (its flash saved) and set aside meanwhile, and on × boots again from
// its file and takes the core back, its clock as the needs ask.
// A RemoteFM1 unit can be its panel (PanelLink, on every tab): claimed by hand (attachPanel), it keeps the clock and the
// sound going while it is attached, PLAY shown or not.

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
    /// Stopped and its whole flash saved, whatever FIZZ still held back (a backup is about to take the core).
    func setAside()
}

extension EmulatorEngine {
    public func setAside() { stop() }
}

extension FizzEmulator: EmulatorEngine {
    /// (save(force:): a STORE just made may not be counted dirty yet; the copy syncs it first)
    public func setAside() {
        stop()
        save(force: true)
    }
}

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
    /// PLAY's sound (MIDI & audio > Sound, kept): off, the clock runs silent where it would be heard.
    public var soundOn = UserDefaults.standard.object(forKey: "play.soundOn") as? Bool ?? true {
        didSet { if soundOn != oldValue { UserDefaults.standard.set(soundOn, forKey: "play.soundOn"); update() } }
    }
    /// A note sent to it while it is the unit (EDIT's keybed): heard from then on.
    public private(set) var notes = false
    /// Its STATUS as PLAY's sheet last read it (while an FM-1 is the unit; else UnitModel's)
    public private(set) var status: FizzStatus?
    /// A RemoteFM1 unit as THIS iPHONE's panel: the link's state (nil: none attached), and its port.
    public private(set) var panel: PanelLink.State?
    public private(set) var panelPort: String?
    /// The backup PLAY plays (nil: THIS iPHONE's own FIZZ).
    public private(set) var backup: BackupPlaying?
    /// What PLAY drives: the backup's throwaway FIZZ while one plays, else the app's own.
    public var active: FizzEmulator? { backup?.emulator ?? emulator }

    @ObservationIgnored private var wanted: FizzEmulator.ClockMode = .stopped
    @ObservationIgnored private let ops = DispatchQueue(label: "fizz.emulator.host", qos: .userInitiated)
    @ObservationIgnored private var launched = false
    @ObservationIgnored private var closed = false
    @ObservationIgnored private var poke: (@Sendable () -> Void)?
    @ObservationIgnored private var lent = 0
    @ObservationIgnored private var panelLink: PanelLink?

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
        guard !launched, !closed else { return }
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

    /// RESET: a fresh flash (the saved one removed); the clock as it was. (THIS iPHONE's: a backup playing ends first.)
    public func reset() async {
        await endBackup()
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

    // MARK: a RemoteFM1 unit as its panel

    /// Claim a RemoteFM1 unit (its port opened with its channel messages) as the panel: its controls play this FIZZ, and
    /// FIZZ's LCD and LEDs show on it; the clock and the sound run while it is attached. A panel attached before is
    /// let go first. With a fake engine (the tests) the port is closed and nothing happens.
    public func attachPanel(_ transport: any MIDITransport) {
        guard let e = emulator, !closed else { transport.close(); return }
        panelLink?.release()
        let link = PanelLink(transport: transport, emulator: e)
        link.log = { print("FIZZ: \($0)") }
        panelLink = link
        panelPort = transport.name
        panel = .idle
        let id = ObjectIdentifier(link)
        link.onState = { [weak self] s in Task { @MainActor [weak self] in self?.panelChanged(id, s) } }
        link.claim()
        update()
    }

    /// Let the panel go (SESSION 0: the unit shows its waiting screen).
    public func detachPanel() { panelLink?.release() }

    private func panelChanged(_ link: ObjectIdentifier, _ s: PanelLink.State) {
        guard let l = panelLink, ObjectIdentifier(l) == link else { return }
        panel = s
        if case .released = s {
            panelLink = nil
            panelPort = nil
        }
        update()
    }

    /// Its STATUS through a port of its own (PLAY's MEMORY while an FM-1 is the unit).
    public func readStatus() async {
        guard let e = active, e.isBooted, e.mode != .stopped else { return }
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
        guard launched, !closed else { return }
        // (a backup playing: its throwaway FIZZ is no unit; THIS iPHONE's stays stopped until ×)
        let e: any EmulatorEngine = backup?.emulator ?? engine, own = backup == nil
        // (a RemoteFM1 unit attached as the panel: heard as PLAY is, on whatever tab)
        let c = Self.clock(playShown: playShown || (panelLink != nil && own), serving: serving && own,
                           trackOn: trackOn && serving && own, notes: notes && own,
                           playing: wanted == .audio && e.isPlaying, now: wanted)
        let h = c == .audio && !soundOn ? .timer : c           // (the sound off: silent)
        let w = h == .stopped && lent > 0 && own ? .timer : h   // (lend: SEND's link needs its clock)
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

    /// Done with it (a test's end: the core is the process's one, and a clock change still queued here would start
    /// this emulator again under the next one): nothing asked of the clock from now on, what was asked made, the
    /// clock stopped.
    public func close() async {
        closed = true
        panelLink?.release()
        let e = engine, b = backup?.emulator
        await withCheckedContinuation { (k: CheckedContinuation<Void, Never>) in
            ops.async { b?.stop(); e.stop(); k.resume() }
        }
        mode = .stopped
    }

    // MARK: a backup played (mock-up 44)

    /// PLAY on a backup: `flash` (the core's whole image, StoreImage.hostFlash) booted in a throwaway FizzEmulator with
    /// no flash file. THIS iPHONE's is stopped first (its flash saved: the core is the process's one) and stays
    /// stopped, its port gone, until endBackup(). Another backup playing is thrown away first.
    public func playBackup(flash: [UInt8], name: String, date: Date) async {
        guard launched, !closed, emulator != nil else { return }
        play.letGoAll()
        let old = backup?.emulator
        let b = FizzEmulator(storage: nil, identity: Self.identity)
        b.log = { print("FIZZ (backup): \($0)") }
        let own = engine
        await withCheckedContinuation { (k: CheckedContinuation<Void, Never>) in
            ops.async {
                if let old { old.stop() } else { own.setAside() }   // (THIS iPHONE's flash saved while it has the core)
                b.boot(flash: flash)                                // (the core taken: THIS iPHONE's transports lost)
                k.resume()
            }
        }
        backup = BackupPlaying(emulator: b, name: name, date: date)
        wanted = .stopped
        mode = .stopped
        notes = false
        update()
        await settle()
        poke?()                                                     // (THIS iPHONE's port gone: UnitModel looks again)
    }

    /// ×: the backup's FIZZ thrown away, THIS iPHONE's booted again from its saved flash (the core taken back) and its
    /// clock started as the needs ask; its port back.
    public func endBackup() async {
        guard let b = backup else { return }
        play.letGoAll()
        backup = nil
        let own = engine, throwaway = b.emulator
        await withCheckedContinuation { (k: CheckedContinuation<Void, Never>) in
            ops.async {
                throwaway.stop()
                own.boot()
                k.resume()
            }
        }
        wanted = .stopped
        mode = .stopped
        update()
        await settle()
        poke?()
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
