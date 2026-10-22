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
| `remote/`, `js/use-fm1.js`, `js/fm1-link.js`, `js/remote-hook.js` | Connect FM-1 (WebUSB / Web MIDI, RemoteFM1) |
| `js/digitone.js`, `js/digitone-ui.js`, `js/fm1dnsnd.js` | Digitone `.dnsnd` sounds read in the browser (ChoralRoot's reader) and sent to FIZZ |
| `js/feedback.js`, `../.github/ISSUE_TEMPLATE/feedback.yml` | the Feedback button's prefilled issue |
| `tools/test_engine.mjs`, `tools/test_site.mjs` | the checks (Node; headless Chromium over the DevTools protocol) |

## Credits and licences

FIZZ is GPL-3.0-only. The player, its engine, the AudioWorklet host, the saves and Connect FM-1 are copied from
[RemoteFM1](https://github.com/Quixotic7/RemoteFM1)'s site (GPL-3.0-only; libremotefm1 compiled with Emscripten, MIT);
the panel and the settings sheet follow [FM1VST](https://github.com/Quixotic7/FM1VST)'s design (GPL-3.0). The Digitone
reader is [ChoralRoot FM-1](https://github.com/Quixotic7/ChoralRootFM1)'s (GPL-3.0-only). FM TONE and the effects are
ChoralRoot's, a fork of Felucca (Leo Kuroshita, Hügelton Instruments). Drawn with plutovg (MIT). Inter Tight, SIL Open
Font License 1.1 (`fonts/OFL.txt`).
