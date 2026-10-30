// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// A RemoteFM1 unit as the panel of the emulated FIZZ (THIS iPHONE): its keys, buttons, knobs and MASTER play the
// phone's FIZZ, and FIZZ's LCD and LEDs show on the unit. The sound stays on the phone.
//
// libremotefm1's host session (CRemoteFM1, session.h) runs it: HELLO on the port, SESSION start, then the screen as
// 16x16 tiles that changed (at the unit's rate: 20 frames a second at most, two in flight), LEDS on a change and once
// a second, the panel's plain MIDI and PANEL_R (every 500 ms, the session's heartbeat) decoded into events. Everything
// here runs on one serial queue: the session is single-threaded.
//
// TAP TO CLAIM (the owner's decision): nothing is sent to the unit until claim(). Once the session is up, FIZZ lets go
// when it is no longer the unit's host: a PANEL_R without the session flag (another host took the unit, or the unit
// ended the session), no PANEL_R for the heartbeat's time, the unit's shadow reset (it restarted), or the unit gone.
// libremotefm1 would start the session again or look for the unit again in those cases; here its messages from that
// moment are dropped (no SESSION start, no HELLO, no SESSION end: another host's session is left alone), and the link
// ends. A claim that finds no RemoteFM1 unit within a second ends too. release() ends the session (SESSION 0) and
// the link. A link is used once: claim again with a new one.

import CRemoteFM1
import CoreGraphics
import FizzKit
import Foundation

public final class PanelLink: @unchecked Sendable {
    public enum State: Sendable, Equatable {
        case idle           // made, nothing sent
        case claiming       // HELLO / SESSION start sent, no session yet
        case active         // the unit is FIZZ's panel
        case released(Release)
    }

    /// Why the link ended.
    public enum Release: String, Sendable, Equatable {
        case byUser         // release(): SESSION 0 sent
        case sessionEnded   // PANEL_R without the session flag: another host took the unit, or the unit ended it
        case heartbeat      // no PANEL_R for the heartbeat's time (the session moved elsewhere, or the unit stalled)
        case unitReset      // the unit's shadow was reset outside the session's start (it restarted)
        case lost           // the port went away, or the unit fell silent
        case notFound       // nothing answered HELLO as RemoteFM1 v2
        case refused        // the unit never acknowledged SESSION start
    }

    /// What the session has done (libremotefm1's counters).
    public struct Stats: Sendable, Equatable {
        public var frames = 0, tiles = 0, bodyBytes = 0, ledsSent = 0, panelR = 0, events = 0, sessions = 0
    }

    public let portName: String
    /// Each change of state (on the link's queue).
    public var onState: (@Sendable (State) -> Void)? {
        get { lock.withLock { stateFn } }
        set { lock.withLock { stateFn = newValue } }
    }
    /// libremotefm1's log lines and the link's own.
    public var log: (@Sendable (String) -> Void)? {
        get { lock.withLock { logFn } }
        set { lock.withLock { logFn = newValue } }
    }

    public var state: State { lock.withLock { stateNow } }
    /// The unit's HELLO_R once found.
    public var hello: RemoteFM1Hello? { lock.withLock { helloNow } }
    public var stats: Stats { lock.withLock { statsNow } }

    private let transport: any MIDITransport
    private let emulator: FizzEmulator
    private let queue = DispatchQueue(label: "fizz.panel.link", qos: .userInitiated)
    private let lock = NSLock()
    private let pollMs: Int
    private var stateFn: (@Sendable (State) -> Void)?
    private var logFn: (@Sendable (String) -> Void)?
    private var stateNow = State.idle
    private var helloNow: RemoteFM1Hello?
    private var statsNow = Stats()

    // the queue's
    private var session: OpaquePointer?
    private var timer: DispatchSourceTimer?
    private var rxTask: Task<Void, Never>?
    private var lcdTask: Task<Void, Never>?
    private var wasActive = false, stopping = false, muted = false
    private var ending: Release?
    private var epoch: UInt64 = 0
    private var keysHeld = Set<Int>(), buttonsHeld = Set<Int>()
    private var velocity = [UInt8](repeating: 100, count: 27)
    private var lcdSeq: UInt32?              // (lock)

