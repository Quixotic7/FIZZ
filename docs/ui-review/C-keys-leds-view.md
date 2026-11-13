# C: key states, LEDs and view-model filling (FIZZ, read-only review, 2026-11-13)

Sizes assume about 3-4 B per simple statement at -Os. A `ui_RR` call with float literals costs about 25-30 B.

## 1. The scale key state is worked out in four places, and the view converts LEDs back into states

Copies:
- `ctl_snd.c:448-475` `qd_keys_leds`: held or sounding → LIT, root → ROOT, in the scale → SCALE, otherwise OFF.
- `ctl_mfx.c:557-568`: the MIDI FX W keys. The same SCALE branch, typed out again.
- `ctl_leds.c:325-332`: the ksteps LEDs for B1..B10.
- `view.c:930-935`: `bst[]`, the same logic expressed as `ui_rec_key` states.
- Inverse mappings: `view.c:843-858` (`qd_view_kst`) and `ctl_rec.c:676-685` (`qd_mk_kst`) turn LED levels back into states.

Differences the owner may want to unify:
- **MIDI FX ignores `keys_held`.** A held W key lights only once its note sounds, so with an ARP or CHORD the key you press may stay dark. Everywhere else a held key lights at once.
- **ksteps B1..B10 never light for "sounding".** Notes played by the sequencer or MIDI don't light them (both the LED and `bst`), but they do light the W keys on the keyboard page. Their root test is `% 12` without the `((n%12)+12)%12` guard.
- **ksteps never shows a selected step's notes.** `ctl_leds.c:321` blinks the black keys by chromatic pitch, then lines 328-332 overwrite B1..B10. `bst` has no step-note state either. KEYS mode does show them: the LED blinks, and `view.c:885-892` draws them magenta on the keybed.
- **Labels and LEDs use different pitches.** `lb_notes` (`view_labels.c:103-123`) names the snapped, played note (`qd_rec_key_note`). The LEDs mark the key's own pitch (`ctl_snd.c:463`, `ctl_leds.c:305`).
- **The blink's off phase shows the key as out of the scale.** `qd_view_kst` maps every LED level except LIT, ROOT and SCALE to 0. So a step note's off phase is drawn as out of the scale (G3), not in its scale state.

Helper:
- `int qd_key_note_shown(const qd_app_t*, int key)`: the scale degree for a SCALE W key or a ksteps B key, otherwise the key's own pitch.
- `uint8_t qd_key_st(const qd_app_t*, int key, const uint8_t snd[128])`: the `ui_rec_key` state.
- `static const uint8_t ST_LED[8]`: the state-to-LED table.

`qd_keys_leds`, the MIDI FX block, the ksteps loop, `bst` and `qd_view_kst` all become loops over `qd_key_st`. About 40 statements. Net saving about 150-200 B. Risk: medium (the LED-level tests in test_ctl, and the decisions above).

## 2. The page's step row (trig / sel / play) is filled 7 times, in 4 shapes

Screen copies:
- `view.c:919-941` `draw_keys`: `page_trig[16]` as bytes, plus `sel` and `play`; also `895-900` for the recording cursor's page.
- `view.c:1677-1687` `qd_view_snd`: `row_trig`, `row_sel`, `row_play`.
- `view.c:531-543` and `553-555` `fill_layer`: `b11_sel`, `b11_play` and `trig[8][16]`.
- `view.c:2826-2828` and `2840-2842` `qd_view_ov`: the held mask and each track's playhead.
- `view.c:146-151` and `183-184` `fill_lane`: `UI_C_ON` / `UI_C_HELD` per cell. `ui_held.c:1230-1238` packs these back into masks.
- More packers: `ui_keys.c:155-158` and `ui_layer.c:72-79`.

LED copies:
- `ctl_leds.c:87-121` (`step_level` + `leds_whites`).
- `ctl_mfx.c:518-530`, which types `step_level` out again.
- `ctl_tools.c:705-707` is a genuine preview and should stay as it is.

Differences:
- **Which page.** `snd` uses `held[0]`'s page while anything is selected (`view.c:1679`). Every other screen and the LEDs use `page[sel]`.
- **Playhead.**
  - On the keys row it is drawn only with ksteps (`ui_keys.c:160`), although `view.c` fills `v.play` whenever the row is shown.
  - The layer, `snd` and EDIT rows always draw it.
  - The MIDI FX W LEDs neither invert the playhead nor give `wk_pending` feedback (`ctl_leds.c:105`).
