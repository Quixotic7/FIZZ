/* SPDX-License-Identifier: GPL-3.0-only */
/* The controller's dispatch (ctl.h): the modes, the buttons' table, the keys' tables, the knobs' table, the layers,
 * the timers.
 *
 * A key, button or knob goes through one table: the first row whose condition holds takes it. A key's release goes
 * to the row that took its press (a page key pressed with F1 held is a page key until it is released, whatever is let
 * go of first). The F layers (OCT- = F1, OCT+ = F2, both = F3) are read at each press: none while steps are held (as
 * in Form: OCT- / OCT+ are then NOTE's octave and SOUND's bank). */
#include <string.h>

#include "app.h"
#include "ui.h"
#include "../seq/scale.h"
#include "../seq/sounds.h"

const uint8_t QD_WHITE_KEY[16] = {0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26};
const uint8_t QD_BLACK_KEY[11] = {1, 3, 5, 8, 10, 13, 15, 17, 20, 22, 25};   /* F#3 .. F#5 */

int qd_ctl_white(int key)
{
    int i;
    for (i = 0; i < 16; i++)
        if (QD_WHITE_KEY[i] == key)
            return i;
    return -1;
}

int qd_ctl_black(int key)
{
    int i;
    for (i = 0; i < 11; i++)
        if (QD_BLACK_KEY[i] == key)
            return i;
    return -1;
}

static int held(const qd_app_t *a, int b) { return (int)((a->buttons_held >> b) & 1u); }
int qd_ctl_shift(const qd_app_t *a) { return held(a, QD_B_FUNC); }
qd_pattern_t *qd_ctl_pat(qd_app_t *a) { return &a->proj.pat[a->seq.cur]; }
const qd_pattern_t *qd_ctl_cpat(const qd_app_t *a) { return &a->proj.pat[a->seq.cur]; }
static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

static int steps_held(const qd_app_t *a) { return a->ui.nheld > 0 || a->ui.wk_pending; }

/* P1.3d: F3 is a latched menu (ctl_latch.c): open, it is the layer whatever is held; an OCT key a latch ate is no
 * layer until it is let go */
int qd_ctl_layer(const qd_app_t *a)
{
    const int dn = held(a, QD_B_OCTDN) && !(a->ui.oct_eaten & 1u), up = held(a, QD_B_OCTUP) && !(a->ui.oct_eaten & 2u);
    if (a->ui.mode != QD_MODE_STEP)
        return QD_LAYER_NONE;
    if (a->ui.latch == QD_LATCH_F3)
        return QD_LAYER_F3;
    if (steps_held(a) || (!dn && !up))
        return QD_LAYER_NONE;
    if (dn && up)
        return QD_LAYER_F3;
    return dn ? QD_LAYER_F1 : QD_LAYER_F2;
}

/* P1.3d (§8b 2.2): the knobs' jobs in the hold modes, [mode][SELECT, KNOB 1..4]. With a step held SELECT and KNOB 1
 * are the main value (NOTE: SELECT note 1, KNOB 1..4 notes 2..5), KNOB 2 the mode's extra; with nothing held KNOB 1..4
 * the same on the track's defaults (SELECT the page). A knob without a job does nothing. P-LOCK has no row: its knobs
 * are the screen's p-locks (held) and the track's sound (nothing held) */
static const uint8_t KNOB_JOB[UI_NFIELD][5] = {         /* P1.3e: KNOB 4 the quiet main value where it is free */
    [UI_F_NOTE] = {QD_KJ_NOTE1, QD_KJ_NOTE2, QD_KJ_NOTE3, QD_KJ_NOTE4, QD_KJ_NOTE5},
    [UI_F_VEL] = {QD_KJ_MAIN, QD_KJ_MAIN, 0, 0, QD_KJ_QUIET},
    [UI_F_LEN] = {QD_KJ_MAIN, QD_KJ_MAIN, QD_KJ_TIE, 0, QD_KJ_QUIET},
    [UI_F_MICRO] = {QD_KJ_MAIN, QD_KJ_MAIN, QD_KJ_STRUM, 0, QD_KJ_QUIET},
    [UI_F_RATCH] = {QD_KJ_MAIN, QD_KJ_MAIN, QD_KJ_RAMP, 0, QD_KJ_QUIET},
    [UI_F_CHNC] = {QD_KJ_MAIN, QD_KJ_MAIN, QD_KJ_COND, 0, QD_KJ_QUIET},
    [UI_F_COND] = {QD_KJ_MAIN, QD_KJ_MAIN, QD_KJ_CHANCE, 0, QD_KJ_QUIET},
    [UI_F_TRIG] = {QD_KJ_MAIN, QD_KJ_MAIN, 0, 0, QD_KJ_QUIET},
    [UI_F_SOUND] = {QD_KJ_MAIN, QD_KJ_MAIN, 0, 0, QD_KJ_QUIET},
};
/* P1.3e: a mode key held with nothing else held edits the track's default of that mode */
int qd_ctl_mkey(const qd_app_t *a)
{
    const qd_ui_t *u = &a->ui;
    if (u->mode != QD_MODE_STEP || u->mkey < 0 || u->nheld || u->wk_pending || qd_ctl_layer(a) != QD_LAYER_NONE)
        return -1;
    return u->mkey;
}
int qd_ctl_mkey_shown(const qd_app_t *a)
{
    return qd_ctl_mkey(a) >= 0 && (a->ui.mkey_used || a->ms - a->ui.mkey_t0 >= QD_TAP_MS);
}

int qd_ctl_knob_job(int mode, int knob)
{
    const int i = knob == QD_K_SELECT ? 0 : knob >= QD_K_1 && knob <= QD_K_4 ? knob - QD_K_1 + 1 : -1;
    if (mode < 0 || mode >= UI_NFIELD || i < 0)
        return QD_KJ_NONE;
    return KNOB_JOB[mode][i];
}

int qd_ctl_layer_shown(const qd_app_t *a)
{
    const int l = qd_ctl_layer(a);
    if (l == QD_LAYER_F1 || l == QD_LAYER_F2)          /* (alone a moment: not the first of a chord) */
        return a->ms - a->ui.f_ms >= QD_CHORD_MS ? l : QD_LAYER_NONE;
    return l;
}

void qd_ctl_init(qd_app_t *a)
{
    qd_ui_t *u = &a->ui;
    int i;
    memset(u, 0, sizeof *u);
    u->mode = QD_MODE_IDLE;            /* power-on: the idle screen (any key or button: the step view) */
    u->follow = 1;
    u->hmode = UI_F_NOTE;
    u->tk = QD_TK_NONE;
    u->rec_step = -1;
    u->entry_oct = QD_ENTRY_OCT;
    u->f3_anchor = -1;
    u->pg_copied = -1;
    u->f2_last_w = -1;
    u->pv_note = -1;
    u->mkey = -1;
    u->seq_bank = QD_PAT_START / 16;
    for (i = 0; i < 8; i++)
        u->last_step[i] = -1;
    qd_clip_init(&u->clip);
    for (i = 0; i < 8; i++)
        u->play_page[i] = -1;
    for (i = 0; i < 128; i++)
        u->rec_t0[i] = -1;
}

/* the tempo set (F3 + ALGORITHM): the current pattern's, the sequencer's, the sound's */
void qd_ctl_set_tempo(qd_app_t *a, int bpm)
{
    bpm = bpm < QD_BPM_MIN ? QD_BPM_MIN : bpm > QD_BPM_MAX ? QD_BPM_MAX : bpm;
    qd_ctl_pat(a)->bpm = (uint16_t)bpm;
    qd_seq_tempo_mbpm(&a->seq, bpm * 1000);
    qd_app_tempo(a, bpm);
}

