/* SPDX-License-Identifier: GPL-3.0-only */
/* The pattern model and its editing API (pattern.h). */
#include <string.h>

#include "pattern.h"

/* ---------------------------------------------------------------- tables --- */
static const int16_t QD_LEN_E[QD_NLEN] = {1, 2, 4, 6, 8, 12, 16, 24, 32, 48, 64, 96, 128, 192, 256, 384, 512, 768,
                                          1024, -1};
static const char *const QD_LEN_N[QD_NLEN] = {"1/8", "1/4", "1/2", "3/4", "1", "1.5", "2", "3", "4", "6", "8", "12",
                                              "16", "24", "32", "48", "64", "96", "128", "INF"};

int qd_len_eighths(int len) { return len < 0 || len >= QD_NLEN ? 8 : QD_LEN_E[len]; }
const char *qd_len_name(int len) { return len < 0 || len >= QD_NLEN ? "?" : QD_LEN_N[len]; }

/* a step's ticks at each rate: 8, 4, 2, 4/3, 1, 2/3, 1/2, 3/2, 3/4 sixteenths */
static const int32_t QD_RATE_T[QD_NRATE] = {8 * QD_TPS, 4 * QD_TPS, 2 * QD_TPS, 4 * QD_TPS / 3, QD_TPS,
                                            2 * QD_TPS / 3, QD_TPS / 2, 3 * QD_TPS / 2, 3 * QD_TPS / 4};
static const char *const QD_RATE_N[QD_NRATE] = {"1/8x", "1/4x", "1/2x", "3/4x", "1x", "3/2x", "2x", "2/3x", "4/3x"};
int64_t qd_rate_ticks(int rate) { return rate < 0 || rate >= QD_NRATE ? QD_TPS : QD_RATE_T[rate]; }
const char *qd_rate_name(int rate) { return rate < 0 || rate >= QD_NRATE ? "?" : QD_RATE_N[rate]; }

int qd_cond_ab(int a, int b)
{
    if (b < 2 || b > 8 || a < 1 || a > b)
        return -1;
    return QD_COND_AB + (b * (b - 1)) / 2 - 1 + (a - 1);   /* B = 2 starts at 0: 2 entries, B = 3 at 2, .. */
}

int qd_cond_ab_of(int cond, int *a, int *b)
{
    int bb, k = cond - QD_COND_AB;
    if (cond < QD_COND_AB || cond >= QD_NCOND)
        return 0;
    for (bb = 2; bb <= 8; bb++) {
        if (k < bb) {
            if (a)
                *a = k + 1;
            if (b)
                *b = bb;
            return 1;
        }
        k -= bb;
    }
    return 0;
}

const char *qd_cond_name(int cond, char *buf)
{
    static const char *const N[QD_COND_AB] = {"ALWAYS", "FILL", "!FILL", "PRE", "!PRE", "NEI", "!NEI", "1ST",
                                              "!1ST"};
    int a, b;
    if (cond >= 0 && cond < QD_COND_AB)
        return strcpy(buf, N[cond]);
    if (qd_cond_ab_of(cond, &a, &b)) {
        buf[0] = (char)('0' + a), buf[1] = ':', buf[2] = (char)('0' + b), buf[3] = 0;
        return buf;
    }
    return strcpy(buf, "?");
}

int qd_lock_range(int id, int16_t *min, int16_t *max)
{
    int16_t lo = 0, hi = 127;
    if (id < 0 || id >= QD_LK_N)
        return 0;
    if (id < QD_QUAD_NP) {
        if (!qd_quad_value_range(id, &lo, &hi, 0))
            return 0;
    } else if (id == QD_LK_PAN) {
        lo = -64, hi = 63;
    }
    if (min)
        *min = lo;
    if (max)
        *max = hi;
    return 1;
}

