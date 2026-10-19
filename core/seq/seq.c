/* SPDX-License-Identifier: GPL-3.0-only */
/* The sequencer (seq.h). */
#include <string.h>

#include "seq.h"
#include "sounds.h"

#define QD_TIE_SKIP 8                   /* OFFs held back for a TIE, per track (qd_seq_t.skip_*) */

static uint64_t splitmix(uint64_t z)
{
    z += 0x9E3779B97F4A7C15ull;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}

uint32_t qd_seq_roll(uint64_t seed, int track, int64_t n)
{
    return (uint32_t)(splitmix(seed ^ ((uint64_t)(track & 255) << 56) ^ (uint64_t)n) % 100u);
}

/* a pattern's seed: the run's, mixed with where the pattern started (a chain repeated rolls anew) */
static uint64_t seg_seed(const qd_seq_t *s, int64_t origin) { return splitmix(s->seed_play ^ (uint64_t)origin); }

static int64_t L_of(const qd_pattern_t *P, int t) { return qd_rate_ticks(qd_tset_rate(&P->trk[t])); }
static int len_of(const qd_pattern_t *P, int t) { return qd_track_len(&P->trk[t]); }

/* ---------------------------------------------------------------- the queue --- */
static int ev_less(const qd_sev_t *a, const qd_sev_t *b)
{
    if (a->tick != b->tick)
        return a->tick < b->tick;
    if (a->kind != b->kind)
        return a->kind < b->kind;
    if (a->track != b->track)
        return a->track < b->track;
    return a->seq < b->seq;
}

static void heap_up(qd_sev_t *h, int i)
{
    while (i > 0) {
        const int p = (i - 1) / 2;
        qd_sev_t x;
        if (!ev_less(&h[i], &h[p]))
            break;
        x = h[i], h[i] = h[p], h[p] = x;
        i = p;
    }
}

static void heap_down(qd_sev_t *h, int n, int i)
{
    for (;;) {
        int l = 2 * i + 1, r = l + 1, m = i;
        qd_sev_t x;
        if (l < n && ev_less(&h[l], &h[m]))
            m = l;
        if (r < n && ev_less(&h[r], &h[m]))
            m = r;
        if (m == i)
            break;
        x = h[i], h[i] = h[m], h[m] = x;
        i = m;
    }
}

static void rec_unref(qd_seq_t *s, int r)
{
    if (r < 0 || r >= QD_SEQ_NREC || !s->rec[r].used)
        return;
    if (s->rec[r].refs)
        s->rec[r].refs--;
    if (!s->rec[r].refs)
        s->rec[r].used = 0;
}

static int push(qd_seq_t *s, qd_sev_t *e)
{
    if (s->nev >= QD_SEQ_NEV) {
        s->dropped_ev++;
        return 0;
    }
    e->seq = s->seqno++;
    if (e->rec >= 0)
        s->rec[e->rec].refs++;
    if (e->kind == QD_EV_TIE)
        s->tie_pending[e->track]++;
    s->ev[s->nev] = *e;
    heap_up(s->ev, s->nev++);
    return 1;
}

static qd_sev_t pop(qd_seq_t *s)
{
    qd_sev_t e = s->ev[0];
    s->ev[0] = s->ev[--s->nev];
    heap_down(s->ev, s->nev, 0);
    if (e.kind == QD_EV_TIE && s->tie_pending[e.track] > 0)
        s->tie_pending[e.track]--;
    return e;
}

static int rec_alloc(qd_seq_t *s)
{
    int i;
    for (i = 0; i < QD_SEQ_NREC; i++)
        if (!s->rec[i].used) {
            memset(&s->rec[i], 0, sizeof s->rec[i]);
            s->rec[i].used = 1;
            return i;
        }
    s->dropped_rec++;
    return -1;
}

/* an OFF for (track, note, serial) at tick */
static void push_off(qd_seq_t *s, int t, int note, uint32_t serial, int64_t tick, int64_t g, int64_t origin, int step,
                     int hit)
{
    qd_sev_t e;
    memset(&e, 0, sizeof e);
    e.step = (uint8_t)step;
    e.hit = (uint8_t)hit;
    e.tick = tick;
    e.gate = 0;
    e.g = g;
    e.origin = origin;
    e.serial = serial;
    e.rec = -1;
    e.kind = QD_EV_OFF;
    e.track = (uint8_t)t;
    e.note = (uint8_t)note;
    push(s, &e);
}

/* the OFFs held back for a TIE that will not come: out at tick */
static void unskip(qd_seq_t *s, int t, int64_t tick)
{
    int k;
    for (k = 0; k < s->skip_n[t]; k++)
        push_off(s, t, s->skip_note[t][k], s->skip_serial[t][k], tick, tick, s->origin, 0, 0);
    s->skip_n[t] = 0;
}

