// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// The FM-1's track edited and auditioned live (site/manager/live.js, the TRACK switch on):
//   - ParamPump: each value turned a PARAM with its ACK, one in flight (the unit holds one SysEx frame), the latest
//     value of each id winning while one waits; a MIDI sound's change the whole sound as an AUDITION in the same
//     queue. The EDIT screen's 500 ms track read-back (a track DUMP) skips while the pump is busy and for 700 ms after
//     the last PARAM, so a drag is not pulled back (followBlocked).
//   - TrackAudition: a preset or a sound heard on a track, nothing stored (STORE with LOAD + AUDITION); the track's
//     own sound read first and kept, so KEEP leaves it and CANCEL puts the track back exactly (its DUMP sent back:
//     trackFrame). An AUDITION whose reply was lost is checked by reading the track, else sent again (3 times).

import Foundation

// MARK: - PARAMs

public actor ParamPump {
    /// The read-back waits this long after the last PARAM (live.js: 0.7 s).
    public static let followQuietMs = 700.0
    static let soundKey = -1

    let link: FizzLink
    let clock: any FizzClock
    /// The track the PARAMs go to (0..5; -1: none yet).
    public private(set) var track = -1
    /// Off: nothing is sent (TRACK off); turned off, the queue is dropped.
    public private(set) var isOn = false
    /// When the last PARAM was queued or answered (the clock's ms).
    public private(set) var lastParamAt = -Double.infinity
    public private(set) var isPumping = false
    private var order: [Int] = []               // (ids in the order first queued, as a JS Map)
    private var values: [Int: Int] = [:]
    private var sound: [UInt8]?
    private var onOld: (@Sendable () -> Void)?
    private var onRefused: (@Sendable (Int, FizzRefusal) -> Void)?
    private var logger: (@Sendable (String) -> Void)?

    public init(link: FizzLink, clock: any FizzClock = SystemClock()) {
        self.link = link
        self.clock = clock
    }

    public func setTrack(_ t: Int) { track = t }

    public func setOn(_ on: Bool) {
        isOn = on
        if !on { clear() }
    }

    /// BAD COMMAND to a PARAM: this FIZZ cannot edit a track live (update FIZZ); the pump turns itself off.
    public func setOnOld(_ f: (@Sendable () -> Void)?) { onOld = f }
    /// A PARAM (id, or -1 for a whole sound) FIZZ refused otherwise (logged by the page).
    public func setOnRefused(_ f: (@Sendable (Int, FizzRefusal) -> Void)?) { onRefused = f }
    public func setLogger(_ f: (@Sendable (String) -> Void)?) { logger = f }

    /// Values waiting (the whole sound counts one).
    public var pending: Int { order.count }

    /// A value turned: queued (the latest of each id kept) and sent when the one in flight is answered.
    public func param(id: Int, value: Int) {
        guard isOn else { return }
        if values[id] == nil { order.append(id) }
        values[id] = value
        lastParamAt = clock.now
        kick()
    }

    /// A MIDI sound's change: the whole sound (an AUDITION frame for the track), the latest one.
    public func sound(_ frame: [UInt8]) {
        guard isOn else { return }
        if sound == nil { order.append(Self.soundKey) }
        sound = frame
        lastParamAt = clock.now
        kick()
    }

    /// The queue dropped (REVERT, another track, TRACK off).
    public func clear() {
        order.removeAll()
        values.removeAll()
        sound = nil
    }

    /// A read-back of the track now would race an edit: something queued or in flight, or a PARAM within 0.7 s.
    public func followBlocked() -> Bool {
        isPumping || !order.isEmpty || clock.now - lastParamAt < Self.followQuietMs
    }

    /// Marks now as the last PARAM (a REVERT, an AUDITION: the read-back waits as well).
    public func touch() { lastParamAt = clock.now }

    /// Returns when nothing is queued or in flight.
    public func idle() async {
        while isPumping || (isOn && !order.isEmpty) { try? await clock.sleep(ms: 5) }
    }

    private func kick() {
        guard !isPumping else { return }
        isPumping = true
        Task { await self.pump() }
    }

    private func pump() async {
        defer { isPumping = false }
        while isOn, !order.isEmpty {
            let id = order.removeFirst()
            if id == Self.soundKey {
                guard let f = sound else { continue }
                sound = nil
                let r = try? await link.store(f)
                lastParamAt = clock.now
                if let x = r?.refusal { logger?("AUDITION: \(x.name)"); onRefused?(id, x) }
                continue
            }
            guard let v = values.removeValue(forKey: id) else { continue }
            let r = try? await link.param(track: .index(track), id: id, value: v)
            lastParamAt = clock.now
            if let x = r?.refusal {
                if x.isOld {
                    logger?("PARAM: this FIZZ cannot edit a track live (BAD COMMAND)")
                    isOn = false
                    clear()
                    onOld?()
                    return
                }
                logger?("PARAM \(id): \(x.name)")
                onRefused?(id, x)
            }
        }
    }
}

