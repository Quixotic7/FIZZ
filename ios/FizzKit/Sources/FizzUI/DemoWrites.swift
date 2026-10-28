// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// The demo unit's writes (docs/SYSEX.md "The web manager's commands", core/app/fizz_sx.c as the page sees it): STORE /
// STORE6 (into MY PRESETS, the pool, a track; AUDITION), TAGS, RENAME, DELETE (IN USE without CONFIRM), SWAP, POOL
// REF, USERS, PROJECT, FLUSH, PARAM, BROWSE, a track's DUMP, and the restore (FZSR: the image pulled twice with 0x30
// requests, each asked again every second, then FZOK or FZER). Its state changes as a unit's would; BUSY while
// "playing" (DemoUnit.setPlaying) for what needs the unit stopped. A demo, not FIZZ: no undo, one pool a project, the
// steps using a pool place made up from its number.

import Foundation
import FizzData
import FizzKit

/// One of the demo unit's tracks: the sound it plays.
public struct DemoTrack: Sendable, Equatable {
    public var name: String, blob: [UInt8], drive: Int, pan: Int, chorus: Int, delay: Int, reverb: Int, tags: Int
    /// the preset it plays (0: its own sound), its pool sound, edited since
    public var id: Int, sound: Int, edited: Bool

    init(_ p: DemoPreset) {
        name = p.name; blob = p.blob ?? DemoUnit.blob(p); drive = p.drive; pan = p.pan; chorus = p.chorus
        delay = p.delay; reverb = p.reverb; tags = p.tags; id = p.id; sound = 0; edited = false
    }
}

public struct DemoProject: Sendable, Equatable {
    public var name: String, pages: Int, demo: Bool
}

/// What a demo unit holds now. Its lock guards everything; DemoReplies works under it.
public final class DemoState: @unchecked Sendable {
    let lock = NSLock()
    var mine: [Int: DemoPreset] = [:]
    var factory: [DemoPreset]
    var pool: [Int: Int] = [:]                  // the open project's: place -> preset id
    var pools: [Int: [Int: Int]] = [:]          // the others', by project
    var projects: [DemoProject?]
    var current: Int
    var playing = false, dirty = false
    var written = 4711
    var version = 0                             // the store's records changed (the image made again)
    var nextId = 0x2000
    var tracks: [DemoTrack]
    var selected = 2
    var browse: (track: Int, id: Int, until: Date)?
    var image: [UInt8]?
    var imageVersion = -1

    init(mine: [DemoPreset], factory: [DemoPreset], pool: [(place: Int, preset: DemoPreset)],
         projects: [(name: String, pages: Int, demo: Bool)?], current: Int) {
        for p in mine { self.mine[p.slot] = p }
        self.factory = factory
        for e in pool { self.pool[e.place] = e.preset.id }
        self.projects = projects.map { $0.map { DemoProject(name: $0.name, pages: $0.pages, demo: $0.demo) } }
        self.current = current
        tracks = (0..<6).map { t in mine.indices.contains(t) ? DemoTrack(mine[t]) : DemoTrack(DemoPreset(slot: 0, id: 0, name: "TRACK \(t + 1)", tags: 0, algorithm: 1)) }
        nextId = max(0x2000, (mine.map(\.id).max() ?? 0) + 1)
    }

    var mineList: [DemoPreset] { mine.keys.sorted().map { mine[$0]! } }

    func preset(id: Int) -> DemoPreset? {
        mine.values.first { $0.id == id } ?? factory.first { $0.id == id }
    }

    /// The steps that lock a pool place (made up: every third place, a few steps).
    static func steps(_ place: Int) -> Int { place % 3 == 0 ? place % 7 + 1 : 0 }

    /// This project's tracks that play a preset (bit t).
    func tracksPlaying(_ id: Int) -> Int {
        tracks.enumerated().reduce(0) { $0 | ($1.element.id == id && id != 0 ? 1 << $1.offset : 0) }
    }

