/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* libremotefm1's own host tests (host/tests at RemoteFM1 f44af95, vendored/), each main renamed (run_*.c): 0 when
 * every check passed. Test code only: no product links it. */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

int rfm_check_codec_main(void);
int rfm_check_panel_main(void);
int rfm_check_screen_main(int argc, char **argv);
int rfm_check_session_main(void);

#ifdef __cplusplus
}
#endif
