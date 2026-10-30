// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// PLAY's controller input (mock-up 55, MIDI & audio › MIDI input): the CoreMIDI sources listed (All inputs, None, or
// one device), live as devices come and go. Never listed: the FM-1 in use (the manager's port), an FM-1 used as the
// panel, and an OMX-27 while it is in REMOTE mode (its messages are OmxLink's). Its own CoreMIDI client and input port
// (MIDI 1.0 protocol); each message, whole, to the router on the main thread. Written for FIZZ (RemoteFM1's
// ControllerInput is not used).

import FizzKit
import Foundation
import Observation
#if canImport(CoreMIDI)
import CoreMIDI
#endif

/// A source a controller can play from.
public struct MidiDevice: Sendable, Hashable, Identifiable {
    /// CoreMIDI's unique ID
    public let id: Int32
    public let name: String
    /// the endpoint (0: a test's)
    public let ref: UInt32
    public init(id: Int32, name: String, ref: UInt32 = 0) { self.id = id; self.name = name; self.ref = ref }
}

@MainActor @Observable
public final class MidiInput {
    /// MIDI input: All inputs, None, or one device (by its unique ID)
    public enum Selection: Equatable, Sendable {
        case all, none, one(Int32)

        /// as kept: "*" all, "" none, the ID (the website's and RemoteFM1's form)
        public init(stored: String?) {
            switch stored {
            case nil, "*": self = .all
            case "": self = .none
            case let s?: self = Int32(s).map { .one($0) } ?? .all
            }
        }

        public var stored: String {
            switch self {
            case .all: "*"
            case .none: ""
            case .one(let id): String(id)
            }
        }
    }

    /// What is never a controller: the unit in use, the panel, the OMX-27 in REMOTE mode.
    public struct Skip: Sendable {
        /// the FM-1 in use (its source endpoint and port name)
        public var unitRef: UInt32 = 0
        public var unitName: String?
        /// the FM-1 used as the panel (its port name)
        public var panelName: String?
        /// the OMX-27's port while it is in REMOTE mode
        public var omxID: Int32?
        public var omxName: String?
        public init() {}

        public func skips(_ d: MidiDevice) -> Bool {
            if d.ref != 0 && d.ref == unitRef { return true }
            if let n = unitName, d.name == n { return true }
            if let n = panelName, d.name == n { return true }
            if let i = omxID, d.id == i { return true }
            if let n = omxName, d.name == n { return true }
            return false
        }
    }

    /// The sources there now, less those skipped (the MIDI input page's list).
    public private(set) var devices: [MidiDevice] = []
    public var selection: Selection {
        didSet {
            defaults.set(selection.stored, forKey: Self.key)
            connect()
        }
    }
    /// What is skipped (set again whenever the unit, the panel or the OMX changes).
    public var skip = Skip() { didSet { refresh() } }
    /// CoreMIDI did not start (why)
    public private(set) var problem: String?

    /// each message from the sources listened to
    @ObservationIgnored public var onMessage: (([UInt8]) -> Void)?
    @ObservationIgnored private let defaults: UserDefaults
    @ObservationIgnored private var all: [MidiDevice] = []
    @ObservationIgnored private var listening: Set<UInt32> = []
    @ObservationIgnored private var demo = false
    #if canImport(CoreMIDI)
    @ObservationIgnored private var client = MIDIClientRef()
    @ObservationIgnored private var port = MIDIPortRef()
    #endif

    static let key = "play.midiIn"

    public init(defaults: UserDefaults = .standard) {
        self.defaults = defaults
        selection = Selection(stored: defaults.string(forKey: Self.key))
    }

    /// A fixed list of sources (the demo, the snapshots, the tests): nothing is opened.
    public init(demo devices: [MidiDevice], selection: Selection, defaults: UserDefaults = .standard) {
        self.defaults = defaults
        self.selection = selection
        demo = true
        all = devices
        self.devices = devices
    }

    /// The sources listened to now.
    public var listened: [MidiDevice] { Self.listened(devices, selection) }

    /// The selection's sources among those listed.
    public static func listened(_ listed: [MidiDevice], _ s: Selection) -> [MidiDevice] {
        switch s {
        case .all: listed
        case .none: []
        case .one(let id): listed.filter { $0.id == id }
        }
    }

    /// The name shown on the MIDI input row: "All inputs", "None", the device's (or "Not found" while it is away).
    public var selectionName: String {
        switch selection {
        case .all: "All inputs"
        case .none: "None"
        case .one(let id): devices.first { $0.id == id }?.name ?? "Not here"
        }
    }

