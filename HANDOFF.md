# FIZZ handoff (2026-10-25, after 0.12.0-beta.4)

This is for a new Claude session picking up FIZZ. Read this first, then:
- `CHANGELOG.md`;
- `docs/TRACK-TOOLS-PLAN.md` §10 (TOOLS, with the owner's revisions);
- `docs/WEB-MANAGER-PLAN.md` and `docs/MANAGER-P2-PLAN.md` (the web Manager);
- `docs/SYSEX.md`;
- `firmware/BUDGET.md` and `firmware/DUALCORE.md`;
- the memory files (`fmtoneseq-quadrille-plan.md`, `choralroot-fm1-device-build.md` and the FIZZ rules).

## Where things stand

- **Released: FIZZ 0.12.0-beta.4.** It's tag `v0.12.0-beta.4` on master ea351a1, pushed.
  - It's a GitHub pre-release, and it's on https://quixotic7.github.io/FIZZ/install/.
  - The Manager is live at https://quixotic7.github.io/FIZZ/manager/.
  - beta.1 to beta.3 came out on 2026-10-24 and 2026-10-25.
- **The owner's FM-1** runs exactly the beta.4 release package. It booted with 0 xruns.
- **What is in FIZZ now:**
  - **6 tracks.** Changed from 8 to free memory. Old 8-track projects started fresh; MY PRESETS were kept.
  - **TOOLS**, opened by holding ARP:
    - B1–B8 are ROTATE, REVERSE, RANDOM, EUCLID, GRIDS, HUMANIZE, QUANTIZE and LENGTH;
    - PRESETS also reaches STRETCH and CLIP (copy, paste and clear);
    - B11 and ALGORITHM set the scope (SELECTED, PAGE, TRACK or PATTERN);
    - SELECT changes the page, KNOB 4 is each tool's 4th value, OCT+ keeps a change, OCT− reverts it, HOME leaves;
    - changes play live, with ghosts of the old steps and LEDs showing the result.

    The owner tested TOOLS on the unit and is happy with it.
  - **Record modes** (SHIFT + REC, KNOB 3):
    - CAPTURE is the default. It arms while stopped; the first note clears the track to page 1 with lengths 16 (the
      rate is kept) and starts recording; PLAY or REC ends the take and sets its length.
    - REPLACE and OVERDUB record into the existing sequence.
  - **Other changes:**
    - CLEAR TRACK is a full reset: one page, all lengths 16.
    - The live keyboards start on F, matching the keybed.
    - Turning SELECT or ALGORITHM while holding PLAY (tempo and swing) shows the tempo Ape.
    - The F1 page double-click works again.
    - The playhead moves in half steps, with no arrow bounce.
    - The sound editor follows the LFOs while playing.
    - The sound-browser freezes are fixed.
  - **The FIZZ Manager** (`site/manager/`, Web MIDI SysEx, Chrome, Edge or Brave):
    - **BACKUP:** whole-store backup into the browser, with download and restore.
    - **STATUS:** project and preset counts.
    - **LIBRARY:**
      - one list with filters, tags, rename, delete and reorder;
      - + POOL / − POOL with a ring and a badge;
      - EDIT, LOAD ON FM-1 (opens the unit's own browser; OCT− cancels, OCT+ loads) and EXPORT.
    - **POOL:** place, swap, clear, sort and load a project.
    - **IMPORT:** Digitone `.dnsnd`, `.dn2pst` and `.dn2snd`, plus `.fizzlib`, `.fizzsnd` and `.fizzbak` presets.
    - **EDIT:**
      - all 110 values on one page;
      - follows the FM-1's selected track;
      - the TRACK switch in the header turns live editing on; with it off the Manager never changes tracks.
    - **EXPORT:** `.fizzsnd`, `.dnsnd` and `.dn2pst`/`.dn2snd`. The Digitone formats are marked experimental.
- **SysEx speed:** a whole 292 KB backup takes about 10 s. During a transfer the unit draws a frame a second, and
  FZBR requests are served between screen bands. Stats line 16 holds the `xfer_*` counters.

## Memory (the beta.4 build) — tight

| | dual (released) | single-core |
|---|---|---|
| main RAM (.data+.bss) free | 1,308 B | 700 B |
| pool free (fm1-base needs 8,192) | ~22.4 KB | ~12.2 KB |
| app-slot flash free (of 581,564) | **~9.2 KB** | **~5.4 KB** |

- **Flash is the binding limit.** Any new feature must find room first: trims, or dead code removed.
- **New state** goes in `qd_app_t`, `qd_sx_t` or `qd_sx_work_t` (the pool), never in file statics in main RAM.
- **The ratchet** is `tools/budget/ratchet.txt`. Raise it only with a reason in the commit. After a merge, set it with
  `cd tools && python3 budget/budget.py --ratchet-set`.

## Open items, in order

1. **The HOME screen flashing for a frame** while in other menus. The owner reported it; it isn't fixed and wasn't
   reproduced on the host.
   - Stats line 16's `blips=` counter, which catches FIZZ switching screens, read 0. So the flash is probably in the
     screen-send path: a stale partial frame or rectangle reaching the panel.
   - Ask the owner when it happens (which menu, playing or not, knob or idle), and for a video. Then add a check on
     what is actually sent to the panel.
2. **The owner's feedback after beta.4** was about to be given when this was written. Ask for it.
3. **Two xruns at boot** were seen once after a flash (`xruns_boot=2`), not seen at beta.4's boot. Watch for it.
4. **Web Manager, next phases** (`docs/MANAGER-P2-PLAN.md`):
   - P2-C: the projects tab, a project's patterns, and copying or moving a pattern between projects.
   - P2-D: the project library in the browser, with unload and load and the sounds re-linked.

   The owner decided: on unload, ask each time whether to remove MY presets only that project uses; on a conflict,
   ask and show both versions. Flash estimates: C about 1.6–3.2 KB, D about 3–5.8 KB, so there is not room for both
   without trims.
5. **Screen mirror** (`docs/WEB-MIRROR-PLAN.md`, on branch `web-mirror-plan`, not merged).
   - The recommendation is FIZZ's own mirror: about 2–2.5 KB of flash, no main RAM.
   - The owner said no redesign: reuse the current web player, including its screen-only view, exactly as it looks.
   - Not approved to build yet.
6. **Earlier list, still open:**
   - Scope is the slowest screen (~93 ms).
   - The single-core package sends whole rows.
   - FM1VST core-quadrille is on an old core (0.10.4) and needs updating before the owner prototypes there again.
   - The L1 provenance questions were never asked (OMX-27 credit, Melodee's saturation numbers, the send-guard fix).
   - W7/W8 are dark. There are five options for them; the owner hasn't chosen.
7. **Known gaps:**
   - A CAPTURE take started while synced to external MIDI clock restarts on FIZZ's own clock.
   - The simulator scripts `boot.txt` and `views.txt` fail `fz_stats_worst <1024` (1108): a long stats line 4.
     They were already failing.

## Repos and layout

- **FIZZ repo:** `ChoralRootFM1/FMToneSeq` = github.com/Quixotic7/FIZZ, MPL-2.0. `site/` stays GPL.
  - `core/` is the C core.
  - `firmware/app/` is the port, on the submodule `firmware/fm1-base` (Charles Vestal's).
  - `site/` is the player, the install page and `manager/`.
- **Untracked:** `research/fm1-resources.html` isn't the agents' file. Move it aside (`mv`) for `release.py`, which
  needs a clean tree, then put it back.
- **Old branches and worktrees** (`.claude/worktrees/`, `scratchpad/rel*`) are all merged except `web-mirror-plan`.
  They can be pruned.

## How to build, test, release, flash

- **Tests:**
  - `make -C tools test`, `test-pace` and `budget-gate`;
  - the site tests: `node site/tools/test_site.mjs`, `test_engine`, `test_manager`, `test_manager_page` and
    `test_install`. The page tests serve `site/` themselves with the cache off.
  - After a core change, rebuild the site's wasm with `sh site/build_wasm.sh` (emsdk is in ~/emsdk) and commit
    `site/fizz.wasm` and `site/fizz.json`.
- **Firmware builds:** `python3 firmware/build.py` (dual), `--single-core` and `--host`. They use the JieLi toolchain in
  Docker Desktop; start it with `open -g -a Docker`.
- **Release:**
  1. Write `## Unreleased` in CHANGELOG with a one-line intro.
  2. Commit, and move `research/fm1-resources.html` aside.
  3. Run `python3 firmware/release.py VERSION`, then `node site/tools/test_install.mjs`.
  4. Commit and tag as it prints.
  5. Push with `-c http.version=HTTP/1.1 -c http.postBuffer=524288000`.
  6. Watch the runs with `gh run watch`.

  The owner approves each release.
- **Flashing** (the main session only, never agents; standing permission on the owner's unit):
  1. Run the CoreMIDI rescan:
     `python3 -c "import ctypes, ctypes.util; ctypes.CDLL(ctypes.util.find_library('CoreMIDI')).MIDIRestart()"`.
     The Mac's MIDI port drops often; the unit is fine.
  2. Back up the store as its own command, and check that it printed its CRC line:
     `python3 firmware/tools/fizz_xdata.py backup ../FIZZ-saves/DATE-before-X-store.bin`.
     Never pipe it into the flash: a pipe once hid a failed backup.
  3. Run `fm1_install.py --info`, then `fm1_install.py PACKAGE --yes`.
  4. Check `fizz_xdata.py stats`: up, xruns, store_wiped.
- **Stats:** `fizz_xdata.py stats [--reset]`.
  - Line 16 holds the transfer counters and the blip counter.
  - Remote presses: `firmware/fm1-base/tools/fm1_remote.py tap PLAY wait 800`.
- **Local Manager for the owner:** `python3 -m http.server 8767 --bind 127.0.0.1 -d site`. The app stops preview
  servers after a while. Brave may cache old modules; a hard reload fixes it.

## How the owner works (check the memory files)

- **Code** is written by Opus agents. The main session designs, briefs, merges, verifies and flashes.
- **Running agents in parallel:** give each one its own worktree, and merge into master afterwards.
- **New screens:** mock-up first, then approval, then build. Don't redesign things the owner likes; the web player's
  look stays.
- **Clarity rules:** no tutorial text, nothing under 12 px, and text only for values and brief popups.
- **Gestures:** single clicks are preferred.
- **Never:**
  - commit Elektron or owner preset files (read them in place only);
  - use `rm -rf` or `git stash`;
  - open GUIs in the foreground.