    nonisolated(unsafe) private static let hostName = strdup(RemoteFM1Unit.hostName)!

    /// transport: the unit's port pair, opened with its channel messages (the panel's events); the link owns it and
    /// closes it when it ends. emulator: the FIZZ it plays.
    public init(transport: any MIDITransport, emulator: FizzEmulator, pollMs: Int = 5) {
        self.transport = transport
        self.emulator = emulator
        self.portName = transport.name
        self.pollMs = max(1, pollMs)
    }

    deinit {
        if let s = session { rfm_session_free(s) }
    }

    // MARK: - claim / release

    /// Take the unit: HELLO, then SESSION start; .active once it is acknowledged. Once only.
    public func claim() {
        queue.async { [self] in
            guard session == nil, case .idle = state else { return }
            epoch = DispatchTime.now().uptimeNanoseconds
            var cfg = rfm_session_cfg_t()
            rfm_session_cfg_default(&cfg)
            cfg.host_name = UnsafePointer(Self.hostName)
            cfg.transport = Int32(RFM_T_MIDI)
            var cb = rfm_session_cb_t()
            cb.ctx = Unmanaged.passUnretained(self).toOpaque()
            cb.event = { ctx, ev in
                guard let ctx, let ev else { return }
                Unmanaged<PanelLink>.fromOpaque(ctx).takeUnretainedValue().onEvent(ev.pointee)
            }
            cb.state = { ctx, st, why in
                guard let ctx else { return }
                Unmanaged<PanelLink>.fromOpaque(ctx).takeUnretainedValue()
                    .onSessionState(st, why.map { String(cString: $0) } ?? "")
            }
            cb.log = { ctx, msg in
                guard let ctx, let msg else { return }
                Unmanaged<PanelLink>.fromOpaque(ctx).takeUnretainedValue().say(String(cString: msg))
            }
            guard let s = rfm_session_new(&cfg, &cb) else { end(.lost); return }
            session = s
            let id = portName.withCString { name in
                rfm_session_add_link(s, Int32(RFM_LINK_SX), { ctx, p, n in
                    guard let ctx, let p else { return -1 }
                    return Unmanaged<PanelLink>.fromOpaque(ctx).takeUnretainedValue()
                        .write(Array(UnsafeBufferPointer(start: p, count: Int(n))))
                }, Unmanaged.passUnretained(self).toOpaque(), name)
            }
            guard id >= 0 else { end(.lost); return }
            setState(.claiming)
            startReading(link: id)
            startScreen()
            let t = DispatchSource.makeTimerSource(queue: queue)
            t.schedule(deadline: .now(), repeating: .milliseconds(pollMs), leeway: .milliseconds(1))
            t.setEventHandler { [weak self] in self?.poll() }
            timer = t
            t.resume()
        }
    }

    /// Let the unit go: SESSION 0 (its waiting screen comes back), then .released(.byUser).
    public func release() {
        queue.async { [self] in
            guard let s = session, ending == nil else {
                if session == nil, case .idle = state { finish(.byUser) }
                return
            }
            stopping = true
            rfm_session_stop(s, now)
            settle()
        }
    }

    /// Returns once the calls made so far are done (the tests).
    public func sync() async {
        await withCheckedContinuation { (k: CheckedContinuation<Void, Never>) in queue.async { k.resume() } }
    }

    // MARK: - the session (the queue)

    private var now: UInt32 { UInt32(truncatingIfNeeded: (DispatchTime.now().uptimeNanoseconds &- epoch) / 1_000_000) }

    private func poll() {
        guard let s = session, ending == nil else { return }
        rfm_session_poll(s, now)
        settle()
    }

    private func received(_ m: [UInt8], link: Int32) {
        guard let s = session, ending == nil else { return }
        // (a key's velocity: the event says only down / up)
        if m.count >= 3, m[0] == 0x90 | UInt8(RFM_MIDI_KEY_CH), m[2] > 0 {
            let k = Int(m[1]) - Int(RFM_MIDI_KEY_NOTE0)
            if (0..<27).contains(k) { velocity[k] = m[2] }
        }
        rfm_session_rx(s, link, m, UInt32(m.count), now)
        settle()
    }