    /// A MY preset's users: each project whose pool references it, with the steps; this project's tracks.
    func users(ofPreset id: Int) -> (projects: [(Int, Int)], tracks: Int) {
        var out: [(Int, Int)] = []
        let all = pools.merging([current: pool]) { $1 }
        for p in all.keys.sorted() {
            let places = all[p]!.filter { $0.value == id }.map(\.key)
            if !places.isEmpty { out.append((p, places.reduce(0) { $0 + Self.steps($1) })) }
        }
        return (out, tracksPlaying(id))
    }

    /// A pool place's users: this project's steps on it, the tracks playing its sound.
    func users(ofPlace k: Int) -> (projects: [(Int, Int)], tracks: Int) {
        guard let id = pool[k] else { return ([], 0) }
        let n = Self.steps(k)
        return (n > 0 ? [(current, n)] : [], tracksPlaying(id))
    }

    func bump() {
        version += 1
        written += 1
        dirty = true
    }

    /// Time goes by: the unit's browser closes (as OCT+ on the unit: the track loads the preset).
    func tick() {
        guard let b = browse, Date() >= b.until else { return }
        browse = nil
        if let p = preset(id: b.id) { tracks[b.track] = DemoTrack(p) }
    }

    /// A restored image's presets and projects taken (fizzstore.js readRegion's view of it).
    func restored(_ img: [UInt8], _ sum: FizzRegionSummary?) {
        if let sum {
            mine = [:]
            for p in sum.presets {
                var d = DemoPreset(slot: p.slot, id: p.id, name: p.name, tags: p.tagMask,
                                   algorithm: p.blob.count > 2 ? Int(p.blob[2]) + 1 : 1)
                d.blob = p.blob.count == FizzSysEx.blob6Len ? p.blob : nil
                d.drive = p.drive; d.pan = p.pan; d.imported = p.imported
                d.chorus = p.sends.chorus; d.delay = p.sends.delay; d.reverb = p.sends.reverb
                mine[p.slot] = d
            }
            var ps = [DemoProject?](repeating: nil, count: 8)
            for p in sum.projects where p.slot >= 1 && p.slot <= 8 {
                ps[p.slot - 1] = DemoProject(name: p.name, pages: projects[p.slot - 1]?.pages ?? 1, demo: p.demo)
            }
            projects = ps
            if projects[current - 1] == nil { current = (projects.firstIndex { $0 != nil } ?? 0) + 1 }
            pool = pool.filter { preset(id: $0.value) != nil }
            nextId = max(nextId, (mine.values.map(\.id).max() ?? 0) + 1)
        }
        bump()
        dirty = false
        image = img
        imageVersion = version
    }
}

extension DemoUnit {
    /// The demo unit playing: what needs it stopped answers BUSY (FZER 10 for a restore).
    public func setPlaying(_ on: Bool) { state.lock.withLock { state.playing = on } }
    public var isPlaying: Bool { state.lock.withLock { state.playing } }
    /// MY PRESETS now, in slot order.
    public var minePresets: [DemoPreset] { state.lock.withLock { state.mineList } }
    /// The open project's pool now: place -> preset id.
    public var poolPlaces: [Int: Int] { state.lock.withLock { state.pool } }
    /// The projects now (1..8: nil an empty slot) and the open one.
    public var projectSlots: (list: [DemoProject?], current: Int) { state.lock.withLock { (state.projects, state.current) } }
    /// The tracks' sounds now.
    public var trackSounds: [DemoTrack] { state.lock.withLock { state.tracks } }
}

// MARK: - The replies to the writes

extension DemoReplies {
    static func ack(_ cmd: UInt8, _ st: Int, _ a: Int = 0, _ b: Int = 0) -> [UInt8] {
        frame(FizzSysEx.REPLY.ack, [Int(cmd), st] + r14(a) + r14(b))
    }
    static func err(_ cmd: UInt8, _ st: Int) -> [UInt8] { frame(FizzSysEx.REPLY.error, [st, Int(cmd)]) }
    static func stored(_ st: Int, pool: Bool = false, slot: Int = 0) -> [UInt8] {
        frame(FizzSysEx.REPLY.store, [st, pool ? 1 : 0] + r14(slot))
    }

