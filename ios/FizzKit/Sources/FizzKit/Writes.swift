// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// FIZZ's writes over the link (site/manager/fizzlink.js store, flush .. browse, param, dumpTrack, note; docs/SYSEX.md
// "The web manager's commands"): each with the page's timeout and tries. A write is sent once (only PARAM is sent
// again: a value set twice is harmless); a quiet link is woken by a STATUS first (wake). FIZZ's refusals (an ACK with a
// status, a STORE reply with one, or its ERROR for that command) come back as values (FizzResult.refused); a link
// that fails (lost, no answer) throws FizzLinkError.

import Foundation

/// FIZZ's no to a command (fizz_sysex.mjs STATUS, core/app/fizz_sx.h).
public enum FizzRefusal: Error, Sendable, Equatable, Hashable {
    case full                                   // 1: no free place
    case badLength, badChecksum, badPatch, badSlot, badVersion
    case updateFizz                             // 7 BAD COMMAND: an older FIZZ
    case busy                                   // 8: playing, or a job running
    case notFound                               // 9: nothing there
    case inUse(projects: Int, steps: Int)       // 10: DELETE without CONFIRM of something used
    case badTrack                               // 11
    case other(Int)

    /// A status as FIZZ sends it; a and b: its ACK's numbers (IN USE: the projects and the steps).
    public init(status: Int, a: Int = 0, b: Int = 0) {
        switch status {
        case FizzSysEx.ST.full: self = .full
        case FizzSysEx.ST.badLength: self = .badLength
        case FizzSysEx.ST.badChecksum: self = .badChecksum
        case FizzSysEx.ST.badPatch: self = .badPatch
        case FizzSysEx.ST.badSlot: self = .badSlot
        case FizzSysEx.ST.badVersion: self = .badVersion
        case FizzSysEx.ST.badCommand: self = .updateFizz
        case FizzSysEx.ST.busy: self = .busy
        case FizzSysEx.ST.notFound: self = .notFound
        case FizzSysEx.ST.inUse: self = .inUse(projects: a, steps: b)
        case FizzSysEx.ST.badTrack: self = .badTrack
        default: self = .other(status)
        }
    }

    public var status: Int {
        switch self {
        case .full: FizzSysEx.ST.full
        case .badLength: FizzSysEx.ST.badLength
        case .badChecksum: FizzSysEx.ST.badChecksum
        case .badPatch: FizzSysEx.ST.badPatch
        case .badSlot: FizzSysEx.ST.badSlot
        case .badVersion: FizzSysEx.ST.badVersion
        case .updateFizz: FizzSysEx.ST.badCommand
        case .busy: FizzSysEx.ST.busy
        case .notFound: FizzSysEx.ST.notFound
        case .inUse: FizzSysEx.ST.inUse
        case .badTrack: FizzSysEx.ST.badTrack
        case .other(let s): s
        }
    }

    /// fizz_sysex.mjs's name ("BUSY", "IN USE", "BAD COMMAND"), or the number.
    public var name: String { FizzSysEx.statusName(status) ?? String(status) }

    /// BAD COMMAND: this FIZZ does not know the command (update FIZZ).
    public var isOld: Bool { self == .updateFizz }
}

/// A write's answer: done (with what the unit said), or refused.
public enum FizzResult<Value: Sendable & Equatable>: Sendable, Equatable {
    case ok(Value)
    case refused(FizzRefusal)

    public var value: Value? { if case .ok(let v) = self { v } else { nil } }
    public var refusal: FizzRefusal? { if case .refused(let r) = self { r } else { nil } }
    public var isOK: Bool { value != nil }
}

extension FizzLink {
    /// The Bluetooth package's MIDI port as a computer names it ("FM-1 C1F0 Bluetooth"): over it a restore is refused
    /// (FZER 15) and the page offers none (fizzlink.js isBlePort).
    public static func isBlePort(_ name: String) -> Bool {
        name.trimmingCharacters(in: .whitespacesAndNewlines)
            .range(of: "\\sbluetooth$", options: [.regularExpression, .caseInsensitive]) != nil
    }