/* ---------------------------------------------------------------- patterns --- */
void qd_tset_init(qd_tset_t *s, int track)
{
    memset(s, 0, sizeof *s);
    s->def_note = 60;
    s->def_vel = 100;
    s->def_len = QD_LEN_1;
    s->chan = (uint8_t)(track & 15);
    s->limit = 6;
    s->reserve = 0;
    s->rate = QD_RATE_1;
    s->length = 16;
}

void qd_pat_init(qd_pattern_t *p)
{
    int t;
    memset(p, 0, sizeof *p);
    for (t = 0; t < QD_PAT_NTRACK; t++)
        qd_tset_init(&p->trk[t], t);
    p->master = QD_MASTER_AUTO;
    p->swing = 50;
    p->bpm = 120;
}

void qd_project_init(qd_project_t *pr)
{
    int k;
    for (k = 0; k < QD_NPAT; k++)
        qd_pat_init(&pr->pat[k]);
}

const char *qd_pat_name(int k, char *buf)
{
    k = k < 0 ? 0 : k >= QD_NPAT ? QD_NPAT - 1 : k;
    buf[0] = (char)('A' + k / 16);
    buf[1] = (char)('0' + (k % 16 + 1) / 10);
    buf[2] = (char)('0' + (k % 16 + 1) % 10);
    buf[3] = 0;
    return buf;
}

int64_t qd_pat_master_ticks(const qd_pattern_t *p)
{
    int64_t m = 0;
    int t;
    if (p->master == QD_MASTER_INF)
        return 0;
    if (p->master != QD_MASTER_AUTO)
        return (int64_t)p->master * QD_TPS;
    for (t = 0; t < QD_PAT_NTRACK; t++) {
        const int64_t l = (int64_t)(p->trk[t].length ? p->trk[t].length : 1) * qd_rate_ticks(p->trk[t].rate);
        m = l > m ? l : m;
    }
    return m;
}

static int lock_before(const qd_lock_t *a, const qd_lock_t *b)
{
    return a->step < b->step || (a->step == b->step && a->id < b->id);
}

int qd_pat_check(const qd_pattern_t *p)
{
    int t, i, start = 0;
    uint8_t has[QD_PAT_NTRACK][QD_NSTEP];
    memset(has, 0, sizeof has);
    for (t = 0; t < QD_PAT_NTRACK; t++) {
        if (p->lock_start[t] != start)
            return 0;
        for (i = start; i < start + p->lock_n[t]; i++) {
            if (p->lock[i].step >= QD_NSTEP || p->lock[i].id >= QD_LK_N)
                return 0;
            if (i > start && !lock_before(&p->lock[i - 1], &p->lock[i]))
                return 0;
            has[t][p->lock[i].step] = 1;
        }
        start += p->lock_n[t];
    }
    if (start != p->nlock || p->nlock > QD_NLOCK)
        return 0;
    for (t = 0; t < QD_PAT_NTRACK; t++)
        for (i = 0; i < QD_NSTEP; i++)
            if (!!(p->step[t][i].flags & QD_SF_LOCKS) != has[t][i])
                return 0;
    return 1;
}

/* ---------------------------------------------------------------- the lock pool --- */
/* the first index of track t's locks at or after (s, id) */
static int lock_pos(const qd_pattern_t *p, int t, int s, int id)
{
    int i = p->lock_start[t], e = i + p->lock_n[t];
    while (i < e && (p->lock[i].step < s || (p->lock[i].step == s && p->lock[i].id < id)))
        i++;
    return i;
}

static void flag_locks(qd_pattern_t *p, int t, int s)
{
    int n = 0;
    qd_locks(p, t, s, &n);
    if (n)
        p->step[t][s].flags |= QD_SF_LOCKS;
    else
        p->step[t][s].flags &= (uint8_t)~QD_SF_LOCKS;
}

static void pool_insert(qd_pattern_t *p, int t, int at, const qd_lock_t *l)
{
    int k;
    memmove(&p->lock[at + 1], &p->lock[at], (size_t)(p->nlock - at) * sizeof p->lock[0]);
    p->lock[at] = *l;
    p->nlock++;
    p->lock_n[t]++;
    for (k = t + 1; k < QD_PAT_NTRACK; k++)
        p->lock_start[k]++;
}

