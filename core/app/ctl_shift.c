/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* P1.5c (PLAN version 2.6): SHIFT's keyboard and undo / redo. Built through ctl_latch.c (it includes this file).
 *
 *   SHIFT + F1 / F2      undo / redo (undo.h: a gesture a level, eight), a takeover with what it was (a glyph)
 *   SHIFT + B1           the snapshot of the active pattern stored (RAM)      B6   tap tempo (the last taps' average)
 *   SHIFT + B3           .. recalled onto it at once (undoable)               B7   the metronome on / off
 *   SHIFT + B4           the pattern copied (RAM)                             B9   the track doubled (its looped
 *   SHIFT + B5           .. pasted onto the active one (undoable)                  pages copied after themselves)
 *   SHIFT + B11          the track cleared: pressed twice within QD_ARM_MS (the first arms it, B11 blinking)
 *   SHIFT + B8           P1.5z: PAGE FOLLOW on / off (the global one, the settings' KNOB 2 and the track settings' page 2
 *                        KNOB 3 are the same flag), its takeover
 * While SHIFT is held only these keys light (and SHIFT's map draws them). B2, B10 free; the W keys unchanged. P1.5z: in
 * the sound editor too (its map not shown there). SHIFT + B6's taps show the TAP takeover (QD_TAP_TK_MS). */

#define QD_TAP_RESET_MS 2000u          /* a tap this long after the last starts a new count */

const uint8_t QD_SHIFT_KEYS[11] = {1, 0, 1, 1, 1, 1, 1, 1, 1, 0, 1};   /* B1..B11: a shortcut there (P1.5z: B8) */

static void pat_reload(qd_app_t *a)                   /* the tracks' sounds from the pattern (its copy replaced) */
{
    int t;
    for (t = 0; t < QD_NTRACK; t++)
        qd_track_from_tsound(&a->model.t[t], &qd_proj_pat(&a->proj, a->seq.cur)->snd[t]);
    a->snd_pat = a->seq.cur;
}

/* 2026-11-13 (owner): the PATTERNS page's F1 / F2, as the steps' (ctl.c k_f1_white, k_f2_white): F1 + W the first
 * pattern copied into the hand, the next ones pasted; F2 + W Form's pick-up / drop: not loaded, a cut (the slot
 * emptied); loaded: an empty slot pasted, a full one pasted over while holding, else cut. The hand is the pattern
 * clipboard (SHIFT + B4 / B5's: compressed in the undo log on the unit; a pattern too big for it: "too big", nothing
 * cut). The active pattern's paste or cut is one undo level; another's is not (its older levels are forgotten). A
 * pattern out of RAM (the unit keeps two) waits for the prefetch (pc_job), one at a time: a W meanwhile does nothing */
enum { PC_COPY = 1, PC_CUT, PC_PASTE };
int qd_pc_layer(const qd_app_t *a)
{
    const qd_ui_t *u = &a->ui;
    const int dn = ((a->buttons_held >> QD_B_OCTDN) & 1u) && !(u->oct_eaten & 1u);
    const int up = ((a->buttons_held >> QD_B_OCTUP) & 1u) && !(u->oct_eaten & 2u);
    if (!(u->seq_used & 2) || u->mode == QD_MODE_IDLE || qd_ctl_shift(a) || dn == up)
        return 0;
    return dn ? 1 : 2;
}
static void pc_do(qd_app_t *a, int op, int k)
{
    const int cur = k == a->seq.cur;
    if (op != PC_PASTE) {
        if (!qd_hist_store_k(a, 1, k)) {
            qd_ctl_confirm(a, "PATTERN", "too big");
            a->ui.pc_src = -1;
            return;
        }
        if (op == PC_COPY)
            return;
    }
    if (cur)
        qd_undo_hint(a, op == PC_CUT ? QD_UK_CLEAR : QD_UK_PASTE);
    else
        qd_undo_forget_pat(a, k);
    if (op == PC_CUT)
        qd_pat_init(qd_proj_pat(&a->proj, k));
    else
        qd_hist_recall_k(a, 1, k);
    if (cur)
        pat_reload(a);
    a->edit_gen++;
}
static void pc_run(qd_app_t *a, int op, int k)
{
    if (k == a->seq.cur || qd_proj_resident(&a->proj, k)) {
        pc_do(a, op, k);
    } else {
        a->ui.pc_job = (int8_t)k;
        a->ui.pc_op = (int8_t)op;
    }
}
void qd_pc_ms(qd_app_t *a)
{
    const int k = a->ui.pc_job;
    if (k >= 0 && qd_proj_resident(&a->proj, k)) {
        a->ui.pc_job = -1;
        pc_do(a, a->ui.pc_op, k);
    }
}
int qd_pc_white(qd_app_t *a, int k)
{
    qd_ui_t *u = &a->ui;
    const int l = qd_pc_layer(a);
    int op;
    if (!l)
        return 0;
    if (u->pc_job >= 0)
        return 1;
    if (l == 1) {
        op = u->pc_src < 0 ? PC_COPY : PC_PASTE;
        if (op == PC_COPY)
            u->pc_src = (int8_t)k, u->pc_hold = 1;      /* (Form: a copy loads the hand, the next F2 pastes) */
    } else {
        op = !a->undo.pclip_ok || (qd_proj_used(&a->proj, k) && !u->pc_hold) ? PC_CUT : PC_PASTE;
        u->pc_hold = op == PC_CUT;
        u->pc_last = (int8_t)k, u->pc_act = (int8_t)(op == PC_CUT ? 1 : 2);
    }
    pc_run(a, op, k);
    return 1;
}
void qd_pc_up(qd_app_t *a, int d)
{
    qd_ui_t *u = &a->ui;
    if (d < 0)
        u->pc_src = -1;                                 /* (F1 let go: its copy stays in the hand) */
    else
        u->pc_hold = 0, u->pc_last = -1;                /* (F2 let go: the hand stays loaded, not holding) */
}

/* SHIFT + F1 / F2: undo / redo */
void qd_shift_undo(qd_app_t *a, int dir)
{
    qd_ui_t *u = &a->ui;
    const int k = qd_undo_step(a, dir);
    u->undo_dir = dir;
    u->undo_kind = k;                                   /* (-1: nothing there: the takeover says so, dim) */
    u->cf_on = 0;                                       /* (a confirm still up gives way) */
    qd_ctl_takeover(a, QD_TK_UNDO, 0, dir, 0, -1);
}

/* the track doubled: its looped pages copied after the last of them (the loop twice as long); 0: no room (T1: the
 * track tools' op, tools.c) */
static int double_track(qd_app_t *a) { return qd_tl_double(qd_ctl_pat(a), a->sel, 0); }

static void tap_tempo(qd_app_t *a)
{
    qd_ui_t *u = &a->ui;
    int i, n = 0;
    uint32_t sum = 0;
    if (u->ntap && a->ms - u->tap_ms[(u->ntap - 1) % 8] > QD_TAP_RESET_MS)
        u->ntap = 0;
    if (!u->ntap)                                       /* (the TAP ape: no bounce in while its screen is still up) */
        u->tap_cont = u->tk == UI_TK_TAP && a->ms - u->tk_ms < QD_TAP_TK_MS + QD_TK_FADE_MS;
    u->tap_ms[u->ntap % 8] = a->ms;
    u->ntap++;
    for (i = 1; i < (u->ntap < 4 ? u->ntap : 4); i++) {   /* the intervals between the last (up to) four taps */
        const uint32_t t1 = u->tap_ms[(u->ntap - i) % 8], t0 = u->tap_ms[(u->ntap - i - 1) % 8];
        sum += t1 - t0, n++;
    }
    qd_ctl_takeover(a, UI_TK_TAP, 0, 1, 0, -1);        /* P1.5z: the TAP takeover, from the first tap */
    if (n && sum) {
        const int bpm = (int)(60000u * (uint32_t)n / sum);
        if (!qd_seq_ext_running(&a->seq))
            qd_ctl_set_tempo(a, bpm);
        a->ui.tempo_ms = a->ms ? a->ms : 1u;
    }                                                   /* (P1.5z: TAP's takeover, no more TEMPO's) */
}

void qd_shift_black(qd_app_t *a, int b)
{
    qd_ui_t *u = &a->ui;
    qd_hist_t *h = &a->undo;
    if (b != 10)
        u->clr_armed = 0;
    switch (b) {
    case 0:                                             /* B1: the snapshot stored */
        qd_ctl_confirm(a, "SNAPSHOT", qd_hist_store(a, 0) ? "stored" : "too big");   /* (P2.1: the device's room) */
        break;
    case 2:                                             /* B3: recalled, at once */
        if (!h->snap_ok)
            break;
        qd_undo_hint(a, QD_UK_SNAP);
        qd_hist_recall(a, 0);
        pat_reload(a);
        qd_ctl_confirm(a, "SNAPSHOT", "recalled");
        break;
    case 3:                                             /* B4: the pattern copied */
        qd_ctl_confirm(a, "PATTERN", qd_hist_store(a, 1) ? "copied" : "too big");
        break;
    case 4:                                             /* B5: .. pasted onto the active one */
        if (!h->pclip_ok)
            break;
        qd_undo_hint(a, QD_UK_PASTE);
        qd_hist_recall(a, 1);
        pat_reload(a);
        qd_ctl_confirm(a, "PATTERN", "pasted");
        break;
    case 5:                                             /* B6: tap tempo */
        tap_tempo(a);
        break;
    case 6:                                             /* B7: the metronome */
        a->metro = !a->metro;
        a->metro_beat = -1;
        qd_ctl_confirm(a, "CLICK", a->metro ? "on" : "off");
        break;
    case 7:                                             /* P1.5z, B8: page follow on / off */
        u->follow = !u->follow;
        u->play_page[a->sel] = -1;
        qd_ctl_takeover(a, UI_TK_FOLLOW, 0, u->follow ? 1 : -1, 0, -1);
        break;
    case 8:                                             /* B9: the track doubled */
        qd_undo_hint(a, QD_UK_DOUBLE);
        if (double_track(a))
            qd_ctl_confirm(a, "DOUBLED", 0);
        else
            qd_ctl_confirm(a, "FULL", 0);
        break;
    case 10:                                            /* B11: the track cleared, pressed twice */
        if (u->mode == QD_MODE_STEP && QD_VIEW_GEEK(u->view) && !u->latch) {   /* P2.11: on a Geek Out page: the
                                                         * counters from zero (steals, late halves, the longest ISR
                                                         * and lock) */
            qd_view_geek_reset(a);
            qd_ctl_confirm(a, "RESET", 0);
            break;
        }
        if (u->clr_armed == 1 && a->ms - u->clr_ms < QD_ARM_MS) {   /* (2: F2 + B9's own arm) */
            u->clr_armed = 0;
            qd_undo_hint(a, QD_UK_CLEAR);
            qd_clip_clear_track(&u->clip, qd_ctl_pat(a), a->seq.cur, a->sel);
            qd_ctl_confirm(a, "TRACK CLEARED", 0);   /* (2026-11-13, owner: one message) */
        } else {
            u->clr_armed = 1;
            u->clr_ms = a->ms;
            qd_ctl_confirm(a, "CLEAR?", 0);
        }
        break;
    default:
        break;
    }
}

/* SHIFT + B7 held, any knob: the click's level (0..100 %, of the full click) */
int qd_shift_metro_knob(qd_app_t *a, int knob, int d)
{
    static char txt[8];
    int l = a->metro_lvl + d * 5;
    (void)knob;
    a->ui.metro_turned = 1;
    l = l < 0 ? 0 : l > 100 ? 100 : l;
    a->metro_lvl = l;
    snprintf(txt, sizeof txt, "%d", l);
    qd_ctl_confirm(a, "CLICK", txt);
    return 1;
}

/* SHIFT held: only the shortcuts light (B11 blinking while armed); the W keys dark */
void qd_shift_leds(const qd_app_t *a, uint8_t *key)
{
    int i;
    for (i = 0; i < 16; i++)
        key[QD_WHITE_KEY[i]] = QD_LED_OFF;
    for (i = 0; i < 11; i++)
        key[QD_BLACK_KEY[i]] = QD_SHIFT_KEYS[i] ? QD_LED_LIT : QD_LED_OFF;
    if (!a->undo.snap_ok)
        key[QD_BLACK_KEY[2]] = QD_LED_DIM;              /* (nothing to recall, nothing to paste: dim) */
    if (!a->undo.pclip_ok)
        key[QD_BLACK_KEY[4]] = QD_LED_DIM;
    if (a->ui.clr_armed == 1 && a->ms - a->ui.clr_ms < QD_ARM_MS)
        key[QD_BLACK_KEY[10]] = (a->ms / QD_FAST_MS) & 1u ? QD_LED_LIT : QD_LED_OFF;
}
