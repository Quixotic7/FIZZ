// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// The unit picker's demo (`-demo units`, fizzshots, the tests): the demo FIZZ on USB and an FM-1 paired over
// Bluetooth that does not answer, beside the app's emulated FIZZ. `-demo panel` (mock-up 57): a RemoteFM1 unit on USB
// instead of the FIZZ one (libremotefm1's fake unit, DemoRemoteFM1Port), which can be THIS iPHONE's panel.

import Foundation
import FizzEngine
import FizzKit

/// The demo FM-1 on USB ("FIZZ FM-1") and a silent one over Bluetooth ("FM-1 Bluetooth"); either can be pulled out.
public final class DemoUnitsSource: LinkSource, MIDIPortScanning, @unchecked Sendable {
    public static let silentName = "FM-1 Bluetooth"
    /// The RemoteFM1 units' ports (`remoteFM1`, `remoteFM1Bluetooth`).
    public static let panelName = "RemoteFM1 FM-1"
    public static let panelBluetoothName = "FM-1 7938 Bluetooth"
    public let demo: DemoScanner
    public var scanner: any MIDIPortScanning { self }
    public let changes: AsyncStream<Void>
    private let changed: AsyncStream<Void>.Continuation
    public let clock: any FizzClock = SystemClock()
    private let lock = NSLock()
    private var usb: Bool
    private var ble: Bool
    private var rfm: Bool
    private let rfmBle: Bool

    /// remoteFM1: a RemoteFM1 unit on USB (plugged in or pulled out with setRemoteFM1); remoteFM1Bluetooth: one over
    /// Bluetooth.
    public init(usb: Bool = true, bluetooth: Bool = true, remoteFM1: Bool = false, remoteFM1Bluetooth: Bool = false,
                bulkMs: ClosedRange<Double> = 3...6) {
        demo = DemoScanner(units: [.fizz(port: "FIZZ FM-1", bulkMs: bulkMs)])
        self.usb = usb
        ble = bluetooth
        rfm = remoteFM1
        rfmBle = remoteFM1Bluetooth
        (changes, changed) = AsyncStream.makeStream(of: Void.self, bufferingPolicy: .bufferingNewest(1))
    }

    static let silent = MIDIPortCandidate(source: MIDIEndpointInfo(ref: 900, name: silentName, entity: 900, device: 900),
                                          destination: MIDIEndpointInfo(ref: 901, name: silentName, entity: 900, device: 900))

    static let panel = MIDIPortCandidate(source: MIDIEndpointInfo(ref: 910, name: panelName, entity: 910, device: 910),
                                         destination: MIDIEndpointInfo(ref: 911, name: panelName, entity: 910, device: 910))
    static let panelBluetooth = MIDIPortCandidate(
        source: MIDIEndpointInfo(ref: 920, name: panelBluetoothName, entity: 920, device: 920),
        destination: MIDIEndpointInfo(ref: 921, name: panelBluetoothName, entity: 920, device: 920))

    public func candidates() -> [MIDIPortCandidate] {
        let (u, b, r) = lock.withLock { (usb, ble, rfm) }
        return (u ? demo.candidates() : []) + (r ? [Self.panel] : []) + (rfmBle ? [Self.panelBluetooth] : [])
            + (b ? [Self.silent] : [])
    }

    public func open(_ c: MIDIPortCandidate) throws -> any MIDITransport {
        if c == Self.silent { return SilentPort(name: c.name) }
        if c == Self.panel { return DemoRemoteFM1Port(name: c.name) }
        if c == Self.panelBluetooth { return DemoRemoteFM1Port(name: c.name, bluetooth: true) }
        return try demo.open(c)
    }

    /// The RemoteFM1 unit on USB plugged in or pulled out.
    public func setRemoteFM1(_ on: Bool) {
        lock.withLock { rfm = on }
        changed.yield()
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