void qd_ctl_takeover(qd_app_t *a, int tk, int knob, int dir, int lock, int step)
{
    qd_ui_t *u = &a->ui;
    u->tk = tk;
    u->tk_ms = a->ms;
    u->tk_dir = dir > 0 ? 1 : -1;
    u->tk_knob = knob;
    u->tk_lock = lock;
    u->tk_step = step;
}

void qd_ctl_confirm(qd_app_t *a, const char *big, const char *small)
{
    a->ui.cf_big = big;
    a->ui.cf_small = small;
    a->ui.cf_ms = a->ms;
    a->ui.cf_on = 1;
}

/* the page edited on the selected track (a change slides the lane in) */
void qd_ctl_set_page(qd_app_t *a, int p)
{
    qd_ui_t *u = &a->ui;
    p = clampi(p, 0, QD_NPAGE - 1);
    if (p == u->page[a->sel])
        return;
    u->slide_ms = a->ms;
    u->slide_dir = p > u->page[a->sel] ? 1 : -1;
    u->slide_from = u->page[a->sel];
    u->page[a->sel] = p;
}

int qd_ctl_playing_page(const qd_app_t *a, int t)
{
    const int s = qd_seq_track_step(&a->seq, t);
    return s < 0 ? -1 : s / QD_PAGE;
}

/* P1.3c: the octave (or SOUND's bank) changed: its takeover for a second */
void qd_ctl_octave(qd_app_t *a, int kind, int from, int to)
{
    qd_ui_t *u = &a->ui;
    u->oct_kind = kind;
    u->oct_from = from;
    u->oct_to = to;
    u->oct_ms = a->ms;
    qd_ctl_takeover(a, UI_TK_OCTAVE, 0, to >= from ? 1 : -1, 0, -1);
}

int qd_ctl_oct_offset(const qd_app_t *a, int *shown)
{
    const qd_ui_t *u = &a->ui;
    *shown = 1;
    if (u->mode == QD_MODE_KEYS)
        return a->oct;                                  /* the keyboard: 0 is the centre */
    if (u->mode == QD_MODE_STEP && u->nheld && u->hmode == UI_F_NOTE)
        return u->entry_oct - QD_ENTRY_OCT;             /* NOTE entry: octave 4 is the centre */
    if (u->mode == QD_MODE_STEP && u->nheld && u->hmode == UI_F_SOUND)
        return u->sound_bank;                           /* SOUND: the first bank is the centre */
    *shown = 0;
    return 0;
}

uint32_t qd_ctl_oct_blink_ms(int off)
{
    off = off < 0 ? -off : off;
    return off == 0 ? 0u : off == 1 ? QD_OCT_BLINK1_MS : off == 2 ? QD_OCT_BLINK2_MS : QD_OCT_BLINK3_MS;
}

/* a note turned on a knob: it sounds a moment (the one before ends) */
void qd_ctl_preview(qd_app_t *a, int note, int vel)
{
    qd_ui_t *u = &a->ui;
    if (u->pv_note >= 0)
        qd_player_key_off(&a->player, u->pv_track, u->pv_note);
    u->pv_note = -1;
    if (note < 1 || note > 127)
        return;
    qd_player_key_on(&a->player, a->sel, note, vel < 1 ? 1 : vel > 127 ? 127 : vel);
    u->pv_note = note;
    u->pv_track = a->sel;
    u->pv_ms = a->ms;
}

void qd_ctl_select_track(qd_app_t *a, int t)
{
    if (t == a->sel || t < 0 || t >= QD_NTRACK)
        return;
    qd_step_release_all(a);
    a->ui.f2_loaded = a->ui.f2_holding = 0;             /* (Form's: the pick-up / drop hand is per track) */
    a->sel = t;
    if (a->ui.follow && a->seq.playing) {               /* following: the new track's playing page at once */
        a->ui.page[t] = qd_ctl_playing_page(a, t);
        a->ui.play_page[t] = a->ui.page[t];
    }
    if (a->ui.slatch)                                   /* the step latch: the new track's selection */
        qd_step_latch_seed(a);
}
static void select_track(qd_app_t *a, int t) { qd_ctl_select_track(a, t); }

static void set_mode(qd_app_t *a, int mode)
{
    qd_ui_t *u = &a->ui;
    if (u->mode == QD_MODE_KEYS && mode != QD_MODE_KEYS && u->rec)
        qd_rec_toggle(a);                               /* leaving the keyboard: the recording ends */
    if (mode != QD_MODE_STEP) {
        qd_latch_close(a);                              /* (P1.3d: F3's latch and the step latch end with it) */
        u->slatch = 0;
        qd_step_release_all(a);
    }
    u->mode = mode;
}

/* any input: the idle clock restarts; from the idle screen, the step view (1: it woke) */
static int touch(qd_app_t *a)
{
    a->ui.touch_ms = a->ms;
    if (held(a, QD_B_FUNC))
        a->ui.shift_used = 1;
    if (a->ui.mode == QD_MODE_IDLE) {
        a->ui.mode = QD_MODE_STEP;
        return 1;
    }
    return 0;
}

/* ================================================================ the keys ===
 * A row: when its condition holds at the press, its handler takes the press and, later, the release. */
typedef int (*qd_when_fn)(const qd_app_t *a);
typedef void (*qd_key_fn)(qd_app_t *a, int key, int idx, int down, int vel);   /* idx: white 0..15 / black 0..10 */

static int w_seq_held(const qd_app_t *a) { return held(a, QD_B_MIX); }        /* printed SEQ: the patterns */
static int w_sel_held(const qd_app_t *a) { return held(a, QD_B_PATTERN); }    /* printed SEL: the tracks */
static int w_tracks_held(const qd_app_t *a) { return held(a, QD_B_TRACKS); }
static int w_keys_mode(const qd_app_t *a) { return a->ui.mode == QD_MODE_KEYS; }
static int w_tracks_screen(const qd_app_t *a) { return a->ui.mode == QD_MODE_TRACKS; }
static int w_page_held(const qd_app_t *a) { return a->ui.mode == QD_MODE_STEP && held(a, QD_B_PAGE); }
static int w_step_view(const qd_app_t *a) { return a->ui.mode == QD_MODE_STEP; }
static int w_steps_held(const qd_app_t *a) { return a->ui.mode == QD_MODE_STEP && steps_held(a); }
static int w_f1(const qd_app_t *a) { return qd_ctl_layer(a) == QD_LAYER_F1; }
static int w_f2(const qd_app_t *a) { return qd_ctl_layer(a) == QD_LAYER_F2; }
static int w_f3(const qd_app_t *a) { return qd_ctl_layer(a) == QD_LAYER_F3; }

/* SEQ held + white key n: pattern n of the bank shown (P1.3d: SEQ + B1..B4 = bank A..D), queued by the launch mode
 * at the press (stopped: at once); held QD_SEQ_HOLD_MS: at once (sel_seq_timers) */
static void k_pattern(qd_app_t *a, int key, int w, int down, int vel)
{
    qd_ui_t *u = &a->ui;
    const uint16_t bit = (uint16_t)(1u << w);
    (void)key, (void)vel;
    if (w < 0)
        return;
    if (!down) {
        u->seq_down &= (uint16_t)~bit;
        return;
    }
    u->seq_down |= bit;
    u->seq_done &= (uint16_t)~bit;
    u->seq_t0[w] = a->ms;
    u->seq_pat[w] = (uint8_t)(u->seq_bank * 16 + w);
    qd_seq_select_at(&a->seq, u->seq_pat[w], a->proj.launch);
    if (!a->seq.playing)
        qd_app_tempo(a, qd_seq_bpm(&a->seq));
}

