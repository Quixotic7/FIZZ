// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// FIZZ's own firmware core running in the app: an emulated FM-1. The core (CFizzCore, built by ios/engine/build_core.sh)
// is single-threaded and exists once per process; fizzhost.c serialises it with its clock token. Its clock runs from
// the audio render callback (start(sound: true)), from a timer with the sound dropped (start(sound: false), or when
// audio cannot start), or by hand (run(ms:), render(frames:), the tests). A service timer on its own queue moves what
// the core sends (MIDI to the transports, the LCD and the LEDs to the screens) and saves the flash after it changed.

import CFizzCore
import CoreGraphics
import FizzKit
import Foundation

public final class FizzEmulator: @unchecked Sendable {
    public enum ClockMode: Sendable, Equatable {
        case stopped    // nothing runs it (run(ms:) / render(frames:) do, by hand)
        case audio      // the audio callback runs it
        case timer      // a timer runs it in real time, the sound dropped
    }

    /// The compiled-in core (version, licence, credits, FIZZ's button labels).
    public let info = FizzCoreInfo.current
    /// Where the flash is kept between launches (nil: nowhere).
    public let storage: FlashStorage?
    /// What the update handshake answers ("FM-1_46..": FizzKit's FM1Identity.isFizz).
    public let identity: String
    /// Ms of quiet after the flash changed before it is saved (and at most `saveMaxMs` after the first change).
    public var saveQuietMs: Double = 1500
    public var saveMaxMs: Double = 10_000
    /// Called after each save (the bytes' dirty count), and with what went wrong.
    public var log: (@Sendable (String) -> Void)?

    private let lock = NSLock()               // the Swift state below
    private let pumpLock = NSLock()           // the pump's state (one pump at a time)
    private let snapLock = NSLock()           // one flash copy at a time
    private let queue = DispatchQueue(label: "fizz.emulator.service", qos: .userInitiated)
    private static let onQueue = DispatchSpecificKey<Bool>()

    private var booted = false
    private var clock: ClockMode = .stopped
    private var resumeMode: ClockMode?
    private var timer: DispatchSourceTimer?
    private var audio: EmulatorAudio?
    private var timerBase: (wall: Double, dev: UInt32) = (0, 0)
    private var table: PanelTable?
    private var masterValue: Int?
    private var transports: [Weak] = []
    private var subscribers: [UUID: AsyncStream<FizzPanelFrame>.Continuation] = [:]
    private var observers: [NSObjectProtocol] = []

    // the pump's (pumpLock)
    private var sx: [UInt8]?
    private var rgba = [UInt8](repeating: 0, count: 240 * 240 * 4)
    private var lcdSeen: UInt32 = 0, ledSeen: UInt32 = 0
    private var image: CGImage?
    private var ledsNow = FizzLEDs.off
    private var savedDirty: UInt32 = 0, seenDirty: UInt32 = 0, dirtyAt = 0.0, unsavedSince: Double?

    private struct Weak { weak var t: EmulatorTransport? }

    public init(storage: FlashStorage? = .documents, identity: String = "FM-1_46999999") {
        self.storage = storage
        self.identity = identity
        queue.setSpecific(key: Self.onQueue, value: true)
        // the app sent to the background: the clock and the sound stop, the flash is saved; back: they start again
        let nc = NotificationCenter.default
        observers.append(nc.addObserver(forName: Notification.Name("UIApplicationDidEnterBackgroundNotification"),
                                        object: nil, queue: nil) { [weak self] _ in self?.suspend() })
        observers.append(nc.addObserver(forName: Notification.Name("UIApplicationWillEnterForegroundNotification"),
                                        object: nil, queue: nil) { [weak self] _ in self?.resume() })
    }

    deinit {
        for o in observers { NotificationCenter.default.removeObserver(o) }
    }

    // MARK: - the one core

    nonisolated(unsafe) private static weak var owner: FizzEmulator?
    private static let ownerLock = NSLock()