    /// A STORE frame as parseDump reads a DUMP (the flag set).
    static func asDump(_ f: [UInt8]) -> FizzDump? {
        var b = f
        guard b.count >= 9 else { return nil }
        b[6] |= UInt8(FizzSysEx.FLAGS.dump)
        let sum = b[5..<(b.count - 2)].reduce(0) { $0 + Int($1) }
        b[b.count - 2] = UInt8((0x80 - (sum & 0x7f)) & 0x7f)
        return FizzSysEx.parseDump(b)
    }

    /// The write commands (under the state's lock); nil: not one.
    func write(_ b: [UInt8], _ s: DemoState) -> [[UInt8]]? {
        let cmd = b[5], n = b.count - 2                     // (n: the checksum's index)
        let at = { (o: Int) -> Int in o < n ? Int(b[o]) : 0 }
        let u14 = { (o: Int) -> Int in (at(o) << 7) | at(o + 1) }
        let u21 = { (o: Int) -> Int in (at(o) << 14) | (at(o + 1) << 7) | at(o + 2) }
        typealias C = FizzSysEx.CMD
        let st = FizzSysEx.ST.self
        switch cmd {
        case C.store, C.store6:
            return [store(b, s)]
        case C.flush:
            if s.playing { return [Self.err(cmd, st.busy)] }
            s.dirty = false
            return [status(s)]
        case C.tags:
            let src = at(6), slot = u14(7), tags = at(9) & 0x7f
            if src == 0, s.mine[slot] != nil { s.mine[slot]!.tags = tags }
            else if src == 1, let k = s.factory.firstIndex(where: { $0.slot == slot }) { s.factory[k].tags = tags }
            else { return [Self.ack(cmd, src > 1 ? st.badSlot : st.notFound)] }
            s.bump()
            return [Self.ack(cmd, st.ok, slot, tags)]
        case C.rename:
            let src = at(6), slot = u14(7)
            var name = String(decoding: (9..<min(21, n)).map { b[$0] }, as: UTF8.self)
            while name.last == " " { name.removeLast() }
            if src == 0, s.mine[slot] != nil { s.mine[slot]!.name = name }
            else if src == FizzSysEx.sourceProject, (1...8).contains(slot), s.projects[slot - 1] != nil {
                s.projects[slot - 1]!.name = name
            } else { return [Self.ack(cmd, src == 0 || src == 3 ? st.notFound : st.badSlot)] }
            s.bump()
            return [Self.ack(cmd, st.ok, slot)]
        case C.delete:
            return [delete(src: at(6), slot: u14(7), confirm: at(9) & 1 != 0, s)]
        case C.swap:
            return [swap(src: at(6), a: u14(7), b: u14(9), s)]
        case C.poolRef:
            return [poolRef(slot: u14(6), id: u21(8), s)]
        case C.users:
            let src = at(6), slot = u14(7)
            let u: (projects: [(Int, Int)], tracks: Int)
            if src == 0, let p = s.mine[slot] { u = s.users(ofPreset: p.id) }
            else if src == 2, s.pool[slot] != nil { u = s.users(ofPlace: slot) }
            else { return [Self.err(cmd, st.notFound)] }
            return [Self.frame(FizzSysEx.REPLY.users, [src] + Self.r14(slot) + [u.projects.count] +
                               u.projects.flatMap { [$0.0 - 1] + Self.r14($0.1) } + [u.tracks])]
        case C.project:
            return [project(op: at(6), p: at(7), q: at(8), s)]
        case C.param:
            let t = at(6) == 127 ? s.selected : at(6), id = at(7), v = u21(8) - (1 << 20), wantAck = at(11) & 1 != 0
            var r = st.ok
            if t > 5 { r = st.badTrack } else if id >= DemoBlob.mins.count { r = st.notFound } else {
                DemoBlob.set(&s.tracks[t].blob, id: id, value: v)
                if id == 73 { s.tracks[t].drive = max(0, min(127, v)) }
                s.tracks[t].edited = true
            }
            return wantAck || r != st.ok ? [Self.ack(cmd, r, t + 1, id)] : []
        case C.browse:
            let t = at(6) == 127 ? s.selected : at(6), id = u21(7)
            if t > 5 { return [Self.ack(cmd, st.badTrack)] }
            let all = s.mineList + s.factory
            guard let k = all.firstIndex(where: { $0.id == id }) else { return [Self.ack(cmd, st.notFound)] }
            s.browse = (t, id, Date().addingTimeInterval(DemoBlob.browseSeconds))
            s.selected = t
            return [Self.ack(cmd, st.ok, t + 1, k + 1)]
        default:
            return nil
        }
    }