/* TRACKS held + white key 1..8: that track */
static void k_track_select(qd_app_t *a, int key, int w, int down, int vel)
{
    (void)key, (void)vel;
    a->ui.tracks_tap = 0;
    if (down && w >= 0 && w < QD_NTRACK)
        select_track(a, w);
}

/* SEL held + W1..W8: a tap mutes / unmutes track 1..8 (at the release), held QD_SEL_HOLD_MS solos / unsolos it
 * (sel_seq_timers); W9..W16 nothing */
static void k_sel_white(qd_app_t *a, int key, int w, int down, int vel)
{
    qd_ui_t *u = &a->ui;
    const uint16_t bit = (uint16_t)(1u << w);
    (void)key, (void)vel;
    if (w >= QD_NTRACK)
        return;
    if (down) {
        u->sel_down |= bit;
        u->sel_done &= (uint16_t)~bit;
        u->sel_t0[w] = a->ms;
        return;
    }
    if (!(u->sel_down & bit))
        return;
    u->sel_down &= (uint16_t)~bit;
    if (!(u->sel_done & bit))
        a->seq.mute ^= (uint8_t)bit;
}

/* SEL held + B1..B8: select tracks 1..8; B11: every mute and solo off */
static void k_sel_black(qd_app_t *a, int key, int b, int down, int vel)
{
    (void)key, (void)vel;
    if (down && b < QD_NTRACK)
        select_track(a, b);
    else if (down && b == 10)
        a->seq.mute = a->seq.solo = 0;
}

/* SEL's and SEQ's holds */
static void sel_seq_timers(qd_app_t *a)
{
    qd_ui_t *u = &a->ui;
    int w;
    for (w = 0; w < 16; w++) {
        const uint16_t bit = (uint16_t)(1u << w);
        if (w < QD_NTRACK && (u->sel_down & bit) && !(u->sel_done & bit) && held(a, QD_B_PATTERN) &&
            a->ms - u->sel_t0[w] >= QD_SEL_HOLD_MS) {
            a->seq.solo ^= (uint8_t)bit;
            u->sel_done |= bit;
        }
        if ((u->seq_down & bit) && !(u->seq_done & bit) && held(a, QD_B_MIX) &&
            a->ms - u->seq_t0[w] >= QD_SEQ_HOLD_MS) {
            qd_seq_select_at(&a->seq, u->seq_pat[w], QD_LAUNCH_NOW);
            u->seq_done |= bit;
        }
    }
}

/* the keyboard: the selected track (and the recording's: ctl_rec.c) */
static void k_note(qd_app_t *a, int key, int idx, int down, int vel)
{
    (void)idx;
    if (down) {
        const int note = qd_rec_key_note(a, key);
        qd_app_note_on(a, key, note, vel);
        qd_rec_key(a, note, 1, vel);
    } else if ((a->keys_struck >> key) & 1u) {
        qd_rec_key(a, a->key_note[key], 0, 0);
    }
}

/* the tracks screen: white 1..8 select, 9..16 mute / unmute tracks 1..8 */
static void k_tracks_screen(qd_app_t *a, int key, int w, int down, int vel)
{
    (void)key, (void)vel;
    if (!down)
        return;
    if (w < QD_NTRACK)
        select_track(a, w);
    else
        a->seq.mute ^= (uint8_t)(1u << (w - QD_NTRACK));
}

/* PAGE held + white key 1..8: edit that page */
static void k_page(qd_app_t *a, int key, int w, int down, int vel)
{
    (void)key, (void)vel;
    if (down && w < QD_NPAGE) {
        a->ui.page_used = 1;
        qd_ctl_set_page(a, w);
    }
}

/* the step view's white keys: the steps (ctl_step.c: tap, hold; with steps held: more held steps) */
static void k_step(qd_app_t *a, int key, int w, int down, int vel)
{
    (void)key, (void)vel;
    if (down)
        qd_step_white_down(a, w);
    else
        qd_step_white_up(a, w);
}

static int step_of(const qd_app_t *a, int w) { return a->ui.page[a->sel] * QD_PAGE + w; }

static void no_room(qd_app_t *a) { qd_ctl_confirm(a, "NO ROOM", "for locks: the pattern has 1024"); }

/* F1 + W (§8b 2.1): the first W of an F1 hold copies its step, every later W pastes it (the hold's edits one undo
 * level). As Form's F1 copy, it loads F2's hand: the next F2 hold's first W drops it */
static void k_f1_white(qd_app_t *a, int key, int w, int down, int vel)
{
    qd_ui_t *u = &a->ui;
    (void)key, (void)vel;
    if (!down)
        return;
    if (!u->f1_copied) {
        qd_clip_copy_step(&u->clip, qd_ctl_pat(a), a->sel, step_of(a, w));
        u->f1_copied = 1;
        u->f2_loaded = u->f2_holding = 1;               /* (Form: "a copy loads the buffer .. the next F2 press pastes") */
    } else if (qd_clip_paste_step(&u->clip, qd_ctl_pat(a), a->seq.cur, a->sel, step_of(a, w)) == QD_EFULL) {
        no_room(a);
    }
}

/* F2 + W: Form's pick-up / drop, exactly (omx_mode_form.cpp, v1.15.4: seqF2Loaded_, seqF2Holding_):
 *   not loaded            CUT that step, even an empty one (an eraser); loaded, holding
 *   loaded, empty step    PASTE (never a cut); empty-handed
 *   loaded, holding, a filled step    PASTE over it; empty-handed
 *   loaded, empty-handed, a filled step    CUT it; holding
 * F2 let go: neither loaded nor holding (b_oct). The hold's edits are one undo level (Form's F2 has none; ours) */
static void k_f2_white(qd_app_t *a, int key, int w, int down, int vel)
{
    qd_ui_t *u = &a->ui;
    qd_pattern_t *p = qd_ctl_pat(a);
    const int s = step_of(a, w);
    int cut;
    (void)key, (void)vel;
    if (!down)
        return;
    if (!u->f2_loaded)
        cut = 1;
    else if (!qd_step_is_on(qd_step(p, a->sel, s)))
        cut = 0;
    else
        cut = !u->f2_holding;
    if (cut) {
        qd_clip_cut_step(&u->clip, p, a->seq.cur, a->sel, s);
        u->f2_loaded = u->f2_holding = 1;
    } else {
        if (qd_clip_paste_step(&u->clip, p, a->seq.cur, a->sel, s) == QD_EFULL)
            no_room(a);
        u->f2_holding = 0;
    }
    u->f2_last_w = w;
    u->f2_last_act = cut ? 1 : 2;
}

/* F3 + W: the page's end (the page joins the loop); W a held + W b: start a .. end b */
static void k_f3_white(qd_app_t *a, int key, int w, int down, int vel)
{
    qd_ui_t *u = &a->ui;
    qd_pattern_t *p = qd_ctl_pat(a);
    qd_tset_t *ts = &p->trk[a->sel];
    const int pg = u->page[a->sel];
    (void)key, (void)vel;
    if (!down) {
        if (w == u->f3_anchor)
            u->f3_anchor = -1;
        return;
    }
    if (u->f3_anchor < 0 || u->f3_anchor == w) {
        u->f3_anchor = w;
        qd_page_set_range(p, a->sel, pg, ts->pstart[pg] > w ? w : ts->pstart[pg], w);
    } else {
        const int s0 = u->f3_anchor < w ? u->f3_anchor : w, s1 = u->f3_anchor < w ? w : u->f3_anchor;
        qd_page_set_range(p, a->sel, pg, s0, s1);
    }
    ts->loop = (uint8_t)(qd_track_loop(ts) | (1u << pg));
}