    /// The core is the process's one: an emulator that boots takes it from another (stopped, its transports lost).
    private func claim() {
        let prev: FizzEmulator? = Self.ownerLock.withLock {
            let p = Self.owner
            Self.owner = self
            return p === self ? nil : p
        }
        prev?.release()
    }

    private func release() {
        stopClock()
        lock.withLock { booted = false }
        for t in openTransports() { t.lostByEmulator() }
    }

    // MARK: - power

    public var isBooted: Bool { lock.withLock { booted } }
    public var mode: ClockMode { lock.withLock { clock } }

    /// Power on from the saved flash (a fresh one when nothing is saved).
    public func boot() { boot(flash: storage?.load(size: info.flashSize)) }

    /// Power on (again) from a flash image (nil: a fresh, erased flash). The clock keeps its mode.
    public func boot(flash: [UInt8]?) {
        claim()
        pumpLock.withLock {
            fzh_lock()
            if let flash {
                flash.withUnsafeBufferPointer { fzh_boot($0.baseAddress, UInt32($0.count)) }
            } else {
                fzh_boot(nil, 0)
            }
            let t = PanelTable.read()
            fzh_unlock()
            sx = nil
            savedDirty = fzh_flash_dirty(); seenDirty = savedDirty; unsavedSince = nil
            lock.withLock {
                table = t
                booted = true
                timerBase = (Self.nowMs, fzh_ms())
            }
        }
        if let m = lock.withLock({ masterValue }) { fzh_master(Int32(m)) }
        pump()
    }

    /// A fresh flash: the saved one removed, FIZZ started again on an erased store.
    public func reset() {
        storage?.remove()
        boot(flash: nil)
    }

    /// Start the clock: from the sound (falls back to the timer when audio cannot start), or the timer alone.
    @discardableResult
    public func start(sound: Bool = true) -> ClockMode {
        if !isBooted { boot() }
        stopClock()
        var m = ClockMode.timer
        if sound {
            do {
                let a = try EmulatorAudio()
                lock.withLock { audio = a }
                m = .audio
            } catch {
                log?("emulator: no sound (\(error)); the clock runs on a timer")
            }
        }
        lock.withLock {
            clock = m
            timerBase = (Self.nowMs, fzh_ms())
        }
        let t = DispatchSource.makeTimerSource(queue: queue)
        t.schedule(deadline: .now(), repeating: .milliseconds(4), leeway: .milliseconds(1))
        t.setEventHandler { [weak self] in self?.service() }
        lock.withLock { timer = t }
        t.resume()
        return m
    }

    /// Stop the clock (and the sound); the flash is saved.
    public func stop() {
        stopClock()
        pump()
        save()
    }

    /// Stop, save, and power the core down (the app is going away).
    public func shutdown() {
        stop()
        guard isBooted else { return }
        fzh_lock()
        fzh_shutdown()
        fzh_unlock()
        save(force: true)
        lock.withLock { booted = false }
    }

    /// The app went to the background: everything stops (resume() starts it again as it was).
    public func suspend() {
        let m = mode
        guard m != .stopped else { return }
        lock.withLock { resumeMode = m }
        stop()
    }

    public func resume() {
        guard let m = lock.withLock({ () -> ClockMode? in defer { resumeMode = nil }; return resumeMode }) else { return }
        start(sound: m == .audio)
    }

    private func stopClock() {
        let (t, a) = lock.withLock { () -> (DispatchSourceTimer?, EmulatorAudio?) in
            defer { timer = nil; audio = nil; clock = .stopped }
            return (timer, audio)
        }
        a?.stop()
        if let t {
            t.cancel()
            if DispatchQueue.getSpecific(key: Self.onQueue) == nil { queue.sync {} }   // (a service running finishes)
        }
    }

    // MARK: - the clock by hand (stopped mode)

    /// n device milliseconds (their sound dropped), then what the core sent moved on.
    public func run(ms: Int) {
        var left = ms
        while left > 0 {
            let k = min(left, 20)
            _ = fzh_run(UInt32(k))
            pump()
            left -= k
        }
    }