// MARK: - AUDITION, KEEP, CANCEL

public actor TrackAudition {
    /// The track's own sound while something else is heard on it (the KEEP / CANCEL bar).
    public struct Pending: Sendable, Equatable {
        /// the track, 0..5
        public let track: Int
        /// what it plays now
        public var name: String
        /// its own sound: the DUMP read before
        public let own: [UInt8]
    }

    let link: FizzLink
    public private(set) var pending: Pending?
    private var logger: (@Sendable (String) -> Void)?

    public init(link: FizzLink) { self.link = link }

    public func setLogger(_ f: (@Sendable (String) -> Void)?) { logger = f }

    /// The track's DUMP (slot 1..6, 0 the selected one) and its sound read.
    public func readTrack(slot: Int = 0) async -> (frame: [UInt8], dump: FizzDump)? {
        guard let f = (try? await link.dumpTrack(slot: slot)) ?? nil, let d = FizzSysEx.parseDump(f) else { return nil }
        return (f, d)
    }

    /// A preset picked (its DUMP frame and LIST id) heard on the track (trackSlot 0: the selected one, 1..6),
    /// nothing stored. nil: no answer (or no track to read); a refusal leaves the bar up (CANCEL still puts it back).
    public func pick(preset frame: [UInt8], id: Int, name: String, trackSlot: Int = 0) async -> FizzResult<FizzStoreReply>? {
        guard let d = FizzSysEx.parseDump(frame) else { return nil }
        return await play(name: name, trackSlot: trackSlot) { t in
            try FizzSysEx.storeSound(name: d.name, blob: d.blob, drive: d.drive, track: t, pan: d.pan, chorus: d.chorus,
                                     delay: d.delay, reverb: d.reverb, tags: .mask(d.tags), audition: true,
                                     extras: FizzTrackExtrasIn(id: id, sound: 0, edited: false))
        }
    }

    /// Any sound heard on the track (an IMPORT row: live.js pickSound): `frame(track)` makes its AUDITION frame
    /// (extras id 0, edited, as the page gives).
    public func play(name: String, trackSlot: Int = 0,
                     frame: @Sendable (Int) throws -> [UInt8]) async -> FizzResult<FizzStoreReply>? {
        if pending == nil {
            guard let r = await readTrack(slot: trackSlot), let t = r.dump.track else { return nil }
            pending = Pending(track: t, name: "", own: r.frame)
        }
        guard var p = pending, let f = try? frame(p.track) else { return nil }
        p.name = name
        pending = p
        let x = await sure(f, name: FizzSysEx.parseDump(Self.dumped(f))?.name ?? name, track: p.track)
        if x?.isOK == true { logger?("TRACK \(p.track + 1) plays \(name) (AUDITION)") }
        return x
    }

    /// KEEP: the track keeps what it plays.
    public func keep() { pending = nil }

    /// CANCEL: the track's own sound put back. false: it did not go back yet (the bar stays: CANCEL again).
    public func cancel() async -> Bool {
        guard let p = pending else { return true }
        pending = nil
        let name = FizzSysEx.parseDump(p.own)?.name ?? ""
        let x = await sure(FizzSysEx.trackFrame(p.own), name: name, track: p.track)
        if x?.isOK == true { return true }
        if pending == nil { pending = p }
        return false
    }

    /// REVERT: a track put back exactly as its DUMP `base` was.
    public func revert(to base: [UInt8]) async -> FizzResult<FizzStoreReply>? {
        try? await link.store(FizzSysEx.trackFrame(base))
    }

    /// An AUDITION (it sets the track's sound: sending it again is harmless): its reply lost, the track read to see
    /// whether it took (its name), else sent again; three times at most.
    func sure(_ frame: [UInt8], name: String, track t: Int) async -> FizzResult<FizzStoreReply>? {
        for i in 0..<3 {
            if let x = try? await link.store(frame) { return x }
            if let d = await readTrack(slot: t + 1)?.dump, jsTrim(d.name) == jsTrim(name) {
                logger?("AUDITION: its reply lost, the track has it")
                return .ok(FizzStoreReply(status: 0, statusName: "OK", pool: false, slot: 0))
            }
            logger?("AUDITION: no answer, sent again (\(i + 1))")
        }
        return nil
    }

    /// A STORE frame with the DUMP flag, so parseDump reads it (the name as FIZZ will show it).
    static func dumped(_ f: [UInt8]) -> [UInt8] {
        var b = f
        guard b.count >= 9 else { return b }
        b[6] |= UInt8(FizzSysEx.FLAGS.dump)
        b[b.count - 2] = UInt8(FizzSysEx.checksum(b[5..<(b.count - 2)].map(Int.init)))
        return b
    }
}
