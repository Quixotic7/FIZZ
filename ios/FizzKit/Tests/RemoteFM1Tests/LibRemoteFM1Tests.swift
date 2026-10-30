// SPDX-License-Identifier: MPL-2.0
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// libremotefm1's own host tests (RemoteFM1's host/tests, vendored in CRemoteFM1Checks unchanged): the codec, the
// panel decoder, the screen encoder and the session against the fake unit. Each returns 0 when every check passed (its
// failures on stderr). test_cross (the device's own code) and test_coremidi (the GPL macOS back end) are not here.

import CRemoteFM1Checks
import Testing

@Suite("libremotefm1", .serialized)
struct LibRemoteFM1Tests {
    @Test func codec() { #expect(rfm_check_codec_main() == 0) }
    @Test func panel() { #expect(rfm_check_panel_main() == 0) }
    @Test func session() { #expect(rfm_check_session_main() == 0) }

    @Test func screen() {
        var name = Array("test_screen".utf8CString)
        let r = name.withUnsafeMutableBufferPointer { b -> Int32 in
            var argv: [UnsafeMutablePointer<CChar>?] = [b.baseAddress, nil]
            return rfm_check_screen_main(1, &argv)
        }
        #expect(r == 0)
    }
}
