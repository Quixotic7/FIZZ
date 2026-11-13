A. Step selection and value resolution: review (read-only)

All paths are under core/app/ unless they say otherwise. The byte figures assume about 3-4 B for each simple statement at -Os.

FINDINGS

1. Building the 16-bit page mask from held[] (7 copies)
   Copies: ctl_step.c:268-271, view.c:535-537, view.c:925-927, view.c:1683-1685, ctl_leds.c:116-120, ctl_mfx.c:526-527 (a per-step loop over held), ctl_tools.c:623-630 (the same, plus the ROTATE remap).
   Difference: view.c:1679 (the sound editor's head) takes the page from held[0]. Every other copy uses the page shown. FLAG: the row can show a different page from the B11 row and the keyboard page.
   Helper: uint16_t qd_sel_mask(const qd_ui_t *u, int page).
   Saves about 25 statements, roughly 80-110 B. Risk: low.

2. "Is step s selected?" (5 copies)
   Copies: ctl_step.c:123 held_find (static), ctl_preview.c:108-115 is_held, view.c:146-147, view.c:183-184, ctl_mfx.c:526-527.
   Helper: export held_find as int qd_sel_find(const qd_ui_t *u, int s).
   Saves about 40 B. Risk: low.

3. "Selected step's value: its lock, else its base" (8 copies)
   Copies: ctl_snd.c:81-89 snd_now, ctl_snd.c:631-641 snd_cur, ctl_snd.c:604-605, ctl_step.c:794, ctl_step.c:811, ctl_rec.c:434, ctl_mix.c:130, ctl.c:2580-2581.
   Differences:
   - snd_cur has an auto-take branch. snd_now has none, and it drives the labels and the filter machine (ctl_snd.c:332, 377, 382, 761). FLAG: while automation records, the editor's labels and machine read the track value, not the take.
   - ctl.c:2580 ignores the follow step. qd_fx_send_shown (ctl_mix.c:124) uses it. The comment says this is on purpose.
   Helpers:
   - int qd_step_val(const qd_app_t *a, int s, int id), meaning the lock, else qd_step_base.
   - int qd_sel_val(const qd_app_t *a, int id), meaning nheld ? qd_step_val(held[0]) : auto ? take : track.
   Saves about 70-100 B. Risk: low. Merging snd_now into snd_cur is a behaviour change, so it is medium.

4. Turning a value: selection gets locks, else the auto take, else a live edit (7 copies)
   Copies: ctl_snd.c:147-156 (MIDI sound), ctl_snd.c:599-627 (snd_pair), ctl_snd.c:643-658 (snd_put), ctl_snd.c:897-910, ctl_mix.c:316-325, ctl_mix.c:389-397, plus ctl_step.c:790-819.
   lock_all and qd_step_lock_fine are one loop. They differ only in turn(d) against d, and in whether they stop at the first EFULL.
   The NO ROOM confirm is written out 5 times (ctl_snd.c:609, ctl_snd.c:651, ctl_step.c:800, ctl_step.c:813, ctl_rec.c:457). ctl.c:690 already has a static no_room().
   FLAG: the mix and the effects call qd_step_promote_all first (ctl_mix.c:316, 390), so a W pressed for less than 250 ms already takes locks. The sound editor does not do this.
   Helpers:
   - int qd_sel_turn(qd_app_t *a, int id, int d), which returns 1 when the turn went to locks or the take.
   - one lock_add(a, id, dd).
   - export qd_ctl_no_room.
   Saves about 150-250 B. Risk: medium, because the takeover calls differ per page.

5. Counting and clearing the selection's locks on a page's ids, with the RATIO B pair (7 copies)
   Copies: ctl_snd.c:183-187, ctl_snd.c:533-542 (count), ctl_snd.c:546-552 (clear), ctl_snd.c:932-939 (LED), ctl_step.c:312-318 (mix and effects), ctl.c:2400-2416 (k_unlock), ctl_step.c:983-1000 (all locks).
   FLAG, four inconsistencies:
   - The sound editor counts first and does nothing when there are no locks. The mix and the effects always call clip_keep and show the confirm.
   - The confirm subtitles differ: "F1 + B11 undoes it" against none.
   - With nothing selected, F2 + knob (k_unlock) clears every step of the track, but B11 + F2 does nothing.
   - The B11 LED shows "has locks" in the sound editor but never in qd_panel_leds (mix and effects).
   Helper: int qd_sel_locks(qd_app_t *a, const int8_t *ids, int n, int clear). It handles the RB1/RB2 pair and returns the count.
   Saves about 150-200 B. Risk: low-medium.

6. Mix lock id to mix field (6 copies), and a duplicated knob table
   Copies: ctl_mix.c:323-324, ctl_step.c:777-779, view.c:2201-2205 (write), view.c:2210-2212, ../seq/player.c:696-697, and qd_fx_send ctl_mix.c:60-64.
   The static ID[2][4] table is duplicated exactly at ctl_mix.c:292-293 and ctl_mix.c:331-332. qd_mix_knob could call qd_panel_lock_id.
   Helper: int qd_mix_value(const qd_mix_track_t *m, int id), placed in voice/mix.h.
   Saves about 120 B, 16 B of it the table. Risk: low.

7. The "following step" rule, which the comments call "the one rule" (4 copies)
   Copies: view.c:1566-1574 qd_view_edit_follows, ctl_mix.c:116-123 qd_fx_sends_follows, view.c:1698 (inline in the sound editor), ctl_rec.c:582-586 mk_res.
   FLAG: the guards differ.
   - wk_pending is checked in the first two only.
   - keys_held is checked in the sends copy only.
   - mk_res ignores slatch and uses qd_seq_track_step (the step now) instead of fol_step (the step heard).
   Helper: int qd_sel_follow(const qd_app_t *a) for the shared core (playing, fol_t==sel, fol_step>=0, !nheld, !wk_pending, !slatch). Callers add their page's own guards.
   Saves about 50 B. Risk: low-medium.

8. "Finger on a step" predicates (5 forms)
   Forms: ctl.c:52 steps_held, ctl.c:55-59 finger_held (static), view_labels.c:253 (no wk_pending), ctl.c:2175 when_cursor (!wk_down && (slatch||!nheld), no wk_pending), and ctl.c:1736 and ctl.c:1804 ((slatch||!nheld)).
   FLAG: for the first 250 ms of a W press, ALGO is still the cursor (when_cursor) while the F layers count it as a finger.
   Helper: export qd_ctl_finger() and use it everywhere.
   Saves about 40 B. Risk: low.

9. The idiom "selection emptied, its step band dropped" (6 copies)
   Copies: ctl_step.c:178-182, 221-223, 253-256, 288-292, 457-461, 476-479.
   FLAG: only some copies reset note_keys. The cursor, cursor_page and B11 copies keep a stale NOTE-entry chord.
   Helper: static void sel_clear(qd_ui_t *u).
   Saves about 70 B. Risk: low.

10. Ending the latch without qd_step_latch(a,0) (7 sites)
    Sites: ctl.c:347-348 (set_mode), ctl.c:1405-1406, ctl.c:1412-1413, ctl.c:1628-1629, ctl_mfx.c:335-336, and ctl.c:1329 and ctl.c:1882 (slatch=0 only).
    FLAG: only qd_step_latch saves lsel_mask. So LFO restores the selection after LFO-off or B11+F1, but not after HOME, a mode change, SET, REC or MIDI FX keys.
    ctl.c:1329 and ctl.c:1882 are most likely dead: set_mode from KEYS has already cleared the latch. Check that qd_fx_button and qd_snd_button never set it.
    ctl.c:1015-1016 is dead: qd_step_latch has already released everything.
    FLAG: LFO-on restores the last selection on the step views, the effects and the sound editor, but starts empty on the mix and MIDI FX.
    The seeding rules differ by site: seed from last_step (track change, ctl.c:335), restore from lsel (LFO), page start (ALGO), pstart (SELECT), fresh (B11).
    Helper: void qd_step_latch_end(qd_app_t *a). Saves about 30 B. Risk: medium (behaviour).

11. Preamble for opening a page over the step view (5 copies)
    Copies: ctl_lib.c:27-33, ctl_mfx.c:107-112, ctl_tools.c:368-372, ctl_presets.c:167-172, ctl_presets.c:590-594.
    The preamble is: STEP mode, release unless latched, tk=NONE, latch_open.
    Helper: void qd_ctl_open_over(qd_app_t *a, int latch, int keep_keys). The MIDI FX screen allows KEYS.
    Saves about 60-80 B. Risk: low.
    FLAG: qd_sv_open (ctl_presets.c:586) takes the source step only with slatch, so a step held by a finger is ignored.

12. ekeys and ksteps: one concept, two flags
    ksteps is set only at ctl.c:353 and ctl.c:1000, always to !ekeys on the keyboard page. In STEP mode it goes stale.
    POSSIBLE BUG: w_shift_white (ctl.c:496) reads ksteps in STEP mode. After the keyboard page (ekeys 0) → STEP → a B11 click (ekeys 1), ksteps is still 1, so SHIFT + W is not swallowed on the step keyboard.
    br_back_to (ctl_presets.c:220-226) restores ekeys only for SND, FX and MIX.
    Proposal: drop ksteps and add int qd_w_steps(a) = mode==KEYS ? !ekeys : !qd_keys_on(a).
    Saves about 30 B plus 1 B of RAM. Risk: low-medium.

13. MIDI FX keeps a private W selection, mfx_wheld
    Where: ctl_mfx.c:296-317 and ctl_mfx.c:343-363. The mix and the effects use held[] instead (qd_step_white_up_hold).
    The loop that sets the steps' MIDI FX is duplicated at ctl_mfx.c:196-202, ctl_mfx.c:428-437 and ctl_step.c:301-302. At ctl_mfx.c:196-202 the else branch is dead, because the outer condition requires mfx_wheld.
    mf_lane_sel (ctl_mfx.c:57) reads held[0], but qd_mfx_vgroup reads mfx_wheld first.
    Helper: static void mf_set_steps(a, m). Saves about 50 B, risk low.
    Folding mfx_wheld into held[] would save 150+ B, but the risk is high.

14. Knob roles per screen (ALGORITHM and PRESETS)
    - Step views, no finger (ctl.c:2178-2203): ALGO the cursor, PRESETS the browser. With a selection, PRESETS is the sound lock.
    - Keyboard page (ctl.c:2031-2049): ALGO the cursor. FLAG: KNOB 1..4 stay live track edits (FREQ...) even with a selection, where the step views lock. when_auto (ctl.c:2125-2128) also records over a keyboard selection.
    - Sound editor (ctl_snd.c:838-844): ALGO the cursor. PRESETS is the fifth value, else KNOB 1, never the preset.
    - Effects (ctl.c:2226-2236, ctl_mix.c:364-368): ALGO the cursor. PRESETS is the sound lock or the browser. FLAG: this differs from the sound editor.
    - Mix (ctl_mix.c:302-305): FLAG: ALGO selects the TRACK, against "ALGO selects steps everywhere". PRESETS is mute.
    - MIDI FX (ctl_mfx.c:418-421): ALGO the cursor.
    Proposal: one KNOB_ROWS row before the mix row, {when_algo_cursor, k_algo_cursor}. Its guard: no finger, no fx_bheld, no mfx_bheld, no shift. It replaces 3 ALGO branches (about 40 B).
    qd_ctl_knob_role ignores a (ctl.c:2152-2156), so call qd_ctl_layout_role directly (about 10 B).
    k_stepview's SELECT, ALGO and PRESETS (ctl.c:1997-2009) look almost unreachable in STEP mode: when_cursor and k_held come first. Verify.

PROPOSAL: ONE SELECTION LAYER (ctl_sel.c, or the top of ctl_step.c)

    typedef struct {
        int8_t  track;    /* a->sel */
        int16_t first;    /* held[0], or the mfx_wheld first, or -1 */
        uint8_t n;        /* nheld */
        uint8_t src;      /* QD_SRC_NONE, FINGER (wk/nheld && !slatch), LATCH, CURSOR (scursor), B11 */
        uint8_t page;     /* the page of first, else the page shown */
        uint16_t mask;    /* qd_sel_mask(u, page) */
        int16_t follow;   /* qd_sel_follow, -1 */
    } qd_sel_t;

    void qd_selection(const qd_app_t *a, qd_sel_t *s);   /* filled once per call site, or cached per frame in view.c */
    int  qd_sel_find(const qd_ui_t *u, int s);
    uint16_t qd_sel_mask(const qd_ui_t *u, int page);
    int  qd_ctl_finger(const qd_app_t *a);
    int  qd_step_val(const qd_app_t *a, int s, int id);
    int  qd_sel_val(const qd_app_t *a, int id);          /* selection, else the take, else the track */
    int  qd_sel_turn(qd_app_t *a, int id, int d);        /* 1: locked or taken; else the caller edits live */
    int  qd_sel_locks(qd_app_t *a, const int8_t *ids, int n, int clear);
    void qd_step_latch_end(qd_app_t *a);                 /* the one way off; saves lsel */
    int  qd_w_steps(const qd_app_t *a);                  /* replaces ksteps */

Call sites that switch:
- Selection struct, mask and find: view.c (535, 925, 1679-1697, 2193-2212, 146, 183), ctl_leds.c (116, 302), ctl_mfx.c (57, 343, 526), ctl_mix.c (116-130), ctl_tools.c (623), ctl_preview.c (108).
- Value resolution: ctl_snd.c (snd_now, snd_cur, snd_put, snd_pair, msnd_knob, qd_snd_knob, both LED functions, B11), ctl_mix.c (qd_mix_knob, qd_fx_knob), ctl_step.c (lock_all, lock_fine, b11_reset, clear_locks), ctl.c (k_unlock, 2580, when_cursor, when_auto).
- Latch ends: set_mode, home and the SET/REC menus, qd_mfx_rec.

Total if everything lands: about 900-1300 B. The low-risk subset is 1, 2, 3 (without the snd_now merge), 6, 8, 9, 11 and the no_room export, about 450-550 B, with no behaviour change.

Items 4, 5, 7, 10, 12, 13 and 14 carry the flagged inconsistencies. Each needs the owner to choose which behaviour wins before it is unified.
