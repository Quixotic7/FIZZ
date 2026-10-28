// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// The emulated FIZZ as the app's unit source: one port pair, "FIZZ (this iPhone)", which FizzLink.probe / find and
// UnitModel connect to as to an FM-1 (the update handshake, IDENTITY, then everything else).

import FizzKit
import Foundation

public final class EmulatorLinkSource: LinkSource, MIDIPortScanning, @unchecked Sendable {
    #if os(iOS)
    public static let defaultPortName = "FIZZ (this iPhone)"
    #else
    public static let defaultPortName = "FIZZ (this Mac)"
    #endif

    public let emulator: FizzEmulator
    public let portName: String
    public var scanner: any MIDIPortScanning { self }
    public let changes: AsyncStream<Void>
    private let changed: AsyncStream<Void>.Continuation
    public let clock: any FizzClock

    public init(emulator: FizzEmulator, portName: String = EmulatorLinkSource.defaultPortName,
                clock: any FizzClock = SystemClock()) {
        self.emulator = emulator
        self.portName = portName
        self.clock = clock
        (changes, changed) = AsyncStream.makeStream(of: Void.self, bufferingPolicy: .bufferingNewest(1))
    }

    /// The one port pair (its refs are no CoreMIDI object's).
    public var candidate: MIDIPortCandidate {
        MIDIPortCandidate(source: MIDIEndpointInfo(ref: 0x4649_0001, name: portName, entity: 0x4649_0003, device: 0x4649_0004),
                          destination: MIDIEndpointInfo(ref: 0x4649_0002, name: portName, entity: 0x4649_0003,
                                                        device: 0x4649_0004))
    }

    /// The port while the emulated FIZZ is powered on.
    public func candidates() -> [MIDIPortCandidate] { emulator.isBooted ? [candidate] : [] }

    public func open(_ candidate: MIDIPortCandidate) throws -> any MIDITransport {
        guard emulator.isBooted, candidate == self.candidate else { throw MIDITransportError.lost }
        return emulator.makeTransport(name: portName)
    }

    /// As if the port came or went (after a boot, a shutdown).
    public func poke() { changed.yield() }
}