    /// CoreMIDI started (once); the sources listed and listened to.
    public func start() {
        guard !demo else { return }
        #if canImport(CoreMIDI)
        if client == 0 {
            var st = MIDIClientCreateWithBlock("FIZZ controller" as CFString, &client) { @Sendable [weak self] n in
                guard n.pointee.messageID == .msgSetupChanged else { return }
                DispatchQueue.main.async { MainActor.assumeIsolated { self?.refresh() } }
            }
            if st == noErr {
                st = MIDIInputPortCreateWithProtocol(client, "FIZZ controller in" as CFString, ._1_0, &port) {
                    @Sendable [weak self] list, _ in
                    let msgs = MidiUMP.messages(list)
                    guard !msgs.isEmpty else { return }
                    DispatchQueue.main.async { MainActor.assumeIsolated { for m in msgs { self?.onMessage?(m) } } }
                }
            }
            if st != noErr {
                if client != 0 { MIDIClientDispose(client) }
                client = 0
                port = 0
                problem = "MIDI did not start (CoreMIDI error \(st))."
                return
            }
        }
        #endif
        refresh()
    }

    /// The sources read again, and listened to as the selection says.
    public func refresh() {
        #if canImport(CoreMIDI)
        if !demo && client != 0 {
            all = (0..<MIDIGetNumberOfSources()).compactMap { i -> MidiDevice? in
                let e = MIDIGetSource(i)
                var off: Int32 = 0, id: Int32 = 0
                MIDIObjectGetIntegerProperty(e, kMIDIPropertyOffline, &off)
                guard off == 0 else { return nil }
                MIDIObjectGetIntegerProperty(e, kMIDIPropertyUniqueID, &id)
                return MidiDevice(id: id, name: Self.name(e), ref: e)
            }
        }
        #endif
        let now = all.filter { !skip.skips($0) }
        if now != devices { devices = now }
        connect()
    }

    private func connect() {
        guard !demo else { return }
        #if canImport(CoreMIDI)
        guard port != 0 else { return }
        let want = Set(listened.map(\.ref))
        for r in listening.subtracting(want) { MIDIPortDisconnectSource(port, r) }
        for r in want.subtracting(listening) { MIDIPortConnectSource(port, r, nil) }
        listening = want
        #endif
    }

    #if canImport(CoreMIDI)
    private static func name(_ e: MIDIEndpointRef) -> String {
        for key in [kMIDIPropertyDisplayName, kMIDIPropertyName] {
            var s: Unmanaged<CFString>?
            if MIDIObjectGetStringProperty(e, key, &s) == noErr, let v = s?.takeRetainedValue() as String?, !v.isEmpty { return v }
        }
        return "MIDI device"
    }
    #endif
}

/// MIDI 1.0 bytes from UMP words: the channel voice messages (type 2) and the system ones (type 1: clock, transport).
/// Everything else (SysEx, MIDI 2.0 voice, utility) is left out.
public enum MidiUMP {
    public static func messages<C: Collection>(_ words: C) -> [[UInt8]] where C.Element == UInt32, C.Index == Int {
        var out: [[UInt8]] = []
        var i = words.startIndex
        while i < words.endIndex {
            let w = words[i], type = w >> 28
            let size: Int = switch type {
            case 0x0, 0x1, 0x2, 0x6, 0x7: 1
            case 0x3, 0x4, 0x8, 0x9, 0xA: 2
            case 0xB, 0xC: 3
            default: 4
            }
            let st = UInt8((w >> 16) & 0xFF), d1 = UInt8((w >> 8) & 0x7F), d2 = UInt8(w & 0x7F)
            if type == 0x2, st >= 0x80, st < 0xF0 {
                let k = st & 0xF0
                out.append(k == 0xC0 || k == 0xD0 ? [st, d1] : [st, d1, d2])
            } else if type == 0x1, st >= 0xF8 {
                out.append([st])
            }
            i += size
        }
        return out
    }

    #if canImport(CoreMIDI)
    /// (CoreMIDI's thread) an event list's messages
    static func messages(_ list: UnsafePointer<MIDIEventList>) -> [[UInt8]] {
        var out: [[UInt8]] = []
        let off = MemoryLayout<MIDIEventPacket>.offset(of: \MIDIEventPacket.words) ?? 12
        for p in list.unsafeSequence() {
            let n = Int(p.pointee.wordCount)
            let w = UnsafeRawPointer(p).advanced(by: off).assumingMemoryBound(to: UInt32.self)
            out += messages(UnsafeBufferPointer(start: w, count: n))
        }
        return out
    }
    #endif
}
