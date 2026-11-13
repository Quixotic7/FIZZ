# UI and control code: review and refactor plan (2026-11-13)

Scope: `core/app/` (ctl*.c, view.c, view_labels.c) and `core/ui/` (the screens), at master `fb1eb96`.
Goals: (1) the same behaviour in every mode, (2) flash back, so the drawing units can go from -Oz back to -Os.
The detailed findings, with file:line for every copy, are in `docs/ui-review/`:

| file | theme |
|---|---|
| `A-selection.md` | step selection, the LFO latch, ALGORITHM's cursor, "step vs track" values, knob roles |
| `B-drawing.md` | the screens' shared parts: frame, head, foot cells, step rows, keybeds, rolls |
| `C-keys-leds-view.md` | key states, LEDs, the view models view.c fills |
| `D-dispatch-dead.md` | ctl.c's dispatch, takeovers, page opening, dead code |

## 1. Where the flash stands

Before the refactor, from `firmware/build.py`'s size report:

| package | app slot free | other |
|---|---|---|
| USB | 32 B | |
| Bluetooth | 80 B | second blob (XDATA2) 20 B free |
| single-core | 20 B | |

The linker drops unreferenced functions (`-ffunction-sections`, `--gc-sections`). Dead *functions* cost no flash.
Dead *branches*, dead *cases* and fields that are filled but never drawn do cost flash.

What -Oz saves on the USB package. This was measured by building with `USB_OZ` emptied and comparing the link maps:

| unit (USB_OZ) | -Oz | -Os | -Os costs | draws every frame? |
|---|---|---|---|---|
| ui_layer.o | 93,416 | 100,386 | **6,970** | yes, when open: the sound editor (+1,610), MIDI FX (+1,822), the dispatcher (+486); the rest are cold screens |
| ui_idle.o | 4,518 | 5,870 | 1,352 | the idle screen |
| ctl_latch.o | 30,592 | 31,566 | 974 | no (controls) |
| ui_turn.o | 15,384 | 15,876 | 492 | takeovers, the knob band |
| ctl_step.o | 12,642 | 13,040 | 398 | no |
| ctl_rec.o | 8,676 | 8,848 | 172 | no |
| chord.o | 460 | 564 | 104 | no |
| ui_octave.o | 1,458 | 1,540 | 82 | the octave takeover |
| clip.o | 1,998 | 2,028 | 30 | no |
| ui_picker.o | (dropped) | | 0 | no caller: the linker drops it; its -Oz entries in build.py do nothing |

The control units (ctl_*, clip, chord) are not drawing code, so -Oz there does not slow the frames.
**The owner's goal, "screens draw at -Os again", is about ui_layer.o, ui_turn.o, ui_octave.o and ui_idle.o.**
For ui_layer.o, putting back only the hot part (the sound editor, MIDI FX and the dispatcher) costs about 3.9 KB, against 7 KB for the whole unit; see §5.

## 2. The root causes

1. **No selection layer.** Every mode reads `ui.held[]`, `ui.nheld`, `ui.slatch` and `wk_pending` itself. Each mode then works out again: the first selected step, the page mask, "a finger is on a step", "the step's lock, else the track's value", and "follow the playing step".
   - There are 7 copies of the page mask, 5 of "is step s selected", 8 of "lock else base", 4 of the follow rule, 5 different "finger" predicates and 7 hand-written latch ends.
   - MIDI FX keeps a second, private selection (`mfx_wheld`).
   - `ksteps` duplicates `ekeys` and goes stale outside the keyboard page.
2. **No screen furniture.** Every screen draws its own head, its own KNOB 1..4 foot, its own 16-step row and its own magenta step frame.
   - The frame `ui_RR(0,0,240,240,8,UI_STEP_FRAME,3,…)` is written 14 times.
   - The foot cells sit on 4 column grids.
   - The 16-step row has 7 geometries.
   - The line helper (`line2`, `ln`, `tl_line`, `gk_line`) is copied 9 times, although `qd_line` does the same job.
3. **No key-state function.** "In the scale / a root / sounding" for a key is computed in 4 places. Two more functions turn LED levels back into states.
4. **Per-button and per-takeover bookkeeping.**
   - Holds are timed six times over in `qd_ctl_ms`.
   - Takeovers have one start (`qd_ctl_takeover`) but about 20 ad-hoc ends.
   - "Press twice to confirm" is written six ways.
   - Opening a page over the step view is written nine times.
