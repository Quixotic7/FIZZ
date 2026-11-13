# D: control dispatch, takeovers, dead code (read-only review, 2026-11-13)

Sizes are measured, not guessed: `arm-none-eabi-nm -S` on a copy of firmware/app/build/fizz-ble.elf (built 14:03 today, -Os, no ICF, gc-sections). Biggest functions: qd_ctl_ms 1748 B, qd_tools_knob 966, qd_br_key 800, b_oct 788, qd_ctl_preview_sync 786, qd_shift_black 784, k_f1_black 700, qd_type_knob 680, tl_apply 676, qd_tools_page 648, strike 508, k_unlock 452, tl_clip 442, k_menu 422, k_f3 420.

## 1. Findings

**F1. Opening a page: 9 hand-rolled copies.** ctl_lib.c:25-34 (lib_open_page), ctl_presets.c:163-172 (br_open), ctl_presets.c:589-595 (qd_sv_open), ctl_tools.c:369-373, ctl_mfx.c:109-113, ctl_mix.c:66-76 (panel_open), ctl.c:1626-1631 (SHIFT+REC), ctl.c:1449-1455 (home_hold), ctl.c:1551-1555 (f3_open). Each one does some subset of: set_mode(STEP), release held steps, end the step latch, `tk = NONE`, latch_open.
Where they differ (for the owner to decide):
- REC settings and HOME-hold settings end the step lock. All the others keep it.
- EDIT, MIDI FX, TOOLS and F3 leave a running takeover up. The browser, SAVE, PRJ and the panels clear it.
- MIDI FX keeps KEYS mode. All the others force STEP.
- Pressing the page's own key again goes to the next page on FX, MIX, SHIFT+SEL, SHIFT+REC and SHIFT+SAVE, but closes the page on EDIT, ARP and SEQ (TOOLS).

Refactor: add `void qd_page_open(qd_app_t *a, int latch, unsigned f)` in ctl_latch.c, with flags PO_KEEP_KEYS, PO_END_LOCK and PO_KEEP_TK. ~100-130 B. Risk: medium (tests pin each variant).

**F2. b_tracks, b_page and b_snd are one function three times.** ctl.c:1319-1330, 1335-1342 and 1872-1883: `if IDLE return; if (!STEP) set_mode(STEP); X_button(a)`. The `keys … slatch = 0` lines (1320-1321, 1328-1329, 1877, 1881-1882) are already **dead**: set_mode(STEP) from KEYS has cleared slatch and released the steps (ctl.c:343-349). Refactor: `static void page_btn(qd_app_t *a, void (*open)(qd_app_t *))`. ~50 B. Risk: low.

**F3. Hold timers and press bookkeeping, per button.** qd_ctl_ms repeats `held(B) && !used && ms - t0 >= HOLD && mode != IDLE → used = 1; act` six times: ARP 2519-2522, LFO 2528-2530, PLAY 2590-2595, SEL 2596-2603, HOME 2604-2605, REC 2606-2612. The press side repeats `x_ms = ms; x_used = 0` in b_shift 1348-1350, b_home 1421-1423, b_lfo 1465-1469, b_fill 1518-1520, b_play 1567-1569, b_rec 1613-1616, b_sel 1684-1685 and b_seq 1849-1851. touch() (362-379) separately marks lfo_used, home_used and oct_tap_used. SEL-hold (2598-2602) is the SHIFT+SEL body (1687-1691) copied.
Refactor:
- `uint32_t btn_ms[QD_NBTN]` and `uint16_t btn_used`, set in qd_ctl_button before dispatch.
- A table `{button, hold_ms, when(), act()}`.
- `static void trk_open(a)` for the SEL-hold and SHIFT+SEL body.

~120-180 B, and ~10 fields of RAM. Risk: medium (the used-flag semantics differ: home_used bits, seq_used bit 1).

**F4. Takeovers: one way to start one, many ways to end one.** Arming is centralised (qd_ctl_takeover, ctl.c:209-219; 29 callers). Ending is not:
- Lifetimes are an if-chain in qd_ctl_ms (2537-2555): PRESET held, MFX_STEP held, TOOL 800 ms, SCALE/ROOT while SHIFT plus 250 ms, TAP 2.8 s, default 1.12 s.
- Callers set `tk_hold` after the call (ctl_step.c:971, 1285; ctl_mfx.c:77, 440).
- There are 20 bare `tk = QD_TK_NONE` sites. Six of them are the same `if (tk == HELDKNOB || tk == MFX_STEP) tk = NONE` (ctl_step.c:181, 222, 255, 291, 460, 478).
- The other popups run their own clocks with literals: confirm (cf_*, 1100 ms), tempo_ms (view 1000u), mfx_ms (view 2000u), mk_ms (ctl_rec.c:951, 1000u), browser fx (800u/900u), clear-arm (view.c:623 uses 2000u, not QD_ARM_MS), pflash/wflash and snd_fpop.
- oct_ms (ctl.c:283) always equals tk_ms for UI_TK_OCTAVE.