    /// n frames of sound at `rate` (44100: the device's own samples), running the clock as far as they need.
    public func render(frames n: Int, rate: Double = FizzCoreInfo.sampleRate) -> (left: [Float], right: [Float]) {
        var l = [Float](repeating: 0, count: n), r = [Float](repeating: 0, count: n)
        _ = fzh_render(&l, &r, UInt32(n), rate)
        pump()
        return (l, r)
    }

    /// Device milliseconds since power-on.
    public var deviceMs: Int { Int(fzh_ms()) }

    // MARK: - the panel

    public func press(_ b: FizzButton) { fzh_button(UInt32(b.rawValue), 1) }
    public func release(_ b: FizzButton) { fzh_button(UInt32(b.rawValue), 0) }
    /// Down and up within one device ms (the core sees a tap).
    public func tap(_ b: FizzButton) { press(b); release(b) }
    /// By FIZZ's label (or the FM-1's printed one); false: no such button.
    @discardableResult
    public func press(label: String) -> Bool { FizzButton(label: label).map { press($0) } != nil }
    @discardableResult
    public func release(label: String) -> Bool { FizzButton(label: label).map { release($0) } != nil }
    /// Detents, + clockwise.
    public func turn(_ k: FizzKnob, by detents: Int) { fzh_enc(UInt32(k.rawValue), Int32(clamping: detents)) }
    /// The MASTER pot, 0..1023 (kept across power-ons).
    public func setMaster(_ v: Int) {
        let c = max(0, min(1023, v))
        lock.withLock { masterValue = c }
        fzh_master(Int32(c))
    }
    /// Note key 0 (F3) .. 26 (G5), velocity 1..127.
    public func noteOn(key: Int, velocity: Int = 100) {
        guard (0..<27).contains(key) else { return }
        fzh_key(UInt32(key), 1, UInt32(max(1, min(127, velocity))))
    }
    public func noteOff(key: Int) {
        guard (0..<27).contains(key) else { return }
        fzh_key(UInt32(key), 0, 0)
    }
    /// Every key and button let go, the input not yet applied dropped.
    public func releaseAll() { fzh_input_clear() }

    /// The LEDs as last published.
    public var leds: FizzLEDs { pumpLock.withLock { refreshLEDs(); return ledsNow } }

    /// The LCD as last published (240 x 240).
    public var lcdImage: CGImage? { pumpLock.withLock { refreshLCD(); return image } }

    /// The LCD and the LEDs each time they change (at most ~60 a second; the newest kept).
    public func frames() -> AsyncStream<FizzPanelFrame> {
        let (s, c) = AsyncStream.makeStream(of: FizzPanelFrame.self, bufferingPolicy: .bufferingNewest(1))
        let id = UUID()
        lock.withLock { subscribers[id] = c }
        c.onTermination = { [weak self] _ in self?.lock.withLock { _ = self?.subscribers.removeValue(forKey: id) } }
        if let f = pumpLock.withLock({ () -> FizzPanelFrame? in
            refreshLCD(); refreshLEDs()
            return image.map { FizzPanelFrame(lcd: $0, leds: ledsNow, seq: lcdSeen) }
        }) { c.yield(f) }
        return s
    }

    /// The core's own one-line state ("keys: A1 stopped, 120 BPM, ..").
    public func uiInfo() -> String {
        var b = [CChar](repeating: 0, count: 512)
        fzh_lock()
        fzh_ui_info(&b, UInt32(b.count))
        fzh_unlock()
        return String(decoding: b.prefix { $0 != 0 }.map { UInt8(bitPattern: $0) }, as: UTF8.self)
    }

    /// FIZZ's transport running.
    public var isPlaying: Bool { uiInfo().contains(" playing,") }

    // MARK: - the cost

    public struct Stats: Sendable, Equatable {
        public let blocks: UInt64, msRun: UInt64
        /// mean and longest time of one core audio() block (128 frames, 2.9 ms of sound)
        public let audioMeanUs: Double, audioMaxUs: Double
        /// mean and longest device ms (tick, frame or idle, audio, the queues)
        public let msMeanUs: Double, msMaxUs: Double
        /// the clock's share of real time (a device ms takes msMeanUs of 1000 us)
        public var load: Double { msMeanUs / 1000 }
    }