/* track t's waiting events of steps at or after x dropped (not the OFFs: their notes have started) */
static void drop_from(qd_seq_t *s, int t, int64_t x, int64_t now_tick)
{
    int i, n = 0, ties = 0;
    for (i = 0; i < s->nev; i++) {
        qd_sev_t *e = &s->ev[i];
        if (e->track == t && e->kind != QD_EV_OFF && e->g >= x) {
            if (e->kind == QD_EV_TIE) {
                ties++;
                if (s->tie_pending[t] > 0)
                    s->tie_pending[t]--;
            }
            rec_unref(s, e->rec);
            continue;
        }
        s->ev[n++] = *e;
    }
    s->nev = n;
    for (i = n / 2 - 1; i >= 0; i--)
        heap_down(s->ev, n, i);
    if (ties && !s->tie_pending[t])
        unskip(s, t, now_tick);
}

/* ---------------------------------------------------------------- the cursors --- */
static void cur_enter(qd_seq_t *s, qd_tcur_t *c, int pat, int64_t x)
{
    (void)s;
    c->pat = pat;
    c->origin = x;
    c->g = x;
    c->n = 0;
    c->idx = 0;
    c->pass = 0;
    c->pre = 0;
}

static void hist_push(qd_tcur_t *c, int64_t g, int r)
{
    c->hg[c->hw] = g;
    c->hr[c->hw] = (uint8_t)r;
    c->hw = (c->hw + 1) % QD_SEQ_NHIST;
    if (c->hn < QD_SEQ_NHIST)
        c->hn++;
}

static void hist_drop_from(qd_tcur_t *c, int64_t x)
{
    while (c->hn) {
        const int i = (c->hw - 1 + QD_SEQ_NHIST) % QD_SEQ_NHIST;
        if (c->hg[i] < x)
            break;
        c->hw = i;
        c->hn--;
    }
}

/* the state of track t's last conditional step at or before g, from origin on (none: 0) */
static int hist_at(const qd_tcur_t *c, int64_t g, int64_t origin)
{
    int k;
    for (k = 0; k < c->hn; k++) {
        const int i = (c->hw - 1 - k + 2 * QD_SEQ_NHIST) % QD_SEQ_NHIST;
        if (c->hg[i] > g)
            continue;
        return c->hg[i] < origin ? 0 : c->hr[i];
    }
    return 0;
}

/* the cursor reaches a queued pattern's start: it enters it */
static void cur_fix(qd_seq_t *s, qd_tcur_t *c)
{
    if (s->sw_pat >= 0 && c->origin < s->sw_x && c->g >= s->sw_x)
        cur_enter(s, c, s->sw_pat, s->sw_x);
}

static void rewind(qd_seq_t *s, int t, int64_t x, int pat)
{
    qd_tcur_t *c = &s->cur_t[t];
    drop_from(s, t, x, s->pos / QD_SUB);
    hist_drop_from(c, x);
    cur_enter(s, c, pat, x);
    s->rewinds++;
}

/* the switch to pat at tick x (cpos: its place in the chain, -1 none); cursors past x wound back */
static void schedule(qd_seq_t *s, int64_t x, int pat, int cpos)
{
    int t;
    s->sw_pat = pat;
    s->sw_x = x;
    s->sw_cpos = cpos;
    for (t = 0; t < QD_PAT_NTRACK; t++) {
        const qd_tcur_t *c = &s->cur_t[t];
        const int pending = c->origin > s->origin;      /* it entered a pattern that has not started */
        if ((pending && (c->origin != x || c->pat != pat)) || (!pending && c->g > x))
            rewind(s, t, x, pat);
    }
}

/* ---------------------------------------------------------------- a step resolved --- */
static void resolve_one(qd_seq_t *s, int t);

static void ensure(qd_seq_t *s, int t, int64_t g)   /* track t resolved through grid tick g */
{
    int guard = 0;
    for (;;) {
        qd_tcur_t *c = &s->cur_t[t];
        cur_fix(s, c);
        if (c->g > g || guard++ > 4096)
            break;
        resolve_one(s, t);
    }
}