    func store(_ b: [UInt8], _ s: DemoState) -> [UInt8] {
        let st = FizzSysEx.ST.self, F = FizzSysEx.FLAGS.self
        guard let d = Self.asDump(b) else { return Self.stored(st.badLength) }
        let flags = Int(b[6]), slot = (Int(b[7]) << 7) | Int(b[8]), t = Int(b[22])
        let load = flags & F.load != 0, pool = flags & F.pool != 0
        if load && t > 5 { return Self.stored(st.badTrack) }
        if load && flags & F.audition != 0 {                 // (the track only: nothing stored)
            var tr = DemoTrack(DemoPreset(slot: 0, id: 0, name: d.name, tags: d.tags, algorithm: 1))
            tr.blob = d.blob.count == FizzSysEx.blob6Len ? d.blob : DemoBlob.six(d.blob)
            tr.drive = d.drive; tr.pan = d.pan; tr.chorus = d.chorus; tr.delay = d.delay; tr.reverb = d.reverb
            tr.id = d.extras?.id ?? 0; tr.sound = d.extras?.sound ?? 0; tr.edited = d.extras?.edited ?? true
            s.tracks[t] = tr
            return Self.stored(st.ok)
        }
        if slot > 256 { return Self.stored(st.badSlot) }
        var place = 0
        if pool {
            guard let k = (1...128).first(where: { s.pool[$0] == nil }) else { return Self.stored(st.full, pool: true) }
            place = k
        }
        let at = slot == 0 || pool ? (1...256).first { s.mine[$0] == nil } : slot
        guard let at else { return Self.stored(st.full, pool: pool) }
        if s.mine[at] != nil && s.playing { return Self.stored(st.busy) }       // (over a used place: a job)
        let id = s.mine[at]?.id ?? s.nextId
        if s.mine[at] == nil { s.nextId += 1 }
        var p = DemoPreset(slot: at, id: id, name: d.name, tags: d.tags, algorithm: d.blob.count > 2 ? Int(d.blob[2]) + 1 : 1)
        p.blob = d.blob.count == FizzSysEx.blob6Len ? d.blob : DemoBlob.six(d.blob)
        p.drive = d.drive; p.pan = d.pan; p.chorus = d.chorus; p.delay = d.delay; p.reverb = d.reverb
        p.imported = flags & F.own == 0
        s.mine[at] = p
        if pool { s.pool[place] = id }
        if load { s.tracks[t] = DemoTrack(p) }
        s.bump()
        return Self.stored(st.ok, pool: pool, slot: pool ? place : at)
    }

