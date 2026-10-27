/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* The track settings, the mix and the effects (P1.4b; PLAN.md §8b "P1.4b, track settings and the mixer" and the
 * owner's 2.3 additions; ctl.h). Built through ctl_latch.c (it includes this file: three latched pages, the latch rule:
 * OCT- or HOME close them; P1.6h: the mix and the effects HOME only, F1 / F2 there the step view's layers; the core's
 * source lists stay as they are).
 *
 *   SEL tapped (held: the track map, as before)  THE TRACK SETTINGS of the selected track in the pattern: KNOB 1 MODE
 *        (GLOBAL, a new track's default: it follows the project's key and SCALE / ROOT edit that one; LOCAL: its own,
 *        kept while GLOBAL; never set, a copy of the global one at the switch), KNOB 2 SCALE, KNOB 3 ROOT, KNOB 4 MIDI CHANNEL; W the steps (a tap
 *        makes, removes); REC tapped: the keyboard on / off, the page stays (W then notes); SEL again (or SELECT): the
 *        second page, KNOB 1 VOICES (the limit), KNOB 2 SWING on / off, KNOB 3 PAGE FOLLOW (the global setting's),
 *        KNOB 4 MIDI FX; the third, KNOB 1 KIND, KNOB 2 NAME, (KNOB 3 ENGINE: next), KNOB 4 MIDI OUT (FIZZ MIDI (c): OFF,
 *        1..16, the track's notes to USB on that channel); B1..B8 the track
 *   ENV (printed) = MIX  six faders with their pan dials and sends; KNOB 1 LEVEL, KNOB 2 PAN, KNOB 3 DELAY, KNOB 4
 *        REVERB of the selected track; MIX again (or SELECT): the SENDS, every track's three (KNOB 1 CHORUS, KNOB 2
 *        DELAY, KNOB 3 REVERB); again: the faders. On both pages: B1..B6 tapped the track, held QD_SEL_HOLD_MS its
 *        mute (at the hold), double-tapped (the second press within QD_CLICK_MS) its solo; B11 every mute and solo
 *        off; ALGORITHM the track; PRESETS left mutes the selected track, right unmutes it
 *   FX (printed) tapped  THE EFFECTS (global, ChoralRoot's: no sound load touches them): DELAY (TIME, SPREAD,
 *        FEEDBACK, SEND), DELAY 2 (SYNC, MODE, TONE, WOW), REVERB (SIZE, DAMP, DECAY, SEND), REVERB 2 (PRE-DELAY, MOD,
 *        LOW CUT, WIDTH), CHORUS (RATE, DEPTH, WIDTH, SEND), MASTER (the master filter: LOW CUT, HIGH CUT, SLOPE, ON);
 *        B1 SENDS, B2 CHORUS, B3 DELAY, B4 REVERB, B5 MASTER, B6 ROUTING (again: an effect's second page), FX again or SELECT the next page.
 *        SENDS (2026-10-26, the sequencer clean-up; was the OVERVIEW): the current track's three sends as bars (KNOB
 *        1..3; SHIFT + SELECT or FX + ALGORITHM the track), the page's steps on the lane and the white keys as the
 *        EDIT home (held: a send is the step's lock), REC the keyboard on / off as the sound editor's.
 *        ROUTING (2026-10-26, ChoralRoot 0.27's): KNOB 1 the mode (PARALLEL / CHAIN), ALGORITHM (and KNOB 2, and
 *        SENDS' KNOB 4) the selected track's DRY / WET (mix.h dry: both modes; CHAIN runs the effects in series with
 *        each track's sends as the wet / dry of each); a
 *        block's page key held (B2 CHORUS, B3 DELAY, B4 REVERB, B5 FILTER) lifts it and KNOB 1 moves it (qd_fx_route_move).
 *        SEND is the selected track's send into that effect; TIME / SPREAD are the free or the synced value by SYNC
 *        (ChoralRoot's FC_TIME, FC_SPREAD). FX held + ALGORITHM still chooses the track. */

const qd_fxpage_t QD_FX_PAGES[QD_FX_NPAGE] = {
    {"SENDS", QD_FXG_OVER, UI_FXD_OVER, {QD_FXC_SENDX(2), QD_FXC_SENDX(0), QD_FXC_SENDX(1), QD_FXC_DRY}},
    {"CHORUS", QD_FXG_CHORUS, UI_FXD_CHORUS, {QD_FXP_CRATE, QD_FXP_CDEPTH, QD_FXP_CWIDTH, QD_FXC_SEND}},   /* (2026-10-26:
                                                         * the owner: the pages and B2..B4 in SENDS' order, C D R) */
    {"DELAY", QD_FXG_DELAY, UI_FXD_DELAY, {QD_FXC_TIME, QD_FXC_SPREAD, QD_FXP_DFDBK, QD_FXC_SEND}},
    {"DELAY 2", QD_FXG_DELAY, UI_FXD_DELAY2, {QD_FXP_DSYNC, QD_FXP_DMODE, QD_FXP_DTONE, QD_FXP_DWOW}},
    {"REVERB", QD_FXG_REVERB, UI_FXD_REVERB, {QD_FXP_RSIZE, QD_FXP_RDAMP, QD_FXP_RDECAY, QD_FXC_SEND}},
    {"REVERB 2", QD_FXG_REVERB, UI_FXD_REVERB2, {QD_FXP_RPRE, QD_FXP_RMOD, QD_FXP_RLOW, QD_FXP_RWIDTH}},
    {"MASTER", QD_FXG_MASTER, UI_FXD_MASTER, {QD_FXP_FLOW, QD_FXP_FHIGH, QD_FXP_FSLOPE, QD_FXP_FON}},
    {"ROUTING", QD_FXG_ROUTE, UI_FXD_ROUTE, {QD_FXP_RMODE, QD_FXC_DRY, QD_FXC_NONE, QD_FXC_NONE}},
};
const char *const QD_FX_NAME[QD_NFXG] = {"DELAY", "REVERB", "CHORUS", "MASTER", "SENDS", "ROUTING"};
const uint8_t QD_FX_KEY[QD_FX_NKEY] = {QD_FXG_OVER, QD_FXG_CHORUS, QD_FXG_DELAY, QD_FXG_REVERB, QD_FXG_MASTER,
                                       QD_FXG_ROUTE};

int qd_fx_cell_id(int c)
{
    if (c == QD_FXC_TIME)
        return qd_fx_get(QD_FXP_DSYNC) ? QD_FXP_DDIV : QD_FXP_DTIME;
    if (c == QD_FXC_SPREAD)
        return qd_fx_get(QD_FXP_DSYNC) ? QD_FXP_DRATIO : QD_FXP_DSPREAD;
    return c;
}
int qd_fx_send_id(int fx) { return fx == 0 ? QD_LK_DELAY : fx == 1 ? QD_LK_REVERB : QD_LK_CHORUS; }
int qd_fx_send(const qd_app_t *a, int t, int fx)
{
    const qd_mix_track_t *m = &a->model.t[t].mix;
    return fx == 0 ? m->delay : fx == 1 ? m->reverb : m->chorus;
}

static void panel_open(qd_app_t *a, int kind)
{
    qd_ui_t *u = &a->ui;
    /* (2026-10-26, the owner: the latch and its selection stay on every page, as on EDIT: the W keys there select, the
     * knobs lock on the selection; the white keys' LEDs show the steps everywhere now, so nothing locks unseen. P1.6h
     * let a selection go on opening the mix; P1.6d the track settings) */
    u->tk = QD_TK_NONE;
    u->sel_down &= 0xFFu;                               /* (the mix's black keys: none down) */
    qd_latch_open(a, kind);
    u->menu_knob = -1;
}

/* the buttons (the step view first: ctl.c's b_* set it) */
void qd_trk_button(qd_app_t *a)
{
    if (a->ui.latch == QD_LATCH_TRK) {
        a->ui.trk_page = (a->ui.trk_page + 1) % 3;      /* (P1.6d: a third page, KIND and NAME) */
        a->ui.menu_knob = -1;
        return;
    }
    panel_open(a, QD_LATCH_TRK);
    a->ui.trk_page = 0;
}
void qd_mix_button(qd_app_t *a)
{
    if (a->ui.latch == QD_LATCH_MIX) {
        a->ui.mix_page ^= 1;
        a->ui.menu_knob = -1;
        return;
    }
    panel_open(a, QD_LATCH_MIX);
    a->ui.mix_page = 0;
}
void qd_fx_button(qd_app_t *a)
{
    if (a->ui.latch == QD_LATCH_FX) {
        a->ui.fx_page = (a->ui.fx_page + 1) % QD_FX_NPAGE;
        a->ui.menu_knob = -1;
        return;
    }
    panel_open(a, QD_LATCH_FX);                         /* (2026-10-26, the owner: on the page it was left on; P1.5b
                                                         * opened it on SENDS) */
    a->ui.ekeys = 0;                                    /* (2026-10-26: on the steps; REC the keyboard) */
}
int qd_fx_open(const qd_app_t *a) { return a->ui.mode == QD_MODE_STEP && a->ui.latch == QD_LATCH_FX; }
int qd_fx_sends_open(const qd_app_t *a)
{
    return a->ui.mode == QD_MODE_STEP && a->ui.latch == QD_LATCH_FX && QD_FX_PAGES[a->ui.fx_page].fx == QD_FXG_OVER;
}
/* 2026-10-26 (the owner: the sends "should update during playback", as the sound editor's pictures do; "not when
 * locked or while holding a keyboard key"): playing, the selected track's step just heard, nothing held or selected,
 * the step latch off, no key down */
int qd_fx_sends_follows(const qd_app_t *a)
{
    const qd_ui_t *u = &a->ui;
    if (!a->seq.playing || u->fol_step < 0 || u->fol_t != a->sel || u->nheld || u->wk_pending || u->slatch ||
        a->keys_held)
        return -1;
    return u->fol_step;
}
int qd_fx_send_shown(const qd_app_t *a, int fx, int *locked)
{
    const qd_ui_t *u = &a->ui;
    const int fs = qd_fx_sends_follows(a);
    int16_t x;
    *locked = 1;
    if (u->nheld && qd_lock_get(qd_ctl_cpat(a), a->sel, u->held[0], qd_fx_send_id(fx), &x))
        return x;                                       /* (P1.6d: the held step's lock) */
    if (fs >= 0 && qd_lock_get(qd_ctl_cpat(a), a->sel, fs, qd_fx_send_id(fx), &x))
        return x;                                       /* (the step just heard: its lock) */
    *locked = 0;
    return qd_fx_send(a, a->sel, fx);
}

/* the mix's black keys: sel_down / sel_done's bits 8..13 (SEL + W keeps 0..7), sel_t0 the press, dt_w -1 - b the
 * last tap (dt_ms its release) */
#define MIXB(b) ((uint16_t)(0x100u << (b)))
static void mix_black(qd_app_t *a, int b, int down)
{
    qd_ui_t *u = &a->ui;
    const uint16_t bit = MIXB(b);
    if (b == 10 && down)
        a->seq.mute = a->seq.solo = 0;
    if (b >= QD_NTRACK)
        return;
    if (down) {
        u->sel_down |= bit;
        u->sel_done &= (uint16_t)~bit;
        u->sel_t0[b] = a->ms;
        if (u->dt_w == -1 - b && a->ms - u->dt_ms < QD_CLICK_MS) {   /* a double tap: solo */
            a->seq.solo ^= (uint8_t)(1u << b);
            u->sel_done |= bit;
            u->dt_w = 0;
        }
        return;
    }
    if (!(u->sel_down & bit))
        return;
    u->sel_down &= (uint16_t)~bit;
    if (!(u->sel_done & bit)) {                         /* a tap: the track */
        qd_ctl_select_track(a, b);
        u->dt_w = -1 - b;
        u->dt_ms = a->ms;
    }
}
/* the mix's black keys held: the mute at the hold */
void qd_mix_ms(qd_app_t *a)
{
    qd_ui_t *u = &a->ui;
    int b;
    if (u->latch != QD_LATCH_MIX)
        return;
    for (b = 0; b < QD_NTRACK; b++)
        if ((u->sel_down & MIXB(b)) && !(u->sel_done & MIXB(b)) && a->ms - u->sel_t0[b] >= QD_SEL_HOLD_MS) {
            a->seq.mute ^= (uint8_t)(1u << b);
            u->sel_done |= MIXB(b);
        }
}

/* the black keys: B1..B8 the track (settings); the mix: mix_black; B1..B4 the effect (again: its next page) */
void qd_panel_black(qd_app_t *a, int b, int down)
{
    qd_ui_t *u = &a->ui;
    if (u->latch == QD_LATCH_MIX) {
        mix_black(a, b, down);
        return;
    }
    if (u->latch == QD_LATCH_FX) {
        static const int8_t BLK[QD_FX_NKEY] = {-1, QD_RT_CHORUS, QD_RT_DELAY, QD_RT_REVERB, QD_RT_FILTER, -1};
        int p, first = -1, next = -1, g;
        if (b >= QD_FX_NKEY)
            return;
        if (QD_FX_PAGES[u->fx_page].fx == QD_FXG_ROUTE && BLK[b] >= 0) {   /* ROUTING (2026-10-26): the block's page key
                                                         * held lifts its block (KNOB 1 moves it); a tap: its page */
            if (down) {
                u->fx_bheld = b + 1, u->fx_bused = 0, u->fx_bms = a->ms;
                return;
            }
            if (u->fx_bheld != b + 1)
                return;
            u->fx_bheld = 0;
            if (u->fx_bused || a->ms - u->fx_bms >= QD_TAP_MS)
                return;
        } else if (!down) {
            return;
        }
        g = QD_FX_KEY[b];                               /* (P1.5b: B1 OVERVIEW .. B6 ROUTING) */
        for (p = 0; p < QD_FX_NPAGE; p++) {
            if (QD_FX_PAGES[p].fx != g)
                continue;
            if (first < 0)
                first = p;
            if (next < 0 && p > u->fx_page && QD_FX_PAGES[u->fx_page].fx == g)
                next = p;
        }
        u->fx_page = QD_FX_PAGES[u->fx_page].fx == g && next >= 0 ? next : first;
        u->menu_knob = -1;
        return;
    }
    if (!down)
        return;
    if (b < QD_NTRACK)
        qd_ctl_select_track(a, b);
}

static int cl(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

/* the knobs; 1: taken */
int qd_trk_knob(qd_app_t *a, int knob, int d)
{
    qd_ui_t *u = &a->ui;
    qd_tset_t *ts = &qd_ctl_pat(a)->trk[a->sel];
    if (knob == QD_K_SELECT) {
        u->trk_page = cl(u->trk_page + d, 0, 2);
        return 1;
    }
    if (knob < QD_K_1)
        return 1;
    u->menu_knob = knob - QD_K_1;
    if (u->trk_page == 2) {                             /* P1.6d, page 3: KIND, NAME; FIZZ MIDI (c): KNOB 4 MIDI OUT
                                                         * (KNOB 3 ENGINE: the MIDI track's, next) */
        qd_track_t *tk = &a->model.t[a->sel];
        if (knob == QD_K_1)
            qd_track_set_kind(tk, tk->kind < 7 ? cl(tk->kind + d, 0, 6) : d > 0 ? 0 : 6);
        else if (knob == QD_K_2)                        /* a turn: the name keyboard */
            qd_name_open(a, 3, a->sel);
        else if (knob == QD_K_4)                        /* OFF, 1..16 (its notes owed end: app.c mo_check) */
            ts->mout = (uint8_t)cl(ts->mout + d, 0, 16);
        return 1;
    }
    if (!u->trk_page) {
        uint8_t *sc = ts->key_global ? &a->proj.key_scale : &ts->scale;   /* GLOBAL: the project's key */
        uint8_t *rt = ts->key_global ? &a->proj.key_root : &ts->root;
        switch (knob) {
        case QD_K_1:                                    /* MODE: left GLOBAL, right LOCAL (its own key back; never set,
                                                         * CHROMATIC C: a copy of the global one) */
            if (d > 0 && ts->key_global && ts->scale == QD_SCALE_CHROMATIC && !ts->root)
                ts->scale = a->proj.key_scale, ts->root = a->proj.key_root;
            ts->key_global = d < 0;
            break;
        case QD_K_2: *sc = (uint8_t)cl(*sc + d, 0, QD_NSCALE - 1); break;
        case QD_K_3: *rt = (uint8_t)(((*rt + d) % 12 + 12) % 12); break;
        default:                                        /* MIDI CH 1..15 (FIZZ MIDI (a): 16 the remote's; a stored 16
                                                         * stays until turned down) */
            ts->chan = (uint8_t)cl(ts->chan + d, 0, ts->chan > 14 ? ts->chan : 14);
            break;
        }
    } else {
        switch (knob) {
        case QD_K_1: ts->limit = (uint8_t)cl((ts->limit > QD_POOL_LIMIT_MAX ? QD_POOL_LIMIT_MAX : ts->limit) + d, 1,
                                             QD_POOL_LIMIT_MAX); break;   /* (P2.6: up to the unit's voices) */
        case QD_K_2: ts->swing = d > 0; break;
        case QD_K_3: u->follow = d > 0, u->play_page[a->sel] = -1; break;
        default:                                        /* P1.6c: KNOB 4 MIDI FX */
            ts->mfx = (uint8_t)cl(ts->mfx + d, 0, QD_MFX_NGROUP);
            qd_mfx_track_changed(a, a->sel);
            break;
        }
    }
    return 1;
}

int qd_mix_knob(qd_app_t *a, int knob, int d)
{
    static const int16_t ID[2][4] = {{QD_LK_LEVEL, QD_LK_PAN, QD_LK_DELAY, QD_LK_REVERB},
                                     {QD_LK_CHORUS, QD_LK_DELAY, QD_LK_REVERB, -1}};
    qd_ui_t *u = &a->ui;
    const qd_mix_track_t *m = &a->model.t[a->sel].mix;
    int id, v;
    if (knob == QD_K_SELECT) {                          /* the page */
        u->mix_page = cl(u->mix_page + d, 0, 1);
        u->menu_knob = -1;
        return 1;
    }
    if (knob == QD_K_ALGO) {                            /* the track */
        qd_ctl_select_track(a, cl(a->sel + d, 0, QD_NTRACK - 1));
        return 1;
    }
    if (knob == QD_K_PRESETS) {                         /* left: muted, right: heard */
        a->seq.mute = (uint8_t)(d < 0 ? a->seq.mute | (1u << a->sel) : a->seq.mute & ~(1u << a->sel));
        return 1;
    }
    if (knob < QD_K_1)
        return 1;
    id = ID[u->mix_page & 1][knob - QD_K_1];
    if (id < 0)
        return 1;
    u->menu_knob = knob - QD_K_1;
    qd_step_promote_all(a);
    if (u->nheld) {                                     /* P1.6d: a step held (or the latch's selection): locks */
        qd_step_lock_fine(a, id, qd_ctl_acc(a, d, 128));
        return 1;
    }
    if (qd_auto_on(a) && qd_auto_put(a, id, qd_auto_cur(a, id) + qd_ctl_acc(a, d, 128)))
        return 1;                                       /* P1.6d: automation recording */
    v = id == QD_LK_LEVEL ? m->level : id == QD_LK_PAN ? m->pan : id == QD_LK_DELAY ? m->delay
      : id == QD_LK_REVERB ? m->reverb : m->chorus;
    qd_player_live_edit(&a->player, a->sel, id, v + qd_ctl_acc(a, d, 128));
    return 1;
}
/* P1.6d: the lock id a knob turns on the mix / the effects page now (-1 none) */
int qd_panel_lock_id(const qd_app_t *a, int knob)
{
    static const int16_t ID[2][4] = {{QD_LK_LEVEL, QD_LK_PAN, QD_LK_DELAY, QD_LK_REVERB},
                                     {QD_LK_CHORUS, QD_LK_DELAY, QD_LK_REVERB, -1}};
    const qd_ui_t *u = &a->ui;
    if (knob < QD_K_1)
        return -1;
    if (u->latch == QD_LATCH_MIX)
        return ID[u->mix_page & 1][knob - QD_K_1];
    if (u->latch == QD_LATCH_FX) {
        const qd_fxpage_t *pg = &QD_FX_PAGES[u->fx_page];
        const int c = pg->c[knob - QD_K_1];
        if (c == QD_FXC_SEND || (c <= QD_FXC_SENDX(0) && c != QD_FXC_NONE && c > QD_FXC_SENDX(3)))
            return qd_fx_send_id(c == QD_FXC_SEND ? pg->fx : QD_FXC_SENDX(0) - c);
    }
    return -1;
}
/* P1.6d: the W keys on the mix and the effects' pages: held, the steps the sends lock on (a tap does nothing here;
 * 2026-10-26: on every effects page, SENDS included: its W1..W8 no longer pick the track); 1 taken */
int qd_panel_steps(const qd_app_t *a, int w)
{
    (void)w;
    return a->ui.latch == QD_LATCH_MIX || a->ui.latch == QD_LATCH_TRK || a->ui.latch == QD_LATCH_FX;
}

int qd_fx_knob(qd_app_t *a, int knob, int d)
{
    qd_ui_t *u = &a->ui;
    const qd_fxpage_t *pg = &QD_FX_PAGES[u->fx_page];
    int c;
    if (knob == QD_K_SELECT) {
        u->fx_page = (u->fx_page + d % QD_FX_NPAGE + QD_FX_NPAGE) % QD_FX_NPAGE;
        u->menu_knob = -1;
        return 1;
    }
    if (knob == QD_K_ALGO && pg->fx == QD_FXG_ROUTE && !u->fx_bheld) {   /* ROUTING: ALGORITHM the track's DRY / WET
                                                         * (the owner, 2026-10-26; KNOB 2 too) */
        qd_mix_track_t *mt = &a->model.t[a->sel].mix;
        mt->dry = (uint8_t)cl(mt->dry + qd_ctl_acc(a, d, 128), 0, 127);
        u->menu_knob = 1;
        return 1;
    }
    if (knob < QD_K_1)
        return 1;
    if (u->fx_bheld && pg->fx == QD_FXG_ROUTE) {        /* ROUTING, a block held: KNOB 1 moves it a place a detent */
        static const int8_t BLK[QD_FX_NKEY] = {-1, QD_RT_CHORUS, QD_RT_DELAY, QD_RT_REVERB, QD_RT_FILTER, -1};
        if (knob == QD_K_1)
            qd_fx_route_move(BLK[(u->fx_bheld - 1) % QD_FX_NKEY], d);
        u->fx_bused = 1;
        return 1;
    }
    c = pg->c[knob - QD_K_1];
    u->menu_knob = knob - QD_K_1;
    if (c == QD_FXC_NONE)
        return 1;
    if (c == QD_FXC_DRY) {                              /* 2026-10-26: the selected track's DRY / WET */
        qd_mix_track_t *mt = &a->model.t[a->sel].mix;
        mt->dry = (uint8_t)cl(mt->dry + qd_ctl_acc(a, d, 128), 0, 127);
        return 1;
    }
    if (c == QD_FXC_SEND || c <= QD_FXC_SENDX(0)) {    /* the selected track's send into this effect (the overview:
                                                         * the cell's) */
        const int fx = c == QD_FXC_SEND ? pg->fx : QD_FXC_SENDX(0) - c;
        qd_step_promote_all(a);
        if (u->nheld) {                                 /* P1.6d: a step held (or the latch's selection): locks */
            qd_step_lock_fine(a, qd_fx_send_id(fx), qd_ctl_acc(a, d, 128));
            return 1;
        }
        if (qd_auto_on(a) &&
            qd_auto_put(a, qd_fx_send_id(fx), qd_auto_cur(a, qd_fx_send_id(fx)) + qd_ctl_acc(a, d, 128)))
            return 1;                                   /* P1.6d: automation recording */
        qd_player_live_edit(&a->player, a->sel, qd_fx_send_id(fx), qd_fx_send(a, a->sel, fx) + qd_ctl_acc(a, d, 128));
        return 1;
    }
    c = qd_fx_cell_id(c);
    {
        int lo, hi;
        qd_fx_range(c, &lo, &hi, 0);
        qd_fx_set(c, qd_fx_get(c) + qd_ctl_acc(a, d, hi - lo + 1));
    }
    return 1;
}

/* the black keys' LEDs on these pages */
void qd_panel_leds(const qd_app_t *a, uint8_t *black)
{
    const qd_ui_t *u = &a->ui;
    int i;
    for (i = 0; i < 11; i++)
        black[i] = QD_LED_OFF;
    if (u->latch == QD_LATCH_FX) {
        for (i = 0; i < QD_FX_NKEY; i++)
            black[i] = QD_FX_KEY[i] == QD_FX_PAGES[u->fx_page].fx ? QD_LED_LIT : QD_LED_DIM;
        if (QD_FX_PAGES[u->fx_page].fx == QD_FXG_ROUTE)   /* ROUTING: B2..B5 the blocks (glowing; the held one lit) */
            for (i = 1; i <= 4; i++)
                black[i] = u->fx_bheld == i + 1 ? QD_LED_LIT : QD_LED_MID;
        return;
    }
    if (u->latch == QD_LATCH_MIX) {                     /* playing, the selected brightest, muted dim, soloed blinking */
        for (i = 0; i < QD_NTRACK; i++)
            black[i] = (a->seq.solo >> i) & 1 ? ((a->ms / QD_FAST_MS) & 1u ? QD_LED_OFF : QD_LED_LIT)
                     : (a->seq.mute >> i) & 1 ? QD_LED_DIM : i == a->sel ? QD_LED_LIT : QD_LED_MID;
        black[10] = a->seq.mute || a->seq.solo ? QD_LED_LIT : QD_LED_OFF;
        return;
    }
    for (i = 0; i < QD_NTRACK; i++)
        black[i] = i == a->sel ? QD_LED_LIT : QD_LED_DIM;
}