static int cond_ok(qd_seq_t *s, int t, const qd_tcur_t *c, int cond)
{
    int a, b;
    switch (cond) {
    case QD_COND_ALWAYS:
        return 1;
    case QD_COND_FILL:
        return s->fill;
    case QD_COND_NFILL:
        return !s->fill;
    case QD_COND_PRE:
        return c->pre;
    case QD_COND_NPRE:
        return !c->pre;
    case QD_COND_NEI:
    case QD_COND_NNEI: {
        int r = 0;
        if (t > 0) {
            ensure(s, t - 1, c->g);
            r = hist_at(&s->cur_t[t - 1], c->g, c->origin);
        }
        return cond == QD_COND_NEI ? r : !r;
    }
    case QD_COND_1ST:
        return c->pass == 0;
    case QD_COND_N1ST:
        return c->pass != 0;
    default:
        if (qd_cond_ab_of(cond, &a, &b))
            return (int)(c->pass % b) == a - 1;
        return 1;
    }
}

static int is_conditional(int cond, int chance)
{
    return chance < 100 || cond == QD_COND_FILL || cond == QD_COND_NFILL || cond == QD_COND_1ST ||
           cond == QD_COND_N1ST || cond >= QD_COND_AB;
}

/* the ramp's velocity of hit h of n */
static int ramp_vel(int v, int ramp, int h, int n)
{
    int x = v;
    if (n > 1) {
        if (ramp == QD_RAMP_UP)
            x = v * (h + 1) / n;
        else if (ramp == QD_RAMP_DOWN)
            x = v * (n - h) / n;
        else if (ramp == QD_RAMP_ACCENT)
            x = h ? v * 5 / 8 : v;
    }
    return x < 1 ? 1 : x > 127 ? 127 : x;
}

/* the trig's record: the patch (the step's sound lock: a pool sound, else the track's sound; its locks over it), the
 * own mask, the mix locks, the locks as they are */
static void fill_rec(const qd_seq_t *s, qd_trec_t *r, int t, int step, const qd_tcur_t *c, const qd_pattern_t *P,
                     const qd_rstep_t *st)
{
    int n, k;
    const qd_lock_t *lk = qd_locks(P, t, step, &n);
    r->track = (uint8_t)t;
    r->type = (uint8_t)st->type;
    r->sound = (uint8_t)st->sound;
    r->step = (int16_t)step;
    r->pat = (int16_t)c->pat;
    if (st->sound) {
        memcpy(r->patch, qd_sound_patch(s->proj, st->sound), sizeof r->patch);
        r->at.own[0] = r->at.own[1] = 0xFFFFFFFFu;
        r->at.own[2] = (1u << (QD_QUAD_NP - 64)) - 1u;
    } else {
        memcpy(r->patch, s->model->t[t].patch, sizeof r->patch);
    }
    r->nlk = 0;
    for (k = 0; k < n; k++) {
        const int id = lk[k].id;
        r->lk_id[r->nlk] = (uint8_t)id;
        r->lk_v[r->nlk++] = lk[k].value;
        if (id < QD_QUAD_NP) {
            r->patch[id] = lk[k].value;
            r->at.own[id >> 5] |= 1u << (id & 31);
        } else if (id < QD_LK_N) {
            r->at.mixm |= (uint8_t)(1u << (id - QD_LK_PAN));
            r->at.mixv[id - QD_LK_PAN] = lk[k].value;
        }
    }
}

static void ev_base(qd_sev_t *e, int t, const qd_tcur_t *c, int step, int rec, int kind)
{
    memset(e, 0, sizeof *e);
    e->g = c->g;
    e->origin = c->origin;
    e->rec = (int16_t)rec;
    e->kind = (uint8_t)kind;
    e->track = (uint8_t)t;
    e->step = (uint8_t)step;
}

