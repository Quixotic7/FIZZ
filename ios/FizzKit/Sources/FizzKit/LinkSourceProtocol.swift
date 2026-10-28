// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// Where the app's units come from (FizzUI's CoreMIDI and demo sources, FizzEngine's emulated FIZZ).

import Foundation

/// Port pairs to probe, and a signal whenever ports come or go.
public protocol LinkSource: Sendable {
    var scanner: any MIDIPortScanning { get }
    /// one value per change of the ports (one listener)
    var changes: AsyncStream<Void> { get }
    var clock: any FizzClock { get }
}
