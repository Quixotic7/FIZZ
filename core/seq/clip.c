/* SPDX-License-Identifier: GPL-3.0-only */
/* Copy, paste and undo (clip.h). */
#include <string.h>

#include "clip.h"

void qd_clip_init(qd_clip_t *c) { memset(c, 0, sizeof *c); }

void qd_clip_keep(qd_clip_t *c, const qd_pattern_t *p, int k, int t)
{
    qd_undo_t *u;
    if (t < 0 || t >= QD_PAT_NTRACK)
        return;
    u = &c->undo[t];
    qd_steps_get(p, t, 0, QD_NSTEP, u->step, u->lk, &u->nlk);
    u->ts = p->trk[t];
    u->pat = k;
    u->valid = 1;
}

int qd_clip_can_undo(const qd_clip_t *c, int k, int t)
{
    return t >= 0 && t < QD_PAT_NTRACK && c->undo[t].valid && c->undo[t].pat == k;
}

int qd_clip_undo(qd_clip_t *c, qd_pattern_t *p, int k, int t)
{
    static qd_undo_t now;
    qd_undo_t *u;
    int r;
    if (!qd_clip_can_undo(c, k, t))
        return 0;
    u = &c->undo[t];
    qd_steps_get(p, t, 0, QD_NSTEP, now.step, now.lk, &now.nlk);
    now.ts = p->trk[t];
    now.pat = k;
    now.valid = 1;
    r = qd_steps_put(p, t, 0, QD_NSTEP, u->step, u->lk, u->nlk);
    if (r != QD_OK)
        return r;
    memcpy(p->trk[t].pstart, u->ts.pstart, sizeof p->trk[t].pstart);   /* the pages come back with the steps (the
                                                                     * defaults, rate, mute, key stay as they are) */
    memcpy(p->trk[t].pend, u->ts.pend, sizeof p->trk[t].pend);
    p->trk[t].loop = u->ts.loop;
    *u = now;                                           /* (undo again: redo) */
    return 1;
}

int qd_clip_copy_step(qd_clip_t *c, const qd_pattern_t *p, int t, int s)
{
    static qd_lock_t lk[QD_NLOCK];
    int r, n = 0;
    r = qd_steps_get(p, t, s, 1, &c->st.step, lk, &n);
    if (r != QD_OK)
        return r;
    c->st.nlk = n > QD_LK_N ? QD_LK_N : n;
    memcpy(c->st.lk, lk, (size_t)c->st.nlk * sizeof lk[0]);
    c->st.valid = 1;
    return QD_OK;
}

int qd_clip_paste_step(qd_clip_t *c, qd_pattern_t *p, int k, int t, int s)
{
    static qd_undo_t old;
    int r;
    if (!c->st.valid || t < 0 || t >= QD_PAT_NTRACK || s < 0 || s >= QD_NSTEP)
        return QD_EARG;
    old = c->undo[t];
    qd_clip_keep(c, p, k, t);
    r = qd_steps_put(p, t, s, 1, &c->st.step, c->st.lk, c->st.nlk);
    if (r != QD_OK)
        c->undo[t] = old;                               /* nothing changed: the earlier undo level stays */
    return r;
}

int qd_clip_cut_step(qd_clip_t *c, qd_pattern_t *p, int k, int t, int s)
{
    int r = qd_clip_copy_step(c, p, t, s);
    if (r != QD_OK)
        return r;
    qd_clip_keep(c, p, k, t);
    return qd_step_clear(p, t, s);
}

int qd_clip_copy_page(qd_clip_t *c, const qd_pattern_t *p, int t, int page)
{
    int r;
    if (page < 0 || page >= QD_NPAGE)
        return QD_EARG;
    r = qd_steps_get(p, t, page * QD_PAGE, QD_PAGE, c->pg.step, c->pg.lk, &c->pg.nlk);
    if (r != QD_OK)
        return r;
    c->pg.start = p->trk[t].pstart[page];
    c->pg.end = p->trk[t].pend[page];
    c->pg.valid = 1;
    return QD_OK;
}

int qd_clip_paste_page(qd_clip_t *c, qd_pattern_t *p, int k, int t, int page)
{
    static qd_undo_t old;
    int r;
    if (!c->pg.valid || t < 0 || t >= QD_PAT_NTRACK || page < 0 || page >= QD_NPAGE)
        return QD_EARG;
    old = c->undo[t];
    qd_clip_keep(c, p, k, t);
    r = qd_steps_put(p, t, page * QD_PAGE, QD_PAGE, c->pg.step, c->pg.lk, c->pg.nlk);
    if (r != QD_OK) {
        c->undo[t] = old;                               /* nothing changed: the earlier undo level stays */
        return r;
    }
    p->trk[t].pstart[page] = c->pg.start;
    p->trk[t].pend[page] = c->pg.end;
    p->trk[t].loop = (uint8_t)(qd_track_loop(&p->trk[t]) | (1u << page));
    return QD_OK;
}

int qd_clip_clear_page(qd_clip_t *c, qd_pattern_t *p, int k, int t, int page)
{
    if (t < 0 || t >= QD_PAT_NTRACK || page < 0 || page >= QD_NPAGE)
        return QD_EARG;
    qd_clip_keep(c, p, k, t);
    return qd_page_clear(p, t, page);
}

int qd_clip_clear_track(qd_clip_t *c, qd_pattern_t *p, int k, int t)
{
    if (t < 0 || t >= QD_PAT_NTRACK)
        return QD_EARG;
    qd_clip_keep(c, p, k, t);
    return qd_track_clear(p, t);
}

int qd_clip_clear_steps(qd_clip_t *c, qd_pattern_t *p, int k, int t, const int *steps, int n)
{
    int i;
    if (t < 0 || t >= QD_PAT_NTRACK)
        return QD_EARG;
    qd_clip_keep(c, p, k, t);
    for (i = 0; i < n; i++)
        qd_step_clear(p, t, steps[i]);
    return QD_OK;
}