static void emit_step(qd_seq_t *s, int t, int step, const qd_tcur_t *c, const qd_pattern_t *P, const qd_rstep_t *st,
                      int64_t L)
{
    const qd_tset_t *ts = &P->trk[t];
    const int type = st->type, le = qd_len_eighths(st->len);
    const int64_t sw = ts->swing && (c->idx & 1) ? (int64_t)(P->swing - 50) * L / 50 : 0;
    const int64_t start = c->g + sw + (int64_t)st->micro * L / 24;
    const int64_t gate_all = le < 0 ? -1 : L * le / 8;
    uint8_t notes[QD_NNOTE];
    int nn = st->nn, k, h, ri = rec_alloc(s);
    qd_sev_t e;
    if (ri < 0)
        return;
    fill_rec(s, &s->rec[ri], t, step, c, P, st);
    for (k = 0; k < nn; k++) {
        const int x = st->notes[k] + ts->transpose;
        notes[k] = (uint8_t)(x < 1 ? 1 : x > 127 ? 127 : x);
    }
    if (type == QD_TRIG_LOCK) {
        ev_base(&e, t, c, step, ri, QD_EV_LOCK);
        e.tick = start;
        push(s, &e);
    } else if (type == QD_TRIG_TIE && ts->limit == 1) {   /* MONO: legato into the sounding voice */
        if (nn) {
            ev_base(&e, t, c, step, ri, QD_EV_TIE);
            e.tick = start;
            e.note = notes[0];
            e.vel = (uint8_t)st->vel;
            e.gate = gate_all;
            push(s, &e);
        }
    } else {                                            /* NOTE (and TIE on a POLY track: a NOTE) */
        const int nh = st->ratch, ramp = st->ramp;
        const int64_t slice = L / nh, strum = (int64_t)st->strum * L / 24;
        for (h = 0; h < nh; h++) {
            int64_t gate;
            if (nh == 1)
                gate = gate_all;
            else if (gate_all < 0)
                gate = h < nh - 1 ? slice : -1;         /* INF: the last hit holds */
            else if (gate_all <= L)
                gate = gate_all / nh;                   /* a step or less: that part of each slice */
            else
                gate = h < nh - 1 ? slice : gate_all - (nh - 1) * slice;   /* the last hit holds to the length */
            for (k = 0; k < nn; k++) {
                ev_base(&e, t, c, step, ri, QD_EV_ON);
                e.tick = start + h * slice + k * strum;
                e.note = notes[k];
                e.vel = (uint8_t)ramp_vel(st->vel, ramp, h, nh);
                e.gate = gate;
                e.hit = (uint8_t)h;
                push(s, &e);
            }
        }
    }
    if (!s->rec[ri].refs)
        s->rec[ri].used = 0;
}

static void resolve_one(qd_seq_t *s, int t)
{
    qd_tcur_t *c = &s->cur_t[t];
    const qd_pattern_t *P;
    qd_rstep_t st;
    int64_t L;
    int len, step;
    cur_fix(s, c);
    P = &s->proj->pat[c->pat];
    L = L_of(P, t);
    len = len_of(P, t);
    if (c->idx >= len) {                                /* (the loop shortened under the cursor) */
        c->idx = 0;
        c->pass++;
    }
    step = qd_track_loop_step(&P->trk[t], c->idx);      /* the loop's position -> its page's step (PLAN §8b) */
    qd_step_resolve(P, t, step, &st);                   /* the track's defaults where the step does not override */
    if (st.on) {
        const int conditional = is_conditional(st.cond, st.chance);
        int ok = cond_ok(s, t, c, st.cond);
        s->resolved++;
        if (ok && st.chance < 100)
            ok = qd_seq_roll(seg_seed(s, c->origin), t, c->n) < (uint32_t)st.chance;
        if (conditional) {
            c->pre = ok;
            hist_push(c, c->g, ok);
        }
        if (ok)
            emit_step(s, t, step, c, P, &st, L);
    }
    c->g += L;
    c->n++;
    if (++c->idx >= len) {
        c->idx = 0;
        c->pass++;
    }
}

/* ---------------------------------------------------------------- the transport --- */
void qd_seq_init(qd_seq_t *s, qd_project_t *proj, qd_model_t *model)
{
    memset(s, 0, sizeof *s);
    s->proj = proj;
    s->model = model;
    s->mbpm = 120000;
    s->seed = 0x5155414452494C4Cull;                   /* "QUADRILL" */
    s->sw_pat = -1;
    s->sw_cpos = -1;
}

void qd_seq_seed(qd_seq_t *s, uint64_t seed) { s->seed = seed; }

void qd_seq_tempo_mbpm(qd_seq_t *s, int32_t mbpm)
{
    s->mbpm = mbpm < 30000 ? 30000 : mbpm > 300000 ? 300000 : mbpm;
}

int qd_seq_bpm(const qd_seq_t *s)
{
    if (s->ext && s->clk_adv > 0)                       /* subticks a sample -> BPM: adv / 8 / 1000 x 1000 */
        return (int)((s->clk_adv + 4000) / 8000);   /* (8 x milli-BPM subticks a sample) */
    return (s->mbpm + 500) / 1000;
}

void qd_seq_fill(qd_seq_t *s, int on) { s->fill = !!on; }

static void start_at(qd_seq_t *s, int pat)
{
    int t;
    s->cur = pat;
    s->origin = 0;
    s->pos = 0;
    s->nev = 0;
    s->nstart = 1;
    s->seed_play = s->seed;
    s->sw_pat = -1;
    s->sw_cpos = -1;
    for (t = 0; t < QD_SEQ_NREC; t++)
        s->rec[t].used = 0;
    for (t = 0; t < QD_PAT_NTRACK; t++) {
        qd_tcur_t *c = &s->cur_t[t];
        memset(c, 0, sizeof *c);
        cur_enter(s, c, pat, 0);
        s->inf_n[t] = 0;
        s->tie_pending[t] = 0;
        s->flash_step[t] = -1;
        s->skip_n[t] = 0;
    }
    if (!s->ext)
        qd_seq_tempo_mbpm(s, (int32_t)s->proj->pat[pat].bpm * 1000);
    if (s->nchain > 1) {
        const int64_t m = qd_pat_master_ticks(&s->proj->pat[pat]);
        if (m > 0)
            schedule(s, m, s->chain[1], 1);
    }
}

