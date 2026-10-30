// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// The link's transport: complete SysEx messages out and in. CoreMIDI's (MIDI 1.0 protocol, UMP event lists) and the
// port scanner that finds the FM-1's port pairs. Bluetooth LE MIDI ports are ordinary endpoints once connected.

import Foundation

/// One MIDI in/out pair carrying complete messages.
public protocol MIDITransport: AnyObject, Sendable {
    var name: String { get }
    /// Complete SysEx messages received, F0 .. F7. Finishes when the port is lost or closed.
    var messages: AsyncStream<[UInt8]> { get }
    /// The port went away (unplugged, or a send failed).
    var isLost: Bool { get }
    /// One complete message (a SysEx F0 .. F7, or a short channel message).
    func send(_ bytes: [UInt8]) throws
    func close()
    /// No FM-1 behind it (the emulated FIZZ in this app): nothing paced for the unit's one-frame inbox, its flash or
    /// its USB (FizzLink's frame gap, the restore's first wait). Every real port: false.
    var isEmulated: Bool { get }
}

extension MIDITransport {
    public var isEmulated: Bool { false }
}

public enum MIDITransportError: Error, Equatable {
    case lost
    case closed
    case coreMIDI(Int32)
}

/// An endpoint as the scanner sees it.
public struct MIDIEndpointInfo: Sendable, Hashable {
    public let ref: UInt32
    public let name: String
    public let entity: UInt32       // 0: none (a virtual endpoint)
    public let device: UInt32       // 0: none
    public let offline: Bool

    public init(ref: UInt32, name: String, entity: UInt32 = 0, device: UInt32 = 0, offline: Bool = false) {
        self.ref = ref; self.name = name; self.entity = entity; self.device = device; self.offline = offline
    }
}

/// A source paired with a destination: a port pair worth probing.
public struct MIDIPortCandidate: Sendable, Hashable {
    public let source: MIDIEndpointInfo
    public let destination: MIDIEndpointInfo
    public var name: String { source.name }
    /// The owning device (0: unknown): one unit can show two port pairs.
    public var device: UInt32 { source.device }

    public init(source: MIDIEndpointInfo, destination: MIDIEndpointInfo) {
        self.source = source; self.destination = destination
    }
}

/// Lists port pairs and opens transports on them (CoreMIDI's, or a test's).
public protocol MIDIPortScanning: Sendable {
    func candidates() -> [MIDIPortCandidate]
    func open(_ candidate: MIDIPortCandidate) throws -> any MIDITransport
}

public enum MIDIPorts {
    /// Names worth probing (never other gear); hosts can show stale names ("FIZZ FM-1", "ChoralRoot FM-1").
    public static let namePattern = "fm-?1|felucca|ota|composite|sinco|usb-midi|fizz|choralroot"

    public static func isCandidateName(_ name: String) -> Bool {
        name.range(of: namePattern, options: [.regularExpression, .caseInsensitive]) != nil
    }

    /// Ports never probed even as a last resort: the host's own virtual buses.
    public static let neverPattern = "network|session|iac|bus \\d|ableton|logic|garageband"

    /// What to probe: the FM-1-like names; when there are none (a host may name a USB device's port generically, e.g.
    /// "Port 1" on iOS), every other port pair, apart from the host's own buses.
    public static func candidates(sources: [MIDIEndpointInfo], destinations: [MIDIEndpointInfo]) -> [MIDIPortCandidate] {
        let named = pair(sources: sources, destinations: destinations)
        if !named.isEmpty { return named }
        return pair(sources: sources, destinations: destinations, anyName: true)
            .filter { $0.name.range(of: neverPattern, options: [.regularExpression, .caseInsensitive]) == nil }
    }

    /// Each candidate source with its destination: the same entity first, then the same name, then the only one.
    public static func pair(sources: [MIDIEndpointInfo], destinations: [MIDIEndpointInfo],
                            anyName: Bool = false) -> [MIDIPortCandidate] {
        var out: [MIDIPortCandidate] = []
        for s in sources where !s.offline && (anyName || isCandidateName(s.name)) {
            let live = destinations.filter { !$0.offline }
            let d = (s.entity != 0 ? live.first { $0.entity == s.entity } : nil)
                ?? live.first { $0.name == s.name }
                ?? (destinations.count == 1 ? live.first : nil)
            if let d { out.append(MIDIPortCandidate(source: s, destination: d)) }
        }
        return out
    }