    func delete(src: Int, slot: Int, confirm: Bool, _ s: DemoState) -> [UInt8] {
        let cmd = FizzSysEx.CMD.delete, st = FizzSysEx.ST.self
        guard src == 0 || src == 2 else { return Self.ack(cmd, st.badSlot) }
        if s.playing { return Self.ack(cmd, st.busy) }
        if src == 0 {
            guard let p = s.mine[slot] else { return Self.ack(cmd, st.notFound) }
            let u = s.users(ofPreset: p.id)
            if !confirm && (!u.projects.isEmpty || u.tracks != 0) {
                return Self.ack(cmd, st.inUse, u.projects.count, u.projects.reduce(0) { $0 + $1.1 })
            }
            s.mine[slot] = nil
            s.pool = s.pool.filter { $0.value != p.id }
            s.pools = s.pools.mapValues { $0.filter { $0.value != p.id } }
            for t in s.tracks.indices where s.tracks[t].id == p.id { s.tracks[t].id = 0 }   // (their own copy)
        } else {
            guard s.pool[slot] != nil else { return Self.ack(cmd, st.notFound) }
            let u = s.users(ofPlace: slot)
            if !confirm && (!u.projects.isEmpty || u.tracks != 0) {
                return Self.ack(cmd, st.inUse, u.projects.count, u.projects.reduce(0) { $0 + $1.1 })
            }
            s.pool[slot] = nil
        }
        s.bump()
        return Self.ack(cmd, st.ok, slot)
    }

    func swap(src: Int, a: Int, b: Int, _ s: DemoState) -> [UInt8] {
        let cmd = FizzSysEx.CMD.swap, st = FizzSysEx.ST.self
        if src == 0 {
            guard (1...256).contains(a), (1...256).contains(b) else { return Self.ack(cmd, st.badSlot) }
            guard s.mine[a] != nil || s.mine[b] != nil else { return Self.ack(cmd, st.notFound) }
            let pa = s.mine[a], pb = s.mine[b]
            s.mine[a] = pb.map { var q = $0; q.slot = a; return q }
            s.mine[b] = pa.map { var q = $0; q.slot = b; return q }
        } else if src == 2 {
            if s.playing { return Self.ack(cmd, st.busy) }
            guard (1...128).contains(a), (1...128).contains(b) else { return Self.ack(cmd, st.badSlot) }
            guard s.pool[a] != nil || s.pool[b] != nil else { return Self.ack(cmd, st.notFound) }
            (s.pool[a], s.pool[b]) = (s.pool[b], s.pool[a])
        } else { return Self.ack(cmd, st.badSlot) }
        s.bump()
        return Self.ack(cmd, st.ok, a, b)
    }

    func poolRef(slot: Int, id: Int, _ s: DemoState) -> [UInt8] {
        let cmd = FizzSysEx.CMD.poolRef, st = FizzSysEx.ST.self
        guard slot <= 128 else { return Self.ack(cmd, st.badSlot) }
        if id == 0 {                                          // (the place cleared: DELETE's job, CONFIRM)
            guard slot > 0 else { return Self.ack(cmd, st.badSlot) }
            if s.playing { return Self.ack(cmd, st.busy) }
            guard s.pool[slot] != nil else { return Self.ack(cmd, st.notFound) }
            s.pool[slot] = nil
            s.bump()
            return Self.ack(cmd, st.ok, slot, 0)
        }
        guard s.preset(id: id) != nil else { return Self.ack(cmd, st.notFound) }
        let k = slot > 0 ? slot : (1...128).first { s.pool[$0] == nil }
        guard let k else { return Self.ack(cmd, st.full) }
        s.pool[k] = id
        s.bump()
        return Self.ack(cmd, st.ok, k, id)
    }

