# FIZZ

FIZZ is a step sequencer for FM TONE, ChoralRoot's FM synth voice, made for the M-VAVE FM-1. It has eight tracks
sharing 16 voices, track defaults with per-step overrides, p-locks, a pool of 128 sounds, pages with their own start
and end, conditions, chance, ratchets and chords of up to six notes.

**Phase 1** (now) runs on the Mac as an [FM1VST](https://github.com/Quixotic7/FM1VST) core. The real FM-1 is its panel
through RemoteFM1: the keys, the knobs and the screen are the unit's, and the sound comes from the Mac. Phase 2 will be
the FM-1 firmware itself.

- **[Play the preview in the browser](https://quixotic7.github.io/FIZZ/)** (the on-screen FM-1, or your own FM-1 with
  RemoteFM1), and send feedback from there. The site is [site/](site/README.md).
- **[PLAN.md](PLAN.md)**: the design, the controls (§8b) and the build log (§13).
- **[PLAYING.md](PLAYING.md)**: how to play it, organised by "how do I ...".

## Build and test

The core is plain C99 with no OS calls (simulated time, deterministic). Its tests and the headless screen renders need
only a C compiler:

```sh
make -C tools test      # the sequencer, the voices, the controller (ASan + UBSan) and bit-exact checks against ChoralRoot
make -C tools frames    # every screen as a PNG in core/tests/out/ (and the idle screen as a GIF, with ffmpeg)
make -C tools audio     # the demo patterns as WAV files
```

The bit-exact tests read ChoralRoot's tagged source from a checkout next to this one (`CR_REPO`, default
`../ChoralRootFM1`).

To build the FM1VST core module, configure an FM1VST checkout with this tree as `FIZZ_DIR`, building only this core:

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