static void pool_remove(qd_pattern_t *p, int t, int at, int n)
{
    int k;
    if (n <= 0)
        return;
    memmove(&p->lock[at], &p->lock[at + n], (size_t)(p->nlock - at - n) * sizeof p->lock[0]);
    p->nlock = (uint16_t)(p->nlock - n);
    p->lock_n[t] = (uint16_t)(p->lock_n[t] - n);
    for (k = t + 1; k < QD_PAT_NTRACK; k++)
        p->lock_start[k] = (uint16_t)(p->lock_start[k] - n);
}

/* track t's locks on steps [s0, s1) replaced by add[0..n) (their steps in [s0, s1), any order); QD_EFULL: nothing
 * changed */
static int replace_range(qd_pattern_t *p, int t, int s0, int s1, const qd_lock_t *add, int n)
{
    const int a = lock_pos(p, t, s0, 0), b = lock_pos(p, t, s1, 0);
    int i, j;
    if (p->nlock - (b - a) + n > QD_NLOCK)
        return QD_EFULL;
    pool_remove(p, t, a, b - a);
    for (i = 0; i < n; i++) {                           /* each in its sorted place (an equal one: replaced) */
        const int at = lock_pos(p, t, add[i].step, add[i].id);
        if (at < p->lock_start[t] + p->lock_n[t] && p->lock[at].step == add[i].step && p->lock[at].id == add[i].id)
            p->lock[at].value = add[i].value;
        else
            pool_insert(p, t, at, &add[i]);
    }
    for (j = s0; j < s1; j++)
        flag_locks(p, t, j);
    return QD_OK;
}

static int ts_ok(int t, int s) { return t >= 0 && t < QD_PAT_NTRACK && s >= 0 && s < QD_NSTEP; }

static int16_t clamp_lock(int id, int v)
{
    int16_t lo = 0, hi = 0;
    qd_lock_range(id, &lo, &hi);
    return (int16_t)(v < lo ? lo : v > hi ? hi : v);
}

const qd_lock_t *qd_locks(const qd_pattern_t *p, int t, int s, int *n)
{
    int a, b;
    if (!ts_ok(t, s)) {
        *n = 0;
        return p->lock;
    }
    a = lock_pos(p, t, s, 0);
    b = lock_pos(p, t, s + 1, 0);
    *n = b - a;
    return &p->lock[a];
}

int qd_lock_count(const qd_pattern_t *p, int t, int s)
{
    int n;
    qd_locks(p, t, s, &n);
    return n;
}

int qd_lock_get(const qd_pattern_t *p, int t, int s, int id, int16_t *value)
{
    int i;
    if (!ts_ok(t, s) || id < 0 || id >= QD_LK_N)
        return 0;
    i = lock_pos(p, t, s, id);
    if (i < p->lock_start[t] + p->lock_n[t] && p->lock[i].step == s && p->lock[i].id == id) {
        if (value)
            *value = p->lock[i].value;
        return 1;
    }
    return 0;
}

int qd_lock_set(qd_pattern_t *p, int t, int s, int id, int value)
{
    int i;
    qd_lock_t l;
    if (!ts_ok(t, s) || id < 0 || id >= QD_LK_N)
        return QD_EARG;
    i = lock_pos(p, t, s, id);
    if (i < p->lock_start[t] + p->lock_n[t] && p->lock[i].step == s && p->lock[i].id == id) {
        p->lock[i].value = clamp_lock(id, value);
        return QD_OK;
    }
    if (p->nlock >= QD_NLOCK)
        return QD_EFULL;
    l.step = (uint8_t)s;
    l.id = (uint8_t)id;
    l.value = clamp_lock(id, value);
    pool_insert(p, t, i, &l);
    p->step[t][s].flags |= QD_SF_LOCKS;
    if ((p->step[t][s].flags & QD_SF_TYPE) == QD_TRIG_NONE) {   /* a lock on an empty step: a LOCK step (Elektron's
                                                                 * habit, PLAN §8) */
        p->step[t][s].flags |= QD_TRIG_LOCK;
        p->step[t][s].chance = 100;
        p->used = 1;
    }
    return QD_OK;
}

