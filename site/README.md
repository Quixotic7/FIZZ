# FIZZ's preview site

A public page where anyone can play FIZZ in the browser, with the on-screen FM-1 or with their own FM-1 running
RemoteFM1, and send feedback. GitHub Pages publishes this folder (`.github/workflows/pages.yml`, on a push to master
that touches `site/`); the workflow compiles nothing, so the wasm is built here and committed.

## After FIZZ changes: rebuild and refresh

```sh
site/build_wasm.sh && node site/tools/test_engine.mjs     # fizz.wasm + fizz.json from this checkout, then checked
git add site/fizz.wasm site/fizz.json && git commit -m "site: FIZZ <version>"   # the push to master deploys it
```

`build_wasm.sh` needs Emscripten (`~/emsdk`, sourced by itself) and an FM1VST tree with `core-glue/fizz`
(`FM1VST_DIR`, default the `quadrille` worktree). It builds FIZZ from this checkout (`FIZZ_DIR`); the version comes
from the fizz glue's `VERSION` line, so bump it there when FIZZ's version changes. `fizz.json` records the version,
the FIZZ and FM1VST commits and whether the FIZZ tree was dirty.

Optional, before a push:

```sh
python3 -m http.server 8765 --bind 127.0.0.1 -d site &    # then open http://127.0.0.1:8765/
node site/tools/test_site.mjs                             # headless Chromium: boots, sound, flash, feedback, guide
```

**Caching.** GitHub Pages caches for about 10 minutes. The workflow publishes `site/` through
`node site/tools/assemble.mjs _site`, which versions every asset URL (the pages' scripts and styles, every module
import, the worklet, the engine, Connect FM-1's modules) with the site's build, a hash of all its files, and writes it
into the published `fizz.json`. The page reads `fizz.json` uncached on load and when the tab comes back, and offers
"A new FIZZ is out: Reload" when it is newer than itself. A new FIZZ version starts from the previous version's
projects (copied: the old save is never changed), and says so if it cannot read them. `node site/tools/test_update.mjs`
checks all of this in headless Chromium against a server that caches as Pages does.

`site/build_remote.sh` refreshes Connect FM-1 (`remote/`, RemoteFM1's web back end) from RemoteFM1's `origin/main`;
it is needed only when RemoteFM1's protocol changes.

## What is here

| Path | What |
|---|---|
| `index.html`, `js/main.js` | the player: RemoteFM1's FM-1 Player cut to one firmware, FIZZ |
| `guide.html`, `js/keymap.js`, `css/guide.css` | the newcomer's guide and the one-screen key map |
| `fizz.wasm`, `fizz.json` | FIZZ compiled to WebAssembly (-Os), and its build information |
| `engine/` | RemoteFM1's native engine: `engine.js` (the runtime, published), `wasm_glue.c`, `coro_wasm.c` and `CMakeLists.txt` (the build) |
| `worklet/host.js`, `js/audio.js` | the AudioWorklet host and the page's side of it |
| `js/saves.js` | FIZZ's flash (its projects) kept in IndexedDB; named saves, export and import |
| `js/rec-ui.js`, `js/recorder.js` | the recorder (RemoteFM1's): the record button, Shift+R, Takes, WAV files |
| `remote/`, `js/use-fm1.js`, `js/fm1-link.js`, `js/remote-hook.js` | Connect FM-1 (WebUSB / Web MIDI, RemoteFM1) |
| `js/digitone.js`, `js/digitone-ui.js`, `js/fm1dnsnd.js` | Digitone `.dnsnd` sounds read in the browser (ChoralRoot's reader) and sent to FIZZ over its SysEx (`js/fizz_sysex.js`, FIZZ's `tools/fizz_sysex.mjs` copied) |
| `js/feedback.js` | the Feedback button: FIZZ's Discord (https://discord.gg/emf6DmZ9V) |
| `tools/assemble.mjs`, `js/build.js` | the published tree with every asset URL versioned (the workflow runs it) |
| `tools/test_engine.mjs`, `tools/test_site.mjs`, `tools/test_update.mjs` | the checks (Node; headless Chromium over the DevTools protocol; a new deploy over a cached old one) |
| `install/`, `css/install.css` | the install page (`/install/`, MPL-2.0: fm1-base's web installer for FIZZ): the firmware chosen from `firmware/releases.json`, then FIZZ's data written (`install/fizz_data.js`), going back to the stock firmware, the crash report. `install/fm1lib.js` is generated (`firmware/tools/sync_installer.py`) |
| `firmware/` | the released packages, `firmware/VERSION/fizz.fwsc` and `fizz-xdata.bin` (the dual-core build) with `SHA256SUMS`, and `releases.json` (newest first): written by `firmware/release.py`, never by hand. Served from Pages because GitHub's release files can't be fetched by a page (no CORS) |
| `tools/test_install.mjs` | the install page's checks: `releases.json` and its files, FIZZ's data writer against a simulated FM-1, `fm1lib.js` against fm1-base |

## A firmware release (the install page)

```sh
# CHANGELOG.md: write the "## Unreleased" notes for testers, commit, then on a clean tree:
python3 firmware/release.py 0.11.0-beta.1 --dry-run     # builds and stages into firmware/app/build/release-dry-run/
python3 firmware/release.py 0.11.0-beta.1               # the same into site/firmware/ and CHANGELOG.md
node site/tools/test_install.mjs                        # then the git commands it prints: commit, tag, push
```

Pushing master deploys the page with the new version; pushing the tag `v0.11.0-beta.1` makes the GitHub
pre-release (`.github/workflows/release.yml`).

## Credits and licences

FIZZ is GPL-3.0-only. The player, its engine, the AudioWorklet host, the saves and Connect FM-1 are copied from
[RemoteFM1](https://github.com/Quixotic7/RemoteFM1)'s site (GPL-3.0-only; libremotefm1 compiled with Emscripten, MIT);
the panel and the settings sheet follow [FM1VST](https://github.com/Quixotic7/FM1VST)'s design (GPL-3.0). The Digitone
reader is [ChoralRoot FM-1](https://github.com/Quixotic7/ChoralRootFM1)'s (GPL-3.0-only). FM TONE and the effects are
ChoralRoot's, a fork of Felucca (Leo Kuroshita, Hügelton Instruments). Drawn with plutovg (MIT). Inter Tight, SIL Open
Font License 1.1 (`fonts/OFL.txt`).