/* the black keys with steps held: the mode's ten values, CLEAR (ctl_step.c) */
static void k_value(qd_app_t *a, int key, int b, int down, int vel)
{
    (void)key, (void)vel;
    qd_step_value_key(a, b, down);
}

/* F1 + B1..B8 (§8b 2.1's page layer): tap = edit that page; a double tap (the second release within QD_CLICK_MS of
 * the first) = loop it alone; held QD_PAGE_HOLD_MS = the page in / out of the loop
 * (never empty: qd_ctl_ms does it at the time); hold A + press B = the loop A..B (edit A); after B10's copy, a press
 * pastes the page there (and edits it). B9 tap = clear the page; held QD_CLEAR_HOLD_MS = clear the track (qd_ctl_ms).
 * B10 = copy the page edited. B11 = undo / redo */
static void page_toggle(qd_app_t *a, int pg)
{
    qd_tset_t *ts = &qd_ctl_pat(a)->trk[a->sel];
    const int loop = qd_track_loop(ts) ^ (1 << pg);
    if (!loop) {
        qd_ctl_confirm(a, "ONE PAGE", "the loop is never empty");
        return;
    }
    qd_page_set_loop(qd_ctl_pat(a), a->sel, loop);
}

static void k_f1_black(qd_app_t *a, int key, int b, int down, int vel)
{
    qd_ui_t *u = &a->ui;
    qd_pattern_t *p = qd_ctl_pat(a);
    const uint16_t bit = (uint16_t)(1u << b);
    (void)key, (void)vel;
    if (b < QD_NPAGE) {
        if (down) {
            uint16_t pages;
            u->lk_down |= bit;
            u->pg_t0[b] = a->ms;
            u->pg_done &= (uint16_t)~bit;
            if (u->pg_copied >= 0) {                    /* a page copied: paste it here */
                if (qd_clip_paste_page(&u->clip, p, a->seq.cur, a->sel, b) == QD_EFULL)
                    no_room(a);
                qd_ctl_set_page(a, b);
                u->pg_done |= bit;
                return;
            }
            pages = u->lk_down & 0xFFu;
            if (pages & ~bit) {                         /* another page key is down: the loop A..B, edit A */
                int lo = 8, hi = -1, k;
                for (k = 0; k < QD_NPAGE; k++)
                    if ((pages >> k) & 1u) {
                        lo = k < lo ? k : lo;
                        hi = k > hi ? k : hi;
                    }
                qd_page_set_loop(p, a->sel, (int)(((1u << (hi + 1)) - 1u) & ~((1u << lo) - 1u)));
                qd_ctl_set_page(a, lo);
                u->pg_range = 1;
                u->pg_done |= pages;
            }
            return;
        }
        u->lk_down &= (uint16_t)~bit;
        if (u->pg_range) {                              /* a range: nothing more until the page keys are up */
            if (!(u->lk_down & 0xFFu))
                u->pg_range = 0;
            return;
        }
        if (u->pg_done & bit)                           /* held (toggled), pasted */
            return;
        if (a->ms - u->pg_t0[b] >= QD_PAGE_HOLD_MS) {   /* (held, and the timer did not see it: toggle now) */
            page_toggle(a, b);
            u->pg_taps[b] = 0;
        } else if (u->pg_taps[b] && a->ms - u->pg_up[b] < QD_CLICK_MS) {   /* the double tap: loop it alone */
            qd_page_set_loop(p, a->sel, 1 << b);
            qd_ctl_set_page(a, b);
            u->pg_taps[b] = 0;
        } else {                                        /* a tap: edit it */
            qd_ctl_set_page(a, b);
            u->pg_taps[b] = 1;
            u->pg_up[b] = a->ms;
        }
        u->pg_done |= bit;
        return;
    }
    if (b == 8) {                                       /* B9: tap the page, held the track */
        if (down) {
            u->b9_down = 1;
            u->b9_done = 0;
            u->b9_t0 = a->ms;
        } else if (u->b9_down) {
            u->b9_down = 0;
            if (!u->b9_done) {
                qd_clip_clear_page(&u->clip, p, a->seq.cur, a->sel, u->page[a->sel]);
                qd_ctl_confirm(a, "PAGE CLEARED", "F1 + B11 undoes it");
            }
        }
        return;
    }
    if (!down)
        return;
    if (b == 9) {                                       /* B10: copy the page; then B1..B8 paste it */
        qd_clip_copy_page(&u->clip, p, a->sel, u->page[a->sel]);
        u->pg_copied = u->page[a->sel];
    } else if (b == 10) {
        const int r = qd_clip_undo(&u->clip, p, a->seq.cur, a->sel);
        qd_ctl_confirm(a, r == 1 ? "UNDONE" : r == QD_EFULL ? "NO ROOM" : "NO UNDO",
                       r == 1 ? "again to redo" : r == QD_EFULL ? "for locks: the pattern has 1024"
                                                                : "nothing cleared or pasted on this track");
    }
}

/* F1's timers: a page key held QD_PAGE_HOLD_MS toggles its page in the loop; B9 held QD_CLEAR_HOLD_MS clears the
 * track */
static void f1_timers(qd_app_t *a)
{
    qd_ui_t *u = &a->ui;
    int b;
    if (qd_ctl_layer(a) != QD_LAYER_F1)
        return;
    for (b = 0; b < QD_NPAGE; b++) {
        const uint16_t bit = (uint16_t)(1u << b);
        if ((u->lk_down & bit) && !(u->pg_done & bit) && !u->pg_range && a->ms - u->pg_t0[b] >= QD_PAGE_HOLD_MS) {
            page_toggle(a, b);
            u->pg_done |= bit;
            u->pg_taps[b] = 0;
        }
    }
    if (u->b9_down && !u->b9_done && a->ms - u->b9_t0 >= QD_CLEAR_HOLD_MS) {
        qd_clip_clear_track(&u->clip, qd_ctl_pat(a), a->seq.cur, a->sel);
        qd_ctl_confirm(a, "TRACK CLEARED", "F1 + B11 undoes it");
        u->b9_done = 1;
    }
}

/* the rates slowest first (the tracks screen's RATE picker): the 21 codes by their step's length */
static const uint8_t *rate_order(void)
{
    static uint8_t order[QD_NRATE];
    static int done;
    int i, j;
    if (!done) {
        for (i = 0; i < QD_NRATE; i++) {
            const int64_t l = qd_rate_ticks(i);
            for (j = i; j > 0 && qd_rate_ticks(order[j - 1]) < l; j--)
                order[j] = order[j - 1];
            order[j] = (uint8_t)i;
        }
        done = 1;
    }
    return order;
}
int qd_ctl_rate_index(int rate)
{
    const uint8_t *o = rate_order();
    int i;
    for (i = 0; i < QD_NRATE; i++)
        if (o[i] == rate)
            return i;
    return 0;
}
int qd_ctl_rate_at(int i) { return rate_order()[i < 0 ? 0 : i >= QD_NRATE ? QD_NRATE - 1 : i]; }

/* F3 + B1..B7: the rate's base (1/1 .. 1/64), the type kept; B8 / B9 / B10: normal / dotted / triplet, the base kept */
static void k_f3_black(qd_app_t *a, int key, int b, int down, int vel)
{
    qd_tset_t *ts = &qd_ctl_pat(a)->trk[a->sel];
    (void)key, (void)vel;
    if (!down)
        return;
    if (b < QD_NVAL)
        ts->rbase = (uint8_t)b;
    else if (b < QD_NVAL + QD_NRTYPE)
        ts->rtype = (uint8_t)(b - QD_NVAL);
}

