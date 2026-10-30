// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2026 RemoteFM1 contributors
// Derived from RemoteFM1IOS af91bc5 RFMKit/Sources/RFMKit/OMX27/OmxLink.swift, adapted: its target is PLAY's panel (PlayModel, MidiHub.swift); its pair readable by MidiHub.
// USE MY OMX-27 (the standalone app): the OMX-27 in REMOTE mode as the FM-1's controller (OmxWire the wire, OmxViews
// the views, OmxScreen its screen). The website's js/omx-link.js, ported as is. The ports whose names say OMX; the
// MODE command puts it in REMOTE mode, and while it is there its keys, encoder and pots drive the panel (an
// OmxTarget) and the app lights its 27 keys and draws its 128 x 32 screen. Connected once, it connects by itself at
// the next start (auto()) and when it is plugged in. Disconnect clears its LEDs and screen; the OMX can't be told to
// leave REMOTE mode: AUX + encoder hold on it does. While it is in REMOTE mode the app's MIDI input should skip its
// port (owns()).

import Foundation
import Observation
#if canImport(CoreMIDI)
import CoreMIDI
#endif

/// the panel the OMX-27 plays (the app's PanelFeed, through an adapter)
@MainActor public protocol OmxTarget: AnyObject {
    /// the FM-1's key 0..26
    func omxKey(_ k: Int, down: Bool)
    func omxButton(_ l: PanelFeed.Label, down: Bool)
    func omxTurn(_ k: PanelFeed.Knob, _ detents: Int)
    /// the encoder now turns this knob (the UI may show it)
    func omxSelect(_ k: PanelFeed.Knob)
    /// MASTER, 0..1023
    func omxMaster(_ v: Int)
    /// K5 away from MASTER (pot, at: 0..1023; first: once per wait): MASTER waits for it
    func omxPickup(pot: Int, at: Int, first: Bool)
    /// the Keyboard view: one channel message to the firmware's MIDI in
    func omxFirmwareMIDI(_ bytes: [UInt8])
    /// MASTER now, 0..1023
    var omxMasterNow: Int { get }
    /// the FM-1 key k's LED, 0..255
    func omxKeyLevel(_ k: Int) -> UInt8
    /// everything the OMX holds let go
    func omxReleaseAll()
}

@MainActor @Observable public final class OmxLink {
    public enum Tone: String, Sendable { case off, busy, on, bad }

    static let offMs = 50.0      // 52 03 then 52 02 within this: still in REMOTE mode
    static let answerMs = 2500.0
    static let tickMs = 10.0
    public static let leave = "To leave remote mode on the OMX-27, hold AUX and the encoder."
    static let prefsKey = "rfm1.omx", autoKey = "rfm1.omxAuto"

    // the status, for the UI
    public private(set) var tone = Tone.off
    public private(set) var detail = "Not connected."
    /// Connect was asked for (and not Disconnect since)
    public private(set) var want = false
    /// in REMOTE mode as the app sees it (52 03 + 52 02 at once: still)
    public private(set) var active = false
    /// the OMX's port name ("" none)
    public var port: String { pair?.name ?? "" }
    /// the button's title: Connect, Disconnect, Try again
    public var buttonTitle: String { want && tone != .bad ? "Disconnect" : tone == .bad ? "Try again" : "Connect my OMX-27" }

    @ObservationIgnored public weak var target: (any OmxTarget)?
    /// a short message to show for a moment (ok: a success)
    @ObservationIgnored public var onToast: ((_ text: String, _ ok: Bool) -> Void)?
    /// REMOTE mode began or ended (the MIDI input picks its sources again)
    @ObservationIgnored public var onActive: ((Bool) -> Void)?

    @ObservationIgnored public let views: OmxViews
    @ObservationIgnored private let transport: any OmxTransport
    @ObservationIgnored private let clock: any OmxClock
    @ObservationIgnored private let defaults: UserDefaults
    @ObservationIgnored private var started = false
    @ObservationIgnored public private(set) var pair: OmxPair?
    @ObservationIgnored public private(set) var session: OmxSession?
    @ObservationIgnored private var timer: (any OmxTimer)?
    @ObservationIgnored private var answerTimer: (any OmxTimer)?
    @ObservationIgnored private var offTimer: (any OmxTimer)?
    @ObservationIgnored private var ownTurn = false
    @ObservationIgnored private var shown = ""
    @ObservationIgnored private var pots: [Int: (v: Int, hires: Int)] = [:]   // pot -> its latest message, not taken yet
    @ObservationIgnored private var potOrder: [Int] = []