- **Selected-step blink.** `ctl_leds.c:119` uses `QD_FAST_MS`, the MIDI FX keys use `QD_BLINK_MS` (`ctl_mfx.c:516, 525`), and TOOLS uses `QD_FAST_MS` in the opposite phase.
- **What counts as a trig.** The screens use `qd_step_is_on`, so LOCK steps look like trigs and out-of-range steps are drawn. The LEDs dim LOCK steps and turn out-of-range steps off. OVERVIEW tells both apart.

Net saving about 200 B (see proposal A). Risk: low to medium.

## 3. The pages block (`loop`, `pstart`, `pend`, `play_page`, `trig[8][16]`) is copied

Copies: `view.c:549-555` (`fill_layer`), `746-752` (`fill_turn` `UI_TK_PAGE`) and `111-113` (`fill_lane`, without `trig`).

Helper: `static void fill_pages(const qd_app_t*, int t, uint8_t *loop, uint8_t ps[8], uint8_t pe[8], int *play, uint8_t (*trig)[16])`. About 50 B. Risk: low.

## 4. Lock chips (`locked` / `lockany`) follow different rules in 4 copies

Copies:
- `view.c:1717-1718`: the MIDI page sets `locked` only and never `lockany`.
- `view.c:1751-1753` and `1761-1762`: the RB1/RB2 pair.
- `fill_mix` `2195-2213`.
- `fill_fx` `2288-2292`.
- `ctl_snd.c:930-937`: the B11 count, with the RB2 pair again.

Differences:
- **Following the playing step.** The sound editor and FX do it; the mix doesn't.
- **Both at once.** While following, the sound editor can show `locked` and `lockany` together; in FX they exclude each other.

Helper: `static uint8_t lock_st(const qd_app_t*, int id, int fol)` returning 0, 1 for a filled chip, or 2 for an outlined one, and handling RB2 itself. About 60 B. Risk: low.

"A step is selected" (the frame and the count) is also defined differently:
- the mix and FX use `nheld || wk_pending` (the same two lines at `2193-2194` and `2347-2348`);
- the sound editor uses `nheld`;
- the keys page uses `framed = nheld > 0` (`929`);
- MIDI FX uses `qd_mfx_steps_held`.

## 5. The step frame and the "STEP n" text are repeated

- **The full-screen step frame**, `ui_RR(0,0,240,240,8,UI_STEP_FRAME,3,QD_NONE)`, appears 13 times:
  - `ui_mfx.c:1124, 1141, 1162, 1226, 1235`
  - `ui_keys.c:169`, `ui_presets.c:211`
  - `ui_tracks.c:126, 699`
  - `ui_snd.c:1045, 1107`
  - `ui_held.c:1005`, `view.c:2669`

  Replacing them with `void ui_step_frame(void)` saves about 300 B. This is the biggest cheap win. Risk: low.
- **"STEP n+k"** is built identically at `view.c:1371-1373` and `ui_held.c:1013-1015, 1099-1101`. Plain "STEP n" appears in `ui_snd.c:1050`, `ui_mfx.c:1237`, `view.c:657, 1063` and `ui_picker.c:55`. A `char *ui_step_txt(char*, int step, int more)` saves about 80 B.
  - Visual difference: the sound editor, MIDI FX and the takeovers never show "+k". The mix and FX show the magenta count chip instead.
- **Default-step resolution** is built the same way in two places: `view.c:343-346` and `1360-1364` both build an ON step and resolve it with `_def`. A helper saves about 20 B.

## 6. view_labels.c

- **The "STEP n" W-label loop appears 6 times:** `356-357`, `391-392`, `410-411`, `459-460`, `476-477`, `488-489`. An `lb_steps(a, o)` helper saves about 100 B.
- **`if (qd_snd_keys(a)) lb_notes(a, o, !rec_ekeys)` appears 6 times:** `336`, `353`, `388`, `453`, `546`, `550`. This could become one pass after `lb_ctx`.
  - Decide one thing first. With FULL in the sound editor, B9/B10 are labelled DICE / NEW SOUND (`461-462`), but `ctl_leds.c:385-387` lights them as notes.
- **Dead block:** `if (0 && ...)` at `214-223` (source only, no flash).
- **Wasted work:** `343-345` runs the whole `fill_fx` (including `qd_view_kst`) only to read 4 labels.

## 7. ctl_leds.c