/* the black keys with nothing held: B1..B11 the hold mode (P1.3c: B11 = P-LOCK, PLAN question 37; P1.3d: B10 is
 * empty, reserved: LOCKS is dropped) */
static void k_mode(qd_app_t *a, int key, int b, int down, int vel)
{
    qd_ui_t *u = &a->ui;
    (void)key, (void)vel;
    if (!down) {                                        /* P1.3e: the mode key let go: back to the lane */
        if (u->mkey == b) {
            u->mkey = -1;
            if (u->tk == QD_TK_DEFAULT)
                u->tk = QD_TK_NONE;
        }
        return;
    }
    if (b < UI_NFIELD && b != UI_F_B10)
        u->hmode = b;
    if (b < UI_F_B10) {                                 /* P1.3e: held, it holds the track's default (B1..B9) */
        u->mkey = b;
        u->mkey_t0 = a->ms;
        u->mkey_used = 0;
    }
}

/* P1.3e: the other black keys while a mode key is held: B11 resets the mode's default, the rest nothing */
static int w_mkey(const qd_app_t *a) { return qd_ctl_mkey(a) >= 0; }
static void k_mkey(qd_app_t *a, int key, int b, int down, int vel)
{
    (void)key, (void)vel;
    if (down && b == 10) {
        qd_step_default_reset(a, a->ui.mkey);
        a->ui.mkey_used = 1;
    }
}

/* SEQ held + B1..B4: the bank A..D the W keys and the map show (P1.3d) */
static void k_bank(qd_app_t *a, int key, int b, int down, int vel)
{
    (void)key, (void)vel;
    if (down && b < QD_NBANK)
        a->ui.seq_bank = b;
}

static void k_none(qd_app_t *a, int key, int idx, int down, int vel) { (void)a, (void)key, (void)idx, (void)down, (void)vel; }

typedef struct {
    qd_when_fn when;
    qd_key_fn fn;
} qd_key_row_t;
static const qd_key_row_t WHITE_ROWS[] = {
    {w_seq_held, k_pattern},           /* SEQ held: A01 .. A16 (SHIFT: at once) */
    {w_sel_held, k_sel_white},         /* SEL held: W1..W8 mute, W9..W16 solo */
    {w_tracks_held, k_track_select},   /* TRACKS held: tracks 1..8 */
    {w_keys_mode, k_note},             /* the keyboard */
    {w_tracks_screen, k_tracks_screen},   /* the tracks screen: select 1..8, mute 9..16 */
    {w_page_held, k_page},             /* PAGE held: edit pages 1..8 */
    {w_steps_held, k_step},            /* steps held: more held steps */
    {w_f1, k_f1_white},                /* F1: the first W copies, the later ones paste */
    {w_f2, k_f2_white},                /* F2: Form's pick-up / drop */
    {w_f3, k_f3_white},                /* F3: the page's end, start..end */
    {w_step_view, k_step},             /* the step view: the steps */
};
static const qd_key_row_t BLACK_ROWS[] = {
    {w_seq_held, k_bank},              /* SEQ held: B1..B4 the bank (P1.3d) */
    {w_sel_held, k_sel_black},         /* SEL held: B1..B8 select the track */
    {w_keys_mode, k_note},             /* the keyboard */
    {w_mkey, k_mkey},                  /* a mode key held (P1.3e): B11 resets its default, the rest nothing */
    {w_steps_held, k_value},           /* steps held: the mode's values, B11 CLEAR */
    {w_f1, k_f1_black},                /* F1: pages, clear page / track, copy / paste a page, undo */
    {w_f2, k_none},                    /* F2: (only the step pick-up / drop, on W) */
    {w_f3, k_f3_black},                /* F3: the rate */
    {w_step_view, k_mode},             /* nothing held: the hold mode */
};
void qd_ctl_key(qd_app_t *a, int key, int down, int vel)
{
    const int w = qd_ctl_white(key), b = qd_ctl_black(key);
    const qd_key_row_t *rows = w >= 0 ? WHITE_ROWS : BLACK_ROWS;
    const int n = w >= 0 ? (int)(sizeof WHITE_ROWS / sizeof WHITE_ROWS[0]) : (int)(sizeof BLACK_ROWS / sizeof BLACK_ROWS[0]);
    int i;
    if (down) {
        a->ui.key_row[key] = 0;
        if (touch(a))                                   /* the press that woke the unit does nothing else */
            return;
        for (i = 0; i < n; i++)
            if (rows[i].when(a)) {
                a->ui.key_row[key] = (int8_t)(i + 1);   /* the row that took the press takes the release */
                rows[i].fn(a, key, w >= 0 ? w : b, 1, vel);
                return;
            }
    } else if (a->ui.key_row[key] > 0 && a->ui.key_row[key] <= n) {
        const qd_key_row_t *r = &rows[a->ui.key_row[key] - 1];
        a->ui.key_row[key] = 0;
        r->fn(a, key, w >= 0 ? w : b, 0, 0);
    }
}

/* ================================================================ the buttons === */
static void b_tracks(qd_app_t *a, int down)
{
    if (down) {
        a->ui.tracks_tap = 1;
    } else if (a->ui.tracks_tap) {                      /* a tap: the tracks screen on / off */
        a->ui.tracks_tap = 0;
        set_mode(a, a->ui.mode == QD_MODE_TRACKS ? QD_MODE_STEP : QD_MODE_TRACKS);
    }
}

/* PAGE: a tap = page follow on / off; held + W1..W8 = edit that page */
static void b_page(qd_app_t *a, int down)
{
    qd_ui_t *u = &a->ui;
    if (down) {
        u->page_used = 0;
    } else if (!u->page_used && u->mode == QD_MODE_STEP) {
        u->follow = !u->follow;
        u->play_page[a->sel] = -1;
        qd_ctl_confirm(a, u->follow ? "FOLLOW" : "NO FOLLOW", u->follow ? "the page follows the playhead"
                                                                        : "the page stays where you put it");
    }
}

static void b_shift(qd_app_t *a, int down)
{
    if (down)
        a->ui.shift_used = 0;
}

/* HOME (P1.3e, §8b 2.2): first it closes an open latch (F3's menu, the step latch); else, in the sequencer, it
 * switches its view (PIANO ROLL <-> STEPS); from any other screen it comes back to the sequencer in its last view */
static void b_home(qd_app_t *a, int down)
{
    qd_ui_t *u = &a->ui;
    const int latched = u->latch != QD_LATCH_NONE || u->slatch;
    if (!down)
        return;
    qd_latch_close(a);
    u->slatch = 0;
    qd_step_release_all(a);
    u->tk = QD_TK_NONE;
    if (!latched && !u->home_woke && u->mode == QD_MODE_STEP)
        u->view = (u->view + 1) % QD_NVIEW;
    u->home_woke = 0;
    set_mode(a, QD_MODE_STEP);
}

/* P1.3d (§8b 2.2): LFO (printed) toggles the step latch: as if a step is always held, a W tap selects (ctl_step.c) */
static void b_lfo(qd_app_t *a, int down)
{
    if (!down || a->ui.mode != QD_MODE_STEP)
        return;
    if (!a->ui.slatch)
        qd_latch_close(a);                              /* (F3's menu gives way to the selection) */
    qd_step_latch(a, !a->ui.slatch);
}