int qd_lock_clear(qd_pattern_t *p, int t, int s, int id)
{
    int i;
    if (!ts_ok(t, s) || id < 0 || id >= QD_LK_N)
        return QD_EARG;
    i = lock_pos(p, t, s, id);
    if (i < p->lock_start[t] + p->lock_n[t] && p->lock[i].step == s && p->lock[i].id == id) {
        pool_remove(p, t, i, 1);
        flag_locks(p, t, s);
    }
    return QD_OK;
}

/* ---------------------------------------------------------------- steps --- */
const qd_step_t *qd_step(const qd_pattern_t *p, int t, int s)
{
    static const qd_step_t none;
    return ts_ok(t, s) ? &p->step[t][s] : &none;
}
int qd_step_type(const qd_step_t *st) { return st->flags & QD_SF_TYPE; }
int qd_step_ratchet(const qd_step_t *st) { return 1 + ((st->flags & QD_SF_RATCH) >> QD_SF_RATCH_SH); }
int qd_step_ramp(const qd_step_t *st) { return (st->flags & QD_SF_RAMP) >> QD_SF_RAMP_SH; }

int qd_step_notes(const qd_step_t *st, uint8_t *notes)
{
    int k, n = 0;
    if (!st->note[0])
        return 0;
    notes[n++] = st->note[0];
    for (k = 1; k < QD_NNOTE; k++)
        if (st->note[k]) {
            const int x = st->note[0] + st->note[k];
            notes[n++] = (uint8_t)(x > 127 ? 127 : x);
        }
    return n;
}

int qd_step_clear(qd_pattern_t *p, int t, int s)
{
    if (!ts_ok(t, s))
        return QD_EARG;
    replace_range(p, t, s, s + 1, 0, 0);               /* (removing never fills the pool) */
    memset(&p->step[t][s], 0, sizeof p->step[t][s]);
    return QD_OK;
}

int qd_step_set_trig(qd_pattern_t *p, int t, int s, int type)
{
    qd_step_t *st;
    if (!ts_ok(t, s) || type < QD_TRIG_NONE || type > QD_TRIG_TIE)
        return QD_EARG;
    if (type == QD_TRIG_NONE)
        return qd_step_clear(p, t, s);
    st = &p->step[t][s];
    if ((st->flags & QD_SF_TYPE) == QD_TRIG_NONE) {     /* a new trig: the track's defaults */
        const qd_tset_t *ts = &p->trk[t];
        const uint8_t keep = st->flags & QD_SF_LOCKS;
        memset(st, 0, sizeof *st);
        st->flags = keep;
        st->note[0] = ts->def_note ? ts->def_note : 60;
        st->vel = ts->def_vel ? ts->def_vel : 100;
        st->len = ts->def_len < QD_NLEN ? ts->def_len : QD_LEN_1;
        st->chance = 100;
    }
    if (!st->note[0])
        st->note[0] = 60;
    if (!st->vel)
        st->vel = 100;
    st->flags = (uint8_t)((st->flags & ~QD_SF_TYPE) | type);
    p->used = 1;
    return QD_OK;
}

static qd_step_t *trig_of(qd_pattern_t *p, int t, int s)   /* the step, a NOTE trig made if empty */
{
    if (!ts_ok(t, s))
        return 0;
    if ((p->step[t][s].flags & QD_SF_TYPE) == QD_TRIG_NONE)
        qd_step_set_trig(p, t, s, QD_TRIG_NOTE);
    return &p->step[t][s];
}