    func project(op: Int, p: Int, q: Int, _ s: DemoState) -> [UInt8] {
        let cmd = FizzSysEx.CMD.project, st = FizzSysEx.ST.self
        if s.playing { return Self.ack(cmd, st.busy) }
        guard (1...8).contains(p) else { return Self.ack(cmd, st.badSlot) }
        switch op {
        case FizzProjectOp.load.rawValue:
            guard s.projects[p - 1] != nil else { return Self.ack(cmd, st.notFound) }
            if p != s.current {
                s.pools[s.current] = s.pool
                s.pool = s.pools.removeValue(forKey: p) ?? [:]
                s.current = p
            }
        case FizzProjectOp.new.rawValue:
            guard s.projects[p - 1] == nil else { return Self.ack(cmd, st.badSlot) }
            s.projects[p - 1] = DemoProject(name: "PROJECT \(p)", pages: 0, demo: false)
        case FizzProjectOp.copy.rawValue:
            guard (1...8).contains(q), q != p else { return Self.ack(cmd, st.badSlot) }
            guard let src = s.projects[p - 1] else { return Self.ack(cmd, st.notFound) }
            s.projects[q - 1] = DemoProject(name: src.name, pages: src.pages, demo: src.demo)
            let pl = p == s.current ? s.pool : s.pools[p] ?? [:]
            if q == s.current { s.pool = pl } else { s.pools[q] = pl }
        case FizzProjectOp.delete.rawValue:
            guard s.projects[p - 1] != nil else { return Self.ack(cmd, st.notFound) }
            s.projects[p - 1] = nil
            s.pools[p] = nil
            if p == s.current {
                s.pool = [:]
                if let k = s.projects.firstIndex(where: { $0 != nil }) {
                    s.current = k + 1
                    s.pool = s.pools.removeValue(forKey: k + 1) ?? [:]
                } else {
                    s.projects[p - 1] = DemoProject(name: "PROJECT \(p)", pages: 0, demo: false)
                }
            }
        default:
            return Self.ack(cmd, st.badCommand)
        }
        s.bump()
        return Self.ack(cmd, st.ok, p, q)
    }

    /// A track's DUMP (slot 0: the selected one), with its extras (id, pool sound, flags, channel, the selected one).
    func trackDump(_ slot: Int, _ s: DemoState) -> [UInt8] {
        let t = slot == 0 ? s.selected : slot - 1
        guard (0...5).contains(t) else { return Self.err(FizzSysEx.CMD.dump, FizzSysEx.ST.badSlot) }
        let tr = s.tracks[t]
        let xf = (tr.edited ? 1 : 0) | (t == s.selected ? 2 : 0) | (s.playing ? 8 : 0) | (s.browse?.track == t ? 16 : 0)
        let body = [FizzSysEx.FLAGS.dump | FizzSysEx.FLAGS.load, 0, 0] + Self.name12(tr.name) +
            [tr.drive & 0x7f, t, max(0, min(127, tr.pan + 64)), tr.chorus & 0x7f, tr.delay & 0x7f, tr.reverb & 0x7f, 0x0f] +
            FizzSysEx.pack7(tr.blob).map(Int.init) + [tr.tags & 0x7f] + Self.r21(tr.id) + Self.r14(tr.sound) +
            [xf, t, s.selected]
        return Self.frame(FizzSysEx.CMD.store6, body)
    }
}

// MARK: - The restore

extension DemoFizz {
    /// FZSR len crc: refused as FIZZ does (Bluetooth 15, playing 10, another size 11), else the pulls.
    func restore(_ words: [UInt32]) {
        guard words.count >= 2 else { return }
        let s = unit.state
        let no: UInt32? = FizzLink.isBlePort(name) ? 15 : s.lock.withLock({ s.playing }) ? 10 : words[0] != unit.storeSize ? 11 : nil
        if let no {
            let f = FM1Bulk.updateFrame(cmd: FM1Bulk.bulkCmd, payload: Array("FZER".utf8) + FM1Bulk.le32(no))
            enqueue(replyMs) { [f] }
            return
        }
        guard beginRestore() else { return }
        let len = Int(words[0]), crc = words[1]
        Task { await self.pullAll(len: len, crc: crc) }
    }

    private func pullAll(len: Int, crc: UInt32) async {
        var got: [[UInt8]] = [[], []]
        var code: UInt32 = 1
        for pass in 0..<2 {
            var a = 0
            while a < len {
                let n = min(512, len - a)
                guard let d = await pull(a, n) else { code = pass == 0 ? 12 : 13; break }
                got[pass] += d
                a += n
            }
            if code != 1 { break }
            if pass == 0, FM1Bulk.crc32(got[0]) != crc { code = 12; break }   // (nothing erased)
        }
        if code == 1 {
            let sum = try? await FizzDataShared.engine().readRegion(got[1])
            let s = unit.state
            s.lock.withLock { s.restored(got[1], sum) }
        }
        endRestore()
        yield(FM1Bulk.updateFrame(cmd: FM1Bulk.bulkCmd, payload: Array((code == 1 ? "FZOK" : "FZER").utf8) + FM1Bulk.le32(code)))
    }