/* ARP held: FILL (the FILL / NOT FILL conditions) */
static void b_fill(qd_app_t *a, int down) { qd_seq_fill(&a->seq, down); }

static void b_play(qd_app_t *a, int down)
{
    if (!down)
        return;
    if (held(a, QD_B_REC)) {                            /* REC + PLAY: play with recording armed, in the keyboard */
        a->ui.rec_down_used = 1;
        if (a->ui.mode != QD_MODE_KEYS)
            set_mode(a, QD_MODE_KEYS);
        if (!a->seq.playing)
            qd_app_play(a);
        if (!a->ui.rec)
            qd_rec_toggle(a);
        return;
    }
    if (a->seq.playing)
        qd_app_stop(a);
    else
        qd_app_play(a);
}

/* REC: a tap switches the keybed between the sequencer and the keyboard; SHIFT + REC in the keyboard: armed / not */
static void b_rec(qd_app_t *a, int down)
{
    qd_ui_t *u = &a->ui;
    if (down) {
        u->rec_down_used = 0;
        if (qd_ctl_shift(a)) {
            u->rec_down_used = 1;
            if (u->mode == QD_MODE_KEYS) {
                qd_rec_toggle(a);
                qd_ctl_confirm(a, u->rec ? (a->seq.playing ? "LIVE REC" : "STEP REC") : "REC OFF",
                               u->rec ? (a->seq.playing ? "notes go in as played" : "a chord a step")
                                      : "the keyboard plays");
            }
        }
        return;
    }
    if (u->rec_down_used)
        return;
    set_mode(a, u->mode == QD_MODE_KEYS ? QD_MODE_STEP : QD_MODE_KEYS);
}

/* OCT- / OCT+ (d -1 / +1): the layers in the step view (F1 / F2 / both F3); with steps held: NOTE's octave, SOUND's
 * bank; the keyboard: the octave (step recording: OCT+ a rest, OCT- a step back), both: every voice out (PANIC) */
static void b_oct(qd_app_t *a, int d, int down)
{
    qd_ui_t *u = &a->ui;
    const int both = held(a, QD_B_OCTDN) && held(a, QD_B_OCTUP);
    if (!down) {
        u->oct_eaten &= (uint8_t)~(d < 0 ? 1u : 2u);   /* (a key a latch ate: free again) */
        if (d < 0) {                                    /* F1 let go: its step copy and page copy end (the clipboard
                                                         * stays: F2's hand stays loaded, as Form's) */
            u->f1_copied = 0;
            u->pg_copied = -1;
            u->b9_down = 0;
        } else {                                        /* F2 let go: the hand empties (Form's) */
            u->f2_loaded = u->f2_holding = 0;
            u->f2_last_w = -1;
        }
        if (!held(a, QD_B_OCTDN) && !held(a, QD_B_OCTUP)) {
            u->lk_down = 0;
            u->pg_range = 0;
            if (!u->latch)
                u->f3_anchor = -1;
            qd_clip_group(&u->clip, 0);                 /* the hold's edits were one undo level */
        }
        return;
    }
    if (u->mode == QD_MODE_KEYS || u->mode == QD_MODE_TRACKS) {
        if (both) {
            qd_pool_kill_all(&a->pool);
            return;
        }
        if (u->mode != QD_MODE_KEYS)
            return;
        if (u->rec && !a->seq.playing) {                /* step recording: OCT+ a rest, OCT- a step back */
            if (d > 0)
                qd_rec_rest(a);
            else
                qd_rec_cursor(a, -1);
        } else {
            const int from = a->oct;
            a->oct = clampi(a->oct + d, QD_OCT_MIN, QD_OCT_MAX);
            qd_ctl_octave(a, UI_OCT_KEYS, from, a->oct);
        }
        return;
    }
    if (u->mode != QD_MODE_STEP)
        return;
    if (steps_held(a) && !u->latch) {                   /* NOTE: the entry octave; SOUND: the bank; else nothing */
        qd_step_promote_all(a);
        qd_step_oct(a, d);
        return;
    }
    if (qd_latch_oct(a, d))                             /* P1.3d: a latched menu: OCT- back, OCT+ confirm */
        return;
    if (both) {                                         /* P1.3d: both together open F3, latched */
        qd_latch_open(a, QD_LATCH_F3);
        u->oct_eaten |= 3u;
    } else {
        u->f_ms = a->ms;                                /* the layer's first key */
    }
    if (!u->clip.group)
        qd_clip_group(&u->clip, 1);                     /* a layer's hold: its edits one undo level */
}
static void b_octdn(qd_app_t *a, int down) { b_oct(a, -1, down); }
static void b_octup(qd_app_t *a, int down) { b_oct(a, 1, down); }
static void b_none(qd_app_t *a, int down) { (void)a, (void)down; }

/* the buttons by role (PLAN §8b; what is not built yet: the sound editor, the mix, saving) */
static void (*const BUTTONS[QD_NBTN])(qd_app_t *a, int down) = {
    [QD_B_TRACKS] = b_tracks,          /* tap: the tracks screen; held + white 1..8: select */
    [QD_B_PATTERN] = b_none,           /* printed SEL, held: the tracks (B select, W mute / solo: the keys' table) */
    [QD_B_PAGE] = b_page,              /* tap: page follow on / off; held + white 1..8: edit that page */
    [QD_B_MUTE] = b_lfo,               /* printed LFO: the step latch on / off (P1.3d) */
    [QD_B_SOUND] = b_none,             /* the sound editor (next milestone) */
    [QD_B_FUNC] = b_shift,             /* SHIFT, held */
    [QD_B_HOME] = b_home,              /* the step view */
    [QD_B_SAVE] = b_none,
    [QD_B_KEYS] = b_fill,              /* ARP held: FILL */
    [QD_B_MIX] = b_none,               /* printed SEQ, held: the patterns (W: the keys' table) */
    [QD_B_PLAY] = b_play,              /* play / stop; REC + PLAY: play, recording armed */
    [QD_B_REC] = b_rec,                /* tap: the keyboard <-> the sequencer; SHIFT: armed */
    [QD_B_OCTDN] = b_octdn,            /* F1 */
    [QD_B_OCTUP] = b_octup,            /* F2 */
};

void qd_ctl_button(qd_app_t *a, int button, int down)
{
    if (down) {
        if (button != QD_B_FUNC)
            a->ui.home_woke = touch(a) && button == QD_B_HOME;
        else
            a->ui.touch_ms = a->ms;
        if (a->ui.mode == QD_MODE_IDLE)
            a->ui.mode = QD_MODE_STEP;
    }
    BUTTONS[button](a, down);
}

/* ================================================================ the knobs === */
/* the selected track's sound: the quick page (KNOB 1..4: FREQ, DECAY, HARM, FDBK) */
static void live_edit(qd_app_t *a, int id, int d, int tk, int knob)
{
    const qd_track_t *t = &a->model.t[a->sel];
    qd_player_live_edit(&a->player, a->sel, id, t->patch[id] + d * qd_step_id_detent(id));
    qd_ctl_takeover(a, tk, knob, d, 0, -1);
}

/* the track's sound from the pool (PRESETS with nothing held, SOUND's ALGORITHM) */
static void track_sound(qd_app_t *a, int d)
{
    qd_track_t *t = &a->model.t[a->sel];
    const int k = clampi((int)t->sound + d, 1, a->proj.nsound);
    qd_track_load_sound(t, k, qd_sound_name(&a->proj, k), qd_sound_patch(&a->proj, k));
    qd_ctl_takeover(a, QD_TK_PRESET, 0, d, 0, -1);
}