    /// transport nil: CoreMIDI; clock nil: the main queue's; defaults: where the view prefs and the auto flag live
    public init(target: (any OmxTarget)? = nil, transport: (any OmxTransport)? = nil, clock: (any OmxClock)? = nil,
                defaults: UserDefaults = .standard) {
        self.target = target
        #if canImport(CoreMIDI)
        self.transport = transport ?? OmxCoreMIDI()
        #else
        self.transport = transport!
        #endif
        self.clock = clock ?? OmxMainClock()
        self.defaults = defaults
        let saved = defaults.data(forKey: Self.prefsKey).flatMap { try? JSONDecoder().decode(OmxViewPrefs.self, from: $0) }
        views = OmxViews(saved: saved)
    }

    private func set(_ tone: Tone, _ detail: String) {
        if self.tone != tone { self.tone = tone }
        if self.detail != detail { self.detail = detail }
    }

    private var autoFlag: Bool {
        get { defaults.bool(forKey: Self.autoKey) }
        set { defaults.set(newValue, forKey: Self.autoKey) }
    }

    /// an endpoint the app's MIDI input leaves alone (REMOTE mode: its events are ours)
    public func owns(id: Int32, name: String? = nil) -> Bool {
        guard active, let pair else { return false }
        return id == pair.input.id || (name != nil && name == pair.input.name)
    }

    #if canImport(CoreMIDI)
    public func owns(_ endpoint: MIDIEndpointRef) -> Bool {
        guard active else { return false }
        let e = OmxCoreMIDI.identity(endpoint)
        return owns(id: e.id, name: e.name)
    }
    #endif

    /// the button: Connect (or Try again), or Disconnect
    public func toggle() { if want && tone != .bad { disconnect() } else { connect() } }

    public func connect() {
        want = true
        if !autoFlag { autoFlag = true }
        if let pair, let session {
            session.enter()
            set(.busy, "Asking \(pair.name) for remote mode...")
            expectAnswer()
            return
        }
        if !started {
            if let why = transport.start(onSetupChange: { [weak self] in self?.scan() }) {
                want = false
                set(.bad, why)
                return
            }
            started = true
        }
        scan()
    }

    /// at the app's start: connects by itself when it was connected last time
    public func auto() { if autoFlag && !want { connect() } }

    public func disconnect() {
        want = false
        autoFlag = false
        let was = active
        session?.clear()
        drop()
        set(.off, was ? "Disconnected. The OMX-27 stays in remote mode: \(Self.leave.prefix(1).lowercased() + Self.leave.dropFirst())"
                      : "Not connected.")
    }

    /// the app going away: the OMX's LEDs and screen cleared (it stays in REMOTE mode)
    public func willClose() { if let session, session.active { session.clear() } }

    /// the ports now: the OMX's pair attached, or let go when it went
    public func scan() {
        guard want, started else { return }
        let ps = transport.ports(), p = omxPair(inputs: ps.inputs, outputs: ps.outputs)
        if let cur = pair, p == nil || p!.input.id != cur.input.id || p!.output.id != cur.output.id {
            let name = cur.name
            drop()
            set(.busy, "\(name) unplugged: it connects again when it is back.")
            if p == nil { return }
        }
        if pair == nil, let p { attach(p) }
        else if pair == nil { set(.busy, "Plug in your OMX-27: it connects when it appears.") }
    }

    private func attach(_ p: OmxPair) {
        pair = p
        transport.open(input: p.input, output: p.output) { [weak self] d in self?.message(d) }
        let t = transport, c = clock
        session = OmxSession(send: { b, d in t.send(b, delayMs: d) }, now: { c.now })
        shown = ""
        session!.enter()
        set(.busy, "Asking \(p.name) for remote mode...")
        expectAnswer()
        timer?.cancel()
        timer = clock.every(Self.tickMs) { [weak self] in self?.tick() }
    }

    private func expectAnswer() {
        answerTimer?.cancel()
        answerTimer = clock.after(Self.answerMs) { [weak self] in
            guard let self, let session = self.session, !session.active, self.want, let pair = self.pair else { return }
            self.set(.bad, "\(pair.name) did not answer: it needs the OMX-27 firmware with REMOTE mode. Connect asks again.")
        }
    }

    private func drop() {
        timer?.cancel(); timer = nil
        answerTimer?.cancel(); answerTimer = nil
        offTimer?.cancel(); offTimer = nil
        pots.removeAll(); potOrder.removeAll()
        let was = active
        active = false
        release()
        transport.close()
        pair = nil
        session = nil
        if !want && started { transport.stop(); started = false }
        if was { changed() }
    }

    private func release() {
        apply(views.releaseAll())
        target?.omxReleaseAll()
    }

