# libremotefm1 in FIZZ for iOS: provenance

CRemoteFM1 is the portable core of libremotefm1, the host library of RemoteFM1. It is a protocol v2 codec, the host
session, the panel decoder, the screen encoder, a reference TILES decoder and a fake unit. CRemoteFM1Checks holds
libremotefm1's own host tests.

- **Repository:** RemoteFM1, <https://github.com/Quixotic7/RemoteFM1>
- **Commit:** `f44af95c2c2401ad1dbcadbd1c44b24eb8ba0f0d`, tag `relicense-mpl-2026-10-30`, branch `mpl-relicense`.
  That commit's message: "Relicense the host library's portable core and the site's MIDI learn / OMX-27 modules to
  MPL-2.0 (the author's decision)".
- **Licence:** MPL-2.0. The first line of every file says so at that commit, and RemoteFM1's `LICENSING.md` agrees.
  Earlier RemoteFM1 commits of the host files are GPL-3.0-only. Do not take them from there.
- **Copied:** 2026-10-30, for branch `ios-rfm-panel-core` (part V).
- **Changes:** none. Every file is byte-identical to the commit. The git blob ids match, and they are listed below.
  The layout puts the files where their own `#include` lines find them: `include/` is the public header path,
  and `remote_proto.h` sits beside `codec.c`. No include line was changed.

| Here | RemoteFM1 at f44af95 | blob |
|---|---|---|
| `src/codec.c` | `host/src/codec.c` | b61162c78d |
| `src/session.c` | `host/src/session.c` | 30eab74c63 |
| `src/panel.c` | `host/src/panel.c` | 69d2f7aee8 |
| `src/screen.c` | `host/src/screen.c` | c2fbf4450a |
| `src/tiles_ref.c` | `host/src/tiles_ref.c` | 91b7e13a73 |
| `src/fake.c` | `host/src/fake.c` | 36c101f8a5 |
| `src/remote_proto.h` | `firmware/src/remote_proto.h` (the framing helpers `codec.c` includes; MPL-2.0 firmware, vendored with the lead's approval) | 187d136de6 |
| `include/rfm/codec.h` | `host/include/rfm/codec.h` | 555e7ac5a3 |
| `include/rfm/session.h` | `host/include/rfm/session.h` | 4056f74ee0 |
| `include/rfm/panel.h` | `host/include/rfm/panel.h` | dcc85e0087 |
| `include/rfm/screen.h` | `host/include/rfm/screen.h` | 674da64d0c |
| `include/rfm/tiles_ref.h` | `host/include/rfm/tiles_ref.h` | c151ff7c05 |
| `include/rfm/fake.h` | `host/include/rfm/fake.h` | c791640e69 |
| `include/rfm1.h` | `proto/rfm1.h` (MPL-2.0 since 4bf5c3b; taken at f44af95, which the codec was written against) | 83fd819924 |
| `../CRemoteFM1Checks/vendored/check.h` | `host/tests/check.h` | 0dd0985678 |
| `../CRemoteFM1Checks/vendored/frames.c` | `host/tests/frames.c` | 23761f1a7a |
| `../CRemoteFM1Checks/vendored/frames.h` | `host/tests/frames.h` | 581bdb1198 |
| `../CRemoteFM1Checks/vendored/test_codec.c` | `host/tests/test_codec.c` | 4df54c409e |
| `../CRemoteFM1Checks/vendored/test_panel.c` | `host/tests/test_panel.c` | c54f13b969 |
| `../CRemoteFM1Checks/vendored/test_screen.c` | `host/tests/test_screen.c` | ce5690ff55 |
| `../CRemoteFM1Checks/vendored/test_session.c` | `host/tests/test_session.c` | 2b34d13037 |

**Not copied:**

- `host/tests/test_cross.c`. It compiles the device's own `firmware/src/rfm_app.c`, `rfm_draw.c` and `rfm_push.c`,
  and the lead said to leave it out.
- `host/tests/test_coremidi.c`. It needs the macOS back end.
- `host/src/macos/`, `host/include/rfm/macos.h`, `docs/PROTOCOL.md` and the tools. These are GPL-3.0-only.
- Nothing from RemoteFM1IOS, which is GPL-3.0.

**Written for FIZZ (MPL-2.0):**

- `CRemoteFM1Checks/run_*.c`. Each wrapper includes one vendored test with its `main` renamed.
- `CRemoteFM1Checks/include/CRemoteFM1Checks.h`
- `Tests/RemoteFM1Tests`. These run the vendored tests under `swift test`.
- Everything that uses the library: `FizzEngine/PanelLink.swift`, `RemoteFM1Unit.swift` and their tests. They were
  written from libremotefm1's headers and RemoteFM1's protocol document. Nothing was copied from either.

**Updating:** copy the same paths from a newer RemoteFM1 commit only after checking that every file's first line still
says MPL-2.0. Record that commit here and change nothing in the files.