/* SELECT: the page edited (with or without steps held); its picker takes the screen, but in the STEPS view (P1.3g),
 * which shows every page already: there the edited row just moves */
static void page_knob(qd_app_t *a, int d)
{
    qd_ctl_set_page(a, a->ui.page[a->sel] + d);
    if (a->ui.view != QD_VIEW_STEPS)
        qd_ctl_takeover(a, UI_TK_PAGE, 0, d, 0, -1);
}

/* the step view, nothing held: SELECT the page, ALGORITHM the mode's default (SHIFT: the scale), PRESETS the track's
 * sound (SHIFT: the root). P1.3d (§8b 2.2): KNOB 1..4 the track's defaults in the mode's knob layout (KNOB_JOB); in
 * P-LOCK KNOB 1..4 the track's sound (FREQ, DECAY, HARM, FDBK, live) and ALGORITHM the track (1..8, its chooser) */
static int k_stepview(qd_app_t *a, int knob, int d)
{
    qd_tset_t *ts = &qd_ctl_pat(a)->trk[a->sel];
    switch (knob) {
    case QD_K_SELECT:
        page_knob(a, d);
        return 1;
    case QD_K_ALGO:
        if (qd_ctl_shift(a)) {                          /* (temporary: the track's scale) */
            ts->scale = (uint8_t)clampi(ts->scale + d, 0, QD_NSCALE - 1);
            ts->key_global = 0;
            qd_ctl_takeover(a, UI_TK_SCALE, 0, d, 0, -1);
            return 1;
        }
        if (a->ui.hmode == UI_F_PLOCK) {                /* P-LOCK: the track */
            qd_ctl_select_track(a, clampi(a->sel + d, 0, QD_NTRACK - 1));
            qd_ctl_takeover(a, UI_TK_TRACK, 0, d, 0, -1);
            return 1;
        }
        return qd_step_default_knob(a, d);
    case QD_K_PRESETS:
        if (qd_ctl_shift(a)) {                          /* (temporary: the track's root) */
            ts->root = (uint8_t)(((ts->root + d) % 12 + 12) % 12);
            ts->key_global = 0;
            qd_ctl_takeover(a, UI_TK_ROOT, 0, d, 0, -1);
            return 1;
        }
        track_sound(a, d);
        return 1;
    default:
        if (a->ui.hmode == UI_F_PLOCK) {
            live_edit(a, qd_step_quick_id(knob - QD_K_1), d, UI_TK_FREQ + (knob - QD_K_1), knob - QD_K_1);
        } else {
            const int job = qd_ctl_knob_job(a->ui.hmode, knob);
            if (job != QD_KJ_NONE)
                qd_step_job_default(a, job, knob, d);
        }
        return 1;
    }
}

/* P1.3i: a tempo turn, every path: following a running MIDI clock, the tempo is the clock's: a brief EXT CLOCK says so;
 * else (stopped, or the unit's own clock) the pattern's tempo, the sequencer's and the sound's */
static void tempo_turn(qd_app_t *a, int d)
{
    if (qd_seq_ext_running(&a->seq)) {
        qd_ctl_confirm(a, "EXT CLOCK", "the tempo follows MIDI clock");
    } else {
        qd_ctl_set_tempo(a, a->bpm + d);
        a->ui.tempo_ms = a->ms ? a->ms : 1u;
    }
}

/* the keyboard (not recording live): SELECT tempo (SHIFT: swing), PRESETS the sound, KNOB 1..4 the quick page */
static int k_track(qd_app_t *a, int knob, int d)
{
    switch (knob) {
    case QD_K_SELECT:
        if (qd_ctl_shift(a)) {
            qd_pattern_t *p = qd_ctl_pat(a);
            p->swing = (uint8_t)clampi(p->swing + d, 50, 80);
            qd_ctl_takeover(a, UI_TK_SWING, 1, d, 0, -1);
        } else {
            tempo_turn(a, d);
            if (!qd_seq_ext_running(&a->seq))
                qd_ctl_takeover(a, UI_TK_TEMPO, 0, d, 0, -1);
        }
        return 1;
    case QD_K_PRESETS:
        track_sound(a, d);
        return 1;
    case QD_K_ALGO:
        return 1;                                       /* (the track's ALGO: the sound editor) */
    default:
        live_edit(a, qd_step_quick_id(knob - QD_K_1), d, UI_TK_FREQ + (knob - QD_K_1), knob - QD_K_1);
        return 1;
    }
}

/* F3 + ALGORITHM: tempo; SHIFT + F3 + ALGORITHM: swing; F3 + SELECT: the page (P1.3h; they were on SELECT). P1.3c
 * (§8b 2.1, F3's knobs): KNOB 1 the rate's note value (1/2 .. 1/32), KNOB 2 normal / dotted / triplet, KNOB 3 the
 * edited page's start, KNOB 4 its end (start <= end: each stops at the other). The F3 map is the screen: no takeover */
static int k_f3(qd_app_t *a, int knob, int d)
{
    qd_pattern_t *p = qd_ctl_pat(a);
    qd_tset_t *ts = &p->trk[a->sel];
    const int pg = a->ui.page[a->sel];
    switch (knob) {
    case QD_K_1:
        ts->rbase = (uint8_t)clampi(ts->rbase + d, 0, QD_NVAL - 1);
        return 1;
    case QD_K_2:
        ts->rtype = (uint8_t)clampi(ts->rtype + d, 0, QD_NRTYPE - 1);
        return 1;
    case QD_K_3:
        qd_page_set_range(p, a->sel, pg, clampi(ts->pstart[pg] + d, 0, ts->pend[pg]), ts->pend[pg]);
        return 1;
    case QD_K_4:
        qd_page_set_range(p, a->sel, pg, ts->pstart[pg], clampi(ts->pend[pg] + d, ts->pstart[pg], QD_PAGE - 1));
        return 1;
    default:
        break;
    }
    if (knob == QD_K_SELECT) {                          /* P1.3h: SELECT the page edited (the map follows) */
        qd_ctl_set_page(a, a->ui.page[a->sel] + d);
        return 1;
    }
    if (knob != QD_K_ALGO)
        return 1;                                       /* P1.3j: PRESETS has no job in F3 (it went through to the
                                                         * step view and changed the track's sound under the map) */
    if (qd_ctl_shift(a)) {                              /* P1.3h: ALGORITHM the tempo, SHIFT + ALGORITHM the swing, */
        p->swing = (uint8_t)clampi(p->swing + d, 50, 80);   /* live on the map (no takeover: the map is the screen) */
        a->ui.tempo_ms = a->ms ? a->ms : 1u;
    } else
        tempo_turn(a, d);
    return 1;
}

/* the tracks screen: KNOB 1 (and SELECT) LEN (the loop, Form's flat rebuild), KNOB 2 RATE, KNOB 3 LIMIT, KNOB 4 SWING */
static int k_tracks(qd_app_t *a, int knob, int d)
{
    qd_pattern_t *p = qd_ctl_pat(a);
    qd_tset_t *ts = &p->trk[a->sel];
    switch (knob) {
    case QD_K_SELECT:
    case QD_K_1:
        qd_track_set_length(p, a->sel, clampi(qd_track_len(ts) + d, 1, QD_NSTEP));
        qd_ctl_takeover(a, UI_TK_LEN, 0, d, 0, -1);
        return 1;
    case QD_K_2:
        qd_tset_set_rate(ts, qd_ctl_rate_at(qd_ctl_rate_index(qd_tset_rate(ts)) + d));
        qd_ctl_takeover(a, UI_TK_RATE, 1, d, 0, -1);
        return 1;
    case QD_K_3:
        ts->limit = (uint8_t)clampi(ts->limit + d, 1, 16);
        qd_ctl_takeover(a, UI_TK_LIMIT, 2, d, 0, -1);
        return 1;
    case QD_K_4:
        ts->swing = d > 0;
        qd_ctl_takeover(a, UI_TK_TSWING, 3, d, 0, -1);
        return 1;
    default:
        return k_track(a, knob, d);
    }
}

