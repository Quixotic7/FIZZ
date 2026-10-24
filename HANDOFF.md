# FIZZ handoff (2026-10-24, after 0.11.0-beta.2)

For a new Claude session picking up FIZZ. Read this, then `PLAN.md` (§13 "Phase 2" and the Phase 1 log),
`firmware/DUALCORE.md`, `firmware/BUDGET.md`, and the memory file `fmtoneseq-quadrille-plan.md`.

## Where things stand

- **Released: FIZZ 0.11.0-beta.2** (tag `v0.11.0-beta.2`, master a4640e1, pushed). It's on the install page
  https://quixotic7.github.io/FIZZ/install/ and is a GitHub pre-release. Notes are in `CHANGELOG.md`.
  beta.1 was released earlier the same day.
- **The owner's FM-1** runs the beta.2 candidate build (3859734, dual core, SHA-256 3851bda9…). The released package
  is the same code built by `firmware/release.py` (image 537,128 B, identity FM-1_46001100).
- **The owner's verdict on beta.2:** "This is a lot better." The LEDs were already confirmed fixed (P2.7).

### What beta.2 measured on the unit (demo playing, dual core, 8 voices)

| Screen | ms a frame |
|---|---|
| Piano roll | 60 |
| Steps | 30 |
| Edit | 35 |
| Meters | 65 |
| Numbers | 34 |
| Voices | 39 |
| Scope | 93 |

- **Tearing:** 0–4 torn frames in 10 s; Scope 0.
- **Audio:** no xruns, isr_avg ~2.85 ms per 5.8 ms half, cpu_isr ~48 %. Core 1 takes one voice more than half.
- **Voice engine:** `quad_render` alone runs 185 µs per 128 frames, against ChoralRoot's 176. The clock is the stock
  360 MHz. The rails stay at M-VAVE's stock 1.60 V / 1.38 V (the owner's choice; see PLAN.md).

## Repos and layout