    public var stats: Stats {
        var s = fzh_stats_t()
        fzh_stats(&s)
        return Stats(blocks: s.blocks, msRun: s.ms_run,
                     audioMeanUs: s.blocks == 0 ? 0 : Double(s.audio_ns) / Double(s.blocks) / 1000,
                     audioMaxUs: Double(s.audio_max_ns) / 1000,
                     msMeanUs: s.ms_run == 0 ? 0 : Double(s.clock_ns) / Double(s.ms_run) / 1000,
                     msMaxUs: Double(s.clock_max_ns) / 1000)
    }

    public func resetStats() { fzh_stats_reset() }

    // MARK: - the flash

    /// The whole flash as it is now (FIZZ's lazy writes made first).
    public func flashImage() -> [UInt8] { copyFlash().bytes }

    func copyFlash() -> (bytes: [UInt8], dirty: UInt32) {
        snapLock.withLock {
            var b = [UInt8](repeating: 0, count: info.flashSize)
            var d: UInt32 = 0
            if lock.withLock({ clock == .audio && audio?.isRunning == true }) {
                _ = fzh_flash_take(&b, &d)                  // (a copy asked for before and never taken)
                fzh_flash_request()
                let end = Self.nowMs + 500
                while Self.nowMs < end {
                    if fzh_flash_take(&b, &d) != 0 { return (b, d) }
                    usleep(500)
                }
            }
            fzh_lock()
            d = fzh_flash_copy(&b)
            fzh_unlock()
            return (b, d)
        }
    }

    /// Save the flash now if it changed since the last save (force: whatever it holds).
    public func save(force: Bool = false) {
        guard let storage, isBooted else { return }
        let dirty = fzh_flash_dirty()
        guard force || dirty != pumpLock.withLock({ savedDirty }) else { return }
        let (b, d) = copyFlash()
        do {
            try storage.save(b)
            pumpLock.withLock { savedDirty = d; seenDirty = d; unsavedSince = nil }
            log?("emulator: flash saved (\(d))")
        } catch {
            log?("emulator: the flash was not saved: \(error)")
        }
    }

    // MARK: - MIDI

    /// One complete message into the core (a SysEx F0 .. F7, a channel or real-time message); false: its queue is full.
    @discardableResult
    public func midiIn(_ bytes: [UInt8]) -> Bool {
        guard let s = bytes.first else { return true }
        let pkts: [UInt32]
        if s == 0xF0 {
            pkts = FizzSysEx.usbPackets(bytes)
        } else if s >= 0x80 && s < 0xF0 {
            let d1 = bytes.count > 1 ? UInt32(bytes[1] & 0x7F) : 0, d2 = bytes.count > 2 ? UInt32(bytes[2] & 0x7F) : 0
            pkts = [UInt32(s >> 4) | UInt32(s) << 8 | d1 << 16 | d2 << 24]
        } else if s >= 0xF8 {
            pkts = [15 | UInt32(s) << 8]
        } else {
            return true
        }
        for p in pkts where fzh_midi_in(p) == 0 { return false }
        return true
    }

    /// A new port onto the emulated unit (FizzLink talks to it as to an FM-1).
    public func makeTransport(name: String = EmulatorLinkSource.defaultPortName) -> EmulatorTransport {
        let t = EmulatorTransport(emulator: self, name: name)
        lock.withLock {
            transports = transports.filter { $0.t != nil }
            transports.append(Weak(t: t))
        }
        return t
    }

    func openTransports() -> [EmulatorTransport] { lock.withLock { transports.compactMap(\.t) } }

    // MARK: - the service (its queue, every 4 ms while the clock runs)