void qd_seq_play(qd_seq_t *s)
{
    s->ext = 0;
    s->ext_wait = 0;
    if (s->nchain > 0) {
        s->cpos = 0;
        s->cur = s->chain[0];
    }
    start_at(s, s->cur);
    s->playing = 1;
}

/* every note of the sequencer's out now (its waiting OFFs, the INF notes, the OFFs held for a TIE); the queue empty */
static void all_off(qd_seq_t *s, qd_seq_out_fn out, void *ctx)
{
    int t, k;
    qd_seq_ev_t o;
    memset(&o, 0, sizeof o);
    o.sample = s->now;
    o.tick = s->pos / QD_SUB;
    o.kind = QD_EV_OFF;
    o.gate = 0;
    o.pat = s->cur;
    while (s->nev) {
        const qd_sev_t e = pop(s);
        if (e.kind == QD_EV_OFF && out) {
            o.track = e.track, o.note = e.note, o.serial = e.serial, o.step = e.step;
            out(ctx, &o);
        }
        rec_unref(s, e.rec);
    }
    for (t = 0; t < QD_PAT_NTRACK; t++) {
        for (k = 0; k < s->inf_n[t] && out; k++) {
            o.track = t, o.note = s->inf_note[t][k], o.serial = s->inf_serial[t][k];
            out(ctx, &o);
        }
        for (k = 0; k < s->skip_n[t] && out; k++) {
            o.track = t, o.note = s->skip_note[t][k], o.serial = s->skip_serial[t][k];
            out(ctx, &o);
        }
        s->inf_n[t] = 0;
        s->skip_n[t] = 0;
        s->tie_pending[t] = 0;
    }
    for (t = 0; t < QD_SEQ_NREC; t++)
        s->rec[t].used = 0;
}

void qd_seq_stop(qd_seq_t *s, qd_seq_out_fn out, void *ctx)
{
    if (!s->playing)
        return;
    all_off(s, out, ctx);
    s->playing = 0;
    s->ext = 0;
    s->ext_wait = 0;
    s->pos = 0;
    s->sw_pat = -1;
}

int64_t qd_seq_tick(const qd_seq_t *s) { return s->pos / QD_SUB; }

static int64_t next_boundary(const qd_seq_t *s, int64_t tick)
{
    const int64_t m = qd_pat_master_ticks(&s->proj->pat[s->cur]);
    if (m <= 0)
        return -1;
    return s->origin + ((tick - s->origin) / m + 1) * m;
}

const char *qd_launch_name(int launch)
{
    static const char *const N[QD_NLAUNCH] = {"END", "BAR", "BEAT", "NOW"};
    return launch >= 0 && launch < QD_NLAUNCH ? N[launch] : "?";
}

void qd_seq_select(qd_seq_t *s, int pat, int immediate)
{
    qd_seq_select_at(s, pat, immediate ? QD_LAUNCH_NOW : QD_LAUNCH_END);
}

void qd_seq_select_at(qd_seq_t *s, int pat, int launch)
{
    int64_t x;
    if (pat < 0 || pat >= QD_NPAT)
        return;
    s->nchain = 0;
    if (!s->playing) {
        s->cur = pat;
        s->sw_pat = -1;
        if (!s->ext)
            qd_seq_tempo_mbpm(s, (int32_t)s->proj->pat[pat].bpm * 1000);
        return;
    }
    {
        const int64_t now = (s->pos + QD_SUB - 1) / QD_SUB;
        if (launch == QD_LAUNCH_NOW)
            x = (now + QD_TPS - 1) / QD_TPS * QD_TPS;
        else if (launch == QD_LAUNCH_BAR || launch == QD_LAUNCH_BEAT) {   /* from the playing pattern's start */
            const int64_t u = (launch == QD_LAUNCH_BAR ? 16 : 4) * (int64_t)QD_TPS, d = now - s->origin;
            x = s->origin + (d <= 0 ? 0 : (d + u - 1) / u * u);
        } else if (s->sw_pat >= 0)
            x = s->sw_x;                                /* one already queued: the same change point */
        else
            x = next_boundary(s, now);
    }
    if (x < 0) {                                        /* master INF: it waits for ever */
        s->sw_pat = pat;
        s->sw_x = INT64_MAX / 4;
        s->sw_cpos = -1;
        return;
    }
    schedule(s, x, pat, -1);
}

