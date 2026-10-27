/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* The player (player.h). */
#include <string.h>

#include "qd_div.h"                             /* (P2.4: no 64-bit division routine in the audio path) */
#include "player.h"
#include "scale.h"

#include "mfx.c"                                        /* (P1.6c: the MIDI FX groups; built here) */

uint32_t qd_patch_hash(const int16_t *patch)
{
    uint32_t h = 2166136261u;
    int i;
    for (i = 0; i < QD_QUAD_NP; i++) {
        const uint16_t v = (uint16_t)patch[i];
        if (i == QD_QP_DRIVE && !v)
            continue;                                   /* (P1.4a: DRIVE 0 hashes as before it existed) */
        if (i >= QD_QUAD_NP5) {                         /* (E1: a value 74.. at its default hashes as before it existed) */
            int16_t lo, hi, def;
            if (qd_quad_value_range(i, &lo, &hi, &def) && patch[i] == def)
                continue;
        }
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

static void mfx_out_on(void *ctx, int g, const qd_mfx_note_t *n);
static void mfx_out_off(void *ctx, int g, int track, int pitch);
static void mfx_key(void *ctx, int track, int *scale, int *root);

void qd_player_init(qd_player_t *pl, qd_seq_t *s, qd_pool_t *pool, qd_model_t *m)
{
    qd_mfx_io_t io;
    memset(pl, 0, sizeof *pl);
    pl->seq = s;
    pl->pool = pool;
    pl->model = m;
    io.on = mfx_out_on, io.off = mfx_out_off, io.key = mfx_key, io.ctx = pl;
    qd_mfx_init(&pl->mfx, s->proj->mfx, &io);
    pl->mfx_audible = 0xFF;
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
    int id;
    for (id = 0; r->nlk && id < QD_LK_N; id++) {        /* (P2.1b: in the locks' order, by id; their values in the
                                                         * record's patch and mix) */
        if (!((r->lkm[id >> 5] >> (id & 31)) & 1u))
            continue;
        if (id < QD_QUAD_NP) {
            if (qd_pool_write(pl->pool, i, id, r->patch[id], 1) == 2) {
                pl->dips++;
                plog(pl, e, QD_LOG_DIP, i, id, r->patch[id]);
            }
        } else {
            qd_pool_set_mix(pl->pool, i, id - QD_LK_PAN, r->at.mixv[id - QD_LK_PAN]);
        }
    }
}

/* ---------------------------------------------------------------- 2026-10-26: SLIDE (player.h) */
static void slide_write(qd_player_t *pl, int i, int port, int time, int slope)   /* the glide's values onto voice i */
{
    qd_pool_write(pl->pool, i, QD_QP_PORT, port, 1);
    if (time >= 0)
        qd_pool_write(pl->pool, i, QD_QP_PTIM, time, 1);
    if (slope >= 0)
        qd_pool_write(pl->pool, i, QD_QP_PSLOPE, slope, 1);
}
static int slide_held(const qd_player_t *pl, int t, uint32_t serial)   /* an OFF of a note a LEGATO slide holds */
{
    int k;
    for (k = 0; k < QD_NNOTE; k++)
        if (serial && pl->slide_hold[t][k] == serial)
            return 1;
    return 0;
}
static void slide_unhold(qd_player_t *pl, int t)       /* the held notes let go (the one moved legato is gone already) */
{
    int k;
    for (k = 0; k < QD_NNOTE; k++) {
        const uint32_t sr = pl->slide_hold[t][k];
        const int note = pl->slide_hold_note[t][k];
        if (sr && pl->last_on[t][note] == sr) {
            qd_pool_note_off(pl->pool, t, note);
            pl->last_on[t][note] = 0;
            pl->offs++;
        }
        pl->slide_hold[t][k] = 0;
    }
}
/* the arm for e's track: the next note after the arming step takes it (cur), for every note at its tick */
static int slide_take(qd_player_t *pl, const qd_seq_ev_t *e)
{
    const int t = e->track;
    if (pl->slide_arm[t].on && e->tick > pl->slide_arm[t].tick) {
        pl->slide_cur[t] = pl->slide_arm[t];
        pl->slide_cur[t].tick = e->tick;
        pl->slide_arm[t].on = 0;
    } else if (pl->slide_cur[t].on && pl->slide_cur[t].tick != e->tick) {
        pl->slide_cur[t].on = 0;                        /* (a later note: the slide is over) */
    }
    return pl->slide_cur[t].on;
}
/* after e played: its own SLIDE lock arms the track's next note (LEGATO: this note kept in gate until then) */
static void slide_arm(qd_player_t *pl, const qd_seq_ev_t *e)
{
    const qd_trec_t *r = e->rec;
    const int t = e->track;
    if (!(r->slide_m & 1u) || r->slide_v[0] == QD_PORT_OFF) {
        if (r->slide_m & 1u)
            pl->slide_arm[t].on = 0;                    /* (an OFF lock: no slide, whatever the sound's PORT) */
        return;
    }
    pl->slide_arm[t].on = 1;
    pl->slide_arm[t].port = (int8_t)r->slide_v[0];
    pl->slide_arm[t].time = (int8_t)(r->slide_m & 2u ? r->slide_v[1] : -1);
    pl->slide_arm[t].slope = (int8_t)(r->slide_m & 4u ? r->slide_v[2] : -1);
    pl->slide_arm[t].tick = e->tick;
    if (r->slide_v[0] == QD_PORT_LEGATO) {
        int k;
        for (k = 0; k < QD_NNOTE; k++)
            if (!pl->slide_hold[t][k] || pl->slide_hold_note[t][k] == (e->note & 127)) {
                pl->slide_hold[t][k] = e->serial;
                pl->slide_hold_note[t][k] = (uint8_t)(e->note & 127);
                break;
            }
    }
}

static void note_on(qd_player_t *pl, const qd_seq_ev_t *e)
{
    const qd_trec_t *r = e->rec;
    const int t = e->track;
    const qd_tset_t *ts = &qd_proj_pat(pl->seq->proj, r->pat)->trk[t];
    qd_quad_owner_t *o = &pl->model->t[t].owner;
    const int held = qd_pool_gated(pl->pool, t);        /* (E1: LEGATO's test, before this note takes a voice) */
    const int sl = slide_take(pl, e);
    int i = -1;
    qd_pool_set_track(pl->pool, t, ts->limit, ts->reserve);
    if (sl && pl->slide_cur[t].port == QD_PORT_LEGATO) {   /* the note before, kept in gate: moved, no new attack */
        int k;
        for (k = 0; k < pl->pool->nvoice && i < 0; k++)
            if (qd_pool_sounding(pl->pool, k) && pl->pool->v[k].track == t && pl->pool->v[k].q.gate &&
                slide_held(pl, t, pl->last_on[t][pl->pool->v[k].q.note & 127]))
                i = k;
        if (i >= 0) {
            slide_write(pl, i, QD_PORT_ON, pl->slide_cur[t].time, pl->slide_cur[t].slope);
            pl->last_on[t][pl->pool->v[i].q.note & 127] = 0;
            qd_pool_legato(pl->pool, i, e->note, e->vel);
            qd_pool_glide_tie(pl->pool, o, e->note);
            apply_locks(pl, e, i);
            pl->last_on[t][e->note & 127] = e->serial;
            slide_unhold(pl, t);
            pl->legatos++;
            plog(pl, e, QD_LOG_LEGATO, i, -1, 0);
            slide_arm(pl, e);
            return;
        }
    }
    qd_pool_set_pan(pl->pool, t, pl->model->t[t].mix.pan);   /* (2026-10-26: SPREAD's middle) */
    i = qd_pool_note_on_at(pl->pool, t, o, e->note, e->vel, r->patch, &r->at);
    if (sl)
        slide_write(pl, i, QD_PORT_ON, pl->slide_cur[t].time, pl->slide_cur[t].slope);
    qd_pool_glide_note(pl->pool, i, o, r, e->tick, held);   /* E1: portamento's start (the chord: the trig's notes) */
    pl->last_on[t][e->note & 127] = e->serial;
    pl->ons++;
    plog(pl, e, QD_LOG_ON, i, -1, 0);
    if (sl)
        slide_unhold(pl, t);
    slide_arm(pl, e);
}

/* ---------------------------------------------------------------- P1.6c: the MIDI FX groups */
/* the group a trig plays through: 1..5, 0 none (or a group with no effect playing: straight on, as before) */
static int mfx_route(const qd_player_t *pl, int pat, int t, int step)
{
    const qd_project_t *pr = pl->seq->proj;
    int g;
    if (pat < 0 || pat >= QD_NPAT)
        return 0;
    g = qd_proj_cpat(pr, pat)->trk[t].mfx & QD_TMFX_OFF ? 0 : qd_step_mfx(qd_proj_cpat(pr, pat), t, step);   /* (bypassed) */
    return g && qd_mfx_group_active(&pr->mfx[g - 1]) ? g : 0;
}
static int64_t mfx_tick_of(const qd_player_t *pl, int64_t seq_tick) { return pl->mfx_base + seq_tick; }
int64_t qd_player_mfx_tick(const qd_player_t *pl) { return pl->mfx_tk; }   /* (P2.4: kept with mfx_sub) */

/* a group's note to the voices: its trig's sound and locks (the record while it plays, else resolved again), on its
 * own track */
static void mfx_out_on(void *ctx, int g, const qd_mfx_note_t *n)
{
    qd_player_t *pl = (qd_player_t *)ctx;
    const int t = n->track;
    const qd_trec_t *r = (const qd_trec_t *)n->rec;
    qd_quad_owner_t *o;
    qd_seq_ev_t e;
    int held, i;
    (void)g;
    if (t >= QD_PAT_NTRACK)
        return;
    o = &pl->model->t[t].owner;
    if (!r && n->pat >= 0 && n->pat < QD_NPAT && n->step >= 0) {   /* (P1.6e: a preview's, live, too) */
        qd_seq_step_rec(pl->seq, n->pat, t, n->step, &pl->mrec);
        r = &pl->mrec;
    }
    held = qd_pool_gated(pl->pool, t);
    qd_pool_set_pan(pl->pool, t, pl->model->t[t].mix.pan);
    if (r && n->pat >= 0 && n->pat < QD_NPAT) {
        const qd_tset_t *ts = &qd_proj_pat(pl->seq->proj, n->pat)->trk[t];
        qd_pool_set_track(pl->pool, t, ts->limit, ts->reserve);
        i = qd_pool_note_on_at(pl->pool, t, o, n->pitch, n->vel, r->patch, &r->at);
    } else {
        i = qd_pool_note_on(pl->pool, t, o, n->pitch, n->vel, pl->model->t[t].patch);
    }
    qd_pool_glide_note(pl->pool, i, o, 0, 0, held);
    pl->ons++, pl->mfx_ons++;
    memset(&e, 0, sizeof e);
    e.sample = pl->seq->now, e.tick = pl->mfx.now, e.track = t, e.note = n->pitch, e.vel = n->vel;
    e.step = n->step, e.pat = n->pat, e.gate = n->len;
    plog(pl, &e, QD_LOG_ON, i, -1, 0);
}
static void mfx_out_off(void *ctx, int g, int track, int pitch)
{
    qd_player_t *pl = (qd_player_t *)ctx;
    qd_seq_ev_t e;
    (void)g;
    if (track >= QD_PAT_NTRACK)
        return;
    qd_pool_note_off(pl->pool, track, pitch);
    pl->offs++, pl->mfx_offs++;
    memset(&e, 0, sizeof e);
    e.sample = pl->seq->now, e.tick = pl->mfx.now, e.track = track, e.note = pitch, e.step = -1, e.pat = -1;
    plog(pl, &e, QD_LOG_OFF, -1, -1, 0);
}
static void mfx_key(void *ctx, int track, int *scale, int *root)
{
    const qd_player_t *pl = (const qd_player_t *)ctx;
    qd_track_key(pl->seq->proj, qd_seq_track_pat(pl->seq, track), track, scale, root);
}

static void mfx_seq_note(qd_player_t *pl, const qd_seq_ev_t *e, int g, int on)
{
    qd_mfx_note_t n;
    memset(&n, 0, sizeof n);
    n.track = (uint8_t)e->track, n.pitch = n.prev = (uint8_t)(e->note & 127), n.vel = (uint8_t)e->vel;
    n.pat = (int16_t)e->pat, n.step = (int16_t)e->step;
    n.glen = e->gate > 0 && e->gate < 0x7FFFFFFF ? (int32_t)e->gate : 0;
    n.rec = on ? e->rec : 0;
    qd_mfx_note(&pl->mfx, g, on, &n, mfx_tick_of(pl, e->tick));
}

/* a TIE or an ON through a group: the track's notes before it into a group end first (a TIE's previous note never
 * gets its OFF) */
static void mfx_end_track(qd_player_t *pl, const qd_seq_ev_t *e)
{
    const int t = e->track;
    int p;
    for (p = 0; p < 128; p++)
        if (pl->on_grp[t][p] && pl->last_on[t][p]) {
            qd_seq_ev_t o = *e;
            o.note = p;
            mfx_seq_note(pl, &o, pl->on_grp[t][p], 0);
            pl->on_grp[t][p] = 0;
            pl->last_on[t][p] = 0;
        }
}

void qd_player_event(void *ctx, const qd_seq_ev_t *e)
{
    qd_player_t *pl = (qd_player_t *)ctx;
    const int t = e->track;
    int i;
    if (t < 0 || t >= QD_PAT_NTRACK)
        return;
    if (e->kind == QD_EV_ON || e->kind == QD_EV_TIE) {   /* P1.6c: through a group */
        const int g = mfx_route(pl, e->pat, t, e->step);
        if (pl->on_grp[t][e->note & 127] && pl->last_on[t][e->note & 127]) {   /* (struck again: the one before
                                                         * into a group ends) */
            mfx_seq_note(pl, e, pl->on_grp[t][e->note & 127], 0);
            pl->on_grp[t][e->note & 127] = 0;
        }
        if (g) {
            if (e->kind == QD_EV_TIE) {
                int p;
                for (p = 0; p < 128; p++)               /* (the tied note held straight: it ends too) */
                    if (!pl->on_grp[t][p] && pl->last_on[t][p]) {
                        qd_pool_note_off(pl->pool, t, p);
                        pl->last_on[t][p] = 0;
                    }
                mfx_end_track(pl, e);
            }
            pl->on_grp[t][e->note & 127] = (uint8_t)g;
            pl->last_on[t][e->note & 127] = e->serial;
            mfx_seq_note(pl, e, g, 1);
            return;
        }
        if (e->kind == QD_EV_TIE)
            mfx_end_track(pl, e);                       /* (a tie straight on after notes into a group) */
        pl->on_grp[t][e->note & 127] = 0;
    }
    switch (e->kind) {
    case QD_EV_ON:
        note_on(pl, e);
        break;
    case QD_EV_OFF:
        if (slide_held(pl, t, e->serial)) {             /* (2026-10-26: a LEGATO slide: in gate until the next note) */
            pl->offs_kept++;
            plog(pl, e, QD_LOG_OFF_KEPT, -1, -1, 0);
            break;
        }
        if (e->serial && pl->last_on[t][e->note & 127] == e->serial && pl->on_grp[t][e->note & 127]) {
            mfx_seq_note(pl, e, pl->on_grp[t][e->note & 127], 0);   /* P1.6c: to the group its ON went to */
            pl->on_grp[t][e->note & 127] = 0;
            pl->last_on[t][e->note & 127] = 0;
            break;
        }
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
        if (slide_take(pl, e))                          /* (2026-10-26: a slide armed: the tie glides with it) */
            slide_write(pl, held, QD_PORT_ON, pl->slide_cur[t].time, pl->slide_cur[t].slope);
        qd_pool_legato(pl->pool, held, e->note, e->vel);
        qd_pool_glide_tie(pl->pool, &pl->model->t[t].owner, e->note);   /* (E1: the track's last note) */
        apply_locks(pl, e, held);
        pl->last_on[t][e->note & 127] = e->serial;
        pl->legatos++;
        plog(pl, e, QD_LOG_LEGATO, held, -1, 0);
        slide_unhold(pl, t);
        slide_arm(pl, e);
        break;
    }
    default:
        break;
    }
}

void qd_player_block(qd_player_t *pl)
{
    qd_seq_t *s = pl->seq;
    const int playing = s->playing && !(s->ext && s->ext_wait);
    if (playing && (!pl->mfx_play || s->pos < pl->mfx_pos)) {   /* P1.6c: the transport (re)starts: the groups' clock
                                                         * carries on from where it is, the grid from here */
        pl->mfx_base = qd_player_mfx_tick(pl) - s->tick;
        pl->mfx.qref = pl->mfx_base;
    }
    qd_seq_block(s, qd_player_event, pl);
    if (playing) {
        pl->mfx_sub = pl->mfx_base * QD_SUB + s->pos;
        pl->mfx_q = pl->mfx_base + s->tick, pl->mfx_r = s->tick_rem;   /* (P2.4: its quotient, from the transport's) */
    } else {
        const int32_t d = QD_SEQ_BLOCK * 8 * s->mbpm;    /* (< 2^27) */
        pl->mfx_sub += d;
        if (pl->mfx_r >= 0 && pl->mfx_q >= 0) {
            const int32_t u = pl->mfx_r + d;
            pl->mfx_q += u / QD_SUB, pl->mfx_r = u % QD_SUB;
        } else {
            pl->mfx_q = qd_divmod64(pl->mfx_sub, QD_SUB, &pl->mfx_r);
        }
    }
    pl->mfx_tk = pl->mfx_q >= 0 && pl->mfx_r >= 0 ? pl->mfx_q + (pl->mfx_r > 0)   /* (P2.4: once a block, no 64-bit
                                                         * division: (mfx_sub + QD_SUB - 1) / QD_SUB) */
                                                  : qd_div64(pl->mfx_sub + QD_SUB - 1, QD_SUB);
    pl->mfx_play = playing;
    pl->mfx_pos = s->pos;
    {   /* a track that falls silent: its sequencer notes out of the groups */
        uint8_t aud = 0;
        int t;
        for (t = 0; t < QD_PAT_NTRACK; t++)
            if (qd_seq_audible(s, qd_seq_track_pat(s, t), t))
                aud |= (uint8_t)(1u << t);
        if (aud != pl->mfx_audible) {
            for (t = 0; t < QD_PAT_NTRACK; t++)
                if (((pl->mfx_audible >> t) & 1u) && !((aud >> t) & 1u))
                    qd_mfx_flush_track(&pl->mfx, t, 0);
            pl->mfx_audible = aud;
        }
    }
    qd_mfx_advance(&pl->mfx, qd_player_mfx_tick(pl));
}

void qd_player_stop(qd_player_t *pl)
{
    qd_seq_stop(pl->seq, qd_player_event, pl);
    qd_mfx_reset(&pl->mfx);                             /* P1.6c: every group's notes end, its arps and repeats stop */
    memset(pl->on_grp, 0, sizeof pl->on_grp);
    memset(pl->key_grp, 0, sizeof pl->key_grp);
    memset(pl->slide_arm, 0, sizeof pl->slide_arm);     /* (2026-10-26: no slide waits) */
    memset(pl->slide_cur, 0, sizeof pl->slide_cur);
    memset(pl->slide_hold, 0, sizeof pl->slide_hold);
}

void qd_player_flush_track(qd_player_t *pl, int t)
{
    if (t < 0 || t >= QD_PAT_NTRACK)
        return;
    qd_mfx_flush_track(&pl->mfx, t, -1);
    memset(pl->on_grp[t], 0, sizeof pl->on_grp[t]);
    memset(pl->key_grp[t], 0, sizeof pl->key_grp[t]);
    pl->slide_arm[t].on = pl->slide_cur[t].on = 0;
    memset(pl->slide_hold[t], 0, sizeof pl->slide_hold[t]);
    qd_pool_release_all(pl->pool, t);
}

static void mfx_key_note(qd_player_t *pl, int t, int note, int vel, int g, int on)
{
    qd_mfx_note_t n;
    memset(&n, 0, sizeof n);
    n.track = (uint8_t)t, n.pitch = n.prev = (uint8_t)note, n.vel = (uint8_t)vel;
    n.pat = -1, n.step = -1, n.live = 1;
    qd_mfx_note(&pl->mfx, g, on, &n, qd_player_mfx_tick(pl));
}

void qd_player_key_on(qd_player_t *pl, int t, int note, int vel)
{
    int held, i;
    if (t < 0 || t >= QD_PAT_NTRACK || note < 0 || note > 127)
        return;
    {   /* P1.6c: through the track's group */
        const qd_pattern_t *P = qd_seq_track_pat(pl->seq, t);
        const int g = P->trk[t].mfx & QD_TMFX_OFF ? 0 : qd_step_mfx(P, t, -1);   /* (QD_TMFX_OFF: bypassed) */
        if (pl->key_grp[t][note])
            mfx_key_note(pl, t, note, vel, pl->key_grp[t][note], 0);
        pl->key_grp[t][note] = 0;
        if (g && qd_mfx_group_active(&pl->seq->proj->mfx[g - 1])) {
            pl->key_grp[t][note] = (uint8_t)g;
            mfx_key_note(pl, t, note, vel, g, 1);
            return;
        }
    }
    held = qd_pool_gated(pl->pool, t);
    qd_pool_set_pan(pl->pool, t, pl->model->t[t].mix.pan);
    i = qd_pool_note_on(pl->pool, t, &pl->model->t[t].owner, note, vel, pl->model->t[t].patch);
    qd_pool_glide_note(pl->pool, i, &pl->model->t[t].owner, 0, 0, held);   /* E1: a played note glides alone */
    pl->last_on[t][note] = 0;
}

void qd_player_key_off(qd_player_t *pl, int t, int note)
{
    if (t < 0 || t >= QD_PAT_NTRACK || note < 0 || note > 127)
        return;
    if (pl->key_grp[t][note]) {                         /* P1.6c */
        mfx_key_note(pl, t, note, 0, pl->key_grp[t][note], 0);
        pl->key_grp[t][note] = 0;
        return;
    }
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
    P = qd_proj_pat(pl->seq->proj, pat);
    qd_step_resolve(P, t, s, &st);
    if (!st.on || st.type == QD_TRIG_LOCK)
        return 0;
    qd_seq_step_rec(pl->seq, pat, t, s, &r);
    ts = &P->trk[t];
    qd_pool_set_track(pl->pool, t, ts->limit, ts->reserve);
    {   /* P1.6e (the owner): through the step's MIDI FX group (its lock, else the track's): the arp, the chord .. heard;
         * owned as a key's notes (qd_player_key_off ends each; qd_player_preview_end the group's) */
        const int g = mfx_route(pl, pat, t, s);
        if (g) {
            for (i = 0; i < st.nn && i < QD_NNOTE; i++) {
                const int note = st.notes[i] & 127;
                qd_mfx_note_t mn;
                if (pl->key_grp[t][note])
                    mfx_key_note(pl, t, note, 0, pl->key_grp[t][note], 0);
                pl->key_grp[t][note] = (uint8_t)g;
                memset(&mn, 0, sizeof mn);
                mn.track = (uint8_t)t, mn.pitch = mn.prev = (uint8_t)note, mn.vel = (uint8_t)st.vel;
                mn.pat = (int16_t)pat, mn.step = (int16_t)s, mn.live = 1, mn.rec = &r;
                qd_mfx_note(&pl->mfx, g, 1, &mn, qd_player_mfx_tick(pl));
                notes[n++] = (uint8_t)note;
            }
            pl->previews++;
            return n;
        }
    }
    for (i = 0; i < st.nn && i < QD_NNOTE; i++) {
        const int note = st.notes[i] & 127;
        qd_pool_set_pan(pl->pool, t, pl->model->t[t].mix.pan);
        qd_pool_note_on_at(pl->pool, t, &pl->model->t[t].owner, note, st.vel, r.patch, &r.at);
        pl->last_on[t][note] = 0;
        notes[n++] = (uint8_t)note;
    }
    pl->previews++;
    return n;
}

void qd_player_preview_end(qd_player_t *pl, int t)
{
    if (t < 0 || t >= QD_PAT_NTRACK)
        return;
    qd_mfx_flush_track(&pl->mfx, t, 1);                 /* (the keys' notes in the groups: a HOLD arp's too) */
    memset(pl->key_grp[t], 0, sizeof pl->key_grp[t]);
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
    {   /* P1.5a: the pattern edited keeps its tracks' sounds (its trigs play them) */
        qd_tsound_t *ps = &qd_proj_pat(pl->seq->proj, pl->seq->cur)->snd[t];
        if (ps->patch[id] != value) {
            ps->patch[id] = (int16_t)value;
            ps->edited = 1;
        }
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

void qd_player_voice_lock(qd_player_t *pl, int t, int id, int value)
{
    int16_t lo, hi;
    int i;
    if (t < 0 || t >= QD_PAT_NTRACK || !qd_lock_range(id, &lo, &hi))
        return;
    value = value < lo ? lo : value > hi ? hi : value;
    for (i = 0; i < pl->pool->nvoice; i++) {
        if (!voice_of_track(pl->pool, i, t))
            continue;
        if (id < QD_QUAD_NP) {
            if (qd_pool_write(pl->pool, i, id, value, 1) == 2)
                pl->dips++;
        } else if (id < QD_LK_N) {
            qd_pool_set_mix(pl->pool, i, id - QD_LK_PAN, value);
        }
    }
}
