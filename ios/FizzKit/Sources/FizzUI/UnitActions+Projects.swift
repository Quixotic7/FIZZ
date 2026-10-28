// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// PROJECTS' one write the write layer did not have (phase I6): NEW, the FM-1's PROJECT 2 (core/app/persist.c
// qd_proj_new: an empty project in slot p, opened; the web Manager has no NEW).

import Foundation
import FizzKit

extension UnitActions {
    /// NEW: an empty project in slot p (1..8), opened on the unit (the screen offers it on an empty slot only: the
    /// unit erases a slot's project first).
    public func newProject(_ p: Int) async -> UnitOutcome {
        guard canWrite, let link else { return .notNow }
        do {
            let r = try await link.project(.new, p)
            let o = Self.outcome(r)
            if case .refused = o { say("NEW: \(o.text("NEW"))") }
            await refreshRows(projects: true)
            return o
        } catch {
            say("NEW: \(error)")
            return .noAnswer
        }
    }
}
