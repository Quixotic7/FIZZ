/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* T1: TOOLS, the track tools (ctl.h; docs/TRACK-TOOLS-PLAN.md §2, the owner's decisions §8). Built through ctl_latch.c
 * (it includes this file). The operations are seq/tools.c's.
 *
 * LIVE PREVIEW, SEEN AND HEARD (§2): the first change of a tool opens an undo gesture of its own and HOLDS it open
 * (undo.h's hold: the quiet time does not close it); its "before" is the original. Every later change reverts the
 * pattern from that copy (qd_undo_revert_open) and applies the tool with its values now, so the player plays the
 * result with nothing new on its side. Detents are combined: a change marks the state dirty, the next millisecond
 * writes it. OCT+ closes the gesture (one undo level); OCT- reverts and lets it go (no level); HOME, another tool,
 * another scope or another track keep it. When the log cannot keep the original (the device's, a big pattern) the
 * change is DRAWN ONLY: the pattern stays as it is, the screen computes the result, OCT+ writes it without an undo
 * level (the crossed-out arrow says so). */

#define TL_PEND_LIVE(a) (!((a)->ui.tl.flags & QD_TLF_NOLIVE) && (a)->undo.open && (a)->undo.hold)

int qd_tools_open(const qd_app_t *a) { return a->ui.mode == QD_MODE_STEP && a->ui.latch == QD_LATCH_TOOLS; }

static int tl_page(const qd_app_t *a) { return a->ui.page[a->sel] & 7; }

/* EUCLID's longest length in this scope: the page's steps, the track's loop, the longest loop */
static int tl_maxlen(const qd_app_t *a)
{
    const qd_pattern_t *p = qd_ctl_cpat(a);
    const qd_tools_t *T = &a->ui.tl;
    int t, m = 1;
    if (T->scope == QD_TS_PAGE) {
        const int s0 = p->trk[a->sel].pstart[tl_page(a)] & 15, s1 = p->trk[a->sel].pend[tl_page(a)] & 15;
        return s1 >= s0 ? s1 - s0 + 1 : 1;
    }
    if (T->scope == QD_TS_TRACK)
        return qd_track_len(&p->trk[a->sel]);
    for (t = 0; t < QD_PAT_NTRACK; t++)
        m = qd_track_len(&p->trk[t]) > m ? qd_track_len(&p->trk[t]) : m;
    return m;
}

static void tl_norm(qd_app_t *a)                      /* EUCLID's values within the scope's length */
{
    qd_tools_t *T = &a->ui.tl;
    const int mx = tl_maxlen(a);
    if (T->elen < 1)
        T->elen = 1;
    if (T->elen > mx)
        T->elen = (uint8_t)mx;
    if (T->ehits > T->elen)
        T->ehits = T->elen;
    if (T->erot >= T->elen)
        T->erot = 0;
}

static void tl_settle(qd_tools_t *T)                  /* nothing pending: the result is the new original */
{
    T->flags &= (uint8_t)~(QD_TLF_PEND | QD_TLF_NOLIVE | QD_TLF_FLIP | QD_TLF_DIRTY);
    T->rot = 0;
    T->lpow = 0;
}

int qd_tools_pending(qd_app_t *a)
{
    qd_tools_t *T = &a->ui.tl;
    if (!(T->flags & QD_TLF_PEND))
        return 0;
    if ((T->flags & QD_TLF_NOLIVE) || TL_PEND_LIVE(a))
        return 1;
    tl_settle(T);                                       /* (the gesture closed elsewhere: an undo, another operation,
                                                         * a pattern change: what is there now is kept) */
    return 0;
}

/* the tool with its values onto the pattern (from what is there: the original, after a revert). PAGE: the page the
 * change began on (opage: page follow may move the edited page meanwhile) */
static void tl_apply(qd_app_t *a)
{
    qd_pattern_t *p = qd_ctl_pat(a);
    const qd_tools_t *T = &a->ui.tl;
    uint8_t pos[QD_NSTEP];
    const int all = T->scope == QD_TS_PATTERN, t0 = all ? 0 : a->sel, t1 = all ? QD_PAT_NTRACK : a->sel + 1;
    const int page = T->opage & 7;
    int t;
    for (t = t0; t < t1; t++) {
        int n;
        if (T->tool == QD_TL_LENGTH) {
            qd_tl_length(p, t, T->scope == QD_TS_PAGE ? page : -1, T->lpow, 0);
            continue;
        }
        n = qd_tl_positions(p, t, T->scope, page, pos);
        if (T->tool == QD_TL_ROTATE)
            qd_tl_rotate(p, t, pos, n, T->rot);
        else if (T->tool == QD_TL_REVERSE && (T->flags & QD_TLF_FLIP))
            qd_tl_reverse(p, t, pos, n);
        else if (T->tool == QD_TL_EUCLID)
            qd_tl_euclid(p, t, pos, n, T->ehits, T->elen, T->erot, T->emode);
    }
}

static void tl_capture(qd_app_t *a)                   /* the lane as it was (its diff: kept, new, going) */
{
    qd_tools_t *T = &a->ui.tl;
    const qd_pattern_t *p = qd_ctl_cpat(a);
    int w;
    T->opage = (uint8_t)tl_page(a);
    T->orig = 0;
    if (T->tool == QD_TL_LENGTH) {
        T->orig = (uint16_t)qd_track_loop(&p->trk[a->sel]);
        return;
    }
    for (w = 0; w < QD_PAGE; w++)
        if (p->step[a->sel][T->opage * QD_PAGE + w].flags & QD_SF_ON)
            T->orig |= (uint16_t)(1u << w);
}

/* a value changed: the change written (live), or only drawn */
static void tl_change(qd_app_t *a)
{
    qd_tools_t *T = &a->ui.tl;
    if (!qd_tools_pending(a)) {
        qd_undo_hint(a, QD_UK_EDIT);                    /* (a gesture of its own: its before is the original) */
        T->flags = (uint8_t)((T->flags & ~QD_TLF_NOLIVE) | QD_TLF_PEND);
        tl_capture(a);
        if (!qd_undo_can_revert(a)) {                   /* (the log cannot keep it: drawn only) */
            T->flags |= QD_TLF_NOLIVE;
            return;
        }
        a->undo.hold = 1;
        tl_apply(a);
        return;
    }
    if (T->flags & QD_TLF_NOLIVE)
        return;
    if (!qd_undo_revert_open(a)) {                      /* (its copy gone meanwhile: what is there is kept) */
        a->undo.hold = 0;
        tl_settle(T);
        return;
    }
    tl_apply(a);
}

static void tl_keep(qd_app_t *a)                      /* OCT+ (HOME, a switch): kept, one undo level */
{
    qd_tools_t *T = &a->ui.tl;
    if (T->flags & QD_TLF_DIRTY)                        /* (a detent not written yet: it counts) */
        T->flags &= (uint8_t)~QD_TLF_DIRTY, tl_change(a);
    if (!qd_tools_pending(a))
        return;
    if (T->flags & QD_TLF_NOLIVE)
        tl_apply(a);                                    /* (written now; the gesture keeps no level: no room) */
    qd_undo_close(a);
    tl_settle(T);
}

static void tl_revert(qd_app_t *a)                    /* OCT-: back to the original; nothing pending: TOOLS closes */
{
    qd_tools_t *T = &a->ui.tl;
    T->flags &= (uint8_t)~QD_TLF_DIRTY;
    if (!qd_tools_pending(a)) {
        tl_settle(T);
        qd_latch_close(a);
        return;
    }
    if (!(T->flags & QD_TLF_NOLIVE)) {
        qd_undo_revert_open(a);
        qd_undo_close(a);                               /* (no difference: no level) */
    }
    tl_settle(T);
}
static void tl_closed(qd_app_t *a) { tl_keep(a); }   /* (HOME, another page: a pending change kept) */

void qd_tools_screen(qd_app_t *a)
{
    qd_ui_t *u = &a->ui;
    if (u->mode != QD_MODE_STEP)
        qd_ctl_set_mode(a, QD_MODE_STEP);
    if (!u->slatch)
        qd_step_release_all(a);
    qd_latch_open(a, QD_LATCH_TOOLS);
    tl_settle(&u->tl);
    tl_norm(a);
    qd_ctl_takeover(a, UI_TK_TOOL, 0, 0, 0, -1);        /* (the tool's name) */
}

void qd_tools_black(qd_app_t *a, int b, int down)
{
    qd_tools_t *T = &a->ui.tl;
    if (!down)
        return;
    if (b < QD_TL_N) {
        if (!QD_TL_BUILT(b))                            /* (RANDOM, GRIDS, HUMANIZE: T2) */
            return;
        if (b != T->tool) {
            tl_keep(a);                                 /* (the new tool starts from the result) */
            T->tool = (uint8_t)b;
            tl_norm(a);
            qd_ctl_takeover(a, UI_TK_TOOL, 0, 0, 0, -1);
        } else if (b == QD_TL_ROTATE && T->rot) {      /* again: ROTATE back to 0 */
            T->rot = 0;
            T->flags |= QD_TLF_DIRTY;
        } else if (b == QD_TL_REVERSE) {               /* again: REVERSE flips the result */
            T->flags ^= QD_TLF_FLIP;
            T->flags |= QD_TLF_DIRTY;
        }
        return;
    }
    if (b == 10) {                                      /* B11: the scope, PAGE -> TRACK -> PATTERN */
        tl_keep(a);
        T->scope = (uint8_t)((T->scope + 1) % QD_TS_N);
        tl_norm(a);
        qd_ctl_takeover(a, UI_TK_TOOL, 1, 0, 0, -1);
    }                                                   /* (B8..B10, COPY / PASTE / CLEAR: T3) */
}

void qd_tools_white(qd_app_t *a, int w, int down)
{
    if (!down || w >= QD_PAT_NTRACK || w == a->sel)
        return;
    tl_keep(a);
    qd_ctl_select_track(a, w);
    tl_norm(a);
}

static int tl_clamp(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

int qd_tools_knob(qd_app_t *a, int knob, int d)
{
    qd_tools_t *T = &a->ui.tl;
    const int main = knob == QD_K_SELECT || knob == QD_K_1;
    int ch = 0;
    if (!d)
        return 1;
    switch (T->tool) {
    case QD_TL_ROTATE:                                  /* SELECT (or KNOB 1): the offset from the original */
        if (main)
            T->rot = (int8_t)tl_clamp(T->rot + d, -127, 127), ch = 1;
        break;
    case QD_TL_EUCLID:
        tl_norm(a);
        if (knob == QD_K_1) {
            T->ehits = (uint8_t)tl_clamp(T->ehits + d, 0, T->elen), ch = 1;
        } else if (knob == QD_K_2) {
            T->elen = (uint8_t)tl_clamp(T->elen + qd_ctl_acc(a, d, tl_maxlen(a)), 1, tl_maxlen(a)), ch = 1;
            tl_norm(a);
        } else if (knob == QD_K_3) {
            T->erot = (uint8_t)(((T->erot + d) % T->elen + T->elen) % T->elen), ch = 1;
        } else if (knob == QD_K_4) {
            const int m = tl_clamp(T->emode + (d > 0 ? 1 : -1), 0, QD_EU_N - 1);
            ch = m != T->emode;
            T->emode = (uint8_t)m;
        }
        break;
    case QD_TL_LENGTH:                                  /* SELECT (or KNOB 1): x2 / /2 a detent */
        if (main) {
            const int l = tl_clamp(T->lpow + d, -3, 3);
            ch = l != T->lpow;
            T->lpow = (int8_t)l;
        }
        break;
    default:
        break;
    }
    if (ch)
        T->flags |= QD_TLF_DIRTY;
    return 1;
}

void qd_tools_ms(qd_app_t *a)
{
    qd_tools_t *T = &a->ui.tl;
    if (T->flags & QD_TLF_PEND)
        qd_tools_pending(a);                            /* (a gesture closed elsewhere: the dot goes) */
    if (!(T->flags & QD_TLF_DIRTY))
        return;
    T->flags &= (uint8_t)~QD_TLF_DIRTY;
    if (qd_tools_open(a))
        tl_change(a);
}

void qd_tools_leds(const qd_app_t *a, uint8_t *key)
{
    const qd_tools_t *T = &a->ui.tl;
    int i;
    for (i = 0; i < 16; i++)                            /* W1..W6 the tracks (W7.. dark) */
        key[QD_WHITE_KEY[i]] = i < QD_PAT_NTRACK ? (i == a->sel ? QD_LED_LIT : QD_LED_DIM) : QD_LED_OFF;
    for (i = 0; i < 11; i++)                            /* B1..B7 the tools built, B11 the scope */
        key[QD_BLACK_KEY[i]] = i < QD_TL_N ? (!QD_TL_BUILT(i) ? QD_LED_OFF : i == T->tool ? QD_LED_LIT : QD_LED_DIM)
                             : i == 10 ? QD_LED_LIT : QD_LED_OFF;
}

/* ---------------------------------------------------------------- what the screen shows */
static int tl_drawn_only(const qd_app_t *a)
{
    return (a->ui.tl.flags & QD_TLF_PEND) && (a->ui.tl.flags & QD_TLF_NOLIVE);
}

static void tl_step_view(const qd_pattern_t *p, int t, int s, int *on, int *note)
{
    qd_rstep_t r;
    qd_step_resolve(p, t, s, &r);
    *on = r.on;
    *note = r.on && r.nn ? r.notes[0] : 0;
}

void qd_tools_lane(const qd_app_t *a, int w, int *on, int *was, int *note)
{
    const qd_tools_t *T = &a->ui.tl;
    const qd_pattern_t *p = qd_ctl_cpat(a);
    const int t = a->sel, page = tl_page(a), s = page * QD_PAGE + (w & 15);
    uint8_t pos[QD_NSTEP];
    int n, i, src = s;
    tl_step_view(p, t, s, on, note);
    if (!tl_drawn_only(a)) {                            /* the pattern is the result (or nothing is pending) */
        const int live = (T->flags & QD_TLF_PEND) && TL_PEND_LIVE(a) && T->tool != QD_TL_LENGTH;
        *was = live && T->opage == page ? (T->orig >> (w & 15)) & 1 : *on;
        return;
    }
    *was = *on;                                         /* (drawn only: the pattern is the original) */
    if (T->tool == QD_TL_LENGTH || (T->scope == QD_TS_PAGE && page != T->opage))
        return;
    n = qd_tl_positions(p, t, T->scope, page, pos);
    i = qd_tl_pos_of(pos, n, s);
    if (i < 0)
        return;
    if (T->tool == QD_TL_ROTATE)
        src = pos[(((i - T->rot) % n) + n) % n];
    else if (T->tool == QD_TL_REVERSE && (T->flags & QD_TLF_FLIP))
        src = pos[n - 1 - i];
    if (src != s) {
        tl_step_view(p, t, src, on, note);
    } else if (T->tool == QD_TL_EUCLID) {
        const int hit = qd_tl_euclid_hit(i, T->ehits, T->elen, T->erot);
        *on = T->emode == QD_EU_NEW ? hit : T->emode == QD_EU_ADD ? hit || *was : hit && *was;
        if (*on && !*was)
            *note = p->trk[t].def.note[0];
    }
}

int qd_tools_result_len(const qd_app_t *a, uint8_t *loop)
{
    const qd_tools_t *T = &a->ui.tl;
    const qd_pattern_t *p = qd_ctl_cpat(a);
    qd_tset_t ts = p->trk[a->sel];
    if (tl_drawn_only(a) && T->tool == QD_TL_LENGTH)
        qd_tl_length((qd_pattern_t *)p, a->sel, T->scope == QD_TS_PAGE ? T->opage & 7 : -1, T->lpow, &ts);
    *loop = (uint8_t)qd_track_loop(&ts);
    return qd_track_len(&ts);
}
