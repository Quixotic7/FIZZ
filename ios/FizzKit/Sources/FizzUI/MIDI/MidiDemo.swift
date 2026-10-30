// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// The mock-ups' MIDI (55, 56) for the snapshots and the simulator: the learned map drawn (KN1, KN2, KN3, MASTER, PLAY),
// two controllers (KeyStep 37, nanoKONTROL2), an OMX-27 that answers REMOTE mode (no device is touched), all in memory.
// Launch argument `-midi <state>` (PLAY's first showing): page, knobs, omx, input (the sheet's page, scrolled), learn,
// pick:<ID> (learn with a control picked), bound:<ID> (and bound), pickup (after Done: MASTER waiting).

import Foundation

/// An OMX-27 in REMOTE mode that is not there: it answers MODE with "entered" and takes whatever is sent.
@MainActor public final class MidiDemoOmx: OmxTransport {
    private var onMessage: (@MainActor ([UInt8]) -> Void)?
    public init() {}
    public func start(onSetupChange: @escaping @MainActor () -> Void) -> String? { nil }
    public func stop() {}
    public func ports() -> (inputs: [OmxPort], outputs: [OmxPort]) {
        ([OmxPort(id: 27, name: "OMX-27")], [OmxPort(id: 28, name: "OMX-27")])
    }
    public func open(input: OmxPort, output: OmxPort, onMessage: @escaping @MainActor ([UInt8]) -> Void) {
        self.onMessage = onMessage
    }
    public func close() { onMessage = nil }
    public func send(_ bytes: [UInt8], delayMs: Double) {
        guard bytes == OmxWire.enterRemote() else { return }
        Task { @MainActor [weak self] in self?.onMessage?(OmxWire.statusMsg(OmxWire.STATUS.on)) }
    }
}

extension PlayModel {
    /// The map drawn in 55.
    public static let demoMap = MidiMap([
        MidiMapping(src: "cc:1:21", target: "KN1", mode: .abs), MidiMapping(src: "cc:1:22", target: "KN2", mode: .rel64),
        MidiMapping(src: "cc:1:23", target: "KN3", mode: .abs), MidiMapping(src: "cc:1:7", target: "MASTER"),
        MidiMapping(src: "note:10:36", target: "PLAY"),
    ])

    /// PLAY's MIDI as the mock-ups draw it: the demo map, KeyStep 37 picked, the OMX-27 connected (or not).
    public func useMidiDemo(omx connected: Bool = true) async {
        let d = UserDefaults(suiteName: "fizz.midi.demo") ?? .standard
        d.removePersistentDomain(forName: "fizz.midi.demo")
        let input = MidiInput(demo: [MidiDevice(id: 1, name: "KeyStep 37"), MidiDevice(id: 2, name: "nanoKONTROL2")],
                              selection: .one(1), defaults: d)
        let omx = OmxLink(transport: MidiDemoOmx(), clock: OmxMainClock(), defaults: d)
        useMidi(store: MidiMapStore(map: Self.demoMap), input: input, omx: omx)
        midi.transpose = 1
        if connected {
            omx.connect()
            for _ in 0..<50 where !omx.active { try? await Task.sleep(for: .milliseconds(10)) }
        }
    }

    /// PLAY shown: the controller input and the OMX-27 started, or the demo a `-midi` launch argument asks for.
    public func startMidi() {
        let args = ProcessInfo.processInfo.arguments
        guard let i = args.firstIndex(of: "-midi"), i + 1 < args.count else { midi.start(); return }
        guard !midiDemoShown else { return }
        midiDemoShown = true
        let state = args[i + 1]
        Task { @MainActor in
            await useMidiDemo()
            showMidiDemo(state)
        }
    }

    /// One of the mock-ups' MIDI states (the launch argument's, the snapshots').
    public func showMidiDemo(_ state: String) {
        let parts = state.split(separator: ":", maxSplits: 1).map(String.init)
        switch parts[0] {
        case "page", "knobs", "omx":
            sheet = true
            sheetPage = .midi
            midiScrollTo = parts[0] == "page" ? nil : parts[0]
        case "input":
            sheet = true
            sheetPage = .input
        case "learn":
            midi.demoLearn(picked: nil)
        case "pick":
            midi.demoLearn(picked: parts.count > 1 ? parts[1] : "KN3")
        case "bound":
            let t = parts.count > 1 ? parts[1] : "KN3"
            midi.demoLearn(picked: t, bound: midi.store.sourcesOf(t).last)
        case "pickup":
            setMaster(634, haptic: false)
            midi.demoPickup(pot: 380, toast: "MASTER: turn the pot to 62% to pick it up")
        default:
            break
        }
    }
}