void qd_seq_chain(qd_seq_t *s, const int *pats, int n)
{
    int k;
    n = n < 0 ? 0 : n > QD_SEQ_NCHAIN ? QD_SEQ_NCHAIN : n;
    for (k = 0; k < n; k++)
        s->chain[k] = pats[k] < 0 ? 0 : pats[k] >= QD_NPAT ? QD_NPAT - 1 : pats[k];
    if (!n) {
        s->nchain = 0;
        return;
    }
    if (!s->playing) {
        s->nchain = n;
        s->cpos = 0;
        s->cur = s->chain[0];
        return;
    }
    {   /* playing: the chain's first at the end of the master length */
        const int64_t now = (s->pos + QD_SUB - 1) / QD_SUB;
        const int64_t x = s->sw_pat >= 0 ? s->sw_x : next_boundary(s, now);
        s->nchain = n;
        if (x >= 0)
            schedule(s, x, s->chain[0], 0);
    }
}

int qd_seq_queued(const qd_seq_t *s) { return s->playing ? s->sw_pat : -1; }

int64_t qd_seq_to_change(const qd_seq_t *s)
{
    if (!s->playing || s->sw_pat < 0 || s->sw_x >= INT64_MAX / 8)
        return 0;
    return s->sw_x - s->pos / QD_SUB;
}

/* the queued pattern starts (its tick has come) */
static void take_switch(qd_seq_t *s)
{
    const int64_t x = s->sw_x;
    int t;
    for (t = 0; t < QD_PAT_NTRACK; t++) {               /* (every cursor into it: they are ahead, normally) */
        int guard = 0;
        while (s->cur_t[t].origin < x && guard++ < 4096)
            resolve_one(s, t);
    }
    s->cur = s->sw_pat;
    s->origin = x;
    s->nstart++;
    if (s->sw_cpos >= 0)
        s->cpos = s->sw_cpos;
    s->sw_pat = -1;
    s->sw_cpos = -1;
    if (!s->ext)
        qd_seq_tempo_mbpm(s, (int32_t)s->proj->pat[s->cur].bpm * 1000);
    if (s->nchain > 1) {
        const int np = (s->cpos + 1) % s->nchain;
        const int64_t m = qd_pat_master_ticks(&s->proj->pat[s->cur]);
        if (m > 0)
            schedule(s, x + m, s->chain[np], np);
    }
}

/* an event out: its sample in this block (P0 at the block's first sample, adv subticks a sample) */
static void dispatch(qd_seq_t *s, qd_sev_t *e, int64_t pos0, int64_t adv, qd_seq_out_fn out, void *ctx)
{
    const int64_t d = e->tick * QD_SUB - pos0;
    int64_t off = d <= 0 || adv <= 0 ? 0 : (d + adv - 1) / adv;
    const int t = e->track;
    const qd_trec_t *r = e->rec >= 0 ? &s->rec[e->rec] : 0;
    const qd_pattern_t *P = &s->proj->pat[r ? r->pat : s->cur_t[t].pat];
    qd_seq_ev_t o;
    int k;
    off = off > QD_SEQ_BLOCK - 1 ? QD_SEQ_BLOCK - 1 : off;
    memset(&o, 0, sizeof o);
    o.sample = s->now + (uint64_t)off;
    o.tick = e->tick;
    o.kind = e->kind;
    o.track = t;
    o.note = e->note;
    o.vel = e->vel;
    o.gate = e->gate;
    o.serial = e->serial;
    o.rec = r;
    o.step = e->step;
    o.pat = r ? r->pat : s->cur_t[t].pat;
    o.hit = e->hit;
    switch (e->kind) {
    case QD_EV_OFF:
        if (s->tie_pending[t] > 0 && P->trk[t].limit == 1) {   /* a TIE follows on this MONO track: it holds */
            if (s->skip_n[t] < QD_TIE_SKIP) {
                s->skip_note[t][s->skip_n[t]] = e->note;
                s->skip_serial[t][s->skip_n[t]++] = e->serial;
            }
            break;
        }
        if (out)
            out(ctx, &o);
        break;
    case QD_EV_LOCK:
        if (out)
            out(ctx, &o);
        break;
    case QD_EV_ON:
    case QD_EV_TIE:
        if (!qd_seq_audible(s, P, t)) {                 /* a muted (or not soloed) track plays nothing: it runs on */
            if (e->kind == QD_EV_TIE && !s->tie_pending[t])
                unskip(s, t, e->tick);
            break;
        }
        if (e->kind == QD_EV_TIE) {                     /* the TIE takes the held voice (an INF one too): its OFF
                                                         * ends it */
            s->skip_n[t] = 0;
            s->inf_n[t] = 0;
        }
        if (s->inf_n[t] && s->inf_g[t] != e->g) {     /* INF notes: held until the track's next NOTE */
            qd_seq_ev_t f = o;
            f.kind = QD_EV_OFF;
            f.rec = 0;
            for (k = 0; k < s->inf_n[t]; k++) {
                f.note = s->inf_note[t][k];
                f.serial = s->inf_serial[t][k];
                if (out)
                    out(ctx, &f);
            }
            s->inf_n[t] = 0;
        }
        o.serial = ++s->serial ? s->serial : ++s->serial;
        if (out)
            out(ctx, &o);
        s->played++;
        s->flash_step[t] = e->step;
        s->flash_tick[t] = e->tick;
        s->flash_sample[t] = o.sample;
        if (e->gate >= 0) {
            push_off(s, t, e->note, o.serial, e->tick + (e->gate > 0 ? e->gate : 1), e->g, e->origin, e->step, e->hit);
        } else if (s->inf_n[t] < QD_NNOTE * 8) {
            s->inf_g[t] = e->g;
            s->inf_note[t][s->inf_n[t]] = e->note;
            s->inf_serial[t][s->inf_n[t]++] = o.serial;
        }
        break;
    default:
        break;
    }
    rec_unref(s, e->rec);
}