    /// After every call into the session: its stats copied; an end asked for inside it made now.
    private func settle() {
        if let s = session {
            var st = rfm_session_stats_t()
            rfm_session_stats(s, &st)
            let n = Stats(frames: Int(st.frames), tiles: Int(st.tiles), bodyBytes: Int(st.body_bytes),
                          ledsSent: Int(st.leds_sent), panelR: Int(st.panel_r), events: Int(st.events),
                          sessions: Int(st.sessions))
            lock.withLock { statsNow = n }
        }
        if let r = ending { finish(r) }
    }

    private func write(_ bytes: [UInt8]) -> Int32 {
        if muted { return 0 }                   // (the link is ending: nothing more goes to the unit)
        do { try transport.send(bytes) } catch { return -1 }
        return 0
    }

    private func onSessionState(_ st: Int32, _ why: String) {
        say("session: \(String(cString: rfm_session_state_name(st))) (\(why))")
        switch Int(st) {
        case Int(RFM_S_ACTIVE):
            wasActive = true
            if let s = session { lock.withLock { helloNow = RemoteFM1Hello(rfm_session_hello(s).pointee) } }
            setState(.active)
        case Int(RFM_S_STARTING):
            guard wasActive else { return }      // (the first start: claiming)
            end(why == "no PANEL_R" ? .heartbeat : why == "shadow reset" ? .unitReset : .sessionEnded)
        case Int(RFM_S_DISCOVER):
            if wasActive || why == "link lost" { end(.lost) }
            else if why == "SESSION not acknowledged" { end(.refused) }
            else if why != "start" { end(.notFound) }                // ("no unit answered", after a second)
        case Int(RFM_S_STOPPED):
            if stopping { end(.byUser) }
        default:
            break
        }
    }

    /// The link ends once the session's call returns (settle); from now on nothing goes to the unit.
    private func end(_ r: Release) {
        guard ending == nil else { return }
        if r != .byUser { muted = true }
        ending = r
    }

    private func finish(_ r: Release) {
        if case .released = state { return }
        timer?.cancel(); timer = nil
        rxTask?.cancel(); rxTask = nil
        lcdTask?.cancel(); lcdTask = nil
        if let s = session { rfm_session_free(s); session = nil }
        // what the unit held, let go on FIZZ
        for k in keysHeld { emulator.noteOff(key: k) }
        for b in buttonsHeld { if let fb = FizzButton(rawValue: b) { emulator.release(fb) } }
        keysHeld = []; buttonsHeld = []
        transport.close()
        say("panel let go: \(r.rawValue)")
        setState(.released(r))
    }

    private func setState(_ s: State) {
        let fn: (@Sendable (State) -> Void)? = lock.withLock {
            guard stateNow != s else { return nil }
            stateNow = s
            return stateFn
        }
        fn?(s)
    }

    private func say(_ s: String) { log?("panel \(portName): \(s)") }

    // MARK: - the unit's port

    private func startReading(link id: Int32) {
        let messages = transport.messages
        rxTask = Task { [weak self] in
            for await m in messages {
                guard let self else { return }
                self.queue.async { self.received(m, link: id) }
            }
            // (the port went away or was closed)
            guard let self, !Task.isCancelled else { return }
            self.queue.async {
                guard self.session != nil, self.ending == nil else { return }
                self.end(self.stopping ? .byUser : .lost)
                self.settle()
            }
        }
    }

    // MARK: - the unit's input -> FIZZ

