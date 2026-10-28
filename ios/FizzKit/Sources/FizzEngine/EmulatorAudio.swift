// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// The emulated FIZZ's sound: an AVAudioSourceNode at the core's own 44.1 kHz whose render callback runs the device
// clock (fzh_render: the device clock is the audio clock, as in FM1VST's plugin). The engine's mixer converts to the
// hardware's rate when it is not 44.1 kHz (the session asks for 44.1 kHz first).

import AVFoundation
import CFizzCore
import Foundation

final class EmulatorAudio: @unchecked Sendable {
    let engine = AVAudioEngine()
    private var observers: [NSObjectProtocol] = []
    private let lock = NSLock()
    private var stopped = false

    init() throws {
        #if os(iOS)
        let s = AVAudioSession.sharedInstance()
        try s.setCategory(.playback, mode: .default, options: [])
        try? s.setPreferredSampleRate(FizzCoreInfo.sampleRate)
        try? s.setPreferredIOBufferDuration(256.0 / FizzCoreInfo.sampleRate)
        try s.setActive(true)
        #endif
        guard let fmt = AVAudioFormat(standardFormatWithSampleRate: FizzCoreInfo.sampleRate, channels: 2) else {
            throw EmulatorError.audio("no 44.1 kHz stereo format")
        }
        let node = AVAudioSourceNode(format: fmt) { _, _, frames, abl -> OSStatus in
            let bufs = UnsafeMutableAudioBufferListPointer(abl)
            guard bufs.count >= 2, let l = bufs[0].mData?.assumingMemoryBound(to: Float.self),
                  let r = bufs[1].mData?.assumingMemoryBound(to: Float.self) else { return noErr }
            _ = fzh_render(l, r, frames, Double(FZH_FS))
            return noErr
        }
        engine.attach(node)
        engine.connect(node, to: engine.mainMixerNode, format: fmt)
        engine.prepare()
        try engine.start()
        let nc = NotificationCenter.default
        // the engine stops on a route or format change, and on an interruption: started again after
        observers.append(nc.addObserver(forName: .AVAudioEngineConfigurationChange, object: engine, queue: nil) {
            [weak self] _ in self?.restart()
        })
        #if os(iOS)
        observers.append(nc.addObserver(forName: AVAudioSession.interruptionNotification, object: nil, queue: nil) {
            [weak self] n in
            let t = (n.userInfo?[AVAudioSessionInterruptionTypeKey] as? UInt).flatMap(AVAudioSession.InterruptionType.init)
            if t == .ended { self?.restart() }
        })
        #endif
    }

    var isRunning: Bool { engine.isRunning }

    private func restart() {
        lock.withLock {
            guard !stopped, !engine.isRunning else { return }
            #if os(iOS)
            try? AVAudioSession.sharedInstance().setActive(true)
            #endif
            try? engine.start()
        }
    }

    /// Stopped for good (no render callback runs after this returns).
    func stop() {
        lock.withLock {
            stopped = true
            for o in observers { NotificationCenter.default.removeObserver(o) }
            observers.removeAll()
            engine.stop()
        }
        #if os(iOS)
        try? AVAudioSession.sharedInstance().setActive(false, options: .notifyOthersOnDeactivation)
        #endif
    }
}

public enum EmulatorError: Error, Equatable {
    case audio(String)
    case notBooted
}
