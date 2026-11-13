# B: shared screen furniture in core/ui (read-only review, 2026-11-13)

All paths are under FMToneSeq/core/ui. Sizes marked "nm" are measured with arm-none-eabi-nm on tools/build/budget/arm/*.o (today's budget build; it may lag the source a little). Other sizes are estimates.

## 1. Findings

**F1. Copies of the line wrapper; qd_line already exists (largest pixel-safe win)**
- Copies: `line2` ui_layer.c:53, ui_held.c:116, ui_lane.c:51, ui_octave.c:23, ui_turn.c:35. `ln` ui_tracks.c:16, ui_kit.c:230. `tl_line` ui_tools.c:19. `gk_line` ui_geek.c:23.
- Each copy is out of line, 46 B (nm). Together they make about 400 B.
- `qd_line(x0,y0,x1,y1,col,lw)` in gfx/qd_gfx.h:98 has the same signature. The device raster draws it the same way (qd_gfx_fast.c:1610: move, line_to, the same `paint`).
- Fix: `#define line2 qd_line`, or replace the call sites. Saves about 400 B.
- Risk: low. The pixels stay the same; only the hash id changes.

**F2. Copies of ease, clamp and lerp; a duplicated palette**
- `mf_ease` (ui_mfx.c:22) and `lb_ease` (ui_lib.c:11) are 60 B each (nm) and match `ui_ease` (ui_kit.c:31).
- `MF_TP` (ui_mfx.c:16) is `UI_TRACK_PAL` (ui_kit.c:220) value for value.
- `cx16` (ui_layer.c:170) and `col_x` (ui_lane.c:49) are the same formula, 40 B each.
- `cat` exists in 3 units: ui_layer.c:42, ui_held.c:55, ui_keys.c:28 (32–38 B each). ui_mfx.c:1237 uses strcat.
- `three` / `lb_num3` exist in 3 forms: ui_held.c:67, ui_picker.c:17, ui_lib.c:52.
- Fix: export `ui_cat` and `ui_num3` from ui_kit.c, and use the existing ui_ease and UI_TRACK_PAL. Saves about 250 B.
- Risk: low (no pixel change).

**F3. The step frame `ui_RR(0,0,240,240,8,UI_STEP_FRAME,3,QD_NONE)` has 14 copies**
- Copies: ui_mfx.c:1124, 1141, 1162, 1226, 1235; ui_snd.c:1045, 1107; ui_held.c:1005; ui_keys.c:169; ui_tracks.c:126, 699; ui_presets.c:211; app/view.c:2669.
- Geometry is identical everywhere. Two spellings exist: 240.f and (float)QD_W.
- **Differences to flag:**
  - **Z-order.** snd, held and presets draw the frame first, under the content. mfx, keys, mix and fx draw it last.
  - **MIDI FX, step selected.** The frame is missing on the ALGORITHM-turn path (ui_mfx.c:1111-1115) and the slot-roll path (1165-1174): both return before it.
  - **Keys page.** LIVE rec returns early (ui_keys.c:104), so there is no frame there.
  - **EDIT.** view.c:2669 redraws the frame only because `ui_rec_keys_foot(kst,1)` clears a full-width band (ui_kit.c:545) over it. Insetting that clear to x 3..237 / y ..237 would drop the redraw.
- Fix: `void ui_step_frame(int on)`, called last by every screen; or one call in view.c after the screen.
- Saves about 20–26 B a copy, so about 200–300 B. ui_mfx_draw's five tail copies may already be cross-jumped, so less there.
- Risk: low. Pixels change only where the missing frames get fixed.

**F4. KNOB 1..4 foot cells: five different implementations and four column grids**

| Screen | Column centre (cx) | Label | Value | Underline / lock |
|---|---|---|---|---|
| ui_snd.c:1164-1221 | 35.25+56.5k | y193, 600 | 18/500, fixed chip box 52.5×32 | underline y196.5 ±16 |
| sd_midi, ui_snd.c:1062-1091 | same | y189 | 18/300 | underline y192.5; chip = text+14, 26 high |
| mf_foot, ui_mfx.c:442-458 | 31+59.3k | y204, 500 | 19→12/400, fitted by a `while` of up to 7 ui_TW calls a frame | underline y209 ±12, lw 1.6 |
| ui_fx_draw, ui_tracks.c:682-697 | 30+60k | y204, 600 | 16/500 | ui_chip 15 px; underline **below** the value at y235 ±18 |
| ui_mfx_step_draw, ui_mfx.c:1255-1264 | 31 / 90 | y204 | 19/400 | — |

Related: `ui_auto_cue` (ui_layer.c:682) marks columns at 30+60k, which matches only fx. ui_mix puts its lock chips in the head (ui_tracks.c:120-124).

- **Flag:** this is the biggest visual inconsistency (grid, label weight, value size and underline place).
- Proposed helper: `void ui_foot_cell(int k, const char *lab, const char *val, qd_rgb col, unsigned fl /*LAST, LOCKED, LOCKANY, DIM*/, float a)` on one grid (`ui_foot_cx(k)`). Screens keep their extras (snd's mod/LFO glyph, ratio fraction, LFO wave) as pre-draws.
- Saves 300–500 B (fx 5.5 KB and mfx 11 KB functions shrink).
- Risk: medium (pixels move).

**F5. Fitting text to a width is repeated about 14 times**
- The pattern is `w = ui_TW(..); sz = w > room ? sz*room/w : sz; clamp 12; ui_T(..)`.
- Sites: ui_snd.c:1177-1179, 1186-1188, 1190-1192, 1216-1219; ui_tracks.c:690-694; ui_mfx.c:452-454; ui_layer.c:553-556, 743-746, 782-784, 912-915; ui_picker.c:100-102; ui_presets.c:113-115; ui_turn.c:620-627, 638-641; ui_held.c:1107-1111.
- ui_tracks.c:687-688 measures `w` and throws it away (a wasted extern ui_TW call per cell per frame).
- Helper: `float ui_T_fit(const char*s, float x, float y, float sz, int wt, float ls, qd_rgb c, int al, float room)`. Saves about 25–35 B a site, so about 350 B, and speeds up mf_foot.
- Risk: low (same maths).

**F6. Head and page-dot copies**
- Page dots have 7 copies: ui_snd.c:1112-1113, 1046-1047; ui_tracks.c:29-30; ui_mfx.c:278-281; ui_layer.c:600-601, 811-812, 883-884.
- **Flag (dots):**
  - mf_head uses y 13, an unselected UI_G1 ring and an unringed white selected dot. The others use y 14 and UI_G2.
  - panel_head puts the dots *after* the title; the others put them before it.
- The title has two looks: "main" (y 18, ls 0.8, no rule: ui_head, panel_head, held, snd, octave) and "browser" (y 17, ls 1.0, rule at y 25: pr_head, the picker branch of ui_browse_draw, type, prj, mf_head). menu_head and menu_trk put the rule at y 27. ui_mfx_step_draw uses (10,19) with a 10..230 rule at y 27.
- **Flag (title).**
- The back button has 4 copies: ui_layer.c:564-565, 885-886, 326-327 (x 180), and ui_lib.c:48 (`lb_back`, 26 wide, r 3, lw 1.3, against 24 / r 4 / lw 1.2).
- The track box: tl_head draws its own 18 px box (ui_tools.c:154-155) against ui_box's 16 px. Its place varies: (8,5), (214,4), (184,5), or right-aligned in ui_head_track.
- Knob badges (a filled circle with a digit or letter) have 6 copies: ui_layer.c:549-550, 851-852 (inline copy of menu_row_sz's badge), ui_turn.c:600-601, ui_snd.c:1130-1131, 1184-1185, ui_mfx.c:1102-1103.
- Helpers: `float ui_dots(float x,float y,int n,int cur,qd_rgb on)`, `float ui_title(const char*,float x,qd_rgb,int rule)`, `void ui_back_btn(float x)`, `void ui_badge(const char*,float x,float y,float r,qd_rgb)`.
- Saves about 400 B. Risk: low if copied exactly, medium if unified.

**F7. The "STEP n" label is built 6 ways**
- held_head (ui_held.c:1009-1017) and head_value (1095-1103) build the *same* "STEP n+m · FIELD" string twice.
- sd_midi uses cat (ui_snd.c:1050). ui_mfx_step_draw uses strcat (ui_mfx.c:1237). ui_picker.c:53-73 uses hand loops. view.c:1061 uses snprintf. ui_type.c:202 uses ui_itoa into lab+5.
- Helper: `char *ui_step_label(char *o,int step,int more,const char *field)`. Saves about 120 B. Risk: low.

**F8. EDIT builds its head value twice**
- `ed_head_val` (ui_held.c:1118-1135) and the switch in `ui_edit_draw` (1159-1220) build identical value strings. TRIGS (1120) and NAMES (1198) are the same table.
- Fix: keep only ed_head_val, and turn the switch into a dy table {NOTE −34, VEL −30, LEN −56, MICRO −52, RATCH −62, COND −10, CHNC −16, TRIG −14, SOUND −4, FUNC −36, SLIDE −14}, plus nobig/pull/sub for the few special cases.
- Saves about 250–350 B (ui_edit_draw is 1,460 B, nm). Risk: low.

**F9. The page's 16-step row: one helper, seven geometries**

| Drawer | x formula | Cell |
|---|---|---|
| ui_page_row (ui_kit.c:531) | 14+13i+2.7(i/4) | 11×12 |
| steps_row ov_x (ui_lane.c:546) | 20+13i+2(i/4) | 11.5 wide |
| lane col_x (ui_lane.c:49) and ui_layer.c:170 | 9+13.5i+2(i/4) | — |
| held f_sound (ui_held.c:820) | 13+13.5i+2(i/4) | 10×10 |
| held f_func (ui_held.c:955) | 28+12.5i+2(i/4) | 10×16 |
| tools tl_lane (ui_tools.c:178) and mfx mf_lane_cells (ui_mfx.c:939) | 8+14i, no group gaps | 13 wide |
| ui_rec_keys_foot (ui_kit.c:547) | 20+13.3i, no group gaps | 11.2 wide |

- The same ui_page_row is drawn at y 26 (snd), 38 (keys), 6 (EDIT) and 208 (F layers).
- **Flag:** the orange W-key foot does not line up with the step row on any screen.
- Fix: `float ui_step_x(int i)` plus `UI_STEP_W`, used by all seven. f_sound and f_func can call ui_page_row-style drawing.
- Saves about 100–150 B. Risk: medium (columns move).

**F10. Keybeds, cassettes, hexagons and the list bar**
- Keybeds:
  - `mf_keyboard` (ui_mfx.c:685, 428 B nm) re-implements `ui_keybed` (ui_kit.c:161).
  - `piano88` (ui_octave.c:35) re-declares `WI`/`ISB` as int[12] (ui_octave.c:20-21), duplicating ui_kit.c:488-489.
  - The keys grid (ui_keys.c:129-130) and ui_rec_keys_foot (ui_kit.c:546-547) are the same `ui_rec_key` loop: `ui_rec_key_row(n,x0,pitch,y,w,h,st)`.
- Cassettes: cassette (ui_held.c:770), pr_cassette (ui_presets.c:94) and lb_cassette (ui_lib.c:58) each carry the same reel loop. Reel places differ: 0.32/0.68 against 0.3/0.7, and r 9 / ≤12 / ≤13. **Flag.** Helper: `ui_reels(x,y,w,h,r,spin)`.
- Hexagon: f_trig hand-builds a hexagon path (ui_held.c:866-880) instead of calling `ui_mfx_hex`. Its number is 14/700 against r·1.2/600.
- "Where in the list" bar: pr_rollo (ui_presets.c:194-199) and mf_roll (ui_mfx.c:927-931) differ only in y. Helper: `ui_list_bar`.
- Rolls: f_sound (22 px, ui_ease, ui_held.c:792) and pr_rollo (14 px, lb_ease, cassette ×0.3) are the same idea at different strengths. **Flag.** mf_roll is a continuous rolodex and octave has no roll on purpose. A `ui_roll_dy(roll,dir,px)` helper is too small to matter; the reels and the bar are the real duplication.
- Saves about 500 B. Risk: low/medium.

**F11. Dead code and fields**
- `UI_SND_FRAME_TRACK` and `UI_SND_FRAME_STEP` are used nowhere (only ui.h:379, 381). The ui.h:722-725 doc is stale ("orange frames").
- `ui_snd_t.keys` and `kst[27]` (ui.h:766-767) are never written or read.
- `ui_fx_t.keys_down` (ui.h:199) is never touched. `fx.keys`/`kst` are live (view.c:2310-2314, ui_tracks.c:533-534). SENDS could go through view.c's generic foot (view.c:2660) with clear=0 and drop them.
- `ui_layer_t` fields written but never drawn: `shift`, `held_track`, `page_clip`, `step_clip`, `rate`, `rate_idx` (view.c:557-587).
  - Also `page_on[16]`: its fill loop at view.c:573-574 is real code and per-frame work.
  - `clip_step` and `rate_names[9]` are never written.
- `ui_keys_t.step` is written (view.c ~907) but never read.
- `ui_picker.c` as a whole: `ui_picker_draw` has no caller. The linker discards it already (1,456 B, map line 266), so there is no flash win. `ui_picker_t` and the build.py OZ lists still name it.
- `ui_cat.h` is included twice (ui_tracks.c:289, 327).
- Saves about 40 B of code plus RAM. Risk: none.

**F12. Copy-pasted branches**
- sd_mixrow and sd_mixcell (ui_snd.c:78-127) repeat the X/Y carrier loop and the slider (only the coordinates differ). Helper: `sd_mix_slider(x0,x1,y,r,xc,yc,hcol)`.
- The modmark and lfomark branches (ui_snd.c:1174-1188) share 4 of 6 lines.
- menu_set2, menu_home and menu_trk page 2 (ui_layer.c:739-746, 778-784, 906-915) are the same "row + 26 px value fitted to 150" block: `menu_val(k,y,name,val,col)` returning lx.
- ui_mfx_draw's F1/F2 card branches (1129-1164) repeat the arrow+icon tail.
- Saves about 300 B. Risk: low.

## 2. Proposed shared screen-furniture layer (in ui_kit.c, exported in ui.h)

- **Primitives:** `qd_line` (drop every line2/ln copy), `ui_ease`, `ui_cat`, `ui_num3`, `ui_T_fit`, `ui_badge`.
- **Head:** `ui_title(title,x,col,rule)`; `ui_dots(x,y,n,cur,on)`; `ui_back_btn(x)`; `ui_step_label(...)`; one track-identity mark (`ui_box` + kind icon, at one place). Pick one of the two head families: y 18 / ls 0.8 with an optional rule at y 25.
- **Frame:** `ui_step_frame()`, drawn **last** by the caller (view.c) from a single `framed` flag that every view model fills. This fixes the MIDI FX early returns and the EDIT redraw.
- **Step row:** `ui_step_x(i)` / `UI_STEP_W` and `ui_page_row(y,trig,sel,red,play)` for every 16-step strip, including the W-key foot (`ui_rec_key_row` on the same columns). Pick one y per context: the head row at y 26 and the foot at y 184/208.
- **Foot:** `ui_foot_cx(k)` and `ui_foot_cell(k,lab,val,col,flags,a)`: label y 204/12-600, value 18 fitted, underline under the label, a lock as `ui_chip`. snd, MIDI, mfx, fx, mfx-step and auto_cue use it.
- **Keys:** `ui_keybed` (mfx and octave on its tables), `ui_rec_key_row`.
- **Lists:** `ui_reels`, `ui_list_bar`.

Rough total: about 2.5–3.5 KB, of which about 1.2 KB is pixel-identical (F1, F2, F3, F5, F7, F8, F11). That could be enough to put ui_held.o, ui_tracks.o and ui_kit.o back at -Os.