    /// A short channel message as a MIDI 1.0 channel voice UMP (type 2); nil for anything else.
    public static func shortMessageWord(_ b: [UInt8], group: UInt8 = 0) -> UInt32? {
        guard let st = b.first, st >= 0x80, st < 0xf0 else { return nil }
        let d1 = b.count > 1 ? UInt32(b[1] & 0x7f) : 0, d2 = b.count > 2 ? UInt32(b[2] & 0x7f) : 0
        return (UInt32(0x2) << 28) | (UInt32(group & 0xf) << 24) | (UInt32(st) << 16) | (d1 << 8) | d2
    }
}

#if canImport(CoreMIDI)
import CoreMIDI

/// CoreMIDI's endpoints: candidates, transports, and setup changes. Create it on the main thread: CoreMIDI sends its
/// notifications on the run loop of the thread that made the client.
public final class MIDIPortScanner: MIDIPortScanning, @unchecked Sendable {
    let client: MIDIClientRef
    private let lock = NSLock()
    private var open: [Weak] = []
    private let changed: AsyncStream<Void>.Continuation
    /// A value whenever endpoints come or go.
    public let portsChanged: AsyncStream<Void>

    private struct Weak { weak var t: CoreMIDITransport? }

    public init(clientName: String = "FizzKit") throws {
        (portsChanged, changed) = AsyncStream.makeStream(of: Void.self, bufferingPolicy: .bufferingNewest(1))
        var c = MIDIClientRef()
        let box = NotifyBox()
        let st = MIDIClientCreateWithBlock(clientName as CFString, &c) { msg in box.handle(msg) }
        guard st == noErr else { throw MIDITransportError.coreMIDI(st) }
        client = c
        box.scanner = self
    }

    deinit {
        changed.finish()
        MIDIClientDispose(client)
    }

    private final class NotifyBox: @unchecked Sendable {
        weak var scanner: MIDIPortScanner?
        func handle(_ msg: UnsafePointer<MIDINotification>) { scanner?.notified(msg) }
    }

    private func notified(_ msg: UnsafePointer<MIDINotification>) {
        switch msg.pointee.messageID {
        case .msgObjectRemoved:
            let gone = msg.withMemoryRebound(to: MIDIObjectAddRemoveNotification.self, capacity: 1) { $0.pointee.child }
            lock.lock()
            let ts = open.compactMap(\.t)
            open = open.filter { $0.t != nil }
            lock.unlock()
            for t in ts where t.uses(gone) { t.markLost() }
            changed.yield()
        case .msgObjectAdded, .msgSetupChanged:
            changed.yield()
        default:
            break
        }
    }

    public func sources() -> [MIDIEndpointInfo] {
        (0..<MIDIGetNumberOfSources()).map { info(MIDIGetSource($0)) }
    }

    public func destinations() -> [MIDIEndpointInfo] {
        (0..<MIDIGetNumberOfDestinations()).map { info(MIDIGetDestination($0)) }
    }

    public func candidates() -> [MIDIPortCandidate] {
        MIDIPorts.candidates(sources: sources(), destinations: destinations())
    }

    public func open(_ candidate: MIDIPortCandidate) throws -> any MIDITransport {
        let t = try CoreMIDITransport(client: client, candidate: candidate)
        lock.lock()
        open.append(Weak(t: t))
        lock.unlock()
        return t
    }

    private func info(_ ep: MIDIEndpointRef) -> MIDIEndpointInfo {
        var entity = MIDIEntityRef(), device = MIDIDeviceRef()
        if MIDIEndpointGetEntity(ep, &entity) != noErr { entity = 0 }
        if entity != 0, MIDIEntityGetDevice(entity, &device) != noErr { device = 0 }
        var off: Int32 = 0
        _ = MIDIObjectGetIntegerProperty(ep, kMIDIPropertyOffline, &off)
        return MIDIEndpointInfo(ref: ep, name: Self.name(ep), entity: entity, device: device, offline: off != 0)
    }

    static func name(_ obj: MIDIObjectRef) -> String {
        for key in [kMIDIPropertyDisplayName, kMIDIPropertyName] {
            var s: Unmanaged<CFString>?
            if MIDIObjectGetStringProperty(obj, key, &s) == noErr, let v = s?.takeRetainedValue() { return v as String }
        }
        return ""
    }
}

