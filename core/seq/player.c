/* SPDX-License-Identifier: GPL-3.0-only */
/* The player (player.h). */
#include <string.h>

#include "player.h"

uint32_t qd_patch_hash(const int16_t *patch)
{
    uint32_t h = 2166136261u;
    int i;
    for (i = 0; i < QD_QUAD_NP; i++) {
        const uint16_t v = (uint16_t)patch[i];
        if (i == QD_QP_DRIVE && !v)
            continue;                                   /* (P1.4a: DRIVE 0 hashes as before it existed) */
        h = (h ^ (v & 255u)) * 16777619u;
        h = (h ^ (v >> 8)) * 16777619u;
    }
    return h;
}

void qd_player_voice_patch(const qd_pool_t *pool, int i, int16_t *out)
{
    const qd_pool_voice_t *v = &pool->v[i];
    int k;
    memcpy(out, v->pend ? v->pend_patch : v->q.patch, QD_QUAD_NP * sizeof *out);
    if (!v->pend)
        for (k = 0; k < v->dip_n; k++)
            out[v->dip_id[k]] = v->dip_v[k];
}

void qd_player_init(qd_player_t *pl, qd_seq_t *s, qd_pool_t *pool, qd_model_t *m)
{
    memset(pl, 0, sizeof *pl);
    pl->seq = s;
    pl->pool = pool;
    pl->model = m;
}

static void plog(qd_player_t *pl, const qd_seq_ev_t *e, int kind, int voice, int id, int value)
{
    qd_plog_t l;
    if (!pl->log)
        return;
    memset(&l, 0, sizeof l);
    l.sample = e ? e->sample : pl->seq->now;
    l.tick = e ? e->tick : 0;
    l.kind = kind;
    l.track = e ? e->track : 0;
    l.note = e ? e->note : 0;
    l.vel = e ? e->vel : 0;
    l.voice = voice;
    l.id = id;
    l.value = value;
    l.step = e ? e->step : -1;
    l.pat = e ? e->pat : -1;
    l.hit = e ? e->hit : 0;
    l.gate = e ? e->gate : 0;
    if (voice >= 0 && voice < pl->pool->nvoice) {
        int16_t p[QD_QUAD_NP];
        const qd_pool_voice_t *v = &pl->pool->v[voice];
        qd_player_voice_patch(pl->pool, voice, p);
        l.hash = qd_patch_hash(p);
        l.at = v->pend ? v->pend_at : v->at;
        if (kind == QD_LOG_LOCK || kind == QD_LOG_LEGATO || kind == QD_LOG_DIP)
            l.note = v->pend ? v->pend_note : v->q.note;
    }
    pl->log(pl->log_ctx, &l);
}

static int voice_of_track(const qd_pool_t *p, int i, int t)   /* sounding (gate or tail) or waiting, on track t */
{
    const qd_pool_voice_t *v = &p->v[i];
    return (qd_pool_sounding(p, i) && v->track == t) || (v->pend && v->pend_track == t);
}

/* the trig's locks onto voice i (each made its own; routing values dip) */
static void apply_locks(qd_player_t *pl, const qd_seq_ev_t *e, int i)
{
    const qd_trec_t *r = e->rec;
    int k;
    for (k = 0; k < r->nlk; k++) {
        const int id = r->lk_id[k];
        if (id < QD_QUAD_NP) {
            if (qd_pool_write(pl->pool, i, id, r->lk_v[k], 1) == 2) {
                pl->dips++;
                plog(pl, e, QD_LOG_DIP, i, id, r->lk_v[k]);
            }
        } else if (id < QD_LK_N) {
            qd_pool_set_mix(pl->pool, i, id - QD_LK_PAN, r->lk_v[k]);
        }
    }
}

static void note_on(qd_player_t *pl, const qd_seq_ev_t *e)
{
    const qd_trec_t *r = e->rec;
    const qd_tset_t *ts = &pl->seq->proj->pat[r->pat].trk[e->track];
    int i;
    qd_pool_set_track(pl->pool, e->track, ts->limit, ts->reserve);
    i = qd_pool_note_on_at(pl->pool, e->track, &pl->model->t[e->track].owner, e->note, e->vel, r->patch, &r->at);
    pl->last_on[e->track][e->note & 127] = e->serial;
    pl->ons++;
    plog(pl, e, QD_LOG_ON, i, -1, 0);
}

