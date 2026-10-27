# FIZZ handoff (2026-10-27, after 0.12.0-beta.9)

This is for a new Claude session picking up FIZZ. Read this first, then:
- `CHANGELOG.md`;
- `docs/TRACK-TOOLS-PLAN.md` §10 (TOOLS, with the owner's revisions);
- `docs/WEB-MANAGER-PLAN.md` and `docs/MANAGER-P2-PLAN.md` (the web Manager);
- `docs/SYSEX.md`;
- `firmware/BUDGET.md` and `firmware/DUALCORE.md`;
- the memory files (`fmtoneseq-quadrille-plan.md`, `choralroot-fm1-device-build.md` and the FIZZ rules).

## Where things stand

- **2026-10-27, after beta.9, on master and the unit (unreleased):** Geek Out's fifth page **TRACKS** (QD_VIEW_TRACKS,
  the owner's spec: six equal rows filling the screen, no header, each track's voices before the FX in its colour, a
  silent track one flat line). The mixer's `vtap` copies each voice's block after DRIVE (mix.c, RAM code: 72 B left);
  app.c `tsc_take` sums them per track every 8th sample into `qd_geek_t.u.tk`, which shares SCOPE's room (a union: the
  device's NOINIT had no 1.7 KB; the size check is device only, a desktop pool of 64 grows it); the view's points are
  int8 (`ui_geek_t.w.tsc`). Also the last ~7 ms at a pattern change: the clean slot is chosen outside the lock
  (pattern.c stg_choose / stg_chosen), the sounds a pattern needs are staged (persist.c sst_*), and the `lkp_*` stats
  time the locked ms's parts (lock_clock ~0.3 ms on the unit). FM1VST's quadrille worktree builds the standalone with
  this tree (`cmake --build build-quadrille --target FM1VST_Standalone`; its core.cmake VERSION follows FIZZ's tag).

- **Released: FIZZ 0.12.0-beta.9** (2026-10-27, tag `v0.12.0-beta.9` on master 04f84ba, pushed; the GitHub pre-release and
  the install page both green). CHANGELOG `## 0.12.0-beta.9` has the testers' notes. Of note for the code:
  - **SENDS is the owner's cat**, **MASTER the two heads**: art/cat-sends.svg and art/filter-head.svg ->
    tools/gen_cat.py -> core/ui/ui_cat.h (1/8 px curves); ui_tracks.c cat_draw / head_draw. One glow layer only (two
    made a SENDS frame 70 ms on the unit; 51 now, MASTER 19). The sends shown follow the heard step
    (ctl_mix.c qd_fx_sends_follows / qd_fx_send_shown). Mock-ups drawn over the simulator's real screens
    (mockups/ref/*-real-N.png, their scripts beside: the layout is the firmware's own).
  - **The VOICE page** (B1: TRIG KEYS PORT VOICE MOD): five sound values 110..114 (quad_voice.h QD_QP_VMODE ..; the
    blob's last four bytes, zeros = POLY, OFF, DETUNE 20, SPREAD 64, MID; not lockable: qd_quad_lockable). The pool
    (pool.c qd_pool_note_on_at) starts UNISON n real voices (the track's LIMIT keeps the middle ones), a detune factor
    each (q.udet, a block's increment) and a pan around the track's (p->pan); a unison note's voices follow its first
    (sib_next: writes, mix, legato, glides). MONO caps the track; PRIORITY: the cross-track steal robs the lowest, never
    a higher one (all equal: as before). **The mix lock ids moved to 115..**: pattern records 'P' 6; a 'P' 3..5's ids
    >= 110 move up 5 when read (persist.c; test_persist voice_ids_test).
  - **Patterns are read outside the audio lock** (the owner's pop at a pattern change: an 18.6 ms read under the lock).
    pattern.c: with `stg_on` (the app), qd_proj_fetch only stages; the free part (app.c ms_free -> qd_proj_fetch_work)
    chooses the slot (its clean check hashes the pattern: ~7 ms), the next fetch gives it up, the free part reads it in,
    the next fetch maps it. Stopped, a pattern chosen not in RAM waits in ui.pat_wait. Pool sounds the next steps lock
    are read the same way (persist.c per.sst, qd_persist_snd_work). On the unit: a press 0.07 ms, the locked ms 0.3 ms
    at most (stats lkp_*: the locked ms's parts, app.c qd_lkp_max). A pattern start ends the MIDI FX groups' sequencer
    notes (player.c qd_player_pattern_ms, from the locked ms: **the RAM code region has 104 B**; qd_player_block lives
    there, keep code out of it).
  - The live keys' HOLD on LFO (ui.khold, wherever qd_keys_on), the editor's groups keep their page (ui.snd_grp_pg), FX
    keeps its page and PRESETS works there, the browser goes back to SND / FX / MIX with the keys as they were, the
    engine 6 dB lower with SETTINGS' BOOST (app.c chain_shares; the 'G' record's b[5] top bit).
  - The owner wants to move fast: flash first so they can try, run the full suite after (memory fizz-flash-first).
  - A second session works on the web Manager (projects tab, full-screen pages, the Library's own scroll and pool
    filter) and on loading a backup into the web emulator, in the same folder (`site/manager`, `site/`).

- **Released: FIZZ 0.12.0-beta.8** (2026-10-26 evening, tag `v0.12.0-beta.8` on master 39d4703, pushed; the GitHub
  pre-release and the install page both deployed green). It holds the sequencer clean-up, the flash diet and the
  owner's rounds of the day (CHANGELOG `## 0.12.0-beta.8`). The owner's unit runs the build just before the release
  package (same code, a dev package); flash the release package next time. Open from the day: the two-heads MASTER
  picture (mocked up, skipped for now), a Geek Out SCOPE page per track, synced delay in-between times, the REPLACE
  stall.

- **2026-10-26 evening, the owner's first rounds on the clean-up build (on master and the unit, unreleased):** CHANGELOG
  `## Unreleased` lists them. Of note for the code: **DRY / WET is per track** (`qd_mix_track_t.dry`, the header
  record's tail: persist.c), applied as Q12 gains `g_lvl` / `g_send[3]` computed at the control rate (app.c
  chain_shares, called from qd_app_tick_locked) because **the RAM code region is full** (148 B free after this: mix.c's
  whole .text lives there, fizz.ld `.ram_text`; a 64-bit division or a few more lines there fail the link with
  "RAMTEXT overflowed"); CHAIN (qd_fx.c qd_fx_block) runs the effects in series on a wet accumulator with each effect's
  share of the earlier wet `qd_cg[]` (the tracks' sends level-weighted). `QD_FXP_RDRY` / `RSPARE1/2` are the old
  chain amounts' slots, stored but unused. The MASTER page keeps its slope curves: the owner's two-heads idea is mocked up (mockups/fizz-master-heads.html, the sheet in png/), not built.
- **2026-10-26, the sequencer clean-up, on master (unreleased):** docs/SEQ-CLEANUP-PLAN.md, the owner's approved
  mock-ups mockups/fizz-seq-cleanup.html (mockups/png/fizz-seq-cleanup-sheet.png), built by the main session by hand
  in three commits (stage A knobs / latch / OCT, stage B SLIDE / SOUND / TRIG, stage C SENDS); CHANGELOG `## Unreleased`
  has the testers' notes. Code: `KNOB_JOB` in ctl.c (SELECT column 0 everywhere, KNOB 1 the main value, NOTE 1..4 on
  KNOB 1..4, ALGORITHM note 5 only with `held_key[i] >= 0`: ctl_step.c qd_step_knob); `qd_step_latch` no longer seeds
  (qd_step_cursor seeds on its first turn); b_oct arms `oct_tap` with nothing held too (a quick release is the entry
  octave; qd_ctl_layer_shown waits QD_TAP_MS for the map whenever a tap is armed). **SLIDE** (`UI_F_SLIDE`, B10): a
  step's `QD_QP_PORT` / `PTIM` / `PSLOPE` locks (pattern.c `qd_step_slide`), read by the player as the 303's slide:
  seq.c fill_rec keeps them out of the record's patch (`slide_m` / `slide_v`), player.c `slide_arm` / `slide_take`
  arm the track's next note (`slide_arm[t]`, `slide_cur[t]`; ON: `slide_write` PORT ON + TIME / SLOPE onto the new
  voice before `qd_pool_glide_note`; LEGATO: the sliding note's OFF is kept back (`slide_hold`) and the next note
  moves that voice with `qd_pool_legato`); notes through a MIDI FX group do not slide (mfx_seq_note's path is
  untouched). The held screen `ui_held.c f_slide`, the lane `ui_lane.c l_slide`, the EDIT head "ON / OFF / LEGATO".
  P-LOCK is gone (`UI_F_PLOCK`, its knob paths in ctl.c / ctl_step.c, `qd_panel_white`); `UI_LOCK_COL` keeps the
  padlock's sand. TRIG's held screen draws the group (`ui_held_t.mfx_*`, view.c fill_held_at). **SENDS**: QD_FX_PAGES[0]
  renamed, `ui_tracks.c fx_sends` (the EDIT home's row via `ui_steps_row`, `ui_lane_t.cells` asks fill_lane for the
  cells in any view), `qd_fx_sends_open`, `qd_snd_keys` is true there too with `ekeys` (REC toggles it, the keys rows
  moved before the panel rows in ctl.c, ctl_leds.c lights them), `qd_panel_steps` true on every panel. Tests: test_ctl
  updated throughout (g_slide, g_mkey's SOUND-on-B11 case, the beat's bass slide). Not done: a Geek Out SCOPE page per
  track (the owner's wish), synced delay in-between times, the REPLACE stall.

- **2026-10-26 night, the flash diet (tier 1), on master and the unit, unreleased:** 575,344 -> 557,472 B (24 KB free).
  One definition of fz_tables.h's tables (`FZ_TABLES_DEFINE` in quad_voice.c; a test that links no voice unit
  defines it itself); `QUAD_PAGES`, `QUAD_MAP`, `N_QUAD_OFS`, the MIDI FX `PAR` table and the glyph metrics `QF_ADV`
  / `QF_OOFF` in the data blob (`QD_XDATA`; the blob 89.5 of 94 KB); core/ui/fz_math.h: the drawing's own sinf ..
  atan2f (defined in ui_kit.c; test_gfx_fast defines its own copy), included after <math.h> in every drawing unit,
  view.c, ctl_leds.c, both app.c; the stats reply sends `<id>:<value>` with the names generated into
  firmware/app/fz_stats_names.h and firmware/tools/fizz_stats_names.py by tools/gen_stats_names.py
  (`stats-names-check` in make test). **Tier 2 (the same night):** the fast font has two weights, 300 and 700
  (tools/gen_fast_font.c `FW`; 400 and 500 read light: `weight_index` ties to the lighter; the small-size atlases are
  at 300 now), the owner's choice over dropping the light one (mockups/png/font-weights-sheet.png); the freed blob
  holds `QUAD_HARM` (`QD_XDATA`), measured on the unit at no cost to the audio ISR (1,589 vs 1,585 us avg). After it:
  fizz.bin 543,568 B (38 KB free), the blob 90.6 of 94 KB (3.6 KB left: the blob is full now). The owner keeps Geek Out
  ("I like them") and wants a **SCOPE view with a scope per track, pre-FX** (not built; a Geek Out page).
  Stripping glyphs is not worth it: only `$;@[]^`{|}` (10 of 95 ASCII) are unused, ~4 KB of blob, nothing of the slot.

- **Released: FIZZ 0.12.0-beta.7** (2026-10-26, tag `v0.12.0-beta.7`, pushed; the unit runs that package): **the FX ROUTING redone as ChoralRoot 0.27's**
  (built by the main session by hand, mockups/fizz-routing.html approved): `QD_FXP_RMODE` (PARALLEL / CHAIN),
  `QD_FXP_ORDER` (0..23 into `QD_RT_ORD`), `QD_FXP_CAMT_C/D/R` (the chain amounts) replace the four cross-feeds
  (XDR..XPOS; an old SET record with fewer values gets the routing's defaults). PARALLEL is bit for bit the old block
  (test_fx_bitexact); CHAIN runs the four in order on core 0 within the block (the delay's wet through its own
  up-sampler `qd_ud`). The page: `ui_tracks.c fx_route`; a block's page key held (`ui.fx_bheld`) lifts it, KNOB 1
  `qd_fx_route_move`; the keys' LEDs glow. Flash after it: fizz.bin 575,344 B (~6.2 KB free).

- **Released: FIZZ 0.12.0-beta.6** (2026-10-26, tag `v0.12.0-beta.6` on master 8ae2dad, pushed; the unit runs that exact
  package, 0 xruns). It holds everything below plus the owner's 2026-10-25 night round (track settings MODE GLOBAL/LOCAL on
  KNOB 1, REC toggles the keyboard on that page, the SHIFT popup gone in 250 ms, MIDI FX bypass keeps the group and flushes
  it, SHIFT + ARP, FILL on SHIFT + FX, page keys close the latched patterns map, scale LEDs 75/40 %). CHANGELOG has the notes.
- **2026-10-25 evening** (now in beta.6): CHANGELOG `## 0.12.0-beta.6` lists it all:
  the beta.5 test fixes (A01, GEEK OUT one view, SAVED at once, the MIDI FX OFF hint, the page LEDs) and the owner's
  next round (global scale via SHIFT + ALGORITHM / PRESETS on a following track, the popup held while SHIFT is down;
  the white keys on the track settings page; KNOB 2 rolls names; SEQ held latches the patterns map; ARP held toggles
  the whole MIDI FX group with a message, ARP lit while MIDI FX is on; HOME keeps the view; the new-preset save no
  longer scans 64 patterns from flash). Still open from the owner: REPLACE of a used preset rewrites patterns under
  the lock (`qd_preset_will_change`) and may stall the same way.
- **Two FM-1s are on USB at times** (the other is the owner's ChoralRoot dev unit, FM-1_920, port "FM-1"). CoreMIDI
  keeps old port names: the FIZZ unit's port is named "ChoralRoot FM-1". Always pass `--port` (fm1_install.py and
  fizz_xdata.py) and check `--info` reports FM-1_46.. before a flash; the loader's port is "Felucca Update"; after
  the write the installer's final check may read the other unit (FM-1_920 "expected FM-1_4600"): confirm with
  `--info --port "ChoralRoot FM-1"`. If the loader ("Felucca Update") does not answer, run the CoreMIDI rescan, then the install with `--port "Felucca Update"`.

- **Released: FIZZ 0.12.0-beta.5** (tag `v0.12.0-beta.5` on master 9878bff, pushed; GitHub pre-release and the install page, both workflows green). The unit runs exactly that package (0 xruns at boot). It holds the owner's feedback round after beta.4:
  Everything in CHANGELOG's `## Unreleased`: SEQ tapped = TOOLS, ARP held = arp on/off, SHIFT + SAVE = the project
  page; B9/B10 STRETCH/CLIP, STRETCH's 13-rung ratio ladder, CLIP labels + scope line + always-on COPY/PASTE + B10
  again; the mixer's black keys (tap select, hold mute, double-tap solo, B11 clear; SELECT page, ALGORITHM track,
  PRESETS mute); hundredth-steps with acceleration on every hi-res value (unit + Manager); the NOTE hold / layer
  keybeds on F; MODES the default latch mode; the KNOBS layouts removed (A only, PAGE FOLLOW on KNOB 1); the pacing
  fix (only bulk transfers slow the frames; PARAM never counts; `xfer_run` on stats line 16); the Manager's LIBRARY
  button widths; the guide and PLAYING.md updated. Flash after it: fizz.bin 572,720 B (8.8 KB free). Store backup:
  `../FIZZ-saves/2026-10-25-before-b5-store.bin`. The unit booted with 0 xruns.
- **Deferred ideas from the owner (not built):** synced delay times in between the divisions (Digitone-like) via
  SHIFT on the delay TIME knob; it touches the stored FX settings (`XP_DDIV` is an index into XF_DIV).
- **Decided, not to build:** delay TIME direction stays as the Digitone's (CW longer); the arp's downward range is
  already OCT DIST negative.

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

1. **The owner's list (2026-10-27):** synced delay
   times in between the divisions (Digitone-like) via SHIFT on the delay TIME knob (it touches the stored FX
   settings: `XP_DDIV` is an index into XF_DIV); the REPLACE stall (REPLACE of a used preset rewrites patterns under
   the lock, `qd_preset_will_change`: likely the same cure as the pattern reads). The HOME screen flash: the owner has
   not seen it again (closed).
2. **The web Manager / emulator session's work** (see Where things stand): merge, test and release with the firmware.
3. **SysEx frames to the unit got lost** (the owner's FM-1, 2026-10-27). Cause: fm1-base holds ONE inbound frame (usb.c
   `sx_collect = !sx_ready`: a frame arriving while one is held is dropped) and FIZZ took FZBI / FZSR / STORE / FP only
   once a main-loop pass (up to a ~170 ms frame). Fixed in FIZZ: app.c `fz_hq`, a 4-deep queue in the pool that every
   `fz_input` (~2 ms) fills from the slot, served in order (between bands only the light ones, as before). Single
   requests after a quiet spell: 0 of 20 lost (were often lost). And fm1-base's usb.c holds TWO frames in order
   (firmware/build.py OVERLAY "bsp/chip/usb.c": fm1-base is charlesvestal's, the submodule unchanged; the update
   loader keeps its own copy): the restore check 0 re-asks in 4.6 s, 2 in flight ~0, 8 in flight still 1-4 % (a
   burst's third and later). Hosts keep ~3 ms between sends (the Manager does) or at most 2 in flight. Also safe now: `fz_pull`
   (restore / FZXD pulls asked again each 1 s for ~90 s), fizz_xdata.py re-asks FZBI / FZBR, re-sends FZSR,
   `restore --check` measures the link with no erase. Never leave FM1VST's Remote FM-1 on with a FIZZ unit.
4. **One xrun at boot** after some flashes (`xruns=1` right after the install), none after. Watch for it.
5. **Web Manager, next phases** (`docs/MANAGER-P2-PLAN.md`):
   - P2-C: the projects tab, a project's patterns, and copying or moving a pattern between projects.
   - P2-D: the project library in the browser, with unload and load and the sounds re-linked.

   The owner decided: on unload, ask each time whether to remove MY presets only that project uses; on a conflict,
   ask and show both versions. Flash estimates: C about 1.6–3.2 KB, D about 3–5.8 KB, so there is not room for both
   without trims.
6. **Screen mirror** (`docs/WEB-MIRROR-PLAN.md`, on branch `web-mirror-plan`, not merged).
   - The recommendation is FIZZ's own mirror: about 2–2.5 KB of flash, no main RAM.
   - The owner said no redesign: reuse the current web player, including its screen-only view, exactly as it looks.
   - Not approved to build yet.
7. **Earlier list, still open:**
   - Scope is the slowest screen (~93 ms).
   - The single-core package sends whole rows.
   - FM1VST core-quadrille is on an old core (0.10.4) and needs updating before the owner prototypes there again.
   - The L1 provenance questions were never asked (OMX-27 credit, Melodee's saturation numbers, the send-guard fix).
   - W7/W8 are dark. There are five options for them; the owner hasn't chosen.
8. **Known gaps:**
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
  3b. **Then write the data blob** `python3 firmware/tools/fizz_xdata.py push --blob firmware/app/build/fizz-xdata.bin`
     **after every dev flash, even when the blob did not change** (seen 2026-10-26: after an install without the push
     the app answered SysEx but ran no audio and no frames, stats `up=0 xdata=0 isr_n=0`; the push brought it up).
     The blob holds the fonts, the ape and, since the diet, the editor's and MIDI FX tables; without it the unit
     shows FIZZ NEEDS ITS DATA (`xdata=1`). A release's install page writes it by itself.
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