5. **Leftovers of removed features**, still compiled in. The biggest are five takeover kinds that nothing shows any more: LEN, RATE, LIMIT, TSWING and TRACK (`ui_turn.c` d_rate, d_len, d_limit, d_track; view.c fill_turn).

## 3. The proposed shared layer

### Control side (`core/app/ctl_sel.c`, or the top of ctl_step.c)

```c
typedef struct {
    int8_t   track;      /* a->sel */
    int16_t  first;      /* the first selected step (0..127), -1 none */
    uint8_t  n;          /* how many */
    uint8_t  src;        /* QD_SEL_NONE / FINGER (W held) / LATCH (LFO) / CURSOR (ALGORITHM) / FOLLOW (the step playing) */
    uint8_t  page;       /* the page the row shows */
    uint16_t mask;       /* the selected steps on that page */
    int16_t  follow;     /* the step heard, -1 */
} qd_sel_t;
void qd_selection(const qd_app_t *a, qd_sel_t *s);          /* the one place the rules live */
int  qd_sel_find(const qd_app_t *a, int step);               /* is step selected */
int  qd_ctl_finger(const qd_app_t *a);                       /* a finger on a step (wk_pending included) */
int  qd_step_val(const qd_app_t *a, int step, int id);       /* the step's lock, else the track's */
int  qd_sel_locks(qd_app_t *a, const int8_t *ids, int n, int clear);   /* count / clear (RATIO B's pair) */
void qd_step_latch_end(qd_app_t *a);                         /* every latch end: saves the selection for LFO */
int  qd_w_steps(const qd_app_t *a);                          /* the W keys are steps (replaces ksteps) */
void qd_page_open(qd_app_t *a, int latch, int flags);        /* opening a page over the step view */
```

Every mode's knob handler asks `qd_selection` once. ALGORITHM's cursor becomes one row of the knob table, not three branches.

### Screen side (`core/ui/ui_kit.c`, exported in ui.h)

- **One row model.** `ui_row_t {trig, lock, out, sel; play, page; framed, nsel}` is filled once a frame by `qd_view_row()`. The keys page, the sound editor, the F layers, EDIT, OVERVIEW, the mix and the effects point at it.
  - `ui_page_row(y, const ui_row_t *)` draws it, and `ui_step_x(i)` is the one column geometry.
  - `qd_row_led()` lights the W keys from the same model: one blink rate, one playhead rule.
- **One key state.** `qd_key_st(a, key)` returns the state that drives both the LEDs (through a table) and the orange key grids (`ui_rec_key`, `bst`, `kst`).
- **Furniture.**
  - `ui_step_frame()`, drawn last on every screen.
  - `ui_foot_cell(k, lab, val, col, flags)` on one grid.
  - `ui_dots`, `ui_title`, `ui_back_btn` and `ui_badge` for the head pieces.
  - `ui_step_label()` for "STEP n+k".
  - `ui_T_fit()` for text that must fit a width.
  - Plus `qd_line`, `ui_ease`, `ui_cat` and `ui_num3`.

## 4. Ranked opportunities

Saved bytes are estimates unless marked (m), which means measured in the link map. The risk column says whether pixels or behaviour change.

### Stage 1: no change to pixels or behaviour (low risk), about 3–3.5 KB

| # | change | saves | risk |
|---|---|---|---|
| 1.1 | Remove the five dead takeovers (LEN, RATE, LIMIT, TSWING, TRACK) from ui_turn.c and view.c `fill_turn` | ~1–1.3 KB | low: no caller anywhere, tests and render tools included |
| 1.2 | Replace the 9 line-wrapper copies with `qd_line` | ~400 B (46 B each, m) | low: verify pixels |
| 1.3 | Remove the helper duplicates: `mf_ease`/`lb_ease` → `ui_ease`, `MF_TP` → `UI_TRACK_PAL`, `cx16`/`col_x`, `cat`×3, `three`/`lb_num3` | ~250 B | low |
| 1.4 | Add `ui_step_frame()`: 14 copies → 1 call each, keeping each screen's draw order | ~200–300 B | low |
| 1.5 | Add `ui_T_fit()` (14 sites); one site measures text and throws the width away, every frame | ~350 B | low |
| 1.6 | EDIT's head value: keep `ed_head_val`; the second switch becomes a table of offsets. Add `ui_step_label()` | ~350 B | low |
| 1.7 | Control tables point at the real functions: duplicate predicates (when_f3/w_f3, …) and pass-through thunks | ~170 B | none |
| 1.8 | Remove the dead branches and fields listed below | ~300 B | low |
| 1.9 | ctl_leds.c: the `bk[11]` copies ×3, redundant clears, `blink()` exported, the two breath functions merged | ~150 B | low |
| 1.10 | view_labels.c: `lb_steps()` (6 loops), `lb_notes` guard; the SENDS labels without a whole `fill_fx` | ~100 B | low |
| 1.11 | `b_tracks`/`b_page`/`b_snd` as one helper; `step_of` defined once; `no_room()` exported (5 copies); the mix's `ID[2][4]` table once | ~200 B | low |

