// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// FIZZ for iPhone and iPad: the web Manager over CoreMIDI. Launched with -demo (or -demo update|notfizz|two) it runs
// against a FIZZ unit simulated in the app.

import FizzUI
import SwiftUI

@main
struct FIZZApp: App {
    // (the iPhone turns sideways on every tab, the iPad every way)
    @UIApplicationDelegateAdaptor(FizzAppDelegate.self) private var delegate

    var body: some Scene {
        WindowGroup {
            FizzAppView(source: LinkSources.fromArguments())
        }
    }
}
