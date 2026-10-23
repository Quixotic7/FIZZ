/* SPDX-License-Identifier: GPL-3.0-only */
/* Copy, paste and undo (clip.h). */
#include <string.h>

#include "clip.h"
#ifdef QD_CLIP_ZBYTES
#include "zr.h"
#endif

void qd_clip_init(qd_clip_t *c)
{
    memset(c, 0, sizeof *c);
    c->group_t = -1;
}

/* ---------------------------------------------------------------- the levels (P2.1: read and kept here) */
/* (a level read out, a track's steps as they are: two of them for a moment; P2.1b: at the device's sizes borrowed
 * from the drawing's arena (pattern.h qd_tmp_borrow), 0 when it has no room: the edit then keeps no level) */
#ifdef QD_CLIP_ZBYTES
static qd_undo_t *lv_tmp;
static int lv_depth;
static qd_undo_t *lv_open(void)
{
    if (!lv_depth++)
        lv_tmp = (qd_undo_t *)qd_tmp_borrow(2u * sizeof(qd_undo_t));
    return lv_tmp;
}
static void lv_close(void)
{
    if (!--lv_depth && lv_tmp) {
        qd_tmp_return(lv_tmp);
        lv_tmp = 0;
    }
}
#else
static qd_undo_t lv_tmp[2];
static qd_undo_t *lv_open(void) { return lv_tmp; }
static void lv_close(void) {}
#endif
#ifdef QD_CLIP_ZBYTES
static int lv_valid(const qd_clip_t *c, int t) { return c->zlen[t] != 0; }
static int lv_pat(const qd_clip_t *c, int t) { return c->zpat[t]; }
static void lv_get(const qd_clip_t *c, int t, qd_undo_t *out)
{
    if (!c->zlen[t])
        memset(out, 0, sizeof *out);
    else
        zr_unpack(c->z + c->zoff[t], c->zlen[t], (uint8_t *)out, sizeof *out);
}
static void lv_drop(qd_clip_t *c, int t)                /* (its bytes out, the later ones moved down) */
{
    const uint32_t off = c->zoff[t], n = c->zlen[t];
    uint32_t end = 0;
    int i;
    if (!n)
        return;
    for (i = 0; i < QD_PAT_NTRACK; i++)
        if (c->zlen[i] && c->zoff[i] + c->zlen[i] > end)
            end = c->zoff[i] + c->zlen[i];
    memmove(c->z + off, c->z + off + n, end - off - n);
    for (i = 0; i < QD_PAT_NTRACK; i++)
        if (c->zlen[i] && c->zoff[i] > off)
            c->zoff[i] = (uint16_t)(c->zoff[i] - n);
    c->zlen[t] = 0;
}
static void lv_put(qd_clip_t *c, int t, const qd_undo_t *u)
{
    uint32_t n, end;
    int i;
    lv_drop(c, t);
    if (!u->valid)
        return;
    n = zr_pack((const uint8_t *)u, sizeof *u, 0, 0);
    for (;;) {
        int old = -1;
        for (end = 0, i = 0; i < QD_PAT_NTRACK; i++)
            if (c->zlen[i] && c->zoff[i] + c->zlen[i] > end)
                end = c->zoff[i] + c->zlen[i];
        if (end + n <= QD_CLIP_ZBYTES)
            break;
        for (i = 0; i < QD_PAT_NTRACK; i++)             /* (no room: the oldest other level goes) */
            if (c->zlen[i] && (old < 0 || c->zage[i] < c->zage[old]))
                old = i;
        if (old < 0) {
            c->zdropped++;                              /* (bigger than all the room: not kept) */
            return;
        }
        lv_drop(c, old);
        c->zdropped++;
    }
    zr_pack((const uint8_t *)u, sizeof *u, c->z + end, n);
    c->zoff[t] = (uint16_t)end, c->zlen[t] = (uint16_t)n;
    c->zpat[t] = (int16_t)u->pat;
    c->zage[t] = ++c->zclock;
}
#else
static int lv_valid(const qd_clip_t *c, int t) { return c->undo[t].valid; }
static int lv_pat(const qd_clip_t *c, int t) { return c->undo[t].pat; }
static void lv_get(const qd_clip_t *c, int t, qd_undo_t *out) { *out = c->undo[t]; }
static void lv_put(qd_clip_t *c, int t, const qd_undo_t *u) { c->undo[t] = *u; }
#endif

void qd_clip_group(qd_clip_t *c, int on)
{
    c->group = on;
    c->group_t = -1;
}

/* an edit's undo level: kept, unless the group already kept this track's (1: kept now) */
static int keep_edit(qd_clip_t *c, const qd_pattern_t *p, int k, int t)
{
    if (c->group && c->group_t == t && c->group_k == k && lv_valid(c, t) && lv_pat(c, t) == k)
        return 0;
    qd_clip_keep(c, p, k, t);
    if (c->group) {
        c->group_t = t;
        c->group_k = k;
    }
    return 1;
}

void qd_clip_keep(qd_clip_t *c, const qd_pattern_t *p, int k, int t)
{
    qd_undo_t *u = lv_open();
    if (u && t >= 0 && t < QD_PAT_NTRACK) {
        memset(u, 0, sizeof *u);
        qd_steps_get(p, t, 0, QD_NSTEP, u->step, u->lk, &u->nlk);
        u->ts = p->trk[t];
        u->pat = k;
        u->valid = 1;
        lv_put(c, t, u);
    }
    lv_close();
}