    private func changed() { onActive?(active) }

    // MARK: the OMX's messages

    func message(_ d: [UInt8]) {
        guard let session, let m = session.feed(d) else { return }
        let now = clock.now
        if case .status(let s) = m {
            if s == OmxWire.STATUS.on {
                answerTimer?.cancel(); answerTimer = nil
                shown = ""
                // (MODE while already in REMOTE mode: the OMX answers 52 03 and 52 02 within a few ms: still connected)
                offTimer?.cancel(); offTimer = nil
                let name = pair?.name ?? "The OMX-27"
                set(.on, "\(name) in remote mode: \(views.view.name) view. \(Self.leave)")
                if !active {
                    active = true
                    onToast?("\(name) in remote mode: its keys, knobs and screen play the FM-1.", true)
                    changed()
                }
                refresh()
            } else if s == OmxWire.STATUS.off {
                offTimer?.cancel()
                offTimer = clock.after(Self.offMs) { [weak self] in
                    guard let self else { return }
                    self.offTimer = nil
                    guard let session = self.session, !session.active else { return }
                    let name = self.pair?.name ?? "The OMX-27"
                    self.active = false
                    self.release()
                    self.set(.busy, "\(name) left remote mode: pick REMOTE on it again, or Connect.")
                    self.onToast?("\(name) left remote mode.", false)
                    self.changed()
                }
            }
            return
        }
        guard session.active else { return }
        switch m {
        case .pot(let p, let v, let hires):
            // (pots stream every 2-3 ms while they move: the latest of each, taken on the next tick)
            if pots[p] == nil { potOrder.append(p) }
            pots[p] = (v, hires)
            return
        case .key(let k, let ev): apply(views.key(k, ev, now))
        case .enc(let delta): apply(views.enc(delta))
        default: break
        }
        refresh()
    }

    private func takePots() -> Bool {
        guard !potOrder.isEmpty else { return false }
        let now = clock.now
        for p in potOrder {
            guard let m = pots[p] else { continue }
            apply(views.pot(p, m.v, m.hires, now, target?.omxMasterNow ?? 768))
        }
        pots.removeAll(); potOrder.removeAll()
        return true
    }

    private func apply(_ acts: [OmxAction]) {
        for a in acts {
            switch a {
            case .key(let k, let down): target?.omxKey(k, down: down)
            case .button(let b, let down): target?.omxButton(b, down: down)
            case .turn(let role, let steps):
                ownTurn = true
                target?.omxTurn(role, steps)
                ownTurn = false
            case .select(let role): target?.omxSelect(role)
            case .midi(let bytes): target?.omxFirmwareMIDI(bytes)
            case .master(let v): target?.omxMaster(v)
            case .pickup(let pot, let at, let first): target?.omxPickup(pot: pot, at: at, first: first)
            case .view(let v): set(tone, "\(pair?.name ?? "The OMX-27") in remote mode: \(v.name) view. \(Self.leave)")
            case .theme: break
            }
            switch a {
            case .theme, .select, .button, .view: keep()
            default: break
            }
        }
    }

    private func keep() {
        let v = views.saved()
        let old = defaults.data(forKey: Self.prefsKey).flatMap { try? JSONDecoder().decode(OmxViewPrefs.self, from: $0) }
        guard v != old, let data = try? JSONEncoder().encode(v) else { return }
        defaults.set(data, forKey: Self.prefsKey)
    }

    /// a knob turned on the panel (not by the OMX): the encoder turns that one next
    public func knobUsed(_ k: PanelFeed.Knob) {
        guard !ownTurn else { return }
        views.knobUsed(k)
    }

    // MARK: what it shows

    /// the LEDs and the screen again now (the FM-1's LEDs changed; the tick does it within 10 ms anyway)
    public func refresh() {
        guard let session, session.active else { return }
        session.setLeds(views.leds { [weak self] k in self?.target?.omxKeyLevel(k) ?? 0 })
        draw()
        session.tick()
    }

    private func tick() {
        guard let session, session.active else { return }
        if takePots() { return refresh() }
        // (the FM-1's key LEDs, mirrored: the website refreshes on its LED events; here every tick, the session
        // sends only what changed)
        session.setLeds(views.leds { [weak self] k in self?.target?.omxKeyLevel(k) ?? 0 })
        draw()
        session.tick()
    }

    private func draw() {
        guard let session else { return }
        let s = views.screen(clock.now), key = "\(s.title)|\(s.line)|\(s.view.rawValue)"
        if key == shown { return }
        shown = key
        session.setFrame(OmxScreen.render(s))
    }
}