Refactor:
- A lifetime table `TK_LIFE[]` with {ms, hold-while flag}.
- `qd_ctl_tk_close(a, mask)`.
- Drop oct_ms; name the literals.

~60-100 B. Risk: medium.

**F5. "Press twice to confirm": six implementations.**
- Clear the track: SHIFT+B11 (ctl_shift.c:117-135, clr_armed=1), F2+B9 (ctl.c:549-565, clr_armed=2) and TOOLS CLEAR (ctl_tools.c:326-331; its tl_armed ignores the tag).
- Browser DELETE?/OUT? (ctl_presets.c:363-367, 386-391).
- MIDI FX REMOVE (ctl_mfx.c:266-273).
- Projects copy (ctl_lib.c:109-112).

Inconsistencies:
- The words: "TRACK CLEARED" (F2+B9) vs "CLEARED" (SHIFT+B11).
- Clear-arm times out after 2 s. The others never time out (another key disarms them).

Refactor: `static int arm2(qd_app_t *a, int tag, const char *q)` (1 = confirmed) plus one `clear_track(a)`. ~70-100 B. Risk: low.

**F6. Snapshot/pattern recall: three copies** of `undo_hint; qd_hist_recall; pat_reload; confirm`: ctl_shift.c:78-85, 89-96 and ctl_tools.c:317-321. Refactor: `static int hist_paste(qd_app_t *a, int slot, const char *word)`. ~40 B. Risk: low.

**F7. Keyboard octave and panic copied inside b_oct.** The octave `from = oct; oct = clamp; qd_ctl_octave(UI_OCT_KEYS…)` appears at ctl.c:1730-1733, 1780-1782 and 1793-1795. The panic `kill_all; midi_out_panic` appears at 1768-1769 and 1790-1791.
Inconsistency: on the step keyboard a tap in SOUND moves the bank (1728); on the editor's keyboard it is always the octave.
Refactor: `keys_oct(a, d)` and `panic(a)`. ~50 B. Risk: low.

**F8. k_note (ctl.c:658-674) and k_snd_note (457-469)** are identical except for the note source (qd_rec_key_note vs qd_snd_key_note) and k_note's B11 skip. Refactor: `static void play_key(a, key, note, down, vel)`. ~80 B (102 B each now). Risk: low.

**F9. Thunks and exact duplicates in the tables.**
- Identical predicates: when_f3/w_f3 (20 B each), when_stepview/w_step_view (18), when_mkey/w_mkey (14), when_lib/w_lib and when_type/w_type (10). when_steps_held only wraps w_steps_held.
- Knob forwarders with the same signature as their target: k_held, k_live, k_mfx, k_libk, k_mix (10 B each).
- One-line wrappers: select_track, track_sound, qd_ctl_knob_role, qd_lib_back (10-12 B each).

Pointing KNOB_ROWS at the w_* predicates and the qd_* targets saves ~170 B. Risk: zero.
Separately, about a dozen key thunks (k_tools_*, k_mfx_black, k_panel_black, k_value, k_latch_default, k_type_*, k_lib_*, k_mk; 12-26 B each, ~200 B in all) only adapt arguments. They go away if the callees take `qd_key_fn`'s signature. Risk: medium (public prototypes, tests).

**F10. The SEQ/SEL/FX-held guards differ by page.**
- Keys: w_snd 392, w_snd_keys 441 and w_mfx 485 exclude SEQ and SEL.
- Knobs: when_snd 2205 and when_mfx 2215 exclude SEQ and FX; when_tools 2437 excludes SEQ and SEL; when_mix 2223 and when_fx 2225 exclude nothing.

Effect: SEQ held + ALGORITHM sets the launch mode on EDIT, MIDI FX and TOOLS, but turns the page's knob on MIX and FX. Refactor: one `page_free(a)` predicate. Behaviour is the owner's call.

**F11. qd_tools_knob (ctl_tools.c:462-579, 966 B).** Eight copies of `n = clamp(*v + step); ch = n != *v; *v = n` (494-503, 511-513, 521-523, 531-533, 540-542, 549-551, 555-557, 562-564).
Refactor:
- A table `{tool, knob, offsetof(v), lo, hi, mul, acc}` plus `static int tl_set8(uint8_t *v, int n)`.
- `tl_lpage(T)` for `scope <= QD_TS_PAGE ? page : -1` (172, 178, 729, 732, 806).
- `tl_before(a, p, t, page, b)` for the undo_before offset math (655, 758, 808, 812).

~300-400 B. Risk: medium (test_tools).

**F12. Browser (ctl_presets.c).**
- `br_relist(); if (moved) br_apply()` five times (373-376, 399-401, 418-420, 451-453, 468-470): fold the apply into br_relist.
- "Track's own sound back" three times (125-127, 211-213, 253-255): `br_restore(a)`.
- qd_br_goto 284-293 repeats qd_br_turn 198-204.
- The clamp in qd_br_chosen is computed twice (80-81).