int qd_clip_can_undo(const qd_clip_t *c, int k, int t)
{
    return t >= 0 && t < QD_PAT_NTRACK && lv_valid(c, t) && lv_pat(c, t) == k;
}

static int clip_undo(qd_clip_t *c, qd_pattern_t *p, int k, int t, qd_undo_t *lv);
int qd_clip_undo(qd_clip_t *c, qd_pattern_t *p, int k, int t)
{
    qd_undo_t *lv = lv_open();
    const int r = lv ? clip_undo(c, p, k, t, lv) : 0;
    lv_close();
    return r;
}
static int clip_undo(qd_clip_t *c, qd_pattern_t *p, int k, int t, qd_undo_t *lv)
{
    qd_undo_t *u = &lv[0], *now = &lv[1];
    int r;
    if (!qd_clip_can_undo(c, k, t))
        return 0;
    lv_get(c, t, u);
    memset(now, 0, sizeof *now);
    qd_steps_get(p, t, 0, QD_NSTEP, now->step, now->lk, &now->nlk);
    now->ts = p->trk[t];
    now->pat = k;
    now->valid = 1;
    r = qd_steps_put(p, t, 0, QD_NSTEP, u->step, u->lk, u->nlk);
    if (r != QD_OK)
        return r;
    c->group_t = -1;                                    /* (a group's next edit keeps a new level) */
    memcpy(p->trk[t].pstart, u->ts.pstart, sizeof p->trk[t].pstart);   /* the pages come back with the steps (the
                                                                     * defaults, rate, mute, key stay as they are) */
    memcpy(p->trk[t].pend, u->ts.pend, sizeof p->trk[t].pend);
    p->trk[t].loop = u->ts.loop;
    lv_put(c, t, now);                                  /* (undo again: redo) */
    return 1;
}

int qd_clip_copy_step(qd_clip_t *c, const qd_pattern_t *p, int t, int s)
{
    qd_undo_t *lv = lv_open();                          /* (a level's locks room: the step's locks for a moment) */
    qd_lock_t *lk = lv ? lv[0].lk : 0;
    int r, n = 0;
    r = lk ? qd_steps_get(p, t, s, 1, &c->st.step, lk, &n) : QD_EFULL;
    if (r == QD_OK) {
        c->st.nlk = n > QD_LK_N ? QD_LK_N : n;
        memcpy(c->st.lk, lk, (size_t)c->st.nlk * sizeof lk[0]);
        c->st.valid = 1;
    }
    lv_close();
    return r;
}

int qd_clip_paste_step(qd_clip_t *c, qd_pattern_t *p, int k, int t, int s)
{
    qd_undo_t *lv, *old;
    int r, og;
    if (!c->st.valid || t < 0 || t >= QD_PAT_NTRACK || s < 0 || s >= QD_NSTEP)
        return QD_EARG;
    lv = lv_open();
    old = lv ? &lv[1] : 0;
    if (old)
        lv_get(c, t, old);
    og = c->group_t;
    keep_edit(c, p, k, t);
    r = qd_steps_put(p, t, s, 1, &c->st.step, c->st.lk, c->st.nlk);
    if (r != QD_OK) {                                   /* nothing changed: the earlier undo level stays */
        if (old)
            lv_put(c, t, old);
        c->group_t = og;
    }
    lv_close();
    return r;
}

int qd_clip_cut_step(qd_clip_t *c, qd_pattern_t *p, int k, int t, int s)
{
    int r = qd_clip_copy_step(c, p, t, s);
    if (r != QD_OK)
        return r;
    keep_edit(c, p, k, t);
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
    qd_undo_t *lv, *old;
    int r, og;
    if (!c->pg.valid || t < 0 || t >= QD_PAT_NTRACK || page < 0 || page >= QD_NPAGE)
        return QD_EARG;
    lv = lv_open();
    old = lv ? &lv[1] : 0;
    if (old)
        lv_get(c, t, old);
    og = c->group_t;
    keep_edit(c, p, k, t);
    r = qd_steps_put(p, t, page * QD_PAGE, QD_PAGE, c->pg.step, c->pg.lk, c->pg.nlk);
    if (r != QD_OK) {                                   /* nothing changed: the earlier undo level stays */
        if (old)
            lv_put(c, t, old);
        c->group_t = og;
        lv_close();
        return r;
    }
    lv_close();
    p->trk[t].pstart[page] = c->pg.start;
    p->trk[t].pend[page] = c->pg.end;
    p->trk[t].loop = (uint8_t)(qd_track_loop(&p->trk[t]) | (1u << page));
    return QD_OK;
}

int qd_clip_clear_page(qd_clip_t *c, qd_pattern_t *p, int k, int t, int page)
{
    if (t < 0 || t >= QD_PAT_NTRACK || page < 0 || page >= QD_NPAGE)
        return QD_EARG;
    keep_edit(c, p, k, t);
    return qd_page_clear(p, t, page);
}

int qd_clip_clear_track(qd_clip_t *c, qd_pattern_t *p, int k, int t)
{
    if (t < 0 || t >= QD_PAT_NTRACK)
        return QD_EARG;
    keep_edit(c, p, k, t);
    return qd_track_clear(p, t);
}

int qd_clip_clear_steps(qd_clip_t *c, qd_pattern_t *p, int k, int t, const int *steps, int n)
{
    int i;
    if (t < 0 || t >= QD_PAT_NTRACK)
        return QD_EARG;
    keep_edit(c, p, k, t);
    for (i = 0; i < n; i++)
        qd_step_clear(p, t, steps[i]);
    return QD_OK;
}
