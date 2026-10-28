// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// Bluetooth LE MIDI pairing: iOS's own panel (CoreAudioKit), over the top screen with a DONE button. A paired FM-1 is
// then an ordinary CoreMIDI port.

import SwiftUI

#if os(iOS)
import CoreAudioKit
import ObjectiveC
import UIKit

@MainActor
enum BluetoothPairing {
    static func present() {
        let scenes = UIApplication.shared.connectedScenes.compactMap { $0 as? UIWindowScene }
        let scene = scenes.first { $0.activationState == .foregroundActive } ?? scenes.first
        guard let root = scene?.keyWindow?.rootViewController ?? scene?.windows.first?.rootViewController else { return }
        var top = root
        while let p = top.presentedViewController { top = p }
        let panel = PairingPanel()
        let nav = UINavigationController(rootViewController: panel)
        panel.navigationItem.rightBarButtonItem = UIBarButtonItem(systemItem: .done, primaryAction: UIAction { [weak nav] _ in
            nav?.dismiss(animated: true)
        })
        nav.overrideUserInterfaceStyle = .dark
        nav.modalPresentationStyle = .formSheet
        top.present(nav, animated: true)
    }
}

/// iOS's panel. Its loadView makes the table at UIScreen's applicationFrame, which iOS 27 gives as NaN high: the app
/// stopped (CALayerInvalidGeometry) as the panel opened. Then the same table is made at the screen's bounds.
private final class PairingPanel: CABTMIDICentralViewController {
    override func loadView() {
        let f = (UIScreen.main.value(forKey: "applicationFrame") as? NSValue)?.cgRectValue ?? .null
        guard f.origin.y.isNaN || f.height.isNaN || f.width.isNaN else { super.loadView(); return }
        let t = UITableView(frame: UIScreen.main.bounds, style: .insetGrouped)
        t.autoresizingMask = [.flexibleWidth, .flexibleHeight]
        t.delegate = self
        t.dataSource = self
        t.allowsSelection = true
        t.allowsMultipleSelectionDuringEditing = true
        view = t
        // (as its own loadView ends: two flags of its own cleared)
        for name in ["didCleanup", "didRegisterObserver"] {
            guard let iv = class_getInstanceVariable(CABTMIDICentralViewController.self, name) else { continue }
            Unmanaged.passUnretained(self).toOpaque().advanced(by: ivar_getOffset(iv)).storeBytes(of: 0, as: UInt8.self)
        }
    }
}
#else
@MainActor
enum BluetoothPairing {
    static func present() {}
}
#endif