int qd_step_set_notes(qd_pattern_t *p, int t, int s, const uint8_t *notes, int n)
{
    uint8_t v[QD_NNOTE];
    int i, j, m = 0;
    qd_step_t *st;
    if (!ts_ok(t, s) || n < 1 || n > QD_NNOTE || !notes)
        return QD_EARG;
    for (i = 0; i < n; i++) {                           /* ascending, no repeats */
        const uint8_t x = notes[i];
        if (x < 1 || x > 127)
            return QD_EARG;
        for (j = 0; j < m && v[j] != x; j++)
            ;
        if (j < m)
            continue;
        for (j = m; j > 0 && v[j - 1] > x; j--)
            v[j] = v[j - 1];
        v[j] = x;
        m++;
    }
    for (i = 1; i < m; i++)
        if (v[i] - v[0] > 48)
            return QD_ENOTE;
    st = trig_of(p, t, s);
    memset(st->note, 0, sizeof st->note);
    st->note[0] = v[0];
    for (i = 1; i < m; i++)
        st->note[i] = (uint8_t)(v[i] - v[0]);
    return QD_OK;
}

int qd_step_set_root(qd_pattern_t *p, int t, int s, int note)
{
    if (!ts_ok(t, s) || note < 1 || note > 127)
        return QD_EARG;
    trig_of(p, t, s)->note[0] = (uint8_t)note;
    return QD_OK;
}

int qd_step_set_offset(qd_pattern_t *p, int t, int s, int k, int off)
{
    qd_step_t *st;
    int j;
    if (!ts_ok(t, s) || k < 1 || k >= QD_NNOTE || off < 0 || off > 48)
        return QD_EARG;
    st = trig_of(p, t, s);
    for (j = 1; j < QD_NNOTE; j++)
        if (off && j != k && st->note[j] == off)
            return QD_ENOTE;
    st->note[k] = (uint8_t)off;
    return QD_OK;
}

int qd_step_set_vel(qd_pattern_t *p, int t, int s, int vel)
{
    if (!ts_ok(t, s) || vel < 1 || vel > 127)
        return QD_EARG;
    trig_of(p, t, s)->vel = (uint8_t)vel;
    return QD_OK;
}
int qd_step_set_len(qd_pattern_t *p, int t, int s, int len)
{
    if (!ts_ok(t, s) || len < 0 || len >= QD_NLEN)
        return QD_EARG;
    trig_of(p, t, s)->len = (uint8_t)len;
    return QD_OK;
}
int qd_step_set_micro(qd_pattern_t *p, int t, int s, int micro)
{
    if (!ts_ok(t, s) || micro < -23 || micro > 23)
        return QD_EARG;
    trig_of(p, t, s)->micro = (int8_t)micro;
    return QD_OK;
}
int qd_step_set_ratchet(qd_pattern_t *p, int t, int s, int count, int ramp)
{
    qd_step_t *st;
    if (!ts_ok(t, s) || count < 1 || count > 8 || ramp < QD_RAMP_FLAT || ramp > QD_RAMP_ACCENT)
        return QD_EARG;
    st = trig_of(p, t, s);
    st->flags = (uint8_t)((st->flags & ~(QD_SF_RATCH | QD_SF_RAMP)) | ((count - 1) << QD_SF_RATCH_SH) |
                          (ramp << QD_SF_RAMP_SH));
    return QD_OK;
}
int qd_step_set_cond(qd_pattern_t *p, int t, int s, int cond)
{
    if (!ts_ok(t, s) || cond < 0 || cond >= QD_NCOND)
        return QD_EARG;
    trig_of(p, t, s)->cond = (uint8_t)cond;
    return QD_OK;
}
int qd_step_set_chance(qd_pattern_t *p, int t, int s, int chance)
{
    if (!ts_ok(t, s) || chance < 0 || chance > 100)
        return QD_EARG;
    trig_of(p, t, s)->chance = (uint8_t)chance;
    return QD_OK;
}
int qd_step_set_sound(qd_pattern_t *p, int t, int s, int sound)
{
    if (!ts_ok(t, s) || sound < 0 || sound > QD_QUAD_NPRESET)
        return QD_EARG;
    trig_of(p, t, s)->sound = (uint8_t)sound;
    return QD_OK;
}
int qd_step_set_strum(qd_pattern_t *p, int t, int s, int strum)
{
    if (!ts_ok(t, s) || strum < 0 || strum > 23)
        return QD_EARG;
    trig_of(p, t, s)->strum = (uint8_t)strum;
    return QD_OK;
}

