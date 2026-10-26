/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
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
qd_pattern_t *qd_ctl_pat(qd_app_t *a) { return qd_proj_pat(&a->proj, a->seq.cur); }
const qd_pattern_t *qd_ctl_cpat(const qd_app_t *a) { return qd_proj_cpat(&a->proj, a->seq.cur); }
static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

static int steps_held(const qd_app_t *a) { return a->ui.nheld > 0 || a->ui.wk_pending; }
/* P1.5m: a finger on a step (a key down, or a held step not the latch's): the step latch's selection alone is no finger,
 * so the F layers work over it (the owner: "the F1 / F2 shortcuts do not work when the step lock is turned on") */
static int finger_held(const qd_app_t *a)
{
    const qd_ui_t *u = &a->ui;
    return u->wk_down || u->wk_pending || (u->nheld > 0 && !u->slatch);
}

/* P1.3d: F3 is a latched menu (ctl_latch.c): open, it is the layer whatever is held; an OCT key a latch ate is no
 * layer until it is let go */
int qd_ctl_layer(const qd_app_t *a)
{
    const int dn = held(a, QD_B_OCTDN) && !(a->ui.oct_eaten & 1u), up = held(a, QD_B_OCTUP) && !(a->ui.oct_eaten & 2u);
    if (a->ui.mode != QD_MODE_STEP)
        return QD_LAYER_NONE;
    if (a->ui.latch == QD_LATCH_F3)
        return QD_LAYER_F3;
    if (qd_snd_keys(a))                                 /* (P1.6h: the editor's keyboard: F1 / F2 its octave) */
        return QD_LAYER_NONE;
    if (finger_held(a) || (!dn && !up))                 /* (P1.5m: was steps_held: the latch's selection blocked them) */
        return QD_LAYER_NONE;
    if (up && !dn && a->ui.hmode == UI_F_PLOCK && !a->ui.latch)
        return QD_LAYER_NONE;                           /* (P1.5o: P-LOCK: F2 held is the unlock, no layer) */
    if (dn && up)                                       /* (P1.6h: on a main page both together are nothing) */
        return QD_LATCH_MAIN(a->ui.latch) ? QD_LAYER_NONE : QD_LAYER_F3;
    return dn ? QD_LAYER_F1 : QD_LAYER_F2;
}

/* P1.6h (the owner: "If the white keys are steps then F1 / F2 across all modes should work for copy and cut"): the
 * main pages whose W keys are the steps; there F1 / F2 held are the step view's layers (their W, F1's black keys,
 * their pictures) */
int qd_ctl_main_steps(const qd_app_t *a)
{
    const qd_ui_t *u = &a->ui;
    if (u->mode != QD_MODE_STEP)
        return 0;
    switch (u->latch) {
    case QD_LATCH_MIX:
    case QD_LATCH_FX: return 1;
    case QD_LATCH_SND: return !u->ekeys;                /* (its keyboard: F1 / F2 the octave) */
    case QD_LATCH_MFX: return !qd_mfx_lane_page(a);     /* (STEPS; an arp lane's W keys are its steps) */
    default: return 0;
    }
}
int qd_ctl_main_layer(const qd_app_t *a)
{
    const int l = qd_ctl_main_steps(a) ? qd_ctl_layer(a) : QD_LAYER_NONE;
    return l == QD_LAYER_F1 || l == QD_LAYER_F2 ? l : QD_LAYER_NONE;
}

/* P1.3d (§8b 2.2): the knobs' jobs in the hold modes, [mode][SELECT, KNOB 1..4]. With a step held SELECT and KNOB 1
 * are the main value (NOTE: SELECT note 1, KNOB 1..4 notes 2..5), KNOB 2 the mode's extra; with nothing held KNOB 1..4
 * the same on the track's defaults (SELECT the page). A knob without a job does nothing. P-LOCK has no row: its knobs
 * are the screen's p-locks (held) and the track's sound (nothing held) */
static const uint8_t KNOB_JOB[UI_NFIELD][5] = {         /* P1.3e: KNOB 4 the quiet main value where it is free */
    [UI_F_NOTE] = {QD_KJ_CHORD, QD_KJ_NOTE2, QD_KJ_NOTE3, QD_KJ_NOTE4, QD_KJ_NOTE5},   /* (P1.5o: SELECT the chord) */
    [UI_F_VEL] = {QD_KJ_MAIN, QD_KJ_MAIN, 0, 0, QD_KJ_QUIET},
    [UI_F_LEN] = {QD_KJ_MAIN, QD_KJ_MAIN, QD_KJ_TIE, 0, QD_KJ_QUIET},
    [UI_F_MICRO] = {QD_KJ_MAIN, QD_KJ_MAIN, QD_KJ_STRUM, 0, QD_KJ_QUIET},
    [UI_F_RATCH] = {QD_KJ_MAIN, QD_KJ_MAIN, QD_KJ_RAMP, 0, QD_KJ_QUIET},
    [UI_F_CHNC] = {QD_KJ_MAIN, QD_KJ_MAIN, QD_KJ_COND, 0, QD_KJ_QUIET},
    [UI_F_COND] = {QD_KJ_MAIN, QD_KJ_MAIN, QD_KJ_CHANCE, 0, QD_KJ_QUIET},
    [UI_F_TRIG] = {QD_KJ_MAIN, QD_KJ_MAIN, QD_KJ_MFX, 0, QD_KJ_QUIET},   /* (P1.6c: KNOB 2 the MIDI FX) */
    [UI_F_FUNC] = {0, QD_KJ_FTYPE, QD_KJ_LEAP, QD_KJ_JTGT, QD_KJ_QUIET},   /* P1.3y: SELECT the page; KNOB 1 the kind,
                                                         * 2 LEAP's n, 3 JUMP's target */
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
    if ((l == QD_LAYER_F1 || l == QD_LAYER_F2) && a->ui.oct_tap && !a->ui.oct_tap_used)   /* P1.5n: the latch's
                                                         * selection: maybe a tap (its octave), no map for a tap */
        return a->ms - a->ui.f_ms >= QD_TAP_MS ? l : QD_LAYER_NONE;
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
    u->fol_step = -1;
    u->fol_g = -1;
    u->view = QD_VIEW_EDIT;            /* P1.5s (the owner: "this home screen should be the default one") */
    u->follow = 1;
    u->lmode = QD_LMODE_MODES;         /* (the owner, 2026-10-25: MODES the default; a saved setting overrides) */
    u->hmode = UI_F_NOTE;
    u->tk = QD_TK_NONE;
    u->rec_step = -1;
    u->entry_oct = QD_ENTRY_OCT;
    u->fn_leap = 2;
    u->snd_knob = -1;
    u->snd_page = QD_SND_START_PAGE;                    /* (P1.5l: RATIO; then the last page used) */
    u->fn_target = -1;
    u->f3_anchor = -1;
    u->f1_src = -1;
    u->pg_copied = -1;
    u->f2_last_w = -1;
    u->pv_note = -1;
    u->mkey = -1;
    u->seq_bank = QD_PAT_START / 16;
    u->tl.scope = QD_TS_TRACK;                          /* T1: TOOLS starts on ROTATE, the track */
    u->tl.opage = 0xFF;
    for (i = 0; i < QD_PAT_NTRACK; i++)
        u->last_step[i] = -1;
    qd_clip_init(&u->clip);
    for (i = 0; i < QD_PAT_NTRACK; i++)
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
    u->tk_hold = 0;                                     /* (P1.5v: the pool picker's caller sets it) */
}

void qd_ctl_imported(qd_app_t *a, const char *name, int pool, int slot)
{
    qd_ui_t *u = &a->ui;
    memset(u->imp_name, 0, sizeof u->imp_name);
    strncpy(u->imp_name, name, sizeof u->imp_name - 1);
    u->imp_pool = pool;
    u->imp_slot = slot;
    u->cf_on = 0;
    qd_ctl_takeover(a, QD_TK_IMPORT, 0, 0, 0, -1);
}

void qd_ctl_confirm(qd_app_t *a, const char *big, const char *small)
{
    a->ui.cf_col = 0;
    a->ui.cf_big = big;
    a->ui.cf_small = small;
    a->ui.cf_ms = a->ms;
    a->ui.cf_on = 1;
}