/// One port pair through CoreMIDI: SysEx out as UMP SysEx7 event lists, in reassembled from them.
public final class CoreMIDITransport: MIDITransport, @unchecked Sendable {
    public let name: String
    public let messages: AsyncStream<[UInt8]>
    private let cont: AsyncStream<[UInt8]>.Continuation
    private let source: MIDIEndpointRef, destination: MIDIEndpointRef
    private var inPort = MIDIPortRef(), outPort = MIDIPortRef()
    private let lock = NSLock()
    private var reassembler = SysEx7Reassembler()
    private var lost = false, closed = false

    public var isLost: Bool { lock.lock(); defer { lock.unlock() }; return lost }

    init(client: MIDIClientRef, candidate: MIDIPortCandidate) throws {
        name = candidate.name
        source = candidate.source.ref
        destination = candidate.destination.ref
        (messages, cont) = AsyncStream.makeStream(of: [UInt8].self)
        let box = ReceiveBox()
        var st = MIDIInputPortCreateWithProtocol(client, "FizzKit in" as CFString, ._1_0, &inPort) { list, _ in
            box.transport?.receive(list)
        }
        guard st == noErr else { throw MIDITransportError.coreMIDI(st) }
        st = MIDIOutputPortCreate(client, "FizzKit out" as CFString, &outPort)
        guard st == noErr else { MIDIPortDispose(inPort); throw MIDITransportError.coreMIDI(st) }
        box.transport = self
        st = MIDIPortConnectSource(inPort, source, nil)
        guard st == noErr else { dispose(); throw MIDITransportError.coreMIDI(st) }
    }

    deinit { dispose() }

    private final class ReceiveBox: @unchecked Sendable {
        weak var transport: CoreMIDITransport?
    }

    func uses(_ obj: MIDIObjectRef) -> Bool { obj == source || obj == destination }

    private func receive(_ list: UnsafePointer<MIDIEventList>) {
        var got: [[UInt8]] = []
        lock.lock()
        for p in list.unsafeSequence() { got += reassembler.feed(p.words()) }
        let done = closed || lost
        lock.unlock()
        guard !done else { return }
        for m in got { cont.yield(m) }
    }

    public func send(_ bytes: [UInt8]) throws {
        lock.lock()
        let (isClosed, isLost) = (closed, lost)
        lock.unlock()
        if isClosed { throw MIDITransportError.closed }
        if isLost { throw MIDITransportError.lost }
        let umps: [[UInt32]]
        if bytes.first == 0xf0 {
            let w = UMP.sysEx7Words(bytes)
            umps = stride(from: 0, to: w.count, by: 2).map { [w[$0], w[$0 + 1]] }
        } else if let w = MIDIPorts.shortMessageWord(bytes) {
            umps = [[w]]
        } else {
            return
        }
        let size = MemoryLayout<MIDIEventList>.size + umps.count * 32 + 64
        let raw = UnsafeMutableRawPointer.allocate(byteCount: size, alignment: MemoryLayout<MIDIEventList>.alignment)
        defer { raw.deallocate() }
        let list = raw.bindMemory(to: MIDIEventList.self, capacity: 1)
        var pkt: UnsafeMutablePointer<MIDIEventPacket>? = MIDIEventListInit(list, ._1_0)
        for u in umps {
            pkt = u.withUnsafeBufferPointer { MIDIEventListAdd(list, size, pkt!, 0, u.count, $0.baseAddress!) }
            guard pkt != nil else { throw MIDITransportError.coreMIDI(-1) }
        }
        let st = MIDISendEventList(outPort, destination, list)
        if st != noErr {
            markLost()
            throw MIDITransportError.lost
        }
    }

    func markLost() {
        lock.lock()
        let was = lost || closed
        lost = true
        lock.unlock()
        if !was { cont.finish() }
    }

    public func close() {
        lock.lock()
        let was = closed
        closed = true
        lock.unlock()
        guard !was else { return }
        dispose()
        cont.finish()
    }

    private func dispose() {
        if inPort != 0 { MIDIPortDisconnectSource(inPort, source); MIDIPortDispose(inPort); inPort = 0 }
        if outPort != 0 { MIDIPortDispose(outPort); outPort = 0 }
    }
}
#endif