- **FIZZ repo:** `ChoralRootFM1/FMToneSeq` = github.com/Quixotic7/FIZZ, MPL-2.0. `site/` stays GPL.
  - The core is in `core/`.
  - The firmware port is `firmware/app/`, on the submodule `firmware/fm1-base` (Charles Vestal's, MPL-2.0).
- **Untracked:** `research/fm1-resources.html` isn't the agents' file. It was moved aside during the release and put
  back; `firmware/release.py` refuses an unclean tree.
- **FM1VST** worktree `FM1VST/.claude/worktrees/quadrille`, branch `core-quadrille`, is the Phase 1 Standalone with
  RemoteFM1.
  - Its FIZZ core is old (0.10.4-p24), and one commit is unpushed.
  - Bring the core up to date there before the owner prototypes in the Standalone again.
  - Pushes to core-quadrille are approved; FM1VST main is never touched.

## How to build, flash, measure

- **Builds and tests:**
  - core tests: `make -C tools test`, `make -C tools test-pace`, `make -C tools budget-gate`;
  - firmware: `python3 firmware/build.py` (dual) or `--single-core`, `--host` for simulator tests, `--define NAME=VALUE`
    for options.
  - Builds run the JieLi toolchain in **Docker Desktop**. Start it with `open -g -a Docker`. It may show an admin
    prompt; the owner clicked "Don't Allow" and it still works.
- **Release:** write `## Unreleased` in `CHANGELOG.md`, commit, then on a clean tree:
  `python3 firmware/release.py VERSION --dry-run`, then without `--dry-run`, then `node site/tools/test_install.mjs`.
  Run the printed git commands. Push with `-c http.version=HTTP/1.1 -c http.postBuffer=524288000`. `gh` works for
  run status.
- **Flashing (only the main session, never agents):**
  - Quit any FM1VST Standalone first.
  - Run `python3 firmware/fm1-base/tools/fm1_install.py --info`, then `fm1_install.py PACKAGE --yes`.
  - If "the update loader did not appear": run the CoreMIDI rescan
    `python3 -c "import ctypes, ctypes.util; ctypes.CDLL(ctypes.util.find_library('CoreMIDI')).MIDIRestart()"`
    and run the install again; it finishes the write.
  - Standing permission covers FIZZ builds on the owner's unit. Back up first: copy the Standalone's
    `~/Library/Application Support/fm1emu/fizz/flash.bin` to `FIZZ-saves/DATE-before-X-flash.bin`.
- **Stats:**
  - Read with `python3 firmware/tools/fizz_xdata.py stats [--reset]`. Line 15 holds a frame's phases and the
    rectangle stats; line 11 the screen send and torn counts; line 5 the xrun records; line 8 the boot bench and
    clock.
  - Remote presses: `firmware/fm1-base/tools/fm1_remote.py tap PLAY wait 800`.
  - Next view = `press GLO wait 100 tap HOME wait 100 release GLO`.
  - The scratchpad scripts `view_tour.sh` and `phase_tour.sh` don't survive the session; rewrite them from these
    commands.
  - **zsh gotcha:** `X="python3 path"; $X` doesn't word-split in zsh. Write `python3 $T` with `T` = the path.

## Open items, in order

1. **The owner's pacing choices** (asked, not answered). Each is one `--define`:
   - `QD_PACE_PLAYHEAD=2`: the playhead moves in half steps;
   - `QD_IDLE_FPS=10`: idle bubbles slower;
   - `QD_EDIT_ANIM_FPS=15`;
   - `QD_SCOPE_FPS=15`;
   - `QD_GEEK_STEADY_DOT=1`.
   The recommendation was half steps, idle 10 fps, and a steady Geek Out dot. If chosen, make them the defaults in
   `core/qd_config.h` (the FIZZ agent's suggestion), re-render, build, flash and measure.
2. **Scope** is the slowest screen (~93 ms). Its waveform changes wide and tall every frame, so it goes in two parts.
   Options: narrower x-segments (needs memory; dual has ~1.2 KB RAM and ~368 B pool margin left), or a lower scope
   frame rate.
3. **The single-core package** (`fizz-1core.fwsc`) still sends whole rows, so ROLL goes in two parts and waits
   ~16 ms. Consider `FZ_FB_PARTS=0` there or dirty rectangles if memory allows. The released build is the dual one.
4. **Memory is nearly full:**
   - dual: RAM ~1.2 KB free, NOINIT 104 B, RAM code region 124 B, pool ~8.5 KB with fm1-base needing 8,192;
   - 1-core: RAM ~0.7 KB.
   Any new feature must find room first.
5. **Phase 1 upkeep:** update FM1VST core-quadrille and the website's wasm (`site/`) to the current core; the site
   still shows the older core.
6. **Owner questions never asked (L1):** the OMX-27 credit, Melodee's two saturation numbers, the send-guard overflow
   fix. See `research/PROVENANCE.md` and `research/L1-CLEANROOM-SPEC.md`.
7. **Not planned:** asynchronous core 1. It doesn't fit; the reasoning is in `firmware/DUALCORE.md` "P2.16".
   Bluetooth comes later; USB only for now.

## How the owner works (from memory files; check them)

- **Code** is written by Opus agents; the main session designs, briefs, verifies and flashes.
- **New screens:** mock-up first, then approval, then build (`mockups/`, rendered by `mockups/render_png.py`).
- **Clarity rules:** no tutorial text, nothing under 12 px, text only for values.
- **Gestures:** single clicks over holds.
- **GPL references** (Felucca, JIANT, Melodee's `fm1_power.h`) only through clean-room specs.
- **Never:** commit Elektron or the owner's preset files, `rm -rf`, `git stash`, or launch GUIs in the foreground
  (`open -g`).
- **Before flashing:** the unit must start quiet and MASTER must work. The owner keeps MASTER low on first tries.
