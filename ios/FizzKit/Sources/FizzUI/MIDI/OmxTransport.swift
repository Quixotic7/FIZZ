// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2026 RemoteFM1 contributors
// Derived from RemoteFM1IOS af91bc5 RFMKit/Sources/RFMKit/OMX27/OmxTransport.swift, adapted: the CoreMIDI client and ports named for FIZZ.
// What OmxLink talks through, behind protocols so the Mac tests drive it with a fake OMX-27 and a fake clock:
//   OmxTransport   the MIDI ports: the endpoints now, one source listened to and one destination sent to, setup
//                  changes. OmxCoreMIDI is CoreMIDI's: UMP (MIDI 1.0 protocol) both ways, SysEx7 packets out with
//                  host-time timestamps (the screen's chunks are paced), SysEx7 packets in turned back into bytes and
//                  reassembled F0..F7 on the main thread.
//   OmxClock       milliseconds and timers on the main thread (OmxMainClock: dispatch timers on the main queue)
// omxPair picks the OMX's ports: an output whose name says OMX and the input of the same name (else the first OMX
// input), as the website's omx27.js.

import Foundation
#if canImport(CoreMIDI)
import CoreMIDI
#endif

/// a MIDI endpoint (CoreMIDI: its unique ID, display name and ref)
public struct OmxPort: Hashable, Sendable {
    public var id: Int32
    public var name: String
    public var ref: UInt32
    public init(id: Int32, name: String, ref: UInt32 = 0) { self.id = id; self.name = name; self.ref = ref }
}

public struct OmxPair: Equatable, Sendable {
    public var input: OmxPort, output: OmxPort
    public var name: String { output.name }
}

/// an output whose name says OMX and the input of the same name (else the first OMX input)
public func omxPair(inputs: [OmxPort], outputs: [OmxPort]) -> OmxPair? {
    let outs = outputs.filter { OmxWire.isOmxPortName($0.name) }, ins = inputs.filter { OmxWire.isOmxPortName($0.name) }
    for o in outs {
        if let i = ins.first(where: { $0.name == o.name }) ?? ins.first { return OmxPair(input: i, output: o) }
    }
    return nil
}

@MainActor public protocol OmxTransport: AnyObject {
    /// starts (setup changes come to onSetupChange); nil when it runs, else why not
    func start(onSetupChange: @escaping @MainActor () -> Void) -> String?
    /// no more setup changes, nothing listened to
    func stop()
    /// the endpoints there now (online)
    func ports() -> (inputs: [OmxPort], outputs: [OmxPort])
    /// listens to input (each SysEx whole, F0..F7, on the main thread) and sends to output from now on
    func open(input: OmxPort, output: OmxPort, onMessage: @escaping @MainActor ([UInt8]) -> Void)
    /// stops listening and sending
    func close()
    /// a whole message to the output, delayMs from now
    func send(_ bytes: [UInt8], delayMs: Double)
}

@MainActor public protocol OmxTimer: AnyObject { func cancel() }

@MainActor public protocol OmxClock: AnyObject {
    /// milliseconds, any origin
    var now: Double { get }
    func after(_ ms: Double, _ f: @escaping @MainActor () -> Void) -> any OmxTimer
    func every(_ ms: Double, _ f: @escaping @MainActor () -> Void) -> any OmxTimer
}

/// timers on the main queue
@MainActor public final class OmxMainClock: OmxClock {
    public init() {}
    public var now: Double { ProcessInfo.processInfo.systemUptime * 1000 }

    private final class T: OmxTimer {
        let src: DispatchSourceTimer
        init(_ src: DispatchSourceTimer) { self.src = src }
        func cancel() { src.cancel() }
    }

    private func timer(_ ms: Double, repeating: Bool, _ f: @escaping @MainActor () -> Void) -> any OmxTimer {
        let src = DispatchSource.makeTimerSource(queue: .main)
        let t = T(src)
        let ns = Int(max(0, ms) * 1_000_000)
        src.schedule(deadline: .now() + .nanoseconds(ns), repeating: repeating ? .nanoseconds(ns) : .never,
                     leeway: .milliseconds(1))
        src.setEventHandler { [weak t] in
            MainActor.assumeIsolated {
                if !repeating { t?.cancel() }
                f()
            }
        }
        src.resume()
        return t
    }

    public func after(_ ms: Double, _ f: @escaping @MainActor () -> Void) -> any OmxTimer { timer(ms, repeating: false, f) }
    public func every(_ ms: Double, _ f: @escaping @MainActor () -> Void) -> any OmxTimer { timer(ms, repeating: true, f) }
}

/// SysEx from a byte stream that may split it: each whole message F0..F7 (real-time bytes inside are skipped; any
/// other status byte drops the one being gathered)
public struct OmxSysexAssembler: Sendable {
    private var buf: [UInt8] = []
    private var inside = false
    public static let maxLength = 4096
    public init() {}

