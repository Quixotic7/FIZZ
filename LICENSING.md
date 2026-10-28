# Licensing

**FIZZ is MPL-2.0** (the full text is in `LICENSE`). Every source file carries an `SPDX-License-Identifier: MPL-2.0`
header and MPL's Exhibit A notice.
- If you change one of these files and distribute it, publish your change to that file under MPL-2.0.
- Files of your own that you build with FIZZ (a platform, an app, a closed library such as JieLi's Bluetooth stack)
  may use any licence.

Until L1 (2026-10-23) FIZZ was GPL-3.0-only. The owner, its author, relicensed his code. The code that came from
Felucca (Leo Kuroshita) through ChoralRoot was not relicensed: it was **rewritten**, clean-room, with every output
unchanged. `research/PROVENANCE.md` lists where each part came from.

## What is under which licence

| Code | Author | Licence |
|---|---|---|
| Everything in `core/` and `tools/` not named below | FIZZ, written for the owner (Michael P Jones, Quixotic7) | MPL-2.0 |
| FM TONE and the effects (`core/voice/quad/eng_quad.c`, `quad_tables.h`, `quad_times.h`; `core/voice/fx/cr_fx.c`, `cr_ram.h`, `fz_out.c`), from ChoralRoot FM-1 v0.25.1 | the owner (ChoralRoot's author) | MPL-2.0 |
| The building blocks, tables and voice parts that replace Felucca's (`core/voice/quad/fz_*.h`, `tools/gen_fz_tables.py`) | FIZZ (L1, clean-room from `research/L1-CLEANROOM-SPEC.md`) | MPL-2.0 |
| ChoralRoot's second-core code (`firmware/src/cr_cpu1.h`, ChoralRoot FM-1), for the port's reuse (P2.6), with a provenance line where it is used | the owner (ChoralRoot's author): relicensed MPL-2.0 for FIZZ on 2026-10-23 | MPL-2.0 |
| `core/tests/ref/cr_quad_ref.c`, `cr_fx_ref.c`: test oracles that compile ChoralRoot's own tree (Felucca code included), fetched into `tools/build/` at test time | the owner; they build GPL code | GPL-3.0-only (`LICENSES/GPL-3.0.txt`): development only, never part of the firmware |
| `site/`: the web page, the browser build, and copies from RemoteFM1 (except the modules named in the next row) | the owner and RemoteFM1's authors | GPL-3.0-only (`LICENSES/GPL-3.0.txt`): not firmware |
| The `site/` data modules FIZZ for iOS bundles or ports: `site/js/fm1dnsnd.js`, `fizz_sysex.js`, `library.js`, `dn2pst.js`, `fizz2dn.js`, `digitone.js`; `site/manager/fizzstore.js`, `fizzbak.js`, `projects.js`, `fizzvalues.js`, `sound.js`, `ui.js`, `importer.js`, `exporter.js`, `fizzlink.js` | the owner | MPL-2.0: relicensed by him on 2026-10-28 so the iOS app carries no GPL (`ios/jsbundle/build.mjs` refuses a GPL module) |
| `ios/`: FIZZ for iOS, and the FM1VST glue its emulated FIZZ compiles in (`core-api/fm1core.h`, `core-glue/common/nor.*`, `panel.h`, `sysex.h`, `version.h`, `fifo.*`, relicensed in FM1VST on 2026-10-28) | the owner | MPL-2.0 |
| `site/install/` (`index.html`, `install.js`, `fizz_data.js`, `notes.js`, `fm1lib.js`) and `site/tools/test_install.mjs`: the install page | fm1-base's web installer (Charles Vestal; Leo Kuroshita, Hügelton Instruments), FIZZ's changes by the owner | MPL-2.0, each file's header kept (see below) |
| `site/firmware/`: the released packages | FIZZ, fm1-base, felucca-bsp | MPL-2.0 (the firmware's licences) |
| `firmware/release.py`, `firmware/tools/sync_installer.py`, `CHANGELOG.md`, `.github/workflows/release.yml` | the owner | MPL-2.0 |

**The install page beside the GPL site.** `site/install/` is fm1-base's MPL-2.0 installer (`web/installer.html`,
`fm1ota.js`, `fm1pkg.js`) adapted for FIZZ. Its files stay MPL-2.0 with their copyright lines; `fm1lib.js` is
generated from the submodule (`firmware/tools/sync_installer.py`) and is fm1-base's code unchanged. MPL-2.0 is
file-level and GPL-compatible (MPL §3.3), so these files sit in the GPL-3.0-only `site/` as separate files of their own
licence: the site's pages link to the install page, the install page uses the site's stylesheet (`css/install.css`,
GPL-3.0-only like the rest of `css/`), its font and FIZZ's SysEx helper (`js/fizz_sysex.js`, MPL-2.0). Nothing GPL is
copied into the MPL files.

## Third parties in the firmware

| What | Licence | Where |
|---|---|---|
| plutovg v1.3.3 (Samuel Ugochukwu), the 2D drawing | MIT | `third_party/plutovg/` (`LICENSE`) |
| FreeType's rasterizer, stroker, math and types inside plutovg (`plutovg-ft-*`) | FreeType License (FTL) | `third_party/plutovg/source/FTL.TXT`. The binary credit: "Portions of this software are copyright © 2006 The FreeType Project (www.freetype.org). All rights reserved." FIZZ's changes to these files are listed in `third_party/README.md` |
| stb_truetype, stb_image, stb_image_write (Sean Barrett), inside plutovg | MIT OR Unlicense | `third_party/plutovg/source/plutovg-stb-*.h` |
| Inter Tight (The Inter Project Authors), the type | SIL OFL 1.1 (no Reserved Font Name) | `assets/fonts/` (`OFL.txt`); embedded in `core/gfx/fonts_gen.h` |

## Credits (designs and facts, no code)

- **SLOOP 2.6** (isod89, github.com/isod89/sloop-fm1): the designs of the Clean chain's one-block look-ahead
  limiter and of the stereo chorus's two taps, both written anew in ChoralRoot.
- **OMX-27** (okyeron/OMX-27, its Form sequencer and MIDI FX; contributors including j__r0d): the MIDI FX groups'
  mechanics, re-implemented from a behaviour description.
- **namesizer** (the owner's, MIT) and its phonic-nonsense adaptation (Euan Goddard, MIT): the name ideas and the
  phonics grapheme lists in `core/seq/names.c`.
- **The TAP ape** (`core/ui/ape_poses.h`): traced from the owner's own artwork.
- **Elektron's Digitone**: FM TONE's design and measured envelope times.
- **Felucca** (Leo Kuroshita, Hügelton Instruments): the firmware ChoralRoot began from. No Felucca code remains in
  FIZZ (L1).
- **Melodee** (Kerem Kilic): two saturation limits in the voice mix, through ChoralRoot.
