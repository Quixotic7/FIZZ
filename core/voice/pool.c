/* SPDX-License-Identifier: GPL-3.0-only */
/* The voice pool (pool.h; PLAN.md §2). */
#include <string.h>

#include "pool.h"

static int sounding(const qd_pool_voice_t *v) { return v->q.active && !v->q.kill; }
static int is_free(const qd_pool_voice_t *v) { return !v->q.active && !v->pend; }

void qd_pool_init(qd_pool_t *p, int nvoice)
{
    int i;
    memset(p, 0, sizeof *p);
    p->nvoice = nvoice < 1 ? 1 : nvoice > QD_POOL_MAX ? QD_POOL_MAX : nvoice;
    for (i = 0; i < QD_POOL_MAX; i++)
        qd_quad_voice_init(&p->v[i].q, (uint32_t)i);
    for (i = 0; i < QD_POOL_NTRACK; i++)
        p->limit[i] = QD_POOL_LIMIT_MAX;
}

void qd_pool_set_track(qd_pool_t *p, int track, int limit, int reserve)
{
    if (track < 0 || track >= QD_POOL_NTRACK)
        return;
    p->limit[track] = (uint8_t)(limit < 1 ? 1 : limit > QD_POOL_LIMIT_MAX ? QD_POOL_LIMIT_MAX : limit);
    p->reserve[track] = (uint8_t)(reserve < 0 ? 0 : reserve > QD_POOL_LIMIT_MAX ? QD_POOL_LIMIT_MAX : reserve);
}

int qd_pool_count(const qd_pool_t *p, int track)
{
    int i, n = 0;
    for (i = 0; i < p->nvoice; i++) {
        const qd_pool_voice_t *v = &p->v[i];
        n += (sounding(v) && v->track == track) || (v->pend && v->pend_track == track);
    }
    return n;
}

int qd_pool_sounding(const qd_pool_t *p, int i) { return i >= 0 && i < p->nvoice && sounding(&p->v[i]); }
int qd_pool_used(const qd_pool_t *p, int i) { return i >= 0 && i < p->nvoice && !is_free(&p->v[i]); }

int qd_pool_in_use(const qd_pool_t *p)
{
    int i, n = 0;
    for (i = 0; i < p->nvoice; i++)
        n += !is_free(&p->v[i]);
    return n;
}

/* a note into slot i now (a free voice, or one that has not sounded: from silence) */
static void start(qd_pool_t *p, int i, int track, qd_quad_owner_t *o, int note, int vel, const int16_t *patch)
{
    qd_pool_voice_t *v = &p->v[i];
    v->q.active = 0;                                    /* (a fresh start: nothing of the old note) */
    v->track = (uint8_t)track;
    qd_quad_voice_start(&v->q, patch, o, note, vel);
    v->age = ++p->clock;
    v->serial++;
    p->starts++;
}

/* the note takes slot i, which sounds: faded over the next block, the note waits for the one after (pend); a voice
 * that has not rendered a block since its start is silent: restarted in place at once */
static int take(qd_pool_t *p, int i, int track, qd_quad_owner_t *o, int note, int vel, const int16_t *patch)
{
    qd_pool_voice_t *v = &p->v[i];
    if (!v->q.env_out) {
        start(p, i, track, o, note, vel, patch);
        return i;
    }
    qd_quad_voice_kill(&v->q);
    v->pend = 1;
    v->pend_track = (uint8_t)track;
    v->pend_owner = o;
    v->pend_note = (uint8_t)note;
    v->pend_vel = (uint8_t)vel;
    v->pend_off = 0;
    memcpy(v->pend_patch, patch, sizeof v->pend_patch);
    return i;
}

/* track's own voice to take: a released one, the quietest; else the oldest held */
static int victim(const qd_pool_t *p, int track)
{
    int i, rel = -1, held = -1;
    int32_t rl = 0;
    for (i = 0; i < p->nvoice; i++) {
        const qd_pool_voice_t *v = &p->v[i];
        if (!sounding(v) || v->track != track)
            continue;
        if (!v->q.gate) {
            int32_t l = qd_quad_voice_level(&v->q);
            if (rel < 0 || l < rl) {
                rel = i;
                rl = l;
            }
        } else if (held < 0 || v->age < p->v[held].age) {
            held = i;
        }
    }
    return rel >= 0 ? rel : held;
}

