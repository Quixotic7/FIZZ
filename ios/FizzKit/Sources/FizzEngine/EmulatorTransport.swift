// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// A MIDI port onto the emulated FIZZ, so FizzLink talks to it exactly as to an FM-1. FIZZ's own SysEx (7D 46 5A:
// IDENTITY, LIST, DUMP, STATUS, STORE, PARAM ..) and notes go into the core (midi_in) and its replies come back from
// midi_out_take. What the FM-1's base firmware answers and the core does not, this file answers, as the web Manager's
// browser unit does (site/manager/simfizz.js, a model of firmware/app/app.c app_usb_cmd):
//   - the update handshake (cmd 0x11): the identity "FM-1_46..";
//   - the bulk channel in the update's framing: FZBI (offset, size, CRC-32), FZBR off n (n <= 256; FZER 11 outside),
//     FZSR len crc (FZER 10 while playing, 11 another size; the image pulled twice with 0x30 read requests, 512 bytes
//     each, 3 tries of 2 s; 12 not whole, nothing changed; 13 cut off while writing: a cleared store; 14 not read back
//     as sent; FZOK: FIZZ started again on it).
// The store the bulk channel shows is the USB package's region (73 sectors from 0x97000), made from the core's 1 MiB
// flash record by record (StoreImage), so a backup of it restores onto an FM-1 and an FM-1's onto it.

import FizzKit
import Foundation

public final class EmulatorTransport: MIDITransport, @unchecked Sendable {
    public let name: String
    public let messages: AsyncStream<[UInt8]>
    private let cont: AsyncStream<[UInt8]>.Continuation
    let emulator: FizzEmulator
    private let lock = NSLock()
    private var closed = false, lostFlag = false
    private var chain: Task<Void, Never>?
    private var region: [UInt8]?              // the store as the last FZBI saw it (device layout)
    private var inbox: RestoreInbox?          // a restore pulling its image
    /// a restore's read: tries and the wait for each (ms)
    public var pullTries = 3, pullMs = 2000.0

    init(emulator: FizzEmulator, name: String) {
        self.emulator = emulator
        self.name = name
        (messages, cont) = AsyncStream.makeStream(of: [UInt8].self)
    }

    public var isLost: Bool { lock.withLock { lostFlag } }
    /// No FM-1 behind it: FizzLink paces nothing (every frame is taken at once).
    public var isEmulated: Bool { true }

    public func send(_ bytes: [UInt8]) throws {
        let (c, l, box) = lock.withLock { (closed, lostFlag, inbox) }
        if l { throw MIDITransportError.lost }
        if c { throw MIDITransportError.closed }
        if bytes == FM1Bulk.handshakeQuery { deliver(identityFrame); return }
        if let box {                                    // (pulling an image: only its answers are heard)
            if let r = FM1Bulk.parseResponse(bytes) { Task { await box.took(r.addr, r.data) } }
            return
        }
        if bytes.count > 2, bytes[0] == 0xF0, bytes[1] != 0x7D {
            if let u = Self.updateBody(bytes), u[2] == FM1Bulk.bulkCmd, emulator.isBooted { bulk(u) }
            return
        }
        guard emulator.isBooted else { return }
        emulator.midiIn(bytes)
    }

    public func close() {
        let box: RestoreInbox? = lock.withLock {
            guard !closed else { return nil }
            closed = true
            cont.finish()
            return inbox
        }
        if let box { Task { await box.cut() } }
    }

    /// The emulated unit went away (another emulator took the core).
    func lostByEmulator() {
        lock.withLock {
            guard !lostFlag && !closed else { return }
            lostFlag = true
            cont.finish()
        }
    }

    /// One message from the unit.
    func deliver(_ m: [UInt8]) {
        lock.withLock {
            guard !closed && !lostFlag else { return }
            cont.yield(m)
        }
    }

    var identityFrame: [UInt8] {
        let id = Array(emulator.identity.utf8.prefix(27))
        return FM1Bulk.updateFrame(cmd: 0x11, payload: id + [UInt8](repeating: 0, count: 27 - id.count))
    }

    /// fm1-base's framing (00 59 cmd len16 00 body ~sum) unpacked, nil when it is not one (or its sum is wrong).
    static func updateBody(_ d: [UInt8]) -> [UInt8]? {
        guard d.count >= 3, d[d.count - 1] == 0xF7 else { return nil }
        let u = FM1Bulk.unpack7(d[1..<(d.count - 1)])
        guard u.count >= 7, u[0] == 0, u[1] == 0x59, (Int(u[3]) | Int(u[4]) << 8) + 7 == u.count else { return nil }
        let s = u[6..<(u.count - 1)].reduce(0) { $0 + Int($1) }
        return UInt8(~s & 0xFF) == u[u.count - 1] ? u : nil
    }

    private func reply(_ tag: String, _ data: [UInt8]) {
        deliver(FM1Bulk.updateFrame(cmd: FM1Bulk.bulkCmd, payload: Array(tag.utf8) + data))
    }

    /// The bulk side's work, one request after another.
    private func enqueue(_ work: @escaping @Sendable () async -> Void) {
        lock.withLock {
            let prev = chain
            chain = Task { await prev?.value; await work() }
        }
    }