    /// Over FIZZ's Bluetooth port.
    public var viaBle: Bool { Self.isBlePort(portName) }

    // MARK: Phase 1: an ACK, or FIZZ's no

    static func acked(_ a: FizzAnswer) -> FizzResult<FizzAck> {
        switch a {
        case .reply(.ack(let k?)):
            return k.status == 0 ? .ok(k) : .refused(FizzRefusal(status: k.status, a: k.a, b: k.b))
        case .reply:
            return .refused(.other(-1))
        case .refused(let s, _, _):
            return .refused(FizzRefusal(status: s ?? -1))
        }
    }

    func command(_ f: [UInt8], _ cmd: UInt8, timeout: Double = 15000, retry: Int = 0) async throws -> FizzResult<FizzAck> {
        Self.acked(try await ask1(f, cmd: cmd, timeout: timeout, retry: retry))
    }

    /// TAGS: a MY or factory preset's FIZZ tags (ACK a = slot, b = tags).
    public func tags(source: FizzSource = .mine, slot: Int, tags: FizzTags) async throws -> FizzResult<FizzAck> {
        try await command(try FizzSysEx.tagsRequest(source: source, slot: slot, tags: tags), FizzSysEx.CMD.tags)
    }

    /// RENAME: a MY preset (source .mine) or a project (.project, slot 1..8).
    public func rename(source: FizzSource = .mine, slot: Int, name: String) async throws -> FizzResult<FizzAck> {
        try await command(try FizzSysEx.renameRequest(source: source, slot: slot, name: name), FizzSysEx.CMD.rename)
    }

    /// DELETE a MY preset or a pool place. Without confirm a used one answers IN USE (.inUse(projects, steps)) and
    /// nothing changes: ask, then send it again with confirm.
    public func remove(source: FizzSource = .mine, slot: Int, confirm: Bool = false) async throws -> FizzResult<FizzAck> {
        try await command(try FizzSysEx.deleteRequest(source: source, slot: slot, confirm: confirm), FizzSysEx.CMD.delete)
    }

    /// SWAP two places (MY PRESETS: their ids go with them; the pool: every pattern's locks too, a job, stopped
    /// only). Into an empty place: a move.
    public func swap(source: FizzSource = .mine, a: Int, b: Int) async throws -> FizzResult<FizzAck> {
        try await command(try FizzSysEx.swapRequest(source: source, a: a, b: b), FizzSysEx.CMD.swap)
    }

    /// POOL REF: pool place `slot` (0: the first free) references preset `id` (0: the place cleared). ACK a = the
    /// place, b = the id.
    public func poolRef(slot: Int = 0, id: Int) async throws -> FizzResult<FizzAck> {
        try await command(FizzSysEx.poolRefRequest(slot: slot, id: id), FizzSysEx.CMD.poolRef)
    }

    /// PROJECT: load p, new p, copy p onto q, delete p (1..8). A job, stopped only (BUSY while playing).
    public func project(_ op: FizzProjectOp, _ p: Int, _ q: Int = 0) async throws -> FizzResult<FizzAck> {
        try await command(FizzSysEx.projectRequest(op: op, p: p, q: q), FizzSysEx.CMD.project, timeout: 30000)
    }

    /// RENAME of project p (1..8).
    public func renameProject(_ p: Int, name: String) async throws -> FizzResult<FizzAck> {
        try await rename(source: .project, slot: p, name: name)
    }

    /// FLUSH: everything written (stopped only: BUSY while playing), then STATUS.
    public func flush() async throws -> FizzResult<FizzStatus> {
        let r = try await ask1(FizzSysEx.flushRequest(), cmd: FizzSysEx.CMD.flush, want: {
            if case .status(_?) = $0 { true } else { false }
        })
        switch r {
        case .reply(.status(let s?)): return .ok(s)
        case .reply(.ack(let k?)): return .refused(FizzRefusal(status: k.status, a: k.a, b: k.b))
        case .reply: return .refused(.other(-1))
        case .refused(let s, _, _): return .refused(FizzRefusal(status: s ?? -1))
        }
    }