/* ---------------------------------------------------------------- clear and copy --- */
/* steps [ss, ss + n) of src's track st onto [ds, ds + n) of dst's track dt, with their locks */
static int copy_steps(qd_pattern_t *dst, int dt, int ds, const qd_pattern_t *src, int st, int ss, int n)
{
    static qd_lock_t tmp[QD_NLOCK];                     /* (single-threaded, as the whole core) */
    qd_step_t steps[QD_NSTEP];
    int a, b, i, k = 0, r;
    if (dt < 0 || dt >= QD_PAT_NTRACK || st < 0 || st >= QD_PAT_NTRACK || n < 1 || ds < 0 || ss < 0 ||
        ds + n > QD_NSTEP || ss + n > QD_NSTEP)
        return QD_EARG;
    a = lock_pos(src, st, ss, 0);
    b = lock_pos(src, st, ss + n, 0);
    for (i = a; i < b; i++) {
        tmp[k] = src->lock[i];
        tmp[k].step = (uint8_t)(tmp[k].step - ss + ds);
        k++;
    }
    memcpy(steps, &src->step[st][ss], (size_t)n * sizeof steps[0]);
    r = replace_range(dst, dt, ds, ds + n, tmp, k);
    if (r != QD_OK)
        return r;
    for (i = 0; i < n; i++) {
        const uint8_t has = dst->step[dt][ds + i].flags & QD_SF_LOCKS;
        dst->step[dt][ds + i] = steps[i];
        dst->step[dt][ds + i].flags = (uint8_t)((steps[i].flags & ~QD_SF_LOCKS) | has);
        if (steps[i].flags & QD_SF_TYPE)
            dst->used = 1;
    }
    return QD_OK;
}

int qd_step_copy(qd_pattern_t *dst, int dt, int ds, const qd_pattern_t *src, int st, int ss)
{
    return copy_steps(dst, dt, ds, src, st, ss, 1);
}

int qd_page_clear(qd_pattern_t *p, int t, int page)
{
    int s;
    if (t < 0 || t >= QD_PAT_NTRACK || page < 0 || page >= QD_NSTEP / QD_PAGE)
        return QD_EARG;
    replace_range(p, t, page * QD_PAGE, (page + 1) * QD_PAGE, 0, 0);
    for (s = page * QD_PAGE; s < (page + 1) * QD_PAGE; s++)
        memset(&p->step[t][s], 0, sizeof p->step[t][s]);
    return QD_OK;
}

int qd_page_copy(qd_pattern_t *dst, int dt, int dpage, const qd_pattern_t *src, int st, int spage)
{
    if (dpage < 0 || dpage >= QD_NSTEP / QD_PAGE || spage < 0 || spage >= QD_NSTEP / QD_PAGE)
        return QD_EARG;
    return copy_steps(dst, dt, dpage * QD_PAGE, src, st, spage * QD_PAGE, QD_PAGE);
}

int qd_track_clear(qd_pattern_t *p, int t)
{
    if (t < 0 || t >= QD_PAT_NTRACK)
        return QD_EARG;
    replace_range(p, t, 0, QD_NSTEP, 0, 0);
    memset(p->step[t], 0, sizeof p->step[t]);
    return QD_OK;
}

int qd_track_copy(qd_pattern_t *dst, int dt, const qd_pattern_t *src, int st)
{
    qd_tset_t ts;
    int r;
    if (dt < 0 || dt >= QD_PAT_NTRACK || st < 0 || st >= QD_PAT_NTRACK)
        return QD_EARG;
    ts = src->trk[st];
    r = copy_steps(dst, dt, 0, src, st, 0, QD_NSTEP);
    if (r == QD_OK)
        dst->trk[dt] = ts;
    return r;
}