    private func service() {
        // (a clock started while another emulator took the core: stopped here, the core left to its owner)
        guard Self.ownerLock.withLock({ Self.owner === self }) else { stopClock(); return }
        if lock.withLock({ clock == .timer }) {
            // the timer clock: the device ms due by the wall clock (a late timer catches up 100 ms at most)
            let now = Self.nowMs
            let (base, dev) = lock.withLock { timerBase }
            var due = Int(now - base) - Int(fzh_ms() &- dev)
            if due > 100 {
                lock.withLock { timerBase = (now - Double(fzh_ms() &- dev), dev) }
                due = 100
            }
            if due > 0 { _ = fzh_run(UInt32(due)) }
        }
        pump()
        persistLater()
    }

    /// What the core sent: MIDI to the transports, the panel to the screens.
    func pump() {
        var msgs: [[UInt8]] = []
        var frame: FizzPanelFrame?
        pumpLock.withLock {
            var buf = [UInt32](repeating: 0, count: 512)
            while true {
                let n = Int(fzh_midi_out(&buf, UInt32(buf.count)))
                for p in buf[0..<n] { take(p, into: &msgs) }
                if n < buf.count { break }
            }
            if lock.withLock({ !subscribers.isEmpty }) {
                let a = refreshLCD(), b = refreshLEDs()
                if a || b, let image { frame = FizzPanelFrame(lcd: image, leds: ledsNow, seq: lcdSeen) }
            }
        }
        if !msgs.isEmpty {
            let ts = openTransports()
            for m in msgs { for t in ts { t.deliver(m) } }
        }
        if let frame {
            for c in lock.withLock({ Array(subscribers.values) }) { c.yield(frame) }
        }
    }

    // a USB-MIDI packet out: SysEx reassembled (CIN 4 goes on, 5..7 end); the rest not passed on
    private func take(_ p: UInt32, into msgs: inout [[UInt8]]) {
        let cin = p & 15, b = [UInt8(p >> 8 & 0xFF), UInt8(p >> 16 & 0xFF), UInt8(p >> 24 & 0xFF)]
        guard cin >= 4 && cin <= 7 else { return }
        if b[0] == 0xF0 { sx = [] }
        let k = cin == 4 ? 3 : Int(cin) - 4
        sx?.append(contentsOf: b[0..<k])
        if cin != 4, let m = sx {
            if m.first == 0xF0 && m.last == 0xF7 { msgs.append(m) }
            sx = nil
        }
    }

    @discardableResult
    private func refreshLCD() -> Bool {
        guard fzh_lcd_rgba(&rgba, &lcdSeen) != 0 else { return false }
        image = Self.makeImage(rgba)
        return true
    }

    @discardableResult
    private func refreshLEDs() -> Bool {
        var raw = [UInt8](repeating: 0, count: 44)
        var green: UInt8 = 0
        let n = fzh_leds(&raw, &green)
        guard n != ledSeen, let t = lock.withLock({ table }) else { return false }
        ledSeen = n
        ledsNow = t.leds(raw: raw, playGreen: green)
        return true
    }

    static func makeImage(_ rgba: [UInt8]) -> CGImage? {
        guard let p = CGDataProvider(data: Data(rgba) as CFData) else { return nil }
        return CGImage(width: 240, height: 240, bitsPerComponent: 8, bitsPerPixel: 32, bytesPerRow: 960,
                       space: CGColorSpace(name: CGColorSpace.sRGB) ?? CGColorSpaceCreateDeviceRGB(),
                       bitmapInfo: CGBitmapInfo(rawValue: CGImageAlphaInfo.noneSkipLast.rawValue), provider: p,
                       decode: nil, shouldInterpolate: false, intent: .defaultIntent)
    }

    // the flash saved once it has been quiet saveQuietMs (or saveMaxMs after its first change)
    private func persistLater() {
        guard storage != nil else { return }
        let d = fzh_flash_dirty(), now = Self.nowMs
        let due: Bool = pumpLock.withLock {
            guard d != savedDirty else { unsavedSince = nil; return false }
            if d != seenDirty { seenDirty = d; dirtyAt = now }
            if unsavedSince == nil { unsavedSince = now }
            return now - dirtyAt >= saveQuietMs || now - (unsavedSince ?? now) >= saveMaxMs
        }
        if due { save() }
    }

    static var nowMs: Double { Double(DispatchTime.now().uptimeNanoseconds) / 1e6 }
}
