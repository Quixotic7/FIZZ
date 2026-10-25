/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* T1: TOOLS, the track tools (ctl.h; docs/TRACK-TOOLS-PLAN.md §2, the owner's decisions §8, his revision §10). Built
 * through ctl_latch.c (it includes this file). The operations are seq/tools.c's.
 *
 * THE CONTROLS: B1..B7 or PRESETS the tool; B11 or KNOB 4 the scope (SELECTED STEPS, PAGE, TRACK, PATTERN); SELECT the
 * page shown; W1..W16 the page's steps (a press: in / out of the selection, the scope SELECTED); KNOB 1..3 and
 * ALGORITHM the tool's values. OCT+ keeps, OCT- reverts (it never leaves); HOME is the only way out. TOOLS works on
 * the track selected when it opened (SHIFT + SELECT still changes it: a pending change is kept first).
 *
 * LIVE PREVIEW, SEEN AND HEARD (§2): the first change of a tool opens an undo gesture of its own and HOLDS it open
 * (undo.h's hold: the quiet time does not close it); its "before" is the original. Every later change reverts the
 * pattern from that copy (qd_undo_revert_open) and applies the tool with its values now, so the player plays the
 * result with nothing new on its side. Detents are combined: a change marks the state dirty, the next millisecond
 * writes it. OCT+ closes the gesture (one undo level); OCT- reverts and lets it go (no level); HOME, another tool or
 * another scope keep it. When the log cannot keep the original (the device's, a big pattern) the change is DRAWN
 * ONLY: the pattern stays as it is, the screen computes the result, OCT+ writes it without an undo level (the
 * crossed-out arrow says so). */

#define TL_PEND_LIVE(a) (!((a)->ui.tl.flags & QD_TLF_NOLIVE) && (a)->undo.open && (a)->undo.hold)
enum { TLM_KEPT, TLM_REVERTED };                        /* (the popup's messages: tk_step) */

int qd_tools_open(const qd_app_t *a) { return a->ui.mode == QD_MODE_STEP && a->ui.latch == QD_LATCH_TOOLS; }

static int tl_page(const qd_app_t *a) { return a->ui.page[a->sel] & 7; }
static int tl_clamp(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

/* the scope's positions of track t (PAGE: page `page`; SELECTED: the latch's selection, in step order) */
static int tl_pos(const qd_app_t *a, const qd_pattern_t *p, int t, int page, uint8_t *pos)
{
    const qd_ui_t *u = &a->ui;
    int i, j, n = 0;
    if (u->tl.scope != QD_TS_SEL)
        return qd_tl_positions(p, t, u->tl.scope, page, pos);
    for (i = 0; u->slatch && i < u->nheld; i++) {      /* (insertion: up to 16) */
        const uint8_t s = (uint8_t)(u->held[i] & 127);
        for (j = n; j > 0 && pos[j - 1] > s; j--)
            pos[j] = pos[j - 1];
        pos[j] = s;
        n++;
    }
    return n;
}

/* EUCLID's longest length in this scope: the selection's / page's steps, the track's loop, the longest loop */
static int tl_maxlen(const qd_app_t *a)
{
    const qd_pattern_t *p = qd_ctl_cpat(a);
    uint8_t pos[QD_NSTEP];
    int t, m = 1;
    if (a->ui.tl.scope != QD_TS_PATTERN)
        return tl_clamp(tl_pos(a, p, a->sel, tl_page(a), pos), 1, QD_NSTEP);
    for (t = 0; t < QD_PAT_NTRACK; t++)
        m = qd_track_len(&p->trk[t]) > m ? qd_track_len(&p->trk[t]) : m;
    return m;
}

static void tl_norm(qd_app_t *a)                      /* EUCLID's values within the scope's length */
{
    qd_tools_t *T = &a->ui.tl;
    const int mx = tl_maxlen(a);
    if (T->tool != QD_TL_EUCLID)
        return;
    T->v.eu.len = (uint8_t)tl_clamp(T->v.eu.len, 1, mx);
    if (T->v.eu.hits > T->v.eu.len)
        T->v.eu.hits = T->v.eu.len;
    if (T->v.eu.rot >= T->v.eu.len)
        T->v.eu.rot = 0;
}

static void tl_defaults(qd_tools_t *T)                /* a tool chosen: its values from their defaults */
{
    memset(&T->v, 0, sizeof T->v);
    if (T->tool == QD_TL_EUCLID)
        T->v.eu.hits = 4, T->v.eu.len = 16;
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
 * change began on (page follow or SELECT may move the page shown meanwhile) */
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
        if (T->tool == QD_TL_LENGTH) {                  /* (SELECTED: its page's end, as PAGE) */
            qd_tl_length(p, t, T->scope <= QD_TS_PAGE ? page : -1, T->lpow, 0);
            continue;
        }
        n = tl_pos(a, p, t, page, pos);
        if (T->tool == QD_TL_ROTATE)
            qd_tl_rotate(p, t, pos, n, T->rot);
        else if (T->tool == QD_TL_REVERSE && (T->flags & QD_TLF_FLIP))
            qd_tl_reverse(p, t, pos, n);
        else if (T->tool == QD_TL_EUCLID)
            qd_tl_euclid(p, t, pos, n, T->v.eu.hits, T->v.eu.len, T->v.eu.rot, T->v.eu.mode);
    }
}

static void tl_capture(qd_app_t *a)                   /* the lane as it was (its diff: kept, new, going) */
{
    qd_tools_t *T = &a->ui.tl;
    const qd_pattern_t *p = qd_ctl_cpat(a);
    int w;
    T->opage = (uint8_t)tl_page(a);
    T->otrack = (uint8_t)a->sel;
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

static void tl_pop(qd_app_t *a, int kind, int msg)    /* the popup: 0 the tool's name, 1 the scope, 2 a message */
{
    qd_ctl_takeover(a, UI_TK_TOOL, kind, 0, 0, msg);
}

static int tl_keep_q(qd_app_t *a)                     /* kept (one undo level): 1 if there was a change */
{
    qd_tools_t *T = &a->ui.tl;
    if (T->flags & QD_TLF_DIRTY)                        /* (a detent not written yet: it counts) */
        T->flags &= (uint8_t)~QD_TLF_DIRTY, tl_change(a);
    if (!qd_tools_pending(a))
        return 0;
    if (T->flags & QD_TLF_NOLIVE)
        tl_apply(a);                                    /* (written now; the gesture keeps no level: no room) */
    qd_undo_close(a);
    tl_settle(T);
    return 1;
}
static void tl_keep(qd_app_t *a) { tl_keep_q(a); }   /* (HOME, a switch: kept quietly) */
static void tl_confirm(qd_app_t *a)                   /* OCT+: kept, "KEPT" */
{
    if (tl_keep_q(a))
        tl_pop(a, 2, TLM_KEPT);
}

static void tl_revert(qd_app_t *a)                    /* OCT-: back to the original (TOOLS stays: HOME leaves) */
{
    qd_tools_t *T = &a->ui.tl;
    T->flags &= (uint8_t)~QD_TLF_DIRTY;
    if (!qd_tools_pending(a)) {
        tl_settle(T);
        return;
    }
    if (!(T->flags & QD_TLF_NOLIVE)) {
        qd_undo_revert_open(a);
        qd_undo_close(a);                               /* (no difference: no level) */
    }
    tl_settle(T);
    tl_pop(a, 2, TLM_REVERTED);
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
    if (u->slatch)                                      /* (the step lock on: the selected steps) */
        u->tl.scope = QD_TS_SEL;
    tl_norm(a);
    tl_pop(a, 0, 0);                                    /* (the tool's name) */
}

static void tl_tool(qd_app_t *a, int b)               /* tool b chosen (the pending change kept first) */
{
    qd_tools_t *T = &a->ui.tl;
    tl_keep(a);
    T->tool = (uint8_t)b;
    tl_defaults(T);
    tl_norm(a);
    tl_pop(a, 0, 0);
}

static void tl_scope(qd_app_t *a, int sc)
{
    qd_tools_t *T = &a->ui.tl;
    tl_keep(a);
    T->scope = (uint8_t)sc;
    tl_norm(a);
    tl_pop(a, 1, 0);
}

void qd_tools_black(qd_app_t *a, int b, int down)
{
    qd_tools_t *T = &a->ui.tl;
    if (!down)
        return;
    if (b < QD_TL_N) {
        if (!QD_TL_BUILT(b))
            return;
        if (b != T->tool) {
            tl_tool(a, b);
        } else if (b == QD_TL_ROTATE && T->rot) {      /* again: ROTATE back to 0 */
            T->rot = 0;
            T->flags |= QD_TLF_DIRTY;
        } else if (b == QD_TL_REVERSE) {               /* again: REVERSE flips the result */
            T->flags ^= QD_TLF_FLIP;
            T->flags |= QD_TLF_DIRTY;
        }
        return;
    }
    if (b == 10)                                        /* B11: the scope, round */
        tl_scope(a, (T->scope + 1) % QD_TS_N);
}

/* W: the page's step in / out of the selection (the scope SELECTED: another scope's change kept first; SELECTED's
 * pending change follows the selection, live) */
void qd_tools_white(qd_app_t *a, int w, int down)
{
    qd_tools_t *T = &a->ui.tl;
    if (!down)
        return;
    if (T->scope != QD_TS_SEL)
        tl_scope(a, QD_TS_SEL);
    qd_step_select_toggle(a, tl_page(a) * QD_PAGE + (w & 15));
    tl_norm(a);
    if (T->flags & QD_TLF_PEND)
        T->flags |= QD_TLF_DIRTY;
}

int qd_tools_knob(qd_app_t *a, int knob, int d)
{
    qd_tools_t *T = &a->ui.tl;
    int ch = 0;
    if (!d)
        return 1;
    if (knob == QD_K_SELECT) {                          /* the page shown (PAGE's change kept first) */
        if (T->scope == QD_TS_PAGE)
            tl_keep(a);
        qd_ctl_set_page(a, (tl_page(a) + d % QD_NPAGE + QD_NPAGE) % QD_NPAGE);
        tl_norm(a);
        return 1;
    }
    if (knob == QD_K_PRESETS) {                         /* the tools, round (the built ones) */
        int b = T->tool, k, n = tl_clamp(d < 0 ? -d : d, 1, QD_TL_N);
        while (n--)                                     /* (a detent a tool) */
            for (k = 0; k < QD_TL_N; k++) {
                b = (b + (d > 0 ? 1 : QD_TL_N - 1)) % QD_TL_N;
                if (QD_TL_BUILT(b))
                    break;
            }
        tl_tool(a, b);
        return 1;
    }
    if (knob == QD_K_4) {                               /* the scope */
        const int sc = tl_clamp(T->scope + d, 0, QD_TS_N - 1);
        if (sc != T->scope)
            tl_scope(a, sc);
        return 1;
    }
    switch (T->tool) {
    case QD_TL_ROTATE:                                  /* KNOB 1: the offset from the original */
        if (knob == QD_K_1)
            T->rot = (int8_t)tl_clamp(T->rot + d, -127, 127), ch = 1;
        break;
    case QD_TL_EUCLID:
        tl_norm(a);
        if (knob == QD_K_1) {
            T->v.eu.hits = (uint8_t)tl_clamp(T->v.eu.hits + d, 0, T->v.eu.len), ch = 1;
        } else if (knob == QD_K_2) {
            T->v.eu.len = (uint8_t)tl_clamp(T->v.eu.len + qd_ctl_acc(a, d, tl_maxlen(a)), 1, tl_maxlen(a)), ch = 1;
            tl_norm(a);
        } else if (knob == QD_K_3) {
            T->v.eu.rot = (uint8_t)(((T->v.eu.rot + d) % T->v.eu.len + T->v.eu.len) % T->v.eu.len), ch = 1;
        } else if (knob == QD_K_ALGO) {                 /* (the landing: was KNOB 4) */
            const int m = tl_clamp(T->v.eu.mode + (d > 0 ? 1 : -1), 0, QD_EU_N - 1);
            ch = m != T->v.eu.mode;
            T->v.eu.mode = (uint8_t)m;
        }
        break;
    case QD_TL_LENGTH:                                  /* KNOB 1: x2 / /2 a detent */
        if (knob == QD_K_1) {
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
    if (T->flags & QD_TLF_PEND) {
        if (a->sel != T->otrack)                        /* (SHIFT + SELECT: another track; the change kept) */
            tl_keep(a);
        else
            qd_tools_pending(a);                        /* (a gesture closed elsewhere: the dot goes) */
    }
    if (!(T->flags & QD_TLF_DIRTY))
        return;
    T->flags &= (uint8_t)~QD_TLF_DIRTY;
    if (qd_tools_open(a))
        tl_change(a);
}

static int tl_selected(const qd_app_t *a, int s)
{
    int i;
    for (i = 0; a->ui.slatch && i < a->ui.nheld; i++)
        if (a->ui.held[i] == s)
            return 1;
    return 0;
}

void qd_tools_leds(const qd_app_t *a, uint8_t *key)
{
    const qd_tools_t *T = &a->ui.tl;
    const int page = tl_page(a);
    int i;
    for (i = 0; i < 16; i++) {                          /* W: the page's steps as the change makes them, the selected
                                                         * blinking */
        int on, was, note;
        qd_tools_lane(a, i, &on, &was, &note);
        key[QD_WHITE_KEY[i]] = tl_selected(a, page * QD_PAGE + i) ? ((a->ms / QD_FAST_MS) & 1u ? QD_LED_DIM : QD_LED_LIT)
                             : on ? QD_LED_LIT : QD_LED_OFF;
    }
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

uint16_t qd_tools_sel_mask(const qd_app_t *a)         /* the page's selected steps */
{
    uint16_t m = 0;
    int w;
    for (w = 0; w < 16; w++)
        if (tl_selected(a, tl_page(a) * QD_PAGE + w))
            m |= (uint16_t)(1u << w);
    return m;
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
    n = tl_pos(a, p, t, T->opage & 7, pos);
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
        const int hit = qd_tl_euclid_hit(i, T->v.eu.hits, T->v.eu.len, T->v.eu.rot);
        *on = T->v.eu.mode == QD_EU_NEW ? hit : T->v.eu.mode == QD_EU_ADD ? hit || *was : hit && *was;
        if (*on && !*was)
            *note = p->trk[t].def.note[0];
    }
}

/* the playing step's position in the scope (-1: none) */
int qd_tools_play_pos(const qd_app_t *a)
{
    const int ps = a->seq.playing ? qd_seq_track_step(&a->seq, a->sel) : -1;
    uint8_t pos[QD_NSTEP];
    return ps < 0 ? -1 : qd_tl_pos_of(pos, tl_pos(a, qd_ctl_cpat(a), a->sel, tl_page(a), pos), ps);
}

int qd_tools_result_len(const qd_app_t *a, uint8_t *loop)
{
    const qd_tools_t *T = &a->ui.tl;
    const qd_pattern_t *p = qd_ctl_cpat(a);
    qd_tset_t ts = p->trk[a->sel];
    if (tl_drawn_only(a) && T->tool == QD_TL_LENGTH)
        qd_tl_length((qd_pattern_t *)p, a->sel, T->scope <= QD_TS_PAGE ? T->opage & 7 : -1, T->lpow, &ts);
    *loop = (uint8_t)qd_track_loop(&ts);
    return qd_track_len(&ts);
}
