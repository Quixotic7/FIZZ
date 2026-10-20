/* SPDX-License-Identifier: GPL-3.0-only */
/* The track settings, the mix and the effects (P1.4b; PLAN.md §8b "P1.4b, track settings and the mixer" and the
 * owner's 2.3 additions; ctl.h). Built through ctl_latch.c (it includes this file: three latched pages, the latch rule:
 * OCT- or HOME close them; the core's source lists stay as they are).
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
    {"DELAY", 0, UI_FXD_DELAY, {QD_FXC_TIME, QD_FXC_SPREAD, QD_FXP_DFDBK, QD_FXC_SEND}},
    {"DELAY 2", 0, UI_FXD_DELAY2, {QD_FXP_DSYNC, QD_FXP_DMODE, QD_FXP_DTONE, QD_FXP_DWOW}},
    {"REVERB", 1, UI_FXD_REVERB, {QD_FXP_RSIZE, QD_FXP_RDAMP, QD_FXP_RDECAY, QD_FXC_SEND}},
    {"REVERB 2", 1, UI_FXD_REVERB2, {QD_FXP_RPRE, QD_FXP_RMOD, QD_FXP_RLOW, QD_FXP_RWIDTH}},
    {"CHORUS", 2, UI_FXD_CHORUS, {QD_FXP_CRATE, QD_FXP_CDEPTH, QD_FXP_CWIDTH, QD_FXC_SEND}},
    {"MASTER", 3, UI_FXD_MASTER, {QD_FXP_FLOW, QD_FXP_FHIGH, QD_FXP_FSLOPE, QD_FXP_FON}},
};
const char *const QD_FX_NAME[4] = {"DELAY", "REVERB", "CHORUS", "MASTER"};

int qd_fx_cell_id(int c)
{
    if (c == QD_FXC_TIME)
        return qd_fx_get(QD_FXP_DSYNC) ? QD_FXP_DDIV : QD_FXP_DTIME;
    if (c == QD_FXC_SPREAD)
        return qd_fx_get(QD_FXP_DSYNC) ? QD_FXP_DRATIO : QD_FXP_DSPREAD;
    return c;
}
int qd_fx_send_id(int fx) { return fx == 0 ? QD_LK_DELAY : fx == 1 ? QD_LK_REVERB : QD_LK_CHORUS; }

static void panel_open(qd_app_t *a, int kind)
{
    qd_ui_t *u = &a->ui;
    u->slatch = 0;
    qd_step_release_all(a);
    u->tk = QD_TK_NONE;
    qd_latch_open(a, kind);
    u->menu_knob = -1;
}

/* the buttons (the step view first: ctl.c's b_* set it) */
void qd_trk_button(qd_app_t *a)
{
    if (a->ui.latch == QD_LATCH_TRK) {
        a->ui.trk_page ^= 1;
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
}

/* the black keys: B1..B8 the track (settings, mix); B1..B4 the effect (again: its next page) */
void qd_panel_black(qd_app_t *a, int b)
{
    qd_ui_t *u = &a->ui;
    if (u->latch == QD_LATCH_FX) {
        int p, first = -1, next = -1;
        if (b > 3)
            return;
        for (p = 0; p < QD_FX_NPAGE; p++) {
            if (QD_FX_PAGES[p].fx != b)
                continue;
            if (first < 0)
                first = p;
            if (next < 0 && p > u->fx_page && QD_FX_PAGES[u->fx_page].fx == b)
                next = p;
        }
        u->fx_page = QD_FX_PAGES[u->fx_page].fx == b && next >= 0 ? next : first;
        u->menu_knob = -1;
        return;
    }
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
        u->trk_page = cl(u->trk_page + d, 0, 1);
        return 1;
    }
    if (knob < QD_K_1)
        return 1;
    u->menu_knob = knob - QD_K_1;
    if (!u->trk_page) {
        switch (knob) {
        case QD_K_1: ts->scale = (uint8_t)cl(ts->scale + d, 0, QD_NSCALE - 1), ts->key_global = 0; break;
        case QD_K_2: ts->root = (uint8_t)(((ts->root + d) % 12 + 12) % 12), ts->key_global = 0; break;
        case QD_K_3: ts->key_global = d > 0; break;
        default: ts->chan = (uint8_t)cl(ts->chan + d, 0, 15); break;
        }
    } else {
        switch (knob) {
        case QD_K_1: ts->limit = (uint8_t)cl(ts->limit + d, 1, 16); break;
        case QD_K_2: ts->swing = d > 0; break;
        case QD_K_3: u->follow = d > 0, u->play_page[a->sel] = -1; break;
        default: break;
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
    v = id == QD_LK_LEVEL ? m->level : id == QD_LK_PAN ? m->pan : id == QD_LK_DELAY ? m->delay
      : id == QD_LK_REVERB ? m->reverb : m->chorus;
    qd_player_live_edit(&a->player, a->sel, id, v + d);
    return 1;
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
    if (c == QD_FXC_SEND) {                             /* the selected track's send into this effect */
        const qd_mix_track_t *m = &a->model.t[a->sel].mix;
        const int id = qd_fx_send_id(pg->fx);
        qd_player_live_edit(&a->player, a->sel, id,
                            (id == QD_LK_DELAY ? m->delay : id == QD_LK_REVERB ? m->reverb : m->chorus) + d);
        return 1;
    }
    c = qd_fx_cell_id(c);
    qd_fx_set(c, qd_fx_get(c) + d);
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
        for (i = 0; i < 4; i++)
            black[i] = i == QD_FX_PAGES[u->fx_page].fx ? QD_LED_LIT : QD_LED_DIM;
        return;
    }
    for (i = 0; i < QD_NTRACK; i++)
        black[i] = i == a->sel ? QD_LED_LIT : QD_LED_DIM;
}