void qd_player_event(void *ctx, const qd_seq_ev_t *e)
{
    qd_player_t *pl = (qd_player_t *)ctx;
    const int t = e->track;
    int i;
    if (t < 0 || t >= QD_PAT_NTRACK)
        return;
    switch (e->kind) {
    case QD_EV_ON:
        note_on(pl, e);
        break;
    case QD_EV_OFF:
        if (e->serial && pl->last_on[t][e->note & 127] == e->serial) {
            qd_pool_note_off(pl->pool, t, e->note);
            pl->last_on[t][e->note & 127] = 0;
            pl->offs++;
            plog(pl, e, QD_LOG_OFF, -1, -1, 0);
        } else {                                        /* a newer strike of this note owns the voice */
            pl->offs_kept++;
            plog(pl, e, QD_LOG_OFF_KEPT, -1, -1, 0);
        }
        break;
    case QD_EV_LOCK: {
        int any = 0;
        for (i = 0; i < pl->pool->nvoice; i++)
            if (voice_of_track(pl->pool, i, t)) {
                apply_locks(pl, e, i);
                plog(pl, e, QD_LOG_LOCK, i, -1, 0);
                any = 1;
            }
        if (!any)
            plog(pl, e, QD_LOG_LOCK, -1, -1, 0);
        pl->locks++;
        break;
    }
    case QD_EV_TIE: {
        int held = -1;
        for (i = 0; i < pl->pool->nvoice; i++) {        /* the MONO track's voice, its key down */
            const qd_pool_voice_t *v = &pl->pool->v[i];
            if (qd_pool_sounding(pl->pool, i) && v->track == t && v->q.gate)
                held = i;
        }
        if (held < 0) {                                 /* nothing to tie into: a NOTE */
            note_on(pl, e);
            break;
        }
        pl->last_on[t][pl->pool->v[held].q.note & 127] = 0;
        qd_pool_legato(pl->pool, held, e->note, e->vel);
        apply_locks(pl, e, held);
        pl->last_on[t][e->note & 127] = e->serial;
        pl->legatos++;
        plog(pl, e, QD_LOG_LEGATO, held, -1, 0);
        break;
    }
    default:
        break;
    }
}

void qd_player_block(qd_player_t *pl) { qd_seq_block(pl->seq, qd_player_event, pl); }

void qd_player_stop(qd_player_t *pl) { qd_seq_stop(pl->seq, qd_player_event, pl); }

void qd_player_key_on(qd_player_t *pl, int t, int note, int vel)
{
    if (t < 0 || t >= QD_PAT_NTRACK || note < 0 || note > 127)
        return;
    qd_pool_note_on(pl->pool, t, &pl->model->t[t].owner, note, vel, pl->model->t[t].patch);
    pl->last_on[t][note] = 0;
}

void qd_player_key_off(qd_player_t *pl, int t, int note)
{
    if (t < 0 || t >= QD_PAT_NTRACK || note < 0 || note > 127)
        return;
    qd_pool_note_off(pl->pool, t, note);
}

int qd_player_preview(qd_player_t *pl, int pat, int t, int s, uint8_t *notes)
{
    static qd_trec_t r;                                 /* (single-threaded, as the whole core) */
    const qd_pattern_t *P;
    const qd_tset_t *ts;
    qd_rstep_t st;
    int i, n = 0;
    if (pat < 0 || pat >= QD_NPAT || t < 0 || t >= QD_PAT_NTRACK || s < 0 || s >= QD_NSTEP)
        return 0;
    P = &pl->seq->proj->pat[pat];
    qd_step_resolve(P, t, s, &st);
    if (!st.on || st.type == QD_TRIG_LOCK)
        return 0;
    qd_seq_step_rec(pl->seq, pat, t, s, &r);
    ts = &P->trk[t];
    qd_pool_set_track(pl->pool, t, ts->limit, ts->reserve);
    for (i = 0; i < st.nn && i < QD_NNOTE; i++) {
        const int note = st.notes[i] & 127;
        qd_pool_note_on_at(pl->pool, t, &pl->model->t[t].owner, note, st.vel, r.patch, &r.at);
        pl->last_on[t][note] = 0;
        notes[n++] = (uint8_t)note;
    }
    pl->previews++;
    return n;
}

void qd_player_live_edit(qd_player_t *pl, int t, int id, int value)
{
    qd_track_t *tk;
    int16_t lo, hi;
    int i;
    if (t < 0 || t >= QD_PAT_NTRACK || !qd_lock_range(id, &lo, &hi))
        return;
    tk = &pl->model->t[t];
    value = value < lo ? lo : value > hi ? hi : value;
    if (id >= QD_QUAD_NP) {                             /* the track's mix (the voices' own mix locks stay) */
        switch (id) {
        case QD_LK_PAN: tk->mix.pan = (int8_t)value; break;
        case QD_LK_DELAY: tk->mix.delay = (uint8_t)value; break;
        case QD_LK_REVERB: tk->mix.reverb = (uint8_t)value; break;
        case QD_LK_CHORUS: tk->mix.chorus = (uint8_t)value; break;
        case QD_LK_LEVEL: tk->mix.level = (uint8_t)value; break;
        default: break;
        }
        return;
    }
    if (tk->patch[id] != value) {
        tk->patch[id] = (int16_t)value;
        tk->edited = 1;
    }
    for (i = 0; i < pl->pool->nvoice; i++) {
        const qd_pool_voice_t *v = &pl->pool->v[i];
        const qd_vattr_t *at = v->pend ? &v->pend_at : &v->at;   /* (a voice waiting: its waiting note's) */
        if (voice_of_track(pl->pool, i, t) && !((at->own[id >> 5] >> (id & 31)) & 1u)) {
            if (qd_pool_write(pl->pool, i, id, value, 0) == 2) {
                qd_seq_ev_t e;
                memset(&e, 0, sizeof e);
                e.sample = pl->seq->now;
                e.track = t;
                pl->dips++;
                plog(pl, &e, QD_LOG_DIP, i, id, value);
            }
        }
    }
    qd_seq_live_patch(pl->seq, t, id, value);
}
