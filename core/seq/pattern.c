/* SPDX-License-Identifier: GPL-3.0-only */
/* The pattern model and its editing API (pattern.h). */
#include <string.h>

#include "pattern.h"
#include "scale.h"
#include "sounds.h"

/* ---------------------------------------------------------------- tables --- */
static const int16_t QD_LEN_E[QD_NLEN] = {1, 2, 4, 6, 8, 12, 16, 24, 32, 48, 64, 96, 128, 192, 256, 384, 512, 768,
                                          1024, -1};
static const char *const QD_LEN_N[QD_NLEN] = {"1/8", "1/4", "1/2", "3/4", "1", "1.5", "2", "3", "4", "6", "8", "12",
                                              "16", "24", "32", "48", "64", "96", "128", "INF"};

int qd_len_eighths(int len) { return len < 0 || len >= QD_NLEN ? 8 : QD_LEN_E[len]; }
const char *qd_len_name(int len) { return len < 0 || len >= QD_NLEN ? "?" : QD_LEN_N[len]; }

/* a rate code's step in ticks: the base's (16 sixteenths for 1/1, halving to 1/4 of one for 1/64), x 3/2 dotted,
 * x 2/3 triplet (all whole: pattern.h) */
int64_t qd_rate_ticks(int rate)
{
    int64_t l;
    if (rate < 0 || rate >= QD_NRATE)
        return QD_TPS;
    l = (int64_t)QD_TPS * 16 >> (rate / 3);
    return rate % 3 == QD_RTYPE_DOTTED ? l * 3 / 2 : rate % 3 == QD_RTYPE_TRIPLET ? l * 2 / 3 : l;
}
int qd_rate_make(int base, int type)
{
    base = base < 0 ? 0 : base >= QD_NVAL ? QD_NVAL - 1 : base;
    type = type < 0 ? 0 : type >= QD_NRTYPE ? QD_NRTYPE - 1 : type;
    return QD_RATE_CODE(base, type);
}
int qd_rate_val(int rate) { return rate < 0 || rate >= QD_NRATE ? 4 : rate / 3; }
int qd_rate_type(int rate) { return rate < 0 || rate >= QD_NRATE ? 0 : rate % 3; }
const char *qd_rate_val_name(int base)
{
    static const char *const N[QD_NVAL] = {"1/1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/64"};
    return N[base < 0 ? 0 : base >= QD_NVAL ? QD_NVAL - 1 : base];
}
const char *qd_rate_name(int rate)
{
    static const char *const N[QD_NRATE] = {"1/1", "1/1.", "1/1T", "1/2", "1/2.", "1/2T", "1/4", "1/4.", "1/4T",
                                            "1/8", "1/8.", "1/8T", "1/16", "1/16.", "1/16T", "1/32", "1/32.", "1/32T",
                                            "1/64", "1/64.", "1/64T"};
    return rate < 0 || rate >= QD_NRATE ? "?" : N[rate];
}
int qd_tset_rate(const qd_tset_t *ts) { return qd_rate_make(ts->rbase, ts->rtype); }
void qd_tset_set_rate(qd_tset_t *ts, int rate)
{
    ts->rbase = (uint8_t)qd_rate_val(rate);
    ts->rtype = (uint8_t)qd_rate_type(rate);
}

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

int qd_ratch_count(int ratch) { return 1 + (ratch & QD_RT_COUNT); }
int qd_ratch_ramp(int ratch) { return (ratch & QD_RT_RAMP) >> QD_RT_RAMP_SH; }
int qd_ratch_make(int count, int ramp)
{
    count = count < 1 ? 1 : count > 8 ? 8 : count;
    ramp = ramp < 0 ? 0 : ramp > QD_RAMP_ACCENT ? QD_RAMP_ACCENT : ramp;
    return (count - 1) | (ramp << QD_RT_RAMP_SH);
}

/* ---------------------------------------------------------------- patterns --- */
void qd_sdef_init(qd_sdef_t *d)
{
    memset(d, 0, sizeof *d);
    d->note[0] = 60;
    d->vel = 100;
    d->len = QD_LEN_1;
    d->chance = 100;
    d->cond = QD_COND_ALWAYS;
    d->type = QD_TRIG_NOTE;
}