int qd_pool_note_on(qd_pool_t *p, int track, qd_quad_owner_t *o, int note, int vel, const int16_t *patch)
{
    int i, best = -1, t;
    if (track < 0 || track >= QD_POOL_NTRACK || !o || !patch)
        return -1;
    /* 0. the same note again on this track */
    for (i = 0; i < p->nvoice; i++) {
        qd_pool_voice_t *v = &p->v[i];
        if (v->pend && v->pend_track == track && v->pend_note == note) {   /* still waiting: the newer strike */
            v->pend_vel = (uint8_t)vel;
            v->pend_owner = o;
            v->pend_off = 0;
            memcpy(v->pend_patch, patch, sizeof v->pend_patch);
            return i;
        }
        if (sounding(v) && v->track == track && v->q.note == note && (best < 0 || v->age > p->v[best].age))
            best = i;
    }
    if (best >= 0) {
        qd_pool_voice_t *v = &p->v[best];
        if (!memcmp(v->q.patch, patch, sizeof v->q.patch)) {   /* the same sound: struck again in place */
            qd_quad_voice_start(&v->q, patch, o, note, vel);
            v->age = ++p->clock;
            p->restrikes++;
            return best;
        }
        qd_quad_voice_release(&v->q);                    /* another sound: this one releases, a fresh voice below */
    }
    /* 1. the track at its limit: its own voice */
    if (qd_pool_count(p, track) >= p->limit[track]) {
        best = victim(p, track);
        if (best >= 0) {
            p->steals_own++;
            return take(p, best, track, o, note, vel, patch);
        }
        for (i = 0; i < p->nvoice; i++)                 /* (its limit is all waiting notes: the newest takes the place
                                                         * of the first) */
            if (p->v[i].pend && p->v[i].pend_track == track) {
                qd_pool_voice_t *v = &p->v[i];
                v->pend_owner = o;
                v->pend_note = (uint8_t)note;
                v->pend_vel = (uint8_t)vel;
                v->pend_off = 0;
                memcpy(v->pend_patch, patch, sizeof v->pend_patch);
                p->steals_own++;
                return i;
            }
    }
    /* 2. a free voice */
    for (i = 0; i < p->nvoice; i++)
        if (is_free(&p->v[i])) {
            start(p, i, track, o, note, vel, patch);
            p->frees++;
            return i;
        }
    /* 3. across the tracks: a released voice (the quietest), else the oldest held note of the track furthest over
     * its reserve; a track at or under its reserve is not robbed */
    {
        int over[QD_POOL_NTRACK], top = 0, bt = -1;
        for (t = 0; t < QD_POOL_NTRACK; t++) {
            over[t] = qd_pool_count(p, t) - p->reserve[t];
            if (over[t] > top) {
                top = over[t];
                bt = t;
            }
        }
        best = -1;
        for (i = 0; i < p->nvoice; i++) {               /* a released voice of any track over its reserve */
            const qd_pool_voice_t *v = &p->v[i];
            if (sounding(v) && !v->q.gate && over[v->track] > 0 &&
                (best < 0 || qd_quad_voice_level(&v->q) < qd_quad_voice_level(&p->v[best].q)))
                best = i;
        }
        if (best < 0 && bt >= 0) {                      /* the furthest over (a tie: the one whose held note is oldest) */
            uint32_t oldest = 0xFFFFFFFFu;
            for (t = 0; t < QD_POOL_NTRACK; t++) {
                if (over[t] != top)
                    continue;
                for (i = 0; i < p->nvoice; i++) {
                    const qd_pool_voice_t *v = &p->v[i];
                    if (sounding(v) && v->q.gate && v->track == t && v->age < oldest) {
                        oldest = v->age;
                        best = i;
                    }
                }
            }
        }
        if (best < 0) {
            p->drops++;
            return -1;
        }
        p->steals_cross++;
        return take(p, best, track, o, note, vel, patch);
    }
}

void qd_pool_note_off(qd_pool_t *p, int track, int note)
{
    int i;
    for (i = 0; i < p->nvoice; i++) {
        qd_pool_voice_t *v = &p->v[i];
        if (sounding(v) && v->track == track && v->q.note == note && v->q.gate)
            qd_quad_voice_release(&v->q);
        if (v->pend && v->pend_track == track && v->pend_note == note)
            v->pend_off = 1;                            /* (it still starts, and is released at once: a short tap
                                                         * sounds) */
    }
}

void qd_pool_release_all(qd_pool_t *p, int track)
{
    int i;
    for (i = 0; i < p->nvoice; i++) {
        qd_pool_voice_t *v = &p->v[i];
        if (sounding(v) && (track < 0 || v->track == track))
            qd_quad_voice_release(&v->q);
        if (v->pend && (track < 0 || v->pend_track == track))
            v->pend_off = 1;
    }
}

void qd_pool_kill_all(qd_pool_t *p)
{
    int i;
    for (i = 0; i < p->nvoice; i++) {
        p->v[i].pend = 0;
        qd_quad_voice_kill(&p->v[i].q);
    }
}

void qd_pool_block_begin(qd_pool_t *p)
{
    int i;
    for (i = 0; i < p->nvoice; i++) {
        qd_pool_voice_t *v = &p->v[i];
        if (v->pend && !v->q.active) {                  /* its fade has been rendered: the note starts */
            v->pend = 0;
            start(p, i, v->pend_track, v->pend_owner, v->pend_note, v->pend_vel, v->pend_patch);
            if (v->pend_off)
                qd_quad_voice_release(&v->q);
        }
    }
}

int qd_pool_render(qd_pool_t *p, int i, int32_t *out)
{
    if (i < 0 || i >= p->nvoice)
        return 0;
    return qd_quad_voice_render(&p->v[i].q, out);
}