void qd_ctl_confirm_col(qd_app_t *a, const char *big, const char *small, uint32_t col)
{
    qd_ctl_confirm(a, big, small);
    a->ui.cf_col = col;
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
    if (a->ui.latch == QD_LATCH_BROWSE)
        qd_latch_close(a);                              /* (P1.5t: the browser was for the other track: a cancel) */
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
    if (held(a, QD_B_MUTE) && !a->ui.lmode_open)
        a->ui.lfo_used = 1;                             /* (P1.5b: LFO with a knob or a key: no latch-mode screen) */
    if (held(a, QD_B_OCTDN) || held(a, QD_B_OCTUP))
        a->ui.oct_tap_used = 1;                         /* (P1.5m: an OCT with something else: a layer, no tap) */
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

/* printed SEQ: the patterns, held or latched (after a hold: seq_used bit 1) */
static int w_seq_held(const qd_app_t *a) { return held(a, QD_B_MIX) || (a->ui.seq_used & 2); }
static int w_sel_held(const qd_app_t *a) { return held(a, QD_B_PATTERN) && a->ui.latch != QD_LATCH_SET; }   /* printed SEL: the
                                                         * tracks (P1.3y: not SHIFT + SEL's settings) */
/* P1.4a: the sound editor open: the black keys are its pages (ctl_snd.c); the W keys stay the steps */
static int w_snd(const qd_app_t *a) { return qd_snd_open(a) && !held(a, QD_B_MIX) && !held(a, QD_B_PATTERN); }
static void k_snd_black(qd_app_t *a, int key, int b, int down, int vel)
{
    (void)key, (void)vel;
    if (down && a->ui.snd_fpop && b != QD_SG_FILTER) {   /* P1.6d: the machines' popup: B1..B6 one of them */
        qd_snd_fpop_key(a, b);
        a->ui.snd_fkey_used = 1;
        return;
    }
    if (b == QD_SG_FILTER) {                            /* P1.6d: B4 FILTERS: a tap its pages, held the popup */
        qd_snd_fkey(a, down);
        return;
    }
    if (b >= 5 && b <= 7)                               /* P1.5k: B6..B8 the LFOs (a tap their page, held: the mapping) */
        qd_snd_lfo_key(a, b - 5, down);
    else if (b == 0)                                    /* E4: B1: its group; held, the modulation slots' mapping */
        qd_snd_mod_key(a, down);
    else if (down)
        qd_snd_black(a, b);
    else
        qd_snd_black_up(a, b);
}
/* P1.3y: a settings menu open: the keys do nothing (KNOB 1..4 its values; OCT- / HOME back) */
static int w_menu(const qd_app_t *a) { return a->ui.mode == QD_MODE_STEP && QD_LATCH_MENU(a->ui.latch); }
static int w_menu_w(const qd_app_t *a) { return w_menu(a) && a->ui.latch != QD_LATCH_TRK; }
static int w_tracks_held(const qd_app_t *a) { return held(a, QD_B_TRACKS); }
static int w_keys_mode(const qd_app_t *a) { return a->ui.mode == QD_MODE_KEYS; }
/* P1.5a: the pages (P1.5t: the library browser, SAVE PRESET), the name editor, the project pages (ctl_lib.c) */
static int w_lib(const qd_app_t *a) { return qd_lib_open(a); }
static void k_lib_white(qd_app_t *a, int key, int w, int down, int vel) { qd_lib_key(a, key, w, -1, down, vel); }
static void k_lib_black(qd_app_t *a, int key, int b, int down, int vel) { qd_lib_key(a, key, -1, b, down, vel); }
/* P1.5u: a name typed (SAVE PRESET, the name editor): every key types (ctl_type.c), before every other row */
static int w_type(const qd_app_t *a) { return qd_type_open(a); }
static void k_type_white(qd_app_t *a, int key, int w, int down, int vel) { (void)key, (void)vel, qd_type_key(a, 1, w, down); }
static void k_type_black(qd_app_t *a, int key, int b, int down, int vel) { (void)key, (void)vel, qd_type_key(a, 0, b, down); }
/* P1.4b: the track settings, the mix, the effects: black keys the track (the effect), the W keys nothing */
static int w_panel(const qd_app_t *a) { return a->ui.mode == QD_MODE_STEP && QD_LATCH_PANEL(a->ui.latch); }
static void k_panel_black(qd_app_t *a, int key, int b, int down, int vel)
{
    (void)key, (void)vel;
    qd_panel_black(a, b, down);
}
/* P1.5b: the sound editor with the keyboard on: SCALE, the W keys the track's scale (the black keys stay its pages);
 * FULL, every key as the keyboard (k_note's notes; the pages on SELECT). Played, not recorded */
static int w_snd_keys(const qd_app_t *a) { return qd_snd_keys(a) && !held(a, QD_B_MIX) && !held(a, QD_B_PATTERN); }
static int w_snd_mk(const qd_app_t *a) { return qd_snd_open(a) && a->ui.snd_lk == 4; }   /* E4: B1 held: W the source */
static void k_snd_mk(qd_app_t *a, int key, int w, int down, int vel)
{
    (void)key, (void)vel;
    if (down)
        qd_snd_mod_src(a, w);
}
static int w_snd_full(const qd_app_t *a) { return w_snd_keys(a) && a->proj.rec_ekeys; }
static void k_snd_note(qd_app_t *a, int key, int idx, int down, int vel)
{
    (void)idx;
    if (down) {
        qd_app_note_on(a, key, a->proj.rec_ekeys || qd_ctl_white(key) < 0 ? qd_rec_key_note(a, key)
                                                                            : qd_snd_scale_note(a, qd_ctl_white(key)),
                       vel);
        qd_rec_key(a, a->key_note[key], 1, vel);        /* (P1.6g: recording in the editor too) */
    } else if ((a->keys_struck >> key) & 1u) {
        qd_rec_key(a, a->key_note[key], 0, 0);
    }
}
static int w_step_view(const qd_app_t *a) { return a->ui.mode == QD_MODE_STEP; }
/* T1: TOOLS: B1..B7 the tool, B11 the scope, W1..W6 the track (ctl_tools.c) */
static int w_tools(const qd_app_t *a) { return qd_tools_open(a); }
static void k_tools_black(qd_app_t *a, int key, int b, int down, int vel)
{
    (void)key, (void)vel;
    qd_tools_black(a, b, down);
}
static void k_tools_white(qd_app_t *a, int key, int w, int down, int vel)
{
    (void)key, (void)vel;
    qd_tools_white(a, w, down);
}
/* P1.6c: the MIDI FX screen: B1..B8 the slots, B10 bypass, B11 remove; the W keys the track through the group (the
 * arp's lanes: their steps) */
static int w_mfx(const qd_app_t *a) { return qd_mfx_open(a) && !held(a, QD_B_MIX) && !held(a, QD_B_PATTERN); }
static void k_mfx_black(qd_app_t *a, int key, int b, int down, int vel)
{
    (void)key, (void)vel;
    qd_mfx_black(a, b, down);
}
static void k_mfx_white(qd_app_t *a, int key, int w, int down, int vel) { qd_mfx_white(a, key, w, down, vel); }
/* P1.5c: SHIFT held in the step view (no menu open): its black-key shortcuts */
static int w_shift_keys(const qd_app_t *a)
{
    return a->ui.mode == QD_MODE_STEP && qd_ctl_shift(a) &&
           (!a->ui.latch || a->ui.latch == QD_LATCH_F3 || a->ui.latch == QD_LATCH_SND ||   /* (P1.5z: the editor too) */
            a->ui.latch == QD_LATCH_MFX);              /* (P1.6c: and the MIDI FX screen) */
}
static void k_shift_black(qd_app_t *a, int key, int b, int down, int vel)
{
    (void)key, (void)vel;
    if (b == 6) {                                       /* B7: on / off at its release; held, a knob: the click's level */
        if (down)
            a->ui.shift_used = 1, a->ui.metro_held = 1, a->ui.metro_turned = 0;
        else if (a->ui.metro_held && !a->ui.metro_turned)
            qd_shift_black(a, 6);
        if (!down)
            a->ui.metro_held = 0;
        return;
    }
    if (down) {
        a->ui.shift_used = 1;
        qd_shift_black(a, b);
    }
}
static void k_panel_white(qd_app_t *a, int key, int w, int down, int vel)
{
    (void)key, (void)vel;
    if (qd_panel_steps(a, w)) {                         /* P1.6d: the mix, the effects: held, the steps their sends
                                                         * lock on */
        if (down)
            qd_step_white_down(a, w);
        else if (a->ui.latch == QD_LATCH_TRK)
            qd_step_white_up(a, w);                     /* (the track settings: a tap makes, removes) */
        else
            qd_step_white_up_hold(a, w);
        return;
    }
    if (down)
        qd_panel_white(a, w);
}
static int w_steps_held(const qd_app_t *a) { return a->ui.mode == QD_MODE_STEP && steps_held(a); }
static int w_f1(const qd_app_t *a) { return qd_ctl_layer(a) == QD_LAYER_F1; }
/* P1.5m: the step latch's selection (no finger) with an F layer down: the layer's rows first, the selection untouched */
static int w_flayer_latch(const qd_app_t *a)
{
    return a->ui.mode == QD_MODE_STEP && a->ui.slatch && !finger_held(a) && qd_ctl_layer(a) != QD_LAYER_NONE;
}
static void k_flayer_white(qd_app_t *a, int key, int w, int down, int vel);
static int w_main_layer(const qd_app_t *a);
static void k_main_black(qd_app_t *a, int key, int b, int down, int vel);
static void k_flayer_black(qd_app_t *a, int key, int b, int down, int vel);
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
        if ((u->seq_down & bit) && !(u->seq_done & bit) && w_seq_held(a) &&
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
    if (u->latch == QD_LATCH_MFX)
        u->mfx_fstep = 1;                               /* (P1.6h: the MIDI FX screen's picture: the step's) */
    if (!u->f1_copied) {
        qd_clip_copy_step(&u->clip, qd_ctl_pat(a), a->sel, step_of(a, w));
        u->f1_copied = 1;
        u->f1_src = step_of(a, w);                      /* (P1.6h: its picture marks where it came from) */
        u->f2_loaded = u->f2_holding = 1;               /* (Form: "a copy loads the buffer .. the next F2 press pastes") */
    } else if (qd_undo_hint(a, QD_UK_PASTE),
               qd_clip_paste_step(&u->clip, qd_ctl_pat(a), a->seq.cur, a->sel, step_of(a, w)) == QD_EFULL) {
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
    if (u->latch == QD_LATCH_MFX)
        u->mfx_fstep = 1;                               /* (P1.6h: the MIDI FX screen's picture: the step's) */
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

/* F1 + B1..B8 (§8b 2.1's page layer): tap = edit that page; a double tap (2026-10-25: the second PRESS within
 * QD_CLICK_MS of the first tap's release; it was the second release, and a press on the unit's keys is ~80..120 ms,
 * so most double taps there came out as two taps) = loop it alone; held QD_PAGE_HOLD_MS = the page in / out of the loop
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
                qd_undo_hint(a, QD_UK_PASTE);
                if (qd_clip_paste_page(&u->clip, p, a->seq.cur, a->sel, b) == QD_EFULL)
                    no_room(a);
                qd_ctl_set_page(a, b);
                u->pg_done |= bit;
                return;
            }
            pages = u->lk_down & 0xFFu;
            if (u->pg_taps[b] == 1 && a->ms - u->pg_up[b] < QD_CLICK_MS)
                u->pg_taps[b] = 2;                      /* (the second press of a double tap: decided at its release) */
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
        } else if (u->pg_taps[b] == 2) {                /* the double tap: loop it alone, edit it */
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
                qd_undo_hint(a, QD_UK_CLEAR);
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
        qd_undo_hint(a, QD_UK_CLEAR);
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
    if (b < UI_NFIELD)
        u->hmode = b;
    if (b < UI_F_PLOCK) {                               /* P1.3e: held, it holds the track's default (B1..B10) */
        u->mkey = b;
        u->mkey_t0 = a->ms;
        u->mkey_used = 0;
    }
}

/* P1.3e: the other black keys while a mode key is held: B11 resets the mode's default, the rest nothing */
static int w_mkey(const qd_app_t *a) { return qd_ctl_mkey(a) >= 0; }
/* P1.3y: LFO held: a black key picks the hold mode, the latch and the selection untouched */
static int w_lfo_held(const qd_app_t *a) { return a->ui.mode == QD_MODE_STEP && held(a, QD_B_MUTE); }
static void k_lfo_mode(qd_app_t *a, int key, int b, int down, int vel)
{
    (void)key, (void)vel;
    if (!down || b < 0 || b >= UI_NFIELD)
        return;
    a->ui.hmode = b;
    a->ui.lfo_used = 1;
}
/* P1.5b: the step latch in MODES, no finger on a step: the black keys choose the hold mode (as with nothing held) */
int qd_ctl_latch_modes(const qd_app_t *a)
{
    const qd_ui_t *u = &a->ui;
    return u->mode == QD_MODE_STEP && u->slatch && u->lmode == QD_LMODE_MODES && !u->wk_down && !u->wk_pending;
}
static int w_latch_modes(const qd_app_t *a) { return qd_ctl_latch_modes(a); }
static int w_latch_default(const qd_app_t *a) { return qd_step_latch_default(a); }
static void k_latch_default(qd_app_t *a, int key, int b, int down, int vel)
{
    (void)key, (void)vel;
    qd_step_value_key_default(a, b, down);
}
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
    {w_type, k_type_white},            /* P1.5u: a name typed: L..Z, SPACE (SHIFT: the signs) */
    {w_seq_held, k_pattern},           /* SEQ held: A01 .. A16 (SHIFT: at once) */
    {w_sel_held, k_sel_white},         /* SEL held: W1..W8 mute, W9..W16 solo */
    {w_tools, k_tools_white},          /* T1: TOOLS: W1..W6 the track */
    {w_tracks_held, k_track_select},   /* FX (TRACKS) held: tracks 1..8 (over the pages too) */
    {w_lib, k_lib_white},              /* P1.5a: the sounds page's slots, the projects */
    {w_menu_w, k_none},                /* P1.3y: a settings menu: nothing (the track settings: the steps, below) */
    {w_main_layer, k_flayer_white},    /* P1.6h: a main page's steps, F1 / F2 held: the step view's copy, pick-up / drop */
    {w_panel, k_panel_white},          /* P1.4b: the track settings, the mix, the effects: the steps (P1.5b: the
                                        * overview's W1..W8 the track) */
    {w_mfx, k_mfx_white},              /* P1.6c: the MIDI FX screen: the track through the group (the lanes' steps) */
    {w_snd_mk, k_snd_mk},              /* E4: the editor, B1 held: W1..W8 the modulation source */
    {w_snd_keys, k_snd_note},          /* P1.5b: the sound editor's keyboard */
    {w_keys_mode, k_note},             /* the keyboard */
    {w_flayer_latch, k_flayer_white},  /* P1.5m: the latch's selection, F1 / F2 / F3 down: the layer's W */
    {w_steps_held, k_step},            /* steps held: more held steps */
    {w_f1, k_f1_white},                /* F1: the first W copies, the later ones paste */
    {w_f2, k_f2_white},                /* F2: Form's pick-up / drop */
    {w_f3, k_f3_white},                /* F3: the page's end, start..end */
    {w_step_view, k_step},             /* the step view: the steps */
};
static const qd_key_row_t BLACK_ROWS[] = {
    {w_type, k_type_black},            /* P1.5u: a name typed: A..K (SHIFT: 0..9 -) */
    {w_seq_held, k_bank},              /* SEQ held: B1..B4 the bank (P1.3d) */
    {w_sel_held, k_sel_black},         /* SEL held: B1..B8 select the track */
    {w_shift_keys, k_shift_black},     /* P1.5c: SHIFT's shortcuts (snapshot, copy / paste, tap, click, double, clear) */
    {w_tools, k_tools_black},          /* T1: TOOLS: B1..B7 the tool, B11 the scope */
    {w_lib, k_lib_black},              /* P1.5a: the banks, LIBRARY / POOL, NAME, delete */
    {w_main_layer, k_main_black},      /* P1.6h: .. F1 held: its pages, CLEAR, COPY, UNDO (MIDI FX: B9, B11; B1..B8 stay
                                        * the slots); F2: nothing */
    {w_panel, k_panel_black},          /* P1.4b: the track settings, the mix: the track; the effects: B1..B4 */
    {w_menu, k_none},                  /* P1.3y: a settings menu: nothing */
    {w_mfx, k_mfx_black},              /* P1.6c: the MIDI FX screen: the slots, bypass, remove */
    {w_snd_full, k_snd_note},          /* P1.5b: the editor's keyboard, FULL: the black keys play too */
    {w_snd, k_snd_black},              /* P1.4a: the sound editor: B1..B5 its groups, B11 the page's locks */
    {w_keys_mode, k_note},             /* the keyboard */
    {w_lfo_held, k_lfo_mode},          /* P1.3y: LFO held: the hold mode, the latch untouched */
    {w_mkey, k_mkey},                  /* a mode key held (P1.3e): B11 resets its default, the rest nothing */
    {w_flayer_latch, k_flayer_black},  /* P1.5m: the latch's selection, F1 / F2 / F3 down: the layer's black keys */
    {w_latch_modes, k_mode},           /* P1.5b: the latch in MODES: the hold mode (a finger on a step: the values) */
    {w_steps_held, k_value},           /* steps held: the mode's values, B11 CLEAR */
    {w_f1, k_f1_black},                /* F1: pages, clear page / track, copy / paste a page, undo */
    {w_f2, k_none},                    /* F2: (only the step pick-up / drop, on W) */
    {w_f3, k_f3_black},                /* F3: the rate */
    {w_latch_default, k_latch_default},   /* P1.6d: the latch in VALUES, nothing selected: the track's defaults */
    {w_step_view, k_mode},             /* nothing held: the hold mode */
};
static int w_main_layer(const qd_app_t *a) { return qd_ctl_main_layer(a) != QD_LAYER_NONE; }
static void k_main_black(qd_app_t *a, int key, int b, int down, int vel)
{
    qd_ui_t *u = &a->ui;
    const int mfx = u->latch == QD_LATCH_MFX;
    const uint16_t bit = (uint16_t)(1u << b);
    if (down) {                                         /* (the press decides; its release goes the same way) */
        if (qd_ctl_layer(a) == QD_LAYER_F1 && (!mfx || b == 8 || b == 10)) {
            u->main_f1b |= bit;
            k_f1_black(a, key, b, 1, vel);
        } else if (mfx) {
            qd_mfx_black(a, b, 1);
        }
        return;
    }
    if (u->main_f1b & bit) {
        u->main_f1b &= (uint16_t)~bit;
        k_f1_black(a, key, b, 0, vel);
    } else if (mfx) {
        qd_mfx_black(a, b, 0);
    }
}
static void k_flayer_white(qd_app_t *a, int key, int w, int down, int vel)
{
    const int l = qd_ctl_layer(a);
    if (l == QD_LAYER_F1)
        k_f1_white(a, key, w, down, vel);
    else if (l == QD_LAYER_F2)
        k_f2_white(a, key, w, down, vel);
    else if (l == QD_LAYER_F3)
        k_f3_white(a, key, w, down, vel);
}
static void k_flayer_black(qd_app_t *a, int key, int b, int down, int vel)
{
    const int l = qd_ctl_layer(a);
    if (l == QD_LAYER_F1)
        k_f1_black(a, key, b, down, vel);
    else if (l == QD_LAYER_F3)
        k_f3_black(a, key, b, down, vel);
}
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
        if (held(a, QD_B_PATTERN))
            a->ui.sel_used = 1;                         /* (P1.3y: SEL's map at once) */
        if (held(a, QD_B_MIX))
            a->ui.seq_used |= 1;                        /* (P1.5a: SEQ's map at once, its release no tap) */
        if (held(a, QD_B_MUTE)) {
            a->ui.lfo_used = 1;                         /* (P1.5b: LFO with a key is no latch-mode switch; P1.5h: a key
                                                         * closes its screen) */
            a->ui.lmode_open = 0;
        }
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
/* P1.4b: FX (printed; TRACKS): a tap opens the effects (again: the next page); held + W1..W8 or ALGORITHM: the track.
 * (The tracks screen of the clocks is retired: its LENGTH and RATE are F3's, LIMIT and SWING the track settings',
 * its mutes SEL's) */
static void b_tracks(qd_app_t *a, int down)
{
    if (down) {
        a->ui.tracks_tap = 1;
    } else if (a->ui.tracks_tap) {
        a->ui.tracks_tap = 0;
        if (a->ui.mode == QD_MODE_IDLE)
            return;
        if (a->ui.mode != QD_MODE_STEP)
            set_mode(a, QD_MODE_STEP);
        qd_fx_button(a);
    }
}

/* P1.4b: ENV (printed) = MIX: a tap opens the mix (again: the sends, again the faders). (P1.3b's page follow is on
 * the track settings and the global settings now; its PAGE + W pages are SELECT's and F1's) */
static void b_page(qd_app_t *a, int down)
{
    if (!down || a->ui.mode == QD_MODE_IDLE)
        return;
    if (a->ui.mode != QD_MODE_STEP)
        set_mode(a, QD_MODE_STEP);
    qd_mix_button(a);
}

static void b_shift(qd_app_t *a, int down)
{
    if (down)
        a->ui.shift_used = 0;
}

/* HOME (P1.3e, §8b 2.2): first it closes an open latch (F3's menu, the step latch); else, in the sequencer, it
 * switches its view (PIANO ROLL -> STEPS -> EDIT (P1.3r) -> PIANO ROLL); from any other screen it comes back to the
 * sequencer in its last view */
/* P1.3y: HOME always comes back to the view in use (PIANO ROLL, STEPS or EDIT) and never cycles: from any screen, a
 * latch closed (and the view in the same press), a pop-up gone; SHIFT + HOME cycles ROLL, STEPS, EDIT, GEEK OUT (the owner,
 * 2026-10-25: HOME keeps the view, a GEEK OUT page too, and GEEK OUT comes back on the page last shown) */
static void b_home(qd_app_t *a, int down)
{
    qd_ui_t *u = &a->ui;
    if (!down)
        return;
    if (u->latch == QD_LATCH_BROWSE) {                  /* P1.5t: the browser: a cancel (BACK shows), to the view */
        qd_br_home(a);
        u->slatch = 0;
        qd_step_release_all(a);
        u->home_woke = 0;
        set_mode(a, QD_MODE_STEP);
        return;
    }
    qd_latch_close(a);
    u->seq_used = 0;                                    /* the latched patterns closed */
    u->slatch = 0;
    qd_step_release_all(a);
    u->tk = QD_TK_NONE;
    u->cf_on = 0;
    if (qd_ctl_shift(a) && !u->home_woke) {
        if (QD_VIEW_GEEK(u->view))
            u->geek = (uint8_t)(u->view - QD_VIEW_METERS);   /* (the page kept, a restored one too) */
        u->view = QD_VIEW_GEEK(u->view) ? QD_VIEW_ROLL : u->view == QD_VIEW_EDIT ? QD_VIEW_METERS + u->geek : u->view + 1;
        u->shift_used = 1;
    }
    u->home_woke = 0;
    set_mode(a, QD_MODE_STEP);
}

/* P1.3d (§8b 2.2): LFO (printed) toggles the step latch: as if a step is always held, a W tap selects (ctl_step.c) */
/* P1.3y: LFO held + a black key: that hold mode (the latch untouched); a tap (no black key between) toggles it */
/* P1.5h (the owner: the switch "only pops up and shows after releasing the hold"): LFO held QD_LFO_HOLD_MS alone opens
 * the latch mode's screen while it is held (qd_ctl_ms); SELECT turns VALUES / MODES there (set at once); the release
 * closes it, keeping the mode shown (no toggle); a black key closes it and picks the hold mode */
static void b_lfo(qd_app_t *a, int down)
{
    if (down) {
        a->ui.lfo_used = 0;
        a->ui.lfo_ms = a->ms;
        a->ui.lmode_open = 0;
        return;
    }
    if (a->ui.lmode_open) {                             /* the screen closes; the mode stays as shown */
        a->ui.lmode_open = 0;
        if (a->ui.tk == QD_TK_LMODE)
            a->ui.tk = QD_TK_NONE;
        return;
    }
    if (a->ui.lfo_used || a->ui.mode != QD_MODE_STEP)
        return;
    if (a->ms - a->ui.lfo_ms >= QD_LFO_HOLD_MS)         /* (held long, the screen never opened: nothing) */
        return;
    if (!a->ui.slatch && !QD_LATCH_MAIN(a->ui.latch))
        qd_latch_close(a);                              /* (F3's menu gives way to the selection; P1.4a: the sound
                                                         * editor takes it: the selection's turns are its p-locks;
                                                         * P1.6h: so do the mix, the effects, the MIDI FX screen) */
    qd_step_latch(a, !a->ui.slatch);
    if (a->ui.slatch && (a->ui.latch == QD_LATCH_MIX || a->ui.latch == QD_LATCH_FX))
        qd_step_release_all(a);                         /* (P1.6h: there it starts empty: the knobs the track's until
                                                         * W keys select) */
}

/* P1.6d (the owner's batch): ARP tapped (released under QD_ARP_HOLD_MS, nothing with it) opens the MIDI FX screen
 * (tapped again, or HOME, closes it); SHIFT + ARP held is FILL (the FILL / NOT FILL conditions), at once, while both
 * are down; ARP held QD_ARP_HOLD_MS alone turns the track's MIDI FX off / back on at once (qd_ctl_ms; the owner,
 * 2026-10-25: as the group OFF, the group kept, a popup); TOOLS is SEQ tapped */
static void b_fill(qd_app_t *a, int down)
{
    qd_ui_t *u = &a->ui;
    if (down) {
        u->arp_ms = a->ms;
        u->arp_used = 0;
        if (qd_ctl_shift(a)) {                          /* SHIFT + ARP: FILL */
            u->arp_used = 1;
            u->shift_used = 1;
            u->arp_fill = 1;
            qd_seq_fill(&a->seq, 1);
        }
        return;
    }
    if (u->arp_fill) {
        u->arp_fill = 0;
        qd_seq_fill(&a->seq, 0);
    }
    if (!u->arp_used && a->ms - u->arp_ms < QD_ARP_HOLD_MS && u->mode != QD_MODE_IDLE) {
        if (qd_mfx_open(a))
            qd_latch_close(a);
        else
            qd_mfx_screen(a);
    }
    u->arp_used = 1;
}

/* REC held or tapped while held: arming in the record settings' MODE (P1.3y: LIVE as P1.3x; STEP: the cursor) */
static void rec_arm(qd_app_t *a)
{
    if (a->ui.rec)
        return;
    if (a->proj.rec_mode)
        qd_rec_arm_step(a);
    else
        qd_rec_arm_live(a);
}

/* PLAY (P1.3x): a tap plays / stops on its RELEASE (held QD_PLAY_HOLD_MS it opens F3 instead: qd_ctl_ms); armed and
 * waiting, the tap starts playback with the recording (CAPTURE: the take, from the top); in a CAPTURE take its press
 * marks the end, playback going on; in another live recording it stops both. REC + PLAY (PLAY pressed while REC is
 * held, stopped): armed in the MODE and started at once, on the press (REC's release then no tap) */
static void b_play(qd_app_t *a, int down)
{
    qd_ui_t *u = &a->ui;
    if (down) {
        u->play_down_ms = a->ms;
        u->play_used = 0;
        u->cap_press = qd_seq_tick(&a->seq);
        if (u->cap == 2) {                              /* CAPTURE: the take's end, on the press */
            u->play_used = 1;
            qd_rec_end(a);
        } else if (held(a, QD_B_REC) && !a->seq.playing && !qd_ctl_shift(a)) {
            u->play_used = 1;
            u->rec_down_used = 1;
            rec_arm(a);
            qd_rec_go(a, 0);
        }
        return;
    }
    if (u->play_used)
        return;
    u->play_used = 1;
    if (u->rec && (u->live_wait || u->cap == 1)) {      /* P1.6g: armed: playback and the recording now (any view) */
        qd_rec_go(a, 0);
        return;
    }
    if (a->seq.playing) {
        if (u->rec && !qd_rec_stepping(a))              /* (P1.6g: a live recording ends with the transport) */
            qd_rec_end(a);
        qd_app_stop(a);
    } else {
        qd_app_play(a);
    }
}

/* REC: a tap switches the keybed between the sequencer and the keyboard; held QD_REC_HOLD_MS (P1.3x, qd_ctl_ms) it
 * arms recording in the record settings' MODE (P1.3y: LIVE as P1.3x; STEP: step recording, the cursor); a tap while
 * recording ends it (live: playback goes on). P1.3y: SHIFT + REC opens the record settings (latched; it no longer
 * arms step recording) */
static void b_rec(qd_app_t *a, int down)
{
    qd_ui_t *u = &a->ui;
    if (down) {
        u->rec_down_used = 0;
        u->rec_auto_used = 0;
        u->rec_down_ms = a->ms;
        u->cap_press = qd_seq_tick(&a->seq);            /* (a CAPTURE ended by this tap: here) */
        if (qd_ctl_shift(a)) {
            u->rec_down_used = 1;
            u->shift_used = 1;
            if (u->latch == QD_LATCH_REC) {             /* P1.6d: open: the next page */
                u->rec_page ^= 1;
                u->menu_knob = -1;
                return;
            }
            if (u->mode != QD_MODE_STEP)
                set_mode(a, QD_MODE_STEP);
            u->slatch = 0;
            qd_step_release_all(a);
            qd_latch_open(a, QD_LATCH_REC);
            u->rec_page = 0;
        }
        return;
    }
    /* P1.6g (the owner's recording, in every view): held past QD_REC_HOLD_MS stopped, it armed (qd_ctl_ms; PLAY then
     * starts it); held past it playing with no knob turned, at its release the recording starts and latches; a knob
     * turned while held was a punch-in (automation, while held), nothing latches; a tap while armed or recording
     * stops it (playback goes on); a tap otherwise the view's own job (the keyboard / steps, the editor's keys) */
    if (u->rec_down_used || u->rec_auto_used)           /* (shifted; armed at the hold; a punch-in) */
        return;
    if (a->ms - u->rec_down_ms >= QD_REC_HOLD_MS) {     /* held, no knob: playing, the recording latches */
        rec_arm(a);
        return;
    }
    if (u->rec) {                                       /* a tap while armed or recording: it stops */
        qd_rec_end(a);
        return;
    }
    if (qd_mfx_open(a) || u->latch == QD_LATCH_TRK) {   /* P1.6e: the MIDI FX screen stays: its white keys STEPS / KEYS
                                                         * (2026-10-25: the track settings too) */
        qd_mfx_rec(a);
        return;
    }
    if (qd_snd_open(a)) {                               /* P1.5b: the sound editor stays: its keyboard on / off */
        u->ekeys = !u->ekeys;
        qd_step_release_all(a);
        u->slatch = 0;
        return;
    }
    set_mode(a, u->mode == QD_MODE_KEYS ? QD_MODE_STEP : QD_MODE_KEYS);
}

/* SEL (printed): held, the track map (after QD_TAP_MS, or at once with a key); P1.3y: SHIFT + SEL the global settings
 * (latched); P1.4b: a tap (released before the map shows, no key with it) the track settings (again: their next page) */
static void b_sel(qd_app_t *a, int down)
{
    qd_ui_t *u = &a->ui;
    if (!down) {
        if (!u->sel_used && a->ms - u->sel_ms < QD_TAP_MS && u->mode != QD_MODE_IDLE) {
            if (u->mode != QD_MODE_STEP && u->latch != QD_LATCH_TRK)
                set_mode(a, QD_MODE_STEP);
            qd_trk_button(a);
        }
        return;
    }
    u->sel_ms = a->ms;
    u->sel_used = 0;
    if (qd_ctl_shift(a) && u->mode != QD_MODE_IDLE) {
        u->shift_used = 1;
        u->sel_used = 1;
        if (u->mode != QD_MODE_STEP)
            set_mode(a, QD_MODE_STEP);
        u->slatch = 0;
        qd_step_release_all(a);
        qd_latch_open(a, QD_LATCH_SET);
    }
}
int qd_ctl_sel_map(const qd_app_t *a)
{
    return held(a, QD_B_PATTERN) && a->ui.latch != QD_LATCH_SET && (a->ui.sel_used || a->ms - a->ui.sel_ms >= QD_TAP_MS);
}

/* OCT- / OCT+ (d -1 / +1): the layers in the step view (F1 / F2 / both F3); with steps held: NOTE's octave, SOUND's
 * bank; the keyboard: the octave (step recording: OCT+ a rest, OCT- a step back), both: every voice out (PANIC) */
static void b_oct(qd_app_t *a, int d, int down)
{
    qd_ui_t *u = &a->ui;
    const int both = held(a, QD_B_OCTDN) && held(a, QD_B_OCTUP);
    if (down && qd_ctl_shift(a) && !both && u->mode != QD_MODE_IDLE) {   /* P1.5c: SHIFT + F1 / F2: undo / redo */
        u->shift_used = 1;
        u->oct_eaten |= (uint8_t)(d < 0 ? 1u : 2u);
        qd_shift_undo(a, d);
        return;
    }
    if (qd_mfx_oct(a, d, down))                         /* P1.6c: the MIDI FX screen: F1 / F2 held + B copy / move
                                                         * (P1.6h: no close; STEPS goes on here: the step layers) */
        return;
    if (qd_lib_oct(a, d, down))                         /* P1.5a: the sounds page, the name editor, the projects */
        return;
    if (!down) {
        if (u->oct_tap == d) {                          /* P1.5m: the latch's selection, an OCT tapped (nothing with it):
                                                         * NOTE's octave, SOUND's bank, as for a held step */
            if (!u->oct_tap_used && u->slatch && u->nheld && u->mode == QD_MODE_STEP && !u->latch)
                qd_step_oct(a, d);
            u->oct_tap = 0;
        }
        u->oct_eaten &= (uint8_t)~(d < 0 ? 1u : 2u);   /* (a key a latch ate: free again) */
        if (d < 0) {                                    /* F1 let go: its step copy and page copy end (the clipboard
                                                         * stays: F2's hand stays loaded, as Form's) */
            u->f1_copied = 0;
            u->f1_src = -1;
            u->pg_copied = -1;
            u->b9_down = 0;
        } else {                                        /* F2 let go: the hand empties (Form's) */
            u->f2_loaded = u->f2_holding = 0;
            u->f2_last_w = -1;
            u->f2_knob = 0;
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
        if (qd_rec_stepping(a)) {                       /* step recording: OCT+ a rest, OCT- a step back */
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
    if (qd_snd_keys(a)) {                               /* P1.5b: the editor's keyboard: the octave (both: PANIC) */
        if (both) {
            qd_pool_kill_all(&a->pool);
        } else {
            const int from = a->oct;
            a->oct = clampi(a->oct + d, QD_OCT_MIN, QD_OCT_MAX);
            qd_ctl_octave(a, UI_OCT_KEYS, from, a->oct);
        }
        return;
    }
    if (finger_held(a) && !u->latch) {                  /* NOTE: the entry octave; SOUND: the bank; else nothing */
        qd_step_promote_all(a);
        qd_step_oct(a, d);
        return;
    }
    if (u->slatch && u->nheld && !u->latch) {           /* P1.5m: the latch's selection: a tap is its octave / bank (on
                                                         * the release, qd_ctl_oct_tap), held it is the layer */
        u->oct_tap = both ? 0 : d;
        u->oct_tap_used = both;
    }
    if (qd_latch_oct(a, d))                             /* P1.3d: a latched menu: OCT- back, OCT+ confirm (P1.6h: a
                                                         * main page none: its F1 / F2 the step view's layers) */
        return;
    if (both) {                                         /* P1.3d: both together open F3, latched (P1.6h: from the step
                                                         * view; on a main page nothing) */
        if (!u->latch) {
            qd_latch_open(a, QD_LATCH_F3);
            u->oct_eaten |= 3u;
        }
    } else {
        u->f_ms = a->ms;                                /* the layer's first key */
    }
    if (!u->clip.group)
        qd_clip_group(&u->clip, 1);                     /* a layer's hold: its edits one undo level */
}
static void b_octdn(qd_app_t *a, int down) { b_oct(a, -1, down); }
static void b_octup(qd_app_t *a, int down) { b_oct(a, 1, down); }
void qd_ctl_set_mode(qd_app_t *a, int mode) { set_mode(a, mode); }

/* P1.5a: SAVE: the sounds page, storing the selected track's sound (SAVE again stores); T1b: SHIFT + SAVE the
 * project page (again: the projects) */
static void b_save(qd_app_t *a, int down)
{
    if (!down || a->ui.mode == QD_MODE_IDLE)
        return;
    if (qd_ctl_shift(a)) {
        a->ui.shift_used = 1;
        qd_prj_button(a);
        return;
    }
    qd_lib_save(a);
}

/* P1.5a: SEQ (printed): held, the patterns (their map after QD_TAP_MS, or at once with a key or a knob); the owner,
 * 2026-10-25: a hold latches the map on release (the same keys; SEQ tapped or HOME: closed); T1b: tapped with no map
 * latched, TOOLS (latched; tapped again or HOME: closed). seq_used: bit 0 used while held, bit 1 latched. */
static void b_seq(qd_app_t *a, int down)
{
    qd_ui_t *u = &a->ui;
    if (down) {
        u->seq_ms = a->ms;
        u->seq_used &= 2;
        return;
    }
    if (u->mode == QD_MODE_IDLE || qd_ctl_shift(a))
        return;
    if (!(u->seq_used & 1) && a->ms - u->seq_ms < QD_TAP_MS) {   /* a tap: the latched map closed, else TOOLS */
        if (u->seq_used & 2)
            u->seq_used = 0;
        else
            qd_tools_open(a) ? qd_latch_close(a) : qd_tools_screen(a);
        return;
    }
    u->seq_used = 2;                                    /* a hold (a key or a knob in it too): the map latched */
}
int qd_ctl_seq_map(const qd_app_t *a)
{
    return (a->ui.seq_used & 2) || (held(a, QD_B_MIX) && (a->ui.seq_used || a->ms - a->ui.seq_ms >= QD_TAP_MS));
}

/* P1.4a: EDIT (printed): the sound editor for the selected track, latched (EDIT again or HOME: closed; P1.6h: F1 no
 * more); from another screen (the keyboard, the tracks) it opens over the step view */
static void b_snd(qd_app_t *a, int down)
{
    int keys;
    if (!down || a->ui.mode == QD_MODE_IDLE)
        return;
    keys = a->ui.mode == QD_MODE_KEYS && !a->ui.rec;    /* P1.5b: from the keyboard, its keyboard stays on */
    if (a->ui.mode != QD_MODE_STEP)
        set_mode(a, QD_MODE_STEP);
    qd_snd_button(a);
    if (keys && qd_snd_open(a)) {
        a->ui.ekeys = 1;
        a->ui.slatch = 0;
    }
}

/* the buttons by role (PLAN §8b; what is not built yet: the sound editor, the mix, saving) */
static void (*const BUTTONS[QD_NBTN])(qd_app_t *a, int down) = {
    [QD_B_TRACKS] = b_tracks,          /* tap: the tracks screen; held + white 1..8: select */
    [QD_B_PATTERN] = b_sel,            /* printed SEL, held: the tracks (B select, W mute / solo: the keys' table);
                                        * SHIFT: the global settings (P1.3y) */
    [QD_B_PAGE] = b_page,              /* printed ENV = MIX (P1.4b): the mix, again the sends */
    [QD_B_MUTE] = b_lfo,               /* printed LFO: the step latch on / off (P1.3d) */
    [QD_B_SOUND] = b_snd,              /* printed EDIT: the sound editor (P1.4a) */
    [QD_B_FUNC] = b_shift,             /* SHIFT, held */
    [QD_B_HOME] = b_home,              /* the step view */
    [QD_B_SAVE] = b_save,              /* P1.5a: the sounds page: the track's sound into the LIBRARY; SHIFT: the
                                        * project (T1b) */
    [QD_B_KEYS] = b_fill,              /* ARP: tapped MIDI FX, held the MIDI FX on / off, SHIFT + ARP FILL */
    [QD_B_MIX] = b_seq,                /* printed SEQ, held: the patterns, latched on release (W: the keys' table);
                                        * tapped: TOOLS (T1b) */
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
    if (qd_type_open(a) && qd_type_button(a, button, down))
        return;                                         /* P1.5u: a name typed: F1, F2, SAVE, HOME, SHIFT + a button */
    BUTTONS[button](a, down);
}

/* ================================================================ the knobs === */
/* the selected track's sound: the quick page (KNOB 1..4: FREQ, DECAY, HARM, FDBK) */
static void live_edit(qd_app_t *a, int id, int d, int tk, int knob)
{
    const qd_track_t *t = &a->model.t[a->sel];
    qd_player_live_edit(&a->player, a->sel, id, t->patch[id] + qd_step_id_turn(a, id, d));
    qd_ctl_takeover(a, tk, knob, d, 0, -1);
}

/* the track's sound (PRESETS with nothing held): P1.5t: the library browser (ctl_presets.c) */
static void track_sound(qd_app_t *a, int d) { qd_br_turn(a, d); }

/* SELECT: the page edited (with or without steps held); its picker takes the screen, but in the STEPS view (P1.3g),
 * which shows every page already: there the edited row just moves */
static void page_knob(qd_app_t *a, int d)
{
    qd_ctl_set_page(a, a->ui.page[a->sel] + d);
    if (!QD_VIEW_ROWS(a->ui.view))
        qd_ctl_takeover(a, UI_TK_PAGE, 0, d, 0, -1);
}

/* SHIFT + ALGORITHM: the track's scale, SHIFT + PRESETS: its root (the step view's; P1.5b: the keyboard's and the
 * editor's with the keyboard on too); a track that follows the global key: the project's key; 1 taken */
static int scale_root(qd_app_t *a, int knob, int d)
{
    qd_tset_t *ts = &qd_ctl_pat(a)->trk[a->sel];
    const int g = ts->key_global;
    uint8_t *sc = g ? &a->proj.key_scale : &ts->scale, *rt = g ? &a->proj.key_root : &ts->root;
    if (knob == QD_K_ALGO)
        *sc = (uint8_t)clampi(*sc + d, 0, QD_NSCALE - 1);
    else if (knob == QD_K_PRESETS)
        *rt = (uint8_t)(((*rt + d) % 12 + 12) % 12);
    else
        return 0;
    qd_ctl_takeover(a, knob == QD_K_ALGO ? UI_TK_SCALE : UI_TK_ROOT, 0, d, 0, -1);
    return 1;
}

/* the step view, nothing held: SELECT the page, ALGORITHM the mode's default (SHIFT: the scale), PRESETS the track's
 * sound (SHIFT: the root). P1.3d (§8b 2.2): KNOB 1..4 the track's defaults in the mode's knob layout (KNOB_JOB); in
 * P-LOCK KNOB 1..4 the track's sound (FREQ, DECAY, HARM, FDBK, live) and ALGORITHM the track (1..8, its chooser) */
static int k_stepview(qd_app_t *a, int knob, int d)
{
    switch (knob) {
    case QD_K_SELECT:
        page_knob(a, d);
        return 1;
    case QD_K_ALGO:
        if (qd_ctl_shift(a))                            /* the track's scale */
            return scale_root(a, knob, d);
        if (a->ui.hmode == UI_F_PLOCK) {                /* P-LOCK: the track */
            qd_ctl_select_track(a, clampi(a->sel + d, 0, QD_NTRACK - 1));
            qd_ctl_takeover(a, UI_TK_TRACK, 0, d, 0, -1);
            return 1;
        }
        return qd_step_default_knob(a, d);
    case QD_K_PRESETS:
        if (qd_ctl_shift(a))                            /* the track's root */
            return scale_root(a, knob, d);
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
        qd_ctl_set_tempo(a, a->bpm + qd_ctl_acc(a, d, QD_BPM_MAX - QD_BPM_MIN + 1));
        a->ui.tempo_ms = a->ms ? a->ms : 1u;
    }
}

/* the keyboard (not recording live): SELECT tempo (P1.5z: SHIFT + SELECT is the track now; the swing is F3 +
 * ALGORITHM), PRESETS the sound, KNOB 1..4 the quick page */
static int k_track(qd_app_t *a, int knob, int d)
{
    switch (knob) {
    case QD_K_SELECT:
        tempo_turn(a, d);
        if (!qd_seq_ext_running(&a->seq))
            qd_ctl_takeover(a, UI_TK_TEMPO, 0, d, 0, -1);
        return 1;
    case QD_K_PRESETS:
        if (qd_ctl_shift(a))                            /* P1.5b: the root */
            return scale_root(a, knob, d);
        track_sound(a, d);
        return 1;
    case QD_K_ALGO:
        if (qd_ctl_shift(a))                            /* P1.5b: the scale */
            return scale_root(a, knob, d);
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
    if (knob == QD_K_SELECT) {                          /* P1.3x: SELECT the tempo, its metronome over the map */
        tempo_turn(a, d);
        if (!qd_seq_ext_running(&a->seq))
            qd_ctl_takeover(a, UI_TK_TEMPO, 0, d, 0, -1);
        return 1;
    }
    if (knob != QD_K_ALGO)
        return 1;                                       /* P1.3j: PRESETS has no job in F3 (it went through to the
                                                         * step view and changed the track's sound under the map) */
    p->swing = (uint8_t)clampi(p->swing + d, 50, 80);   /* P1.3x: ALGORITHM the swing, its metronome over the map */
    a->ui.tempo_ms = a->ms ? a->ms : 1u;
    qd_ctl_takeover(a, UI_TK_SWING, 1, d, 0, -1);
    return 1;
}

static int when_f3(const qd_app_t *a) { return qd_ctl_layer(a) == QD_LAYER_F3; }
static int when_seq(const qd_app_t *a) { return a->ui.mode != QD_MODE_IDLE && w_seq_held(a); }
/* SEQ held + ALGORITHM: the launch mode (END BAR BEAT NOW), a project setting */
static int k_seq(qd_app_t *a, int knob, int d)
{
    if (held(a, QD_B_MIX))
        a->ui.seq_used |= 1;
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
static int when_auto(const qd_app_t *a) { return qd_auto_on(a) && !a->ui.nheld; }   /* P1.6d: automation */
static int when_step_rec(const qd_app_t *a) { return a->ui.mode == QD_MODE_KEYS && qd_rec_stepping(a); }
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

/* P1.3y (one meaning a knob): no finger on a step (nothing held, or the step latch's selection), every view: SELECT
 * the page, ALGORITHM the step cursor, PRESETS the track's preset (ctl.h QD_KR_*). SHIFT + ALGORITHM / PRESETS stay
 * the scale / root; a mode key held its default. KNOB 1..4 go on to the selection's values or the track's defaults */
int qd_ctl_layout_role(int knob)
{
    static const uint8_t KNOB_ROLES[3] = {QD_KR_PAGE, QD_KR_CURSOR, QD_KR_PRESET};   /* [SELECT, ALGO, PRESETS] */
    if (knob < QD_K_SELECT || knob > QD_K_PRESETS)
        return QD_KR_NONE;
    return KNOB_ROLES[knob == QD_K_SELECT ? 0 : knob == QD_K_ALGO ? 1 : 2];
}
int qd_ctl_knob_role(const qd_app_t *a, int knob)
{
    (void)a;
    return qd_ctl_layout_role(knob);
}
static int when_geek(const qd_app_t *a)
{
    const qd_ui_t *u = &a->ui;
    return u->mode == QD_MODE_STEP && QD_VIEW_GEEK(u->view) && !u->latch && !u->slatch && !u->nheld && !u->wk_down &&
           qd_ctl_mkey(a) < 0 && !qd_ctl_shift(a);
}
static int k_geek(qd_app_t *a, int knob, int d)   /* SELECT turns the Geek Out pages (round) */
{
    qd_ui_t *u = &a->ui;
    if (knob == QD_K_SELECT && d) {
        u->geek = (uint8_t)((u->view - QD_VIEW_METERS + (d > 0 ? 1 : 3)) % 4);
        u->view = QD_VIEW_METERS + u->geek;
    }
    return 1;
}
static int when_cursor(const qd_app_t *a)
{
    const qd_ui_t *u = &a->ui;
    return u->mode == QD_MODE_STEP && !u->wk_down && (u->slatch || !u->nheld) && qd_ctl_mkey(a) < 0 &&
           !qd_ctl_shift(a);
}
static int k_steps_cursor(qd_app_t *a, int knob, int d)
{
    if (knob == QD_K_SELECT && a->ui.nheld && a->ui.hmode == UI_F_NOTE)
        return 0;                                       /* P1.5o (the owner): NOTE, the latch's selection: SELECT moves
                                                         * its chord (k_held, QD_KJ_CHORD) ; ALGORITHM
                                                         * keeps its role (the cursor) */
    switch (qd_ctl_knob_role(a, knob)) {
    case QD_KR_CURSOR:
        qd_step_cursor(a, d);
        return 1;
    case QD_KR_PRESET:
        if (a->ui.nheld)
            return 0;                                   /* (P1.5v: the latch's selection: its sound lock from the pool,
                                                         * as a finger: k_held) */
        track_sound(a, d);
        return 1;
    case QD_KR_PAGE:
        page_knob(a, d);
        qd_step_cursor_page(a);
        return 1;
    default:
        return 0;
    }
}

/* P1.4a: the sound editor's knobs (ctl_snd.c; PRESETS with nothing held: the track's preset; SHIFT + ALGORITHM / PRESETS
 * there: the scale / root as in the step view) */
static int when_snd(const qd_app_t *a) { return qd_snd_open(a) && !held(a, QD_B_MIX) && !held(a, QD_B_TRACKS); }
static int k_snd(qd_app_t *a, int knob, int d)
{
    if (a->ui.ekeys && qd_ctl_shift(a) && scale_root(a, knob, d))   /* P1.5b: the keyboard on: the scale, the root */
        return 1;
    if (knob == QD_K_PRESETS && !a->ui.nheld) {
        if (qd_ctl_shift(a))
            return 0;
        track_sound(a, d);
        return 1;
    }
    return qd_snd_knob(a, knob, d);
}

/* P1.6c: the MIDI FX screen's knobs (ctl_mfx.c) */
static int when_mfx(const qd_app_t *a) { return qd_mfx_open(a) && !held(a, QD_B_MIX) && !held(a, QD_B_TRACKS); }
static int k_mfx(qd_app_t *a, int knob, int d) { return qd_mfx_knob(a, knob, d); }

/* P1.3y: the settings menus' knobs (KNOB 1..4 their rows; the encoders nothing) */
static int when_menu(const qd_app_t *a) { return QD_MENU_UP(&a->ui); }
static int when_lib(const qd_app_t *a) { return qd_lib_open(a); }
static int when_type(const qd_app_t *a) { return qd_type_open(a); }   /* P1.5u: a name typed: its knobs only */
static int k_libk(qd_app_t *a, int knob, int d) { return qd_lib_knob(a, knob, d); }
static int when_mix(const qd_app_t *a) { return a->ui.mode == QD_MODE_STEP && a->ui.latch == QD_LATCH_MIX; }
static int k_mix(qd_app_t *a, int knob, int d) { return qd_mix_knob(a, knob, d); }
static int when_fx(const qd_app_t *a) { return a->ui.mode == QD_MODE_STEP && a->ui.latch == QD_LATCH_FX; }
static int k_fx(qd_app_t *a, int knob, int d) { return qd_fx_knob(a, knob, d); }
static int k_menu(qd_app_t *a, int knob, int d)
{
    qd_project_t *pr = &a->proj;
    qd_ui_t *u = &a->ui;
    if (u->latch == QD_LATCH_TRK)
        return qd_trk_knob(a, knob, d);
    if (u->latch == QD_LATCH_REC) {
        if (knob == QD_K_SELECT) {                      /* P1.6d: the pages */
            u->rec_page = clampi(u->rec_page + d, 0, 1);
            return 1;
        }
        if (u->rec_page) {                              /* P1.6d, page 2: KNOB 1 AUTOMATION */
            if (knob == QD_K_1)
                pr->rec_auto = d > 0;
            if (knob >= QD_K_1)
                u->menu_knob = knob - QD_K_1;
            return 1;
        }
        switch (knob) {
        case QD_K_1: pr->rec_mode = d > 0; break;
        case QD_K_2: pr->rec_quant = (uint8_t)clampi(pr->rec_quant + d * 5, 0, 100); break;
        case QD_K_3:                                    /* the TAKE: CAPTURE, REPLACE, OVERDUB (stored 0, 2, 1) */
            pr->rec_dub = (uint8_t)((3 - clampi((3 - pr->rec_dub) % 3 + (d > 0 ? 1 : -1), 0, 2)) % 3);
            break;
        case QD_K_4: pr->rec_ekeys = d > 0; break;      /* P1.5b: EDIT KEYS: SCALE / FULL */
        default: break;
        }
    } else {
        switch (knob) {
        case QD_K_1: u->follow = d > 0; break;      /* (2026-10-25: KNOBS gone, PAGE FOLLOW moved to KNOB 1) */
        default: break;
        }
    }
    return 1;
}

/* P1.3y: FX (printed; TRACKS) held + ALGORITHM: the track, a big track picture while turning (its tap stays the
 * tracks screen) */
static int when_fx_held(const qd_app_t *a) { return a->ui.mode != QD_MODE_IDLE && held(a, QD_B_TRACKS); }
static int k_fx_track(qd_app_t *a, int knob, int d)
{
    if (knob != QD_K_ALGO)
        return 0;
    a->ui.tracks_tap = 0;
    qd_ctl_select_track(a, clampi(a->sel + d, 0, QD_NTRACK - 1));
    qd_ctl_takeover(a, UI_TK_TRACKBIG, 0, d, 0, -1);
    return 1;
}

static int when_keys_shift(const qd_app_t *a) { return qd_keys_on(a) && qd_ctl_shift(a); }
/* P1.6d (the owner): the step latch on, SHIFT: ALGORITHM the scale, PRESETS the root, as everywhere (VALUES or MODES,
 * a selection or none; a finger on a step keeps SHIFT's strum / ramp) */
static int when_latch_shift(const qd_app_t *a)
{
    return a->ui.mode == QD_MODE_STEP && a->ui.slatch && qd_ctl_shift(a) && !finger_held(a) &&
           (!a->ui.latch || a->ui.latch == QD_LATCH_F3);
}
/* P1.5z (the owner: "a quick shortcut for changing the track ... Shift + SEL knob"): SHIFT + SELECT the selected track,
 * a track a detent, round 1..8, FX + ALGORITHM's big track takeover; in every view, the sound editor (the new track's
 * sound), the browser (its preview put back first, then the new track's own sound), the mix, the effects. Not in the
 * typewriter (its row is first: SELECT the letter). It was: the keyboard's swing (F3 + ALGORITHM has it) and,
 * elsewhere, the page as without SHIFT */
static int when_shift(const qd_app_t *a) { return qd_ctl_shift(a) && a->ui.mode != QD_MODE_IDLE; }
static int k_shift_track(qd_app_t *a, int knob, int d)
{
    const int t = ((a->sel + d) % QD_NTRACK + QD_NTRACK) % QD_NTRACK;
    if (knob != QD_K_SELECT || !d)
        return 0;
    a->ui.shift_used = 1;
    a->ui.tracks_tap = 0;
    if (a->ui.mode == QD_MODE_STEP && a->ui.latch == QD_LATCH_BROWSE)
        qd_br_track(a, t);                              /* (the preview put back, then the new track) */
    else
        qd_ctl_select_track(a, t);
    qd_ctl_takeover(a, UI_TK_TRACKBIG, 0, d, 0, -1);
    return 1;
}
/* P1.5o (the owner): F2 held + a knob in the sound editor or in P-LOCK: that value's locks cleared, on the held or
 * selected steps, with nothing held on every step of the track; a word says so in the knob's colour */
static int when_unlock(const qd_app_t *a)
{
    return a->ui.mode == QD_MODE_STEP && held(a, QD_B_OCTUP) && !held(a, QD_B_OCTDN) &&
           (qd_snd_open(a) || (a->ui.hmode == UI_F_PLOCK && !a->ui.latch) ||
            a->ui.latch == QD_LATCH_MIX || a->ui.latch == QD_LATCH_FX);   /* (P1.6d: the sends' locks) */
}
static int k_unlock(qd_app_t *a, int knob, int d)
{
    static const qd_rgb KC[7] = {0xffd23fu, UI_KALG, 0xf1f0eau, UI_K1, UI_K2, UI_K3, UI_K4};
    qd_ui_t *u = &a->ui;
    qd_pattern_t *p = qd_ctl_pat(a);
    int id = -1, s, i, n = 0;
    const char *name = "";
    (void)d;
    if (qd_snd_open(a)) {
        static const int SLOT[QD_NKNOB] = {-1, 4, -1, 0, 1, 2, 3};
        const int k = SLOT[knob];
        id = k >= 0 ? qd_snd_ids(a, u->snd_page)[k] : -1;
    } else if (u->latch == QD_LATCH_MIX || u->latch == QD_LATCH_FX) {   /* P1.6d: a send's (a mix value's) locks */
        id = qd_panel_lock_id(a, knob);
        if (id < 0)
            return 1;
    } else if (knob >= QD_K_1) {
        id = qd_step_quick_id(knob - QD_K_1);
    } else if (knob == QD_K_PRESETS) {
        id = QD_LOCK_SOUND;
    }
    if (id == -1 && knob != QD_K_PRESETS)
        return 1;
    u->f2_knob = 1;                                     /* (P1.6h: the page stays on screen, no F2 picture) */
    qd_clip_keep(&u->clip, qd_ctl_cpat(a), a->seq.cur, a->sel);
    for (i = 0; i < (u->nheld ? u->nheld : QD_NSTEP); i++) {
        int16_t v;
        s = u->nheld ? u->held[i] : i;
        if (id == QD_LOCK_SOUND) {
            if (qd_step_is_on(qd_step(p, a->sel, s)) && qd_step(p, a->sel, s)->sound) {
                qd_step_set_sound(p, a->sel, s, 0);
                n++;
            }
            continue;
        }
        n += qd_lock_get(p, a->sel, s, id, &v);
        qd_lock_clear(p, a->sel, s, id);
        if (id == QD_QP_RB1) {                          /* (RATIO B: the pair's other half) */
            n += qd_lock_get(p, a->sel, s, QD_QP_RB2, &v);
            qd_lock_clear(p, a->sel, s, QD_QP_RB2);
        }
    }
    {
        static const char *const MIXN[5] = {"PAN", "DELAY", "REVERB", "CHORUS", "LEVEL"};
        name = id == QD_LOCK_SOUND ? "SOUND" : id == QD_QP_RB1 ? "RATIO B" : id >= QD_LK_PAN ? MIXN[id - QD_LK_PAN]
                                                                                           : qd_quad_value_name(id);
    }
    qd_ctl_confirm_col(a, name, "unlocked", KC[knob]);
    (void)n;
    return 1;
}
/* P1.5h: the latch mode's screen up (LFO held): SELECT VALUES / MODES (set at once, saved), the other knobs nothing */
static int when_lmode(const qd_app_t *a) { return a->ui.lmode_open; }
static int when_metro(const qd_app_t *a) { return a->ui.metro_held && qd_ctl_shift(a); }
static int k_lmode(qd_app_t *a, int knob, int d)
{
    if (knob == QD_K_SELECT && d) {
        a->ui.lmode = d > 0 ? QD_LMODE_MODES : QD_LMODE_VALUES;
        a->ui.lfo_used = 1;
    }
    return 1;
}
static int when_tools(const qd_app_t *a) { return qd_tools_open(a) && !held(a, QD_B_MIX) && !held(a, QD_B_PATTERN); }
static const struct {
    qd_when_fn when;
    int (*fn)(qd_app_t *a, int knob, int d);           /* 1: taken (else the next row) */
} KNOB_ROWS[] = {
    {when_type, qd_type_knob},         /* P1.5u: a name typed: ALGORITHM the cursor, SELECT the character, PRESETS ideas */
    {when_metro, qd_shift_metro_knob}, /* P1.5c: SHIFT + B7 held: any knob the click's level */
    {when_shift, k_shift_track},       /* P1.5z: SHIFT + SELECT: the track (the others go on down the rows) */
    {when_lmode, k_lmode},             /* P1.5h: LFO held, its screen up: SELECT the latch mode */
    {when_unlock, k_unlock},           /* P1.5o: F2 held (the editor, P-LOCK) + a knob: that value unlocked */
    {when_fx_held, k_fx_track},        /* P1.3y: FX held: ALGORITHM the track (over every page) */
    {when_tools, qd_tools_knob},       /* T1: TOOLS: the tool's values */
    {when_keys_shift, scale_root},     /* P1.5b: the keyboard on (the editor's too), SHIFT: ALGORITHM the scale, PRESETS
                                        * the root, recording or not */
    {when_latch_shift, scale_root},    /* P1.6d: the step latch on, SHIFT: the scale, the root */
    {when_lib, k_libk},                /* P1.5a: the sounds page (SELECT / PRESETS), the name editor, the projects */
    {when_menu, k_menu},               /* P1.3y: a settings menu: KNOB 1..4 its rows (P1.4b: the track settings) */
    {when_mix, k_mix},                 /* P1.4b: the mix */
    {when_fx, k_fx},                   /* P1.4b: the effects */
    {when_mfx, k_mfx},                 /* P1.6c: the MIDI FX screen */
    {when_snd, k_snd},                 /* P1.4a: the sound editor: its page's values (a finger: p-locks) */
    {when_seq, k_seq},                 /* SEQ held: ALGORITHM the launch mode */
    {when_f3, k_f3},                   /* F3: SELECT page, ALGO tempo (SHIFT: swing), KNOB 1..4 value, type, start, end */
    {when_auto, k_live},               /* P1.6d: automation recording: the quick page as locks on the step playing */
    {when_geek, k_geek},               /* P2.11: a Geek Out page, nothing held: the knobs do nothing */
    {when_cursor, k_steps_cursor},     /* P1.3y: no finger on a step: ALGORITHM cursor, PRESETS preset, SELECT page */
    {when_steps_held, k_held},         /* a step held: SELECT page, ALGORITHM value, PRESETS sound lock, p-locks */
    {when_mkey, k_mkey_knob},          /* a mode key held (P1.3e): its default */
    {when_step_rec, k_cursor},         /* step recording: PRESETS moves the cursor */
    {when_stepview, k_stepview},       /* the step view: page, the mode's default, the sound, the quick page */
    {when_always, k_track},            /* the keyboard: tempo, the sound, the quick page */
};

/* P1.5b: the acceleration curve: x1 at QD_ACC_SLOW_MS a detent and slower, rising (squared) to steps / 16 at
 * QD_ACC_FAST_MS and faster; several detents in one turn count as that many in its time (after a pause of
 * QD_ACC_SLOW_MS or more: none, however many) */
int qd_knob_accel(int d, uint32_t dt_ms, int steps, int fine)
{
    const int ad = d < 0 ? -d : d, maxm = steps / 16;
    float f, dt;
    if (fine || steps <= QD_ACC_MIN_STEPS || maxm <= 1 || !ad || dt_ms >= QD_ACC_SLOW_MS)
        return d;                                       /* (a turn's first detents after a pause: as they come) */
    dt = (float)dt_ms / (float)ad;
    f = dt >= (float)QD_ACC_SLOW_MS ? 0.f : dt <= (float)QD_ACC_FAST_MS ? 1.f
      : ((float)QD_ACC_SLOW_MS - dt) / (float)(QD_ACC_SLOW_MS - QD_ACC_FAST_MS);
    return d * (1 + (int)((float)(maxm - 1) * f * f + 0.5f));
}
int qd_ctl_acc(const qd_app_t *a, int d, int steps) { return qd_knob_accel(d, a->ui.knob_dt, steps, qd_ctl_shift(a)); }

void qd_ctl_knob(qd_app_t *a, int knob, int d)
{
    qd_ui_t *u = &a->ui;
    size_t i;
    u->last_knob = knob;                                /* (P1.6d: automation's cue) */
    if (knob >= 0 && knob < 8 && d) {                   /* P1.5b: the time since this knob's last detent that way */
        const int dir = d > 0 ? 1 : -1;
        u->knob_dt = u->knob_dir[knob] == dir && u->knob_ms[knob] ? a->ms - u->knob_ms[knob] : 100000u;
        u->knob_ms[knob] = a->ms ? a->ms : 1u;
        u->knob_dir[knob] = dir;
    }
    touch(a);
    for (i = 0; i < sizeof KNOB_ROWS / sizeof KNOB_ROWS[0]; i++)
        if (KNOB_ROWS[i].when(a) && KNOB_ROWS[i].fn(a, knob, d))
            return;
}

/* ================================================================ time === */
void qd_ctl_ms(qd_app_t *a)
{
    qd_ui_t *u = &a->ui;
    qd_lib_ms(a);                                       /* P1.5a: the sounds page's sound heard */
    qd_snd_rnd_ms(a);                                   /* P1.5c: the editor's dice, its bottle */
    qd_snd_fkey_ms(a);                                  /* P1.6d: the FILTER key held: the machines' popup */
    if (a->buttons_held == 1u << QD_B_KEYS && !u->arp_used && a->ms - u->arp_ms >= QD_ARP_HOLD_MS &&
        u->mode != QD_MODE_IDLE) {
        qd_tset_t *ts = &qd_ctl_pat(a)->trk[a->sel];   /* the owner, 2026-10-25: ARP held: the track's MIDI FX off
                                                         * (as the group OFF, the group kept), again: back on */
        u->arp_used = 1;
        if (ts->mfx & 0x7fu)
            ts->mfx ^= QD_TMFX_OFF;
        qd_ctl_confirm(a, qd_mfx_tgroup(a) ? "MIDI FX ON" : "MIDI FX OFF", 0);
    }
    qd_tools_ms(a);                                     /* T1: TOOLS' detents written (combined) */
    if (u->arp_fill && !(held(a, QD_B_KEYS) && qd_ctl_shift(a))) {   /* (FILL while SHIFT and ARP are both down) */
        u->arp_fill = 0;
        qd_seq_fill(&a->seq, 0);
    }
    if (held(a, QD_B_MUTE) && !u->lfo_used && !u->lmode_open && u->mode == QD_MODE_STEP &&
        a->ms - u->lfo_ms >= QD_LFO_HOLD_MS)
        u->lmode_open = 1;                              /* P1.5h: LFO held alone: the latch mode's screen, while held */
    qd_step_promote_due(a);                             /* a white key down 250 ms: held */
    f1_timers(a);                                       /* F1: a page key held 400 ms, B9 held 600 ms */
    sel_seq_timers(a);                                  /* SEL + W held: solo; SEQ + W held: at once */
    qd_mix_ms(a);                                       /* the mix: B1..B6 held, the mute */
    if (u->pv_note >= 0 && a->ms - u->pv_ms >= QD_PREVIEW_MS)
        qd_ctl_preview(a, -1, 0);                       /* a knob's note: its moment is over */
    if (u->tk == QD_TK_PRESET && u->tk_hold) {         /* P1.5v: the step's pool picker: up while the step is held */
        if (!u->nheld)
            u->tk = QD_TK_NONE, u->tk_hold = 0, u->tk_lock = 0;   /* (let go: closed, the choice kept) */
    } else if (u->tk == QD_TK_MFX_STEP && u->tk_hold) {   /* P1.6e: STEPS' steps held: up while they are */
        if (!u->mfx_wheld)
            u->tk = QD_TK_NONE, u->tk_hold = 0;
    } else if (u->tk == UI_TK_TOOL && u->tk_knob != 1 && a->ms - u->tk_ms >= QD_TOOL_POP_MS) {
        u->tk = QD_TK_NONE;                             /* (T1: a tool's name, a message: ~0.8 s) */
    } else if ((u->tk == UI_TK_SCALE || u->tk == UI_TK_ROOT) && qd_ctl_shift(a)) {
        u->tk_ms = a->ms;                               /* (the key's popup: up while SHIFT is held, .. */
        u->tk_hold = 1;
    } else if ((u->tk == UI_TK_SCALE || u->tk == UI_TK_ROOT) && u->tk_hold) {
        u->tk_ms = a->ms - (QD_TK_MS + QD_TK_FADE_MS - 250u);   /* .. gone 250 ms after its release) */
        u->tk_hold = 0;
    } else if (u->tk != QD_TK_NONE &&
               a->ms - u->tk_ms >= (u->tk == UI_TK_TAP ? QD_TAP_TK_MS : QD_TK_MS) + QD_TK_FADE_MS) {
        u->tk = QD_TK_NONE;                             /* (P1.5z: TAP's stays 2 s after the last tap) */
    }
    if (u->cf_on && a->ms - u->cf_ms >= QD_CONFIRM_MS)
        u->cf_on = 0;
    if (a->seq.flash_sample[a->sel] != u->flash_seen && a->seq.flash_sample[a->sel] <= a->seq.now) {
        u->flash_seen = a->seq.flash_sample[a->sel];    /* a trig of the selected track: the held step's? */
        if (u->nheld && a->seq.flash_step[a->sel] == u->held[0]) {
            u->held_play_ms = a->ms;
            u->held_played = 1;
        }
    }
    if (!a->seq.playing || u->fol_t != a->sel) {         /* P1.5q: the step the selected track plays (EDIT follows it) */
        u->fol_step = -1;
        u->fol_g = -1;
        u->fol_t = a->sel;
    } else if (a->seq.heard_step[a->sel] >= 0 && a->seq.heard_sample[a->sel] <= a->seq.now &&
               a->seq.heard_g[a->sel] != u->fol_g) {   /* (a new step: its first note heard, muted or not; its other
                                                         * notes, its ratchet's hits the same step) */
        u->fol_g = a->seq.heard_g[a->sel];
        u->fol_step = a->seq.heard_step[a->sel];
        u->fol_ms = a->ms;
    }
    if (u->rec_live && !a->seq.playing)                 /* the transport stopped under live notes: they end */
        qd_rec_key(a, -1, 0, 0);
    if (held(a, QD_B_PLAY) && !u->play_used && a->ms - u->play_down_ms >= QD_PLAY_HOLD_MS) {
        u->play_used = 1;                               /* P1.3x: PLAY held: F3, latched (the step view) */
        if (u->mode == QD_MODE_STEP && qd_ctl_layer(a) == QD_LAYER_NONE) {
            qd_latch_open(a, QD_LATCH_F3);
            if (!u->clip.group)
                qd_clip_group(&u->clip, 1);
        }
    }
    if (held(a, QD_B_REC) && !u->rec_down_used && !u->rec_auto_used && a->ms - u->rec_down_ms >= QD_REC_HOLD_MS &&
        !a->seq.playing && u->mode != QD_MODE_IDLE) {
        u->rec_down_used = 1;                           /* P1.6g: stopped, REC held: armed, latched, in any view (the
                                                         * record settings' MODE: LIVE waits for the first note or PLAY;
                                                         * STEP the cursor) */
        rec_arm(a);
    }
    qd_rec_ms(a);
    qd_auto_ms(a);                                      /* P1.6d: an automation take ends */
    qd_mfx_ms(a);                                       /* P1.6e: a slot key let go however: its move dropped */
    {   /* page follow: the page edited moves when the playhead goes to another page (a page chosen by hand stays
         * until then); not while steps are held or a layer is */
        const int pp = qd_ctl_playing_page(a, a->sel);
        if (pp < 0) {
            u->play_page[a->sel] = -1;
        } else if (pp != u->play_page[a->sel]) {
            u->play_page[a->sel] = pp;
            if (u->follow && u->nheld == 0 && !u->wk_down && qd_ctl_layer(a) == QD_LAYER_NONE &&
                !(u->mode == QD_MODE_KEYS && u->rec) && !qd_tools_open(a))   /* (TOOLS: the page SELECT chose stays,
                                                         * the one its W keys and a PAGE change work on) */
                qd_ctl_set_page(a, pp);
        }
    }
    if (u->mode != QD_MODE_IDLE && !a->seq.playing && a->ms - u->touch_ms >= QD_IDLE_MS && !a->keys_held &&
        !a->buttons_held) {
        set_mode(a, QD_MODE_IDLE);                      /* two minutes untouched, stopped: the idle screen */
        u->tk = QD_TK_NONE;
    }
}