void qd_tset_init(qd_tset_t *s, int track)
{
    int p;
    memset(s, 0, sizeof *s);
    qd_sdef_init(&s->def);
    s->chan = (uint8_t)(track & 15);
    s->limit = 6;
    s->reserve = 0;
    qd_tset_set_rate(s, QD_RATE_1);
    s->loop = 1;
    for (p = 0; p < QD_NPAGE; p++) {
        s->pstart[p] = 0;
        s->pend[p] = QD_PAGE - 1;
    }
    s->scale = QD_SCALE_CHROMATIC;
    s->root = 0;
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
    for (t = 0; t < QD_PAT_NTRACK; t++)
        qd_tsound_default(t, &p->snd[t]);
}

void qd_project_init(qd_project_t *pr)
{
    int k;
    for (k = 0; k < QD_NPAT; k++)
        qd_pat_init(&pr->pat[k]);
    qd_sounds_init(pr);
    pr->pool_empty = 0;
    pr->key_scale = QD_SCALE_CHROMATIC;
    pr->key_root = 0;
    pr->launch = 0;                                     /* (QD_LAUNCH_END) */
    pr->rec_mode = 0;                                   /* P1.3y: LIVE, as played (0 %), REPLACE */
    pr->rec_quant = 0;
    pr->rec_dub = 0;
    pr->rec_ekeys = 0;                                  /* P1.5b: the editor's keyboard: SCALE */
    memset(pr->mfx, 0, sizeof pr->mfx);                 /* P1.6c: the MIDI FX groups empty */
    pr->rec_auto = 1;                                   /* P1.6d: AUTOMATION ON (live recording's knobs, as before) */
}