- **The black-key copy from `bk[11]` appears 3 times:** `203-206`, `214-217`, `274-277`. If `qd_panel_leds` and `qd_snd_leds` wrote `key[]` like the MFX, TOOLS, SHIFT and BROWSE functions do, the copies disappear. About 45 B.
- **Redundant clears:** the black-key clears at `132-133` and `249-250` reset keys that nothing has written since the memset at `367`. About 20 B.
- **B11 is written three times:** at `288`, at `389-390` (the same value) and at `391-393`. Line 288 is redundant if `qd_ctl_b11_free` is true there; verify.
- **The pflash / wflash bodies are identical** (`397-401` and `402-406`). A `flash_once(uint8_t*, uint32_t)` helper saves about 30 B.
- **`qd_led_breath` and `qd_led_breath_lo` (`52-63`) have the same body** with different constants. Merging them saves about 40 B of soft-float code.
- **The blink idiom is typed out by hand** in `ctl_mix:428`, `ctl_shift:168`, `ctl_tools:707`, `ctl_mfx:517`, `ctl_presets:498` and `ctl_leds:252` (the last in the opposite phase). Exporting `blink()` as `qd_led_blink` saves about 30 B.
- **The `leds_step` if-chain:** the branches really differ, so a table isn't worth it.

## 8. Dead code and fields

- `ui_snd_t.keys` and `kst[27]` (`ui.h:766-767`): never filled (`view.c:1789`) and never drawn.
- The `full` argument of `qd_view_kst` and its 27-key path: every caller passes 0 (`view.c:2314`, `2666`; tests at `9942` and `10024`).
- `ui_fx_t.keys_down`: never filled and never drawn.
- `ui_layer_t` fields:
  - `page_on[16]` is filled at `573-574` but never drawn.
  - `shift` (`558`), `held_track` (`584`) and `rate_idx` (`587`) are filled but never drawn.
  - `RN` with its `(void)RN` (`521`, `588`) is never used.
  - `clip_step` is neither filled nor drawn.

  Together about 40 B.
- **Likely-unreachable takeovers:** the `fill_turn` cases `UI_TK_LEN`, `RATE`, `LIMIT`, `TSWING` and `TRACK` (`705-740`, `794-811`) and their drawings in `ui_turn.c`.
  - No `qd_ctl_takeover` call passes these kinds.
  - The setters are `ctl.c:284, 1962 (FREQ..FDBK), 1974, 1990, 2084, 2091, 2315, 2364`, `ctl_rec:489`, `ctl_shift`, `ctl_tools` and the table at `ctl_step.c:102-103`.
  - The comment at `ctl.h:67` is stale.

  Removing them could save 0.5-1 KB. Confirm with a trap first. Risk: medium.
- **`ui_keys_t.page_trig[16]`** is a byte array that `ui_keys.c` packs back into a mask; it could be a `uint16`.
- **The SENDS strip is drawn by its own path.** It uses `ui_fx_t.keys`/`kst` (`view.c:2310-2314`, `ui_tracks.c:533-534`, clear = 0). Every other page uses the foot overlay (`view.c:2661-2667`, clear = 1). Adding `QD_SCR_FX`+SENDS to the overlay removes `kst` from `ui_fx_t`. About 40 B.
- **Screens and LEDs disagree about where the keys are shown.** The keys' LEDs come on for every FX page and for OVERVIEW (`ctl_leds.c:376`), but the screen draws the strip only on LANE, EDIT, GEEK, MIX, TOOLS and SENDS.
  - With FULL, the black keys' states never appear on screen, because the strip shows only the 16 W keys.

## Proposal

**A. One step-row model, filled once per frame at the top of `view_draw`:**
```c
typedef struct { uint16_t trig, lock, out, sel; int8_t play, page; uint8_t framed, nsel; } ui_row_t;
void qd_view_row(const qd_app_t *a, ui_row_t *r);
void ui_page_row(float y, const ui_row_t *r, int red);
uint8_t qd_row_led(const ui_row_t *r, int w, uint32_t ms);
```
- `page` is `page[sel]` (decide what the sound editor should use).
- `sel` is the held steps on that page (or `f1_src` under F1). `framed` and `nsel` follow one rule (decide on `wk_pending`).
- The keys, sound editor, layer, EDIT and OVERVIEW models, and the mix/FX `step`/`nsel` fields, all point to the same row.
- `qd_row_led` gives: out of range → off, LOCK → dim, trig → lit, empty → faint; a selected step blinks at one rate; the playhead inverts. `leds_whites` and the MIDI FX STEPS loop both use it.

**B. One key-state function** (`qd_key_st`, as in finding 1):
- Its states feed `ST_LED[]` for the LEDs and `ui_rec_key` for the screens directly.
- `qd_view_kst` stops converting LEDs back into states, and `bst` becomes the states of B1..B10.
- The step-note blink is applied on the LED side only.
- One `qd_ctl_sounding` scan per frame instead of up to four.

The safe items total about 0.9-1.3 KB. Confirming the dead takeovers adds another 0.5-1 KB.