    /// One pull: asked, and again every second (10 times), as fz_pull.
    private func pull(_ a: Int, _ n: Int) async -> [UInt8]? {
        for _ in 0..<10 {
            yield(FM1Bulk.readRequest(addr: UInt32(a), len: n))
            let end = Date().addingTimeInterval(1)
            while Date() < end {
                if let d = answer(for: UInt32(a)) { return d.count == n ? d : nil }
                if gone { return nil }
                try? await Task.sleep(for: .microseconds(300))
            }
        }
        return nil
    }
}

// MARK: - A track's blob changed by a PARAM

enum DemoBlob {
    /// LOAD ON FM-1: the demo's browser closes by itself after this long, loading the preset
    static let browseSeconds = 2.5
    /// Each value's minimum (site/manager/fizzvalues.js VALUES, ids 0..114).
    static let mins: [Int] = [
        1, 0, 0, 0, -2600, 0, 0, -64, -100, -100, -100, -100, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, -6400, 0, 0, 0, 0, 0, 0, 0, 0, 0, -6400, 0, -64, 0, 0, 0, 0, -6400, -6400, 0, -64, 0, 0, 0, 0, -6400,
        -6400, 0, -64, 0, 0, 0, 0, -6400, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -64, 0, 0, 0, 0, 0, 0, -127, 0, 0, -127, 0, 0,
        -127, 0, 0, -127, 0, 0, -127, 0, 0, -127, 0, 0, -127, 0, 0, -127, 0, 1, 0, 0, 0,
    ]
    /// The ones kept in two bytes (dn2pst.js quadPack)
    static let hires: Set<Int> = [4, 5, 34, 35, 37, 40, 47, 54, 55, 62, 63, 70]

    /// The blob's byte of value `id` (0..72), from byte 2 on, the hi-res ones two.
    static func offset(_ id: Int) -> Int {
        (0..<id).reduce(2) { $0 + (hires.contains($1) ? 2 : 1) }
    }

    /// Value `id` set in a version-6 blob as FM TONE packs it (dn2pst.js quadPack6).
    static func set(_ blob: inout [UInt8], id: Int, value v: Int) {
        guard blob.count == FizzSysEx.blob6Len, mins.indices.contains(id) else { return }
        let u = v - mins[id]
        switch id {
        case 0...72:
            let o = offset(id)
            blob[o] = UInt8(truncatingIfNeeded: u)
            if hires.contains(id) { blob[o + 1] = UInt8(truncatingIfNeeded: u >> 8) }
        case 73: blob[87] = UInt8(max(0, min(127, v)))
        case 74...109: blob[88 + id - 74] = UInt8(truncatingIfNeeded: u)
        case 110: blob[124] = (blob[124] & ~1) | UInt8(v & 1)
        case 111: blob[125] = UInt8(truncatingIfNeeded: v - 1)
        case 112: blob[126] = UInt8(truncatingIfNeeded: v - 20)
        case 113: blob[127] = UInt8(truncatingIfNeeded: v - 64)
        default: blob[124] = (blob[124] & 1) | UInt8(((v - 1) & 3) << 1)
        }
    }

    /// A version-5 blob as a version-6 one (zeros, the VOICE page's defaults).
    static func six(_ b: [UInt8]) -> [UInt8] {
        var x = [UInt8](repeating: 0, count: FizzSysEx.blob6Len)
        for (i, v) in b.prefix(88).enumerated() { x[i] = v }
        x[1] = 6
        x[126] = 20; x[127] = 64
        return x
    }
}
