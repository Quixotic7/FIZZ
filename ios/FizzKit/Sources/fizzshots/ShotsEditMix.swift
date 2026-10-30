// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// fizzshots for mock-ups 45 and 46: AMP's PAN and sends (ROUND BASS on TRACK 3: PAN L12, CHO 0, DEL 24, REV 40; PAN
// turned), the iPad's AMP; LIBRARY's + NEW sheet, a new MIDI sound and a new FM sound in EDIT (unsaved).

#if os(macOS)
import FizzData
import FizzKit
import FizzUI
import SwiftUI

extension Shots {
    @MainActor
    static func editMixShots(_ m: UnitModel, _ out: URL) async {
        let e = m.editor
        let edit = { (name: String) in save(RootView(model: m, tab: .constant(.edit), wide: false), phone, out, name) }
        // mock-up 45
        await m.actions.setTrackOn(true)
        m.editTarget = .preset(m.mine[7])
        await e.show()
        await e.keepOn()
        e.page = .amp
        e.set(EditMix.pan, -12); e.set(EditMix.chorus, 0); e.set(EditMix.delay, 24); e.set(EditMix.reverb, 40)
        edit("edit-mix")
        e.turning = EditTurn(id: EditMix.pan, label: "PAN", tint: FZ.text, top: true)
        edit("edit-mix-pan")
        e.turning = EditTurn(id: EditMix.reverb, label: "REVERB", tint: FZ.violet, top: true)
        edit("edit-mix-reverb")
        e.turning = nil
        save(RootView(model: m, tab: .constant(.edit), wide: true), pad, out, "ipad-edit-mix")
        await e.revert()
        e.page = .ops
        e.hide()
        await m.actions.setTrackOn(false)

        // mock-up 46
        save(RootView(model: m, tab: .constant(.library), wide: false), phone, out, "library-new-button")
        save(SnapshotSheet(top: 440) { RootView(model: m, tab: .constant(.library), wide: false) } content: { NewSoundSheetShot(model: m) },
             phone, out, "library-new")
        for midi in [true, false] {
            m.editTarget = .new(midi: midi)
            await e.show()
            edit(midi ? "edit-new-midi" : "edit-new-fm")
            if !midi { save(RootView(model: m, tab: .constant(.edit), wide: true), pad, out, "ipad-edit-new-fm") }
            // (the next one would ask first)
            e.discardNew()
        }
        e.discardNew()
        e.hide()
        await until { !m.actions.borrowed && m.actions.pending == nil }
    }
}
#endif