    private func onEvent(_ ev: rfm_event_t) {
        guard let table = emulator.panelTable else { return }
        let i = Int(ev.index), v = Int(ev.value)
        switch Int(ev.type) {
        case Int(RFM_EV_KEY):
            guard (0..<27).contains(i) else { return }
            if v != 0 {
                keysHeld.insert(i)
                emulator.noteOn(key: i, velocity: Int(velocity[i]))
            } else {
                keysHeld.remove(i)
                emulator.noteOff(key: i)
            }
        case Int(RFM_EV_BUTTON):
            guard let label = table.btnID.firstIndex(of: UInt8(i)), let b = FizzButton(rawValue: label) else { return }
            if v != 0 { buttonsHeld.insert(label); emulator.press(b) } else { buttonsHeld.remove(label); emulator.release(b) }
        case Int(RFM_EV_ENC):
            // the unit's detents are the matrix encoder's; a role's clockwise turn is enc[enc_id] += dir
            guard let role = table.encID.firstIndex(of: UInt8(i)), let k = FizzKnob(rawValue: role), v != 0 else { return }
            emulator.turn(k, by: v * Int(table.encDir[role] < 0 ? -1 : 1))
        case Int(RFM_EV_MASTER):
            emulator.setMaster(v)
        default:
            break
        }
    }

    // MARK: - FIZZ's LCD and LEDs -> the unit

    private func startScreen() {
        let frames = emulator.frames()
        lcdTask = Task { [weak self] in
            for await f in frames {
                guard let self else { return }
                let lcd = f.seq != self.lock.withLock({ self.lcdSeq }) ? Self.lcdSlots(f.lcd) : nil
                let leds = Self.leds(f.leds)
                self.queue.async {
                    guard let s = self.session, self.ending == nil else { return }
                    if let lcd {
                        lcd.withUnsafeBufferPointer { rfm_session_set_frame(s, $0.baseAddress) }
                        self.lock.withLock { self.lcdSeq = f.seq }
                    }
                    var l = leds
                    rfm_session_set_leds(s, &l)
                }
            }
        }
    }

    /// FIZZ's LCD (RGBA from RGB565) back to the panel's RGB565, each slot's two bytes big-endian in memory, as the
    /// unit takes them (screen.h). Exact: fizzhost's RGBA repeats the high bits.
    static func lcdSlots(_ img: CGImage) -> [UInt16]? {
        guard img.width == 240, img.height == 240, img.bitsPerPixel == 32, let d = img.dataProvider?.data,
              CFDataGetLength(d) >= 239 * img.bytesPerRow + 240 * 4 else { return nil }
        let px = CFDataGetBytePtr(d)!, row = img.bytesPerRow
        var out = [UInt16](repeating: 0, count: 240 * 240)
        for y in 0..<240 {
            for x in 0..<240 {
                let p = px + y * row + x * 4
                let v = UInt16(p[0] >> 3) << 11 | UInt16(p[1] >> 2) << 5 | UInt16(p[2] >> 3)
                out[y * 240 + x] = v.bigEndian
            }
        }
        return out
    }

    /// FIZZ's LED levels as LEDS' four planes: column c's byte, bit (line + 1) (the firmware's packed row bits).
    static func leds(_ l: FizzLEDs) -> rfm_leds_t {
        var out = rfm_leds_t()
        var planes = [[UInt8]](repeating: [UInt8](repeating: 0, count: 11), count: 4)   // lit mid dim faint
        for c in 0..<11 {
            for line in 0..<4 {
                let v = l.raw.count == 44 ? l.raw[c * 4 + line] : 0
                let plane: Int? = v >= FizzLEDs.litMin ? 0 : v >= 96 ? 1 : v >= 40 ? 2 : v > 0 ? 3 : nil
                if let plane { planes[plane][c] |= UInt8(1 << (line + 1)) }
            }
        }
        withUnsafeMutableBytes(of: &out.lit) { $0.copyBytes(from: planes[0]) }
        withUnsafeMutableBytes(of: &out.mid) { $0.copyBytes(from: planes[1]) }
        withUnsafeMutableBytes(of: &out.dim) { $0.copyBytes(from: planes[2]) }
        withUnsafeMutableBytes(of: &out.faint) { $0.copyBytes(from: planes[3]) }
        out.flags = l.playGreen > 0 ? UInt8(RFM_LEDS_F_PLAY_GREEN) : 0
        return out
    }
}
