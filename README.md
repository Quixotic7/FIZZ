# FIZZ

FIZZ is a groovebox firmware for the M-VAVE FM-1, built around one engine: FM TONE, ChoralRoot's FM synth voice. It
runs on the FM-1 itself, and the same code plays in the browser.

- **Six tracks** sharing 16 voices, each an FM TONE sound or a MIDI track; eight pages of 16 steps a track, each page
  with its own start and end.
- **Steps**: track defaults with per-step values, chords of up to six notes, conditions, chance, ratchets, micro-timing,
  slides, step functions (jumps, leaps, random) and p-locks of every sound value, the mix and the sends.
- **Sounds**: factory presets, 256 of your own (MY PRESETS), a pool of 128 per project, sound locks per step, and a
  full sound editor.
- **MIDI FX** (the arpeggiator and friends) and **TOOLS** (rotate, reverse, random, Euclid, Grids, humanize, quantize,
  stretch, clip) for making patterns and reworking the ones you have.
- **A mixer and effects**: chorus, delay and reverb with sends per track.
- **Eight projects** of 64 patterns each, with copy, cut and paste of steps, pages and patterns, and eight levels of
  undo.
- **Live**: a keyboard page with step and live recording, MIDI in and out, clock, USB audio, and Bluetooth MIDI in the
  Bluetooth package.

## Get it

- **[Install FIZZ on an FM-1](https://quixotic7.github.io/FIZZ/install/)** from the browser, over USB. The
  [releases](https://github.com/Quixotic7/FIZZ/releases) hold the packages (USB and Bluetooth) and
  [CHANGELOG.md](CHANGELOG.md) what changed.
- **[Play it in the browser](https://quixotic7.github.io/FIZZ/)**: the on-screen FM-1, or your own FM-1 as the panel
  through RemoteFM1.
- **[The Manager](https://quixotic7.github.io/FIZZ/manager/)**: presets, projects and backups for a unit, over USB.
  A native iOS Manager is in [ios/](ios/).

## Read

- **[PLAYING.md](PLAYING.md)**: how to play it, organised by "how do I ...".
- **[BUILDING.md](BUILDING.md)**: building the firmware packages and how a release is made.
- **[PLAN.md](PLAN.md)**: the design and the build log. **[docs/SYSEX.md](docs/SYSEX.md)**: the SysEx the Manager
  speaks. The site is [site/](site/README.md).

## Build and test

The core (`core/`) is plain C99 with no OS calls (simulated time, deterministic); the FM-1 port is `firmware/app/`, on
[fm1-base](firmware/fm1-base). The core's tests and the headless screen renders need only a C compiler:

```sh
make -C tools test      # the sequencer, the voices, the controller (ASan + UBSan) and bit-exact checks against ChoralRoot
make -C tools frames    # every screen as a PNG in core/tests/out/ (and the idle screen as a GIF, with ffmpeg)
make -C tools audio     # the demo patterns as WAV files
```

The bit-exact tests read ChoralRoot's tagged source from a checkout next to this one (`CR_REPO`, default
`../ChoralRootFM1`).

The firmware packages are built with `python3 firmware/build.py` (see [BUILDING.md](BUILDING.md)); the browser FIZZ
with `sh site/build_wasm.sh`.

FIZZ also runs on the Mac as an [FM1VST](https://github.com/Quixotic7/FM1VST) core. To build that module, configure an FM1VST checkout with this tree as `FIZZ_DIR`, building only this core:

```sh
cmake -S path/to/FM1VST -B build-fizz -DFM1_ONLY_CORES=fizz -DFIZZ_DIR=$PWD
cmake --build build-fizz --target fizz_core core_smoke core_conformance
ctest --test-dir build-fizz -R 'fizz|common'
make -C tools abi FM1CORE=path/to/build-fizz/cores/fizz.fm1core   # the module driven through the core ABI
```

## License

MPL-2.0 (see [LICENSE](LICENSE) and [LICENSING.md](LICENSING.md); `site/` and the bit-exact tests' oracles stay
GPL-3.0-only). Until L1 (2026-10-23) FIZZ was GPL-3.0-only; Felucca's code was rewritten, not relicensed
([research/PROVENANCE.md](research/PROVENANCE.md)).

## Credits

- FM TONE, the effects (chorus, delay, reverb) and the output chain: ChoralRoot FM-1 v0.25.1, the owner's (relicensed
  MPL-2.0 by him); ChoralRoot began as a fork of Felucca (Leo Kuroshita, Hügelton Instruments), whose code FIZZ no
  longer holds (L1: rewritten clean-room).
- Drawn with [plutovg](https://github.com/sammycage/plutovg) (MIT; its FreeType parts FTL, stb MIT OR Unlicense),
  vendored in `third_party/plutovg` with FIZZ's changes listed in `third_party/README.md`.
- Type: [Inter Tight](https://github.com/rsms/inter) (SIL Open Font License 1.1).
- The sequencer's controls owe a lot to the OMX-27 Form sequencer and to Elektron's machines.