~110-130 B. Risk: low.

**F13. Preview (ctl_preview.c).**
- strike's step_ms (153-157) repeats qd_ctl_len_ms's formula (127-133): add `qd_ctl_step_ms(a)`, ~40 B.
- drop (81-96, 176 B) shifts ten parallel pvs_* arrays. An array of structs makes it one memmove, ~150 B. Tests read A.ui.pvs_until[0] and pvs_step[0] (test_ctl.c:2851), so those lines need edits. Risk: low-medium.

**F14. Small items.**
- step_of exists twice (ctl.c:688 and ctl_step.c:119; 28 B each).
- KNOB_ROLES (ctl.c:2145-2156) is an identity left from the removed KNOBS layouts B..D. Switch on the knob in k_steps_cursor instead (~50 B).
- clampi is defined four times (all inlined, 0 B).
- qd_latch_t.name and .back (ctl_latch.c:36-50) are never read; .confirm is only tested for truth (ctl_leds.c:454); on_back and on_confirm are used by TOOLS alone. Cutting them saves 104 B of pointers plus their strings.

## 2. Dead code (with evidence)

- **qd_step_clear_locks** (ctl_step.c:982-998, prototype ctl.h:1069). P-LOCK's B11. `grep -rnw` over core, firmware/app, tools and ios/engine/src finds only the definition and the prototype. The linker already drops it, so no flash is saved; source only.
- **qd_ctl_set_view** (ctl.c:1387, ctl.h:1021). Same grep: definition and prototype only. Also dropped by the linker.
- **qd_preset_walk** (ctl.h:764). A prototype with no definition anywhere.
- **The SEL-held map.** w_sel_held (ctl.c:388) returns 0, so WHITE_ROWS[2] (1165) and BLACK_ROWS[2] (1192) never fire. That leaves dead:
  - k_sel_white (607-625, 94 B) and k_sel_black (628-635, 36 B);
  - the SEL half of sel_seq_timers (644-648);
  - qd_ctl_sel_map (1694, returns 0), hence view.c:1497-1498 and the QD_SCR_SEL case at view.c:2605-2608;
  - sel_down/sel_done bits 0..7. The mix now owns bits 8..13; ctl.c:986, 1385 and ctl_mix.c:73 mask around the dead half.
- **QD_TK_LMODE** (ctl.h:231). Never assigned (grep: only checked, at ctl.c:1473 and view.c:2673). Both checks are dead.
- **The F1+B9 hold-clear leftovers** (removed 2026-11-11):
  - b9_done is only ever written 0 (ctl.c:859), so `if (!b9_done)` (863) is always true;
  - b9_t0 is written at 860 and never read;
  - QD_CLEAR_HOLD_MS (ctl.h:191) is unused;
  - the comments at ctl.c:782, 884-885 and ctl.h:44 are stale.
- **k_unlock unreachable branches** (ctl.c:2391-2395). when_unlock (2369-2374) only allows the editor, MIX or FX, and those branches return before reaching them (P-LOCK leftover). The lock counting is dead too: `n += qd_lock_get(...)` at 2410 and 2413 ends in `(void)n` at 2423.
- **The redundant keys/slatch lines in b_tracks and b_snd** (F2).
- **qd_lib_ms** (ctl_lib.c:218) has an empty body but is called every millisecond (ctl.c:2516).
- **QD_MODE_TRACKS.** Never entered by the controller (grep: app.c:187 range check, the glue's name string, ctl.c:1766). The `|| QD_MODE_TRACKS` in b_oct is dead. UI_TK_TRACK's case at view.c:794 is marked unused in its own comment.
- **Fields written but never read** (a script over core, firmware/app, tools and ios for `.f`/`->f` reads):
  - ui.snd_ms (ctl_snd.c:139, 849, 855)
  - br.fx_k (ctl_presets.c:359)
  - ty.hit_shift (ctl_type.c:103)
  - ui.sel_panel (ctl_step.c:145)
  - nm.ret (ctl_lib.c:45)
  - ui.b9_t0
  - app.h:236 snd_gen and snd_ms (no reference at all)
  - qd_lockscreen_t.name (ctl.h:787)
- **Test-only callers.** qd_ctl_layout_role is used outside its wrapper only by test_ctl.c:3666-3667.
- **`#if 0`.** None in core/app (grep).
- **Likely shadowed:** k_stepview's SELECT and PRESETS cases (ctl.c:2000-2007). when_cursor (2172) takes them whenever no finger is down. I did not prove this; check with a coverage run.

Rough total: ~1.3-1.8 KB of flash across F1-F14, plus the dead rows. The low-risk set alone (F2, F5-F9 thunks, F12, F14, the dead SEL map, k_unlock) comes to ~700-800 B.