    /// USERS as a result: who uses a MY preset (its projects' pools) or a pool place (this project's steps).
    public func usedBy(source: FizzSource, slot: Int) async throws -> FizzResult<FizzUsers> {
        switch try await users(source: source, slot: slot) {
        case .reply(.users(let u?)): return .ok(u)
        case .reply(.ack(let k?)): return .refused(FizzRefusal(status: k.status, a: k.a, b: k.b))
        case .reply: return .refused(.other(-1))
        case .refused(let s, _, _): return .refused(FizzRefusal(status: s ?? -1))
        }
    }

    /// BROWSE: the unit's own sound browser opened on a track at preset `id`, as PRESETS turned to it (OCT- on the
    /// unit cancels, OCT+ loads). ACK a = the track + 1, b = the browser's place; BUSY: a name being typed on the unit.
    public func browse(track: FizzTrack = .selected, id: Int) async throws -> FizzResult<FizzAck> {
        try await command(FizzSysEx.browseRequest(track: track, id: id), FizzSysEx.CMD.browse, timeout: 3000)
    }

    /// PARAM with its ACK: one value of a track's sound, as a knob turned on the unit (1 s, sent again once: a value
    /// set twice is harmless). One at a time: ParamPump.
    public func param(track: FizzTrack, id: Int, value: Int) async throws -> FizzResult<FizzAck> {
        try await command(FizzSysEx.paramRequest(track: track, id: id, value: value, ack: true), FizzSysEx.CMD.param,
                          timeout: 1000, retry: 1)
    }

    // MARK: STORE

    /// STORE / STORE6 (a preset saved or imported, an AUDITION, a track put back): FIZZ's STORE reply (its slot,
    /// pool), or its no (FULL, BUSY: a used place while playing, BAD TRACK ..). Sent once; 3 s.
    public func store(_ frame: [UInt8]) async throws -> FizzResult<FizzStoreReply> {
        await wake()
        return try await op(timeout: 3000, start: { frame }) { p -> Step<FizzResult<FizzStoreReply>> in
            switch FizzSysEx.parseReply(p) {
            case .store(let r)?:
                return .done((r.status ?? -1) == 0 ? .ok(r) : .refused(FizzRefusal(status: r.status ?? -1)))
            case .error(let e)? where e.command == Int(FizzSysEx.CMD.store) || e.command == Int(FizzSysEx.CMD.store6):
                return .done(.refused(FizzRefusal(status: e.status ?? -1)))
            default:
                return .notMine
            }
        }
    }

    // MARK: a track's sound

    /// A track's sound (slot 1..6, 0 the selected one): its DUMP frame with its extras; nil when this FIZZ has none
    /// (an older one: BAD SLOT) or it did not answer (800 ms, 2 tries more).
    public func dumpTrack(slot: Int = 0) async throws -> [UInt8]? {
        try await op(timeout: 800, retry: 2, partial: { nil }, start: {
            try FizzSysEx.dumpRequest(source: .track, slot: slot)
        }) { p in
            if let d = FizzSysEx.parseDump(p), d.source == "track", slot == 0 || d.track == slot - 1 { return .done(d.frame) }
            if case .error(let e)? = FizzSysEx.parseReply(p), e.command == Int(FizzSysEx.CMD.dump) { return .done(nil) }
            return .notMine
        }
    }

    /// A note to the FM-1 (no reply; not held in the queue, but paced as every frame): channel 0..15.
    public func note(channel: Int, note n: Int, velocity: Int) async throws {
        let b: [UInt8] = velocity > 0 ? [UInt8(0x90 | (channel & 15)), UInt8(n & 127), UInt8(velocity & 127)]
            : [UInt8(0x80 | (channel & 15)), UInt8(n & 127), 0]
        try await send(b)
    }

    /// One channel message (status 0x80..0xEF) or real-time byte to the FM-1's MIDI in, paced as every frame (a MIDI
    /// controller's CCs and clock on PLAY); anything else is not sent.
    public func midiIn(_ bytes: [UInt8]) async throws {
        guard let s = bytes.first, (s >= 0x80 && s < 0xF0) || s >= 0xF8 else { return }
        try await send(bytes)
    }
}