void qd_seq_block(qd_seq_t *s, qd_seq_out_fn out, void *ctx)
{
    int64_t adv, pos0, pos1, tend;
    int t, guard = 0;
    if (!s->playing || (s->ext && s->ext_wait)) {
        s->now += QD_SEQ_BLOCK;
        return;
    }
    adv = s->ext ? s->clk_adv : 8 * (int64_t)s->mbpm;
    pos0 = s->pos;
    pos1 = pos0 + QD_SEQ_BLOCK * adv;
    if (s->ext) {
        const int64_t cap = s->clk_target + (int64_t)QD_CLOCK_TICKS * QD_SUB;
        pos1 = pos1 > cap ? cap : pos1 < pos0 ? pos0 : pos1;
    }
    while (s->sw_pat >= 0 && s->sw_x <= (pos1 - 1) / QD_SUB && guard++ < 64)   /* (in ticks: a pattern waiting
                                                                                  * for ever has a huge sw_x) */
        take_switch(s);
    tend = (pos1 + QD_SUB - 1) / QD_SUB;
    for (t = 0; t < QD_PAT_NTRACK; t++) {               /* the steps within two of their own steps */
        int n = 0;
        for (;;) {
            qd_tcur_t *c = &s->cur_t[t];
            cur_fix(s, c);
            if (c->g >= tend + 2 * L_of(&s->proj->pat[c->pat], t) || n++ > 4096)
                break;
            resolve_one(s, t);
        }
    }
    while (s->nev && s->ev[0].tick * QD_SUB < pos1) {   /* this block's events */
        qd_sev_t e = pop(s);
        dispatch(s, &e, pos0, adv, out, ctx);
    }
    s->pos = pos1;
    s->now += QD_SEQ_BLOCK;
}

/* ---------------------------------------------------------------- MIDI clock in --- */
void qd_seq_midi_start(qd_seq_t *s)
{
    if (s->nchain > 0) {
        s->cpos = 0;
        s->cur = s->chain[0];
    }
    s->ext = 1;
    start_at(s, s->cur);
    s->playing = 1;
    s->ext_wait = 1;
    s->clk_niv = s->clk_wiv = 0;
}

void qd_seq_midi_continue(qd_seq_t *s)
{
    if (s->playing && !s->ext_wait)
        return;
    s->ext = 1;
    s->playing = 1;
    s->ext_wait = 1;
}

/* stopped by the clock: the notes out, the position kept; the cursors wound back to it (CONTINUE resolves again) */
void qd_seq_midi_stop(qd_seq_t *s, qd_seq_out_fn out, void *ctx)
{
    const int64_t now = (s->pos + QD_SUB - 1) / QD_SUB;
    int t;
    if (!s->playing)
        return;
    all_off(s, out, ctx);
    for (t = 0; t < QD_PAT_NTRACK; t++) {
        qd_tcur_t *c = &s->cur_t[t];
        const qd_pattern_t *P = &s->proj->pat[c->pat];
        const int64_t L = L_of(P, t);
        const int len = len_of(P, t);
        while (c->n > 0 && c->g - L >= now) {
            c->g -= L;
            c->n--;
            if (--c->idx < 0) {
                c->idx = len - 1;
                c->pass--;
            }
        }
        hist_drop_from(c, now);
    }
    s->playing = 0;
    s->ext_wait = 0;
}

