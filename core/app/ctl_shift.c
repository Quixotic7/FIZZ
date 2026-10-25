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

#define QD_ARM_MS 2000u                /* B11's second press within this */
#define QD_TAP_RESET_MS 2000u          /* a tap this long after the last starts a new count */

const uint8_t QD_SHIFT_KEYS[11] = {1, 0, 1, 1, 1, 1, 1, 1, 1, 0, 1};   /* B1..B11: a shortcut there (P1.5z: B8) */

static void pat_reload(qd_app_t *a)                   /* the tracks' sounds from the pattern (its copy replaced) */
{
    int t;
    for (t = 0; t < QD_NTRACK; t++)
        qd_track_from_tsound(&a->model.t[t], &qd_proj_pat(&a->proj, a->seq.cur)->snd[t]);
    a->snd_pat = a->seq.cur;
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
        if (u->clr_armed && a->ms - u->clr_ms < QD_ARM_MS) {
            u->clr_armed = 0;
            qd_undo_hint(a, QD_UK_CLEAR);
            qd_clip_clear_track(&u->clip, qd_ctl_pat(a), a->seq.cur, a->sel);
            qd_ctl_confirm(a, "CLEARED", 0);
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
    if (a->ui.clr_armed && a->ms - a->ui.clr_ms < QD_ARM_MS)
        key[QD_BLACK_KEY[10]] = (a->ms / QD_FAST_MS) & 1u ? QD_LED_LIT : QD_LED_OFF;
}