Dead branches and fields for 1.8:
- the SEL-held map (`w_sel_held` returns 0: k_sel_white, k_sel_black, half of sel_seq_timers, qd_ctl_sel_map, and view.c's SEL screen);
- `k_unlock`'s unreachable branches;
- the F1+B9 hold-clear leftovers (b9_done, b9_t0);
- `QD_TK_LMODE`;
- b_oct's QD_MODE_TRACKS branch;
- `KNOB_ROLES`, an identity table;
- `qd_lib_ms`, empty but called every ms;
- `ui_layer_t.page_on` (filled every frame, never drawn), shift, held_track, rate_idx;
- `ui_snd_t.keys`/`kst[27]`, `ui_fx_t.keys_down`, `ui_keys_t.step`;
- `UI_SND_FRAME_*`, ctl_mfx.c:196-202, ctl.c:1015-1016, view_labels.c:214-223.

### Stage 2: the shared layers, same behaviour (medium risk), about 1.5–2 KB

| # | change | saves |
|---|---|---|
| 2.1 | `qd_sel_*` (mask, find, val, finger, follow core) replaces the copies in ctl_step, ctl_mix, ctl_mfx, ctl_tools, ctl_leds and view | ~450–550 B |
| 2.2 | `qd_mix_value()` (6 copies, player.c included) | ~120 B |
| 2.3 | `qd_page_open()` replaces the nine page-opening preambles; each page's current differences are kept as flags until the owner decides | ~100–130 B |
| 2.4 | A takeover lifetime table plus `qd_ctl_tk_close()`; `oct_ms` merged into `tk_ms` | ~60–100 B |
| 2.5 | `arm2()`, `clear_track()`, `hist_paste()`, `play_key()`; b_oct's octave change written once | ~250 B |
| 2.6 | `ui_row_t` and `qd_view_row()`; `fill_pages()` | ~250 B |
| 2.7 | `qd_key_st()` for the LEDs and the screens | ~150–200 B |
| 2.8 | `qd_tools_knob` as a parameter table | ~300–400 B |
| 2.9 | The browser's relist/apply pairs, preview `drop` as one array of structs | ~250 B |

### Stage 3: the owner's decisions, where behaviour or pixels change

These are the per-mode differences the review found. My recommendation for each is in **bold**. Each one, once decided, also removes a special case.

Selection and knobs:
1. **ALGORITHM on the mix** selects the track; everywhere else it is the step cursor. → **the step cursor everywhere** (the track stays on SELECT / FX + ALGORITHM).
2. **The keyboard page with steps selected:** KNOB 1..4 still edit the track live; the step views lock the selected steps. → **lock, as the step views do.**
3. **A short W press (under 250 ms)** takes locks on the mix and the effects at once, but not in the sound editor. → **the sound editor's rule everywhere.**
4. **LFO on** brings back the last selection on the step views, the effects and the sound editor, but starts empty on the mix and MIDI FX. Also, HOME, a mode change, SET or REC lose the saved selection. → **restore everywhere; every latch end saves it.**
5. **The sound editor's step row** shows the selected step's page; the other rows show the page shown. → **the page shown.**
6. **SEQ held + ALGORITHM** sets the launch mode on EDIT, MIDI FX and TOOLS, but turns the page's knob on MIX and FX. → **the launch mode everywhere.**

Clearing:

7. **Clearing locks.** The sound editor does nothing when there are none; the mix and the effects always confirm. The subtitles differ, and B11 lights for "has locks" only in the sound editor. → **the sound editor's way everywhere, one subtitle.**
8. **Clearing a track.** The messages are "TRACK CLEARED" (F2+B9) and "CLEARED" (SHIFT+B11). Only the clear arm times out (2 s); DELETE?/OUT?, the MIDI FX remove and the project copy never do. → **one message, every arm times out after 2 s.**

Pages:

9. **A page key pressed again:** FX, MIX, SHIFT+SEL, SHIFT+REC and SHIFT+SAVE go to the next page; EDIT, ARP and SEQ (TOOLS) close the page. → **keep as is** (pages vs. modes). Decide only if it bothers you.
10. **The step lock and a running takeover when a page opens.** Only REC settings and HOME settings end the lock. Some pages clear the takeover, others leave it up. → **every page keeps the lock and clears the takeover.**

Screens (pixels move; render the mock-ups first):

11. **The step frame** goes missing in MIDI FX while ALGORITHM is turned or a slot is held, and on the keys page during LIVE recording. It is drawn under the content on three screens and over it on four. → **always shown when a step is selected, drawn last.**
12. **KNOB 1..4's foot cells** sit on four different grids (the sound editor, MIDI FX, the effects, the knob band's automation mark). → **one grid, the sound editor's.**
13. **The 16-step row** has seven geometries, and the orange W keys' foot never lines up with any step row. → **one geometry (`ui_page_row`'s), the W foot on the same columns.**
14. **Selected steps blink** at `QD_FAST_MS` in the step view and `QD_BLINK_MS` in MIDI FX; TOOLS blinks in the opposite phase. → **one rate.**