    public mutating func feed<C: Sequence>(_ bytes: C) -> [[UInt8]] where C.Element == UInt8 {
        var out: [[UInt8]] = []
        for b in bytes {
            if b == 0xF0 { buf = [b]; inside = true; continue }
            guard inside else { continue }
            if b == 0xF7 {
                buf.append(b)
                out.append(buf)
                buf = []
                inside = false
            } else if b >= 0xF8 {
                continue
            } else if b >= 0x80 {
                buf = []
                inside = false
            } else if buf.count < Self.maxLength {
                buf.append(b)
            } else {
                buf = []
                inside = false
            }
        }
        return out
    }
}

/// UMP (MIDI 1.0 protocol) SysEx7 packets: a whole SysEx (F0..F7) to 64-bit messages, and messages back to bytes
public enum OmxUMP {
    /// F0 data... F7 -> SysEx7 messages (two words each: complete, or start, continue..., end), group 0
    public static func sysex7(_ msg: [UInt8]) -> [UInt32] {
        var data = msg
        if data.first == 0xF0 { data.removeFirst() }
        if data.last == 0xF7 { data.removeLast() }
        var words: [UInt32] = []
        let n = data.count, parts = max(1, (n + 5) / 6)
        for p in 0..<parts {
            let chunk = Array(data[(p * 6)..<min(n, p * 6 + 6)])
            let status: UInt32 = parts == 1 ? 0 : p == 0 ? 1 : p == parts - 1 ? 3 : 2
            var b = [UInt32](repeating: 0, count: 6)
            for (i, x) in chunk.enumerated() { b[i] = UInt32(x & 0x7F) }
            words.append(0x3000_0000 | status << 20 | UInt32(chunk.count) << 16 | b[0] << 8 | b[1])
            words.append(b[2] << 24 | b[3] << 16 | b[4] << 8 | b[5])
        }
        return words
    }

    /// the words a UMP message of this first word takes
    static func size(_ w0: UInt32) -> Int {
        switch w0 >> 28 {
        case 0x0, 0x1, 0x2, 0x6, 0x7: 1
        case 0x3, 0x4, 0x8, 0x9, 0xA: 2
        case 0xB, 0xC: 3
        default: 4
        }
    }

    /// UMP words -> the bytes of their SysEx7 messages (F0 on a start, F7 on an end); anything else is left out
    public static func bytes<C: Collection>(_ words: C) -> [UInt8] where C.Element == UInt32, C.Index == Int {
        var out: [UInt8] = []
        var i = words.startIndex
        while i < words.endIndex {
            let w0 = words[i], n = size(w0)
            if w0 >> 28 == 0x3, i + 1 < words.endIndex {
                let w1 = words[i + 1], status = (w0 >> 20) & 0xF, count = min(6, Int((w0 >> 16) & 0xF))
                let b: [UInt8] = [UInt8((w0 >> 8) & 0x7F), UInt8(w0 & 0x7F), UInt8((w1 >> 24) & 0x7F),
                                  UInt8((w1 >> 16) & 0x7F), UInt8((w1 >> 8) & 0x7F), UInt8(w1 & 0x7F)]
                if status == 0 || status == 1 { out.append(0xF0) }
                if status <= 3 { out += b[0..<count] }
                if status == 0 || status == 3 { out.append(0xF7) }
            }
            i += n
        }
        return out
    }
}

#if canImport(CoreMIDI)
/// CoreMIDI: one client and its two ports, made once and kept (messages already scheduled still go out after close())
@MainActor public final class OmxCoreMIDI: OmxTransport {
    private var client = MIDIClientRef()
    private var inPort = MIDIPortRef()
    private var outPort = MIDIPortRef()
    private var source = MIDIEndpointRef()
    private var dest = MIDIEndpointRef()
    private var onSetup: (@MainActor () -> Void)?
    private var onMessage: (@MainActor ([UInt8]) -> Void)?
    private var assembler = OmxSysexAssembler()
    private var generation = 0
    private let ticksPerMs: Double

    public init() {
        var tb = mach_timebase_info_data_t()
        mach_timebase_info(&tb)
        ticksPerMs = 1_000_000 * Double(tb.denom) / Double(tb.numer)
    }