static int when_f3(const qd_app_t *a) { return qd_ctl_layer(a) == QD_LAYER_F3; }
static int when_seq(const qd_app_t *a) { return a->ui.mode != QD_MODE_IDLE && held(a, QD_B_MIX); }
/* SEQ held + ALGORITHM: the launch mode (END BAR BEAT NOW), a project setting */
static int k_seq(qd_app_t *a, int knob, int d)
{
    if (knob != QD_K_ALGO)
        return 0;
    a->proj.launch = (uint8_t)clampi(a->proj.launch + d, 0, QD_NLAUNCH - 1);
    return 1;
}
static int when_steps_held(const qd_app_t *a) { return w_steps_held(a); }
/* P1.3e: a mode key held: SELECT and KNOB 1..4 its default by KNOB_JOB, ALGORITHM the main default (SHIFT, PRESETS:
 * as the step view) */
static int when_mkey(const qd_app_t *a) { return qd_ctl_mkey(a) >= 0; }
static int k_mkey_knob(qd_app_t *a, int knob, int d)
{
    int job;
    if (qd_ctl_shift(a) || knob == QD_K_PRESETS)
        return 0;
    a->ui.mkey_used = 1;
    job = knob == QD_K_ALGO ? QD_KJ_MAIN : qd_ctl_knob_job(a->ui.mkey, knob);
    if (job != QD_KJ_NONE)
        qd_step_job_default(a, job, knob, d);
    return 1;
}
static int when_live(const qd_app_t *a) { return a->ui.mode == QD_MODE_KEYS && a->ui.rec && a->seq.playing; }
static int when_step_rec(const qd_app_t *a) { return a->ui.mode == QD_MODE_KEYS && a->ui.rec && !a->seq.playing; }
static int when_tracks(const qd_app_t *a) { return a->ui.mode == QD_MODE_TRACKS; }
static int when_stepview(const qd_app_t *a) { return a->ui.mode == QD_MODE_STEP; }
static int when_always(const qd_app_t *a) { (void)a; return 1; }
static int k_held(qd_app_t *a, int knob, int d) { return qd_step_knob(a, knob, d); }
static int k_live(qd_app_t *a, int knob, int d) { return qd_rec_knob(a, knob, d); }
static int k_cursor(qd_app_t *a, int knob, int d)
{
    if (knob != QD_K_PRESETS)
        return 0;
    qd_rec_cursor(a, d);                                /* step recording: PRESETS moves the cursor */
    return 1;
}

/* P1.3q: the STEPS view with no finger on a step (nothing held, or the step latch's selection): PRESETS is a step
 * cursor (SHIFT + PRESETS stays the root, a mode key held its default) */
static int when_cursor(const qd_app_t *a)
{
    const qd_ui_t *u = &a->ui;
    return u->mode == QD_MODE_STEP && u->view == QD_VIEW_STEPS && !u->wk_down && (u->slatch || !u->nheld) &&
           qd_ctl_mkey(a) < 0 && !qd_ctl_shift(a);
}
static int k_steps_cursor(qd_app_t *a, int knob, int d)
{
    if (knob != QD_K_PRESETS)
        return 0;
    qd_step_cursor(a, d);
    return 1;
}

static const struct {
    qd_when_fn when;
    int (*fn)(qd_app_t *a, int knob, int d);           /* 1: taken (else the next row) */
} KNOB_ROWS[] = {
    {when_seq, k_seq},                 /* SEQ held: ALGORITHM the launch mode */
    {when_f3, k_f3},                   /* F3: SELECT page, ALGO tempo (SHIFT: swing), KNOB 1..4 value, type, start, end */
    {when_cursor, k_steps_cursor},     /* P1.3q: STEPS, no finger on a step: PRESETS the step cursor */
    {when_steps_held, k_held},         /* a step held: SELECT page, ALGORITHM value, PRESETS sound lock, p-locks */
    {when_mkey, k_mkey_knob},          /* a mode key held (P1.3e): its default */
    {when_live, k_live},               /* live recording: the quick page as locks on the step playing */
    {when_step_rec, k_cursor},         /* step recording: PRESETS moves the cursor */
    {when_tracks, k_tracks},           /* the tracks screen: LEN RATE LIMIT SWING */
    {when_stepview, k_stepview},       /* the step view: page, the mode's default, the sound, the quick page */
    {when_always, k_track},            /* the keyboard: tempo, the sound, the quick page */
};

void qd_ctl_knob(qd_app_t *a, int knob, int d)
{
    size_t i;
    touch(a);
    for (i = 0; i < sizeof KNOB_ROWS / sizeof KNOB_ROWS[0]; i++)
        if (KNOB_ROWS[i].when(a) && KNOB_ROWS[i].fn(a, knob, d))
            return;
}

/* ================================================================ time === */
void qd_ctl_ms(qd_app_t *a)
{
    qd_ui_t *u = &a->ui;
    qd_step_promote_due(a);                             /* a white key down 250 ms: held */
    f1_timers(a);                                       /* F1: a page key held 400 ms, B9 held 600 ms */
    sel_seq_timers(a);                                  /* SEL + W held: solo; SEQ + W held: at once */
    if (u->pv_note >= 0 && a->ms - u->pv_ms >= QD_PREVIEW_MS)
        qd_ctl_preview(a, -1, 0);                       /* a knob's note: its moment is over */
    if (u->tk != QD_TK_NONE && a->ms - u->tk_ms >= QD_TK_MS + QD_TK_FADE_MS)
        u->tk = QD_TK_NONE;
    if (u->cf_on && a->ms - u->cf_ms >= QD_CONFIRM_MS)
        u->cf_on = 0;
    if (a->seq.flash_sample[a->sel] != u->flash_seen && a->seq.flash_sample[a->sel] <= a->seq.now) {
        u->flash_seen = a->seq.flash_sample[a->sel];    /* a trig of the selected track: the held step's? */
        if (u->nheld && a->seq.flash_step[a->sel] == u->held[0]) {
            u->held_play_ms = a->ms;
            u->held_played = 1;
        }
    }
    if (u->rec_live && !a->seq.playing)                 /* the transport stopped under live notes: they end */
        qd_rec_key(a, -1, 0, 0);
    {   /* page follow: the page edited moves when the playhead goes to another page (a page chosen by hand stays
         * until then); not while steps are held or a layer is */
        const int pp = qd_ctl_playing_page(a, a->sel);
        if (pp < 0) {
            u->play_page[a->sel] = -1;
        } else if (pp != u->play_page[a->sel]) {
            u->play_page[a->sel] = pp;
            if (u->follow && u->nheld == 0 && !u->wk_down && qd_ctl_layer(a) == QD_LAYER_NONE &&
                !(u->mode == QD_MODE_KEYS && u->rec))
                qd_ctl_set_page(a, pp);
        }
    }
    if (u->mode != QD_MODE_IDLE && !a->seq.playing && a->ms - u->touch_ms >= QD_IDLE_MS && !a->keys_held &&
        !a->buttons_held) {
        set_mode(a, QD_MODE_IDLE);                      /* two minutes untouched, stopped: the idle screen */
        u->tk = QD_TK_NONE;
    }
}