Keys and LEDs:

15. **MIDI FX's W keys** ignore a key held (so it can stay dark with an ARP). The B11 step keyboard's black keys never light for a sounding note. In the sound editor's FULL keys, B9 and B10 are labelled DICE and NEW SOUND but lit as notes. → **one key state (`qd_key_st`) everywhere.**

Possible bug:

16. **`ksteps` goes stale in STEP mode.** `w_shift_white` (ctl.c:496) reads it there. Sequence: keyboard page with B11 off → STEP view → B11 click; SHIFT + W is then not taken by the step keyboard. → **fix: `qd_w_steps()`.**

### Stage 4: back to -Os

Each package's -Oz set comes back out of `firmware/build.py` as room allows. This is the order of value: frame time first.
1. `ui_turn.o` (+492 B on USB): the takeovers and the knob band, drawn often.
2. `ui_layer.o`'s hot screens. Rather than the whole unit (+7 KB), the unit goes back to -Os and the cold screens get a `FZ_COLD` attribute (`__attribute__((minsize))`: what -Oz is, function by function). The cold screens are the menus, Geek Out, TOOLS, the browser, the typewriter, the projects, SHIFT, import and the preset takeovers.
   - That costs about 3.9 KB (the sound editor, MIDI FX and the dispatcher at -Os).
   - It still needs a check that the toolchain's clang 4.0.1 honours `minsize` per function. A first small test showed no difference, so it must be measured on the real unit.
3. `ui_octave.o` (+82 B) and `ui_idle.o` (+1,352 B).
4. The control units stay at -Oz: they don't draw.

On Bluetooth, ui_layer.o lives in the second blob, so its savings land there (20 B free now).
`BLE_X2_FN` (`qd_view_ov`, `fill_fx`, `pr_fill`, `fill_shift` in the blob) can move back into the slot only when the slot has room. They are main-loop models, so they run equally fast from either place. Moving them back only matters for balancing.

Stages 1 + 2 should free about 4.5–5.5 KB per package. That covers stage 4's items 1, 3 and most likely 2; the exact numbers follow each step.

## 5. How each step is checked

- `make -C tools build/test_ctl && tools/build/test_ctl`, and the dev-BLE profile's test_ctl. Their determinism line hashes the model and the samples of a whole session, so it must not change in stages 1 and 2.
- Every screen rendered by `tools/build/render_frames` and compared with the baseline taken before stage 1. Stages 1 and 2 must be pixel-identical.
- All three packages built, with each one's free bytes recorded below.
- Before main: `make -C tools test`.

## 6. Flash free, step by step

| step | USB | Bluetooth (slot / blob 2) | single-core |
|---|---|---|---|
| before (fb1eb96) | 32 | 80 / 20 | 20 |