int qd_step_mfx(const qd_pattern_t *p, int t, int s)
{
    int m;
    if (t < 0 || t >= QD_PAT_NTRACK)
        return 0;
    m = s >= 0 && s < QD_NSTEP ? p->step[t][s].mfx : QD_SMFX_TRACK;
    if (m == QD_SMFX_OFF)
        return 0;
    if (m >= QD_SMFX_G1 && m < QD_SMFX_G1 + QD_MFX_NGROUP)
        return m - QD_SMFX_G1 + 1;
    return p->trk[t].mfx <= QD_MFX_NGROUP ? p->trk[t].mfx : 0;
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

/* ---------------------------------------------------------------- pages --- */
int qd_track_loop(const qd_tset_t *ts) { return ts->loop ? ts->loop : 1; }

static int page_n(const qd_tset_t *ts, int p)
{
    const int a = ts->pstart[p] & 15, b = ts->pend[p] & 15;
    return b >= a ? b - a + 1 : 1;
}

int qd_track_len(const qd_tset_t *ts)
{
    const int m = qd_track_loop(ts);
    int p, n = 0;
    for (p = 0; p < QD_NPAGE; p++)
        if ((m >> p) & 1)
            n += page_n(ts, p);
    return n < 1 ? 1 : n;
}

int qd_track_loop_step(const qd_tset_t *ts, int pos)
{
    const int m = qd_track_loop(ts), len = qd_track_len(ts);
    int p;
    pos %= len;
    if (pos < 0)
        pos += len;
    for (p = 0; p < QD_NPAGE; p++) {
        int n;
        if (!((m >> p) & 1))
            continue;
        n = page_n(ts, p);
        if (pos < n)
            return p * QD_PAGE + (ts->pstart[p] & 15) + pos;
        pos -= n;
    }
    return 0;
}

int qd_track_step_pos(const qd_tset_t *ts, int s)
{
    const int m = qd_track_loop(ts);
    int p, pos = 0;
    if (s < 0 || s >= QD_NSTEP)
        return -1;
    for (p = 0; p < QD_NPAGE; p++) {
        if (!((m >> p) & 1))
            continue;
        if (p == s / QD_PAGE) {
            const int k = s % QD_PAGE, a = ts->pstart[p] & 15, b = ts->pend[p] & 15;
            return k >= a && k <= (b >= a ? b : a) ? pos + k - a : -1;
        }
        pos += page_n(ts, p);
    }
    return -1;
}

int qd_page_in_range(const qd_tset_t *ts, int s)
{
    const int p = s / QD_PAGE, k = s % QD_PAGE;
    if (s < 0 || s >= QD_NSTEP)
        return 0;
    {
        const int a = ts->pstart[p] & 15, b = ts->pend[p] & 15;
        return k >= a && k <= (b >= a ? b : a);
    }
}

int qd_page_set_range(qd_pattern_t *p, int t, int page, int start, int end)
{
    if (t < 0 || t >= QD_PAT_NTRACK || page < 0 || page >= QD_NPAGE || start < 0 || end > 15 || start > end)
        return QD_EARG;
    p->trk[t].pstart[page] = (uint8_t)start;
    p->trk[t].pend[page] = (uint8_t)end;
    return QD_OK;
}

int qd_page_set_loop(qd_pattern_t *p, int t, int mask)
{
    if (t < 0 || t >= QD_PAT_NTRACK)
        return QD_EARG;
    p->trk[t].loop = (uint8_t)(mask & 0xFF ? mask & 0xFF : 1);
    return QD_OK;
}

int qd_track_set_length(qd_pattern_t *p, int t, int n)
{
    int k, pg;
    if (t < 0 || t >= QD_PAT_NTRACK || n < 1 || n > QD_NSTEP)
        return QD_EARG;
    k = (n + QD_PAGE - 1) / QD_PAGE;
    for (pg = 0; pg < k; pg++) {
        p->trk[t].pstart[pg] = 0;
        p->trk[t].pend[pg] = (uint8_t)(pg < k - 1 ? QD_PAGE - 1 : n - (k - 1) * QD_PAGE - 1);
    }
    p->trk[t].loop = (uint8_t)((1u << k) - 1u);
    return QD_OK;
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
        const int64_t l = (int64_t)qd_track_len(&p->trk[t]) * qd_rate_ticks(qd_tset_rate(&p->trk[t]));
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
    if (!(p->step[t][s].flags & QD_SF_ON)) {            /* a lock on an empty step: a LOCK step (Elektron's habit) */
        p->step[t][s].flags |= QD_SF_ON;
        p->step[t][s].type = QD_TRIG_LOCK;
        p->step[t][s].ovr |= QD_OV(QD_PM_TRIG);
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
int qd_step_is_on(const qd_step_t *st) { return (st->flags & QD_SF_ON) != 0; }
int qd_step_overrides(const qd_step_t *st, int pm) { return (st->ovr >> pm) & 1u; }

int qd_notes_abs(const uint8_t *note6, uint8_t *notes)
{
    int k, n = 0, i, j;
    for (k = 0; k < QD_NNOTE; k++)                      /* (P1.3f: the slots hold pitches, 0 none) */
        if (note6[k])
            notes[n++] = note6[k] > 127 ? 127 : note6[k];
    for (i = 1; i < n; i++)                             /* ascending (the slots keep their own order) */
        for (j = i; j > 0 && notes[j - 1] > notes[j]; j--) {
            const uint8_t x = notes[j];
            notes[j] = notes[j - 1];
            notes[j - 1] = x;
        }
    return n;
}

void qd_step_resolve(const qd_pattern_t *p, int t, int s, qd_rstep_t *r)
{
    qd_step_resolve_def(&p->trk[t < 0 || t >= QD_PAT_NTRACK ? 0 : t].def, qd_step(p, t, s), r);
}

qd_pattern_t *qd_pat_scratch(void)
{
    static qd_pattern_t scratch;
    return &scratch;
}

void qd_step_resolve_def(const qd_sdef_t *d, const qd_step_t *st, qd_rstep_t *r)
{
    const uint16_t o = st->ovr;
#define PICK(pm, sv, dv) (((o >> (pm)) & 1u) ? (sv) : (dv))
    memset(r, 0, sizeof *r);
    r->ovr = o;
    r->on = qd_step_is_on(st);
    if ((o >> QD_PM_NOTE) & 1u)
        memcpy(r->note, st->note, sizeof r->note);
    else
        memcpy(r->note, d->note, sizeof r->note);
    r->nn = qd_notes_abs(r->note, r->notes);
    if (!r->nn) {                                       /* (no notes at all: C4) */
        r->note[0] = 60;
        r->nn = qd_notes_abs(r->note, r->notes);
    }
    r->func = st->func < QD_NFN ? st->func : 0;          /* P1.3w: the step's own (no default) */
    r->vel = PICK(QD_PM_VEL, st->vel, d->vel);
    r->vel = r->vel < 1 ? 1 : r->vel > 127 ? 127 : r->vel;
    r->len = PICK(QD_PM_LEN, st->len, d->len);
    r->micro = PICK(QD_PM_MICRO, st->micro, d->micro);
    {
        const int rt = PICK(QD_PM_RATCH, st->ratch, d->ratch);
        r->ratch = qd_ratch_count(rt);
        r->ramp = qd_ratch_ramp(rt);
    }
    r->chance = PICK(QD_PM_CHANCE, st->chance, d->chance);
    r->cond = PICK(QD_PM_COND, st->cond, d->cond);
    r->type = r->on ? PICK(QD_PM_TRIG, st->type, d->type) : QD_TRIG_NONE;
    if (r->on && (r->type < QD_TRIG_NOTE || r->type > QD_TRIG_TIE))
        r->type = QD_TRIG_NOTE;
    r->sound = PICK(QD_PM_SOUND, st->sound, 0);
    r->strum = PICK(QD_PM_STRUM, st->strum, d->strum);
#undef PICK
}

int qd_step_type(const qd_pattern_t *p, int t, int s)
{
    qd_rstep_t r;
    qd_step_resolve(p, t, s, &r);
    return r.type;
}

int qd_step_notes(const qd_pattern_t *p, int t, int s, uint8_t *notes)
{
    qd_rstep_t r;
    qd_step_resolve(p, t, s, &r);
    memcpy(notes, r.notes, sizeof r.notes);
    return r.nn;
}

int qd_step_clear(qd_pattern_t *p, int t, int s)
{
    if (!ts_ok(t, s))
        return QD_EARG;
    replace_range(p, t, s, s + 1, 0, 0);               /* (removing never fills the pool) */
    memset(&p->step[t][s], 0, sizeof p->step[t][s]);
    return QD_OK;
}

int qd_step_on(qd_pattern_t *p, int t, int s)
{
    if (!ts_ok(t, s))
        return QD_EARG;
    p->step[t][s].flags |= QD_SF_ON;
    p->used = 1;
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
    st->flags |= QD_SF_ON;
    st->type = (uint8_t)type;
    st->ovr |= QD_OV(QD_PM_TRIG);
    p->used = 1;
    return QD_OK;
}

/* the step to set parameter pm on: on (a trig made if empty), its value of pm made its own (the track's default
 * copied in when it did not override it) */
static qd_step_t *own(qd_pattern_t *p, int t, int s, int pm)
{
    qd_step_t *st = &p->step[t][s];
    const qd_sdef_t *d = &p->trk[t].def;
    if (!(st->flags & QD_SF_ON))
        qd_step_on(p, t, s);
    if (!((st->ovr >> pm) & 1u)) {
        switch (pm) {
        case QD_PM_NOTE: memcpy(st->note, d->note, sizeof st->note); break;
        case QD_PM_VEL: st->vel = d->vel; break;
        case QD_PM_LEN: st->len = d->len; break;
        case QD_PM_MICRO: st->micro = d->micro; break;
        case QD_PM_RATCH: st->ratch = d->ratch; break;
        case QD_PM_CHANCE: st->chance = d->chance; break;
        case QD_PM_COND: st->cond = d->cond; break;
        case QD_PM_TRIG: st->type = d->type; break;
        case QD_PM_SOUND: st->sound = 0; break;
        case QD_PM_STRUM: st->strum = d->strum; break;
        default: break;
        }
        st->ovr |= QD_OV(pm);
    }
    if (pm == QD_PM_NOTE) {                             /* (no notes at all: C4) */
        int k, any = 0;
        for (k = 0; k < QD_NNOTE; k++)
            any |= st->note[k];
        if (!any)
            st->note[0] = 60;
    }
    return st;
}

int qd_step_reset(qd_pattern_t *p, int t, int s, int pm)
{
    if (!ts_ok(t, s) || pm < 0 || pm >= QD_NPM)
        return QD_EARG;
    p->step[t][s].ovr &= (uint16_t)~QD_OV(pm);
    return QD_OK;
}

/* notes[0..n) -> the slots in that order (P1.3f: pitches, not offsets; note k is slot k), repeats dropped, the rest 0 */
static int notes_pack(const uint8_t *notes, int n, uint8_t *out)
{
    uint8_t v[QD_NNOTE];
    int i, j, m = 0;
    if (n < 1 || n > QD_NNOTE || !notes)
        return QD_EARG;
    for (i = 0; i < n; i++) {
        const uint8_t x = notes[i];
        if (x < 1 || x > 127)
            return QD_EARG;
        for (j = 0; j < m && v[j] != x; j++)
            ;
        if (j == m)
            v[m++] = x;
    }
    memset(out, 0, QD_NNOTE);
    memcpy(out, v, (size_t)m);
    return QD_OK;
}

int qd_step_set_notes(qd_pattern_t *p, int t, int s, const uint8_t *notes, int n)
{
    uint8_t v[QD_NNOTE];
    int r;
    if (!ts_ok(t, s))
        return QD_EARG;
    r = notes_pack(notes, n, v);
    if (r != QD_OK)
        return r;
    memcpy(own(p, t, s, QD_PM_NOTE)->note, v, sizeof v);
    return QD_OK;
}

/* one slot of notes (the step's, or the track's default chord): pitch 0 empties it; a pitch another slot has, or the
 * last note emptied: refused */
static int slot_ok(const uint8_t *cur, int k, int pitch)
{
    int j, left = 0;
    for (j = 0; j < QD_NNOTE; j++) {
        if (j == k)
            continue;
        if (pitch && cur[j] == pitch)
            return QD_ENOTE;
        left += cur[j] != 0;
    }
    return !pitch && !left ? QD_EARG : QD_OK;
}

int qd_step_set_note(qd_pattern_t *p, int t, int s, int k, int pitch)
{
    int r;
    if (!ts_ok(t, s) || k < 0 || k >= QD_NNOTE || pitch < 0 || pitch > 127)
        return QD_EARG;
    {
        const qd_step_t *c = &p->step[t][s];
        r = slot_ok((c->ovr >> QD_PM_NOTE) & 1u ? c->note : p->trk[t].def.note, k, pitch);
        if (r != QD_OK)
            return r;
    }
    own(p, t, s, QD_PM_NOTE)->note[k] = (uint8_t)pitch;
    return QD_OK;
}

/* all six slots at once (a chord moved together: no slot is ever another's pitch mid-way) */
static int slots_ok(const uint8_t *v)
{
    int i, j, n = 0;
    for (i = 0; i < QD_NNOTE; i++) {
        if (v[i] > 127)
            return QD_EARG;
        n += v[i] != 0;
        for (j = 0; j < i; j++)
            if (v[i] && v[j] == v[i])
                return QD_ENOTE;
    }
    return n ? QD_OK : QD_EARG;
}

int qd_step_set_slots(qd_pattern_t *p, int t, int s, const uint8_t *slots)
{
    int r;
    if (!ts_ok(t, s) || !slots)
        return QD_EARG;
    r = slots_ok(slots);
    if (r == QD_OK)
        memcpy(own(p, t, s, QD_PM_NOTE)->note, slots, QD_NNOTE);
    return r;
}

int qd_tdef_set_slots(qd_pattern_t *p, int t, const uint8_t *slots)
{
    int r;
    if (t < 0 || t >= QD_PAT_NTRACK || !slots)
        return QD_EARG;
    r = slots_ok(slots);
    if (r == QD_OK)
        memcpy(p->trk[t].def.note, slots, QD_NNOTE);
    return r;
}

int qd_tdef_set_note(qd_pattern_t *p, int t, int k, int pitch)
{
    int r;
    if (t < 0 || t >= QD_PAT_NTRACK || k < 0 || k >= QD_NNOTE || pitch < 0 || pitch > 127)
        return QD_EARG;
    r = slot_ok(p->trk[t].def.note, k, pitch);
    if (r == QD_OK)
        p->trk[t].def.note[k] = (uint8_t)pitch;
    return r;
}

int qd_step_set_vel(qd_pattern_t *p, int t, int s, int vel)
{
    if (!ts_ok(t, s) || vel < 1 || vel > 127)
        return QD_EARG;
    own(p, t, s, QD_PM_VEL)->vel = (uint8_t)vel;
    return QD_OK;
}
int qd_step_set_len(qd_pattern_t *p, int t, int s, int len)
{
    if (!ts_ok(t, s) || len < 0 || len >= QD_NLEN)
        return QD_EARG;
    own(p, t, s, QD_PM_LEN)->len = (uint8_t)len;
    return QD_OK;
}
int qd_step_set_micro(qd_pattern_t *p, int t, int s, int micro)
{
    if (!ts_ok(t, s) || micro < -23 || micro > 23)
        return QD_EARG;
    own(p, t, s, QD_PM_MICRO)->micro = (int8_t)micro;
    return QD_OK;
}
int qd_step_set_ratchet(qd_pattern_t *p, int t, int s, int count, int ramp)
{
    if (!ts_ok(t, s) || count < 1 || count > 8 || ramp < QD_RAMP_FLAT || ramp > QD_RAMP_ACCENT)
        return QD_EARG;
    own(p, t, s, QD_PM_RATCH)->ratch = (uint8_t)qd_ratch_make(count, ramp);
    return QD_OK;
}
int qd_step_set_cond(qd_pattern_t *p, int t, int s, int cond)
{
    if (!ts_ok(t, s) || cond < 0 || cond >= QD_NCOND)
        return QD_EARG;
    own(p, t, s, QD_PM_COND)->cond = (uint8_t)cond;
    return QD_OK;
}
int qd_step_set_chance(qd_pattern_t *p, int t, int s, int chance)
{
    if (!ts_ok(t, s) || chance < 0 || chance > 100)
        return QD_EARG;
    own(p, t, s, QD_PM_CHANCE)->chance = (uint8_t)chance;
    return QD_OK;
}
int qd_step_set_sound(qd_pattern_t *p, int t, int s, int sound)
{
    if (!ts_ok(t, s) || sound < 0 || sound > QD_NSOUND)
        return QD_EARG;
    if (!sound) {                                       /* the track's sound: the default, so no override */
        if (!(p->step[t][s].flags & QD_SF_ON))
            qd_step_on(p, t, s);
        p->step[t][s].sound = 0;
        p->step[t][s].ovr &= (uint16_t)~QD_OV(QD_PM_SOUND);
        return QD_OK;
    }
    own(p, t, s, QD_PM_SOUND)->sound = (uint8_t)sound;
    return QD_OK;
}
int qd_step_set_strum(qd_pattern_t *p, int t, int s, int strum)
{
    if (!ts_ok(t, s) || strum < 0 || strum > 23)
        return QD_EARG;
    own(p, t, s, QD_PM_STRUM)->strum = (uint8_t)strum;
    return QD_OK;
}

int qd_step_set_func(qd_pattern_t *p, int t, int s, int func)
{
    qd_step_t *st;
    if (!ts_ok(t, s) || func < 0 || func >= QD_NFN)
        return QD_EARG;
    st = &p->step[t][s];
    if (func && !(st->flags & QD_SF_ON))                /* (a function on an empty step: a trig) */
        st->flags |= QD_SF_ON;
    st->func = (uint8_t)func;
    p->used = 1;
    return QD_OK;
}

int qd_step_set_mfx(qd_pattern_t *p, int t, int s, int m)
{
    qd_step_t *st;
    if (!ts_ok(t, s) || m < 0 || m >= QD_SMFX_G1 + QD_MFX_NGROUP)
        return QD_EARG;
    st = &p->step[t][s];
    if (m && !(st->flags & QD_SF_ON))                   /* (a MIDI FX on an empty step: a trig) */
        st->flags |= QD_SF_ON;
    st->mfx = (uint8_t)m;
    p->used = 1;
    return QD_OK;
}

const char *qd_fn_name(int f, char *out)
{
    static const char *const N[QD_FN_JUMP] = {"NONE", "RESTART", "REVERSE", "FORWARD", "FLIP", "RANDOM", "RND FUNC"};
    if (QD_FN_IS_LEAP(f)) {                             /* "LEAP +2", "LEAP -3" */
        const int n = QD_FN_LEAP_N(f), m = n < 0 ? -n : n;
        int i = 0;
        const char *a = "LEAP ";
        while (*a)
            out[i++] = *a++;
        out[i++] = n < 0 ? '-' : '+';
        if (m >= 10)
            out[i++] = (char)('0' + m / 10);
        out[i++] = (char)('0' + m % 10);
        out[i] = 0;
        return out;
    }
    if (f >= QD_FN_JUMP && f < QD_FN_LEAP) {
        const int n = f - QD_FN_JUMP + 1;
        int i = 0;
        const char *a = "JUMP ";
        while (*a)
            out[i++] = *a++;
        if (n >= 100)
            out[i++] = (char)('0' + n / 100);
        if (n >= 10)
            out[i++] = (char)('0' + n / 10 % 10);
        out[i++] = (char)('0' + n % 10);
        out[i] = 0;
        return out;
    }
    {
        const char *a = N[f > 0 && f < QD_FN_JUMP ? f : 0];
        int i = 0;
        while ((out[i] = a[i]) != 0)
            i++;
    }
    return out;
}

/* ---------------------------------------------------------------- the track's defaults --- */
qd_sdef_t *qd_tdef(qd_pattern_t *p, int t) { return &p->trk[t < 0 || t >= QD_PAT_NTRACK ? 0 : t].def; }

int qd_tdef_set_notes(qd_pattern_t *p, int t, const uint8_t *notes, int n)
{
    uint8_t v[QD_NNOTE];
    int r;
    if (t < 0 || t >= QD_PAT_NTRACK)
        return QD_EARG;
    r = notes_pack(notes, n, v);
    if (r == QD_OK)
        memcpy(p->trk[t].def.note, v, sizeof v);
    return r;
}

int qd_tdef_set(qd_pattern_t *p, int t, int pm, int v)
{
    qd_sdef_t *d;
    if (t < 0 || t >= QD_PAT_NTRACK)
        return QD_EARG;
    d = &p->trk[t].def;
    switch (pm) {
    case QD_PM_NOTE:
        if (v < 1 || v > 127)
            return QD_EARG;
        d->note[0] = (uint8_t)v;
        return QD_OK;
    case QD_PM_VEL:
        if (v < 1 || v > 127)
            return QD_EARG;
        d->vel = (uint8_t)v;
        return QD_OK;
    case QD_PM_LEN:
        if (v < 0 || v >= QD_NLEN)
            return QD_EARG;
        d->len = (uint8_t)v;
        return QD_OK;
    case QD_PM_MICRO:
        if (v < -23 || v > 23)
            return QD_EARG;
        d->micro = (int8_t)v;
        return QD_OK;
    case QD_PM_RATCH:
        if (v < 0 || v > (int)(QD_RT_COUNT | QD_RT_RAMP))
            return QD_EARG;
        d->ratch = (uint8_t)v;
        return QD_OK;
    case QD_PM_CHANCE:
        if (v < 0 || v > 100)
            return QD_EARG;
        d->chance = (uint8_t)v;
        return QD_OK;
    case QD_PM_COND:
        if (v < 0 || v >= QD_NCOND)
            return QD_EARG;
        d->cond = (uint8_t)v;
        return QD_OK;
    case QD_PM_TRIG:
        if (v < QD_TRIG_NOTE || v > QD_TRIG_TIE)
            return QD_EARG;
        d->type = (uint8_t)v;
        return QD_OK;
    case QD_PM_STRUM:
        if (v < 0 || v > 23)
            return QD_EARG;
        d->strum = (uint8_t)v;
        return QD_OK;
    default:
        return QD_EARG;
    }
}

int qd_tdef_get(const qd_pattern_t *p, int t, int pm)
{
    const qd_sdef_t *d = &p->trk[t < 0 || t >= QD_PAT_NTRACK ? 0 : t].def;
    switch (pm) {
    case QD_PM_NOTE: return d->note[0];
    case QD_PM_VEL: return d->vel;
    case QD_PM_LEN: return d->len;
    case QD_PM_MICRO: return d->micro;
    case QD_PM_RATCH: return d->ratch;
    case QD_PM_CHANCE: return d->chance;
    case QD_PM_COND: return d->cond;
    case QD_PM_TRIG: return d->type;
    case QD_PM_STRUM: return d->strum;
    default: return 0;
    }
}

/* ---------------------------------------------------------------- clear and copy --- */
int qd_steps_get(const qd_pattern_t *p, int t, int s, int n, qd_step_t *steps, qd_lock_t *lk, int *nlk)
{
    int a, b, i, k = 0;
    if (t < 0 || t >= QD_PAT_NTRACK || n < 1 || s < 0 || s + n > QD_NSTEP)
        return QD_EARG;
    a = lock_pos(p, t, s, 0);
    b = lock_pos(p, t, s + n, 0);
    for (i = a; i < b; i++) {
        lk[k] = p->lock[i];
        lk[k].step = (uint8_t)(lk[k].step - s);
        k++;
    }
    *nlk = k;
    memcpy(steps, &p->step[t][s], (size_t)n * sizeof steps[0]);
    return QD_OK;
}

int qd_steps_put(qd_pattern_t *p, int t, int s, int n, const qd_step_t *steps, const qd_lock_t *lk, int nlk)
{
    static qd_lock_t tmp[QD_NLOCK];                     /* (single-threaded, as the whole core) */
    int i, r;
    if (t < 0 || t >= QD_PAT_NTRACK || n < 1 || s < 0 || s + n > QD_NSTEP || nlk < 0 || nlk > QD_NLOCK)
        return QD_EARG;
    for (i = 0; i < nlk; i++) {
        tmp[i] = lk[i];
        tmp[i].step = (uint8_t)(lk[i].step + s);
    }
    r = replace_range(p, t, s, s + n, tmp, nlk);
    if (r != QD_OK)
        return r;
    for (i = 0; i < n; i++) {
        const uint8_t has = p->step[t][s + i].flags & QD_SF_LOCKS;
        p->step[t][s + i] = steps[i];
        p->step[t][s + i].flags = (uint8_t)((steps[i].flags & ~QD_SF_LOCKS) | has);
        if (steps[i].flags & QD_SF_ON)
            p->used = 1;
    }
    return QD_OK;
}

/* steps [ss, ss + n) of src's track st onto [ds, ds + n) of dst's track dt, with their locks */
static int copy_steps(qd_pattern_t *dst, int dt, int ds, const qd_pattern_t *src, int st, int ss, int n)
{
    static qd_lock_t lk[QD_NLOCK];
    static qd_step_t steps[QD_NSTEP];
    int nl = 0, r;
    if (dt < 0 || dt >= QD_PAT_NTRACK || ds < 0 || ds + n > QD_NSTEP)
        return QD_EARG;
    r = qd_steps_get(src, st, ss, n, steps, lk, &nl);
    if (r != QD_OK)
        return r;
    return qd_steps_put(dst, dt, ds, n, steps, lk, nl);
}

int qd_step_copy(qd_pattern_t *dst, int dt, int ds, const qd_pattern_t *src, int st, int ss)
{
    return copy_steps(dst, dt, ds, src, st, ss, 1);
}

int qd_page_clear(qd_pattern_t *p, int t, int page)
{
    int s;
    if (t < 0 || t >= QD_PAT_NTRACK || page < 0 || page >= QD_NPAGE)
        return QD_EARG;
    replace_range(p, t, page * QD_PAGE, (page + 1) * QD_PAGE, 0, 0);
    for (s = page * QD_PAGE; s < (page + 1) * QD_PAGE; s++)
        memset(&p->step[t][s], 0, sizeof p->step[t][s]);
    return QD_OK;
}

int qd_page_copy(qd_pattern_t *dst, int dt, int dpage, const qd_pattern_t *src, int st, int spage)
{
    if (dpage < 0 || dpage >= QD_NPAGE || spage < 0 || spage >= QD_NPAGE)
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