    // MARK: - the bulk channel (app.c app_usb_cmd)

    private func bulk(_ u: [UInt8]) {
        guard u.count >= 10 else { return }
        let tag = String(decoding: u[6..<10], as: UTF8.self), n = u.count
        switch tag {
        case "FZBI":
            enqueue { [self] in
                guard let r = readRegion() else { reply("FZER", FM1Bulk.le32(1)); return }
                reply("FZBI", FM1Bulk.le32(StoreImage.deviceRegion) + FM1Bulk.le32(UInt32(r.count)) +
                    FM1Bulk.le32(FM1Bulk.crc32(r)))
            }
        case "FZBR":
            guard n >= 7 + 12 else { return }
            let off = Int(FM1Bulk.rd32(u, 10)), len = Int(FM1Bulk.rd32(u, 14))
            enqueue { [self] in
                guard let r = lock.withLock({ region }) ?? readRegion() else { reply("FZER", FM1Bulk.le32(1)); return }
                if len > FM1Bulk.chunk || off > r.count || len > r.count - off { reply("FZER", FM1Bulk.le32(11)); return }
                reply("FZBD", FM1Bulk.le32(UInt32(off)) + r[off..<(off + len)])
            }
        case "FZSR":
            guard n >= 7 + 12 else { return }
            let len = Int(FM1Bulk.rd32(u, 10)), crc = FM1Bulk.rd32(u, 14)
            enqueue { [self] in
                if emulator.isPlaying { reply("FZER", FM1Bulk.le32(10)); return }
                if len != StoreImage.device.size { reply("FZER", FM1Bulk.le32(11)); return }
                await restore(len: len, crc: crc)
            }
        default:
            break
        }
    }

    /// The store now, in the FM-1's layout (FIZZ's lazy writes made first).
    private func readRegion() -> [UInt8]? {
        let r = try? StoreImage.convert(emulator.flashImage(), from: StoreImage.host, to: StoreImage.device)
        lock.withLock { region = r }
        return r
    }

    // fz_restore: the image pulled once (its CRC checked), then again as the sectors are written; FIZZ started again
    private func restore(len: Int, crc: UInt32) async {
        let box = RestoreInbox()
        lock.withLock { inbox = box }
        let k = 512
        func pull(_ addr: Int) async -> [UInt8]? {
            let n = min(k, len - addr)
            for _ in 0..<pullTries {
                if lock.withLock({ closed || lostFlag }) { return nil }
                deliver(FM1Bulk.readRequest(addr: UInt32(addr), len: n))
                if let d = await box.wait(UInt32(addr), n, ms: pullMs) { return d }
            }
            return nil
        }
        var code: UInt32 = 0
        var c: UInt32 = 0
        var img = [UInt8](repeating: 0xFF, count: len)
        var written = false
        pass1: do {
            for a in stride(from: 0, to: len, by: k) {
                guard let d = await pull(a) else { code = 12; break pass1 }
                c = FM1Bulk.crc32(d, c)
            }
            if c != crc { code = 12; break pass1 }
            written = true                                  // (app_panic: "RESTORING ITS STORE")
            for a in stride(from: 0, to: len, by: k) {
                guard let d = await pull(a) else { code = 13; break pass1 }
                img.replaceSubrange(a..<(a + d.count), with: d)
            }
            code = FM1Bulk.crc32(img) == crc ? 0 : 14
        }
        lock.withLock { inbox = nil; region = nil }
        if written {
            // the region written: FIZZ again on it (cut short or not as sent: a cleared store, as the unit's)
            let host = code == 0 ? try? StoreImage.convert(img, from: StoreImage.device, to: StoreImage.host) : nil
            emulator.boot(flash: host)
            emulator.save(force: true)
        }
        reply(code == 0 ? "FZOK" : "FZER", FM1Bulk.le32(code == 0 ? 1 : code))
    }
}

/// A restore's read answers: the one waited for, or nil after its time.
actor RestoreInbox {
    private var waiting: (addr: UInt32, len: Int, gen: Int, k: CheckedContinuation<[UInt8]?, Never>)?
    private var early: (UInt32, [UInt8])?
    private var gen = 0
    private var cutOff = false

    func wait(_ addr: UInt32, _ len: Int, ms: Double) async -> [UInt8]? {
        if cutOff { return nil }
        if let e = early, e.0 == addr, e.1.count == len { early = nil; return e.1 }
        gen += 1
        let g = gen
        return await withCheckedContinuation { k in
            waiting = (addr, len, g, k)
            Task {
                try? await Task.sleep(for: .microseconds(Int(ms * 1000)))
                self.expire(g)
            }
        }
    }

    func took(_ addr: UInt32, _ data: [UInt8]) {
        if let w = waiting, w.addr == addr, w.len == data.count {
            waiting = nil
            w.k.resume(returning: data)
        } else {
            early = (addr, data)
        }
    }

    private func expire(_ g: Int) {
        guard let w = waiting, w.gen == g else { return }
        waiting = nil
        w.k.resume(returning: nil)
    }

    func cut() {
        cutOff = true
        if let w = waiting { waiting = nil; w.k.resume(returning: nil) }
    }
}
