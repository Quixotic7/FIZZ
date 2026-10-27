// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// Bluetooth LE MIDI pairing: iOS's own panel (CoreAudioKit). A paired FM-1 is then an ordinary CoreMIDI port.

import SwiftUI

#if os(iOS)
import CoreAudioKit
import UIKit

struct BluetoothSheet: View {
    var body: some View { BluetoothPanel().ignoresSafeArea() }
}

private struct BluetoothPanel: UIViewControllerRepresentable {
    func makeUIViewController(context: Context) -> UINavigationController {
        let nav = UINavigationController(rootViewController: CABTMIDICentralViewController())
        nav.overrideUserInterfaceStyle = .dark
        return nav
    }
    func updateUIViewController(_ vc: UINavigationController, context: Context) {}
}
#else
struct BluetoothSheet: View {
    @Environment(\.dismiss) private var dismiss
    var body: some View {
        IconView(Icons.bluetooth, color: FZ.blue.color, lineWidth: 2.6)
            .frame(width: 80, height: 80)
            .padding(60)
            .onTapGesture { dismiss() }
    }
}
#endif
