// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// The unit picker's demo (`-demo units`, fizzshots, the tests): the demo FIZZ on USB and an FM-1 paired over
// Bluetooth that does not answer, beside the app's emulated FIZZ.

import Foundation
import FizzKit

/// The demo FM-1 on USB ("FIZZ FM-1") and a silent one over Bluetooth ("FM-1 Bluetooth"); either can be pulled out.
public final class DemoUnitsSource: LinkSource, MIDIPortScanning, @unchecked Sendable {
    public static let silentName = "FM-1 Bluetooth"
    public let demo: DemoScanner
    public var scanner: any MIDIPortScanning { self }
    public let changes: AsyncStream<Void>
    private let changed: AsyncStream<Void>.Continuation
    public let clock: any FizzClock = SystemClock()
    private let lock = NSLock()
    private var usb: Bool
    private var ble: Bool

    public init(usb: Bool = true, bluetooth: Bool = true, bulkMs: ClosedRange<Double> = 3...6) {
        demo = DemoScanner(units: [.fizz(port: "FIZZ FM-1", bulkMs: bulkMs)])
        self.usb = usb
        ble = bluetooth
        (changes, changed) = AsyncStream.makeStream(of: Void.self, bufferingPolicy: .bufferingNewest(1))
    }

    static let silent = MIDIPortCandidate(source: MIDIEndpointInfo(ref: 900, name: silentName, entity: 900, device: 900),
                                          destination: MIDIEndpointInfo(ref: 901, name: silentName, entity: 900, device: 900))

    public func candidates() -> [MIDIPortCandidate] {
        let (u, b) = lock.withLock { (usb, ble) }
        return (u ? demo.candidates() : []) + (b ? [Self.silent] : [])
    }

    public func open(_ c: MIDIPortCandidate) throws -> any MIDITransport {
        if c == Self.silent { return SilentPort(name: c.name) }
        return try demo.open(c)
    }

    /// The USB FM-1 plugged in or pulled out.
    public func setUSB(_ on: Bool) {
        lock.withLock { usb = on }
        if !on { demo.unplugAll() }
        changed.yield()
    }

    public func poke() { changed.yield() }
}

/// A port that never answers (an FM-1 paired but asleep).
final class SilentPort: MIDITransport, @unchecked Sendable {
    let name: String
    let messages: AsyncStream<[UInt8]>
    private let cont: AsyncStream<[UInt8]>.Continuation
    var isLost: Bool { false }

    init(name: String) {
        self.name = name
        (messages, cont) = AsyncStream.makeStream(of: [UInt8].self)
    }

    func send(_ bytes: [UInt8]) throws {}
    func close() { cont.finish() }
}
