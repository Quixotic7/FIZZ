// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// Which ways the app turns. Until 0.12 the iPhone turned sideways only while PLAY showed, and leaving PLAY turned it
// back upright; now the manager has a sideways layout (LandscapeRoot.swift), so every tab turns with the hand and
// switching between PLAY and the manager never turns the screen.

#if os(iOS)
import UIKit
#endif

/// The ways the screen can be turned.
public enum Turn: String, CaseIterable, Sendable {
    case portrait, upsideDown, landscapeLeft, landscapeRight

    public var sideways: Bool { self == .landscapeLeft || self == .landscapeRight }
}

public enum Turning {
    /// The ways allowed: the iPad all four; the iPhone all but upside down. The same on PLAY and on the manager's
    /// tabs, so going from one to the other keeps the screen the way it is.
    public static func allowed(pad: Bool, playShown: Bool = false) -> Set<Turn> {
        pad ? Set(Turn.allCases) : [.portrait, .landscapeLeft, .landscapeRight]
    }
}

#if os(iOS)
extension Set where Element == Turn {
    var mask: UIInterfaceOrientationMask {
        var m: UIInterfaceOrientationMask = []
        if contains(.portrait) { m.insert(.portrait) }
        if contains(.upsideDown) { m.insert(.portraitUpsideDown) }
        if contains(.landscapeLeft) { m.insert(.landscapeLeft) }
        if contains(.landscapeRight) { m.insert(.landscapeRight) }
        return m
    }
}
#endif