void qd_seq_midi_clock(qd_seq_t *s)
{
    const int64_t pulse = (int64_t)QD_CLOCK_TICKS * QD_SUB;
    uint64_t sum = 0;
    int k;
    if (!s->ext || !s->playing)
        return;
    if (s->ext_wait) {                                  /* the first pulse after START / CONTINUE: it runs */
        s->ext_wait = 0;
        s->clk_target = s->pos;
        s->clk_adv = s->clk_niv ? s->clk_adv : 8 * (int64_t)s->mbpm;
        s->clk_last = s->now;
        s->clk_pulses = 0;
        return;
    }
    {
        const uint64_t d = s->now - s->clk_last;
        s->clk_last = s->now;
        s->clk_iv[s->clk_wiv] = (uint32_t)(d > 44100u ? 44100u : d ? d : 1u);
        s->clk_wiv = (s->clk_wiv + 1) % 24;
        if (s->clk_niv < 24)
            s->clk_niv++;
    }
    s->clk_target += pulse;
    s->clk_pulses++;
    if (s->pos < s->clk_target)                         /* behind: to the pulse at once */
        s->pos = s->clk_target;
    for (k = 0; k < s->clk_niv; k++)
        sum += s->clk_iv[k];
    s->clk_adv = (pulse * s->clk_niv + (int64_t)sum / 2) / (int64_t)sum;   /* the last intervals' tempo */
}

/* ---------------------------------------------------------------- the view --- */
int qd_seq_audible(const qd_seq_t *s, const qd_pattern_t *P, int t)
{
    if (P->trk[t].mute || ((s->mute >> t) & 1u))
        return 0;
    return !s->solo || ((s->solo >> t) & 1u);
}

int qd_seq_track_step(const qd_seq_t *s, int t)
{
    const qd_pattern_t *P;
    if (t < 0 || t >= QD_PAT_NTRACK || !s->playing)
        return -1;
    P = qd_seq_track_pat(s, t);
    return qd_track_loop_step(&P->trk[t], (int)qd_seq_track_pos(s, t));
}

const qd_pattern_t *qd_seq_track_pat(const qd_seq_t *s, int t)
{
    if (t < 0 || t >= QD_PAT_NTRACK)
        t = 0;
    if (!s->playing)
        return &s->proj->pat[s->cur];
    return &s->proj->pat[s->cur_t[t].origin > s->pos / QD_SUB ? s->cur : s->cur_t[t].pat];
}

double qd_seq_track_pos(const qd_seq_t *s, int t)
{
    const qd_tcur_t *c;
    const qd_pattern_t *P;
    double x, len;
    if (t < 0 || t >= QD_PAT_NTRACK || !s->playing)
        return 0.0;
    c = &s->cur_t[t];
    P = &s->proj->pat[c->pat];
    len = (double)len_of(P, t);
    {
        const double T = (double)s->pos / QD_SUB, L = (double)L_of(P, t);
        if (c->origin > T) {                            /* (the cursor is in the next pattern already: the old one's
                                                         * head, from that pattern's start) */
            const qd_pattern_t *Q = &s->proj->pat[s->cur];
            const double LQ = (double)L_of(Q, t), lq = (double)len_of(Q, t);
            x = (T - (double)s->origin) / LQ;
            x -= lq * (double)(int64_t)(x / lq);
            return x < 0 ? 0 : x;
        }
        x = (double)c->idx - ((double)c->g - T) / L;
    }
    while (x < 0)
        x += len;
    while (x >= len)
        x -= len;
    return x;
}

void qd_seq_live_patch(qd_seq_t *s, int t, int id, int value)
{
    int i;
    if (id < 0 || id >= QD_QUAD_NP)
        return;
    for (i = 0; i < QD_SEQ_NREC; i++) {
        qd_trec_t *r = &s->rec[i];
        if (r->used && r->track == t && !((r->at.own[id >> 5] >> (id & 31)) & 1u))
            r->patch[id] = (int16_t)value;
    }
}

void qd_seq_step_rec(const qd_seq_t *s, int pat, int t, int step, qd_trec_t *r)
{
    qd_tcur_t c;
    qd_rstep_t st;
    const qd_pattern_t *P;
    memset(r, 0, sizeof *r);
    if (pat < 0 || pat >= QD_NPAT || t < 0 || t >= QD_PAT_NTRACK || step < 0 || step >= QD_NSTEP)
        return;
    P = &s->proj->pat[pat];
    memset(&c, 0, sizeof c);
    c.pat = pat;
    qd_step_resolve(P, t, step, &st);
    fill_rec(s, r, t, step, &c, P, &st);
}
