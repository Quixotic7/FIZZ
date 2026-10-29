// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// Where the app's units come from: CoreMIDI's ports, or the demo unit (launched with -demo, -demo update|notfizz|two).
// (The LinkSource protocol itself is FizzKit's, so FizzEngine's emulated unit can be one too.)

import Foundation
import FizzKit

#if canImport(CoreMIDI)
/// CoreMIDI's ports. Make it on the main thread (CoreMIDI notifies on the thread that made the client).
public struct CoreMIDILinkSource: LinkSource {
    let midi: MIDIPortScanner
    public var scanner: any MIDIPortScanning { midi }
    public var changes: AsyncStream<Void> { midi.portsChanged }
    public let clock: any FizzClock = SystemClock()

    public init() throws { midi = try MIDIPortScanner(clientName: "FIZZ") }
}
#endif

public enum DemoVariant: String, Sendable, CaseIterable {
    case fizz, update, notfizz, two
}

/// The demo units behind DemoScanner.
public final class DemoLinkSource: LinkSource, @unchecked Sendable {
    public let variant: DemoVariant
    public let demo: DemoScanner
    public var scanner: any MIDIPortScanning { demo }
    public let changes: AsyncStream<Void>
    private let changed: AsyncStream<Void>.Continuation
    public let clock: any FizzClock

    /// bulkMs: the FIZZ unit's FZBR to FZBD time (ms; 3..6 as a unit).
    public init(_ variant: DemoVariant = .fizz, clock: any FizzClock = SystemClock(), bulkMs: ClosedRange<Double> = 3...6) {
        self.variant = variant
        self.clock = clock
        let units: [DemoUnit] = switch variant {
        case .fizz: [.fizz(bulkMs: bulkMs)]
        case .update: [.updateMode()]
        case .notfizz: [.notFizz()]
        case .two: [.fizz(bulkMs: bulkMs), .choralRoot()]
        }
        demo = DemoScanner(units: units)
        (changes, changed) = AsyncStream.makeStream(of: Void.self, bufferingPolicy: .bufferingNewest(1))
    }

    /// As if a port came or went.
    public func poke() { changed.yield() }
}

public enum LinkSources {
    /// From the launch arguments: `-demo [update|notfizz|two]` gives the demo unit, `-emulator` no FM-1 at all (the
    /// emulated FIZZ is the unit), else CoreMIDI.
    public static func fromArguments(_ args: [String] = ProcessInfo.processInfo.arguments) -> any LinkSource {
        if args.contains("-emulator") { return NoPorts() }
        if let k = args.firstIndex(of: "-demo") {
            let v = k + 1 < args.count ? DemoVariant(rawValue: args[k + 1]) : nil
            return DemoLinkSource(v ?? .fizz)
        }
        #if canImport(CoreMIDI)
        if let s = try? CoreMIDILinkSource() { return s }
        #endif
        return NoPorts()
    }
}

/// No MIDI at all (CoreMIDI refused a client): the app keeps looking.
public struct NoPorts: LinkSource, MIDIPortScanning {
    public init() {}
    public var scanner: any MIDIPortScanning { self }
    public var changes: AsyncStream<Void> { AsyncStream { _ in } }
    public var clock: any FizzClock { SystemClock() }
    public func candidates() -> [MIDIPortCandidate] { [] }
    public func open(_ candidate: MIDIPortCandidate) throws -> any MIDITransport { throw MIDITransportError.lost }
}