    public func start(onSetupChange: @escaping @MainActor () -> Void) -> String? {
        onSetup = onSetupChange
        if client != 0 { return nil }
        var st = MIDIClientCreateWithBlock("FIZZ OMX-27" as CFString, &client) { @Sendable [weak self] n in
            guard n.pointee.messageID == .msgSetupChanged else { return }
            DispatchQueue.main.async { MainActor.assumeIsolated { self?.onSetup?() } }
        }
        if st == noErr {
            st = MIDIInputPortCreateWithProtocol(client, "FIZZ OMX-27 in" as CFString, ._1_0, &inPort) { @Sendable [weak self] list, refCon in
                let gen = Int(bitPattern: refCon)
                let bytes = Self.sysexBytes(list)
                guard !bytes.isEmpty else { return }
                DispatchQueue.main.async { MainActor.assumeIsolated { self?.received(bytes, gen) } }
            }
        }
        if st == noErr { st = MIDIOutputPortCreate(client, "FIZZ OMX-27 out" as CFString, &outPort) }
        if st != noErr {
            if client != 0 { MIDIClientDispose(client) }
            client = 0; inPort = 0; outPort = 0
            return "MIDI did not start (CoreMIDI error \(st)): try again."
        }
        return nil
    }

    public func stop() {
        onSetup = nil
        close()
    }

    public func ports() -> (inputs: [OmxPort], outputs: [OmxPort]) {
        func port(_ e: MIDIEndpointRef) -> OmxPort? {
            var off: Int32 = 0, id: Int32 = 0
            MIDIObjectGetIntegerProperty(e, kMIDIPropertyOffline, &off)
            guard off == 0 else { return nil }
            MIDIObjectGetIntegerProperty(e, kMIDIPropertyUniqueID, &id)
            return OmxPort(id: id, name: Self.name(e), ref: e)
        }
        let ins = (0..<MIDIGetNumberOfSources()).compactMap { port(MIDIGetSource($0)) }
        let outs = (0..<MIDIGetNumberOfDestinations()).compactMap { port(MIDIGetDestination($0)) }
        return (ins, outs)
    }

    public func open(input: OmxPort, output: OmxPort, onMessage: @escaping @MainActor ([UInt8]) -> Void) {
        close()
        guard inPort != 0 else { return }
        self.onMessage = onMessage
        source = input.ref
        dest = output.ref
        MIDIPortConnectSource(inPort, source, UnsafeMutableRawPointer(bitPattern: generation))
    }

    public func close() {
        if source != 0, inPort != 0 { MIDIPortDisconnectSource(inPort, source) }
        source = 0
        dest = 0
        onMessage = nil
        assembler = OmxSysexAssembler()
        generation += 1
    }

    public func send(_ bytes: [UInt8], delayMs: Double) {
        guard dest != 0, outPort != 0 else { return }
        let words = OmxUMP.sysex7(bytes)
        let size = MemoryLayout<MIDIEventList>.size + words.count * 8 + 64
        let raw = UnsafeMutableRawPointer.allocate(byteCount: size, alignment: 8)
        defer { raw.deallocate() }
        let list = raw.bindMemory(to: MIDIEventList.self, capacity: 1)
        let time: MIDITimeStamp = delayMs > 0 ? mach_absolute_time() + MIDITimeStamp(delayMs * ticksPerMs) : 0
        var pkt: UnsafeMutablePointer<MIDIEventPacket>? = MIDIEventListInit(list, ._1_0)
        words.withUnsafeBufferPointer { w in
            var i = 0
            while i < w.count, let p = pkt {
                pkt = MIDIEventListAdd(list, size, p, time, 2, w.baseAddress! + i)
                i += 2
            }
        }
        guard pkt != nil else { return }
        MIDISendEventList(outPort, dest, list)
    }

    private func received(_ bytes: [UInt8], _ gen: Int) {
        guard gen == generation, let onMessage else { return }
        for m in assembler.feed(bytes) { onMessage(m) }
    }

    /// (CoreMIDI's thread) an event list's SysEx bytes
    private nonisolated static func sysexBytes(_ list: UnsafePointer<MIDIEventList>) -> [UInt8] {
        var out: [UInt8] = []
        let off = MemoryLayout<MIDIEventPacket>.offset(of: \MIDIEventPacket.words) ?? 12
        for p in list.unsafeSequence() {
            let n = Int(p.pointee.wordCount)
            let w = UnsafeRawPointer(p).advanced(by: off).assumingMemoryBound(to: UInt32.self)
            out += OmxUMP.bytes(UnsafeBufferPointer(start: w, count: n))
        }
        return out
    }

    private static func name(_ e: MIDIEndpointRef) -> String {
        for key in [kMIDIPropertyDisplayName, kMIDIPropertyName] {
            var s: Unmanaged<CFString>?
            if MIDIObjectGetStringProperty(e, key, &s) == noErr, let v = s?.takeRetainedValue() as String?, !v.isEmpty { return v }
        }
        return ""
    }

    /// the endpoint's unique ID and name (OmxLink.owns)
    public static func identity(_ e: MIDIEndpointRef) -> (id: Int32, name: String) {
        var id: Int32 = 0
        MIDIObjectGetIntegerProperty(e, kMIDIPropertyUniqueID, &id)
        return (id, name(e))
    }
}
#endif
