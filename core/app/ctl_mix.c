/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* The track settings, the mix and the effects (P1.4b; PLAN.md §8b "P1.4b, track settings and the mixer" and the
 * owner's 2.3 additions; ctl.h). Built through ctl_latch.c (it includes this file: three latched pages, the latch rule:
 * OCT- or HOME close them; P1.6h: the mix and the effects HOME only, F1 / F2 there the step view's layers; the core's
 * source lists stay as they are).
 *
 *   SEL tapped (held: the track map, as before)  THE TRACK SETTINGS of the selected track in the pattern: KNOB 1 SCALE,
 *        KNOB 2 ROOT, KNOB 3 FOLLOW KEY (the project's global key), KNOB 4 MIDI CHANNEL; SEL again (or SELECT): the
 *        second page, KNOB 1 VOICES (the limit), KNOB 2 SWING on / off, KNOB 3 PAGE FOLLOW (the global setting's);
 *        B1..B8 the track
 *   ENV (printed) = MIX  eight faders with their pan dials and sends; B1..B8 (or SELECT) the track, KNOB 1 LEVEL,
 *        KNOB 2 PAN, KNOB 3 DELAY, KNOB 4 REVERB of it; MIX again: the SENDS, every track's three (KNOB 1 CHORUS,
 *        KNOB 2 DELAY, KNOB 3 REVERB); again: the faders
 *   FX (printed) tapped  THE EFFECTS (global, ChoralRoot's: no sound load touches them): DELAY (TIME, SPREAD,
 *        FEEDBACK, SEND), DELAY 2 (SYNC, MODE, TONE, WOW), REVERB (SIZE, DAMP, DECAY, SEND), REVERB 2 (PRE-DELAY, MOD,
 *        LOW CUT, WIDTH), CHORUS (RATE, DEPTH, WIDTH, SEND), MASTER (the master filter: LOW CUT, HIGH CUT, SLOPE, ON);
 *        B1 DELAY, B2 REVERB, B3 CHORUS, B4 MASTER (again: an effect's second page), FX again or SELECT the next page.
 *        SEND is the selected track's send into that effect; TIME / SPREAD are the free or the synced value by SYNC
 *        (ChoralRoot's FC_TIME, FC_SPREAD). FX held + ALGORITHM still chooses the track. */

const qd_fxpage_t QD_FX_PAGES[QD_FX_NPAGE] = {
    {"OVERVIEW", QD_FXG_OVER, UI_FXD_OVER, {QD_FXC_SENDX(2), QD_FXC_SENDX(0), QD_FXC_SENDX(1), QD_FXC_NONE}},
    {"DELAY", QD_FXG_DELAY, UI_FXD_DELAY, {QD_FXC_TIME, QD_FXC_SPREAD, QD_FXP_DFDBK, QD_FXC_SEND}},
    {"DELAY 2", QD_FXG_DELAY, UI_FXD_DELAY2, {QD_FXP_DSYNC, QD_FXP_DMODE, QD_FXP_DTONE, QD_FXP_DWOW}},
    {"REVERB", QD_FXG_REVERB, UI_FXD_REVERB, {QD_FXP_RSIZE, QD_FXP_RDAMP, QD_FXP_RDECAY, QD_FXC_SEND}},
    {"REVERB 2", QD_FXG_REVERB, UI_FXD_REVERB2, {QD_FXP_RPRE, QD_FXP_RMOD, QD_FXP_RLOW, QD_FXP_RWIDTH}},
    {"CHORUS", QD_FXG_CHORUS, UI_FXD_CHORUS, {QD_FXP_CRATE, QD_FXP_CDEPTH, QD_FXP_CWIDTH, QD_FXC_SEND}},
    {"MASTER", QD_FXG_MASTER, UI_FXD_MASTER, {QD_FXP_FLOW, QD_FXP_FHIGH, QD_FXP_FSLOPE, QD_FXP_FON}},
    {"ROUTING", QD_FXG_ROUTE, UI_FXD_ROUTE, {QD_FXP_XDR, QD_FXP_XCD, QD_FXP_XCR, QD_FXP_XPOS}},
};
const char *const QD_FX_NAME[QD_NFXG] = {"DELAY", "REVERB", "CHORUS", "MASTER", "OVERVIEW", "ROUTING"};
const uint8_t QD_FX_KEY[QD_FX_NKEY] = {QD_FXG_OVER, QD_FXG_DELAY, QD_FXG_REVERB, QD_FXG_CHORUS, QD_FXG_MASTER,
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
    if (kind == QD_LATCH_TRK) {                         /* (P1.6d: the mix and the effects keep the held steps and the
                                                         * latch's selection: their sends lock on them) */
        u->slatch = 0;
        qd_step_release_all(a);
    } else if (u->slatch && u->nheld && !u->sel_panel && !u->wk_down && !u->wk_pending) {
        qd_step_release_all(a);                         /* P1.6h (the owner: "the level kept reverting"): a selection
                                                         * made elsewhere is let go (the latch stays on: W keys
                                                         * select here); one made on the mix or the effects stays */
    }
    u->tk = QD_TK_NONE;
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
    panel_open(a, QD_LATCH_FX);
    a->ui.fx_page = 0;                                  /* (P1.5b: FX opens on the overview) */
}

/* the black keys: B1..B8 the track (settings, mix); B1..B4 the effect (again: its next page) */
void qd_panel_black(qd_app_t *a, int b)
{
    qd_ui_t *u = &a->ui;
    if (u->latch == QD_LATCH_FX) {
        int p, first = -1, next = -1, g;
        if (b >= QD_FX_NKEY)
            return;
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
    if (b < QD_NTRACK)
        qd_ctl_select_track(a, b);
}

/* P1.5b: W1..W8 on the effects' overview: the track (the other pages: nothing) */
void qd_panel_white(qd_app_t *a, int w)
{
    if (a->ui.latch == QD_LATCH_FX && QD_FX_PAGES[a->ui.fx_page].fx == QD_FXG_OVER && w < QD_NTRACK)
        qd_ctl_select_track(a, w);
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
    if (u->trk_page == 2) {                             /* P1.6d, page 3: KIND, NAME */
        qd_track_t *tk = &a->model.t[a->sel];
        if (knob == QD_K_1)
            qd_track_set_kind(tk, tk->kind < 7 ? cl(tk->kind + d, 0, 6) : d > 0 ? 0 : 6);
        else if (knob == QD_K_2)                        /* a turn: the name keyboard */
            qd_name_open(a, 3, a->sel);
        return 1;
    }
    if (!u->trk_page) {
        switch (knob) {
        case QD_K_1: ts->scale = (uint8_t)cl(ts->scale + d, 0, QD_NSCALE - 1), ts->key_global = 0; break;
        case QD_K_2: ts->root = (uint8_t)(((ts->root + d) % 12 + 12) % 12), ts->key_global = 0; break;
        case QD_K_3: ts->key_global = d > 0; break;
        default: ts->chan = (uint8_t)cl(ts->chan + d, 0, 15); break;
        }
    } else {
        switch (knob) {
        case QD_K_1: ts->limit = (uint8_t)cl((ts->limit > QD_POOL_LIMIT_MAX ? QD_POOL_LIMIT_MAX : ts->limit) + d, 1,
                                             QD_POOL_LIMIT_MAX); break;   /* (P2.6: up to the unit's voices) */
        case QD_K_2: ts->swing = d > 0; break;
        case QD_K_3: u->follow = d > 0, u->play_page[a->sel] = -1; break;
        default: ts->mfx = (uint8_t)cl(ts->mfx + d, 0, QD_MFX_NGROUP); break;   /* P1.6c: KNOB 4 MIDI FX */
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
    if (knob == QD_K_SELECT || knob == QD_K_ALGO) {     /* the track */
        qd_ctl_select_track(a, cl(a->sel + d, 0, QD_NTRACK - 1));
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
/* P1.6d: the W keys on the mix and the effects' pages (the overview's W1..W8 the tracks): held, the steps the sends
 * lock on (a tap does nothing here); 1 taken */
int qd_panel_steps(const qd_app_t *a, int w)
{
    return a->ui.latch == QD_LATCH_MIX ||
           (a->ui.latch == QD_LATCH_FX && !(QD_FX_PAGES[a->ui.fx_page].fx == QD_FXG_OVER && w < QD_NTRACK));
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
    if (knob < QD_K_1)
        return 1;
    c = pg->c[knob - QD_K_1];
    u->menu_knob = knob - QD_K_1;
    if (c == QD_FXC_NONE)
        return 1;
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
        return;
    }
    for (i = 0; i < QD_NTRACK; i++)
        black[i] = i == a->sel ? QD_LED_LIT : QD_LED_DIM;
}
